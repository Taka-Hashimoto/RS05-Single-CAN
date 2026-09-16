#include "RobStride05.h"
#include <assert.h>
#include <deque>
#include <iostream>
#include <limits>
#include <vector>

std::deque<RobStrideFrame> received;
std::vector<RobStrideFrame> sent;
bool acceptTx = true;
bool UnoQCan::begin() { return true; }
bool UnoQCan::send(const RobStrideFrame &f) { if (acceptTx) sent.push_back(f); return acceptTx; }
bool UnoQCan::receive(RobStrideFrame &f) {
    if (received.empty()) return false;
    f = received.front(); received.pop_front(); return true;
}
bool UnoQCan::healthy() const { return true; }
void UnoQCan::end() {}

void expect(uint32_t id, std::initializer_list<uint8_t> data) {
    const auto &f = sent.back();
    assert(f.id == id && f.extended && !f.remote && !f.fd && f.length == 8);
    assert(std::vector<uint8_t>(f.data, f.data + 8) == std::vector<uint8_t>(data));
}
int main() {
    UnoQCan can;
    RobStride05 motor(can, 0x7F);
    assert(motor.begin());
    assert(!motor.hasFeedback() && isnan(motor.position()) && motor.feedbackAge(0) == UINT32_MAX);
    assert(motor.enable()); expect(0x0300FD7F, {0,0,0,0,0,0,0,0});
    assert(motor.disable()); expect(0x0400FD7F, {0,0,0,0,0,0,0,0});
    assert(motor.torque(0)); expect(0x017FFF7F, {0x7F,0xFF,0x7F,0xFF,0,0,0,0});
    assert(motor.motion(-RobStride05::PositionMax, 50, 500, 5, 5.5f));
    expect(0x01FFFF7F, {0,0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF});
    assert(motor.motion(RobStride05::PositionMax, -50, 0, 0, -5.5f));
    expect(0x0100007F, {0xFF,0xFF,0,0,0,0,0,0});
    assert(motor.motion(0, 0, 250, 2.5f, 0));
    expect(0x017FFF7F, {0x7F,0xFF,0x7F,0xFF,0x7F,0xFF,0x7F,0xFF});
    size_t count = sent.size();
    assert(!motor.torque(NAN) && !motor.torque(INFINITY) && !motor.torque(5.501f));
    assert(!motor.motion(13, 0, 0, 0, 0) && !motor.motion(0, 51, 0, 0, 0));
    assert(!motor.motion(0, 0, -1, 0, 0) && !motor.motion(0, 0, 0, 5.01f, 0));
    assert(count == sent.size());
    acceptTx = false; assert(!motor.disable()); acceptTx = true;
    assert(motor.selectMotionMode()); expect(0x1200FD7F, {5,0x70,0,0,0,0,0,0});
    assert(motor.setCommunicationTimeout(150)); expect(0x1200FD7F, {0x28,0x70,0,0,0xB8,0x0B,0,0});
    assert(!motor.setCommunicationTimeout(0));
    assert(motor.requestConfiguration());
    expect(0x1100FD7F, {0x28,0x70,0,0,0,0,0,0});
    RobStrideFrame mode{0x11007FFD, {5,0x70,0,0,0,0,0,0}};
    RobStrideFrame timeout{0x11007FFD, {0x28,0x70,0,0,0xB8,0x0B,0,0}};
    received = {mode, timeout}; motor.update(1); assert(motor.configurationConfirmed());
    assert(motor.requestConfiguration()); assert(!motor.configurationConfirmed());
    timeout.id |= 1UL << 16; received = {mode, timeout}; motor.update(2); assert(!motor.configurationConfirmed());
    assert(motor.rejectedParameter() == 0x7028);
    assert(motor.requestConfiguration() && motor.rejectedParameter() == 0);
    timeout.id = 0x11007FFD; timeout.data[4] = 0;
    received = {timeout}; motor.update(3); assert(!motor.configurationConfirmed());

    assert(motor.setCommunicationTimeout(150, true));
    expect(0x0800FD7F, {0x0C,0x20,4,0,0xB8,0x0B,0,0});
    assert(!motor.setCommunicationTimeout(5001, true));
    assert(motor.requestConfiguration());
    expect(0x0900FD7F, {0x0C,0x20,0,0,0,0,0,0});
    RobStrideFrame legacy{0x09007FFD, {0x0C,0x20,4,0,0xB8,0x0B,0,0}};
    received = {mode}; motor.update(4);
    for (int mutation = 0; mutation < 5; ++mutation) {
        auto bad = legacy;
        if (mutation == 0) bad.data[2] = 3;
        if (mutation == 1) bad.data[3] = 1;
        if (mutation == 2) bad.data[4] = 0;
        if (mutation == 3) bad.id = 0x09007EFD;
        if (mutation == 4) bad.id = 0x08007FFD;
        received = {bad}; motor.update(5); assert(!motor.configurationConfirmed());
    }
    received = {legacy}; motor.update(6); assert(motor.configurationConfirmed());
    assert(motor.requestConfiguration());
    legacy.id = 0x09017FFD;
    received = {legacy}; motor.update(7);
    assert(motor.rejectedParameter() == 0x200C && !motor.configurationConfirmed());

    RobStrideFrame feedback{0x02807FFD, {0,0,0xFF,0xFF,0xFF,0xFF,0x01,0x2C}};
    received = {feedback}; motor.update(100);
    assert(motor.hasFeedback() && motor.mode() == 2 && !motor.hasFault());
    assert(fabs(motor.position() + RobStride05::PositionMax) < 0.0001);
    assert(motor.velocity() == 50 && motor.torque() == 5.5f && motor.temperature() == 30);
    assert(motor.feedbackAge(125) == 25);
    for (int mutation = 0; mutation < 10; ++mutation) {
        auto bad = feedback;
        switch (mutation) {
            case 0: bad.id = 0x02807EFD; break;
            case 1: bad.id = 0x02807FFE; break;
            case 2: bad.extended = false; break;
            case 3: bad.length = 7; break;
            case 4: bad.length = 9; break;
            case 5: bad.remote = true; break;
            case 6: bad.fd = true; break;
            case 7: bad.id |= 0x20000000; break;
            case 8: bad.id = 0x03807FFD; break;
            case 9: bad.id = 0x02C07FFD; break;
        }
        received = {bad}; motor.update(200);
        assert(motor.feedbackAge(200) == 100 && motor.temperature() == 30);
    }
    assert(motor.requestVersion()); expect(0x0400FD7F, {0,0xC4,0,0,0,0,0,0});
    RobStrideFrame version{0x02007FFD, {0,0xC4,0x56,1,2,3,4,0}};
    received = {version}; motor.update(250);
    assert(motor.hasVersion() && motor.version() == 0x01020304 && motor.feedbackAge(250) == 150);
    feedback.id = 0x02017FFD;
    feedback.data[6] = feedback.data[7] = 0;
    received = {feedback}; motor.update(UINT32_MAX - 10);
    assert(motor.hasFeedback() && motor.temperature() == 0 && motor.faults() == 1);
    assert(motor.feedbackAge(9) == 20);
    RobStrideFrame fault{0x15007FFD, {1,0x40,1,0,1,0,0,0}};
    received = {fault}; motor.update(10);
    assert(motor.faultDetails() == 0x14001 && motor.warnings() == 1 && motor.hasFault());
    assert(motor.feedbackAge(10) == 21);
    std::cout << "PASS: TX golden frames, RX units/faults, malformed filtering, configuration/version, time wrap\n";
}
