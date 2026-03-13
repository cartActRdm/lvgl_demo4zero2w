#!/usr/bin/env bash
set -euo pipefail

PI_HOST="${PI_HOST:-haicoin@RPI-TEST}"
PI_DIR="${PI_DIR:-~/lvgl-zero2w}"

rsync -av --delete \
  --exclude build \
  --exclude .git \
  ./ "${PI_HOST}:${PI_DIR}"
