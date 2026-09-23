#pragma once
#include <Arduino.h>

struct GpsFix {
  double   lat;
  double   lon;
  uint8_t  sats;
  uint32_t fixMillis;   // millis() when this fix was captured
};

void gps_init();
void gps_update();                       // call every loop(), never block
bool gps_get_fix(GpsFix *out);           // true only if fix is valid AND fresh
bool gps_get_last_known(GpsFix *out);    // true if we EVER had a valid fix
