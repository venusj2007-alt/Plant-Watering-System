/**
 * @file  PumpController.cpp
 * @purpose Implementation of the non-blocking water-pump controller.
 * @layer Service
 *
 * Owns the MOSFET gate (pump switch) and the green "pump active" LED. Enforces
 * mutual exclusion (only one cycle at a time), times the active cycle via
 * millis() (never delay()), and accounts the millilitres consumed so
 * ModeManager can update the remaining-water estimate after a cycle ends.
 *
 * Why these design choices:
 *  - begin() forces both outputs LOW first so the pump could not possibly be
 *    energized during boot EEPROM reads / LCD init (SRS §12 "Safety on Startup").
 *  - The "just-finished" latch is one-shot and polled by ModeManager via
 *    takeFinished(); this avoids losing the completion edge even if tick() is
 *    invoked many times before ModeManager runs.
 *  - Elapsed time uses unsigned subtraction (millis() - m_startMs) which is
 *    correct even across the 49-day millis() roll-over.
 */

#include "PumpController.h"

// Sentinel: the start-time field before any cycle has been started. Its value
// is irrelevant once m_running is false; we set it for determinism.
static constexpr uint32_t kNoStartTime = 0xFFFFFFFFUL;

PumpController::PumpController(DigitalOutput& gate, DigitalOutput& greenLed)
    : m_gate(gate)
    , m_greenLed(greenLed)
    , m_running(false)
    , m_startMs(kNoStartTime)
    , m_durationSec(0)
    , m_justFinished(false)
    , m_waterUsedMl(0)
{
}

void PumpController::begin()
{
    // Force the actuator OFF BEFORE any other subsystem is touched (SRS §12).
    // This guarantees the pump cannot energize unexpectedly during boot.
    m_gate.setLow();
    m_greenLed.setLow();
    m_running      = false;
    m_startMs      = kNoStartTime;
    m_durationSec  = 0;
    m_justFinished = false;
    m_waterUsedMl  = 0;
}

bool PumpController::start(uint8_t durationSec)
{
    // Mutual exclusion (SRS §12): no second cycle may start while one is active.
    if (m_running)
    {
        return false;
    }
    m_running      = true;
    m_startMs      = millis();
    m_durationSec  = durationSec;
    m_justFinished = false;
    m_waterUsedMl  = 0;
    m_gate.setHigh();
    m_greenLed.setHigh();
    return true;
}

void PumpController::tick()
{
    if (!m_running)
    {
        return;
    }
    // Unsigned subtraction handles millis() roll-over correctly. Cast duration
    // to uint32_t so the multiply never truncates (60 * 1000 fits comfortably
    // in uint32_t; still, future durations would not silently wrap).
    uint32_t elapsed = millis() - m_startMs;
    uint32_t target  = (uint32_t)m_durationSec * 1000UL;
    if (elapsed >= target)
    {
        // Duration expired: de-energize the pump + LED, latch a one-shot
        // completion event, and record the millilitres consumed this cycle.
        // The volume is derived from the configured flow rate (Config.h) and
        // the programmed duration - see SRS §12 "Tank Volume Estimation".
        m_gate.setLow();
        m_greenLed.setLow();
        m_running      = false;
        m_waterUsedMl  = (uint32_t)kPumpFlowRateMlPerSec * (uint32_t)m_durationSec;
        m_justFinished = true;
    }
}

bool PumpController::isRunning() const
{
    return m_running;
}

bool PumpController::takeFinished(uint32_t& outWaterMl)
{
    // One-shot edge event: ModeManager consumes this each tick so the "cycle
    // just ended" event is never missed even with many loop iterations.
    if (!m_justFinished)
    {
        return false;
    }
    outWaterMl     = m_waterUsedMl;
    m_justFinished = false;
    return true;
}