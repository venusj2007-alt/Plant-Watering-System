/**
 * @file  LCDManager.cpp
 * @purpose Implementation of the flicker-free 16x2 I2C LCD renderer.
 * @layer Service
 *
 * Design notes (SRS §2.11 - "LCD must never flicker"):
 *  - lcd.clear() is invoked ONLY when the screen identity actually changes
 *    (changeScreen). Within a screen, prompt-label text is rewritten only when
 *    comparing the new label would otherwise change visible characters - for
 *    input prompts that's once per screen-enter; subsequent loop iterations
 *    update only the live digit cells via refreshInput().
 *  - All static text lives in Flash via F() to conserve the Nano's 2 KB SRAM
 *    (Golden Constraint 4 / SRS §20).
 *  - Long messages are centered when possible; numeric answer fields and the
 *    "Mode: ..." / "Tank: ..." status lines remain left-aligned per the SRS
 *    examples in §2.7.
 *  - Cursor (underline + blink) is enabled only during numeric entry per
 *    design A18. ensureCursorOff() is called by every non-prompt renderer so
 *    the cursor can never leak into an idle screen.
 *
 * Per design Q3 the LiquidCrystal_I2C + Wire libraries are called directly.
 */

#include "LCDManager.h"

#include <Arduino.h>

LCDManager::LCDManager()
    : m_lcd(kLcdI2cAddress, kLcdColumns, kLcdRows)
    , m_currentScreen(SCREEN_NONE)
    , m_cursorEnabled(false)
{
    m_lastInput[0] = '\0';
}

void LCDManager::begin()
{
    m_lcd.init();
    m_lcd.backlight();
    m_lcd.clear();
    m_lcd.noCursor();
    m_lcd.noBlink();
    m_currentScreen = SCREEN_NONE;
}

// ---------------------------------------------------------------------------
// Boot / home / main menu
// ---------------------------------------------------------------------------

void LCDManager::showSplash()
{
    // Splash shows the project name + a version line built from Version.h so
    // the firmware advertises its own identity without a new manager class
    // (per the approved "show version in splash" suggestion with the explicit
    // "no SplashManager" constraint).
    if (m_currentScreen != SCREEN_SPLASH)
    {
        changeScreen(SCREEN_SPLASH);
        centerPrint(0, F("Plant Watering"));
        // Row 2: "System v<minor>.<patch>" — fits comfortably in 16 columns.
        char versionLine[17];
        snprintf(versionLine, sizeof(versionLine), "System v%u.%u",
                 (unsigned)kVersionMinor, (unsigned)kVersionPatch);
        m_lcd.setCursor((uint8_t)((kLcdColumns - strlen(versionLine)) / 2), 1);
        m_lcd.print(versionLine);
    }
}

void LCDManager::showHome()
{
    if (m_currentScreen != SCREEN_HOME)
    {
        changeScreen(SCREEN_HOME);
        centerPrint(0, F("Plant Watering"));
        centerPrint(1, F("A = Menu"));
    }
}

void LCDManager::showMainMenu()
{
    if (m_currentScreen != SCREEN_MAIN_MENU)
    {
        changeScreen(SCREEN_MAIN_MENU);
        printAt(0, 0, F("1:M 2:T"));
        printAt(0, 1, F("3:Status 4:Tank"));
    }
}

// ---------------------------------------------------------------------------
// Configuration prompts (live input on row 2 per design + the confirmed
// "live input on row 2" decision for the timer-interval and tank-capacity
// screens). A colon-prefixed label on row 1 keeps the prompt visible while
// the user types on row 2, so the prompt itself never gets overwritten.
// ---------------------------------------------------------------------------

void LCDManager::showMoistureThresholdPrompt(const char* inputDigits)
{
    if (m_currentScreen != SCREEN_MOIST_THRESHOLD)
    {
        changeScreen(SCREEN_MOIST_THRESHOLD);
        printAt(0, 0, F("Threshold (%)"));
        refreshInput(inputDigits);
    }
    else
    {
        refreshInput(inputDigits);
    }
}

void LCDManager::showMoistureDurationPrompt(const char* inputDigits)
{
    if (m_currentScreen != SCREEN_MOIST_DURATION)
    {
        changeScreen(SCREEN_MOIST_DURATION);
        printAt(0, 0, F("Pump Time (sec):"));
        refreshInput(inputDigits);
    }
    else
    {
        refreshInput(inputDigits);
    }
}

void LCDManager::showTimerIntervalPrompt(const char* inputDigits)
{
    // Per confirmed decision: live digits render on row 2 (the SRS "row 1"
    // wording is a typo). "Water Every" stays on row 1; "Minutes" is
    // overwritten by the live digits so the user sees one growing number.
    if (m_currentScreen != SCREEN_TIMER_INTERVAL)
    {
        changeScreen(SCREEN_TIMER_INTERVAL);
        printAt(0, 0, F("Water Every"));
        printAt(0, 1, F("Minutes"));
        refreshInput(inputDigits);
    }
    else
    {
        refreshInput(inputDigits);
    }
}

