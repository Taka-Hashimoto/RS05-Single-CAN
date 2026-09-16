#include <Arduino_RouterBridge.h>
#include <RobStride05.h>

UnoQCan can;
RobStride05 motor(can, 0x7F);
constexpr float Pi = 3.14159265358979323846f;
constexpr float Amplitude = Pi / 2;
constexpr uint32_t CycleMs = 20000;
constexpr uint32_t CommandMs = 10;
constexpr uint32_t FeedbackTimeoutMs = 100;
constexpr float Kp = 1.0f;
constexpr float Kd = 0.1f;
constexpr float EffortLimit = 0.3f;
constexpr float SpeedLimit = 1.5f;
constexpr float TravelLimit = 100 * Pi / 180;
constexpr int StopPin = 2;
constexpr bool LegacyTimeout = true;
bool ready = false;
const char *result = "idle";

bool feedbackOk() {
    motor.update(millis());
    if (!can.healthy()) { result = "CAN error"; return false; }
    if (motor.hasFault()) { result = "motor fault"; return false; }
    if (!motor.hasFeedback() || motor.feedbackAge(millis()) > FeedbackTimeoutMs) {
        result = "feedback timeout";
        return false;
    }
    return true;
}

bool stopMotor() {
    uint32_t start = millis();
    uint32_t last = start - 20;
    bool sent = false;
    while (millis() - start < 300) {
        uint32_t now = millis();
        if (now - last >= 20) {
            last = now;
            if (motor.disable()) sent = true;
        }
        motor.update(now);
        if (sent && can.healthy() && motor.hasFeedback() && motor.mode() == 0 &&
            motor.feedbackAge(now) < 10 && motor.feedbackAge(now) < now - start) return true;
        delay(1);
    }
    return false;
}

bool prepare() {
    if (!stopMotor()) { result = "stop not confirmed"; return false; }
    if (!feedbackOk()) return false;
    result = "configuration TX failed";
    if (!motor.selectMotionMode()) return false;
    delay(10);
    if (!motor.setCommunicationTimeout(150, LegacyTimeout)) return false;
    delay(10);
    if (!motor.requestConfiguration()) return false;
    uint32_t start = millis();
    while (millis() - start < 200) {
        if (!feedbackOk()) return false;
        if (motor.configurationConfirmed()) return true;
        delay(1);
    }
    result = "configuration not confirmed";
    return false;
}

void runSine() {
    if (!prepare()) return;
    if (digitalRead(StopPin) == LOW) { result = "stop input active"; return; }
    float center = motor.position();
    if (fabsf(center) + TravelLimit > RobStride05::PositionMax) {
        result = "center too close to position boundary";
        return;
    }
    if (fabsf(motor.velocity()) > 0.1f) { result = "motor is moving"; return; }
    result = "start TX failed";
    if (!motor.torque(0)) return;
    delay(10);
    if (!motor.enable()) return;

    uint32_t start = millis();
    uint32_t last = start;
    bool running = false;
    result = "cycle complete";
    while (millis() - start < CycleMs) {
        uint32_t now = millis();
        if (digitalRead(StopPin) == LOW) { result = "stop input"; break; }
        if (!feedbackOk()) break;
        if (fabsf(motor.velocity()) > SpeedLimit) { result = "speed limit"; break; }
        if (fabsf(motor.position() - center) > TravelLimit) { result = "travel limit"; break; }
        if (motor.mode() == 2) running = true;
        else if (running || now - start > 100) { result = "unexpected motor mode"; break; }
        if (now - last > 30) { result = "control loop delayed"; break; }
        if (now - last >= CommandMs) {
            last = now;
            float omega = 2 * Pi / (CycleMs * 0.001f);
            float phase = omega * ((now - start) * 0.001f);
            float position = center + Amplitude * sinf(phase);
            float velocity = Amplitude * omega * cosf(phase);
            // Bound the estimated PD effort; this is not a motor current limit.
            float effort = Kp * fabsf(position - motor.position()) +
                           Kd * fabsf(velocity - motor.velocity());
            if (effort > EffortLimit) { result = "effort limit"; break; }
            if (!motor.motion(position, velocity, Kp, Kd, 0)) {
                result = "motion TX failed";
                break;
            }
        }
        delay(1);
    }
}

void setup() {
    pinMode(StopPin, INPUT_PULLUP);
    ready = motor.begin();
    if (ready) ready = stopMotor();
    Serial.begin(115200);
    Serial.println(ready ? "Stopped. Send r for one +/-90 degree cycle (20 s)." :
                           "CAN initialization or stop confirmation failed; reset to retry.");
}

void loop() {
    static uint32_t lastStop = 0;
    if (!ready) { delay(10); return; }
    motor.update(millis());
    if (millis() - lastStop >= 100) {
        lastStop = millis();
        if (!motor.disable() || !can.healthy()) {
            ready = false;
            Serial.println("Stop TX/CAN error; reset to retry.");
            return;
        }
    }
    if (Serial.available() && Serial.read() == 'r') {
        while (Serial.available()) Serial.read();
        runSine();
        bool stopped = stopMotor();
        Serial.println(result);
        Serial.println(stopped ? "Disabled. Send r to run again." : "STOP NOT CONFIRMED; remove motor power.");
        if (!stopped) ready = false;
        while (Serial.available()) Serial.read();
    }
    delay(1);
}
