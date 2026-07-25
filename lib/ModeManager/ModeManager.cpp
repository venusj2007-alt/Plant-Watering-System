/**
 * @file  ModeManager.cpp
 * @purpose Implementation of the operational background engine: decides when
 *         to water under Moisture or Timer Mode, enforces mutual exclusion,
 *         cooldown, hysteresis, and the low-water condition, and accounts used
 *         water via PumpController so the tank-level estimate stays current.
 * @layer Logic
 *
 * Per-loop tick model (SRS §9.1, all non-blocking):
 *   1. If a watering cycle is in progress: poll PumpController; on its
 *      one-shot "finished" edge, subtract consumed water, persist the new
 *      remaining estimate, restart the relevant timer/cooldown, set the
 *      hysteresis lock (Moisture Mode only), then re-evaluate the low-water
 *      state.
 *   2. If OP_LOW_WATER: do NOT request any watering. Each tick, check whether
 *      a refill (typically the user pressing "C" via MenuManager) has bumped
 *      the persisted remaining estimate above the threshold; if so, drop back
 *      to OP_NORMAL and re-arm the mode's timers.
 *   3. If OP_NORMAL: detect setting drift (mode, interval, durations) and
 *      re-arm timers when needed; then evaluate the active mode's watering
 *      conditions and request a new cycle through PumpController if allowed.
 *
 * Key rules (SRS §11 Moisture Mode, §12 Pump Control + design A7/A8/A11/A12):
 *  - Mutual exclusion: only one cycle at a time (PumpController.start refuses
 *    overlap and returns false; m_opState=OP_WATERING for the duration).
 *  - Moisture Mode hysteresis: after a moisture-triggered cycle the lock is
 *    SET; it clears only once the measured moisture rises above
 *    (threshold + kHysteresisPercent). This prevents rapid on/off cycling.
 *  - Moisture Mode cooldown: a kCooldownMs window starts after every
 *    moisture-triggered cycle; no new request is accepted before it elapses.
 *  - Timer Mode: the interval itself spaces cycles; no cooldown applies
 *    (design A7). The interval timer is restarted after each cycle AND when
 *    the configured interval changes mid-session (design A8).
 *  - Background-rule (SRS §9 end + design Q12/Q13): tick() keeps running
 *    regardless of UI state. STATE_WATERING as a UI screen is rendered by
 *    MenuManager poll, not requested here.
 *  - Cycle never interrupted by low-water inside a run (design A12): only the
 *    NEXT request is suppressed; the in-flight cycle completes, then
 *    low-water is checked.
 *  - Boot-path low-water (design A11): if the persisted estimate is already
 *    below threshold at begin time, opState starts OP_LOW_WATER.
 */

#include "ModeManager.h"

#include <Arduino.h>

ModeManager::ModeManager(EEPROMManager& eeprom,
                         SensorManager& sensor,
                         PumpController& pump)
    : m_eeprom(eeprom)
    , m_sensor(sensor)
    , m_pump(pump)
    , m_intervalTimer()
    , m_cooldownTimer()
    , m_opState(OP_NORMAL)
    , m_hysteresisLock(false)
    , m_cachedMode(MODE_MOISTURE)
    , m_cachedInterval(0)
    , m_cachedMoistureDuration(0)
    , m_cachedTimerDuration(0)
{
}

void ModeManager::begin()
{
    const SystemSettings& s = m_eeprom.settings();

    // Prime the cached snapshot so detectSettingsChange() has a sane baseline.
    m_cachedMode             = (OperatingMode)s.mode;
    m_cachedInterval         = s.timerInterval;
    m_cachedMoistureDuration = s.moisturePumpDuration;
    m_cachedTimerDuration    = s.timerPumpDuration;

    // Clear transient watering-side state so a hard reboot cannot resume an
    // imaginary in-flight cycle.
    m_hysteresisLock = false;
    m_cooldownTimer.stop();

    // Arm the interval timer only in Timer Mode. In Moisture Mode the interval
    // timer is irrelevant (cooldown spans cycles instead).
    if (m_cachedMode == MODE_TIMER)
    {
        m_intervalTimer.start((uint32_t)s.timerInterval * 60000UL);
    }
    else
    {
        m_intervalTimer.stop();
    }

    // Initial safety state: a first-boot-after-factory-reset or a previously
    // emptied tank both arrive here with remaining water below the threshold
    // and MUST NOT be allowed to water until refilled (design A11).
    m_opState = (s.remainingWater < kLowWaterThresholdMl) ? OP_LOW_WATER
                                                          : OP_NORMAL;
}

