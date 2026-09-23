/**
 * @file    gps_module.cpp
 * @brief   NEO-6M GPS implementation — UART read loop, NMEA parser,
 *          fix validation, map URL formatter.
 *
 * Person C — Location & Communication
 *
 * Parsing strategy: lightweight manual parser (no TinyGPS++ dependency)
 * so the Wokwi simulation has zero external library requirements.
 * Handles $GPRMC and $GPGGA.  All other sentence types are silently skipped.
 *
 * NMEA sentence format reminder:
 *   $<talker><type>,<f0>,<f1>,...,<fn>*<checksum><CR><LF>
 */

#include "gps_module.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ── Module-private state ──────────────────────────────────────────────────────

static GpsData        g_gps;               // single shared GPS data struct
static char           g_nmea_buf[128];     // raw sentence accumulation buffer
static uint8_t        g_nmea_idx = 0;      // write index into g_nmea_buf
static bool           g_in_sentence = false;
static unsigned long  g_init_time_ms = 0;
static unsigned long  g_last_valid_fix_ms = 0;
static bool           g_fix_stable = false;

// ── Helper: NMEA checksum validation ─────────────────────────────────────────
/**
 * Verify XOR checksum embedded in sentence.
 * Sentence must be the raw string starting with '$'.
 * Returns true if checksum is valid OR if there is no '*' (skip check).
 */
static bool nmea_checksum_ok(const char* sentence) {
    const char* star = strchr(sentence, '*');
    if (!star) return true;         // no checksum field — accept

    uint8_t computed = 0;
    for (const char* p = sentence + 1; p < star; p++) {
        computed ^= (uint8_t)(*p);
    }
    // Parse the two hex digits after '*'
    char hex[3] = { star[1], star[2], '\0' };
    uint8_t given = (uint8_t)strtol(hex, nullptr, 16);
    return computed == given;
}

// ── Helper: extract Nth comma-delimited field from NMEA sentence ──────────────
/**
 * Copies field[n] (0-based, counting from the sentence type, e.g. "GPRMC")
 * into dest (max dest_len-1 chars + null terminator).
 * Returns false if field does not exist.
 */
static bool nmea_field(const char* sentence, uint8_t n, char* dest, size_t dest_len) {
    const char* p = sentence;
    uint8_t field_idx = 0;

    while (*p && field_idx < n) {
        if (*p == ',') field_idx++;
        p++;
    }
    if (!*p) { dest[0] = '\0'; return false; }

    size_t i = 0;
    while (*p && *p != ',' && *p != '*' && i < dest_len - 1) {
        dest[i++] = *p++;
    }
    dest[i] = '\0';
    return i > 0;
}

// ── Helper: convert NMEA lat/lon dddmm.mmmm → decimal degrees ────────────────
static float nmea_deg_min_to_decimal(const char* raw) {
    if (!raw || raw[0] == '\0') return 0.0f;
    // Find decimal point to split degrees from minutes
    const char* dot = strchr(raw, '.');
    if (!dot) return 0.0f;
    int deg_digits = (int)(dot - raw) - 2;   // minutes always 2 digits before '.'
    if (deg_digits < 1) return 0.0f;

    char deg_buf[5] = {};
    strncpy(deg_buf, raw, deg_digits);
    deg_buf[deg_digits] = '\0';
    float degrees = (float)atoi(deg_buf);
    float minutes = atof(raw + deg_digits);
    return degrees + minutes / 60.0f;
}

// ── Parser: $GPRMC ─────────────────────────────────────────────────────────────
/**
 * $GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a*hh
 * Field indices (0 = sentence type):
 *  0 GPRMC
 *  1 UTC time hhmmss.ss
 *  2 Status A=active/valid, V=void
 *  3 Latitude  llll.ll
 *  4 N/S
 *  5 Longitude yyyyy.yy
 *  6 E/W
 *  7 Speed over ground knots
 *  8 Course over ground degrees
 *  9 Date ddmmyy
 * 10 Magnetic variation (optional)
 * 11 E/W for variation
 */
