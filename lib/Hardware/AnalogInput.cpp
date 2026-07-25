/**
 * @file  AnalogInput.cpp
 * @purpose Implementation of the analog-input HAL wrapper.
 * @layer HAL (Hardware Abstraction Layer)
 *
 * Encapsulates pinMode/analogRead for a single analog input pin. Returns only
 * the raw 10-bit ADC reading (0-1023). Any multi-sample filtering, calibration,
 * or percent mapping is performed by the owning service (SensorManager), NOT
 * here, so this class stays a thin, reusable HAL primitive.
 *
 * Depends on: <Arduino.h> (pinMode, analogRead), PinDefinitions (caller side).
 */

#include "AnalogInput.h"

// Sentinel used to mark an unbound wrapper (before begin() is called). 0xFF does
// not collide with any valid Nano analog pin number.
static constexpr uint8_t kUnboundPin = 0xFF;

AnalogInput::AnalogInput()
    : m_pin(kUnboundPin)
{
}

void AnalogInput::begin(uint8_t pin)
{
    m_pin = pin;
    // Configure explicitly as a high-impedance input (no internal pull-up) so
    // the external sensor signal is read unaltered.
    pinMode(pin, INPUT);
    digitalWrite(pin, LOW); // ensure pull-up is off
}

uint16_t AnalogInput::readRaw() const
{
    if (m_pin == kUnboundPin)
    {
        return 0;
    }
    return analogRead(m_pin);
}