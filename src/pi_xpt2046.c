#include "pi_xpt2046.h"

#include <errno.h>
#include <fcntl.h>
#include <gpiod.h>
#include <linux/spi/spidev.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define XPT2046_CMD_X 0xD0U
#define XPT2046_CMD_Y 0x90U
#define XPT2046_CMD_Z1 0xB0U
#define XPT2046_CMD_Z2 0xC0U
#define XPT2046_SAMPLE_COUNT 5U

typedef struct {
    int spi_fd;
    struct gpiod_chip *chip;
    struct gpiod_line_request *irq_request;
    pi_xpt2046_config_t config;
    lv_point_t last_point;
    bool warned_irq_read;
} pi_xpt2046_state_t;

static pi_xpt2046_state_t g_state = {
    .spi_fd = -1,
};

static uint16_t xpt2046_read_channel(uint8_t cmd) {
    uint8_t tx[3] = {cmd, 0, 0};
    uint8_t rx[3] = {0, 0, 0};
    struct spi_ioc_transfer tr = {
        .tx_buf = (uintptr_t)tx,
        .rx_buf = (uintptr_t)rx,
        .len = sizeof(tx),
        .speed_hz = g_state.config.speed_hz,
        .bits_per_word = 8,
    };

    if (ioctl(g_state.spi_fd, SPI_IOC_MESSAGE(1), &tr) < 1) {
        return 0;
    }

    return (uint16_t)((((uint16_t)rx[1] << 8U) | (uint16_t)rx[2]) >> 3U) & 0x0FFFU;
}

static int compare_u16(const void *a, const void *b) {
    const uint16_t va = *(const uint16_t *)a;
    const uint16_t vb = *(const uint16_t *)b;
    return (va > vb) - (va < vb);
}

static uint16_t median_sample(uint8_t cmd) {
    uint16_t samples[XPT2046_SAMPLE_COUNT];

    for (size_t i = 0; i < XPT2046_SAMPLE_COUNT; ++i) {
        samples[i] = xpt2046_read_channel(cmd);
    }

    qsort(samples, XPT2046_SAMPLE_COUNT, sizeof(samples[0]), compare_u16);
    return samples[XPT2046_SAMPLE_COUNT / 2U];
}

static bool irq_pressed(void) {
    enum gpiod_line_value value;

    if (g_state.irq_request == NULL) {
        return true;
    }

    value = gpiod_line_request_get_value(g_state.irq_request, (unsigned int)g_state.config.irq_gpio);
    if (value == GPIOD_LINE_VALUE_ERROR) {
        if (!g_state.warned_irq_read) {
            fprintf(stderr, "[lvgl-zero2w] warning: failed to read XPT2046 IRQ GPIO%d: %s\n",
                    g_state.config.irq_gpio,
                    strerror(errno));
            g_state.warned_irq_read = true;
        }
        return true;
    }

    return value == GPIOD_LINE_VALUE_ACTIVE;
}

static bool pressure_pressed(uint16_t *z1_out, uint16_t *z2_out) {
    const uint16_t z1 = median_sample(XPT2046_CMD_Z1);
    const uint16_t z2 = median_sample(XPT2046_CMD_Z2);

    if (z1_out != NULL) {
        *z1_out = z1;
    }
    if (z2_out != NULL) {
        *z2_out = z2;
    }

    return z1 >= g_state.config.pressure_min && z2 > z1;
}

static lv_coord_t map_raw_axis(uint16_t value, uint16_t raw_min, uint16_t raw_max, uint16_t out_max) {
    if (out_max == 0) {
        return 0;
    }

    if (raw_max <= raw_min) {
        return (lv_coord_t)(value < out_max ? value : (out_max - 1U));
    }

    if (value <= raw_min) {
        return 0;
    }
    if (value >= raw_max) {
        return (lv_coord_t)(out_max - 1U);
    }

    return (lv_coord_t)(((uint32_t)(value - raw_min) * (uint32_t)(out_max - 1U)) /
                        (uint32_t)(raw_max - raw_min));
}

static void apply_rotation(lv_coord_t *x, lv_coord_t *y) {
    lv_coord_t px = *x;
    lv_coord_t py = *y;
    const lv_coord_t max_x = (lv_coord_t)(g_state.config.width - 1U);
    const lv_coord_t max_y = (lv_coord_t)(g_state.config.height - 1U);

    switch (g_state.config.rotation) {
    case 90:
        *x = max_x - py;
        *y = px;
        break;
    case 180:
        *x = max_x - px;
        *y = max_y - py;
        break;
    case 270:
        *x = py;
        *y = max_y - px;
        break;
    case 0:
    default:
        break;
    }
}

bool pi_xpt2046_read_raw_sample(pi_xpt2046_raw_sample_t *sample) {
    uint16_t z1 = 0;
    uint16_t z2 = 0;

    if (sample == NULL) {
        return false;
    }

    memset(sample, 0, sizeof(*sample));

    if (!irq_pressed()) {
        return false;
    }

    if (!pressure_pressed(&z1, &z2)) {
        sample->z1 = z1;
        sample->z2 = z2;
        return false;
    }

    sample->x = median_sample(XPT2046_CMD_X);
    sample->y = median_sample(XPT2046_CMD_Y);
    sample->z1 = z1;
    sample->z2 = z2;
    sample->pressed = true;
    return true;
}

