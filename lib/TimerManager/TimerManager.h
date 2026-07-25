/**
 * @file  TimerManager.h
 * @purpose Provide a reusable non-blocking software timer value type used by
 *         logic-layer modules for cooldowns, watering intervals, message windows,
 *         and status-page auto-advance.
 * @layer Logic (timing primitive)
 *
 * Single responsibility: implement generic millis()-based timing. Owns no
 * domain semantics (it does not know about watering, menus, etc.). Multiple
 * independent SoftwareTimer instances may be created by callers.
 *
 * May access: millis() directly (per design Q3 - millis is a permitted primitive).
 * Accessed by: ModeManager (interval + cooldown), MenuManager (message + page
 *              timers). NOTE: PumpController uses its own millis() stopwatch and
 *              does NOT depend on TimerManager (Service layer may not depend on
 *              Logic layer).
 */
#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H

#include <Arduino.h>
#include <stdint.h>

/**
 * @class SoftwareTimer
 * @brief Non-blocking one-shot timer with latched expiry semantics.
 *
 * The timer is "latched": once elapsed >= duration, isExpired() keeps returning
 * true until start()/restart()/stop() is called. This makes it easy to poll for
 * "has the window ended yet?" without missing the edge.
 *
 * Time tracking uses millis() and never blocks.
 */
class SoftwareTimer
{
public:
    /**
     * @brief Construct a non-running timer with zero duration.
     */
    SoftwareTimer();

    /**
     * @brief Start the timer for the given duration.
     *        Resets start timestamp, stores duration, marks running.
     * @param durationMs  Duration in milliseconds.
     */
    void start(uint32_t durationMs);

    /**
     * @brief Convenience start helper: accepts minutes and converts to ms.
     * @param minutes  Duration in minutes (1..65535).
     */
    void startMinutes(uint16_t minutes);

    /**
     * @brief Restart the timer using the previously configured duration.
     *        Resets the start timestamp to now; remains running.
     */
    void restart();

    /**
     * @brief Stop the timer (mark not running). isExpired() will return false.
     */
    void stop();

    /**
     * @brief Check whether the timer is currently running.
     * @return true if start()/restart() has been called and stop() has not.
     */
    bool isRunning() const;

    /**
     * @brief Check whether the configured duration has elapsed.
     * @return true if running and (millis() - start) >= duration. Latched: stays
     *         true until restart()/stop().
     */
    bool isExpired() const;

    /**
     * @brief Elapsed time since the current run began.
     * @return Milliseconds elapsed, or 0 if not running.
     */
    uint32_t elapsedMs() const;

    /**
     * @brief Remaining time until expiry (clamped to 0).
     * @return Milliseconds remaining, or 0 if not running / already expired.
     */
    uint32_t remainingMs() const;

    /**
     * @brief Get the configured duration.
     * @return The duration set by the last start()/startMinutes() call (ms).
     */
    uint32_t getDurationMs() const;

    // -----------------------------------------------------------------------
    // No constants / structs / enums declared in this class scope.
    // -----------------------------------------------------------------------
private:
    /** millis() timestamp of the most recent start/restart. */
    uint32_t m_startMs;

    /** Configured duration in milliseconds (0 if never started). */
    uint32_t m_durationMs;

    /** Whether the timer is currently running. */
    bool     m_running;
};

#endif // TIMER_MANAGER_H