void ModeManager::tick()
{
    // Step 1: lifecycle of an in-progress watering cycle. PumpController owns
    // the per-cycle stopwatch; we just consume its finished edge.
    if (m_opState == OP_WATERING)
    {
        m_pump.tick();
        handleCycleFinish();
        return;
    }

    // Step 2: low-water hold. We do not request new watering; we only test
    // whether the user has refilled (via MenuManager's 'C' key path which
    // persists directly through EEPROMManager.setRemainingWater / tank menu
    // refill). Once the persisted estimate climbs back to >= threshold, drop
    // to OP_NORMAL and re-arm the mode-appropriate timers.
    if (m_opState == OP_LOW_WATER)
    {
        if (m_eeprom.settings().remainingWater >= kLowWaterThresholdMl)
        {
            m_opState = OP_NORMAL;
            resetModeState();
        }
        return;
    }

    // Step 3: OP_NORMAL - keep the snapshot in sync with EEPROM (the user may
    // have changed mode / interval / durations via the menu) then evaluate the
    // current mode's watering conditions.
    detectSettingsChange();

    if (m_cachedMode == MODE_MOISTURE)
    {
        evaluateMoistureMode();
    }
    else
    {
        evaluateTimerMode();
    }
}

OperationalState ModeManager::getOperationalState() const
{
    return m_opState;
}

bool ModeManager::isWatering() const
{
    return (m_opState == OP_WATERING);
}

