#include "gps_module.h"
#include "comms_config.h"
#include <TinyGPS++.h>

static TinyGPSPlus   nmea;
static HardwareSerial gpsSerial(2);
static GpsFix        lastFix   = {0, 0, 0, 0};
static bool          everFixed = false;

void gps_init() {
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
}

void gps_update() {
  while (gpsSerial.available()) nmea.encode(gpsSerial.read());

  bool ok = nmea.location.isValid()
         && nmea.location.age() < GPS_MAX_AGE_MS
         && nmea.satellites.value() >= GPS_MIN_SATS;
  if (ok) {
    lastFix.lat       = nmea.location.lat();
    lastFix.lon       = nmea.location.lng();
    lastFix.sats      = nmea.satellites.value();
    lastFix.fixMillis = millis();
    everFixed = true;
  }
}

bool gps_get_fix(GpsFix *out) {
  if (!everFixed || millis() - lastFix.fixMillis > GPS_MAX_AGE_MS) return false;
  *out = lastFix;
  return true;
}

bool gps_get_last_known(GpsFix *out) {
  if (!everFixed) return false;
  *out = lastFix;
  return true;
}
