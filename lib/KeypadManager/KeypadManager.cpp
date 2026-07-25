/**
 * @file  KeypadManager.cpp
 * @purpose Implementation of the 4x4 matrix keypad scanner / debouncer.
 * @layer Service
 *
 * Design notes:
 *  - The Chris--A/Keypad library performs matrix scanning + debouncing in a
 *    non-blocking way: getKey() returns the next key transition or NO_KEY. We
 *    map NO_KEY to our kNoKey sentinel ('\0') so callers test a single value.
 *  - The standard 4x4 keypad is wired row-major; the keymap below matches the
 *    physical labels of a typical membrane 4x4 keypad (design A15):
 *        Row0: 1 2 3 A
 *        Row1: 4 5 6 B
 *        Row2: 7 8 9 C
 *        Row3: * 0 # D
 *  - Pin arrays are static file-scope constants so the Keypad member can be
 *    initialized once via the member initializer list (Keypad's constructor
 *    takes raw pointers, so the arrays must outlive every call to getKey()).
 *  - Per SRS §5.3 Rule 3, ALL keypad reading goes through KeypadManager. No
 *    other module calls keypad.getKey() directly.
 */

#include "KeypadManager.h"

// Standard 4x4 membrane keypad layout (design A15).
static char s_keymap[kKeypadRows][kKeypadCols] =
{
    { '1', '2', '3', 'A' },
    { '4', '5', '6', 'B' },
    { '7', '8', '9', 'C' },
    { '*', '0', '#', 'D' }
};

// Row and column pin arrays - bound to the PinDefinitions constants. These
// must have static storage so the Keypad library can hold the pointers safely
// for the lifetime of the program.
static byte s_rowPins[kKeypadRows] =
{
    kPinKeypadRow1,
    kPinKeypadRow2,
    kPinKeypadRow3,
    kPinKeypadRow4
};

static byte s_colPins[kKeypadCols] =
{
    kPinKeypadCol1,
    kPinKeypadCol2,
    kPinKeypadCol3,
    kPinKeypadCol4
};

KeypadManager::KeypadManager()
    // The Keypad library constructor accepts (keymap, rowPins, colPins, numRows,
    // numCols). We pass static-storage arrays so the pointers remain valid for
    // the program lifetime.
    : m_keypad((char*)s_keymap, s_rowPins, s_colPins, kKeypadRows, kKeypadCols)
{
}

void KeypadManager::begin()
{
    // The Keypad constructor already calls pinMode on every row/column pin.
    // Nothing further to do here, but begin() keeps a consistent lifecycle with
    // the other managers.
}

char KeypadManager::poll()
{
    // getKey() returns NO_KEY ('\0' on the Arduino Keypad library) when no key
    // is active. Returning it directly satisfies the API contract (kNoKey is
    // also '\0' via Constants.h).
    return m_keypad.getKey();
}