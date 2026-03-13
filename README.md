# LVGL for Raspberry Pi Zero 2 W

这个项目现在同时保留两条路：

- **主路线：Pi 上直接 userspace 驱动 ILI9341 + XPT2046**（`spidev` + `libgpiod`，不走 `fbdev`）
- **辅助路线：主机 SDL 模拟器**，用来本地看 UI

当前默认 demo 已经不是最初那个 “Hello + 单按钮”，而是一个更接近实机触控面板的小型多级页面界面：

- 基于 **Material Design 1** 风格重做首页与二级详情页
- `Home / Control / Status / Settings / About` 五个页面，首页负责一级导航，其他页面负责二级信息与交互
- 顶部彩色 app bar、卡片分层、状态 chip、列表式详情布局，更贴近小型 embedded console / control panel
- 首页聚焦卡片 + 下一项预览卡片，减少 240x320 小屏上的拥挤感
- 二级页统一成可滚动列表布局，适合内容超出一屏时继续浏览
- `Control` 页面保留滑条与状态反馈；`Settings` 页面保留三个轻量本地策略开关；`Status` / `About` 页面用于摘要与说明；“电源管理”会出现在首页轮换卡片中，点击后进入含“关机 / 重启”功能的二级页面
- 图标现在走 **运行时直接加载项目内 PNG 文件** 的路径，不再依赖编译进程序的内置图像资源
- 顶部标题栏按钮已经按 240x320 小屏做过一轮细调：尺寸收紧、白色图标、避免重复打开页面时图标漂移
- 底部 snackbar 现在改成更接近 Android 的单行/双行自适应样式
- 默认优先通过 LVGL FreeType 直接调用系统中文字体文件，避免每次改文案都重生子集字库

布局按 **240x320 竖屏 ILI9341** 做了触摸友好化，控件尺寸和间距更适合直接 SPI + 触摸屏使用，逻辑保持轻量，适合 Pi Zero 2 W。

针对你已经验证过的接线，Pi 侧默认按下面参数工作：

- 显示 SPI: **`/dev/spidev0.0`**（CE0 / spi0.0）
- 触摸 SPI: **`/dev/spidev0.1`**（CE1 / spi0.1）
- DC: **GPIO24**
- RESET: **GPIO25**
- 触摸 IRQ: **GPIO23**（XPT2046 PENIRQ，低有效）
- 分辨率: **240x320**
- 已知可用旋转: **0 / 180**
- 已知失败: **270**

## 快速导航

