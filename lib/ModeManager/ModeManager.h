/**
 * @file  ModeManager.h
 * @purpose Owns the operational background engine: decides when to water,
 *         enforces mutual exclusion, cooldown, hysteresis, and the low-water
 *         condition, and coordinates with PumpController / SensorManager.
 * @layer Logic
 *
 * Single responsibility: run the active operating mode (Moisture or Timer) in
 * the background every loop. Does NOT render the UI (that is MenuManager) and
 * does NOT directly touch GPIO (that is PumpController / HAL). MenuManager
 * polls getOperationalState() to decide whether to show STATE_WATERING /
 * STATE_LOW_WATER; it never commands the pump (design Q7).
 *
 * Key rules (SRS §11, §12, §16 + design A7/A8/A11/A12):
 *  - Mutual exclusion: only one watering cycle at a time; new requests ignored
 *    while PumpController.isRunning().
 *  - Moisture Mode: watering requested when (filtered moisture < threshold) and
 *    the hysteresis lock is cleared and the cooldown is not active. After a
 *    moisture-triggered cycle the lock is set; it clears once moisture rises
 *    above (threshold + kHysteresisPercent).
 *  - Timer Mode: watering requested when the interval timer has expired. No
 *    cooldown applies (interval itself spaces cycles).
 *  - Low-water: after each completed cycle, water used is subtracted from the
 *    persisted remaining-water estimate. If that estimate drops below
 *    kLowWaterThresholdMl, the pump is disabled until the estimate is restored
 *    (refilled via MenuManager 'C'/tank menu). A cycle is never interrupted
 *    mid-run (design A12); only the next request is prevented.
 *  - Setting drift (design A8): reads settings every tick. If the timer
 *    interval changes, the interval timer is restarted. Mode changes reset the
 *    new mode's timers and clear moisture cooldown/hysteresis for a clean start.
 *
 * Per SRS §8.2 layering, ModeManager (Logic) may depend on Service (SensorManager,
 * PumpController) and Logic (EEPROMManager, TimerManager) - both downward / intra,
 * never upward. It does NOT depend on MenuManager or StatusIndicator.
 *
 * May access: EEPROMManager, SensorManager, PumpController, TimerManager (class),
 *             Config, Constants, SystemTypes.
 * Accessed by: Application (tick), MenuManager (poll opState), StatusManager,
 *              StatusIndicator (poll opState).
 */
#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <Arduino.h>
#include <stdint.h>

#include "Config.h"
#include "Constants.h"
#include "EEPROMManager.h"
#include "PumpController.h"
#include "SensorManager.h"
#include "TimerManager.h"  // SoftwareTimer (Logic timing primitive)

/**
 * @class ModeManager
 * @brief Background watering engine; owns system OperationalState.
 */
class ModeManager
{
public:
    /**
     * @brief Construct the manager wired to its service/logic collaborators.
     */
    ModeManager(EEPROMManager& eeprom,
                SensorManager& sensor,
                PumpController& pump);

    /**
     * @brief Initialize timers and operational state from the current settings.
     *        Must be called AFTER EEPROMManager::begin() and PumpController::begin().
     *        If settings.remainingWater < kLowWaterThresholdMl at boot, the
     *        initial opState is OP_LOW_WATER (design A11).
     */
    void begin();

    /**
     * @brief Advance the watering engine by one loop iteration. Call every loop.
     *        Non-blocking. Handles cycle completion, request evaluation, cooldown,
     *        hysteresis, interval expiry, and low-water state transitions.
     */
    void tick();

    /**
     * @brief Current operational state for polling by UI / indicator modules.
     * @return OP_NORMAL, OP_WATERING, or OP_LOW_WATER.
     */
    OperationalState getOperationalState() const;

    /**
     * @brief Convenience predicate (equivalent to getOperationalState() == OP_WATERING).
     * @return true if a watering cycle is currently in progress.
     */
    bool isWatering() const;

    /**
     * @brief Milliseconds remaining until the next scheduled watering.
     *        Meaningful in Timer Mode (interval remaining). In Moisture Mode or
     *        while watering / low-water, returns 0.
     * @return Remaining milliseconds, or 0 if not applicable.
     */
    uint32_t getTimeToNextWateringMs() const;

private:
    // --- references to collaborators (injected) -----------------------------
    /** Persistent settings + remaining-water mirror. */
    EEPROMManager&  m_eeprom;
    /** Filtered moisture source. */
    SensorManager&  m_sensor;
    /** Pump actuator + green LED + cycle accounting. */
    PumpController& m_pump;

    // --- owned timers (Logic primitive instances) ---------------------------
    /** Timer-mode interval timer (restarted after each cycle / on interval change). */
    SoftwareTimer m_intervalTimer;
    /** Moisture-mode cooldown timer (started after each moisture-triggered cycle). */
    SoftwareTimer m_cooldownTimer;

    // --- operational state --------------------------------------------------
    /** Current operational state (OP_NORMAL / OP_WATERING / OP_LOW_WATER). */
    OperationalState m_opState;

    /** Hysteresis lock for Moisture Mode (true after a cycle until moisture > threshold+hysteresis). */
    bool m_hysteresisLock;

    // --- cached settings snapshot (used to detect drift, design A8) ---------
    OperatingMode m_cachedMode;
    uint16_t      m_cachedInterval;
    uint8_t       m_cachedMoistureDuration;
    uint8_t       m_cachedTimerDuration;

    // --- private helpers ----------------------------------------------------

    /** Detect setting changes since last tick and react (restart interval timer,
     *  reset mode timers / clear cooldown+hysteresis on mode change). */
    void detectSettingsChange();

    /** Evaluate Moisture Mode: request watering if conditions are met. */
    void evaluateMoistureMode();

    /** Evaluate Timer Mode: request watering if interval expired. */
    void evaluateTimerMode();

    /** Handle a just-finished watering cycle: subtract water, persist, restart
     *  the relevant timer/cooldown, update low-water state. */
    void handleCycleFinish();

    /** Re-arm the new mode's timers and clear transient locks (on mode change). */
    void resetModeState();
};

#endif // MODE_MANAGER_H