void LCDManager::showTimerDurationPrompt(const char* inputDigits)
{
    if (m_currentScreen != SCREEN_TIMER_DURATION)
    {
        changeScreen(SCREEN_TIMER_DURATION);
        printAt(0, 0, F("Pump Time (s)"));
        refreshInput(inputDigits);
    }
    else
    {
        refreshInput(inputDigits);
    }
}

// ---------------------------------------------------------------------------
// Temporary message screens
// ---------------------------------------------------------------------------

void LCDManager::showSettingsSaved()
{
    if (m_currentScreen != SCREEN_SAVE_SETTINGS)
    {
        changeScreen(SCREEN_SAVE_SETTINGS);
        centerPrint(0, F("Settings Saved"));
        // Row 2 left blank intentionally.
    }
}

void LCDManager::showInvalidValue()
{
    if (m_currentScreen != SCREEN_INVALID_VALUE)
    {
        changeScreen(SCREEN_INVALID_VALUE);
        centerPrint(0, F("Invalid Value"));
        centerPrint(1, F("Try Again"));
    }
}

void LCDManager::showWatering()
{
    if (m_currentScreen != SCREEN_WATERING)
    {
        changeScreen(SCREEN_WATERING);
        centerPrint(0, F("Watering"));
    }
}

void LCDManager::showLowWater()
{
    if (m_currentScreen != SCREEN_LOW_WATER)
    {
        changeScreen(SCREEN_LOW_WATER);
        centerPrint(0, F("Low Water"));
        centerPrint(1, F("Remaining"));
    }
}

// ---------------------------------------------------------------------------
// Tank menu screens
// ---------------------------------------------------------------------------

void LCDManager::showTankMenu()
{
    if (m_currentScreen != SCREEN_TANK_MENU)
    {
        changeScreen(SCREEN_TANK_MENU);
        printAt(0, 0, F("Refill Tank"));
        printAt(0, 1, F("Set Tank Size"));
    }
}

void LCDManager::showRefillConfirm()
{
    if (m_currentScreen != SCREEN_REFILL_CONFIRM)
    {
        changeScreen(SCREEN_REFILL_CONFIRM);
        centerPrint(0, F("Tank Filled?"));
    }
}

void LCDManager::showTankCapacityPrompt(const char* inputDigits)
{
    // Per confirmed decision: the "row 2 blank" SRS wording is a prompt
    // description; live digits render on row 2 for consistency with all other
    // numeric prompts.
    if (m_currentScreen != SCREEN_TANK_CAPACITY)
    {
        changeScreen(SCREEN_TANK_CAPACITY);
        printAt(0, 0, F("Tank Capacity"));
        refreshInput(inputDigits);
    }
    else
    {
        refreshInput(inputDigits);
    }
}

// ---------------------------------------------------------------------------
// Status pages (left-aligned key:value pairs per SRS §2.7 examples)
// ---------------------------------------------------------------------------

void LCDManager::showStatusPage1(const char* modeLabel, uint16_t waterMl)
{
    if (m_currentScreen != SCREEN_STATUS)
    {
        changeScreen(SCREEN_STATUS);
    }
    char line[17];
    // Row 1: "Mode: <label>"   (label passed in as a static Flash string).
    // Using snprintf into a small stack buffer keeps formatting local; line is
    // 17 chars (16 columns + NUL) so no overflow is possible.
    snprintf(line, sizeof(line), "Mode: %s", modeLabel);
    printAt(0, 0, line);
    snprintf(line, sizeof(line), "Tank: %u mL", (unsigned)waterMl);
    printAt(0, 1, line);
}

void LCDManager::showStatusPage2(uint8_t moisturePercent, bool pumpOn)
{
    if (m_currentScreen != SCREEN_STATUS)
    {
        changeScreen(SCREEN_STATUS);
    }
    char line[17];
    snprintf(line, sizeof(line), "Moisture: %u%%", (unsigned)moisturePercent);
    printAt(0, 0, line);
    printAt(0, 1, pumpOn ? F("Pump: ON") : F("Pump: OFF"));
}

void LCDManager::showStatusPage3Moisture(uint8_t thresholdPercent, uint8_t pumpDurationSec)
{
    if (m_currentScreen != SCREEN_STATUS)
    {
        changeScreen(SCREEN_STATUS);
    }
    char line[17];
    snprintf(line, sizeof(line), "Threshold: %u%%", (unsigned)thresholdPercent);
    printAt(0, 0, line);
    snprintf(line, sizeof(line), "Time: %u sec", (unsigned)pumpDurationSec);
    printAt(0, 1, line);
}

void LCDManager::showStatusPage3Timer(uint16_t intervalMin, uint8_t pumpDurationSec)
{
    if (m_currentScreen != SCREEN_STATUS)
    {
        changeScreen(SCREEN_STATUS);
    }
    char line[17];
    // SRS §2.7 literal example: "Interval:180m" (unit "m" directly after the
    // number, no space). Followed here exactly.
    snprintf(line, sizeof(line), "Interval:%um", (unsigned)intervalMin);
    printAt(0, 0, line);
    snprintf(line, sizeof(line), "Time: %u sec", (unsigned)pumpDurationSec);
    printAt(0, 1, line);
}

