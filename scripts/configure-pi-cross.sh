#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build/pi \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/Toolchain-rpi-zero2w.cmake" \
  -DTARGET_HOST_SDL=OFF \
  -DTARGET_PI_FBDEV=ON
