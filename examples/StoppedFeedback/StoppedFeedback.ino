#include <Arduino_RouterBridge.h>
#include <RobStride05.h>

UnoQCan can;
RobStride05 motor(can, 0x7F);
bool ready;
uint32_t lastQuery;

void setup() {
    Serial.begin(115200);
    ready = motor.begin();
    if (!ready) Serial.println("CAN initialization failed");
}
void loop() {
    if (!ready) return;
    motor.update(millis());
    if (millis() - lastQuery < 100) return;
    lastQuery = millis();
    if (!motor.disable()) Serial.println("Stop TX rejected");
    if (motor.hasFeedback() && motor.feedbackAge(millis()) < 200) {
        Serial.println(motor.position(), 4);
    } else {
        Serial.println("No fresh feedback");
    }
}
