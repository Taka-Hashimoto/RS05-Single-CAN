#include <Arduino_RouterBridge.h>
#include <RobStride05.h>

UnoQCan can;
RobStride05 motor(can, 0x7F);
constexpr float Amplitude = PI / 2;   // radians: 90 degrees
constexpr float Period = 20;          // seconds per cycle
constexpr float Kp = 1.0f;
constexpr float Kd = 0.1f;
constexpr int StopPin = 2;
bool ready = false;
bool running = false;
float center;
uint32_t started, lastCommand;

bool stop(const char *message) {
    running = false;
    uint32_t start = millis();
    bool confirmed = false;
    while (millis() - start < 300 && !confirmed) {
        bool sent = motor.disable();
        delay(10);
        motor.update(millis());
        confirmed = sent && can.healthy() && motor.hasFeedback() &&
                    motor.feedbackAge(millis()) < 10 && motor.mode() == 0;
    }
    Serial.println(message);
    Serial.println(confirmed ? "Disabled. Send r to start." : "Stop unconfirmed; remove motor power.");
    if (!confirmed) ready = false;
    while (Serial.available()) Serial.read();
    return confirmed;
}

void setup() {
    pinMode(StopPin, INPUT_PULLUP);
    Serial.begin(115200);
    if (!motor.begin()) { Serial.println("CAN initialization failed"); return; }
    if (!stop("Starting disabled")) return;
    bool configured = motor.selectMotionMode();
    delay(10);
    configured = motor.setCommunicationTimeout(150, true) && configured;
    delay(10);
    configured = motor.requestConfiguration() && configured;
    for (int i = 0; i < 20; ++i) {
        motor.disable();
        delay(10);
        motor.update(millis());
    }
    ready = configured && motor.configurationConfirmed() && can.healthy();
    if (!ready) Serial.println("Configuration failed; reset to retry.");
}

void loop() {
    if (!ready) { delay(10); return; }
    motor.update(millis());
    bool feedbackOk = can.healthy() && motor.hasFeedback() &&
                      motor.feedbackAge(millis()) < 100 && !motor.hasFault();

    if (!running) {
        if (!motor.disable()) { ready = false; stop("CAN TX failed"); return; }
        if (Serial.available() && Serial.read() == 'r') {
            while (Serial.available()) Serial.read();
            if (!feedbackOk || digitalRead(StopPin) == LOW || fabsf(motor.velocity()) > 0.1f) {
                Serial.println("Not ready to start");
                return;
            }
            center = motor.position();
            if (fabsf(center) + Amplitude + 0.175f > RobStride05::PositionMax) {
                Serial.println("Position too close to boundary");
                return;
            }
            if (!motor.torque(0) || !motor.enable()) { stop("Start TX failed"); return; }
            started = lastCommand = millis();
            running = true;
        }
        delay(10);
        return;
    }

    uint32_t now = millis();
    float time = (now - started) * 0.001f;
    if (time >= Period) { stop("Cycle complete"); return; }
    if (!feedbackOk || digitalRead(StopPin) == LOW || now - lastCommand > 30 ||
        (time > 0.1f && motor.mode() != 2)) {
        stop("Stopped: button, feedback, motor mode or CAN error");
        return;
    }
    if (now - lastCommand < 10) return;
    lastCommand = now;

    float omega = 2 * PI / Period;
    float position = center + Amplitude * sinf(omega * time);
    float velocity = Amplitude * omega * cosf(omega * time);
    float effort = Kp * fabsf(position - motor.position()) + Kd * fabsf(velocity - motor.velocity());
    if (effort > 0.3f || fabsf(motor.velocity()) > 1.5f ||
        fabsf(motor.position() - center) > Amplitude + 0.175f) {
        stop("Motion limit reached");
        return;
    }
    if (!motor.motion(position, velocity, Kp, Kd, 0)) stop("Motion TX failed");
}
