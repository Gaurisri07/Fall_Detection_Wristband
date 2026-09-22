# /sensing — Sensing & Fall Detection Core (Person A)

## Scope
This module owns motion sensing and fall classification only. It does NOT
own the grace period, buzzer/LED timing, GPS, SMS, or button handling —
that's the main firmware's job. The only thing that crosses the boundary
is the `FallDetector` interface below.

## Files
- `imu_driver.h/.cpp` — MPU6050 I2C read loop, raw registers -> g's and deg/s
- `fall_detector.h/.cpp` — 4-stage classifier: freefall -> impact -> settle -> stillness -> confirmed
- `testing_harness/synthetic_fall_data.py` — generates labeled synthetic IMU traces
- `testing_harness/tune_thresholds.py` — Python port of the same state machine, used to grid-search thresholds against synthetic data. **Keep this in sync with fall_detector.cpp by hand** — there's no code generation between them yet.

## Interface contract (what the rest of the team needs to know)
```cpp
FallDetector detector;                 // construct once, defaults from FallThresholds
detector.update(imu.read());           // call every loop() iteration
if (detector.fallDetected()) {         // one-shot true on the iteration FALL_CONFIRMED is reached
    // main firmware: start the 10s grace period / buzzer here
}
// ... if user cancels during grace period, or after alert flow finishes:
detector.reset();
```

## Running the tuning harness
```
cd testing_harness
python3 synthetic_fall_data.py   # writes labeled CSVs to ./data/ (optional, for inspection)
python3 tune_thresholds.py       # grid search, prints top 10 threshold combos + best candidate
```
Copy the winning thresholds into `FallThresholds` at the top of `fall_detector.h`.

## Known limitation — read before hardware testing
The synthetic scenarios are intentionally simple (walking, arm swing, hard
sit, one canonical fall shape). Hitting 100% sensitivity/specificity here
is a sanity check, NOT proof the classifier works. The real false-positive
sources that matter:
- taking the wristband off / putting it down hard
- getting into a car, sitting on a bike
- dropping the arm fast while gesturing

None of these are modeled yet. Next step once hardware arrives: log real
IMU traces for these motions, add them as new labeled scenarios in
`synthetic_fall_data.py`, and re-run the sweep before trusting any
threshold set from synthetic data alone.

## Next steps (in priority order)
1. Flash `imu_driver` alone, confirm `isConnected()` is true, log raw values over serial
2. Confirm accel magnitude reads ~1.0g at rest (sanity check on scaling/wiring)
3. Log real arm-swing and sit-down traces, add as harness scenarios
4. Log real (padded/controlled) drop-test traces, re-tune thresholds
5. Hand final `FallThresholds` values to the main firmware owner along with this interface doc
