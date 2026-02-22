# mmWave OS

Small, command-line-first firmware for ESP32-C6 that boots straight into NuttX NSH,
reads an LD2410 mmWave radar sensor, and reports occupancy data to Home Assistant.

This project is intentionally minimal: no GUI, no heavy framework layers, and no
cloud dependency required for core sensing.

## Why this exists

The goal is low-latency presence sensing for home automation on very constrained
hardware. The device should power on, initialize fast, and be useful immediately
from a serial console.

## Current capabilities

- Boots to NSH shell on ESP32-C6
- Registers an LD2410 driver as `/dev/mmwave0`
- Exposes live radar readings through `mmwave`
- Stores persistent settings in LittleFS at `/config`
- Pushes occupancy state to Home Assistant via REST (`hactl`)
- Optional startup automation for Wi-Fi + HA reporting via boot scripts

## Hardware target

- **MCU:** ESP32-C6 (RISC-V, 512KB SRAM)
- **Sensor:** HLK-LD2410C (UART)
- **Connectivity:** Wi-Fi (active), Thread/Matter path planned

Pin wiring and placement guidance are in [docs/WIRING.md](docs/WIRING.md).

## Project layout

- `drivers/mmwave/` → LD2410 kernel-level character driver
- `apps/mmwave/` → shell command for sensor read/config
- `apps/hactl/` → Home Assistant integration command
- `apps/sysinfo/` → runtime diagnostics (heap/uptime/sensor)
- `apps/config/` → persistent key/value configuration tool
- `boards/esp32c6/` → defconfig, bring-up, boot scripts, partitions
- `scripts/` → setup, configure, build, and flash helpers
- `docs/` → quickstart and hardware wiring

## Quick start

```bash
./scripts/setup-toolchain.sh
./scripts/configure.sh
./scripts/build.sh
./scripts/flash.sh
```

Then open serial at 115200 and you should land at an `nsh>` prompt.

For a full walkthrough, see [docs/QUICKSTART.md](docs/QUICKSTART.md).

## Built-in commands

- `mmwave` — read/watch radar state and tune gates/sensitivity
- `hactl` — configure, test, and push to Home Assistant
- `config` — get/set/list/reset persistent settings
- `sysinfo` — check uptime, heap, and device health

## Boot flow

On startup, the board bring-up and scripts perform:

1. mount LittleFS at `/config`
2. register mmWave device (`/dev/mmwave0`)
3. run system init scripts from ROMFS
4. optionally auto-connect Wi-Fi and start HA reporting
5. drop into NSH shell

## Scope notes

This repository is an implementation foundation, not a finished product image.
Matter/Thread integration is planned but not complete in the current code.

## Testing

The project includes a host-side test suite that runs on your Mac or Linux
workstation — no ESP32 or NuttX simulator needed. Tests use the
[Unity](https://github.com/ThrowTheSwitch/Unity) framework and compile the
driver source directly against lightweight NuttX header stubs.

```bash
cd tests
make test
```

This builds and runs three suites:

- **test_parser** — exercises the LD2410 binary frame parser: valid frames,
  back-to-back frames, garbage rejection, corrupted headers/tails, oversized
  length fields, and parser state reset behavior (16 tests)
- **test_data_extract** — verifies that parsed frames populate `mmwave_data_s`
  correctly: all target states, field extraction, engineering mode per-gate
  arrays, boundary values, and invalid-type rejection (21 tests)
- **test_ha_format** — confirms the Home Assistant JSON body and HTTP request
  formatting: state on/off, attribute values, structural validity, truncation
  handling, and full request assembly (22 tests)

Run a single suite with `make test_parser`, `make test_data_extract`, or
`make test_ha_format`. See [tests/](tests/) for the full structure.

## License

MIT
