#!/usr/bin/env bash
set -euo pipefail

SERVICE_NAME="lvgl-zero2w.service"
SERVICE_DST="/etc/systemd/system/${SERVICE_NAME}"

sudo systemctl disable --now "$SERVICE_NAME" 2>/dev/null || true
sudo rm -f "$SERVICE_DST"
sudo systemctl daemon-reload

echo "Removed $SERVICE_NAME"
