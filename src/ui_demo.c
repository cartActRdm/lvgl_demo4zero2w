#include "ui_demo.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "lvgl.h"
#include "ui_font_runtime.h"

typedef enum {
    PAGE_HOME = 0,
    PAGE_CONTROL,
    PAGE_STATUS,
    PAGE_SETTINGS,
    PAGE_ABOUT,
    PAGE_SHUTDOWN,
    PAGE_MAX
} ui_page_t;

typedef struct {
    ui_page_t page;
    const char *title;
    const char *subtitle;
    const char *hero_value;
    const char *hero_caption;
    const char *summary;
    const char *icon_path;
    lv_color_t color;
} feature_meta_t;

enum {
    UI_W = 240,
    UI_H = 320,
    UI_PAGE_ANIM_TIME = 260,
    UI_PAGE_ANIM_OFFSET = 26,
    UI_HOME_PAD = 12,
    UI_PAGE_PAD = 14,
    UI_APPBAR_H = 42,
    UI_TOPBAR_H = UI_APPBAR_H + 22,
    UI_FAB_SIZE = 46,
    UI_SNACK_W = 184,
    UI_SNACK_MIN_H = 36,
    UI_HOME_FOCUS_H = 132,
    UI_APPBAR_ICON_BTN = 36,
    UI_APPBAR_ICON_ZOOM = 192
};

typedef struct {
    lv_obj_t *root_pages[PAGE_MAX];
    lv_obj_t *header_roots[PAGE_MAX];
    lv_obj_t *content_roots[PAGE_MAX];
    lv_obj_t *detail_title[PAGE_MAX];
    lv_obj_t *detail_subtitle[PAGE_MAX];
    lv_obj_t *detail_hero_value[PAGE_MAX];
    lv_obj_t *detail_hero_caption[PAGE_MAX];
    lv_obj_t *detail_summary[PAGE_MAX];
    lv_obj_t *detail_status_chip[PAGE_MAX];
    lv_obj_t *app_action_icon[PAGE_MAX];
    lv_obj_t *home_title;
    lv_obj_t *home_subtitle;
    lv_obj_t *home_metric_label;
    lv_obj_t *home_state_label;
    lv_obj_t *home_focus_card;
    lv_obj_t *home_focus_icon;
    lv_obj_t *home_focus_title;
    lv_obj_t *home_focus_subtitle;
    lv_obj_t *home_focus_caption;
    lv_obj_t *home_focus_value_chip;
    lv_obj_t *home_prev_hint;
    lv_obj_t *home_next_card;
    lv_obj_t *home_next_icon;
    lv_obj_t *home_next_hint;
    lv_obj_t *snackbar;
    lv_obj_t *snackbar_label;
    lv_obj_t *control_slider;
    lv_obj_t *control_slider_label;
    lv_obj_t *control_power_chip;
    lv_obj_t *status_mode_chip;
    lv_obj_t *settings_dim_switch;
    lv_obj_t *settings_sound_switch;
    lv_obj_t *settings_lock_switch;
    ui_page_t active_page;
    bool system_running;
    bool eco_dim;
    bool touch_sound;
    bool auto_lock;
    uint8_t home_focus_index;
} ui_demo_state_t;

static ui_demo_state_t g_ui;

static void set_active_page(ui_page_t page, bool forward);

static const char *UI_ICON_BACK = "S:icon_sources/material-icons/back_arrow_24.png";
static const char *UI_ICON_REFRESH = "S:icon_sources/material-icons/refresh_24.png";
static const char *UI_ICON_PLAY = "S:icon_sources/material-icons/play_arrow_24.png";
static const char *UI_ICON_PAUSE = "S:icon_sources/material-icons/pause_24.png";
static const char *UI_ICON_RESTORE = "S:icon_sources/material-icons/restore_page_24.png";
static const char *UI_ICON_HOME = "S:icon_sources/material-icons/home_dashboard_24.png";
static const char *UI_ICON_CONTROL = "S:icon_sources/material-icons/control_tune_24.png";
static const char *UI_ICON_STATUS = "S:icon_sources/material-icons/status_analytics_24.png";
static const char *UI_ICON_SETTINGS = "S:icon_sources/material-icons/settings_admin_panel_24.png";
static const char *UI_ICON_ABOUT = "S:icon_sources/material-icons/about_info_24.png";

static const feature_meta_t k_features[] = {
    {PAGE_CONTROL,
     "控制中心",
     "输出控制",
     "68%",
     "目标输出与启停",
     "调节目标输出，启动或暂停系统，使用 Material 风格的大面积主按钮。",
     "S:icon_sources/material-icons/control_tune_24.png",
     LV_COLOR_MAKE(0x42, 0x85, 0xF4)},
    {PAGE_STATUS,
     "运行状态",
     "遥测概览",
     "24ms",
     "链路与刷新摘要",
     "把运行摘要、链路信息和最近动作整理成卡片，适合二级详情浏览。",
     "S:icon_sources/material-icons/status_analytics_24.png",
     LV_COLOR_MAKE(0x00, 0x96, 0x88)},
    {PAGE_SETTINGS,
     "偏好设置",
     "本地策略",
     "3项",
     "亮度反馈与锁定",
     "集中展示待机调光、触摸反馈和自动锁定等本地策略。",
     "S:icon_sources/material-icons/settings_admin_panel_24.png",
     LV_COLOR_MAKE(0xAB, 0x47, 0xBC)},
    {PAGE_ABOUT,
     "关于 Demo",
     "项目说明",
     "MD1",
     "Material Design 1 风格",
     "用于演示一级首页与二级详情结构，也保留了 SPI 小屏场景需要的高对比和大触控区域。",
     "S:icon_sources/material-icons/about_info_24.png",
     LV_COLOR_MAKE(0xFB, 0x8C, 0x00)},
    {PAGE_SHUTDOWN,
     "电源管理",
     "关机与重启",
     "Power",
     "系统电源动作",
     "提供关机与重启两个系统级动作入口，也会出现在首页轮换卡片中。",
     "S:icon_sources/material-icons/pause_24.png",
     LV_COLOR_MAKE(0xE5, 0x39, 0x35)},
};

static const feature_meta_t *feature_for_page(ui_page_t page) {
    for (size_t i = 0; i < sizeof(k_features) / sizeof(k_features[0]); ++i) {
        if (k_features[i].page == page) {
            return &k_features[i];
        }
    }
    return &k_features[0];
}

static lv_obj_t *create_text_label(lv_obj_t *parent,
                                   const char *text,
                                   lv_color_t color,
                                   lv_coord_t width,
                                   lv_text_align_t align) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    if (width > 0) {
        lv_obj_set_width(label, width);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    }
    lv_obj_set_style_text_font(label, ui_font_get_default(), 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, align, 0);
    return label;
}

static void style_surface(lv_obj_t *obj, lv_color_t color, lv_coord_t radius, uint8_t opa) {
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, opa, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_0, 0);
    lv_obj_set_style_shadow_ofs_y(obj, 0, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_black(), 0);
}

