#ifndef PI_XPT2046_H
#define PI_XPT2046_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

typedef struct {
    const char *spi_path;
    const char *gpiochip_path;
    int irq_gpio;
    uint32_t speed_hz;
    uint8_t spi_mode;
    uint16_t width;
    uint16_t height;
    uint16_t raw_x_min;
    uint16_t raw_x_max;
    uint16_t raw_y_min;
    uint16_t raw_y_max;
    uint16_t pressure_min;
    uint8_t rotation;
    bool swap_xy;
    bool invert_x;
    bool invert_y;
} pi_xpt2046_config_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t z1;
    uint16_t z2;
    bool pressed;
} pi_xpt2046_raw_sample_t;

int pi_xpt2046_init(const pi_xpt2046_config_t *config);
void pi_xpt2046_shutdown(void);
void pi_xpt2046_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
bool pi_xpt2046_read_raw_sample(pi_xpt2046_raw_sample_t *sample);
const pi_xpt2046_config_t *pi_xpt2046_get_active_config(void);

#endif
