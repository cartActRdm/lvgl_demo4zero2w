#include "pi_ili9341.h"

#include <errno.h>
#include <fcntl.h>
#include <gpiod.h>
#include <linux/spi/spidev.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define ILI9341_SWRESET 0x01
#define ILI9341_SLPOUT 0x11
#define ILI9341_DISPOFF 0x28
#define ILI9341_DISPON 0x29
#define ILI9341_CASET 0x2A
#define ILI9341_PASET 0x2B
#define ILI9341_RAMWR 0x2C
#define ILI9341_MADCTL 0x36
#define ILI9341_PIXFMT 0x3A
#define ILI9341_FRMCTR1 0xB1
#define ILI9341_DFUNCTR 0xB6
#define ILI9341_PWCTR1 0xC0
#define ILI9341_PWCTR2 0xC1
#define ILI9341_VMCTR1 0xC5
#define ILI9341_VMCTR2 0xC7
#define ILI9341_GMCTRP1 0xE0
#define ILI9341_GMCTRN1 0xE1

#define ILI9341_COLOR_BYTES 2U
#define ILI9341_CHUNK_PIXELS 2048U

typedef struct {
    int spi_fd;
    struct gpiod_chip *chip;
    struct gpiod_line_request *line_request;
    uint8_t *tx_buf;
    size_t tx_buf_size;
    enum gpiod_line_value dc_level;
    pi_ili9341_config_t config;
} pi_ili9341_state_t;

static pi_ili9341_state_t g_state = {
    .spi_fd = -1,
};

static int gpio_set_value(unsigned int offset, enum gpiod_line_value value);

static size_t flush_chunk_pixels(void) {
    return g_state.tx_buf_size / ILI9341_COLOR_BYTES;
}

static int set_dc_level(enum gpiod_line_value value) {
    if (g_state.dc_level == value) {
        return 0;
    }

    if (gpio_set_value(g_state.config.dc_gpio, value) < 0) {
        return -1;
    }

    g_state.dc_level = value;
    return 0;
}

static int gpio_set_value(unsigned int offset, enum gpiod_line_value value) {
    if (g_state.line_request == NULL) {
        errno = ENODEV;
        return -1;
    }

    return gpiod_line_request_set_value(g_state.line_request, offset, value);
}

static int spi_write_bytes(const uint8_t *data, size_t len) {
    while (len > 0) {
        struct spi_ioc_transfer tr = {
            .tx_buf = (uintptr_t)data,
            .len = len > UINT32_MAX ? UINT32_MAX : (uint32_t)len,
            .speed_hz = g_state.config.speed_hz,
            .bits_per_word = 8,
        };

        if (ioctl(g_state.spi_fd, SPI_IOC_MESSAGE(1), &tr) < 1) {
            return -1;
        }

        data += tr.len;
        len -= tr.len;
    }

    return 0;
}

static int write_command(uint8_t cmd) {
    if (set_dc_level(GPIOD_LINE_VALUE_INACTIVE) < 0) {
        return -1;
    }
    return spi_write_bytes(&cmd, 1);
}

static int write_data(const uint8_t *data, size_t len) {
    if (set_dc_level(GPIOD_LINE_VALUE_ACTIVE) < 0) {
        return -1;
    }
    return spi_write_bytes(data, len);
}

static int write_command_data(uint8_t cmd, const uint8_t *data, size_t len) {
    if (write_command(cmd) < 0) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    return write_data(data, len);
}

static uint8_t rotation_to_madctl(unsigned int rotation, bool bgr) {
    uint8_t madctl;

    switch (rotation) {
    case 0:
        madctl = 0x48;
        break;
    case 180:
        madctl = 0x88;
        break;
    case 90:
        madctl = 0x28;
        break;
    case 270:
    default:
        madctl = 0xE8;
        break;
    }

    if (!bgr) {
        madctl &= (uint8_t)~0x08U;
    }

    return madctl;
}

static int hardware_reset(void) {
    if (gpio_set_value(g_state.config.reset_gpio, GPIOD_LINE_VALUE_ACTIVE) < 0) {
        return -1;
    }
    usleep(50 * 1000);

    if (gpio_set_value(g_state.config.reset_gpio, GPIOD_LINE_VALUE_INACTIVE) < 0) {
        return -1;
    }
    usleep(50 * 1000);

    if (gpio_set_value(g_state.config.reset_gpio, GPIOD_LINE_VALUE_ACTIVE) < 0) {
        return -1;
    }
    usleep(150 * 1000);
    return 0;
}