static bool read_point(lv_point_t *point) {
    pi_xpt2046_raw_sample_t sample;
    uint16_t raw_x;
    uint16_t raw_y;
    lv_coord_t x;
    lv_coord_t y;

    if (!pi_xpt2046_read_raw_sample(&sample)) {
        return false;
    }

    raw_x = sample.x;
    raw_y = sample.y;

    if (g_state.config.swap_xy) {
        const uint16_t tmp = raw_x;
        raw_x = raw_y;
        raw_y = tmp;
    }

    x = map_raw_axis(raw_x, g_state.config.raw_x_min, g_state.config.raw_x_max, g_state.config.width);
    y = map_raw_axis(raw_y, g_state.config.raw_y_min, g_state.config.raw_y_max, g_state.config.height);

    if (g_state.config.invert_x) {
        x = (lv_coord_t)(g_state.config.width - 1U) - x;
    }
    if (g_state.config.invert_y) {
        y = (lv_coord_t)(g_state.config.height - 1U) - y;
    }

    apply_rotation(&x, &y);

    if (x < 0) {
        x = 0;
    } else if ((uint16_t)x >= g_state.config.width) {
        x = (lv_coord_t)(g_state.config.width - 1U);
    }

    if (y < 0) {
        y = 0;
    } else if ((uint16_t)y >= g_state.config.height) {
        y = (lv_coord_t)(g_state.config.height - 1U);
    }

    point->x = x;
    point->y = y;
    return true;
}

static int init_irq_gpio(const pi_xpt2046_config_t *config) {
    struct gpiod_line_settings *settings = NULL;
    struct gpiod_line_config *line_cfg = NULL;
    struct gpiod_request_config *req_cfg = NULL;
    unsigned int offset;
    int rc = -1;

    if (config->irq_gpio < 0) {
        return 0;
    }

    g_state.chip = gpiod_chip_open(config->gpiochip_path);
    if (g_state.chip == NULL) {
        fprintf(stderr, "[lvgl-zero2w] warning: failed to open %s for XPT2046 IRQ: %s\n",
                config->gpiochip_path,
                strerror(errno));
        return -1;
    }

    settings = gpiod_line_settings_new();
    line_cfg = gpiod_line_config_new();
    req_cfg = gpiod_request_config_new();
    if (settings == NULL || line_cfg == NULL || req_cfg == NULL) {
        fprintf(stderr, "[lvgl-zero2w] warning: failed to allocate libgpiod objects for XPT2046 IRQ\n");
        goto out;
    }

    offset = (unsigned int)config->irq_gpio;
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_active_low(settings, true);

    if (gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings) < 0) {
        fprintf(stderr, "[lvgl-zero2w] warning: failed to configure XPT2046 IRQ GPIO%d: %s\n",
                config->irq_gpio,
                strerror(errno));
        goto out;
    }

    gpiod_request_config_set_consumer(req_cfg, "lvgl-zero2w-xpt2046");
    g_state.irq_request = gpiod_chip_request_lines(g_state.chip, req_cfg, line_cfg);
    if (g_state.irq_request == NULL) {
        fprintf(stderr, "[lvgl-zero2w] warning: failed to request XPT2046 IRQ GPIO%d: %s\n",
                config->irq_gpio,
                strerror(errno));
        goto out;
    }

    rc = 0;

out:
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    return rc;
}

int pi_xpt2046_init(const pi_xpt2046_config_t *config) {
    uint8_t mode = config->spi_mode;
    uint8_t bits = 8;

    memset(&g_state, 0, sizeof(g_state));
    g_state.spi_fd = -1;
    g_state.config = *config;

    g_state.spi_fd = open(config->spi_path, O_RDWR);
    if (g_state.spi_fd < 0) {
        fprintf(stderr, "[lvgl-zero2w] failed to open touch SPI device %s: %s\n",
                config->spi_path,
                strerror(errno));
        pi_xpt2046_shutdown();
        return -1;
    }

    if (ioctl(g_state.spi_fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(g_state.spi_fd, SPI_IOC_RD_MODE, &mode) < 0 ||
        ioctl(g_state.spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(g_state.spi_fd, SPI_IOC_RD_BITS_PER_WORD, &bits) < 0 ||
        ioctl(g_state.spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &g_state.config.speed_hz) < 0 ||
        ioctl(g_state.spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &g_state.config.speed_hz) < 0) {
        fprintf(stderr, "[lvgl-zero2w] failed to configure touch SPI device %s: %s\n",
                config->spi_path,
                strerror(errno));
        pi_xpt2046_shutdown();
        return -1;
    }

    if (init_irq_gpio(config) < 0) {
        fprintf(stderr, "[lvgl-zero2w] continuing without usable XPT2046 IRQ, falling back to pressure polling\n");
        if (g_state.irq_request != NULL) {
            gpiod_line_request_release(g_state.irq_request);
            g_state.irq_request = NULL;
        }
        if (g_state.chip != NULL) {
            gpiod_chip_close(g_state.chip);
            g_state.chip = NULL;
        }
    }

    return 0;
}

void pi_xpt2046_shutdown(void) {
    if (g_state.spi_fd >= 0) {
        close(g_state.spi_fd);
        g_state.spi_fd = -1;
    }

    if (g_state.irq_request != NULL) {
        gpiod_line_request_release(g_state.irq_request);
        g_state.irq_request = NULL;
    }

    if (g_state.chip != NULL) {
        gpiod_chip_close(g_state.chip);
        g_state.chip = NULL;
    }
}

void pi_xpt2046_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    lv_point_t point = g_state.last_point;
    const bool pressed = read_point(&point);

    (void)drv;

    if (pressed) {
        g_state.last_point = point;
        data->state = LV_INDEV_STATE_PRESSED;
        data->point = point;
        return;
    }

    data->state = LV_INDEV_STATE_RELEASED;
    data->point = g_state.last_point;
}

const pi_xpt2046_config_t *pi_xpt2046_get_active_config(void) {
    return &g_state.config;
}
