#!/usr/bin/env bash
#
# setup-toolchain.sh — Install ESP-IDF and NuttX build prerequisites
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ESP_IDF_VERSION="v5.3"
NUTTX_VERSION="nuttx-12.7.0"

echo "═══════════════════════════════════════"
echo "  mmWave OS — Toolchain Setup"
echo "═══════════════════════════════════════"
echo ""

# ─── 1. System Dependencies ───

echo "[1/5] Installing system dependencies..."

if command -v brew &>/dev/null; then
  # macOS
  brew install --quiet kconfig-frontends genromfs python3 cmake ninja || true
elif command -v apt-get &>/dev/null; then
  # Debian/Ubuntu
  sudo apt-get update -qq
  sudo apt-get install -y -qq \
    git wget python3 python3-pip python3-venv \
    cmake ninja-build gperf ccache dfu-util \
    libffi-dev libssl-dev libusb-1.0-0 \
    kconfig-frontends genromfs
else
  echo "WARNING: Unsupported package manager. Install manually:"
  echo "  kconfig-frontends, genromfs, python3, cmake, ninja"
fi

# ─── 2. ESP-IDF ───

echo ""
echo "[2/5] Installing ESP-IDF ${ESP_IDF_VERSION}..."

ESP_IDF_PATH="${HOME}/esp-idf"
if [ ! -d "$ESP_IDF_PATH" ]; then
  git clone --depth 1 --branch "$ESP_IDF_VERSION" \
    https://github.com/espressif/esp-idf.git "$ESP_IDF_PATH"
  cd "$ESP_IDF_PATH"
  git submodule update --init --depth 1
  ./install.sh esp32c6
else
  echo "  ESP-IDF already exists at $ESP_IDF_PATH"
fi

# ─── 3. esptool ───

echo ""
echo "[3/5] Installing esptool..."

pip3 install --quiet esptool 2>/dev/null || \
  pip3 install --quiet --user esptool

# ─── 4. NuttX Source ───

echo ""
echo "[4/5] Cloning NuttX..."

NUTTX_PATH="${PROJECT_DIR}/nuttx"
NUTTX_APPS_PATH="${PROJECT_DIR}/nuttx-apps"

if [ ! -d "$NUTTX_PATH" ]; then
  git clone --depth 1 --branch "$NUTTX_VERSION" \
    https://github.com/apache/nuttx.git "$NUTTX_PATH"
else
  echo "  NuttX already exists at $NUTTX_PATH"
fi

if [ ! -d "$NUTTX_APPS_PATH" ]; then
  git clone --depth 1 --branch "$NUTTX_VERSION" \
    https://github.com/apache/nuttx-apps.git "$NUTTX_APPS_PATH"
else
  echo "  NuttX apps already exists at $NUTTX_APPS_PATH"
fi

# ─── 5. Symlink our custom code into NuttX tree ───

echo ""
echo "[5/5] Linking mmWave OS sources into NuttX..."

# Link our driver into NuttX drivers directory
DRIVER_DEST="$NUTTX_PATH/drivers/mmwave"
if [ ! -L "$DRIVER_DEST" ] && [ ! -d "$DRIVER_DEST" ]; then
  ln -sf "$PROJECT_DIR/drivers/mmwave" "$DRIVER_DEST"
  echo "  Linked drivers/mmwave → NuttX"
fi

# Link our apps into NuttX apps directory
for app in mmwave hactl sysinfo config; do
  APP_DEST="$NUTTX_APPS_PATH/$app"
  if [ ! -L "$APP_DEST" ] && [ ! -d "$APP_DEST" ]; then
    ln -sf "$PROJECT_DIR/apps/$app" "$APP_DEST"
    echo "  Linked apps/$app → NuttX apps"
  fi
done

# Link board ROMFS
ROMFS_DEST="$NUTTX_PATH/boards/risc-v/esp32c6/esp32c6-devkitc/src/romfs"
if [ ! -L "$ROMFS_DEST" ] && [ ! -d "$ROMFS_DEST" ]; then
  ln -sf "$PROJECT_DIR/boards/esp32c6/romfs" "$ROMFS_DEST"
  echo "  Linked board ROMFS"
fi

echo ""
echo "═══════════════════════════════════════"
echo "  Setup complete!"
echo ""
echo "  ESP-IDF  : $ESP_IDF_PATH"
echo "  NuttX    : $NUTTX_PATH"
echo "  Apps     : $NUTTX_APPS_PATH"
echo ""
echo "  Next: ./scripts/configure.sh"
echo "═══════════════════════════════════════"
