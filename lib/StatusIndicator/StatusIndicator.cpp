/**
 * @file  StatusIndicator.cpp
 * @purpose Implementation of the dedicated red status-LED driver.
 * @layer Service (with user-approved upward dependency on ModeManager)
 *
 * Behavior (SRS §3.1 update - this file supersedes any prior red-LED
 * description):
 *   - OFF                         : normal operation (opState == OP_NORMAL and
 *                                    no sensor fault).
 *   - Solid ON                    : low-water condition (opState == OP_LOW_WATER
 *                                    without a system error).
 *   - Blink 500 ms ON / 500 ms OFF: system / hardware error (sensor fault).
 *   - Priority: if both low-water AND an error are present, the error wins and
 *     the LED blinks (it is NOT solid ON).
 *
 * Implementation notes:
 *  - Blink period is split across two named constants (kStatusBlinkOnMs and
 *    kStatusBlinkOffMs) per Constants.h. They are the same here (500 ms each)
 *    but kept split to allow future asymmetry (e.g. short ON / long OFF) without
 *    touching this file.
 *  - applyState() is state-driven: if newState differs from m_state for a
 *    non-blink case, the LED is forced to the right level immediately - the
 *    user gets instant feedback on a transition into low-water (or out of it).
 *    Inside the blink case (INDICATOR_ERROR), applyState() also handles the
 *    half-period toggle on every tick it is re-entered; this is naturally
 *    idempotent across repeated calls because transitions across the half-
 *    period are timestamp-gated.
 *  - No delay(); every timing check uses millis() with unsigned subtraction.
 *  - StatusIndicator owns the red LED only. The green LED is owned by
 *    PumpController (design A12 / SRS §3.1 update).
 */

#include "StatusIndicator.h"

#include <Arduino.h>

StatusIndicator::StatusIndicator(DigitalOutput&  redLed,
                                 ModeManager&   modeManager,
                                 SensorManager& sensorManager)
    : m_redLed(redLed)
    , m_modeManager(modeManager)
    , m_sensorManager(sensorManager)
    , m_state(INDICATOR_NORMAL)
    , m_blinkOn(false)
    , m_lastToggleMs(0)
{
}

void StatusIndicator::begin()
{
    // Force the red LED to a known-OFF state at boot and reset blink
    // bookkeeping. The next tick() will compute the correct state.
    m_redLed.setLow();
    m_state        = INDICATOR_NORMAL;
    m_blinkOn      = false;
    m_lastToggleMs = 0;
}

void StatusIndicator::tick()
{
    // Each loop iteration: query the current system status, decide the
    // effective indicator state, then apply it (which also drives the blink
    // half-period transitions on the ERROR branch).
    IndicatorState newState = computeState();
    applyState(newState);
}

StatusIndicator::IndicatorState StatusIndicator::computeState() const
{
    // Priority rule: error > low-water > normal (SRS §3.1 update). Sensor
    // faults are the only currently-defined system-error source; future
    // hardware-error additions should be aggregated here.
    if (m_sensorManager.isError())
    {
        return INDICATOR_ERROR;
    }
    if (m_modeManager.getOperationalState() == OP_LOW_WATER)
    {
        return INDICATOR_LOW_WATER;
    }
    return INDICATOR_NORMAL;
}

void StatusIndicator::applyState(IndicatorState newState)
{
    if (newState == INDICATOR_ERROR)
    {
        // If we just transitioned into the blink case (from a non-error
        // state), force the LED ON and seed the blink bookkeeping so the
        // first 500 ms ON half-period begins cleanly. This guarantees the
        // user sees an immediate visible blink start, not a 500 ms gap.
        if (m_state != INDICATOR_ERROR)
        {
            m_state        = INDICATOR_ERROR;
            m_blinkOn      = true;
            m_lastToggleMs = millis();
            m_redLed.setHigh();
            return;
        }

        // Continuing the blink: check whether the current half-period has
        // elapsed; if so, flip the level and reset the timestamp.
        uint32_t now = millis();
        uint32_t halfPeriod = m_blinkOn ? kStatusBlinkOnMs : kStatusBlinkOffMs;
        if ((now - m_lastToggleMs) >= halfPeriod)
        {
            m_blinkOn = !m_blinkOn;
            m_lastToggleMs = now;
            if (m_blinkOn)
            {
                m_redLed.setHigh();
            }
            else
            {
                m_redLed.setLow();
            }
        }
        return;
    }

    // Non-error states: drive solidly and reset blink bookkeeping so a later
    // transition back into INDICATOR_ERROR seeds cleanly (no stale timestamps).
    if (newState == INDICATOR_LOW_WATER)
    {
        if (m_state != INDICATOR_LOW_WATER)
        {
            m_state   = INDICATOR_LOW_WATER;
            m_blinkOn = false;
            m_redLed.setHigh();
        }
        return;
    }

    // INDICATORNORMAL: ensure OFF (also covers the first-tick case where the
    // constructor's defaults are still active).
    if (m_state != INDICATOR_NORMAL)
    {
        m_state   = INDICATOR_NORMAL;
        m_blinkOn = false;
        m_redLed.setLow();
    }
}