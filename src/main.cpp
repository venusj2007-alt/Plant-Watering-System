/**
 * @file  main.cpp
 * @purpose Arduino entry point. Contains ONLY setup() and loop(), delegating
 *         all behaviour to a single Application instance per SRS §6.3.
 * @layer Boot
 *
 * No application logic, state-machine implementation, hardware control, menu
 * handling, timing logic, or business logic is present in this file: every
 * behavior lives inside Application (or the classes Application owns).
 *
 * The single static Application instance `g_app` is constructed before setup()
 * runs (static-storage-duration initialization), so setup() only has to call
 * g_app.begin() to bring the whole system up, and loop() only has to call
 * g_app.tick() to advance it.
 */

#include <Arduino.h>

#include "Application.h"

/// Single composition root for the entire firmware. Constructed once; its
/// constructor wires every collaborator (HAL objects + every manager). Static
/// storage duration keeps it in BSS / data so no dynamic allocation is used
/// (Golden Constraint: no new/delete/malloc/free).
static Application g_app;

/**
 * @brief Arduino setup hook. Initializes the whole system via the composition
 *        root. No watering occurs during this call (SRS §14).
 */
void setup()
{
    g_app.begin();
}

/**
 * @brief Arduino loop hook. Runs one non-blocking main-loop iteration. The
 *        Arduino runtime calls this repeatedly as fast as it can; Application
 *        ::tick() is O(1) and never blocks, so input remains responsive (<100 ms
 *        detection time per SRS §19) while background sampling, watering
 *        evaluation, UI FSM, and the status indicator all advance in lockstep.
 */
void loop()
{
    g_app.tick();
}