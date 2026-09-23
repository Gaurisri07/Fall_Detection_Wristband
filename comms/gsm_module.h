#pragma once
#include <Arduino.h>
#include "gps_module.h"

bool gsm_init();                                          // true = ready to send
bool gsm_send_sms(const char *number, const char *msg);   // raw SMS
// fix == nullptr -> "location unavailable" message
bool gsm_send_alert(const char *number, const GpsFix *fix);
