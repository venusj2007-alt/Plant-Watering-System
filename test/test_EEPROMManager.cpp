/**
 * @file  test_EEPROMManager.cpp
 * @purpose Unity unit tests for EEPROMManager's defensive validation logic.
 * @layer Test (host-side; AVR EEPROM is shimmed by PlatformIO's native test env)
 *
 * Validates that every user-input setter rejects out-of-range values (returns
 * false and leaves the cached mirror unchanged) and accepts in-range values
 * (returns true and updates the mirror). This isolates the A14 contract
 * (no silent clamping for user values) from the rest of the system.
 */

#include <Arduino.h>
#include <unity.h>

#include "EEPROMManager.h"

static EEPROMManager g_eeprom;

void setUp(void)
{
    g_eeprom.begin();
}

void tearDown(void) {}

void test_default_threshold_loaded(void)
{
    // Fresh EEPROM (or blank host EPROM shim) should yield the factory default.
    TEST_ASSERT_EQUAL(kDefaultMoistureThreshold,
                      g_eeprom.settings().moistureThreshold);
}

void test_threshold_accepts_valid_range(void)
{
    TEST_ASSERT_TRUE(g_eeprom.setMoistureThreshold(0));
    TEST_ASSERT_EQUAL(0, g_eeprom.settings().moistureThreshold);
    TEST_ASSERT_TRUE(g_eeprom.setMoistureThreshold(100));
    TEST_ASSERT_EQUAL(100, g_eeprom.settings().moistureThreshold);
    TEST_ASSERT_TRUE(g_eeprom.setMoistureThreshold(50));
    TEST_ASSERT_EQUAL(50, g_eeprom.settings().moistureThreshold);
}

void test_threshold_rejects_out_of_range(void)
{
    uint8_t cached = g_eeprom.settings().moistureThreshold;
    TEST_ASSERT_FALSE(g_eeprom.setMoistureThreshold(101));
    TEST_ASSERT_EQUAL(cached, g_eeprom.settings().moistureThreshold);
    // boundary: 0 is valid, so rejection only for >100 (uint8_t can't be <0)
}

void test_pump_duration_accepts_valid_range(void)
{
    TEST_ASSERT_TRUE(g_eeprom.setMoisturePumpDuration(1));
    TEST_ASSERT_EQUAL(1, g_eeprom.settings().moisturePumpDuration);
    TEST_ASSERT_TRUE(g_eeprom.setMoisturePumpDuration(60));
    TEST_ASSERT_EQUAL(60, g_eeprom.settings().moisturePumpDuration);
}

void test_pump_duration_rejects_out_of_range(void)
{
    uint8_t cached = g_eeprom.settings().moisturePumpDuration;
    TEST_ASSERT_FALSE(g_eeprom.setMoisturePumpDuration(0));
    TEST_ASSERT_EQUAL(cached, g_eeprom.settings().moisturePumpDuration);
    TEST_ASSERT_FALSE(g_eeprom.setMoisturePumpDuration(61));
    TEST_ASSERT_EQUAL(cached, g_eeprom.settings().moisturePumpDuration);
}

void test_timer_interval_accepts_valid_range(void)
{
    TEST_ASSERT_TRUE(g_eeprom.setTimerInterval(1));
    TEST_ASSERT_EQUAL(1, g_eeprom.settings().timerInterval);
    TEST_ASSERT_TRUE(g_eeprom.setTimerInterval(999));
    TEST_ASSERT_EQUAL(999, g_eeprom.settings().timerInterval);
}

void test_timer_interval_rejects_out_of_range(void)
{
    uint16_t cached = g_eeprom.settings().timerInterval;
    TEST_ASSERT_FALSE(g_eeprom.setTimerInterval(0));
    TEST_ASSERT_EQUAL(cached, g_eeprom.settings().timerInterval);
    TEST_ASSERT_FALSE(g_eeprom.setTimerInterval(1000));
    TEST_ASSERT_EQUAL(cached, g_eeprom.settings().timerInterval);
}

void test_tank_capacity_accepts_valid_range(void)
{
    TEST_ASSERT_TRUE(g_eeprom.setTankCapacity(1));
    TEST_ASSERT_EQUAL(1, g_eeprom.settings().tankCapacity);
    TEST_ASSERT_TRUE(g_eeprom.setTankCapacity(9999));
    TEST_ASSERT_EQUAL(9999, g_eeprom.settings().tankCapacity);
}

void test_tank_capacity_rejects_out_of_range(void)
{
    uint16_t cached = g_eeprom.settings().tankCapacity;
    TEST_ASSERT_FALSE(g_eeprom.setTankCapacity(0));
    TEST_ASSERT_EQUAL(cached, g_eeprom.settings().tankCapacity);
    TEST_ASSERT_FALSE(g_eeprom.setTankCapacity(10000));
    TEST_ASSERT_EQUAL(cached, g_eeprom.settings().tankCapacity);
}

void test_set_tank_capacity_resets_remaining_water(void)
{
    // SRS §9.4: changing tank capacity resets remaining estimate.
    g_eeprom.setRemainingWater(500);
    g_eeprom.setTankCapacity(2000);
    TEST_ASSERT_EQUAL(2000, g_eeprom.settings().remainingWater);
}

void test_mode_accepts_valid_values(void)
{
    TEST_ASSERT_TRUE(g_eeprom.setMode(MODE_MOISTURE));
    TEST_ASSERT_EQUAL(MODE_MOISTURE, (OperatingMode)g_eeprom.settings().mode);
    TEST_ASSERT_TRUE(g_eeprom.setMode(MODE_TIMER));
    TEST_ASSERT_EQUAL(MODE_TIMER, (OperatingMode)g_eeprom.settings().mode);
}

void test_calibration_accepts_valid_adc(void)
{
    TEST_ASSERT_TRUE(g_eeprom.setDryCal(0));
    TEST_ASSERT_EQUAL(0, g_eeprom.settings().dryCal);
    TEST_ASSERT_TRUE(g_eeprom.setDryCal(1023));
    TEST_ASSERT_EQUAL(1023, g_eeprom.settings().dryCal);
    TEST_ASSERT_TRUE(g_eeprom.setWetCal(400));
    TEST_ASSERT_EQUAL(400, g_eeprom.settings().wetCal);
}

void test_calibration_rejects_invalid_adc(void)
{
    uint16_t cached = g_eeprom.settings().dryCal;
    TEST_ASSERT_FALSE(g_eeprom.setDryCal(1024));
    TEST_ASSERT_EQUAL(cached, g_eeprom.settings().dryCal);
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_threshold_loaded);
    RUN_TEST(test_threshold_accepts_valid_range);
    RUN_TEST(test_threshold_rejects_out_of_range);
    RUN_TEST(test_pump_duration_accepts_valid_range);
    RUN_TEST(test_pump_duration_rejects_out_of_range);
    RUN_TEST(test_timer_interval_accepts_valid_range);
    RUN_TEST(test_timer_interval_rejects_out_of_range);
    RUN_TEST(test_tank_capacity_accepts_valid_range);
    RUN_TEST(test_tank_capacity_rejects_out_of_range);
    RUN_TEST(test_set_tank_capacity_resets_remaining_water);
    RUN_TEST(test_mode_accepts_valid_values);
    RUN_TEST(test_calibration_accepts_valid_adc);
    RUN_TEST(test_calibration_rejects_invalid_adc);
    return UNITY_END();
}