static lv_obj_t *create_card(lv_obj_t *parent, lv_coord_t width, lv_color_t bg) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, width, LV_SIZE_CONTENT);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    style_surface(card, bg, 12, LV_OPA_COVER);
    lv_obj_set_style_pad_left(card, 12, 0);
    lv_obj_set_style_pad_right(card, 12, 0);
    lv_obj_set_style_pad_top(card, 12, 0);
    lv_obj_set_style_pad_bottom(card, 12, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    return card;
}

static lv_obj_t *create_chip(lv_obj_t *parent, const char *text, lv_color_t bg, lv_color_t fg) {
    lv_obj_t *chip = lv_obj_create(parent);
    lv_obj_set_height(chip, LV_SIZE_CONTENT);
    lv_obj_set_width(chip, LV_SIZE_CONTENT);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    style_surface(chip, bg, 10, LV_OPA_COVER);
    lv_obj_set_style_shadow_width(chip, 0, 0);
    lv_obj_set_style_pad_left(chip, 8, 0);
    lv_obj_set_style_pad_right(chip, 8, 0);
    lv_obj_set_style_pad_top(chip, 4, 0);
    lv_obj_set_style_pad_bottom(chip, 4, 0);
    lv_obj_t *label = create_text_label(chip, text, fg, 0, LV_TEXT_ALIGN_CENTER);
    lv_obj_center(label);
    return chip;
}

static lv_obj_t *create_color_icon_badge(lv_obj_t *parent, const char *icon_path, lv_color_t bg, lv_coord_t size) {
    lv_obj_t *holder = lv_obj_create(parent);
    lv_obj_t *img;

    lv_obj_set_size(holder, size, size);
    lv_obj_clear_flag(holder, LV_OBJ_FLAG_SCROLLABLE);
    style_surface(holder, bg, size / 3, LV_OPA_COVER);
    lv_obj_set_style_shadow_width(holder, 0, 0);
    lv_obj_set_style_pad_all(holder, 0, 0);

    img = lv_img_create(holder);
    lv_img_set_src(img, icon_path);
    lv_img_set_zoom(img, UI_APPBAR_ICON_ZOOM);
    lv_obj_center(img);
    return holder;
}

static void hide_page(lv_obj_t *obj);

static void page_set_x(void *obj, int32_t value) {
    lv_obj_set_x((lv_obj_t *)obj, (lv_coord_t)value);
}

static void page_set_y_anim(void *obj, int32_t value) {
    lv_obj_set_y((lv_obj_t *)obj, (lv_coord_t)value);
}

static void page_set_h_anim(void *obj, int32_t value) {
    lv_obj_set_height((lv_obj_t *)obj, (lv_coord_t)value);
}

static void page_set_opa(void *obj, int32_t value) {
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)value, 0);
}

static void hide_page_ready_cb(lv_anim_t *a) {
    if (a != NULL && a->var != NULL) {
        hide_page((lv_obj_t *)a->var);
    }
}

static void animate_page(lv_obj_t *obj,
                         lv_coord_t from_x,
                         lv_coord_t to_x,
                         lv_opa_t from_opa,
                         lv_opa_t to_opa,
                         lv_anim_path_cb_t path_cb,
                         bool hide_when_done) {
    lv_anim_t a;

    lv_anim_del(obj, NULL);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(obj, from_x);
    lv_obj_set_style_opa(obj, from_opa, 0);

    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, from_x, to_x);
    lv_anim_set_time(&a, UI_PAGE_ANIM_TIME);
    lv_anim_set_path_cb(&a, path_cb != NULL ? path_cb : lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, page_set_x);
    if (hide_when_done) {
        lv_anim_set_ready_cb(&a, hide_page_ready_cb);
    }
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, from_opa, to_opa);
    lv_anim_set_time(&a, UI_PAGE_ANIM_TIME - 30);
    lv_anim_set_exec_cb(&a, page_set_opa);
    lv_anim_start(&a);
}

static void animate_child_y(void *obj, int32_t value) {
    lv_obj_set_y((lv_obj_t *)obj, (lv_coord_t)value);
}

static void animate_staggered_children_delayed(lv_obj_t *container, uint32_t base_delay) {
    if (container == NULL) {
        return;
    }

    const uint32_t count = lv_obj_get_child_cnt(container);
    for (uint32_t i = 0; i < count; ++i) {
        lv_obj_t *child = lv_obj_get_child(container, i);
        lv_coord_t final_y = lv_obj_get_y(child);
        lv_anim_t a;

        lv_anim_del(child, NULL);
        lv_obj_set_y(child, final_y + 14);
        lv_obj_set_style_opa(child, LV_OPA_0, 0);

        lv_anim_init(&a);
        lv_anim_set_var(&a, child);
        lv_anim_set_values(&a, final_y + 14, final_y);
        lv_anim_set_delay(&a, base_delay + (35 * i));
        lv_anim_set_time(&a, 160);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&a, animate_child_y);
        lv_anim_start(&a);

        lv_anim_init(&a);
        lv_anim_set_var(&a, child);
        lv_anim_set_values(&a, LV_OPA_0, LV_OPA_COVER);
        lv_anim_set_delay(&a, base_delay + (35 * i));
        lv_anim_set_time(&a, 130);
        lv_anim_set_exec_cb(&a, page_set_opa);
        lv_anim_start(&a);

    }
}

static void animate_staggered_children(lv_obj_t *container) {
    animate_staggered_children_delayed(container, 0);
}

static void hide_page(lv_obj_t *obj) {
    lv_anim_del(obj, NULL);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
}

static void show_snackbar(const char *text) {
    lv_label_set_text(g_ui.snackbar_label, text);
    lv_obj_update_layout(g_ui.snackbar_label);

    lv_coord_t text_h = lv_obj_get_height(g_ui.snackbar_label);
    lv_coord_t target_h = text_h + 16;
    if (target_h < UI_SNACK_MIN_H) {
        target_h = UI_SNACK_MIN_H;
    }

    lv_obj_set_size(g_ui.snackbar, UI_SNACK_W, target_h);
    lv_obj_align(g_ui.snackbar, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_center(g_ui.snackbar_label);
    lv_obj_clear_flag(g_ui.snackbar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_ui.snackbar);
    lv_anim_del(g_ui.snackbar, NULL);
    lv_obj_set_style_opa(g_ui.snackbar, LV_OPA_COVER, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, g_ui.snackbar);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_0);
    lv_anim_set_delay(&a, 1400);
    lv_anim_set_time(&a, 260);
    lv_anim_set_exec_cb(&a, page_set_opa);
    lv_anim_start(&a);
}

