# RS05 Single CAN

An unofficial CAN library for **one RobStride 05 motor on Arduino UNO Q**. Uses the RobStride proprietary protocol. Multiple motors, other motor models, MIT control mode, and CANopen are not supported.

## What you need

- Arduino UNO Q with ArduinoCore-zephyr installed
- One RobStride 05 and its power supply
- A CAN transceiver compatible with 3.3 V logic

## Wiring

| UNO Q | CAN transceiver |
|---|---|
| D4 | TXD |
| D5 | RXD |
| GND | GND |

Connect the transceiver's CANH/CANL to the motor. Enable the transceiver according to its documentation and use 120-ohm termination at both ends of the bus. The library sets CAN to 1 Mbps.

## Get started with Arduino App Lab

This library is not yet in the library catalog. Add it once on your UNO Q:

1. In a terminal **on the UNO Q**, run:

   ```sh
   git clone https://github.com/Taka-Hashimoto/RS05-Single-CAN.git ~/RS05SingleCAN
   ```

2. Create an App Lab app. In the **UNO Q terminal**, open its build settings (replace `your-app` with the app's folder name):

   ```sh
   nano ~/ArduinoApps/your-app/sketch/sketch.yaml
   ```

   Add the local library under `profiles.default.libraries`, keeping existing entries. If `libraries:` already exists, add only the `- dir:` line beneath it:

   ```yaml
       libraries:
         - dir: /home/arduino/RS05SingleCAN
   ```

   Save with **Ctrl+O**, **Enter**, then exit with **Ctrl+X**. This file is edited from the terminal, not the App Lab code editor.

3. Copy [StoppedFeedback](examples/StoppedFeedback/StoppedFeedback.ino) into the app's `sketch/sketch.ino`. Set `0x7F` to your motor's CAN ID, click **Run**, and view the serial output. The example prints the angle in radians with the drive disabled.

Tested with App CLI 0.12.1 and ArduinoCore-zephyr 0.90.0. For Arduino IDE, see [installation instructions](API.md#arduino-ide).

## Before moving the motor

Your sketch must send commands regularly and stop on faults or lost feedback. Use `disable()` to stop the drive; a zero-torque command does not disable it.

**Automatic motor stopping after a CAN disconnection is not verified on the tested firmware.** Do not rely on it as your only way to stop the motor.

For a manually started ±90° motion example, see [SinePosition](examples/SinePosition).

See the [API reference](API.md) for torque and motion commands, and [protocol notes](PROTOCOL.md) for firmware compatibility.

[MIT License](LICENSE)
