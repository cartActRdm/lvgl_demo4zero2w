#!/usr/bin/env bash
set -euo pipefail

sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  git \
  pkg-config \
  libgpiod-dev \
  libfreetype-dev \
  fonts-noto-cjk

cat <<'EOF'

基础构建依赖已经安装完。

下一步通常是：
1. 在 raspi-config 里启用 SPI
2. 确认 `/dev/spidev0.0`、`/dev/spidev0.1` 和 `/dev/gpiochip0` 存在
3. 把当前用户加入 `gpio` / `spi` 组（重新登录后生效）
4. 拉取第三方依赖并编译运行：
   ./scripts/fetch-third-party.sh
   ./scripts/configure-pi-native.sh
   ./scripts/build-pi.sh
   ./scripts/run-pi.sh

如果看到权限错误，可以先试：
  sudo usermod -aG gpio,spi $USER

EOF
