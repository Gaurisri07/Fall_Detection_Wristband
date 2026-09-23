#pragma once
// ---- Phase switch: 1 = stub (Wokwi / no hardware), 0 = real SIM800L ----
#define GSM_USE_STUB 1

// ---- GPS (NEO-6M) on UART2 ----
#define GPS_RX_PIN 16          // ESP32 RX  <- GPS TX
#define GPS_TX_PIN 17          // ESP32 TX  -> GPS RX
#define GPS_BAUD 9600
#define GPS_MAX_AGE_MS 5000    // fix older than this = stale
#define GPS_MIN_SATS 0         // raise to 3+ on real hardware

// ---- GSM (SIM800L) on UART1 ----
#define GSM_RX_PIN 26          // ESP32 RX  <- SIM800L TX
#define GSM_TX_PIN 27          // ESP32 TX  -> SIM800L RX
#define GSM_BAUD 9600
