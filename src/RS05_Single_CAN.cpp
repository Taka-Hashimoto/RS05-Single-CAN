#include "RS05_Single_CAN.h"
#include <Arduino.h>

RS05Motor::RS05Motor(uint8_t motorId, uint8_t hostId) : motor_(can_, motorId, hostId) {}
uint32_t RS05Motor::feedbackAge() const { return motor_.feedbackAge(millis()); }

bool RS05Motor::begin(bool legacyTimeout) {
    if (active_) return fail("Already active");
    legacyTimeout_ = legacyTimeout;
    initialized_ = motor_.begin();
    if (!initialized_) { error_ = "CAN initialization failed"; return false; }
    error_ = "";
    return stop();
}

bool RS05Motor::stop() {
    active_ = false;
    position_ = velocity_ = kp_ = kd_ = torque_ = 0;
    if (!initialized_) { error_ = "Call begin first"; return false; }
    uint32_t start = millis();
    while (millis() - start < 300) {
        motor_.update(millis());
        bool sent = motor_.disable();
        delay(10);
        motor_.update(millis());
        if (sent && can_.healthy() && motor_.hasFeedback() &&
            feedbackAge() < 10 && motor_.mode() == 0) return true;
    }
    error_ = "Stop not confirmed";
    return false;
}

bool RS05Motor::fail(const char *message) {
    error_ = message;
    stop();
    return false;
}

bool RS05Motor::feedbackOk() {
    motor_.update(millis());
    if (!can_.healthy()) { error_ = "CAN error"; return false; }
    if (motor_.hasFault()) { error_ = "Motor fault"; return false; }
    if (!motor_.hasFeedback() || feedbackAge() >= 100) {
        error_ = "Feedback timeout";
        return false;
    }
    return true;
}

bool RS05Motor::setLimits(const Limits &limits) {
    if (active_) return fail("Set limits while stopped");
    if (!isfinite(limits.torque) || limits.torque <= 0 || limits.torque > RobStride05::TorqueMax ||
        !isfinite(limits.speed) || limits.speed <= 0 || limits.speed > RobStride05::VelocityMax ||
        !isfinite(limits.travel) || limits.travel <= 0 || limits.travel > RobStride05::PositionMax) {
        error_ = "Invalid limits";
        return false;
    }
    limits_ = limits;
    return true;
}

bool RS05Motor::start() {
    if (active_) return fail("Already active");
    error_ = "";
    if (!stop()) return false;
    if (!feedbackOk()) return false;
    bool sent = motor_.selectMotionMode();
    delay(10);
    sent = motor_.setCommunicationTimeout(150, legacyTimeout_) && sent;
    delay(10);
    sent = motor_.requestConfiguration() && sent;
    if (!sent) return fail("Configuration TX failed");
    uint32_t start = millis();
    while (millis() - start < 200 && !motor_.configurationConfirmed()) {
        if (!motor_.disable() || !feedbackOk()) return fail("Configuration communication failed");
        delay(10);
    }
    if (!motor_.configurationConfirmed()) return fail("Configuration not confirmed");
    if (!feedbackOk()) return false;
    center_ = motor_.position();
    if (fabsf(center_) + limits_.travel > RobStride05::PositionMax ||
        fabsf(motor_.velocity()) > 0.1f) return fail("Start position or speed out of range");
    if (!motor_.torque(0)) return fail("Zero command failed");
    delay(10);
    if (!motor_.enable()) return fail("Enable TX failed");
    start = millis();
    while (millis() - start < 100) {
        delay(10);
        if (!motor_.torque(0) || !feedbackOk()) return fail("Enable communication failed");
        if (motor_.mode() == 2) {
            position_ = center_;
            sentAt_ = targetAt_ = millis();
            active_ = true;
            return true;
        }
    }
    return fail("Enable not confirmed");
}

bool RS05Motor::motion(float position, float velocity, float kp, float kd, float torque) {
    if (!active_) { error_ = "Not active"; return false; }
    if (!isfinite(position) || fabsf(position) > RobStride05::PositionMax ||
        fabsf(position - center_) > limits_.travel ||
        !isfinite(velocity) || fabsf(velocity) > limits_.speed ||
        !isfinite(kp) || kp < 0 || kp > 500 || !isfinite(kd) || kd < 0 || kd > 5 ||
        !isfinite(torque) || fabsf(torque) > limits_.torque) return fail("Invalid target");
    position_ = position;
    velocity_ = velocity;
    kp_ = kp;
    kd_ = kd;
    torque_ = torque;
    targetAt_ = millis();
    return true;
}

bool RS05Motor::torque(float nm) { return motion(center_, 0, 0, 0, nm); }

void RS05Motor::update() {
    if (!initialized_) return;
    motor_.update(millis());
    uint32_t now = millis();
    if (!active_) {
        if (now - sentAt_ >= 100) {
            sentAt_ = now;
            if (!motor_.disable()) error_ = "Stop TX failed";
        }
        return;
    }
    if (!feedbackOk()) { fail(error_); return; }
    if (motor_.mode() != 2) { fail("Unexpected motor mode"); return; }
    if (now - sentAt_ > 30) { fail("Control loop delayed"); return; }
    if (now - targetAt_ >= 100) { fail("Target timeout"); return; }
    float effort = kp_ * fabsf(position_ - motor_.position()) +
                   kd_ * fabsf(velocity_ - motor_.velocity()) + fabsf(torque_);
    if (effort > limits_.torque || fabsf(motor_.velocity()) > limits_.speed ||
        fabsf(motor_.position() - center_) > limits_.travel) { fail("Motion limit exceeded"); return; }
    if (now - sentAt_ >= 10) {
        sentAt_ = now;
        if (!motor_.motion(position_, velocity_, kp_, kd_, torque_)) fail("Motion TX failed");
    }
}
