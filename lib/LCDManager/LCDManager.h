/**
 * @file  LCDManager.h
 * @purpose Service-layer wrapper around the 16x2 I2C LCD. Owns all screen
 *         rendering with flicker-free partial-redraw logic.
 * @layer Service
 *
 * Single responsibility: render text to the LCD. Makes NO application decisions
 * (no menu navigation, no mode logic). MenuManager chooses which renderer to
 * call; LCDManager handles cursor, centering, and "only redraw what changed".
 *
 * Flicker policy (SRS §2.11): only call lcd.clear() when the screen identity
 * changes; otherwise update only modified characters. All static text is stored
 * in Flash via F() to conserve SRAM.
 *
 * Per SRS §8.2 layering, LCDManager (Service) must NOT depend on any Logic
 * module. Therefore every renderer takes primitive arguments (no status-page
 * structs from StatusManager); MenuManager assembles the primitives from
 * StatusManager and passes them here.
 *
 * May access: LiquidCrystal_I2C + Wire (per design Q3, called directly here).
 * Accessed by: MenuManager, Application (splash only).
 */
#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <stdint.h>

#include "Constants.h"
#include "Version.h"

/**
 * @class LCDManager
 * @brief Flicker-free 16x2 I2C LCD renderer with named screen methods.
 */
class LCDManager
{
public:
    /**
     * @brief Construct the LCD wrapper (binds the underlying LiquidCrystal_I2C
     *        to the configured I2C address and 16x2 geometry).
     */
    LCDManager();

    /**
     * @brief Initialize the LCD: init, backlight on, clear, cursor off.
     */
    void begin();

    // ---- Boot / home -------------------------------------------------------

    /**
     * @brief Render the 2-second splash screen with project name and version.
     *        Row 1: "Plant Watering" (centered).
     *        Row 2: "System vX.Y" (centered, using Version.h constants).
     *        The delay() that holds this screen is in Application::begin(),
     *        NOT here (only allowed blocking delay per Golden Constraint).
     */
    void showSplash();

    /** Render the idle home screen (centered: "Plant Watering" / "A = Menu"). */
    void showHome();

    /** Render the main menu ("1:M 2:T" / "3:Status 4:Tank"). */
    void showMainMenu();

    // ---- Configuration prompts (input shown live) --------------------------

    /**
     * @brief Render the moisture threshold prompt with live input.
     * @param inputDigits  Null-terminated string of entered digits (may be "").
     */
    void showMoistureThresholdPrompt(const char* inputDigits);

    /**
     * @brief Render the moisture-mode pump duration prompt with live input.
     * @param inputDigits  Null-terminated string of entered digits (may be "").
     */
    void showMoistureDurationPrompt(const char* inputDigits);

    /**
     * @brief Render the timer interval prompt with live input.
     * @param inputDigits  Null-terminated string of entered digits (may be "").
     */
    void showTimerIntervalPrompt(const char* inputDigits);

    /**
     * @brief Render the timer-mode pump duration prompt with live input.
     * @param inputDigits  Null-terminated string of entered digits (may be "").
     */
    void showTimerDurationPrompt(const char* inputDigits);

    // ---- Temporary message screens ----------------------------------------

    /** Render the "Settings Saved" confirmation message (timed by MenuManager). */
    void showSettingsSaved();

    /** Render the "Invalid Value" / "Try Again" error message (timed). */
    void showInvalidValue();

    /** Render the watering indicator ("Watering", centered). Pump is ON. */
    void showWatering();

    /** Render the low-water takeover ("Low Water" / "Remaining"). */
    void showLowWater();

    // ---- Tank menu screens ------------------------------------------------

    /** Render the tank menu ("Refill Tank" / "Set Tank Size"). */
    void showTankMenu();

    /** Render the refill confirmation prompt ("Tank Filled?"). */
    void showRefillConfirm();

    /**
     * @brief Render the tank-capacity entry prompt with live input.
     * @param inputDigits  Null-terminated string of entered digits (may be "").
     */
    void showTankCapacityPrompt(const char* inputDigits);

