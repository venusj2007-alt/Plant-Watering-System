/**
 * @file  EEPROMManager.cpp
 * @purpose Implementation of the persistent-configuration manager: EEPROM
 *         signature validation, settings load/save, factory defaults, and the
 *         defensive validation of every value entering or leaving the mirror.
 * @layer Logic
 *
 * EEPROM layout (Constants.h addresses; confirmed in design A1):
 *   addr 0       : OperatingMode            (1 byte)
 *   addr 1       : MoistureThreshold        (1 byte)
 *   addr 2..3    : TimerInterval            (2 bytes, little-endian via put/get)
 *   addr 4       : MoisturePumpDuration     (1 byte)
 *   addr 5       : TimerPumpDuration        (1 byte)
 *   addr 6..7    : TankCapacity             (2 bytes)
 *   addr 8..9    : RemainingWater           (2 bytes)
 *   addr 10..11  : DryCal ADC               (2 bytes)
 *   addr 12..13  : WetCal ADC               (2 bytes)
 *   addr 14..15  : Signature 0x55AA         (2 bytes; signature is internal per A13)
 *
 * Key design decisions:
 *  - Signature gate (SRS §10): on first boot or after corruption, defaults
 *    plus the signature are written in one pass; thereafter the saved
 *    configuration is loaded. Sanitization replaces any single out-of-range
 *    field with its factory default WITHOUT rewriting EEPROM (the user can
 *    notice and explicitly save, or the next setter normalizes it).
 *  - Wear minimization (SRS §2.12 / §20): every setter persists a field ONLY
 *    when its accepted value differs from the cached mirror value. Putting
 *    defaults down on a blank EEPROM is a single-pass write (worst case).
 *  - Defensive validation (design A14): every user-facing setter rejects
 *    out-of-range input with `false` and leaves the cached value untouched.
 *    EXCEPT setRemainingWater, which is NOT user input - it is an internal
 *    estimate maintained by ModeManager. Clamping there is the documented
 *    contract; A14 (no silent clamp) applies to user-entered values only.
 *  - Multi-byte values are written via EEPROM.put/get (design Q19); the AVR
 *    core stores them little-endian, matching 0x55AA as 0xAA,0x55 in memory.
 */

#include "EEPROMManager.h"

#include <Arduino.h>
#include <EEPROM.h>

EEPROMManager::EEPROMManager()
    : m_signatureValid(false)
{
    // Zero-init the mirror so any field read before begin() returns something
    // safe. begin() always overwrites every field before use.
    m_settings.mode                 = 0;
    m_settings.moistureThreshold    = 0;
    m_settings.timerInterval        = 0;
    m_settings.moisturePumpDuration = 0;
    m_settings.timerPumpDuration    = 0;
    m_settings.tankCapacity         = 0;
    m_settings.remainingWater        = 0;
    m_settings.dryCal               = 0;
    m_settings.wetCal               = 0;
}

bool EEPROMManager::begin()
{
    // Signature gate: read the 2-byte magic at the signature address. If it
    // matches 0x55AA, the EEPROM holds a previously-saved configuration; load
    // and sanitize it. Otherwise, the EEPROM is blank/corrupt - write the
    // factory-default set + signature in one pass.
    uint16_t storedSig = readU16(kAddrSignature);
    if (storedSig == kEepromSignature)
    {
        m_signatureValid = true;
        loadFromEeprom();
        sanitizeLoaded();
        return true;
    }
    // First boot or corruption: defaults must be written so subsequent boots
    // can load them.
    m_signatureValid = false;
    resetToFactoryDefaults();
    // After writing defaults, the EEPROM now contains a valid signature too;
    // mark it as such so callers can rely on isSignatureValid() post-begin.
    m_signatureValid = true;
    return false;
}

const SystemSettings& EEPROMManager::settings() const
{
    return m_settings;
}

bool EEPROMManager::isSignatureValid() const
{
    return m_signatureValid;
}

// ---------------------------------------------------------------------------
// Mutators (each validates defensively; returns acceptance per A14)
// ---------------------------------------------------------------------------

bool EEPROMManager::setMode(OperatingMode mode)
{
    // OperatingMode enum only defines MODE_MOISTURE=0 / MODE_TIMER=1; anything
    // else is corrupt input and must be rejected (A14).
    if (mode != MODE_MOISTURE && mode != MODE_TIMER)
    {
        return false;
    }
    uint8_t next = (uint8_t)mode;
    if (next == m_settings.mode)
    {
        return true; // no change, no wear
    }
    persistIfChangedU8(kAddrMode, m_settings.mode, next);
    m_settings.mode = next;
    return true;
}

bool EEPROMManager::setMoistureThreshold(uint8_t percent)
{
    if (percent < kMinMoisturePercent || percent > kMaxMoisturePercent)
    {
        return false;
    }
    persistIfChangedU8(kAddrMoistureThreshold, m_settings.moistureThreshold, percent);
    m_settings.moistureThreshold = percent;
    return true;
}

