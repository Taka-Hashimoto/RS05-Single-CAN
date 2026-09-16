# Sine position

Send `r` in the serial monitor for one ±90° cycle in 20 seconds, centered on the starting position. The drive starts disabled and stops after the cycle.

1. Follow the [installation and wiring steps](../../README.md), then use `SinePosition.ino` as your sketch. Set the motor ID (`0x7F`).
2. Connect a normally open stop button between **D2 and GND**. Use an unloaded shaft with room to turn.
3. Run and send `r`. Press the button to stop early. Serial input is processed only while stopped.

Change `Amplitude` (radians) and `Period` (seconds) at the top; the gains are the last two arguments to `motion()`. `RS05Motor` handles configuration, 100 Hz transmission, limits and stopping. See the [API reference](../../API.md) for limits and firmware selection.

**CAN-disconnection stopping remains unverified.** The button depends on CAN; keep motor power disconnect accessible. Closing the monitor does not stop motion.

This version uses the new high-level API and has not been driven on hardware. The earlier low-level version completed one cycle and confirmed drive disable; actual angle extrema were not recorded.
