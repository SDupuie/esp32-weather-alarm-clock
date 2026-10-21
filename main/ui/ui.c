#include "app_ui.h"
#include "ui_private.h"
#include "assets/weather_icons.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "message_service.h"
#include "location_scoring.h"
#include "weather_client.h"

#define SWIPE_THRESHOLD_PX 70
#define SWIPE_AXIS_RATIO_X10 15
#define CLOCK_STALE_WARNING_MS (135ULL * 1000ULL)

typedef enum {
  APP_VIEW_ANALOG = 0,
  APP_VIEW_DIGITAL,
  APP_VIEW_FORECAST,
  APP_VIEW_RADAR,
  APP_VIEW_MESSAGE,
  APP_VIEW_ALERTS,
  APP_VIEW_ALERT_DETAIL
} app_view_mode_t;

typedef struct {
  lv_obj_t *stage;
  lv_obj_t *gesture_layer;
  lv_obj_t *night_overlay;
  wac_settings_view_t settings_view;
  bool settings_suspended;
  app_view_mode_t current_view;
  app_view_mode_t last_home_view;
  app_alert_snapshot_t alerts;
  uint64_t alert_detail_id;
  lv_point_t gesture_start;
  bool gesture_tracking;
  bool backup_notice_open;
  wac_time_warning_t backup_dismiss_warning;
  bool night_overlay_active;
  bool has_unread_messages;
  char displayed_message_id[APP_MESSAGE_ID_LEN];
  /* Stays pending across navigation until the sole outstanding result arrives. */
  bool message_ack_pending;
  bool message_ack_return_allowed;
  bool message_held;
  bool has_important_messages;
  app_weather_snapshot_t weather;
  app_radar_snapshot_t radar;
  bool radar_visit_active;
  bool radar_pause_pending;
  uint32_t radar_displayed_generation;
  device_config_t config_snapshot;
  int last_displayed_minute_key;
  int64_t last_minute_advance_ms;
  wac_time_snapshot_t clock;
  wac_alarm_snapshot_t alarm;
  wac_audio_snapshot_t audio;
  bool snoozed_view;
  bool alarm_action_pending;
  app_alarm_action_t pending_alarm_action;
  wac_alarm_occurrence_t pending_alarm_occurrence;
  bool alarm_action_return_allowed;
  bool alarm_slide_pending;
  lv_timer_t *alarm_confirmation_timer;
} app_ui_state_t;

static app_ui_state_t s_ui;
static esp_err_t (*s_clock_warning_dismiss_handler)(wac_time_warning_t);
static esp_err_t (*s_alarm_action_handler)(app_alarm_action_t, const wac_alarm_occurrence_t *);
static esp_err_t (*s_radar_action_handler)(app_radar_action_t, time_t);
static const char *TAG = "rwd_app_ui";

lv_color_t wac_ui_color_bg(void)
{
  return lv_color_hex(0x07101c);
}

lv_color_t wac_ui_color_text_primary(void)
{
  return lv_color_hex(0xf5f8ff);
}

lv_color_t wac_ui_color_text_muted(void)
{
  return lv_color_hex(0xa6b6cf);
}

lv_color_t wac_ui_color_night_primary(void)
{
  return lv_color_hex(0xecc0c0);
}

lv_color_t wac_ui_color_night_muted(void)
{
  return lv_color_hex(0xc17a7a);
}

bool wac_ui_strings_equal_ignore_case(const char *a, const char *b)
{
  if (a == NULL || b == NULL) {
    return false;
  }

  while (*a != '\0' && *b != '\0') {
    if (tolower((unsigned char) *a) != tolower((unsigned char) *b)) {
      return false;
    }
    a++;
    b++;
  }

  return *a == '\0' && *b == '\0';
}

void wac_ui_copy_uppercase_string(char *dest, size_t dest_size, const char *src)
{
  if (dest == NULL || dest_size == 0) {
    return;
  }

  if (src == NULL) {
    dest[0] = '\0';
    return;
  }

  size_t index = 0;
  while (src[index] != '\0' && index + 1 < dest_size) {
    dest[index] = (char) toupper((unsigned char) src[index]);
    index++;
  }
  dest[index] = '\0';
}

void wac_ui_set_label_text(lv_obj_t *label, const char *text)
{
  if (label == NULL) {
    return;
  }

  const char *next_text = text != NULL ? text : "";
  const char *current_text = lv_label_get_text(label);
  if (current_text != NULL && strcmp(current_text, next_text) == 0) {
    return;
  }

  lv_label_set_text(label, next_text);
}

bool wac_ui_set_weather_image(lv_obj_t *image, const lv_image_dsc_t *source, int32_t size)
{
  if (source != NULL && lv_image_get_src(image) == source) return false;

  /* Keep the original canvas as the layout box. Scaling the stored crop with
   * CONTAIN would enlarge differently shaped icons by different amounts. */
  lv_image_set_src(image, source);
  lv_obj_set_size(image, size, size);
  lv_image_set_inner_align(image, LV_IMAGE_ALIGN_TOP_LEFT);
  const weather_icon_geometry_t *geometry = weather_icon_geometry_for_image(source);
  int32_t x = geometry ? geometry->crop_x : 0;
  int32_t y = geometry ? geometry->crop_y : 0;
  lv_image_set_offset_x(image, x);
  lv_image_set_offset_y(image, y);
  /* The crop begins at (x,y), so its negative pivot scales around the
   * original canvas origin and keeps its intended position in that box. */
  lv_image_set_pivot(image, -x, -y);
  lv_image_set_scale(image, geometry ? (uint32_t)size * LV_SCALE_NONE / geometry->canvas_size : LV_SCALE_NONE);
  return true;
}

lv_obj_t *wac_ui_create_label(
  lv_obj_t *parent,
  const lv_font_t *font,
  lv_color_t color,
  lv_text_align_t align,
  const char *text
)
{
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text != NULL ? text : "");
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_align(label, align, 0);
  if (font != NULL) {
    lv_obj_set_style_text_font(label, font, 0);
  }
  return label;
}

