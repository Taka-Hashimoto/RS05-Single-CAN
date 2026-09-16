# Sine position

Commands one ±90° sine-wave cycle in 20 seconds, centered on the position measured when you start. It then disables the drive. It does not change the motor's zero point.

1. Follow the [library installation and wiring steps](../../README.md). Use an unloaded shaft with room to turn.
2. Set the motor ID in `SinePosition.ino`. `LegacyTimeout = true` matches the tested firmware `0x00050003`; see [firmware notes](../../PROTOCOL.md).
3. Connect a normally open stop button between UNO Q **D2 and GND**. Pressing it requests drive disable over CAN.
4. Upload the sketch. In App Lab, copy it into `sketch/sketch.ino`, keeping your app's library configuration.
5. Open the serial monitor and send `r`. One cycle runs, then the result is printed. Send a new `r` to repeat.

The sketch starts disabled. Motion runs entirely on the MCU at 100 Hz. During motion, serial input/output is not processed; use the D2 button to stop early. Closing the monitor does not stop the cycle. It does not restart automatically after a fault.

Adjust `Amplitude` (radians), `CycleMs`, `Kp`, and `Kd` at the top of the sketch. Defaults are Kp = 1 N·m/rad and Kd = 0.1 N·m·s/rad, with zero added torque. Actual travel depends on tracking; ±90° is the target, not a measured guarantee.

The sketch disables on motor faults, CAN errors, feedback older than 100 ms, a control interval over 30 ms, speed above 1.5 rad/s, travel beyond ±100°, or estimated PD effort above 0.3 N·m. The effort check uses the latest feedback; it is not a hardware torque limit. A stop is confirmed using fresh disabled-state feedback.

**Motor-side stopping after a CAN disconnection remains unverified.** The D2 button also depends on CAN. Keep a way to disconnect motor power available.

Build-checked for UNO Q with ArduinoCore-zephyr 0.90.0. This sine-wave example has not been run on a motor.
