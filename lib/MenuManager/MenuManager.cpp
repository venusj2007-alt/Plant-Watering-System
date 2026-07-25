/**
 * @file  MenuManager.cpp
 * @purpose Implementation of the UI Finite-State Machine driver.
 * @layer Logic
 *
 * Per-loop model (SRS §9.1; everything non-blocking):
 *   tick() poll:
 *     1. Low-water safety takeover (design A4): if ModeManager reports
 *        OP_LOW_WATER from any state except BOOT/LOW_WATER, immediately switch
 *        UI to STATE_LOW_WATER and drop the in-flight tick. Otherwise,
 *     2. Transient "Invalid Value" guard: if currently showing the invalid
 *        message, swallow all keypad input; on the message timer's expiry
 *        return to the previous input state with an empty buffer.
 *     3. Poll the keypad once, then dispatch to the active state's handler.
 *
 * Per-state handlers receive the polled key and:
 *   - Update the numeric entry buffer (digits / * for backspace);
 *   - On '#' perform value validation against the field's range, persist via
 *     EEPROMManager on success (router through STATE_SAVE_SETTINGS for the
 *     "Settings Saved" message), or begin the invalid-value transient;
 *   - On 'D' return to the parent menu level (STATE_MAIN_MENU, or STATE_TANK_MENU
 *     for the tank sub-states); on screen-specific special keys ('A', 'C', '#'...)
 *     trigger the documented action.
 *
 * Background watering rule (SRS §9 end + design Q12/Q13): when ModeManager
 * starts OP_WATERING while the UI is in any menu state (NOT STATE_HOME), the
 * UI is NOT auto-switched to STATE_WATERING. The watering cycle runs in the
 * background. Only tickHome transfers UI -> STATE_WATERING when opState goes
 * OP_WATERING mid-POLL.
 *
 * Low-water takeover vs. STATE_WATERING: STATE_WATERING polls opState and
 * returns to HOME when the cycle ends - including the cycle-completed edge
 * where ModeManager flips to OP_LOW_WATER; the next tick then sees OP_LOW_WATER
 * and the top-of-tick takeover swaps us to STATE_LOW_WATER cleanly.
 *
 * EEPROM persistence (SRS §2.5/§2.6 + design Q11): threshold / interval / the
 * two pump durations / tank capacity are persisted on their own `#`-confirm.
 * The operating mode is set at the END of the moisture duration OR timer
 * duration flow (setMode is called alongside the final setMoisturePumpDuration /
 * setTimerPumpDuration), then the user sees "Settings Saved" for
 * kMessageScreenMs (3 s) and returns to the home screen. The C-key / tank-menu
 * refill route re-uses the same STATE_SAVE_SETTINGS path so the user sees the
 * "Settings Saved" acknowledgment for refill too (design A5).
 */

#include "MenuManager.h"

#include <Arduino.h>

MenuManager::MenuManager(LCDManager&     lcd,
                         KeypadManager&  keypad,
                         StatusManager&  status,
                         EEPROMManager&  eeprom,
                         ModeManager&    mode)
    : m_lcd(lcd)
    , m_keypad(keypad)
    , m_status(status)
    , m_eeprom(eeprom)
    , m_mode(mode)
    , m_state(STATE_BOOT)
    , m_invalidReturnState(STATE_HOME)
    , m_inInvalidMessage(false)
    , m_inputLen(0)
    , m_maxInputDigits(0)
    , m_statusPage(1)
    , m_lastKey(kNoKey)
{
    m_input[0] = '\0';
}

void MenuManager::begin()
{
    m_state              = STATE_BOOT;  // transitional placeholder
    m_inInvalidMessage   = false;
    m_invalidReturnState = STATE_HOME;
    m_statusPage         = 1;
    m_lastKey            = kNoKey;
    resetInput();

    // Pick the initial visible UI from the ModeManager-derived opState so the
    // first screen reflects any low-water boot condition (design A11) - sending
    // the entire transition through gotoState() so the on-entry render + cursor
    // state is set up correctly.
    if (m_mode.getOperationalState() == OP_LOW_WATER)
    {
        gotoState(STATE_LOW_WATER);
    }
    else
    {
        gotoState(STATE_HOME);
    }
}

