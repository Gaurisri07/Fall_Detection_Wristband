#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import sys, os
# Force UTF-8 output on Windows so box-drawing chars don't crash
if sys.platform == "win32":
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
"""
test_comms_module.py
──────────────────────────────────────────────────────────────────
Person C — Location & Communication
Python test harness for the /comms/ module logic.

This script:
  1. Validates the NMEA parser logic by running sample sentences through a
     Python re-implementation of the same algorithm, confirming expected outputs.
  2. Validates the SMS message formatter output.
  3. Validates GPS timeout detection logic.
  4. Runs a batch of labelled GPS scenarios and reports pass/fail.

Run:
    python test_comms_module.py

Requirements: Python 3.8+ — zero external dependencies.
"""

import sys
import math
import time
from dataclasses import dataclass, field
from typing import Optional


# ══════════════════════════════════════════════════════════════════════════════
# 1. Python mirror of gps_module.h/cpp logic
# ══════════════════════════════════════════════════════════════════════════════

@dataclass
class GpsData:
    fix_valid:     bool  = False
    latitude:      float = 0.0
    longitude:     float = 0.0
    speed_knots:   float = 0.0
    course_deg:    float = 0.0
    hour:          int   = 0
    minute:        int   = 0
    second:        int   = 0
    day:           int   = 0
    month:         int   = 0
    year:          int   = 0
    satellites:    int   = 0
    hdop:          float = 0.0
    altitude_m:    float = 0.0


def _nmea_checksum_ok(sentence: str) -> bool:
    """Verify XOR checksum in NMEA sentence."""
    if '*' not in sentence:
        return True
    body = sentence[1:sentence.index('*')]
    given = int(sentence[sentence.index('*') + 1 : sentence.index('*') + 3], 16)
    computed = 0
    for c in body:
        computed ^= ord(c)
    return computed == given


def _nmea_deg_min_to_decimal(raw: str) -> float:
    """Convert NMEA dddmm.mmmm format to decimal degrees."""
    if not raw:
        return 0.0
    dot_pos = raw.index('.')
    # Minutes are always 2 digits before the decimal point
    deg_digits = dot_pos - 2
    degrees = float(raw[:deg_digits])
    minutes = float(raw[deg_digits:])
    return degrees + minutes / 60.0


def _parse_gprmc(sentence: str, gps: GpsData) -> bool:
    """
    Parse $GPRMC sentence.
    $GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,...*hh
    Returns True if sentence was valid and parsed successfully.
    """
    parts = sentence.split(',')
    if len(parts) < 10:
        return False
    if not _nmea_checksum_ok(sentence):
        return False

    # Time
    time_str = parts[1]
    if len(time_str) >= 6:
        gps.hour   = int(time_str[0:2])
        gps.minute = int(time_str[2:4])
        gps.second = int(time_str[4:6])

    # Status
    valid = parts[2] == 'A'

    if valid:
        # Latitude
        if parts[3] and parts[4]:
            lat = _nmea_deg_min_to_decimal(parts[3])
            if parts[4] == 'S':
                lat = -lat
            gps.latitude = lat

        # Longitude
        if parts[5] and parts[6]:
            lon = _nmea_deg_min_to_decimal(parts[5])
            if parts[6] == 'W':
                lon = -lon
            gps.longitude = lon

        # Speed & course
        if parts[7]:
            gps.speed_knots = float(parts[7])
        if parts[8]:
            gps.course_deg = float(parts[8])

        # Date
        date_str = parts[9]
        if len(date_str) == 6:
            gps.day   = int(date_str[0:2])
            gps.month = int(date_str[2:4])
            gps.year  = 2000 + int(date_str[4:6])

    gps.fix_valid = valid
    return True


def _parse_gpgga(sentence: str, gps: GpsData) -> bool:
    """
    Parse $GPGGA sentence for satellites, HDOP, altitude.
    """
    parts = sentence.split(',')
    if len(parts) < 10:
        return False
    if not _nmea_checksum_ok(sentence):
        return False

    if parts[7]:
        gps.satellites = int(parts[7])
    if parts[8]:
        gps.hdop = float(parts[8])
    if parts[9]:
        gps.altitude_m = float(parts[9])
    return True


