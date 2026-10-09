#include "ui_private.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/round_stage_day.h"
#include "connectivity.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "location_lookup.h"

#define SCAN_LIMIT 10

/* Retain the original key layouts and press previews. The symbols page also
 * includes colon, preserving input supported by the standard layout. */
#define SETTINGS_KB_BTN(width) (LV_BUTTONMATRIX_CTRL_POPOVER | (width))

static const char *const SETTINGS_KEYBOARD_LOWER_MAP[] = {
  "1#", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", LV_SYMBOL_BACKSPACE, "\n",
  "ABC", "a", "s", "d", "f", "g", "h", "j", "k", "l", LV_SYMBOL_NEW_LINE, "\n",
  "_", "-", "^", "z", "x", "c", "v", "b", "n", "m", ".", ",", "\n",
  LV_SYMBOL_KEYBOARD,
#if LV_USE_ARABIC_PERSIAN_CHARS == 1
  LV_KEYBOARD_CTRL_BUTTON_MODE_TEXT_ARABIC,
#endif
  LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
};

static const lv_buttonmatrix_ctrl_t SETTINGS_KEYBOARD_LOWER_CTRL_MAP[] = {
  LV_KEYBOARD_CTRL_BUTTON_FLAGS | 5, SETTINGS_KB_BTN(4), SETTINGS_KB_BTN(4), SETTINGS_KB_BTN(4), SETTINGS_KB_BTN(4),
  SETTINGS_KB_BTN(4), SETTINGS_KB_BTN(4), SETTINGS_KB_BTN(4), SETTINGS_KB_BTN(4), SETTINGS_KB_BTN(4), SETTINGS_KB_BTN(4),
  LV_BUTTONMATRIX_CTRL_CHECKED | 7,
  LV_KEYBOARD_CTRL_BUTTON_FLAGS | 6, SETTINGS_KB_BTN(3), SETTINGS_KB_BTN(3), SETTINGS_KB_BTN(3), SETTINGS_KB_BTN(3),
  SETTINGS_KB_BTN(3), SETTINGS_KB_BTN(3), SETTINGS_KB_BTN(3), SETTINGS_KB_BTN(3), SETTINGS_KB_BTN(3),
  LV_BUTTONMATRIX_CTRL_CHECKED | 7,
  LV_BUTTONMATRIX_CTRL_CHECKED | SETTINGS_KB_BTN(1), LV_BUTTONMATRIX_CTRL_CHECKED | SETTINGS_KB_BTN(1),
  SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1),
  SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), LV_BUTTONMATRIX_CTRL_CHECKED | SETTINGS_KB_BTN(1),
  LV_BUTTONMATRIX_CTRL_CHECKED | SETTINGS_KB_BTN(1),
  LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2,
#if LV_USE_ARABIC_PERSIAN_CHARS == 1
  LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2,
#endif
  LV_BUTTONMATRIX_CTRL_CHECKED | 2, 6, LV_BUTTONMATRIX_CTRL_CHECKED | 2, LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2
};

static const char *const SETTINGS_KEYBOARD_UPPER_MAP[] = {
  "1#", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", LV_SYMBOL_BACKSPACE, "\n",
  "abc", "A", "S", "D", "F", "G", "H", "J", "K", "L", LV_SYMBOL_NEW_LINE, "\n",
  "_", "-", "^", "Z", "X", "C", "V", "B", "N", "M", ".", ",", "\n",
  LV_SYMBOL_CLOSE,
#if LV_USE_ARABIC_PERSIAN_CHARS == 1
  LV_KEYBOARD_CTRL_BUTTON_MODE_TEXT_ARABIC,
#endif
  LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
};

static const lv_buttonmatrix_ctrl_t SETTINGS_KEYBOARD_UPPER_CTRL_MAP[] = {
  LV_KEYBOARD_CTRL_BUTTON_FLAGS | 5, SETTINGS_KB_BTN(4), SETTINGS_KB_BTN(4), SETTINGS_KB_BTN(4), SETTINGS_KB_BTN(4),
  SETTINGS_KB_BTN(4), SETTINGS_KB_BTN(4), SETTINGS_KB_BTN(4), SETTINGS_KB_BTN(4), SETTINGS_KB_BTN(4), SETTINGS_KB_BTN(4),
  LV_BUTTONMATRIX_CTRL_CHECKED | 7,
  LV_KEYBOARD_CTRL_BUTTON_FLAGS | 6, SETTINGS_KB_BTN(3), SETTINGS_KB_BTN(3), SETTINGS_KB_BTN(3), SETTINGS_KB_BTN(3),
  SETTINGS_KB_BTN(3), SETTINGS_KB_BTN(3), SETTINGS_KB_BTN(3), SETTINGS_KB_BTN(3), SETTINGS_KB_BTN(3),
  LV_BUTTONMATRIX_CTRL_CHECKED | 7,
  LV_BUTTONMATRIX_CTRL_CHECKED | SETTINGS_KB_BTN(1), LV_BUTTONMATRIX_CTRL_CHECKED | SETTINGS_KB_BTN(1),
  SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1),
  SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), LV_BUTTONMATRIX_CTRL_CHECKED | SETTINGS_KB_BTN(1),
  LV_BUTTONMATRIX_CTRL_CHECKED | SETTINGS_KB_BTN(1),
  LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2,
#if LV_USE_ARABIC_PERSIAN_CHARS == 1
  LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2,
#endif
  LV_BUTTONMATRIX_CTRL_CHECKED | 2, 6, LV_BUTTONMATRIX_CTRL_CHECKED | 2, LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2
};

static const char *const SETTINGS_KEYBOARD_SPECIAL_MAP[] = {
  "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", LV_SYMBOL_BACKSPACE, "\n",
  "abc", "+", "&", "/", "*", "=", "%", "!", "?", "#", "<", ">", "\n",
  "\\", "@", "$", "(", ")", "{", "}", "[", "]", ";", "\"", "'", "\n",
  LV_SYMBOL_KEYBOARD,
#if LV_USE_ARABIC_PERSIAN_CHARS == 1
  LV_KEYBOARD_CTRL_BUTTON_MODE_TEXT_ARABIC,
#endif
  LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, "^", ":", LV_SYMBOL_OK, ""
};

