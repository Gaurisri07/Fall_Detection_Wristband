#ifndef FALL_DETECTOR_H
#define FALL_DETECTOR_H

#include <Arduino.h>
#include "imu_driver.h"

enum FallState {
  NORMAL,
  FREEFALL,
  IMPACT,
  POST_FALL_STILL,
  FALL_CONFIRMED
};

// All tunable thresholds live here so the Python harness and the firmware
// share one source of truth. Update these from tune_thresholds.py output,
// not by hand-guessing on-device.
struct FallThresholds {
  float freefall_g        = 0.4f;   // magnitude below this = weightless
  float impact_g          = 2.5f;   // magnitude above this = impact
  uint32_t freefall_to_impact_ms = 500;  // max gap between freefall and impact
  float stillness_gyro_dps = 30.0f; // below this angular velocity = "still"
  uint32_t stillness_window_ms = 1500; // how long stillness must hold to confirm
  uint32_t impact_settle_ms = 150;  // ignore gyro noise from the impact itself
  uint32_t stillness_timeout_ms = 3000; // give up waiting for stillness after this long
};

class FallDetector {
public:
  explicit FallDetector(const FallThresholds &thresholds = FallThresholds());

  // Feed one IMU sample per loop iteration.
  void update(const ImuSample &sample);

  // True exactly once per fall event, on the loop iteration where
  // FALL_CONFIRMED is first reached. Main firmware should treat this as
  // "start the 10s grace period."
  bool fallDetected();

  FallState getState() const { return state_; }

  // Main firmware calls this after a user cancels during the grace period,
  // or after an alert has been sent, to start monitoring fresh.
  void reset();

private:
  FallThresholds thresh_;
  FallState state_ = NORMAL;
  bool fall_flagged_this_update_ = false;

  uint32_t freefall_t_ms_ = 0;
  uint32_t impact_t_ms_ = 0;
  uint32_t stillness_start_ms_ = 0; // 0 means "not yet started"

  static float magnitude(float x, float y, float z);
};

#endif // FALL_DETECTOR_H
