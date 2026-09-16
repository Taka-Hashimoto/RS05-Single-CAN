#include "RobStride05.h"
#include <string.h>

namespace {
bool inRange(float value, float low, float high) {
    return isfinite(value) && value >= low && value <= high;
}
uint16_t encode(float value, float low, float high) {
    return uint16_t((value - low) / (high - low) * 65535.0f);
}
float decode(const uint8_t *data, float low, float high) {
    return low + ((uint16_t(data[0]) << 8) | data[1]) / 65535.0f * (high - low);
}
uint32_t littleEndian(const uint8_t *data) {
    return uint32_t(data[0]) | uint32_t(data[1]) << 8 |
        uint32_t(data[2]) << 16 | uint32_t(data[3]) << 24;
}
}
RobStride05::RobStride05(UnoQCan &can, uint8_t motorId, uint8_t hostId)
    : can_(can), motorId_(motorId), hostId_(hostId) {}

bool RobStride05::begin() { return can_.begin(); }
bool RobStride05::send(uint8_t type, uint16_t idData, const uint8_t *data) {
    RobStrideFrame frame;
    frame.id = uint32_t(type) << 24 | uint32_t(idData) << 8 | motorId_;
    if (data) memcpy(frame.data, data, 8);
    return can_.send(frame);
}
bool RobStride05::enable() { return send(3, hostId_); }
bool RobStride05::disable() { return send(4, hostId_); }
bool RobStride05::torque(float nm) { return motion(0, 0, 0, 0, nm); }
bool RobStride05::motion(float position, float velocity, float kp, float kd, float torque) {
    if (!inRange(position, -PositionMax, PositionMax) ||
        !inRange(velocity, -VelocityMax, VelocityMax) ||
        !inRange(torque, -TorqueMax, TorqueMax) ||
        !inRange(kp, 0, 500) || !inRange(kd, 0, 5)) return false;
    uint16_t fields[] = {encode(position, -PositionMax, PositionMax),
        encode(velocity, -VelocityMax, VelocityMax), encode(kp, 0, 500), encode(kd, 0, 5)};
    uint8_t data[8];
    for (int i = 0; i < 4; ++i) {
        data[2 * i] = fields[i] >> 8;
        data[2 * i + 1] = fields[i];
    }
    return send(1, encode(torque, -TorqueMax, TorqueMax), data);
}
bool RobStride05::parameter(uint8_t type, uint16_t index, uint32_t value) {
    uint8_t data[8] = {uint8_t(index), uint8_t(index >> 8), 0, 0,
        uint8_t(value), uint8_t(value >> 8), uint8_t(value >> 16), uint8_t(value >> 24)};
    if (type == 8) data[2] = 4; // Legacy setting type: uint32. Subcommand 0 never saves to flash.
    return send(type, hostId_, data);
}
bool RobStride05::selectMotionMode() {
    modeConfirmed_ = false;
    return parameter(18, 0x7005);
}
bool RobStride05::setCommunicationTimeout(uint32_t milliseconds, bool legacyAccess) {
    if (!milliseconds || milliseconds > 5000) return false;
    timeoutConfirmed_ = false;
    timeoutTicks_ = milliseconds * 20;
    legacyTimeout_ = legacyAccess;
    return parameter(legacyTimeout_ ? 8 : 18, legacyTimeout_ ? 0x200C : 0x7028, timeoutTicks_);
}
bool RobStride05::requestConfiguration() {
    rejectedParameter_ = 0;
    modeConfirmed_ = false;
    timeoutConfirmed_ = false;
    bool modeSent = parameter(17, 0x7005);
    bool timeoutSent = parameter(legacyTimeout_ ? 9 : 17, legacyTimeout_ ? 0x200C : 0x7028);
    return modeSent && timeoutSent;
}
bool RobStride05::configurationConfirmed() const {
    return !rejectedParameter_ && modeConfirmed_ && timeoutConfirmed_;
}
bool RobStride05::requestVersion() {
    const uint8_t data[8] = {0, 0xC4};
    versionPending_ = send(4, hostId_, data);
    return versionPending_;
}
void RobStride05::update(uint32_t nowMs) {
    RobStrideFrame frame;
    for (int count = 0; count < 32 && can_.receive(frame); ++count) {
        if (!frame.extended || frame.remote || frame.fd || frame.length != 8 ||
            frame.id > 0x1FFFFFFF || uint8_t(frame.id >> 8) != motorId_ ||
            uint8_t(frame.id) != hostId_) continue;
        uint8_t type = frame.id >> 24;
        if (type == 17) {
            uint8_t result = frame.id >> 16 & 0xFF;
            if (result > 1 || frame.data[2] || frame.data[3]) continue;
            uint16_t index = uint16_t(frame.data[1]) << 8 | frame.data[0];
            if (index != 0x7005 && (index != 0x7028 || legacyTimeout_)) continue;
            if (result == 1) { rejectedParameter_ = index; continue; }
            uint32_t value = littleEndian(frame.data + 4);
            if (index == 0x7005) modeConfirmed_ = frame.data[4] == 0;
            if (index == 0x7028) timeoutConfirmed_ = timeoutTicks_ && value == timeoutTicks_;
        } else if (type == 9 && legacyTimeout_) {
            uint8_t result = frame.id >> 16 & 0xFF;
            if (result > 1 || frame.data[0] != 0x0C || frame.data[1] != 0x20 || frame.data[3]) continue;
            if (result == 1) { rejectedParameter_ = 0x200C; continue; }
            if (frame.data[2] != 4) continue;
            timeoutConfirmed_ = timeoutTicks_ && littleEndian(frame.data + 4) == timeoutTicks_;
        } else if (type == 21) {
            if (frame.id >> 16 & 0xFF) continue;
            faultDetails_ = littleEndian(frame.data);
            warnings_ = littleEndian(frame.data + 4);
        } else if (type == 2) {
            // Type 4 version replies also use Type 2, but contain no telemetry.
            if (frame.data[0] == 0 && frame.data[1] == 0xC4 && frame.data[2] == 0x56) {
                if (versionPending_) {
                    version_ = uint32_t(frame.data[3]) << 24 | uint32_t(frame.data[4]) << 16 |
                        uint32_t(frame.data[5]) << 8 | frame.data[6];
                    versionValid_ = true;
                    versionPending_ = false;
                }
                continue;
            }
            if ((frame.id >> 22 & 3) == 3) continue;
            position_ = decode(frame.data, -PositionMax, PositionMax);
            velocity_ = decode(frame.data + 2, -VelocityMax, VelocityMax);
            torque_ = decode(frame.data + 4, -TorqueMax, TorqueMax);
            temperature_ = (uint16_t(frame.data[6]) << 8 | frame.data[7]) * 0.1f;
            mode_ = frame.id >> 22 & 3;
            faults_ = frame.id >> 16 & 0x3F;
            receivedMs_ = nowMs;
            valid_ = true;
        }
    }
}
