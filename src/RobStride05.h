#pragma once
#include "UnoQCan.h"
#include <math.h>
#include <stdint.h>

class RobStride05 {
public:
    static constexpr float PositionMax = 12.566370614359172f;
    static constexpr float VelocityMax = 50.0f;
    static constexpr float TorqueMax = 5.5f;
    RobStride05(UnoQCan &can, uint8_t motorId, uint8_t hostId = 0xFD);
    bool begin();
    bool enable();
    bool disable();
    bool torque(float nm);
    bool motion(float positionRad, float velocityRadS, float kp, float kd, float torqueNm);
    void update(uint32_t nowMs);
    bool selectMotionMode();
    bool setCommunicationTimeout(uint32_t milliseconds, bool legacyAccess = false);
    bool requestConfiguration();
    bool configurationConfirmed() const;
    uint16_t rejectedParameter() const { return rejectedParameter_; }
    bool requestVersion();
    bool hasVersion() const { return versionValid_; }
    uint32_t version() const { return version_; }
    bool hasFeedback() const { return valid_; }
    uint32_t feedbackAge(uint32_t nowMs) const { return valid_ ? nowMs - receivedMs_ : UINT32_MAX; }
    float position() const { return position_; }
    float velocity() const { return velocity_; }
    float torque() const { return torque_; }
    float temperature() const { return temperature_; }
    uint8_t mode() const { return mode_; }
    uint8_t faults() const { return faults_; }
    uint32_t faultDetails() const { return faultDetails_; }
    uint32_t warnings() const { return warnings_; }
    bool hasFault() const { return faults_ || faultDetails_ || warnings_; }
private:
    bool send(uint8_t type, uint16_t idData, const uint8_t *data = nullptr);
    bool parameter(uint8_t type, uint16_t index, uint32_t value = 0);
    UnoQCan &can_;
    uint8_t motorId_, hostId_;
    bool valid_ = false, modeConfirmed_ = false, timeoutConfirmed_ = false;
    bool legacyTimeout_ = false;
    bool versionPending_ = false, versionValid_ = false;
    uint16_t rejectedParameter_ = 0;
    uint32_t receivedMs_ = 0, timeoutTicks_ = 0, version_ = 0;
    float position_ = NAN, velocity_ = NAN, torque_ = NAN, temperature_ = NAN;
    uint8_t mode_ = 0, faults_ = 0;
    uint32_t faultDetails_ = 0, warnings_ = 0;
};
