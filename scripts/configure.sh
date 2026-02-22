#!/usr/bin/env bash
#
# configure.sh — Configure NuttX for mmWave OS (ESP32-C6)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
NUTTX_PATH="${PROJECT_DIR}/nuttx"

echo "═══════════════════════════════════════"
echo "  mmWave OS — Configure NuttX"
echo "═══════════════════════════════════════"

# Source ESP-IDF environment
if [ -f "$HOME/esp-idf/export.sh" ]; then
  echo "[1/3] Loading ESP-IDF environment..."
  source "$HOME/esp-idf/export.sh" 2>/dev/null
else
  echo "ERROR: ESP-IDF not found. Run setup-toolchain.sh first."
  exit 1
fi

# Configure NuttX with ESP32-C6 NSH base config
echo "[2/3] Applying base ESP32-C6 NSH configuration..."
cd "$NUTTX_PATH"

# Start from the stock esp32c6-devkitc:nsh config
./tools/configure.sh esp32c6-devkitc:nsh

# Overlay our custom defconfig on top
echo "[3/3] Merging mmWave OS configuration..."

# Import our custom config options
# NuttX's kconfig merge: we write our overrides into .config
# then run olddefconfig to resolve dependencies

cat "$PROJECT_DIR/boards/esp32c6/defconfig" >> .config

# Resolve all kconfig dependencies
make olddefconfig

echo ""
echo "═══════════════════════════════════════"
echo "  Configuration complete!"
echo ""
echo "  To customize further: cd nuttx && make menuconfig"
echo "  To build:             ./scripts/build.sh"
echo "═══════════════════════════════════════"
