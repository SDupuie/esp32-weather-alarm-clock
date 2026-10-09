#pragma once

#include "app_runtime.h"
#include "lvgl.h"

const lv_image_dsc_t *weather_icon_image_for_name(const char *icon_name);
const lv_image_dsc_t *weather_icon_image_for_snapshot(const app_weather_snapshot_t *weather);