void MenuManager::tick()
{
    // (1) Low-water safety takeover: pre-empts every menu state regardless of
    // what the user is doing - this is a deliberate edge of design A4 (a safety
    // condition that justifies dropping any partial numeric entry).
    if (m_state != STATE_LOW_WATER && m_state != STATE_BOOT)
    {
        if (m_mode.getOperationalState() == OP_LOW_WATER)
        {
            gotoState(STATE_LOW_WATER);
            return;
        }
    }

    // (2) Transient invalid-message guard: while the "Invalid Value / Try Again"
    // screen is showing, the keypad is locked; on the message timer expiry we
    // transition back to the user's prior input screen with a fresh buffer.
    if (m_inInvalidMessage)
    {
        if (m_messageTimer.isExpired())
        {
            m_inInvalidMessage = false;
            gotoState(m_invalidReturnState);
        }
        return;
    }

    // (3) Poll the keypad once per loop, then run the active state handler.
    char key = m_keypad.poll();
    m_lastKey = key;

    switch (m_state)
    {
        case STATE_BOOT:
            // Unreachable post-begin: Application moves us off STATE_BOOT
            // before tick() fires for the first time.
            break;
        case STATE_HOME:
            tickHome(key);
            break;
        case STATE_MAIN_MENU:
            tickMainMenu(key);
            break;
        case STATE_MOISTURE_THRESHOLD:
            tickMoistureThreshold(key);
            break;
        case STATE_MOISTURE_DURATION:
            tickMoistureDuration(key);
            break;
        case STATE_TIMER_INTERVAL:
            tickTimerInterval(key);
            break;
        case STATE_TIMER_DURATION:
            tickTimerDuration(key);
            break;
        case STATE_SAVE_SETTINGS:
            tickSaveSettings();
            break;
        case STATE_STATUS:
            tickStatus(key);
            break;
        case STATE_WATERING:
            tickWatering();
            break;
        case STATE_LOW_WATER:
            tickLowWater(key);
            break;
        case STATE_TANK_MENU:
            tickTankMenu(key);
            break;
        case STATE_REFILL_CONFIRM:
            tickRefillConfirm(key);
            break;
        case STATE_SET_TANK_CAPACITY:
            tickSetTankCapacity(key);
            break;
        case STATE_ERROR:
        default:
            // Defensive: an unreachable state still renders an error screen.
            m_lcd.showError(F("Unexpected"), F("State"));
            break;
    }
}

SystemState MenuManager::getCurrentState() const
{
    return m_state;
}

// ---------------------------------------------------------------------------
// Per-state handlers
// ---------------------------------------------------------------------------

void MenuManager::tickHome(char key)
{
    // Reflect the background engine's current condition: low-water takes
    // priority over the watering visual (the top-of-tick takeover already
    // redirected STATE_LOW_WATER before we got here, but defensively check
    // anyway). Order: LOW_WATER, WATERING, then user-key actions.
    OperationalState op = m_mode.getOperationalState();
    if (op == OP_LOW_WATER)
    {
        gotoState(STATE_LOW_WATER);
        return;
    }
    if (op == OP_WATERING)
    {
        // Only from STATE_HOME do we render the visual watering screen; the
        // background rule keeps menus undisturbed when watering starts while
        // we're in a non-HOME UI state (SRS §9 end + design Q13).
        gotoState(STATE_WATERING);
        return;
    }

    if (key == 'A')
    {
        gotoState(STATE_MAIN_MENU);
        return;
    }
    if (key == 'C')
    {
        // C-key refill from HOME (SRS §2.8 / §16): silently restore the
        // remaining-water estimate to the configured capacity and show the
        // "Settings Saved" message route (design A5).
        doTankRefill();
        return;
    }
    // All other keys are no-ops on the home screen (SRS §2.10).
}

