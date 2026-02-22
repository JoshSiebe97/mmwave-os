# Wiring and Placement Guide (ESP32-C6 + LD2410C)

This guide covers the wiring and physical placement choices that matter most for stable radar readings.

## Basic pin map

| ESP32-C6 DevKitC | LD2410C | Notes |
|------------------|---------|-------|
| 3.3V             | VCC     | 3.3V only |
| GND              | GND     | common ground required |
| GPIO5 (UART1 TX) | RX      | controller TX → sensor RX |
| GPIO4 (UART1 RX) | TX      | sensor TX → controller RX |
| GPIO6 (optional) | OUT     | optional digital presence signal |

## Electrical notes

- **Supply voltage:** LD2410C runs at 3.3V and usually draws around 70mA.
- **Signal levels:** UART is 3.3V logic, so direct connection to ESP32-C6 is fine.
- **Serial settings:** 256000 baud, 8N1.
- **Grounding:** bad or floating ground is the number one cause of flaky readings.

## UART defaults

This project assumes UART1 on:

- RX: GPIO4
- TX: GPIO5

If you change pins in board config or menuconfig, make sure your runtime configuration matches.

## Placement tips

- Mount around 1.5–2.0m high, pointed into the occupied space.
- Keep the antenna side clear of metal and dense electronics.
- Avoid mounting directly near fans/HVAC or vibrating surfaces.
- Thin walls can leak detection into adjacent rooms.
- For multiple sensors, separate them by roughly 3m or more when possible.

## Range model

The LD2410 reports range in gates. In this setup, each gate is roughly 75cm:

- Gate 0: 0–75cm
- Gate 1: 75–150cm
- Gate 2: 150–225cm
- …
- Gate 8: up to about 6.75m

## Quick validation after flashing

At the NSH prompt:

```bash
nsh> mmwave
nsh> mmwave -w
```

- `mmwave` gives a one-shot snapshot.
- `mmwave -w` gives live updates and is the easiest way to validate placement.

If readings are unstable or missing, check power, TX/RX crossover, and ground first.
