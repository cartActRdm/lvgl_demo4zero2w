#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

PI_HOST="${PI_HOST:-haicoin@RPI-TEST}"
PI_DIR="${PI_DIR:-~/lvgl-zero2w}"
INCLUDE_CMAKE="${INCLUDE_CMAKE:-1}"

RSYNC_ARGS=( -av )

rsync "${RSYNC_ARGS[@]}" src/ "${PI_HOST}:${PI_DIR}/src/"
rsync "${RSYNC_ARGS[@]}" config/ "${PI_HOST}:${PI_DIR}/config/"

if [[ "$INCLUDE_CMAKE" == "1" ]]; then
  rsync "${RSYNC_ARGS[@]}" CMakeLists.txt "${PI_HOST}:${PI_DIR}/CMakeLists.txt"
fi

echo "Synced UI files to ${PI_HOST}:${PI_DIR}"
