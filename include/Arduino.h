#ifndef ARDUINO_H_NATIVE_SHIM
#define ARDUINO_H_NATIVE_SHIM

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>

#ifndef ARDUINO
// Shim for host-native compilation (macOS/Linux) when building C++ code
// without the ESP32 Arduino framework installed.

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
        (void)addr; (void)quantity; (void)sendStop;
        return quantity;
    }
    int read() { return 0; }
};

extern TwoWire Wire;

#endif // ARDUINO

#endif // ARDUINO_H_NATIVE_SHIM
