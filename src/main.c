#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "lvgl.h"
#include "ui_demo.h"

#if LV_USE_FS_STDIO || LV_USE_PNG
#include "src/extra/lv_extra.h"
#endif

#ifdef USE_SDL
#include "sdl/sdl.h"
#endif

#ifdef USE_ILI9341_SPI
#include "pi_ili9341.h"
#include "pi_xpt2046.h"
#endif

static volatile sig_atomic_t keep_running = 1;

enum {
    PI_DRAW_BUF_ROWS_DEFAULT = 240,
    PI_DRAW_BUF_ROWS_MIN = 20,
    PI_DRAW_BUF_ROWS_MAX = 240,
    LVGL_LOOP_SLEEP_US_DEFAULT = 3000,
    LVGL_LOOP_SLEEP_US_MIN = 1000,
    LVGL_LOOP_SLEEP_US_MAX = 20000,
};

static const char *env_or_default(const char *name, const char *fallback) {
    const char *value = getenv(name);
    return (value != NULL && value[0] != '\0') ? value : fallback;
}

static uint32_t env_u32_or_default(const char *name, uint32_t fallback) {
    const char *value = getenv(name);
    char *endptr = NULL;
    unsigned long parsed;

    if (value == NULL || value[0] == '\0') {
        return fallback;
    }

    parsed = strtoul(value, &endptr, 0);
    if (endptr == value || *endptr != '\0') {
        return fallback;
    }

    return (uint32_t)parsed;
}

static int32_t env_i32_or_default(const char *name, int32_t fallback) {
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

    return (int32_t)parsed;
}

static uint32_t clamp_draw_buf_rows(uint32_t rows) {
    if (rows < PI_DRAW_BUF_ROWS_MIN) {
        return PI_DRAW_BUF_ROWS_MIN;
    }
    if (rows > PI_DRAW_BUF_ROWS_MAX) {
        return PI_DRAW_BUF_ROWS_MAX;
    }
    return rows;
}

static uint32_t clamp_loop_sleep_us(uint32_t sleep_us) {
    if (sleep_us < LVGL_LOOP_SLEEP_US_MIN) {
        return LVGL_LOOP_SLEEP_US_MIN;
    }
    if (sleep_us > LVGL_LOOP_SLEEP_US_MAX) {
        return LVGL_LOOP_SLEEP_US_MAX;
    }
    return sleep_us;
}

static void on_signal(int signo) {
    (void)signo;
    keep_running = 0;
}

static void print_runtime_config(void) {
    printf("[lvgl-zero2w] font file: %s\n",
           env_or_default("LVGL_FONT_FILE", "auto-detect common CJK system fonts"));
    printf("[lvgl-zero2w] font size: %dpx\n", env_i32_or_default("LVGL_FONT_PX", 14));
#ifdef USE_ILI9341_SPI
    printf("[lvgl-zero2w] SPI panel: %s\n", env_or_default("ILI9341_SPI_DEV", "/dev/spidev0.0"));
    printf("[lvgl-zero2w] GPIO chip: %s\n", env_or_default("ILI9341_GPIOCHIP", "/dev/gpiochip0"));
    printf("[lvgl-zero2w] DC=GPIO%u RESET=GPIO%u\n",
           env_u32_or_default("ILI9341_DC_GPIO", 24),
           env_u32_or_default("ILI9341_RESET_GPIO", 25));
    printf("[lvgl-zero2w] size=240x320 rotation=%u speed=%uHz\n",
           env_u32_or_default("ILI9341_ROTATION", 0),
           env_u32_or_default("ILI9341_SPI_SPEED", 75000000));
    printf("[lvgl-zero2w] touch SPI: %s IRQ=GPIO%d speed=%uHz pressure_min=%u\n",
           env_or_default("XPT2046_SPI_DEV", "/dev/spidev0.1"),
           (int)env_i32_or_default("XPT2046_IRQ_GPIO", 23),
           env_u32_or_default("XPT2046_SPI_SPEED", 2000000),
           env_u32_or_default("XPT2046_PRESSURE_MIN", 40));
    printf("[lvgl-zero2w] draw buffer rows: %u\n",
           clamp_draw_buf_rows(env_u32_or_default("LVGL_DRAW_BUF_ROWS", PI_DRAW_BUF_ROWS_DEFAULT)));
    printf("[lvgl-zero2w] main loop sleep: %uus\n",
           clamp_loop_sleep_us(env_u32_or_default("LVGL_LOOP_SLEEP_US", LVGL_LOOP_SLEEP_US_DEFAULT)));
#endif
}