static void update_home_focus_card(void) {
    const size_t count = sizeof(k_features) / sizeof(k_features[0]);
    const feature_meta_t *meta = &k_features[g_ui.home_focus_index % count];
    const feature_meta_t *prev = &k_features[(g_ui.home_focus_index + count - 1U) % count];
    const feature_meta_t *next = &k_features[(g_ui.home_focus_index + 1U) % count];

    lv_img_set_src(g_ui.home_focus_icon, meta->icon_path);
    lv_label_set_text(g_ui.home_focus_title, meta->title);
    lv_label_set_text(g_ui.home_focus_subtitle, meta->subtitle);
    lv_label_set_text(g_ui.home_focus_caption, meta->hero_caption);
    lv_label_set_text(lv_obj_get_child(g_ui.home_focus_value_chip, 0), meta->hero_value);
    lv_img_set_src(g_ui.home_next_icon, next->icon_path);
    lv_label_set_text_fmt(g_ui.home_prev_hint, "↑  %s", prev->title);
    lv_label_set_text_fmt(g_ui.home_next_hint, "下一项 · %s", next->title);
    lv_obj_set_style_bg_color(g_ui.home_focus_card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(g_ui.home_focus_card, lv_color_mix(meta->color, lv_color_white(), LV_OPA_60), 0);
    lv_obj_set_style_bg_color(g_ui.home_focus_value_chip, meta->color, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(g_ui.home_focus_value_chip, 0), lv_color_white(), 0);
    lv_obj_set_style_text_color(g_ui.home_prev_hint, lv_color_mix(prev->color, lv_color_hex(0x607D8B), LV_OPA_60), 0);
    lv_obj_set_style_border_color(g_ui.home_next_card, lv_color_mix(next->color, lv_color_white(), LV_OPA_70), 0);
}

static void update_home_summary(void) {
    const int slider = lv_slider_get_value(g_ui.control_slider);
    lv_label_set_text_fmt(g_ui.home_metric_label,
                          "%s  ·  目标 %d%%",
                          g_ui.system_running ? "运行中" : "待机中",
                          slider);
    lv_label_set_text_fmt(g_ui.home_state_label,
                          "调光%s · 触摸%s · 锁定%s",
                          g_ui.eco_dim ? "开" : "关",
                          g_ui.touch_sound ? "开" : "关",
                          g_ui.auto_lock ? "开" : "关");
    update_home_focus_card();
}

static void refresh_dynamic_content(void) {
    const int slider = lv_slider_get_value(g_ui.control_slider);

    lv_label_set_text_fmt(g_ui.control_slider_label, "%d%%", slider);
    lv_label_set_text(lv_obj_get_child(g_ui.control_power_chip, 0),
                      g_ui.system_running ? "运行中" : "待机中");
    lv_obj_set_style_bg_color(g_ui.control_power_chip,
                              g_ui.system_running ? lv_color_hex(0xE8F0FE) : lv_color_hex(0xECEFF1),
                              0);
    lv_obj_set_style_text_color(lv_obj_get_child(g_ui.control_power_chip, 0),
                                g_ui.system_running ? lv_color_hex(0x1A73E8) : lv_color_hex(0x546E7A),
                                0);

    lv_label_set_text(lv_obj_get_child(g_ui.status_mode_chip, 0),
                      g_ui.system_running ? "Live telemetry" : "Idle snapshot");

    lv_obj_add_state(g_ui.settings_dim_switch, g_ui.eco_dim ? LV_STATE_CHECKED : 0);
    lv_obj_clear_state(g_ui.settings_dim_switch, g_ui.eco_dim ? 0 : LV_STATE_CHECKED);
    lv_obj_add_state(g_ui.settings_sound_switch, g_ui.touch_sound ? LV_STATE_CHECKED : 0);
    lv_obj_clear_state(g_ui.settings_sound_switch, g_ui.touch_sound ? 0 : LV_STATE_CHECKED);
    lv_obj_add_state(g_ui.settings_lock_switch, g_ui.auto_lock ? LV_STATE_CHECKED : 0);
    lv_obj_clear_state(g_ui.settings_lock_switch, g_ui.auto_lock ? 0 : LV_STATE_CHECKED);

    lv_label_set_text_fmt(g_ui.detail_summary[PAGE_CONTROL],
                          "使用大面积卡片与主按钮来模拟 Material Design 1 的层次。\n\n当前目标输出 %d%%，%s。",
                          slider,
                          g_ui.system_running ? "系统正在响应" : "系统处于待机");

    lv_label_set_text_fmt(g_ui.detail_summary[PAGE_STATUS],
                          "链路摘要：SPI 直驱稳定，触摸轮询正常。\n最近目标 %d%%，界面动画采用轻量 slide + fade。",
                          slider);

    lv_label_set_text_fmt(g_ui.detail_summary[PAGE_SETTINGS],
                          "本地策略：调光 %s / 触摸音 %s / 自动锁定 %s。\n适合用来演示二级页面中的列表与开关控件。",
                          g_ui.eco_dim ? "开" : "关",
                          g_ui.touch_sound ? "开" : "关",
                          g_ui.auto_lock ? "开" : "关");

    lv_label_set_text_fmt(g_ui.detail_summary[PAGE_ABOUT],
                          "这个 demo 参考 Material Design 1：彩色 app bar、卡片分层、FAB 和明确的页面切换。\n字体来源：%s",
                          ui_font_source_description());

    update_home_summary();
}

static void set_active_page(ui_page_t page, bool forward) {
    lv_obj_t *next = g_ui.root_pages[page];
    lv_obj_t *current = g_ui.root_pages[g_ui.active_page];

    if (page == g_ui.active_page) {
        return;
    }

    lv_obj_move_foreground(next);

    if (forward) {
        animate_page(next,
                     UI_W,
                     0,
                     LV_OPA_70,
                     LV_OPA_COVER,
                     lv_anim_path_ease_out,
                     false);
        animate_page(current,
                     0,
                     -(UI_W / 3),
                     LV_OPA_COVER,
                     LV_OPA_30,
                     lv_anim_path_ease_in,
                     true);
        animate_staggered_children_delayed(g_ui.header_roots[page], 20);
        animate_staggered_children_delayed(g_ui.content_roots[page], 90);
    } else {
        animate_page(next,
                     -(UI_W / 4),
                     0,
                     LV_OPA_30,
                     LV_OPA_COVER,
                     lv_anim_path_ease_out,
                     false);
        animate_page(current,
                     0,
                     UI_W,
                     LV_OPA_COVER,
                     LV_OPA_50,
                     lv_anim_path_ease_in,
                     true);
        animate_staggered_children_delayed(g_ui.header_roots[page], 10);
        animate_staggered_children_delayed(g_ui.content_roots[page], 70);
    }

    g_ui.active_page = page;
}

static void open_detail_event_cb(lv_event_t *e) {
    const ui_page_t page = (ui_page_t)(intptr_t)lv_event_get_user_data(e);
    const feature_meta_t *meta = feature_for_page(page);
    set_active_page(page, true);
    show_snackbar(meta->title);
}

static void back_home_event_cb(lv_event_t *e) {
    (void)e;
    set_active_page(PAGE_HOME, false);
    show_snackbar("返回主页");
}

static void fab_event_cb(lv_event_t *e) {
    const ui_page_t page = (ui_page_t)(intptr_t)lv_event_get_user_data(e);

    if (page == PAGE_CONTROL) {
        g_ui.system_running = !g_ui.system_running;
        lv_img_set_src(g_ui.app_action_icon[PAGE_CONTROL], g_ui.system_running ? UI_ICON_PAUSE : UI_ICON_PLAY);
        refresh_dynamic_content();
        show_snackbar(g_ui.system_running ? "控制已启动" : "控制已暂停");
        return;
    }

    if (page == PAGE_STATUS) {
        show_snackbar("状态已刷新");
        return;
    }

    if (page == PAGE_SETTINGS) {
        g_ui.eco_dim = true;
        g_ui.touch_sound = false;
        g_ui.auto_lock = true;
        refresh_dynamic_content();
        show_snackbar("设置恢复推荐值");
        return;
    }

    show_snackbar("这是 Material 风格结构 demo");
}

static void animate_home_focus_change(bool next_dir) {
    lv_anim_t a;
    lv_anim_del(g_ui.home_focus_card, NULL);
    lv_anim_del(g_ui.home_prev_hint, NULL);
    lv_anim_del(g_ui.home_next_card, NULL);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_ui.home_focus_card);
    lv_anim_set_values(&a, 0, next_dir ? -52 : 52);
    lv_anim_set_time(&a, 170);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_exec_cb(&a, page_set_y_anim);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_ui.home_focus_card);
    lv_anim_set_values(&a, UI_HOME_FOCUS_H + 12, UI_HOME_FOCUS_H - 26);
    lv_anim_set_time(&a, 150);
    lv_anim_set_exec_cb(&a, page_set_h_anim);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_ui.home_focus_card);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_20);
    lv_anim_set_time(&a, 150);
    lv_anim_set_exec_cb(&a, page_set_opa);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_ui.home_prev_hint);
    lv_anim_set_values(&a, 0, next_dir ? -12 : 20);
    lv_anim_set_time(&a, 170);
    lv_anim_set_exec_cb(&a, page_set_y_anim);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_ui.home_next_card);
    lv_anim_set_values(&a, 0, next_dir ? -30 : 18);
    lv_anim_set_time(&a, 170);
    lv_anim_set_exec_cb(&a, page_set_y_anim);
    lv_anim_start(&a);
}

