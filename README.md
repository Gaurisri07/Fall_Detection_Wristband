# Fall Detection Wristband

An IoT wearable fall-detection and emergency alert system built on the **ESP32 DevKit V1**, **MPU6050 6-axis IMU**, **NEO-6M GPS module**, and **SIM800L GSM module**.

---

## 📌 Repository Structure

```
Fall_Detection_Wristband/
├── platformio.ini              # PlatformIO project config (ESP32 DevKit V1 & Native envs)
├── run_demo.sh                 # Helper script to compile & launch live host demo
├── PRESENTATION_NOTES.md       # Presentation notes, algorithm breakdown & false-negative bug details
├── README.md                   # Root project documentation
├── .gitignore                  # PlatformIO, Python, and OS ignore rules
│
├── sensing/                    # Sensing & Fall Detection Core (Person A)
│   ├── imu_driver.h/.cpp       # MPU6050 I2C driver & raw register conversion
│   ├── fall_detector.h/.cpp    # 4-stage classifier (Freefall -> Impact -> Settle -> Stillness)
│   ├── README.md               # Module scope & interface contract documentation
│   └── testing_harness/
│       ├── synthetic_fall_data.py # Synthetic IMU trace generator
│       └── tune_thresholds.py     # Threshold grid-search tuning harness
│
├── src/                        # Firmware & Demo Executables
│   ├── main.cpp                # ESP32 firmware loop (IMU read + Serial state logger)
│   └── demo_native.cpp         # Host-native presentation runner (runs C++ code without board)
│
├── include/                    # Native Compatibility Headers
│   ├── Arduino.h               # Native build compatibility shim
│   └── Wire.h                  # Native I2C compatibility shim
│
├── gps/                        # [Placeholder] GPS Location Acquisition (Teammate Module)
│   └── README.md
├── gsm/                        # [Placeholder] Cellular SMS/Alert Dispatch (Teammate Module)
│   └── README.md
└── firmware-main/              # [Placeholder] System State Machine & Grace Period (Teammate Module)
    └── README.md
```

---

## 🚀 Quick Start & Live Presentation Demo

### 1. Run Live Demo (No Hardware Required)
To demo the fall detection classifier live in class without an ESP32 board:

```bash
./run_demo.sh
```

This compiles `src/demo_native.cpp` natively against the exact `sensing/fall_detector.cpp` C++ logic and streams timestamped state transitions with narration output.

### 2. Build ESP32 Firmware (PlatformIO)
Targeting **ESP32 DevKit V1** over USB Serial (115200 baud):

```bash
# Build firmware binary
pio run -e esp32dev

# Upload to board and open Serial monitor
pio run -e esp32dev --target upload --target monitor
```

### 3. Run Threshold Tuning Harness
To run the threshold grid search against synthetic scenarios:

```bash
cd sensing/testing_harness
python3 tune_thresholds.py
```

---

## 📊 Fall Detection Algorithm Overview

The classifier uses a 4-stage state machine:

1. **`NORMAL`**: Monitors baseline accelerometer magnitude ($||a|| \approx 1.0\text{g}$).
2. **`FREEFALL`**: Detects weightlessness ($||a|| < 0.4\text{g}$).
3. **`IMPACT`**: Detects deceleration collision spike ($||a|| > 2.5\text{g}$) within 500ms of freefall.
4. **`POST_FALL_STILL`**: Applies a **150ms impact-settle buffer** to ignore kinetic impact gyro noise, then verifies $1500\text{ms}$ of sustained low angular velocity ($||\omega|| < 30\text{ dps}$).
5. **`FALL_CONFIRMED`**: Latches fall detection state and notifies main firmware to initiate the 10-second grace period.

For complete presentation talking points and false-negative bug analysis, see [PRESENTATION_NOTES.md](PRESENTATION_NOTES.md).