static int hal_init(void) {
#ifdef USE_SDL
    sdl_init();

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[800 * 100];
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 800 * 100);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = 800;
    disp_drv.ver_res = 480;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&indev_drv);
    return 0;
#endif

#ifdef USE_ILI9341_SPI
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[240 * PI_DRAW_BUF_ROWS_MAX];
    static pi_ili9341_config_t panel_config;
    static pi_xpt2046_config_t touch_config;
    static lv_disp_drv_t disp_drv;
    static lv_indev_drv_t indev_drv;

    const uint32_t rotation = env_u32_or_default("ILI9341_ROTATION", 0);
    const uint32_t draw_buf_rows = clamp_draw_buf_rows(env_u32_or_default("LVGL_DRAW_BUF_ROWS", PI_DRAW_BUF_ROWS_DEFAULT));
    if (!(rotation == 0 || rotation == 180)) {
        fprintf(stderr, "[lvgl-zero2w] warning: rotation %u is experimental on this panel; 0 and 180 are the known-good values.\n", rotation);
    }

    panel_config.spi_path = env_or_default("ILI9341_SPI_DEV", "/dev/spidev0.0");
    panel_config.gpiochip_path = env_or_default("ILI9341_GPIOCHIP", "/dev/gpiochip0");
    panel_config.dc_gpio = env_u32_or_default("ILI9341_DC_GPIO", 24);
    panel_config.reset_gpio = env_u32_or_default("ILI9341_RESET_GPIO", 25);
    panel_config.speed_hz = env_u32_or_default("ILI9341_SPI_SPEED", 75000000);
    panel_config.spi_mode = (uint8_t)env_u32_or_default("ILI9341_SPI_MODE", 0);
    panel_config.rotation = (uint8_t)rotation;
    panel_config.invert_colors = env_u32_or_default("ILI9341_INVERT", 0) != 0;
    panel_config.bgr = env_u32_or_default("ILI9341_BGR", 1) != 0;
    panel_config.width = 240;
    panel_config.height = 320;

    if (pi_ili9341_init(&panel_config) < 0) {
        return -1;
    }

    touch_config.spi_path = env_or_default("XPT2046_SPI_DEV", "/dev/spidev0.1");
    touch_config.gpiochip_path = panel_config.gpiochip_path;
    touch_config.irq_gpio = (int)env_i32_or_default("XPT2046_IRQ_GPIO", 23);
    touch_config.speed_hz = env_u32_or_default("XPT2046_SPI_SPEED", 2000000);
    touch_config.spi_mode = (uint8_t)env_u32_or_default("XPT2046_SPI_MODE", 0);
    touch_config.width = panel_config.width;
    touch_config.height = panel_config.height;
    touch_config.raw_x_min = (uint16_t)env_u32_or_default("XPT2046_RAW_X_MIN", 200);
    touch_config.raw_x_max = (uint16_t)env_u32_or_default("XPT2046_RAW_X_MAX", 3900);
    touch_config.raw_y_min = (uint16_t)env_u32_or_default("XPT2046_RAW_Y_MIN", 200);
    touch_config.raw_y_max = (uint16_t)env_u32_or_default("XPT2046_RAW_Y_MAX", 3900);
    touch_config.pressure_min = (uint16_t)env_u32_or_default("XPT2046_PRESSURE_MIN", 40);
    touch_config.rotation = (uint8_t)rotation;
    touch_config.swap_xy = env_u32_or_default("XPT2046_SWAP_XY", 1) != 0;
    touch_config.invert_x = env_u32_or_default("XPT2046_INVERT_X", 0) != 0;
    touch_config.invert_y = env_u32_or_default("XPT2046_INVERT_Y", 0) != 0;

    if (pi_xpt2046_init(&touch_config) < 0) {
        pi_ili9341_shutdown();
        return -1;
    }

    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 240 * draw_buf_rows);

    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = pi_ili9341_flush;
    disp_drv.hor_res = panel_config.width;
    disp_drv.ver_res = panel_config.height;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = pi_xpt2046_read;
    lv_indev_drv_register(&indev_drv);
    return 0;
