# Quick Start Guide

This is the fastest way to get mmWave OS running on real hardware.

For deeper details, see [WIRING.md](WIRING.md) and [README.md](../README.md).

## What you need

- ESP32-C6 DevKitC
- HLK-LD2410C mmWave radar module
- USB-C cable
- 4 jumper wires

## 1) Wire it up

Use this basic connection:

| ESP32-C6 | LD2410C |
|----------|---------|
| 3.3V     | VCC     |
| GND      | GND     |
| GPIO5    | RX      |
| GPIO4    | TX      |

If you want the full placement notes and troubleshooting tips, see [WIRING.md](WIRING.md).

## 2) Install the toolchain

```bash
cd mmwave-os
./scripts/setup-toolchain.sh
```

This script sets up ESP-IDF, pulls NuttX/NuttX-apps, and installs build tools.

Typical first-time setup takes around 10–15 minutes.

## 3) Configure NuttX

```bash
./scripts/configure.sh
```

Optional customization:

```bash
cd nuttx && make menuconfig
```

## 4) Build firmware

```bash
./scripts/build.sh
```

Expected output is `nuttx/nuttx.bin`.

## 5) Flash to the board

```bash
# auto-detect port
./scripts/flash.sh

# or provide one explicitly
./scripts/flash.sh /dev/cu.usbmodem14101
```

## 6) Open the serial console

```bash
screen /dev/cu.usbmodem14101 115200
```

After boot, you should end up at an `nsh>` prompt.

## 7) Configure Wi-Fi

```bash
nsh> config set wifi.ssid "MyNetwork"
nsh> config set wifi.psk "MyPassword"
nsh> config set boot.autostart_wifi 1

# apply now (or reboot)
nsh> ifup wlan0
nsh> wapi essid wlan0 "MyNetwork"
nsh> wapi passphrase wlan0 "MyPassword"
nsh> dhcpc_start wlan0
```

## 8) Verify the radar sensor

```bash
nsh> mmwave
nsh> mmwave -w
nsh> mmwave -j
nsh> mmwave -e on
```

Use `mmwave -w` for live updates while you move around in front of the sensor.

## 9) Connect Home Assistant

Create a long-lived access token in Home Assistant, then:

```bash
nsh> hactl config 192.168.1.100 "eyJ0eXAiOi..."
nsh> hactl test
nsh> hactl push
nsh> hactl start

# optional: start reporting on boot
nsh> config set boot.autostart_ha 1
```

The firmware publishes to `binary_sensor.mmwave_presence` with occupancy and distance/energy attributes.

## Troubleshooting

| Issue | What to check |
|------|----------------|
| No sensor data | TX/RX crossed correctly and sensor powered at 3.3V |
| Wi-Fi not connecting | `config get wifi.ssid` and `config get wifi.psk` |
| HA push failing | `hactl test`, token validity, HA IP/port |
| Board unstable after bad flash | Enter ROM bootloader (BOOT + reset) and reflash |
| Memory pressure | `sysinfo -m` |
