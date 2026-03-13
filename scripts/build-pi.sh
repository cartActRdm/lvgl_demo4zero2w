#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-build/pi-native}"
cmake --build "$BUILD_DIR" -j"$(nproc)"