void MenuManager::tickMainMenu(char key)
{
    switch (key)
    {
        case '1':
            gotoState(STATE_MOISTURE_THRESHOLD);
            break;
        case '2':
            gotoState(STATE_TIMER_INTERVAL);
            break;
        case '3':
            gotoState(STATE_STATUS);
            break;
        case '4':
            gotoState(STATE_TANK_MENU);
            break;
        case 'D':
        case 'A':
            // D returns one level back to HOME; A is conventionally the
            // "open menu" key, accepting it here as a back-to-home gesture
            // is a safe convenience.
            gotoState(STATE_HOME);
            break;
        default:
            break;
    }
}

void MenuManager::tickMoistureThreshold(char key)
{
    if (key >= '0' && key <= '9')
    {
        addDigit(key);
    }
    else if (key == '*')
    {
        deleteLastDigit();
    }
    else if (key == '#')
    {
        uint32_t v = 0;
        // Validate the parsed value against the moisture-threshold range.
        // On accept, persist; on reject (or empty buffer), show the invalid
        // message and ask again from this same screen (SRS §2.5).
        if (parseUint(v) &&
            v >= kMinMoisturePercent && v <= kMaxMoisturePercent)
        {
            m_eeprom.setMoistureThreshold((uint8_t)v);
            gotoState(STATE_MOISTURE_DURATION);
        }
        else
        {
            beginInvalidMessage(STATE_MOISTURE_THRESHOLD);
        }
    }
    else if (key == 'D')
    {
        // "Back one level" to the main menu (SRS §2.4); partial entry lost.
        gotoState(STATE_MAIN_MENU);
    }
}

void MenuManager::tickMoistureDuration(char key)
{
    if (key >= '0' && key <= '9')
    {
        addDigit(key);
    }
    else if (key == '*')
    {
        deleteLastDigit();
    }
    else if (key == '#')
    {
        uint32_t v = 0;
        if (parseUint(v) &&
            v >= kMinPumpDurationSec && v <= kMaxPumpDurationSec)
        {
            m_eeprom.setMoisturePumpDuration((uint8_t)v);
            // End of the Moisture Mode flow: the operating mode is fixed to
            // Moisture here (design Q6), and the "Settings Saved" transient
            // then returns the UI to HOME.
            m_eeprom.setMode(MODE_MOISTURE);
            gotoState(STATE_SAVE_SETTINGS);
        }
        else
        {
            beginInvalidMessage(STATE_MOISTURE_DURATION);
        }
    }
    else if (key == 'D')
    {
        gotoState(STATE_MAIN_MENU);
    }
}

void MenuManager::tickTimerInterval(char key)
{
    if (key >= '0' && key <= '9')
    {
        addDigit(key);
    }
    else if (key == '*')
    {
        deleteLastDigit();
    }
    else if (key == '#')
    {
        uint32_t v = 0;
        if (parseUint(v) &&
            v >= kMinTimerIntervalMin && v <= kMaxTimerIntervalMin)
        {
            m_eeprom.setTimerInterval((uint16_t)v);
            gotoState(STATE_TIMER_DURATION);
        }
        else
        {
            beginInvalidMessage(STATE_TIMER_INTERVAL);
        }
    }
    else if (key == 'D')
    {
        gotoState(STATE_MAIN_MENU);
    }
}

void MenuManager::tickTimerDuration(char key)
{
    if (key >= '0' && key <= '9')
    {
        addDigit(key);
    }
    else if (key == '*')
    {
        deleteLastDigit();
    }
    else if (key == '#')
    {
        uint32_t v = 0;
        if (parseUint(v) &&
            v >= kMinPumpDurationSec && v <= kMaxPumpDurationSec)
        {
            m_eeprom.setTimerPumpDuration((uint8_t)v);
            // End of the Timer Mode flow: lock the operating mode to Timer.
            m_eeprom.setMode(MODE_TIMER);
            gotoState(STATE_SAVE_SETTINGS);
        }
        else
        {
            beginInvalidMessage(STATE_TIMER_DURATION);
        }
    }
    else if (key == 'D')
    {
        gotoState(STATE_MAIN_MENU);
    }
}

