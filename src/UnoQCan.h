#pragma once
#include <stdint.h>

struct RobStrideFrame {
    uint32_t id = 0;
    uint8_t data[8] = {};
    uint8_t length = 8;
    bool extended = true;
    bool remote = false;
    bool fd = false;
};

class UnoQCan {
public:
    bool begin();
    bool send(const RobStrideFrame &frame);
    bool receive(RobStrideFrame &frame);
    bool healthy() const;
    void end();
private:
    bool started_ = false;
    int filter_ = -1;
};
