#!/usr/bin/env bash
#
# build.sh — Build the mmWave OS NuttX firmware
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
NUTTX_PATH="${PROJECT_DIR}/nuttx"

echo "═══════════════════════════════════════"
echo "  mmWave OS — Build"
echo "═══════════════════════════════════════"

# Source ESP-IDF environment
if [ -f "$HOME/esp-idf/export.sh" ]; then
  source "$HOME/esp-idf/export.sh" 2>/dev/null
fi

cd "$NUTTX_PATH"

# Clean previous build artifacts (optional)
if [ "${1:-}" = "clean" ]; then
  echo "[clean] Removing previous build..."
  make distclean 2>/dev/null || true
  echo "  Run ./scripts/configure.sh to re-configure"
  exit 0
fi

# Calculate CPU count for parallel build
NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "[build] Building with -j${NPROC}..."
echo ""

START_TIME=$(date +%s)

make -j"$NPROC" 2>&1 | tee "$PROJECT_DIR/build.log"
BUILD_STATUS=${PIPESTATUS[0]}

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

echo ""

if [ "$BUILD_STATUS" -eq 0 ]; then
  # Show build output info
  FIRMWARE="$NUTTX_PATH/nuttx.bin"
  if [ -f "$FIRMWARE" ]; then
    SIZE=$(stat -f%z "$FIRMWARE" 2>/dev/null || stat -c%s "$FIRMWARE" 2>/dev/null || echo "?")
    echo "═══════════════════════════════════════"
    echo "  Build SUCCESS (${ELAPSED}s)"
    echo ""
    echo "  Firmware : nuttx/nuttx.bin"
    echo "  Size     : ${SIZE} bytes"
    echo ""
    echo "  Flash:   ./scripts/flash.sh [port]"
    echo "═══════════════════════════════════════"
  fi
else
  echo "═══════════════════════════════════════"
  echo "  Build FAILED (${ELAPSED}s)"
  echo "  See build.log for details"
  echo "═══════════════════════════════════════"
  exit 1
fi
