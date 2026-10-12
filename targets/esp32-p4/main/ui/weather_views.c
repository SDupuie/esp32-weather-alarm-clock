#include "ui_private.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "assets/conditions_icon_pressure.h"
#include "assets/conditions_icon_rain.h"
#include "assets/conditions_icon_snow.h"
#include "assets/conditions_icon_wind.h"
#include "assets/conditions_wind_arrow.h"
#include "assets/forecast_tomorrow_frame_day.h"
#include "assets/forecast_tomorrow_frame_night.h"
#include "assets/lv_font_wind_speed_70.h"
#include "assets/wac_fonts.h"
#include "assets/round_stage_day.h"
#include "assets/round_stage_night.h"
#include "assets/weather_icons.h"
#include "weather_icon_names.h"

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
/* Conditions-view positions use a 760px face coordinate space. The face
 * container is 760px and centered on screen. */
#define COND_FACE_SIZE 760
#define COND_FACE_CENTER (COND_FACE_SIZE / 2)
/* Baseline radii of the curved headline and detail. */
#define COND_HEADLINE_RADIUS 322
#define COND_DETAIL_RADIUS 286
#define COND_HEADLINE_MAX_PX 600
#define COND_DETAIL_MAX_PX 500
#define COND_DIAL_SIZE 334
#define COND_DIAL_TOP 122
/* Scale the 400px compass SVG to the 334px dial size. */
#define COND_DIAL_SCALE (334.0 / 400.0)
#define COND_TICK_COUNT 20
#define COND_WIND_CENTER_Y 292
#define COND_WIND_WIDTH 190
#define COND_DIVIDER_TOP 470
#define COND_DIVIDER_WIDTH 440
#define COND_COLUMNS_TOP 506
#define COND_COLUMN_COUNT 3
/* 1 : 1.1 : 1 of 520px, the middle (pressure) column a touch wider. */
#define COND_COLUMN_SIDE_WIDTH 168
#define COND_COLUMN_MIDDLE_WIDTH 184
#define COND_COLUMN_PAD 8
#define FORECAST_ROW_COUNT 4

typedef struct {
  lv_obj_t *forecast_view;
  lv_obj_t *conditions_view;
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
  lv_obj_t *conditions_stage_image;
  lv_obj_t *conditions_headline;
  lv_obj_t *conditions_detail;
  lv_obj_t *conditions_ring;
  lv_obj_t *conditions_ticks[COND_TICK_COUNT];
  lv_obj_t *conditions_cardinals[4];
  lv_obj_t *conditions_wind_arrow;
  lv_obj_t *conditions_speed_label;
  lv_obj_t *conditions_unit_label;
  lv_obj_t *conditions_from_label;
  lv_obj_t *conditions_gust_label;
  lv_obj_t *conditions_divider_lines[2];
  lv_obj_t *conditions_change_label;
  lv_obj_t *conditions_columns[COND_COLUMN_COUNT];
  lv_obj_t *conditions_column_icons[COND_COLUMN_COUNT];
  lv_obj_t *conditions_column_labels[COND_COLUMN_COUNT];
  lv_obj_t *conditions_column_values[COND_COLUMN_COUNT];
  lv_obj_t *message_stage_image;
  lv_obj_t *message_face;
  lv_obj_t *message_title_label;
  lv_obj_t *message_card;
  lv_obj_t *message_text_label;
  lv_obj_t *message_meta_label;
  lv_obj_t *message_dismiss_label;
  lv_obj_t *message_empty_label;
  lv_point_precise_t conditions_tick_points[COND_TICK_COUNT][2];
} weather_views_t;

static weather_views_t s_weather;

static const char *const WEEKDAY_SHORT[] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static void update_conditions_view(const app_weather_snapshot_t *weather, bool time_24h);

static lv_color_t color_text_subtle(void)
{
  return lv_color_hex(0x6d819d);
}

static lv_color_t color_important(void)
{
  return lv_color_hex(0xf6d6b1);
}

static lv_color_t color_conditions_accent(void)
{
  return lv_color_hex(0xb8ddff);
}

static lv_color_t color_alert(void)
{
  return lv_color_hex(0xffc466);
}

