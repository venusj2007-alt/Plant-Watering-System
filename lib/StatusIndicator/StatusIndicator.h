/**
 * @file  StatusIndicator.h
 * @purpose Dedicated driver for the red status LED, indicating system health:
 *         OFF (normal), solid ON (low-water), or 500 ms blink (system/hardware
 *         error). Errors take priority over low-water.
 * @layer Service (with an acknowledged upward dependency on ModeManager)
 *
 * Single responsibility: drive ONE digital output (the red LED) based on the
 * current system status. Owns the blink timing; owns no other logic.
 *
 * This module is the result of the SRS LED-behavior update: the red LED is no
 * longer toggled by Application or ModeManager directly. The Application calls
 * StatusIndicator::tick() once per main loop; StatusIndicator then queries
 * ModeManager (for OperationalState) and SensorManager (for fault flag) itself
 * and drives the red LED accordingly.
 *
 * Architectural note (design A12): StatusIndicator normally lives in the
 * Service layer but depends on ModeManager (Logic). This upward dependency is
 * an explicit, user-approved exception, because the indicator must reflect
 * system status owned by ModeManager. It is a leaf driver and introduces no
 * cycles (ModeManager does not depend on StatusIndicator).
 *
 * Red LED behavior (supersedes all prior descriptions - SRS §3.1 update):
 *   - OFF                         : normal operation.
 *   - Solid ON                    : low-water condition (OP_LOW_WATER).
 *   - Blink 500 ms ON / 500 ms OFF: system / hardware error (e.g. sensor fault).
 *   - Error has priority over low-water (blink overrides solid ON).
 *
 * May access: DigitalOutput (HAL), ModeManager (Logic), SensorManager (Service),
 *             Constants (blink timing).
 * Accessed by: Application (tick only).
 */
#ifndef STATUS_INDICATOR_H
#define STATUS_INDICATOR_H

#include <Arduino.h>
#include <stdint.h>

#include "Constants.h"
#include "DigitalOutput.h"  // HAL: lib/Hardware (resolved by PlatformIO LDF)
#include "ModeManager.h"
#include "SensorManager.h"
#include "SystemTypes.h"

/**
 * @class StatusIndicator
 * @brief Drives the red status LED from current system status with blink support.
 */
class StatusIndicator
{
public:
    /**
     * @brief Construct the indicator wired to its red-LED HAL output and its
     *        status sources (ModeManager, SensorManager).
     */
    StatusIndicator(DigitalOutput&  redLed,
                    ModeManager&   modeManager,
                    SensorManager& sensorManager);

    /**
     * @brief Force the red LED OFF and initialize blink bookkeeping. Call once
     *        at boot, after ModeManager::begin() and SensorManager::begin().
     */
    void begin();

    /**
     * @brief Update the red LED once per main loop. Queries the current system
     *        status, applies the OFF / solid-ON / blink decision (error has
     *        priority over low-water), and drives the pin.
     */
    void tick();

private:
    /**
     * @enum IndicatorState
     * @brief Effective indicator mode chosen for the current loop iteration.
     */
    enum IndicatorState
    {
        INDICATOR_NORMAL    = 0,  // LED OFF
        INDICATOR_LOW_WATER = 1,  // LED solid ON
        INDICATOR_ERROR     = 2   // LED blinking 500 ms ON / 500 ms OFF
    };

    /** HAL output driving the red LED. */
    DigitalOutput&  m_redLed;
    /** Source of OperationalState (low-water detection). */
    ModeManager&    m_modeManager;
    /** Source of sensor-fault state (system error detection). */
    SensorManager&  m_sensorManager;

    /** Currently effective indicator mode (input to blink bookkeeping). */
    IndicatorState  m_state;

    /** Whether the LED is currently physically ON during the blink cycle. */
    bool            m_blinkOn;

    /** millis() timestamp of the last blink half-period transition. */
    uint32_t        m_lastToggleMs;

    /** Compute the effective indicator state from current system status. */
    IndicatorState computeState() const;

    /** Apply the given effective state to the LED hardware. Handles blink
     *  transitions for INDICATOR_ERROR and solid states otherwise. */
    void applyState(IndicatorState newState);
};

#endif // STATUS_INDICATOR_H