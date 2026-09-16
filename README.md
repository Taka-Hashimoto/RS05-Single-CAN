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

## Get started

1. Download this repository using **Code → Download ZIP**.
2. In Arduino IDE, use **Sketch → Include Library → Add .ZIP Library**. Install **Arduino_RouterBridge** through Library Manager for the example's serial output.
3. Open **File → Examples → RS05 Single CAN → StoppedFeedback**. Change `0x7F` to your motor's CAN ID.
4. Select your UNO Q, upload the example, and open the Serial Monitor. It prints the motor angle in radians while keeping the drive disabled.

For Arduino CLI, build from the repository root:

```sh
arduino-cli compile examples/StoppedFeedback
```

Tested with ArduinoCore-zephyr 0.90.0 and Arduino CLI 1.5.1.

## Before moving the motor

Your sketch must send commands regularly and stop on faults or lost feedback. Use `disable()` to stop the drive; a zero-torque command does not disable it.

**Automatic motor stopping after a CAN disconnection is not verified on the tested firmware.** Do not rely on it as your only way to stop the motor.

See the [API reference](API.md) for torque and motion commands, and [protocol notes](PROTOCOL.md) for firmware compatibility.

[MIT License](LICENSE)