static const lv_buttonmatrix_ctrl_t SETTINGS_KEYBOARD_SPECIAL_CTRL_MAP[] = {
  SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1),
  SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1),
  LV_BUTTONMATRIX_CTRL_CHECKED | 2,
  LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2, SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1),
  SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1),
  SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1),
  SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1),
  SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1),
  SETTINGS_KB_BTN(1), SETTINGS_KB_BTN(1),
  LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2,
#if LV_USE_ARABIC_PERSIAN_CHARS == 1
  LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2,
#endif
  LV_BUTTONMATRIX_CTRL_CHECKED | 2, 4, LV_BUTTONMATRIX_CTRL_CHECKED | 2, SETTINGS_KB_BTN(1),
  SETTINGS_KB_BTN(1), LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2
};

#undef SETTINGS_KB_BTN

typedef enum { SAVE_DEVICE, SAVE_WEATHER, SAVE_ALARM, SAVE_TOGGLE } save_kind_t;
typedef struct {
  save_kind_t kind;
  unsigned route;
  device_config_t baseline;
  device_config_t draft;
} save_request_t;
typedef struct {
  bool ready;
  bool lookup_failed;
  esp_err_t error;
  save_kind_t kind;
  unsigned route;
  device_config_t draft;
} save_completion_t;
typedef struct {
  bool ready;
  esp_err_t error;
  unsigned route;
  size_t count;
  connectivity_scan_result_t networks[SCAN_LIMIT];
} scan_completion_t;

/* Only these completion slots cross task boundaries. Workers never keep
 * LVGL objects or access drafts. The heartbeat consumes each slot once. */
static portMUX_TYPE s_completion_lock = portMUX_INITIALIZER_UNLOCKED;
static save_completion_t s_save_completion;
static scan_completion_t s_scan_completion;
static struct {
  lv_obj_t *root, *keyboard, *status, *body;
  lv_obj_t *hour, *minute, *meridiem;
  /* Only one top-level editor is active; its nested views share this draft. */
  device_config_t applied, baseline, draft;
  connectivity_scan_result_t networks[SCAN_LIMIT];
  size_t network_count;
  unsigned route;
  bool saving, scanning, restarting;
  char notice[192];
} s_settings;

enum {
  ACTION_BACK = 100, ACTION_DONE, ACTION_CANCEL, ACTION_SAVE,
  ACTION_ALARM_TOGGLE, ACTION_FACE, ACTION_FORMAT, ACTION_ZERO,
  ACTION_UNITS, ACTION_SHARING, ACTION_SCAN, ACTION_MANUAL,
  ACTION_ALARM_DRAFT_TOGGLE, ACTION_PREVIEW,
  ACTION_DAY_BASE = 200, ACTION_SOUND_BASE = 220,
  ACTION_SNOOZE_BASE = 230, ACTION_MODE_BASE = 240,
  ACTION_NETWORK_BASE = 260
};
enum { FIELD_ROOM, FIELD_ID, FIELD_SSID, FIELD_PASSWORD, FIELD_LOCATION,
  FIELD_VOLUME, FIELD_DAY_BRIGHTNESS, FIELD_NIGHT_BRIGHTNESS, FIELD_OFFSET };

static void action_event(lv_event_t *event);
static void scan_networks(void);

static lv_color_t primary(void) { return lv_color_hex(0xf5f8ff); }
static lv_color_t muted(void) { return lv_color_hex(0xb7cbe2); }
static lv_color_t accent(void) { return lv_color_hex(0x245289); }

static void format_time(char *text, size_t size, unsigned hour, unsigned minute, const device_config_t *config)
{
  if (strcmp(config->time_format, "24") == 0) snprintf(text, size, "%02u:%02u", hour, minute);
  else {
    unsigned display_hour = hour % 12;
    if (!display_hour) display_hour = 12;
    snprintf(text, size, config->leading_zero_12h ? "%02u:%02u %s" : "%u:%02u %s", display_hour, minute, hour < 12 ? "AM" : "PM");
  }
}

static void format_fixed_time(char *text, size_t size, const char *hhmm, const device_config_t *config)
{
  unsigned hour = 0, minute = 0;
  sscanf(hhmm, "%u:%u", &hour, &minute);
  format_time(text, size, hour, minute, config);
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width, const lv_font_t *font)
{
  lv_obj_t *obj = lv_label_create(parent);
  lv_label_set_text(obj, text);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_width(obj, width);
  lv_obj_set_style_text_font(obj, font, 0);
  lv_obj_set_style_text_color(obj, primary(), 0);
  lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
  return obj;
}

static lv_obj_t *button(const char *text, int action, int x, int y, int width, int height, bool selected)
{
  lv_obj_t *obj = lv_button_create(s_settings.body);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, width, height);
  lv_obj_set_style_radius(obj, 18, 0);
  lv_obj_set_style_bg_color(obj, selected ? accent() : lv_color_hex(0x17263b), 0);
  lv_obj_set_style_border_color(obj, lv_color_hex(0x345574), 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_shadow_width(obj, 0, 0);
  lv_obj_add_event_cb(obj, action_event, LV_EVENT_CLICKED, (void *)(intptr_t)action);
  lv_obj_t *text_obj = label(obj, text, 0, 0, width - 16, &lv_font_montserrat_24);
  lv_obj_center(text_obj);
  if (s_settings.saving || s_settings.restarting) lv_obj_add_state(obj, LV_STATE_DISABLED);
  return obj;
}

static void row(const char *title, const char *value, int action, int y)
{
  char text[160];
  snprintf(text, sizeof(text), "%s\n%s", title, value);
  button(text, action, 140, y, 520, 96, false);
}

static void show_notice(const char *text)
{
  snprintf(s_settings.notice, sizeof(s_settings.notice), "%s", text ? text : "");
  if (s_settings.status) lv_label_set_text(s_settings.status, s_settings.notice);
}