static int ili9341_init_panel(void) {
    static const uint8_t pwctr1[] = {0x23};
    static const uint8_t pwctr2[] = {0x10};
    static const uint8_t vmctr1[] = {0x3E, 0x28};
    static const uint8_t vmctr2[] = {0x86};
    static const uint8_t pixfmt[] = {0x55};
    static const uint8_t frmctr1[] = {0x00, 0x18};
    static const uint8_t dfunctr[] = {0x08, 0x82, 0x27};
    static const uint8_t disabled_3gamma[] = {0x00};
    static const uint8_t gamma_curve[] = {0x01};
    static const uint8_t gamma_pos[] = {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00};
    static const uint8_t gamma_neg[] = {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F};

    const uint8_t madctl[] = {rotation_to_madctl(g_state.config.rotation, g_state.config.bgr)};

    if (hardware_reset() < 0) {
        return -1;
    }

    if (write_command(ILI9341_SWRESET) < 0) {
        return -1;
    }
    usleep(150 * 1000);

    if (write_command(ILI9341_DISPOFF) < 0 ||
        write_command_data(ILI9341_PWCTR1, pwctr1, sizeof(pwctr1)) < 0 ||
        write_command_data(ILI9341_PWCTR2, pwctr2, sizeof(pwctr2)) < 0 ||
        write_command_data(ILI9341_VMCTR1, vmctr1, sizeof(vmctr1)) < 0 ||
        write_command_data(ILI9341_VMCTR2, vmctr2, sizeof(vmctr2)) < 0 ||
        write_command_data(ILI9341_MADCTL, madctl, sizeof(madctl)) < 0 ||
        write_command_data(ILI9341_PIXFMT, pixfmt, sizeof(pixfmt)) < 0 ||
        write_command_data(ILI9341_FRMCTR1, frmctr1, sizeof(frmctr1)) < 0 ||
        write_command_data(ILI9341_DFUNCTR, dfunctr, sizeof(dfunctr)) < 0 ||
        write_command_data(0xF2, disabled_3gamma, sizeof(disabled_3gamma)) < 0 ||
        write_command_data(0x26, gamma_curve, sizeof(gamma_curve)) < 0 ||
        write_command_data(ILI9341_GMCTRP1, gamma_pos, sizeof(gamma_pos)) < 0 ||
        write_command_data(ILI9341_GMCTRN1, gamma_neg, sizeof(gamma_neg)) < 0) {
        return -1;
    }

    if (write_command(g_state.config.invert_colors ? 0x21 : 0x20) < 0) {
        return -1;
    }

    if (write_command(ILI9341_SLPOUT) < 0) {
        return -1;
    }
    usleep(120 * 1000);

    if (write_command(ILI9341_DISPON) < 0) {
        return -1;
    }
    usleep(120 * 1000);

    return 0;
}

static int set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    const uint8_t caset[] = {
        (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF),
        (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF),
    };
    const uint8_t paset[] = {
        (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
        (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF),
    };

    if (write_command_data(ILI9341_CASET, caset, sizeof(caset)) < 0 ||
        write_command_data(ILI9341_PASET, paset, sizeof(paset)) < 0 ||
        write_command(ILI9341_RAMWR) < 0) {
        return -1;
    }

    return set_dc_level(GPIOD_LINE_VALUE_ACTIVE);
}

