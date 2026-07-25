/**
 * @file  SensorManager.h
 * @purpose Service-layer soil-moisture acquisition: sampling, moving-average
 *         filtering, and ADC->percent mapping using EEPROM calibration values.
 * @layer Service
 *
 * Single responsibility: produce a stable moisture percentage from raw analog
 * samples. Does NOT make watering decisions (that is ModeManager). Does NOT
 * store calibration (that is EEPROMManager); this module reads the live
 * SystemSettings mirror (via const reference) so calibration edits are picked
 * up automatically.
 *
 * Per SRS §8.2 layering, SensorManager (Service) MUST NOT depend on EEPROMManager
 * (Logic). The calibration values reach this module as a const SystemSettings&
 * passed in by Application - a SystemTypes type (dependency-free), so no
 * layering violation occurs.
 *
 * Sampling is non-blocking: every kMoistureSampleMs (500 ms) one raw reading is
 * pushed into a ring buffer of kMovingAvgSamples (10); the filtered value and
 * its percent mapping are recomputed on each tick. May access: AnalogInput (HAL),
 * SystemTypes, Config (calibration bounds), Constants (timing/window size).
 * Accessed by: ModeManager, StatusManager, MenuManager (status), Application.
 */
#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <stdint.h>

#include "Config.h"
#include "Constants.h"
#include "SystemTypes.h"
#include "AnalogInput.h"  // HAL: lib/Hardware (resolved by PlatformIO LDF)

/**
 * @class SensorManager
 * @brief Non-blocking soil-moisture sampler + average filter + percent mapper.
 */
class SensorManager
{
public:
    /**
     * @brief Construct the sensor manager bound to an analog input.
     * @param input   The HAL analog input connected to the moisture sensor.
     */
    explicit SensorManager(AnalogInput& input);

    /**
     * @brief Bind the live settings mirror (for calibration values) and reset
     *        the filter buffer. Call once during Application boot, AFTER
     *        EEPROMManager::begin() has populated the mirror.
     * @param settings  Const reference to the canonical SystemSettings mirror.
     */
    void begin(const SystemSettings& settings);

    /**
     * @brief Advance sampling by one loop iteration. Must be called every loop
     *        so the 500 ms sampling cadence is respected. Non-blocking.
     */
    void tick();

    /**
     * @brief Get the most-recent filtered moisture as a percent (0-100).
     *        Returns the last computed value even if no new sample was taken
     *        this loop.
     * @return Moisture percent, clamped to 0..100.
     */
    uint8_t getMoisturePercent() const;

    /**
     * @brief Get the most-recent filtered raw ADC reading (0-1023).
     * @return Filtered raw value (moving average over up to 10 samples).
     */
    uint16_t getRawFiltered() const;

    /**
     * @brief Indicate whether a sensor fault is currently detected.
     *        Sensor fault detection is reserved for future implementation
     *        (SRS §11). For this version, always returns false.
     * @return true if a sensor fault is currently active.
     */
    bool isError() const;

private:
    /** HAL analog input bound to the moisture sensor. */
    AnalogInput* m_input;

    /** Live reference to the canonical settings mirror (for calibration). */
    const SystemSettings* m_settings;

    /** Ring buffer of recent raw ADC samples (size kMovingAvgSamples). */
    uint16_t   m_buffer[kMovingAvgSamples];

    /** Insertion index into the ring buffer (mod kMovingAvgSamples). */
    uint8_t    m_index;

    /** Number of valid samples currently in the buffer (0..kMovingAvgSamples). */
    uint8_t    m_count;

    /** millis() timestamp of the last sample push. */
    uint32_t   m_lastSampleMs;

    /** Last computed moving-average raw value. */
    uint16_t   m_filtered;

    /** Last computed moisture percent (0..100). */
    uint8_t    m_percent;

    /** Sticky fault flag (reserved for future; always false this version). */
    bool       m_error;

    /** Take one raw sample and push it into the ring buffer. */
    void       takeSample();

    /** Recompute the moving average over m_count samples. */
    void       recomputeAverage();

    /**
     * @brief Map a raw ADC value to a moisture percent using the dry/wet
     *        calibration constants. Higher ADC = drier medium (capacitive
     *        sensor convention). Result clamped to 0..100.
     */
    uint8_t    mapToPercent(uint16_t raw) const;
};

#endif // SENSOR_MANAGER_H