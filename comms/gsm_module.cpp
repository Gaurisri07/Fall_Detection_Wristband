#include "gsm_module.h"
#include "comms_config.h"

// ---------- message formatting (same in stub + real) ----------
static void build_alert(char *out, size_t n, const GpsFix *fix) {
  if (fix) {
    snprintf(out, n,
      "FALL DETECTED! Needs help. Location: https://maps.google.com/?q=%.6f,%.6f",
      fix->lat, fix->lon);
  } else {
    snprintf(out, n, "FALL DETECTED! Needs help. Location unavailable (no GPS fix).");
  }
}

bool gsm_send_alert(const char *number, const GpsFix *fix) {
  char msg[161];
  build_alert(msg, sizeof(msg), fix);
  return gsm_send_sms(number, msg);
}

#if GSM_USE_STUB
// ================= PHASE 1: STUB =================
bool gsm_init() {
  Serial.println("[GSM STUB] init ok");
  return true;
}

bool gsm_send_sms(const char *number, const char *msg) {
  Serial.printf("[GSM STUB] SMS to %s: %s\n", number, msg);
  return true;
}

#else
// ================= PHASE 2: REAL SIM800L =================
static HardwareSerial gsmSerial(1);

static bool waitFor(const char *token, uint32_t timeoutMs) {
  String buf;
  uint32_t t = millis();
  while (millis() - t < timeoutMs) {
    while (gsmSerial.available()) {
      buf += (char)gsmSerial.read();
      if (buf.indexOf(token) >= 0) return true;
    }
    delay(1);
  }
  return false;
}

static bool sendCmd(const char *cmd, const char *expect = "OK", uint32_t to = 2000) {
  while (gsmSerial.available()) gsmSerial.read();   // flush
  gsmSerial.print(cmd);
  gsmSerial.print("\r");
  return waitFor(expect, to);
}

bool gsm_init() {
  gsmSerial.begin(GSM_BAUD, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);
  delay(1000);

  bool alive = false;
  for (int i = 0; i < 5 && !alive; i++) alive = sendCmd("AT");
  if (!alive) { Serial.println("[GSM] no response"); return false; }

  sendCmd("ATE0");            // echo off
  sendCmd("AT+CMGF=1");       // text mode

  // wait up to 30s for network registration (home=,1 roaming=,5)
  for (int i = 0; i < 30; i++) {
    if (sendCmd("AT+CREG?", ",1", 1000) || sendCmd("AT+CREG?", ",5", 1000)) {
      Serial.println("[GSM] registered");
      return true;
    }
    delay(1000);
  }
  Serial.println("[GSM] no network");
  return false;
}

bool gsm_send_sms(const char *number, const char *msg) {
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(number);
  gsmSerial.print("\"\r");
  if (!waitFor(">", 5000)) return false;
  gsmSerial.print(msg);
  gsmSerial.write(0x1A);                 // Ctrl+Z = send
  bool ok = waitFor("+CMGS:", 30000);
  Serial.println(ok ? "[GSM] SMS sent" : "[GSM] SMS failed");
  return ok;
}
#endif
