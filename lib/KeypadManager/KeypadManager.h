/**
 * @file  KeypadManager.h
 * @purpose Service-layer wrapper around the 4x4 matrix Keypad library.
 * @layer Service
 *
 * Single responsibility: scan the keypad, debounce, and return the currently
 * pressed character (or kNoKey). No input buffering, no value parsing, no menu
 * decisions - those belong to MenuManager. Non-blocking.
 *
 * May access: Keypad library + PinDefinitions (per design Q3 the Keypad library
 *             is called directly by its owning service).
 * Accessed by: MenuManager only.
 */
#ifndef KEYPAD_MANAGER_H
#define KEYPAD_MANAGER_H

#include <Arduino.h>
#include <Keypad.h>
#include <stdint.h>

#include "Constants.h"
#include "PinDefinitions.h"

/**
 * @class KeypadManager
 * @brief Encapsulates matrix keypad scanning and debouncing for the firmware.
 *
 * Supports all 16 keys of a standard 4x4 keypad: 0-9, A, B, C, D, *, #.
 * Key 'B' is currently unused by any feature and reserved for future use
 * (design A9). Polling is non-blocking: callers should invoke poll() once per
 * main loop iteration.
 */
class KeypadManager
{
public:
    /**
     * @brief Construct the wrapper (binds the underlying Keypad to the
     *        row/column pins from PinDefinitions.h).
     */
    KeypadManager();

    /**
     * @brief Perform library-level initialization. Safe to call once at boot.
     */
    void begin();

    /**
     * @brief Poll the keypad once, non-blocking.
     * @return The currently pressed key character ('0'-'9', 'A'-'D', '*', '#'),
     *         or kNoKey ('\0') when no key is pressed.
     */
    char poll();

    // -----------------------------------------------------------------------
    // No public constants / structs / enums.
    // -----------------------------------------------------------------------
private:
    /** Underlying Keypad library instance (matrix scan + debounce). */
    Keypad m_keypad;
};

#endif // KEYPAD_MANAGER_H