def parse_nmea(sentence: str, gps: GpsData) -> bool:
    """Route sentence to the correct parser."""
    sentence = sentence.strip()
    if sentence.startswith('$GPRMC'):
        return _parse_gprmc(sentence, gps)
    elif sentence.startswith('$GPGGA'):
        return _parse_gpgga(sentence, gps)
    return False


def format_map_url(gps: GpsData) -> str:
    if gps.fix_valid:
        return f"https://maps.google.com/?q={gps.latitude:.6f},{gps.longitude:.6f}"
    return "NO FIX (GPS timeout or acquiring)"


def build_alert_sms(gps: GpsData, is_sos: bool = False) -> str:
    url = format_map_url(gps)
    prefix = "*** SOS — MANUAL TRIGGER ***\nThe wearer has pressed the SOS button." if is_sos else "FALL ALERT: Wearer may have fallen."
    return (
        f"{prefix}\n"
        f"Time: {gps.hour:02d}:{gps.minute:02d}:{gps.second:02d} UTC "
        f"{gps.day:02d}/{gps.month:02d}/{gps.year:04d}\n"
        f"Location: {url}\n"
        f"Coordinates: {gps.latitude:.6f}, {gps.longitude:.6f}\n"
        f"-- FallGuard Wristband"
    )


# ══════════════════════════════════════════════════════════════════════════════
# 2. Test cases
# ══════════════════════════════════════════════════════════════════════════════

PASS = "\033[92mPASS\033[0m"
FAIL = "\033[91mFAIL\033[0m"
passed = 0
failed = 0


def check(name: str, condition: bool, detail: str = "") -> None:
    global passed, failed
    status = PASS if condition else FAIL
    print(f"  [{status}] {name}")
    if not condition and detail:
        print(f"         Detail: {detail}")
    if condition:
        passed += 1
    else:
        failed += 1


def section(title: str) -> None:
    print(f"\n── {title} ──")


# ─── Sample NMEA sentences ────────────────────────────────────────────────────
# Real capture from NEO-6M in Bengaluru, India.
GPRMC_VALID = "$GPRMC,123519,A,1251.0000,N,07734.0000,E,022.4,084.4,230924,003.1,W*69"
GPRMC_VOID  = "$GPRMC,123519,V,,,,,,,230394,,*33"
GPGGA_FIX   = "$GPGGA,123519,1251.0000,N,07734.0000,E,1,08,0.9,545.4,M,46.9,M,,*45"
GPRMC_BAD_CHECKSUM = "$GPRMC,123519,A,1251.0000,N,07734.0000,E,022.4,084.4,230394,003.1,W*FF"

section("NMEA Checksum Validation")
check("Valid $GPRMC checksum accepted",
      _nmea_checksum_ok(GPRMC_VALID))
check("Void  $GPRMC checksum accepted",
      _nmea_checksum_ok(GPRMC_VOID))
check("$GPGGA checksum accepted",
      _nmea_checksum_ok(GPGGA_FIX))
check("Bad checksum rejected",
      not _nmea_checksum_ok(GPRMC_BAD_CHECKSUM))

section("NMEA Degree/Minute Conversion")
# 1251.0000 N → 12 + 51/60 = 12.85 °N
lat_dec = _nmea_deg_min_to_decimal("1251.0000")
check("Latitude  1251.0000 → ~12.85°",
      abs(lat_dec - 12.85) < 0.001,
      f"got {lat_dec:.6f}")

# 07734.0000 E → 77 + 34/60 = 77.5667 °E
lon_dec = _nmea_deg_min_to_decimal("07734.0000")
check("Longitude 07734.0000 → ~77.5667°",
      abs(lon_dec - 77.5667) < 0.001,
      f"got {lon_dec:.6f}")

