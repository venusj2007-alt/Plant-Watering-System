/**
 * @file  DigitalOutput.h
 * @purpose Hardware Abstraction Layer wrapper for a single digital output pin.
 * @layer HAL (Hardware Abstraction Layer)
 *
 * Single responsibility: encapsulate pinMode/digitalWrite for one output pin
 * so that no upper layer ever calls Arduino core GPIO functions directly (SRS
 * §5.3 Rule 5, §8.3). Owns no application semantics.
 *
 * May access: Arduino core (pinMode, digitalWrite) only.
 * Accessed by: PumpController (pump gate + green LED), StatusIndicator (red LED).
 */
#ifndef DIGITAL_OUTPUT_H
#define DIGITAL_OUTPUT_H

#include <Arduino.h>

/**
 * @class DigitalOutput
 * @brief Thin OO wrapper over a single Arduino digital output pin.
 *        Tracks the cached output level so callers may query without re-reading
 *        the pin hardware.
 */
class DigitalOutput
{
public:
    /**
     * @brief Default constructor. The pin must be bound via begin() before use.
     */
    DigitalOutput();

    /**
     * @brief Bind this wrapper to a physical pin and configure it as OUTPUT.
     *        The pin is driven LOW initially (safe default).
     * @param pin  Arduino digital pin number (use constants from PinDefinitions.h).
     */
    void begin(uint8_t pin);

    /**
     * @brief Drive the output HIGH.
     */
    void setHigh();

    /**
     * @brief Drive the output LOW.
     */
    void setLow();

    /**
     * @brief Toggle the cached output level (HIGH<->LOW).
     */
    void toggle();

    /**
     * @brief Query the last written level without touching the hardware.
     * @return true if the pin was last driven HIGH.
     */
    bool isHigh() const;

private:
    /** Physical Arduino pin number this wrapper is bound to. */
    uint8_t m_pin;

    /** Cached last-written output level (true = HIGH). */
    bool    m_state;
};

#endif // DIGITAL_OUTPUT_H