#include "app_ui.h"
#include "ui_private.h"

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

#define SWIPE_THRESHOLD_PX 70
#define SWIPE_AXIS_RATIO_X10 15
#define CLOCK_STALE_WARNING_MS (135ULL * 1000ULL)

typedef enum {
  APP_VIEW_ANALOG = 0,
  APP_VIEW_DIGITAL,
  APP_VIEW_FORECAST,
  APP_VIEW_CONDITIONS,
  APP_VIEW_MESSAGE
} app_view_mode_t;

typedef struct {
  lv_obj_t *stage;
  lv_obj_t *gesture_layer;
  lv_obj_t *night_overlay;
  wac_settings_view_t settings_view;
  bool settings_suspended;
  app_view_mode_t current_view;
  app_view_mode_t last_home_view;
  /* The weather views left of home, nearest first. Snapshotted when leaving
   * home so an alert starting or clearing mid-visit doesn't reshuffle the
   * row; the new order applies from the next swipe out of home. */
  app_view_mode_t weather_row[2];
  bool weather_alert_active;
  lv_point_t gesture_start;
  bool gesture_tracking;
  bool clock_usable;
  wac_time_warning_t clock_warning;
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
} app_ui_state_t;

static app_ui_state_t s_ui;
static esp_err_t (*s_clock_warning_dismiss_handler)(wac_time_warning_t);
static esp_err_t (*s_alarm_action_handler)(app_alarm_action_t, const wac_alarm_occurrence_t *);
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

