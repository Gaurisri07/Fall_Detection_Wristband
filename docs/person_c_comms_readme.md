# Person C — Location & Communication Module
## `/comms/` — GPS + GSM for FallGuard Wristband

---

### What this module does

Person C owns everything in `/comms/`. It gives the rest of the system two clean capabilities:

| Capability | File | How |
|---|---|---|
| Know **where** the wearer is | `gps_module.*` | Reads NMEA sentences from the Wokwi NEO-6M chip over Serial2 |
| **Notify** the caregiver | `gsm_module.*` | Phase 1: logs a formatted SMS to Serial; Phase 2: fires real AT commands |

Person B's state machine calls exactly **three functions from this module**:

```cpp
gps_update();                          // call every loop()
gsm_send_alert(gps_get_data());        // when fall is confirmed
gsm_send_sos(gps_get_data());          // when SOS button pressed
```

---

### File map

```
/comms/
  gps_module.h       ← public API + GpsData struct (read this first)
  gps_module.cpp     ← NMEA parser, fix validator, map URL formatter
  gsm_module.h       ← public API + GsmResult codes
  gsm_module.cpp     ← Phase 1 stub + Phase 2 AT command driver

/wokwi/
  diagram.json           ← circuit: ESP32 + NEO-6M + LEDs + buttons + buzzer
  comms_test_standalone.ino  ← test sketch (runs WITHOUT Person A or B code)

/test/
  test_comms_module.py   ← Python harness, 32 unit tests, 0 dependencies
```

---

### How to run in Wokwi (Phase 1)

1. Go to [wokwi.com](https://wokwi.com) → **New Project → ESP32**
2. Replace the auto-generated `diagram.json` with the contents of `/wokwi/diagram.json`
3. Replace the auto-generated sketch with `comms_test_standalone.ino`
4. Click **▶ Start Simulation**
5. Open the **Serial Monitor** — you will see:
   - GPS NMEA sentences being parsed, fix acquired
   - After 15 s: a formatted FALL ALERT SMS logged to serial
   - After 25 s: a formatted SOS SMS logged to serial

> **SIM800L gap:** Wokwi does not simulate SIM800L. That's intentional — `gsm_module.cpp` is in `GSM_STUB_MODE=1` (the default), which logs the exact formatted SMS that would be transmitted. The log output is the evidence for your review deck.

---

### How to run the Python test harness

```bash
python test/test_comms_module.py
```

Expected output: `Results: 32/32 passed`

Tests cover:
- NMEA XOR checksum validation
- `dddmm.mmmm` → decimal degrees conversion
- `$GPRMC` full parse (lat, lon, speed, course, time, date, fix status)
- `$GPGGA` partial parse (satellites, HDOP, altitude)
- Void (no-fix) sentence handling
- Bad checksum rejection
- Google Maps URL formatting (with and without fix)
- SMS message body — auto alert format
- SMS message body — manual SOS format
- GPS timeout detection logic

---

### Phase 1 → Phase 2 swap (one line change)

In `gsm_module.h`:

```cpp
// Phase 1 (Wokwi):
#define GSM_STUB_MODE 1    ← current

// Phase 2 (real SIM800L):
#define GSM_STUB_MODE 0    ← change this when you have hardware
```

Person B's `state_machine.cpp` does **not change at all**.

---

### Emergency number configuration

In `gsm_module.h`:
```cpp
#define EMERGENCY_NUMBER  "+919876543210"   // ← change before deploying
```

---

### Interface contract with Person B

Person B's state machine should `#include "../comms/gps_module.h"` and `#include "../comms/gsm_module.h"` and call:

| Function | When to call | Returns |
|---|---|---|
| `gps_init()` | `setup()` | void |
| `gsm_init()` | `setup()` | void |
| `gps_update()` | every `loop()` | bool (new sentence parsed) |
| `gsm_update()` | every `loop()` | void |
| `gps_has_fix()` | anytime | bool |
| `gps_timed_out()` | after fall detected | bool |
| `gsm_send_alert(gps_get_data())` | fall confirmed state | GsmResult |
| `gsm_send_sos(gps_get_data())` | SOS button pressed | GsmResult |

---

### Acceptance criteria coverage (from deck)

| Criterion | How this module addresses it |
|---|---|
| GPS fix within 30 s | `GPS_FIX_TIMEOUT_MS = 35000` with `gps_timed_out()` detection |
| Alert includes map link | `gps_format_map_url()` → Google Maps URL in SMS body |
| Alert includes timestamp | UTC time from `$GPRMC` parsed into `GpsData` |
| SMS sent on fall confirm | `gsm_send_alert()` triggered by state machine |
| Manual SOS bypass | `gsm_send_sos()` triggered directly from SOS button handler |
| No-fix graceful handling | SMS shows `"NO FIX"` placeholder instead of crashing |
