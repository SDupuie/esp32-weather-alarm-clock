#include "ui_private.h"
#include "src/misc/cache/instance/lv_image_cache.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/forecast_tomorrow_frame_day.h"
#include "assets/forecast_tomorrow_frame_night.h"
#include "assets/wac_fonts.h"
#include "assets/round_stage_day.h"
#include "assets/round_stage_night.h"
#include "assets/weather_icons.h"
#include "weather_icon_names.h"
#include "weather_client.h"
#include "assets/wac_icons.h"

#define FACE_CONTENT_SIZE 780
#define FORECAST_TOMORROW_FRAME_TOP 68
#define FORECAST_TOMORROW_LABEL_TOP 146
/* A 276px box at top 180 enlarged 1.34x around its center has visual center
 * 318 and grows to about 370px. This layout sets the box directly to 364px,
 * so top is 318-364/2=136 to keep that center; top 180 places it 44px lower. */
#define FORECAST_TOMORROW_ICON_TOP 136
#define FORECAST_TOMORROW_ICON_SIZE 364
#define FORECAST_TOMORROW_TEMPS_TOP 464
#define FORECAST_ROW_ICON_SIZE 90
#define FORECAST_ROW1_X 220
#define FORECAST_ROW1_Y 600
#define FORECAST_ROW2_X 330
#define FORECAST_ROW2_Y 640
#define FORECAST_ROW3_X 450
#define FORECAST_ROW3_Y 640
#define FORECAST_ROW4_X 560
#define FORECAST_ROW4_Y 600
#define FORECAST_ROW_COUNT 4

typedef struct {
  bool palette_applied;
  bool palette_night;
  bool palette_weather_valid;
  bool palette_important;
  bool radar_palette_applied;
  bool radar_palette_night;
  lv_obj_t *forecast_view;
  lv_obj_t *forecast_credit;
  lv_obj_t *radar_view;
  lv_obj_t *radar_backdrop;
  lv_obj_t *radar_image;
  lv_obj_t *radar_heading;
  lv_obj_t *radar_time;
  lv_obj_t *radar_unavailable;
  lv_obj_t *radar_marker;
  lv_obj_t *radar_legend;
  lv_obj_t *radar_legend_labels[3];
  lv_obj_t *radar_credit;
  lv_obj_t *radar_control;
  lv_obj_t *radar_control_icon;
  lv_image_dsc_t radar_source;
  uint32_t radar_generation;
  lv_obj_t *message_view;
  lv_obj_t *status_stack;
  lv_obj_t *weather_status_label;
  lv_obj_t *forecast_stage_image;
  lv_obj_t *forecast_tomorrow_frame;
  lv_obj_t *forecast_tomorrow_day_label;
  lv_obj_t *forecast_tomorrow_icon;
  lv_obj_t *forecast_updating_label;
  lv_obj_t *forecast_tomorrow_temps_label;
  lv_obj_t *forecast_row_day_labels[FORECAST_ROW_COUNT];
  lv_obj_t *forecast_row_icons[FORECAST_ROW_COUNT];
  lv_obj_t *forecast_row_temps_labels[FORECAST_ROW_COUNT];
  lv_obj_t *message_stage_image;
  lv_obj_t *message_face;
  lv_obj_t *message_title_label;
  lv_obj_t *message_card;
  lv_obj_t *message_text_label;
  lv_obj_t *message_meta_label;
  lv_obj_t *message_dismiss_label;
  lv_obj_t *message_empty_label;
} weather_views_t;

static weather_views_t s_weather;

/* Presentation cache only: service records and navigation remain with their
 * owners. Row callback data points to a full ID, never a pointer-sized cast. */
typedef struct {
  lv_obj_t *list, *detail, *subtitle, *rows, *checking;
  lv_obj_t *list_back, *notice, *notice_primary, *notice_secondary;
  lv_obj_t *detail_heading, *detail_content;
  lv_obj_t *list_track, *detail_track;
  weather_alert_row_t *row_data;
  size_t row_count;
  uint32_t rows_revision, detail_revision;
  uint64_t detail_id;
  bool rows_valid, detail_valid, rows_night, detail_night;
  bool rows_24h, detail_24h;
  bool list_palette_valid, detail_palette_valid, list_palette_night, detail_palette_night;
  lv_event_cb_t row_cb;
} alert_views_t;

static alert_views_t s_alerts;


static const char *const WEEKDAY_SHORT[] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static lv_color_t color_text_subtle(void)
{
  return lv_color_hex(0x6d819d);
}

static lv_color_t color_important(void)
{
  return lv_color_hex(0xf6d6b1);
}

/* Provider dates are calendar values, not timestamps. Derive weekdays without
 * the running clock or timezone normalization changing the supplied date. */
static int calendar_month_days(int year, int month)
{
  static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return days[month - 1] + (month == 2 && year % 4 == 0
    && (year % 100 != 0 || year % 400 == 0));
}

static bool forecast_calendar(const char date[11], struct tm *calendar)
{
  if (date[10] != '\0' || date[4] != '-' || date[7] != '-') return false;
  for (int i = 0; i < 10; ++i) {
    if (i != 4 && i != 7 && (date[i] < '0' || date[i] > '9')) return false;
  }
  int year = (date[0] - '0') * 1000 + (date[1] - '0') * 100
    + (date[2] - '0') * 10 + date[3] - '0';
  int month = (date[5] - '0') * 10 + date[6] - '0';
  int day = (date[8] - '0') * 10 + date[9] - '0';
  if (year < 1 || month < 1 || month > 12 || day < 1
      || day > calendar_month_days(year, month)) return false;
  static const int month_offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  int weekday_year = year - (month < 3);
  *calendar = (struct tm){.tm_year = year - 1900, .tm_mon = month - 1, .tm_mday = day,
    .tm_wday = (weekday_year + weekday_year / 4 - weekday_year / 100
      + weekday_year / 400 + month_offsets[month - 1] + day) % 7};
  return true;
}

const char *wac_ui_forecast_weekday(const app_forecast_day_t *forecast)
{
  struct tm calendar;
  return forecast_calendar(forecast->date, &calendar) ? WEEKDAY_SHORT[calendar.tm_wday] : "--";
}

static const char *first_forecast_caption(const app_weather_snapshot_t *weather, const struct tm *local_calendar)
{
  struct tm forecast;
  if (!forecast_calendar(weather->forecast[0].date, &forecast)) return "--";
  if (local_calendar != NULL) {
    int year = local_calendar->tm_year + 1900;
    int month = local_calendar->tm_mon + 1;
    int day = local_calendar->tm_mday + 1;
    if (day > calendar_month_days(year, month)) {
      day = 1;
      if (++month > 12) { month = 1; ++year; }
    }
    if (forecast.tm_year == year - 1900 && forecast.tm_mon == month - 1
        && forecast.tm_mday == day) return "Tomorrow";
  }
  return WEEKDAY_SHORT[forecast.tm_wday];
}

static void update_pending_weather_labels(void)
{
  const char *pending_temp = "--";

  wac_ui_set_label_text(s_weather.forecast_tomorrow_day_label, "--");
  wac_ui_set_label_text(s_weather.forecast_tomorrow_temps_label, pending_temp);

  for (int i = 0; i < FORECAST_ROW_COUNT; ++i) {
    wac_ui_set_label_text(s_weather.forecast_row_day_labels[i], "--");
    wac_ui_set_label_text(s_weather.forecast_row_temps_labels[i], pending_temp);
  }
}

