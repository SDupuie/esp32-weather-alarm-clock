#include "ui_private.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "weather_icon_names.h"
#include "assets/analog_center_cap.h"
#include "assets/analog_edge_indicator.h"
#include "assets/analog_hour_hand.h"
#include "assets/analog_minute_hand.h"
#include "assets/analog_second_hand.h"
#include "assets/analog_stage_day.h"
#include "assets/analog_stage_night.h"
#include "assets/conditions_edge_indicator.h"
#include "assets/weather_icons.h"
#include "assets/round_stage_day.h"
#include "assets/round_stage_night.h"
#include "assets/digital_stage_day.h"
#include "assets/digital_stage_night.h"
#include "assets/lv_font_clock_time_144.h"
#include "assets/lv_font_temp_102.h"
#include "assets/lv_font_temp_105.h"

/* This panel's visible round glass is mounted a bit high relative to the
 * pixel array (not a coordinate bug — ticks/numerals computed from
 * ANALOG_FACE_CENTER sit 50-100px from the true edge and are unaffected, but
 * the background ring at ~20px from the edge was cut off at the bottom
 * while touching the top without compensating for it). The compensating
 * -18px trim is now baked into the stage art itself (see FACE_CY in
 * tools/render_analog_stage_svg.py / render_screen_stage_svg.py) rather than
 * applied as a runtime lv_obj_align offset here — the offset-align approach
 * was tried first and confirmed (by hiding the image entirely) to itself be
 * the source of a separate bright horizontal-line rendering artifact at the
 * bottom of the screen. Foreground content (ticks, hands, labels) is
 * intentionally left unshifted since it already has enough margin. */
#define ANALOG_FACE_SIZE 760
#define ANALOG_FACE_CENTER (ANALOG_FACE_SIZE / 2)
#define ANALOG_DAY_TOP 130
#define ANALOG_DATE_TOP 178
#define ANALOG_WEATHER_ICON_OFFSET_Y -38
#define ANALOG_WEATHER_ICON_SCALE 512
#define ANALOG_WEATHER_ICON_OPACITY 92
#define ANALOG_TICK_COUNT 60
#define ANALOG_NUMERAL_COUNT 12
#define ANALOG_TICK_OUTER_RADIUS 348
#define ANALOG_NUMERAL_RADIUS 286
#define ANALOG_MINOR_TICK_LENGTH 16
#define ANALOG_MAJOR_TICK_LENGTH 24
#define ANALOG_QUARTER_TICK_LENGTH 26
#define ANALOG_HOUR_ASSET_X 350
#define ANALOG_HOUR_ASSET_Y 228
#define ANALOG_HOUR_PIVOT_X 30
#define ANALOG_HOUR_PIVOT_Y 152
#define ANALOG_MINUTE_ASSET_X 360
#define ANALOG_MINUTE_ASSET_Y 138
#define ANALOG_MINUTE_PIVOT_X 20
#define ANALOG_MINUTE_PIVOT_Y 242
#define ANALOG_SECOND_ASSET_X 372
#define ANALOG_SECOND_ASSET_Y 118
#define ANALOG_SECOND_PIVOT_X 8
#define ANALOG_SECOND_PIVOT_Y 262
/* Must center the 72x72 cap image on the hands' shared pivot (380,380) —
 * i.e. pivot minus half the cap size. X was previously 364, landing the cap
 * 20px right of the true pivot, so hand tails poked out past its left edge. */
#define ANALOG_CENTER_CAP_X 344
#define ANALOG_CENTER_CAP_Y 344
/* Must match the crop offset render_edge_indicator.py prints when
 * regenerated (scripts/generate-analog-parity-assets.sh) -- the asset is
 * cropped to its content bounding box rather than stored as a full 800x800
 * canvas, so this positions that crop back where it belongs on the face. */
#define ANALOG_EDGE_INDICATOR_X 13
#define ANALOG_EDGE_INDICATOR_Y 99
#define ANALOG_TEMP_BLOCK_WIDTH 300
#define ANALOG_TEMP_BLOCK_TOP 472
#define ANALOG_TEMP_LABEL_HEIGHT 116
/* The temperature uses a native-size 105px font without scaling.
 * Retain its extra draw margin for clipping and invalidation. */
#define ANALOG_TEMP_LABEL_EXT_DRAW_MARGIN 200
#define EDGE_INDICATOR_PULSE_MIN_OPA 184
#define EDGE_INDICATOR_PULSE_MAX_OPA 255
#define EDGE_INDICATOR_IMPORTANT_NIGHT_TINT_OPA 66
#define EDGE_INDICATOR_UNREAD_NIGHT_TINT_OPA 46
#define EDGE_INDICATOR_PULSE_DURATION_MS 900
/* Unlike ANALOG_FACE_SIZE, nothing here has a rotating-hand pivot calibrated
 * against this value, so it's safe to grow independently of the analog
 * screen. Paired with FACE_RADIUS=390 in render_screen_stage_svg.py so the
 * digital/forecast/message background art matches. */
#define FACE_CONTENT_SIZE 780
#define DIGITAL_DAY_TOP 80
#define DIGITAL_DATE_TOP 115
/* 150 was correct for the old scaled lv_font_montserrat_48 (line_height 52,
 * base_line 9, scaled 3x -> effective line_height 156 / base_line 27, baseline
 * at 150+156-27=279). lv_font_clock_time_144 -- generated from digits, ':'
 * and '-', with no descenders/tall accents to reserve room for -- reports a
 * much tighter line_height 104 / base_line 2, landing the baseline at
 * 150+104-2=252: 27px higher, which read as the time floating away from the
 * PM/divider line below it. +27 restores the original baseline position. */
#define DIGITAL_TIME_TOP 177
#define DIGITAL_TIME_WIDTH 620
#define DIGITAL_MERIDIEM_TOP 268
/* Approved appearance is documented in mockups/alarm-design.html. The
 * constants below define the ESP32-P4 layout values used by this file. */
#define DIGITAL_TIME_GAP 14
#define DIGITAL_DIVIDER_WIDTH 500
#define DIGITAL_DIVIDER_TOP 300
#define DIGITAL_DIVIDER_BOTTOM 486
#define DIGITAL_CURRENT_TOP 320
#define DIGITAL_CURRENT_WIDTH 600
#define DIGITAL_CURRENT_HEIGHT 150
#define DIGITAL_CURRENT_TEMP_LEFT 25
#define DIGITAL_CURRENT_COPY_LEFT 400
#define DIGITAL_CURRENT_COPY_WIDTH 270
#define DIGITAL_CURRENT_ICON_SIZE 200
#define DIGITAL_FORECAST_TOP 500
#define DIGITAL_FORECAST_WIDTH 478
#define DIGITAL_FORECAST_ICON_SIZE 75
#define CONDITIONS_EDGE_INDICATOR_X 569
#define CONDITIONS_EDGE_INDICATOR_Y 107
#define DAY_COUNT APP_FORECAST_DAYS

typedef struct {
  lv_obj_t *analog_view;
  lv_obj_t *digital_view;
  lv_obj_t *clock_status_label;
  lv_obj_t *clock_status_container;
  lv_obj_t *clock_status_icon;
  lv_obj_t *unavailable_view;
  lv_obj_t *unavailable_stage;
  lv_obj_t *unavailable_title;
  lv_obj_t *unavailable_time;
  lv_obj_t *unavailable_sync;
  lv_obj_t *analog_stage_image;
  lv_obj_t *analog_weather_layer;
  lv_obj_t *analog_face;
  lv_obj_t *analog_weather_icon;
  lv_obj_t *analog_tick_lines[ANALOG_TICK_COUNT];
  lv_obj_t *analog_numeral_labels[ANALOG_NUMERAL_COUNT];
  lv_obj_t *analog_day_label;
  lv_obj_t *analog_date_label;
  lv_obj_t *analog_temp_label;
  lv_obj_t *analog_high_low_label;
  lv_obj_t *analog_edge_indicator;
  lv_obj_t *analog_hour_asset;
  lv_obj_t *analog_minute_asset;
  lv_obj_t *analog_center_cap;
  lv_obj_t *analog_temp_container;
  lv_obj_t *digital_stage_image;
  lv_obj_t *digital_day_label;
  lv_obj_t *digital_date_label;
  lv_obj_t *digital_time_label;
  lv_obj_t *digital_meridiem_label;
  lv_obj_t *digital_current_icon;
  lv_obj_t *digital_temp_label;
  lv_obj_t *digital_summary_label;
  lv_obj_t *digital_high_low_label;
  lv_obj_t *digital_edge_indicator;
  lv_obj_t *digital_divider_top;
  lv_obj_t *digital_divider_bottom;
  lv_obj_t *digital_current_panel;
  lv_obj_t *digital_forecast_container;
  lv_obj_t *digital_forecast_day_labels[DAY_COUNT];
  lv_obj_t *digital_forecast_icons[DAY_COUNT];
  lv_obj_t *digital_forecast_temp_labels[DAY_COUNT];
  lv_obj_t *analog_weather_indicator;
  lv_obj_t *digital_weather_indicator;
  lv_obj_t *second_hand;
  lv_obj_t *backup_indicator;
  lv_obj_t *backup_overlay;
  lv_obj_t *backup_card;
  lv_obj_t *backup_title;
  lv_obj_t *backup_detail;
  lv_obj_t *backup_dismiss;
  lv_obj_t *backup_error;
  lv_point_precise_t analog_tick_points[ANALOG_TICK_COUNT][2];
} clock_view_objects_t;

static clock_view_objects_t s_clock;
static const char *TAG = "rwd_app_ui";

