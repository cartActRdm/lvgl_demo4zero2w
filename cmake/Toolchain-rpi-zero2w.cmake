set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Raspberry Pi OS 64-bit on Zero 2 W => prefer AArch64 toolchain.
# Adjust these paths for your toolchain/sysroot.
set(TOOLCHAIN_PREFIX aarch64-linux-gnu)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)

# Optional: point this to your Raspberry Pi sysroot.
# set(CMAKE_SYSROOT /opt/sysroots/rpi-zero2w)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
