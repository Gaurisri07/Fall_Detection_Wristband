# Fall Detection Wristband

ESP32 wristband prototype combining Person A's fall-detection code with Person C's GPS/GSM communication modules and a Wokwi demonstration circuit.

## Repository contents

- `sensing/`: Person A's MPU6050 driver, fall detector, and synthetic-data tools.
- `comms/`: Person C's NEO-6M GPS parser and GSM message stub/driver.
- `src/main.cpp`: interactive Wokwi demo firmware; the timed fall/SOS events call Person C's alert functions and control the circuit indicators.
- `src/gps_module.cpp`, `src/gsm_module.cpp`: PlatformIO wrappers that compile the communications sources.
- `wokwi/`: circuit diagram and standalone communications self-test sketch.
- `diagram.json` and `wokwi.toml`: root Wokwi entry files used by VS Code.
- `test/test_comms_module.py`: Person C communications logic test harness.
- `src/demo_native.cpp`: Person A host-native fall-detector demo.

## Run the Wokwi demo

1. Build the ESP32 firmware with `pio run -e esp32dev` (or PlatformIO: Build in VS Code).
2. Open this repository folder in VS Code.
3. Open the root `diagram.json` with **Wokwi Diagram Editor** and press **Play**.
4. View Serial Monitor at 115200 baud.

The green STATUS LED turns on at startup. After 15 seconds, the demo simulates a fall and turns on the ALERT LED and buzzer. Press SOS for an immediate alert; press CANCEL to silence the alarm. If SOS was not pressed, a timed SOS runs at 25 seconds.

This Wokwi circuit has no MPU6050, so its fall event is timer-triggered; it does not run Person A's motion detector. GSM is in stub mode, so formatted alerts appear in Serial and no real SMS is transmitted. Person A's detector remains available in `sensing/` and through the native demo.

## Person A native demo and tuning

Run `./run_demo.sh` to compile and demonstrate the fall classifier without an ESP32 board. To tune thresholds, run `python3 sensing/testing_harness/tune_thresholds.py`.

## Person C communication tests

Run `python test/test_comms_module.py` (Python 3.8+, no external dependencies). It checks NMEA parsing, coordinate conversion, map URLs, alert text, and GPS timeout behavior.

For the classifier design and presentation notes, see [PRESENTATION_NOTES.md](PRESENTATION_NOTES.md).
