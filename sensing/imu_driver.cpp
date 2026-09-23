#include "imu_driver.h"

bool ImuDriver::begin(TwoWire &wire) {
  wire_ = &wire;
  wire_->begin();

  // Wake the sensor (it powers on in sleep mode)
  writeRegister(REG_PWR_MGMT_1, 0x00);
  delay(50);

  // Configure full-scale ranges
  writeRegister(REG_ACCEL_CONFIG, ACCEL_FS_SEL_4G);
  writeRegister(REG_GYRO_CONFIG, GYRO_FS_SEL_500DPS);

  // Verify the device is actually present and responding
  wire_->beginTransmission(MPU6050_ADDR);
  connected_ = (wire_->endTransmission() == 0);
  return connected_;
}

void ImuDriver::writeRegister(uint8_t reg, uint8_t value) {
  wire_->beginTransmission(MPU6050_ADDR);
  wire_->write(reg);
  wire_->write(value);
  wire_->endTransmission();
}

int16_t ImuDriver::readWord(uint8_t reg_high) {
  wire_->beginTransmission(MPU6050_ADDR);
  wire_->write(reg_high);
  wire_->endTransmission(false);
  wire_->requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)2, (uint8_t) true);
  int16_t value = (wire_->read() << 8) | wire_->read();
  return value;
}

ImuSample ImuDriver::read() {
  ImuSample s;
  s.t_ms = millis();

  if (!connected_) {
    s.valid = false;
    return s;
  }

  // Accel/gyro registers are contiguous: AX,AY,AZ,TEMP,GX,GY,GZ
  int16_t raw_ax = readWord(REG_ACCEL_XOUT_H);
  int16_t raw_ay = readWord(REG_ACCEL_XOUT_H + 2);
  int16_t raw_az = readWord(REG_ACCEL_XOUT_H + 4);
  // skip temp register (REG_ACCEL_XOUT_H + 6)
  int16_t raw_gx = readWord(REG_ACCEL_XOUT_H + 8);
  int16_t raw_gy = readWord(REG_ACCEL_XOUT_H + 10);
  int16_t raw_gz = readWord(REG_ACCEL_XOUT_H + 12);

  s.ax_g = raw_ax / ACCEL_SENSITIVITY_4G;
  s.ay_g = raw_ay / ACCEL_SENSITIVITY_4G;
  s.az_g = raw_az / ACCEL_SENSITIVITY_4G;
  s.gx_dps = raw_gx / GYRO_SENSITIVITY_500;
  s.gy_dps = raw_gy / GYRO_SENSITIVITY_500;
  s.gz_dps = raw_gz / GYRO_SENSITIVITY_500;
  s.valid = true;

  return s;
}
