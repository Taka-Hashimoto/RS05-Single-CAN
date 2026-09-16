# RS05 Single CAN

A single-motor CAN library for RobStride 05 on Arduino UNO Q.

**Supports one motor only.** This is an unofficial library for ArduinoCore-zephyr and the RobStride proprietary protocol. Multiple motors on the same CAN bus, RobStride 02, the MIT control protocol, and CANopen are not supported.

Use one `UnoQCan` instance and one `RobStride05` instance. The motor ID is configurable, but creating more instances does not add multi-motor support: `update()` consumes frames from a shared receive queue and discards frames addressed to other motors.

```text
src/
  RobStride05.h/.cpp       Motor commands, decoding, and cached state
  UnoQCan.h/.cpp           UNO Q CAN initialization and transport
examples/StoppedFeedback/ Read feedback with the drive disabled
tests/                   Protocol tests
```

The library has no Python, RPC, UI, experiment logging, or application control logic. It uses two small classes without an inheritance hierarchy or an external CAN library.

## Installation

Place this repository in your Arduino sketchbook as `libraries/RS05SingleCAN`, or install its ZIP through Arduino IDE's **Add .ZIP Library** command. Repository: [Taka-Hashimoto/RS05-Single-CAN](https://github.com/Taka-Hashimoto/RS05-Single-CAN).

The package name is **RS05 Single CAN**. The C++ header and class names remain `RobStride05.h`, `RobStride05`, and `UnoQCan`, so existing sketches do not need API changes.

With the UNO Q core installed, build the example from the repository root:

```sh
arduino-cli compile examples/StoppedFeedback
```

The example's `sketch.yaml` references this library using the relative path `../..`; it does not depend on a particular checkout location. It uses the installed core and pins its Arduino library dependencies.

Tested with ArduinoCore-zephyr **0.90.0** and Arduino CLI **1.5.1** on UNO Q. The example uses RouterBridge for serial output; the library itself has no Bridge dependency.

## Wiring

| UNO Q | CAN transceiver |
|---|---|
| D4 / PA12 / CAN TX | TXD |
| D5 / PA11 / CAN RX | RXD |
| GND | GND |

Use a transceiver compatible with 3.3 V logic. Connect CANH/CANL to the motor and provide the motor's power supply separately. Configure the transceiver's STB/EN pins and terminate both ends of the bus with 120 ohms.

The transport uses **1 Mbps, Classic CAN, 29-bit extended IDs, and 8-byte frames**. It owns CAN1; do not also use the Arduino `CAN` object or repurpose the CAN pins as GPIO.

## Minimal example

Type 4 disable commands produce Type 2 feedback, so reading the motor does not require enabling its drive. Set motor ID `0x7F` and host ID `0xFD` to match your setup.

```cpp
#include <Arduino_RouterBridge.h>
#include <RobStride05.h>

UnoQCan can;
RobStride05 motor(can, 0x7F, 0xFD);
bool ready;
uint32_t lastQuery;

void setup() {
    Serial.begin(115200);
    ready = motor.begin();
    if (!ready) Serial.println("CAN initialization failed");
}

void loop() {
    if (!ready) return;
    motor.update(millis());
    if (millis() - lastQuery < 100) return;
    lastQuery = millis();
    if (!motor.disable()) Serial.println("Stop TX rejected");
    if (motor.hasFeedback() && motor.feedbackAge(millis()) < 200) {
        Serial.println(motor.position(), 4);
    } else {
        Serial.println("No fresh feedback");
    }
}
```

## API

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

A command returning `true` means **the local CAN transmit queue accepted it**, not that transmission completed or the motor acknowledged it. Check responses separately with `update()` and the cached state. `can.healthy()` detects asynchronous transmit errors, receive queue overflow, and bus-off. To clear latched transport errors, first stop the motor, then call `can.end()` followed by `begin()`.

Configuration and diagnostic methods should be used with the drive disabled:

| Method | Purpose |
|---|---|
| `selectMotionMode()` | Set run_mode=0 without saving it permanently |
| `setCommunicationTimeout(ms, legacyAccess=false)` | Set a timeout of 1–5000 ms; explicitly pass `true` for legacy access |
| `requestConfiguration()` / `configurationConfirmed()` | Request mode and timeout readback / check that the values match |
| `rejectedParameter()` | Get the index rejected by the firmware; zero means no rejection, not successful confirmation |
| `requestVersion()` / `hasVersion()` / `version()` | Request firmware version, which also stops the drive / check receipt / read the raw value |

Continue calling `update()` after configuration requests. `requestConfiguration()` clears previous confirmation flags. There is no public API for arbitrary parameters.

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

## License

[MIT](LICENSE). Copyright (c) 2026 Takanori Hashimoto.

ArduinoCore-zephyr, RouterBridge, and other dependencies retain their own licenses. Their sources and binaries are not included in this repository.
