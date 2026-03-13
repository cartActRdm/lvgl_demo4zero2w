#!/usr/bin/env bash
set -euo pipefail

SPI_DEV="${ILI9341_SPI_DEV:-/dev/spidev0.0}"
TOUCH_SPI_DEV="${XPT2046_SPI_DEV:-/dev/spidev0.1}"
GPIOCHIP="${ILI9341_GPIOCHIP:-/dev/gpiochip0}"
DC_GPIO="${ILI9341_DC_GPIO:-24}"
RESET_GPIO="${ILI9341_RESET_GPIO:-25}"
TOUCH_IRQ_GPIO="${XPT2046_IRQ_GPIO:-23}"

printf 'Display SPI:  %s\n' "$SPI_DEV"
printf 'Touch SPI:    %s\n' "$TOUCH_SPI_DEV"
printf 'GPIO chip:    %s\n' "$GPIOCHIP"
printf 'DC GPIO:      %s\n' "$DC_GPIO"
printf 'RESET GPIO:   %s\n' "$RESET_GPIO"
printf 'Touch IRQ:    %s\n' "$TOUCH_IRQ_GPIO"
printf '\n-- display spi --\n'
ls -l "$SPI_DEV"
printf '\n-- touch spi --\n'
ls -l "$TOUCH_SPI_DEV"
printf '\n-- gpiochip --\n'
ls -l "$GPIOCHIP"
printf '\n-- all spi devices --\n'
ls -l /dev/spidev* || true
printf '\n-- all gpiochips --\n'
ls -l /dev/gpiochip* || true
