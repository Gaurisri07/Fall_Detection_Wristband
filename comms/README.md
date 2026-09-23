# /comms — Person C (GPS + GSM)

- `gps_module.*`  NEO-6M read, NMEA parsing (TinyGPSPlus), fix validation
- `gsm_module.*`  SIM800L AT-command driver + SMS formatting (stub in Phase 1)
- `comms_config.h`  pins + `GSM_USE_STUB` (1 = log to serial, 0 = real SIM800L)

## API (for Person B)
```cpp
gps_init(); gps_update();                          // call gps_update() every loop()
bool gps_get_fix(GpsFix *out);                     // valid + fresh only
bool gps_get_last_known(GpsFix *out);              // fallback
bool gsm_init();
bool gsm_send_alert(const char *number, const GpsFix *fix);  // nullptr = no location
```
```cpp
GpsFix f;
bool have = gps_get_fix(&f) || gps_get_last_known(&f);
gsm_send_alert(EMERGENCY_NUMBER, have ? &f : nullptr);
```
Library: TinyGPSPlus. Status: written, not yet compiled/tested in Wokwi.
