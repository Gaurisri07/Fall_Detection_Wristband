/**
 * @file    gsm_module.h
 * @brief   SIM800L GSM — Phase 1 stub (log-to-Serial) + Phase 2 AT command driver.
 *
 * Person C — Location & Communication
 * Owns: /comms/gsm_module.*
 *
 * CRITICAL DESIGN DECISION — Phase 1 / Phase 2 swap:
 * ─────────────────────────────────────────────────────
 * In Phase 1 (Wokwi simulation), SIM800L is not natively supported.
 * This header declares the EXACT same public function signatures that
 * Phase 2 (real hardware) will use.  Person B's state machine calls
 * gsm_send_alert() and gsm_send_sos() — it does NOT change between phases.
 *
 * To go from Phase 1 → Phase 2:
 *   1. Set GSM_STUB_MODE to 0 below (or pass -DGSM_STUB_MODE=0 in build flags).
 *   2. Plug in a real SIM800L on GSM_TX_PIN / GSM_RX_PIN.
 *   That's it.  Person B's code is untouched.
 *
 * Pin layout (matches deck wiring diagram):
 *   ESP32 GPIO 4  → SIM800L RXD
 *   ESP32 GPIO 5  ← SIM800L TXD
 *   PWR_KEY on GPIO 23 (active-low pulse to power on/off)
 *   SIM800L needs 4.1 V @ 2 A peak — supplied via the deck's LDO stage.
 */

#pragma once
#include <Arduino.h>
#include "gps_module.h"   // for GpsData used in the alert payload

// ── Compile-time stub toggle ──────────────────────────────────────────────────
// 1 = Phase 1 (Wokwi / no SIM800L) — logs to Serial instead of transmitting.
// 0 = Phase 2 (real hardware) — sends actual AT commands.
#ifndef GSM_STUB_MODE
  #define GSM_STUB_MODE 1
#endif

// ── Pin & UART configuration ──────────────────────────────────────────────────
#define GSM_TX_PIN    4    // ESP32 TX → SIM800L RX
#define GSM_RX_PIN    5    // ESP32 RX ← SIM800L TX
#define GSM_PWR_PIN   23   // power-key pulse
#define GSM_BAUD      9600

// ── Emergency contact ─────────────────────────────────────────────────────────
// Change this to the caregiver / emergency number at deployment.
// Use international format, e.g. "+919876543210".
#define EMERGENCY_NUMBER  "+919876543210"   // ← SET THIS before deploying

// ── Return codes ─────────────────────────────────────────────────────────────
typedef enum {
    GSM_OK            = 0,   ///< SMS queued / sent successfully
    GSM_ERR_NOT_READY = 1,   ///< module not yet initialised or not registered
    GSM_ERR_NO_NETWORK= 2,   ///< no signal / not registered
    GSM_ERR_SEND_FAIL = 3,   ///< AT command sequence failed
    GSM_ERR_TIMEOUT   = 4,   ///< module did not respond within AT_TIMEOUT_MS
} GsmResult;

// ── Public API — called by Person B's state machine ──────────────────────────

/**
 * @brief  Power-on and initialise the SIM800L.
 *         Blocks for up to ~5 s while the module boots and registers.
 *         In stub mode this just prints a banner and returns immediately.
 *         Call once from setup().
 */
void gsm_init();

/**
 * @brief  Non-blocking poll — must be called every loop() iteration.
 *         In Phase 2, processes any unsolicited result codes (URCs)
 *         arriving from the modem.  In stub mode: no-op.
 */
void gsm_update();

/**
 * @brief  True if the module is registered on a network and ready to send.
 *         In stub mode always returns true after gsm_init().
 */
bool gsm_is_ready();

/**
 * @brief  Send (or simulate) an automatic fall-detection alert SMS.
 *
 *         Message format:
 *         "FALL ALERT: [Name] may have fallen.
 *          Time: HH:MM:SS UTC DD/MM/YYYY
 *          Location: <map URL or NO FIX>
 *          Coordinates: lat, lon
 *          -- Sent by FallGuard Wristband"
 *
 * @param  gps_data  Latest GpsData from gps_get_data().
 * @return GsmResult code.
 */
GsmResult gsm_send_alert(const GpsData& gps_data);

/**
 * @brief  Send (or simulate) a manual SOS SMS triggered by button press.
 *         Same payload as gsm_send_alert() but prefixed "SOS — MANUAL TRIGGER".
 *
 * @param  gps_data  Latest GpsData from gps_get_data().
 * @return GsmResult code.
 */
GsmResult gsm_send_sos(const GpsData& gps_data);

/**
 * @brief  Return a human-readable string for a GsmResult code.
 *         Useful for Serial debug output.
 */
const char* gsm_result_str(GsmResult r);
