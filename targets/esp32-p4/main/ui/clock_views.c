#include "ui_private.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include "assets/wac_icons.h"
#include "assets/wac_fonts.h"

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
#define DIGITAL_DATE_TOP 128
/* 150 was correct for the old scaled lv_font_montserrat_48 (line_height 52,
 * base_line 9, scaled 3x -> effective line_height 156 / base_line 27, baseline
 * at 150+156-27=279). lv_font_clock_time_144 -- generated from digits, ':'
 * and '-', with no descenders/tall accents to reserve room for -- reports a
 * much tighter line_height 104 / base_line 2, landing the baseline at
 * 150+104-2=252: 27px higher, which read as the time floating away from the
 * PM/divider line below it. +27 restores the original baseline position. */
#define DIGITAL_TIME_TOP 177
#define DIGITAL_TIME_WIDTH 620
#define DIGITAL_MERIDIEM_TOP 257
/* Approved appearance is documented in mockups/alarm-design.html. The
 * constants below define the ESP32-P4 layout values used by this file. */
#define DIGITAL_TIME_GAP 14
#define DIGITAL_DIVIDER_WIDTH 500
#define DIGITAL_DIVIDER_TOP 300
#define DIGITAL_DIVIDER_BOTTOM 486
#define DIGITAL_CURRENT_TOP 320
#define DIGITAL_CURRENT_WIDTH 600
#define DIGITAL_CURRENT_HEIGHT 150
#define DIGITAL_CURRENT_TEMP_LEFT 35
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
  lv_obj_t *clock_status_sync_icon;
  lv_obj_t *analog_weather_waiting;
  lv_obj_t *digital_weather_waiting;
  bool weather_available;
  bool weather_notice;
  bool night;
  bool night_applied;
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
  lv_obj_t *machine_indicator, *machine_icon;
  lv_obj_t *backup_indicator;
  lv_obj_t *backup_overlay;
  lv_obj_t *backup_card;
  lv_obj_t *backup_title;
  lv_obj_t *backup_detail;
  lv_obj_t *backup_dismiss;
  lv_obj_t *alarm_view;
  lv_obj_t *alarm_stage;
  lv_obj_t *alarm_day;
  lv_obj_t *alarm_date;
  lv_obj_t *alarm_time;
  lv_obj_t *alarm_meridiem;
  lv_obj_t *alarm_divider;
  lv_obj_t *alarm_snooze;
  lv_obj_t *alarm_snooze_title;
  lv_obj_t *alarm_snooze_duration;
  lv_obj_t *alarm_dismiss;
  lv_obj_t *alarm_dismiss_title;
  lv_obj_t *alarm_slide;
  lv_obj_t *alarm_slide_label;
  lv_obj_t *alarm_slide_handle;
  lv_obj_t *alarm_slide_chevrons[2];
  bool alarm_slide_available;
  bool alarm_slide_tracking;
  lv_indev_t *alarm_slide_indev;
  int32_t alarm_slide_start_x;
  int32_t alarm_slide_start_offset;
  lv_opa_t alarm_slide_label_target;
  lv_obj_t *alarm_footer;
  lv_obj_t *alarm_snoozed_title;
  lv_obj_t *alarm_countdown;
  lv_obj_t *alarm_deadline;
  lv_obj_t *alarm_badge;
  lv_obj_t *alarm_badge_icon;
  lv_obj_t *alarm_badge_text;
  lv_obj_t *alarm_badge_close;
  lv_obj_t *unavailable_alarm_caption;
  lv_obj_t *unavailable_alarm_button;
  lv_obj_t *unavailable_alarm_button_text;
  lv_point_precise_t analog_tick_points[ANALOG_TICK_COUNT][2];
} clock_view_objects_t;

static clock_view_objects_t s_clock;

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

    /* Small bounds let LVGL skip this tick in unrelated drawing strips.
     * The line widget supplies the stroke/rounded-end drawing margin. */
    int32_t left = LV_MIN(outer_x, inner_x);
    int32_t top = LV_MIN(outer_y, inner_y);
    s_clock.analog_tick_points[i][0].x = outer_x - left;
    s_clock.analog_tick_points[i][0].y = outer_y - top;
    s_clock.analog_tick_points[i][1].x = inner_x - left;
    s_clock.analog_tick_points[i][1].y = inner_y - top;

    s_clock.analog_tick_lines[i] = lv_line_create(parent);
    lv_obj_set_size(s_clock.analog_tick_lines[i], abs(outer_x - inner_x) + 1,
      abs(outer_y - inner_y) + 1);
    lv_obj_set_pos(s_clock.analog_tick_lines[i], left, top);
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

static int32_t scale_dimension(int32_t value, int32_t scale)
{
  return (value * scale + 128) / 256;
}

