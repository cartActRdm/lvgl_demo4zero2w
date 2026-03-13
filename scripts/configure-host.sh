#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build/host -DTARGET_HOST_SDL=ON -DTARGET_PI_FBDEV=OFF
