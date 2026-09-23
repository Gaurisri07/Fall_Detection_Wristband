/**
 * @file    comms_test_standalone.ino
 * @brief   Standalone Wokwi sketch to test Person C's /comms/ module
 *          in isolation — without needing Person A or B code.
 *
 * Usage (in Wokwi):
 *   1. Open diagram.json to load the circuit.
 *   2. Upload this sketch.
 *   3. Watch the Serial Monitor (115200 baud).
 *
 * What this tests:
 *   - GPS initialisation and fix acquisition from the Wokwi NEO-6M chip.
 *   - NMEA sentence parsing — confirms lat/lon/time are decoded correctly.
 *   - Map URL generation.
 *   - GSM stub alert and SOS messages are formatted and logged correctly.
 *   - GPS timeout detection (comment out GPS hardware in diagram.json to test).
 */

#include "../comms/gps_module.h"
#include "../comms/gsm_module.h"

// ── Test state ────────────────────────────────────────────────────────────────
static bool alert_sent = false;
static bool sos_sent   = false;
static unsigned long test_start_ms = 0;

// Simulate a "fall confirmed" event after 15 s (so we can test gsm_send_alert)
static const unsigned long FAKE_FALL_AFTER_MS = 15000UL;
// Simulate an SOS button press after 25 s
static const unsigned long FAKE_SOS_AFTER_MS  = 25000UL;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("========================================");
    Serial.println(" Person C — /comms/ Module Self-Test   ");
    Serial.println(" Wokwi ESP32 + NEO-6M simulation       ");
    Serial.println("========================================\n");

    gps_init();
    gsm_init();

    test_start_ms = millis();
    Serial.println("[TEST] Setup complete. Waiting for GPS fix...");
    Serial.println("[TEST] Fake 'fall confirmed' will fire in 15 s.");
    Serial.println("[TEST] Fake 'SOS button' will fire in 25 s.\n");
}

void loop() {
    // ── 1. Poll GPS every loop iteration ─────────────────────────────────────
    bool new_sentence = gps_update();
    if (new_sentence) {
        const GpsData& g = gps_get_data();
        // Print a compact status line each time a sentence arrives
        Serial.printf("[GPS] Fix:%s  Lat:%.5f  Lon:%.5f  Sats:%d  Time:%02d:%02d:%02d\n",
                      gps_has_fix() ? "YES" : "no ",
                      (double)g.latitude, (double)g.longitude,
                      g.satellites, g.hour, g.minute, g.second);
    }

    // ── 2. Detect GPS timeout ─────────────────────────────────────────────────
    if (gps_timed_out() && !gps_has_fix()) {
        static bool timeout_reported = false;
        if (!timeout_reported) {
            Serial.println("[GPS] *** TIMEOUT — no fix acquired within window ***");
            Serial.println("[TEST] Continuing with NO FIX — alert will show placeholder.");
            timeout_reported = true;
        }
    }

    // ── 3. Simulate fall confirmed (auto alert) after FAKE_FALL_AFTER_MS ─────
    unsigned long elapsed = millis() - test_start_ms;
    if (!alert_sent && elapsed >= FAKE_FALL_AFTER_MS) {
        alert_sent = true;
        Serial.println("\n[TEST] >>> Simulating FALL CONFIRMED event <<<");
        GsmResult result = gsm_send_alert(gps_get_data());
        Serial.print("[TEST] gsm_send_alert returned: ");
        Serial.println(gsm_result_str(result));

        // Also print the map URL directly
        char url[120];
        gps_format_map_url(url, sizeof(url));
        Serial.print("[TEST] Map URL: "); Serial.println(url);
        Serial.println();
    }

    // ── 4. Simulate SOS button press after FAKE_SOS_AFTER_MS ─────────────────
    if (!sos_sent && elapsed >= FAKE_SOS_AFTER_MS) {
        sos_sent = true;
        Serial.println("[TEST] >>> Simulating SOS BUTTON PRESS <<<");
        GsmResult result = gsm_send_sos(gps_get_data());
        Serial.print("[TEST] gsm_send_sos returned: ");
        Serial.println(gsm_result_str(result));
        Serial.println();
        Serial.println("[TEST] All test cases complete.");
    }

    // ── 5. Poll GSM for any URCs ──────────────────────────────────────────────
    gsm_update();

    delay(10);  // ~100 Hz loop
}
