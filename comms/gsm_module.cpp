/**
 * @file    gsm_module.cpp
 * @brief   SIM800L implementation — Phase 1 stub + Phase 2 AT command driver.
 *
 * Person C — Location & Communication
 *
 * Both phases share the same message formatter (build_alert_sms / build_sos_sms).
 * The only thing that differs is what happens once the message string is ready:
 *   Phase 1: Serial.println(message)            — visible in Wokwi serial monitor
 *   Phase 2: issue_at_command(AT+CMGS, ...)     — real transmission
 *
 * AT command reference (SIM800L):
 *   ATE0           — echo off
 *   AT+CMGF=1      — text mode SMS
 *   AT+CMGS="NUM"  — set recipient, then send message text terminated by Ctrl-Z (0x1A)
 *   AT+CSQ         — signal quality (0-31; >=10 usable)
 *   AT+CREG?       — network registration status
 */

#include "gsm_module.h"
#include <stdio.h>
#include <string.h>

// ── Module-private state ──────────────────────────────────────────────────────
static bool g_gsm_ready   = false;
static bool g_gsm_inited  = false;

#if !GSM_STUB_MODE
// AT command timeout
static const unsigned long AT_TIMEOUT_MS = 8000UL;

// ── Helper: send an AT command and wait for "expected" response ───────────────
static bool at_send(const char* cmd, const char* expected, unsigned long timeout_ms) {
    Serial1.println(cmd);
    unsigned long start = millis();
    String response = "";
    while (millis() - start < timeout_ms) {
        while (Serial1.available()) {
            char c = (char)Serial1.read();
            response += c;
        }
        if (response.indexOf(expected) >= 0) return true;
    }
    Serial.print("[GSM] Timeout waiting for: "); Serial.println(expected);
    Serial.print("[GSM] Got: "); Serial.println(response);
    return false;
}

// ── Helper: read signal quality, return -1 on failure ────────────────────────
static int8_t at_signal_quality() {
    Serial1.println("AT+CSQ");
    unsigned long start = millis();
    String response = "";
    while (millis() - start < 3000UL) {
        while (Serial1.available()) response += (char)Serial1.read();
        if (response.indexOf("+CSQ:") >= 0) break;
    }
    int idx = response.indexOf("+CSQ:");
    if (idx < 0) return -1;
    return (int8_t)response.substring(idx + 5).toInt();
}
#endif  // !GSM_STUB_MODE

// ── Shared message builders ───────────────────────────────────────────────────

