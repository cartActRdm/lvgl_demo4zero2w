#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

mkdir -p third_party

LVGL_REPO_URL="${LVGL_REPO_URL:-https://github.com/lvgl/lvgl.git}"
LV_DRIVERS_REPO_URL="${LV_DRIVERS_REPO_URL:-https://github.com/lvgl/lv_drivers.git}"

if [ ! -d third_party/lvgl/.git ]; then
  git clone --depth 1 --branch v8.3.11 "$LVGL_REPO_URL" third_party/lvgl
else
  echo "third_party/lvgl already exists, skipping"
fi

if [ ! -d third_party/lv_drivers/.git ]; then
  git clone --depth 1 --branch v8.3.0 "$LV_DRIVERS_REPO_URL" third_party/lv_drivers
else
  echo "third_party/lv_drivers already exists, skipping"
fi
