# esphome-mitsubishi-msc

An ESPHome external component for older Mitsubishi Electric mini-split indoor
units that use the "MSC" infrared protocol (remote KP1A style). It gives you:

- A `climate` entity with the correct Low / Medium / High fan speeds
- A `select` entity for all 7 vertical vane positions
- **Two-way sync:** commands from the original remote are decoded and shown in
  Home Assistant, without ESPHome retransmitting them

It is a standalone replacement for `heatpumpir` + `protocol: mitsubishi_msc`
and does not depend on the HeatpumpIR library.

## Why not just use `heatpumpir`?

1. **Fan speeds are shifted.** The stock `heatpumpir` platform sends
   `FAN_2/FAN_3/FAN_4` for Low/Medium/High on every protocol. The MSC protocol
   only has three speeds (`FAN_1`..`FAN_3`), so Low behaves like Medium, Medium
   like High, and High falls back to Auto. Reported upstream as
   [esphome/esphome#20429](https://github.com/esphome/esphome/issues/20429);
   the same kind of bug is being fixed for Mitsubishi Heavy ZJ/ZMP in
   [#16877](https://github.com/esphome/esphome/pull/16877).
2. **Vane control is limited.** Home Assistant's climate swing modes
   (off/vertical/horizontal/both) can't represent the remote's 7 vane positions.
3. **No receive support.** If someone uses the remote, Home Assistant never
   finds out.

## Tested hardware

| Item | Notes |
|---|---|
| Indoor units | Mitsubishi MS09TW (x2), MS17TN (x1) - cooling only, no CN105 port |
| Controller | M5Stack AtomS3 Lite (ESP32-S3) |
| IR | M5Stack Unit IR (U002): receiver on GPIO1, transmitter on GPIO2 |
| ESPHome | 2026.9.0 / 2026.9.1, ESP-IDF framework |

The HeatpumpIR library lists the same protocol for MSC-GA20VB / GA25VB / GA35VB
units (remote P/N KP1A). Those should work but have **not** been tested here.

## Installation

```yaml
external_components:
  - source: github://jaylyon/esphome-mitsubishi-msc
    components: [mitsubishi_msc]
```

For local development, point at a folder instead:

```yaml
external_components:
  - source:
      type: local
      path: components
```

## Example configuration

```yaml
esphome:
  name: living-room-ac
  friendly_name: Living Room AC

esp32:
  variant: esp32s3
  framework:
    type: esp-idf

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
ota:
  - platform: esphome

remote_receiver:
  id: ir_receiver
  pin:
    number: GPIO1
    inverted: true        # the Unit IR receiver output is active-low
    mode:
      input: true
      pullup: true
  tolerance: 55%

remote_transmitter:
  id: ir_transmitter
  pin: GPIO2
  carrier_duty_percent: 50%
  non_blocking: true

climate:
  - platform: mitsubishi_msc
    id: ac
    name: None
    receiver_id: ir_receiver
    transmitter_id: ir_transmitter
    min_temperature: 17
    max_temperature: 30

select:
  - platform: mitsubishi_msc
    mitsubishi_msc_id: ac
    name: "Vertical Vane"
```

## Options

### `climate`

| Option | Default | Description |
|---|---|---|
| `transmitter_id` | required | `remote_transmitter` to send IR with |
| `receiver_id` | optional | `remote_receiver`; enables remote-to-HA sync |
| `min_temperature` | `17` | Lowest setpoint, 16-31 C (also clamps transmitted values) |
| `max_temperature` | `30` | Highest setpoint, 16-31 C |
| `power_flag` | `false` | Send `0xA4`/`0xA0` in the power byte like real remotes do, instead of `0x24`/`0x20`. Try it if a unit obeys its remote but ignores commands from ESPHome |
| `supports_heat` | `false` | Expose Heat mode. Untested; the target units are cooling-only |
| `sensor` | optional | Optional external temperature sensor, as for other `climate_ir` platforms |

Supported modes: Off, Cool, Dry, Fan only, and Heat/Cool (which sends the
unit's Auto mode). Fan modes: Auto, Low, Medium, High.

### `select`

| Option | Description |
|---|---|
| `mitsubishi_msc_id` | required, the `climate` above |

Options: Auto, Highest, Second Highest, Middle, Next Lower, Lowest, Swing.
The select is a separate entity because Home Assistant's thermostat dialog only
shows what the climate entity itself reports, so it won't appear inside that
dialog. Add it to a dashboard card next to the climate entity if you want them
together.

## Receiving from the original remote

When `receiver_id` is set, frames from the original remote are decoded and
reflected in Home Assistant: power, mode, target temperature, fan speed and vane
position. This only updates state and never calls the transmit path, so there
is no feedback loop.

**Receiver polarity matters.** The Unit IR receiver output is active-low. If
`inverted: true` is missing, the raw timings show negative marks and no frame
is ever decoded. To check what your receiver sees, temporarily add
`dump: raw` to the `remote_receiver` and set `logger: level: DEBUG`.
A good frame starts `3449, -1702, 452, -1256, ...` and logs
`Received Mitsubishi MSC frame: power=... mode=... temp=... fan/vane=...`.

## Protocol notes

14-byte frames, 38 kHz, pulse-distance encoding, each byte sent LSB first.

| Item | Value |
|---|---|
| Header | mark 3060 us, space 1580 us (real remotes measure ~3450 / 1700) |
| Bit | mark ~350 us, space 390 us = 0, 1150 us = 1 |
| Trailer | one bit-mark |

| Byte | Meaning |
|---|---|
| 0-4 | Fixed prefix `23 CB 26 01 00` |
| 5 | Power: `0x24` on, `0x20` off (real remotes send `0xA4` / `0xA0`; bit `0x04` is the power flag) |
| 6 | Mode: `0x02` Dry, `0x03` Cool, `0x07` Fan, `0x08` Auto (`0x01` Heat is untested) |
| 7 | `31 - temperature in C` |
| 8 | Fan (low 3 bits) OR vane (bits 3-5) |
| 9-12 | `0x00` in every capture |
| 13 | Sum of bytes 0-12, modulo 256 |

Fan: Auto `0x00`, Low `0x02`, Medium `0x03`, High `0x05`.
Vane: Auto `0x00`, Highest `0x08`, Second `0x10`, Middle `0x18`,
Next lower `0x20`, Lowest `0x28`, Swing `0x38`.

Example, powered on, Cool, 24 C, fan Low, vane Highest:
`23 CB 26 01 00 A4 03 07 0A 00 00 00 00 CD`.

## Limitations

- Temperatures are whole degrees C. A 71 F setpoint is sent as 22 C.
- No horizontal vane control (these units have none).
- Heat mode is untested.
- Bytes 9-12 are always sent as `0x00`; timers and other remote features aren't
  modelled.
- There is no current-temperature reading from the AC, because the protocol is
  one-way. Use the optional `sensor` option if you want one.

## Development

The frame helpers (checksum, frame building, validation) live in
`components/mitsubishi_msc/mitsubishi_msc_protocol.h` and have a host-side test
that needs only a C++ compiler. It rebuilds a real captured frame byte-for-byte:

```bash
g++ -std=c++17 -Wall -Wextra -I components/mitsubishi_msc tests/test_protocol.cpp -o /tmp/test_protocol && /tmp/test_protocol
```

## Credits

Protocol constants follow
[ToniA/arduino-heatpumpir](https://github.com/ToniA/arduino-heatpumpir)
(`MitsubishiMSCHeatpumpIR`), checked against captures of the original remote.

## License

MIT. See [LICENSE](LICENSE).
