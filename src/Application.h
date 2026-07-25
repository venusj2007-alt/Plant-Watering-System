/**
 * @file  Application.h
 * @purpose System coordinator / composition root. Owns every HAL object and
 *         every module instance, wires collaborators, and runs the per-loop
 *         dispatch sequence defined in SRS §9.1.
 * @layer Application (top-level coordinator)
 *
 * Single responsibility: construct, wire, and schedule. Contains NO business
 * logic and NO direct hardware access (the only exception is the 2 s splash
 * delay, explicitly permitted by the SRS golden constraint, executed via
 * delay() inside Application::begin()).
 *
 * Per-loop sequence (SRS §9.1), all non-blocking:
 *   1. SensorManager.tick()       -- update moisture (background sampling)
 *   2. ModeManager.tick()         -- (internally ticks PumpController, evaluates
 *                                     watering, handles cycle finish / low-water)
 *   3. MenuManager.tick()         -- poll keypad, run UI FSM, render LCD
 *   4. StatusIndicator.tick()     -- drive red LED from operational state
 *
 * Construction order (declaration order in this header) MUST match the dependency
 * order so the member initializer list wires references to already-constructed
 * members.
 *
 * May access: every owned module (all of lib/). Accessed by: main.cpp only.
 */
#ifndef APPLICATION_H
#define APPLICATION_H

#include <Arduino.h>

#include "Config.h"
#include "Constants.h"
#include "PinDefinitions.h"
#include "SystemTypes.h"
#include "Version.h"

#include "EEPROMManager.h"
#include "KeypadManager.h"
#include "LCDManager.h"
#include "MenuManager.h"
#include "ModeManager.h"
#include "PumpController.h"
#include "SensorManager.h"
#include "StatusIndicator.h"
#include "StatusManager.h"

#include "AnalogInput.h"   // HAL
#include "DigitalOutput.h" // HAL

/**
 * @class Application
 * @brief Composition root and main-loop scheduler.
 */
class Application
{
public:
    /**
     * @brief Construct the application and wire every collaborator. Member
     *        construction happens in declaration order (see members below);
     *        the initializer list passes the right references to each module.
     */
    Application();

    /**
     * @brief Initialize all subsystems in a safe, dependency-correct order:
     *          1. Force the pump gate LOW (PumpController::begin) BEFORE any
     *             other subsystem is touched (SRS §12 safety-on-startup).
     *          2. EEPROMManager::begin()       -- load / validate / default.
     *          3. LCDManager::begin() + splash + delay(2000) (only allowed delay).
     *          4. KeypadManager::begin().
     *          5. SensorManager::begin(settings).
     *          6. ModeManager::begin()          -- arm timers, set initial opState.
     *          7. StatusManager::begin().
     *          8. StatusIndicator::begin()      -- red LED OFF.
     *          9. MenuManager::begin()          -- pick initial UI state.
     *        No watering occurs during begin() (SRS §14).
     */
    void begin();

    /**
     * @brief Execute one main-loop iteration (all non-blocking). See header
     *        comment for the per-loop sequence. Called from Arduino loop().
     */
    void tick();

private:
    // --- HAL (owned; bound to physical pins from PinDefinitions.h) ---------
    /** Analog input bound to the soil-moisture sensor (pin A0). */
    AnalogInput   m_moistureInput;
    /** Digital output bound to the pump MOSFET gate (pin D10). */
    DigitalOutput m_mosfetGate;
    /** Digital output bound to the green "pump active" LED (pin D11). */
    DigitalOutput m_greenLed;
    /** Digital output bound to the red status LED (pin D12). */
    DigitalOutput m_redLed;

    // --- Logic: storage ----------------------------------------------------
    /** Owner of the canonical SystemSettings mirror + EEPROM I/O. */
    EEPROMManager m_eeprom;

    // --- Service: input/output -------------------------------------------
    /** 4x4 matrix keypad scanner. */
    KeypadManager m_keypad;
    /** 16x2 I2C LCD renderer. */
    LCDManager    m_lcd;

    // --- Service: sensor + actuator --------------------------------------
    /** Soil-moisture sampler + filter; bound to m_moistureInput. */
    SensorManager m_sensor;
    /** Pump + green-LED driver; bound to m_mosfetGate and m_greenLed. */
    PumpController m_pump;

    // --- Logic: operational engine --------------------------------------
    /** Background watering engine; bound to m_eeprom/m_sensor/m_pump. */
    ModeManager   m_mode;

    // --- Logic: status aggregation --------------------------------------
    /** Read-only status aggregator; bound to m_eeprom/m_sensor/m_pump/m_mode. */
    StatusManager m_status;

    // --- Service: indicator --------------------------------------------
    /** Red status LED driver; bound to m_redLed/m_mode/m_sensor. */
    StatusIndicator m_indicator;

    // --- Logic: UI FSM -------------------------------------------------
    /** User-interface FSM; bound to m_lcd/m_keypad/m_status/m_eeprom/m_mode. */
    MenuManager   m_menu;
};

#endif // APPLICATION_H