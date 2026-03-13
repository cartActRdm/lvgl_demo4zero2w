cmake_minimum_required(VERSION 3.16)
project(lvgl_zero2w C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

option(TARGET_HOST_SDL "Build host SDL simulator" ON)
option(TARGET_PI_FBDEV "Build legacy Linux fbdev target for Raspberry Pi" OFF)
option(TARGET_PI_SPI "Build direct spidev + libgpiod target for Raspberry Pi" OFF)
option(TARGET_PI_EVDEV "Enable Linux evdev touch input for legacy fbdev target" OFF)
option(UI_USE_SYSTEM_FREETYPE "Load Chinese fonts from system TTF/TTC/OTF files via LVGL FreeType" ON)

set(APP_UI_USE_FREETYPE 0)
set(APP_UI_FREETYPE_INCLUDE_DIRS "")
set(APP_UI_FREETYPE_LIBRARIES "")

if(UI_USE_SYSTEM_FREETYPE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FREETYPE2 REQUIRED freetype2)
  set(APP_UI_USE_FREETYPE 1)
  set(APP_UI_FREETYPE_INCLUDE_DIRS ${FREETYPE2_INCLUDE_DIRS})
  set(APP_UI_FREETYPE_LIBRARIES ${FREETYPE2_LIBRARIES})
endif()

set(LVGL_REPO_URL "$ENV{LVGL_REPO_URL}")
if(LVGL_REPO_URL STREQUAL "")
  set(LVGL_REPO_URL "https://github.com/lvgl/lvgl.git")
endif()

set(LV_DRIVERS_REPO_URL "$ENV{LV_DRIVERS_REPO_URL}")
if(LV_DRIVERS_REPO_URL STREQUAL "")
  set(LV_DRIVERS_REPO_URL "https://github.com/lvgl/lv_drivers.git")
endif()

set(LVGL_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lvgl")
set(LV_DRIVERS_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lv_drivers")
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(LVGL_INCLUDE_DIR "")
set(LV_DRIVERS_INCLUDE_DIR "")
set(LV_DRIVERS_SOURCE_DIR "")

if(EXISTS "${LVGL_LOCAL_DIR}/CMakeLists.txt")
  set(LVGL_INCLUDE_DIR ${LVGL_LOCAL_DIR})
  add_subdirectory(${LVGL_LOCAL_DIR} ${CMAKE_BINARY_DIR}/third_party/lvgl)
else()
  FetchContent_Declare(
    lvgl
    GIT_REPOSITORY ${LVGL_REPO_URL}
    GIT_TAG v8.3.11
  )
  FetchContent_MakeAvailable(lvgl)
  set(LVGL_INCLUDE_DIR ${lvgl_SOURCE_DIR})
endif()

target_include_directories(lvgl PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/config
)
target_compile_definitions(lvgl PUBLIC
  LV_CONF_INCLUDE_SIMPLE
  LV_DRV_CONF_INCLUDE_SIMPLE
  LV_USE_FREETYPE=${APP_UI_USE_FREETYPE}
  LV_FREETYPE_CACHE_SIZE=16384
  LV_FREETYPE_SBIT_CACHE=1
  LV_FREETYPE_CACHE_FT_FACES=4
  LV_FREETYPE_CACHE_FT_SIZES=8
)

if(APP_UI_USE_FREETYPE)
  target_include_directories(lvgl PUBLIC ${APP_UI_FREETYPE_INCLUDE_DIRS})
  target_link_libraries(lvgl PUBLIC ${APP_UI_FREETYPE_LIBRARIES})
endif()

if(TARGET_HOST_SDL OR TARGET_PI_FBDEV)
  if(EXISTS "${LV_DRIVERS_LOCAL_DIR}/lv_drv_conf_template.h")
    set(LV_DRIVERS_SOURCE_DIR ${LV_DRIVERS_LOCAL_DIR})
  else()
    FetchContent_Declare(
      lv_drivers
      GIT_REPOSITORY ${LV_DRIVERS_REPO_URL}
      GIT_TAG v8.3.0
    )
    FetchContent_GetProperties(lv_drivers)
    if(NOT lv_drivers_POPULATED)
      FetchContent_Populate(lv_drivers)
    endif()
    set(LV_DRIVERS_SOURCE_DIR ${lv_drivers_SOURCE_DIR})
  endif()

  set(LV_DRIVERS_INCLUDE_DIR ${LV_DRIVERS_SOURCE_DIR})

  add_library(lv_drivers STATIC
    ${LV_DRIVERS_SOURCE_DIR}/display/fbdev.c
    ${LV_DRIVERS_SOURCE_DIR}/indev/evdev.c
    ${LV_DRIVERS_SOURCE_DIR}/sdl/sdl.c
    ${LV_DRIVERS_SOURCE_DIR}/sdl/sdl_common.c
  )

  target_include_directories(lv_drivers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${THIRD_PARTY_DIR}
    ${LV_DRIVERS_SOURCE_DIR}
    ${LVGL_INCLUDE_DIR}
  )
  target_compile_definitions(lv_drivers PUBLIC LV_DRV_CONF_INCLUDE_SIMPLE LV_CONF_INCLUDE_SIMPLE LV_LVGL_H_INCLUDE_SIMPLE)
endif()

add_library(app_ui
  src/ui_demo.c
  src/ui_font_runtime.c
  src/fonts/lv_font_cn_14.c
)

target_include_directories(app_ui
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${THIRD_PARTY_DIR}
    ${LVGL_INCLUDE_DIR}
    ${LV_DRIVERS_INCLUDE_DIR}
)

target_compile_definitions(app_ui PUBLIC
  LV_CONF_INCLUDE_SIMPLE
  LV_DRV_CONF_INCLUDE_SIMPLE
  LV_USE_FREETYPE=${APP_UI_USE_FREETYPE}
)

target_include_directories(app_ui PRIVATE ${APP_UI_FREETYPE_INCLUDE_DIRS})
target_link_libraries(app_ui PUBLIC ${APP_UI_FREETYPE_LIBRARIES})

if(TARGET_HOST_SDL)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED sdl2)

  add_executable(lvgl-sim
    src/main.c
  )

  target_include_directories(lvgl-sim PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${THIRD_PARTY_DIR}
    ${LVGL_INCLUDE_DIR}
    ${LV_DRIVERS_INCLUDE_DIR}
  )

  target_compile_definitions(lvgl-sim PRIVATE
    LV_CONF_INCLUDE_SIMPLE
    LV_DRV_CONF_INCLUDE_SIMPLE
    USE_SDL
  )

  target_link_libraries(lvgl-sim PRIVATE
    app_ui
    lvgl
    lv_drivers
    ${SDL2_LIBRARIES}
  )

  target_link_directories(lvgl-sim PRIVATE ${SDL2_LIBRARY_DIRS})
endif()

if(TARGET_PI_FBDEV)
  add_executable(lvgl-pi-fb
    src/main.c
  )

  target_include_directories(lvgl-pi-fb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${THIRD_PARTY_DIR}
    ${LVGL_INCLUDE_DIR}
    ${LV_DRIVERS_INCLUDE_DIR}
  )

  target_compile_definitions(lvgl-pi-fb PRIVATE
    LV_CONF_INCLUDE_SIMPLE
    LV_DRV_CONF_INCLUDE_SIMPLE
    USE_FBDEV
  )

  if(TARGET_PI_EVDEV)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBEVDEV REQUIRED libevdev)
    target_compile_definitions(lvgl-pi-fb PRIVATE USE_EVDEV)
    target_include_directories(lvgl-pi-fb PRIVATE ${LIBEVDEV_INCLUDE_DIRS})
    target_link_directories(lvgl-pi-fb PRIVATE ${LIBEVDEV_LIBRARY_DIRS})
  endif()

  target_link_libraries(lvgl-pi-fb PRIVATE
    app_ui
    lvgl
    lv_drivers
    m
    pthread
    ${LIBEVDEV_LIBRARIES}
  )
endif()

if(TARGET_PI_SPI)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBGPIOD REQUIRED libgpiod)

  add_executable(lvgl-pi-spi
    src/main.c
    src/pi_ili9341.c
    src/pi_xpt2046.c
  )

  target_include_directories(lvgl-pi-spi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${THIRD_PARTY_DIR}
    ${LVGL_INCLUDE_DIR}
    ${LIBGPIOD_INCLUDE_DIRS}
  )

  target_compile_definitions(lvgl-pi-spi PRIVATE
    LV_CONF_INCLUDE_SIMPLE
    LV_DRV_CONF_INCLUDE_SIMPLE
    USE_ILI9341_SPI
  )

  target_link_directories(lvgl-pi-spi PRIVATE ${LIBGPIOD_LIBRARY_DIRS})
  target_link_libraries(lvgl-pi-spi PRIVATE
    app_ui
    lvgl
    m
    pthread
    ${LIBGPIOD_LIBRARIES}
  )
endif()
