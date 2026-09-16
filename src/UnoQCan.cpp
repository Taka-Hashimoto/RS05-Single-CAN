#include "UnoQCan.h"
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyrPinctrl.h>
#include <string.h>

namespace {
const device *const deviceCan = DEVICE_DT_GET(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), cans, 0));
K_MSGQ_DEFINE(receiveQueue, sizeof(can_frame), 32, 4);
atomic_t transportError;

void received(const device *, can_frame *frame, void *) {
    if (k_msgq_put(&receiveQueue, frame, K_NO_WAIT) != 0) atomic_set(&transportError, 1);
}
void sent(const device *, int error, void *) {
    if (error) atomic_set(&transportError, 1);
}
}

bool UnoQCan::begin() {
    if (started_) return true;
    if (zephyr::arduino::init_dev_apply_pinctrl(deviceCan) != 0 ||
        !device_is_ready(deviceCan)) return false;
    can_stop(deviceCan);
    if (can_set_mode(deviceCan, CAN_MODE_NORMAL) != 0 ||
        can_set_bitrate(deviceCan, 1000000) != 0) return false;
    can_filter filter{};
    filter.flags = CAN_FILTER_IDE;
    filter_ = can_add_rx_filter(deviceCan, received, nullptr, &filter);
    if (filter_ < 0) return false;
    k_msgq_purge(&receiveQueue);
    atomic_clear(&transportError);
    if (can_start(deviceCan) != 0) {
        can_remove_rx_filter(deviceCan, filter_);
        filter_ = -1;
        return false;
    }
    started_ = true;
    return true;
}
bool UnoQCan::send(const RobStrideFrame &input) {
    if (!started_ || !input.extended || input.remote || input.fd ||
        input.length != 8 || input.id > 0x1FFFFFFF) return false;
    can_frame frame{};
    frame.id = input.id;
    frame.flags = CAN_FRAME_IDE;
    frame.dlc = 8;
    memcpy(frame.data, input.data, 8);
    return can_send(deviceCan, &frame, K_NO_WAIT, sent, nullptr) == 0;
}
bool UnoQCan::receive(RobStrideFrame &output) {
    can_frame frame{};
    if (!started_ || k_msgq_get(&receiveQueue, &frame, K_NO_WAIT) != 0) return false;
    output.id = frame.id;
    output.extended = frame.flags & CAN_FRAME_IDE;
    output.remote = frame.flags & CAN_FRAME_RTR;
    output.fd = frame.flags & CAN_FRAME_FDF;
    output.length = frame.dlc;
    memcpy(output.data, frame.data, 8);
    return true;
}
bool UnoQCan::healthy() const {
    can_state state;
    return started_ && !atomic_get(&transportError) &&
        can_get_state(deviceCan, &state, nullptr) == 0 &&
        state != CAN_STATE_BUS_OFF && state != CAN_STATE_STOPPED;
}
void UnoQCan::end() {
    if (!started_) return;
    can_stop(deviceCan);
    can_remove_rx_filter(deviceCan, filter_);
    started_ = false;
}
