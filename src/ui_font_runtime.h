#pragma once

#include <stdbool.h>

#include "lvgl.h"

const lv_font_t *ui_font_get_default(void);
const char *ui_font_source_description(void);
bool ui_font_uses_system_font(void);