void MenuManager::tickSaveSettings()
{
    // Pure timed-screen handler - no keypad role. After kMessageScreenMs the
    // UI returns Home regardless of where the user was prior (this is the
    // converged "every successful save" terminal of the design Q11 spec).
    if (m_messageTimer.isExpired())
    {
        gotoState(STATE_HOME);
    }
}

void MenuManager::tickStatus(char key)
{
    // # OR D both exit to HOME (design Q9). Other keys are no-ops; pages
    // cycle on the page timer without any user input.
    if (key == '#' || key == 'D')
    {
        gotoState(STATE_HOME);
        return;
    }

    // Page auto-advance: 1->2->3->1 every kStatusPageMs (SRS §2.7).
    if (m_pageTimer.isExpired())
    {
        m_statusPage = (m_statusPage % 3u) + 1u;
        m_pageTimer.start(kStatusPageMs);
        renderStatusPage();
    }
}

void MenuManager::tickWatering()
{
    // Visual watering screen: stays as long as ModeManager says OP_WATERING;
    // drops back HOME as soon as the cycle finishes (the cycle-finishing edge
    // may instead trigger LOW_WATER via the top-of-tick takeover on the next
    // iteration, which is the correct precedence).
    if (m_mode.getOperationalState() != OP_WATERING)
    {
        gotoState(STATE_HOME);
    }
    // Keypad intentionally ignored during the watering visual.
}

void MenuManager::tickLowWater(char key)
{
    // 'C' is the documented refill-from-low-water shortcut (SRS §16). It
    // reuses doTankRefill() so the user sees "Settings Saved" before the
    // engine resumes (design A5/A6).
    if (key == 'C')
    {
        doTankRefill();
        return;
    }
    // If the user already refilled via the menu (tank-menu path) the opState
    // will no longer be OP_LOW_WATER; return UI to HOME so the active mode
    // can resume normally.
    if (m_mode.getOperationalState() != OP_LOW_WATER)
    {
        gotoState(STATE_HOME);
    }
    // All other keys are ignored while in low-water takeover.
}

void MenuManager::tickTankMenu(char key)
{
    switch (key)
    {
        case '1':
            gotoState(STATE_REFILL_CONFIRM);
            break;
        case '2':
            gotoState(STATE_SET_TANK_CAPACITY);
            break;
        case 'D':
            // Q10: D from STATE_TANK_MENU goes back one level to STATE_MAIN_MENU.
            gotoState(STATE_MAIN_MENU);
            break;
        default:
            break;
    }
}

void MenuManager::tickRefillConfirm(char key)
{
    if (key == '#')
    {
        // Confirmed: reset the remaining water to the configured capacity and
        // show "Settings Saved" (SRS §2.8 + Q11).
        doTankRefill();
    }
    else if (key == 'D')
    {
        gotoState(STATE_TANK_MENU);
    }
}

void MenuManager::tickSetTankCapacity(char key)
{
    if (key >= '0' && key <= '9')
    {
        addDigit(key);
    }
    else if (key == '*')
    {
        deleteLastDigit();
    }
    else if (key == '#')
    {
        uint32_t v = 0;
        if (parseUint(v) &&
            v >= kMinTankCapacityMl && v <= kMaxTankCapacityMl)
        {
            // EEPROMManager.setTankCapacity writes the new capacity AND resets
            // the remaining-water estimate to the new capacity per SRS §9.4.
            m_eeprom.setTankCapacity((uint16_t)v);
            gotoState(STATE_SAVE_SETTINGS);
        }
        else
        {
            beginInvalidMessage(STATE_SET_TANK_CAPACITY);
        }
    }
    else if (key == 'D')
    {
        // Q10 / SRS §9.4: cancel returns to the tank menu.
        gotoState(STATE_TANK_MENU);
    }
}