static void keyboard_event(lv_event_t *event)
{
  (void) event;
  lv_keyboard_set_textarea(s_settings.keyboard, NULL);
  lv_obj_add_flag(s_settings.keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void field_focus(lv_event_t *event)
{
  if (s_settings.saving || s_settings.restarting) return;
  lv_keyboard_set_textarea(s_settings.keyboard, lv_event_get_target(event));
  lv_obj_clear_flag(s_settings.keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_settings.keyboard);
}

static void field_changed(lv_event_t *event)
{
  if (s_settings.saving || s_settings.restarting) return;
  const char *value = lv_textarea_get_text(lv_event_get_target(event));
  char *dest = NULL;
  size_t length = 0;
  switch ((int)(intptr_t)lv_event_get_user_data(event)) {
    case FIELD_ROOM: dest = s_settings.draft.room_name; length = sizeof(s_settings.draft.room_name); break;
    case FIELD_ID: dest = s_settings.draft.device_id; length = sizeof(s_settings.draft.device_id); break;
    case FIELD_SSID: dest = s_settings.draft.wifi_ssid; length = sizeof(s_settings.draft.wifi_ssid); break;
    case FIELD_PASSWORD: dest = s_settings.draft.wifi_password; length = sizeof(s_settings.draft.wifi_password); break;
    case FIELD_LOCATION:
      dest = s_settings.draft.location; length = sizeof(s_settings.draft.location);
      if (strcmp(dest, value) != 0) {
        s_settings.draft.latitude[0] = '\0';
        s_settings.draft.longitude[0] = '\0';
        s_settings.draft.timezone[0] = '\0';
      }
      break;
    default: return;
  }
  snprintf(dest, length, "%s", value);
}

static void field(const char *title, const char *value, int field_id, size_t length, int y, bool password)
{
  label(s_settings.body, title, 150, y, 500, &lv_font_montserrat_24);
  lv_obj_t *obj = lv_textarea_create(s_settings.body);
  lv_obj_set_pos(obj, 150, y + 38);
  lv_obj_set_size(obj, 500, 78);
  lv_textarea_set_one_line(obj, true);
  lv_textarea_set_max_length(obj, length - 1);
  lv_textarea_set_password_mode(obj, password);
  lv_textarea_set_text(obj, value);
  lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(obj, primary(), 0);
  lv_obj_set_style_bg_color(obj, lv_color_hex(0x17263b), 0);
  lv_obj_set_style_border_color(obj, lv_color_hex(0x345574), 0);
  lv_obj_set_style_radius(obj, 18, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
  lv_obj_set_style_bg_color(obj, muted(), LV_PART_CURSOR | LV_STATE_FOCUSED);
  lv_obj_add_event_cb(obj, field_focus, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(obj, field_changed, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)field_id);
  if (s_settings.saving || s_settings.restarting) lv_obj_add_state(obj, LV_STATE_DISABLED);
}

static void slider_changed(lv_event_t *event)
{
  if (s_settings.saving) return;
  int value = lv_slider_get_value(lv_event_get_target(event));
  int field_id = (int)(intptr_t)lv_event_get_user_data(event);
  if (field_id == FIELD_VOLUME) s_settings.draft.alarm_volume = value;
  else if (field_id == FIELD_DAY_BRIGHTNESS) s_settings.draft.day_brightness = value;
  else s_settings.draft.night_brightness = value;
  char text[80];
  snprintf(text, sizeof(text), "%s %d%%", field_id == FIELD_VOLUME ? "Volume" : field_id == FIELD_DAY_BRIGHTNESS ? "Day" : "Night", value);
  show_notice(text);
}

static void slider(const char *title, int field_id, int value, int minimum, int y)
{
  char text[80];
  snprintf(text, sizeof(text), "%s: %d%%", title, value);
  label(s_settings.body, text, 150, y, 500, &lv_font_montserrat_24);
  lv_obj_t *obj = lv_slider_create(s_settings.body);
  lv_obj_set_pos(obj, 175, y + 70);
  lv_obj_set_size(obj, 450, 24);
  lv_slider_set_range(obj, minimum, 100);
  lv_slider_set_value(obj, value, LV_ANIM_OFF);
  lv_obj_add_event_cb(obj, slider_changed, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)field_id);
  if (s_settings.saving) lv_obj_add_state(obj, LV_STATE_DISABLED);
}

static void time_changed(lv_event_t *event)
{
  (void) event;
  int hour = lv_roller_get_selected(s_settings.hour);
  int minute = lv_roller_get_selected(s_settings.minute);
  if (s_settings.meridiem) hour = ((hour + 1) % 12) + 12 * lv_roller_get_selected(s_settings.meridiem);
  wac_settings_view_t view = wac_ui_settings_view();
  if (view == WAC_SETTINGS_ALARM_TIME) {
    s_settings.draft.alarm_hour = hour;
    s_settings.draft.alarm_minute = minute;
  } else {
    char *dest = view == WAC_SETTINGS_NIGHT_START ? s_settings.draft.night_shift_start : s_settings.draft.night_shift_end;
    snprintf(dest, DEVICE_CONFIG_TIME_STR_LEN, "%02d:%02d", hour, minute);
  }
}

static lv_obj_t *roller(const char *options, int selected, int x, int width, lv_event_cb_t callback)
{
  lv_obj_t *obj = lv_roller_create(s_settings.body);
  lv_roller_set_options(obj, options, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_selected(obj, selected, LV_ANIM_OFF);
  lv_roller_set_visible_row_count(obj, 3);
  lv_obj_set_pos(obj, x, 280);
  lv_obj_set_width(obj, width);
  lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, 0);
  lv_obj_add_event_cb(obj, callback, LV_EVENT_VALUE_CHANGED, NULL);
  return obj;
}

static void time_picker(int hour, int minute, const device_config_t *draft)
{
  char hours[80] = "", minutes[190] = "", piece[8];
  bool twelve = strcmp(draft->time_format, "12") == 0;
  for (int i = 0; i < (twelve ? 12 : 24); ++i) {
    snprintf(piece, sizeof(piece), "%s%02d", i ? "\n" : "", twelve ? i + 1 : i);
    strcat(hours, piece);
  }
  for (int i = 0; i < 60; ++i) {
    snprintf(piece, sizeof(piece), "%s%02d", i ? "\n" : "", i);
    strcat(minutes, piece);
  }
  label(s_settings.body, "Hour", 170, 220, 160, &lv_font_montserrat_24);
  label(s_settings.body, "Minute", 350, 220, 160, &lv_font_montserrat_24);
  s_settings.hour = roller(hours, twelve ? ((hour + 11) % 12) : hour, 180, 150, time_changed);
  s_settings.minute = roller(minutes, minute, 355, 150, time_changed);
  s_settings.meridiem = twelve ? roller("AM\nPM", hour >= 12, 535, 110, time_changed) : NULL;
  button("Done", ACTION_DONE, 270, 662, 260, 88, true);
}

static void offset_changed(lv_event_t *event)
{
  int value = (int)lv_roller_get_selected(lv_event_get_target(event)) * 5 - 120;
  if (wac_ui_settings_view() == WAC_SETTINGS_SUNSET_OFFSET) s_settings.draft.night_sunset_offset_minutes = value;
  else s_settings.draft.night_sunrise_offset_minutes = value;
}

static void restart_timer(lv_timer_t *timer)
{
  (void) timer;
  esp_restart();
}

static void save_task(void *arg)
{
  save_request_t *request = arg;
  esp_err_t error;
  bool lookup_failed = false;
  if (request->kind == SAVE_WEATHER) {
    bool lookup = !request->draft.latitude[0] || !request->draft.longitude[0] || !request->draft.timezone[0];
    if (lookup) {
      location_lookup_result_t result = {0};
      error = connectivity_is_wifi_connected() ? location_lookup_resolve(request->draft.location, &result) : ESP_ERR_WIFI_NOT_CONNECT;
      if (error != ESP_OK) { lookup_failed = true; goto complete; }
      snprintf(request->draft.location, sizeof(request->draft.location), "%s", result.location);
      snprintf(request->draft.latitude, sizeof(request->draft.latitude), "%s", result.latitude);
      snprintf(request->draft.longitude, sizeof(request->draft.longitude), "%s", result.longitude);
      snprintf(request->draft.timezone, sizeof(request->draft.timezone), "%s", result.timezone);
    }
    error = device_config_save_weather(&request->baseline, &request->draft);
  } else if (request->kind == SAVE_DEVICE) error = device_config_save_device(&request->baseline, &request->draft);
  else if (request->kind == SAVE_ALARM) error = device_config_save_alarm(&request->baseline, &request->draft);
  else error = device_config_set_alarm_enabled(request->draft.alarm_enabled);
complete:
  portENTER_CRITICAL(&s_completion_lock);
  s_save_completion.kind = request->kind;
  s_save_completion.route = request->route;
  s_save_completion.draft = request->draft;
  s_save_completion.lookup_failed = lookup_failed;
  s_save_completion.error = error;
  s_save_completion.ready = true;
  portEXIT_CRITICAL(&s_completion_lock);
  free(request);
  vTaskDelete(NULL);
}

static void submit_save(save_kind_t kind)
{
  if (s_settings.saving || s_settings.restarting) return;
  if (kind == SAVE_ALARM && !s_settings.draft.alarm_repeat_days) {
    show_notice("Choose at least one Repeat day before saving."); return;
  }
  if (kind == SAVE_DEVICE && (!s_settings.draft.room_name[0] || !s_settings.draft.device_id[0])) {
    show_notice("Enter a Room Name and Device ID before saving."); return;
  }
  if (kind == SAVE_DEVICE && strcmp(s_settings.draft.message_sharing, "shared") == 0
      && strcmp(s_settings.draft.device_id, "clock-esp32-p4") == 0) {
    show_notice("Shared clocks need a distinct Device ID. Open Device Settings."); return;
  }
  save_request_t *request = calloc(1, sizeof(*request));
  if (!request) { show_notice("Could not start saving. Try again."); return; }
  request->kind = kind;
  request->route = s_settings.route;
  if (kind != SAVE_TOGGLE) { request->baseline = s_settings.baseline; request->draft = s_settings.draft; }
  else { request->baseline = s_settings.applied; request->draft = s_settings.applied; request->draft.alarm_enabled = !request->draft.alarm_enabled; }
  if (kind == SAVE_WEATHER && request->draft.location[0] == '\0') {
    free(request); show_notice("Enter a City, State before saving."); return;
  }
  s_settings.saving = true;
  show_notice(kind == SAVE_WEATHER ? "Checking Location and saving..." : "Saving settings...");
  if (xTaskCreate(save_task, "settings_save", 8192, request, 4, NULL) != pdPASS) {
    s_settings.saving = false;
    free(request); show_notice("Could not start saving. Try again.");
  }
  wac_settings_render(wac_ui_settings_view());
}

static void scan_task(void *arg)
{
  unsigned route = (unsigned)(uintptr_t)arg;
  scan_completion_t result = {0};
  result.route = route;
  result.error = connectivity_scan_networks(result.networks, SCAN_LIMIT, &result.count);
  result.ready = true;
  portENTER_CRITICAL(&s_completion_lock);
  s_scan_completion = result;
  portEXIT_CRITICAL(&s_completion_lock);
  vTaskDelete(NULL);
}

static void scan_networks(void)
{
  if (s_settings.scanning) return;
  s_settings.scanning = true;
  show_notice("Scanning for Networks...");
  if (xTaskCreate(scan_task, "settings_scan", 4096, (void *)(uintptr_t)s_settings.route, 4, NULL) != pdPASS) {
    s_settings.scanning = false;
    show_notice("Could not start the scan. Try Refresh.");
  }
  wac_settings_render(wac_ui_settings_view());
}

void wac_settings_start_weather(const device_config_t *config)
{
  s_settings.baseline = *config;
  s_settings.draft = *config;
}

void wac_settings_begin(lv_obj_t *parent, const device_config_t *config)
{
  s_settings.applied = *config;
  s_settings.baseline = s_settings.draft = *config;
  ++s_settings.route;
  s_settings.notice[0] = '\0';
  s_settings.root = lv_obj_create(parent);
  lv_obj_set_size(s_settings.root, 800, 800);
  lv_obj_set_pos(s_settings.root, 0, 0);
  lv_obj_set_style_bg_color(s_settings.root, lv_color_hex(0x08121f), 0);
  lv_obj_set_style_bg_opa(s_settings.root, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_settings.root, 0, 0);
  lv_obj_set_style_pad_all(s_settings.root, 0, 0);
  lv_obj_remove_flag(s_settings.root, LV_OBJ_FLAG_SCROLLABLE);
}

void wac_settings_close(void)
{
  ++s_settings.route;
  if (s_settings.root) lv_obj_delete(s_settings.root);
  s_settings.root = s_settings.body = s_settings.status = s_settings.keyboard = NULL;
}

static void navigate(wac_settings_view_t view)
{
  ++s_settings.route;
  s_settings.notice[0] = '\0';
  wac_ui_settings_navigate(view);
}

static void back(void)
{
  switch (wac_ui_settings_view()) {
    case WAC_SETTINGS_ENTRY: wac_ui_settings_close(); break;
    case WAC_SETTINGS_DEVICE_SETUP:
      s_settings.draft = s_settings.baseline;
      navigate(WAC_SETTINGS_ENTRY); break;
    case WAC_SETTINGS_WEATHER:
      s_settings.draft = s_settings.baseline;
      navigate(WAC_SETTINGS_ENTRY); break;
    case WAC_SETTINGS_ALARM:
      s_settings.draft = s_settings.baseline;
      navigate(WAC_SETTINGS_ENTRY); break;
    case WAC_SETTINGS_DISPLAY: case WAC_SETTINGS_DEVICE: case WAC_SETTINGS_WIFI: case WAC_SETTINGS_NIGHT:
      navigate(WAC_SETTINGS_DEVICE_SETUP); break;
    case WAC_SETTINGS_CREDENTIALS: navigate(WAC_SETTINGS_WIFI); break;
    case WAC_SETTINGS_ALARM_TIME: case WAC_SETTINGS_REPEAT: case WAC_SETTINGS_SOUND:
      navigate(WAC_SETTINGS_ALARM); break;
    case WAC_SETTINGS_SOUND_PICKER: case WAC_SETTINGS_SNOOZE:
      navigate(WAC_SETTINGS_SOUND); break;
    case WAC_SETTINGS_NIGHT_START: case WAC_SETTINGS_NIGHT_END:
      navigate(WAC_SETTINGS_NIGHT_FIXED); break;
    case WAC_SETTINGS_SUNSET_OFFSET: case WAC_SETTINGS_SUNRISE_OFFSET:
      navigate(WAC_SETTINGS_NIGHT_SOLAR); break;
    default: navigate(WAC_SETTINGS_NIGHT); break;
  }
}

static void action_event(lv_event_t *event)
{
  int action = (int)(intptr_t)lv_event_get_user_data(event);
  if (s_settings.saving || s_settings.restarting) return;
  if (action > WAC_SETTINGS_NONE && action <= WAC_SETTINGS_BRIGHTNESS) {
    if (action == WAC_SETTINGS_DEVICE_SETUP || action == WAC_SETTINGS_WEATHER || action == WAC_SETTINGS_ALARM) {
      s_settings.baseline = s_settings.draft = s_settings.applied;
    }
    navigate(action);
    if (action == WAC_SETTINGS_WIFI) scan_networks();
    return;
  }
  if (action == ACTION_BACK || action == ACTION_DONE || action == ACTION_CANCEL) { back(); return; }
  if (action == ACTION_SAVE) {
    submit_save(wac_ui_settings_view() == WAC_SETTINGS_WEATHER ? SAVE_WEATHER : wac_ui_settings_view() == WAC_SETTINGS_ALARM ? SAVE_ALARM : SAVE_DEVICE);
    return;
  }
  if (action == ACTION_ALARM_TOGGLE) { submit_save(SAVE_TOGGLE); return; }
  if (action == ACTION_SCAN) { scan_networks(); return; }
  if (action == ACTION_MANUAL) { navigate(WAC_SETTINGS_CREDENTIALS); return; }
  if (action >= ACTION_NETWORK_BASE && action < ACTION_NETWORK_BASE + (int)s_settings.network_count) {
    connectivity_scan_result_t *network = &s_settings.networks[action - ACTION_NETWORK_BASE];
    if (strcmp(network->ssid, s_settings.draft.wifi_ssid) != 0) s_settings.draft.wifi_password[0] = '\0';
    snprintf(s_settings.draft.wifi_ssid, sizeof(s_settings.draft.wifi_ssid), "%s", network->ssid);
    navigate(WAC_SETTINGS_CREDENTIALS); return;
  }
  device_config_t *draft = &s_settings.draft;
  if (action == ACTION_FACE) snprintf(draft->default_clock_face, sizeof(draft->default_clock_face), "%s", strcmp(draft->default_clock_face, "analog") == 0 ? "digital" : "analog");
  else if (action == ACTION_FORMAT) snprintf(draft->time_format, sizeof(draft->time_format), "%s", strcmp(draft->time_format, "24") == 0 ? "12" : "24");
  else if (action == ACTION_ZERO) draft->leading_zero_12h = !draft->leading_zero_12h;
  else if (action == ACTION_UNITS) snprintf(s_settings.draft.units, sizeof(s_settings.draft.units), "%s", strcmp(s_settings.draft.units, "metric") == 0 ? "imperial" : "metric");
  else if (action == ACTION_SHARING) snprintf(draft->message_sharing, sizeof(draft->message_sharing), "%s", strcmp(draft->message_sharing, "shared") == 0 ? "single" : "shared");
  else if (action == ACTION_ALARM_DRAFT_TOGGLE) s_settings.draft.alarm_enabled = !s_settings.draft.alarm_enabled;
  else if (action >= ACTION_DAY_BASE && action < ACTION_DAY_BASE + 7) s_settings.draft.alarm_repeat_days ^= 1 << (action - ACTION_DAY_BASE);
  else if (action >= ACTION_SOUND_BASE && action < ACTION_SOUND_BASE + 3) s_settings.draft.alarm_sound = action - ACTION_SOUND_BASE;
  else if (action >= ACTION_SNOOZE_BASE && action < ACTION_SNOOZE_BASE + 3) s_settings.draft.alarm_snooze_minutes = 5 * (action - ACTION_SNOOZE_BASE + 1);
  else if (action >= ACTION_MODE_BASE && action < ACTION_MODE_BASE + 4) draft->night_shift_mode = action - ACTION_MODE_BASE;
  else if (action == ACTION_PREVIEW) { show_notice("Sound preview is not available yet."); return; }
  wac_settings_render(wac_ui_settings_view());
}

void wac_settings_poll(void)
{
  save_completion_t save;
  scan_completion_t scan;
  portENTER_CRITICAL(&s_completion_lock);
  save = s_save_completion;
  scan = s_scan_completion;
  s_save_completion.ready = s_scan_completion.ready = false;
  portEXIT_CRITICAL(&s_completion_lock);
  if (save.ready) {
    s_settings.saving = false;
    if (save.error == ESP_OK) device_config_copy(&s_settings.applied);
    if (s_settings.root && save.route == s_settings.route) {
      if (save.error == ESP_OK) {
        if (save.kind == SAVE_DEVICE || save.kind == SAVE_WEATHER) {
          s_settings.restarting = true;
          show_notice("Settings Saved. Restarting...");
          lv_timer_t *timer = lv_timer_create(restart_timer, 900, NULL);
          if (timer) lv_timer_set_repeat_count(timer, 1);
          else esp_restart();
        } else if (save.kind == SAVE_ALARM) {
          s_settings.baseline = s_settings.draft = s_settings.applied;
          navigate(WAC_SETTINGS_ENTRY);
          show_notice("Alarm settings saved.");
        } else show_notice("Alarm setting saved.");
      } else {
        char text[192];
        if (save.kind == SAVE_WEATHER && !save.lookup_failed) {
          /* Keep a successfully resolved location after a storage failure,
           * so Retry can save it even if Wi-Fi drops in the meantime. */
          s_settings.draft = save.draft;
        }
        if (save.kind == SAVE_WEATHER && save.error == ESP_ERR_NOT_FOUND) snprintf(text, sizeof(text), "Location Not Found. Try City, State.");
        else if (save.error == ESP_ERR_WIFI_NOT_CONNECT) snprintf(text, sizeof(text), "Wi-Fi Required. Connect before saving a location.");
        else if (save.lookup_failed) snprintf(text, sizeof(text), "Could not check this location (%s). Try Save again.", esp_err_to_name(save.error));
        else snprintf(text, sizeof(text), "Could not save (%s). Check your entries and try again.", esp_err_to_name(save.error));
        show_notice(text);
      }
      wac_settings_render(wac_ui_settings_view());
    } else if (s_settings.root) {
      /* A newer view may have been opened while the previous request was
       * finishing. Unlock its controls without applying old navigation. */
      wac_settings_render(wac_ui_settings_view());
    }
  }
  if (scan.ready) {
    s_settings.scanning = false;
    if (s_settings.root && scan.route == s_settings.route && wac_ui_settings_view() == WAC_SETTINGS_WIFI) {
      s_settings.network_count = scan.error == ESP_OK ? scan.count : 0;
      memcpy(s_settings.networks, scan.networks, sizeof(s_settings.networks));
      char text[160];
      if (scan.error != ESP_OK) snprintf(text, sizeof(text), "Could not scan (%s). Refresh or enter a network manually.", esp_err_to_name(scan.error));
      else snprintf(text, sizeof(text), "%s", scan.count ? "Choose a Network" : "No Networks Found. Refresh or enter a network manually.");
      show_notice(text);
      wac_settings_render(WAC_SETTINGS_WIFI);
    } else if (s_settings.root && wac_ui_settings_view() == WAC_SETTINGS_WIFI) {
      wac_settings_render(WAC_SETTINGS_WIFI);
    }
  }
}

void wac_settings_render(wac_settings_view_t view)
{
  if (!s_settings.root || view == WAC_SETTINGS_NONE) return;
  lv_obj_clean(s_settings.root);
  s_settings.status = s_settings.keyboard = NULL;
  s_settings.hour = s_settings.minute = s_settings.meridiem = NULL;
  s_settings.body = s_settings.root;
  lv_obj_t *bg = lv_image_create(s_settings.root);
  lv_image_set_src(bg, &round_stage_day);
  lv_obj_remove_flag(bg, LV_OBJ_FLAG_CLICKABLE);
  const char *titles[] = {"", "Settings", "Device Setup", "Display", "Device", "Weather Location", "Wi-Fi", "Network Details", "Alarm", "Alarm Time", "Repeat", "Sound & Snooze", "Alarm Sound", "Snooze", "Night Shift", "Night Shift Mode", "Fixed Schedule", "Night Starts", "Night Ends", "Sunrise/Sunset", "Sunset Offset", "Sunrise Offset", "Brightness"};
  label(s_settings.body, titles[view], 245, 110, 390, &lv_font_montserrat_32);
  button(LV_SYMBOL_LEFT, ACTION_BACK, 145, 88, 88, 88, false);
  if (s_settings.saving || s_settings.restarting) {
    s_settings.status = label(s_settings.root, s_settings.notice, 155, 368, 490, &lv_font_montserrat_24);
    return;
  }
  char text[160];
  device_config_t *d = &s_settings.draft;
  static const char *modes[] = {"Off", "Always On", "Fixed Schedule", "Sunset to Sunrise"};
  static const char *sounds[] = {"Classic Beep", "Gentle Chime", "Rising Tones"};
  switch (view) {
    case WAC_SETTINGS_ENTRY:
      row("Alarm", s_settings.applied.alarm_enabled ? "On" : "Off", ACTION_ALARM_TOGGLE, 212);
      row("Alarm Settings", "Time, Repeat, Sound & Snooze", WAC_SETTINGS_ALARM, 320);
      row("Device Setup", s_settings.applied.room_name, WAC_SETTINGS_DEVICE_SETUP, 428);
      row("Weather Location", s_settings.applied.location, WAC_SETTINGS_WEATHER, 536);
      break;
    case WAC_SETTINGS_DEVICE_SETUP:
      snprintf(text, sizeof(text), "%s · %s-hour", d->default_clock_face, d->time_format);
      row("Display Settings", text, WAC_SETTINGS_DISPLAY, 212);
      row("Night Shift", modes[d->night_shift_mode], WAC_SETTINGS_NIGHT, 320);
      row("Device Settings", d->room_name, WAC_SETTINGS_DEVICE, 428);
      row("Wi-Fi", d->wifi_ssid[0] ? d->wifi_ssid : "Choose a Network", WAC_SETTINGS_WIFI, 536);
      button("Cancel", ACTION_CANCEL, 174, 646, 162, 88, false);
      button("Save & Restart", ACTION_SAVE, 358, 646, 268, 88, true);
      break;
    case WAC_SETTINGS_DISPLAY:
      row("Home Screen", strcmp(d->default_clock_face, "analog") == 0 ? "Analog" : "Digital", ACTION_FACE, 212);
      row("Time Format", strcmp(d->time_format, "24") == 0 ? "24-Hour" : "12-Hour", ACTION_FORMAT, 320);
      row("Leading Zero", d->leading_zero_12h ? "On" : "Off", ACTION_ZERO, 428);
      button("Done", ACTION_DONE, 270, 662, 260, 88, true); break;
    case WAC_SETTINGS_DEVICE:
      field("Room Name", d->room_name, FIELD_ROOM, sizeof(d->room_name), 204, false);
      field("Device ID", d->device_id, FIELD_ID, sizeof(d->device_id), 360, false);
      row("House Messages", strcmp(d->message_sharing, "shared") == 0 ? "Shared" : "Single", ACTION_SHARING, 510);
      if (!s_settings.notice[0]) show_notice("Each shared clock needs a distinct Device ID.");
      button("Done", ACTION_DONE, 270, 662, 260, 88, true); break;
    case WAC_SETTINGS_WEATHER:
      field("City, State", s_settings.draft.location, FIELD_LOCATION, sizeof(s_settings.draft.location), 212, false);
      row("Units", strcmp(s_settings.draft.units, "metric") == 0 ? "Metric" : "Imperial", ACTION_UNITS, 416);
      if (!s_settings.notice[0]) show_notice("Time zone is set automatically from this location.");
      button("Cancel", ACTION_CANCEL, 178, 646, 206, 88, false);
      button("Save", ACTION_SAVE, 412, 646, 210, 88, true); break;
    case WAC_SETTINGS_WIFI: {
      lv_obj_t *list = lv_obj_create(s_settings.body);
      lv_obj_set_pos(list, 140, 205); lv_obj_set_size(list, 520, 360);
      lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(list, 0, 0);
      lv_obj_set_style_pad_all(list, 0, 0);
      s_settings.body = list;
      for (size_t i = 0; i < s_settings.network_count; ++i) {
        snprintf(text, sizeof(text), "%s  %s", s_settings.networks[i].ssid, s_settings.networks[i].requires_password ? "Secured" : "");
        button(text, ACTION_NETWORK_BASE + i, 0, i * 82, 520, 74, false);
      }
      s_settings.body = s_settings.root;
      lv_obj_t *refresh = button("Refresh", ACTION_SCAN, 175, 570, 210, 72, false);
      if (s_settings.scanning) lv_obj_add_state(refresh, LV_STATE_DISABLED);
      button("Manual Network", ACTION_MANUAL, 410, 570, 220, 72, false);
      button("Done", ACTION_DONE, 270, 662, 260, 88, true); break;
    }
    case WAC_SETTINGS_CREDENTIALS:
      field("Network Name", d->wifi_ssid, FIELD_SSID, sizeof(d->wifi_ssid), 204, false);
      field("Password", d->wifi_password, FIELD_PASSWORD, sizeof(d->wifi_password), 372, true);
      button("Back", ACTION_BACK, 187, 646, 136, 88, false);
      button("Save & Restart", ACTION_SAVE, 345, 646, 268, 88, true); break;
    case WAC_SETTINGS_ALARM:
      format_time(text, sizeof(text), d->alarm_hour, d->alarm_minute, d);
      row("Time", text, WAC_SETTINGS_ALARM_TIME, 212);
      snprintf(text, sizeof(text), "%s", d->alarm_repeat_days == 0x3e ? "Monday–Friday" : d->alarm_repeat_days == 0x7f ? "Every Day" : "Selected Days");
      row("Repeat", text, WAC_SETTINGS_REPEAT, 320);
      row("Sound & Snooze", sounds[d->alarm_sound], WAC_SETTINGS_SOUND, 428);
      button(d->alarm_enabled ? "Alarm On" : "Alarm Off", ACTION_ALARM_DRAFT_TOGGLE, 225, 544, 350, 72, d->alarm_enabled);
      button("Cancel", ACTION_CANCEL, 178, 646, 206, 88, false);
      button("Save", ACTION_SAVE, 412, 646, 210, 88, true); break;
    case WAC_SETTINGS_ALARM_TIME: time_picker(d->alarm_hour, d->alarm_minute, d); break;
    case WAC_SETTINGS_REPEAT: {
      static const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
      for (int i = 0; i < 7; ++i) button(days[i], ACTION_DAY_BASE + i, i < 4 ? 140 : 410, 210 + (i < 4 ? i : i - 4) * 104, 250, 88, d->alarm_repeat_days & (1 << i));
      button("Done", ACTION_DONE, 270, 662, 260, 88, true); break;
    }
    case WAC_SETTINGS_SOUND:
      row("Alarm Sound", sounds[d->alarm_sound], WAC_SETTINGS_SOUND_PICKER, 212);
      slider("Volume", FIELD_VOLUME, d->alarm_volume, 0, 350);
      snprintf(text, sizeof(text), "%u Minutes", d->alarm_snooze_minutes);
      row("Snooze", text, WAC_SETTINGS_SNOOZE, 510);
      button("Done", ACTION_DONE, 270, 662, 260, 88, true); break;
    case WAC_SETTINGS_SOUND_PICKER:
      for (int i = 0; i < 3; ++i) button(sounds[i], ACTION_SOUND_BASE + i, 170, 212 + i * 110, 460, 96, d->alarm_sound == (device_config_alarm_sound_t)i);
      button("Preview Sound", ACTION_PREVIEW, 225, 558, 350, 72, false);
      button("Done", ACTION_DONE, 270, 662, 260, 88, true); break;
    case WAC_SETTINGS_SNOOZE:
      for (int i = 0; i < 3; ++i) { snprintf(text, sizeof(text), "%d Minutes", 5 * (i + 1)); button(text, ACTION_SNOOZE_BASE + i, 170, 244 + i * 122, 460, 104, d->alarm_snooze_minutes == 5 * (i + 1)); }
      button("Done", ACTION_DONE, 270, 662, 260, 88, true); break;
    case WAC_SETTINGS_NIGHT:
      row("Mode", modes[d->night_shift_mode], WAC_SETTINGS_NIGHT_MODE, 212);
      char start[24], end[24];
      format_fixed_time(start, sizeof(start), d->night_shift_start, d);
      format_fixed_time(end, sizeof(end), d->night_shift_end, d);
      snprintf(text, sizeof(text), "%s – %s", start, end);
      row("Fixed Schedule", text, WAC_SETTINGS_NIGHT_FIXED, 320);
      snprintf(text, sizeof(text), "Sunset %+d min · Sunrise %+d min", d->night_sunset_offset_minutes, d->night_sunrise_offset_minutes);
      row("Sunrise/Sunset Offsets", text, WAC_SETTINGS_NIGHT_SOLAR, 428);
      snprintf(text, sizeof(text), "Day %u%% · Night %u%%", d->day_brightness, d->night_brightness);
      row("Brightness", text, WAC_SETTINGS_BRIGHTNESS, 536);
      button("Done", ACTION_DONE, 270, 662, 260, 88, true); break;
    case WAC_SETTINGS_NIGHT_MODE:
      for (int i = 0; i < 4; ++i) button(modes[i], ACTION_MODE_BASE + i, 140, 212 + i * 108, 520, 96, d->night_shift_mode == (device_config_night_mode_t)i);
      button("Done", ACTION_DONE, 270, 662, 260, 88, true); break;
    case WAC_SETTINGS_NIGHT_FIXED:
      format_fixed_time(text, sizeof(text), d->night_shift_start, d);
      row("Night Starts", text, WAC_SETTINGS_NIGHT_START, 244);
      format_fixed_time(text, sizeof(text), d->night_shift_end, d);
      row("Night Ends", text, WAC_SETTINGS_NIGHT_END, 364);
      if (!s_settings.notice[0]) show_notice(strcmp(d->night_shift_start, d->night_shift_end) == 0 ? "Night Shift stays on all day." : strcmp(d->night_shift_end, d->night_shift_start) < 0 ? "Ends the next day." : "Starts and ends on the same day.");
      button("Done", ACTION_DONE, 270, 662, 260, 88, true); break;
    case WAC_SETTINGS_NIGHT_START: case WAC_SETTINGS_NIGHT_END: {
      int hour = 0, minute = 0;
      sscanf(view == WAC_SETTINGS_NIGHT_START ? d->night_shift_start : d->night_shift_end, "%d:%d", &hour, &minute);
      time_picker(hour, minute, d); break;
    }
    case WAC_SETTINGS_NIGHT_SOLAR:
      label(s_settings.body, s_settings.applied.location, 150, 205, 500, &lv_font_montserrat_24);
      snprintf(text, sizeof(text), "%+d Minutes", d->night_sunset_offset_minutes);
      row("Night Starts · Sunset", text, WAC_SETTINGS_SUNSET_OFFSET, 280);
      snprintf(text, sizeof(text), "%+d Minutes", d->night_sunrise_offset_minutes);
      row("Night Ends · Sunrise", text, WAC_SETTINGS_SUNRISE_OFFSET, 388);
      button("Done", ACTION_DONE, 270, 662, 260, 88, true); break;
    case WAC_SETTINGS_SUNSET_OFFSET: case WAC_SETTINGS_SUNRISE_OFFSET: {
      char options[240] = "", piece[8];
      for (int i = -120; i <= 120; i += 5) { snprintf(piece, sizeof(piece), "%s%+d", i == -120 ? "" : "\n", i); strcat(options, piece); }
      int offset = view == WAC_SETTINGS_SUNSET_OFFSET ? d->night_sunset_offset_minutes : d->night_sunrise_offset_minutes;
      roller(options, (offset + 120) / 5, 250, 300, offset_changed);
      label(s_settings.body, "Minutes before (−) or after (+)", 150, 500, 500, &lv_font_montserrat_24);
      button("Done", ACTION_DONE, 270, 662, 260, 88, true); break;
    }
    case WAC_SETTINGS_BRIGHTNESS:
      slider("Day Brightness", FIELD_DAY_BRIGHTNESS, d->day_brightness, 5, 224);
      slider("Night Brightness", FIELD_NIGHT_BRIGHTNESS, d->night_brightness, 5, 412);
      button("Done", ACTION_DONE, 270, 662, 260, 88, true); break;
    default: break;
  }
  int notice_y = (view == WAC_SETTINGS_DEVICE_SETUP || view == WAC_SETTINGS_ENTRY
    || view == WAC_SETTINGS_NIGHT || view == WAC_SETTINGS_NIGHT_MODE) ? 170
    : (view == WAC_SETTINGS_WEATHER || view == WAC_SETTINGS_CREDENTIALS
       || view == WAC_SETTINGS_NIGHT_FIXED || view == WAC_SETTINGS_NIGHT_SOLAR) ? 540 : 620;
  s_settings.status = label(s_settings.root, s_settings.notice, 155, notice_y, 490, &lv_font_montserrat_18);
  lv_obj_set_style_text_color(s_settings.status, muted(), 0);
  s_settings.keyboard = lv_keyboard_create(s_settings.root);
  lv_keyboard_set_map(s_settings.keyboard, LV_KEYBOARD_MODE_TEXT_LOWER,
                      SETTINGS_KEYBOARD_LOWER_MAP, SETTINGS_KEYBOARD_LOWER_CTRL_MAP);
  lv_keyboard_set_map(s_settings.keyboard, LV_KEYBOARD_MODE_TEXT_UPPER,
                      SETTINGS_KEYBOARD_UPPER_MAP, SETTINGS_KEYBOARD_UPPER_CTRL_MAP);
  lv_keyboard_set_map(s_settings.keyboard, LV_KEYBOARD_MODE_SPECIAL,
                      SETTINGS_KEYBOARD_SPECIAL_MAP, SETTINGS_KEYBOARD_SPECIAL_CTRL_MAP);
  lv_keyboard_set_popovers(s_settings.keyboard, true);
  lv_obj_set_size(s_settings.keyboard, 680, 280);
  lv_obj_set_pos(s_settings.keyboard, 60, 480);
  lv_obj_set_style_text_font(s_settings.keyboard, &lv_font_montserrat_24, LV_PART_ITEMS);
  lv_obj_add_event_cb(s_settings.keyboard, keyboard_event, LV_EVENT_READY, NULL);
  lv_obj_add_event_cb(s_settings.keyboard, keyboard_event, LV_EVENT_CANCEL, NULL);
  lv_obj_add_flag(s_settings.keyboard, LV_OBJ_FLAG_HIDDEN);
}