lv_color_t wac_ui_color_night_alert(void)
{
  return lv_color_hex(0xec9682);
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

static void yield_ui_bootstrap(void)
{
  vTaskDelay(pdMS_TO_TICKS(1));
}

static void open_settings(void);
static void update_backup_notice(void);
static void machine_open_event_cb(lv_event_t *event);
static void apply_view_night_state(bool active);
static void update_weather_status(void);
static void update_alarm_surface(void);
static void update_clock_status(const wac_time_snapshot_t *clock);
static void refresh_message_ui_state(
  const app_message_snapshot_t *snapshot,
  uint32_t unread_count,
  bool has_important_message
);

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

static bool alarm_page(void)
{
  return alarm_ringing() || (s_ui.snoozed_view
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
  wac_settings_set_visible(!s_ui.settings_suspended && !alarm_ringing());
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
  s_ui.settings_suspended = false;
  s_ui.alarm_action_return_allowed = false;
  wac_settings_close();
  update_backup_notice();
  update_alarm_surface();
}

static void open_settings(void)
{
  if (alarm_ringing()) return;
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
    s_ui.weather_alert_active, s_ui.night_overlay_active);
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

  wac_clock_set_visible(next_view == APP_VIEW_ANALOG, next_view == APP_VIEW_DIGITAL, s_ui.clock_usable);
  if (next_view != APP_VIEW_ANALOG && next_view != APP_VIEW_DIGITAL)
    wac_clock_update_status("", false, false);

  wac_weather_set_visible(next_view == APP_VIEW_FORECAST,
    next_view == APP_VIEW_CONDITIONS, next_view == APP_VIEW_MESSAGE);

  apply_view_night_state(s_ui.night_overlay_active);
  update_weather_status();
  update_alarm_surface();
}

/* During a storm alert, conditions jumps ahead of the forecast so one swipe
 * from home lands on it; the forecast is still one more swipe away. */
static void snapshot_weather_row(void)
{
  s_ui.weather_row[0] = s_ui.weather_alert_active ? APP_VIEW_CONDITIONS : APP_VIEW_FORECAST;
  s_ui.weather_row[1] = s_ui.weather_alert_active ? APP_VIEW_FORECAST : APP_VIEW_CONDITIONS;
}

static int weather_row_index(app_view_mode_t view)
{
  for (int i = 0; i < 2; ++i) {
    if (s_ui.weather_row[i] == view) {
      return i;
    }
  }
  return -1;
}

/* Navigation map:
 * only the configured face is home, and every screen is a fixed neighbour
 * of home that returns with the opposite swipe:
 *   right -> messages
 *   left  -> forecast -> conditions (conditions first during an alert)
 *   down  -> Settings (anywhere on the face)
 * Swipe up from home is reserved for a future view below home. */
static void handle_swipe(int delta_x, int delta_y)
{
  if (alarm_ringing() || settings_visible() || (s_ui.backup_notice_open && !alarm_page())) {
    return;
  }

  int abs_x = LV_ABS(delta_x);
  int abs_y = LV_ABS(delta_y);
  bool is_home = s_ui.current_view == APP_VIEW_ANALOG || s_ui.current_view == APP_VIEW_DIGITAL;

  if (abs_x >= SWIPE_THRESHOLD_PX && (abs_x * 10) > (abs_y * SWIPE_AXIS_RATIO_X10)) {
    if (is_home) {
      if (delta_x < 0) {
        snapshot_weather_row();
        set_view_mode(s_ui.weather_row[0]);
      } else {
        set_view_mode(APP_VIEW_MESSAGE);
      }
      return;
    }

    int row_index = weather_row_index(s_ui.current_view);
    if (row_index >= 0) {
      if (delta_x < 0 && row_index == 0) {
        set_view_mode(s_ui.weather_row[1]);
      } else if (delta_x > 0) {
        set_view_mode(row_index == 0 ? s_ui.last_home_view : s_ui.weather_row[0]);
      }
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
  if (alarm_ringing() || settings_visible() || (s_ui.backup_notice_open && !alarm_page())) {
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

    if (s_ui.current_view == APP_VIEW_MESSAGE
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
  if (alarm_page() || s_ui.clock_warning == WAC_TIME_WARNING_NONE || settings_visible()) return;
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
    ? s_clock_warning_dismiss_handler(s_ui.clock_warning) : ESP_ERR_INVALID_STATE;
  if (error != ESP_OK) {
    ESP_LOGW(TAG, "Backup-clock dismissal failed: %s", esp_err_to_name(error));
    update_backup_notice();
    return;
  }
  s_ui.backup_dismiss_warning = s_ui.clock_warning;
  update_backup_notice();
}

static void update_backup_notice(void)
{
  bool home = !alarm_page() && !settings_visible()
    && (s_ui.current_view == APP_VIEW_ANALOG || s_ui.current_view == APP_VIEW_DIGITAL);
  wac_clock_update_backup(home, s_ui.current_view == APP_VIEW_ANALOG,
    s_ui.clock_usable, s_ui.clock_warning, s_ui.backup_notice_open,
    s_ui.backup_dismiss_warning != WAC_TIME_WARNING_NONE,
    s_ui.night_overlay_active);
  wac_clock_update_machine(home, s_ui.current_view == APP_VIEW_ANALOG, s_ui.clock_usable,
    s_ui.audio.machine_active, s_ui.night_overlay_active);
}

void app_ui_set_clock_warning_dismiss_handler(esp_err_t (*handler)(wac_time_warning_t))
{
  s_clock_warning_dismiss_handler = handler;
}

static void machine_open_event_cb(lv_event_t *event)
{
  (void)event;
  if (alarm_ringing() || s_ui.backup_notice_open) return;
  bool returning_to_draft = s_ui.settings_suspended;
  open_settings();
  if (settings_visible()) wac_settings_start_machine(!returning_to_draft);
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
    && s_ui.current_view == APP_VIEW_MESSAGE && !settings_visible() && !alarm_ringing();
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
  wac_weather_surface_t surface = WAC_WEATHER_SURFACE_HOME;
  if (alarm_page()) surface = WAC_WEATHER_SURFACE_UNAVAILABLE_HOME;
  else if (s_ui.current_view == APP_VIEW_FORECAST) surface = WAC_WEATHER_SURFACE_FORECAST;
  else if (s_ui.current_view == APP_VIEW_CONDITIONS) surface = WAC_WEATHER_SURFACE_CONDITIONS;
  else if (s_ui.current_view == APP_VIEW_MESSAGE) surface = WAC_WEATHER_SURFACE_MESSAGE;
  else if (!s_ui.clock_usable) surface = WAC_WEATHER_SURFACE_UNAVAILABLE_HOME;
  char notice[64] = "";
  if (s_ui.weather.valid && s_ui.weather.stale) {
    const wac_time_snapshot_t *clock = &s_ui.clock;
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
  wac_weather_update_status(&s_ui.weather, s_ui.night_overlay_active, surface, notice);
}

static void update_alarm_surface(void)
{
  bool home = s_ui.current_view == APP_VIEW_ANALOG || s_ui.current_view == APP_VIEW_DIGITAL;
  bool page = alarm_page();
  wac_clock_alarm_surface_t surface = alarm_ringing() ? WAC_CLOCK_ALARM_RINGING
    : settings_visible() ? WAC_CLOCK_ALARM_HIDDEN
    : page ? WAC_CLOCK_ALARM_SNOOZED
    : home ? WAC_CLOCK_ALARM_HOME : WAC_CLOCK_ALARM_HIDDEN;
  wac_clock_set_visible(!page && s_ui.current_view == APP_VIEW_ANALOG,
    !page && s_ui.current_view == APP_VIEW_DIGITAL, s_ui.clock_usable);
  wac_weather_set_visible(!page && s_ui.current_view == APP_VIEW_FORECAST,
    !page && s_ui.current_view == APP_VIEW_CONDITIONS,
    !page && s_ui.current_view == APP_VIEW_MESSAGE);
  bool blocked = s_ui.backup_notice_open || (s_ui.clock.usable
    && (clock_is_stale() || s_ui.clock.source == WAC_TIME_SOURCE_RTC));
  wac_clock_update_alarm(&s_ui.config_snapshot, &s_ui.clock, &s_ui.alarm, surface,
    s_ui.current_view == APP_VIEW_ANALOG, s_ui.night_overlay_active, blocked,
    s_ui.alarm_action_pending);
  wac_settings_set_visible(settings_visible() && !alarm_ringing());
  update_night_overlay_visibility();
  update_clock_status(&s_ui.clock);
  update_weather_status();
  update_backup_notice();
}

/* Apply published Display choices once without replacing an editor or a
 * non-home destination. Later manual navigation returns to the saved face. */
void wac_ui_apply_display(const device_config_t *config)
{
  s_ui.config_snapshot = *config;
  app_view_mode_t home = wac_ui_strings_equal_ignore_case(config->default_clock_face, "analog")
    ? APP_VIEW_ANALOG : APP_VIEW_DIGITAL;
  s_ui.last_home_view = home;
  if (s_ui.current_view == APP_VIEW_ANALOG || s_ui.current_view == APP_VIEW_DIGITAL)
    s_ui.current_view = home;
  struct tm local_time;
  bool usable = s_ui.clock.usable && localtime_r(&s_ui.clock.utc_now, &local_time) != NULL;
  wac_clock_update_time(config, usable ? &local_time : NULL, s_ui.night_overlay_active);
  wac_weather_update(&s_ui.weather, usable ? &local_time : NULL,
    wac_ui_strings_equal_ignore_case(config->time_format, "24"));
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

static void open_alarm_event_cb(lv_event_t *event)
{
  (void)event;
  if (alarm_ringing() || (s_ui.backup_notice_open && !alarm_page())) return;
  s_ui.snoozed_view = false;
  bool held_alarm = s_ui.settings_suspended && s_ui.settings_view >= WAC_SETTINGS_ALARM
    && s_ui.settings_view <= WAC_SETTINGS_SNOOZE;
  if (held_alarm) {
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
  wac_ui_settings_navigate(WAC_SETTINGS_ALARM);
}

static void alarm_badge_event_cb(lv_event_t *event)
{
  if (alarm_ringing() || settings_visible() || s_ui.backup_notice_open) return;
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
    set_view_mode(s_ui.last_home_view);
    s_ui.snoozed_view = snoozed;
  }
  update_alarm_surface();
}

static void build_app_shell(const device_config_t *config)
{
  lv_obj_t *screen = lv_screen_active();
  wac_settings_close();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, wac_ui_color_bg(), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  memset(&s_ui, 0, sizeof(s_ui));
  wac_clock_reset();
  wac_weather_reset();
  s_ui.config_snapshot = *config;
  snapshot_weather_row();

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
  wac_weather_build_conditions(s_ui.stage);
  yield_ui_bootstrap();
  wac_weather_build_message(s_ui.stage);
  yield_ui_bootstrap();
  wac_clock_build_unavailable(s_ui.stage);
  build_gesture_layer(s_ui.stage);
  wac_weather_build_status(s_ui.stage);
  wac_clock_build_status(s_ui.stage);
  build_night_overlay(s_ui.stage);
  wac_clock_build_machine(s_ui.stage, machine_open_event_cb);
  wac_clock_build_backup(s_ui.stage, backup_open_event_cb, backup_dismiss_event_cb);
  wac_clock_build_alarm(s_ui.stage, alarm_action_event_cb, alarm_badge_event_cb, open_alarm_event_cb);

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
  s_ui.clock = runtime->clock;
  s_ui.alarm = runtime->alarm;
  s_ui.audio = runtime->audio;
  s_ui.config_snapshot = *config;
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
  s_ui.clock_usable = runtime->clock.usable;
  s_ui.clock_warning = runtime->clock.warning;
  if (s_ui.clock_warning == WAC_TIME_WARNING_NONE) s_ui.backup_notice_open = false;
  /* The owner may acknowledge one warning and reveal another between UI
   * heartbeats. The newly displayed warning needs its own dismissal. */
  if (s_ui.clock_warning == WAC_TIME_WARNING_NONE
      || (s_ui.backup_dismiss_warning != WAC_TIME_WARNING_NONE
          && s_ui.clock_warning != s_ui.backup_dismiss_warning)) {
    s_ui.backup_dismiss_warning = WAC_TIME_WARNING_NONE;
  }
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
    wac_weather_update(&s_ui.weather, time_available ? &local_time : NULL,
      wac_ui_strings_equal_ignore_case(config->time_format, "24"));
    s_ui.weather_alert_active = s_ui.weather.has_conditions && s_ui.weather.conditions.alert;
  } else {
    wac_clock_update_weather(NULL);
    wac_weather_update(&s_ui.weather, NULL,
      wac_ui_strings_equal_ignore_case(config->time_format, "24"));
  }
  refresh_message_ui_state(&runtime->message, runtime->unread_message_count, runtime->has_important_message);
  s_ui.night_overlay_active = runtime->night_active;
  wac_settings_apply_night(runtime->night_active);
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
