/**
 * @file  TimerManager.cpp
 * @purpose Implementation of the SoftwareTimer primitive - a latched, non-
 *         blocking one-shot millis()-based timer.
 * @layer Logic (timing primitive)
 *
 * Design notes:
 *  - Latched expiry: once the timer's duration has elapsed, isExpired() keeps
 *    returning true until the caller calls restart()/start()/stop(). This
 *    matters because callers (MenuManager, ModeManager) may poll the timer
 *    many loop iterations after the edge - the expiry must not "self-clear"
 *    and be missed.
 *  - All time math uses unsigned subtraction (millis() - m_startMs), which is
 *    correct across the ~49-day millis() roll-over without any special case.
 *  - The timer never calls delay() and never blocks; querying is O(1).
 *  - startMinutes() is a thin convenience for callers expressing wall-clock
 *    intervals (e.g. the timer-mode watering interval). It multiplies minutes
 *    by 60000UL in 32-bit space, so the max storable minutes (65535) -> ~65.5 d
 *    of duration, well within uint32_t range.
 */

#include "TimerManager.h"

#include <Arduino.h>

SoftwareTimer::SoftwareTimer()
    : m_startMs(0)
    , m_durationMs(0)
    , m_running(false)
{
}

void SoftwareTimer::start(uint32_t durationMs)
{
    m_startMs   = millis();
    m_durationMs = durationMs;
    m_running    = true;
}

void SoftwareTimer::startMinutes(uint16_t minutes)
{
    start((uint32_t)minutes * 60000UL);
}

void SoftwareTimer::restart()
{
    if (!m_running && m_durationMs == 0)
    {
        // Nothing to restart (never started). Avoid silently starting a 0 ms
        // timer that would be immediately expired; require an explicit start().
        return;
    }
    m_startMs = millis();
    m_running = true;
}

void SoftwareTimer::stop()
{
    m_running = false;
}

bool SoftwareTimer::isRunning() const
{
    return m_running;
}

bool SoftwareTimer::isExpired() const
{
    if (!m_running)
    {
        return false;
    }
    // Unsigned subtraction is roll-over safe. The timer is "latched": once the
    // window has elapsed, isExpired() stays true until the caller restart()s
    // or stop()s - the elapsed-vs-duration check below keeps returning true.
    return (millis() - m_startMs) >= m_durationMs;
}

uint32_t SoftwareTimer::elapsedMs() const
{
    if (!m_running)
    {
        return 0;
    }
    return millis() - m_startMs;
}

uint32_t SoftwareTimer::remainingMs() const
{
    if (!m_running)
    {
        return 0;
    }
    uint32_t elapsed = millis() - m_startMs;
    if (elapsed >= m_durationMs)
    {
        return 0;
    }
    return m_durationMs - elapsed;
}

uint32_t SoftwareTimer::getDurationMs() const
{
    return m_durationMs;
}