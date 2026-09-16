# API reference

For Arduino App Lab, follow the [installation and first-run steps](README.md#get-started-with-arduino-app-lab). For wiring, see the [README](README.md#wiring).

## Create the motor object

```cpp
#include <RobStride05.h>

UnoQCan can;
RobStride05 motor(can, 0x7F);
```

Replace `0x7F` with your motor's CAN ID. The optional third argument is the host ID (default `0xFD`). Use only one `UnoQCan` and one `RobStride05` object; multiple motors are not supported.

Call `motor.begin()` once in `setup()` and check its result. Call `motor.update(millis())` regularly in `loop()`, then read the cached values. See [StoppedFeedback](examples/StoppedFeedback/StoppedFeedback.ino) for a complete example that keeps the drive disabled.

## Methods

| Method | Purpose |
|---|---|
| `begin()` | Initialize CAN without enabling or configuring the motor |
| `enable()` / `disable()` | Send a drive enable / disable command |
| `torque(nm)` | Command torque with Kp and Kd set to zero |
| `motion(position, velocity, kp, kd, torque)` | Command motion in rad, rad/s, N·m/rad, N·m·s/rad, and N·m |
| `update(nowMs)` | Process received frames; normally pass `millis()` |
| `hasFeedback()` / `feedbackAge(nowMs)` | Check whether feedback has arrived / its age in ms |
| `position()` / `velocity()` / `torque()` / `temperature()` | Read cached values in rad / rad/s / N·m / °C |
| `mode()` / `faults()` | Read mode (0 disabled, 1 calibration, 2 running) / Type 2 fault bits |
| `faultDetails()` / `warnings()` / `hasFault()` | Read Type 21 details / warnings / combined fault or warning status |

Getters return cached values without sending requests or waiting for replies. Before the first feedback, measurements are `NaN` and age is `UINT32_MAX`. A mode or fault value of zero alone does not indicate that feedback has arrived.

A command returning `true` means **the local CAN transmit queue accepted it**, not that transmission completed or the motor acknowledged it. Check responses separately with `update()` and the cached state. `can.healthy()` detects asynchronous transmit errors, receive queue overflow, and bus-off. It does not indicate whether the motor is replying; check `motor.feedbackAge(millis())` separately. To clear latched transport errors, first stop the motor, then call `can.end()` followed by `motor.begin()`. `can.end()` stops the CAN controller; it does not send a motor disable command.

For feedback while stopped, send `motor.disable()` periodically and process the replies with `motor.update(millis())`. The example does this every 100 ms. Reading a getter alone does not request new feedback or enable the drive.

Configuration and diagnostic methods should be used with the drive disabled:

| Method | Purpose |
|---|---|
| `selectMotionMode()` | Set run_mode=0 without saving it permanently |
| `setCommunicationTimeout(ms, legacyAccess=false)` | Set a timeout of 1–5000 ms; explicitly pass `true` for legacy access |
| `requestConfiguration()` / `configurationConfirmed()` | Request mode and timeout readback / check that the values match |
| `rejectedParameter()` | Get the index rejected by the firmware; zero means no rejection, not successful confirmation |
| `requestVersion()` / `hasVersion()` / `version()` | Request firmware version, which also stops the drive / check receipt / read the raw value |

Set the motion mode and communication timeout before calling `requestConfiguration()`. Continue calling `update()` after configuration requests. `requestConfiguration()` clears previous confirmation flags. For the tested firmware `0x00050003`, use `setCommunicationTimeout(ms, true)`; see [firmware compatibility](PROTOCOL.md) for the legacy access details. There is no public API for arbitrary parameters.

## Driving and stopping

The protocol conversion ranges are position ±4π rad, velocity ±50 rad/s, torque ±5.5 N·m, Kp 0–500, and Kd 0–5. Non-finite or out-of-range commands return `false` without transmission. These are encoding ranges, not safe operating limits for an application.

The calling application must configure and verify the mode and timeout while stopped, explicitly enable with zero commands, send periodic commands, enforce output limits, and stop on faults or stale feedback. The library does not schedule commands or implement automatic stopping. Use `disable()` to stop the drive; zero torque is not equivalent. A disable command cannot reach the motor over a disconnected CAN bus.

**Motor-side communication-loss stopping has not been verified on tested firmware `0x00050003`, even though timeout readback succeeded.** Configuration confirmation is not proof of stopping behavior. See [PROTOCOL.md](PROTOCOL.md) for firmware compatibility and the observed limitation.

The library does not change the zero point, save persistent parameters, or switch protocols.

## Validation

Run the protocol tests from the repository root on a system with g++:

```sh
./tests/run.sh
```

Tests compile the real protocol implementation and mock only CAN transmission and reception. They check fixed frames derived from the official specification, conversions, malformed frames and wrong IDs, configuration readback, version replies, and timestamp wraparound.

Hardware tests on 2026-09-16 with UNO Q and RobStride 05 confirmed disabled feedback, small torque commands in both directions, motion commands, and drive disable responses. Test applications and logs are not part of this repository. Low gains left position errors; loaded operation, endurance, and communication-loss stopping remain unverified.

## Arduino IDE

Download this repository as a ZIP and install it using **Sketch → Include Library → Add .ZIP Library**. Install the UNO Q board package (**ArduinoCore-zephyr**). Install **Arduino_RouterBridge** through Library Manager for serial output.

Open **File → Examples → RS05 Single CAN → StoppedFeedback**, set the motor ID, select UNO Q, and upload. The App Lab setup above was build-checked; the Arduino IDE GUI workflow has not been tested.
