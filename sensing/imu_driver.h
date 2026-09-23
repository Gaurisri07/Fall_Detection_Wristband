#ifndef IMU_DRIVER_H
#define IMU_DRIVER_H

#include <Arduino.h>
#include <Wire.h>

// MPU6050 register map (only what we need)
#define MPU6050_ADDR         0x68
#define REG_PWR_MGMT_1       0x6B
#define REG_ACCEL_CONFIG     0x1C
#define REG_GYRO_CONFIG      0x1B
#define REG_ACCEL_XOUT_H     0x3B

// Full-scale ranges we configure the sensor for.
// +-4g / +-500dps give enough headroom for impact spikes without losing
// resolution on normal wrist motion. Revisit after first real drop tests.
#define ACCEL_FS_SEL_4G      0x08
#define GYRO_FS_SEL_500DPS   0x08
#define ACCEL_SENSITIVITY_4G 8192.0f   // LSB per g
#define GYRO_SENSITIVITY_500 65.5f     // LSB per deg/s

struct ImuSample {
  float ax_g, ay_g, az_g;     // acceleration in g
  float gx_dps, gy_dps, gz_dps; // angular velocity in deg/s
  uint32_t t_ms;
  bool valid;
};

class ImuDriver {
public:
  // Call once in setup(). Returns false if the device doesn't ACK on I2C
  // (bad wiring, wrong address, or dead sensor) so the caller can flag a
  // hardware fault instead of silently reading garbage.
  bool begin(TwoWire &wire = Wire);

  // Blocking read of one sample. Call this every loop() iteration at your
  // target sampling rate (see SAMPLE_INTERVAL_MS below for the default).
  ImuSample read();

  bool isConnected() const { return connected_; }

private:
  TwoWire *wire_;
  bool connected_ = false;

  void writeRegister(uint8_t reg, uint8_t value);
  int16_t readWord(uint8_t reg_high);
};

// Recommended sampling interval. 20ms (~50Hz) is enough to catch an impact
// spike without flooding the fall-detector math or draining battery.
// Tune down if free-fall detection misses fast impacts in testing.
static const uint32_t SAMPLE_INTERVAL_MS = 20;

#endif // IMU_DRIVER_H