uint32_t ModeManager::getTimeToNextWateringMs() const
{
    // Only meaningful in Timer Mode while idle. While watering or in low-water
    // the answer is "no imminent schedule", reported as 0 to keep the caller
    // (status screen / StatusManager) safe.
    if (m_opState != OP_NORMAL || m_cachedMode != MODE_TIMER)
    {
        return 0;
    }
    return m_intervalTimer.remainingMs();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void ModeManager::detectSettingsChange()
{
    const SystemSettings& s = m_eeprom.settings();

    // Mode switch -> full re-arm of the mode's timers + clear locks (design A8).
    if ((OperatingMode)s.mode != m_cachedMode)
    {
        m_cachedMode = (OperatingMode)s.mode;
        resetModeState();
        return; // resetModeState already read the latest interval/durations
    }

    // Interval change while in Timer Mode -> restart the interval timer at the
    // new value (design A8): this is effectively a fresh schedule so the next
    // watering is reset relative to "now".
    if (m_cachedMode == MODE_TIMER && s.timerInterval != m_cachedInterval)
    {
        m_cachedInterval = s.timerInterval;
        m_intervalTimer.start((uint32_t)s.timerInterval * 60000UL);
    }

    // Pump durations are consumed at PumpController.start() call time, so a
    // changed duration simply takes effect on the next requested cycle - we
    // only need to keep the cache updated for drift detection.
    m_cachedMoistureDuration = s.moisturePumpDuration;
    m_cachedTimerDuration    = s.timerPumpDuration;
}

void ModeManager::evaluateMoistureMode()
{
    // Mutual exclusion: PumpController refuses overlapping starts, but we also
    // guard the opState explicitly so callers polling getOperationalState()
    // stay consistent.
    if (m_pump.isRunning() || m_opState == OP_WATERING)
    {
        return;
    }

    // Cooldown: an active (started, not yet expired) cooldown suppresses
    // requests. After it expires (latched), isExpired() stays true; the
    // "still counting" predicate is isRunning() && !isExpired().
    if (m_cooldownTimer.isRunning() && !m_cooldownTimer.isExpired())
    {
        return;
    }

    uint8_t moisture = m_sensor.getMoisturePercent();
    uint8_t threshold = m_eeprom.settings().moistureThreshold;

    // Hysteresis (SRS §11): once watering has been triggered by "moisture <
    // threshold", do NOT water again until moisture rises above
    // (threshold + kHysteresisPercent). The lock serves as the memory of that
    // past trigger so noisy readings near the threshold don't oscillate.
    if (m_hysteresisLock)
    {
        // Defensive: threshold + hysteresis may overflow uint8_t only at
        // threshold == 100, in which case lock can never legitimately clear
        // because the reading can't exceed 100 %; leaving the mode / reboot
        // is the practical reset path.
        uint16_t clearLevel = (uint16_t)threshold + (uint16_t)kHysteresisPercent;
        if ((uint16_t)moisture > clearLevel)
        {
            m_hysteresisLock = false;
        }
        else
        {
            return;
        }
    }

    // Request a cycle when moisture has fallen below threshold.
    if (moisture < threshold)
    {
        uint8_t durationSec = m_eeprom.settings().moisturePumpDuration;
        if (m_pump.start(durationSec))
        {
            m_opState = OP_WATERING;
            // Hysteresis lock + cooldown are armed AFTER the cycle completes
            // (in handleCycleFinish) so a cycle interrupted by power-loss
            // doesn't leave them set with no timing baseline.
        }
    }
}

void ModeManager::evaluateTimerMode()
{
    if (m_pump.isRunning() || m_opState == OP_WATERING)
    {
        return;
    }

    // The interval timer was started in begin()/resetModeState() (or restarted
    // after the last cycle). When its window has elapsed, latch-fires a
    // watering request. Note: we do NOT require isRunning() because resetState
    // may have just stopped it (mode switch) - but if it is stopped, isExpired()
    // is false anyway, so no spurious watering.
    if (m_intervalTimer.isExpired())
    {
        uint8_t durationSec = m_eeprom.settings().timerPumpDuration;
        if (m_pump.start(durationSec))
        {
            m_opState = OP_WATERING;
        }
    }
}

void ModeManager::handleCycleFinish()
{
    uint32_t waterUsedMl = 0;
    if (!m_pump.takeFinished(waterUsedMl))
    {
        return; // cycle still in progress - nothing to do this tick
    }

    // PumpController reports the litres consumed this cycle. Subtract from the
    // persisted estimate, clamp to zero (no signed underflow), and persist via
    // the EEPROMManager setter (which clamps to tankCapacity defensively).
    const SystemSettings& s = m_eeprom.settings();
    uint32_t currentRemaining = s.remainingWater;
    uint16_t newRemaining = 0;
    if (waterUsedMl < currentRemaining)
    {
        newRemaining = (uint16_t)(currentRemaining - waterUsedMl);
    }
    // setRemainingWater clamps to tankCapacity (no-op here since used only
    // reduces the value) and persists.
    m_eeprom.setRemainingWater(newRemaining);

    // Re-fetch settings so the latest persisted estimate drives the low-water
    // decision (the setter wrote the new value into the mirror).
    const SystemSettings& s2 = m_eeprom.settings();

    // Arm the next-cycle spacing for the active mode.
    if ((OperatingMode)s2.mode == MODE_MOISTURE)
    {
        // Moisture Mode: cooldown + hysteresis lock keep the soil from being
        // rewatered too quickly.
        m_cooldownTimer.start(kCooldownMs);
        m_hysteresisLock = true;
    }
    else
    {
        // Timer Mode: the interval restarts from "cycle just ended". The
        // interval timer's duration is already cached; restart() reuses it.
        m_intervalTimer.restart();
    }

    // Low-water transition (SRS §16 + design A12 / A14): a finished cycle may
    // drop the estimate below the threshold; if so, take the engine to
    // OP_LOW_WATER so subsequent cycles are blocked until refilled.
    if (s2.remainingWater < kLowWaterThresholdMl)
    {
        m_opState = OP_LOW_WATER;
    }
    else
    {
        m_opState = OP_NORMAL;
    }
}

void ModeManager::resetModeState()
{
    // Called on mode change (and on the low-water -> normal transition) to
    // re-arm the new mode's timers with the current EEPROM settings and to
    // drop any now-irrelevant watering-side state.
    const SystemSettings& s = m_eeprom.settings();

    m_hysteresisLock = false;
    m_cooldownTimer.stop();

    if (m_cachedMode == MODE_TIMER)
    {
        m_intervalTimer.start((uint32_t)s.timerInterval * 60000UL);
    }
    else
    {
        m_intervalTimer.stop();
    }

    // The interval / durations are re-read here (caller already updated
    // m_cachedMode; caching the rest keeps the snapshot consistent).
    m_cachedInterval         = s.timerInterval;
    m_cachedMoistureDuration = s.moisturePumpDuration;
    m_cachedTimerDuration    = s.timerPumpDuration;
}