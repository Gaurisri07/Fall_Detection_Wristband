#ifndef WIRE_H_NATIVE_SHIM
#define WIRE_H_NATIVE_SHIM

#ifdef ARDUINO
// For ESP32, use the Arduino core's actual I2C Wire interface.
#include_next <Wire.h>
#else
#include "Arduino.h"
#endif

#endif
