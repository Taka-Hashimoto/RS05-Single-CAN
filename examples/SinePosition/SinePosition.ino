#include <Arduino_RouterBridge.h>
#include <RS05_Single_CAN.h>

RS05Motor motor(0x7F);
constexpr float Amplitude = PI / 2;
constexpr float Period = 20;
float center;
uint32_t started;

void setup() {
    pinMode(2, INPUT_PULLUP);
    Serial.begin(115200);
    if (!motor.begin()) Serial.println(motor.error());
    else Serial.println("Send r to start; D2 to GND stops motion.");
}

void loop() {
    bool wasActive = motor.active();
    motor.update();
    if (wasActive && !motor.active()) {
        Serial.println(motor.error());
        while (Serial.available()) Serial.read();
        return;
    }
    if (!motor.active()) {
        if (Serial.available() && Serial.read() == 'r' && digitalRead(2) == HIGH) {
            while (Serial.available()) Serial.read();
            if (motor.start()) { center = motor.position(); started = millis(); }
            else Serial.println(motor.error());
        }
        return;
    }

    float time = (millis() - started) * 0.001f;
    if (time >= Period || digitalRead(2) == LOW) {
        Serial.println(motor.stop() ? "Disabled" : motor.error());
        while (Serial.available()) Serial.read();
    } else {
        float omega = 2 * PI / Period;
        if (!motor.motion(center + Amplitude * sinf(omega * time),
                     Amplitude * omega * cosf(omega * time), 1.0f, 0.1f)) {
            Serial.println(motor.error());
            while (Serial.available()) Serial.read();
        }
    }
}
