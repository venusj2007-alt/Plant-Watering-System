/**
 * @file  PinDefinitions.h
 * @purpose Define all Arduino Nano pin assignments for the Plant Watering System.
 * @layer Include (HAL configuration)
 *
 * Single responsibility: enumerate every physical pin used by the firmware as a
 * compile-time constant so that no other module hard-codes pin numbers. Reserved
 * (unused) pins are documented in comments to support future expansion.
 *
 * Pin assignments are FIXED per SRS §4 and must not change unless the hardware
 * schematic is revised. Analog pins (A0, A4, A5) use Arduino core macros; digital
 * pins use bare integers because the AVR Arduino core does not define D-prefixed
 * macros.
 */
#ifndef PIN_DEFINITIONS_H
#define PIN_DEFINITIONS_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Analog inputs
// ---------------------------------------------------------------------------

/** Soil moisture sensor analog input (0-1023 ADC). SRS pin A0. */
constexpr uint8_t kPinSoilMoisture = A0;

// ---------------------------------------------------------------------------
// I2C bus (shared by LCD)
// ---------------------------------------------------------------------------

/** LCD I2C SDA line. SRS pin A4 (shared hardware bus). Documentary only: the
 *  Wire library defines the SDA pin at the hardware level, so this symbol is
 *  not directly referenced by application code; kept for pin-map completeness. */
[[maybe_unused]] constexpr uint8_t kPinI2cSda = A4;

/** LCD I2C SCL line. SRS pin A5 (shared hardware bus). Documentary only: same
 *  rationale as kPinI2cSda. */
[[maybe_unused]] constexpr uint8_t kPinI2cScl = A5;

// ---------------------------------------------------------------------------
// Keypad matrix (rows D2-D5, columns D6-D9)
// ---------------------------------------------------------------------------

/** Keypad row 1. SRS pin D2. */
constexpr uint8_t kPinKeypadRow1 = 2;

/** Keypad row 2. SRS pin D3. */
constexpr uint8_t kPinKeypadRow2 = 3;

/** Keypad row 3. SRS pin D4. */
constexpr uint8_t kPinKeypadRow3 = 4;

/** Keypad row 4. SRS pin D5. */
constexpr uint8_t kPinKeypadRow4 = 5;

/** Keypad column 1. SRS pin D6. */
constexpr uint8_t kPinKeypadCol1 = 6;

/** Keypad column 2. SRS pin D7. */
constexpr uint8_t kPinKeypadCol2 = 7;

/** Keypad column 3. SRS pin D8. */
constexpr uint8_t kPinKeypadCol3 = 8;

/** Keypad column 4. SRS pin D9. */
constexpr uint8_t kPinKeypadCol4 = 9;

// ---------------------------------------------------------------------------
// Output actuators / indicators
// ---------------------------------------------------------------------------

/** MOSFET gate controlling the water pump. SRS pin D10 (PWM-capable for future use). */
constexpr uint8_t kPinPumpGate = 10;

/** Green status LED. SRS pin D11. Indicates pump active.
 *  Owner: PumpController (SRS §3.1 LED-behavior update / §18 dependency rules).
 *  No other module may drive this pin. */
constexpr uint8_t kPinGreenLed = 11;

/** Red status LED. SRS pin D12. Indicates error / low-water.
 *  Owner: StatusIndicator (SRS §3.1 LED-behavior update / §18 dependency rules).
 *  No other module may drive this pin. */
constexpr uint8_t kPinRedLed = 12;

// ---------------------------------------------------------------------------
// Reserved (intentionally unused for future expansion - SRS §4)
// ---------------------------------------------------------------------------
// D0  (RX)  - Reserved for serial communication.
// D1  (TX)  - Reserved for serial communication.
// A1  - Reserved for future sensors (e.g. water level).
// A2  - Reserved for future sensors (e.g. temperature).
// A3  - Reserved for future sensors (e.g. humidity).
// AREF - Reserved for future external analog reference.

#endif // PIN_DEFINITIONS_H