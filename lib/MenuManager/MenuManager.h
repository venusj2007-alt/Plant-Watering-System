/**
 * @file  MenuManager.h
 * @purpose Drive the user-interface Finite-State Machine: render screens,
 *         parse keypad input, validate entries, persist settings, and navigate.
 * @layer Logic
 *
 * Single responsibility: own and run the UI FSM. NEVER directly controls the
 * pump (that is ModeManager / PumpController). MenuManager polls
 * ModeManager::getOperationalState() and getOperationalState()'s watering/
 * low-water conditions to decide which UI screen to show, and performs all
 * numeric entry, validation, and EEPROM writes through EEPROMManager (design Q7,
 * A14). Temporary message screens ("Settings Saved", "Invalid Value / Try
 * Again") are implemented as timed sub-state logic over millis() - never delay()
 * (SRS §2.14 golden constraint).
 *
 * Background watering rule (SRS §9/§12 + design Q12/Q13): a watering cycle
 * triggered while the UI is in any menu state (NOT STATE_HOME) runs in the
 * background; the LCD stays on the current menu and is NOT auto-switched to
 * STATE_WATERING. STATE_WATERING is a VISUAL UI state shown only when watering
 * is initiated while the UI is at STATE_HOME.
 *
 * Low-water takeover (SRS §16 + design A4): when ModeManager reports
 * OP_LOW_WATER, the UI switches to STATE_LOW_WATER regardless of the current
 * menu - low-water is a safety condition that interrupts configuration. The
 * partial user input is discarded and the user must re-enter after refilling.
 *
 * EEPROM write policy (SRS §2.5/§2.6 + Q11): threshold/interval/durations are
 * persisted on their own `#`-confirm; the operating mode is set at the end of
 * the flow. The "Settings Saved" message is shown for kMessageScreenMs (3 s)
 * for every successful save - including tank refill via C-key or tank menu -
 * then returns to the Home screen (design A5, Q11).
 *
 * Per SRS §8.2 layering, MenuManager (Logic) may depend on Service (LCDManager,
 * KeypadManager) and Logic (StatusManager, EEPROMManager, ModeManager,
 * TimerManager). It does NOT access hardware and does NOT depend on
 * PumpController or SensorManager directly.
 *
 * May access: LCDManager, KeypadManager, StatusManager, EEPROMManager,
 *             ModeManager, TimerManager (SoftwareTimer), Config, Constants,
 *             SystemTypes.
 * Accessed by: Application (tick only).
 */
#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include <Arduino.h>
#include <stdint.h>

#include "Config.h"
#include "Constants.h"
#include "EEPROMManager.h"
#include "LCDManager.h"
#include "KeypadManager.h"
#include "ModeManager.h"
#include "StatusManager.h"
#include "TimerManager.h"  // SoftwareTimer
#include "SystemTypes.h"

/**
 * @class MenuManager
 * @brief Owns and runs the UI FSM for the Plant Watering System.
 */
class MenuManager
{
public:
    /**
     * @brief Construct the menu FSM wired to its service/logic collaborators.
     */
    MenuManager(LCDManager&    lcd,
                KeypadManager& keypad,
                StatusManager& status,
                EEPROMManager& eeprom,
                ModeManager&  mode);

    /**
     * @brief Pick the initial UI state from ModeManager's operational state:
     *        OP_LOW_WATER -> STATE_LOW_WATER (design A11), otherwise STATE_HOME.
     *        Must be called after ModeManager::begin() and LCDManager::begin().
     */
    void begin();

    /**
     * @brief Advance the UI FSM by one loop iteration. Polls the keypad, runs
     *        the active state's logic, performs any transition, and updates the
     *        LCD. Call every loop; non-blocking.
     */
    void tick();

    /**
     * @brief Current FSM screen state.
     * @return The active SystemState.
     */
    SystemState getCurrentState() const;

private:
    // --- injected collaborators --------------------------------------------
    LCDManager&    m_lcd;
    KeypadManager& m_keypad;
    StatusManager& m_status;
    EEPROMManager& m_eeprom;
    ModeManager&   m_mode;

    // --- FSM state ---------------------------------------------------------
    SystemState m_state;

    /** State to return to once the "Invalid Value / Try Again" message expires. */
    SystemState m_invalidReturnState;

    /** True while the "Invalid Value" transient message is being displayed. */
    bool        m_inInvalidMessage;

    // --- numeric-entry buffer ---------------------------------------------
    char        m_input[kInputBufferMax];
    uint8_t     m_inputLen;

    /** Max digits accepted in the current input state (field-dependent). */
    uint8_t     m_maxInputDigits;

    // --- owned timers (shared message timer + status-page timer) ----------
    /** Timer for "Settings Saved" and "Invalid Value" transient messages. */
    SoftwareTimer m_messageTimer;
    /** Auto-advance timer for the status screen pages. */
    SoftwareTimer m_pageTimer;

    /** Active status-screen page index (1..3). */
    uint8_t     m_statusPage;

    /** Last key returned by KeypadManager (debounced). */
    char        m_lastKey;

    // --- per-state handlers (dispatched from tick) -------------------------
    void tickHome(char key);
    void tickMainMenu(char key);
    void tickMoistureThreshold(char key);
    void tickMoistureDuration(char key);
    void tickTimerInterval(char key);
    void tickTimerDuration(char key);
    void tickSaveSettings();
    void tickStatus(char key);
    void tickWatering();
    void tickLowWater(char key);
    void tickTankMenu(char key);
    void tickRefillConfirm(char key);
    void tickSetTankCapacity(char key);

    // --- FSM plumbing -----------------------------------------------------
    /**
     * @brief Perform a state transition: set m_state, clear the invalid-message
     *        flag, reset the input buffer, and run the new state's on-entry
     *        render (incl. cursor enable/disable and timer (re)start).
     */
    void gotoState(SystemState nextState);

    /** Reset the numeric-entry buffer to empty. */
    void resetInput();

    /**
     * @brief Append a digit to the input buffer (if within the field's max
     *        digit cap) and refresh the LCD input field. Overflow digits are
     *        silently ignored (SRS §17 / design A15).
     */
    void addDigit(char digit);

    /** Delete the last entered digit; refresh the LCD input field. */
    void deleteLastDigit();

    /**
     * @brief Parse the input buffer to an unsigned integer.
     * @param out  Output parsed value.
     * @return true if the buffer is non-empty; false if empty.
     */
    bool parseUint(uint32_t& out) const;

    /**
     * @brief Start showing the "Invalid Value / Try Again" message for
     *        kMessageScreenMs; on expiry the FSM returns to returnState.
     */
    void beginInvalidMessage(SystemState returnState);

    /** Refresh the active status-screen page on the LCD (1..3). */
    void renderStatusPage();

    /** Re-render the live-digit prompt for the current input state. Called by
     *  addDigit() and deleteLastDigit() to avoid a duplicated per-state switch. */
    void refreshActivePrompt();

    /** Refill the tank silently and route through STATE_SAVE_SETTINGS. */
    void doTankRefill();
};

#endif // MENU_MANAGER_H