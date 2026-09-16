#pragma once
#include "RobStride05.h"

class RS05Motor {
public:
    struct Limits {
        float torque = 0.3f;
        float speed = 1.5f;
        float travel = 1.745329f;
    };
    explicit RS05Motor(uint8_t motorId, uint8_t hostId = 0xFD);
    bool begin(bool legacyTimeout = true);
    bool start();
    bool stop();
    void update();
    bool setLimits(const Limits &limits);
    bool motion(float position, float velocity, float kp, float kd, float torque = 0);
    bool torque(float nm);
    bool active() const { return active_; }
    bool hasFeedback() const { return motor_.hasFeedback(); }
    uint32_t feedbackAge() const;
    float position() const { return motor_.position(); }
    float velocity() const { return motor_.velocity(); }
    float torque() const { return motor_.torque(); }
    float temperature() const { return motor_.temperature(); }
    uint8_t faults() const { return motor_.faults(); }
    uint32_t faultDetails() const { return motor_.faultDetails(); }
    uint32_t warnings() const { return motor_.warnings(); }
    const char *error() const { return error_; }
private:
    bool feedbackOk();
    bool fail(const char *message);
    UnoQCan can_;
    RobStride05 motor_;
    Limits limits_;
    bool initialized_ = false, active_ = false, legacyTimeout_ = true;
    float center_ = 0, position_ = 0, velocity_ = 0, kp_ = 0, kd_ = 0, torque_ = 0;
    uint32_t sentAt_ = 0, targetAt_ = 0;
    const char *error_ = "";
};