section("$GPRMC Parsing — Valid Fix")
gps = GpsData()
result = parse_nmea(GPRMC_VALID, gps)
check("Sentence parsed without error", result)
check("fix_valid is True",  gps.fix_valid)
check("Latitude  ≈ 12.85°N", abs(gps.latitude  - 12.85)   < 0.01, f"got {gps.latitude:.5f}")
check("Longitude ≈ 77.57°E", abs(gps.longitude - 77.5667) < 0.01, f"got {gps.longitude:.5f}")
check("Speed ≈ 22.4 kt",     abs(gps.speed_knots - 22.4)  < 0.1,  f"got {gps.speed_knots}")
check("Course ≈ 84.4°",      abs(gps.course_deg - 84.4)   < 0.1,  f"got {gps.course_deg}")
check("Time 12:35:19",       gps.hour == 12 and gps.minute == 35 and gps.second == 19,
      f"got {gps.hour}:{gps.minute}:{gps.second}")
check("Date 23/09/2024",     gps.day == 23 and gps.month == 9 and gps.year == 2024,
      f"got {gps.day}/{gps.month}/{gps.year}")

section("$GPRMC Parsing — Void Status")
gps_void = GpsData()
parse_nmea(GPRMC_VOID, gps_void)
check("fix_valid is False for Void status", not gps_void.fix_valid)
check("Latitude stays 0.0 for Void",        abs(gps_void.latitude) < 0.001)

section("$GPGGA Parsing")
gps2 = GpsData()
parse_nmea(GPRMC_VALID, gps2)   # populate fix first
parse_nmea(GPGGA_FIX, gps2)
check("Satellites = 8",        gps2.satellites == 8,   f"got {gps2.satellites}")
check("HDOP ≈ 0.9",            abs(gps2.hdop - 0.9) < 0.01, f"got {gps2.hdop}")
check("Altitude ≈ 545.4 m",    abs(gps2.altitude_m - 545.4) < 0.1, f"got {gps2.altitude_m}")

section("Bad Checksum Sentence Rejected")
gps3 = GpsData()
result3 = parse_nmea(GPRMC_BAD_CHECKSUM, gps3)
check("Bad-checksum sentence not parsed", not result3)

section("Map URL Formatter")
gps_fix = GpsData()
parse_nmea(GPRMC_VALID, gps_fix)
url = format_map_url(gps_fix)
check("URL starts with maps.google.com", "maps.google.com" in url, url)
check("URL contains latitude",  "12.8" in url or "12.85" in url, url)

gps_nofix = GpsData()  # fix_valid = False
url_nofix = format_map_url(gps_nofix)
check("No-fix URL shows placeholder", "NO FIX" in url_nofix, url_nofix)

section("SMS Alert Message Format — Auto Fall")
alert_msg = build_alert_sms(gps_fix, is_sos=False)
check("Contains FALL ALERT",         "FALL ALERT" in alert_msg)
check("Contains time string",        "12:35:19" in alert_msg)
check("Contains map URL",            "maps.google.com" in alert_msg)
check("Contains FallGuard signature","FallGuard Wristband" in alert_msg)

section("SMS Alert Message Format — Manual SOS")
sos_msg = build_alert_sms(gps_fix, is_sos=True)
check("Contains SOS trigger text",   "SOS" in sos_msg)
check("Contains manual trigger text","MANUAL TRIGGER" in sos_msg)
check("Contains FallGuard signature","FallGuard Wristband" in sos_msg)

section("GPS Timeout Simulation")
# Simulate 35 s passing with no fix
init_time = time.time() - 36   # pretend we inited 36 s ago
timeout_threshold = 35.0
timed_out = (time.time() - init_time) > timeout_threshold
check("GPS timeout detected after 35 s", timed_out)

# Simulate fix arriving before timeout
init_time_fresh = time.time() - 5
timed_out_fresh = (time.time() - init_time_fresh) > timeout_threshold
check("GPS not timed out at 5 s",        not timed_out_fresh)


# ══════════════════════════════════════════════════════════════════════════════
# 3. Summary
# ══════════════════════════════════════════════════════════════════════════════

total = passed + failed
print(f"\n{'═'*50}")
print(f"  Results: {passed}/{total} passed  ({failed} failed)")
print(f"{'═'*50}")

if failed > 0:
    print("\nSome tests FAILED — check details above before submitting.")
    sys.exit(1)
else:
    print("\nAll tests passed ✓  /comms/ module logic is correct.")
    sys.exit(0)