#endif

    return -1;
}

static void tick_and_pump(struct timespec *last_tick) {
    struct timespec now;
    uint32_t elapsed_ms;

    clock_gettime(CLOCK_MONOTONIC, &now);
    elapsed_ms = (uint32_t)((now.tv_sec - last_tick->tv_sec) * 1000U +
                            (now.tv_nsec - last_tick->tv_nsec) / 1000000U);

    if (elapsed_ms > 0) {
        lv_tick_inc(elapsed_ms);
        *last_tick = now;
    }

    lv_timer_handler();
}

#ifdef USE_ILI9341_SPI
typedef struct {
    const char *name;
    lv_coord_t x;
    lv_coord_t y;
    uint16_t raw_x;
    uint16_t raw_y;
} calibration_point_t;

typedef struct {
    bool swap_xy;
    bool invert_x;
    bool invert_y;
    uint16_t raw_x_min;
    uint16_t raw_x_max;
    uint16_t raw_y_min;
    uint16_t raw_y_max;
} calibration_result_t;

static double abs_double(double value) {
    return value < 0.0 ? -value : value;
}

static double min_double(double a, double b) {
    return a < b ? a : b;
}

static double max_double(double a, double b) {
    return a > b ? a : b;
}

static int round_to_int(double value) {
    return (int)(value >= 0.0 ? value + 0.5 : value - 0.5);
}

static uint16_t clamp_raw_value(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 4095) {
        return 4095;
    }
    return (uint16_t)value;
}

static uint16_t median_u16(uint16_t *values, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1; j < count; ++j) {
            if (values[j] < values[i]) {
                const uint16_t tmp = values[i];
                values[i] = values[j];
                values[j] = tmp;
            }
        }
    }
    return values[count / 2U];
}

static bool wait_for_release(struct timespec *last_tick) {
    pi_xpt2046_raw_sample_t sample;
    unsigned int released_reads = 0;

    while (keep_running) {
        tick_and_pump(last_tick);
        if (!pi_xpt2046_read_raw_sample(&sample)) {
            if (++released_reads >= 4U) {
                return true;
            }
        } else {
            released_reads = 0;
        }
        usleep(20 * 1000);
    }

    return false;
}

