/**
 * @file  Constants.h
 * @purpose Define compile-time numeric constants shared across modules: timing
 *         values, EEPROM geometry, sizes, and the EEPROM signature magic number.
 * @layer Include (dependency-free)
 *
 * Single responsibility: hold pure numeric compile-time constants. No application
 * tunables, no factory defaults, no pin assignments (those live elsewhere). This
 * header must not depend on any other project file.
 */
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// EEPROM layout (addresses, in bytes). Confirmed layout per design phase A1:
//   addr 0       : OperatingMode            (1 byte)
//   addr 1       : MoistureThreshold        (1 byte)
//   addr 2..3    : TimerInterval            (2 bytes)
//   addr 4       : MoisturePumpDuration     (1 byte)
//   addr 5       : TimerPumpDuration       (1 byte)
//   addr 6..7    : TankCapacity            (2 bytes)
//   addr 8..9    : RemainingWater          (2 bytes)
//   addr 10..11  : DryCalibration ADC      (2 bytes)
//   addr 12..13  : WetCalibration ADC      (2 bytes)
//   addr 14..15  : EEPROM signature        (2 bytes)
// ---------------------------------------------------------------------------

/** EEPROM address of the operating mode (0 = Moisture, 1 = Timer). */
constexpr uint16_t kAddrMode = 0;

/** EEPROM address of the moisture threshold (0-100 %). */
constexpr uint16_t kAddrMoistureThreshold = 1;

/** EEPROM address of the timer interval (1-999 min, 2 bytes little-endian). */
constexpr uint16_t kAddrTimerInterval = 2;

/** EEPROM address of the moisture-mode pump duration (1-60 sec). */
constexpr uint16_t kAddrMoisturePumpDuration = 4;

/** EEPROM address of the timer-mode pump duration (1-60 sec). */
constexpr uint16_t kAddrTimerPumpDuration = 5;

/** EEPROM address of the configured tank capacity (1-9999 mL, 2 bytes). */
constexpr uint16_t kAddrTankCapacity = 6;

/** EEPROM address of the estimated remaining water (2 bytes). */
constexpr uint16_t kAddrRemainingWater = 8;

/** EEPROM address of the dry-calibration ADC value (0-1023, 2 bytes). */
constexpr uint16_t kAddrDryCal = 10;

/** EEPROM address of the wet-calibration ADC value (0-1023, 2 bytes). */
constexpr uint16_t kAddrWetCal = 12;

/** EEPROM address of the validation signature (2 bytes, little-endian). */
constexpr uint16_t kAddrSignature = 14;

/** Total number of EEPROM bytes reserved by this firmware (documentary). */
[[maybe_unused]] constexpr uint16_t kEepromUsedBytes = 16;

/** EEPROM validation signature (magic number 0x55AA). Stored little-endian on AVR. */
constexpr uint16_t kEepromSignature = 0x55AA;

// ---------------------------------------------------------------------------
// LCD geometry
// ---------------------------------------------------------------------------

/** I2C address of the 16x2 LCD module (SRS §5.3: LiquidCrystal_I2C lcd(0x27, 16, 2)). */
constexpr uint8_t kLcdI2cAddress = 0x27;

/** Number of LCD columns. */
constexpr uint8_t kLcdColumns = 16;

/** Number of LCD rows. */
constexpr uint8_t kLcdRows = 2;

// ---------------------------------------------------------------------------
// Keypad geometry
// ---------------------------------------------------------------------------

/** Number of keypad rows. */
constexpr uint8_t kKeypadRows = 4;

/** Number of keypad columns. */
constexpr uint8_t kKeypadCols = 4;

// ---------------------------------------------------------------------------
// Timing (non-blocking, millis()-based)
// ---------------------------------------------------------------------------

/** Splash-screen duration at boot. delay() is permitted ONLY for this (SRS golden rule). */
constexpr uint32_t kSplashScreenMs = 2000;

/** Duration of temporary message screens ("Settings Saved", "Invalid Value", "Try Again"). */
constexpr uint32_t kMessageScreenMs = 3000;

/** Auto-advance interval for status-screen pages. */
constexpr uint32_t kStatusPageMs = 3000;

/** Soil-moisture sampling interval (fixed, non-blocking). */
constexpr uint32_t kMoistureSampleMs = 500;

/** Number of samples kept in the moving-average filter. */
constexpr uint8_t kMovingAvgSamples = 10;

// ---------------------------------------------------------------------------
// Low-water / tank threshold
// ---------------------------------------------------------------------------

/** Remaining water (mL) below which the low-water condition is triggered (SRS §16). */
constexpr uint16_t kLowWaterThresholdMl = 250;

// ---------------------------------------------------------------------------
// Status indicator (red LED) blink timing
// ---------------------------------------------------------------------------

/** Red LED ON half-period during error blink (mL irrelevant - duration only). */
constexpr uint32_t kStatusBlinkOnMs = 500;

/** Red LED OFF half-period during error blink. */
constexpr uint32_t kStatusBlinkOffMs = 500;

// ---------------------------------------------------------------------------
// Numeric input buffers (per-field digit caps confirmed in design phase A15)
// ---------------------------------------------------------------------------

/** Maximum digits for moisture threshold (0-100). */
constexpr uint8_t kMaxDigitsThreshold = 3;

/** Maximum digits for timer interval (1-999). */
constexpr uint8_t kMaxDigitsInterval = 3;

/** Maximum digits for pump duration (1-60). */
constexpr uint8_t kMaxDigitsPumpDuration = 2;

/** Maximum digits for tank capacity (1-9999). 4 digits allowed here only. */
constexpr uint8_t kMaxDigitsTankCapacity = 4;

/** Size of the input char buffer (largest field + null terminator). */
constexpr uint8_t kInputBufferMax = 5;

/** Sentinel returned when no keypad key is pressed. */
constexpr char kNoKey = '\0';

#endif // CONSTANTS_H