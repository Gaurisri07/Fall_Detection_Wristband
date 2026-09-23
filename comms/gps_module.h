/**
 * @file    gps_module.h
 * @brief   NEO-6M GPS — UART read, NMEA parsing, fix validation.
 *
 * Person C — Location & Communication
 * Owns: /comms/gps_module.*
 *
 * Phase 1 behaviour (Wokwi simulation):
 *   - Reads NMEA sentences from Serial2 (the Wokwi NEO-6M chip's TX line).
 *   - Parses $GPRMC and $GPGGA sentences.
 *   - Provides a single public struct GpsData and three public functions
 *     that Person B's state machine calls by name.
 *
 * Phase 2 (real hardware): zero code changes needed — only the
 * Serial2 baud rate / pin assignment in gps_module.cpp may need
 * updating for the physical PCB layout.
 */

#pragma once
#include <Arduino.h>

// ── Pin & UART configuration ─────────────────────────────────────────────────
// NEO-6M default baud rate is 9600.  Wokwi routes the chip's TX to
// ESP32 GPIO 16 (RX2) automatically when you use Serial2.
#define GPS_BAUD      9600
#define GPS_RX_PIN    16   // ESP32 RX2
#define GPS_TX_PIN    17   // ESP32 TX2 (not used in read-only mode)

// ── Fix timeout ───────────────────────────────────────────────────────────────
// How long (ms) to wait for a valid GPS fix before declaring GPS_TIMEOUT.
// NEO-6M cold start: up to 30 s.  Wokwi simulation usually provides
// a fix immediately from the first NMEA sentence.
#define GPS_FIX_TIMEOUT_MS  35000UL

// ── Public data structure ─────────────────────────────────────────────────────
/**
 * @struct GpsData
 * @brief  All fields needed by the alert message formatter.
 *
 * Fields are populated by gps_parse_sentence().
 * Consumers should check `fix_valid` before trusting lat/lon.
 */
struct GpsData {
    bool    fix_valid;          ///< true when GPRMC "status" field == 'A'
    float   latitude;           ///< decimal degrees, positive = North
    float   longitude;          ///< decimal degrees, positive = East
    float   speed_knots;        ///< speed over ground (from GPRMC)
    float   course_deg;         ///< course over ground (from GPRMC)
    uint8_t hour;               ///< UTC hour   (from GPRMC hhmmss)
    uint8_t minute;             ///< UTC minute
    uint8_t second;             ///< UTC second
    uint8_t day;                ///< UTC day    (from GPRMC ddmmyy)
    uint8_t month;              ///< UTC month
    uint16_t year;              ///< UTC year (4-digit)
    uint8_t  satellites;        ///< number of satellites in use (from GPGGA)
    float    hdop;              ///< horizontal dilution of precision (GPGGA)
    float    altitude_m;        ///< altitude above MSL in metres (GPGGA)
    unsigned long fix_timestamp_ms; ///< millis() when fix first became valid
};

// ── Public API — called by Person B's state machine ──────────────────────────

/**
 * @brief  Initialise Serial2 for NEO-6M communication.
 *         Call once from setup().
 */
void gps_init();

/**
 * @brief  Non-blocking update — reads all available bytes from the GPS UART,
 *         assembles complete NMEA sentences and parses them into g_gps.
 *
 *         Call every loop() iteration (or at least every 100 ms).
 *         Returns true if a new complete sentence was successfully parsed
 *         this call (useful for debug logging, not required by caller).
 */
bool gps_update();

/**
 * @brief  Return a const reference to the latest parsed GPS data.
 *         Always safe to call; check fix_valid before using lat/lon.
 */
const GpsData& gps_get_data();

/**
 * @brief  True if a valid fix has been held continuously for at least
 *         500 ms (debounce against momentary fix loss).
 */
bool gps_has_fix();

/**
 * @brief  True if gps_init() was called more than GPS_FIX_TIMEOUT_MS ago
 *         and we still have no valid fix.
 */
bool gps_timed_out();

/**
 * @brief  Format "lat,lon" as a Google Maps URL for use in the SMS body.
 *         Writes into caller-supplied buffer.  Safe even when fix_valid
 *         is false (produces a "NO FIX" placeholder string).
 *
 * @param  buf   Output character buffer.
 * @param  len   Size of buf (recommend >= 120).
 */
void gps_format_map_url(char* buf, size_t len);