static bool capture_stable_press(uint16_t *raw_x_out,
                                 uint16_t *raw_y_out,
                                 uint16_t *z1_out,
                                 uint16_t *z2_out,
                                 struct timespec *last_tick) {
    enum { STABLE_SAMPLES = 9, RANGE_LIMIT = 80 };
    uint16_t xs[STABLE_SAMPLES];
    uint16_t ys[STABLE_SAMPLES];
    uint16_t z1s[STABLE_SAMPLES];
    uint16_t z2s[STABLE_SAMPLES];
    size_t count = 0;

    while (keep_running) {
        pi_xpt2046_raw_sample_t sample;
        uint16_t min_x = 0xFFFFU, max_x = 0, min_y = 0xFFFFU, max_y = 0;

        tick_and_pump(last_tick);

        if (!pi_xpt2046_read_raw_sample(&sample)) {
            count = 0;
            usleep(20 * 1000);
            continue;
        }

        xs[count] = sample.x;
        ys[count] = sample.y;
        z1s[count] = sample.z1;
        z2s[count] = sample.z2;
        ++count;

        if (count < STABLE_SAMPLES) {
            usleep(20 * 1000);
            continue;
        }

        for (size_t i = 0; i < STABLE_SAMPLES; ++i) {
            if (xs[i] < min_x) {
                min_x = xs[i];
            }
            if (xs[i] > max_x) {
                max_x = xs[i];
            }
            if (ys[i] < min_y) {
                min_y = ys[i];
            }
            if (ys[i] > max_y) {
                max_y = ys[i];
            }
        }

        if ((max_x - min_x) <= RANGE_LIMIT && (max_y - min_y) <= RANGE_LIMIT) {
            *raw_x_out = median_u16(xs, STABLE_SAMPLES);
            *raw_y_out = median_u16(ys, STABLE_SAMPLES);
            *z1_out = median_u16(z1s, STABLE_SAMPLES);
            *z2_out = median_u16(z2s, STABLE_SAMPLES);
            return true;
        }

        memmove(xs, xs + 1, sizeof(xs[0]) * (STABLE_SAMPLES - 1U));
        memmove(ys, ys + 1, sizeof(ys[0]) * (STABLE_SAMPLES - 1U));
        memmove(z1s, z1s + 1, sizeof(z1s[0]) * (STABLE_SAMPLES - 1U));
        memmove(z2s, z2s + 1, sizeof(z2s[0]) * (STABLE_SAMPLES - 1U));
        count = STABLE_SAMPLES - 1U;
        usleep(20 * 1000);
    }

    return false;
}

static void draw_target(lv_obj_t *marker, lv_coord_t x, lv_coord_t y) {
    lv_obj_set_pos(marker, x - 14, y - 14);
}

static void update_calibration_prompt(lv_obj_t *label, const char *corner_name) {
    lv_label_set_text_fmt(label,
                          "Touch and hold\n%s\n\nWait for the sample to lock\n(press Ctrl+C to quit)",
                          corner_name);
    lv_obj_center(label);
}

static void print_calibration_result(const calibration_result_t *result) {
    printf("\n[lvgl-zero2w] Suggested touch calibration:\n");
    printf("XPT2046_RAW_X_MIN=%u\n", result->raw_x_min);
    printf("XPT2046_RAW_X_MAX=%u\n", result->raw_x_max);
    printf("XPT2046_RAW_Y_MIN=%u\n", result->raw_y_min);
    printf("XPT2046_RAW_Y_MAX=%u\n", result->raw_y_max);
    printf("XPT2046_SWAP_XY=%u\n", result->swap_xy ? 1U : 0U);
    printf("XPT2046_INVERT_X=%u\n", result->invert_x ? 1U : 0U);
    printf("XPT2046_INVERT_Y=%u\n", result->invert_y ? 1U : 0U);
    printf("\n[lvgl-zero2w] Example:\n");
    printf("XPT2046_RAW_X_MIN=%u XPT2046_RAW_X_MAX=%u \\\n", result->raw_x_min, result->raw_x_max);
    printf("XPT2046_RAW_Y_MIN=%u XPT2046_RAW_Y_MAX=%u \\\n", result->raw_y_min, result->raw_y_max);
    printf("XPT2046_SWAP_XY=%u XPT2046_INVERT_X=%u XPT2046_INVERT_Y=%u ./scripts/run-pi.sh\n\n",
           result->swap_xy ? 1U : 0U,
           result->invert_x ? 1U : 0U,
           result->invert_y ? 1U : 0U);
}