// ---------------------------------------------------------------------------
// Generic error
// ---------------------------------------------------------------------------

void LCDManager::showError(const char* line1, const char* line2)
{
    if (m_currentScreen != SCREEN_ERROR)
    {
        changeScreen(SCREEN_ERROR);
    }
    centerPrint(0, line1);
    centerPrint(1, line2);
}

void LCDManager::showError(const __FlashStringHelper* line1,
                           const __FlashStringHelper* line2)
{
    if (m_currentScreen != SCREEN_ERROR)
    {
        changeScreen(SCREEN_ERROR);
    }
    centerPrint(0, line1);
    centerPrint(1, line2);
}

// ---------------------------------------------------------------------------
// Cursor management (design A18)
// ---------------------------------------------------------------------------

void LCDManager::enableCursor()
{
    if (!m_cursorEnabled)
    {
        m_lcd.cursor();
        m_lcd.blink();
        m_cursorEnabled = true;
    }
}

void LCDManager::disableCursor()
{
    if (m_cursorEnabled)
    {
        m_lcd.noCursor();
        m_lcd.noBlink();
        m_cursorEnabled = false;
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void LCDManager::changeScreen(ScreenId id)
{
    m_lcd.clear();
    m_currentScreen = id;
    m_lastInput[0] = '\0';
    // Any screen transition is a safe moment to ensure the cursor is off; the
    // renderers for input prompts will re-enable it explicitly if needed.
    ensureCursorOff();
}

void LCDManager::centerPrint(uint8_t row, const char* text)
{
    // Center within kLcdColumns (16). Negative/overlong strings are clamped to
    // a zero start column (left-aligned), which is harmless given all current
    // callers pass short labels.
    int len = (int)strlen(text);
    int col = (int)(kLcdColumns - (uint8_t)len) / 2;
    if (col < 0)
    {
        col = 0;
    }
    m_lcd.setCursor((uint8_t)col, row);
    m_lcd.print(text);
}

void LCDManager::centerPrint(uint8_t row, const __FlashStringHelper* text)
{
    // PROGMEM-resident variant of centerPrint: strlen_P measures length from
    // Flash, the start column is computed identically, and the LiquidCrystal
    // print(__FlashStringHelper*) overload streams bytes directly from Flash -
    // never copying the string into SRAM. This satisfies the Golden Constraint
    // 4 / SRS §20 F()-memory requirement.
    int len = (int)strlen_P(reinterpret_cast<PGM_P>(text));
    int col = (int)(kLcdColumns - (uint8_t)len) / 2;
    if (col < 0)
    {
        col = 0;
    }
    m_lcd.setCursor((uint8_t)col, row);
    m_lcd.print(text);
}

void LCDManager::printAt(uint8_t col, uint8_t row, const char* text)
{
    if (col >= kLcdColumns)
    {
        col = kLcdColumns - 1; // last visible column; overflow is clipped by LCD
    }
    m_lcd.setCursor(col, row);
    m_lcd.print(text);
}

void LCDManager::printAt(uint8_t col, uint8_t row, const __FlashStringHelper* text)
{
    // PROGMEM-resident variant of printAt: streams the string directly from
    // Flash via the LiquidCrystal print(__FlashStringHelper*) overload so the
    // string never occupies SRAM.
    if (col >= kLcdColumns)
    {
        col = kLcdColumns - 1;
    }
    m_lcd.setCursor(col, row);
    m_lcd.print(text);
}

void LCDManager::refreshInput(const char* inputDigits)
{
    // Flicker-free live-digit update: compare against what was last drawn so a
    // no-op call (same digits) never reaches the LCD. The cursor is enabled
    // here because this helper runs on every prompt iteration; once the user
    // confirms/cancels the cursor is turned off via ensureCursorOff() at the
    // next changeScreen() call.
    const char* current = (inputDigits != nullptr) ? inputDigits : "";

    // If identical to the last drawn buffer, do nothing at all (SRS §2.11).
    if (strncmp(m_lastInput, current, kInputBufferMax) == 0)
    {
        enableCursor();
        return;
    }

    // Overwrite row 2 from column 0 with blanks first, then print the new
    // digits. Cheap and avoids clearing the whole screen.
    m_lcd.setCursor(0, 1);
    for (uint8_t i = 0; i < kLcdColumns; ++i)
    {
        m_lcd.print(' ');
    }
    if (current[0] != '\0')
    {
        m_lcd.setCursor(0, 1);
        m_lcd.print(current);
    }

    // Cursor should sit right after the last entered digit - setCursor there
    // so the user sees the cursor in a natural position.
    m_lcd.setCursor((uint8_t)strlen(current), 1);
    enableCursor();

    // Cache for next comparison.
    strncpy(m_lastInput, current, kInputBufferMax - 1);
    m_lastInput[kInputBufferMax - 1] = '\0';
}

void LCDManager::ensureCursorOff()
{
    if (m_cursorEnabled)
    {
        m_lcd.noCursor();
        m_lcd.noBlink();
        m_cursorEnabled = false;
    }
}