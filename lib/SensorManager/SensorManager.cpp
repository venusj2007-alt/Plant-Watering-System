/**
 * @file  SensorManager.cpp
 * @purpose Implementation of soil-moisture sampling, moving-average filtering,
 *         and ADC->percent mapping.
 * @layer Service
 *
 * Why this design:
 *  - Sampling cadence is millis()-driven so the loop stays fully responsive
 *    (SRS §1.5, §11): one ADC push per kMoistureSampleMs (500 ms), with the
 *    moving average computed in-place from the ring buffer.
 *  - The buffer is a fixed-size ring of kMovingAvgSamples; m_count tracks how
 *    many slots hold valid data, so the average is unbiased while the buffer
 *    fills (0..up to 10 valid samples) and a true rolling average afterwards.
 *  - Calibration values are read live from the bound SystemSettings mirror.
 *    Because the mirror is owned by EEPROMManager and Application passes the
 *    same reference throughout, calibration edits propagate here with no
 *    additional wiring (and SensorManager stays free of EEPROMManager deps).
 *  - The capacitive-sensor convention is "higher ADC = drier", so dryCal
 *    (mapping to 0 %) is the upper bound and wetCal (100 %) the lower bound.
 */

#include "SensorManager.h"

SensorManager::SensorManager(AnalogInput& input)
    : m_input(&input)
    , m_settings(nullptr)
    , m_index(0)
    , m_count(0)
    , m_lastSampleMs(0)
    , m_filtered(0)
    , m_percent(0)
    , m_error(false)
{
    // Zero the ring buffer at construction so partial-population state is clean.
    for (uint8_t i = 0; i < kMovingAvgSamples; ++i)
    {
        m_buffer[i] = 0;
    }
}

void SensorManager::begin(const SystemSettings& settings)
{
    // Bind to the canonical mirror held by EEPROMManager. We store a pointer
    // (not a reference) so begin() can be deferred from construction.
    m_settings = &settings;

    // Reset filter state so stale constructor-time values cannot leak into the
    // first percent computation.
    m_index       = 0;
    m_count       = 0;
    m_lastSampleMs = 0;
    m_filtered    = 0;
    m_percent     = 0;
    m_error       = false;
    for (uint8_t i = 0; i < kMovingAvgSamples; ++i)
    {
        m_buffer[i] = 0;
    }
}

void SensorManager::tick()
{
    // Non-blocking cadence check (SRS §11): only sample when the configured
    // interval has elapsed since the last sample. Unsigned subtraction handles
    // millis() roll-over correctly.
    uint32_t now = millis();
    if (now - m_lastSampleMs < kMoistureSampleMs)
    {
        return;
    }
    m_lastSampleMs = now;

    takeSample();
    recomputeAverage();
    m_percent = mapToPercent(m_filtered);
}

uint8_t SensorManager::getMoisturePercent() const
{
    return m_percent;
}

uint16_t SensorManager::getRawFiltered() const
{
    return m_filtered;
}

bool SensorManager::isError() const
{
    // Sensor fault detection is reserved for future implementation (SRS §11).
    // Keep the API in place so ModeManager / StatusIndicator can already query
    // this flag and react when a real implementation is added later.
    return m_error;
}

void SensorManager::takeSample()
{
    if (m_input == nullptr)
    {
        return;
    }
    uint16_t raw = m_input->readRaw();

    // Ring-buffer insert / overwrite. m_index advances modulo window size; the
    // valid-count climbs toward kMovingAvgSamples but never exceeds it.
    m_buffer[m_index] = raw;
    m_index = (m_index + 1u) % kMovingAvgSamples;
    if (m_count < kMovingAvgSamples)
    {
        ++m_count;
    }
}

void SensorManager::recomputeAverage()
{
    if (m_count == 0)
    {
        m_filtered = 0;
        return;
    }
    // Average over the valid-count (not the window size) so the first 0..9
    // readings are unbiased before the buffer becomes a true rolling window.
    uint32_t sum = 0;
    for (uint8_t i = 0; i < m_count; ++i)
    {
        sum += m_buffer[i];
    }
    m_filtered = (uint16_t)(sum / m_count);
}

uint8_t SensorManager::mapToPercent(uint16_t raw) const
{
    // Without a bound settings mirror (e.g. called before begin()) there is no
    // calibration to map against - return 0 rather than guess.
    if (m_settings == nullptr)
    {
        return 0;
    }

    uint16_t dryCal = m_settings->dryCal;  // ADC at 0 % moisture (upper bound)
    uint16_t wetCal = m_settings->wetCal;  // ADC at 100 % moisture (lower bound)

    // Clamp raw into the calibrated window first (SRS §11 calls for ignoring
    // values outside the expected calibrated range).
    if (raw >= dryCal) { return 0; }      // drier/equal to dry reference
    if (raw <= wetCal) { return 100; }    // wetter/equal to wet reference

    // Linear interpolation within the calibrated window. Arithmetic is done in
    // 32-bit to avoid 16-bit overflow during the multiply by 100 (e.g.
    // (850-400) * 100 = 45000 which overflows uint16_t).
    uint32_t span  = (uint32_t)(dryCal - wetCal);
    uint32_t above = (uint32_t)(dryCal - raw);
    uint32_t pct    = (above * 100UL) / span;
    // pct is in (0, 100) by construction here, but the defensive clamp keeps
    // the contract tight even if calibration constants become degenerate later.
    if (pct > 100UL) { return 100; }
    return (uint8_t)pct;
}