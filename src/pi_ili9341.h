#ifndef PI_ILI9341_H
#define PI_ILI9341_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

typedef struct {
    const char *spi_path;
    const char *gpiochip_path;
    unsigned int dc_gpio;
    unsigned int reset_gpio;
    uint32_t speed_hz;
    uint8_t spi_mode;
    uint8_t rotation;
    bool invert_colors;
    bool bgr;
    uint16_t width;
    uint16_t height;
} pi_ili9341_config_t;

int pi_ili9341_init(const pi_ili9341_config_t *config);
void pi_ili9341_shutdown(void);
void pi_ili9341_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
const pi_ili9341_config_t *pi_ili9341_get_active_config(void);

#endif
