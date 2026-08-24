# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

An ESPHome **external component** (`panasonic_ac`) that drives Panasonic AC units over the CN-CNT serial
connector. It is not a standalone program: it is compiled into an ESPHome firmware image. Forked from
DomiStyle's `esphome-panasonic-ac`.

## Build / verify

There is no test suite. The only verification is that the example config compiles.

```bash
python3 -m venv .venv && ./.venv/bin/pip install -r requirements.txt   # once
./build.sh          # esphome compile ac.example.yaml
```

`build.sh` calls `./.venv/bin/esphome` directly, so the venv must exist. It is also wired up as the
default VS Code build task.

`secrets.yaml` is gitignored. If it is missing, copy `ac.example.secrets.yaml` over it (that is exactly
what `.github/workflows/build.yml` does before compiling).

The ESPHome version lives in **two** places that must be bumped together: `requirements.txt`
(`esphome==`) and `esphome.min_version` in `ac.example.yaml`.

CI (`.github/workflows/build.yml`) creates the same venv from `requirements.txt`, runs `build.sh` on
pushes to `master`, and posts a commit status labelled with the ESPHome version (read out of
`requirements.txt`). Two caches: `~/.cache/esphome/idf` minus `dist/` (esp-idf toolchain, keyed on `requirements.txt`) and
`.esphome/` (build objects, keyed on `requirements.txt` + `ac.example.yaml` + `components/**`, with a
prefix `restore-keys` so an edit still starts from the last incremental build).

## Architecture

### Codegen layer — `components/panasonic_ac/climate.py`

Defines `CONFIG_SCHEMA` and `to_code`. The distinguishing feature is the **`traits:` block**: all six
traits (`vertical_swing_mode`, `horizontal_swing_mode`, `nanoex_mode`, `current_temperature`,
`outside_temperature`, `current_power_consumption`) are `cv.Required` and each takes an explicit
`supported: true|false`. When `supported: true`, the trait's companion entity block (`selector:` /
`switch:` / `sensor:`) becomes mandatory — enforced by the `_validate_*_schema` extra validators.
`to_code` then only instantiates the select/switch/sensor entities for supported traits and wires them
into the C++ object via `set_*` setters, so an unsupported trait produces no entity and no code at all.

### C++ layer

- `esppac.h/.cpp` — `PanasonicAC`, the protocol-agnostic base: `Component` + `uart::UARTDevice` +
  `climate::Climate`. Owns the entity pointers, the RX buffer, and the `update_*` methods that publish
  state. Swing/nanoeX changes coming *from* Home Assistant arrive through `add_on_state_callback`
  lambdas registered in the `set_*` setters, which dispatch to the pure-virtual `on_*_change` hooks.
- `esppac_cnt.h/.cpp` — `CNT::PanasonicACCNT`, the CN-CNT protocol implementation (the only protocol
  here). Owns framing, polling, and all byte↔state translation.
- `panasonic_ac_traits_builder.h/.cpp` — builds `ClimateTraits` plus the custom fan-mode/preset lists.
- `panasonic_ac_select.h` / `panasonic_ac_switch.h` — trivial entities that just echo writes back via
  `publish_state`; the real work happens in the base class's state callbacks.
- `esppac_commands_cnt.h` — `CMD_POLL` and (commented-out) historical command templates.

### Protocol flow

`protocol/cn_cnt_protocol_descrption.md` documents the byte layout — keep it in sync when decoding new
bytes.

**Receive:** `read_data()` appends every available UART byte to `rx_buffer_`. `loop()` treats a
`READ_TIMEOUT` (20 ms) gap as end-of-frame, then `verify_packet()` checks minimum length, header
(`0xF0` control / `0x70` poll), the length byte (`size - 3`), and that all bytes sum to 0.
`handle_packet()` copies `rx_buffer_[2..11]` into the 10-byte `data` vector and calls `set_data()`.

**Transmit:** `handle_poll()` sends `CMD_POLL` every `POLL_INTERVAL` (5 s). Control requests do *not*
write to UART directly — `control()` and the `on_*_change` hooks copy `data` into `cmd` (if empty) and
mutate bytes in place; `handle_cmd()` flushes `cmd` after `CMD_INTERVAL` (250 ms), so several changes
made in one HA action coalesce into one packet. `send_command()` prepends header + length and appends
the two's-complement checksum.

**Index gotcha:** `data[i]` corresponds to `rx_buffer_[i + 2]`. `set_data()` deliberately reads the
extended fields (current/outside temperature at 18/19 with fallbacks at 21/22, power consumption at
28/29/30) straight from `rx_buffer_`, which only holds them for poll responses. `0x80` in a temperature
byte means "not supported".

### Traits building

`PanasonicACTraitsBuilder` is populated during codegen: `to_code` emits
`get_traits_builder().add_vertical_swing_mode()` / `add_horizontal_swing_mode()` for supported traits.
`build_traits()` is called from `PanasonicAC::setup()` and caches its result; after that, `add_*` calls
log a warning and are ignored. So any new trait must be registered from `to_code`, before `setup()`.
`CLIMATE_SWING_BOTH` is only added when both swing directions are present.

Custom fan modes and presets are registered on the *climate entity* (`set_supported_custom_fan_modes` /
`set_supported_custom_presets`), not on the traits object.

**Trailing-space hack:** `FAN_SPEED_LEVEL_AUTO` is `"Auto "` and `PRESET_NONE` is `"None "` — with a
trailing space, because ESPHome parses the space-less strings as the built-in `CLIMATE_FAN_MODE_AUTO`
and `CLIMATE_PRESET_NONE`. Comparisons in `esppac_cnt.cpp` rely on these constants; never "fix" the
spacing.

## Hardware notes (`ac.example.yaml`)

Targets an ESP32-C3 with `esp-idf`. The AC UART is 9600 baud, **even parity**, 1 stop bit on
GPIO18 (RX) / GPIO19 (TX). Because the C3 uses USB_SERIAL_JTAG on those same pins by default, the
logger must be pinned to `hardware_uart: UART0` or logging collides with the AC link.
