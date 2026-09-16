# Protocol and firmware compatibility

This implementation follows printed pages 43–54 of the [official RS05 User Manual 260713](https://github.com/RobStride/Product_Information/blob/main/Product%20Literature/RS05/RS05User%20Manual260713.pdf).

SHA256 of the reference PDF: `1b2c61a6daa8331b9a348455005d10d6e425165c22e6f64c39cbca33f6360e82`.

## Frames and conversions

- Ordinary transmit ID: `(type << 24) | (host << 8) | motor`. For host `FD` and motor `7F`, enable is `0300FD7F`, matching the ID layout in the manual's Type 17 example.
- Type 1 uses ID bits 23:8 for the 16-bit torque command. Its payload contains position, velocity, Kp, and Kd as four **big-endian** 16-bit values.
- Encoding: `uint16((x-min)/(max-min)*65535)`. Decoding: `min+u/65535*(max-min)`. Ranges: position ±4π, velocity ±50, torque ±5.5, Kp 0–500, Kd 0–5. Inputs must be finite and in range; there is no implicit clamping.
- Symmetric 16-bit encoding has no exact zero. A zero command encodes as 32767, equivalent to approximately −0.000084 N·m for torque. It does not disable the drive.
- Type 2 receive ID: bits 15:8 are the motor ID; bits 7:0 are the host ID. The payload contains position, velocity, torque, and temperature as **big-endian** 16-bit values. Temperature is raw × 0.1°C. Bits 21:16 contain faults; bits 23:22 contain mode (0 disabled, 1 calibration, 2 running).
- Fault bits 0–5 represent undervoltage, overcurrent, overtemperature, magnetic encoder fault, overload, and uncalibrated status. Type 21 contains **little-endian** 32-bit fault and warning fields.
- Type 17/18 payloads contain index in bytes 0–1, reserved bytes 2–3, and value in bytes 4–7, using **little-endian** order. Read replies are checked for matching IDs, result status, and reserved fields.
- `canTimeout=3000` represents 150 ms. The caller should configure and read it back while stopped and verify it before enabling. The library never sends the Type 22 persistent-save command.
- Version replies use Type 2 with a `00 C4 56` prefix and a **big-endian** version value in bytes 3–6. They do not update telemetry. A normal telemetry payload matching that prefix is also conservatively excluded.
- Frames with another motor or host ID, standard IDs, RTR, FD, DLC other than 8, IDs exceeding 29 bits, or reserved mode 3 do not update motor state. Type 9/17/21 and version replies do not refresh the feedback timestamp.

Only one motor is supported. Receive filtering protects cached state from unrelated frames; it does not route those frames to additional motor instances.

## Legacy timeout access

Tested firmware raw version `0x00050003` rejects Type 17 reads of `0x7028`, but accepts Type 9 reads of legacy setting `0x200C`. For this firmware, explicitly select `setCommunicationTimeout(150, true)`. The library does not automatically fall back to legacy access or update firmware.

The legacy format is based on the manual's `0x200C CAN_TIMEOUT` table and `otaThread.o` included in the manufacturer's [MotorStudio v1.0.1 release](https://github.com/RobStride/MotorStudio/releases/tag/v1%2C0%2C1):

- Type 8 writes a volatile setting with ID subcommand 0; Type 9 reads it.
- Payload bytes 0–1: index, little endian.
- Byte 2: value type (`4` for uint32; `0` in a read request).
- Byte 3: fragment number, `0`.
- Bytes 4–7: value, little endian.
- Save/reset subcommands 2 and 3 are never sent.
- A successful reply is checked for matching IDs, type, fragment number, and value.

Observed timeout readback:

```text
TX 0900FD7F : 0C 20 00 00 00 00 00 00
RX 09007FFD : 0C 20 04 00 B8 0B 00 00
```

The returned value is 3000 ticks, corresponding to 150 ms under the documented conversion.

## Unverified motor-side timeout behavior

In a hardware test on this firmware, periodic commands were paused for 180 ms. After transmission resumed, feedback reported mode 2 rather than the expected disabled mode 0. The test could not distinguish failure to stop from a restart when commands resumed.

Motor-side communication-loss stopping therefore remains **unverified**. Successful timeout readback must not be treated as proof that the drive will disable on a disconnected CAN bus. No firmware update or persistent parameter save was performed.