static void apply_forecast_night_state(const app_weather_snapshot_t *weather, bool active)
{
  lv_opa_t icon_opa = !weather->valid ? LV_OPA_TRANSP : active ? LV_OPA_80 : LV_OPA_COVER;
  if (s_weather.forecast_stage_image != NULL) {
    wac_ui_set_image_asset(s_weather.forecast_stage_image, active ? &round_stage_night : &round_stage_day);
  }

  if (s_weather.forecast_tomorrow_frame != NULL) {
    wac_ui_set_image_asset(
      s_weather.forecast_tomorrow_frame,
      active ? &forecast_tomorrow_frame_night : &forecast_tomorrow_frame_day
    );
  }

  if (s_weather.forecast_tomorrow_day_label != NULL) {
    lv_obj_set_style_text_color(s_weather.forecast_tomorrow_day_label, active ? wac_ui_color_night_primary() : wac_ui_color_text_primary(), 0);
  }

  if (s_weather.forecast_tomorrow_temps_label != NULL) {
    lv_obj_set_style_text_color(s_weather.forecast_tomorrow_temps_label, active ? wac_ui_color_night_primary() : lv_color_hex(0xf1f7ff), 0);
  }

  if (s_weather.forecast_tomorrow_icon != NULL) {
    wac_ui_apply_image_night_tint(s_weather.forecast_tomorrow_icon, active, lv_color_hex(0xb06b6b), LV_OPA_40);
    lv_obj_set_style_opa(s_weather.forecast_tomorrow_icon, icon_opa, 0);
  }

  if (s_weather.forecast_updating_label != NULL) {
    lv_obj_set_style_text_color(s_weather.forecast_updating_label,
      active ? wac_ui_color_night_muted() : wac_ui_color_text_muted(), 0);
    if (weather->valid) lv_obj_add_flag(s_weather.forecast_updating_label, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(s_weather.forecast_updating_label, LV_OBJ_FLAG_HIDDEN);
  }

  for (int i = 0; i < FORECAST_ROW_COUNT; ++i) {
    if (s_weather.forecast_row_day_labels[i] != NULL) {
      lv_obj_set_style_text_color(
        s_weather.forecast_row_day_labels[i],
        active ? wac_ui_color_night_primary() : lv_color_hex(0xf5f9ff),
        0
      );
    }

    if (s_weather.forecast_row_temps_labels[i] != NULL) {
      lv_obj_set_style_text_color(
        s_weather.forecast_row_temps_labels[i],
        active ? wac_ui_color_night_muted() : lv_color_hex(0xe8f0fb),
        0
      );
    }

    if (s_weather.forecast_row_icons[i] != NULL) {
      wac_ui_apply_image_night_tint(s_weather.forecast_row_icons[i], active, lv_color_hex(0xb06b6b), LV_OPA_40);
      /* Keep the icon's space in the flex row so waiting does not move the
       * provider-date and temperature labels. */
      lv_obj_set_style_opa(s_weather.forecast_row_icons[i], icon_opa, 0);
    }
  }
}

static void apply_message_night_state(bool important, bool active)
{
  if (s_weather.message_stage_image != NULL) {
    wac_ui_set_image_asset(s_weather.message_stage_image, active ? &round_stage_night : &round_stage_day);
  }

  if (s_weather.message_title_label != NULL) {
    lv_obj_set_style_text_color(
      s_weather.message_title_label,
      active ? wac_ui_color_night_primary() : (important ? color_important() : wac_ui_color_text_primary()),
      0
    );
  }

  if (s_weather.message_text_label != NULL) {
    lv_obj_set_style_text_color(s_weather.message_text_label, active ? wac_ui_color_night_primary() : wac_ui_color_text_primary(), 0);
  }

  if (s_weather.message_meta_label != NULL) {
    lv_obj_set_style_text_color(
      s_weather.message_meta_label,
      active ? wac_ui_color_night_muted() : (important ? color_important() : wac_ui_color_text_muted()),
      0
    );
  }

  if (s_weather.message_dismiss_label != NULL) {
    lv_obj_set_style_text_color(s_weather.message_dismiss_label, active ? wac_ui_color_night_muted() : color_text_subtle(), 0);
  }

  if (s_weather.message_empty_label != NULL) {
    lv_obj_set_style_text_color(s_weather.message_empty_label, active ? wac_ui_color_night_muted() : wac_ui_color_text_muted(), 0);
  }
}

static void apply_status_night_state(bool active)
{
  if (s_weather.weather_status_label != NULL) {
    lv_obj_set_style_text_color(s_weather.weather_status_label, active ? wac_ui_color_night_muted() : wac_ui_color_text_muted(), 0);
  }
}

void wac_weather_build_forecast(lv_obj_t *parent)
{
  s_weather.palette_applied = false;
  s_weather.forecast_view = lv_obj_create(parent);
  lv_obj_set_size(s_weather.forecast_view, LV_PCT(100), LV_PCT(100));
  /* See analog_view: opaque, deliberate backing behind the stage image. */
  lv_obj_set_style_bg_color(s_weather.forecast_view, wac_ui_color_bg(), 0);
  lv_obj_set_style_bg_opa(s_weather.forecast_view, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_weather.forecast_view, 0, 0);
  lv_obj_set_style_pad_all(s_weather.forecast_view, 0, 0);
  lv_obj_set_scrollbar_mode(s_weather.forecast_view, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(s_weather.forecast_view, LV_OBJ_FLAG_HIDDEN);

  s_weather.forecast_stage_image = lv_image_create(s_weather.forecast_view);
  lv_image_set_src(s_weather.forecast_stage_image, &round_stage_day);
  lv_obj_align(s_weather.forecast_stage_image, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t *forecast_face = lv_obj_create(s_weather.forecast_view);
  lv_obj_set_size(forecast_face, FACE_CONTENT_SIZE, FACE_CONTENT_SIZE);
  lv_obj_align(forecast_face, LV_ALIGN_CENTER, 0, 0);
  wac_ui_clear_container_chrome(forecast_face);

  s_weather.forecast_tomorrow_frame = lv_image_create(forecast_face);
  lv_image_set_src(s_weather.forecast_tomorrow_frame, &forecast_tomorrow_frame_day);
  lv_obj_align(s_weather.forecast_tomorrow_frame, LV_ALIGN_TOP_MID, 0, FORECAST_TOMORROW_FRAME_TOP);

  s_weather.forecast_tomorrow_day_label = wac_ui_create_label(
    forecast_face, &lv_font_montserrat_24, wac_ui_color_text_primary(), LV_TEXT_ALIGN_CENTER, "--"
  );
  lv_obj_set_width(s_weather.forecast_tomorrow_day_label, LV_PCT(100));
  lv_obj_align(s_weather.forecast_tomorrow_day_label, LV_ALIGN_TOP_MID, 0, FORECAST_TOMORROW_LABEL_TOP);

  s_weather.forecast_tomorrow_icon = lv_image_create(forecast_face);
  wac_ui_set_weather_image(s_weather.forecast_tomorrow_icon, weather_icon_image_for_name(weather_icon_fallback_name()), FORECAST_TOMORROW_ICON_SIZE);
  lv_obj_set_style_opa(s_weather.forecast_tomorrow_icon, LV_OPA_TRANSP, 0);
  lv_obj_align(s_weather.forecast_tomorrow_icon, LV_ALIGN_TOP_MID, 0, FORECAST_TOMORROW_ICON_TOP);

  s_weather.forecast_updating_label = wac_ui_create_label(
    s_weather.forecast_view, &wac_font_weather_32, wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, "Waiting for weather…"
  );
  lv_obj_set_width(s_weather.forecast_updating_label, 400);
  lv_obj_set_pos(s_weather.forecast_updating_label, 200, 290);

  s_weather.forecast_tomorrow_temps_label = wac_ui_create_label(
    forecast_face, &lv_font_montserrat_48, lv_color_hex(0xf1f7ff), LV_TEXT_ALIGN_CENTER, "--"
  );
  lv_obj_set_width(s_weather.forecast_tomorrow_temps_label, LV_PCT(100));
  lv_obj_align(s_weather.forecast_tomorrow_temps_label, LV_ALIGN_TOP_MID, 0, FORECAST_TOMORROW_TEMPS_TOP);

  static const int ROW_X[FORECAST_ROW_COUNT] = {
    FORECAST_ROW1_X, FORECAST_ROW2_X, FORECAST_ROW3_X, FORECAST_ROW4_X
  };
  static const int ROW_Y[FORECAST_ROW_COUNT] = {
    FORECAST_ROW1_Y, FORECAST_ROW2_Y, FORECAST_ROW3_Y, FORECAST_ROW4_Y
  };
  for (int i = 0; i < FORECAST_ROW_COUNT; ++i) {
    lv_obj_t *row = lv_obj_create(forecast_face);
    wac_ui_clear_container_chrome(row);
    lv_obj_set_size(row, 104, 120);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, ROW_X[i] - 52, ROW_Y[i] - 50);
    /* The label/icon/temperature stack is about 144px tall in a 120px row.
     * LVGL's overflow flag uses the parent's extra draw margin, so retain
     * both the flag and transform margins to avoid clipping the end labels. */
    lv_obj_add_flag(row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_transform_width(row, 40, 0);
    lv_obj_set_style_transform_height(row, 40, 0);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(row, 0, 0);

    s_weather.forecast_row_day_labels[i] = wac_ui_create_label(
      row, &lv_font_montserrat_24, lv_color_hex(0xf5f9ff), LV_TEXT_ALIGN_CENTER, "--"
    );
    lv_obj_set_width(s_weather.forecast_row_day_labels[i], LV_PCT(100));

    s_weather.forecast_row_icons[i] = lv_image_create(row);
    wac_ui_set_weather_image(s_weather.forecast_row_icons[i], weather_icon_image_for_name(weather_icon_fallback_name()), FORECAST_ROW_ICON_SIZE);
    lv_obj_set_style_opa(s_weather.forecast_row_icons[i], LV_OPA_TRANSP, 0);

    s_weather.forecast_row_temps_labels[i] = wac_ui_create_label(
      row, &lv_font_montserrat_24, lv_color_hex(0xe8f0fb), LV_TEXT_ALIGN_CENTER, "--"
    );
    lv_obj_set_width(s_weather.forecast_row_temps_labels[i], LV_PCT(100));
  }
  s_weather.forecast_credit = wac_ui_create_weather_credit(s_weather.forecast_view);
}

/* The software renderer cannot stroke bitmap fonts. Reuse LVGL's ordinary
 * label layout for the eight small dark offsets and the normal foreground.
 * No extra labels, raster buffers or curved-text state are needed. */
static void radar_label_darkening_cb(lv_event_t *event)
{
  lv_obj_t *obj = lv_event_get_current_target(event);
  lv_layer_t *layer = lv_event_get_layer(event);
  lv_draw_label_dsc_t dsc;
  lv_draw_label_dsc_init(&dsc);
  lv_obj_init_draw_label_dsc(obj, LV_PART_MAIN, &dsc);
  dsc.text = lv_label_get_text(obj);
  dsc.color = lv_obj_get_style_bg_color(obj, 0);
  bool heading = obj == s_weather.radar_heading;
  dsc.opa = LV_OPA_MIX2(layer->opa, heading ? 179 : 191);
  int radius = heading ? 4 : 2;
  static const int8_t offsets[][2] = {
    {-4, 0}, {-3, -3}, {0, -4}, {3, -3}, {4, 0}, {3, 3}, {0, 4}, {-3, 3},
  };
  for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
    lv_area_t area;
    lv_obj_get_content_coords(obj, &area);
    lv_area_move(&area, offsets[i][0] * radius / 4, offsets[i][1] * radius / 4);
    lv_draw_label(layer, &dsc, &area);
  }
}

/* Radar's full-size pixels contain the map, provider labels and radar. */
void wac_weather_set_radar_enabled(lv_obj_t *parent, bool enabled)
{
  if (!enabled) {
    if (s_weather.radar_view == NULL) return;
    lv_image_set_src(s_weather.radar_image, NULL);
    lv_image_cache_drop(&s_weather.radar_source);
    lv_obj_delete(s_weather.radar_view);
    s_weather.radar_view = s_weather.radar_image = NULL;
    s_weather.radar_backdrop = NULL;
    s_weather.radar_heading = s_weather.radar_time = NULL;
    s_weather.radar_unavailable = s_weather.radar_marker = NULL;
    s_weather.radar_legend = s_weather.radar_credit = NULL;
    s_weather.radar_control = s_weather.radar_control_icon = NULL;
    memset(s_weather.radar_legend_labels, 0, sizeof(s_weather.radar_legend_labels));
    s_weather.radar_source = (lv_image_dsc_t){0};
    s_weather.radar_generation = 0;
    s_weather.radar_palette_applied = false;
    return;
  }
  if (s_weather.radar_view != NULL) return;
  s_weather.radar_palette_applied = false;
  lv_obj_t *view = s_weather.radar_view = lv_obj_create(parent);
  wac_ui_clear_container_chrome(view);
  lv_obj_set_size(view, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(view, LV_OPA_COVER, 0);
  lv_obj_remove_flag(view, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(view, LV_OBJ_FLAG_HIDDEN);
  s_weather.radar_backdrop = lv_image_create(view);
  lv_image_set_src(s_weather.radar_backdrop, &wac_radar_unavailable_glow);
  lv_image_set_pivot(s_weather.radar_backdrop, 0, 0);
  lv_image_set_scale(s_weather.radar_backdrop, 3200);
  lv_obj_set_pos(s_weather.radar_backdrop, 0, 0);
  s_weather.radar_image = lv_image_create(view);
  lv_obj_set_pos(s_weather.radar_image, 0, 0);
  lv_obj_add_flag(s_weather.radar_image, LV_OBJ_FLAG_HIDDEN);

  s_weather.radar_marker = lv_obj_create(view);
  wac_ui_clear_container_chrome(s_weather.radar_marker);
  lv_obj_set_size(s_weather.radar_marker, 18, 18);
  lv_obj_set_pos(s_weather.radar_marker, 391, 391);
  lv_obj_set_style_radius(s_weather.radar_marker, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(s_weather.radar_marker, 2, 0);
  lv_obj_set_style_bg_opa(s_weather.radar_marker, LV_OPA_COVER, 0);
  s_weather.radar_heading = wac_ui_create_label(view, &lv_font_montserrat_32,
    wac_ui_color_text_primary(), LV_TEXT_ALIGN_CENTER, "");
  lv_obj_set_width(s_weather.radar_heading, 390);
  lv_label_set_long_mode(s_weather.radar_heading, LV_LABEL_LONG_MODE_DOTS);
  s_weather.radar_time = wac_ui_create_label(view, &wac_font_weather_18,
    wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, "");
  lv_obj_set_size(s_weather.radar_time, 500, wac_font_weather_18.line_height);
  lv_obj_set_pos(s_weather.radar_time, 150,
    114 - (wac_font_weather_18.line_height - wac_font_weather_18.base_line));
  lv_obj_add_event_cb(s_weather.radar_heading, radar_label_darkening_cb, LV_EVENT_DRAW_MAIN_BEGIN, NULL);
  lv_obj_add_event_cb(s_weather.radar_time, radar_label_darkening_cb, LV_EVENT_DRAW_MAIN_BEGIN, NULL);
  s_weather.radar_unavailable = wac_ui_create_label(view, &lv_font_montserrat_32,
    wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, "Radar unavailable.");
  lv_obj_set_width(s_weather.radar_unavailable, 500);
  lv_obj_set_pos(s_weather.radar_unavailable, 150, 380);

  s_weather.radar_legend = lv_obj_create(view);
  wac_ui_clear_container_chrome(s_weather.radar_legend);
  lv_obj_set_size(s_weather.radar_legend, 430, 12);
  lv_obj_set_pos(s_weather.radar_legend, 185, 674);
  static const uint32_t colors[] = {0x99d98d, 0x52c24f, 0x169830, 0x056b1e,
    0xffe800, 0xffc200, 0xff9900, 0xf03124, 0xbc0c19, 0x730011};
  static const int offsets[] = {0, 48, 95, 143, 143, 215, 287, 287, 359, 430};
  for (unsigned i = 0; i + 1 < sizeof(colors) / sizeof(colors[0]); ++i) {
    if (offsets[i + 1] == offsets[i]) continue;
    lv_obj_t *band = lv_obj_create(s_weather.radar_legend);
    wac_ui_clear_container_chrome(band);
    lv_obj_set_pos(band, offsets[i], 0);
    lv_obj_set_size(band, offsets[i + 1] - offsets[i], 12);
    lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(band, lv_color_hex(colors[i]), 0);
    lv_obj_set_style_bg_grad_color(band, lv_color_hex(colors[i + 1]), 0);
    lv_obj_set_style_bg_grad_dir(band, LV_GRAD_DIR_HOR, 0);
  }
  static const char *const legend[] = {"Light", "Moderate", "Heavy"};
  static const int legend_x[] = {185, 328, 472};
  static const int legend_width[] = {143, 144, 143};
  for (unsigned i = 0; i < 3; ++i) {
    s_weather.radar_legend_labels[i] = wac_ui_create_label(view, &wac_font_settings_24,
      wac_ui_color_text_primary(), LV_TEXT_ALIGN_CENTER, legend[i]);
    lv_obj_set_width(s_weather.radar_legend_labels[i], legend_width[i]);
    lv_obj_set_pos(s_weather.radar_legend_labels[i], legend_x[i], 693);
  }
  s_weather.radar_credit = wac_ui_create_label(view, &wac_font_settings_24,
    wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, "RainViewer · USGS");
  lv_obj_set_width(s_weather.radar_credit, 320);
  lv_obj_set_pos(s_weather.radar_credit, 240, 731);
  s_weather.radar_control = lv_obj_create(view);
  wac_ui_clear_container_chrome(s_weather.radar_control);
  lv_obj_set_size(s_weather.radar_control, 80, 80);
  lv_obj_set_pos(s_weather.radar_control, 360, 572);
  lv_obj_set_style_radius(s_weather.radar_control, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(s_weather.radar_control, 209, 0);
  lv_obj_set_style_border_width(s_weather.radar_control, 1, 0);
  lv_obj_set_style_border_opa(s_weather.radar_control, 46, 0);
  lv_obj_remove_flag(s_weather.radar_control, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  s_weather.radar_control_icon = wac_ui_create_label(s_weather.radar_control,
    &lv_font_montserrat_32, wac_ui_color_text_primary(), LV_TEXT_ALIGN_CENTER, LV_SYMBOL_PLAY);
  lv_obj_align(s_weather.radar_control_icon, LV_ALIGN_CENTER, 2, 0);
  /* A newly eligible location can create this surface after startup. Keep it
   * beneath the existing gesture, status and alarm objects in that case too. */
  lv_obj_move_background(view);
}

bool wac_weather_radar_control_hit(int32_t x, int32_t y)
{
  int32_t dx = x - 400, dy = y - 612;
  return s_weather.radar_control != NULL
    && !lv_obj_has_state(s_weather.radar_control, LV_STATE_DISABLED)
    && dx * dx + dy * dy <= 52 * 52;
}

void wac_weather_update_radar(const app_radar_snapshot_t *radar,
  const device_config_t *config, bool night)
{
  if (s_weather.radar_view == NULL) return;
  bool matching = config->location_ready
    && fabs(radar->latitude - strtod(config->latitude, NULL)) < 0.000001
    && fabs(radar->longitude - strtod(config->longitude, NULL)) < 0.000001;
  bool have = matching && radar->valid && radar->pixels != NULL;
  const uint8_t *pixels = have ? radar->pixels : NULL;
  if (s_weather.radar_generation != radar->generation || s_weather.radar_source.data != pixels) {
    /* Evict the descriptor before changing its external pointer. Detach even
     * while hidden: the client may reuse the retired buffer after this draw. */
    lv_image_set_src(s_weather.radar_image, NULL);
    lv_image_cache_drop(&s_weather.radar_source);
    s_weather.radar_source = (lv_image_dsc_t){
      .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_ARGB8888,
        .w = APP_RADAR_WIDTH, .h = APP_RADAR_HEIGHT, .stride = APP_RADAR_WIDTH * 4},
      .data_size = APP_RADAR_WIDTH * APP_RADAR_HEIGHT * 4, .data = pixels,
    };
    if (have) lv_image_set_src(s_weather.radar_image, &s_weather.radar_source);
    s_weather.radar_generation = radar->generation;
  }
  if (!s_weather.radar_palette_applied || s_weather.radar_palette_night != night) {
    lv_color_t primary = night ? wac_ui_color_night_primary() : wac_ui_color_text_primary();
    lv_color_t muted = night ? wac_ui_color_night_muted() : wac_ui_color_text_muted();
    lv_color_t bg = lv_color_hex(night ? 0x18090c : 0x07101c);
    lv_obj_set_style_bg_color(s_weather.radar_view, bg, 0);
    lv_image_set_src(s_weather.radar_backdrop,
      night ? &wac_radar_unavailable_glow_night : &wac_radar_unavailable_glow);
    lv_obj_set_style_bg_color(s_weather.radar_heading, bg, 0);
    lv_obj_set_style_bg_color(s_weather.radar_time, bg, 0);
    lv_obj_set_style_text_color(s_weather.radar_heading, primary, 0);
    lv_obj_set_style_text_color(s_weather.radar_time, muted, 0);
    lv_obj_set_style_text_color(s_weather.radar_unavailable, muted, 0);
    lv_obj_set_style_text_color(s_weather.radar_credit, muted, 0);
    lv_obj_set_style_bg_color(s_weather.radar_control, bg, 0);
    lv_obj_set_style_border_color(s_weather.radar_control, primary, 0);
    lv_obj_set_style_text_color(s_weather.radar_control_icon, primary, 0);
    lv_obj_set_style_bg_color(s_weather.radar_marker, lv_color_hex(night ? 0xe1b6b6 : 0x93d1ff), 0);
    lv_obj_set_style_border_color(s_weather.radar_marker, lv_color_hex(0xffffff), 0);
    for (unsigned i = 0; i < 3; ++i)
      lv_obj_set_style_text_color(s_weather.radar_legend_labels[i], primary, 0);
    s_weather.radar_palette_applied = true;
    s_weather.radar_palette_night = night;
  }
  lv_obj_t *data_objects[] = {s_weather.radar_image, s_weather.radar_marker,
    s_weather.radar_legend, s_weather.radar_legend_labels[0],
    s_weather.radar_legend_labels[1], s_weather.radar_legend_labels[2]};
  for (unsigned i = 0; i < sizeof(data_objects) / sizeof(data_objects[0]); ++i) {
    if (have) lv_obj_remove_flag(data_objects[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(data_objects[i], LV_OBJ_FLAG_HIDDEN);
  }
  if (have) {
    lv_obj_add_flag(s_weather.radar_unavailable, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_weather.radar_backdrop, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_remove_flag(s_weather.radar_unavailable, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_weather.radar_backdrop, LV_OBJ_FLAG_HIDDEN);
  }
  bool control_enabled = have && (radar->playing
    || (radar->history_count >= 2 && !radar->history_loading));
  if (control_enabled) lv_obj_remove_state(s_weather.radar_control, LV_STATE_DISABLED);
  else lv_obj_add_state(s_weather.radar_control, LV_STATE_DISABLED);
  lv_opa_t control_opa = control_enabled ? LV_OPA_COVER : LV_OPA_40;
  if (lv_obj_get_style_opa(s_weather.radar_control, 0) != control_opa)
    lv_obj_set_style_opa(s_weather.radar_control, control_opa, 0);
  const char *symbol = radar->playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY;
  if (strcmp(lv_label_get_text(s_weather.radar_control_icon), symbol) != 0) {
    wac_ui_set_label_text(s_weather.radar_control_icon, symbol);
    lv_obj_align(s_weather.radar_control_icon, LV_ALIGN_CENTER, radar->playing ? 0 : 2, 0);
  }

  char heading[80];
  /* Use the resolved place name, omitting the address qualifiers in the title. */
  size_t place_len = strcspn(config->location, ",");
  snprintf(heading, sizeof(heading), "%.*s%sRadar", (int)place_len,
    config->location, place_len ? " " : "");
  static const lv_font_t *const heading_fonts[] = {&lv_font_montserrat_32, &lv_font_montserrat_24};
  lv_point_t heading_size;
  lv_text_get_size(&heading_size, heading, heading_fonts[0], 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  const lv_font_t *heading_font = heading_size.x <= 390
    ? heading_fonts[0] : heading_fonts[1];
  if (lv_obj_get_style_text_font(s_weather.radar_heading, 0) != heading_font)
    lv_obj_set_style_text_font(s_weather.radar_heading, heading_font, 0);
  lv_obj_set_height(s_weather.radar_heading, heading_font->line_height);
  lv_obj_set_pos(s_weather.radar_heading, 205,
    78 - (heading_font->line_height - heading_font->base_line));
  wac_ui_set_label_text(s_weather.radar_heading, heading);
  char stamp[80] = "";
  struct tm frame;
  if (have && radar->frame_time > 0 && localtime_r(&radar->frame_time, &frame) != NULL) {
    char date[24], hour[24];
    char month[8];
    strftime(month, sizeof(month), "%b", &frame);
    snprintf(date, sizeof(date), "%s %d", month, frame.tm_mday);
    bool time_24h = wac_ui_strings_equal_ignore_case(config->time_format, "24");
    if (time_24h) strftime(hour, sizeof(hour), "%H:%M %Z", &frame);
    else {
      char zone[8];
      strftime(zone, sizeof(zone), "%Z", &frame);
      snprintf(hour, sizeof(hour), "%d:%02d %s %s", frame.tm_hour % 12 ? frame.tm_hour % 12 : 12,
        frame.tm_min, frame.tm_hour >= 12 ? "PM" : "AM", zone);
    }
    snprintf(stamp, sizeof(stamp), "%s · %s%s%s", date, hour,
      radar->stale ? " (Old)" : "", radar->paused ? " · Paused" : "");
  }
  wac_ui_set_label_text(s_weather.radar_time, stamp);

  lv_obj_move_foreground(s_weather.radar_marker);
}

void wac_weather_build_message(lv_obj_t *parent)
{
  s_weather.palette_applied = false;
  s_weather.message_view = lv_obj_create(parent);
  lv_obj_set_size(s_weather.message_view, LV_PCT(100), LV_PCT(100));
  /* See analog_view: opaque, deliberate backing behind the stage image. */
  lv_obj_set_style_bg_color(s_weather.message_view, wac_ui_color_bg(), 0);
  lv_obj_set_style_bg_opa(s_weather.message_view, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_weather.message_view, 0, 0);
  lv_obj_set_style_pad_all(s_weather.message_view, 0, 0);
  lv_obj_set_scrollbar_mode(s_weather.message_view, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(s_weather.message_view, LV_OBJ_FLAG_HIDDEN);

  s_weather.message_stage_image = lv_image_create(s_weather.message_view);
  lv_image_set_src(s_weather.message_stage_image, &round_stage_day);
  lv_obj_align(s_weather.message_stage_image, LV_ALIGN_CENTER, 0, 0);

  s_weather.message_face = lv_obj_create(s_weather.message_view);
  lv_obj_set_size(s_weather.message_face, 760, 760);
  lv_obj_align(s_weather.message_face, LV_ALIGN_CENTER, 0, 0);
  wac_ui_clear_container_chrome(s_weather.message_face);

  s_weather.message_title_label = wac_ui_create_label(
    s_weather.message_face, &lv_font_montserrat_32, wac_ui_color_text_primary(), LV_TEXT_ALIGN_CENTER, "Message"
  );
  lv_obj_set_width(s_weather.message_title_label, LV_PCT(100));
  lv_obj_align(s_weather.message_title_label, LV_ALIGN_TOP_MID, 0, 138);

  s_weather.message_card = lv_obj_create(s_weather.message_face);
  wac_ui_clear_container_chrome(s_weather.message_card);
  lv_obj_set_size(s_weather.message_card, 430, LV_SIZE_CONTENT);
  lv_obj_align(s_weather.message_card, LV_ALIGN_TOP_MID, 0, 232);
  lv_obj_set_style_pad_row(s_weather.message_card, 26, 0);
  lv_obj_set_layout(s_weather.message_card, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(s_weather.message_card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(
    s_weather.message_card,
    LV_FLEX_ALIGN_CENTER,
    LV_FLEX_ALIGN_CENTER,
    LV_FLEX_ALIGN_CENTER
  );
  lv_obj_add_flag(s_weather.message_card, LV_OBJ_FLAG_HIDDEN);

  s_weather.message_text_label = wac_ui_create_label(
    s_weather.message_card, &lv_font_montserrat_48, wac_ui_color_text_primary(), LV_TEXT_ALIGN_CENTER, "Dinner is ready."
  );
  lv_obj_set_width(s_weather.message_text_label, LV_PCT(100));
  lv_label_set_long_mode(s_weather.message_text_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_letter_space(s_weather.message_text_label, -1, 0);

  s_weather.message_meta_label = wac_ui_create_label(
    s_weather.message_card, &lv_font_montserrat_24, wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, "- Alex, 5:30 PM"
  );
  lv_obj_set_width(s_weather.message_meta_label, LV_PCT(100));

  s_weather.message_dismiss_label = wac_ui_create_label(
    s_weather.message_card, &lv_font_montserrat_18, color_text_subtle(), LV_TEXT_ALIGN_CENTER, "Tap to dismiss"
  );
  lv_obj_set_width(s_weather.message_dismiss_label, LV_PCT(100));
  lv_obj_set_style_text_letter_space(s_weather.message_dismiss_label, 1, 0);

  s_weather.message_empty_label = wac_ui_create_label(
    s_weather.message_face, &lv_font_montserrat_48, wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, "No messages"
  );
  lv_obj_set_width(s_weather.message_empty_label, 420);
  lv_obj_align(s_weather.message_empty_label, LV_ALIGN_TOP_MID, 0, 318);
}

void wac_weather_build_status(lv_obj_t *parent)
{
  s_weather.palette_applied = false;
  s_weather.status_stack = lv_obj_create(parent);
  lv_obj_set_width(s_weather.status_stack, 360);
  lv_obj_set_height(s_weather.status_stack, LV_SIZE_CONTENT);
  lv_obj_align(s_weather.status_stack, LV_ALIGN_BOTTOM_MID, 0, -56);
  lv_obj_set_style_bg_opa(s_weather.status_stack, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_weather.status_stack, 0, 0);
  lv_obj_set_style_pad_all(s_weather.status_stack, 0, 0);
  lv_obj_set_style_pad_row(s_weather.status_stack, 6, 0);
  lv_obj_set_layout(s_weather.status_stack, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(s_weather.status_stack, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scrollbar_mode(s_weather.status_stack, LV_SCROLLBAR_MODE_OFF);

  s_weather.weather_status_label = wac_ui_create_label(
    s_weather.status_stack, &lv_font_montserrat_18, wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, ""
  );
  lv_obj_set_width(s_weather.weather_status_label, LV_PCT(100));
  lv_obj_set_style_text_opa(s_weather.weather_status_label, LV_OPA_70, 0);
  lv_obj_set_style_text_letter_space(s_weather.weather_status_label, 1, 0);
  lv_obj_add_flag(s_weather.weather_status_label, LV_OBJ_FLAG_HIDDEN);
}

static void update_weather_labels(const app_weather_snapshot_t *weather, const struct tm *local_calendar)
{
  if (!weather->valid) {
    return;
  }

  char tomorrow_temps[24];
  snprintf(
    tomorrow_temps,
    sizeof(tomorrow_temps),
    "%d° / %d°",
    weather->forecast[0].high,
    weather->forecast[0].low
  );
  wac_ui_set_label_text(s_weather.forecast_tomorrow_day_label, first_forecast_caption(weather, local_calendar));
  wac_ui_set_label_text(s_weather.forecast_tomorrow_temps_label, tomorrow_temps);
  if (s_weather.forecast_tomorrow_icon != NULL) {
    wac_ui_set_weather_image(
      s_weather.forecast_tomorrow_icon,
      weather_icon_image_for_name(weather->forecast[0].icon_name), FORECAST_TOMORROW_ICON_SIZE
    );
  }

  for (int i = 0; i < FORECAST_ROW_COUNT; ++i) {
    int item_index = i + 1;
    char row_temps[24];
    snprintf(
      row_temps,
      sizeof(row_temps),
      "%d° / %d°",
      weather->forecast[item_index].high,
      weather->forecast[item_index].low
    );
    wac_ui_set_label_text(s_weather.forecast_row_day_labels[i], wac_ui_forecast_weekday(&weather->forecast[item_index]));
    wac_ui_set_label_text(s_weather.forecast_row_temps_labels[i], row_temps);
    if (s_weather.forecast_row_icons[i] != NULL) {
      wac_ui_set_weather_image(
        s_weather.forecast_row_icons[i],
        weather_icon_image_for_name(weather->forecast[item_index].icon_name), FORECAST_ROW_ICON_SIZE
      );
    }
  }
}

void wac_weather_update_status(const app_weather_snapshot_t *weather,
  bool night, bool forecast_visible)
{
  if (!night && forecast_visible && weather->valid && weather->stale) {
    lv_obj_set_width(s_weather.status_stack, 440);
    lv_obj_align(s_weather.status_stack, LV_ALIGN_TOP_LEFT, 180, 120);
    if (lv_obj_get_style_text_font(s_weather.weather_status_label, 0) != &lv_font_montserrat_22)
      lv_obj_set_style_text_font(s_weather.weather_status_label, &lv_font_montserrat_22, 0);
    if (lv_obj_get_style_text_opa(s_weather.weather_status_label, 0) != LV_OPA_COVER)
      lv_obj_set_style_text_opa(s_weather.weather_status_label, LV_OPA_COVER, 0);
    if (lv_obj_get_style_text_letter_space(s_weather.weather_status_label, 0) != 0)
      lv_obj_set_style_text_letter_space(s_weather.weather_status_label, 0, 0);
    wac_ui_set_label_text(s_weather.weather_status_label, "Weather data stale");
    lv_obj_clear_flag(s_weather.weather_status_label, LV_OBJ_FLAG_HIDDEN);
  } else {
    wac_ui_set_label_text(s_weather.weather_status_label, "");
    lv_obj_add_flag(s_weather.weather_status_label, LV_OBJ_FLAG_HIDDEN);
  }
}

void wac_weather_update_message(const app_message_snapshot_t *snapshot)
{
  if (snapshot == NULL || !snapshot->available) {
    if (s_weather.message_card != NULL) lv_obj_add_flag(s_weather.message_card, LV_OBJ_FLAG_HIDDEN);
    if (s_weather.message_empty_label != NULL) lv_obj_clear_flag(s_weather.message_empty_label, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  if (s_weather.message_card != NULL) lv_obj_clear_flag(s_weather.message_card, LV_OBJ_FLAG_HIDDEN);
  if (s_weather.message_empty_label != NULL) lv_obj_add_flag(s_weather.message_empty_label, LV_OBJ_FLAG_HIDDEN);
  wac_ui_set_label_text(s_weather.message_text_label, snapshot->text);
  wac_ui_set_label_text(s_weather.message_meta_label, snapshot->meta);
}

void wac_weather_set_message_dismiss_text(const char *text)
{
  wac_ui_set_label_text(s_weather.message_dismiss_label, text);
}

void wac_weather_reset(void)
{
  if (s_weather.radar_source.data != NULL) lv_image_cache_drop(&s_weather.radar_source);
  free(s_alerts.row_data);
  memset(&s_alerts, 0, sizeof(s_alerts));
  memset(&s_weather, 0, sizeof(s_weather));
}

void wac_weather_set_visible(bool forecast, bool radar, bool message)
{
  lv_obj_t *views[] = {s_weather.forecast_view, s_weather.radar_view, s_weather.message_view};
  bool visible[] = {forecast, radar, message};
  for (size_t i = 0; i < sizeof(views) / sizeof(views[0]); ++i) {
    if (views[i] == NULL) continue;
    if (visible[i]) lv_obj_clear_flag(views[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(views[i], LV_OBJ_FLAG_HIDDEN);
  }
  if (s_weather.status_stack != NULL) {
    if (message) lv_obj_add_flag(s_weather.status_stack, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(s_weather.status_stack, LV_OBJ_FLAG_HIDDEN);
  }
}

void wac_weather_apply_night(const app_weather_snapshot_t *weather, bool important, bool active)
{
  wac_ui_update_weather_credit(s_weather.forecast_credit, weather, true, active);
  bool appearance_changed = !s_weather.palette_applied || s_weather.palette_night != active;
  if (appearance_changed || s_weather.palette_weather_valid != weather->valid)
    apply_forecast_night_state(weather, active);
  if (appearance_changed || s_weather.palette_important != important)
    apply_message_night_state(important, active);
  if (appearance_changed) apply_status_night_state(active);
  s_weather.palette_applied = true;
  s_weather.palette_night = active;
  s_weather.palette_weather_valid = weather->valid;
  s_weather.palette_important = important;
}

void wac_weather_update(const app_weather_snapshot_t *weather, const struct tm *local_calendar)
{
  if (weather->valid) update_weather_labels(weather, local_calendar);
  else update_pending_weather_labels();
}

static lv_color_t alert_color(bool night, uint32_t day, uint32_t dark)
{
  return lv_color_hex(night ? dark : day);
}

static void alert_visible(lv_obj_t *obj, bool visible)
{
  if (visible) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *alert_label(lv_obj_t *parent, const char *text, int x, int y,
  int width, const lv_font_t *font, lv_color_t color, lv_text_align_t align)
{
  lv_obj_t *label = wac_ui_create_label(parent, font, color, align, text);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_width(label, width);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
  return label;
}

static lv_obj_t *alert_icon(lv_obj_t *parent, const lv_image_dsc_t *asset,
  int size, lv_color_t color)
{
  lv_obj_t *icon = lv_image_create(parent);
  lv_image_set_src(icon, asset);
  lv_image_set_scale(icon, (uint32_t)size * LV_SCALE_NONE / asset->header.w);
  lv_obj_set_style_image_recolor(icon, color, 0);
  lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
  lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);
  return icon;
}

static void alert_button_palette(lv_obj_t *obj, bool night)
{
  lv_obj_set_style_bg_color(obj, alert_color(night, 0x17263b, 0x321c20), 0);
  lv_obj_set_style_border_color(obj, alert_color(night, 0x253a57, 0x633737), 0);
  lv_obj_set_style_text_color(obj, alert_color(night, 0xf5f8ff, 0xecc0c0), 0);
}

static lv_obj_t *alert_button(lv_obj_t *parent, int x, int y, int w, int h,
  lv_event_cb_t callback, void *data)
{
  lv_obj_t *obj = lv_button_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_radius(obj, 24, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  alert_button_palette(obj, false);
  lv_obj_add_event_cb(obj, callback, LV_EVENT_CLICKED, data);
  return obj;
}

static lv_obj_t *alert_scroll(lv_obj_t *parent, int y, int height, lv_obj_t **track)
{
  *track = lv_obj_create(parent);
  lv_obj_remove_style_all(*track);
  lv_obj_set_pos(*track, 672, y);
  lv_obj_set_size(*track, 10, height);
  lv_obj_set_style_radius(*track, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(*track, 97, 0);
  lv_obj_remove_flag(*track, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(*track, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_pos(obj, 140, y);
  lv_obj_set_size(obj, 542, height);
  lv_obj_set_style_pad_right(obj, 22, 0);
  lv_obj_set_scroll_dir(obj, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_width(obj, 10, LV_PART_SCROLLBAR);
  lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_SCROLLBAR);
  return obj;
}

static void alert_scroll_track(lv_obj_t *list, lv_obj_t *track)
{
  lv_obj_update_layout(list);
  alert_visible(track, lv_obj_get_scroll_top(list) + lv_obj_get_scroll_bottom(list) > 0);
}

static lv_obj_t *alert_surface(lv_obj_t *parent, const char *title,
  const char *bottom, lv_event_cb_t back_cb, lv_obj_t **heading, lv_obj_t **back)
{
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  alert_visible(root, false);
  lv_obj_t *stage = lv_image_create(root);
  lv_image_set_src(stage, &round_stage_day);
  lv_obj_center(stage);
  *heading = alert_label(root, title, 245, 128, 320, &lv_font_montserrat_36,
    lv_color_hex(0xf5f8ff), LV_TEXT_ALIGN_CENTER);
  lv_obj_set_style_text_letter_space(*heading, 1, 0);
  lv_obj_t *top = alert_button(root, 145, 104, 88, 88, back_cb, NULL);
  lv_obj_center(alert_icon(top, &wac_icon_chevron_left, 32, lv_color_hex(0xf5f8ff)));
  *back = alert_button(root, 254, 650, 292, 80, back_cb, NULL);
  lv_obj_t *label = alert_label(*back, bottom, 0, 0, 292, &wac_font_settings_28,
    lv_color_hex(0xf5f8ff), LV_TEXT_ALIGN_CENTER);
  lv_obj_center(label);
  return root;
}

void wac_weather_build_alerts(lv_obj_t *parent, lv_event_cb_t back_cb,
  lv_event_cb_t row_cb)
{
  lv_obj_t *heading, *back;
  s_alerts.row_cb = row_cb;
  s_alerts.list = alert_surface(parent, "Alerts", "Back to Clock", back_cb,
    &heading, &s_alerts.list_back);
  s_alerts.subtitle = alert_label(s_alerts.list, "", 150, 174, 500,
    &wac_font_settings_28, lv_color_hex(0xa6b6cf), LV_TEXT_ALIGN_CENTER);
  s_alerts.rows = alert_scroll(s_alerts.list, 220, 408, &s_alerts.list_track);
  s_alerts.notice = lv_obj_create(s_alerts.list);
  lv_obj_remove_style_all(s_alerts.notice);
  lv_obj_set_pos(s_alerts.notice, 150, 646);
  lv_obj_set_size(s_alerts.notice, 500, 80);
  lv_obj_remove_flag(s_alerts.notice, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  s_alerts.notice_primary = alert_label(s_alerts.notice, "", 0, 0, 500,
    &wac_font_settings_28, lv_color_hex(0xf5f8ff), LV_TEXT_ALIGN_CENTER);
  s_alerts.notice_secondary = alert_label(s_alerts.notice, "", 0, 40, 500,
    &wac_font_settings_24, lv_color_hex(0xa6b6cf), LV_TEXT_ALIGN_CENTER);
  s_alerts.detail = alert_surface(parent, "", "Back to Alerts", back_cb,
    &s_alerts.detail_heading, &back);
  alert_label(s_alerts.detail, "National Weather Service", 245, 183, 350,
    &wac_font_settings_24, lv_color_hex(0xa6b6cf), LV_TEXT_ALIGN_CENTER);
  /* Fixed heading/footer frame a flowing content area. Actual NWS paragraphs
   * are not constrained to the illustrative mockup's sample text boxes. */
  s_alerts.detail_content = alert_scroll(s_alerts.detail, 227, 401, &s_alerts.detail_track);
}

uint64_t wac_weather_alert_event_id(lv_event_t *event)
{
  const uint64_t *id = lv_event_get_user_data(event);
  return id ? *id : 0;
}

static void alert_time(char *buffer, size_t size, time_t timestamp, bool time_24h,
  bool with_date)
{
  struct tm local;
  if (timestamp <= 0 || !localtime_r(&timestamp, &local)) {
    snprintf(buffer, size, "--");
    return;
  }
  char clock[32], date[32] = "";
  if (time_24h) strftime(clock, sizeof(clock), "%H:%M", &local);
  else snprintf(clock, sizeof(clock), "%d:%02d %s", local.tm_hour % 12 ? local.tm_hour % 12 : 12,
    local.tm_min, local.tm_hour >= 12 ? "PM" : "AM");
  if (with_date) {
    char month[12];
    strftime(month, sizeof(month), "%b", &local);
    snprintf(date, sizeof(date), "%s %d · ", month, local.tm_mday);
  }
  snprintf(buffer, size, "%s%s", date, clock);
}

/* Restyle the fixed frame without changing scroll position or callback data. */
static void alert_frame_palette(lv_obj_t *root, bool night)
{
  bool *valid = root == s_alerts.list ? &s_alerts.list_palette_valid : &s_alerts.detail_palette_valid;
  bool *applied_night = root == s_alerts.list ? &s_alerts.list_palette_night : &s_alerts.detail_palette_night;
  if (*valid && *applied_night == night) return;
  *valid = true;
  *applied_night = night;
  lv_color_t primary = alert_color(night, 0xf5f8ff, 0xecc0c0);
  lv_color_t muted = alert_color(night, 0xa6b6cf, 0xc17a7a);
  lv_obj_set_style_bg_color(root, alert_color(night, 0x07101c, 0x18090c), 0);
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *child = lv_obj_get_child(root, i);
    if (lv_obj_check_type(child, &lv_image_class)) {
      lv_image_set_src(child, night ? &round_stage_night : &round_stage_day);
    } else if (lv_obj_check_type(child, &lv_label_class)) {
      lv_obj_set_style_text_color(child, child == s_alerts.subtitle
        || (root == s_alerts.detail && child != s_alerts.detail_heading) ? muted : primary, 0);
    } else if (lv_obj_check_type(child, &lv_button_class)) {
      alert_button_palette(child, night);
      for (uint32_t j = 0; j < lv_obj_get_child_count(child); ++j) {
        lv_obj_t *content = lv_obj_get_child(child, j);
        lv_obj_set_style_text_color(content, primary, 0);
        lv_obj_set_style_image_recolor(content, primary, 0);
      }
    }
  }
  lv_obj_set_style_bg_color(root == s_alerts.list ? s_alerts.rows : s_alerts.detail_content,
    alert_color(night, 0x93d1ff, 0xe1b6b6), LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_color(root == s_alerts.list ? s_alerts.list_track : s_alerts.detail_track,
    muted, 0);
}

static bool alert_same_rows(const weather_alert_row_t *rows, size_t count)
{
  if (!s_alerts.rows_valid || count != s_alerts.row_count) return false;
  for (size_t i = 0; i < count; ++i) {
    const weather_alert_row_t *a = &rows[i], *b = &s_alerts.row_data[i];
    if (a->id != b->id || a->type != b->type || a->cutoff != b->cutoff) return false;
  }
  return true;
}

static void alert_render_rows(const app_alert_snapshot_t *snapshot, bool time_24h, bool night)
{
  size_t count = (size_t)snapshot->warning_count + snapshot->watch_count;
  weather_alert_row_t *rows = count ? calloc(count, sizeof(*rows)) : NULL;
  if (count && !rows) return; /* Retry on the next normal UI update. */
  /* The worker can replace records between these short copies. Keep the old
   * list unless every row belongs to the snapshot whose counts we used. */
  for (size_t i = 0; i < count; ++i) {
    if (weather_client_alerts_row_copy(i, &rows[i]) != ESP_OK
        || rows[i].revision != snapshot->revision) { free(rows); return; }
  }
  bool same = alert_same_rows(rows, count);
  bool checking = !count && !snapshot->has_success && snapshot->status == APP_ALERT_CHECKING;
  if (same && time_24h == s_alerts.rows_24h && night == s_alerts.rows_night
      && checking == (s_alerts.checking != NULL)) {
    free(rows);
    s_alerts.rows_revision = snapshot->revision;
    return;
  }
  int32_t scroll_y = lv_obj_get_scroll_y(s_alerts.rows);
  /* Buttons hold addresses inside row_data. Delete them before freeing that
   * storage, and retain the replacement rows for their callbacks. */
  lv_obj_clean(s_alerts.rows);
  free(s_alerts.row_data);
  s_alerts.row_data = rows;
  s_alerts.row_count = count;
  s_alerts.checking = NULL;
  size_t catalog_count;
  const weather_alert_type_t *catalog = weather_client_alert_catalog(&catalog_count);
  lv_color_t primary = alert_color(night, 0xf5f8ff, 0xecc0c0);
  lv_color_t muted = alert_color(night, 0xa6b6cf, 0xc17a7a);
  for (size_t i = 0; i < count; ++i) {
    if (rows[i].type >= catalog_count) continue;
    const weather_alert_type_t *type = &catalog[rows[i].type];
    lv_obj_t *row = alert_button(s_alerts.rows, 0, i * 140, 520, 128,
      s_alerts.row_cb, &rows[i].id);
    alert_button_palette(row, night);
    lv_obj_t *triangle = alert_icon(row, &wac_icon_triangle_alert_32, 32,
      lv_color_hex(type->warning ? 0xff4d4d : 0xffe04b));
    lv_obj_set_pos(triangle, 22, 48);
    lv_obj_t *copy = lv_obj_create(row);
    lv_obj_remove_style_all(copy);
    lv_obj_set_size(copy, 370, LV_SIZE_CONTENT);
    lv_obj_remove_flag(copy, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *title = alert_label(copy, type->name, 0, 0, 370, &wac_font_settings_28,
      primary, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_line_space(title, 4, 0);
    lv_obj_update_layout(title);
    char until[96], stamp[64];
    alert_time(stamp, sizeof(stamp), rows[i].cutoff, time_24h, false);
    snprintf(until, sizeof(until), "Until %s", stamp);
    alert_label(copy, until, 0, lv_obj_get_height(title) + 8, 370,
      &wac_font_settings_24, muted, LV_TEXT_ALIGN_LEFT);
    lv_obj_align(copy, LV_ALIGN_LEFT_MID, 68, 0);
    lv_obj_t *chevron = alert_icon(row, &wac_icon_chevron_right, 24, muted);
    lv_obj_set_pos(chevron, 474, 52);
  }
  if (checking) s_alerts.checking = alert_label(s_alerts.rows, "Checking for alerts…", 8, 24,
    504, &wac_font_settings_28, muted, LV_TEXT_ALIGN_CENTER);
  lv_obj_update_layout(s_alerts.rows);
  alert_scroll_track(s_alerts.rows, s_alerts.list_track);
  lv_obj_scroll_to_y(s_alerts.rows, scroll_y, LV_ANIM_OFF);
  s_alerts.rows_valid = true;
  s_alerts.rows_revision = snapshot->revision;
  s_alerts.rows_24h = time_24h;
  s_alerts.rows_night = night;
}

static void alert_update_footer(const app_alert_snapshot_t *snapshot, bool time_24h, bool night)
{
  const char *primary = "", *secondary = "";
  char last_check[104], stamp[64];
  if (snapshot->status == APP_ALERT_FAILED) {
    primary = "Unable to update alerts";
    if (snapshot->has_success) {
      alert_time(stamp, sizeof(stamp), snapshot->last_success, time_24h, false);
      snprintf(last_check, sizeof(last_check), "Last successful check: %s", stamp);
      secondary = last_check;
    } else secondary = "No successful check yet";
  } else if (snapshot->status == APP_ALERT_UNSUPPORTED) {
    primary = "NWS alerts unavailable";
    secondary = "for this location";
  } else if (snapshot->save_failed) primary = "Unable to save alerts";
  alert_visible(s_alerts.list_back, !*primary);
  alert_visible(s_alerts.notice, *primary);
  wac_ui_set_label_text(s_alerts.notice_primary, primary);
  wac_ui_set_label_text(s_alerts.notice_secondary, secondary);
  lv_obj_set_style_text_color(s_alerts.notice_primary, alert_color(night, 0xf5f8ff, 0xecc0c0), 0);
  lv_obj_set_style_text_color(s_alerts.notice_secondary, alert_color(night, 0xa6b6cf, 0xc17a7a), 0);
  lv_obj_set_y(s_alerts.notice_primary, *secondary ? 4 : 24);
}

static int alert_detail_text(const char *text, int y, int inset, const lv_font_t *font,
  lv_color_t color, int spacing, lv_text_align_t align)
{
  if (!*text) return y;
  lv_obj_t *label = alert_label(s_alerts.detail_content, text, inset, y, 520 - inset * 2,
    font, color, align);
  lv_obj_set_style_text_line_space(label, spacing, 0);
  lv_obj_update_layout(label);
  return y + lv_obj_get_height(label);
}

static void alert_render_detail(uint64_t id, uint32_t revision, bool time_24h, bool night)
{
  weather_alert_detail_t detail;
  size_t needed = 0, capacity = 0;
  char *text = NULL;
  esp_err_t result = weather_client_alerts_detail_copy(id, &detail, NULL, 0, &needed);
  /* A revised event may grow between the size query and copy. Retry with its
   * complete new size rather than clipping any protective information. */
  for (int attempt = 0; result == ESP_ERR_INVALID_SIZE && attempt < 3; ++attempt) {
    char *grown = realloc(text, needed);
    if (!grown) { free(text); return; }
    text = grown;
    capacity = needed;
    result = weather_client_alerts_detail_copy(id, &detail, text, capacity, &needed);
  }
  if (result != ESP_OK) {
    free(text);
    if (result == ESP_ERR_NOT_FOUND) {
      lv_obj_clean(s_alerts.detail_content);
      alert_scroll_track(s_alerts.detail_content, s_alerts.detail_track);
      wac_ui_set_label_text(s_alerts.detail_heading, "");
      s_alerts.detail_valid = false;
    }
    return;
  }
  size_t count;
  const weather_alert_type_t *catalog = weather_client_alert_catalog(&count);
  if (detail.row.type >= count) { free(text); return; }
  const weather_alert_type_t *type = &catalog[detail.row.type];
  int32_t scroll_y = s_alerts.detail_valid && id == s_alerts.detail_id
    ? lv_obj_get_scroll_y(s_alerts.detail_content) : 0;
  lv_obj_clean(s_alerts.detail_content);
  wac_ui_set_label_text(s_alerts.detail_heading, type->warning ? "Warning" : "Watch");
  lv_obj_set_style_text_color(s_alerts.detail_heading,
    lv_color_hex(type->warning ? 0xff4d4d : 0xffe04b), 0);
  lv_color_t primary = alert_color(night, 0xf5f8ff, 0xecc0c0);
  lv_color_t muted = alert_color(night, 0xa6b6cf, 0xc17a7a);
  int y = alert_detail_text(type->name, 0, 5,
    &lv_font_montserrat_36, primary, 6, LV_TEXT_ALIGN_CENTER);
  y = y < 72 ? 92 : y + 20;
  y = alert_detail_text(text + detail.area_offset, y, 5, &wac_font_settings_26,
    muted, 8, LV_TEXT_ALIGN_CENTER);
  char issued[64], until[64], metadata[160];
  alert_time(issued, sizeof(issued), detail.row.issued, time_24h, true);
  alert_time(until, sizeof(until), detail.row.cutoff, time_24h, true);
  snprintf(metadata, sizeof(metadata), "Issued %s · Until %s", issued, until);
  y = alert_detail_text(metadata, y, 5, &wac_font_settings_26, muted, 8, LV_TEXT_ALIGN_CENTER);
  y = y < 167 ? 187 : y + 20;
  y = alert_detail_text(text + detail.description_offset, y, 10, &wac_font_settings_28,
    primary, 10, LV_TEXT_ALIGN_LEFT);
  alert_detail_text(text + detail.instruction_offset, y + 20, 10, &wac_font_settings_28,
    alert_color(night, 0x93d1ff, 0xe1b6b6), 10, LV_TEXT_ALIGN_LEFT);
  free(text); /* Labels own their complete copies. */
  lv_obj_update_layout(s_alerts.detail_content);
  alert_scroll_track(s_alerts.detail_content, s_alerts.detail_track);
  lv_obj_scroll_to_y(s_alerts.detail_content, scroll_y, LV_ANIM_OFF);
  s_alerts.detail_valid = true;
  s_alerts.detail_id = id;
  s_alerts.detail_revision = revision;
  s_alerts.detail_night = night;
  s_alerts.detail_24h = time_24h;
}

void wac_weather_update_alerts(const app_alert_snapshot_t *snapshot,
  const device_config_t *config, bool list_visible, bool detail_visible,
  uint64_t detail_id, bool night)
{
  if (!s_alerts.list || !snapshot || !config) return;
  alert_visible(s_alerts.list, list_visible);
  alert_visible(s_alerts.detail, detail_visible);
  if (!list_visible && !detail_visible) return;
  bool time_24h = wac_ui_strings_equal_ignore_case(config->time_format, "24");
  if (list_visible) {
    alert_frame_palette(s_alerts.list, night);
    char totals[96] = "";
    unsigned warnings = snapshot->warning_count, watches = snapshot->watch_count;
    if (warnings || watches) snprintf(totals, sizeof(totals), "%u warning%s · %u watch%s",
      warnings, warnings == 1 ? "" : "s", watches, watches == 1 ? "" : "es");
    else if (snapshot->has_success && snapshot->status == APP_ALERT_CURRENT)
      snprintf(totals, sizeof(totals), "No active alerts");
    wac_ui_set_label_text(s_alerts.subtitle, totals);
    alert_update_footer(snapshot, time_24h, night);
    if (!s_alerts.rows_valid || s_alerts.rows_revision != snapshot->revision
        || s_alerts.rows_night != night || s_alerts.rows_24h != time_24h)
      alert_render_rows(snapshot, time_24h, night);
  }
  if (detail_visible) {
    alert_frame_palette(s_alerts.detail, night);
    if (!s_alerts.detail_valid || s_alerts.detail_revision != snapshot->revision
        || s_alerts.detail_id != detail_id || s_alerts.detail_night != night
        || s_alerts.detail_24h != time_24h)
      alert_render_detail(detail_id, snapshot->revision, time_24h, night);
  }
}