    // ---- Status pages (read-only view) ------------------------------------
    /**
     * The three status pages. MenuManager pulls data from StatusManager and
     * calls the matching renderer; LCDManager formats and centers the text.
     */

    /** Page 1: mode label + remaining water. */
    void showStatusPage1(const char* modeLabel, uint16_t waterMl);

    /** Page 2: live moisture percent + pump on/off. */
    void showStatusPage2(uint8_t moisturePercent, bool pumpOn);

    /** Page 3 variant for Moisture Mode: threshold % + moisture pump time (s). */
    void showStatusPage3Moisture(uint8_t thresholdPercent, uint8_t pumpDurationSec);

    /** Page 3 variant for Timer Mode: interval (min) + timer pump time (s). */
    void showStatusPage3Timer(uint16_t intervalMin, uint8_t pumpDurationSec);

    // ---- Generic error ----------------------------------------------------
    /**
     * @brief Render a two-line error screen.
     * @param line1  Row 1 text (centered).
     * @param line2  Row 2 text (centered).
     */
    void showError(const char* line1, const char* line2);

    /**
     * @brief Render a two-line error screen from Flash-resident strings.
     *        Overload used by callers that pass F()-wrapped text.
     */
    void showError(const __FlashStringHelper* line1,
                   const __FlashStringHelper* line2);

    // ---- Cursor management ------------------------------------------------------------------------------------------
    /**
     * @brief Enable the underline cursor and blinking block cursor during
     *        numeric entry (lcd.cursor() + lcd.blink() per design A18).
     */
    void enableCursor();

    /**
     * @brief Disable both underline and blinking cursors on input completion.
     */
    void disableCursor();

private:
    // -----------------------------------------------------------------------
    // Internal screen identity (used to detect screen changes for flicker-free
    // redraws - kept private; callers reference renderers only).
    // -----------------------------------------------------------------------
    enum ScreenId
    {
        SCREEN_NONE              = 0,
        SCREEN_SPLASH            = 1,
        SCREEN_HOME              = 2,
        SCREEN_MAIN_MENU         = 3,
        SCREEN_MOIST_THRESHOLD   = 4,
        SCREEN_MOIST_DURATION    = 5,
        SCREEN_TIMER_INTERVAL    = 6,
        SCREEN_TIMER_DURATION    = 7,
        SCREEN_SAVE_SETTINGS     = 8,
        SCREEN_INVALID_VALUE     = 9,
        SCREEN_WATERING          = 10,
        SCREEN_LOW_WATER         = 11,
        SCREEN_TANK_MENU         = 12,
        SCREEN_REFILL_CONFIRM    = 13,
        SCREEN_TANK_CAPACITY     = 14,
        SCREEN_STATUS            = 15,
        SCREEN_ERROR             = 16
    };

    /** Underlying LCD driver instance. */
    LiquidCrystal_I2C m_lcd;

    /** Screen currently drawn (used to decide whether a clear() is needed). */
    ScreenId m_currentScreen;

    /** Last input string that was drawn (skip redraw if unchanged). */
    char     m_lastInput[kInputBufferMax];

    /** Whether the cursor is currently enabled. */
    bool     m_cursorEnabled;

    // --- private helpers ----------------------------------------------------

    /** If id differs from m_currentScreen, clear the LCD and remember id. */
    void changeScreen(ScreenId id);

    /** Print text centered on the given row (row 0 or 1). */
    void centerPrint(uint8_t row, const char* text);

    /** PROGMEM overload: centers a Flash-resident (F()-wrapped) string. */
    void centerPrint(uint8_t row, const __FlashStringHelper* text);

    /** Print text left-aligned starting at column col on the given row. */
    void printAt(uint8_t col, uint8_t row, const char* text);

    /** PROGMEM overload: left-aligns a Flash-resident (F()-wrapped) string. */
    void printAt(uint8_t col, uint8_t row, const __FlashStringHelper* text);

    /** Refresh the live input digits on the active prompt screen. */
    void refreshInput(const char* inputDigits);

    /** Cursor housekeeping after leaving an input screen. */
    void ensureCursorOff();
};

#endif // LCD_MANAGER_H