void wac_ui_clear_container_chrome(lv_obj_t *obj)
{
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_outline_width(obj, 0, 0);
  lv_obj_set_style_radius(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_style_pad_row(obj, 0, 0);
  lv_obj_set_style_pad_column(obj, 0, 0);
  lv_obj_set_style_shadow_width(obj, 0, 0);
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

lv_obj_t *wac_ui_create_weather_credit(lv_obj_t *parent)
{
  lv_obj_t *credit = lv_arclabel_create(parent);
  wac_ui_clear_container_chrome(credit);
  /* Use the whole screen, not the inset 760/780px clock/forecast faces.
   * DEFAULT vertical alignment puts the glyph baseline on this circle. */
  lv_obj_set_size(credit, 800, 800);
  lv_obj_set_pos(credit, 0, 0);
  lv_obj_set_style_text_font(credit, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(credit, wac_ui_color_text_muted(), 0);
  lv_obj_set_style_text_letter_space(credit, 0, 0);
  lv_arclabel_set_radius(credit, 387);
  lv_arclabel_set_angle_start(credit, 0);
  lv_arclabel_set_angle_size(credit, 180);
  lv_arclabel_set_dir(credit, LV_ARCLABEL_DIR_COUNTER_CLOCKWISE);
  lv_arclabel_set_text_horizontal_align(credit, LV_ARCLABEL_TEXT_ALIGN_CENTER);
  lv_arclabel_set_text_vertical_align(credit, LV_ARCLABEL_TEXT_ALIGN_DEFAULT);
  lv_arclabel_set_text_static(credit, "");
  lv_obj_add_flag(credit, LV_OBJ_FLAG_HIDDEN);
  return credit;
}

void wac_ui_update_weather_credit(lv_obj_t *credit,
  const app_weather_snapshot_t *weather, bool visible, bool night)
{
  if (credit == NULL) return;
  const char *text = "";
  if (weather != NULL && weather->valid) {
    switch (weather->provider) {
      case DEVICE_CONFIG_WEATHER_OPEN_METEO: text = "Weather by Open-Meteo.com"; break;
      case DEVICE_CONFIG_WEATHER_VISUAL_CROSSING: text = "Weather by Visual Crossing"; break;
      default: break;
    }
  }
  /* LVGL's arc label has no text getter. Retain only its immutable text
   * pointer so an unchanged heartbeat does not reset the widget. */
  if (lv_obj_get_user_data(credit) != text) {
    lv_arclabel_set_text_static(credit, text);
    lv_obj_set_user_data(credit, (void *)text);
  }
  lv_color_t color = night ? wac_ui_color_night_muted() : wac_ui_color_text_muted();
  if (!lv_color_eq(lv_obj_get_style_text_color(credit, 0), color))
    lv_obj_set_style_text_color(credit, color, 0);
  bool hidden = !visible || text[0] == '\0';
  if (hidden != lv_obj_has_flag(credit, LV_OBJ_FLAG_HIDDEN)) {
    if (hidden) lv_obj_add_flag(credit, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(credit, LV_OBJ_FLAG_HIDDEN);
  }
}

static void yield_ui_bootstrap(void)
{
  vTaskDelay(pdMS_TO_TICKS(1));
}

static void open_settings(void);
static void update_backup_notice(void);
static void machine_open_event_cb(lv_event_t *event);
static void alerts_open_event_cb(lv_event_t *event);
static void alert_dismiss_event_cb(lv_event_t *event);
static void settings_open_event_cb(lv_event_t *event);
static void alerts_back_event_cb(lv_event_t *event);
static void alert_row_event_cb(lv_event_t *event);
static void apply_view_night_state(bool active);
static void update_weather_status(void);
static void update_alarm_surface(void);
static void update_clock_status(const wac_time_snapshot_t *clock);
static void refresh_message_ui_state(
  const app_message_snapshot_t *snapshot,
  uint32_t unread_count,
  bool has_important_message
);

void app_ui_set_radar_action_handler(esp_err_t (*handler)(app_radar_action_t, time_t))
{
  s_radar_action_handler = handler;
}

static bool submit_radar_action(app_radar_action_t action)
{
  if (action == APP_RADAR_PAUSE && s_ui.radar_pause_pending) return false;
  esp_err_t result = s_radar_action_handler
    ? s_radar_action_handler(action, s_ui.radar.frame_time) : ESP_ERR_INVALID_STATE;
  if (result != ESP_OK && s_radar_action_handler)
    ESP_LOGW(TAG, "Radar action submission failed: %s", esp_err_to_name(result));
  if (result == ESP_OK) s_ui.radar_pause_pending = action == APP_RADAR_PAUSE;
  return result == ESP_OK;
}

uint32_t app_ui_radar_displayed_generation(void)
{
  return s_ui.radar_displayed_generation;
}

static void reset_clock_health(void)
{
  s_ui.last_displayed_minute_key = -1;
  s_ui.last_minute_advance_ms = esp_timer_get_time() / 1000;
}

static void update_clock_health(const struct tm *local_time)
{
  if (local_time == NULL) {
    reset_clock_health();
    return;
  }

  int minute_key = (local_time->tm_yday * 1440) + (local_time->tm_hour * 60) + local_time->tm_min;
  int64_t now_ms = esp_timer_get_time() / 1000;
  if (minute_key != s_ui.last_displayed_minute_key) {
    s_ui.last_displayed_minute_key = minute_key;
    s_ui.last_minute_advance_ms = now_ms;
  }
}

static bool clock_is_stale(void)
{
  if (s_ui.last_displayed_minute_key < 0) {
    return false;
  }

  int64_t now_ms = esp_timer_get_time() / 1000;
  return (uint64_t) (now_ms - s_ui.last_minute_advance_ms) > CLOCK_STALE_WARNING_MS;
}

static bool settings_visible(void)
{
  return s_ui.settings_view != WAC_SETTINGS_NONE && !s_ui.settings_suspended;
}

static bool alarm_ringing(void) { return s_ui.alarm.phase == WAC_ALARM_RINGING; }

static bool alarm_preempting(void)
{
  return alarm_ringing() || s_ui.alarm_confirmation_timer || s_ui.alarm_slide_pending;
}

static bool alarm_page(void)
{
  return alarm_preempting() || (s_ui.snoozed_view
    && s_ui.alarm.phase == WAC_ALARM_SNOOZED && !settings_visible());
}

static bool same_occurrence(const wac_alarm_occurrence_t *a, const wac_alarm_occurrence_t *b)
{
  return a->valid == b->valid && a->utc == b->utc
    && a->local_date == b->local_date && a->minute == b->minute;
}

void wac_ui_settings_navigate(wac_settings_view_t view)
{
  if (!s_ui.settings_suspended) s_ui.alarm_action_return_allowed = false;
  if (view != WAC_SETTINGS_NONE) {
    s_ui.message_ack_return_allowed = false;
    s_ui.message_held = false;
    wac_clock_update_status("", s_ui.current_view == APP_VIEW_ANALOG, false);
  }
  s_ui.backup_notice_open = false;
  s_ui.settings_view = view;
  update_backup_notice();
  wac_settings_render(view);
  wac_settings_set_visible(!s_ui.settings_suspended && !alarm_preempting());
  update_alarm_surface();
}

wac_settings_view_t wac_ui_settings_view(void)
{
  return s_ui.settings_view;
}

void wac_ui_settings_close(void)
{
  if (!s_ui.config_snapshot.location_ready) {
    wac_settings_start_weather(&s_ui.config_snapshot);
    wac_ui_settings_navigate(WAC_SETTINGS_WEATHER);
    return;
  }
  s_ui.settings_view = WAC_SETTINGS_NONE;
  /* A background Save may close an alarm-hidden editor before a pending
   * Snooze/Dismiss result arrives. Keep that action's return destination. */
  if (!s_ui.settings_suspended) s_ui.alarm_action_return_allowed = false;
  s_ui.settings_suspended = false;
  wac_settings_close();
  update_backup_notice();
  update_alarm_surface();
}

static void open_settings(void)
{
  if (alarm_preempting()) return;
  if (settings_visible()) return;
  s_ui.snoozed_view = false;
  if (s_ui.settings_suspended && s_ui.settings_view != WAC_SETTINGS_NONE) {
    s_ui.settings_suspended = false;
    s_ui.alarm_action_return_allowed = false;
    wac_settings_set_visible(true);
    update_backup_notice();
    update_alarm_surface();
    return;
  }
  wac_settings_apply_night(s_ui.night_overlay_active);
  wac_settings_begin(s_ui.stage, &s_ui.config_snapshot);
  wac_ui_settings_navigate(!s_ui.config_snapshot.location_ready
    ? (s_ui.config_snapshot.wifi_ready ? WAC_SETTINGS_WEATHER : WAC_SETTINGS_DEVICE_SETUP)
    : WAC_SETTINGS_ENTRY);
}

static void update_night_overlay_visibility(void)
{
  if (s_ui.night_overlay == NULL) {
    return;
  }

  bool show_overlay = !alarm_page() && s_ui.night_overlay_active && s_ui.current_view == APP_VIEW_MESSAGE;
  if (show_overlay) {
    lv_obj_clear_flag(s_ui.night_overlay, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(s_ui.night_overlay, LV_OBJ_FLAG_HIDDEN);
  }
}

void wac_ui_apply_image_night_tint(lv_obj_t *image, bool active, lv_color_t tint, lv_opa_t opa)
{
  if (image == NULL) {
    return;
  }

  lv_obj_set_style_image_recolor(image, tint, 0);
  lv_obj_set_style_image_recolor_opa(image, active ? opa : LV_OPA_TRANSP, 0);
}

static void apply_view_night_state(bool active)
{
  wac_clock_apply_night(active);
  wac_weather_apply_night(&s_ui.weather, s_ui.has_important_messages, active);
  wac_clock_update_indicators(s_ui.has_unread_messages, s_ui.has_important_messages,
    s_ui.night_overlay_active);
  update_night_overlay_visibility();
  update_backup_notice();
}

static void set_view_mode(app_view_mode_t next_view)
{
  s_ui.alarm_action_return_allowed = false;
  s_ui.snoozed_view = false;
  if (next_view != s_ui.current_view) {
    s_ui.message_ack_return_allowed = false;
    s_ui.message_held = false;
    wac_weather_set_message_dismiss_text(
      s_ui.message_ack_pending ? "Dismissing..." : "Tap to dismiss");
  }
  s_ui.current_view = next_view;
  if (next_view == APP_VIEW_ANALOG || next_view == APP_VIEW_DIGITAL) {
    s_ui.last_home_view = next_view;
  } else {
    reset_clock_health();
    s_ui.backup_notice_open = false;
  }

  wac_clock_set_visible(next_view == APP_VIEW_ANALOG, next_view == APP_VIEW_DIGITAL, s_ui.clock.usable);
  if (next_view != APP_VIEW_ANALOG && next_view != APP_VIEW_DIGITAL)
    wac_clock_update_status("", false, false);

  wac_weather_set_visible(next_view == APP_VIEW_FORECAST,
    next_view == APP_VIEW_RADAR, next_view == APP_VIEW_MESSAGE);

  apply_view_night_state(s_ui.night_overlay_active);
  update_weather_status();
  update_alarm_surface();
}

static bool radar_enabled(void)
{
  return s_ui.config_snapshot.location_ready
    && location_scoring_label_has_country(s_ui.config_snapshot.location, "US");
}

/* Country eligibility overrides a held visit. A removed surface must release
 * its external pixel reference before main acknowledges the publication. */
static void update_radar_eligibility(void)
{
  bool enabled = radar_enabled();
  wac_weather_set_radar_enabled(s_ui.stage, enabled);
  if (!enabled) {
    if (s_ui.current_view == APP_VIEW_RADAR) s_ui.current_view = APP_VIEW_FORECAST;
    s_ui.radar = (app_radar_snapshot_t){0};
    s_ui.radar_pause_pending = false;
  }
}

/* Navigation map:
 * only the configured face is home, and every screen is a fixed neighbour
 * of home that returns with the opposite swipe:
 *   right -> messages
 *   left  -> forecast -> Radar
 *   down  -> Settings (anywhere on the face)
 * Swipe up from home is reserved for a future view below home. */
static void handle_swipe(int delta_x, int delta_y)
{
  if (alarm_preempting() || settings_visible() || (s_ui.backup_notice_open && !alarm_page())) {
    return;
  }

  int abs_x = LV_ABS(delta_x);
  int abs_y = LV_ABS(delta_y);
  bool is_home = s_ui.current_view == APP_VIEW_ANALOG || s_ui.current_view == APP_VIEW_DIGITAL;

  if (abs_x >= SWIPE_THRESHOLD_PX && (abs_x * 10) > (abs_y * SWIPE_AXIS_RATIO_X10)) {
    if (is_home) {
      if (delta_x < 0) {
        set_view_mode(APP_VIEW_FORECAST);
      } else {
        set_view_mode(APP_VIEW_MESSAGE);
      }
      return;
    }

    if (s_ui.current_view == APP_VIEW_FORECAST) {
      if (delta_x > 0) set_view_mode(s_ui.last_home_view);
      else if (radar_enabled()) set_view_mode(APP_VIEW_RADAR);
      return;
    }
    if (s_ui.current_view == APP_VIEW_RADAR) {
      if (delta_x > 0) set_view_mode(APP_VIEW_FORECAST);
      return;
    }

    if (s_ui.current_view == APP_VIEW_MESSAGE && delta_x < 0) {
      set_view_mode(s_ui.last_home_view);
    }
    return;
  }

  if (abs_y >= SWIPE_THRESHOLD_PX && (abs_y * 10) > (abs_x * SWIPE_AXIS_RATIO_X10)) {
    if (is_home && delta_y > 0) {
      open_settings();
    }
  }
}

static void gesture_layer_event_cb(lv_event_t *event)
{
  if (alarm_preempting() || settings_visible() || (s_ui.backup_notice_open && !alarm_page())) {
    s_ui.gesture_tracking = false;
    return;
  }
  lv_event_code_t code = lv_event_get_code(event);
  lv_indev_t *indev = lv_indev_active();
  if (indev == NULL) {
    return;
  }

  lv_point_t point;
  lv_indev_get_point(indev, &point);

  if (code == LV_EVENT_PRESSED) {
    s_ui.gesture_tracking = true;
    s_ui.gesture_start = point;
    return;
  }

  if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) && s_ui.gesture_tracking) {
    int delta_x = point.x - s_ui.gesture_start.x;
    int delta_y = point.y - s_ui.gesture_start.y;
    s_ui.gesture_tracking = false;

    if (code == LV_EVENT_RELEASED && s_ui.current_view == APP_VIEW_RADAR
        && LV_ABS(delta_x) < 12 && LV_ABS(delta_y) < 12
        && wac_weather_radar_control_hit(s_ui.gesture_start.x, s_ui.gesture_start.y)
        && wac_weather_radar_control_hit(point.x, point.y)) {
      submit_radar_action(s_ui.radar.playing ? APP_RADAR_PAUSE : APP_RADAR_PLAY);
      return;
    }

    if (code == LV_EVENT_RELEASED && s_ui.current_view == APP_VIEW_MESSAGE
        && LV_ABS(delta_x) < 12
        && LV_ABS(delta_y) < 12
        && s_ui.displayed_message_id[0] != '\0') {
      if (s_ui.message_ack_pending) return;
      esp_err_t error = message_service_submit_acknowledgment(
        s_ui.config_snapshot.device_id, s_ui.displayed_message_id);
      s_ui.message_held = true;
      if (error == ESP_OK) {
        s_ui.message_ack_pending = true;
        s_ui.message_ack_return_allowed = true;
        wac_weather_set_message_dismiss_text("Dismissing...");
      } else {
        wac_weather_set_message_dismiss_text("Could not dismiss. Tap to retry.");
        ESP_LOGW(TAG, "Message dismissal submission failed: %s", esp_err_to_name(error));
      }
      return;
    }
    handle_swipe(delta_x, delta_y);
  }
}

static void build_gesture_layer(lv_obj_t *parent)
{
  s_ui.gesture_layer = lv_obj_create(parent);
  lv_obj_set_size(s_ui.gesture_layer, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(s_ui.gesture_layer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_ui.gesture_layer, 0, 0);
  lv_obj_set_style_pad_all(s_ui.gesture_layer, 0, 0);
  lv_obj_add_flag(s_ui.gesture_layer, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_ui.gesture_layer, gesture_layer_event_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(s_ui.gesture_layer, gesture_layer_event_cb, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(s_ui.gesture_layer, gesture_layer_event_cb, LV_EVENT_PRESS_LOST, NULL);
}

/* These controls present the time owner's warning. Dismissal only submits a
 * command; a later owner snapshot confirms that the notice can disappear. */
static void backup_open_event_cb(lv_event_t *event)
{
  (void)event;
  if (alarm_page() || !s_ui.clock.usable || s_ui.clock.warning == WAC_TIME_WARNING_NONE || settings_visible()) return;
  s_ui.backup_notice_open = true;
  s_ui.gesture_tracking = false;
  update_backup_notice();
  update_alarm_surface();
}

static void backup_dismiss_event_cb(lv_event_t *event)
{
  (void)event;
  if (alarm_page() || s_ui.backup_dismiss_warning != WAC_TIME_WARNING_NONE || !s_ui.backup_notice_open) return;
  esp_err_t error = s_clock_warning_dismiss_handler
    ? s_clock_warning_dismiss_handler(s_ui.clock.warning) : ESP_ERR_INVALID_STATE;
  if (error != ESP_OK) {
    ESP_LOGW(TAG, "Backup-clock dismissal failed: %s", esp_err_to_name(error));
    update_backup_notice();
    return;
  }
  s_ui.backup_dismiss_warning = s_ui.clock.warning;
  update_backup_notice();
}

static void update_backup_notice(void)
{
  bool home = !alarm_page() && !settings_visible()
    && (s_ui.current_view == APP_VIEW_ANALOG || s_ui.current_view == APP_VIEW_DIGITAL);
  wac_clock_update_backup(home, s_ui.current_view == APP_VIEW_ANALOG,
    s_ui.clock.usable, s_ui.clock.warning, s_ui.backup_notice_open,
    s_ui.backup_dismiss_warning != WAC_TIME_WARNING_NONE,
    s_ui.night_overlay_active);
  wac_clock_update_machine(home, s_ui.current_view == APP_VIEW_ANALOG, s_ui.clock.usable,
    s_ui.audio.machine_active, s_ui.night_overlay_active);
}

void app_ui_set_clock_warning_dismiss_handler(esp_err_t (*handler)(wac_time_warning_t))
{
  s_clock_warning_dismiss_handler = handler;
}

static void machine_open_event_cb(lv_event_t *event)
{
  (void)event;
  if (alarm_preempting() || s_ui.backup_notice_open) return;
  bool returning_to_draft = s_ui.settings_suspended;
  open_settings();
  if (settings_visible()) wac_settings_start_machine(!returning_to_draft);
}

static void settings_open_event_cb(lv_event_t *event)
{
  (void)event;
  open_settings();
}

static void alert_dismiss_event_cb(lv_event_t *event)
{
  (void)event;
  if (alarm_page() || settings_visible() || s_ui.backup_notice_open) return;
  weather_client_alerts_dismiss();
  weather_client_alerts_copy(&s_ui.alerts);
  update_alarm_surface();
}

static void alerts_open_event_cb(lv_event_t *event)
{
  (void)event;
  if (alarm_page() || settings_visible() || s_ui.backup_notice_open) return;
  weather_client_alerts_dismiss();
  weather_client_alerts_copy(&s_ui.alerts);
  set_view_mode(APP_VIEW_ALERTS);
}

static void alerts_back_event_cb(lv_event_t *event)
{
  (void)event;
  if (alarm_page() || settings_visible()) return;
  set_view_mode(s_ui.current_view == APP_VIEW_ALERT_DETAIL
    ? APP_VIEW_ALERTS : s_ui.last_home_view);
}

static void alert_row_event_cb(lv_event_t *event)
{
  if (alarm_page() || settings_visible() || s_ui.current_view != APP_VIEW_ALERTS) return;
  s_ui.alert_detail_id = wac_weather_alert_event_id(event);
  if (s_ui.alert_detail_id) set_view_mode(APP_VIEW_ALERT_DETAIL);
}

static void build_night_overlay(lv_obj_t *parent)
{
  s_ui.night_overlay = lv_obj_create(parent);
  lv_obj_remove_flag(s_ui.night_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(s_ui.night_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(s_ui.night_overlay, lv_color_hex(0x6b0909), 0);
  lv_obj_set_style_bg_opa(s_ui.night_overlay, LV_OPA_30, 0);
  lv_obj_set_style_border_width(s_ui.night_overlay, 0, 0);
  lv_obj_add_flag(s_ui.night_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_message_ui_state(
  const app_message_snapshot_t *snapshot,
  uint32_t unread_count,
  bool has_important_message
)
{
  s_ui.has_unread_messages = unread_count > 0;
  s_ui.has_important_messages = has_important_message;
  /* Keep the tapped message visible while waiting or showing its error, even
   * if polling finds a newer message. Leaving this view releases that hold. */
  if (s_ui.message_held) return;

  if (snapshot == NULL || !snapshot->available) s_ui.displayed_message_id[0] = '\0';
  else snprintf(s_ui.displayed_message_id, sizeof(s_ui.displayed_message_id), "%s", snapshot->id);
  wac_weather_update_message(snapshot);
}

void app_ui_complete_message_acknowledgment(esp_err_t result)
{
  if (!s_ui.message_ack_pending) return;
  s_ui.message_ack_pending = false;
  bool still_here = s_ui.message_ack_return_allowed
    && s_ui.current_view == APP_VIEW_MESSAGE && !settings_visible() && !alarm_preempting();
  s_ui.message_ack_return_allowed = false;
  if (result != ESP_OK) {
    ESP_LOGW(TAG, "Message dismissal failed: %s", esp_err_to_name(result));
    wac_weather_set_message_dismiss_text(
      still_here ? "Could not dismiss. Tap to retry." : "Tap to dismiss");
    return;
  }
  s_ui.message_held = false;
  wac_weather_set_message_dismiss_text("Tap to dismiss");
  if (still_here) set_view_mode(s_ui.last_home_view);
}

static void update_clock_status(const wac_time_snapshot_t *clock)
{
  bool home = s_ui.current_view == APP_VIEW_ANALOG || s_ui.current_view == APP_VIEW_DIGITAL;
  const char *text = "";
  bool waiting_for_sync = false;
  if (!home) reset_clock_health();
  else if (!alarm_page() && !settings_visible() && clock->usable) {
    if (clock_is_stale()) text = "Clock paused";
    else if (clock->source == WAC_TIME_SOURCE_RTC) {
      text = "Waiting for sync";
      waiting_for_sync = true;
    }
  }
  wac_clock_update_status(text, s_ui.current_view == APP_VIEW_ANALOG, waiting_for_sync);
}

static void update_weather_status(void)
{
  bool forecast_visible = !alarm_page() && s_ui.current_view == APP_VIEW_FORECAST;
  char notice[64] = "";
  if (s_ui.weather.valid && s_ui.weather.stale) {
    const wac_time_snapshot_t *clock = &s_ui.clock;
    /* Backup time can keep the clock usable without establishing an accurate
     * age for downloaded weather; show a general stale notice in that case. */
    if (!clock->usable || clock->source != WAC_TIME_SOURCE_NETWORK
        || s_ui.weather.updated_at <= 0 || clock->utc_now < s_ui.weather.updated_at) {
      snprintf(notice, sizeof(notice), "Weather data stale");
    } else {
      unsigned long age = (unsigned long)difftime(clock->utc_now, s_ui.weather.updated_at);
      if (age < 60) snprintf(notice, sizeof(notice), "Weather updated just now");
      else if (age < 3600) snprintf(notice, sizeof(notice), "Weather updated %lum ago", age / 60);
      else if (age < 86400) snprintf(notice, sizeof(notice), "Weather updated %luh ago", age / 3600);
      else snprintf(notice, sizeof(notice), "Weather updated %lud ago", age / 86400);
    }
  }
  wac_clock_update_weather_notice(notice);
  wac_weather_update_status(&s_ui.weather, s_ui.night_overlay_active, forecast_visible);
}

static void update_alarm_surface(void)
{
  if (s_ui.current_view == APP_VIEW_ALERT_DETAIL) {
    weather_alert_detail_t detail;
    size_t needed;
    if (weather_client_alerts_detail_copy(s_ui.alert_detail_id, &detail, NULL, 0,
        &needed) == ESP_ERR_NOT_FOUND) {
      s_ui.current_view = APP_VIEW_ALERTS;
      s_ui.alert_detail_id = 0;
    }
  }
  bool home = s_ui.current_view == APP_VIEW_ANALOG || s_ui.current_view == APP_VIEW_DIGITAL;
  bool page = alarm_page();
  bool radar_visit = !page && !settings_visible() && !s_ui.backup_notice_open
    && s_ui.current_view == APP_VIEW_RADAR;
  if (radar_visit != s_ui.radar_visit_active) {
    /* Retry a temporarily busy command mailbox on the next UI heartbeat. */
    if (s_radar_action_handler == NULL || submit_radar_action(APP_RADAR_LATEST))
      s_ui.radar_visit_active = radar_visit;
  }
  wac_clock_alarm_surface_t surface = s_ui.alarm_confirmation_timer ? WAC_CLOCK_ALARM_DISMISSED
    : alarm_ringing() || s_ui.alarm_slide_pending ? WAC_CLOCK_ALARM_RINGING
    : settings_visible() ? WAC_CLOCK_ALARM_HIDDEN
    : page ? WAC_CLOCK_ALARM_SNOOZED
    : home ? WAC_CLOCK_ALARM_HOME : WAC_CLOCK_ALARM_HIDDEN;
  wac_clock_set_visible(!page && s_ui.current_view == APP_VIEW_ANALOG,
    !page && s_ui.current_view == APP_VIEW_DIGITAL, s_ui.clock.usable);
  wac_weather_set_visible(!page && s_ui.current_view == APP_VIEW_FORECAST,
    !page && s_ui.current_view == APP_VIEW_RADAR,
    !page && s_ui.current_view == APP_VIEW_MESSAGE);
  bool blocked = s_ui.backup_notice_open || (s_ui.clock.usable
    && (clock_is_stale() || s_ui.clock.source == WAC_TIME_SOURCE_RTC));
  /* The owner clears current when it dismisses. The held presentation keeps
   * only the already captured occurrence, never another alarm state. */
  wac_alarm_snapshot_t presentation = s_ui.alarm;
  if (s_ui.alarm_confirmation_timer || s_ui.alarm_slide_pending)
    presentation.current = s_ui.pending_alarm_occurrence;
  wac_clock_update_alarm(&s_ui.config_snapshot, &s_ui.clock, &presentation, surface,
    s_ui.current_view == APP_VIEW_ANALOG, s_ui.night_overlay_active, blocked,
    s_ui.alarm_action_pending);
  wac_settings_set_visible(settings_visible() && !page);
  update_night_overlay_visibility();
  update_clock_status(&s_ui.clock);
  update_weather_status();
  update_backup_notice();
  wac_clock_update_alerts(&s_ui.alerts,
    home && !page && !settings_visible() && !s_ui.backup_notice_open,
    s_ui.current_view == APP_VIEW_ANALOG, s_ui.clock.usable, s_ui.night_overlay_active);
  wac_weather_update_alerts(&s_ui.alerts, &s_ui.config_snapshot,
    !page && !settings_visible() && s_ui.current_view == APP_VIEW_ALERTS,
    !page && !settings_visible() && s_ui.current_view == APP_VIEW_ALERT_DETAIL,
    s_ui.alert_detail_id, s_ui.night_overlay_active);
}

/* Apply published Display choices once without replacing an editor or a
 * non-home destination. Later manual navigation returns to the saved face. */
void wac_ui_apply_display(const device_config_t *config)
{
  s_ui.config_snapshot = *config;
  update_radar_eligibility();
  app_view_mode_t home = wac_ui_strings_equal_ignore_case(config->default_clock_face, "analog")
    ? APP_VIEW_ANALOG : APP_VIEW_DIGITAL;
  s_ui.last_home_view = home;
  if (s_ui.current_view == APP_VIEW_ANALOG || s_ui.current_view == APP_VIEW_DIGITAL)
    s_ui.current_view = home;
  struct tm local_time;
  bool usable = s_ui.clock.usable && localtime_r(&s_ui.clock.utc_now, &local_time) != NULL;
  wac_clock_update_time(config, usable ? &local_time : NULL, s_ui.night_overlay_active);
  wac_weather_update(&s_ui.weather, usable ? &local_time : NULL);
  wac_weather_update_radar(&s_ui.radar, config, s_ui.night_overlay_active);
  update_alarm_surface();
}


void app_ui_set_alarm_action_handler(esp_err_t (*handler)(app_alarm_action_t,
  const wac_alarm_occurrence_t *))
{
  s_alarm_action_handler = handler;
}

static void submit_alarm_action(app_alarm_action_t action)
{
  if (s_ui.alarm_action_pending) return;
  bool missed = action == APP_ALARM_DISMISS_MISSED;
  if (missed ? !s_ui.alarm.missed.valid : !s_ui.alarm.current.valid) return;
  if (!missed && s_ui.alarm.phase != WAC_ALARM_RINGING
      && s_ui.alarm.phase != WAC_ALARM_SNOOZED) return;
  if (action == APP_ALARM_SNOOZE && !alarm_ringing()) return;
  const wac_alarm_occurrence_t *occurrence = missed ? &s_ui.alarm.missed : &s_ui.alarm.current;
  esp_err_t result = s_alarm_action_handler ? s_alarm_action_handler(action, occurrence)
    : ESP_ERR_INVALID_STATE;
  if (result == ESP_OK) {
    s_ui.alarm_action_pending = true;
    s_ui.pending_alarm_action = action;
    s_ui.pending_alarm_occurrence = *occurrence;
    s_ui.alarm_action_return_allowed = !missed;
  } else {
    ESP_LOGW(TAG, "Alarm action submission failed: %s", esp_err_to_name(result));
  }
  update_alarm_surface();
}

static void alarm_action_event_cb(lv_event_t *event)
{
  submit_alarm_action((app_alarm_action_t)(uintptr_t)lv_event_get_user_data(event));
}

static void alarm_slide_event_cb(lv_event_t *event)
{
  (void)event;
  if (!alarm_ringing() || s_ui.alarm_action_pending || s_ui.alarm_confirmation_timer) return;
  submit_alarm_action(APP_ALARM_DISMISS);
  s_ui.alarm_slide_pending = s_ui.alarm_action_pending
    && s_ui.pending_alarm_action == APP_ALARM_DISMISS;
  if (!s_ui.alarm_slide_pending) wac_clock_cancel_alarm_slide(true);
}

static void cancel_alarm_confirmation(void)
{
  if (s_ui.alarm_confirmation_timer) {
    lv_timer_delete(s_ui.alarm_confirmation_timer);
    s_ui.alarm_confirmation_timer = NULL;
  }
  s_ui.alarm_slide_pending = false;
}

static void alarm_confirmation_expired(lv_timer_t *timer)
{
  if (timer != s_ui.alarm_confirmation_timer) return;
  s_ui.alarm_confirmation_timer = NULL;
  /* A replacement/resumed ring cancels this timer before it can navigate. */
  if (s_ui.alarm.phase == WAC_ALARM_IDLE) set_view_mode(s_ui.last_home_view);
  update_alarm_surface();
}

static void open_alarm_event_cb(lv_event_t *event)
{
  (void)event;
  if (alarm_preempting() || (s_ui.backup_notice_open && !alarm_page())) return;
  s_ui.snoozed_view = false;
  bool held_alarm = s_ui.settings_suspended && s_ui.settings_view >= WAC_SETTINGS_ALARM
    && s_ui.settings_view <= WAC_SETTINGS_SNOOZE;
  if (held_alarm) {
    wac_settings_set_alarm_return_to_clock();
    open_settings();
    return;
  }
  /* This explicit shortcut ends another editor just like its Cancel action.
   * Ringing itself only hides that editor, and a Settings swipe resumes it. */
  wac_settings_close();
  s_ui.settings_suspended = false;
  s_ui.settings_view = WAC_SETTINGS_NONE;
  wac_settings_apply_night(s_ui.night_overlay_active);
  wac_settings_begin(s_ui.stage, &s_ui.config_snapshot);
  wac_settings_set_alarm_return_to_clock();
  wac_ui_settings_navigate(WAC_SETTINGS_ALARM);
}

static void alarm_badge_event_cb(lv_event_t *event)
{
  if (alarm_preempting() || settings_visible() || s_ui.backup_notice_open) return;
  if (s_ui.alarm.missed.valid) submit_alarm_action(APP_ALARM_DISMISS_MISSED);
  else if (s_ui.alarm.phase == WAC_ALARM_SNOOZED) {
    set_view_mode(s_ui.last_home_view);
    s_ui.snoozed_view = true;
    update_alarm_surface();
  } else open_alarm_event_cb(event);
}

void app_ui_complete_alarm_action(app_alarm_action_t action,
  const wac_alarm_occurrence_t *occurrence, esp_err_t result)
{
  if (!s_ui.alarm_action_pending || occurrence == NULL
      || action != s_ui.pending_alarm_action
      || !same_occurrence(occurrence, &s_ui.pending_alarm_occurrence)) return;
  s_ui.alarm_action_pending = false;
  bool return_allowed = s_ui.alarm_action_return_allowed;
  s_ui.alarm_action_return_allowed = false;
  bool slide = s_ui.alarm_slide_pending;
  s_ui.alarm_slide_pending = false;
  /* The owner publishes its live transition before this completion. Storage
   * failure does not undo that transition; rejected stale commands do not move
   * the user away from their current screen or from a newer alarm. */
  bool snoozed = action == APP_ALARM_SNOOZE && s_ui.alarm.phase == WAC_ALARM_SNOOZED
    && same_occurrence(occurrence, &s_ui.alarm.current);
  bool dismissed = action == APP_ALARM_DISMISS && s_ui.alarm.phase == WAC_ALARM_IDLE;
  if (result != ESP_OK) {
    ESP_LOGW(TAG, "Alarm action failed: %s", esp_err_to_name(result));
  }
  if (return_allowed && (snoozed || dismissed)) {
    /* A failed action establishes dismissal only when the live owner reports
     * that same storage failure. A rejected stale command may also find Idle. */
    if (slide && dismissed && (result == ESP_OK || result == s_ui.alarm.storage_error)) {
      s_ui.alarm_confirmation_timer = lv_timer_create(alarm_confirmation_expired, 2200, NULL);
      if (s_ui.alarm_confirmation_timer) lv_timer_set_repeat_count(s_ui.alarm_confirmation_timer, 1);
      else set_view_mode(s_ui.last_home_view);
    } else {
      set_view_mode(s_ui.last_home_view);
      s_ui.snoozed_view = snoozed;
    }
  }
  update_alarm_surface();
}

static void build_app_shell(const device_config_t *config)
{
  if (s_ui.radar_visit_active) submit_radar_action(APP_RADAR_LATEST);
  cancel_alarm_confirmation();
  lv_obj_t *screen = lv_screen_active();
  wac_settings_close();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, wac_ui_color_bg(), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  memset(&s_ui, 0, sizeof(s_ui));
  wac_clock_reset();
  wac_weather_reset();
  s_ui.config_snapshot = *config;

  s_ui.stage = lv_obj_create(screen);
  lv_obj_set_size(s_ui.stage, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(s_ui.stage, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_ui.stage, 0, 0);
  lv_obj_set_style_pad_all(s_ui.stage, 0, 0);
  lv_obj_set_scrollbar_mode(s_ui.stage, LV_SCROLLBAR_MODE_OFF);

  wac_clock_build_analog(s_ui.stage);
  yield_ui_bootstrap();
  wac_clock_build_digital(s_ui.stage);
  yield_ui_bootstrap();
  wac_weather_build_forecast(s_ui.stage);
  yield_ui_bootstrap();
  wac_weather_set_radar_enabled(s_ui.stage, radar_enabled());
  yield_ui_bootstrap();
  wac_weather_build_message(s_ui.stage);
  yield_ui_bootstrap();
  wac_clock_build_unavailable(s_ui.stage);
  build_gesture_layer(s_ui.stage);
  wac_weather_build_status(s_ui.stage);
  wac_clock_build_status(s_ui.stage);
  build_night_overlay(s_ui.stage);
  wac_clock_build_machine(s_ui.stage, machine_open_event_cb);
  wac_clock_build_alerts(s_ui.stage, alerts_open_event_cb, alert_dismiss_event_cb,
    settings_open_event_cb);
  wac_weather_build_alerts(s_ui.stage, alerts_back_event_cb, alert_row_event_cb);
  wac_clock_build_backup(s_ui.stage, backup_open_event_cb, backup_dismiss_event_cb);
  wac_clock_build_alarm(s_ui.stage, alarm_action_event_cb, alarm_badge_event_cb, open_alarm_event_cb,
    alarm_slide_event_cb);

  if (wac_ui_strings_equal_ignore_case(config->default_clock_face, "analog")) {
    s_ui.last_home_view = APP_VIEW_ANALOG;
    set_view_mode(APP_VIEW_ANALOG);
  } else {
    s_ui.last_home_view = APP_VIEW_DIGITAL;
    set_view_mode(APP_VIEW_DIGITAL);
  }
}

esp_err_t app_ui_show_boot_screen(const device_config_t *config,
                                  const app_runtime_state_t *runtime, bool touch_ready)
{
  if (config == NULL || runtime == NULL) return ESP_ERR_INVALID_ARG;
  build_app_shell(config);
  yield_ui_bootstrap();
  esp_err_t err = app_ui_update_runtime(config, runtime);
  if (err != ESP_OK) return err;
  if (!config->location_ready && touch_ready && !alarm_ringing()) open_settings();
  if (runtime->alarm.phase == WAC_ALARM_SNOOZED) {
    s_ui.snoozed_view = true;
    update_alarm_surface();
  }
  return ESP_OK;
}

esp_err_t app_ui_update_runtime(
  const device_config_t *config,
  const app_runtime_state_t *runtime
)
{
  if (config == NULL || runtime == NULL || s_ui.stage == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  bool new_ring = runtime->alarm.phase == WAC_ALARM_RINGING
    && (s_ui.alarm.phase != WAC_ALARM_RINGING
      || !same_occurrence(&s_ui.alarm.current, &runtime->alarm.current));
  bool left_ring = s_ui.alarm.phase == WAC_ALARM_RINGING
    && runtime->alarm.phase != WAC_ALARM_RINGING;
  if (new_ring || left_ring) wac_clock_cancel_alarm_slide(false);
  if (new_ring || (s_ui.alarm_confirmation_timer && runtime->alarm.phase != WAC_ALARM_IDLE))
    cancel_alarm_confirmation();
  s_ui.clock = runtime->clock;
  s_ui.alarm = runtime->alarm;
  s_ui.audio = runtime->audio;
  s_ui.alerts = runtime->alerts;
  s_ui.config_snapshot = *config;
  update_radar_eligibility();
  if (new_ring) {
    s_ui.settings_suspended = s_ui.settings_view != WAC_SETTINGS_NONE;
    s_ui.gesture_tracking = false;
    s_ui.message_ack_return_allowed = false;
    s_ui.message_held = false;
    s_ui.alarm_action_return_allowed = false;
    s_ui.snoozed_view = false;
    s_ui.current_view = s_ui.last_home_view;
  }
  if (runtime->alarm.phase != WAC_ALARM_SNOOZED) s_ui.snoozed_view = false;
  if (!s_ui.clock.usable || s_ui.clock.warning == WAC_TIME_WARNING_NONE) s_ui.backup_notice_open = false;
  /* The owner may acknowledge one warning and reveal another between UI
   * heartbeats. The newly displayed warning needs its own dismissal. */
  if (s_ui.clock.warning == WAC_TIME_WARNING_NONE
      || (s_ui.backup_dismiss_warning != WAC_TIME_WARNING_NONE
          && s_ui.clock.warning != s_ui.backup_dismiss_warning)) {
    s_ui.backup_dismiss_warning = WAC_TIME_WARNING_NONE;
  }
  /* A failed short owner copy must not erase the last usable forecast. */
  if (runtime->weather.valid) {
    s_ui.weather = runtime->weather;
  }

  time_t display_time = runtime->clock.usable ? runtime->clock.utc_now : 0;

  struct tm local_time = {0};
  bool time_available = runtime->clock.usable && localtime_r(&display_time, &local_time) != NULL;
  wac_clock_update_time(config, time_available ? &local_time : NULL, runtime->night_active);
  wac_clock_set_visible(s_ui.current_view == APP_VIEW_ANALOG,
    s_ui.current_view == APP_VIEW_DIGITAL, time_available);
  if (time_available) update_clock_health(&local_time);
  else reset_clock_health();
  if (s_ui.weather.valid) {
    wac_clock_update_weather(&s_ui.weather);
    wac_weather_update(&s_ui.weather, time_available ? &local_time : NULL);
  } else {
    wac_clock_update_weather(NULL);
    wac_weather_update(&s_ui.weather, NULL);
  }
  /* Generation zero is a failed short snapshot copy, except at startup. Keep
   * the last publication until its owner supplies a real replacement. */
  bool pause_confirmed = runtime->radar.paused
    && runtime->radar.frame_time == s_ui.radar.frame_time;
  bool location_changed = runtime->radar.latitude != s_ui.radar.latitude
    || runtime->radar.longitude != s_ui.radar.longitude;
  /* Pause names the frame already shown. A newer worker publication may have
   * been copied before that tap; retain our pointer and its acknowledgement
   * until the owner supplies the requested hold or removes the old image. */
  bool accept_radar = !s_ui.radar_pause_pending || pause_confirmed
    || (runtime->radar.generation != 0 && (!runtime->radar.valid || location_changed));
  if (radar_enabled()) {
    if (accept_radar && (runtime->radar.generation != 0 || s_ui.radar.generation == 0)) {
      s_ui.radar = runtime->radar;
      if (runtime->radar.generation != 0)
        s_ui.radar_displayed_generation = runtime->radar.generation;
      s_ui.radar_pause_pending = false;
    }
  } else if (runtime->radar.generation != 0) {
    /* Eligibility already deleted the descriptor and released its pointer. */
    s_ui.radar_displayed_generation = runtime->radar.generation;
  }
  wac_weather_update_radar(&s_ui.radar, config, runtime->night_active);
  refresh_message_ui_state(&runtime->message, runtime->unread_message_count, runtime->has_important_message);
  s_ui.night_overlay_active = runtime->night_active;
  wac_settings_apply_night(runtime->night_active);
  wac_settings_update_alarm(config, &runtime->alarm);
  wac_settings_update_clock(&runtime->clock);
  wac_settings_update_solar(&runtime->solar);
  wac_settings_update_audio(&runtime->audio);
  apply_view_night_state(s_ui.night_overlay_active);
  update_clock_status(&runtime->clock);
  update_weather_status();
  wac_settings_poll();
  update_alarm_surface();

  return ESP_OK;
}

void app_ui_show_storage_error(esp_err_t error)
{
  if (s_ui.radar_visit_active) submit_radar_action(APP_RADAR_LATEST);
  cancel_alarm_confirmation();
  lv_obj_t *screen = lv_screen_active();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, wac_ui_color_bg(), 0);
  lv_obj_t *title = wac_ui_create_label(screen, &lv_font_montserrat_32, wac_ui_color_text_primary(), LV_TEXT_ALIGN_CENTER, "Settings storage unavailable");
  lv_obj_set_width(title, 520);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -70);
  char text[160];
  snprintf(text, sizeof(text), "Could not read settings (%s).\nRestart after checking storage.\nExisting settings have not been erased.", esp_err_to_name(error));
  lv_obj_t *detail = wac_ui_create_label(screen, &lv_font_montserrat_24, wac_ui_color_text_muted(), LV_TEXT_ALIGN_CENTER, text);
  lv_obj_set_width(detail, 520);
  lv_obj_align(detail, LV_ALIGN_CENTER, 0, 65);
}