bool EEPROMManager::setTimerInterval(uint16_t minutes)
{
    if (minutes < kMinTimerIntervalMin || minutes > kMaxTimerIntervalMin)
    {
        return false;
    }
    persistIfChangedU16(kAddrTimerInterval, m_settings.timerInterval, minutes);
    m_settings.timerInterval = minutes;
    return true;
}

bool EEPROMManager::setMoisturePumpDuration(uint8_t seconds)
{
    if (seconds < kMinPumpDurationSec || seconds > kMaxPumpDurationSec)
    {
        return false;
    }
    persistIfChangedU8(kAddrMoisturePumpDuration, m_settings.moisturePumpDuration, seconds);
    m_settings.moisturePumpDuration = seconds;
    return true;
}

bool EEPROMManager::setTimerPumpDuration(uint8_t seconds)
{
    if (seconds < kMinPumpDurationSec || seconds > kMaxPumpDurationSec)
    {
        return false;
    }
    persistIfChangedU8(kAddrTimerPumpDuration, m_settings.timerPumpDuration, seconds);
    m_settings.timerPumpDuration = seconds;
    return true;
}

bool EEPROMManager::setTankCapacity(uint16_t millilitres)
{
    if (millilitres < kMinTankCapacityMl || millilitres > kMaxTankCapacityMl)
    {
        return false;
    }
    if (millilitres == m_settings.tankCapacity)
    {
        // No change to capacity; SRS §9.4 says remaining=capacity when capacity
        // changes, so when it is unchanged the estimate is untouched too.
        return true;
    }
    persistIfChangedU16(kAddrTankCapacity, m_settings.tankCapacity, millilitres);
    m_settings.tankCapacity = millilitres;
    // SRS §9.4: changing tank capacity resets the remaining-water estimate to
    // the new capacity (the reservoir is presumed filled when re-sized).
    persistIfChangedU16(kAddrRemainingWater, m_settings.remainingWater, millilitres);
    m_settings.remainingWater = millilitres;
    return true;
}

bool EEPROMManager::setRemainingWater(uint16_t millilitres)
{
    // INTENTIONAL ASYMMETRY (do NOT "fix" this to reject):
    // Every other setter in this class rejects out-of-range input with false
    // (design A14 - no silent clamping for user-entered values). This setter
    // is the ONE exception because it is NOT user input - it is an internal
    // estimate maintained by ModeManager after each watering cycle, derived
    // from (pumpFlowRate * duration) and subtraction. Clamping here is the
    // documented contract: the estimate is bounded by tankCapacity naturally
    // (the tank cannot hold more than its configured capacity, and uint16_t
    // underflow is prevented by ModeManager's pre-subtract range check). A
    // future maintainer reading A14 and finding a silent clamp should not
    // "normalize" this setter to return false - the API contract here is
    // deliberately different from the user-facing setters.
    if (m_settings.tankCapacity == 0)
    {
        return false;
    }
    if (millilitres > m_settings.tankCapacity)
    {
        millilitres = m_settings.tankCapacity;
    }
    // millilitres is uint16_t so "below zero" is impossible; the earlier
    // checks for kMinTankCapacityMl (==1) protect against an explicitly-zero
    // estimate being persisted, which would defeat low-water detection.
    persistIfChangedU16(kAddrRemainingWater, m_settings.remainingWater, millilitres);
    m_settings.remainingWater = millilitres;
    return true;
}

bool EEPROMManager::setDryCal(uint16_t adc)
{
    if (adc > kMaxAdc)
    {
        return false;
    }
    persistIfChangedU16(kAddrDryCal, m_settings.dryCal, adc);
    m_settings.dryCal = adc;
    return true;
}

bool EEPROMManager::setWetCal(uint16_t adc)
{
    if (adc > kMaxAdc)
    {
        return false;
    }
    persistIfChangedU16(kAddrWetCal, m_settings.wetCal, adc);
    m_settings.wetCal = adc;
    return true;
}

void EEPROMManager::resetToFactoryDefaults()
{
    // Populate the mirror with factory defaults from Config.h. Note: remaining
    // water is initialized to the tank capacity (design A5 / first-boot fills
    // the tank to its configured maximum).
    m_settings.mode                 = (uint8_t)kDefaultOperatingMode;
    m_settings.moistureThreshold    = kDefaultMoistureThreshold;
    m_settings.timerInterval        = kDefaultTimerInterval;
    m_settings.moisturePumpDuration = kDefaultMoisturePumpDuration;
    m_settings.timerPumpDuration    = kDefaultTimerPumpDuration;
    m_settings.tankCapacity         = kDefaultTankCapacity;
    m_settings.remainingWater       = kDefaultTankCapacity;
    m_settings.dryCal               = kDefaultDryCal;
    m_settings.wetCal               = kDefaultWetCal;
    writeDefaultsToEeprom();
}

// ---------------------------------------------------------------------------
// Private persistence helpers
// ---------------------------------------------------------------------------

uint16_t EEPROMManager::readU16(uint16_t addr) const
{
    // EEPROM.get<uint16_t> reads exactly 2 bytes from `addr` in the platform's
    // native byte order (little-endian on AVR), which matches the way writeU16
    // stores values. No manual byte splitting needed (design Q19).
    uint16_t v = 0;
    EEPROM.get(addr, v);
    return v;
}

