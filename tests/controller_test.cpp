#include "RS05_Single_CAN.h"
#include <assert.h>
#include <deque>
#include <iostream>
#include <string>

uint32_t clockMs = 0;
uint32_t millis() { return clockMs; }
void delay(uint32_t ms) { clockMs += ms; }
std::deque<RobStrideFrame> frames;
bool respond = true, busHealthy = true, accept = true, enabled = false;
bool allowEnable = true, allowStop = true, configurationReplies = true;
unsigned enables = 0, disables = 0, commands = 0;
uint8_t faultBits = 0;
uint16_t speedRaw = 32767;

bool UnoQCan::begin() { return true; }
void UnoQCan::end() {}
bool UnoQCan::healthy() const { return busHealthy; }
bool UnoQCan::receive(RobStrideFrame &frame) {
    if (frames.empty()) return false;
    frame = frames.front(); frames.pop_front(); return true;
}
bool UnoQCan::send(const RobStrideFrame &frame) {
    if (!accept) return false;
    uint8_t type = frame.id >> 24;
    if (type == 3) { ++enables; if (allowEnable) enabled = true; }
    if (type == 4) { ++disables; if (allowStop) enabled = false; }
    if (type == 1) ++commands;
    if (!respond) return true;
    if (type == 1 || type == 3 || type == 4) {
        RobStrideFrame reply{0x02007FFD, {0x7F,0xFF, uint8_t(speedRaw >> 8),uint8_t(speedRaw),0x7F,0xFF,1,0x2C}};
        reply.id |= uint32_t(enabled ? 2 : 0) << 22 | uint32_t(faultBits) << 16;
        frames.push_back(reply);
    }
    if (type == 17 && configurationReplies) {
        if (frame.data[0] == 5) frames.push_back({0x11007FFD, {5,0x70,0,0,0,0,0,0}});
        else frames.push_back({0x11007FFD, {0x28,0x70,0,0,0xB8,0x0B,0,0}});
    }
    if (type == 9 && configurationReplies) frames.push_back({0x09007FFD, {0x0C,0x20,4,0,0xB8,0x0B,0,0}});
    return true;
}

int main() {
    RS05Motor motor(0x7F);
    assert(motor.begin() && !motor.active() && !enabled && enables == 0);
    assert(motor.hasFeedback() && motor.temperature() == 30);
    RS05Motor::Limits limits;
    limits.torque = NAN;
    assert(!motor.setLimits(limits));
    assert(motor.start() && motor.active() && enabled);
    assert(motor.motion(motor.position(), 0, 1, .1f));
    unsigned before = commands;
    delay(9); motor.update(); assert(commands == before);
    delay(1); motor.update(); assert(commands == before + 1);
    assert(motor.stop() && !motor.active() && !enabled);

    assert(motor.start());
    assert(!motor.torque(NAN) && !motor.active() && !enabled);
    assert(std::string(motor.error()) == "Invalid target");
    assert(motor.start());
    assert(motor.motion(motor.position() + 1, 0, 1, 0));
    delay(10); motor.update();
    assert(!motor.active() && !enabled && std::string(motor.error()) == "Motion limit exceeded");

    assert(motor.start());
    for (int i = 0; i < 10; ++i) { delay(10); motor.update(); }
    assert(!motor.active() && !enabled && std::string(motor.error()) == "Target timeout");
    unsigned previousEnables = enables;
    for (int i = 0; i < 20; ++i) { delay(10); motor.update(); }
    assert(enables == previousEnables);

    assert(motor.start());
    delay(31); motor.update();
    assert(!motor.active() && !enabled && std::string(motor.error()) == "Control loop delayed");
    assert(motor.start());
    faultBits = 1;
    delay(10); motor.update(); delay(10); motor.update();
    assert(!motor.active() && !enabled && std::string(motor.error()) == "Motor fault");
    faultBits = 0;
    assert(motor.start());
    respond = false;
    for (int i = 0; i < 12 && motor.active(); ++i) {
        assert(motor.torque(0)); delay(10); motor.update();
    }
    assert(!motor.active() && std::string(motor.error()) == "Stop not confirmed");
    respond = true;
    assert(motor.stop());

    allowEnable = false;
    assert(!motor.start() && !motor.active() && !enabled);
    allowEnable = true;
    assert(motor.start());
    allowStop = false;
    assert(!motor.stop() && !motor.active() && enabled);
    allowStop = true;
    assert(motor.stop());

    assert(motor.start());
    busHealthy = false;
    unsigned previousDisables = disables;
    motor.update();
    assert(!motor.active() && disables > previousDisables);
    busHealthy = true;
    assert(motor.stop());
    assert(motor.begin(false) && motor.start());
    assert(motor.stop());
    configurationReplies = false;
    previousEnables = enables;
    assert(!motor.start() && enables == previousEnables && !enabled);
    assert(std::string(motor.error()) == "Configuration not confirmed");
    configurationReplies = true;
    assert(motor.begin());
    assert(motor.start());
    assert(!motor.motion(motor.position() + 2, 0, 0, 0) && !enabled);
    assert(motor.start());
    speedRaw = 35000;
    delay(10); motor.update(); delay(10); motor.update();
    assert(!motor.active() && !enabled && std::string(motor.error()) == "Motion limit exceeded");
    speedRaw = 32767;
    assert(motor.stop());
    clockMs = UINT32_MAX - 50;
    assert(motor.start());
    assert(motor.torque(0)); delay(10); motor.update();
    assert(motor.active());
    assert(motor.stop());
    std::cout << "PASS: controller start/stop, scheduling, limits, faults, timeouts, no restart, time wrap\n";
}
