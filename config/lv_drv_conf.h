#ifndef LV_DRV_CONF_H
#define LV_DRV_CONF_H

#define USE_SDL 1
#define USE_FBDEV 1
#define USE_EVDEV 1

#define SDL_HOR_RES 800
#define SDL_VER_RES 480
#define MONITOR_ZOOM 1
#define MONITOR_SDL_INCLUDE_PATH <SDL2/SDL.h>

#define FBDEV_PATH "/dev/fb0"
#define EVDEV_NAME "/dev/input/event2"

#endif /* LV_DRV_CONF_H */
