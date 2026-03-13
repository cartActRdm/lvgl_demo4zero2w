#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build/pi-native -DTARGET_HOST_SDL=OFF -DTARGET_PI_SPI=ON
