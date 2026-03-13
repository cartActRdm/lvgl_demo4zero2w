#include "ui_font_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fonts/lv_font_cn_14.h"

#if LV_USE_FREETYPE
#include "src/extra/libs/freetype/lv_freetype.h"
#endif

enum {
    UI_FONT_DEFAULT_PX = 14,
};

typedef struct {
    bool attempted;
    bool uses_system_font;
    const lv_font_t *font;
    char description[256];
#if LV_USE_FREETYPE
    bool freetype_ready;
    lv_ft_info_t ft_info;
#endif
} ui_font_state_t;

static ui_font_state_t g_font_state;

static int env_i32_or_default(const char *name, int fallback) {
    const char *value = getenv(name);
    char *endptr = NULL;
    long parsed;

    if (value == NULL || value[0] == '\0') {
        return fallback;
    }

    parsed = strtol(value, &endptr, 0);
    if (endptr == value || *endptr != '\0') {
        return fallback;
    }

    return (int)parsed;
}

static bool file_readable(const char *path) {
    return path != NULL && path[0] != '\0' && access(path, R_OK) == 0;
}

static const char *pick_system_font_path(void) {
    static const char *const candidates[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.otf",
        "/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.otf",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSerifCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.otf",
        "/usr/share/fonts/truetype/noto/NotoSerifCJK-Regular.otf",
        "/usr/share/fonts/opentype/adobe-source-han-sans/SourceHanSansSC-Regular.otf",
        "/usr/share/fonts/opentype/adobe-source-han-serif/SourceHanSerifCN-Regular.otf",
        "/usr/share/fonts/adobe-source-han-sans/SourceHanSansSC-Regular.otf",
        "/usr/share/fonts/adobe-source-han-serif/SourceHanSerifCN-Regular.otf",
        "/usr/share/fonts/truetype/arphic/uming.ttc",
        "/usr/share/fonts/truetype/arphic/ukai.ttc",
    };
    const char *env_path = getenv("LVGL_FONT_FILE");

    if (file_readable(env_path)) {
        return env_path;
    }

    for (size_t i = 0; i < (sizeof(candidates) / sizeof(candidates[0])); ++i) {
        if (file_readable(candidates[i])) {
            return candidates[i];
        }
    }

    return NULL;
}

const lv_font_t *ui_font_get_default(void) {
    if (g_font_state.attempted) {
        return g_font_state.font;
    }

    memset(&g_font_state, 0, sizeof(g_font_state));
    g_font_state.attempted = true;
    g_font_state.font = &lv_font_cn_14;
    snprintf(g_font_state.description,
             sizeof(g_font_state.description),
             "bundled fallback subset font (lv_font_cn_14)");

#if LV_USE_FREETYPE
    do {
        const char *font_path = pick_system_font_path();
        const int font_px = env_i32_or_default("LVGL_FONT_PX", UI_FONT_DEFAULT_PX);

        if (font_path == NULL) {
            LV_LOG_WARN("No readable CJK system font found; forced system-font mode could not be satisfied, using bundled fallback subset font.");
            break;
        }

        if (!lv_freetype_init(LV_FREETYPE_CACHE_FT_FACES,
                              LV_FREETYPE_CACHE_FT_SIZES,
                              LV_FREETYPE_CACHE_SIZE)) {
            LV_LOG_WARN("lv_freetype_init failed; using bundled fallback subset font.");
            break;
        }

        g_font_state.freetype_ready = true;
        g_font_state.ft_info.name = font_path;
        g_font_state.ft_info.weight = (uint16_t)(font_px > 0 ? font_px : UI_FONT_DEFAULT_PX);
        g_font_state.ft_info.style = FT_FONT_STYLE_NORMAL;
        g_font_state.ft_info.mem = NULL;
        g_font_state.ft_info.mem_size = 0;

        if (!lv_ft_font_init(&g_font_state.ft_info) || g_font_state.ft_info.font == NULL) {
            LV_LOG_WARN("Failed to open system font %s; using bundled fallback subset font.", font_path);
            if (g_font_state.freetype_ready) {
                lv_freetype_destroy();
                g_font_state.freetype_ready = false;
            }
            break;
        }

        g_font_state.font = g_font_state.ft_info.font;
        g_font_state.uses_system_font = true;
        snprintf(g_font_state.description,
                 sizeof(g_font_state.description),
                 "%s (%upx, FreeType)",
                 font_path,
                 (unsigned)g_font_state.ft_info.weight);
        LV_LOG_USER("Using system font: %s", g_font_state.description);
    } while (0);
#else
    LV_LOG_WARN("LV_USE_FREETYPE is disabled; using bundled fallback subset font.");
#endif

    return g_font_state.font;
}

const char *ui_font_source_description(void) {
    (void)ui_font_get_default();
    return g_font_state.description;
}

bool ui_font_uses_system_font(void) {
    (void)ui_font_get_default();
    return g_font_state.uses_system_font;
}