// ---------------------------------------------------------------------------
// FSM plumbing
// ---------------------------------------------------------------------------

void MenuManager::gotoState(SystemState nextState)
{
    m_state            = nextState;
    m_inInvalidMessage = false;
    resetInput();

    // On-entry rendering + per-state side effects (cursor, timers, prompts).
    switch (nextState)
    {
        case STATE_HOME:
            m_lcd.showHome();
            m_lcd.disableCursor();
            break;

        case STATE_MAIN_MENU:
            m_lcd.showMainMenu();
            m_lcd.disableCursor();
            break;

        case STATE_MOISTURE_THRESHOLD:
            m_maxInputDigits = kMaxDigitsThreshold;
            m_lcd.showMoistureThresholdPrompt(m_input);
            m_lcd.enableCursor();
            break;

        case STATE_MOISTURE_DURATION:
            m_maxInputDigits = kMaxDigitsPumpDuration;
            m_lcd.showMoistureDurationPrompt(m_input);
            m_lcd.enableCursor();
            break;

        case STATE_TIMER_INTERVAL:
            m_maxInputDigits = kMaxDigitsInterval;
            m_lcd.showTimerIntervalPrompt(m_input);
            m_lcd.enableCursor();
            break;

        case STATE_TIMER_DURATION:
            m_maxInputDigits = kMaxDigitsPumpDuration;
            m_lcd.showTimerDurationPrompt(m_input);
            m_lcd.enableCursor();
            break;

        case STATE_SAVE_SETTINGS:
            m_lcd.showSettingsSaved();
            m_lcd.disableCursor();
            m_messageTimer.start(kMessageScreenMs);
            break;

        case STATE_STATUS:
            // Restart the page cycle from page 1 and render it immediately.
            m_statusPage = 1;
            m_pageTimer.start(kStatusPageMs);
            renderStatusPage();
            m_lcd.disableCursor();
            break;

        case STATE_WATERING:
            m_lcd.showWatering();
            m_lcd.disableCursor();
            break;

        case STATE_LOW_WATER:
            m_lcd.showLowWater();
            m_lcd.disableCursor();
            break;

        case STATE_TANK_MENU:
            m_lcd.showTankMenu();
            m_lcd.disableCursor();
            break;

        case STATE_REFILL_CONFIRM:
            m_lcd.showRefillConfirm();
            m_lcd.disableCursor();
            break;

        case STATE_SET_TANK_CAPACITY:
            m_maxInputDigits = kMaxDigitsTankCapacity;
            m_lcd.showTankCapacityPrompt(m_input);
            m_lcd.enableCursor();
            break;

        case STATE_BOOT:
        case STATE_ERROR:
        default:
            // BOOT is transitional only (Application moves off it immediately);
            // for ERROR / unexpected, render the generic unexpected-state screen.
            m_lcd.showError(F("Unexpected"), F("State"));
            m_lcd.disableCursor();
            break;
    }
}

void MenuManager::resetInput()
{
    m_input[0] = '\0';
    m_inputLen = 0;
}

void MenuManager::addDigit(char digit)
{
    // Per the per-field cap (max digits confirmed in design A15): overflow
    // digits are silently ignored rather than ringing the buffer. The buffer
    // is also defended against the (impossible here) case of kInputBufferMax
    // being too small to hold the configured maximum digits.
    if (m_inputLen >= m_maxInputDigits)
    {
        return;
    }
    if (m_inputLen >= (kInputBufferMax - 1))
    {
        return;
    }
    m_input[m_inputLen]      = digit;
    m_inputLen++;
    m_input[m_inputLen]      = '\0';

    // Re-render the live input digits on the active prompt screen so the user
    // sees the new character appear (SRS §2.5/§2.6 live digit display).
    refreshActivePrompt();
}