static calibration_result_t solve_calibration(const calibration_point_t *points,
                                              uint16_t width,
                                              uint16_t height) {
    calibration_result_t result;
    const double dx_lr = (abs_double((double)points[1].raw_x - (double)points[0].raw_x) +
                          abs_double((double)points[3].raw_x - (double)points[2].raw_x)) /
                         2.0;
    const double dx_tb = (abs_double((double)points[2].raw_x - (double)points[0].raw_x) +
                          abs_double((double)points[3].raw_x - (double)points[1].raw_x)) /
                         2.0;
    const double dy_lr = (abs_double((double)points[1].raw_y - (double)points[0].raw_y) +
                          abs_double((double)points[3].raw_y - (double)points[2].raw_y)) /
                         2.0;
    const double dy_tb = (abs_double((double)points[2].raw_y - (double)points[0].raw_y) +
                          abs_double((double)points[3].raw_y - (double)points[1].raw_y)) /
                         2.0;
    const bool swap_xy = (dx_tb + dy_lr) > (dx_lr + dy_tb);
    const double raw_left = swap_xy
                                ? (((double)points[0].raw_y + (double)points[2].raw_y) / 2.0)
                                : (((double)points[0].raw_x + (double)points[2].raw_x) / 2.0);
    const double raw_right = swap_xy
                                 ? (((double)points[1].raw_y + (double)points[3].raw_y) / 2.0)
                                 : (((double)points[1].raw_x + (double)points[3].raw_x) / 2.0);
    const double raw_top = swap_xy
                               ? (((double)points[0].raw_x + (double)points[1].raw_x) / 2.0)
                               : (((double)points[0].raw_y + (double)points[1].raw_y) / 2.0);
    const double raw_bottom = swap_xy
                                  ? (((double)points[2].raw_x + (double)points[3].raw_x) / 2.0)
                                  : (((double)points[2].raw_y + (double)points[3].raw_y) / 2.0);
    const double slope_x = (raw_right - raw_left) / (double)(points[1].x - points[0].x);
    const double slope_y = (raw_bottom - raw_top) / (double)(points[2].y - points[0].y);
    const double raw_x_at_left_edge = raw_left - slope_x * (double)points[0].x;
    const double raw_x_at_right_edge = raw_left + slope_x * (double)((int)width - 1 - points[0].x);
    const double raw_y_at_top_edge = raw_top - slope_y * (double)points[0].y;
    const double raw_y_at_bottom_edge = raw_top + slope_y * (double)((int)height - 1 - points[0].y);

    memset(&result, 0, sizeof(result));
    result.swap_xy = swap_xy;
    result.invert_x = raw_x_at_left_edge > raw_x_at_right_edge;
    result.invert_y = raw_y_at_top_edge > raw_y_at_bottom_edge;
    result.raw_x_min = clamp_raw_value(round_to_int(min_double(raw_x_at_left_edge, raw_x_at_right_edge)));
    result.raw_x_max = clamp_raw_value(round_to_int(max_double(raw_x_at_left_edge, raw_x_at_right_edge)));
    result.raw_y_min = clamp_raw_value(round_to_int(min_double(raw_y_at_top_edge, raw_y_at_bottom_edge)));
    result.raw_y_max = clamp_raw_value(round_to_int(max_double(raw_y_at_top_edge, raw_y_at_bottom_edge)));
    return result;
}

static int run_touch_raw_mode(void) {
    struct timespec last_tick;
    bool last_pressed = false;

    clock_gettime(CLOCK_MONOTONIC, &last_tick);
    printf("[lvgl-zero2w] Raw touch mode. Press Ctrl+C to stop.\n");

    while (keep_running) {
        pi_xpt2046_raw_sample_t sample;
        const bool pressed = pi_xpt2046_read_raw_sample(&sample);

        tick_and_pump(&last_tick);
        if (pressed) {
            printf("raw_x=%4u raw_y=%4u z1=%4u z2=%4u\n", sample.x, sample.y, sample.z1, sample.z2);
            fflush(stdout);
        } else if (last_pressed) {
            printf("released\n");
            fflush(stdout);
        }

        last_pressed = pressed;
        usleep(40 * 1000);
    }

    return 0;
}

