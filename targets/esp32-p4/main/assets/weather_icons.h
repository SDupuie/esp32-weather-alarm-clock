#pragma once

#include "lvgl.h"

typedef struct {
  uint16_t canvas_size;
  uint16_t crop_x;
  uint16_t crop_y;
} weather_icon_geometry_t;

const lv_image_dsc_t *weather_icon_image_for_name(const char *icon_name);
const weather_icon_geometry_t *weather_icon_geometry_for_image(const lv_image_dsc_t *image);
