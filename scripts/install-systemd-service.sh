#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

SERVICE_NAME="lvgl-zero2w.service"
SERVICE_SRC="$(pwd)/systemd/${SERVICE_NAME}"
SERVICE_DST="/etc/systemd/system/${SERVICE_NAME}"

if [[ ! -f "$SERVICE_SRC" ]]; then
  echo "Service file not found: $SERVICE_SRC" >&2
  exit 1
fi

sudo install -m 0644 "$SERVICE_SRC" "$SERVICE_DST"
sudo systemctl daemon-reload
sudo systemctl enable "$SERVICE_NAME"

echo "Installed and enabled $SERVICE_NAME"
echo "Start now with: sudo systemctl start $SERVICE_NAME"
echo "Status with:    sudo systemctl status $SERVICE_NAME"
echo "Logs with:      journalctl -u $SERVICE_NAME -f"