static int run_touch_calibration_mode(void) {
    const pi_xpt2046_config_t *cfg = pi_xpt2046_get_active_config();
    struct timespec last_tick;
    const lv_coord_t margin = 28;
    calibration_point_t points[4] = {
        {.name = "top-left", .x = margin, .y = margin},
        {.name = "top-right", .x = (lv_coord_t)(cfg->width - 1U - margin), .y = margin},
        {.name = "bottom-left", .x = margin, .y = (lv_coord_t)(cfg->height - 1U - margin)},
        {.name = "bottom-right", .x = (lv_coord_t)(cfg->width - 1U - margin), .y = (lv_coord_t)(cfg->height - 1U - margin)},
    };
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_t *label = lv_label_create(screen);
    lv_obj_t *marker = lv_obj_create(screen);
    lv_style_t marker_style;
    int rc = 0;

    lv_style_init(&marker_style);
    lv_style_set_radius(&marker_style, 0);
    lv_style_set_border_width(&marker_style, 2);
    lv_style_set_border_color(&marker_style, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_opa(&marker_style, LV_OPA_TRANSP);

    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);

    lv_obj_add_style(marker, &marker_style, 0);
    lv_obj_clear_flag(marker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(marker, 28, 28);

    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, cfg->width - 20);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_scr_load(screen);

    clock_gettime(CLOCK_MONOTONIC, &last_tick);
    printf("[lvgl-zero2w] Touch calibration mode. Tap and hold each target until it locks.\n");

    for (size_t i = 0; i < 4 && keep_running; ++i) {
        uint16_t raw_x = 0, raw_y = 0, z1 = 0, z2 = 0;

        update_calibration_prompt(label, points[i].name);
        draw_target(marker, points[i].x, points[i].y);
        tick_and_pump(&last_tick);

        printf("[lvgl-zero2w] Step %zu/4: %s\n", i + 1U, points[i].name);
        fflush(stdout);

        if (!wait_for_release(&last_tick)) {
            rc = 1;
            break;
        }
        if (!capture_stable_press(&raw_x, &raw_y, &z1, &z2, &last_tick)) {
            rc = 1;
            break;
        }

        points[i].raw_x = raw_x;
        points[i].raw_y = raw_y;

        printf("[lvgl-zero2w] captured %-12s raw_x=%4u raw_y=%4u z1=%4u z2=%4u\n",
               points[i].name,
               raw_x,
               raw_y,
               z1,
               z2);
        fflush(stdout);

        if (!wait_for_release(&last_tick)) {
            rc = 1;
            break;
        }
    }

    if (rc == 0 && keep_running) {
        calibration_result_t result = solve_calibration(points, cfg->width, cfg->height);
        lv_label_set_text(label, "Calibration complete\nSee stdout for suggested env vars");
        lv_obj_center(label);
        lv_obj_add_flag(marker, LV_OBJ_FLAG_HIDDEN);
        tick_and_pump(&last_tick);
        print_calibration_result(&result);
        usleep(500 * 1000);
    }

    lv_style_reset(&marker_style);
    return rc;
}
#endif

typedef enum {
    MODE_UI = 0,
    MODE_TOUCH_RAW,
    MODE_TOUCH_CALIBRATE,
} app_mode_t;

static app_mode_t parse_mode(int argc, char **argv) {
    const char *env_mode = env_or_default("LVGL_PI_MODE", "");

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--touch-raw") == 0) {
            return MODE_TOUCH_RAW;
        }
        if (strcmp(argv[i], "--touch-calibrate") == 0) {
            return MODE_TOUCH_CALIBRATE;
        }
    }

    if (strcmp(env_mode, "touch-raw") == 0) {
        return MODE_TOUCH_RAW;
    }
    if (strcmp(env_mode, "touch-calibrate") == 0 || strcmp(env_mode, "calibrate") == 0) {
        return MODE_TOUCH_CALIBRATE;
    }

    return MODE_UI;
}

