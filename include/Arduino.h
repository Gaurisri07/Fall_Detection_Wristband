#ifndef ARDUINO_H_NATIVE_SHIM
#define ARDUINO_H_NATIVE_SHIM

#ifdef ARDUINO
// For ESP32 builds, continue past this host-only shim and include the real
// Arduino framework header.
#include_next <Arduino.h>
#else
// Minimal host-native compatibility layer for Person A's fall-detector tests.
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>

inline uint32_t millis() {
    return 0;
}

inline void delay(uint32_t ms) {
    (void)ms;
}

class TwoWire {
public:
    void begin() {}
    void beginTransmission(uint8_t addr) { (void)addr; }
    uint8_t endTransmission(bool sendStop = true) { (void)sendStop; return 0; }
    size_t write(uint8_t data) { (void)data; return 1; }
    uint8_t requestFrom(uint8_t addr, uint8_t quantity, uint8_t sendStop = 1) {
        (void)addr; (void)sendStop;
        return quantity;
    }
    int read() { return 0; }
};

extern TwoWire Wire;
#endif

#endif