static void build_alert_sms(char* buf, size_t len, const GpsData& g, bool is_sos) {
    char map_url[120];
    gps_format_map_url(map_url, sizeof(map_url));

    if (is_sos) {
        snprintf(buf, len,
            "*** SOS — MANUAL TRIGGER ***\n"
            "The wearer has pressed the SOS button.\n"
            "Time: %02d:%02d:%02d UTC %02d/%02d/%04d\n"
            "Location: %s\n"
            "Coordinates: %.6f, %.6f\n"
            "-- FallGuard Wristband",
            g.hour, g.minute, g.second,
            g.day, g.month, g.year,
            map_url,
            (double)g.latitude, (double)g.longitude
        );
    } else {
        snprintf(buf, len,
            "FALL ALERT: Wearer may have fallen.\n"
            "Time: %02d:%02d:%02d UTC %02d/%02d/%04d\n"
            "Location: %s\n"
            "Coordinates: %.6f, %.6f\n"
            "-- FallGuard Wristband",
            g.hour, g.minute, g.second,
            g.day, g.month, g.year,
            map_url,
            (double)g.latitude, (double)g.longitude
        );
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void gsm_init() {
    g_gsm_inited = true;

#if GSM_STUB_MODE
    // ── Phase 1: stub mode ─────────────────────────────────────────────────────
    Serial.println("[GSM STUB] Phase 1 mode active.");
    Serial.println("[GSM STUB] Real SIM800L not present — SMS will be logged to Serial.");
    Serial.print  ("[GSM STUB] Emergency number configured: ");
    Serial.println(EMERGENCY_NUMBER);
    g_gsm_ready = true;

#else
    // ── Phase 2: real SIM800L ──────────────────────────────────────────────────
    Serial1.begin(GSM_BAUD, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);

    // PWR_KEY pulse to power on (active low, hold ~1 s)
    pinMode(GSM_PWR_PIN, OUTPUT);
    digitalWrite(GSM_PWR_PIN, LOW);
    delay(1200);
    digitalWrite(GSM_PWR_PIN, HIGH);
    delay(3000);   // wait for module to boot

    // Check AT echo
    if (!at_send("ATE0", "OK", AT_TIMEOUT_MS)) {
        Serial.println("[GSM] ERROR: No response from SIM800L. Check wiring & power.");
        return;
    }

    // Set SMS text mode
    if (!at_send("AT+CMGF=1", "OK", AT_TIMEOUT_MS)) {
        Serial.println("[GSM] ERROR: Could not set SMS text mode.");
        return;
    }

    // Check network registration
    if (!at_send("AT+CREG?", "+CREG: 0,1", AT_TIMEOUT_MS)) {
        Serial.println("[GSM] WARNING: Not yet registered on network.");
        // Don't return — could register later
    }

    int8_t sq = at_signal_quality();
    Serial.print("[GSM] Signal quality: ");
    if (sq < 0) Serial.println("unknown");
    else { Serial.print(sq); Serial.println("/31"); }

    g_gsm_ready = (sq >= 5);
    Serial.print("[GSM] Ready: ");
    Serial.println(g_gsm_ready ? "YES" : "NO (low signal)");
#endif
}

void gsm_update() {
#if !GSM_STUB_MODE
    // Drain any unsolicited result codes to avoid buffer fill
    while (Serial1.available()) {
        String urc = Serial1.readStringUntil('\n');
        urc.trim();
        if (urc.length() > 0) {
            Serial.print("[GSM URC] "); Serial.println(urc);
        }
    }
#endif
}

bool gsm_is_ready() {
    return g_gsm_inited && g_gsm_ready;
}

GsmResult gsm_send_alert(const GpsData& gps_data) {
    if (!g_gsm_ready) return GSM_ERR_NOT_READY;

    char message[512];
    build_alert_sms(message, sizeof(message), gps_data, false);

#if GSM_STUB_MODE
    // ── Phase 1: stub — log to Serial Monitor ─────────────────────────────────
    Serial.println();
    Serial.println("╔══════════════════════════════════════════╗");
    Serial.println("║         [GSM STUB] SMS WOULD SEND        ║");
    Serial.println("╠══════════════════════════════════════════╣");
    Serial.print  ("║ TO: "); Serial.println(EMERGENCY_NUMBER);
    Serial.println("║──────────────────────────────────────────║");
    Serial.println(message);
    Serial.println("╚══════════════════════════════════════════╝");
    Serial.println();
    return GSM_OK;

#else
    // ── Phase 2: real AT commands ──────────────────────────────────────────────
    // Verify signal quality before attempting
    if (at_signal_quality() < 5) return GSM_ERR_NO_NETWORK;

    // Start SMS
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", EMERGENCY_NUMBER);
    Serial1.println(cmd);
    delay(500);

    // Send message body terminated with Ctrl-Z
    Serial1.print(message);
    Serial1.write(0x1A);   // Ctrl-Z = end of SMS

    // Wait for "+CMGS:" confirmation (up to 15 s — GSM can be slow)
    unsigned long start = millis();
    String resp = "";
    while (millis() - start < 15000UL) {
        while (Serial1.available()) resp += (char)Serial1.read();
        if (resp.indexOf("+CMGS:") >= 0) {
            Serial.println("[GSM] Alert SMS sent successfully.");
            return GSM_OK;
        }
        if (resp.indexOf("ERROR") >= 0) {
            Serial.print("[GSM] Send error response: "); Serial.println(resp);
            return GSM_ERR_SEND_FAIL;
        }
    }
    return GSM_ERR_TIMEOUT;
#endif
}

GsmResult gsm_send_sos(const GpsData& gps_data) {
    if (!g_gsm_ready) return GSM_ERR_NOT_READY;

    char message[512];
    build_alert_sms(message, sizeof(message), gps_data, true);

#if GSM_STUB_MODE
    Serial.println();
    Serial.println("╔══════════════════════════════════════════╗");
    Serial.println("║       [GSM STUB] SOS SMS WOULD SEND      ║");
    Serial.println("╠══════════════════════════════════════════╣");
    Serial.print  ("║ TO: "); Serial.println(EMERGENCY_NUMBER);
    Serial.println("║──────────────────────────────────────────║");
    Serial.println(message);
    Serial.println("╚══════════════════════════════════════════╝");
    Serial.println();
    return GSM_OK;

#else
    // Same AT flow as gsm_send_alert — SOS is structurally identical
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", EMERGENCY_NUMBER);
    Serial1.println(cmd);
    delay(500);
    Serial1.print(message);
    Serial1.write(0x1A);

    unsigned long start = millis();
    String resp = "";
    while (millis() - start < 15000UL) {
        while (Serial1.available()) resp += (char)Serial1.read();
        if (resp.indexOf("+CMGS:") >= 0) {
            Serial.println("[GSM] SOS SMS sent successfully.");
            return GSM_OK;
        }
        if (resp.indexOf("ERROR") >= 0) return GSM_ERR_SEND_FAIL;
    }
    return GSM_ERR_TIMEOUT;
#endif
}

const char* gsm_result_str(GsmResult r) {
    switch (r) {
        case GSM_OK:             return "OK";
        case GSM_ERR_NOT_READY:  return "ERR_NOT_READY";
        case GSM_ERR_NO_NETWORK: return "ERR_NO_NETWORK";
        case GSM_ERR_SEND_FAIL:  return "ERR_SEND_FAIL";
        case GSM_ERR_TIMEOUT:    return "ERR_TIMEOUT";
        default:                 return "ERR_UNKNOWN";
    }
}