int main(int argc, char **argv) {
    struct timespec last_tick;
    const app_mode_t mode = parse_mode(argc, argv);

    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        printf("lvgl-zero2w\n\n");
        printf("Usage:\n");
        printf("  lvgl-pi-spi [--touch-raw | --touch-calibrate]\n\n");
        printf("Environment variables:\n");
        printf("  LVGL_PI_MODE         Optional mode: touch-raw or touch-calibrate\n");
        printf("  LVGL_FONT_FILE       Optional TTF/TTC/OTF path for runtime Chinese font auto-loading\n");
        printf("  LVGL_FONT_PX         Runtime font size in pixels (default: 14)\n");
        printf("  LVGL_DRAW_BUF_ROWS   LVGL draw buffer height in rows for SPI panel (default: 240, range: 20-240)\n");
        printf("  LVGL_LOOP_SLEEP_US   Main loop sleep in microseconds; lower = faster touch/UI polling (default: 3000, range: 1000-20000)\n");
        printf("  ILI9341_SPI_DEV      SPI device path (default: /dev/spidev0.0)\n");
        printf("  ILI9341_GPIOCHIP     GPIO chip path (default: /dev/gpiochip0)\n");
        printf("  ILI9341_DC_GPIO      DC GPIO offset (default: 24)\n");
        printf("  ILI9341_RESET_GPIO   RESET GPIO offset (default: 25)\n");
        printf("  ILI9341_SPI_SPEED    SPI speed in Hz (default: 75000000)\n");
        printf("  ILI9341_SPI_MODE     SPI mode (default: 0)\n");
        printf("  ILI9341_ROTATION     Panel rotation: 0 or 180 recommended (default: 0)\n");
        printf("  ILI9341_INVERT       1 to enable panel color inversion (default: 0)\n");
        printf("  ILI9341_BGR          1 to use BGR MADCTL bit (default: 1)\n");
        printf("  XPT2046_SPI_DEV      Touch SPI device path (default: /dev/spidev0.1)\n");
        printf("  XPT2046_IRQ_GPIO     Touch IRQ GPIO offset, active-low; use -1 to disable (default: 23)\n");
        printf("  XPT2046_SPI_SPEED    Touch SPI speed in Hz (default: 2000000)\n");
        printf("  XPT2046_SPI_MODE     Touch SPI mode (default: 0)\n");
        printf("  XPT2046_RAW_X_MIN    Raw minimum for X calibration (default: 200)\n");
        printf("  XPT2046_RAW_X_MAX    Raw maximum for X calibration (default: 3900)\n");
        printf("  XPT2046_RAW_Y_MIN    Raw minimum for Y calibration (default: 200)\n");
        printf("  XPT2046_RAW_Y_MAX    Raw maximum for Y calibration (default: 3900)\n");
        printf("  XPT2046_SWAP_XY      1 to swap raw axes before mapping (default: 1)\n");
        printf("  XPT2046_INVERT_X     1 to flip mapped X (default: 0)\n");
        printf("  XPT2046_INVERT_Y     1 to flip mapped Y (default: 0)\n");
        printf("  XPT2046_PRESSURE_MIN Minimum Z1 value treated as touch (default: 40)\n");
        return 0;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    print_runtime_config();
    lv_init();
#if LV_USE_FS_STDIO || LV_USE_PNG
    lv_extra_init();
#endif

    if (hal_init() < 0) {
        fprintf(stderr, "[lvgl-zero2w] display initialization failed\n");
        return 1;
    }

#ifdef USE_ILI9341_SPI
    if (mode == MODE_TOUCH_RAW) {
        const int rc = run_touch_raw_mode();
        pi_xpt2046_shutdown();
        pi_ili9341_shutdown();
        return rc;
    }
    if (mode == MODE_TOUCH_CALIBRATE) {
        const int rc = run_touch_calibration_mode();
        pi_xpt2046_shutdown();
        pi_ili9341_shutdown();
        return rc;
    }
#endif

    ui_demo_create();

    clock_gettime(CLOCK_MONOTONIC, &last_tick);

    const useconds_t loop_sleep_us = (useconds_t)clamp_loop_sleep_us(
        env_u32_or_default("LVGL_LOOP_SLEEP_US", LVGL_LOOP_SLEEP_US_DEFAULT));

    while (keep_running) {
        tick_and_pump(&last_tick);
        usleep(loop_sleep_us);
    }

#ifdef USE_ILI9341_SPI
    pi_xpt2046_shutdown();
    pi_ili9341_shutdown();
#endif

    return 0;
}
