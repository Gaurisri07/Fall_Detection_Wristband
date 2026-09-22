#include <Arduino.h>
#include "imu_driver.h"
#include "fall_detector.h"

ImuDriver imu;
FallDetector detector;

FallState lastState = NORMAL;
uint32_t lastSampleTime = 0;

const char* getStateName(FallState state) {
  switch (state) {
    case NORMAL:          return "NORMAL";
    case FREEFALL:        return "FREEFALL";
    case IMPACT:          return "IMPACT";
    case POST_FALL_STILL: return "POST_FALL_STILL";
    case FALL_CONFIRMED:  return "FALL_CONFIRMED";
    default:              return "UNKNOWN";
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000); // Wait briefly for Serial monitor on native USB boards

  Serial.println();
  Serial.println("==================================================");
  Serial.println("   FALL DETECTION WRISTBAND - FIRMWARE CORE      ");
  Serial.println("==================================================");

  if (imu.begin()) {
    Serial.println("[IMU] MPU6050 initialized successfully over I2C.");
  } else {
    Serial.println("[ERROR] MPU6050 not detected on I2C bus! Check wiring.");
  }

  Serial.println("[SYSTEM] Fall detector initialized. Monitoring motion...");
  Serial.println("==================================================");
}

void loop() {
  uint32_t now = millis();

  // Enforce 50Hz sampling loop (20ms interval)
  if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = now;

    ImuSample sample = imu.read();
    detector.update(sample);

    FallState currentState = detector.getState();

    // Print state transitions with timestamps over Serial
    if (currentState != lastState) {
      Serial.printf("[%6u ms] STATE TRANSITION: %s -> %s\n",
                    now, getStateName(lastState), getStateName(currentState));
      lastState = currentState;
    }

    // Triggered once on entering FALL_CONFIRMED
    if (detector.fallDetected()) {
      Serial.println();
      Serial.println("**************************************************");
      Serial.printf("[%6u ms] *** FALL CONFIRMED! ***\n", now);
      Serial.println("          Handing off to main alert workflow:     ");
      Serial.println("          -> Initiating 10s grace period          ");
      Serial.println("          -> Sounding buzzer & flashing LED       ");
      Serial.println("**************************************************");
      Serial.println();
    }
  }
}
