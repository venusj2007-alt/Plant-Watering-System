/**
 * @file  EEPROMManager.h
 * @purpose Sole owner of persistent configuration storage and the canonical
 *         in-RAM mirror of all settings (SystemSettings).
 * @layer Logic
 *
 * Single responsibility: read/write all persisted configuration through one
 * controlled interface, validate values defensively, verify the EEPROM
 * signature, and supply factory defaults on first boot / corruption.
 *
 * Per SRS §5.3 Rule 4: NO other module may access <EEPROM.h> directly - all
 * persistence flows through EEPROMManager. Per design A13 the EEPROM signature
 * is an internal concern of this class and is NOT a member of SystemSettings.
 * Per design A14: setters defensively reject out-of-range values (return false)
 * and do NOT silently clamp; the previously stored value is left unchanged.
 *
 * May access: Arduino <EEPROM.h> (put/get) directly (per design Q3).
 * Accessed by: Application (boot), ModeManager, MenuManager, StatusManager,
 *              SensorManager (calibration via const-ref to settings()).
 */
#ifndef EEPROM_MANAGER_H
#define EEPROM_MANAGER_H

#include <Arduino.h>
#include <EEPROM.h>
#include <stdint.h>

#include "Config.h"
#include "Constants.h"
#include "SystemTypes.h"

/**
 * @class EEPROMManager
 * @brief Owns the SystemSettings RAM mirror and mediates all EEPROM I/O.
 *
 * Lifecycle:
 *   1. Construct (mirror is zero-initialized).
 *   2. begin() reads the signature; if valid, loads+validates all fields into
 *      the mirror; if invalid, writes the factory defaults + signature.
 *   3. Other modules read through settings() and mutate through the setters.
 *
 * EEPROM writes occur ONLY when a setter accepts a new value that differs from
 * the cached one, minimizing wear (SRS §2.12 / §20).
 */
class EEPROMManager
{
public:
    /**
     * @brief Construct with a zero-initialized mirror.
     */
    EEPROMManager();

    /**
     * @brief Validate EEPROM signature, load settings (or write factory
     *        defaults + signature on first boot / corruption).
     * @return true if a valid signature was found and existing config loaded;
     *         false if defaults had to be (re)written.
     */
    bool begin();

    /**
     * @brief Read-only access to the canonical settings mirror.
     *        Other modules MUST treat this as immutable.
     * @return Const reference to the in-RAM SystemSettings.
     */
    const SystemSettings& settings() const;

    /**
     * @brief Tell whether the last begin() found a valid signature.
     * @return true if the EEPROM contained a valid 0x55AA signature.
     */
    bool isSignatureValid() const;

    // --- Mutators (each validates defensively; returns acceptance) -------------

    /**
     * @brief Set the active operating mode (0 = Moisture, 1 = Timer).
     * @return true if accepted and persisted; false if invalid (no change).
     */
    bool setMode(OperatingMode mode);

    /**
     * @brief Set the moisture threshold (0-100 %).
     * @return true if accepted and persisted; false if out of range.
     */
    bool setMoistureThreshold(uint8_t percent);

    /**
     * @brief Set the watering interval in minutes (1-999). Timer Mode only.
     * @return true if accepted and persisted; false if out of range.
     */
    bool setTimerInterval(uint16_t minutes);

    /**
     * @brief Set the moisture-mode pump duration in seconds (1-60).
     * @return true if accepted and persisted; false if out of range.
     */
    bool setMoisturePumpDuration(uint8_t seconds);

    /**
     * @brief Set the timer-mode pump duration in seconds (1-60).
     * @return true if accepted and persisted; false if out of range.
     */
    bool setTimerPumpDuration(uint8_t seconds);

    /**
     * @brief Set the configured tank capacity in millilitres (1-9999).
     *        Per SRS §9.4 the remaining-water estimate is also reset to the new
     *        capacity when capacity changes.
     * @return true if accepted and persisted; false if out of range.
     */
    bool setTankCapacity(uint16_t millilitres);

    /**
     * @brief Set the estimated remaining water in millilitres. The value is
     *        clamped to [0, tankCapacity] before persistence.
     * @return true if accepted and persisted; false if tankCapacity is zero.
     */
    bool setRemainingWater(uint16_t millilitres);

    /**
     * @brief Set the dry-calibration raw ADC value (0-1023).
     * @return true if accepted and persisted; false if out of range.
     */
    bool setDryCal(uint16_t adc);

    /**
     * @brief Set the wet-calibration raw ADC value (0-1023).
     * @return true if accepted and persisted; false if out of range.
     */
    bool setWetCal(uint16_t adc);

    /**
     * @brief Reset the full mirror to factory defaults from Config.h and write
     *        defaults + signature to EEPROM. Used for first-boot and recovery.
     */
    void resetToFactoryDefaults();

    // -----------------------------------------------------------------------
    // No public constants / structs / enums declared here (see Config/Constants).
    // -----------------------------------------------------------------------
private:
    /** Canonical RAM mirror of persisted settings (the system's source of truth). */
    SystemSettings m_settings;

    /** Result of the last signature validation. */
    bool m_signatureValid;

    // --- private persistence helpers (use EEPROM.put / EEPROM.get) -------------

    /** Read a little-endian uint16_t from EEPROM at addr. */
    uint16_t readU16(uint16_t addr) const;

    /** Write a little-endian uint16_t to EEPROM at addr. */
    void     writeU16(uint16_t addr, uint16_t value);

    /** Write a single byte to EEPROM at addr. */
    void     writeU8(uint16_t addr, uint8_t value);

    /** Populate m_settings from EEPROM (assumes signature already validated). */
    void     loadFromEeprom();

    /** Validate + sanitize every field loaded from EEPROM against Config ranges. */
    void     sanitizeLoaded();

    /** Write the factory-default mirror + signature to EEPROM in one pass. */
    void     writeDefaultsToEeprom();

    /** Write the whole signature (0x55AA) to its EEPROM address. */
    void     writeSignature();

    /** Persist a single uint8_t field only if it differs from the cached value. */
    void     persistIfChangedU8(uint16_t addr, uint8_t current, uint8_t next);

    /** Persist a single uint16_t field only if it differs from the cached value. */
    void     persistIfChangedU16(uint16_t addr, uint16_t current, uint16_t next);
};

#endif // EEPROM_MANAGER_H