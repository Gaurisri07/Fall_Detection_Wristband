#include <Arduino.h>

#include "../comms/gps_module.h"
#include "../comms/gsm_module.h"

namespace {
constexpr uint8_t STATUS_LED_PIN = 19;
constexpr uint8_t ALERT_LED_PIN = 18;
constexpr uint8_t CANCEL_BUTTON_PIN = 13;
constexpr uint8_t SOS_BUTTON_PIN = 12;
constexpr uint8_t BUZZER_PIN = 21;
constexpr unsigned long BUTTON_DEBOUNCE_MS = 35;
constexpr unsigned long AUTO_FALL_AFTER_MS = 15000;
constexpr unsigned long AUTO_SOS_AFTER_MS = 25000;

struct Button {
    uint8_t pin;
    bool lastReading;
    bool stableReading;
    unsigned long lastChangeMs;
};

Button cancelButton{CANCEL_BUTTON_PIN, HIGH, HIGH, 0};
Button sosButton{SOS_BUTTON_PIN, HIGH, HIGH, 0};
unsigned long demoStartedMs = 0;
bool alarmActive = false;
bool autoFallSent = false;
bool sosSent = false;

bool buttonPressed(Button& button) {
    const bool reading = digitalRead(button.pin);
    const unsigned long now = millis();

    if (reading == LOW && button.stableReading == HIGH) {
        if (now - button.lastChangeMs >= 20) {
            button.stableReading = LOW;
            button.lastChangeMs = now;
            return true;
        }
    } else if (reading == HIGH && button.stableReading == LOW) {
        if (now - button.lastChangeMs >= 20) {
            button.stableReading = HIGH;
            button.lastChangeMs = now;
        }
    }

    return false;
}

void startAlarm(const char* reason) {
    alarmActive = true;
    Serial.println("[DEBUG] Setting Red Alert LED (Pin 18) to HIGH");
    digitalWrite(ALERT_LED_PIN, HIGH);
    tone(BUZZER_PIN, 2200);
    Serial.print("[ALERT] ");
    Serial.println(reason);
    Serial.println("[ALERT] Red LED and buzzer are ON. Press CANCEL to silence.");
}

void clearAlarm() {
    if (!alarmActive) {
        Serial.println("[UI] CANCEL pressed; no active alarm.");
        return;
    }

    alarmActive = false;
    digitalWrite(ALERT_LED_PIN, LOW);
    noTone(BUZZER_PIN);
    Serial.println("[UI] Alarm cancelled. Red LED and buzzer are OFF.");
}

void sendFallAlert() {
    startAlarm("Fall confirmed (demo event)");
    const GsmResult result = gsm_send_alert(gps_get_data());
    Serial.print("[TEST] Fall alert result: ");
    Serial.println(gsm_result_str(result));
}

void sendSosAlert() {
    sosSent = true;
    startAlarm("SOS triggered");
    const GsmResult result = gsm_send_sos(gps_get_data());
    Serial.print("[TEST] SOS result: ");
    Serial.println(gsm_result_str(result));
}
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(300);

    pinMode(STATUS_LED_PIN, OUTPUT);
    pinMode(ALERT_LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(CANCEL_BUTTON_PIN, INPUT_PULLUP);
    pinMode(SOS_BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(STATUS_LED_PIN, HIGH);
    digitalWrite(ALERT_LED_PIN, LOW);
    noTone(BUZZER_PIN);

    cancelButton.stableReading = HIGH;
    sosButton.stableReading = HIGH;
    cancelButton.lastChangeMs = sosButton.lastChangeMs = 0;

    Serial.println("Fall Detection Wristband - interactive Wokwi demo");
    Serial.println("STATUS LED is ON. SOS sends an alert; CANCEL silences it.");
    Serial.println("A simulated fall will trigger after 15 seconds.");
    Serial.println("A simulated SOS will trigger after 25 seconds unless SOS was pressed.");

    gps_init();
    gsm_init();
    demoStartedMs = millis();
}

void loop() {
    if (gps_update()) {
        const GpsData& gps = gps_get_data();
        Serial.printf("[GPS] Fix:%s Lat:%.5f Lon:%.5f Sats:%u\n",
                      gps_has_fix() ? "YES" : "no ",
                      static_cast<double>(gps.latitude),
                      static_cast<double>(gps.longitude),
                      static_cast<unsigned int>(gps.satellites));
    }

    if (buttonPressed(cancelButton)) {
        clearAlarm();
    }

    if (buttonPressed(sosButton)) {
        Serial.println("[UI] SOS button pressed.");
        sendSosAlert();
    }

    const unsigned long elapsed = millis() - demoStartedMs;
    if (!autoFallSent && elapsed >= AUTO_FALL_AFTER_MS) {
        autoFallSent = true;
        Serial.println("[DEMO] Simulating a fall now.");
        sendFallAlert();
    }

    if (!sosSent && elapsed >= AUTO_SOS_AFTER_MS) {
        Serial.println("[DEMO] Simulating an SOS button press now.");
        sendSosAlert();
    }

    gsm_update();
    delay(5);
}
