/**
 * @file  Config.h
 * @purpose Define application-level tunables, factory defaults, and validation
 *         ranges. The single source of compile-time tunable values.
 * @layer Include (depends on SystemTypes for the OperatingMode enum default)
 *
 * Single responsibility: hold tunables + defaults + bounds. No pin assignments
 * (PinDefinitions.h), no raw timing/constants (Constants.h). EEPROMManager reads
 * factory defaults from here; MenuManager and EEPROMManager share the same
 * validation ranges to avoid divergence (design A14).
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#include "SystemTypes.h"

// ---------------------------------------------------------------------------
// Watering algorithm tunables
// ---------------------------------------------------------------------------

/**
 * @brief Moisture hysteresis in percent.
 *
 * After a watering cycle triggered by <threshold>, watering may not be requested
 * again until measured moisture rises above (threshold + kHysteresisPercent),
 * preventing rapid on/off cycling from sensor noise (SRS §11). Confirmed = 5.
 */
constexpr uint8_t kHysteresisPercent = 5;

/**
 * @brief Cooldown period after each watering cycle (Moisture Mode only).
 *
 * Additional watering requests are ignored during this window (SRS §12). Timer
 * Mode is exempt (it uses its own interval spacing - design A7). Confirmed = 30 s.
 */
constexpr uint32_t kCooldownMs = 30000UL;

/**
 * @brief Nominal pump flow rate in millilitres per second.
 *
 * Used to estimate water consumed per cycle: waterUsed = kPumpFlowRateMlPerSec
 * * pumpDurationSec. PROVISIONAL value (design A3): based on a typical small
 * submersible pump. MUST be verified against the selected pump's datasheet or by
 * measurement before deployment. Easily adjusted here if the hardware changes.
 */
constexpr uint16_t kPumpFlowRateMlPerSec = 30;

// ---------------------------------------------------------------------------
// ADC calibration bounds
// ---------------------------------------------------------------------------

/** Minimum raw ADC reading (Arduino 10-bit ADC lower bound; documentary). */
[[maybe_unused]] constexpr uint16_t kMinAdc = 0;

/** Maximum raw ADC reading (Arduino 10-bit ADC upper bound). */
constexpr uint16_t kMaxAdc = 1023;

// ---------------------------------------------------------------------------
// Validation ranges (shared by MenuManager pre-check and EEPROMManager defense)
// ---------------------------------------------------------------------------

/** Minimum soil-moisture threshold (%). */
constexpr uint8_t kMinMoisturePercent = 0;
/** Maximum soil-moisture threshold (%). */
constexpr uint8_t kMaxMoisturePercent = 100;

/** Minimum watering interval (minutes). */
constexpr uint16_t kMinTimerIntervalMin = 1;
/** Maximum watering interval (minutes). */
constexpr uint16_t kMaxTimerIntervalMin = 999;

/** Minimum pump duration (seconds). */
constexpr uint8_t kMinPumpDurationSec = 1;
/** Maximum pump duration (seconds). */
constexpr uint8_t kMaxPumpDurationSec = 60;

/** Minimum tank capacity (mL). */
constexpr uint16_t kMinTankCapacityMl = 1;
/** Maximum tank capacity (mL). */
constexpr uint16_t kMaxTankCapacityMl = 9999;

// ---------------------------------------------------------------------------
// Factory defaults (written to a blank/corrupted EEPROM by EEPROMManager)
// ---------------------------------------------------------------------------

/** Default operating mode at first boot. */
constexpr OperatingMode kDefaultOperatingMode = MODE_MOISTURE;

/** Default moisture threshold (%). SRS §10 factory default. */
constexpr uint8_t kDefaultMoistureThreshold = 40;

/** Default watering interval (minutes). SRS §10 factory default. */
constexpr uint16_t kDefaultTimerInterval = 180;

/** Default moisture-mode pump duration (seconds). SRS §10 factory default. */
constexpr uint8_t kDefaultMoisturePumpDuration = 5;

/** Default timer-mode pump duration (seconds). SRS §10 factory default. */
constexpr uint8_t kDefaultTimerPumpDuration = 5;

/** Default tank capacity (mL). SRS §2.8 factory default. */
constexpr uint16_t kDefaultTankCapacity = 2000;

/**
 * @brief Default dry-calibration raw ADC value.
 *
 * Typical capacitive soil-moisture sensor v1.2 reading in air/dry soil. Higher
 * ADC normally corresponds to drier medium (design A2). Verified default.
 */
constexpr uint16_t kDefaultDryCal = 850;

/**
 * @brief Default wet-calibration raw ADC value.
 *
 * Typical capacitive soil-moisture sensor v1.2 reading submerged in water
 * (design A2). Verified default.
 */
constexpr uint16_t kDefaultWetCal = 400;

#endif // CONFIG_H