- [项目现状总结](#项目现状总结)
- [Pi OS Trixie 64-bit 上推荐流程](#pi-os-trixie-64-bit-上推荐流程)
- [运行时环境变量](#运行时环境变量)
- [当前这块屏的已校准启动脚本](#当前这块屏的已校准启动脚本)
- [修改 UI 后，最少需要同步到 Pi 的内容](#修改-ui-后最少需要同步到-pi-的内容)
- [轻量同步脚本](#轻量同步脚本)
- [中文字体说明](#中文字体说明)
- [故障排查与已知 caveats](#故障排查与已知-caveats)
- [Troubleshooting 实战清单](#troubleshooting-实战清单)

## 目录

- `src/main.c`：程序入口、LVGL 初始化、Pi/SDL HAL、主循环与运行时参数
- `src/ui_demo.c`：当前 Material 风格多页面 demo 的主要 UI/交互逻辑
- `src/ui_font_runtime.c` / `src/ui_font_runtime.h`：运行时系统字体加载与回退逻辑
- `src/fonts/lv_font_cn_14.c` / `src/fonts/lv_font_cn_14.h`：内置中文回退子集字体（仅在找不到系统字体时兜底）
- `src/pi_ili9341.c` / `src/pi_ili9341.h`：Pi 直驱 ILI9341 显示后端
- `src/pi_xpt2046.c` / `src/pi_xpt2046.h`：Pi 直驱 XPT2046 触摸后端
- `config/lv_conf.h`：LVGL 配置（包含 PNG / 文件系统 / FreeType 等开关）
- `scripts/gen_cn_font.py`：旧的子集字体生成脚本（现在主要作为回退字库维护工具）
- `scripts/bootstrap-pi-trixie.sh`：在 Pi 上安装基础构建依赖
- `scripts/configure-pi-native.sh`：配置 Pi 原生构建
- `scripts/configure-pi-cross.sh`：配置交叉编译构建
- `scripts/build-pi.sh`：编译 Pi 版本
- `scripts/run-pi.sh`：按默认 SPI/GPIO 参数运行 Pi 版本
- `scripts/run-pi-calibrated.sh`：按当前这块屏的已校准触摸参数运行 Pi 版本
- `scripts/check-pi-devices.sh`：检查显示 / 触摸 `spidev` 与 `gpiochip` 是否就绪
- `scripts/sync-to-pi.sh`：从开发机同步整个项目到 Pi（可选）
- `scripts/sync-ui-to-pi.sh`：只同步 `src/`、`config/` 和可选 `CMakeLists.txt`
- `scripts/install-systemd-service.sh` / `scripts/uninstall-systemd-service.sh`：安装 / 移除开机自启服务
- `systemd/lvgl-zero2w.service`：systemd 服务定义
- `scripts/fetch-third-party.sh`：把 `lvgl` / `lv_drivers` 拉到本地 `third_party/`
- `scripts/configure-host.sh` / `scripts/build-host.sh` / `scripts/run-host.sh`：本机 SDL 模拟器
- `docs/pi-ili9341-xpt2046.md`：Pi 直驱说明与注意事项

## 项目现状总结

这个项目目前已经从“验证 SPI 屏能亮 + 能点”的阶段，推进到了“可在 Pi Zero 2 W 上直接运行的轻量多页面触控 demo”阶段。按现状看，它的核心特点是：

- **显示链路稳定**：ILI9341 走 userspace SPI 直驱，当前实机稳定默认值是 `ILI9341_SPI_SPEED=75000000`
- **触摸链路可用**：XPT2046 已接成 LVGL pointer input，当前默认灵敏度按 `XPT2046_PRESSURE_MIN=40`
- **渲染参数已经向小屏实机调优**：`LVGL_DRAW_BUF_ROWS=240`
- **主循环响应做过提频**：默认 `LVGL_LOOP_SLEEP_US=3000`，比之前 5ms sleep 更灵敏
- **中文显示更 practical**：优先走系统字体 + FreeType，找不到时再退回仓库内置子集字体
- **UI 已从初始示例重构为 Material 风格 demo**：首页 + 二级详情页 + 顶部彩色 app bar + 卡片分层 + 底部自适应 snackbar

最近几轮已经修掉或调过这些实际问题：

- 修复了一个会导致段错误的空指针问题：`g_ui.status_mode_chip` 未初始化就被访问
- 补上了 LVGL 文件系统 / PNG 初始化，支持直接从项目目录加载 PNG 图标
- 细调了顶部标题栏按钮，避免图标过大、偏位、重复打开页面时持续上漂
- 把标题栏按钮图标统一改成白色
- 把底部 snackbar 改成更像 Android 的单行 / 双行自适应样式，避免中文提示被截断

如果后面继续迭代，这个项目最值得继续打磨的方向大概是：

1. 进一步提升 240x320 小屏上的“产品感”而不只是 demo 感
2. 继续压触摸延迟（例如进一步调 loop sleep，或减少 XPT2046 单次采样开销）
3. 继续检查 UI 文案与布局在中文场景下的溢出边界
4. 在稳定性优先的前提下，再做显示/触摸性能优化

## Pi OS Trixie 64-bit 上推荐流程

先把项目同步到 Pi，然后：

```bash
cd ~/lvgl-zero2w
./scripts/bootstrap-pi-trixie.sh
./scripts/check-pi-devices.sh
./scripts/fetch-third-party.sh
./scripts/configure-pi-native.sh
./scripts/build-pi.sh
./scripts/run-pi.sh
# 或：./scripts/run-pi-calibrated.sh
```

如果你之前已经配置/编译过旧的 `fbdev` 版本，建议先清理：

```bash
rm -rf build/pi-native
./scripts/configure-pi-native.sh
./scripts/build-pi.sh
```

## 运行时环境变量

默认值已经匹配你目前这块屏，但都可以覆盖：

```bash
LVGL_FONT_FILE=/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc
LVGL_FONT_PX=14
LVGL_DRAW_BUF_ROWS=240
LVGL_LOOP_SLEEP_US=3000
LVGL_SHUTDOWN_CMD=/usr/sbin/poweroff
LVGL_REBOOT_CMD=/usr/sbin/reboot

ILI9341_SPI_DEV=/dev/spidev0.0
ILI9341_GPIOCHIP=/dev/gpiochip0
ILI9341_DC_GPIO=24
ILI9341_RESET_GPIO=25
ILI9341_SPI_SPEED=75000000
ILI9341_SPI_MODE=0
ILI9341_ROTATION=0
ILI9341_BGR=1
ILI9341_INVERT=0

XPT2046_SPI_DEV=/dev/spidev0.1
XPT2046_IRQ_GPIO=23
XPT2046_SPI_SPEED=2000000
XPT2046_SPI_MODE=0
XPT2046_RAW_X_MIN=360
XPT2046_RAW_X_MAX=3880
XPT2046_RAW_Y_MIN=156
XPT2046_RAW_Y_MAX=3824
XPT2046_SWAP_XY=0
XPT2046_INVERT_X=1
XPT2046_INVERT_Y=0
XPT2046_PRESSURE_MIN=40
```

示例：

```bash
ILI9341_ROTATION=180 ./scripts/run-pi.sh
```

如果想降速排查花屏：

```bash
ILI9341_SPI_SPEED=8000000 ./scripts/run-pi.sh
```

如果触摸坐标方向不对，先从这几个变量试起：

```bash
XPT2046_SWAP_XY=0 ./scripts/run-pi.sh
XPT2046_INVERT_X=1 ./scripts/run-pi.sh
XPT2046_INVERT_Y=1 ./scripts/run-pi.sh
```

如果想直接看原始触摸坐标：

```bash
./scripts/run-pi.sh --touch-raw
# 或
LVGL_PI_MODE=touch-raw ./scripts/run-pi.sh
```

如果想跑四角校准：

```bash
./scripts/run-pi.sh --touch-calibrate
# 或
LVGL_PI_MODE=touch-calibrate ./scripts/run-pi.sh
```

校准模式会在屏幕上依次显示四个角的目标点，同时在 stdout 打印每次采样结果。
完成后它会给出建议值：

- `XPT2046_RAW_X_MIN`
- `XPT2046_RAW_X_MAX`
- `XPT2046_RAW_Y_MIN`
- `XPT2046_RAW_Y_MAX`
- `XPT2046_SWAP_XY`
- `XPT2046_INVERT_X`
- `XPT2046_INVERT_Y`

如果想禁用 IRQ、只靠压力轮询：

```bash
XPT2046_IRQ_GPIO=-1 ./scripts/run-pi.sh
```

如果想覆盖电源管理页里的系统动作命令：

```bash
LVGL_SHUTDOWN_CMD="systemctl poweroff" ./scripts/run-pi.sh
LVGL_REBOOT_CMD="systemctl reboot" ./scripts/run-pi.sh
```

## 当前这块屏的已校准启动脚本

你现在可以直接用：

```bash
./scripts/run-pi-calibrated.sh
```

它默认等价于：

```bash
XPT2046_RAW_X_MIN=350 XPT2046_RAW_X_MAX=3895 \
XPT2046_RAW_Y_MIN=156 XPT2046_RAW_Y_MAX=3824 \
XPT2046_SWAP_XY=0 XPT2046_INVERT_X=1 XPT2046_INVERT_Y=0 \
./scripts/run-pi.sh
```

如果你后面还想微调，执行时仍然可以覆盖这些环境变量。

## 依赖

Pi 直驱目标需要：

- `libgpiod-dev`
- Linux `spidev` 设备（通常内核自带，无需额外开发包）

脚本 `bootstrap-pi-trixie.sh` 已经会装构建依赖。

## 需要先确认的系统状态

在 Pi 上先确认：

```bash
ls -l /dev/spidev*
ls -l /dev/gpiochip*
```

如果 `/dev/spidev0.0` 或 `/dev/spidev0.1` 不存在，先启用 SPI：

```bash
sudo raspi-config
# Interface Options -> SPI -> Enable
```

如果 `spidev` / GPIO 权限不够，通常把用户加进 `spi` / `gpio` 组即可：

```bash
sudo usermod -aG gpio,spi $USER
# 重新登录后生效
```

## 现在的实现方式

Pi 目标现在不再依赖 Linux framebuffer。
应用直接做这些事：

1. 打开 `/dev/spidev0.0`
2. 用 `libgpiod` 控制 ILI9341 的 DC / RESET
3. 发送 ILI9341 初始化序列
4. 在 LVGL flush 回调里把 RGB565 像素直接推到 LCD
5. 打开 `/dev/spidev0.1`
6. 读取 XPT2046 的原始触摸坐标，并注册成 LVGL pointer input
7. 如果 IRQ GPIO 可用，就用 PENIRQ 先判定是否按下；否则退回压力轮询

这条路更贴近你已经跑通的 Python 直驱验证结果。

## 关于触摸

这次已经补上 **XPT2046 userspace 直驱接入**，目标是先拿到一个能点、能拖的 LVGL pointer input：

- 默认假设 XPT2046 挂在 `spi0.1` / CE1
- 默认假设 PENIRQ 在 GPIO23，低有效
- 默认做 5 次采样取中位值，尽量压一点抖动
- 默认校准范围是比较保守的 `200..3900`
- 默认 `SWAP_XY=1`，优先贴合常见 ILI9341 竖屏接法

它现在追求的是 **先工作**，不是开箱即精准校准。
如果实机方向或边界不对，就先调：

- `XPT2046_SWAP_XY`
- `XPT2046_INVERT_X`
- `XPT2046_INVERT_Y`
- `XPT2046_RAW_{X,Y}_{MIN,MAX}`

## 修改 UI 后，最少需要同步到 Pi 的内容

如果只是像这次一样改 UI、文案、交互，而 **底层 SPI / 触摸驱动没动**，最少同步这些即可：

- `src/`
- `config/`
- `CMakeLists.txt`
- `README.md`（可选，不影响运行）

如果这次要把 **系统字体接入、PNG 图标直载和页面滚动** 一起带到 Pi，上面这组里最关键的是：

- `src/ui_demo.c`
- `src/ui_demo.h`
- `src/ui_font_runtime.c`
- `src/ui_font_runtime.h`
- `src/fonts/lv_font_cn_14.c`
- `src/fonts/lv_font_cn_14.h`
- `src/main.c`
- `config/lv_conf.h`
- `icon_sources/material-icons/`
- `CMakeLists.txt`
- `scripts/bootstrap-pi-trixie.sh`

另外别忘了 Pi 系统里还要真的装有一份 CJK 字体文件，例如 `fonts-noto-cjk`。

然后在 Pi 上重新执行：

```bash
./scripts/configure-pi-native.sh
./scripts/build-pi.sh
./scripts/run-pi.sh
```

如果 `build/pi-native/` 已经是同一套配置，通常重新 `build-pi.sh` 就够；
但凡你改了 CMake、LVGL 配置、依赖开关，保险起见还是先重新 configure 一次。

## 轻量同步脚本

如果你经常只改界面，可以直接在开发机上跑：

```bash
./scripts/sync-ui-to-pi.sh
```

它默认同步：

- `src/`
- `config/`
- `CMakeLists.txt`

也可以关闭 `CMakeLists.txt` 同步：

```bash
INCLUDE_CMAKE=0 ./scripts/sync-ui-to-pi.sh
```

支持覆盖目标：

```bash
PI_HOST=haicoin@RPI-TEST PI_DIR=~/lvgl-zero2w ./scripts/sync-ui-to-pi.sh
```

同步后在 Pi 上通常只需要：

```bash
./scripts/build-pi.sh
sudo ./scripts/run-pi-calibrated.sh
```

## systemd 开机自启

如果你想让它开机自动启动，在 Pi 上执行：

```bash
./scripts/install-systemd-service.sh
sudo systemctl start lvgl-zero2w.service
```

常用查看方式：

```bash
sudo systemctl status lvgl-zero2w.service
journalctl -u lvgl-zero2w.service -f
```

如果要移除：

```bash
./scripts/uninstall-systemd-service.sh
```

当前服务默认直接使用这块屏已经验证过的触摸校准参数，并以 `root` 运行（因为要直接访问 `spidev` / `gpiochip`）。

## 中文字体说明

现在默认走的是 **系统字体优先**：

- 通过 LVGL 8.3 自带的 **FreeType** 接口，在运行时直接打开系统里的中文字体文件
- 默认会优先尝试这些常见路径：
  - `/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc`
  - `/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc`
  - `/usr/share/fonts/opentype/adobe-source-han-sans/SourceHanSansSC-Regular.otf`
  - `/usr/share/fonts/opentype/adobe-source-han-serif/SourceHanSerifCN-Regular.otf`
  - `/usr/share/fonts/adobe-source-han-sans/SourceHanSansSC-Regular.otf`
  - `/usr/share/fonts/adobe-source-han-serif/SourceHanSerifCN-Regular.otf`
- 也可以显式指定：

```bash
LVGL_FONT_FILE=/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc \
LVGL_FONT_PX=14 \
./scripts/run-pi.sh
```

### 为什么这样更 practical

因为这台 Pi Zero 2 W 跑的是 Linux，不是裸机：

- 系统里本来就能装现成中文字体
- UI 文案一改，不需要再手工重生成子集字库
- 更符合“直接调用系统汉字”的诉求

这次保留了仓库里的 `lv_font_cn_14`，但角色已经降级成 **最后兜底回退**：

- 如果运行时找不到可读的系统中文字体
- 或 FreeType 初始化失败
- 应用会退回到仓库内置子集字体，而不是直接崩掉

### Pi 上建议安装

```bash
sudo apt update
sudo apt install -y libfreetype-dev fonts-noto-cjk
```

如果 `fonts-noto-cjk` 装好后路径和上面不完全一样，可以先在 Pi 上查一下：

```bash
fc-list :lang=zh file family | sort
```

然后把你想用的字体路径塞给 `LVGL_FONT_FILE`。

### CMake 开关

默认启用系统字体加载：

```bash
cmake -S . -B build/pi-native -DTARGET_HOST_SDL=OFF -DTARGET_PI_SPI=ON -DUI_USE_SYSTEM_FREETYPE=ON
```

如果你临时想完全关掉 FreeType、只用仓库里的回退子集字体，也可以：

```bash
cmake -S . -B build/pi-native -DTARGET_HOST_SDL=OFF -DTARGET_PI_SPI=ON -DUI_USE_SYSTEM_FREETYPE=OFF
```

### 这次和以前的差别

以前：

- 改中文文案 → 需要重新跑 `scripts/gen_cn_font.py`
- 再把新的 `lv_font_cn_14.c/.h` 同步到 Pi

现在：

- 改中文文案 → 通常直接重新编译/运行就行
- 只要系统字体里有这些字，就能显示

`scripts/gen_cn_font.py` 还留着，但现在主要是给 **回退字库** 用，不再是主流程。

## SDL 模拟器

主机上仍然可以：

```bash
cd /home/haicoin/.openclaw/workspace/lvgl-zero2w
./scripts/configure-host.sh
./scripts/build-host.sh
./scripts/run-host.sh
```

## 故障排查与已知 caveats

- 这版优先追求 **Pi Zero 2 W 上最小可用**
- 已按你已有 Python 初始化序列迁移到 C
- 旋转 **0 / 180** 已知可用，**270** 你那边已知失败，所以这里不把它当推荐值
- 如果画面异常，优先试：
  - `ILI9341_ROTATION=180`
  - `ILI9341_SPI_SPEED=8000000`
  - `ILI9341_INVERT=1`
  - `ILI9341_BGR=0`
- 如果触摸方向异常，优先试：
  - `XPT2046_SWAP_XY=0`
  - `XPT2046_INVERT_X=1`
  - `XPT2046_INVERT_Y=1`
- 目前没有背光 GPIO 管理；默认假设背光已硬件常亮或系统层已处理
- 目前没有 DMA / TE / 双缓冲优化，先以稳定显示和基础触摸为主
- 如果想继续提高触摸/UI 响应，可以进一步尝试更小的 `LVGL_LOOP_SLEEP_US`（例如 2000 或 1000），但要注意 CPU 占用会跟着上升

### Troubleshooting 实战清单

#### 1. 同步到 Pi 后一运行就报“段错误” / `Segmentation fault`

优先做这些：

1. 先在主机上复现一次：

```bash
./scripts/build-host.sh
./build/host-check/lvgl-sim
```

2. 如果主机 SDL 版也崩，先用 gdb 看堆栈：

```bash
gdb -batch -ex run -ex bt --args ./build/host-check/lvgl-sim
```

3. 如果只是最近同步后才崩，优先检查这几类问题：
   - UI 对象指针未初始化就被访问
   - 新增的动态文本 / chip / button 没真正创建成功
   - 新增 PNG / 文件系统支持后，初始化顺序是否正确

这次项目里真实踩到过的根因是：

- `g_ui.status_mode_chip` 没初始化就被 `refresh_dynamic_content()` 访问，导致空指针段错误

#### 2. 图标不显示 / 显示成空白框

优先检查：

1. `config/lv_conf.h` 里是否启用了：
   - `LV_USE_PNG 1`
   - `LV_USE_FS_STDIO 1`
2. 程序启动时是否执行了 `lv_extra_init()`
3. Pi 上是否真的同步了图标目录：

```bash
icon_sources/material-icons/
```

4. 图标路径是否仍然存在，例如：

```bash
S:icon_sources/material-icons/refresh_24.png
```

如果你只是改了 `src/ui_demo.c`，但没把 `icon_sources/material-icons/` 同步过去，运行时一样会找不到图标。

#### 3. 顶部标题栏按钮图标太大、偏位，或每打开一次页面就继续上漂

优先检查：

- app bar 按钮尺寸是否按小屏做过收敛
- 按钮默认 padding / border 是否去掉
- 按钮内部图标是否被重复做了嵌套动画

这次项目里真实踩到过的根因是：

- 二级页 header 的 stagger 动画把按钮内部图标也反复做了位移，导致图标每次打开页面都会继续往上偏

如果复现这个问题，优先检查 `src/ui_demo.c` 里 header 子元素动画逻辑，而不只是盯着图标尺寸本身。

#### 4. 底部 snackbar 中文提示显示不全

优先检查：

- snackbar 高度是不是写死得太小
- 标签宽度是否足够容纳中文换行
- 文本是否允许 wrap

当前实现已经改成：

- 固定宽度
- 单行时最小高度
- 长文本时自动长高为双行

如果你后面又改 snackbar 样式，优先保住这个“自适应高度”逻辑，不要退回到固定 1 行高。

#### 5. 中文不显示 / 乱码 / 只有一部分字能显示

优先检查：

1. Pi 上有没有安装可用中文字体：

```bash
fc-list :lang=zh file family | sort
```

2. 必要时显式指定字体：

```bash
LVGL_FONT_FILE=/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc \
LVGL_FONT_PX=14 \
./scripts/run-pi.sh
```

3. 如果系统字体加载失败，确认 FreeType 相关依赖和 CMake 开关是否正确
4. 如果系统字体仍不可用，再检查仓库内置回退字库是否覆盖了你新加的汉字

#### 6. 触摸方向不对 / 点击位置偏得厉害

优先从这些变量开始试：

```bash
XPT2046_SWAP_XY=0
XPT2046_INVERT_X=1
XPT2046_INVERT_Y=1
```

如果还是不对，直接跑校准：

```bash
./scripts/run-pi.sh --touch-calibrate
```

校准后把输出的：

- `XPT2046_RAW_X_MIN`
- `XPT2046_RAW_X_MAX`
- `XPT2046_RAW_Y_MIN`
- `XPT2046_RAW_Y_MAX`
- `XPT2046_SWAP_XY`
- `XPT2046_INVERT_X`
- `XPT2046_INVERT_Y`

写回运行脚本或环境变量。

#### 7. 触摸响应偏肉 / 轮询不够快

优先检查：

- `LVGL_LOOP_SLEEP_US` 是否过大
- `XPT2046_PRESSURE_MIN` 是否设得过高
- 单次触摸采样次数是否太保守

当前默认建议：

- `LVGL_LOOP_SLEEP_US=3000`
- `XPT2046_PRESSURE_MIN=40`

如果你想更激进一点，可以继续试：

```bash
LVGL_LOOP_SLEEP_US=2000 ./scripts/run-pi.sh
LVGL_LOOP_SLEEP_US=1000 ./scripts/run-pi.sh
```

但要注意 CPU 占用会上升。

#### 8. 改了 UI 后，Pi 上看起来没变化

优先检查是不是同步漏文件了。

最常见的最小同步集合：

- 只改 UI / 文案：
  - `src/`
  - `config/`
  - `CMakeLists.txt`
- 如果涉及 PNG 图标直载：
  - 还要同步 `icon_sources/material-icons/`
- 如果涉及字体加载 / 启动逻辑：
  - 还要同步 `src/main.c`
  - `src/ui_font_runtime.*`

同步后在 Pi 上至少执行：

```bash
./scripts/build-pi.sh
./scripts/run-pi.sh
```

如果你改了 CMake、LVGL 配置或依赖开关，保险起见再跑一次：

```bash
./scripts/configure-pi-native.sh
./scripts/build-pi.sh
```

#### 9. 画面异常 / 花屏 / 刷新不稳定

优先试这些保守参数：

```bash
ILI9341_SPI_SPEED=8000000 ./scripts/run-pi.sh
ILI9341_ROTATION=180 ./scripts/run-pi.sh
ILI9341_INVERT=1 ./scripts/run-pi.sh
ILI9341_BGR=0 ./scripts/run-pi.sh
```

如果保守参数能稳定，说明问题大概率在：

- SPI 频率过高
- rotation / MADCTL 组合不对
- BGR / inversion 设置不匹配当前面板

## 后续可继续补的东西

- 把现在的四角校准继续升级成更完整的多点/矩阵校准
- 开机自启的 systemd service
- SPI 批量写优化 / 更大刷屏块
- 基于实机结果继续微调 `MADCTL` / inversion / SPI 频率 / touch mapping
