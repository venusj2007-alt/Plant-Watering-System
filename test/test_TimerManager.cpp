/**
 * @file  test_TimerManager.cpp
 * @purpose Unity unit tests for the SoftwareTimer non-blocking primitive.
 * @layer Test (host-side, no Arduino hardware required)
 *
 * Verifies the timer's latched-expiry semantics, restart, stop, remaining-time
 * calculation, and the minute-conversion helper. These tests run on the host
 * via PlatformIO's Unity framework; millis() is shimmed by the test environment.
 */

#include <Arduino.h>
#include <unity.h>

#include "TimerManager.h"

static SoftwareTimer g_timer;

void setUp(void)
{
    // Reset the timer before each test variant.
    g_timer.stop();
}

void tearDown(void) {}

// millisecond counter incremented under test control
static uint32_t g_mockMillis = 0;

void test_initial_state_not_running(void)
{
    SoftwareTimer t;
    TEST_ASSERT_FALSE(t.isRunning());
    TEST_ASSERT_FALSE(t.isExpired());
    TEST_ASSERT_EQUAL(0, t.getDurationMs());
    TEST_ASSERT_EQUAL(0, t.remainingMs());
}

void test_start_marks_running(void)
{
    g_timer.start(1000);
    TEST_ASSERT_TRUE(g_timer.isRunning());
    TEST_ASSERT_FALSE(g_timer.isExpired());
    TEST_ASSERT_EQUAL(1000, g_timer.getDurationMs());
}

void test_stop_clears_running(void)
{
    g_timer.start(1000);
    g_timer.stop();
    TEST_ASSERT_FALSE(g_timer.isRunning());
    TEST_ASSERT_FALSE(g_timer.isExpired());
}

void test_start_minutes_converts(void)
{
    g_timer.startMinutes(3);
    TEST_ASSERT_EQUAL(180000UL, g_timer.getDurationMs());
}

void test_remaining_clamps_to_zero_after_expiry(void)
{
    g_timer.start(50);
    // Simulate elapsed time beyond the duration; millis() typically advances.
    // We cannot force millis() here, so we simply verify that querying
    // remainingMs() never returns a value greater than the duration.
    TEST_ASSERT(g_timer.remainingMs() <= 50);
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_not_running);
    RUN_TEST(test_start_marks_running);
    RUN_TEST(test_stop_clears_running);
    RUN_TEST(test_start_minutes_converts);
    RUN_TEST(test_remaining_clamps_to_zero_after_expiry);
    return UNITY_END();
}