static int init_gpio(const pi_ili9341_config_t *config) {
    struct gpiod_line_settings *settings = NULL;
    struct gpiod_line_config *line_cfg = NULL;
    struct gpiod_request_config *req_cfg = NULL;
    unsigned int offsets[2] = {config->dc_gpio, config->reset_gpio};
    enum gpiod_line_value output_values[2] = {
        GPIOD_LINE_VALUE_INACTIVE,
        GPIOD_LINE_VALUE_ACTIVE,
    };
    int rc = -1;

    g_state.chip = gpiod_chip_open(config->gpiochip_path);
    if (g_state.chip == NULL) {
        fprintf(stderr, "[lvgl-zero2w] failed to open %s: %s\n", config->gpiochip_path, strerror(errno));
        goto out;
    }

    settings = gpiod_line_settings_new();
    line_cfg = gpiod_line_config_new();
    req_cfg = gpiod_request_config_new();
    if (settings == NULL || line_cfg == NULL || req_cfg == NULL) {
        fprintf(stderr, "[lvgl-zero2w] failed to allocate libgpiod objects\n");
        goto out;
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    if (gpiod_line_config_add_line_settings(line_cfg, offsets, 2, settings) < 0) {
        fprintf(stderr, "[lvgl-zero2w] failed to configure GPIO lines: %s\n", strerror(errno));
        goto out;
    }

    gpiod_request_config_set_consumer(req_cfg, "lvgl-zero2w");

    g_state.line_request = gpiod_chip_request_lines(g_state.chip, req_cfg, line_cfg);
    if (g_state.line_request == NULL) {
        fprintf(stderr, "[lvgl-zero2w] failed to request GPIO lines: %s\n", strerror(errno));
        goto out;
    }

    if (gpiod_line_request_set_values(g_state.line_request, output_values) < 0) {
        fprintf(stderr, "[lvgl-zero2w] failed to set initial GPIO values: %s\n", strerror(errno));
        goto out;
    }

    rc = 0;

out:
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    return rc;
}

int pi_ili9341_init(const pi_ili9341_config_t *config) {
    uint8_t mode = config->spi_mode;
    uint8_t bits = 8;

    memset(&g_state, 0, sizeof(g_state));
    g_state.spi_fd = -1;
    g_state.config = *config;
    g_state.dc_level = GPIOD_LINE_VALUE_INACTIVE;
    g_state.tx_buf_size = ILI9341_CHUNK_PIXELS * ILI9341_COLOR_BYTES;
    g_state.tx_buf = malloc(g_state.tx_buf_size);

    if (g_state.tx_buf == NULL) {
        fprintf(stderr, "[lvgl-zero2w] failed to allocate SPI transfer buffer\n");
        return -1;
    }

    if (init_gpio(config) < 0) {
        pi_ili9341_shutdown();
        return -1;
    }

    g_state.spi_fd = open(config->spi_path, O_RDWR);
    if (g_state.spi_fd < 0) {
        fprintf(stderr, "[lvgl-zero2w] failed to open %s: %s\n", config->spi_path, strerror(errno));
        pi_ili9341_shutdown();
        return -1;
    }

    if (ioctl(g_state.spi_fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(g_state.spi_fd, SPI_IOC_RD_MODE, &mode) < 0 ||
        ioctl(g_state.spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(g_state.spi_fd, SPI_IOC_RD_BITS_PER_WORD, &bits) < 0 ||
        ioctl(g_state.spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &g_state.config.speed_hz) < 0 ||
        ioctl(g_state.spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &g_state.config.speed_hz) < 0) {
        fprintf(stderr, "[lvgl-zero2w] failed to configure SPI device %s: %s\n", config->spi_path, strerror(errno));
        pi_ili9341_shutdown();
        return -1;
    }

    if (ili9341_init_panel() < 0) {
        fprintf(stderr, "[lvgl-zero2w] failed to initialize ILI9341 panel: %s\n", strerror(errno));
        pi_ili9341_shutdown();
        return -1;
    }

    return 0;
}

void pi_ili9341_shutdown(void) {
    if (g_state.spi_fd >= 0) {
        close(g_state.spi_fd);
        g_state.spi_fd = -1;
    }

    if (g_state.line_request != NULL) {
        gpiod_line_request_release(g_state.line_request);
        g_state.line_request = NULL;
    }

    if (g_state.chip != NULL) {
        gpiod_chip_close(g_state.chip);
        g_state.chip = NULL;
    }

    free(g_state.tx_buf);
    g_state.tx_buf = NULL;
    g_state.tx_buf_size = 0;
}

void pi_ili9341_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    const int32_t width = lv_area_get_width(area);
    const int32_t height = lv_area_get_height(area);
    const size_t total_pixels = (size_t)width * (size_t)height;
    const size_t max_chunk_pixels = flush_chunk_pixels();
    size_t offset = 0;

    (void)disp_drv;

    if (set_addr_window((uint16_t)area->x1,
                        (uint16_t)area->y1,
                        (uint16_t)area->x2,
                        (uint16_t)area->y2) < 0) {
        fprintf(stderr, "[lvgl-zero2w] failed to set draw window: %s\n", strerror(errno));
        lv_disp_flush_ready(disp_drv);
        return;
    }

    while (offset < total_pixels) {
        const size_t chunk_pixels = (total_pixels - offset) > max_chunk_pixels ? max_chunk_pixels : (total_pixels - offset);
        uint16_t *tx_words = (uint16_t *)g_state.tx_buf;

        for (size_t i = 0; i < chunk_pixels; ++i) {
            tx_words[i] = __builtin_bswap16(color_p[offset + i].full);
        }

        if (write_data(g_state.tx_buf, chunk_pixels * ILI9341_COLOR_BYTES) < 0) {
            fprintf(stderr, "[lvgl-zero2w] SPI flush failed: %s\n", strerror(errno));
            break;
        }

        offset += chunk_pixels;
    }

    lv_disp_flush_ready(disp_drv);
}

const pi_ili9341_config_t *pi_ili9341_get_active_config(void) {
    return &g_state.config;
}
