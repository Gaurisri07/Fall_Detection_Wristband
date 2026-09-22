#include "fall_detector.h"
#include <math.h>

FallDetector::FallDetector(const FallThresholds &thresholds)
  : thresh_(thresholds) {}

float FallDetector::magnitude(float x, float y, float z) {
  return sqrtf(x * x + y * y + z * z);
}

void FallDetector::reset() {
  state_ = NORMAL;
  freefall_t_ms_ = 0;
  stillness_start_ms_ = 0;
  fall_flagged_this_update_ = false;
}

bool FallDetector::fallDetected() {
  bool result = fall_flagged_this_update_;
  fall_flagged_this_update_ = false; // one-shot: caller must reset() to re-arm
  return result;
}

void FallDetector::update(const ImuSample &sample) {
  if (!sample.valid) return;

  float accel_mag = magnitude(sample.ax_g, sample.ay_g, sample.az_g);
  float gyro_mag = magnitude(sample.gx_dps, sample.gy_dps, sample.gz_dps);

  switch (state_) {

    case NORMAL:
      if (accel_mag < thresh_.freefall_g) {
        state_ = FREEFALL;
        freefall_t_ms_ = sample.t_ms;
      }
      break;

    case FREEFALL:
      // Must see impact within the allowed window, else the dip was just
      // arm-swing/noise and we drop back to normal monitoring.
      if (sample.t_ms - freefall_t_ms_ > thresh_.freefall_to_impact_ms) {
        state_ = NORMAL;
        break;
      }
      if (accel_mag > thresh_.impact_g) {
        state_ = IMPACT;
      }
      break;

    case IMPACT:
      // Impact itself causes a brief gyro spike that is NOT the same thing
      // as "still moving normally" -- so we don't evaluate stillness until
      // impact_settle_ms has passed. stillness_start_ms_ stays 0 until the
      // gyro actually drops below threshold post-settle.
      state_ = POST_FALL_STILL;
      impact_t_ms_ = sample.t_ms;
      stillness_start_ms_ = 0;
      break;

    case POST_FALL_STILL:
      if (sample.t_ms - impact_t_ms_ < thresh_.impact_settle_ms) {
        // Still settling from the impact itself; don't judge stillness yet.
        break;
      }
      if (sample.t_ms - impact_t_ms_ > thresh_.stillness_timeout_ms) {
        // Never settled down within a reasonable window -> probably not a
        // fall (e.g. still walking around after a hard bump). Back to normal.
        state_ = NORMAL;
        break;
      }
      if (gyro_mag > thresh_.stillness_gyro_dps) {
        // Moving again -> restart the stillness clock, but stay in this
        // state (don't bail to NORMAL on every brief post-impact twitch).
        stillness_start_ms_ = 0;
        break;
      }
      if (stillness_start_ms_ == 0) {
        stillness_start_ms_ = sample.t_ms; // stillness just began
        break;
      }
      if (sample.t_ms - stillness_start_ms_ >= thresh_.stillness_window_ms) {
        state_ = FALL_CONFIRMED;
        fall_flagged_this_update_ = true;
      }
      break;

    case FALL_CONFIRMED:
      // Latched until main firmware calls reset() (after grace-period
      // cancel or after the alert flow completes).
      break;
  }
}
