#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

export XPT2046_RAW_X_MIN="${XPT2046_RAW_X_MIN:-360}"
export XPT2046_RAW_X_MAX="${XPT2046_RAW_X_MAX:-3880}"
export XPT2046_RAW_Y_MIN="${XPT2046_RAW_Y_MIN:-156}"
export XPT2046_RAW_Y_MAX="${XPT2046_RAW_Y_MAX:-3824}"
export XPT2046_SWAP_XY="${XPT2046_SWAP_XY:-0}"
export XPT2046_INVERT_X="${XPT2046_INVERT_X:-1}"
export XPT2046_INVERT_Y="${XPT2046_INVERT_Y:-0}"

exec ./scripts/run-pi.sh "$@"
