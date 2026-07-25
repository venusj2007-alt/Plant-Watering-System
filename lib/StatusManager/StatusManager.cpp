/**
 * @file  StatusManager.cpp
 * @purpose Implementation of the read-only status aggregator.
 * @layer Logic
 *
 * Design notes (SRS §7 / "this module prepares status data only"):
 *  - StatusManager performs NO mutation, NO hardware access, and NO LCD calls.
 *    It only forwards values pulled from its collaborators to the caller.
 *  - getModeLabel() returns a pointer to a string stored in Flash via a small
 *    table; this avoids per-call SRAM allocation while keeping the label
 *    readable for status-screen rendering ("Moisture" / "Timer").
 *  - All other getters are trivial pass-throughs, kept here as one-liners so
 *    the public contract is verified and a single change in the underlying
 *    SystemSettings layout need only touch one call site.
 */

#include "StatusManager.h"

#include <Arduino.h>

// Flash-resident mode labels - matched 1:1 to the OperatingMode enum values so
// the index lookup below never needs a switch statement;_progmem placement is
// handled naturally by the F() style used in the LCDManager mirror of these
// strings. Keep the order in sync with OperatingMode in SystemTypes.h:
//   MODE_MOISTURE = 0, MODE_TIMER = 1.
static const char kLabelMoisture[] PROGMEM = "Moisture";
static const char kLabelTimer[]    PROGMEM = "Timer";
static const char* const kModeLabels[] PROGMEM =
{
    kLabelMoisture,
    kLabelTimer
};

StatusManager::StatusManager(EEPROMManager&  eeprom,
                             SensorManager&  sensor,
                             PumpController& pump,
                             ModeManager&   mode)
    : m_eeprom(eeprom)
    , m_sensor(sensor)
    , m_pump(pump)
    , m_mode(mode)
{
}

void StatusManager::begin()
{
    // Intentionally empty: StatusManager holds no internal state to initialize
    // beyond the injected references. Defined for lifecycle symmetry.
}

const char* StatusManager::getModeLabel() const
{
    // Cast for table offset; the array index is bounded by the (uint8_t) mode
    // value flowing through. getMode() reads EEPROMManager's mirror directly.
    uint8_t idx = (uint8_t)getMode();
    // Defensive bounds check so a corrupted mode never indexes past the table.
    if (idx >= sizeof(kModeLabels) / sizeof(kModeLabels[0]))
    {
        return kLabelMoisture; // safe default (matches factory default)
    }
    // pgm_read_ptr returns the char* itself from PROGMEM; LCDManager will print
    // it via the regular print() which works for PROGMEM-resident char* on AVR
    // only if cast appropriately. Returning a const char* pointer is enough
    // because LCDManager's print(text-char*) overload runs correctly when
    // handed a pointer-to-PROGMEM string (the underlying Print class supports
    // this). 
    return (const char*)pgm_read_ptr(&kModeLabels[idx]);
}

OperatingMode StatusManager::getMode() const
{
    return (OperatingMode)m_eeprom.settings().mode;
}

uint8_t StatusManager::getMoistureThresholdPercent() const
{
    return m_eeprom.settings().moistureThreshold;
}

uint16_t StatusManager::getTimerIntervalMin() const
{
    return m_eeprom.settings().timerInterval;
}

uint8_t StatusManager::getMoisturePumpDurationSec() const
{
    return m_eeprom.settings().moisturePumpDuration;
}

uint8_t StatusManager::getTimerPumpDurationSec() const
{
    return m_eeprom.settings().timerPumpDuration;
}

uint16_t StatusManager::getTankCapacityMl() const
{
    return m_eeprom.settings().tankCapacity;
}

uint8_t StatusManager::getMoisturePercent() const
{
    return m_sensor.getMoisturePercent();
}

uint16_t StatusManager::getRemainingWaterMl() const
{
    return m_eeprom.settings().remainingWater;
}

bool StatusManager::isPumpRunning() const
{
    return m_pump.isRunning();
}

OperationalState StatusManager::getOperationalState() const
{
    return m_mode.getOperationalState();
}

uint32_t StatusManager::getTimeToNextWateringMs() const
{
    return m_mode.getTimeToNextWateringMs();
}