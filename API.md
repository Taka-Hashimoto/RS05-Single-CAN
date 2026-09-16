# API reference

For Arduino App Lab, follow the [installation and first-run steps](README.md#get-started-with-arduino-app-lab). For wiring, see the [README](README.md#wiring).

## Recommended API: RS05Motor

```cpp
#include <RS05_Single_CAN.h>

RS05Motor motor(0x7F);
```

Use one motor object. The optional second constructor argument is the host ID (default `0xFD`). Do not also create a `UnoQCan` or `RobStride05` object for the same CAN controller.

| Method | Purpose |
|---|---|
| `begin(legacyTimeout = true)` | Initialize CAN and confirm the drive is disabled |
| `start()` | Confirm stopped state, configure motion mode and timeout, send zero torque, then confirm drive enable |
| `motion(position, velocity, kp, kd, torque = 0)` | Store the next target in rad, rad/s, N·m/rad, N·m·s/rad, N·m |
| `torque(nm)` | Store a torque target with zero gains |
| `update()` | Receive feedback, check limits/timeouts, and send the stored target every 10 ms |
| `stop()` | Clear the target, send drive disable, and confirm a fresh disabled reply |
| `active()` / `error()` | Read local control status / last error text (`""` if none) |
| `setLimits(limits)` | Set torque, speed and travel limits while stopped |
| `hasFeedback()` / `feedbackAge()` | Check receipt / age in ms |
| `position()` / `velocity()` / `torque()` / `temperature()` | Read cached rad / rad/s / N·m / °C |
| `faults()` / `faultDetails()` / `warnings()` | Read cached fault and warning bits |

`begin()`, `start()`, and `stop()` return `true` only after the required motor replies. They block for bounded confirmation: stop up to 300 ms, start up to about one second including cleanup. `motion()` and `torque(nm)` return target acceptance, not a motor acknowledgment. Getters never wait for replies. Before the first feedback, measurements are `NaN` and age is `UINT32_MAX`.

Call `update()` frequently on the MCU (for example every 1 ms), and refresh the target at least every 100 ms while active. A command interval over 30 ms, target timeout, feedback age of 100 ms, CAN error, motor fault/warning, unexpected motor mode, or exceeded limit triggers disable attempts. Errors remain available until the next `start()` or `begin()`. Restart always requires an explicit `start()`; old targets are cleared. Latched CAN transport errors require an MCU reset before retrying with this API.

Default limits are 0.3 N·m estimated effort, 1.5 rad/s measured/target speed, and ±100° measured/target travel from the starting position. To change them while stopped:

```cpp
RS05Motor::Limits limits;
limits.torque = 0.2f;
limits.speed = 1.0f;
limits.travel = 1.745329f;
if (!motor.setLimits(limits)) { /* handle motor.error() */ }
```

The effort check is `Kp*abs(position error) + Kd*abs(velocity error) + abs(added torque)`. It uses cached feedback and is not a hardware current/torque limit. Invalid targets while active also stop the drive. `active() == false` alone does not confirm physical stopping; check `stop()` and `error()`.

This API owns no background thread. If your sketch blocks or stops calling `update()`, it cannot send a stop until execution resumes. Keep blocking serial/RPC work outside active motion. Motor-side CAN-loss stopping remains unverified on the tested firmware. `begin(false)` selects modern timeout access; default legacy access matches tested firmware `0x00050003`.

See [SinePosition](examples/SinePosition) for the complete motion example and [StoppedFeedback](examples/StoppedFeedback/StoppedFeedback.ino) for readout without enabling.

## Low-level API: RobStride05

The original API remains available for applications that implement their own scheduling and stop policy.

### Create the motor object

```cpp
#include <RobStride05.h>

UnoQCan can;
RobStride05 motor(can, 0x7F);
```

Replace `0x7F` with your motor's CAN ID. The optional third argument is the host ID (default `0xFD`). Use only one `UnoQCan` and one `RobStride05` object; multiple motors are not supported.

Call `motor.begin()` once in `setup()` and check its result. Call `motor.update(millis())` regularly in `loop()`, then read the cached values.

### Methods

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

For feedback while stopped, send `motor.disable()` periodically and process the replies with `motor.update(millis())`. Reading a getter alone does not request new feedback or enable the drive.

Configuration and diagnostic methods should be used with the drive disabled:

| Method | Purpose |
|---|---|
| `selectMotionMode()` | Set run_mode=0 without saving it permanently |
| `setCommunicationTimeout(ms, legacyAccess=false)` | Set a timeout of 1–5000 ms; explicitly pass `true` for legacy access |
| `requestConfiguration()` / `configurationConfirmed()` | Request mode and timeout readback / check that the values match |
| `rejectedParameter()` | Get the index rejected by the firmware; zero means no rejection, not successful confirmation |
| `requestVersion()` / `hasVersion()` / `version()` | Request firmware version, which also stops the drive / check receipt / read the raw value |

Set the motion mode and communication timeout before calling `requestConfiguration()`. Continue calling `update()` after configuration requests. `requestConfiguration()` clears previous confirmation flags. For the tested firmware `0x00050003`, use `setCommunicationTimeout(ms, true)`; see [firmware compatibility](PROTOCOL.md) for the legacy access details. There is no public API for arbitrary parameters.

### Driving and stopping

The protocol conversion ranges are position ±4π rad, velocity ±50 rad/s, torque ±5.5 N·m, Kp 0–500, and Kd 0–5. Non-finite or out-of-range commands return `false` without transmission. These are encoding ranges, not safe operating limits for an application.

The calling application must configure and verify the mode and timeout while stopped, explicitly enable with zero commands, send periodic commands, enforce output limits, and stop on faults or stale feedback. The low-level `RobStride05` class does not schedule commands or implement automatic stopping. Use `disable()` to stop the drive; zero torque is not equivalent. A disable command cannot reach the motor over a disconnected CAN bus.

**Motor-side communication-loss stopping has not been verified on tested firmware `0x00050003`, even though timeout readback succeeded.** Configuration confirmation is not proof of stopping behavior. See [PROTOCOL.md](PROTOCOL.md) for firmware compatibility and the observed limitation.

The library does not change the zero point, save persistent parameters, or switch protocols.

## Validation

Run the protocol tests from the repository root on a system with g++:

```sh
./tests/run.sh
```

Tests compile the real protocol and controller implementations with mocked CAN and, for the controller, a simulated clock. They check fixed frames derived from the official specification, conversions, malformed frames and wrong IDs, configuration readback, version replies, and timestamp wraparound. Controller tests check enable/disable confirmation, periodic transmission, limits, faults, target/feedback timeouts, delayed loops, and no automatic restart.

Hardware tests on 2026-09-16 with UNO Q and RobStride 05 confirmed disabled feedback, small torque commands in both directions, motion commands, and drive disable responses. Test applications and logs are not part of this repository. Low gains left position errors; loaded operation, endurance, and communication-loss stopping remain unverified.

The new `RS05Motor` API passed host tests and UNO Q builds of both examples; its hardware execution has not yet been verified.

## Arduino IDE

Download this repository as a ZIP and install it using **Sketch → Include Library → Add .ZIP Library**. Install the UNO Q board package (**ArduinoCore-zephyr**). Install **Arduino_RouterBridge** through Library Manager for serial output.

Open **File → Examples → RS05 Single CAN → StoppedFeedback**, set the motor ID, select UNO Q, and upload. The App Lab setup above was build-checked; the Arduino IDE GUI workflow has not been tested.
