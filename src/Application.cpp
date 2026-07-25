/**
 * @file  Application.cpp
 * @purpose Implementation of the system coordinator / composition root.
 * @layer Application (top-level coordinator)
 *
 * Single responsibility: construct, wire, and schedule. No business logic, no
 * direct hardware access except for the explicitly-permitted 2 s splash
 * delay() (Golden Constraint). The initialization order in begin() follows
 * SRS §14 verbatim, with the additional pump-off-first safety rule of SRS §12.
 *
 * Member construction order in the initializer list is REQUIRED to match the
 * member declaration order in Application.h so that references passed between
 * members always point to already-constructed objects. The order is:
 *   HAL inputs / outputs -> Logic storage (EEPROMManager) -> Service I/O
 *   (KeypadManager / LCDManager) -> Service sensor+actuator (SensorManager /
 *   PumpController) -> Logic engine (ModeManager) -> Logic aggregator
 *   (StatusManager) -> Service indicator (StatusIndicator) -> Logic UI FSM
 *   (MenuManager).
 *
 * Per-loop dispatch (SRS §9.1), all non-blocking:
 *   1. m_sensor.tick()    -- advance non-blocking moisture sampling cadence
 *   2. m_mode.tick()     -- manage watering cycles / cooldown / low-water
 *                            (internally calls m_pump.tick() while watering)
 *   3. m_menu.tick()     -- poll keypad, run UI FSM, render LCD if changed
 *   4. m_indicator.tick() -- drive red LED from current system status
 *
 * No new features and no architectural changes: every collaborator method
 * invoked below was already specified in Phase 3 and implemented in Phase 5.
 */

#include "Application.h"

#include <Arduino.h>

Application::Application()
    // HAL (no dependencies at construction; pins bound later in begin()).
    : m_moistureInput()
    , m_mosfetGate()
    , m_greenLed()
    , m_redLed()

    // Logic: storage (no constructor args).
    , m_eeprom()

    // Service: input/output (no constructor args; libraries wired internally).
    , m_keypad()
    , m_lcd()

    // Service: sensor bound to m_moistureInput; settings bound later in begin().
    , m_sensor(m_moistureInput)

    // Service: pump bound to m_mosfetGate (pump MOSFET) + m_greenLed (pump LED).
    , m_pump(m_mosfetGate, m_greenLed)

    // Logic: watering engine bound to storage + sensor + pump.
    , m_mode(m_eeprom, m_sensor, m_pump)

    // Logic: status aggregator bound to its four data sources.
    , m_status(m_eeprom, m_sensor, m_pump, m_mode)

    // Service: red status LED driver. Bound to m_redLed (HAL),
    // m_mode (opState), m_sensor (fault flag). This is the one explicit upward
    // dependency in the layering (design A12), approved because the indicator
    // is a leaf driver and introduces no cycles.
    , m_indicator(m_redLed, m_mode, m_sensor)

    // Logic: UI FSM - topmost collaborator; depends on all of the above.
    , m_menu(m_lcd, m_keypad, m_status, m_eeprom, m_mode)
{
}

void Application::begin()
{
    // (1) SAFETY-ON-STARTUP (SRS §12): force the pump gate LOW BEFORE any other
    // subsystem is initialized. The green LED is also forced OFF so that
    // nothing can be confused for an active cycle during boot. Bind the HAL
    // pin numbers here (not in the constructor) because pinMode must run after
    // the Arduino runtime is ready.
    m_mosfetGate.begin(kPinPumpGate);
    m_greenLed.begin(kPinGreenLed);
    m_redLed.begin(kPinRedLed);
    m_moistureInput.begin(kPinSoilMoisture);
    m_pump.begin();            // forces gate + green LED LOW
    m_redLed.setLow();         // defensive: red LED off until first indicator tick

    // (2) EEPROM: load / validate / write factory defaults on first boot.
    // m_eeprom.begin() populates the canonical SystemSettings mirror that the
    // rest of the system reads back through EEPROMManager::settings().
    m_eeprom.begin();

    // (3) LCD: init + splash. The splash-screen delay() is the ONLY delay()
    // permitted anywhere in the firmware (SRS Golden Constraint, explicit).
    m_lcd.begin();
    m_lcd.showSplash();
    delay(kSplashScreenMs);  // 2000 ms; the sole sanctioned blocking wait.

    // (4) Keypad library initialization (pins configured internally by the
    // Keypad library's constructor; begin() is a lifecycle no-op kept for
    // symmetry but still required by the contract).
    m_keypad.begin();

    // (5) Sensor: bind the live settings mirror (calibration values) and reset
    // the filter buffer. Must run AFTER EEPROMManager::begin() so calibration
    // constants are already loaded.
    m_sensor.begin(m_eeprom.settings());

    // (6) ModeManager: arm mode-appropriate timers and pick the initial
    // OperationalState. Must run AFTER EEPROMManager (reads settings),
    // SensorManager (uses moisture), and PumpController (drives pump). If the
    // persisted remaining-water estimate is below the low-water threshold,
    // ModeManager begins in OP_LOW_WATER (design A11).
    m_mode.begin();

    // (7) StatusManager holds no internal state beyond wiring; the call is a
    // lifecycle symmetry marker (begin() is empty in the current impl).
    m_status.begin();

    // (8) StatusIndicator: drive the red LED OFF at boot (the first tick()
    // immediately evaluates the real operational state). Must run AFTER
    // ModeManager + SensorManager so it can poll them.
    m_indicator.begin();

    // (9) MenuManager: pick the initial UI state from ModeManager's
    // OperationalState - STATE_LOW_WATER if low-water at boot (design A11),
    // STATE_HOME otherwise. Must run AFTER LCDManager.begin() and the splash
    // so the LCD is ready to render the initial screen.
    m_menu.begin();

    // SRS §14: no watering occurs during startup. PumpController is idle, the
    // FSM is positioned, and the system is ready for the main loop.
}

void Application::tick()
{
    // Per-loop scheduler (SRS §9.1). Order matters for correctness, not for
    // responsiveness: every step is non-blocking and completes in O(1).
    //
    //   1. Sensor sampling must lead mode eval so the decision step uses the
    //      freshest moisture reading.
    //   2. Mode manager decides whether to start/continue/finish a watering
    //      cycle; while OP_WATERING it ticks PumpController internally.
    //   3. Menu manager polls the keypad and runs the UI FSM. It also observes
    //      ModeManager's OperationalState and switches UI screen accordingly
    //      (STATE_WATERING only when the cycle starts while at STATE_HOME,
    //      STATE_LOW_WATER takeover on safety condition).
    //   4. Status indicator drives the red LED from the freshly-updated
    //      OperationalState + SensorManager fault flag.
    m_sensor.tick();
    m_mode.tick();
    m_menu.tick();
    m_indicator.tick();
}