/**
 * @file  DigitalOutput.cpp
 * @purpose Implementation of the digital-output HAL wrapper.
 * @layer HAL (Hardware Abstraction Layer)
 *
 * Encapsulates pinMode/digitalWrite for a single output pin. The safe state at
 * begin() is LOW so that actuators (especially the pump MOSFET) cannot energize
 * unexpectedly. No upper layer may call Arduino GPIO functions directly;
 * everything routes through here.
 *
 * Depends on: <Arduino.h> (pinMode, digitalWrite), PinDefinitions (caller side).
 */

#include "DigitalOutput.h"

// Sentinel used to mark an unbound wrapper (before begin() is called). Using
// 0xFF avoids colliding with any valid Nano pin number (valid pins are < 30).
static constexpr uint8_t kUnboundPin = 0xFF;

DigitalOutput::DigitalOutput()
    : m_pin(kUnboundPin)
    , m_state(false)
{
}

void DigitalOutput::begin(uint8_t pin)
{
    m_pin  = pin;
    m_state = false;
    pinMode(pin, OUTPUT);
    // Safe default: drive LOW immediately so any actuator starts de-energized.
    digitalWrite(pin, LOW);
}

void DigitalOutput::setHigh()
{
    if (m_pin == kUnboundPin)
    {
        return;
    }
    digitalWrite(m_pin, HIGH);
    m_state = true;
}

void DigitalOutput::setLow()
{
    if (m_pin == kUnboundPin)
    {
        return;
    }
    digitalWrite(m_pin, LOW);
    m_state = false;
}

void DigitalOutput::toggle()
{
    if (m_pin == kUnboundPin)
    {
        return;
    }
    if (m_state)
    {
        setLow();
    }
    else
    {
        setHigh();
    }
}

bool DigitalOutput::isHigh() const
{
    return m_state;
}