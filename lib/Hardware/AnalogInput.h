/**
 * @file  AnalogInput.h
 * @purpose Hardware Abstraction Layer wrapper for a single analog input pin.
 * @layer HAL (Hardware Abstraction Layer)
 *
 * Single responsibility: encapsulate pinMode/analogRead for one analog input
 * pin so that no upper layer calls Arduino core ADC functions directly (SRS
 * §5.3 Rule 5, §8.3). Owns no application semantics, no filtering.
 *
 * May access: Arduino core (pinMode, analogRead) only.
 * Accessed by: SensorManager (soil moisture).
 */
#ifndef ANALOG_INPUT_H
#define ANALOG_INPUT_H

#include <Arduino.h>

/**
 * @class AnalogInput
 * @brief Thin OO wrapper over a single Arduino analog input pin.
 *        Returns raw 10-bit ADC readings (0-1023). Any multi-sample filtering /
 *        calibration is performed by the owning service (SensorManager), NOT here.
 */
class AnalogInput
{
public:
    /**
     * @brief Default constructor. The pin must be bound via begin() before use.
     */
    AnalogInput();

    /**
     * @brief Bind this wrapper to a physical pin and configure it as INPUT with
     *        no pull-up (floating / external signal expected).
     * @param pin  Arduino analog pin number (use constants from PinDefinitions.h).
     */
    void begin(uint8_t pin);

    /**
     * @brief Read the raw 10-bit ADC value from the bound pin.
     * @return Raw ADC reading in the range 0-1023.
     */
    uint16_t readRaw() const;

private:
    /** Physical Arduino analog pin this wrapper is bound to. */
    uint8_t m_pin;
};

#endif // ANALOG_INPUT_H