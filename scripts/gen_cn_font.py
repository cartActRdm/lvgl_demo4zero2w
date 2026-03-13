#!/usr/bin/env python3
from __future__ import annotations

import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src" / "ui_demo.c"
OUT_C = ROOT / "src" / "fonts" / "lv_font_cn_14.c"
OUT_H = ROOT / "src" / "fonts" / "lv_font_cn_14.h"
FONT = Path("/usr/share/fonts/adobe-source-han-serif/SourceHanSerifCN-Regular.otf")

# Small practical supplement beyond the current demo strings.
# Keep explicit problem glyphs here so the subset still covers them even if UI text changes later.
EXTRA_TEXT = (
    "读性对比支下滑查"
    "中文界面设备网络蓝牙电池电源音量亮度时间日期系统连接断开"
    "保存取消确认完成失败错误警告名称密码用户语言主题版本更新"
    "风扇温度湿度电压电流功率模式自动手动正常异常离线在线"
    "加载刷新重试返回首页菜单更多帮助"
)


def ordered_unique(text: str) -> str:
    seen: set[str] = set()
    out: list[str] = []
    for ch in text:
        if ch in "\n\r\t":
            continue
        if ord(ch) < 0x80:
            continue
        if ch not in seen:
            seen.add(ch)
            out.append(ch)
    return "".join(out)


ui_text = SRC.read_text(encoding="utf-8")
ui_non_ascii = ordered_unique(ui_text)
all_symbols = ordered_unique(ui_non_ascii + EXTRA_TEXT)

OUT_C.parent.mkdir(parents=True, exist_ok=True)

cmd = [
    "npx",
    "--yes",
    "lv_font_conv",
    "--font",
    str(FONT),
    "--size",
    "14",
    "--bpp",
    "2",
    "--format",
    "lvgl",
    "--no-kerning",
    "--lv-font-name",
    "lv_font_cn_14",
    "--lv-include",
    "lvgl.h",
    "-r",
    "0x20-0x7E",
    "--symbols",
    all_symbols,
    "-o",
    str(OUT_C),
]

print("Generating:", OUT_C)
print("Subset glyph count:", len(all_symbols) + (0x7E - 0x20 + 1))
subprocess.run(cmd, check=True)

OUT_H.write_text(
    "#pragma once\n\n"
    "#include \"lvgl.h\"\n\n"
    "LV_FONT_DECLARE(lv_font_cn_14);\n",
    encoding="utf-8",
)

print("Wrote:", OUT_H)