static void settle_home_focus_change(bool next_dir) {
    lv_anim_t a;
    lv_obj_set_y(g_ui.home_focus_card, next_dir ? 52 : -52);
    lv_obj_set_style_opa(g_ui.home_focus_card, LV_OPA_20, 0);
    lv_obj_set_height(g_ui.home_focus_card, UI_HOME_FOCUS_H - 18);
    lv_obj_set_y(g_ui.home_prev_hint, next_dir ? 12 : -12);
    lv_obj_set_y(g_ui.home_next_card, next_dir ? 20 : -20);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_ui.home_focus_card);
    lv_anim_set_values(&a, next_dir ? 52 : -52, 0);
    lv_anim_set_time(&a, 260);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_set_exec_cb(&a, page_set_y_anim);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_ui.home_focus_card);
    lv_anim_set_values(&a, UI_HOME_FOCUS_H - 18, UI_HOME_FOCUS_H + 12);
    lv_anim_set_time(&a, 250);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_set_exec_cb(&a, page_set_h_anim);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_ui.home_focus_card);
    lv_anim_set_values(&a, LV_OPA_20, LV_OPA_COVER);
    lv_anim_set_time(&a, 220);
    lv_anim_set_exec_cb(&a, page_set_opa);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_ui.home_prev_hint);
    lv_anim_set_values(&a, next_dir ? 12 : -12, 0);
    lv_anim_set_time(&a, 200);
    lv_anim_set_exec_cb(&a, page_set_y_anim);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_ui.home_next_card);
    lv_anim_set_values(&a, next_dir ? 20 : -20, 0);
    lv_anim_set_time(&a, 200);
    lv_anim_set_exec_cb(&a, page_set_y_anim);
    lv_anim_start(&a);
}

static void home_shift_focus(bool next_dir) {
    const size_t count = sizeof(k_features) / sizeof(k_features[0]);
    animate_home_focus_change(next_dir);
    g_ui.home_focus_index = (uint8_t)((g_ui.home_focus_index + (next_dir ? 1U : (count - 1U))) % count);
    update_home_focus_card();
    settle_home_focus_change(next_dir);
    show_snackbar(k_features[g_ui.home_focus_index].title);
}

static void home_prev_event_cb(lv_event_t *e) {
    (void)e;
    home_shift_focus(false);
}

static void home_next_event_cb(lv_event_t *e) {
    (void)e;
    home_shift_focus(true);
}

static void home_open_focused_event_cb(lv_event_t *e) {
    (void)e;
    const feature_meta_t *meta = &k_features[g_ui.home_focus_index % (sizeof(k_features) / sizeof(k_features[0]))];
    set_active_page(meta->page, true);
    show_snackbar(meta->title);
}

static void slider_event_cb(lv_event_t *e) {
    (void)e;
    refresh_dynamic_content();
    show_snackbar("目标输出已更新");
}

static void switch_event_cb(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);

    if (sw == g_ui.settings_dim_switch) {
        g_ui.eco_dim = lv_obj_has_state(sw, LV_STATE_CHECKED);
        show_snackbar(g_ui.eco_dim ? "待机调光已开启" : "待机调光已关闭");
    } else if (sw == g_ui.settings_sound_switch) {
        g_ui.touch_sound = lv_obj_has_state(sw, LV_STATE_CHECKED);
        show_snackbar(g_ui.touch_sound ? "触摸音已开启" : "触摸音已关闭");
    } else if (sw == g_ui.settings_lock_switch) {
        g_ui.auto_lock = lv_obj_has_state(sw, LV_STATE_CHECKED);
        show_snackbar(g_ui.auto_lock ? "自动锁定已开启" : "自动锁定已关闭");
    }

    refresh_dynamic_content();
}