static void parse_gprmc(const char* sentence) {
    char field[20];

    // Field 2: validity
    if (!nmea_field(sentence, 2, field, sizeof(field))) return;
    bool valid = (field[0] == 'A');

    // Time
    if (nmea_field(sentence, 1, field, sizeof(field)) && strlen(field) >= 6) {
        g_gps.hour   = (field[0]-'0')*10 + (field[1]-'0');
        g_gps.minute = (field[2]-'0')*10 + (field[3]-'0');
        g_gps.second = (field[4]-'0')*10 + (field[5]-'0');
    }

    if (valid) {
        // Latitude
        char lat_raw[16], lat_dir[4];
        if (nmea_field(sentence, 3, lat_raw, sizeof(lat_raw)) &&
            nmea_field(sentence, 4, lat_dir, sizeof(lat_dir))) {
            g_gps.latitude = nmea_deg_min_to_decimal(lat_raw);
            if (lat_dir[0] == 'S') g_gps.latitude = -g_gps.latitude;
        }

        // Longitude
        char lon_raw[16], lon_dir[4];
        if (nmea_field(sentence, 5, lon_raw, sizeof(lon_raw)) &&
            nmea_field(sentence, 6, lon_dir, sizeof(lon_dir))) {
            g_gps.longitude = nmea_deg_min_to_decimal(lon_raw);
            if (lon_dir[0] == 'W') g_gps.longitude = -g_gps.longitude;
        }

        // Speed & course
        if (nmea_field(sentence, 7, field, sizeof(field)))
            g_gps.speed_knots = atof(field);
        if (nmea_field(sentence, 8, field, sizeof(field)))
            g_gps.course_deg = atof(field);

        // Date ddmmyy
        if (nmea_field(sentence, 9, field, sizeof(field)) && strlen(field) == 6) {
            g_gps.day   = (field[0]-'0')*10 + (field[1]-'0');
            g_gps.month = (field[2]-'0')*10 + (field[3]-'0');
            g_gps.year  = 2000 + (field[4]-'0')*10 + (field[5]-'0');
        }
    }

    // Update fix validity & timestamp
    if (valid && !g_gps.fix_valid) {
        g_gps.fix_timestamp_ms = millis();
    }
    g_gps.fix_valid = valid;
    if (valid) g_last_valid_fix_ms = millis();
}

// ── Parser: $GPGGA ─────────────────────────────────────────────────────────────
/**
 * $GPGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x.x,xxxx*hh
 * Field 6 = fix quality (0=no fix, 1=GPS, 2=DGPS)
 * Field 7 = number of satellites
 * Field 8 = HDOP
 * Field 9 = altitude (metres above MSL)
 */
static void parse_gpgga(const char* sentence) {
    char field[20];

    if (nmea_field(sentence, 7, field, sizeof(field)))
        g_gps.satellites = (uint8_t)atoi(field);
    if (nmea_field(sentence, 8, field, sizeof(field)))
        g_gps.hdop = atof(field);
    if (nmea_field(sentence, 9, field, sizeof(field)))
        g_gps.altitude_m = atof(field);
}

// ── Route completed sentence to the right parser ─────────────────────────────
static bool dispatch_sentence(const char* sentence) {
    if (!nmea_checksum_ok(sentence)) return false;

    if (strncmp(sentence, "$GPRMC", 6) == 0) {
        parse_gprmc(sentence);
        return true;
    }
    if (strncmp(sentence, "$GPGGA", 6) == 0) {
        parse_gpgga(sentence);
        return true;
    }
    return false;  // unrecognised sentence type — ignore
}

// ── Public API ────────────────────────────────────────────────────────────────

void gps_init() {
    memset(&g_gps, 0, sizeof(g_gps));
    g_gps.fix_valid = false;
    g_nmea_idx      = 0;
    g_in_sentence   = false;
    g_fix_stable    = false;
    g_init_time_ms  = millis();

    Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    Serial.println("[GPS] Initialised — waiting for NEO-6M fix...");
    Serial.print("[GPS] Timeout in ");
    Serial.print(GPS_FIX_TIMEOUT_MS / 1000);
    Serial.println(" s");
}

bool gps_update() {
    bool parsed = false;

    while (Serial2.available()) {
        char c = (char)Serial2.read();

        if (c == '$') {
            // Start of new sentence — reset accumulation buffer
            g_nmea_buf[0] = '$';
            g_nmea_idx    = 1;
            g_in_sentence = true;
        } else if (g_in_sentence) {
            if (c == '\n' || g_nmea_idx >= (sizeof(g_nmea_buf) - 1)) {
                // Terminate and dispatch
                g_nmea_buf[g_nmea_idx] = '\0';
                g_in_sentence = false;
                if (g_nmea_idx > 6) {           // minimum sane sentence length
                    parsed = dispatch_sentence(g_nmea_buf);
                }
                g_nmea_idx = 0;
            } else if (c != '\r') {
                g_nmea_buf[g_nmea_idx++] = c;
            }
        }
    }

    // Update stable-fix debounce (require 500 ms of continuous valid fix)
    if (g_gps.fix_valid && (millis() - g_last_valid_fix_ms < 2000UL)) {
        if (!g_fix_stable && (millis() - g_gps.fix_timestamp_ms >= 500UL)) {
            g_fix_stable = true;
            Serial.println("[GPS] Fix acquired and stable.");
        }
    } else if (!g_gps.fix_valid) {
        g_fix_stable = false;
    }

    return parsed;
}

const GpsData& gps_get_data() {
    return g_gps;
}

bool gps_has_fix() {
    return g_fix_stable;
}

bool gps_timed_out() {
    if (g_fix_stable) return false;
    return (millis() - g_init_time_ms) > GPS_FIX_TIMEOUT_MS;
}

void gps_format_map_url(char* buf, size_t len) {
    if (g_gps.fix_valid) {
        snprintf(buf, len,
                 "https://maps.google.com/?q=%.6f,%.6f",
                 (double)g_gps.latitude,
                 (double)g_gps.longitude);
    } else {
        snprintf(buf, len, "NO FIX (GPS timeout or acquiring)");
    }
}
