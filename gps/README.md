# /gps — GPS Location Acquisition Module

## Scope
This module owns the NEO-6M GPS integration over UART (HardwareSerial).

## Responsibilities
- Parse NMEA sentence stream (using TinyGPS++ or equivalent library).
- Extract latitude, longitude, altitude, speed, HDOP, and satellite count.
- Expose latest valid coordinates to the main firmware for inclusion in alert SMS messages.

## Status
- **Status**: Scaffolding placeholder (assigned to teammate).
