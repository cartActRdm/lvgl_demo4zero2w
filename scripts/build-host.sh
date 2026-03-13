#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake --build build/host -j"$(nproc)"