void EEPROMManager::writeU16(uint16_t addr, uint16_t value)
{
    EEPROM.put(addr, value);
    // Commit is implicit on AVR (writes happen immediately via the EEPROM
    // controller), no EEPROM.commit() call needed for ATmega328P.
}

void EEPROMManager::writeU8(uint16_t addr, uint8_t value)
{
    EEPROM.write(addr, value);
}

void EEPROMManager::loadFromEeprom()
{
    // 1-byte fields (use EEPROM.read directly to avoid the template overhead
    // of EEPROM.get for a single byte).
    m_settings.mode                 = EEPROM.read(kAddrMode);
    m_settings.moistureThreshold    = EEPROM.read(kAddrMoistureThreshold);
    m_settings.moisturePumpDuration = EEPROM.read(kAddrMoisturePumpDuration);
    m_settings.timerPumpDuration    = EEPROM.read(kAddrTimerPumpDuration);

    // 2-byte fields via the helper (uses EEPROM.get).
    m_settings.timerInterval  = readU16(kAddrTimerInterval);
    m_settings.tankCapacity   = readU16(kAddrTankCapacity);
    m_settings.remainingWater = readU16(kAddrRemainingWater);
    m_settings.dryCal         = readU16(kAddrDryCal);
    m_settings.wetCal         = readU16(kAddrWetCal);
}

void EEPROMManager::sanitizeLoaded()
{
    // Replace any single out-of-range field with its factory default. The
    // sanitized value lives in RAM; EEPROM is NOT rewritten here (the user
    // can notice and explicitly save, or the next setter persists it).
    if (m_settings.mode != (uint8_t)MODE_MOISTURE &&
        m_settings.mode != (uint8_t)MODE_TIMER)
    {
        m_settings.mode = (uint8_t)kDefaultOperatingMode;
    }
    if (m_settings.moistureThreshold < kMinMoisturePercent ||
        m_settings.moistureThreshold > kMaxMoisturePercent)
    {
        m_settings.moistureThreshold = kDefaultMoistureThreshold;
    }
    if (m_settings.timerInterval < kMinTimerIntervalMin ||
        m_settings.timerInterval > kMaxTimerIntervalMin)
    {
        m_settings.timerInterval = kDefaultTimerInterval;
    }
    if (m_settings.moisturePumpDuration < kMinPumpDurationSec ||
        m_settings.moisturePumpDuration > kMaxPumpDurationSec)
    {
        m_settings.moisturePumpDuration = kDefaultMoisturePumpDuration;
    }
    if (m_settings.timerPumpDuration < kMinPumpDurationSec ||
        m_settings.timerPumpDuration > kMaxPumpDurationSec)
    {
        m_settings.timerPumpDuration = kDefaultTimerPumpDuration;
    }
    if (m_settings.tankCapacity < kMinTankCapacityMl ||
        m_settings.tankCapacity > kMaxTankCapacityMl)
    {
        m_settings.tankCapacity = kDefaultTankCapacity;
    }
    // Remaining water is an estimate; if it exceeds the (possibly sanitized)
    // capacity, clamp it down. If it is 0 with a non-zero capacity, leave it
    // (genuine low-water boot path - design A11).
    if (m_settings.remainingWater > m_settings.tankCapacity)
    {
        m_settings.remainingWater = m_settings.tankCapacity;
    }
    if (m_settings.dryCal > kMaxAdc)
    {
        m_settings.dryCal = kDefaultDryCal;
    }
    if (m_settings.wetCal > kMaxAdc)
    {
        m_settings.wetCal = kDefaultWetCal;
    }
}

void EEPROMManager::writeDefaultsToEeprom()
{
    // Single-pass write of every field from the mirror (which resetToFactoryDefaults
    // has just populated with factory values). Uses the same per-field helpers
    // as the live setters - no special "bulk" path needed on a 1 KB EEPROM.
    writeU8(kAddrMode,                  m_settings.mode);
    writeU8(kAddrMoistureThreshold,     m_settings.moistureThreshold);
    writeU16(kAddrTimerInterval,        m_settings.timerInterval);
    writeU8(kAddrMoisturePumpDuration,  m_settings.moisturePumpDuration);
    writeU8(kAddrTimerPumpDuration,     m_settings.timerPumpDuration);
    writeU16(kAddrTankCapacity,         m_settings.tankCapacity);
    writeU16(kAddrRemainingWater,       m_settings.remainingWater);
    writeU16(kAddrDryCal,               m_settings.dryCal);
    writeU16(kAddrWetCal,               m_settings.wetCal);
    writeSignature();
}

void EEPROMManager::writeSignature()
{
    writeU16(kAddrSignature, kEepromSignature);
}

void EEPROMManager::persistIfChangedU8(uint16_t addr, uint8_t current, uint8_t next)
{
    if (next != current)
    {
        writeU8(addr, next);
    }
}

void EEPROMManager::persistIfChangedU16(uint16_t addr, uint16_t current, uint16_t next)
{
    if (next != current)
    {
        writeU16(addr, next);
    }
}