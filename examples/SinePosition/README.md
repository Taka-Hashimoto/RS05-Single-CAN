# Sine position

Send `r` in the serial monitor to command one ±90° cycle in 20 seconds. The center is the current motor position. The drive starts disabled and is disabled again after the cycle.

1. Follow the [installation and wiring steps](../../README.md), then use `SinePosition.ino` as your sketch. Set the motor ID (`0x7F`).
2. Connect a normally open stop button between **D2 and GND**. Use an unloaded shaft with room to turn.
3. Run the sketch and send `r`. Press the button to stop early. Serial input is only processed while stopped.

Change `Amplitude` (radians), `Period` (seconds), `Kp`, and `Kd` at the top. The timeout setup uses legacy access for the tested firmware `0x00050003`; see [firmware notes](../../PROTOCOL.md).

The MCU sends commands at 100 Hz. Faults, stale feedback, CAN errors, delayed control, excessive speed/travel, or estimated PD effort above 0.3 N·m stop the cycle. Actual tracking is not guaranteed. The effort check is not a hardware torque limit.

**CAN-disconnection stopping remains unverified.** The stop button also depends on CAN; keep motor power disconnect accessible. Closing the serial monitor does not stop motion.

This simplified version is build-checked on UNO Q (core 0.90.0), but has not been driven on hardware. The earlier version completed one cycle and confirmed drive disable; actual angle extrema were not recorded.