static lv_color_t color_alert_soft(void)
{
  return lv_color_hex(0xffd296);
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
    lv_image_set_src(s_weather.forecast_stage_image, active ? &round_stage_night : &round_stage_day);
  }

  if (s_weather.forecast_tomorrow_frame != NULL) {
    lv_image_set_src(
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
    lv_image_set_src(s_weather.message_stage_image, active ? &round_stage_night : &round_stage_day);
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
  lv_image_set_src(s_weather.forecast_tomorrow_icon, weather_icon_image_for_name(weather_icon_fallback_name()));
  lv_obj_set_size(s_weather.forecast_tomorrow_icon, FORECAST_TOMORROW_ICON_SIZE, FORECAST_TOMORROW_ICON_SIZE);
  lv_image_set_inner_align(s_weather.forecast_tomorrow_icon, LV_IMAGE_ALIGN_CONTAIN);
  lv_obj_set_style_opa(s_weather.forecast_tomorrow_icon, LV_OPA_TRANSP, 0);
  lv_obj_align(s_weather.forecast_tomorrow_icon, LV_ALIGN_TOP_MID, 0, FORECAST_TOMORROW_ICON_TOP);

  s_weather.forecast_updating_label = wac_ui_create_label(
    s_weather.forecast_view, &wac_font_weather_32, wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, "Updating…"
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
    /* Day label + icon + temp label stack to ~144px (27+90+27), taller than
     * this row's fixed 120px box, so the top/bottom labels were clipped
     * without this — same class of bug as the temp label and message bg.
     * OVERFLOW_VISIBLE alone is not enough: it only widens the clip area
     * passed to children by THIS object's own ext_draw_size, which is 0
     * without a transform style (see lv_refr.c lv_obj_redraw) — the same
     * gap that made the first temp-label fix a no-op. */
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
    lv_image_set_src(s_weather.forecast_row_icons[i], weather_icon_image_for_name(weather_icon_fallback_name()));
    lv_obj_set_size(s_weather.forecast_row_icons[i], FORECAST_ROW_ICON_SIZE, FORECAST_ROW_ICON_SIZE);
    lv_image_set_inner_align(s_weather.forecast_row_icons[i], LV_IMAGE_ALIGN_CONTAIN);
    lv_obj_set_style_opa(s_weather.forecast_row_icons[i], LV_OPA_TRANSP, 0);

    s_weather.forecast_row_temps_labels[i] = wac_ui_create_label(
      row, &lv_font_montserrat_24, lv_color_hex(0xe8f0fb), LV_TEXT_ALIGN_CENTER, "--"
    );
    lv_obj_set_width(s_weather.forecast_row_temps_labels[i], LV_PCT(100));
  }
}

/* Text curved along the top of the circle.
 *
 * Not lv_arclabel: this build has LV_USE_FLOAT off, so lv_arclabel places
 * glyphs with lv_trigo_sin/cos, which take whole degrees -- every letter
 * snaps to the nearest degree (~5.6px at this radius) and the spacing goes
 * uneven ("Ca lm", "fort he"). This does the same layout in double
 * precision: each glyph's baseline midpoint sits exactly on the arc
 * (radius = baseline radius), spaced by its
 * kerned advance and rotated to the tangent. */
typedef struct {
  char text[STORM_TEXT_LEN];
  int32_t radius;
} conditions_arc_text_t;

static conditions_arc_text_t s_headline_arc;
static conditions_arc_text_t s_detail_arc;

/* Minimal UTF-8 decode -- the wording is ASCII plus the degree sign. */
static uint32_t next_utf8_letter(const char *text, size_t *index)
{
  const unsigned char *p = (const unsigned char *) text + *index;
  if (p[0] == 0) {
    return 0;
  }
  if (p[0] < 0x80) {
    *index += 1;
    return p[0];
  }
  if ((p[0] & 0xE0) == 0xC0 && p[1] != 0) {
    *index += 2;
    return ((uint32_t) (p[0] & 0x1F) << 6) | (p[1] & 0x3F);
  }
  if ((p[0] & 0xF0) == 0xE0 && p[1] != 0 && p[2] != 0) {
    *index += 3;
    return ((uint32_t) (p[0] & 0x0F) << 12) | ((uint32_t) (p[1] & 0x3F) << 6) | (p[2] & 0x3F);
  }
  *index += 1;
  return '?';
}

static int32_t arc_text_width(const char *text, const lv_font_t *font)
{
  int32_t total = 0;
  size_t i = 0;
  uint32_t letter = next_utf8_letter(text, &i);
  while (letter != 0) {
    size_t peek = i;
    uint32_t next = next_utf8_letter(text, &peek);
    total += lv_font_get_glyph_width(font, letter, next);
    letter = next;
    i = peek;
  }
  return total;
}

static void conditions_arc_draw_cb(lv_event_t *event)
{
  lv_obj_t *obj = lv_event_get_current_target(event);
  const conditions_arc_text_t *arc = lv_obj_get_user_data(obj);
  if (arc == NULL || arc->text[0] == '\0') {
    return;
  }

  lv_layer_t *layer = lv_event_get_layer(event);
  lv_area_t coords;
  lv_obj_get_coords(obj, &coords);
  const double cx = coords.x1 + lv_area_get_width(&coords) / 2.0;
  const double cy = coords.y1 + lv_area_get_height(&coords) / 2.0;
  const double r = arc->radius;

  const lv_font_t *font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
  lv_draw_letter_dsc_t dsc;
  lv_draw_letter_dsc_init(&dsc);
  dsc.font = font;
  dsc.color = lv_obj_get_style_text_color(obj, LV_PART_MAIN);
  dsc.opa = LV_OPA_MIX2(layer->opa, lv_obj_get_style_text_opa(obj, LV_PART_MAIN));

  /* Centered on 12 o'clock (-90deg in screen coordinates), reading
   * clockwise; `angle` tracks the left edge of the next glyph. */
  double angle = -M_PI / 2.0 - (arc_text_width(arc->text, font) / 2.0) / r;
  size_t i = 0;
  uint32_t letter = next_utf8_letter(arc->text, &i);
  while (letter != 0) {
    size_t peek = i;
    uint32_t next = next_utf8_letter(arc->text, &peek);
    const double advance = lv_font_get_glyph_width(font, letter, next);
    const double mid = angle + (advance / 2.0) / r;

    if (letter != ' ') {
      /* lv_draw_letter() treats `point` as the glyph's pivot: the middle of
       * its advance on the baseline (lv_draw_unit_draw_letter moves the
       * glyph box back by that pivot before drawing). */
      lv_point_t point = {
        .x = (int32_t) lround(cx + r * cos(mid)),
        .y = (int32_t) lround(cy + r * sin(mid)),
      };
      dsc.unicode = letter;
      dsc.rotation = (int32_t) lround((mid * 180.0 / M_PI + 90.0) * 10.0);
      lv_draw_letter(layer, &dsc, &point);
    }

    angle += advance / r;
    letter = next;
    i = peek;
  }
}

static lv_obj_t *create_conditions_arc_label(lv_obj_t *face, conditions_arc_text_t *arc, const lv_font_t *font, lv_color_t color, int32_t radius)
{
  lv_obj_t *obj = lv_obj_create(face);
  wac_ui_clear_container_chrome(obj);
  lv_obj_set_size(obj, COND_FACE_SIZE, COND_FACE_SIZE);
  lv_obj_align(obj, LV_ALIGN_CENTER, 0, 0);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_text_font(obj, font, 0);
  lv_obj_set_style_text_color(obj, color, 0);
  arc->text[0] = '\0';
  arc->radius = radius;
  lv_obj_set_user_data(obj, arc);
  lv_obj_add_event_cb(obj, conditions_arc_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
  return obj;
}

static void set_conditions_arc_text(lv_obj_t *obj, const char *text, const lv_font_t *font)
{
  conditions_arc_text_t *arc = lv_obj_get_user_data(obj);
  if (arc == NULL) {
    return;
  }
  snprintf(arc->text, sizeof(arc->text), "%s", text != NULL ? text : "");
  lv_obj_set_style_text_font(obj, font, 0);
  lv_obj_invalidate(obj);
}

void wac_weather_build_conditions(lv_obj_t *parent)
{
  s_weather.conditions_view = lv_obj_create(parent);
  lv_obj_set_size(s_weather.conditions_view, LV_PCT(100), LV_PCT(100));
  /* Same background treatment as the forecast view: every screen follows
   * the same background logic. */
  lv_obj_set_style_bg_color(s_weather.conditions_view, wac_ui_color_bg(), 0);
  lv_obj_set_style_bg_opa(s_weather.conditions_view, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_weather.conditions_view, 0, 0);
  lv_obj_set_style_pad_all(s_weather.conditions_view, 0, 0);
  lv_obj_set_scrollbar_mode(s_weather.conditions_view, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(s_weather.conditions_view, LV_OBJ_FLAG_HIDDEN);

  s_weather.conditions_stage_image = lv_image_create(s_weather.conditions_view);
  lv_image_set_src(s_weather.conditions_stage_image, &round_stage_day);
  lv_obj_align(s_weather.conditions_stage_image, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t *face = lv_obj_create(s_weather.conditions_view);
  lv_obj_set_size(face, COND_FACE_SIZE, COND_FACE_SIZE);
  lv_obj_align(face, LV_ALIGN_CENTER, 0, 0);
  wac_ui_clear_container_chrome(face);

  /* Headline and detail curve along the top of the circle. */
  s_weather.conditions_headline = create_conditions_arc_label(
    face, &s_headline_arc, &lv_font_montserrat_32, wac_ui_color_text_primary(), COND_HEADLINE_RADIUS
  );
  set_conditions_arc_text(s_weather.conditions_headline, "Checking the weather...", &lv_font_montserrat_32);
  s_weather.conditions_detail = create_conditions_arc_label(
    face, &s_detail_arc, &lv_font_montserrat_18, lv_color_hex(0xebf2ff), COND_DETAIL_RADIUS
  );
  lv_obj_set_style_text_opa(s_weather.conditions_detail, LV_OPA_80, 0);

  /* Compass SVG scaled from 400px by COND_DIAL_SCALE. */
  lv_obj_t *dial = lv_obj_create(face);
  lv_obj_set_size(dial, COND_DIAL_SIZE, COND_DIAL_SIZE);
  lv_obj_align(dial, LV_ALIGN_TOP_MID, 0, COND_DIAL_TOP);
  wac_ui_clear_container_chrome(dial);

  const double dial_c = COND_DIAL_SIZE / 2.0;
  const int32_t ring_d = (int32_t) lround(2 * 182 * COND_DIAL_SCALE);
  s_weather.conditions_ring = lv_obj_create(dial);
  lv_obj_set_size(s_weather.conditions_ring, ring_d, ring_d);
  lv_obj_align(s_weather.conditions_ring, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(s_weather.conditions_ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(s_weather.conditions_ring, lv_color_hex(0x354e7e), 0);
  lv_obj_set_style_bg_opa(s_weather.conditions_ring, 31, 0);
  lv_obj_set_style_border_color(s_weather.conditions_ring, lv_color_hex(0xd6e2f0), 0);
  lv_obj_set_style_border_opa(s_weather.conditions_ring, 66, 0);
  lv_obj_set_style_border_width(s_weather.conditions_ring, 2, 0);
  lv_obj_set_style_pad_all(s_weather.conditions_ring, 0, 0);
  lv_obj_remove_flag(s_weather.conditions_ring, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scrollbar_mode(s_weather.conditions_ring, LV_SCROLLBAR_MODE_OFF);

  /* Ticks every 15deg; the cardinal letters stand in for the 90deg ones. */
  int tick = 0;
  for (int deg = 0; deg < 360 && tick < COND_TICK_COUNT; deg += 15) {
    if (deg % 90 == 0) {
      continue;
    }
    bool major = deg % 45 == 0;
    double rad = deg * M_PI / 180.0;
    double outer = 172 * COND_DIAL_SCALE;
    double inner = (major ? 156 : 164) * COND_DIAL_SCALE;
    s_weather.conditions_tick_points[tick][0] = (lv_point_precise_t) {
      .x = (lv_value_precise_t) (dial_c + outer * sin(rad)),
      .y = (lv_value_precise_t) (dial_c - outer * cos(rad)),
    };
    s_weather.conditions_tick_points[tick][1] = (lv_point_precise_t) {
      .x = (lv_value_precise_t) (dial_c + inner * sin(rad)),
      .y = (lv_value_precise_t) (dial_c - inner * cos(rad)),
    };
    lv_obj_t *line = lv_line_create(dial);
    lv_line_set_points_mutable(line, s_weather.conditions_tick_points[tick], 2);
    lv_obj_set_style_line_width(line, major ? 3 : 2, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(major ? 0xf4f8ff : 0xe0e9f7), 0);
    lv_obj_set_style_line_opa(line, major ? 184 : 87, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
    s_weather.conditions_ticks[tick++] = line;
  }

  static const char *const CARDINAL_TEXT[4] = {"N", "E", "S", "W"};
  for (int i = 0; i < 4; ++i) {
    double rad = i * 90 * M_PI / 180.0;
    double r = 148 * COND_DIAL_SCALE;
    lv_obj_t *label = wac_ui_create_label(dial, &lv_font_montserrat_24, lv_color_hex(0xebf2ff), LV_TEXT_ALIGN_CENTER, CARDINAL_TEXT[i]);
    lv_obj_set_style_text_opa(label, 184, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, (int32_t) lround(r * sin(rad)), (int32_t) lround(-r * cos(rad)));
    s_weather.conditions_cardinals[i] = label;
  }

  /* Weathervane-style arrow (A8, tinted at runtime), rotated about the dial
   * center to the direction the wind comes FROM; fletching upwind, head
   * downwind. */
  s_weather.conditions_wind_arrow = lv_image_create(dial);
  lv_image_set_src(s_weather.conditions_wind_arrow, &conditions_wind_arrow);
  lv_obj_align(s_weather.conditions_wind_arrow, LV_ALIGN_CENTER, 0, 0);
  lv_image_set_pivot(s_weather.conditions_wind_arrow, conditions_wind_arrow.header.w / 2, conditions_wind_arrow.header.h / 2);
  lv_obj_set_style_image_recolor(s_weather.conditions_wind_arrow, color_conditions_accent(), 0);
  lv_obj_set_style_image_recolor_opa(s_weather.conditions_wind_arrow, LV_OPA_COVER, 0);
  lv_obj_add_flag(s_weather.conditions_wind_arrow, LV_OBJ_FLAG_HIDDEN);

  /* Readout inside the compass, centered at y=292 in the face coordinates. */
  lv_obj_t *wind = lv_obj_create(face);
  wac_ui_clear_container_chrome(wind);
  lv_obj_set_size(wind, COND_WIND_WIDTH, LV_SIZE_CONTENT);
  lv_obj_set_layout(wind, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(wind, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(wind, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(wind, 2, 0);
  lv_obj_align(wind, LV_ALIGN_CENTER, 0, COND_WIND_CENTER_Y - COND_FACE_CENTER);

  lv_obj_t *speed_row = lv_obj_create(wind);
  wac_ui_clear_container_chrome(speed_row);
  lv_obj_set_size(speed_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_layout(speed_row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(speed_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(speed_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(speed_row, 8, 0);
  s_weather.conditions_speed_label = wac_ui_create_label(speed_row, &lv_font_wind_speed_70, wac_ui_color_text_primary(), LV_TEXT_ALIGN_CENTER, "--");
  s_weather.conditions_unit_label = wac_ui_create_label(speed_row, &lv_font_montserrat_24, lv_color_hex(0xebf2ff), LV_TEXT_ALIGN_LEFT, "mph");
  lv_obj_set_style_text_opa(s_weather.conditions_unit_label, 184, 0);
  lv_obj_set_style_pad_bottom(s_weather.conditions_unit_label, 10, 0);

  s_weather.conditions_from_label = wac_ui_create_label(wind, &lv_font_montserrat_18, lv_color_hex(0xe8f0fb), LV_TEXT_ALIGN_CENTER, "");
  lv_obj_set_width(s_weather.conditions_from_label, LV_PCT(100));
  s_weather.conditions_gust_label = wac_ui_create_label(wind, &lv_font_montserrat_18, lv_color_hex(0xe8f0fb), LV_TEXT_ALIGN_CENTER, "");
  lv_obj_set_width(s_weather.conditions_gust_label, LV_PCT(100));

  /* "Conditions changing / may change / steady" */
  lv_obj_t *divider = lv_obj_create(face);
  wac_ui_clear_container_chrome(divider);
  lv_obj_set_size(divider, COND_DIVIDER_WIDTH, LV_SIZE_CONTENT);
  lv_obj_align(divider, LV_ALIGN_TOP_MID, 0, COND_DIVIDER_TOP);
  lv_obj_set_layout(divider, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(divider, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(divider, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(divider, 14, 0);
  for (int i = 0; i < 2; ++i) {
    lv_obj_t *line = lv_obj_create(divider);
    wac_ui_clear_container_chrome(line);
    lv_obj_set_height(line, 1);
    lv_obj_set_flex_grow(line, 1);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x93d1ff), 0);
    lv_obj_set_style_bg_opa(line, 89, 0); /* 35% of 255. */
    s_weather.conditions_divider_lines[i] = line;
    if (i == 0) {
      s_weather.conditions_change_label = wac_ui_create_label(divider, &lv_font_montserrat_18, color_conditions_accent(), LV_TEXT_ALIGN_CENTER, "");
    }
  }

  /* Three columns: icon, label, one value. */
  lv_obj_t *columns = lv_obj_create(face);
  wac_ui_clear_container_chrome(columns);
  lv_obj_set_size(columns, 2 * COND_COLUMN_SIDE_WIDTH + COND_COLUMN_MIDDLE_WIDTH, LV_SIZE_CONTENT);
  lv_obj_align(columns, LV_ALIGN_TOP_MID, 0, COND_COLUMNS_TOP);
  lv_obj_set_layout(columns, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(columns, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(columns, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  static const char *const COLUMN_LABELS[COND_COLUMN_COUNT] = {"WIND", "PRESSURE", "RAIN"};
  const lv_image_dsc_t *column_icons[COND_COLUMN_COUNT] = {
    &conditions_icon_wind, &conditions_icon_pressure, &conditions_icon_rain
  };
  for (int i = 0; i < COND_COLUMN_COUNT; ++i) {
    lv_obj_t *column = lv_obj_create(columns);
    wac_ui_clear_container_chrome(column);
    lv_obj_set_size(column, i == 1 ? COND_COLUMN_MIDDLE_WIDTH : COND_COLUMN_SIDE_WIDTH, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(column, COND_COLUMN_PAD, 0);
    lv_obj_set_layout(column, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(column, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(column, 3, 0);
    if (i > 0) {
      lv_obj_set_style_border_side(column, LV_BORDER_SIDE_LEFT, 0);
      lv_obj_set_style_border_width(column, 1, 0);
      lv_obj_set_style_border_color(column, color_conditions_accent(), 0);
      lv_obj_set_style_border_opa(column, 46, 0);
    }

    lv_obj_t *icon = lv_image_create(column);
    lv_image_set_src(icon, column_icons[i]);
    lv_obj_set_style_image_recolor(icon, color_conditions_accent(), 0);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);

    lv_obj_t *label = wac_ui_create_label(column, &lv_font_montserrat_14, color_conditions_accent(), LV_TEXT_ALIGN_CENTER, COLUMN_LABELS[i]);
    lv_obj_set_style_text_letter_space(label, 2, 0);

    lv_obj_t *value = wac_ui_create_label(column, &lv_font_montserrat_24, wac_ui_color_text_primary(), LV_TEXT_ALIGN_CENTER, "--");
    lv_obj_set_width(value, LV_PCT(100));

    s_weather.conditions_columns[i] = column;
    s_weather.conditions_column_icons[i] = icon;
    s_weather.conditions_column_labels[i] = label;
    s_weather.conditions_column_values[i] = value;
  }
}

void wac_weather_build_message(lv_obj_t *parent)
{
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

static void update_weather_labels(const app_weather_snapshot_t *weather, const struct tm *local_calendar,
  bool time_24h)
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
    lv_image_set_src(
      s_weather.forecast_tomorrow_icon,
      weather_icon_image_for_name(weather->forecast[0].icon_name)
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
      lv_image_set_src(
        s_weather.forecast_row_icons[i],
        weather_icon_image_for_name(weather->forecast[item_index].icon_name)
      );
    }
  }

  update_conditions_view(weather, time_24h);
}

static int32_t measure_text_width(const char *text, const lv_font_t *font, int32_t letter_space)
{
  lv_point_t size;
  lv_text_get_size(&size, text != NULL ? text : "", font, letter_space, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  return size.x;
}

/* Choose the largest available fixed font that fits the existing width. */
static const lv_font_t *pick_font_to_fit(const char *text, const lv_font_t *const *fonts, int count, int32_t max_width)
{
  for (int i = 0; i < count; ++i) {
    if (measure_text_width(text, fonts[i], 0) <= max_width) {
      return fonts[i];
    }
  }
  return fonts[count - 1];
}

/* Colors depend only on the supplied weather and current appearance. */
static void apply_conditions_colors(const app_weather_snapshot_t *weather, bool night)
{
  if (s_weather.conditions_view == NULL) {
    return;
  }

  const storm_conditions_t *c = &weather->conditions;
  bool have = weather->valid && weather->has_conditions;
  bool alert = have && c->alert;
  bool wind_alert = false;
  bool pressure_alert = false;
  bool precip_alert = false;
  for (int i = 0; have && i < c->alert_count; ++i) {
    storm_alert_kind_t kind = c->alerts[i].kind;
    wind_alert = wind_alert || kind == STORM_ALERT_GUSTS || kind == STORM_ALERT_WIND;
    pressure_alert = pressure_alert || kind == STORM_ALERT_PRESSURE;
    precip_alert = precip_alert || kind == STORM_ALERT_PRECIP || kind == STORM_ALERT_THUNDER;
  }

  lv_color_t alert_color = night ? wac_ui_color_night_alert() : color_alert();
  lv_color_t accent = night ? wac_ui_color_night_muted() : color_conditions_accent();
  lv_color_t primary = night ? wac_ui_color_night_primary() : wac_ui_color_text_primary();

  lv_image_set_src(s_weather.conditions_stage_image, night ? &round_stage_night : &round_stage_day);
  lv_obj_set_style_text_color(s_weather.conditions_headline, alert ? alert_color : primary, 0);
  lv_obj_set_style_text_color(
    s_weather.conditions_detail,
    alert ? (night ? wac_ui_color_night_alert() : color_alert_soft()) : (night ? wac_ui_color_night_muted() : lv_color_hex(0xebf2ff)),
    0
  );

  lv_obj_set_style_border_color(s_weather.conditions_ring, night ? wac_ui_color_night_muted() : lv_color_hex(0xd6e2f0), 0);
  lv_obj_set_style_bg_color(s_weather.conditions_ring, night ? lv_color_hex(0x521e1e) : lv_color_hex(0x354e7e), 0);
  for (int i = 0; i < COND_TICK_COUNT; ++i) {
    if (s_weather.conditions_ticks[i] != NULL) {
      lv_obj_set_style_line_color(s_weather.conditions_ticks[i], night ? wac_ui_color_night_muted() : lv_color_hex(0xe0e9f7), 0);
    }
  }
  for (int i = 0; i < 4; ++i) {
    lv_obj_set_style_text_color(s_weather.conditions_cardinals[i], night ? wac_ui_color_night_muted() : lv_color_hex(0xebf2ff), 0);
  }
  lv_obj_set_style_image_recolor(
    s_weather.conditions_wind_arrow,
    night ? wac_ui_color_night_alert() : (wind_alert ? color_alert() : color_conditions_accent()),
    0
  );

  lv_obj_set_style_text_color(s_weather.conditions_speed_label, primary, 0);
  lv_obj_set_style_text_color(s_weather.conditions_unit_label, night ? wac_ui_color_night_muted() : lv_color_hex(0xebf2ff), 0);
  lv_obj_set_style_text_color(s_weather.conditions_from_label, night ? wac_ui_color_night_muted() : lv_color_hex(0xe8f0fb), 0);
  lv_obj_set_style_text_color(
    s_weather.conditions_gust_label,
    wind_alert ? alert_color : (night ? wac_ui_color_night_muted() : lv_color_hex(0xe8f0fb)),
    0
  );

  lv_obj_set_style_text_color(
    s_weather.conditions_change_label,
    night ? wac_ui_color_night_primary() : (have && c->changing ? lv_color_hex(0xd6eaff) : color_conditions_accent()),
    0
  );
  for (int i = 0; i < 2; ++i) {
    lv_obj_set_style_bg_color(s_weather.conditions_divider_lines[i],
      lv_color_hex(night ? 0xe1b6b6 : 0x93d1ff), 0);
  }

  bool column_alert[COND_COLUMN_COUNT] = {wind_alert, pressure_alert, precip_alert};
  for (int i = 0; i < COND_COLUMN_COUNT; ++i) {
    lv_obj_set_style_border_color(s_weather.conditions_columns[i], accent, 0);
    lv_obj_set_style_image_recolor(
      s_weather.conditions_column_icons[i],
      night ? wac_ui_color_night_muted() : (column_alert[i] ? color_alert() : color_conditions_accent()),
      0
    );
    lv_obj_set_style_text_color(s_weather.conditions_column_labels[i], accent, 0);
    lv_obj_set_style_text_color(s_weather.conditions_column_values[i], column_alert[i] ? alert_color : primary, 0);
  }
}

/* Wording comes from storm_conditions.c; this function only places it. */
static void update_conditions_view(const app_weather_snapshot_t *weather, bool time_24h)
{
  if (s_weather.conditions_view == NULL) {
    return;
  }

  bool have = weather->valid && weather->has_conditions;
  const storm_conditions_t *c = &weather->conditions;

  static const lv_font_t *const HEADLINE_FONTS[] = {&lv_font_montserrat_32, &lv_font_montserrat_24};
  static const lv_font_t *const DETAIL_FONTS[] = {&lv_font_montserrat_18, &lv_font_montserrat_14};
  static const lv_font_t *const VALUE_FONTS[] = {&lv_font_montserrat_24, &lv_font_montserrat_18};
  static const lv_font_t *const READOUT_FONTS[] = {&lv_font_montserrat_18, &lv_font_montserrat_14};

  bool original_format = time_24h == weather->conditions_time_24h;
  const char *headline = have ? (original_format ? c->headline : weather->conditions_alternate_headline)
    : (weather->valid ? "Weather unavailable" : "Checking the weather...");
  const char *detail = have && c->has_detail
    ? (original_format ? c->detail : weather->conditions_alternate_detail) : "";
  set_conditions_arc_text(s_weather.conditions_headline, headline, pick_font_to_fit(headline, HEADLINE_FONTS, 2, COND_HEADLINE_MAX_PX));
  set_conditions_arc_text(s_weather.conditions_detail, detail, pick_font_to_fit(detail, DETAIL_FONTS, 2, COND_DETAIL_MAX_PX));

  char speed[8] = "--";
  char gust[32] = "";
  if (have && c->has_speed) {
    snprintf(speed, sizeof(speed), "%d", c->speed);
  }
  if (have && c->has_gust) {
    snprintf(gust, sizeof(gust), "gusts to %d %s", c->gust, c->wind_unit);
  }
  wac_ui_set_label_text(s_weather.conditions_speed_label, speed);
  wac_ui_set_label_text(s_weather.conditions_unit_label, have ? c->wind_unit : "");
  const char *from = have && c->has_direction ? c->from : "";
  lv_obj_set_style_text_font(s_weather.conditions_from_label, pick_font_to_fit(from, READOUT_FONTS, 2, COND_WIND_WIDTH), 0);
  wac_ui_set_label_text(s_weather.conditions_from_label, from);
  lv_obj_set_style_text_font(s_weather.conditions_gust_label, pick_font_to_fit(gust, READOUT_FONTS, 2, COND_WIND_WIDTH), 0);
  wac_ui_set_label_text(s_weather.conditions_gust_label, gust);

  if (have && c->has_direction) {
    lv_obj_clear_flag(s_weather.conditions_wind_arrow, LV_OBJ_FLAG_HIDDEN);
    lv_image_set_rotation(s_weather.conditions_wind_arrow, (int32_t) lround(c->direction * 10.0));
  } else {
    lv_obj_add_flag(s_weather.conditions_wind_arrow, LV_OBJ_FLAG_HIDDEN);
  }

  wac_ui_set_label_text(s_weather.conditions_change_label, have ? c->change_label : "");

  const char *values[COND_COLUMN_COUNT] = {
    have ? c->outlook_status : "--",
    have && c->pressure_status[0] != '\0' ? c->pressure_status : "--",
    have ? c->precip_status : "--",
  };
  for (int i = 0; i < COND_COLUMN_COUNT; ++i) {
    int32_t width = (i == 1 ? COND_COLUMN_MIDDLE_WIDTH : COND_COLUMN_SIDE_WIDTH) - 2 * COND_COLUMN_PAD;
    lv_obj_set_style_text_font(s_weather.conditions_column_values[i], pick_font_to_fit(values[i], VALUE_FONTS, 2, width), 0);
    wac_ui_set_label_text(s_weather.conditions_column_values[i], values[i]);
  }

  bool snow = have && strcmp(c->precip_label, "Snow") == 0;
  lv_image_set_src(s_weather.conditions_column_icons[2], snow ? &conditions_icon_snow : &conditions_icon_rain);
  wac_ui_set_label_text(s_weather.conditions_column_labels[2], snow ? "SNOW" : "RAIN");
}

void wac_weather_update_status(const char *status_text, const app_weather_snapshot_t *weather,
  bool night, wac_weather_surface_t surface)
{
  bool unavailable_home = surface == WAC_WEATHER_SURFACE_UNAVAILABLE_HOME;
  if (night || surface == WAC_WEATHER_SURFACE_MESSAGE || unavailable_home) {
    wac_ui_set_label_text(s_weather.weather_status_label, "");
    lv_obj_add_flag(s_weather.weather_status_label, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  bool weather_view = surface == WAC_WEATHER_SURFACE_FORECAST || surface == WAC_WEATHER_SURFACE_CONDITIONS;
  bool have_status = status_text != NULL && status_text[0] != '\0';
  bool data_warning = weather_view
    && ((!have_status && weather->valid && weather->stale)
      || (have_status && (!strcmp(status_text, "Weather request failed. Retrying...")
        || !strcmp(status_text, "Weather refresh failed. Showing last update until retry."))));
  if (have_status || data_warning) {
    if (data_warning) {
      bool forecast = surface == WAC_WEATHER_SURFACE_FORECAST;
      lv_obj_set_width(s_weather.status_stack, forecast ? 440 : 500);
      lv_obj_align(s_weather.status_stack, LV_ALIGN_TOP_LEFT, forecast ? 180 : 150, forecast ? 48 : 680);
      lv_obj_set_style_text_font(s_weather.weather_status_label, &lv_font_montserrat_22, 0);
      lv_obj_set_style_text_opa(s_weather.weather_status_label, LV_OPA_COVER, 0);
      lv_obj_set_style_text_letter_space(s_weather.weather_status_label, 0, 0);
      status_text = weather->valid ? "Weather data stale" : "Weather update failed.";
    } else {
      lv_obj_set_width(s_weather.status_stack, 360);
      lv_obj_align(s_weather.status_stack, LV_ALIGN_BOTTOM_MID, 0, -56);
      lv_obj_set_style_text_font(s_weather.weather_status_label, &lv_font_montserrat_18, 0);
      lv_obj_set_style_text_opa(s_weather.weather_status_label, LV_OPA_70, 0);
      lv_obj_set_style_text_letter_space(s_weather.weather_status_label, 1, 0);
    }
    wac_ui_set_label_text(s_weather.weather_status_label, status_text);
    lv_obj_clear_flag(s_weather.weather_status_label, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  wac_ui_set_label_text(s_weather.weather_status_label, "");
  lv_obj_add_flag(s_weather.weather_status_label, LV_OBJ_FLAG_HIDDEN);
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
  memset(&s_weather, 0, sizeof(s_weather));
}

void wac_weather_set_visible(bool forecast, bool conditions, bool message)
{
  lv_obj_t *views[] = {s_weather.forecast_view, s_weather.conditions_view, s_weather.message_view};
  bool visible[] = {forecast, conditions, message};
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
  apply_forecast_night_state(weather, active);
  apply_message_night_state(important, active);
  apply_status_night_state(active);
  apply_conditions_colors(weather, active);
}

void wac_weather_update(const app_weather_snapshot_t *weather, const struct tm *local_calendar,
  bool time_24h)
{
  if (weather->valid) update_weather_labels(weather, local_calendar, time_24h);
  else update_pending_weather_labels();
}
