#!/usr/bin/env bash
#
# flash.sh — Flash mmWave OS firmware to ESP32-C6
#
# Usage: ./scripts/flash.sh [serial_port]
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
NUTTX_PATH="${PROJECT_DIR}/nuttx"

# Default serial port
PORT="${1:-}"

echo "═══════════════════════════════════════"
echo "  mmWave OS — Flash to ESP32-C6"
echo "═══════════════════════════════════════"

# Auto-detect port if not specified
if [ -z "$PORT" ]; then
  # macOS
  if ls /dev/cu.usbserial-* 1>/dev/null 2>&1; then
    PORT=$(ls /dev/cu.usbserial-* | head -1)
  elif ls /dev/cu.usbmodem* 1>/dev/null 2>&1; then
    PORT=$(ls /dev/cu.usbmodem* | head -1)
  # Linux
  elif ls /dev/ttyUSB* 1>/dev/null 2>&1; then
    PORT=$(ls /dev/ttyUSB* | head -1)
  elif ls /dev/ttyACM* 1>/dev/null 2>&1; then
    PORT=$(ls /dev/ttyACM* | head -1)
  else
    echo "ERROR: No serial port detected."
    echo "Usage: $0 /dev/ttyUSB0"
    exit 1
  fi
  echo "  Auto-detected port: $PORT"
fi

FIRMWARE="$NUTTX_PATH/nuttx.bin"

if [ ! -f "$FIRMWARE" ]; then
  echo "ERROR: Firmware not found at $FIRMWARE"
  echo "Run ./scripts/build.sh first."
  exit 1
fi

SIZE=$(stat -f%z "$FIRMWARE" 2>/dev/null || stat -c%s "$FIRMWARE" 2>/dev/null)
echo "  Firmware: $FIRMWARE ($SIZE bytes)"
echo "  Port:     $PORT"
echo ""

# Source ESP-IDF for esptool if available
if [ -f "$HOME/esp-idf/export.sh" ]; then
  source "$HOME/esp-idf/export.sh" 2>/dev/null
fi

# Flash using esptool.py
echo "[flash] Erasing and writing firmware..."

esptool.py \
  --chip esp32c6 \
  --port "$PORT" \
  --baud 921600 \
  --before default_reset \
  --after hard_reset \
  write_flash \
  --flash_mode dio \
  --flash_freq 80m \
  --flash_size detect \
  0x0 "$NUTTX_PATH/nuttx.bin"

echo ""
echo "═══════════════════════════════════════"
echo "  Flash complete!"
echo ""
echo "  Connect with: screen $PORT 115200"
echo "  Or:           minicom -D $PORT -b 115200"
echo "═══════════════════════════════════════"