static lv_obj_t *create_detail_list_item(lv_obj_t *parent,
                                         const char *title,
                                         const char *subtitle,
                                         lv_color_t accent,
                                         bool with_switch,
                                         lv_obj_t **switch_out) {
    lv_obj_t *row = create_card(parent, UI_W - (UI_PAGE_PAD * 2), lv_color_white());
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *text_col = lv_obj_create(row);
    lv_obj_set_size(text_col, with_switch ? 136 : 188, LV_SIZE_CONTENT);
    lv_obj_clear_flag(text_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(text_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(text_col, 0, 0);
    lv_obj_set_style_pad_all(text_col, 0, 0);
    lv_obj_set_style_pad_row(text_col, 3, 0);
    lv_obj_set_layout(text_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(text_col, LV_FLEX_FLOW_COLUMN);
    create_text_label(text_col, title, lv_color_hex(0x263238), lv_pct(100), LV_TEXT_ALIGN_LEFT);
    create_text_label(text_col, subtitle, lv_color_hex(0x607D8B), lv_pct(100), LV_TEXT_ALIGN_LEFT);

    if (with_switch) {
        *switch_out = lv_switch_create(row);
    }

    return row;
}

static void power_action_event_cb(lv_event_t *e) {
    const intptr_t action = (intptr_t)lv_event_get_user_data(e);
    const char *cmd = NULL;
    const char *pending_text = NULL;
    const char *missing_text = NULL;

    if (action == 1) {
        cmd = getenv("LVGL_SHUTDOWN_CMD");
        pending_text = "正在执行关机...";
        missing_text = "未配置关机命令";
    } else if (action == 2) {
        cmd = getenv("LVGL_REBOOT_CMD");
        pending_text = "正在执行重启...";
        missing_text = "未配置重启命令";
    }

    if (cmd == NULL || cmd[0] == '\0') {
        show_snackbar(missing_text != NULL ? missing_text : "未配置系统动作");
        return;
    }

    show_snackbar(pending_text != NULL ? pending_text : "正在执行系统动作...");
    fflush(stdout);
    fflush(stderr);
    system(cmd);
}

static lv_obj_t *create_home_feature_card(lv_obj_t *parent, const feature_meta_t *meta) {
    lv_obj_t *card = create_card(parent, 102, lv_color_hex(0xFFFFFF));
    lv_obj_set_height(card, 118);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_pad_top(card, 0, 0);
    lv_obj_set_style_pad_bottom(card, 10, 0);
    lv_obj_set_style_pad_left(card, 10, 0);
    lv_obj_set_style_pad_right(card, 10, 0);
    lv_obj_set_style_pad_row(card, 0, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xD8E2EA), 0);
    lv_obj_add_event_cb(card, open_detail_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)meta->page);

    lv_obj_t *accent = lv_obj_create(card);
    lv_obj_add_flag(accent, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(accent, lv_pct(100), 8);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(accent, meta->color, 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(accent, 10, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_set_style_pad_all(accent, 0, 0);

    lv_obj_t *icon_wrap = lv_obj_create(card);
    lv_obj_add_flag(icon_wrap, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(icon_wrap, open_detail_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)meta->page);
    lv_obj_set_size(icon_wrap, lv_pct(100), 86);
    lv_obj_clear_flag(icon_wrap, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(icon_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(icon_wrap, 0, 0);
    lv_obj_set_style_pad_all(icon_wrap, 0, 0);

    lv_obj_t *icon_badge = create_color_icon_badge(icon_wrap, meta->icon_path, meta->color, 54);
    lv_obj_center(icon_badge);
    lv_obj_add_flag(icon_badge, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(icon_badge, open_detail_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)meta->page);
    return card;
}

static lv_obj_t *create_home_power_card(lv_obj_t *parent) {
    const feature_meta_t *meta = feature_for_page(PAGE_SHUTDOWN);
    lv_obj_t *card = create_card(parent, UI_W - (UI_HOME_PAD * 2), lv_color_white());
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_mix(meta->color, lv_color_white(), LV_OPA_50), 0);
    lv_obj_set_style_pad_top(card, 10, 0);
    lv_obj_set_style_pad_bottom(card, 10, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_add_event_cb(card, open_detail_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)PAGE_SHUTDOWN);

    lv_obj_t *top = lv_obj_create(card);
    lv_obj_set_size(top, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_set_style_pad_all(top, 0, 0);
    lv_obj_set_layout(top, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    create_color_icon_badge(top, meta->icon_path, meta->color, 36);
    create_chip(top, "电源", lv_color_hex(0xFDECEA), meta->color);
    create_text_label(card, "电源管理", lv_color_hex(0x263238), UI_W - 72, LV_TEXT_ALIGN_LEFT);
    create_text_label(card, "关机 / 重启", lv_color_hex(0x607D8B), UI_W - 72, LV_TEXT_ALIGN_LEFT);
    return card;
}

static lv_obj_t *create_power_action_card(lv_obj_t *parent,
                                          const char *title,
                                          const char *subtitle,
                                          const char *chip_text,
                                          lv_color_t accent,
                                          intptr_t action_id) {
    lv_obj_t *card = create_card(parent, UI_W - (UI_PAGE_PAD * 2), lv_color_white());
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_mix(accent, lv_color_white(), LV_OPA_60), 0);
    lv_obj_add_event_cb(card, power_action_event_cb, LV_EVENT_CLICKED, (void *)action_id);

    lv_obj_t *row = lv_obj_create(card);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    create_text_label(row, title, lv_color_hex(0x263238), 112, LV_TEXT_ALIGN_LEFT);
    create_chip(row, chip_text, lv_color_mix(accent, lv_color_white(), LV_OPA_80), accent);
    create_text_label(card, subtitle, lv_color_hex(0x607D8B), UI_W - 64, LV_TEXT_ALIGN_LEFT);
    return card;
}

static lv_obj_t *create_detail_page(ui_page_t page) {
    const feature_meta_t *meta = feature_for_page(page);

    lv_obj_t *page_root = lv_obj_create(lv_scr_act());
    lv_obj_set_size(page_root, UI_W, UI_H);
    lv_obj_set_pos(page_root, 0, 0);
    lv_obj_set_scrollbar_mode(page_root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(page_root, LV_DIR_VER);
    lv_obj_set_style_bg_color(page_root, lv_color_hex(0xF5F7FA), 0);
    lv_obj_set_style_bg_opa(page_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(page_root, 0, 0);
    lv_obj_set_style_pad_all(page_root, 0, 0);

    lv_obj_t *appbar = lv_obj_create(page_root);
    g_ui.header_roots[page] = appbar;
    lv_obj_set_size(appbar, UI_W, UI_TOPBAR_H);
    lv_obj_set_pos(appbar, 0, 0);
    lv_obj_clear_flag(appbar, LV_OBJ_FLAG_SCROLLABLE);
    style_surface(appbar, meta->color, 0, LV_OPA_COVER);
    lv_obj_set_style_shadow_width(appbar, 10, 0);
    lv_obj_set_style_shadow_opa(appbar, LV_OPA_20, 0);
    lv_obj_set_style_pad_left(appbar, 8, 0);
    lv_obj_set_style_pad_right(appbar, 8, 0);
    lv_obj_set_style_pad_top(appbar, 10, 0);
    lv_obj_set_style_pad_bottom(appbar, 10, 0);
    lv_obj_set_layout(appbar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(appbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(appbar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *content = lv_obj_create(page_root);
    g_ui.content_roots[page] = content;
    lv_obj_set_size(content, UI_W, UI_H - UI_TOPBAR_H);
    lv_obj_set_pos(content, 0, UI_TOPBAR_H);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_left(content, UI_PAGE_PAD, 0);
    lv_obj_set_style_pad_right(content, UI_PAGE_PAD, 0);
    lv_obj_set_style_pad_top(content, 12, 0);
    lv_obj_set_style_pad_bottom(content, 0, 0);
    lv_obj_set_style_pad_row(content, 10, 0);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *back = lv_btn_create(appbar);
    lv_obj_set_size(back, UI_APPBAR_ICON_BTN, UI_APPBAR_ICON_BTN);
    style_surface(back, lv_color_mix(meta->color, lv_color_black(), LV_OPA_20), UI_APPBAR_ICON_BTN / 2, LV_OPA_30);
    lv_obj_set_style_shadow_width(back, 0, 0);
    lv_obj_set_style_pad_all(back, 0, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_add_event_cb(back, back_home_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_icon = lv_img_create(back);
    lv_img_set_src(back_icon, UI_ICON_BACK);
    lv_img_set_zoom(back_icon, UI_APPBAR_ICON_ZOOM);
    lv_obj_set_style_img_recolor(back_icon, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(back_icon, LV_OPA_COVER, 0);
    lv_obj_center(back_icon);

    lv_obj_t *title_col = lv_obj_create(appbar);
    lv_obj_set_size(title_col, 128, LV_SIZE_CONTENT);
    lv_obj_clear_flag(title_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(title_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_col, 0, 0);
    lv_obj_set_style_pad_all(title_col, 0, 0);
    lv_obj_set_style_pad_row(title_col, 2, 0);
    lv_obj_set_layout(title_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(title_col, LV_FLEX_FLOW_COLUMN);

    g_ui.detail_title[page] = create_text_label(title_col, meta->title, lv_color_white(), 128, LV_TEXT_ALIGN_LEFT);
    g_ui.detail_subtitle[page] = create_text_label(title_col, meta->subtitle, lv_color_hex(0xE8F0FE), 128, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *app_action = lv_btn_create(appbar);
    lv_obj_set_size(app_action, UI_APPBAR_ICON_BTN, UI_APPBAR_ICON_BTN);
    style_surface(app_action, lv_color_mix(meta->color, lv_color_black(), LV_OPA_20), UI_APPBAR_ICON_BTN / 2, LV_OPA_30);
    lv_obj_set_style_shadow_width(app_action, 0, 0);
    lv_obj_set_style_pad_all(app_action, 0, 0);
    lv_obj_set_style_border_width(app_action, 0, 0);
    lv_obj_add_event_cb(app_action, fab_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)page);
    g_ui.app_action_icon[page] = lv_img_create(app_action);
    lv_img_set_src(g_ui.app_action_icon[page], page == PAGE_CONTROL ? UI_ICON_PLAY : (page == PAGE_SETTINGS ? UI_ICON_RESTORE : UI_ICON_REFRESH));
    lv_img_set_zoom(g_ui.app_action_icon[page], UI_APPBAR_ICON_ZOOM);
    lv_obj_set_style_img_recolor(g_ui.app_action_icon[page], lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(g_ui.app_action_icon[page], LV_OPA_COVER, 0);
    lv_obj_center(g_ui.app_action_icon[page]);

    lv_obj_t *hero = create_card(content, UI_W - (UI_PAGE_PAD * 2), lv_color_white());
    lv_obj_set_style_bg_color(hero, lv_color_white(), 0);
    lv_obj_set_style_pad_bottom(hero, 14, 0);

    lv_obj_t *hero_top = lv_obj_create(hero);
    lv_obj_set_size(hero_top, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_clear_flag(hero_top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(hero_top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hero_top, 0, 0);
    lv_obj_set_style_pad_all(hero_top, 0, 0);
    lv_obj_set_layout(hero_top, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(hero_top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hero_top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    create_color_icon_badge(hero_top, meta->icon_path, meta->color, 40);
    g_ui.detail_status_chip[page] = create_chip(hero_top, "Overview", lv_color_hex(0xECEFF1), lv_color_hex(0x546E7A));
    if (page == PAGE_STATUS) {
        g_ui.status_mode_chip = g_ui.detail_status_chip[page];
    }

    g_ui.detail_hero_value[page] = create_text_label(hero, meta->hero_value, meta->color, 0, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_font(g_ui.detail_hero_value[page], ui_font_get_default(), 0);
    g_ui.detail_hero_caption[page] = create_text_label(hero, meta->hero_caption, lv_color_hex(0x607D8B), UI_W - 56, LV_TEXT_ALIGN_LEFT);
    g_ui.detail_summary[page] = create_text_label(hero, meta->summary, lv_color_hex(0x455A64), UI_W - 56, LV_TEXT_ALIGN_LEFT);

    if (page == PAGE_CONTROL) {
        lv_obj_t *card = create_card(content, UI_W - (UI_PAGE_PAD * 2), lv_color_white());
        create_text_label(card, "目标输出", lv_color_hex(0x263238), 0, LV_TEXT_ALIGN_LEFT);
        g_ui.control_slider_label = create_text_label(card, "68%", meta->color, 0, LV_TEXT_ALIGN_LEFT);
        g_ui.control_slider = lv_slider_create(card);
        lv_obj_set_width(g_ui.control_slider, lv_pct(100));
        lv_slider_set_range(g_ui.control_slider, 0, 100);
        lv_slider_set_value(g_ui.control_slider, 68, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(g_ui.control_slider, lv_color_hex(0xCFD8DC), LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_ui.control_slider, meta->color, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(g_ui.control_slider, lv_color_white(), LV_PART_KNOB);
        lv_obj_set_style_border_width(g_ui.control_slider, 2, LV_PART_KNOB);
        lv_obj_set_style_border_color(g_ui.control_slider, meta->color, LV_PART_KNOB);
        lv_obj_set_style_pad_all(g_ui.control_slider, 3, LV_PART_KNOB);
        lv_obj_add_event_cb(g_ui.control_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        g_ui.control_power_chip = create_chip(card, "待机中", lv_color_hex(0xECEFF1), lv_color_hex(0x546E7A));
        create_detail_list_item(content, "控制模式", "点击标题栏右侧即可启停当前控制。", meta->color, false, NULL);
        create_detail_list_item(content, "输出说明", "当前页面统一成列表式布局，便于小屏滚动浏览。", meta->color, false, NULL);
    } else if (page == PAGE_STATUS) {
        create_detail_list_item(content, "触摸轮询", "当前约 12 Hz，采样与校准正常。", meta->color, false, NULL);
        create_detail_list_item(content, "SPI 刷新", "75 MHz 运行稳定，局部刷新正常。", meta->color, false, NULL);
        create_detail_list_item(content, "运行摘要", "Idle snapshot · 标题栏右侧可触发刷新动作。", meta->color, false, NULL);
    } else if (page == PAGE_SETTINGS) {
        create_detail_list_item(content, "待机调光", "空闲时降低亮度", meta->color, true, &g_ui.settings_dim_switch);
        lv_obj_add_event_cb(g_ui.settings_dim_switch, switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        create_detail_list_item(content, "触摸音效", "提供轻量反馈", meta->color, true, &g_ui.settings_sound_switch);
        lv_obj_add_event_cb(g_ui.settings_sound_switch, switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        create_detail_list_item(content, "自动锁定", "长时间无操作自动回退", meta->color, true, &g_ui.settings_lock_switch);
        lv_obj_add_event_cb(g_ui.settings_lock_switch, switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    } else if (page == PAGE_ABOUT) {
        create_detail_list_item(content, "设计语言", "Material Design 1 视觉主体，菜单切换借一点 Metro 动势。", meta->color, false, NULL);
        create_detail_list_item(content, "图标来源", "当前图标直接从项目目录中的 Material Icons PNG 文件加载。", meta->color, false, NULL);
        create_detail_list_item(content, "字体来源", ui_font_source_description(), meta->color, false, NULL);
    } else if (page == PAGE_SHUTDOWN) {
        create_power_action_card(content,
                                 "关机",
                                 "结束当前系统并关闭 Pi 电源。默认执行 LVGL_SHUTDOWN_CMD。",
                                 "Power off",
                                 meta->color,
                                 1);
        create_power_action_card(content,
                                 "重启",
                                 "重新启动系统。默认执行 LVGL_REBOOT_CMD。",
                                 "Reboot",
                                 lv_color_hex(0xFB8C00),
                                 2);
        create_detail_list_item(content, "动作说明", "这些操作会直接调用系统命令，建议仅在 Pi 实机环境中使用。", meta->color, false, NULL);
    }

    lv_obj_t *spacer = lv_obj_create(content);
    lv_obj_set_size(spacer, 1, 24);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);

    hide_page(page_root);
    return page_root;
}

void ui_demo_create(void) {
    lv_obj_t *scr = lv_scr_act();

    g_ui = (ui_demo_state_t){0};
    g_ui.active_page = PAGE_HOME;
    g_ui.eco_dim = true;
    g_ui.touch_sound = false;
    g_ui.auto_lock = true;
    g_ui.system_running = false;

    lv_obj_set_style_bg_color(scr, lv_color_hex(0xECEFF1), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(0x263238), 0);
    lv_obj_set_style_text_font(scr, ui_font_get_default(), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    lv_obj_t *home = lv_obj_create(scr);
    g_ui.root_pages[PAGE_HOME] = home;
    lv_obj_set_size(home, UI_W, UI_H);
    lv_obj_set_pos(home, 0, 0);
    lv_obj_clear_flag(home, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(home, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(home, lv_color_hex(0xECEFF1), 0);
    lv_obj_set_style_bg_opa(home, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(home, 0, 0);
    lv_obj_set_style_pad_all(home, 0, 0);

    lv_obj_t *topbar = lv_obj_create(home);
    g_ui.header_roots[PAGE_HOME] = topbar;
    lv_obj_set_size(topbar, UI_W, UI_TOPBAR_H);
    lv_obj_set_pos(topbar, 0, 0);
    lv_obj_clear_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);
    style_surface(topbar, lv_color_hex(0x1A73E8), 0, LV_OPA_COVER);
    lv_obj_set_style_pad_left(topbar, UI_HOME_PAD, 0);
    lv_obj_set_style_pad_right(topbar, UI_HOME_PAD, 0);
    lv_obj_set_style_pad_top(topbar, 10, 0);
    lv_obj_set_style_pad_bottom(topbar, 10, 0);
    lv_obj_set_layout(topbar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(topbar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(topbar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    g_ui.home_title = create_text_label(topbar, "设备首页", lv_color_white(), 0, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_font(g_ui.home_title, ui_font_get_default(), 0);
    g_ui.home_subtitle = create_text_label(topbar,
                                           "主菜单 · 触摸面板",
                                           lv_color_hex(0xDCE8FF),
                                           UI_W - (UI_HOME_PAD * 2),
                                           LV_TEXT_ALIGN_LEFT);

    lv_obj_t *home_content = lv_obj_create(home);
    g_ui.content_roots[PAGE_HOME] = home_content;
    lv_obj_set_size(home_content, UI_W, UI_H - UI_TOPBAR_H);
    lv_obj_set_pos(home_content, 0, UI_TOPBAR_H);
    lv_obj_set_scroll_dir(home_content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(home_content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(home_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(home_content, 0, 0);
    lv_obj_set_style_pad_top(home_content, 12, 0);
    lv_obj_set_style_pad_bottom(home_content, 12, 0);
    lv_obj_set_style_pad_left(home_content, 0, 0);
    lv_obj_set_style_pad_right(home_content, 0, 0);
    lv_obj_set_style_pad_row(home_content, 10, 0);
    lv_obj_set_layout(home_content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(home_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(home_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *summary = create_card(home_content, UI_W - (UI_HOME_PAD * 2), lv_color_hex(0xFFFFFF));
    lv_obj_set_style_radius(summary, 16, 0);
    lv_obj_set_style_border_width(summary, 1, 0);
    lv_obj_set_style_border_color(summary, lv_color_hex(0xD7E3FC), 0);
    lv_obj_set_style_bg_color(summary, lv_color_hex(0xF8FBFF), 0);
    lv_obj_set_style_pad_top(summary, 10, 0);
    lv_obj_set_style_pad_bottom(summary, 10, 0);

    lv_obj_t *summary_top = lv_obj_create(summary);
    lv_obj_set_size(summary_top, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_clear_flag(summary_top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(summary_top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(summary_top, 0, 0);
    lv_obj_set_style_pad_all(summary_top, 0, 0);
    lv_obj_set_layout(summary_top, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(summary_top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(summary_top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    create_text_label(summary_top, "系统概览", lv_color_hex(0x5F7EA6), 0, LV_TEXT_ALIGN_LEFT);
    create_chip(summary_top,
                g_ui.system_running ? "运行中" : "待机中",
                lv_color_hex(0xE8F0FE),
                lv_color_hex(0x1A73E8));

    g_ui.home_metric_label = create_text_label(summary, "待机中 · 目标 68%", lv_color_hex(0x1F2D3D), UI_W - 48, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_font(g_ui.home_metric_label, ui_font_get_default(), 0);
    g_ui.home_state_label = create_text_label(summary, "调光开 · 触摸关 · 锁定开", lv_color_hex(0x607D8B), UI_W - 48, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *footer_strip = lv_obj_create(summary);
    lv_obj_set_size(footer_strip, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_clear_flag(footer_strip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(footer_strip, lv_color_hex(0xEEF4FB), 0);
    lv_obj_set_style_bg_opa(footer_strip, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(footer_strip, 10, 0);
    lv_obj_set_style_border_width(footer_strip, 0, 0);
    lv_obj_set_style_pad_left(footer_strip, 8, 0);
    lv_obj_set_style_pad_right(footer_strip, 8, 0);
    lv_obj_set_style_pad_top(footer_strip, 4, 0);
    lv_obj_set_style_pad_bottom(footer_strip, 4, 0);
    create_text_label(footer_strip, "SPI 直连 · 240x320 · 快速入口", lv_color_hex(0x5D7388), UI_W - 64, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *section = lv_obj_create(home_content);
    lv_obj_set_size(section, UI_W - (UI_HOME_PAD * 2), LV_SIZE_CONTENT);
    lv_obj_clear_flag(section, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(section, 0, 0);
    lv_obj_set_style_pad_all(section, 0, 0);
    lv_obj_set_style_pad_row(section, 10, 0);
    lv_obj_set_layout(section, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(section, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    g_ui.home_prev_hint = create_text_label(section, "", lv_color_hex(0x90A4AE), 180, LV_TEXT_ALIGN_LEFT);
    lv_obj_add_flag(g_ui.home_prev_hint, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_ui.home_prev_hint, home_prev_event_cb, LV_EVENT_CLICKED, NULL);

    g_ui.home_focus_card = create_card(section, UI_W - (UI_HOME_PAD * 2), lv_color_hex(0xFFFFFF));
    lv_obj_set_height(g_ui.home_focus_card, UI_HOME_FOCUS_H + 12);
    lv_obj_set_style_radius(g_ui.home_focus_card, 16, 0);
    lv_obj_set_style_pad_bottom(g_ui.home_focus_card, 16, 0);
    lv_obj_set_style_border_width(g_ui.home_focus_card, 1, 0);
    lv_obj_set_style_border_color(g_ui.home_focus_card, lv_color_hex(0xD7E3FC), 0);
    lv_obj_add_event_cb(g_ui.home_focus_card, home_open_focused_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *focus_top = lv_obj_create(g_ui.home_focus_card);
    lv_obj_set_size(focus_top, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_clear_flag(focus_top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(focus_top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus_top, 0, 0);
    lv_obj_set_style_pad_all(focus_top, 0, 0);
    lv_obj_set_layout(focus_top, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(focus_top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(focus_top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    g_ui.home_focus_icon = lv_img_create(focus_top);
    lv_img_set_src(g_ui.home_focus_icon, k_features[0].icon_path);

    g_ui.home_focus_value_chip = create_chip(focus_top, k_features[0].hero_value, lv_color_hex(0xE8F0FE), lv_color_hex(0x1A73E8));

    g_ui.home_focus_title = create_text_label(g_ui.home_focus_card, k_features[0].title, lv_color_hex(0x22303A), UI_W - 60, LV_TEXT_ALIGN_LEFT);
    g_ui.home_focus_subtitle = create_text_label(g_ui.home_focus_card, k_features[0].subtitle, lv_color_hex(0x1A73E8), UI_W - 60, LV_TEXT_ALIGN_LEFT);
    g_ui.home_focus_caption = create_text_label(g_ui.home_focus_card, k_features[0].hero_caption, lv_color_hex(0x607D8B), UI_W - 72, LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(g_ui.home_focus_caption, LV_LABEL_LONG_WRAP);

    g_ui.home_next_card = create_card(section, UI_W - (UI_HOME_PAD * 2), lv_color_hex(0xF7F9FB));
    lv_obj_set_height(g_ui.home_next_card, 52);
    lv_obj_set_style_bg_opa(g_ui.home_next_card, LV_OPA_80, 0);
    lv_obj_set_style_border_width(g_ui.home_next_card, 1, 0);
    lv_obj_set_style_border_color(g_ui.home_next_card, lv_color_hex(0xE3EAF0), 0);
    lv_obj_set_style_pad_top(g_ui.home_next_card, 8, 0);
    lv_obj_set_style_pad_bottom(g_ui.home_next_card, 8, 0);
    lv_obj_set_layout(g_ui.home_next_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(g_ui.home_next_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_ui.home_next_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(g_ui.home_next_card, 10, 0);
    lv_obj_add_event_cb(g_ui.home_next_card, home_next_event_cb, LV_EVENT_CLICKED, NULL);
    g_ui.home_next_icon = lv_img_create(g_ui.home_next_card);
    lv_img_set_src(g_ui.home_next_icon, k_features[1].icon_path);
    g_ui.home_next_hint = create_text_label(g_ui.home_next_card, "", lv_color_hex(0x90A4AE), 150, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *bottom_card = create_card(home_content, UI_W - (UI_HOME_PAD * 2), lv_color_hex(0xFFFFFF));
    lv_obj_set_style_border_width(bottom_card, 1, 0);
    lv_obj_set_style_border_color(bottom_card, lv_color_hex(0xE3EAF0), 0);
    lv_obj_set_style_pad_row(bottom_card, 4, 0);
    create_text_label(bottom_card,
                      "状态栏",
                      lv_color_hex(0x607D8B),
                      0,
                      LV_TEXT_ALIGN_LEFT);

    lv_obj_t *status_row = lv_obj_create(bottom_card);
    lv_obj_set_size(status_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_clear_flag(status_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(status_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_row, 0, 0);
    lv_obj_set_style_pad_all(status_row, 0, 0);
    lv_obj_set_style_pad_column(status_row, 6, 0);
    lv_obj_set_layout(status_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW_WRAP);
    create_chip(status_row, "亮度", lv_color_hex(0xE8F0FE), lv_color_hex(0x1A73E8));
    create_chip(status_row, "触摸", lv_color_hex(0xE0F2F1), lv_color_hex(0x00897B));
    create_chip(status_row, "锁定", lv_color_hex(0xF3E5F5), lv_color_hex(0x8E24AA));

    lv_obj_t *home_spacer = lv_obj_create(home_content);
    lv_obj_set_size(home_spacer, 1, 16);
    lv_obj_clear_flag(home_spacer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(home_spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(home_spacer, 0, 0);

    for (ui_page_t page = PAGE_CONTROL; page < PAGE_MAX; ++page) {
        g_ui.root_pages[page] = create_detail_page(page);
    }

    g_ui.snackbar = lv_obj_create(scr);
    lv_obj_set_size(g_ui.snackbar, UI_SNACK_W, UI_SNACK_MIN_H);
    lv_obj_align(g_ui.snackbar, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_clear_flag(g_ui.snackbar, LV_OBJ_FLAG_SCROLLABLE);
    style_surface(g_ui.snackbar, lv_color_hex(0x37474F), 8, LV_OPA_COVER);
    lv_obj_set_style_shadow_width(g_ui.snackbar, 0, 0);
    lv_obj_set_style_pad_left(g_ui.snackbar, 10, 0);
    lv_obj_set_style_pad_right(g_ui.snackbar, 10, 0);
    lv_obj_set_style_pad_top(g_ui.snackbar, 8, 0);
    lv_obj_set_style_pad_bottom(g_ui.snackbar, 8, 0);
    g_ui.snackbar_label = create_text_label(g_ui.snackbar, "", lv_color_white(), UI_SNACK_W - 20, LV_TEXT_ALIGN_CENTER);
    lv_obj_center(g_ui.snackbar_label);
    lv_obj_add_flag(g_ui.snackbar, LV_OBJ_FLAG_HIDDEN);

    refresh_dynamic_content();
    animate_staggered_children_delayed(g_ui.header_roots[PAGE_HOME], 10);
    animate_staggered_children_delayed(g_ui.content_roots[PAGE_HOME], 70);
}
