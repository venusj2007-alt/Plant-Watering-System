/**
 * @file  PumpController.h
 * @purpose Service-layer non-blocking control of the water pump MOSFET and the
 *         green "pump active" LED, with per-cycle water-volume accounting.
 * @layer Service
 *
 * Single responsibility: turn the pump on/off for a configured duration using
 * millis()-based non-blocking timing, and report how much water a finished cycle
 * consumed. Owns the green LED (it directly reflects pump operation - design
 * A12 / SRS §3.1 update). Refuses overlapping cycles (mutual exclusion, SRS §12).
 * Pump is forced OFF on begin() to guarantee no unintended energization at boot.
 *
 * This module does NOT decide when to water (that is ModeManager). It uses its
 * own millis() stopwatch and does NOT depend on TimerManager, because the
 * Service layer may not depend on the Logic layer.
 *
 * May access: DigitalOutput (HAL for pump gate + green LED), Config (flow rate),
 *             Constants.
 * Accessed by: ModeManager only.
 */
#ifndef PUMP_CONTROLLER_H
#define PUMP_CONTROLLER_H

#include <Arduino.h>
#include <stdint.h>

#include "Config.h"
#include "Constants.h"
#include "DigitalOutput.h"  // HAL: lib/Hardware (resolved by PlatformIO LDF)

/**
 * @class PumpController
 * @brief Non-blocking, mutually-exclusive pump driver with volume accounting.
 */
class PumpController
{
public:
    /**
     * @brief Construct the controller bound to its HAL outputs.
     * @param gate      DigitalOutput connected to the MOSFET gate (pump switch).
     * @param greenLed  DigitalOutput connected to the green "pump active" LED.
     */
    PumpController(DigitalOutput& gate, DigitalOutput& greenLed);

    /**
     * @brief Safe-state initialization: force the gate LOW and the green LED OFF,
     *        and mark not-running. Must be called before any start().
     */
    void begin();

    /**
     * @brief Start a watering cycle of the given duration. Refuses if a cycle
     *        is already running (mutual exclusion, SRS §12).
     * @param durationSec  Pump-on duration in seconds (validated by caller;
     *                     passed through unchanged).
     * @return true if the cycle was started; false if already running.
     */
    bool start(uint8_t durationSec);

    /**
     * @brief Advance the active cycle (call each loop iteration). When the
     *        configured duration has elapsed, turns the pump and green LED OFF,
     *        computes water used (kPumpFlowRateMlPerSec * duration), and latches
     *        the finished-flag for ModeManager to consume. Non-blocking.
     */
    void tick();

    /**
     * @brief Is a watering cycle currently in progress?
     * @return true if the pump is currently ON.
     */
    bool isRunning() const;

    /**
     * @brief Consume a just-finished cycle event, atomically.
     *        ModeManager should call this every tick to detect cycle completion.
     *        The flag is cleared after consumption (one-shot).
     * @param outWaterMl  Out-param set to the millilitres consumed this cycle.
     * @return true if a cycle just finished (and outWaterMl is valid);
     *         false otherwise (outWaterMl left unchanged).
     */
    bool takeFinished(uint32_t& outWaterMl);

private:
    /** HAL output driving the pump MOSFET gate. */
    DigitalOutput& m_gate;

    /** HAL output driving the green "pump active" LED. */
    DigitalOutput& m_greenLed;

    /** Whether a watering cycle is currently active. */
    bool     m_running;

    /** millis() timestamp at which the current cycle started. */
    uint32_t m_startMs;

    /** Configured pump-on duration for the current cycle (seconds). */
    uint8_t  m_durationSec;

    /** One-shot flag set when the current cycle just completed. */
    bool     m_justFinished;

    /** Millilitres consumed by the most recently completed cycle. */
    uint32_t m_waterUsedMl;
};

#endif // PUMP_CONTROLLER_H