/**
 * @file  StatusManager.h
 * @purpose Assemble read-only system status data for the Status screen and
 *         decision-making callers. Never touches hardware or the LCD.
 * @layer Logic
 *
 * Single responsibility: expose small getters returning primitives that
 * describe the current system state. Pulls data from EEPROMManager (settings),
 * SensorManager (live moisture), PumpController (pump state), and ModeManager
 * (operational state / time-to-next-watering). Performs no rendering and no
 * mutation.
 *
 * Per SRS §7 folder responsibilities: "This module prepares status data only.
 * It shall not directly control hardware or update the LCD." MenuManager reads
 * from StatusManager and pushes the primitives into LCDManager renderers.
 *
 * May access: EEPROMManager, SensorManager, PumpController, ModeManager,
 *             SystemTypes.
 * Accessed by: MenuManager.
 */
#ifndef STATUS_MANAGER_H
#define STATUS_MANAGER_H

#include <Arduino.h>
#include <stdint.h>

#include "EEPROMManager.h"
#include "ModeManager.h"
#include "PumpController.h"
#include "SensorManager.h"
#include "SystemTypes.h"

/**
 * @class StatusManager
 * @brief Read-only aggregator of current system status primitives.
 */
class StatusManager
{
public:
    /**
     * @brief Construct the aggregator wired to its data sources.
     */
    StatusManager(EEPROMManager&  eeprom,
                  SensorManager&  sensor,
                  PumpController& pump,
                  ModeManager&   mode);

    /**
     * @brief No state to initialize beyond wiring; safe to call once at boot.
     */
    void begin();

    // --- Active settings (from EEPROMManager mirror) ------------------------

    /**
     * @brief Human-readable label of the active operating mode.
     * @return "Moisture" or "Timer" (stored in Flash via F()).
     */
    const char* getModeLabel() const;

    /**
     * @brief Get the active operating mode enum value.
     * @return MODE_MOISTURE or MODE_TIMER.
     */
    OperatingMode getMode() const;

    /** Configured moisture threshold in percent (0-100). */
    uint8_t getMoistureThresholdPercent() const;

    /** Configured timer interval in minutes (1-999). */
    uint16_t getTimerIntervalMin() const;

    /** Configured moisture-mode pump duration in seconds (1-60). */
    uint8_t getMoisturePumpDurationSec() const;

    /** Configured timer-mode pump duration in seconds (1-60). */
    uint8_t getTimerPumpDurationSec() const;

    /** Configured maximum tank capacity in millilitres. */
    uint16_t getTankCapacityMl() const;

    // --- Live run-time data -------------------------------------------------

    /** Latest filtered soil moisture in percent (0-100). */
    uint8_t getMoisturePercent() const;

    /** Estimated remaining water in millilitres (persisted mirror). */
    uint16_t getRemainingWaterMl() const;

    /** Is the pump currently running (per PumpController)? */
    bool isPumpRunning() const;

    /** Current operational state (per ModeManager). */
    OperationalState getOperationalState() const;

    /** Milliseconds until next timer-mode watering (0 if not applicable). */
    uint32_t getTimeToNextWateringMs() const;

private:
    /** Source of persisted settings. */
    EEPROMManager&  m_eeprom;
    /** Source of live moisture. */
    SensorManager&  m_sensor;
    /** Source of pump running state. */
    PumpController& m_pump;
    /** Source of operational state / time-to-next-watering. */
    ModeManager&    m_mode;
};

#endif // STATUS_MANAGER_H