static const char *const WEEKDAY_NAMES[] = {
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};
static const char *const WEEKDAY_NAMES_UPPER[] = {
  "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"
};
static const char *const MONTH_NAMES[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static lv_obj_t *create_weather_indicator(lv_obj_t *view);

static lv_color_t color_accent(void)
{
  return lv_color_hex(0x78b8ff);
}

static lv_color_t color_digital_accent(void)
{
  return lv_color_hex(0x93d1ff);
}

static lv_color_t color_temp(void)
{
  return lv_color_hex(0xfff1d1);
}

static void analog_polar_point(double angle_deg, double radius, int32_t *out_x, int32_t *out_y)
{
  double radians = (angle_deg - 90.0) * (3.14159265358979323846 / 180.0);
  double cos_value = cos(radians);
  double sin_value = sin(radians);

  if (out_x != NULL) {
    *out_x = (int32_t) lround(ANALOG_FACE_CENTER + (cos_value * radius));
  }

  if (out_y != NULL) {
    *out_y = (int32_t) lround(ANALOG_FACE_CENTER + (sin_value * radius));
  }
}

static void position_object_centered(lv_obj_t *obj, int32_t center_x, int32_t center_y)
{
  if (obj == NULL) {
    return;
  }

  lv_obj_update_layout(obj);
  lv_obj_set_pos(
    obj,
    center_x - (lv_obj_get_width(obj) / 2),
    center_y - (lv_obj_get_height(obj) / 2)
  );
}

static void apply_analog_dial_palette(bool night_active)
{
  for (int i = 0; i < ANALOG_TICK_COUNT; ++i) {
    lv_obj_t *tick = s_clock.analog_tick_lines[i];
    if (tick == NULL) {
      continue;
    }

    lv_color_t color = lv_color_hex(0xffffff);
    lv_opa_t opacity = LV_OPA_40;
    int width = 2;

    if ((i % 15) == 0) {
      color = night_active ? lv_color_hex(0xe0acac) : lv_color_hex(0xf4f8ff);
      opacity = night_active ? 64 : LV_OPA_80;
      width = 4;
    } else if ((i % 5) == 0) {
      color = night_active ? lv_color_hex(0xd6a0a0) : lv_color_hex(0xffffff);
      opacity = night_active ? 56 : LV_OPA_70;
      width = 4;
    } else {
      color = night_active ? lv_color_hex(0xc17e7e) : lv_color_hex(0xffffff);
      opacity = night_active ? 32 : LV_OPA_50;
    }

    lv_obj_set_style_line_color(tick, color, 0);
    lv_obj_set_style_line_opa(tick, opacity, 0);
    lv_obj_set_style_line_width(tick, width, 0);
  }

  for (int i = 0; i < ANALOG_NUMERAL_COUNT; ++i) {
    lv_obj_t *numeral = s_clock.analog_numeral_labels[i];
    if (numeral == NULL) {
      continue;
    }

    int display_hour = i == 0 ? 12 : i;
    bool major = (display_hour % 3) == 0;
    lv_obj_set_style_text_color(
      numeral,
      major
        ? (night_active ? lv_color_hex(0xe1b6b6) : lv_color_hex(0xf4f8ff))
        : (night_active ? lv_color_hex(0xc69696) : lv_color_hex(0xe0e9f7)),
      0
    );
    lv_obj_set_style_text_opa(numeral, major ? LV_OPA_90 : 76, 0);
  }
}

static void build_analog_dial_objects(lv_obj_t *parent)
{
  if (parent == NULL) {
    return;
  }

  for (int i = 0; i < ANALOG_TICK_COUNT; ++i) {
    int32_t outer_x = 0;
    int32_t outer_y = 0;
    int32_t inner_x = 0;
    int32_t inner_y = 0;
    int tick_length = ANALOG_MINOR_TICK_LENGTH;

    if ((i % 15) == 0) {
      tick_length = ANALOG_QUARTER_TICK_LENGTH;
    } else if ((i % 5) == 0) {
      tick_length = ANALOG_MAJOR_TICK_LENGTH;
    }

    analog_polar_point(i * 6.0, ANALOG_TICK_OUTER_RADIUS, &outer_x, &outer_y);
    analog_polar_point(i * 6.0, ANALOG_TICK_OUTER_RADIUS - tick_length, &inner_x, &inner_y);

    s_clock.analog_tick_points[i][0].x = outer_x;
    s_clock.analog_tick_points[i][0].y = outer_y;
    s_clock.analog_tick_points[i][1].x = inner_x;
    s_clock.analog_tick_points[i][1].y = inner_y;

    s_clock.analog_tick_lines[i] = lv_line_create(parent);
    lv_obj_set_size(s_clock.analog_tick_lines[i], ANALOG_FACE_SIZE, ANALOG_FACE_SIZE);
    lv_obj_align(s_clock.analog_tick_lines[i], LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_line_rounded(s_clock.analog_tick_lines[i], true, 0);
    lv_line_set_points_mutable(s_clock.analog_tick_lines[i], s_clock.analog_tick_points[i], 2);
  }

  static const int8_t major_offsets[ANALOG_NUMERAL_COUNT][2] = {
    {0, -2}, {0, 0}, {0, 0}, {1, 0}, {0, 0}, {0, 0},
    {0, 2}, {0, 0}, {0, 0}, {-1, 0}, {0, 0}, {0, 0}
  };

  for (int i = 0; i < ANALOG_NUMERAL_COUNT; ++i) {
    int display_hour = i == 0 ? 12 : i;
    bool major = (display_hour % 3) == 0;
    int32_t center_x = 0;
    int32_t center_y = 0;
    char numeral_text[4];

    analog_polar_point(i * 30.0, ANALOG_NUMERAL_RADIUS, &center_x, &center_y);
    center_x += major_offsets[i][0];
    center_y += major_offsets[i][1];
    snprintf(numeral_text, sizeof(numeral_text), "%d", display_hour);

    s_clock.analog_numeral_labels[i] = wac_ui_create_label(
      parent,
      major ? &lv_font_montserrat_32 : &lv_font_montserrat_24,
      wac_ui_color_text_primary(),
      LV_TEXT_ALIGN_CENTER,
      numeral_text
    );
    lv_obj_set_style_text_letter_space(s_clock.analog_numeral_labels[i], 0, 0);
    lv_obj_set_style_transform_scale(s_clock.analog_numeral_labels[i], major ? 384 : 362, 0);
    lv_obj_update_layout(s_clock.analog_numeral_labels[i]);
    lv_obj_set_style_transform_pivot_x(s_clock.analog_numeral_labels[i], lv_obj_get_width(s_clock.analog_numeral_labels[i]) / 2, 0);
    lv_obj_set_style_transform_pivot_y(s_clock.analog_numeral_labels[i], lv_obj_get_height(s_clock.analog_numeral_labels[i]) / 2, 0);
    position_object_centered(s_clock.analog_numeral_labels[i], center_x, center_y);
  }

  apply_analog_dial_palette(false);
}

static void log_object_bounds(
  const char *prefix,
  lv_obj_t *obj,
  const char *icon_name,
  const lv_image_dsc_t *asset,
  bool used_fallback
)
{
  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 0;
  int32_t height = 0;
  bool hidden = true;

  if (obj != NULL) {
    lv_obj_update_layout(obj);
    x = lv_obj_get_x(obj);
    y = lv_obj_get_y(obj);
    width = lv_obj_get_width(obj);
    height = lv_obj_get_height(obj);
    hidden = lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }

  ESP_LOGI(
    TAG,
    "%s: %s%s -> asset=%p cf=%u img=%ux%u stride=%u bytes=%u obj=%p hidden=%d pos=%" PRId32 ",%" PRId32 " size=%" PRId32 "x%" PRId32,
    prefix,
    icon_name != NULL ? icon_name : weather_icon_fallback_name(),
    used_fallback ? " (fallback)" : "",
    (const void *) asset,
    asset != NULL ? (unsigned int) asset->header.cf : 0U,
    asset != NULL ? (unsigned int) asset->header.w : 0U,
    asset != NULL ? (unsigned int) asset->header.h : 0U,
    asset != NULL ? (unsigned int) asset->header.stride : 0U,
    asset != NULL ? (unsigned int) asset->data_size : 0U,
    (void *) obj,
    hidden ? 1 : 0,
    x,
    y,
    width,
    height
  );
}

static void log_analog_hand_state(
  const char *name,
  lv_obj_t *obj,
  const lv_image_dsc_t *asset,
  int pivot_x,
  int pivot_y
)
{
  if (obj == NULL || asset == NULL) {
    ESP_LOGW(TAG, "Analog %s hand missing obj=%p asset=%p", name, (void *) obj, (const void *) asset);
    return;
  }

  lv_obj_update_layout(obj);
  ESP_LOGI(
    TAG,
    "Analog %s hand: asset=%p cf=%u pos=%" PRId32 ",%" PRId32 " size=%" PRId32 "x%" PRId32
    " pivot=%d,%d opa=%u hidden=%d",
    name,
    (const void *) asset,
    (unsigned) asset->header.cf,
    (int32_t) lv_obj_get_x(obj),
    (int32_t) lv_obj_get_y(obj),
    (int32_t) lv_obj_get_width(obj),
    (int32_t) lv_obj_get_height(obj),
    pivot_x,
    pivot_y,
    (unsigned) lv_obj_get_style_opa(obj, 0),
    lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) ? 1 : 0
  );
}

void wac_clock_log_weather(const app_weather_snapshot_t *weather)
{
  bool live_weather = weather != NULL;
  if (s_clock.digital_view != NULL) {
    lv_obj_update_layout(s_clock.digital_view);
  }

  const char *current_icon_name = live_weather
    ? weather_icon_name_for_snapshot(weather)
    : weather_icon_fallback_name();
  bool current_used_fallback = live_weather && !weather_icon_name_is_known(weather->icon_name);
  const lv_image_dsc_t *current_asset = weather_icon_image_for_name(current_icon_name);

  log_object_bounds(
    "DIGITAL current icon",
    s_clock.digital_current_icon,
    current_icon_name,
    current_asset,
    current_used_fallback
  );

  if (s_clock.digital_current_panel != NULL) {
    log_object_bounds(
      "DIGITAL current panel",
      s_clock.digital_current_panel,
      current_icon_name,
      current_asset,
      current_used_fallback
    );
  }

  if (s_clock.digital_forecast_container != NULL) {
    log_object_bounds(
      "DIGITAL forecast container",
      s_clock.digital_forecast_container,
      current_icon_name,
      current_asset,
      current_used_fallback
    );
  }

  for (int i = 0; i < DAY_COUNT; ++i) {
    const char *forecast_icon_name = live_weather
      ? weather_icon_name_or_fallback(weather->forecast[i].icon_name)
      : weather_icon_fallback_name();
    bool used_fallback = live_weather && !weather_icon_name_is_known(weather->forecast[i].icon_name);
    const lv_image_dsc_t *forecast_asset = weather_icon_image_for_name(forecast_icon_name);
    int forecast_temp = live_weather ? weather->forecast[i].high : 0;
    const char *forecast_day = live_weather ? wac_ui_forecast_weekday(&weather->forecast[i]) : "--";

    ESP_LOGI(
      TAG,
      "FORECAST[%d]: %s %s%s %d -> asset=%p obj=%p hidden=%d pos=%" PRId32 ",%" PRId32 " size=%" PRId32 "x%" PRId32,
      i,
      forecast_day,
      forecast_icon_name,
      used_fallback ? " (fallback)" : "",
      forecast_temp,
      (const void *) forecast_asset,
      (void *) s_clock.digital_forecast_icons[i],
      s_clock.digital_forecast_icons[i] != NULL && lv_obj_has_flag(s_clock.digital_forecast_icons[i], LV_OBJ_FLAG_HIDDEN) ? 1 : 0,
      s_clock.digital_forecast_icons[i] != NULL ? lv_obj_get_x(s_clock.digital_forecast_icons[i]) : 0,
      s_clock.digital_forecast_icons[i] != NULL ? lv_obj_get_y(s_clock.digital_forecast_icons[i]) : 0,
      s_clock.digital_forecast_icons[i] != NULL ? lv_obj_get_width(s_clock.digital_forecast_icons[i]) : 0,
      s_clock.digital_forecast_icons[i] != NULL ? lv_obj_get_height(s_clock.digital_forecast_icons[i]) : 0
    );
  }
}

static int32_t scale_dimension(int32_t value, int32_t scale)
{
  return (value * scale + 128) / 256;
}

static void layout_digital_time_group(bool show_meridiem)
{
  if (s_clock.digital_time_label == NULL || s_clock.digital_meridiem_label == NULL) {
    return;
  }

  lv_obj_set_style_transform_scale(s_clock.digital_time_label, LV_SCALE_NONE, 0);
  lv_obj_update_layout(s_clock.digital_time_label);
  lv_obj_update_layout(s_clock.digital_meridiem_label);

  int32_t time_width = lv_obj_get_width(s_clock.digital_time_label);
  int32_t time_scale_x = lv_obj_get_style_transform_scale_x_safe(s_clock.digital_time_label, LV_PART_MAIN);
  int32_t scaled_time_width = scale_dimension(time_width, time_scale_x);
  int32_t meridiem_width = show_meridiem ? lv_obj_get_width(s_clock.digital_meridiem_label) : 0;
  int32_t group_width = scaled_time_width;
  if (show_meridiem) {
    group_width += DIGITAL_TIME_GAP + meridiem_width;
  }

  int32_t block_left = (FACE_CONTENT_SIZE - DIGITAL_TIME_WIDTH) / 2;
  int32_t group_left = block_left + (DIGITAL_TIME_WIDTH - group_width) / 2;
  if (group_left < block_left) {
    group_left = block_left;
  }

  lv_obj_set_pos(s_clock.digital_time_label, group_left, DIGITAL_TIME_TOP);

  if (!show_meridiem) {
    lv_obj_add_flag(s_clock.digital_meridiem_label, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_obj_clear_flag(s_clock.digital_meridiem_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(
    s_clock.digital_meridiem_label,
    group_left + scaled_time_width + DIGITAL_TIME_GAP,
    DIGITAL_MERIDIEM_TOP
  );
}

/* Reserve the widest digits and period from the approved preview. The scale
 * depends on the font, not the current minute, so its size stays steady. */
static void layout_digital_night_time(bool show_meridiem)
{
  lv_point_t size;
  int32_t widest = 0;
  for (int digit = 0; digit < 10; ++digit) {
    char sample[6];
    snprintf(sample, sizeof(sample), "%d%d:%d%d", digit, digit, digit, digit);
    lv_text_get_size(&size, sample, &lv_font_clock_time_144, -2, 0,
      LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (size.x > widest) widest = size.x;
  }
  int32_t period_width = 0;
  if (show_meridiem) {
    const char *periods[] = {"AM", "PM"};
    for (size_t i = 0; i < 2; ++i) {
      lv_text_get_size(&size, periods[i], &lv_font_montserrat_24, 2, 0,
        LV_COORD_MAX, LV_TEXT_FLAG_NONE);
      if (size.x > period_width) period_width = size.x;
    }
  }
  double max_size = 752.0 / hypot(widest / 144.0, 1.0);
  if (show_meridiem) {
    double low = 144.0, high = max_size;
    for (int pass = 0; pass < 12; ++pass) {
      double candidate = (low + high) / 2.0;
      double scale = candidate / 144.0;
      double right = widest * scale / 2.0 + 8 + period_width;
      double bottom = lv_font_clock_time_144.line_height * scale / 2.0
        - lv_font_clock_time_144.base_line * scale + lv_font_montserrat_24.base_line;
      if (hypot(right, bottom) <= 388.0) low = candidate;
      else high = candidate;
    }
    max_size = low;
  }
  int32_t scale = (int32_t)(floor(max_size) * 256.0 / 144.0);
  lv_obj_set_style_transform_pivot_x(s_clock.digital_time_label, 0, 0);
  lv_obj_set_style_transform_pivot_y(s_clock.digital_time_label, 0, 0);
  lv_obj_set_style_transform_scale(s_clock.digital_time_label, scale, 0);
  lv_obj_update_layout(s_clock.digital_time_label);
  lv_obj_update_layout(s_clock.digital_meridiem_label);
  int32_t width = scale_dimension(lv_obj_get_width(s_clock.digital_time_label), scale);
  int32_t height = scale_dimension(lv_obj_get_height(s_clock.digital_time_label), scale);
  int32_t top = FACE_CONTENT_SIZE / 2 - height / 2;
  lv_obj_set_pos(s_clock.digital_time_label, FACE_CONTENT_SIZE / 2 - width / 2, top);
  if (!show_meridiem) {
    lv_obj_add_flag(s_clock.digital_meridiem_label, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_remove_flag(s_clock.digital_meridiem_label, LV_OBJ_FLAG_HIDDEN);
  int32_t baseline = top + height - scale_dimension(lv_font_clock_time_144.base_line, scale);
  lv_obj_set_pos(s_clock.digital_meridiem_label, FACE_CONTENT_SIZE / 2 + width / 2 + 8,
    baseline - lv_font_montserrat_24.line_height + lv_font_montserrat_24.base_line);
}

static void set_object_opa_anim_cb(void *obj, int32_t value)
{
  if (obj == NULL) {
    return;
  }

  lv_obj_set_style_opa((lv_obj_t *) obj, (lv_opa_t) value, 0);
}

static void stop_indicator_pulse(lv_obj_t *indicator)
{
  if (indicator == NULL) {
    return;
  }

  lv_anim_delete(indicator, set_object_opa_anim_cb);
}

static void start_indicator_pulse(lv_obj_t *indicator)
{
  if (indicator == NULL || lv_anim_get(indicator, set_object_opa_anim_cb) != NULL) {
    return;
  }

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, indicator);
  lv_anim_set_exec_cb(&anim, set_object_opa_anim_cb);
  lv_anim_set_values(&anim, EDGE_INDICATOR_PULSE_MIN_OPA, EDGE_INDICATOR_PULSE_MAX_OPA);
  lv_anim_set_time(&anim, EDGE_INDICATOR_PULSE_DURATION_MS);
  lv_anim_set_playback_time(&anim, EDGE_INDICATOR_PULSE_DURATION_MS);
  lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&anim);
}

static void format_high_low(char *out, size_t out_size, int high, int low)
{
  snprintf(out, out_size, "H:%d°  L:%d°", high, low);
}

static void apply_analog_night_state(bool active)
{
  if (s_clock.analog_stage_image != NULL) {
    lv_image_set_src(s_clock.analog_stage_image, active ? &analog_stage_night : &analog_stage_day);
  }

  if (s_clock.analog_weather_icon != NULL) {
    lv_obj_set_style_opa(s_clock.analog_weather_icon, active ? LV_OPA_TRANSP : ANALOG_WEATHER_ICON_OPACITY, 0);
  }

  if (s_clock.analog_day_label != NULL) {
    lv_obj_set_style_text_color(s_clock.analog_day_label, active ? wac_ui_color_night_primary() : wac_ui_color_text_primary(), 0);
  }

  if (s_clock.analog_date_label != NULL) {
    lv_obj_set_style_text_color(s_clock.analog_date_label, active ? wac_ui_color_night_muted() : wac_ui_color_text_muted(), 0);
  }

  apply_analog_dial_palette(active);

  if (s_clock.analog_temp_container != NULL) {
    if (active) {
      lv_obj_add_flag(s_clock.analog_temp_container, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(s_clock.analog_temp_container, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (s_clock.analog_temp_label != NULL) {
    lv_obj_set_style_text_color(s_clock.analog_temp_label, active ? wac_ui_color_night_primary() : color_temp(), 0);
  }

  if (s_clock.analog_high_low_label != NULL) {
    lv_obj_set_style_text_color(s_clock.analog_high_low_label, active ? wac_ui_color_night_muted() : wac_ui_color_text_primary(), 0);
  }

  if (s_clock.analog_hour_asset != NULL) {
    wac_ui_apply_image_night_tint(s_clock.analog_hour_asset, active, lv_color_hex(0xecc0c0), LV_OPA_80);
  }

  if (s_clock.analog_minute_asset != NULL) {
    wac_ui_apply_image_night_tint(s_clock.analog_minute_asset, active, lv_color_hex(0xecc0c0), LV_OPA_80);
  }

  if (s_clock.second_hand != NULL) {
    wac_ui_apply_image_night_tint(s_clock.second_hand, active, lv_color_hex(0xbb8484), LV_OPA_60);
    lv_obj_set_style_opa(s_clock.second_hand, active ? LV_OPA_80 : LV_OPA_COVER, 0);
  }

  if (s_clock.analog_center_cap != NULL) {
    wac_ui_apply_image_night_tint(s_clock.analog_center_cap, active, lv_color_hex(0xd6aaaa), LV_OPA_30);
  }
}

static void apply_digital_night_state(bool active)
{
  if (s_clock.digital_stage_image != NULL) {
    lv_image_set_src(s_clock.digital_stage_image, active ? &digital_stage_night : &digital_stage_day);
  }

  if (s_clock.digital_day_label != NULL) {
    lv_obj_set_style_text_color(s_clock.digital_day_label, active ? wac_ui_color_night_primary() : color_digital_accent(), 0);
  }

  if (s_clock.digital_date_label != NULL) {
    lv_obj_set_style_text_color(s_clock.digital_date_label, active ? wac_ui_color_night_muted() : lv_color_hex(0xbce4ff), 0);
  }

  if (s_clock.digital_time_label != NULL) {
    lv_obj_set_style_text_color(s_clock.digital_time_label, active ? wac_ui_color_night_primary() : wac_ui_color_text_primary(), 0);
  }

  if (s_clock.digital_meridiem_label != NULL) {
    lv_obj_set_style_text_color(s_clock.digital_meridiem_label, active ? wac_ui_color_night_muted() : color_accent(), 0);
  }

  if (s_clock.digital_current_panel != NULL) {
    if (active) {
      lv_obj_add_flag(s_clock.digital_current_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(s_clock.digital_current_panel, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (s_clock.digital_divider_top != NULL) {
    if (active) {
      lv_obj_add_flag(s_clock.digital_divider_top, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(s_clock.digital_divider_top, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (s_clock.digital_divider_bottom != NULL) {
    if (active) {
      lv_obj_add_flag(s_clock.digital_divider_bottom, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(s_clock.digital_divider_bottom, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (s_clock.digital_forecast_container != NULL) {
    if (active) {
      lv_obj_add_flag(s_clock.digital_forecast_container, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(s_clock.digital_forecast_container, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void apply_edge_indicator_state(lv_obj_t *indicator, bool visible, bool important, bool night_active)
{
  if (indicator == NULL) {
    return;
  }

  if (!visible) {
    stop_indicator_pulse(indicator);
    lv_obj_add_flag(indicator, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_obj_clear_flag(indicator, LV_OBJ_FLAG_HIDDEN);
  lv_image_set_src(indicator, &analog_edge_indicator);

  if (important) {
    wac_ui_apply_image_night_tint(
      indicator,
      night_active,
      lv_color_hex(0xd9a0a0),
      EDGE_INDICATOR_IMPORTANT_NIGHT_TINT_OPA
    );
    start_indicator_pulse(indicator);
    return;
  }

  stop_indicator_pulse(indicator);
  wac_ui_apply_image_night_tint(
    indicator,
    night_active,
    lv_color_hex(0xbf8888),
    EDGE_INDICATOR_UNREAD_NIGHT_TINT_OPA
  );
  lv_obj_set_style_opa(indicator, night_active ? LV_OPA_80 : LV_OPA_COVER, 0);
}

void wac_clock_build_analog(lv_obj_t *parent)
{
  s_clock.analog_view = lv_obj_create(parent);
  lv_obj_set_size(s_clock.analog_view, LV_PCT(100), LV_PCT(100));
  /* Opaque, not transparent: a solid, deliberate backing color behind the
   * stage image rather than relying on transparency down through the stack. */
  lv_obj_set_style_bg_color(s_clock.analog_view, wac_ui_color_bg(), 0);
  lv_obj_set_style_bg_opa(s_clock.analog_view, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_clock.analog_view, 0, 0);
  lv_obj_set_style_pad_all(s_clock.analog_view, 0, 0);
  lv_obj_set_scrollbar_mode(s_clock.analog_view, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(s_clock.analog_view, LV_OBJ_FLAG_HIDDEN);

  s_clock.analog_stage_image = lv_image_create(s_clock.analog_view);
  lv_image_set_src(s_clock.analog_stage_image, &analog_stage_day);
  lv_obj_align(s_clock.analog_stage_image, LV_ALIGN_CENTER, 0, 0);

  s_clock.analog_weather_layer = lv_obj_create(s_clock.analog_view);
  lv_obj_set_size(s_clock.analog_weather_layer, ANALOG_FACE_SIZE, ANALOG_FACE_SIZE);
  lv_obj_align(s_clock.analog_weather_layer, LV_ALIGN_CENTER, 0, 0);
  wac_ui_clear_container_chrome(s_clock.analog_weather_layer);
  lv_obj_add_flag(s_clock.analog_weather_layer, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  s_clock.analog_weather_icon = lv_image_create(s_clock.analog_weather_layer);
  lv_image_set_src(s_clock.analog_weather_icon, weather_icon_image_for_snapshot(NULL));
  lv_image_set_scale(s_clock.analog_weather_icon, ANALOG_WEATHER_ICON_SCALE);
  lv_obj_set_style_opa(s_clock.analog_weather_icon, ANALOG_WEATHER_ICON_OPACITY, 0);
  lv_obj_align(s_clock.analog_weather_icon, LV_ALIGN_CENTER, 0, ANALOG_WEATHER_ICON_OFFSET_Y);

  s_clock.analog_face = lv_obj_create(s_clock.analog_view);
  lv_obj_set_size(s_clock.analog_face, ANALOG_FACE_SIZE, ANALOG_FACE_SIZE);
  lv_obj_align(s_clock.analog_face, LV_ALIGN_CENTER, 0, 0);
  wac_ui_clear_container_chrome(s_clock.analog_face);
  lv_obj_add_flag(s_clock.analog_face, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  build_analog_dial_objects(s_clock.analog_face);

  s_clock.analog_edge_indicator = lv_image_create(s_clock.analog_view);
  lv_image_set_src(s_clock.analog_edge_indicator, &analog_edge_indicator);
  lv_obj_align(s_clock.analog_edge_indicator, LV_ALIGN_TOP_LEFT, ANALOG_EDGE_INDICATOR_X, ANALOG_EDGE_INDICATOR_Y);
  lv_obj_add_flag(s_clock.analog_edge_indicator, LV_OBJ_FLAG_HIDDEN);
  s_clock.analog_weather_indicator = create_weather_indicator(s_clock.analog_view);

  s_clock.analog_day_label = wac_ui_create_label(
    s_clock.analog_face, &lv_font_montserrat_32, wac_ui_color_text_primary(), LV_TEXT_ALIGN_CENTER, ""
  );
  lv_obj_align(s_clock.analog_day_label, LV_ALIGN_TOP_MID, 0, ANALOG_DAY_TOP);
  lv_obj_set_style_text_opa(s_clock.analog_day_label, LV_OPA_70, 0);
  lv_obj_set_style_text_letter_space(s_clock.analog_day_label, 1, 0);

  s_clock.analog_date_label = wac_ui_create_label(
    s_clock.analog_face, &lv_font_montserrat_24, wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, ""
  );
  lv_obj_align(s_clock.analog_date_label, LV_ALIGN_TOP_MID, 0, ANALOG_DATE_TOP);
  lv_obj_set_style_text_opa(s_clock.analog_date_label, LV_OPA_60, 0);

  s_clock.analog_temp_container = lv_obj_create(s_clock.analog_face);
  wac_ui_clear_container_chrome(s_clock.analog_temp_container);
  lv_obj_set_size(s_clock.analog_temp_container, ANALOG_TEMP_BLOCK_WIDTH, LV_SIZE_CONTENT);
  lv_obj_align(s_clock.analog_temp_container, LV_ALIGN_TOP_MID, 0, ANALOG_TEMP_BLOCK_TOP);
  lv_obj_add_flag(s_clock.analog_temp_container, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  /* OVERFLOW_VISIBLE only widens the clip region it hands to children by this
   * container's OWN ext_draw_size, which defaults to 0 without a transform
   * style of its own. Without this, the scaled temp label below never
   * actually gets clip room from its parent no matter what margin the label
   * requests for itself. */
  lv_obj_set_style_transform_width(s_clock.analog_temp_container, ANALOG_TEMP_LABEL_EXT_DRAW_MARGIN, 0);
  lv_obj_set_style_transform_height(s_clock.analog_temp_container, ANALOG_TEMP_LABEL_EXT_DRAW_MARGIN, 0);
  lv_obj_set_layout(s_clock.analog_temp_container, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(s_clock.analog_temp_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(
    s_clock.analog_temp_container,
    LV_FLEX_ALIGN_CENTER,
    LV_FLEX_ALIGN_CENTER,
    LV_FLEX_ALIGN_CENTER
  );
  lv_obj_set_style_pad_row(s_clock.analog_temp_container, 8, 0);

  s_clock.analog_temp_label = wac_ui_create_label(
    s_clock.analog_temp_container, &lv_font_temp_105, color_temp(), LV_TEXT_ALIGN_CENTER, "--"
  );
  lv_obj_set_width(s_clock.analog_temp_label, LV_PCT(100));
  lv_obj_set_height(s_clock.analog_temp_label, ANALOG_TEMP_LABEL_HEIGHT);
  lv_obj_set_style_text_letter_space(s_clock.analog_temp_label, -2, 0);
  lv_obj_set_style_transform_width(s_clock.analog_temp_label, ANALOG_TEMP_LABEL_EXT_DRAW_MARGIN, 0);
  lv_obj_set_style_transform_height(s_clock.analog_temp_label, ANALOG_TEMP_LABEL_EXT_DRAW_MARGIN, 0);

  s_clock.analog_high_low_label = wac_ui_create_label(
    s_clock.analog_temp_container,
    &lv_font_montserrat_24,
    wac_ui_color_text_primary(),
    LV_TEXT_ALIGN_CENTER,
    "Please wait"
  );
  lv_obj_set_width(s_clock.analog_high_low_label, LV_PCT(100));
  lv_obj_set_style_text_letter_space(s_clock.analog_high_low_label, 1, 0);

  s_clock.analog_hour_asset = lv_image_create(s_clock.analog_face);
  lv_image_set_src(s_clock.analog_hour_asset, &analog_hour_hand);
  lv_image_set_pivot(s_clock.analog_hour_asset, ANALOG_HOUR_PIVOT_X, ANALOG_HOUR_PIVOT_Y);
  lv_image_set_antialias(s_clock.analog_hour_asset, false);
  lv_obj_set_pos(s_clock.analog_hour_asset, ANALOG_HOUR_ASSET_X, ANALOG_HOUR_ASSET_Y);

  s_clock.analog_minute_asset = lv_image_create(s_clock.analog_face);
  lv_image_set_src(s_clock.analog_minute_asset, &analog_minute_hand);
  lv_image_set_pivot(s_clock.analog_minute_asset, ANALOG_MINUTE_PIVOT_X, ANALOG_MINUTE_PIVOT_Y);
  lv_image_set_antialias(s_clock.analog_minute_asset, false);
  lv_obj_set_pos(s_clock.analog_minute_asset, ANALOG_MINUTE_ASSET_X, ANALOG_MINUTE_ASSET_Y);

  s_clock.second_hand = lv_image_create(s_clock.analog_face);
  lv_image_set_src(s_clock.second_hand, &analog_second_hand);
  lv_image_set_pivot(s_clock.second_hand, ANALOG_SECOND_PIVOT_X, ANALOG_SECOND_PIVOT_Y);
  lv_image_set_antialias(s_clock.second_hand, false);
  lv_obj_set_pos(s_clock.second_hand, ANALOG_SECOND_ASSET_X, ANALOG_SECOND_ASSET_Y);

  s_clock.analog_center_cap = lv_image_create(s_clock.analog_face);
  lv_image_set_src(s_clock.analog_center_cap, &analog_center_cap);
  lv_obj_set_pos(s_clock.analog_center_cap, ANALOG_CENTER_CAP_X, ANALOG_CENTER_CAP_Y);

  log_analog_hand_state("hour", s_clock.analog_hour_asset, &analog_hour_hand, ANALOG_HOUR_PIVOT_X, ANALOG_HOUR_PIVOT_Y);
  log_analog_hand_state(
    "minute",
    s_clock.analog_minute_asset,
    &analog_minute_hand,
    ANALOG_MINUTE_PIVOT_X,
    ANALOG_MINUTE_PIVOT_Y
  );
  log_analog_hand_state(
    "second",
    s_clock.second_hand,
    &analog_second_hand,
    ANALOG_SECOND_PIVOT_X,
    ANALOG_SECOND_PIVOT_Y
  );
}

void wac_clock_build_digital(lv_obj_t *parent)
{
  s_clock.digital_view = lv_obj_create(parent);
  lv_obj_set_size(s_clock.digital_view, LV_PCT(100), LV_PCT(100));
  /* See analog_view: opaque, deliberate backing behind the stage image. */
  lv_obj_set_style_bg_color(s_clock.digital_view, wac_ui_color_bg(), 0);
  lv_obj_set_style_bg_opa(s_clock.digital_view, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_clock.digital_view, 0, 0);
  lv_obj_set_style_pad_all(s_clock.digital_view, 0, 0);
  lv_obj_set_scrollbar_mode(s_clock.digital_view, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(s_clock.digital_view, LV_OBJ_FLAG_HIDDEN);

  s_clock.digital_stage_image = lv_image_create(s_clock.digital_view);
  lv_image_set_src(s_clock.digital_stage_image, &digital_stage_day);
  lv_obj_align(s_clock.digital_stage_image, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t *digital_face = lv_obj_create(s_clock.digital_view);
  lv_obj_set_size(digital_face, FACE_CONTENT_SIZE, FACE_CONTENT_SIZE);
  lv_obj_align(digital_face, LV_ALIGN_CENTER, 0, 0);
  wac_ui_clear_container_chrome(digital_face);
  lv_obj_add_flag(digital_face, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  s_clock.digital_edge_indicator = lv_image_create(s_clock.digital_view);
  lv_image_set_src(s_clock.digital_edge_indicator, &analog_edge_indicator);
  lv_obj_align(s_clock.digital_edge_indicator, LV_ALIGN_TOP_LEFT, ANALOG_EDGE_INDICATOR_X, ANALOG_EDGE_INDICATOR_Y);
  lv_obj_add_flag(s_clock.digital_edge_indicator, LV_OBJ_FLAG_HIDDEN);
  s_clock.digital_weather_indicator = create_weather_indicator(s_clock.digital_view);

  s_clock.digital_day_label = wac_ui_create_label(
    digital_face, &lv_font_montserrat_32, color_digital_accent(), LV_TEXT_ALIGN_CENTER, ""
  );
  lv_obj_set_width(s_clock.digital_day_label, LV_PCT(100));
  lv_obj_align(s_clock.digital_day_label, LV_ALIGN_TOP_MID, 0, DIGITAL_DAY_TOP);
  lv_obj_set_style_text_letter_space(s_clock.digital_day_label, 1, 0);

  s_clock.digital_date_label = wac_ui_create_label(
    digital_face, &lv_font_montserrat_24, lv_color_hex(0xbce4ff), LV_TEXT_ALIGN_CENTER, ""
  );
  lv_obj_set_width(s_clock.digital_date_label, LV_PCT(100));
  lv_obj_align(s_clock.digital_date_label, LV_ALIGN_TOP_MID, 0, DIGITAL_DATE_TOP);
  lv_obj_set_style_text_letter_space(s_clock.digital_date_label, 2, 0);

  /* Day uses the generated font at native size. Night reuses it with a
   * measured transform and explicit draw margins rather than another asset. */
  s_clock.digital_time_label = wac_ui_create_label(
    digital_face, &lv_font_clock_time_144, wac_ui_color_text_primary(), LV_TEXT_ALIGN_CENTER, "--:--"
  );
  lv_obj_set_width(s_clock.digital_time_label, LV_SIZE_CONTENT);
  lv_obj_set_style_text_letter_space(s_clock.digital_time_label, -2, 0);
  lv_obj_set_style_transform_width(s_clock.digital_time_label, 400, 0);
  lv_obj_set_style_transform_height(s_clock.digital_time_label, 200, 0);

  s_clock.digital_meridiem_label = wac_ui_create_label(
    digital_face, &lv_font_montserrat_24, color_accent(), LV_TEXT_ALIGN_CENTER, ""
  );
  lv_obj_set_width(s_clock.digital_meridiem_label, LV_SIZE_CONTENT);
  lv_obj_set_style_text_letter_space(s_clock.digital_meridiem_label, 2, 0);
  layout_digital_time_group(false);

  s_clock.digital_divider_top = lv_obj_create(digital_face);
  wac_ui_clear_container_chrome(s_clock.digital_divider_top);
  lv_obj_set_size(s_clock.digital_divider_top, DIGITAL_DIVIDER_WIDTH, 1);
  lv_obj_align(s_clock.digital_divider_top, LV_ALIGN_TOP_MID, 0, DIGITAL_DIVIDER_TOP);
  lv_obj_set_style_bg_color(s_clock.digital_divider_top, lv_color_hex(0x82cbff), 0);
  lv_obj_set_style_bg_opa(s_clock.digital_divider_top, LV_OPA_50, 0);

  s_clock.digital_current_panel = lv_obj_create(digital_face);
  wac_ui_clear_container_chrome(s_clock.digital_current_panel);
  lv_obj_set_size(s_clock.digital_current_panel, DIGITAL_CURRENT_WIDTH, DIGITAL_CURRENT_HEIGHT);
  lv_obj_align(s_clock.digital_current_panel, LV_ALIGN_TOP_MID, 0, DIGITAL_CURRENT_TOP);
  lv_obj_add_flag(s_clock.digital_current_panel, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  /* current_copy is intentionally positioned to overhang this panel's right
   * edge by ~70px (left 400px, width 270px inside a 600px box).
   * OVERFLOW_VISIBLE alone doesn't widen the clip region LVGL passes
   * to children -- only ext_draw_size does, which only grows when
   * transform_width/height is set (see lv_obj_calculate_ext_draw_size). */
  lv_obj_set_style_transform_width(s_clock.digital_current_panel, 100, 0);
  lv_obj_set_style_transform_height(s_clock.digital_current_panel, 40, 0);

  /* Temperature keeps its native-size generated font. */
  s_clock.digital_temp_label = wac_ui_create_label(
    s_clock.digital_current_panel, &lv_font_temp_102, wac_ui_color_text_primary(), LV_TEXT_ALIGN_LEFT, "--"
  );
  lv_obj_set_width(s_clock.digital_temp_label, 200);
  lv_obj_align(s_clock.digital_temp_label, LV_ALIGN_LEFT_MID, DIGITAL_CURRENT_TEMP_LEFT, 0);
  lv_obj_set_style_text_letter_space(s_clock.digital_temp_label, -2, 0);

  s_clock.digital_current_icon = lv_image_create(s_clock.digital_current_panel);
  lv_image_set_src(s_clock.digital_current_icon, weather_icon_image_for_name(weather_icon_fallback_name()));
  lv_obj_set_size(s_clock.digital_current_icon, DIGITAL_CURRENT_ICON_SIZE, DIGITAL_CURRENT_ICON_SIZE);
  lv_image_set_inner_align(s_clock.digital_current_icon, LV_IMAGE_ALIGN_CONTAIN);
  lv_obj_set_style_opa(s_clock.digital_current_icon, LV_OPA_COVER, 0);
  lv_obj_align(s_clock.digital_current_icon, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(s_clock.digital_current_icon, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  lv_obj_t *current_copy = lv_obj_create(s_clock.digital_current_panel);
  wac_ui_clear_container_chrome(current_copy);
  lv_obj_set_size(current_copy, DIGITAL_CURRENT_COPY_WIDTH, LV_SIZE_CONTENT);
  lv_obj_align(current_copy, LV_ALIGN_LEFT_MID, DIGITAL_CURRENT_COPY_LEFT, 0);
  /* The summary label wraps to 2 lines when the condition text is long
   * ("Mostly Sunny" etc). Its LV_SIZE_CONTENT height is computed against
   * the initial placeholder text at build time; when a runtime weather
   * update swaps in a longer string that newly needs wrapping, this
   * container's height can stay sized for one line and clip the wrapped
   * second line — same clipping pattern as the temp label and forecast
   * rows elsewhere in this file. */
  lv_obj_add_flag(current_copy, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_style_transform_width(current_copy, 40, 0);
  lv_obj_set_style_transform_height(current_copy, 40, 0);
  lv_obj_set_layout(current_copy, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(current_copy, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(current_copy, 3, 0);

  s_clock.digital_summary_label = wac_ui_create_label(
    current_copy, &lv_font_montserrat_32, wac_ui_color_text_primary(), LV_TEXT_ALIGN_LEFT, "Loading Weather"
  );
  lv_obj_set_width(s_clock.digital_summary_label, LV_PCT(100));
  lv_obj_set_height(s_clock.digital_summary_label, LV_SIZE_CONTENT);
  lv_label_set_long_mode(s_clock.digital_summary_label, LV_LABEL_LONG_WRAP);
  s_clock.digital_high_low_label = wac_ui_create_label(
    current_copy, &lv_font_montserrat_24, color_digital_accent(), LV_TEXT_ALIGN_LEFT, "Please wait"
  );
  lv_obj_set_width(s_clock.digital_high_low_label, LV_PCT(100));

  s_clock.digital_divider_bottom = lv_obj_create(digital_face);
  wac_ui_clear_container_chrome(s_clock.digital_divider_bottom);
  lv_obj_set_size(s_clock.digital_divider_bottom, DIGITAL_DIVIDER_WIDTH, 1);
  lv_obj_align(s_clock.digital_divider_bottom, LV_ALIGN_TOP_MID, 0, DIGITAL_DIVIDER_BOTTOM);
  lv_obj_set_style_bg_color(s_clock.digital_divider_bottom, lv_color_hex(0x82cbff), 0);
  lv_obj_set_style_bg_opa(s_clock.digital_divider_bottom, LV_OPA_50, 0);

  s_clock.digital_forecast_container = lv_obj_create(digital_face);
  wac_ui_clear_container_chrome(s_clock.digital_forecast_container);
  lv_obj_set_size(s_clock.digital_forecast_container, DIGITAL_FORECAST_WIDTH, 126);
  lv_obj_align(s_clock.digital_forecast_container, LV_ALIGN_TOP_MID, 0, DIGITAL_FORECAST_TOP);
  lv_obj_add_flag(s_clock.digital_forecast_container, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_layout(s_clock.digital_forecast_container, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(s_clock.digital_forecast_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(
    s_clock.digital_forecast_container,
    LV_FLEX_ALIGN_SPACE_BETWEEN,
    LV_FLEX_ALIGN_START,
    LV_FLEX_ALIGN_START
  );

  for (int i = 0; i < DAY_COUNT; ++i) {
    lv_obj_t *item = lv_obj_create(s_clock.digital_forecast_container);
    wac_ui_clear_container_chrome(item);
    lv_obj_set_size(item, 94, 120);
    lv_obj_set_layout(item, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(item, 0, 0);

    s_clock.digital_forecast_day_labels[i] = wac_ui_create_label(
      item, &lv_font_montserrat_24, color_digital_accent(), LV_TEXT_ALIGN_CENTER, "--"
    );
    lv_obj_set_width(s_clock.digital_forecast_day_labels[i], LV_PCT(100));

    s_clock.digital_forecast_icons[i] = lv_image_create(item);
    lv_image_set_src(s_clock.digital_forecast_icons[i], weather_icon_image_for_name(weather_icon_fallback_name()));
    lv_obj_set_size(s_clock.digital_forecast_icons[i], DIGITAL_FORECAST_ICON_SIZE, DIGITAL_FORECAST_ICON_SIZE);
    lv_image_set_inner_align(s_clock.digital_forecast_icons[i], LV_IMAGE_ALIGN_CONTAIN);
    lv_obj_set_style_opa(s_clock.digital_forecast_icons[i], LV_OPA_COVER, 0);

    s_clock.digital_forecast_temp_labels[i] = wac_ui_create_label(
      item, &lv_font_montserrat_24, wac_ui_color_text_primary(), LV_TEXT_ALIGN_CENTER, "--"
    );
    lv_obj_set_width(s_clock.digital_forecast_temp_labels[i], LV_PCT(100));
  }
}

void wac_clock_build_backup(lv_obj_t *parent, lv_event_cb_t open_cb, lv_event_cb_t dismiss_cb)
{
  s_clock.backup_indicator = lv_button_create(parent);
  wac_ui_clear_container_chrome(s_clock.backup_indicator);
  lv_obj_set_size(s_clock.backup_indicator, 88, 80);
  lv_obj_set_style_radius(s_clock.backup_indicator, 24, 0);
  lv_obj_set_style_text_font(s_clock.backup_indicator, &lv_font_montserrat_32, 0);
  lv_obj_t *icon = lv_label_create(s_clock.backup_indicator);
  lv_label_set_text(icon, LV_SYMBOL_WARNING);
  lv_obj_center(icon);
  lv_obj_add_event_cb(s_clock.backup_indicator, open_cb, LV_EVENT_CLICKED, NULL);

  s_clock.backup_overlay = lv_obj_create(parent);
  wac_ui_clear_container_chrome(s_clock.backup_overlay);
  lv_obj_set_size(s_clock.backup_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(s_clock.backup_overlay, 69, 0);
  lv_obj_remove_flag(s_clock.backup_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_clock.backup_overlay, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
  s_clock.backup_card = lv_obj_create(s_clock.backup_overlay);
  wac_ui_clear_container_chrome(s_clock.backup_card);
  lv_obj_remove_flag(s_clock.backup_card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(s_clock.backup_card, 140, 318);
  lv_obj_set_size(s_clock.backup_card, 520, 290);
  lv_obj_set_style_bg_opa(s_clock.backup_card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_clock.backup_card, 1, 0);
  lv_obj_set_style_radius(s_clock.backup_card, 32, 0);
  lv_obj_set_style_shadow_width(s_clock.backup_card, 40, 0);
  lv_obj_set_style_shadow_ofs_y(s_clock.backup_card, 12, 0);
  lv_obj_set_style_shadow_opa(s_clock.backup_card, 85, 0);
  s_clock.backup_title = wac_ui_create_label(s_clock.backup_card, &lv_font_montserrat_32,
    wac_ui_color_text_primary(), LV_TEXT_ALIGN_CENTER, "");
  lv_obj_set_pos(s_clock.backup_title, 29, 39);
  lv_obj_set_width(s_clock.backup_title, 460);
  s_clock.backup_detail = wac_ui_create_label(s_clock.backup_card, &lv_font_montserrat_28,
    wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, "");
  lv_obj_set_pos(s_clock.backup_detail, 29, 95);
  lv_obj_set_width(s_clock.backup_detail, 460);
  s_clock.backup_error = wac_ui_create_label(s_clock.backup_card, &lv_font_montserrat_18,
    wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, "");
  lv_obj_set_pos(s_clock.backup_error, 19, 131);
  lv_obj_set_width(s_clock.backup_error, 480);
  lv_obj_add_flag(s_clock.backup_error, LV_OBJ_FLAG_HIDDEN);
  s_clock.backup_dismiss = lv_button_create(s_clock.backup_card);
  wac_ui_clear_container_chrome(s_clock.backup_dismiss);
  lv_obj_set_pos(s_clock.backup_dismiss, 84, 161);
  lv_obj_set_size(s_clock.backup_dismiss, 350, 96);
  lv_obj_set_style_radius(s_clock.backup_dismiss, 24, 0);
  lv_obj_set_style_bg_opa(s_clock.backup_dismiss, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_clock.backup_dismiss, 1, 0);
  lv_obj_set_style_text_font(s_clock.backup_dismiss, &lv_font_montserrat_36, 0);
  lv_obj_t *label = lv_label_create(s_clock.backup_dismiss);
  lv_label_set_text(label, "DISMISS");
  lv_obj_center(label);
  lv_obj_add_event_cb(s_clock.backup_dismiss, dismiss_cb, LV_EVENT_CLICKED, NULL);
}

static void update_analog_clock(const struct tm *local_time)
{
  static int s_logged_angle_updates = 0;

  if (local_time == NULL || s_clock.second_hand == NULL) {
    return;
  }

  double minute_angle = local_time->tm_min * 6.0;
  double hour_angle = ((local_time->tm_hour % 12) + (local_time->tm_min / 60.0)) * 30.0;
  double second_angle = local_time->tm_sec * 6.0;

  if (s_clock.analog_hour_asset != NULL) {
    lv_image_set_rotation(s_clock.analog_hour_asset, (int32_t) lround(hour_angle * 10.0));
  }

  if (s_clock.analog_minute_asset != NULL) {
    lv_image_set_rotation(s_clock.analog_minute_asset, (int32_t) lround(minute_angle * 10.0));
  }

  lv_image_set_rotation(s_clock.second_hand, (int32_t) lround(second_angle * 10.0));

  if (s_logged_angle_updates < 3) {
    ESP_LOGI(
      TAG,
      "Analog hand angles update %d: hour=%.1f minute=%.1f second=%.1f",
      s_logged_angle_updates + 1,
      hour_angle,
      minute_angle,
      second_angle
    );
    s_logged_angle_updates++;
  }
}

static void update_day_labels(const struct tm *local_time)
{
  if (local_time == NULL || local_time->tm_wday < 0 || local_time->tm_wday > 6
      || local_time->tm_mon < 0 || local_time->tm_mon > 11) {
    return;
  }

  char analog_date[16];
  char digital_date[16];
  char digital_date_upper[16];
  snprintf(analog_date, sizeof(analog_date), "%s %d", MONTH_NAMES[local_time->tm_mon], local_time->tm_mday);
  snprintf(
    digital_date,
    sizeof(digital_date),
    "%s %d",
    MONTH_NAMES[local_time->tm_mon],
    local_time->tm_mday
  );
  wac_ui_copy_uppercase_string(digital_date_upper, sizeof(digital_date_upper), digital_date);

  wac_ui_set_label_text(s_clock.analog_day_label, WEEKDAY_NAMES[local_time->tm_wday]);
  wac_ui_set_label_text(s_clock.analog_date_label, analog_date);
  wac_ui_set_label_text(s_clock.digital_day_label, WEEKDAY_NAMES_UPPER[local_time->tm_wday]);
  wac_ui_set_label_text(s_clock.digital_date_label, digital_date_upper);
}

static void update_digital_clock(const device_config_t *config, const struct tm *local_time)
{
  if (config == NULL || local_time == NULL) {
    return;
  }

  char time_text[16];
  int hour = local_time->tm_hour;
  bool show_meridiem = false;

  if (wac_ui_strings_equal_ignore_case(config->time_format, "24")) {
    snprintf(time_text, sizeof(time_text), "%02d:%02d", hour, local_time->tm_min);
    wac_ui_set_label_text(s_clock.digital_meridiem_label, "");
  } else {
    bool afternoon = hour >= 12;
    int display_hour = hour % 12;
    if (display_hour == 0) {
      display_hour = 12;
    }

    if (config->leading_zero_12h) {
      snprintf(time_text, sizeof(time_text), "%02d:%02d", display_hour, local_time->tm_min);
    } else {
      snprintf(time_text, sizeof(time_text), "%d:%02d", display_hour, local_time->tm_min);
    }

    wac_ui_set_label_text(s_clock.digital_meridiem_label, afternoon ? "PM" : "AM");
    show_meridiem = true;
  }

  wac_ui_set_label_text(s_clock.digital_time_label, time_text);
  layout_digital_time_group(show_meridiem);
}

static void apply_weather_indicator_state(lv_obj_t *indicator, bool alert, bool night)
{
  if (indicator == NULL) {
    return;
  }

  if (!alert) {
    stop_indicator_pulse(indicator);
    lv_obj_add_flag(indicator, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_obj_clear_flag(indicator, LV_OBJ_FLAG_HIDDEN);
  /* A8 asset: the recolor IS its color. Night shift keeps it red-toned. */
  lv_obj_set_style_image_recolor(indicator, night ? wac_ui_color_night_alert() : lv_color_hex(0xffb040), 0);
  lv_obj_set_style_image_recolor_opa(indicator, LV_OPA_COVER, 0);
  start_indicator_pulse(indicator);
}

static lv_obj_t *create_weather_indicator(lv_obj_t *view)
{
  lv_obj_t *indicator = lv_image_create(view);
  lv_image_set_src(indicator, &conditions_edge_indicator);
  lv_obj_align(indicator, LV_ALIGN_TOP_LEFT, CONDITIONS_EDGE_INDICATOR_X, CONDITIONS_EDGE_INDICATOR_Y);
  lv_obj_add_flag(indicator, LV_OBJ_FLAG_HIDDEN);
  return indicator;
}

void wac_clock_build_unavailable(lv_obj_t *parent)
{
  s_clock.unavailable_view = lv_obj_create(parent);
  wac_ui_clear_container_chrome(s_clock.unavailable_view);
  lv_obj_set_size(s_clock.unavailable_view, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(s_clock.unavailable_view, wac_ui_color_bg(), 0);
  lv_obj_set_style_bg_opa(s_clock.unavailable_view, LV_OPA_COVER, 0);
  lv_obj_add_flag(s_clock.unavailable_view, LV_OBJ_FLAG_HIDDEN);
  s_clock.unavailable_stage = lv_image_create(s_clock.unavailable_view);
  lv_image_set_src(s_clock.unavailable_stage, &round_stage_day);
  lv_obj_center(s_clock.unavailable_stage);
  s_clock.unavailable_title = wac_ui_create_label(s_clock.unavailable_view,
    &lv_font_montserrat_36, wac_ui_color_text_primary(), LV_TEXT_ALIGN_CENTER, "Time unavailable");
  lv_obj_set_pos(s_clock.unavailable_title, 140, 140);
  lv_obj_set_width(s_clock.unavailable_title, 520);
  s_clock.unavailable_time = wac_ui_create_label(s_clock.unavailable_view,
    &lv_font_clock_time_144, wac_ui_color_text_primary(), LV_TEXT_ALIGN_CENTER, "--:--");
  lv_obj_set_pos(s_clock.unavailable_time, 140, 228);
  lv_obj_set_width(s_clock.unavailable_time, 520);
  lv_obj_set_style_text_letter_space(s_clock.unavailable_time, -2, 0);
  s_clock.unavailable_sync = wac_ui_create_label(s_clock.unavailable_view,
    &lv_font_montserrat_36, wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, "Waiting for time sync");
  lv_obj_set_pos(s_clock.unavailable_sync, 120, 405);
  lv_obj_set_width(s_clock.unavailable_sync, 560);
}

void wac_clock_reset(void)
{
  memset(&s_clock, 0, sizeof(s_clock));
}

void wac_clock_set_visible(bool analog, bool digital, bool usable)
{
  if (analog && usable) lv_obj_remove_flag(s_clock.analog_view, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_clock.analog_view, LV_OBJ_FLAG_HIDDEN);
  if (digital && usable) lv_obj_remove_flag(s_clock.digital_view, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_clock.digital_view, LV_OBJ_FLAG_HIDDEN);
  if ((analog || digital) && !usable) lv_obj_remove_flag(s_clock.unavailable_view, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_clock.unavailable_view, LV_OBJ_FLAG_HIDDEN);
  /* Keep the existing edge objects on whichever surface is displayed. */
  lv_obj_t *analog_parent = analog && !usable ? s_clock.unavailable_view : s_clock.analog_view;
  lv_obj_t *digital_parent = digital && !usable ? s_clock.unavailable_view : s_clock.digital_view;
  lv_obj_t *edges[] = {s_clock.analog_edge_indicator, s_clock.analog_weather_indicator,
    s_clock.digital_edge_indicator, s_clock.digital_weather_indicator};
  for (size_t i = 0; i < 4; ++i) {
    lv_obj_t *parent = i < 2 ? analog_parent : digital_parent;
    if (lv_obj_get_parent(edges[i]) != parent) lv_obj_set_parent(edges[i], parent);
  }
}

void wac_clock_apply_night(bool active)
{
  if (s_clock.unavailable_stage) {
    lv_image_set_src(s_clock.unavailable_stage, active ? &round_stage_night : &round_stage_day);
    lv_color_t primary = active ? wac_ui_color_night_primary() : wac_ui_color_text_primary();
    lv_color_t muted = active ? wac_ui_color_night_muted() : wac_ui_color_text_muted();
    lv_obj_set_style_text_color(s_clock.unavailable_title, primary, 0);
    lv_obj_set_style_text_color(s_clock.unavailable_time, primary, 0);
    lv_obj_set_style_text_color(s_clock.unavailable_sync, muted, 0);
  }
  apply_analog_night_state(active);
  apply_digital_night_state(active);
  if (s_clock.clock_status_label) {
    lv_color_t color = active ? wac_ui_color_night_muted() : wac_ui_color_text_muted();
    lv_obj_set_style_text_color(s_clock.clock_status_label, color, 0);
    lv_obj_set_style_text_color(s_clock.clock_status_icon, color, 0);
  }
}

void wac_clock_update_time(const device_config_t *config, const struct tm *local_time, bool night)
{
  if (local_time) {
    update_day_labels(local_time);
    update_digital_clock(config, local_time);
    if (night) layout_digital_night_time(!wac_ui_strings_equal_ignore_case(config->time_format, "24"));
    update_analog_clock(local_time);
  }
  lv_obj_t *hands[] = {s_clock.second_hand, s_clock.analog_hour_asset, s_clock.analog_minute_asset};
  for (size_t i = 0; i < sizeof(hands) / sizeof(hands[0]); ++i) {
    if (!hands[i]) continue;
    if (local_time) lv_obj_remove_flag(hands[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(hands[i], LV_OBJ_FLAG_HIDDEN);
  }
}

void wac_clock_update_weather(const app_weather_snapshot_t *weather)
{
  if (weather == NULL) {
    const char *summary = "Loading Weather";
    const char *pending_temp = "--";
    const char *pending_range = "Please wait";
    const lv_image_dsc_t *fallback_icon = weather_icon_image_for_name(weather_icon_fallback_name());
    wac_ui_set_label_text(s_clock.analog_temp_label, pending_temp);
    wac_ui_set_label_text(s_clock.analog_high_low_label, pending_range);
    if (s_clock.analog_weather_icon != NULL) {
      lv_image_set_src(s_clock.analog_weather_icon, fallback_icon);
    }

    wac_ui_set_label_text(s_clock.digital_temp_label, pending_temp);
    wac_ui_set_label_text(s_clock.digital_summary_label, summary);
    wac_ui_set_label_text(s_clock.digital_high_low_label, pending_range);
    if (s_clock.digital_current_icon != NULL) {
      lv_image_set_src(s_clock.digital_current_icon, fallback_icon);
    }

    for (int i = 0; i < DAY_COUNT; ++i) {
      wac_ui_set_label_text(s_clock.digital_forecast_day_labels[i], "--");
      wac_ui_set_label_text(s_clock.digital_forecast_temp_labels[i], pending_temp);
      if (s_clock.digital_forecast_icons[i] != NULL) {
        lv_image_set_src(s_clock.digital_forecast_icons[i], fallback_icon);
      }
    }

    return;
  }
  char high_low[24];
  char temps[24];

  snprintf(temps, sizeof(temps), "%d°", weather->temp);
  wac_ui_set_label_text(s_clock.analog_temp_label, temps);
  format_high_low(high_low, sizeof(high_low), weather->high, weather->low);
  wac_ui_set_label_text(s_clock.analog_high_low_label, high_low);
  if (s_clock.analog_weather_icon != NULL) {
    lv_image_set_src(s_clock.analog_weather_icon, weather_icon_image_for_name(weather_icon_name_for_snapshot(weather)));
  }

  wac_ui_set_label_text(s_clock.digital_temp_label, temps);
  if (s_clock.digital_current_icon != NULL) {
    lv_image_set_src(s_clock.digital_current_icon, weather_icon_image_for_name(weather->icon_name));
  }
  wac_ui_set_label_text(s_clock.digital_summary_label, weather->summary);
  wac_ui_set_label_text(s_clock.digital_high_low_label, high_low);

  for (int i = 0; i < DAY_COUNT; ++i) {
    char day_temp[16];
    char forecast_day[8];
    snprintf(day_temp, sizeof(day_temp), "%d°", weather->forecast[i].high);
    wac_ui_copy_uppercase_string(forecast_day, sizeof(forecast_day), wac_ui_forecast_weekday(&weather->forecast[i]));
    wac_ui_set_label_text(s_clock.digital_forecast_day_labels[i], forecast_day);
    wac_ui_set_label_text(s_clock.digital_forecast_temp_labels[i], day_temp);
    if (s_clock.digital_forecast_icons[i] != NULL) {
      lv_image_set_src(
        s_clock.digital_forecast_icons[i],
        weather_icon_image_for_name(weather->forecast[i].icon_name)
      );
    }
  }

}

void wac_clock_update_indicators(bool unread, bool important, bool weather_alert, bool night)
{
  apply_edge_indicator_state(s_clock.analog_edge_indicator, unread, important, night);
  apply_edge_indicator_state(s_clock.digital_edge_indicator, unread, important, night);
  apply_weather_indicator_state(s_clock.analog_weather_indicator, weather_alert, night);
  apply_weather_indicator_state(s_clock.digital_weather_indicator, weather_alert, night);
}

void wac_clock_build_status(lv_obj_t *parent)
{
  s_clock.clock_status_container = lv_obj_create(parent);
  wac_ui_clear_container_chrome(s_clock.clock_status_container);
  lv_obj_set_layout(s_clock.clock_status_container, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(s_clock.clock_status_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_clock.clock_status_container, LV_FLEX_ALIGN_CENTER,
    LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(s_clock.clock_status_container, 10, 0);
  lv_obj_remove_flag(s_clock.clock_status_container, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  s_clock.clock_status_icon = wac_ui_create_label(s_clock.clock_status_container,
    &lv_font_montserrat_28, wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, LV_SYMBOL_WARNING);
  s_clock.clock_status_label = wac_ui_create_label(s_clock.clock_status_container,
    &lv_font_montserrat_28, wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, "");
  lv_obj_add_flag(s_clock.clock_status_container, LV_OBJ_FLAG_HIDDEN);
}

void wac_clock_update_status(const char *text, bool analog)
{
  wac_ui_set_label_text(s_clock.clock_status_label, text);
  lv_obj_set_pos(s_clock.clock_status_container, analog ? 220 : 170, analog ? 220 : 642);
  lv_obj_set_size(s_clock.clock_status_container, analog ? 360 : 460, analog ? 72 : 80);
  if (text && text[0]) lv_obj_remove_flag(s_clock.clock_status_container, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_clock.clock_status_container, LV_OBJ_FLAG_HIDDEN);
}

void wac_clock_update_backup(bool home, bool analog, bool usable,
  wac_time_warning_t warning, bool open, bool dismiss_pending, const char *error, bool night)
{
  if (!s_clock.backup_indicator) return;
  bool pending = warning != WAC_TIME_WARNING_NONE;
  if (home && pending) lv_obj_remove_flag(s_clock.backup_indicator, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_clock.backup_indicator, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(s_clock.backup_indicator, 464, !usable ? 56
    : (analog ? 126 : 98));
  if (home && pending && open)
    lv_obj_remove_flag(s_clock.backup_overlay, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_clock.backup_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_color_t primary = night ? wac_ui_color_night_primary() : wac_ui_color_text_primary();
  lv_color_t muted = night ? wac_ui_color_night_muted() : wac_ui_color_text_muted();
  lv_color_t line = lv_color_hex(night ? 0x633737 : 0x253a57);
  lv_obj_set_style_text_color(s_clock.backup_indicator, lv_color_hex(night ? 0xd29191 : 0xefb965), 0);
  lv_obj_set_style_bg_color(s_clock.backup_overlay, lv_color_hex(night ? 0x18090c : 0x07101c), 0);
  lv_obj_set_style_bg_color(s_clock.backup_card, lv_color_hex(night ? 0x261315 : 0x0f1c2d), 0);
  lv_obj_set_style_border_color(s_clock.backup_card, line, 0);
  lv_obj_set_style_text_color(s_clock.backup_title, primary, 0);
  lv_obj_set_style_text_color(s_clock.backup_detail, muted, 0);
  lv_obj_set_style_text_color(s_clock.backup_error, muted, 0);
  lv_obj_set_style_text_color(s_clock.backup_dismiss, primary, 0);
  lv_obj_set_style_bg_color(s_clock.backup_dismiss, lv_color_hex(night ? 0x321c20 : 0x17263b), 0);
  lv_obj_set_style_border_color(s_clock.backup_dismiss, line, 0);
  bool lost = warning == WAC_TIME_WARNING_LOST_TIME;
  wac_ui_set_label_text(s_clock.backup_title, lost ? "Backup Clock Lost Time" : "Backup Clock Unavailable");
  wac_ui_set_label_text(s_clock.backup_detail, lost ? "Check Battery." : "Check Connection.");
  if (dismiss_pending) lv_obj_add_state(s_clock.backup_dismiss, LV_STATE_DISABLED);
  else lv_obj_remove_state(s_clock.backup_dismiss, LV_STATE_DISABLED);
  wac_ui_set_label_text(s_clock.backup_error, error);
  if (error && error[0]) lv_obj_remove_flag(s_clock.backup_error, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_clock.backup_error, LV_OBJ_FLAG_HIDDEN);
}
