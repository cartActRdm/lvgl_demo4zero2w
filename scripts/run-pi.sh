#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

if [[ -z "${LVGL_FONT_FILE:-}" ]]; then
  for candidate in \
    /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc \
    /usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc \
    /usr/share/fonts/opentype/adobe-source-han-sans/SourceHanSansSC-Regular.otf \
    /usr/share/fonts/adobe-source-han-sans/SourceHanSansSC-Regular.otf \
    /usr/share/fonts/opentype/adobe-source-han-serif/SourceHanSerifCN-Regular.otf \
    /usr/share/fonts/adobe-source-han-serif/SourceHanSerifCN-Regular.otf
  do
    if [[ -r "$candidate" ]]; then
      export LVGL_FONT_FILE="$candidate"
      break
    fi
  done
fi

if [[ -z "${LVGL_FONT_FILE:-}" ]]; then
  echo "[lvgl-zero2w] warning: no readable system CJK font found; UI may fall back to bundled subset" >&2
fi

export LVGL_FONT_PX="${LVGL_FONT_PX:-14}"
export LVGL_DRAW_BUF_ROWS="${LVGL_DRAW_BUF_ROWS:-240}"
export LVGL_SHUTDOWN_CMD="${LVGL_SHUTDOWN_CMD:-/usr/sbin/poweroff}"
export LVGL_REBOOT_CMD="${LVGL_REBOOT_CMD:-/usr/sbin/reboot}"

export ILI9341_SPI_DEV="${ILI9341_SPI_DEV:-/dev/spidev0.0}"
export ILI9341_GPIOCHIP="${ILI9341_GPIOCHIP:-/dev/gpiochip0}"
export ILI9341_DC_GPIO="${ILI9341_DC_GPIO:-24}"
export ILI9341_RESET_GPIO="${ILI9341_RESET_GPIO:-25}"
export ILI9341_SPI_SPEED="${ILI9341_SPI_SPEED:-75000000}"
export ILI9341_ROTATION="${ILI9341_ROTATION:-0}"
export ILI9341_BGR="${ILI9341_BGR:-1}"
export ILI9341_INVERT="${ILI9341_INVERT:-0}"

export XPT2046_SPI_DEV="${XPT2046_SPI_DEV:-/dev/spidev0.1}"
export XPT2046_IRQ_GPIO="${XPT2046_IRQ_GPIO:-23}"
export XPT2046_SPI_SPEED="${XPT2046_SPI_SPEED:-2000000}"
export XPT2046_SPI_MODE="${XPT2046_SPI_MODE:-0}"
export XPT2046_RAW_X_MIN="${XPT2046_RAW_X_MIN:-360}"
export XPT2046_RAW_X_MAX="${XPT2046_RAW_X_MAX:-3880}"
export XPT2046_RAW_Y_MIN="${XPT2046_RAW_Y_MIN:-156}"
export XPT2046_RAW_Y_MAX="${XPT2046_RAW_Y_MAX:-3824}"
export XPT2046_SWAP_XY="${XPT2046_SWAP_XY:-0}"
export XPT2046_INVERT_X="${XPT2046_INVERT_X:-1}"
export XPT2046_INVERT_Y="${XPT2046_INVERT_Y:-0}"
export XPT2046_PRESSURE_MIN="${XPT2046_PRESSURE_MIN:-40}"

exec ./build/pi-native/lvgl-pi-spi "$@"
