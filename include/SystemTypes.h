/**
 * @file  SystemTypes.h
 * @purpose Define shared enumerations and the SystemSettings POD struct used
 *         across all firmware modules.
 * @layer Include (dependency-free)
 *
 * Single responsibility: hold cross-module type definitions only. No logic, no
 * constants, no pin assignments. Must not depend on any other project file.
 */
#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// Operating mode (persisted at EEPROM addr 0)
// ---------------------------------------------------------------------------

/**
 * @enum OperatingMode
 * @brief Identifies which automatic watering strategy is active.
 *        Exactly one mode is active at any moment (SRS §2.15).
 */
enum OperatingMode
{
    /** Soil-moisture-driven watering. */
    MODE_MOISTURE = 0,
    /** Time-interval-driven watering. */
    MODE_TIMER    = 1
};

// ---------------------------------------------------------------------------
// UI Finite-State Machine states (SRS §9.2)
// ---------------------------------------------------------------------------

/**
 * @enum SystemState
 * @brief Every screen of the user interface is one independent FSM state.
 *        Note: STATE_TANK_MENU_TRANSITION mentioned in SRS §9.4 is intentionally
 *        NOT enumerated (it is a transient transition action, per design Q8).
 */
enum SystemState
{
    STATE_BOOT                  = 0,  /**< System startup (executed once). */
    STATE_HOME                  = 1,  /**< Idle home screen. */
    STATE_MAIN_MENU             = 2,  /**< Main menu (1:M 2:T 3:Status 4:Tank). */
    STATE_MOISTURE_THRESHOLD    = 3,  /**< Moisture threshold entry (0-100). */
    STATE_MOISTURE_DURATION     = 4,  /**< Moisture-mode pump duration entry (1-60). */
    STATE_TIMER_INTERVAL        = 5,  /**< Timer interval entry (1-999 min). */
    STATE_TIMER_DURATION        = 6,  /**< Timer-mode pump duration entry (1-60). */
    STATE_SAVE_SETTINGS         = 7,  /**< Timed "Settings Saved" screen (3 s). */
    STATE_STATUS                = 8,  /**< Read-only status (3 auto-advancing pages). */
    STATE_WATERING              = 9,  /**< Visual watering screen (only from HOME). */
    STATE_ERROR                 = 10, /**< Reserved / unexpected state. */
    STATE_LOW_WATER             = 11, /**< Low-water takeover (pump disabled, red LED). */
    STATE_TANK_MENU             = 12, /**< Tank management menu. */
    STATE_REFILL_CONFIRM        = 13, /**< Confirm tank refill. */
    STATE_SET_TANK_CAPACITY     = 14  /**< Enter new tank capacity (1-9999 mL). */
};

// ---------------------------------------------------------------------------
// Operational state owned by ModeManager (background watering engine)
// ---------------------------------------------------------------------------

/**
 * @enum OperationalState
 * @brief High-level system status used by StatusIndicator and MenuManager to
 *        decide what to render / indicate. Owned by ModeManager.
 */
enum OperationalState
{
    OP_NORMAL    = 0,  /**< Idle / monitoring; watering allowed when conditions met. */
    OP_WATERING  = 1,  /**< A watering cycle is currently active. */
    OP_LOW_WATER = 2   /**< Estimated remaining water below threshold; pump disabled. */
};

// ---------------------------------------------------------------------------
// System configuration (persisted in EEPROM; RAM mirror owned by EEPROMManager)
// ---------------------------------------------------------------------------

/**
 * @struct SystemSettings
 * @brief Canonical in-RAM mirror of all persisted configuration fields.
 *        The EEPROM signature is intentionally NOT a member (it is an internal
 *        concern of EEPROMManager - design A13). POD only; no methods.
 */
struct SystemSettings
{
    /** Active operating mode (0 = Moisture, 1 = Timer). */
    uint8_t  mode;

    /** Soil moisture threshold in percent (0-100). */
    uint8_t  moistureThreshold;

    /** Watering interval in minutes (1-999) - used by Timer Mode only. */
    uint16_t timerInterval;

    /** Pump duration in seconds (1-60) - used by Moisture Mode only. */
    uint8_t  moisturePumpDuration;

    /** Pump duration in seconds (1-60) - used by Timer Mode only. */
    uint8_t  timerPumpDuration;

    /** Configured maximum tank capacity in millilitres (1-9999). */
    uint16_t tankCapacity;

    /** Estimated remaining water in millilitres (clamped to tankCapacity). */
    uint16_t remainingWater;

    /** Dry-calibration raw ADC value (mapped to 0 %). */
    uint16_t dryCal;

    /** Wet-calibration raw ADC value (mapped to 100 %). */
    uint16_t wetCal;
};

#endif // SYSTEM_TYPES_H