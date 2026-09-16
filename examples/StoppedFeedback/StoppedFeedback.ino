#include <Arduino_RouterBridge.h>
#include <RS05_Single_CAN.h>

RS05Motor motor(0x7F);

void setup() {
    Serial.begin(115200);
    if (!motor.begin()) Serial.println(motor.error());
}

void loop() {
    motor.update();
    if (motor.hasFeedback() && motor.feedbackAge() < 200) {
        Serial.println(motor.position(), 4);
    } else {
        Serial.println("No fresh feedback");
    }
    delay(100);
}