static void layout_digital_time_group(bool show_meridiem)
{
  if (s_clock.digital_time_label == NULL || s_clock.digital_meridiem_label == NULL) {
    return;
  }

  if (lv_obj_get_style_transform_scale_x_safe(s_clock.digital_time_label, 0) != LV_SCALE_NONE
      || lv_obj_get_style_transform_scale_y_safe(s_clock.digital_time_label, 0) != LV_SCALE_NONE)
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

/* Use the approved 250px size except for padded 12-hour time, which keeps its
 * existing widest-digit fit. Neither size depends on the current minute. */
static void layout_digital_night_time(const device_config_t *config, bool show_meridiem)
{
  double max_size = 250.0;
  if (show_meridiem && config->leading_zero_12h) {
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
    const char *periods[] = {"AM", "PM"};
    for (size_t i = 0; i < 2; ++i) {
      lv_text_get_size(&size, periods[i], &lv_font_montserrat_24, 2, 0,
        LV_COORD_MAX, LV_TEXT_FLAG_NONE);
      if (size.x > period_width) period_width = size.x;
    }
    max_size = 752.0 / hypot(widest / 144.0, 1.0);
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
  if (lv_obj_get_style_transform_pivot_x(s_clock.digital_time_label, 0) != 0)
    lv_obj_set_style_transform_pivot_x(s_clock.digital_time_label, 0, 0);
  if (lv_obj_get_style_transform_pivot_y(s_clock.digital_time_label, 0) != 0)
    lv_obj_set_style_transform_pivot_y(s_clock.digital_time_label, 0, 0);
  if (lv_obj_get_style_transform_scale_x_safe(s_clock.digital_time_label, 0) != scale
      || lv_obj_get_style_transform_scale_y_safe(s_clock.digital_time_label, 0) != scale)
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
  snprintf(out, out_size, "H: %d°  L: %d°", high, low);
}

/* These flags describe rendered data and appearance, not fetch/retry policy. */
static void apply_clock_weather_visibility(void)
{
  lv_obj_t *weather[] = {s_clock.analog_temp_container, s_clock.digital_current_panel,
    s_clock.digital_forecast_container};
  for (size_t i = 0; i < sizeof(weather) / sizeof(weather[0]); ++i) {
    if (!weather[i]) continue;
    if (s_clock.night || !s_clock.weather_available) lv_obj_add_flag(weather[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(weather[i], LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_t *waiting[] = {s_clock.analog_weather_waiting, s_clock.digital_weather_waiting};
  for (size_t i = 0; i < sizeof(waiting) / sizeof(waiting[0]); ++i) {
    if (!waiting[i]) continue;
    if (s_clock.night || (s_clock.weather_available && !s_clock.weather_notice)) lv_obj_add_flag(waiting[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(waiting[i], LV_OBJ_FLAG_HIDDEN);
  }
  if (s_clock.analog_weather_icon) {
    if (!s_clock.weather_available) lv_obj_add_flag(s_clock.analog_weather_icon, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(s_clock.analog_weather_icon, LV_OBJ_FLAG_HIDDEN);
  }
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

  if (s_clock.digital_divider_top != NULL) {
    lv_obj_set_style_bg_color(s_clock.digital_divider_top, lv_color_hex(active ? 0xe1b6b6 : 0x93d1ff), 0);
    if (active) {
      lv_obj_add_flag(s_clock.digital_divider_top, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(s_clock.digital_divider_top, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (s_clock.digital_divider_bottom != NULL) {
    lv_obj_set_style_bg_color(s_clock.digital_divider_bottom, lv_color_hex(active ? 0xe1b6b6 : 0x93d1ff), 0);
    if (active) {
      lv_obj_add_flag(s_clock.digital_divider_bottom, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(s_clock.digital_divider_bottom, LV_OBJ_FLAG_HIDDEN);
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

  s_clock.analog_weather_waiting = wac_ui_create_label(s_clock.analog_view,
    &wac_font_settings_28, wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, "Waiting for weather…");
  lv_obj_set_pos(s_clock.analog_weather_waiting, 150, 550);
  lv_obj_set_width(s_clock.analog_weather_waiting, 500);

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

  s_clock.digital_weather_waiting = wac_ui_create_label(s_clock.digital_view,
    &wac_font_settings_28, wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, "Waiting for weather…");
  lv_obj_set_pos(s_clock.digital_weather_waiting, 150, 390);
  lv_obj_set_width(s_clock.digital_weather_waiting, 500);

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
  lv_obj_set_style_bg_color(s_clock.digital_divider_top, lv_color_hex(0x93d1ff), 0);
  lv_obj_set_style_bg_opa(s_clock.digital_divider_top, (lv_opa_t)89, 0);

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
  lv_obj_set_style_bg_color(s_clock.digital_divider_bottom, lv_color_hex(0x93d1ff), 0);
  lv_obj_set_style_bg_opa(s_clock.digital_divider_bottom, (lv_opa_t)89, 0);

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
  s_clock.backup_dismiss = lv_button_create(s_clock.backup_card);
  wac_ui_clear_container_chrome(s_clock.backup_dismiss);
  lv_obj_set_pos(s_clock.backup_dismiss, 84, 161);
  lv_obj_set_size(s_clock.backup_dismiss, 350, 96);
  lv_obj_set_style_radius(s_clock.backup_dismiss, 32, 0);
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
}

static void update_day_labels(const struct tm *local_time)
{
  if (local_time == NULL || local_time->tm_wday < 0 || local_time->tm_wday > 6
      || local_time->tm_mon < 0 || local_time->tm_mon > 11) {
    return;
  }

  char analog_date[16];
  char digital_date[16];
  snprintf(analog_date, sizeof(analog_date), "%s %d", MONTH_NAMES[local_time->tm_mon], local_time->tm_mday);
  snprintf(
    digital_date,
    sizeof(digital_date),
    "%s %d",
    MONTH_NAMES[local_time->tm_mon],
    local_time->tm_mday
  );

  wac_ui_set_label_text(s_clock.analog_day_label, WEEKDAY_NAMES[local_time->tm_wday]);
  wac_ui_set_label_text(s_clock.analog_date_label, analog_date);
  wac_ui_set_label_text(s_clock.digital_day_label, WEEKDAY_NAMES[local_time->tm_wday]);
  wac_ui_set_label_text(s_clock.digital_date_label, digital_date);
}

static void update_digital_clock(const device_config_t *config, const struct tm *local_time, bool night)
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
  if (night) layout_digital_night_time(config, show_meridiem);
  else layout_digital_time_group(show_meridiem);
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
  if (s_clock.night_applied && s_clock.night == active) {
    apply_clock_weather_visibility();
    return;
  }
  s_clock.night_applied = true;
  s_clock.night = active;
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
  apply_clock_weather_visibility();
  if (s_clock.clock_status_label) {
    lv_color_t color = active ? wac_ui_color_night_muted() : wac_ui_color_text_muted();
    lv_obj_set_style_text_color(s_clock.clock_status_label, color, 0);
    lv_obj_set_style_text_color(s_clock.clock_status_icon, color, 0);
    lv_obj_set_style_image_recolor(s_clock.clock_status_sync_icon, color, 0);
  }
}

void wac_clock_update_time(const device_config_t *config, const struct tm *local_time, bool night)
{
  if (local_time) {
    update_day_labels(local_time);
    update_digital_clock(config, local_time, night);
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
  s_clock.weather_available = weather != NULL;
  apply_clock_weather_visibility();
  if (weather == NULL) return;
  char high_low[24];
  char temps[24];

  snprintf(temps, sizeof(temps), "%d°", weather->temp);
  wac_ui_set_label_text(s_clock.analog_temp_label, temps);
  format_high_low(high_low, sizeof(high_low), weather->high, weather->low);
  wac_ui_set_label_text(s_clock.analog_high_low_label, high_low);
  if (s_clock.analog_weather_icon != NULL) {
    wac_ui_set_image_asset(s_clock.analog_weather_icon, weather_icon_image_for_name(weather->icon_name));
  }

  wac_ui_set_label_text(s_clock.digital_temp_label, temps);
  if (s_clock.digital_current_icon != NULL) {
    wac_ui_set_image_asset(s_clock.digital_current_icon, weather_icon_image_for_name(weather->icon_name));
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
      wac_ui_set_image_asset(
        s_clock.digital_forecast_icons[i],
        weather_icon_image_for_name(weather->forecast[i].icon_name)
      );
    }
  }

}

void wac_clock_update_weather_notice(const char *notice)
{
  s_clock.weather_notice = notice && notice[0];
  lv_obj_t *labels[] = {s_clock.analog_weather_waiting, s_clock.digital_weather_waiting};
  bool retained = s_clock.weather_available;
  for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); ++i) {
    if (!labels[i]) continue;
    wac_ui_set_label_text(labels[i], retained ? notice : "Waiting for weather…");
    lv_obj_set_style_text_font(labels[i], retained ? &lv_font_montserrat_22 : &wac_font_settings_28, 0);
    lv_obj_set_pos(labels[i], 150, retained ? (i == 0 ? 582 : 464) : (i == 0 ? 550 : 390));
    lv_obj_move_foreground(labels[i]);
  }
  apply_clock_weather_visibility();
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
  s_clock.clock_status_sync_icon = lv_image_create(s_clock.clock_status_container);
  lv_image_set_src(s_clock.clock_status_sync_icon, &wac_icon_clock);
  lv_obj_set_style_image_recolor(s_clock.clock_status_sync_icon, wac_ui_color_text_muted(), 0);
  lv_obj_set_style_image_recolor_opa(s_clock.clock_status_sync_icon, LV_OPA_COVER, 0);
  lv_obj_add_flag(s_clock.clock_status_sync_icon, LV_OBJ_FLAG_HIDDEN);
  s_clock.clock_status_label = wac_ui_create_label(s_clock.clock_status_container,
    &lv_font_montserrat_28, wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, "");
  lv_obj_add_flag(s_clock.clock_status_container, LV_OBJ_FLAG_HIDDEN);
}

void wac_clock_update_status(const char *text, bool analog, bool waiting_for_sync)
{
  if (waiting_for_sync) {
    lv_obj_add_flag(s_clock.clock_status_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_clock.clock_status_sync_icon, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_remove_flag(s_clock.clock_status_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_clock.clock_status_sync_icon, LV_OBJ_FLAG_HIDDEN);
  }
  wac_ui_set_label_text(s_clock.clock_status_label, text);
  lv_obj_set_pos(s_clock.clock_status_container, analog ? 220 : 170, analog ? 220 : 642);
  lv_obj_set_size(s_clock.clock_status_container, analog ? 360 : 460, analog ? 72 : 80);
  if (text && text[0]) lv_obj_remove_flag(s_clock.clock_status_container, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_clock.clock_status_container, LV_OBJ_FLAG_HIDDEN);
}

void wac_clock_update_backup(bool home, bool analog, bool usable,
  wac_time_warning_t warning, bool open, bool dismiss_pending, bool night)
{
  if (!s_clock.backup_indicator) return;
  bool pending = warning != WAC_TIME_WARNING_NONE;
  if (home && pending) lv_obj_remove_flag(s_clock.backup_indicator, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_clock.backup_indicator, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(s_clock.backup_indicator, 488, !usable ? 56
    : (analog ? 152 : 92));
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
  lv_obj_set_style_text_color(s_clock.backup_dismiss, primary, 0);
  lv_obj_set_style_bg_color(s_clock.backup_dismiss, lv_color_hex(night ? 0x321c20 : 0x17263b), 0);
  lv_obj_set_style_border_color(s_clock.backup_dismiss, line, 0);
  bool lost = warning == WAC_TIME_WARNING_LOST_TIME;
  wac_ui_set_label_text(s_clock.backup_title, lost ? "Backup Clock Lost Time" : "Backup Clock Unavailable");
  wac_ui_set_label_text(s_clock.backup_detail, lost ? "Check Battery." : "Check Connection.");
  if (dismiss_pending) lv_obj_add_state(s_clock.backup_dismiss, LV_STATE_DISABLED);
  else lv_obj_remove_state(s_clock.backup_dismiss, LV_STATE_DISABLED);

}

static void set_alarm_visible(lv_obj_t *obj, bool visible)
{
  if (visible) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *create_alarm_label(lv_obj_t *parent, const lv_font_t *font,
  const char *text, int x, int y, int width)
{
  lv_obj_t *label = wac_ui_create_label(parent, font, wac_ui_color_text_primary(),
    LV_TEXT_ALIGN_CENTER, text);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_width(label, width);
  return label;
}

static lv_obj_t *create_alarm_button(lv_obj_t *parent, int x, int y, int width,
  int height, lv_event_cb_t callback, void *data)
{
  lv_obj_t *button = lv_button_create(parent);
  wac_ui_clear_container_chrome(button);
  lv_obj_remove_flag(button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);
  lv_obj_set_style_radius(button, 32, 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_opa(button, LV_OPA_40, LV_STATE_DISABLED);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, data);
  return button;
}

static void style_alarm_button(lv_obj_t *button, bool night, bool selected)
{
  lv_obj_set_style_bg_color(button, lv_color_hex(selected
    ? (night ? 0x643437 : 0x245289) : (night ? 0x321c20 : 0x17263b)), 0);
  lv_obj_set_style_border_color(button, lv_color_hex(selected
    ? (night ? 0xe1b6b6 : 0x93d1ff) : (night ? 0x633737 : 0x253a57)), 0);
}

/* The owner supplies the intended schedule date/minute. Formatting that date
 * must not replace it with the UTC instant's local date during a DST jump. */
static bool alarm_date_calendar(int32_t date, struct tm *out)
{
  int month = (date / 100) % 100;
  int day = date % 100;
  if (date < 20000101 || month < 1 || month > 12 || day < 1 || day > 31) return false;
  memset(out, 0, sizeof(*out));
  out->tm_year = date / 10000 - 1900;
  out->tm_mon = month - 1;
  out->tm_mday = day;
  out->tm_hour = 12;
  out->tm_isdst = -1;
  return mktime(out) != (time_t)-1;
}

static int32_t alarm_calendar_date(const struct tm *calendar)
{
  return (calendar->tm_year + 1900) * 10000 + (calendar->tm_mon + 1) * 100 + calendar->tm_mday;
}

static void format_alarm_time(char *text, size_t size, const device_config_t *config,
  unsigned minute, bool include_period)
{
  unsigned hour = minute / 60;
  if (wac_ui_strings_equal_ignore_case(config->time_format, "24")) {
    snprintf(text, size, "%02u:%02u", hour, minute % 60);
  } else {
    unsigned display_hour = hour % 12 ? hour % 12 : 12;
    snprintf(text, size, config->leading_zero_12h ? "%02u:%02u%s" : "%u:%02u%s",
      display_hour, minute % 60, include_period ? (hour >= 12 ? " PM" : " AM") : "");
  }
}

/* Keep the gesture local to its handle. PRESS_LOCK supplies pointer capture,
 * while navigation explicitly cancels it before a different occurrence appears. */
#define ALARM_SLIDE_TRAVEL 436

static void alarm_slide_position(void *handle, int32_t offset)
{
  lv_obj_set_x(handle, 8 + offset);
}

static void alarm_slide_opacity(void *label, int32_t opacity)
{
  lv_obj_set_style_opa(label, opacity, 0);
}

static void alarm_slide_fade(lv_opa_t opacity)
{
  if (s_clock.alarm_slide_label_target == opacity) return;
  s_clock.alarm_slide_label_target = opacity;
  lv_anim_delete(s_clock.alarm_slide_label, alarm_slide_opacity);
  lv_anim_t animation;
  lv_anim_init(&animation);
  lv_anim_set_var(&animation, s_clock.alarm_slide_label);
  lv_anim_set_exec_cb(&animation, alarm_slide_opacity);
  lv_anim_set_values(&animation, lv_obj_get_style_opa(s_clock.alarm_slide_label, 0), opacity);
  lv_anim_set_time(&animation, 120);
  lv_anim_start(&animation);
}

void wac_clock_cancel_alarm_slide(bool animate)
{
  s_clock.alarm_slide_tracking = false;
  s_clock.alarm_slide_indev = NULL;
  if (!s_clock.alarm_slide_handle) return;
  lv_anim_delete(s_clock.alarm_slide_handle, alarm_slide_position);
  int32_t offset = lv_obj_get_x(s_clock.alarm_slide_handle) - 8;
  if (animate && offset > 0) {
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, s_clock.alarm_slide_handle);
    lv_anim_set_exec_cb(&animation, alarm_slide_position);
    lv_anim_set_values(&animation, offset, 0);
    lv_anim_set_time(&animation, 220);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
    alarm_slide_fade(LV_OPA_COVER);
  } else {
    alarm_slide_position(s_clock.alarm_slide_handle, 0);
    lv_anim_delete(s_clock.alarm_slide_label, alarm_slide_opacity);
    s_clock.alarm_slide_label_target = LV_OPA_COVER;
    alarm_slide_opacity(s_clock.alarm_slide_label, LV_OPA_COVER);
  }
}

static void alarm_slide_event_cb(lv_event_t *event)
{
  lv_event_code_t code = lv_event_get_code(event);
  lv_indev_t *indev = lv_event_get_indev(event);
  if (code == LV_EVENT_PRESS_LOST || code == LV_EVENT_INDEV_RESET) {
    wac_clock_cancel_alarm_slide(true);
    return;
  }
  if (!indev || !s_clock.alarm_slide_available) return;
  lv_point_t point;
  lv_indev_get_point(indev, &point);
  if (code == LV_EVENT_PRESSED) {
    lv_anim_delete(s_clock.alarm_slide_handle, alarm_slide_position);
    s_clock.alarm_slide_tracking = true;
    s_clock.alarm_slide_indev = indev;
    s_clock.alarm_slide_start_x = point.x;
    s_clock.alarm_slide_start_offset = lv_obj_get_x(s_clock.alarm_slide_handle) - 8;
  } else if (s_clock.alarm_slide_tracking && indev == s_clock.alarm_slide_indev
      && (code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED)) {
    int32_t offset = LV_CLAMP(0, s_clock.alarm_slide_start_offset
      + point.x - s_clock.alarm_slide_start_x, ALARM_SLIDE_TRAVEL);
    alarm_slide_position(s_clock.alarm_slide_handle, offset);
    alarm_slide_fade(LV_MAX(0, LV_OPA_COVER
      - offset * LV_OPA_COVER * 18 / (ALARM_SLIDE_TRAVEL * 10)));
    if (code == LV_EVENT_RELEASED) {
      s_clock.alarm_slide_tracking = false;
      s_clock.alarm_slide_indev = NULL;
      if (offset * 100 >= ALARM_SLIDE_TRAVEL * 94) {
        /* READY is private to this control; ordinary clicks have no action. */
        lv_obj_send_event(s_clock.alarm_slide, LV_EVENT_READY, NULL);
      } else wac_clock_cancel_alarm_slide(true);
    }
  }
}

void wac_clock_build_alarm(lv_obj_t *parent, lv_event_cb_t action_cb,
  lv_event_cb_t badge_cb, lv_event_cb_t open_alarm_cb, lv_event_cb_t slide_cb)
{
  s_clock.alarm_view = lv_obj_create(parent);
  wac_ui_clear_container_chrome(s_clock.alarm_view);
  lv_obj_set_size(s_clock.alarm_view, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(s_clock.alarm_view, LV_OPA_COVER, 0);
  lv_obj_remove_flag(s_clock.alarm_view, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(s_clock.alarm_view, LV_OBJ_FLAG_HIDDEN);
  s_clock.alarm_stage = lv_image_create(s_clock.alarm_view);
  lv_image_set_src(s_clock.alarm_stage, &digital_stage_day);
  lv_obj_center(s_clock.alarm_stage);
  s_clock.alarm_day = create_alarm_label(s_clock.alarm_view,
    &lv_font_montserrat_32, "", 10, 90, 780);
  lv_obj_set_style_text_letter_space(s_clock.alarm_day, 1, 0);
  s_clock.alarm_date = create_alarm_label(s_clock.alarm_view,
    &lv_font_montserrat_24, "", 10, 125, 780);
  lv_obj_set_style_text_letter_space(s_clock.alarm_date, 2, 0);
  /* The existing digit-only font has a tight line box. As on the normal Day
   * clock, add 27px to the CSS wrapper top to preserve its baseline. */
  s_clock.alarm_time = create_alarm_label(s_clock.alarm_view,
    &lv_font_clock_time_144, "--:--", 90, 193, LV_SIZE_CONTENT);
  lv_obj_set_style_text_letter_space(s_clock.alarm_time, -2, 0);
  s_clock.alarm_meridiem = create_alarm_label(s_clock.alarm_view,
    &lv_font_montserrat_24, "", 0, 273, LV_SIZE_CONTENT);
  lv_obj_set_style_text_letter_space(s_clock.alarm_meridiem, 2, 0);
  s_clock.alarm_divider = lv_obj_create(s_clock.alarm_view);
  wac_ui_clear_container_chrome(s_clock.alarm_divider);
  lv_obj_remove_flag(s_clock.alarm_divider, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(s_clock.alarm_divider, 150, 310);
  lv_obj_set_size(s_clock.alarm_divider, 500, 1);
  lv_obj_set_style_bg_opa(s_clock.alarm_divider, 89, 0);

  s_clock.alarm_snooze = create_alarm_button(s_clock.alarm_view, 130, 350, 540, 164,
    action_cb, (void *)(intptr_t) APP_ALARM_SNOOZE);
  lv_obj_set_layout(s_clock.alarm_snooze, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(s_clock.alarm_snooze, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_clock.alarm_snooze, LV_FLEX_ALIGN_CENTER,
    LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(s_clock.alarm_snooze, 6, 0);
  s_clock.alarm_snooze_title = create_alarm_label(s_clock.alarm_snooze,
    &lv_font_montserrat_48, "SNOOZE", 0, 0, LV_SIZE_CONTENT);
  s_clock.alarm_snooze_duration = create_alarm_label(s_clock.alarm_snooze,
    &wac_font_settings_28, "", 0, 0, LV_SIZE_CONTENT);
  s_clock.alarm_dismiss = create_alarm_button(s_clock.alarm_view, 225, 548, 350, 96,
    action_cb, (void *)(intptr_t) APP_ALARM_DISMISS);
  s_clock.alarm_dismiss_title = create_alarm_label(s_clock.alarm_dismiss,
    &lv_font_montserrat_36, "DISMISS", 0, 0, LV_SIZE_CONTENT);
  lv_obj_center(s_clock.alarm_dismiss_title);
  s_clock.alarm_slide = lv_obj_create(s_clock.alarm_view);
  wac_ui_clear_container_chrome(s_clock.alarm_slide);
  lv_obj_remove_flag(s_clock.alarm_slide, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(s_clock.alarm_slide, 130, 548);
  lv_obj_set_size(s_clock.alarm_slide, 540, 104);
  lv_obj_set_style_radius(s_clock.alarm_slide, 56, 0);
  lv_obj_set_style_bg_opa(s_clock.alarm_slide, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_clock.alarm_slide, 1, 0);
  /* The reference border is inset decoration, without a content inset. */
  lv_obj_set_style_pad_all(s_clock.alarm_slide, -1, 0);
  /* Scale the existing 32px font to 29.625px around the caption's center. */
  s_clock.alarm_slide_label = create_alarm_label(s_clock.alarm_slide,
    &lv_font_montserrat_32, "SLIDE TO DISMISS", 0, 0, 540);
  lv_obj_center(s_clock.alarm_slide_label);
  lv_obj_set_style_transform_scale(s_clock.alarm_slide_label, 237, 0);
  lv_obj_set_style_transform_pivot_x(s_clock.alarm_slide_label, 270, 0);
  lv_obj_set_style_transform_pivot_y(s_clock.alarm_slide_label,
    lv_font_montserrat_32.line_height / 2, 0);
  s_clock.alarm_slide_label_target = LV_OPA_COVER;
  s_clock.alarm_slide_handle = lv_obj_create(s_clock.alarm_slide);
  wac_ui_clear_container_chrome(s_clock.alarm_slide_handle);
  lv_obj_remove_flag(s_clock.alarm_slide_handle, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_clock.alarm_slide_handle, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_set_pos(s_clock.alarm_slide_handle, 8, 8);
  lv_obj_set_size(s_clock.alarm_slide_handle, 88, 88);
  lv_obj_set_style_radius(s_clock.alarm_slide_handle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(s_clock.alarm_slide_handle, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_clock.alarm_slide_handle, 1, 0);
  lv_obj_set_style_pad_all(s_clock.alarm_slide_handle, -1, 0);
  lv_obj_add_event_cb(s_clock.alarm_slide_handle, alarm_slide_event_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(s_clock.alarm_slide_handle, alarm_slide_event_cb, LV_EVENT_PRESSING, NULL);
  lv_obj_add_event_cb(s_clock.alarm_slide_handle, alarm_slide_event_cb, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(s_clock.alarm_slide_handle, alarm_slide_event_cb, LV_EVENT_PRESS_LOST, NULL);
  lv_obj_add_event_cb(s_clock.alarm_slide_handle, alarm_slide_event_cb, LV_EVENT_INDEV_RESET, NULL);
  lv_obj_add_event_cb(s_clock.alarm_slide, slide_cb, LV_EVENT_READY, NULL);
  /* Lucide chevrons-right, in its centered 36px box. Existing single-chevron
   * artwork has different proportions; two LVGL lines retain the approved shape. */
  static const lv_point_precise_t chevrons[2][3] = {
    {{9, 11}, {17, 18}, {9, 26}},
    {{20, 11}, {27, 18}, {20, 26}}
  };
  for (unsigned i = 0; i < 2; ++i) {
    s_clock.alarm_slide_chevrons[i] = lv_line_create(s_clock.alarm_slide_handle);
    lv_line_set_points(s_clock.alarm_slide_chevrons[i], chevrons[i], 3);
    lv_obj_set_pos(s_clock.alarm_slide_chevrons[i], 26, 26);
    lv_obj_set_style_line_width(s_clock.alarm_slide_chevrons[i], 3, 0);
    lv_obj_set_style_line_rounded(s_clock.alarm_slide_chevrons[i], true, 0);
  }
  s_clock.alarm_footer = create_alarm_label(s_clock.alarm_view,
    &wac_font_settings_28, "", 200, 679, 400);
  s_clock.alarm_snoozed_title = create_alarm_label(s_clock.alarm_view,
    &lv_font_montserrat_32, "SNOOZED", 160, 350, 480);
  lv_obj_set_style_text_letter_space(s_clock.alarm_snoozed_title, 1, 0);
  s_clock.alarm_countdown = create_alarm_label(s_clock.alarm_view,
    &wac_font_settings_96, "--:--", 140, 392, 520);
  s_clock.alarm_deadline = create_alarm_label(s_clock.alarm_view,
    &wac_font_settings_28, "", 150, 501, 500);

  s_clock.alarm_badge = create_alarm_button(parent, 170, 642, 460, 80, badge_cb, NULL);
  lv_obj_set_style_bg_opa(s_clock.alarm_badge, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_clock.alarm_badge, 0, 0);
  lv_obj_set_layout(s_clock.alarm_badge, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(s_clock.alarm_badge, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_clock.alarm_badge, LV_FLEX_ALIGN_CENTER,
    LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_left(s_clock.alarm_badge, 20, 0);
  lv_obj_set_style_pad_right(s_clock.alarm_badge, 20, 0);
  s_clock.alarm_badge_icon = lv_image_create(s_clock.alarm_badge);
  s_clock.alarm_badge_text = create_alarm_label(s_clock.alarm_badge,
    &wac_font_settings_28, "", 0, 0, LV_SIZE_CONTENT);
  s_clock.alarm_badge_close = lv_image_create(s_clock.alarm_badge);
  lv_image_set_src(s_clock.alarm_badge_close, &wac_icon_close);
  lv_obj_add_flag(s_clock.alarm_badge, LV_OBJ_FLAG_HIDDEN);

  s_clock.unavailable_alarm_caption = create_alarm_label(s_clock.unavailable_view,
    &wac_font_settings_28, "", 155, 472, 490);
  s_clock.unavailable_alarm_button = create_alarm_button(parent,
    230, 574, 340, 104, open_alarm_cb, NULL);
  lv_obj_set_style_radius(s_clock.unavailable_alarm_button, 24, 0);
  s_clock.unavailable_alarm_button_text = create_alarm_label(s_clock.unavailable_alarm_button,
    &wac_font_settings_28, "Alarm Settings", 0, 0, LV_SIZE_CONTENT);
  lv_obj_center(s_clock.unavailable_alarm_button_text);
}

void wac_clock_update_alarm(const device_config_t *config,
  const wac_time_snapshot_t *clock, const wac_alarm_snapshot_t *alarm,
  wac_clock_alarm_surface_t surface, bool analog, bool night,
  bool caption_blocked, bool pending)
{
  if (!s_clock.alarm_view || !config || !clock || !alarm) return;
  bool ringing = surface == WAC_CLOCK_ALARM_RINGING;
  bool snoozed = surface == WAC_CLOCK_ALARM_SNOOZED;
  bool dismissed = surface == WAC_CLOCK_ALARM_DISMISSED;
  bool home = surface == WAC_CLOCK_ALARM_HOME;
  bool missed = alarm->missed.valid;
  lv_color_t primary = night ? wac_ui_color_night_primary() : wac_ui_color_text_primary();
  lv_color_t muted = night ? wac_ui_color_night_muted() : wac_ui_color_text_muted();
  lv_color_t accent = lv_color_hex(night ? 0xe1b6b6 : 0x93d1ff);
  bool slide_available = ringing && !pending;
  if (!slide_available && s_clock.alarm_slide_available) wac_clock_cancel_alarm_slide(false);
  s_clock.alarm_slide_available = slide_available;
  if (slide_available) lv_obj_remove_state(s_clock.alarm_slide_handle, LV_STATE_DISABLED);
  else lv_obj_add_state(s_clock.alarm_slide_handle, LV_STATE_DISABLED);
  set_alarm_visible(s_clock.alarm_view, ringing || snoozed || dismissed);
  set_alarm_visible(s_clock.alarm_snooze, ringing || dismissed);
  set_alarm_visible(s_clock.alarm_dismiss, snoozed);
  set_alarm_visible(s_clock.alarm_slide, ringing || dismissed);
  set_alarm_visible(s_clock.alarm_slide_handle, !dismissed);
  set_alarm_visible(s_clock.alarm_snoozed_title, snoozed);
  set_alarm_visible(s_clock.alarm_countdown, snoozed);
  set_alarm_visible(s_clock.alarm_deadline, snoozed);
  set_alarm_visible(s_clock.alarm_badge, home && !caption_blocked && (clock->usable || missed));
  set_alarm_visible(s_clock.unavailable_alarm_caption, home && !clock->usable);
  set_alarm_visible(s_clock.unavailable_alarm_button, home && !clock->usable);
  lv_obj_t *buttons[] = {s_clock.alarm_snooze, s_clock.alarm_dismiss,
    s_clock.alarm_badge, s_clock.unavailable_alarm_button};
  for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
    if (pending || dismissed) lv_obj_add_state(buttons[i], LV_STATE_DISABLED);
    else lv_obj_remove_state(buttons[i], LV_STATE_DISABLED);
  }
  style_alarm_button(s_clock.alarm_snooze, night, true);
  style_alarm_button(s_clock.alarm_dismiss, night, false);
  style_alarm_button(s_clock.alarm_slide, night, false);
  lv_obj_set_style_bg_color(s_clock.alarm_slide_handle,
    lv_color_hex(night ? 0x643437 : 0x304d70), 0);
  lv_obj_set_style_border_color(s_clock.alarm_slide_handle,
    lv_color_hex(night ? 0x633737 : 0x253a57), 0);
  for (unsigned i = 0; i < 2; ++i)
    lv_obj_set_style_line_color(s_clock.alarm_slide_chevrons[i], primary, 0);
  wac_ui_set_label_text(s_clock.alarm_slide_label, dismissed ? "DISMISSED" : "SLIDE TO DISMISS");
  lv_obj_set_style_text_color(s_clock.alarm_slide_label, dismissed ? primary : muted, 0);
  if (dismissed) {
    lv_anim_delete(s_clock.alarm_slide_label, alarm_slide_opacity);
    s_clock.alarm_slide_label_target = LV_OPA_COVER;
    alarm_slide_opacity(s_clock.alarm_slide_label, LV_OPA_COVER);
  }
  style_alarm_button(s_clock.unavailable_alarm_button, night, false);
  lv_obj_set_style_text_color(s_clock.unavailable_alarm_button_text, primary, 0);
  lv_obj_set_style_text_color(s_clock.unavailable_alarm_caption, muted, 0);
  wac_ui_set_label_text(s_clock.unavailable_alarm_caption, config->alarm_enabled ? "Alarm saved; will arm when time is available"
    : "Alarm off; saved settings are kept");
  lv_obj_set_pos(s_clock.unavailable_alarm_button, 230, missed ? 546 : 574);
  lv_obj_set_height(s_clock.unavailable_alarm_button, missed ? 72 : 104);

  if (ringing || snoozed || dismissed) {
    lv_obj_move_foreground(s_clock.alarm_view);
    lv_obj_set_style_bg_color(s_clock.alarm_view, lv_color_hex(night ? 0x18090c : 0x07101c), 0);
    wac_ui_set_image_asset(s_clock.alarm_stage, night ? &digital_stage_night : &digital_stage_day);
    lv_obj_t *primary_labels[] = {s_clock.alarm_time, s_clock.alarm_countdown,
      s_clock.alarm_snooze_title, s_clock.alarm_snooze_duration, s_clock.alarm_dismiss_title};
    for (size_t i = 0; i < sizeof(primary_labels) / sizeof(primary_labels[0]); ++i)
      lv_obj_set_style_text_color(primary_labels[i], primary, 0);
    lv_obj_set_style_text_color(s_clock.alarm_day, night ? primary : accent, 0);
    lv_obj_set_style_text_color(s_clock.alarm_date, night ? muted : lv_color_hex(0xbce4ff), 0);
    lv_obj_set_style_text_color(s_clock.alarm_meridiem, night ? muted : color_accent(), 0);
    lv_obj_set_style_text_color(s_clock.alarm_snoozed_title, accent, 0);
    lv_obj_set_style_text_color(s_clock.alarm_deadline, muted, 0);
    lv_obj_set_style_text_color(s_clock.alarm_footer, muted, 0);
    lv_obj_set_style_bg_color(s_clock.alarm_divider, accent, 0);
    struct tm scheduled;
    if (alarm->current.valid && alarm_date_calendar(alarm->current.local_date, &scheduled)) {
      char date[24];
      snprintf(date, sizeof(date), "%s %d", MONTH_NAMES[scheduled.tm_mon], scheduled.tm_mday);
      wac_ui_copy_uppercase_string(date, sizeof(date), date);
      wac_ui_set_label_text(s_clock.alarm_day, WEEKDAY_NAMES_UPPER[scheduled.tm_wday]);
      wac_ui_set_label_text(s_clock.alarm_date, date);
    } else {
      wac_ui_set_label_text(s_clock.alarm_day, "--");
      wac_ui_set_label_text(s_clock.alarm_date, "--");
    }
    char time_text[24];
    bool period = alarm->current.valid && !wac_ui_strings_equal_ignore_case(config->time_format, "24");
    if (alarm->current.valid) format_alarm_time(time_text, sizeof(time_text), config, alarm->current.minute, false);
    else snprintf(time_text, sizeof(time_text), "--:--");
    wac_ui_set_label_text(s_clock.alarm_time, time_text);
    wac_ui_set_label_text(s_clock.alarm_meridiem, alarm->current.minute >= 720 ? "PM" : "AM");
    set_alarm_visible(s_clock.alarm_meridiem, period);
    lv_obj_update_layout(s_clock.alarm_time);
    lv_obj_update_layout(s_clock.alarm_meridiem);
    int width = lv_obj_get_width(s_clock.alarm_time);
    int group_width = width + (period ? 14 + lv_obj_get_width(s_clock.alarm_meridiem) : 0);
    int left = 90 + (620 - group_width) / 2;
    lv_obj_set_pos(s_clock.alarm_time, left, 193);
    lv_obj_set_pos(s_clock.alarm_meridiem, left + width + 14, 273);
    char duration[24];
    snprintf(duration, sizeof(duration), "%u minutes", config->alarm_snooze_minutes);
    wac_ui_set_label_text(s_clock.alarm_snooze_duration, duration);
    char countdown[24] = "--:--";
    if (clock->usable) {
      int64_t seconds = alarm->snooze_deadline > clock->utc_now
        ? (int64_t)difftime(alarm->snooze_deadline, clock->utc_now) : 0;
      snprintf(countdown, sizeof(countdown), "%" PRId64 ":%02" PRId64, seconds / 60, seconds % 60);
    }
    wac_ui_set_label_text(s_clock.alarm_countdown, countdown);
    struct tm deadline;
    char again[64] = "Rings again when time is available";
    if (alarm->snooze_deadline > 0 && localtime_r(&alarm->snooze_deadline, &deadline)) {
      format_alarm_time(time_text, sizeof(time_text), config, deadline.tm_hour * 60 + deadline.tm_min, true);
      snprintf(again, sizeof(again), "Rings again at %s", time_text);
    }
    wac_ui_set_label_text(s_clock.alarm_deadline, again);
    lv_obj_set_pos(s_clock.alarm_footer, snoozed ? 150 : 200, 679);
    lv_obj_set_width(s_clock.alarm_footer, snoozed ? 500 : 400);
    wac_ui_set_label_text(s_clock.alarm_footer, snoozed ? "Alarm snoozed"
      : dismissed ? "Alarm dismissed" : "Alarm ringing");
  }

  if (home) {
    struct tm now;
    bool have_now = clock->usable && localtime_r(&clock->utc_now, &now);
    char summary[96];
    char time_text[24];
    const wac_alarm_occurrence_t *occurrence = missed ? &alarm->missed : &alarm->next;
    struct tm calendar;
    bool have_date = occurrence->valid && alarm_date_calendar(occurrence->local_date, &calendar);
    if (missed) {
      format_alarm_time(time_text, sizeof(time_text), config, alarm->missed.minute, true);
      if (have_date && (!have_now || occurrence->local_date < alarm_calendar_date(&now))) {
        snprintf(summary, sizeof(summary), "Missed Alarm · %.3s · %s", WEEKDAY_NAMES[calendar.tm_wday], time_text);
      } else snprintf(summary, sizeof(summary), "Missed Alarm · %s", time_text);
    } else if (alarm->phase == WAC_ALARM_SNOOZED && alarm->snooze_deadline > 0
        && localtime_r(&alarm->snooze_deadline, &calendar)) {
      format_alarm_time(time_text, sizeof(time_text), config, calendar.tm_hour * 60 + calendar.tm_min, true);
      snprintf(summary, sizeof(summary), "Snoozed · %s", time_text);
    } else if (!config->alarm_enabled) snprintf(summary, sizeof(summary), "Alarm off");
    else if (!have_now || !have_date) snprintf(summary, sizeof(summary), "Waiting for sync");
    else {
      const char *day = WEEKDAY_NAMES[calendar.tm_wday];
      char weekday[4];
      snprintf(weekday, sizeof(weekday), "%.3s", day);
      day = weekday;
      struct tm tomorrow = now;
      tomorrow.tm_mday++;
      tomorrow.tm_hour = 12;
      tomorrow.tm_isdst = -1;
      mktime(&tomorrow);
      if (occurrence->local_date == alarm_calendar_date(&now)) day = "Today";
      else if (occurrence->local_date == alarm_calendar_date(&tomorrow)) day = "Tomorrow";
      format_alarm_time(time_text, sizeof(time_text), config, occurrence->minute, true);
      snprintf(summary, sizeof(summary), "%s · %s", day, time_text);
    }
    wac_ui_set_label_text(s_clock.alarm_badge_text, summary);
    bool analog_row = analog && clock->usable;
    lv_obj_set_pos(s_clock.alarm_badge, missed ? 130 : analog_row ? 220 : 170,
      analog_row ? 220 : 642);
    lv_obj_set_size(s_clock.alarm_badge, missed ? 540 : analog_row ? 360 : 460,
      analog_row ? 72 : 80);
    int padding = analog_row && !missed ? 0 : 20;
    lv_obj_set_style_pad_left(s_clock.alarm_badge, padding, 0);
    lv_obj_set_style_pad_right(s_clock.alarm_badge, padding, 0);
    lv_obj_set_style_pad_column(s_clock.alarm_badge, missed || analog_row ? 10 : 14, 0);
    lv_obj_set_style_text_font(s_clock.alarm_badge_text,
      missed ? &wac_font_settings_26 : &wac_font_settings_28, 0);
    /* Use the normal analog row's full width to keep complete captions on one
     * line at the approved font size. */
    lv_point_t text_size;
    int gap = missed || analog_row ? 10 : 14;
    int text_width = (missed ? 540 : analog_row ? 360 : 460) - 2 * padding
      - (missed || analog_row ? 28 : 32) - gap - (missed ? 24 + gap : 0);
    lv_text_get_size(&text_size, lv_label_get_text(s_clock.alarm_badge_text),
      missed ? &wac_font_settings_26 : &wac_font_settings_28,
      0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_width(s_clock.alarm_badge_text, text_size.x > text_width ? text_width : LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(s_clock.alarm_badge_text, accent, 0);
    bool small = missed || analog_row;
    wac_ui_set_image_asset(s_clock.alarm_badge_icon, config->alarm_enabled || missed
      ? (small ? &wac_icon_alarm_clock_small : &wac_icon_alarm_clock)
      : (small ? &wac_icon_alarm_clock_off_small : &wac_icon_alarm_clock_off));
    lv_obj_t *icons[] = {s_clock.alarm_badge_icon, s_clock.alarm_badge_close};
    for (size_t i = 0; i < sizeof(icons) / sizeof(icons[0]); ++i) {
      lv_obj_set_style_image_recolor(icons[i], accent, 0);
      lv_obj_set_style_image_recolor_opa(icons[i], LV_OPA_COVER, 0);
    }
    set_alarm_visible(s_clock.alarm_badge_close, missed);
    lv_obj_move_foreground(s_clock.alarm_badge);
    if (!clock->usable) lv_obj_move_foreground(s_clock.unavailable_alarm_button);
    /* Home captions remain below the existing modal, regardless of build order. */
    if (s_clock.backup_overlay && !lv_obj_has_flag(s_clock.backup_overlay, LV_OBJ_FLAG_HIDDEN))
      lv_obj_move_foreground(s_clock.backup_overlay);
  }
}

void wac_clock_build_machine(lv_obj_t *parent, lv_event_cb_t open_cb)
{
  s_clock.machine_indicator = lv_button_create(parent);
  wac_ui_clear_container_chrome(s_clock.machine_indicator);
  lv_obj_set_size(s_clock.machine_indicator, 88, 80);
  lv_obj_remove_flag(s_clock.machine_indicator, LV_OBJ_FLAG_SCROLLABLE);
  s_clock.machine_icon = lv_image_create(s_clock.machine_indicator);
  lv_image_set_src(s_clock.machine_icon, &wac_icon_volume_off);
  lv_obj_center(s_clock.machine_icon);
  lv_obj_remove_flag(s_clock.machine_icon, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_clock.machine_indicator, open_cb, LV_EVENT_CLICKED, NULL);
}
void wac_clock_update_machine(bool home, bool analog, bool usable, bool playing, bool night)
{
  if (!s_clock.machine_indicator) return;
  if (home) lv_obj_remove_flag(s_clock.machine_indicator, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_clock.machine_indicator, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(s_clock.machine_indicator, 224, !usable ? 56 : analog ? 152 : 92);
  wac_ui_set_image_asset(s_clock.machine_icon, playing ? &wac_icon_volume_2 : &wac_icon_volume_off);
  lv_obj_set_style_image_recolor(s_clock.machine_icon, night ? wac_ui_color_night_muted() : wac_ui_color_text_muted(), 0);
  lv_obj_set_style_image_recolor_opa(s_clock.machine_icon, LV_OPA_COVER, 0);
}