void MenuManager::deleteLastDigit()
{
    if (m_inputLen == 0)
    {
        return;
    }
    m_inputLen--;
    m_input[m_inputLen] = '\0';

    // Re-render the (now shorter) live input. The LCDManager's refreshInput
    // helper compares against its cached "last drawn" buffer so a backspace
    // to the same content it last saw (impossible here, but defensive) would
    // not redraw. For our normal case the new content differs from cached so
    // a redraw happens.
    refreshActivePrompt();
}

bool MenuManager::parseUint(uint32_t& out) const
{
    if (m_inputLen == 0)
    {
        return false;
    }
    uint32_t v = 0;
    for (uint8_t i = 0; i < m_inputLen; ++i)
    {
        // We only ever append '0'..'9' into m_input (see addDigit), so the
        // arithmetic below cannot underflow or misread.
        v = (v * 10u) + (uint32_t)(m_input[i] - '0');
    }
    out = v;
    return true;
}

void MenuManager::beginInvalidMessage(SystemState returnState)
{
    // Drop the screen into "Invalid Value / Try Again" for kMessageScreenMs;
    // the next ticks will pass through the transient guard at the top of
    // tick() and re-route us to returnState when the timer expires.
    m_inInvalidMessage   = true;
    m_invalidReturnState = returnState;
    m_lcd.showInvalidValue();
    m_lcd.disableCursor();
    m_messageTimer.start(kMessageScreenMs);
}

void MenuManager::renderStatusPage()
{
    // Pull the live status primitives from StatusManager and pass them to the
    // matching LCDManager renderer. Page 3 splits on the current mode.
    switch (m_statusPage)
    {
        case 1:
            m_lcd.showStatusPage1(m_status.getModeLabel(),
                                  m_status.getRemainingWaterMl());
            break;
        case 2:
            m_lcd.showStatusPage2(m_status.getMoisturePercent(),
                                  m_status.isPumpRunning());
            break;
        case 3:
            if (m_status.getMode() == MODE_MOISTURE)
            {
                m_lcd.showStatusPage3Moisture(
                    m_status.getMoistureThresholdPercent(),
                    m_status.getMoisturePumpDurationSec());
            }
            else
            {
                m_lcd.showStatusPage3Timer(
                    m_status.getTimerIntervalMin(),
                    m_status.getTimerPumpDurationSec());
            }
            break;
        default:
            // Defensive; m_statusPage is bounded to 1..3 everywhere it's set.
            m_statusPage = 1;
            m_lcd.showStatusPage1(m_status.getModeLabel(),
                                  m_status.getRemainingWaterMl());
            break;
    }
}

void MenuManager::doTankRefill()
{
    // Read the current configured capacity from the live mirror and reset the
    // remaining estimate to it via the setter (which persists defensively
    // and clamps to capacity). Then route through the "Settings Saved" UI
    // transient per design A5.
    const SystemSettings& s = m_eeprom.settings();
    m_eeprom.setRemainingWater(s.tankCapacity);
    gotoState(STATE_SAVE_SETTINGS);
}

void MenuManager::refreshActivePrompt()
{
    // Centralized live-digit re-render for the input states. Called by
    // addDigit() and deleteLastDigit(); prevents the duplicated per-state
    // switch from drifting if a new numeric-entry state is added.
    switch (m_state)
    {
        case STATE_MOISTURE_THRESHOLD:
            m_lcd.showMoistureThresholdPrompt(m_input);
            break;
        case STATE_MOISTURE_DURATION:
            m_lcd.showMoistureDurationPrompt(m_input);
            break;
        case STATE_TIMER_INTERVAL:
            m_lcd.showTimerIntervalPrompt(m_input);
            break;
        case STATE_TIMER_DURATION:
            m_lcd.showTimerDurationPrompt(m_input);
            break;
        case STATE_SET_TANK_CAPACITY:
            m_lcd.showTankCapacityPrompt(m_input);
            break;
        default:
            break;
    }
}