#include "ui_private.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/round_stage_day.h"
#include "assets/round_stage_night.h"
#include "assets/wac_fonts.h"
#include "assets/wac_icons.h"
#include "alarm_schedule.h"
#include "connectivity.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "location_lookup.h"
#include "weather_client.h"

#define SCAN_LIMIT 10

/* Match the four illustrated layouts, retaining LVGL text editing and key
 * press popovers. Only mode selection needs a local callback. */
#define KEY (LV_BUTTONMATRIX_CTRL_POPOVER | 1)
#define UTILITY (LV_KEYBOARD_CTRL_BUTTON_FLAGS | 1)
#define WIDE (LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2)

static const char *const SETTINGS_KEYBOARD_LOWER_MAP[] = {
  "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
  "a", "s", "d", "f", "g", "h", "j", "k", "l", LV_SYMBOL_BACKSPACE, "\n",
  LV_SYMBOL_UP, "z", "x", "c", "v", "b", "n", "m", ".", ",", "\n",
  "1#", LV_SYMBOL_CLOSE, " ", LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
};
static const char *const SETTINGS_KEYBOARD_UPPER_MAP[] = {
  "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
  "A", "S", "D", "F", "G", "H", "J", "K", "L", LV_SYMBOL_BACKSPACE, "\n",
  LV_SYMBOL_UP, "Z", "X", "C", "V", "B", "N", "M", ".", ",", "\n",
  "1#", LV_SYMBOL_CLOSE, " ", LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
};
static const char *const SETTINGS_KEYBOARD_NUMBERS_MAP[] = {
  "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
  "@", "#", "$", "%", "&", "*", "(", ")", "-", LV_SYMBOL_BACKSPACE, "\n",
  "2/2", "?", "!", "\"", "'", ":", ";", "/", "_", ".", "\n",
  "ABC", LV_SYMBOL_CLOSE, " ", LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
};
static const char *const SETTINGS_KEYBOARD_SYMBOLS_MAP[] = {
  "+", "=", "[", "]", "{", "}", "<", ">", "\\", LV_SYMBOL_BACKSPACE, "\n",
  "^", "~", "|", "`", ",", ".", ":", ";", "_", "/", "\n",
  "1/2", "!", "?", "@", "#", "$", "%", "&", "(", ")", "\n",
  "ABC", LV_SYMBOL_CLOSE, " ", LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
};
static const lv_buttonmatrix_ctrl_t SETTINGS_KEYBOARD_LETTER_CTRL[] = {
  KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY,
  KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, 1,
  UTILITY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY,
  WIDE, UTILITY, 6, 1, 1, UTILITY
};
static const lv_buttonmatrix_ctrl_t SETTINGS_KEYBOARD_SYMBOLS_CTRL[] = {
  KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, 1,
  KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY,
  UTILITY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY,
  WIDE, UTILITY, 6, 1, 1, UTILITY
};
#undef KEY
#undef UTILITY
#undef WIDE

typedef enum { SAVE_DEVICE, SAVE_DISPLAY, SAVE_WEATHER, SAVE_ALARM, SAVE_TOGGLE, SAVE_MACHINE } save_kind_t;
typedef struct {
  save_kind_t kind;
  unsigned route;
  device_config_t baseline;
  device_config_t draft;
} save_request_t;
typedef struct {
  bool ready;
  bool lookup_failed;
  bool provider_failed;
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
typedef enum { NOTICE_GENERAL, NOTICE_LOCATION_NOT_FOUND,
  NOTICE_LOCATION_UNAVAILABLE, NOTICE_LOCATION_OFFLINE,
  NOTICE_SAVE_ERROR, NOTICE_LOCATION_CHECKING, NOTICE_PROVIDER_CHECKING, NOTICE_PROVIDER_ERROR, NOTICE_RESTART_SAVED_EDITOR } notice_kind_t;

/* Only these completion slots cross task boundaries. Workers never keep
 * LVGL objects or access drafts. The heartbeat consumes each slot once. */
static portMUX_TYPE s_completion_lock = portMUX_INITIALIZER_UNLOCKED;
static save_completion_t s_save_completion;
static scan_completion_t s_scan_completion;
static struct {
  lv_obj_t *root, *keyboard, *status, *body, *backdrop, *wifi_summary;
  lv_obj_t *heading, *back_button, *normal_note, *save_button, *toggle_button;
  lv_obj_t *preview_button, *preview_caption;
  lv_obj_t *machine_button, *machine_status, *machine_volume, *machine_list, *machine_bar, *machine_thumb, *machine_retry;
  int machine_scroll;
  wac_settings_view_t machine_return;
  esp_err_t machine_submission_error;
  bool saving_machine, machine_changed_while_saving, machine_save_failed;
  /* Only the three immediate preferences cross an outstanding save. This
   * keeps a last drag/choice intact when the shared editor is reset or closed. */
  struct { device_config_sound_machine_sound_t sound; uint8_t volume, timer; } machine_desired;
  wac_audio_snapshot_t audio;
  wac_alarm_snapshot_t alarm;
  bool alarm_snapshot_valid;
  esp_err_t preview_submission_error;
  struct { lv_obj_t *block, *input, *summary, *arrow, *clear; int y; } fields[6];
  lv_obj_t *active_input;
  const char *network_note;
  int notice_y;
  bool field_error;
  notice_kind_t notice_kind;
  lv_obj_t *percent_values[3];
  lv_obj_t *solar_times[2], *solar_note, *solar_description;
  solar_times_t solar;
  bool clock_usable;
  time_t clock_utc;
  /* Only one top-level editor is active; its nested views share this draft. */
  device_config_t applied, baseline, draft;
  /* Weather has an independent draft while its Device parent keeps edits. */
  device_config_t held_device_draft;
  bool holding_device_draft;
  connectivity_scan_result_t networks[SCAN_LIMIT];
  size_t network_count;
  esp_err_t scan_error;
  unsigned route;
  bool saving, scanning, restarting;
  char notice[192];
} s_settings;

enum {
  ACTION_BACK = 100, ACTION_DONE, ACTION_CANCEL, ACTION_SAVE,
  ACTION_ALARM_TOGGLE, ACTION_FACE, ACTION_FORMAT, ACTION_ZERO,
  ACTION_UNITS, ACTION_SHARING, ACTION_SCAN, ACTION_MANUAL, ACTION_PROVIDER,
  ACTION_PREVIEW, ACTION_CLEAR_KEY, ACTION_MACHINE_PLAY, ACTION_MACHINE_RETRY,
  ACTION_HOUR_UP, ACTION_HOUR_DOWN, ACTION_MINUTE_UP, ACTION_MINUTE_DOWN,
  ACTION_AM, ACTION_PM, ACTION_WEEKDAYS, ACTION_EVERY_DAY, ACTION_REPEAT_NONE,
  ACTION_OFFSET_BEFORE, ACTION_OFFSET_AT, ACTION_OFFSET_AFTER,
  ACTION_OFFSET_MINUS, ACTION_OFFSET_PLUS,
  ACTION_DAY_BASE = 200, ACTION_SOUND_BASE = 220,
  ACTION_SNOOZE_BASE = 230, ACTION_MODE_BASE = 240,
  ACTION_NETWORK_BASE = 260, ACTION_MACHINE_SOUND_BASE = 280, ACTION_MACHINE_TIMER_BASE = 300
};
enum { FIELD_ROOM, FIELD_ID, FIELD_SSID, FIELD_PASSWORD, FIELD_LOCATION, FIELD_API_KEY,
  FIELD_VOLUME, FIELD_DAY_BRIGHTNESS, FIELD_NIGHT_BRIGHTNESS };

static void action_event(lv_event_t *event);
static void scan_networks(void);
static void navigate(wac_settings_view_t view);
static void submit_save(save_kind_t kind);
static void focus_validation(wac_settings_view_t view, int field_id, const char *text);

/* Updating these shared color styles preserves the active editor, its cursor
 * and keyboard. Appearance is supplied by the Night Shift owner through ui.c. */
enum { STYLE_PRIMARY, STYLE_MUTED, STYLE_ACCENT, STYLE_BUTTON,
  STYLE_SELECTED, STYLE_TRACK, STYLE_TRACK_ON, STYLE_KNOB, STYLE_RANGE, STYLE_SCROLL_TRACK, STYLE_SCROLL_THUMB, STYLE_COUNT };
static lv_style_t s_palette[STYLE_COUNT];
static bool s_palette_ready, s_night;

static lv_color_t primary(void) { return lv_color_hex(s_night ? 0xecc0c0 : 0xf5f8ff); }
static lv_color_t muted(void) { return lv_color_hex(s_night ? 0xc17a7a : 0xa6b6cf); }
static lv_color_t accent(void) { return lv_color_hex(s_night ? 0xe1b6b6 : 0x93d1ff); }

void wac_settings_apply_night(bool active)
{
  if (s_palette_ready && s_night == active) return;
  if (!s_palette_ready) {
    for (int i = 0; i < STYLE_COUNT; ++i) lv_style_init(&s_palette[i]);
    s_palette_ready = true;
  }
  s_night = active;
  lv_color_t line = lv_color_hex(active ? 0x633737 : 0x253a57);
  lv_color_t selected = lv_color_hex(active ? 0x643437 : 0x245289);
  lv_style_set_text_color(&s_palette[STYLE_PRIMARY], primary());
  lv_style_set_border_color(&s_palette[STYLE_PRIMARY], primary());
  lv_style_set_text_color(&s_palette[STYLE_MUTED], muted());
  lv_style_set_text_color(&s_palette[STYLE_ACCENT], accent());
  lv_style_set_image_recolor(&s_palette[STYLE_PRIMARY], primary());
  lv_style_set_image_recolor_opa(&s_palette[STYLE_PRIMARY], LV_OPA_COVER);
  lv_style_set_image_recolor(&s_palette[STYLE_ACCENT], accent());
  lv_style_set_image_recolor_opa(&s_palette[STYLE_ACCENT], LV_OPA_COVER);
  lv_style_set_bg_color(&s_palette[STYLE_BUTTON], lv_color_hex(active ? 0x321c20 : 0x17263b));
  lv_style_set_border_color(&s_palette[STYLE_BUTTON], line);
  lv_style_set_text_color(&s_palette[STYLE_BUTTON], primary());
  lv_style_set_bg_color(&s_palette[STYLE_SELECTED], selected);
  lv_style_set_border_color(&s_palette[STYLE_SELECTED], accent());
  lv_style_set_bg_color(&s_palette[STYLE_TRACK], line);
  lv_style_set_border_color(&s_palette[STYLE_TRACK], muted());
  lv_style_set_bg_color(&s_palette[STYLE_TRACK_ON], selected);
  lv_style_set_border_color(&s_palette[STYLE_TRACK_ON], accent());
  lv_style_set_bg_color(&s_palette[STYLE_KNOB], primary());
  lv_style_set_bg_color(&s_palette[STYLE_SCROLL_TRACK], muted());
  lv_style_set_bg_color(&s_palette[STYLE_SCROLL_THUMB], accent());
  lv_style_set_bg_color(&s_palette[STYLE_RANGE], lv_color_hex(active ? 0xc17a7a : 0x78b8ff));
  for (int i = 0; i < STYLE_COUNT; ++i) lv_obj_report_style_change(&s_palette[i]);
  if (s_settings.root) lv_obj_set_style_bg_color(s_settings.root, lv_color_hex(active ? 0x18090c : 0x07101c), 0);
  if (s_settings.backdrop) lv_image_set_src(s_settings.backdrop, active ? &round_stage_night : &round_stage_day);
}

static void format_time(char *text, size_t size, unsigned hour, unsigned minute,
  const device_config_t *config)
{
  if (strcmp(config->time_format, "24") == 0) snprintf(text, size, "%02u:%02u", hour, minute);
  else {
    unsigned display_hour = hour % 12;
    if (!display_hour) display_hour = 12;
    snprintf(text, size, "%u:%02u %s", display_hour, minute, hour < 12 ? "AM" : "PM");
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
  lv_obj_add_style(obj, &s_palette[STYLE_PRIMARY], 0);
  lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
  return obj;
}

static lv_obj_t *button_box(int action, int x, int y, int width, int height, bool selected)
{
  lv_obj_t *obj = lv_button_create(s_settings.body);
  lv_obj_remove_style_all(obj);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, width, height);
  lv_obj_set_style_radius(obj, 24, 0);
  lv_obj_add_style(obj, &s_palette[STYLE_BUTTON], 0);
  if (selected) lv_obj_add_style(obj, &s_palette[STYLE_SELECTED], 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(obj, 20, 0);
  lv_obj_set_style_pad_ver(obj, 0, 0);
  lv_obj_set_style_opa(obj, LV_OPA_40, LV_STATE_DISABLED);
  lv_obj_set_style_shadow_width(obj, 0, 0);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(obj, action_event, LV_EVENT_CLICKED, (void *)(intptr_t)action);
  if (action == ACTION_SAVE) s_settings.save_button = obj;
  if (action == ACTION_ALARM_TOGGLE) s_settings.toggle_button = obj;
  if ((s_settings.saving && (!s_settings.saving_machine || action == ACTION_SAVE || action == ACTION_ALARM_TOGGLE))
      || s_settings.restarting) lv_obj_add_state(obj, LV_STATE_DISABLED);
  return obj;
}

static lv_obj_t *button(const char *text, int action, int x, int y, int width, int height, bool selected)
{
  lv_obj_t *obj = button_box(action, x, y, width, height, selected);
  lv_obj_t *text_obj = label(obj, text, 0, 0, width - 42, &lv_font_montserrat_28);
  lv_obj_center(text_obj);
  return obj;
}

static lv_obj_t *image_icon(lv_obj_t *parent, const lv_image_dsc_t *source, int style)
{
  lv_obj_t *obj = lv_image_create(parent);
  lv_image_set_src(obj, source);
  lv_obj_add_style(obj, &s_palette[style], 0);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  return obj;
}

static void chevron(lv_obj_t *parent, bool left)
{
  lv_obj_t *obj = image_icon(parent,
    left ? &wac_icon_chevron_left : &wac_icon_chevron_right, STYLE_PRIMARY);
  lv_obj_align(obj, left ? LV_ALIGN_CENTER : LV_ALIGN_RIGHT_MID, 0, 0);
}

static lv_obj_t *entry_box(const char *title, const char *subtitle, int action, int y, bool selected)
{
  lv_obj_t *obj = button_box(action, 140, y, 520, 96, selected);
  bool solar = action == WAC_SETTINGS_SUNSET_OFFSET || action == WAC_SETTINGS_SUNRISE_OFFSET;
  int text_width = solar ? 325 : 432;
  int title_height = lv_font_get_line_height(&lv_font_montserrat_28);
  lv_obj_t *title_obj = label(obj, title, 0, 0, text_width, &lv_font_montserrat_28);
  lv_obj_set_style_text_align(title_obj, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_long_mode(title_obj, LV_LABEL_LONG_DOT);
  lv_obj_align(title_obj, LV_ALIGN_LEFT_MID, 0, 0);
  if (subtitle) {
    lv_obj_t *detail = label(obj, subtitle, 0, 0,
      action == WAC_SETTINGS_WIFI ? 420 : text_width,
      solar ? &lv_font_montserrat_22 : &wac_font_settings_24);
    lv_obj_add_style(detail, &s_palette[STYLE_MUTED], 0);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_LEFT, 0);
    if (action == WAC_SETTINGS_WIFI) lv_label_set_long_mode(detail, LV_LABEL_LONG_DOT);
    else if (solar) lv_label_set_long_mode(detail, LV_LABEL_LONG_CLIP);
    lv_obj_update_layout(detail);
    int subtitle_height = lv_obj_get_height(detail);
    int stack_height = title_height + 6 + subtitle_height;
    lv_obj_align(title_obj, LV_ALIGN_LEFT_MID, 0, (title_height - stack_height) / 2);
    lv_obj_align(detail, LV_ALIGN_LEFT_MID, 0, (stack_height - subtitle_height) / 2);
    if (action == WAC_SETTINGS_WIFI) s_settings.wifi_summary = detail;
  }
  return obj;
}

static void entry_row(const char *title, const char *subtitle, int action, int y)
{
  chevron(entry_box(title, subtitle, action, y, false), false);
}

static void value_row(const char *title, const char *value, int action, int y)
{
  lv_obj_t *obj = button_box(action, 140, y, 520, 96, false);
  lv_obj_t *title_obj = label(obj, title, 0, 0, action == ACTION_PROVIDER ? 200 : 260, &lv_font_montserrat_28);
  lv_obj_set_style_text_align(title_obj, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(title_obj, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_t *value_obj = label(obj, value, 0, 0, action == ACTION_PROVIDER ? 260 : 200, &lv_font_montserrat_28);
  lv_obj_add_style(value_obj, &s_palette[STYLE_ACCENT], 0);
  lv_obj_set_style_text_align(value_obj, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(value_obj, LV_ALIGN_RIGHT_MID, 0, 0);
}

static lv_obj_t *switch_row(const char *title, bool enabled, int action, int y)
{
  lv_obj_t *obj = button_box(action, 140, y, 520, 96, false);
  lv_obj_t *title_obj = label(obj, title, 0, 0, 260, &lv_font_montserrat_28);
  lv_obj_set_style_text_align(title_obj, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(title_obj, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_t *value = label(obj, enabled ? "On" : "Off", 0, 0, 80, &lv_font_montserrat_28);
  lv_obj_add_style(value, &s_palette[STYLE_ACCENT], 0);
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(value, LV_ALIGN_RIGHT_MID, -108, 0);
  lv_obj_t *track = lv_obj_create(obj);
  lv_obj_remove_style_all(track);
  lv_obj_set_size(track, 100, 56);
  lv_obj_set_style_radius(track, 28, 0);
  lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(track, 1, 0);
  lv_obj_add_style(track, &s_palette[enabled ? STYLE_TRACK_ON : STYLE_TRACK], 0);
  lv_obj_align(track, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_remove_flag(track, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *knob = lv_obj_create(track);
  lv_obj_remove_style_all(knob);
  lv_obj_set_size(knob, 44, 44);
  /* Like the reference CSS, offsets begin inside the one-pixel border. */
  lv_obj_set_pos(knob, enabled ? 49 : 5, 5);
  lv_obj_set_style_radius(knob, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(knob, LV_OPA_COVER, 0);
  lv_obj_add_style(knob, &s_palette[STYLE_KNOB], 0);
  lv_obj_remove_flag(knob, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  return obj;
}

static void check_icon(lv_obj_t *parent)
{
  lv_obj_align(image_icon(parent, &wac_icon_check, STYLE_PRIMARY), LV_ALIGN_RIGHT_MID, 0, 0);
}

static void arrow_button(int action, int x, int y, const lv_image_dsc_t *source)
{
  lv_obj_t *obj = button_box(action, x, y, 130, 96, false);
  lv_obj_center(image_icon(obj, source, STYLE_PRIMARY));
}

static void value_entry(const char *title, const char *value, int action, int y, int height)
{
  lv_obj_t *obj = button_box(action, 140, y, 520, height, false);
  lv_obj_t *caption = label(obj, title, 0, 0, 240, &lv_font_montserrat_28);
  lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(caption, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_t *text = label(obj, value, 0, 0, action == WAC_SETTINGS_REPEAT ? 260 : 200, &wac_font_settings_28);
  lv_obj_add_style(text, &s_palette[STYLE_ACCENT], 0);
  lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(text, LV_ALIGN_RIGHT_MID, -40, 0);
  chevron(obj, false);
  lv_obj_add_style(lv_obj_get_child(obj, -1), &s_palette[STYLE_ACCENT], 0);
}

static void status_mark(const lv_image_dsc_t *source)
{
  lv_obj_t *mark = lv_obj_create(s_settings.body);
  lv_obj_remove_style_all(mark);
  lv_obj_set_pos(mark, 352, 254);
  lv_obj_set_size(mark, 96, 96);
  lv_obj_remove_flag(mark, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_center(image_icon(mark, source, STYLE_ACCENT));
}

static void network_row(const connectivity_scan_result_t *network, size_t index)
{
  lv_obj_t *obj = button_box(ACTION_NETWORK_BASE + index, 0, index * 108, 520, 96, false);
  lv_obj_t *title = label(obj, network->ssid, 0, 0, 420, &lv_font_montserrat_28);
  lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_LEFT, 0);
  char text[64];
  snprintf(text, sizeof(text), "%s · %d dBm", network->requires_password ? "Secured" : "Open", network->rssi);
  lv_obj_t *detail = label(obj, text, 0, 0, 420, &wac_font_settings_24);
  lv_obj_add_style(detail, &s_palette[STYLE_MUTED], 0);
  lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_LEFT, 0);
  int title_height = lv_font_get_line_height(&lv_font_montserrat_28);
  int detail_height = lv_font_get_line_height(&wac_font_settings_24);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, -(detail_height + 6) / 2);
  lv_obj_align(detail, LV_ALIGN_LEFT_MID, 0, (title_height + 6) / 2);
  const lv_image_dsc_t *icon = network->rssi >= -50 ? &wac_icon_wifi
    : network->rssi >= -65 ? &wac_icon_wifi_high : &wac_icon_wifi_low;
  lv_obj_align(image_icon(obj, icon, STYLE_PRIMARY), LV_ALIGN_RIGHT_MID, 0, 0);
}

static void center_icon_text(lv_obj_t *obj, bool icon_first)
{
  lv_obj_update_layout(obj);
  lv_obj_t *caption = lv_obj_get_child(obj, 0);
  lv_obj_t *icon = lv_obj_get_child(obj, 1);
  int text_width = lv_obj_get_width(caption);
  int left = (lv_obj_get_content_width(obj) - text_width - 14 - 32) / 2;
  lv_obj_align(caption, LV_ALIGN_LEFT_MID, left + (icon_first ? 46 : 0), 0);
  lv_obj_align(icon, LV_ALIGN_LEFT_MID, left + (icon_first ? 0 : text_width + 14), 0);
}

static lv_obj_t *icon_button(const char *text, int action, int x, int y, int width, int height,
  const lv_image_dsc_t *source, bool icon_first, bool selected)
{
  lv_obj_t *obj = button_box(action, x, y, width, height, selected);
  label(obj, text, 0, 0, LV_SIZE_CONTENT, &lv_font_montserrat_28);
  image_icon(obj, source, STYLE_PRIMARY);
  center_icon_text(obj, icon_first);
  return obj;
}

static const char *const machine_sounds[] = {"Rain", "Thunder", "Ocean", "Wind", "Forest", "Campfire", "Stream", "Night", "Singing Bowl", "White Noise", "Pink Noise", "Brown Noise"};
static const char *const machine_descriptions[] = {"Steady rainfall.", "Distant rolling thunder.", "Waves washing ashore.", "A soft, steady breeze.", "Birdsong and forest ambience.", "Crackling wood fire.", "Flowing water.", "Crickets and night ambience.", "Resonant singing bowl tones.", "A steady, bright hiss.", "Softer, balanced noise.", "Deep, low noise."};
static const unsigned machine_timers[] = {0, 15, 30, 60, 120};
static bool machine_view(wac_settings_view_t view)
{
  return view >= WAC_SETTINGS_MACHINE && view <= WAC_SETTINGS_MACHINE_TIMER;
}
static void refresh_machine(void)
{
  if (!s_settings.machine_button) return;
  bool busy = s_settings.audio.machine_active || s_settings.audio.machine_pending;
  lv_obj_t *title = lv_obj_get_child(s_settings.machine_button, 0);
  const char *caption = busy ? "Stop" : "Play";
  bool layout_changed = strcmp(lv_label_get_text(title), caption) != 0;
  wac_ui_set_label_text(title, caption);
  layout_changed |= wac_ui_set_image_asset(lv_obj_get_child(s_settings.machine_button, 1),
    busy ? &wac_icon_square : &wac_icon_play);
  if (layout_changed) center_icon_text(s_settings.machine_button, true);
  if (!s_settings.audio.ready && !busy) lv_obj_add_state(s_settings.machine_button, LV_STATE_DISABLED);
  else lv_obj_remove_state(s_settings.machine_button, LV_STATE_DISABLED);
  char text[80];
  if (s_settings.machine_submission_error != ESP_OK) snprintf(text, sizeof(text), "Could not start. Try Play again.");
  else if (!s_settings.audio.ready) snprintf(text, sizeof(text), "Audio unavailable.");
  else if (s_settings.audio.error != ESP_OK) snprintf(text, sizeof(text), "Stopped · Audio failed. Try Play again.");
  else if (s_settings.audio.machine_pending) snprintf(text, sizeof(text), "Starting…");
  else if (!s_settings.audio.machine_active) snprintf(text, sizeof(text), "Stopped");
  else if (s_settings.audio.machine_continuous) snprintf(text, sizeof(text), "Playing · Continuous");
  else snprintf(text, sizeof(text), "Playing · %u min left", (unsigned)((s_settings.audio.machine_remaining_seconds + 59) / 60));
  wac_ui_set_label_text(s_settings.machine_status, text);
}
static void copy_machine_preferences(device_config_t *to, const device_config_t *from)
{
  to->sound_machine_sound = from->sound_machine_sound;
  to->sound_machine_volume = from->sound_machine_volume;
  to->sound_machine_timer_minutes = from->sound_machine_timer_minutes;
}
static void apply_machine_desired(device_config_t *to)
{
  to->sound_machine_sound = s_settings.machine_desired.sound;
  to->sound_machine_volume = s_settings.machine_desired.volume;
  to->sound_machine_timer_minutes = s_settings.machine_desired.timer;
}
static bool machine_desired_equals(const device_config_t *config)
{
  return s_settings.machine_desired.sound == config->sound_machine_sound
    && s_settings.machine_desired.volume == config->sound_machine_volume
    && s_settings.machine_desired.timer == config->sound_machine_timer_minutes;
}

/* The copied owner state drives this existing button and caption in place;
 * playback completion must not rebuild a draft or an active editor. */
static void refresh_audio(void)
{
  if (!s_settings.preview_button) return;
  bool active = s_settings.audio.preview_active;
  lv_obj_t *obj = s_settings.preview_button;
  lv_obj_t *title = lv_obj_get_child(obj, 0);
  lv_obj_t *speaker = lv_obj_get_child(obj, 1);
  lv_obj_t *square = lv_obj_get_child(obj, 2);
  const char *caption = active ? "Stop Preview" : "Preview Sound";
  bool layout_changed = strcmp(lv_label_get_text(title), caption) != 0
    || lv_obj_get_style_align(square, 0) != LV_ALIGN_LEFT_MID;
  wac_ui_set_label_text(title, caption);
  lv_obj_add_flag(active ? speaker : square, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(active ? square : speaker, LV_OBJ_FLAG_HIDDEN);
  if (!s_settings.audio.ready && !active) lv_obj_add_state(obj, LV_STATE_DISABLED);
  else lv_obj_remove_state(obj, LV_STATE_DISABLED);
  if (layout_changed) {
    lv_obj_update_layout(obj);
    int left = (lv_obj_get_content_width(obj) - lv_obj_get_width(title) - 46) / 2;
    lv_obj_align(title, LV_ALIGN_LEFT_MID, left + 46, 0);
    lv_obj_align(speaker, LV_ALIGN_LEFT_MID, left, 0);
    lv_obj_align(square, LV_ALIGN_LEFT_MID, left, 0);
  }
  char text[64];
  if (s_settings.preview_submission_error != ESP_OK)
    snprintf(text, sizeof(text), "Preview could not start.");
  else if (!s_settings.audio.ready)
    snprintf(text, sizeof(text), "Audio unavailable.");
  else if (s_settings.audio.error != ESP_OK)
    snprintf(text, sizeof(text), "Audio preview failed. Try again.");
  else snprintf(text, sizeof(text), "Volume · %u%%", s_settings.draft.alarm_volume);
  wac_ui_set_label_text(s_settings.preview_caption, text);
}

void wac_settings_update_audio(const wac_audio_snapshot_t *audio)
{
  if (!audio) return;
  s_settings.audio = *audio;
  refresh_audio();
  refresh_machine();
}

static void stop_preview(void)
{
  /* Ringing hides accepted previews in the owner snapshot. Still send Stop:
   * a preview may be waiting for speaker setup when the alarm is dismissed.
   * This short owner read performs no playback or hardware operation. */
  wac_audio_copy(&s_settings.audio);
  if (!s_settings.audio.ready) return;
  esp_err_t error = wac_audio_stop_preview();
  if (error != ESP_OK) s_settings.preview_submission_error = error;
  wac_audio_copy(&s_settings.audio);
  refresh_audio();
}

static void refresh_button(void)
{
  lv_obj_t *obj = icon_button("Refresh", ACTION_SCAN, 270, 664, 260, 80,
    &wac_icon_refresh, true, false);
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  center_icon_text(obj, true);
  for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
    lv_obj_add_style(lv_obj_get_child(obj, i), &s_palette[STYLE_ACCENT], 0);
  if (s_settings.scanning) lv_obj_add_state(obj, LV_STATE_DISABLED);
}

static void format_alarm_preview(char *text, size_t size)
{
  const device_config_t *draft = &s_settings.draft;
  if (!draft->alarm_enabled) { snprintf(text, size, "Alarm off"); return; }
  struct tm now;
  if (!s_settings.clock_usable || !localtime_r(&s_settings.clock_utc, &now)) {
    snprintf(text, size, "Waiting for time sync");
    return;
  }
  wac_alarm_schedule_t schedule;
  wac_alarm_schedule_calculate(draft, s_settings.clock_utc, &schedule);
  /* The saved one-time target belongs to the alarm owner. Recalculating it
   * after midnight or its due time would show a different occurrence. */
  if (!draft->alarm_repeat_days && s_settings.alarm_snapshot_valid
      && s_settings.applied.alarm_enabled && !s_settings.applied.alarm_repeat_days
      && draft->alarm_hour == s_settings.applied.alarm_hour
      && draft->alarm_minute == s_settings.applied.alarm_minute) {
    schedule.next = s_settings.alarm.next.valid ? s_settings.alarm.next
      : s_settings.alarm.phase != WAC_ALARM_IDLE ? s_settings.alarm.current
      : (wac_alarm_occurrence_t){0};
    if (!schedule.next.valid) { snprintf(text, size, "No upcoming alarm"); return; }
  }
  if (!schedule.next.valid) { snprintf(text, size, "Waiting for time sync"); return; }
  struct tm tomorrow = now;
  tomorrow.tm_mday++;
  tomorrow.tm_hour = 12;
  tomorrow.tm_isdst = -1;
  mktime(&tomorrow);
  int today_date = (now.tm_year + 1900) * 10000 + (now.tm_mon + 1) * 100 + now.tm_mday;
  int tomorrow_date = (tomorrow.tm_year + 1900) * 10000 + (tomorrow.tm_mon + 1) * 100 + tomorrow.tm_mday;
  /* Label the calculator's intended date and saved minute. A skipped local
   * time may ring at a different wall time without changing that schedule. */
  struct tm next = {.tm_year = schedule.next.local_date / 10000 - 1900,
    .tm_mon = (schedule.next.local_date / 100) % 100 - 1,
    .tm_mday = schedule.next.local_date % 100, .tm_hour = 12, .tm_isdst = -1};
  mktime(&next);
  static const char *const weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char *day = schedule.next.local_date == today_date ? "Today"
    : schedule.next.local_date == tomorrow_date ? "Tomorrow" : weekdays[next.tm_wday];
  char time[32];
  format_time(time, sizeof(time), schedule.next.minute / 60, schedule.next.minute % 60, draft);
  snprintf(text, size, "%s · %s", day, time);
}

static void refresh_notice(void)
{
  if (!s_settings.status) return;
  char alarm_preview[64];
  const char *text = s_settings.notice;
  wac_settings_view_t view = wac_ui_settings_view();
  if (view == WAC_SETTINGS_ALARM) {
    format_alarm_preview(alarm_preview, sizeof(alarm_preview));
    text = s_settings.saving && !s_settings.saving_machine ? "Saving settings..."
      : s_settings.notice_kind == NOTICE_SAVE_ERROR ? "Could not save. Try again."
      : alarm_preview;
  } else if (!s_settings.field_error && view == WAC_SETTINGS_WEATHER) {
    switch (s_settings.notice_kind) {
      case NOTICE_LOCATION_NOT_FOUND: text = "Location not found."; break;
      case NOTICE_LOCATION_UNAVAILABLE: text = "Lookup unavailable."; break;
      case NOTICE_LOCATION_OFFLINE: text = "Wi-Fi Required.\nConnect before saving a location."; break;
      case NOTICE_SAVE_ERROR: text = "Couldn't save settings.\nTry Save again."; break;
      case NOTICE_LOCATION_CHECKING: text = "Checking Location…"; break;
      case NOTICE_PROVIDER_CHECKING: text = "Checking Weather…"; break;
      case NOTICE_PROVIDER_ERROR: text = "Weather check failed.\nTry Save again."; break;
      default: break;
    }
  } else if (!s_settings.field_error && view == WAC_SETTINGS_CREDENTIALS
      && s_settings.notice_kind == NOTICE_SAVE_ERROR)
    text = "Couldn’t save settings.\nTry Save & Restart again.";
  if (s_settings.active_input && !s_settings.field_error) {
    text = "";
    if (wac_ui_settings_view() == WAC_SETTINGS_WEATHER) {
      if (s_settings.notice_kind == NOTICE_LOCATION_NOT_FOUND)
        text = "Location not found.";
      else if (s_settings.notice_kind == NOTICE_LOCATION_UNAVAILABLE)
        text = "Lookup unavailable.";
      else if (s_settings.notice_kind == NOTICE_LOCATION_CHECKING)
        text = "Checking Location…";
    }
  }
  if (view == WAC_SETTINGS_WEATHER) {
    bool two_lines = strchr(text, '\n') != NULL;
    lv_obj_set_pos(s_settings.status, s_settings.field_error ? 230 : 245,
      two_lines ? 170 : 178);
    lv_obj_set_width(s_settings.status, s_settings.field_error ? 340 : 310);
    lv_obj_set_style_text_font(s_settings.status,
      s_settings.field_error || two_lines ? &lv_font_montserrat_18
      : strstr(text, "…") ? &wac_font_settings_26 : &lv_font_montserrat_22, 0);
  }
  lv_label_set_text(s_settings.status, text);
}

void wac_settings_update_alarm(const device_config_t *config, const wac_alarm_snapshot_t *alarm)
{
  if (!config || !alarm) return;
  s_settings.alarm = *alarm;
  s_settings.alarm_snapshot_valid = true;
  bool enabled_changed = s_settings.draft.alarm_enabled != config->alarm_enabled;
  s_settings.applied = *config;
  s_settings.baseline.alarm_enabled = config->alarm_enabled;
  s_settings.draft.alarm_enabled = config->alarm_enabled;
  /* Auto-Off can arrive while any nested editor is open. Change only the
   * immediate switch, retaining all other draft fields and active controls. */
  if (enabled_changed && s_settings.toggle_button) {
    lv_obj_t *value = lv_obj_get_child(s_settings.toggle_button, 1);
    lv_label_set_text(value, config->alarm_enabled ? "On" : "Off");
    lv_obj_t *track = lv_obj_get_child(s_settings.toggle_button, 2);
    lv_obj_remove_style(track, &s_palette[STYLE_TRACK], 0);
    lv_obj_remove_style(track, &s_palette[STYLE_TRACK_ON], 0);
    lv_obj_add_style(track, &s_palette[config->alarm_enabled ? STYLE_TRACK_ON : STYLE_TRACK], 0);
    lv_obj_set_pos(lv_obj_get_child(track, 0), config->alarm_enabled ? 49 : 5, 5);
  }
}

void wac_settings_update_clock(const wac_time_snapshot_t *clock)
{
  if (!clock) return;
  s_settings.clock_usable = clock->usable;
  s_settings.clock_utc = clock->utc_now;
  if (wac_ui_settings_view() == WAC_SETTINGS_ALARM) refresh_notice();
}

static void show_notice(const char *text)
{
  snprintf(s_settings.notice, sizeof(s_settings.notice), "%s", text ? text : "");
  s_settings.notice_kind = NOTICE_GENERAL;
  refresh_notice();
  if (s_settings.normal_note) {
    bool location_note = wac_ui_settings_view() == WAC_SETTINGS_WEATHER
      && s_settings.active_input == s_settings.fields[FIELD_LOCATION].input;
    if (location_note) lv_obj_remove_flag(s_settings.normal_note, LV_OBJ_FLAG_HIDDEN);
    else if (s_settings.notice[0] || s_settings.active_input)
      lv_obj_add_flag(s_settings.normal_note, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(s_settings.normal_note, LV_OBJ_FLAG_HIDDEN);
  }
}

static bool has_text(const char *value)
{
  while (*value) if (!isspace((unsigned char)*value++)) return true;
  return false;
}

static const char *field_summary(int field_id)
{
  const char *value = lv_textarea_get_text(s_settings.fields[field_id].input);
  if (field_id == FIELD_API_KEY)
    return s_settings.draft.weather_provider != DEVICE_CONFIG_WEATHER_VISUAL_CROSSING ? "Not required" : !has_text(value) ? "Required"
      : strcmp(value, s_settings.baseline.weather_api_key) == 0 ? "Configured" : "Entered (not saved)";
  return field_id == FIELD_LOCATION && !value[0] ? "City, State" : value;
}

static void typing_layout(lv_obj_t *input)
{
  s_settings.active_input = input;
  lv_obj_t *active_block = input ? lv_obj_get_parent(input) : NULL;
  for (uint32_t i = 0; i < lv_obj_get_child_count(s_settings.root); ++i) {
    lv_obj_t *obj = lv_obj_get_child(s_settings.root, i);
    bool visible = !input || obj == active_block || obj == s_settings.backdrop
      || obj == s_settings.heading || obj == s_settings.back_button
      || obj == s_settings.keyboard || obj == s_settings.status;
    if (obj == s_settings.keyboard) visible = input != NULL;
    if (visible) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
  for (int i = 0; i < 6; ++i) if (s_settings.fields[i].block) {
    lv_obj_t *block = s_settings.fields[i].block;
    lv_obj_set_y(block, s_settings.fields[i].input == input ? 212 : s_settings.fields[i].y);
    if (!s_settings.fields[i].summary) continue;
    bool typing = s_settings.fields[i].input == input;
    if (s_settings.fields[i].clear) {
      if (typing) lv_obj_remove_flag(s_settings.fields[i].clear, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(s_settings.fields[i].clear, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_t *caption = lv_obj_get_child(block, 0);
    lv_obj_set_height(block, typing ? 140 : 96);
    lv_obj_set_style_border_width(block, typing ? 0 : 1, 0);
    lv_obj_set_style_bg_opa(block, typing ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_pos(caption, typing ? 0 : 20, typing ? 0 : 15);
    lv_obj_set_width(caption, typing ? 520 : 432);
    lv_obj_remove_style(caption, &s_palette[STYLE_MUTED], 0);
    if (typing) lv_obj_add_style(caption, &s_palette[STYLE_MUTED], 0);
    if (typing) {
      lv_obj_remove_flag(s_settings.fields[i].input, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(s_settings.fields[i].summary, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(s_settings.fields[i].arrow, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(block, LV_OBJ_FLAG_CLICKABLE);
    } else {
      lv_obj_add_flag(s_settings.fields[i].input, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(s_settings.fields[i].summary, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(s_settings.fields[i].arrow, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(block, LV_OBJ_FLAG_CLICKABLE);
      lv_label_set_text(s_settings.fields[i].summary, field_summary(i));
    }
  }
  wac_settings_view_t view = wac_ui_settings_view();
  int x = view == WAC_SETTINGS_CREDENTIALS ? 160 : view == WAC_SETTINGS_DEVICE ? 140 : 150;
  lv_obj_set_pos(s_settings.status, input ? 150 : x, input ? 178 : s_settings.notice_y);
  lv_obj_set_width(s_settings.status, input ? 500 : view == WAC_SETTINGS_CREDENTIALS ? 480 : view == WAC_SETTINGS_DEVICE ? 520 : 500);
  lv_obj_set_style_text_font(s_settings.status, input ? &wac_font_settings_24
    : view == WAC_SETTINGS_WEATHER ? &wac_font_settings_26
    : view == WAC_SETTINGS_CREDENTIALS ? &wac_font_settings_24 : &lv_font_montserrat_18, 0);
  lv_obj_remove_style(s_settings.status, &s_palette[STYLE_MUTED], 0);
  if (!input) lv_obj_add_style(s_settings.status, &s_palette[STYLE_MUTED], 0);
  if (!input && s_settings.normal_note && s_settings.notice[0])
    lv_obj_add_flag(s_settings.normal_note, LV_OBJ_FLAG_HIDDEN);
  if (view == WAC_SETTINGS_WEATHER && s_settings.normal_note) {
    if (input && input == s_settings.fields[FIELD_LOCATION].input)
      lv_obj_remove_flag(s_settings.normal_note, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_settings.normal_note, LV_OBJ_FLAG_HIDDEN);
  }
  refresh_notice();
  if (input) {
    lv_keyboard_set_textarea(s_settings.keyboard, input);
    lv_obj_move_foreground(s_settings.keyboard);
  } else lv_keyboard_set_textarea(s_settings.keyboard, NULL);
}

static void keyboard_event(lv_event_t *event)
{
  (void) event;
  if (s_settings.field_error) {
    s_settings.field_error = false;
    show_notice("");
  }
  typing_layout(NULL);
}

static void keyboard_key_event(lv_event_t *event)
{
  lv_obj_t *keyboard = lv_event_get_current_target(event);
  const char *key = lv_keyboard_get_button_text(keyboard,
    lv_keyboard_get_selected_button(keyboard));
  if (!key) return;
  lv_keyboard_mode_t mode = lv_keyboard_get_mode(keyboard);
  if (strcmp(key, LV_SYMBOL_UP) == 0)
    lv_keyboard_set_mode(keyboard, mode == LV_KEYBOARD_MODE_TEXT_UPPER
      ? LV_KEYBOARD_MODE_TEXT_LOWER : LV_KEYBOARD_MODE_TEXT_UPPER);
  else if (strcmp(key, "ABC") == 0) lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
  else if (strcmp(key, "1#") == 0 || strcmp(key, "1/2") == 0)
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_SPECIAL);
  else if (strcmp(key, "2/2") == 0) lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_USER_1);
  else lv_keyboard_def_event_cb(event);
}

static bool utility_icon(const char *key)
{
  return strcmp(key, LV_SYMBOL_UP) == 0 || strcmp(key, LV_SYMBOL_CLOSE) == 0
    || strcmp(key, LV_SYMBOL_LEFT) == 0 || strcmp(key, LV_SYMBOL_RIGHT) == 0
    || strcmp(key, LV_SYMBOL_BACKSPACE) == 0 || strcmp(key, LV_SYMBOL_OK) == 0;
}

static void icon_lines(lv_layer_t *layer, int x, int y,
  const lv_point_t *points, size_t count)
{
  lv_draw_line_dsc_t line;
  lv_draw_line_dsc_init(&line);
  line.color = primary();
  line.width = 2;
  line.round_start = line.round_end = true;
  for (size_t i = 1; i < count; ++i) {
    line.p1.x = x + points[i - 1].x; line.p1.y = y + points[i - 1].y;
    line.p2.x = x + points[i].x; line.p2.y = y + points[i].y;
    lv_draw_line(layer, &line);
  }
}

static void keyboard_draw(lv_event_t *event)
{
  lv_draw_task_t *task = lv_event_get_draw_task(event);
  lv_draw_dsc_base_t *base = lv_draw_task_get_draw_dsc(task);
  if (base->part != LV_PART_ITEMS) return;
  const char *key = lv_keyboard_get_button_text(s_settings.keyboard, base->id1);
  if (!key || !utility_icon(key)) return;
  lv_draw_label_dsc_t *text = lv_draw_task_get_label_dsc(task);
  if (text) { text->opa = LV_OPA_TRANSP; return; }
  if (!lv_draw_task_get_fill_dsc(task)) return;
  lv_area_t area;
  lv_draw_task_get_area(task, &area);
  int x = (area.x1 + area.x2 + 1 - 24) / 2;
  int y = (area.y1 + area.y2 + 1 - 24) / 2;
  /* Curved Lucide paths use the generated alpha artwork. Straight paths
   * remain simple lines over LVGL's real utility keys. */
  if (strcmp(key, LV_SYMBOL_UP) == 0 || strcmp(key, LV_SYMBOL_BACKSPACE) == 0) {
    lv_draw_image_dsc_t image;
    lv_draw_image_dsc_init(&image);
    image.src = strcmp(key, LV_SYMBOL_UP) == 0
      ? &wac_icon_keyboard_shift : &wac_icon_keyboard_delete;
    image.recolor = primary();
    image.recolor_opa = LV_OPA_COVER;
    lv_area_t icon_area = {x, y, x + 23, y + 23};
    lv_draw_image(base->layer, &image, &icon_area);
    return;
  }
  static const lv_point_t left[] = {{15,18},{9,12},{15,6}};
  static const lv_point_t right[] = {{9,6},{15,12},{9,18}};
  static const lv_point_t check[] = {{20,6},{9,17},{4,12}};
  static const lv_point_t cross_a[] = {{6,6},{18,18}};
  static const lv_point_t cross_b[] = {{18,6},{6,18}};
  if (strcmp(key, LV_SYMBOL_LEFT) == 0) icon_lines(base->layer, x, y, left, 3);
  else if (strcmp(key, LV_SYMBOL_RIGHT) == 0) icon_lines(base->layer, x, y, right, 3);
  else if (strcmp(key, LV_SYMBOL_OK) == 0) icon_lines(base->layer, x, y, check, 3);
  else if (strcmp(key, LV_SYMBOL_CLOSE) == 0) {
    icon_lines(base->layer, x, y, cross_a, 2);
    icon_lines(base->layer, x, y, cross_b, 2);
  }
}

static void field_focus(lv_event_t *event)
{
  if ((s_settings.saving && !s_settings.saving_machine) || s_settings.restarting) return;
  lv_obj_t *input = lv_event_get_user_data(event);
  if (!input) input = lv_event_get_target(event);
  if (lv_obj_has_state(input, LV_STATE_DISABLED)) return;
  if (input) lv_obj_add_state(input, LV_STATE_FOCUSED);
  typing_layout(input);
}

static void field_changed(lv_event_t *event)
{
  if ((s_settings.saving && !s_settings.saving_machine) || s_settings.restarting) return;
  const char *value = lv_textarea_get_text(lv_event_get_target(event));
  char *dest = NULL;
  size_t length = 0;
  switch ((int)(intptr_t)lv_event_get_user_data(event)) {
    case FIELD_ROOM: dest = s_settings.draft.room_name; length = sizeof(s_settings.draft.room_name); break;
    case FIELD_ID: dest = s_settings.draft.device_id; length = sizeof(s_settings.draft.device_id); break;
    case FIELD_SSID: dest = s_settings.draft.wifi_ssid; length = sizeof(s_settings.draft.wifi_ssid); break;
    case FIELD_PASSWORD: dest = s_settings.draft.wifi_password; length = sizeof(s_settings.draft.wifi_password); break;
    case FIELD_API_KEY:
      if (s_settings.draft.weather_provider != DEVICE_CONFIG_WEATHER_VISUAL_CROSSING) return;
      dest = s_settings.draft.weather_api_key; length = sizeof(s_settings.draft.weather_api_key); break;
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
  if (s_settings.field_error && has_text(value)) {
    s_settings.field_error = false;
    show_notice("");
  }
}

static void field(const char *title, const char *value, int field_id, size_t length, int y, bool password)
{
  bool compact = wac_ui_settings_view() == WAC_SETTINGS_DEVICE
    || wac_ui_settings_view() == WAC_SETTINGS_WEATHER;
  bool disabled = field_id == FIELD_API_KEY && s_settings.draft.weather_provider != DEVICE_CONFIG_WEATHER_VISUAL_CROSSING;
  lv_obj_t *block = compact ? lv_button_create(s_settings.body) : lv_obj_create(s_settings.body);
  lv_obj_remove_style_all(block);
  lv_obj_set_pos(block, 140, y);
  lv_obj_set_size(block, 520, 140);
  lv_obj_remove_flag(block, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *caption = label(block, title, 0, 0, 520, &lv_font_montserrat_28);
  lv_obj_add_style(caption, &s_palette[STYLE_MUTED], 0);
  lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_t *obj = lv_textarea_create(block);
  lv_obj_remove_style_all(obj);
  lv_textarea_set_one_line(obj, true);
  /* One-line mode normalizes height; restore the approved box afterward. */
  lv_obj_set_pos(obj, 0, 44);
  lv_obj_set_size(obj, 520, 96);
  lv_textarea_set_max_length(obj, length - 1);
  lv_textarea_set_password_mode(obj, password);
  if (field_id == FIELD_API_KEY) lv_textarea_set_password_show_time(obj, 0);
  lv_textarea_set_text(obj, value);
  lv_textarea_set_placeholder_text(obj, field_id == FIELD_LOCATION ? "City, State"
    : field_id == FIELD_API_KEY ? disabled ? "Not required" : "Required" : title);
  lv_obj_set_style_text_font(obj, &lv_font_montserrat_28, 0);
  lv_obj_add_style(obj, &s_palette[STYLE_BUTTON], 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_radius(obj, 24, 0);
  lv_obj_set_style_pad_hor(obj, 20, 0);
  lv_obj_set_style_pad_ver(obj, (94 - lv_font_get_line_height(&lv_font_montserrat_28)) / 2, 0);
  lv_obj_add_style(obj, &s_palette[STYLE_MUTED], LV_PART_TEXTAREA_PLACEHOLDER);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
  lv_obj_add_style(obj, &s_palette[STYLE_KNOB], LV_PART_CURSOR | LV_STATE_FOCUSED);
  lv_obj_add_event_cb(obj, field_focus, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(obj, field_focus, LV_EVENT_FOCUSED, NULL);
  lv_obj_add_event_cb(obj, field_changed, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)field_id);
  if (field_id == FIELD_API_KEY) {
    lv_obj_t *clear = lv_button_create(block);
    lv_obj_remove_style_all(clear);
    lv_obj_set_pos(clear, 410, 0); lv_obj_set_size(clear, 110, 36);
    lv_obj_add_style(clear, &s_palette[STYLE_BUTTON], 0);
    lv_obj_set_style_bg_opa(clear, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(clear, 1, 0); lv_obj_set_style_radius(clear, 18, 0);
    lv_obj_set_style_pad_all(clear, 0, 0);
    lv_obj_remove_flag(clear, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *caption = label(clear, "Clear", 0, 0, 110, &lv_font_montserrat_22);
    lv_obj_center(caption);
    lv_obj_add_event_cb(clear, action_event, LV_EVENT_CLICKED, (void *)(intptr_t)ACTION_CLEAR_KEY);
    lv_obj_add_flag(clear, LV_OBJ_FLAG_HIDDEN);
    s_settings.fields[field_id].clear = clear;
  }
  if ((s_settings.saving && !s_settings.saving_machine) || s_settings.restarting || disabled) lv_obj_add_state(obj, LV_STATE_DISABLED);
  s_settings.fields[field_id].block = block;
  s_settings.fields[field_id].input = obj;
  s_settings.fields[field_id].y = y;
  if (compact) {
    lv_obj_add_style(block, &s_palette[STYLE_BUTTON], 0);
    lv_obj_set_style_radius(block, 24, 0);
    lv_obj_set_style_opa(block, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_set_style_shadow_width(block, 0, 0);
    lv_obj_add_event_cb(block, field_focus, LV_EVENT_CLICKED, obj);
    lv_obj_remove_flag(caption, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_long_mode(caption, LV_LABEL_LONG_DOT);
    lv_obj_t *summary = label(block, field_summary(field_id), 20, 53, 434, &wac_font_settings_24);
    lv_obj_add_style(summary, &s_palette[STYLE_MUTED], 0);
    lv_obj_set_style_text_align(summary, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_long_mode(summary, LV_LABEL_LONG_DOT);
    lv_obj_remove_flag(summary, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *arrow = image_icon(block, &wac_icon_chevron_right, STYLE_PRIMARY);
    lv_obj_set_pos(arrow, 466, 32);
    s_settings.fields[field_id].summary = summary;
    s_settings.fields[field_id].arrow = arrow;
    if ((s_settings.saving && !s_settings.saving_machine) || s_settings.restarting || disabled) lv_obj_add_state(block, LV_STATE_DISABLED);
  }
}

static void focus_validation(wac_settings_view_t view, int field_id, const char *text)
{
  if (wac_ui_settings_view() != view) navigate(view);
  show_notice(text);
  s_settings.field_error = true;
  typing_layout(s_settings.fields[field_id].input);
}

static void slider_changed(lv_event_t *event)
{
  if (s_settings.saving && !s_settings.saving_machine) return;
  int field_id = (int)(intptr_t)lv_event_get_user_data(event);
  int value = lv_slider_get_value(lv_event_get_target(event));
  if (field_id == FIELD_VOLUME) {
    value *= 5;
    s_settings.draft.alarm_volume = value;
  } else if (field_id == FIELD_DAY_BRIGHTNESS) s_settings.draft.day_brightness = value;
  else s_settings.draft.night_brightness = value;
  char text[8];
  snprintf(text, sizeof(text), "%d%%", value);
  lv_label_set_text(s_settings.percent_values[field_id - FIELD_VOLUME], text);
}

static void slider(const char *title, int field_id, int value, int y)
{
  bool volume = field_id == FIELD_VOLUME;
  label(s_settings.body, title, 150, y, volume ? 270 : 350, &lv_font_montserrat_32);
  char text[8];
  snprintf(text, sizeof(text), "%d%%", value);
  s_settings.percent_values[field_id - FIELD_VOLUME] =
    label(s_settings.body, text, volume ? 498 : 510, y, volume ? 150 : 140, &lv_font_montserrat_32);
  lv_obj_t *obj = lv_slider_create(s_settings.body);
  lv_obj_remove_style_all(obj);
  /* Center the thin rail in the same 48px-high touch band. */
  lv_obj_set_pos(obj, 175, y + (volume ? 80 : 88));
  lv_obj_set_size(obj, 450, 8);
  lv_obj_set_ext_click_area(obj, 20);
  lv_obj_add_style(obj, &s_palette[STYLE_BUTTON], LV_PART_MAIN);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_add_style(obj, &s_palette[STYLE_RANGE], LV_PART_INDICATOR);
  lv_obj_add_style(obj, &s_palette[STYLE_RANGE], LV_PART_KNOB);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_KNOB);
  lv_obj_set_style_pad_all(obj, 14, LV_PART_KNOB);
  lv_obj_set_style_shadow_width(obj, 0, LV_PART_KNOB);
  lv_slider_set_range(obj, volume ? 2 : DEVICE_CONFIG_MIN_BRIGHTNESS, volume ? 20 : 100);
  int initial = volume ? (value + 2) / 5 : value;
  if (volume && initial < 2) initial = 2;
  if (volume && initial > 20) initial = 20;
  /* Configure the illustrated control before attaching its callback. Opening
   * it leaves a valid saved volume unchanged until the user adjusts it. */
  lv_slider_set_value(obj, initial, LV_ANIM_OFF);
  lv_obj_add_event_cb(obj, slider_changed, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)field_id);
  if (s_settings.saving && !s_settings.saving_machine) lv_obj_add_state(obj, LV_STATE_DISABLED);
}

static void machine_slider_changed(lv_event_t *event)
{
  unsigned value = lv_slider_get_value(lv_event_get_target(event));
  s_settings.machine_desired.volume = value;
  char text[8]; snprintf(text, sizeof(text), "%u%%", value);
  lv_label_set_text(s_settings.machine_volume, text);
  submit_save(SAVE_MACHINE);
}
static void machine_scroll_event(lv_event_t *event)
{
  (void)event;
  if (!s_settings.machine_list || !s_settings.machine_thumb) return;
  s_settings.machine_scroll = lv_obj_get_scroll_y(s_settings.machine_list);
  int top = s_settings.machine_scroll * (420 - 137) / (1284 - 420);
  if (top < 0) top = 0;
  if (top > 283) top = 283;
  lv_obj_set_y(s_settings.machine_thumb, top);
}
static void machine_bar_event(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_PRESSED && lv_event_get_code(event) != LV_EVENT_PRESSING) return;
  lv_indev_t *indev = lv_indev_active();
  if (!indev || !s_settings.machine_list) return;
  lv_point_t point; lv_indev_get_point(indev, &point);
  int top = point.y - 212 - 137 / 2;
  if (top < 0) top = 0;
  if (top > 283) top = 283;
  lv_obj_scroll_to_y(s_settings.machine_list, top * 864 / 283, LV_ANIM_OFF);
}

static void time_picker(int hour, int minute, const device_config_t *draft)
{
  bool twelve = strcmp(draft->time_format, "12") == 0;
  lv_obj_t *caption = label(s_settings.body, "Hour", 170, 196, 170, &lv_font_montserrat_28);
  lv_obj_add_style(caption, &s_palette[STYLE_MUTED], 0);
  caption = label(s_settings.body, "Minute", 460, 196, 170, &lv_font_montserrat_28);
  lv_obj_add_style(caption, &s_palette[STYLE_MUTED], 0);
  arrow_button(ACTION_HOUR_UP, 200, 240, &wac_icon_chevron_up);
  arrow_button(ACTION_HOUR_DOWN, 200, 472, &wac_icon_chevron_down);
  arrow_button(ACTION_MINUTE_UP, 470, 240, &wac_icon_chevron_up);
  arrow_button(ACTION_MINUTE_DOWN, 470, 472, &wac_icon_chevron_down);
  char text[8];
  snprintf(text, sizeof(text), twelve ? "%d" : "%02d", twelve ? (hour % 12 ? hour % 12 : 12) : hour);
  label(s_settings.body, text, 180, 355, 170, &wac_font_settings_104);
  snprintf(text, sizeof(text), "%02d", minute);
  label(s_settings.body, text, 450, 355, 170, &wac_font_settings_104);
  label(s_settings.body, ":", 365, 357, 70, &wac_font_settings_104);
  if (twelve) {
    button("AM", ACTION_AM, 270, 588, 120, 72, hour < 12);
    button("PM", ACTION_PM, 410, 588, 120, 72, hour >= 12);
  }
  button("Done", ACTION_DONE, 270, 682, 260, 88, true);
}

static void change_time(int action)
{
  wac_settings_view_t view = wac_ui_settings_view();
  device_config_t *draft = &s_settings.draft;
  int hour = draft->alarm_hour, minute = draft->alarm_minute;
  char *dest = NULL;
  if (view != WAC_SETTINGS_ALARM_TIME) {
    dest = view == WAC_SETTINGS_NIGHT_START ? draft->night_shift_start : draft->night_shift_end;
    sscanf(dest, "%d:%d", &hour, &minute);
  }
  bool twelve = strcmp(draft->time_format, "12") == 0;
  if (action == ACTION_HOUR_UP || action == ACTION_HOUR_DOWN) {
    int step = action == ACTION_HOUR_UP ? 1 : -1;
    hour = twelve ? (hour % 12 + step + 12) % 12 + (hour >= 12 ? 12 : 0)
      : (hour + step + 24) % 24;
  } else if (action == ACTION_MINUTE_UP || action == ACTION_MINUTE_DOWN) {
    int total = (hour * 60 + minute + (action == ACTION_MINUTE_UP ? 5 : -5) + 1440) % 1440;
    hour = total / 60;
    minute = total % 60;
  } else hour = hour % 12 + (action == ACTION_PM ? 12 : 0);
  if (dest) snprintf(dest, DEVICE_CONFIG_TIME_STR_LEN, "%02d:%02d", hour, minute);
  else { draft->alarm_hour = hour; draft->alarm_minute = minute; }
}

static void offset_label(char *text, size_t size, int value, bool sunset)
{
  const char *event = sunset ? "sunset" : "sunrise";
  if (!value) snprintf(text, size, "At %s", event);
  else snprintf(text, size, "%d min %s %s", abs(value), value < 0 ? "before" : "after", event);
}

static void refresh_solar(void)
{
  if (!s_settings.solar_note) return;
  const device_config_t *draft = &s_settings.draft;
  struct tm start, end;
  time_t sunset = s_settings.solar.sunset + draft->night_sunset_offset_minutes * 60;
  time_t sunrise = s_settings.solar.sunrise + draft->night_sunrise_offset_minutes * 60;
  bool available = s_settings.solar.available && s_settings.applied.location_ready
    && localtime_r(&sunset, &start) != NULL && localtime_r(&sunrise, &end) != NULL;
  char text[24];
  for (int i = 0; i < 2; ++i) {
    if (available) {
      const struct tm *local = i == 0 ? &start : &end;
      format_time(text, sizeof(text), local->tm_hour, local->tm_min, draft);
    } else snprintf(text, sizeof(text), "--:--");
    lv_label_set_text(s_settings.solar_times[i], text);
  }
  lv_label_set_text(s_settings.solar_note, available
    ? "Starts tonight · Ends tomorrow" : "Sunrise/sunset times unavailable");
  lv_obj_set_y(s_settings.solar_note, available ? 514 : 518);
  lv_obj_set_style_text_font(s_settings.solar_note,
    available ? &wac_font_settings_24 : &lv_font_montserrat_28, 0);
  lv_obj_remove_style(s_settings.solar_note, &s_palette[STYLE_PRIMARY], 0);
  lv_obj_remove_style(s_settings.solar_note, &s_palette[STYLE_MUTED], 0);
  lv_obj_add_style(s_settings.solar_note, &s_palette[available ? STYLE_MUTED : STYLE_PRIMARY], 0);
  lv_label_set_text(s_settings.solar_description, !available
    ? "Keeps the current appearance and\nbrightness until times are available."
    : draft->night_shift_mode == DEVICE_CONFIG_NIGHT_SOLAR
      ? "Updates each day for this location."
      : "Choose Sunset to Sunrise in Mode\nto use this schedule.");
  lv_obj_set_pos(s_settings.solar_description, available ? 150 : 140, 566);
  lv_obj_set_width(s_settings.solar_description, available ? 500 : 520);
  lv_obj_set_style_text_font(s_settings.solar_description,
    available ? &lv_font_montserrat_26 : &lv_font_montserrat_24, 0);
}

void wac_settings_update_solar(const solar_times_t *events)
{
  if (events == NULL) return;
  bool changed = events->available != s_settings.solar.available
    || events->sunrise != s_settings.solar.sunrise || events->sunset != s_settings.solar.sunset;
  s_settings.solar = *events;
  if (changed) refresh_solar();
}

static void offset_picker(int value, bool sunset)
{
  lv_obj_t *caption = label(s_settings.body, sunset ? "Night starts" : "Night ends", 150, 190, 500, &lv_font_montserrat_28);
  lv_obj_add_style(caption, &s_palette[STYLE_MUTED], 0);
  button("Before", ACTION_OFFSET_BEFORE, 164, 244, 148, 88, value < 0);
  button("At", ACTION_OFFSET_AT, 326, 244, 148, 88, value == 0);
  button("After", ACTION_OFFSET_AFTER, 488, 244, 148, 88, value > 0);
  lv_obj_t *minus = button_box(ACTION_OFFSET_MINUS, 140, 370, 110, 104, false);
  lv_obj_center(image_icon(minus, &wac_icon_minus, STYLE_PRIMARY));
  if (!value) lv_obj_add_state(minus, LV_STATE_DISABLED);
  lv_obj_t *plus = button_box(ACTION_OFFSET_PLUS, 550, 370, 110, 104, false);
  lv_obj_center(image_icon(plus, &wac_icon_plus, STYLE_PRIMARY));
  if (abs(value) == 120) lv_obj_add_state(plus, LV_STATE_DISABLED);
  char text[64];
  snprintf(text, sizeof(text), "%d", abs(value));
  label(s_settings.body, text, 270, 366, 260, &wac_font_settings_96);
  caption = label(s_settings.body, "minutes", 270, 474, 260, &lv_font_montserrat_28);
  lv_obj_add_style(caption, &s_palette[STYLE_MUTED], 0);
  offset_label(text, sizeof(text), value, sunset);
  caption = label(s_settings.body, text, 150, 560, 500, &lv_font_montserrat_28);
  lv_obj_add_style(caption, &s_palette[STYLE_MUTED], 0);
  button("Done", ACTION_DONE, 270, 664, 260, 80, true);
}

static void restart_timer(lv_timer_t *timer)
{
  (void) timer;
  esp_restart();
}

static bool needs_location_lookup(const device_config_t *draft)
{
  return !draft->latitude[0] || !draft->longitude[0] || !draft->timezone[0];
}

static void save_task(void *arg)
{
  save_request_t *request = arg;
  esp_err_t error;
  bool lookup_failed = false;
  bool provider_failed = false;
  if (request->kind == SAVE_WEATHER) {
    bool lookup = needs_location_lookup(&request->draft);
    if (lookup) {
      location_lookup_result_t result = {0};
      error = connectivity_is_wifi_connected() ? location_lookup_resolve(request->draft.location, &result) : ESP_ERR_WIFI_NOT_CONNECT;
      if (error != ESP_OK) { lookup_failed = true; goto complete; }
      snprintf(request->draft.location, sizeof(request->draft.location), "%s", result.location);
      snprintf(request->draft.latitude, sizeof(request->draft.latitude), "%s", result.latitude);
      snprintf(request->draft.longitude, sizeof(request->draft.longitude), "%s", result.longitude);
      snprintf(request->draft.timezone, sizeof(request->draft.timezone), "%s", result.timezone);
    }
    bool validate_provider = request->draft.weather_provider == DEVICE_CONFIG_WEATHER_VISUAL_CROSSING
      && (request->baseline.weather_provider != DEVICE_CONFIG_WEATHER_VISUAL_CROSSING
          || strcmp(request->baseline.weather_api_key, request->draft.weather_api_key) != 0);
    if (validate_provider) {
      app_weather_snapshot_t *checked = calloc(1, sizeof(*checked));
      error = checked ? (connectivity_is_wifi_connected()
        ? weather_client_fetch(&request->draft, checked) : ESP_ERR_WIFI_NOT_CONNECT) : ESP_ERR_NO_MEM;
      free(checked);
      if (error != ESP_OK) { provider_failed = true; goto complete; }
    }
    error = device_config_save_weather(&request->baseline, &request->draft);
  } else if (request->kind == SAVE_DEVICE) error = device_config_save_device(&request->baseline, &request->draft);
  else if (request->kind == SAVE_DISPLAY) error = device_config_save_display(&request->baseline, &request->draft);
  else if (request->kind == SAVE_ALARM) error = device_config_save_alarm(&request->baseline, &request->draft);
  else if (request->kind == SAVE_MACHINE) error = device_config_save_sound_machine(&request->baseline, &request->draft);
  else error = device_config_set_alarm_enabled(request->draft.alarm_enabled);
complete:
  portENTER_CRITICAL(&s_completion_lock);
  s_save_completion.kind = request->kind;
  s_save_completion.route = request->route;
  s_save_completion.draft = request->draft;
  s_save_completion.lookup_failed = lookup_failed;
  s_save_completion.provider_failed = provider_failed;
  s_save_completion.error = error;
  s_save_completion.ready = true;
  portEXIT_CRITICAL(&s_completion_lock);
  free(request);
  vTaskDelete(NULL);
}

static void submit_save(save_kind_t kind)
{
  if (s_settings.saving || s_settings.restarting) {
    if (kind == SAVE_MACHINE && s_settings.saving_machine) s_settings.machine_changed_while_saving = true;
    return;
  }
  s_settings.machine_changed_while_saving = false;
  if (kind == SAVE_WEATHER && s_settings.draft.weather_provider == DEVICE_CONFIG_WEATHER_VISUAL_CROSSING
      && !has_text(s_settings.draft.weather_api_key)) {
    focus_validation(WAC_SETTINGS_WEATHER, FIELD_API_KEY, "Enter a Visual Crossing API key.");
    return;
  }
  if (kind == SAVE_DEVICE && wac_ui_settings_view() == WAC_SETTINGS_CREDENTIALS
      && !has_text(s_settings.draft.wifi_ssid)) {
    focus_validation(WAC_SETTINGS_CREDENTIALS, FIELD_SSID, "Enter a network name before saving.");
    return;
  }
  if (kind == SAVE_DEVICE && (!has_text(s_settings.draft.room_name) || !has_text(s_settings.draft.device_id))) {
    bool room_missing = !has_text(s_settings.draft.room_name);
    focus_validation(WAC_SETTINGS_DEVICE, room_missing ? FIELD_ROOM : FIELD_ID,
      room_missing ? "Enter a room name before saving." : "Enter a device ID before saving.");
    return;
  }
  if (kind == SAVE_DEVICE && strcmp(s_settings.draft.message_sharing, "shared") == 0
      && strcmp(s_settings.draft.device_id, "clock-esp32-p4") == 0) {
    show_notice("Shared clocks need a distinct Device ID. Open Messaging."); return;
  }
  save_request_t *request = calloc(1, sizeof(*request));
  if (!request) {
    if (kind == SAVE_MACHINE) s_settings.machine_save_failed = true;
    show_notice("Could not start saving. Try again.");
    s_settings.notice_kind = NOTICE_SAVE_ERROR;
    refresh_notice();
    return;
  }
  request->kind = kind;
  request->route = s_settings.route;
  if (kind != SAVE_TOGGLE) { request->baseline = s_settings.baseline; request->draft = s_settings.draft; }
  else { request->baseline = s_settings.applied; request->draft = s_settings.applied; request->draft.alarm_enabled = !request->draft.alarm_enabled; }
  if (kind == SAVE_MACHINE) {
    request->baseline = request->draft = s_settings.applied;
    apply_machine_desired(&request->draft);
    s_settings.machine_save_failed = false;
  }
  if (kind == SAVE_WEATHER && s_settings.holding_device_draft) request->baseline = s_settings.applied;
  if (kind == SAVE_WEATHER && !has_text(request->draft.location)) {
    free(request);
    focus_validation(WAC_SETTINGS_WEATHER, FIELD_LOCATION, "Enter a city and state before saving.");
    return;
  }
  s_settings.saving = true;
  s_settings.saving_machine = kind == SAVE_MACHINE;
  bool lookup = kind == SAVE_WEATHER && needs_location_lookup(&request->draft);
  show_notice(lookup ? "Checking Location…" : kind == SAVE_WEATHER ? "Saving settings…" : "Saving settings...");
  if (lookup) s_settings.notice_kind = NOTICE_LOCATION_CHECKING;
  else if (kind == SAVE_WEATHER && request->draft.weather_provider == DEVICE_CONFIG_WEATHER_VISUAL_CROSSING
      && (request->baseline.weather_provider != DEVICE_CONFIG_WEATHER_VISUAL_CROSSING
          || strcmp(request->baseline.weather_api_key, request->draft.weather_api_key) != 0))
    s_settings.notice_kind = NOTICE_PROVIDER_CHECKING;
  if (xTaskCreate(save_task, "settings_save", 8192, request, 4, NULL) != pdPASS) {
    s_settings.saving = false;
    s_settings.saving_machine = false;
    if (kind == SAVE_MACHINE) s_settings.machine_save_failed = true;
    free(request); show_notice("Could not start saving. Try again.");
    s_settings.notice_kind = NOTICE_SAVE_ERROR;
  }
  if (kind != SAVE_MACHINE) wac_settings_render(wac_ui_settings_view());
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
  s_settings.scan_error = ESP_OK;
  show_notice("");
  if (xTaskCreate(scan_task, "settings_scan", 4096, (void *)(uintptr_t)s_settings.route, 4, NULL) != pdPASS) {
    s_settings.scanning = false;
    s_settings.scan_error = ESP_FAIL;
  }
  wac_settings_render(wac_ui_settings_view());
}

void wac_settings_start_machine(bool return_to_clock)
{
  s_settings.machine_return = return_to_clock ? WAC_SETTINGS_NONE : wac_ui_settings_view();
  navigate(WAC_SETTINGS_MACHINE);
}

void wac_settings_start_weather(const device_config_t *config)
{
  s_settings.holding_device_draft = false;
  s_settings.baseline = *config;
  s_settings.draft = *config;
}

void wac_settings_begin(lv_obj_t *parent, const device_config_t *config)
{
  wac_settings_apply_night(s_night);
  s_settings.preview_submission_error = ESP_OK;
  s_settings.machine_submission_error = ESP_OK;
  s_settings.machine_scroll = 0;
  s_settings.machine_return = WAC_SETTINGS_ENTRY;
  s_settings.applied = *config;
  s_settings.alarm_snapshot_valid = false;
  if (!s_settings.saving_machine && !s_settings.machine_save_failed) {
    s_settings.machine_desired.sound = config->sound_machine_sound;
    s_settings.machine_desired.volume = config->sound_machine_volume;
    s_settings.machine_desired.timer = config->sound_machine_timer_minutes;
  }
  s_settings.holding_device_draft = false;
  s_settings.baseline = s_settings.draft = *config;
  ++s_settings.route;
  s_settings.notice[0] = '\0';
  s_settings.notice_kind = NOTICE_GENERAL;
  s_settings.field_error = false;
  s_settings.network_note = "Enter the network name and password.";
  s_settings.root = lv_obj_create(parent);
  lv_obj_set_size(s_settings.root, 800, 800);
  lv_obj_set_pos(s_settings.root, 0, 0);
  lv_obj_set_style_bg_color(s_settings.root, lv_color_hex(s_night ? 0x18090c : 0x07101c), 0);
  lv_obj_set_style_bg_opa(s_settings.root, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_settings.root, 0, 0);
  lv_obj_set_style_pad_all(s_settings.root, 0, 0);
  lv_obj_remove_flag(s_settings.root, LV_OBJ_FLAG_SCROLLABLE);
}

void wac_settings_close(void)
{
  s_settings.holding_device_draft = false;
  stop_preview();
  ++s_settings.route;
  if (s_settings.root) lv_obj_delete(s_settings.root);
  s_settings.root = s_settings.body = s_settings.status = s_settings.keyboard = NULL;
  s_settings.backdrop = s_settings.wifi_summary = NULL;
  s_settings.preview_button = s_settings.preview_caption = NULL;
  s_settings.machine_button = s_settings.machine_status = s_settings.machine_volume = NULL;
  s_settings.machine_list = s_settings.machine_bar = s_settings.machine_thumb = s_settings.machine_retry = NULL;
  s_settings.heading = s_settings.back_button = s_settings.normal_note = s_settings.active_input = NULL;
  s_settings.save_button = s_settings.toggle_button = NULL;
  s_settings.solar_note = s_settings.solar_description = NULL;
  memset(s_settings.solar_times, 0, sizeof(s_settings.solar_times));
  memset(s_settings.fields, 0, sizeof(s_settings.fields));
}

/* Hide the existing editor as a unit. Its draft, active field and keyboard
 * remain owned here while the alarm temporarily takes the whole screen. */
void wac_settings_set_visible(bool visible)
{
  if (!s_settings.root) return;
  if (!visible) stop_preview();
  if (visible) lv_obj_remove_flag(s_settings.root, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_settings.root, LV_OBJ_FLAG_HIDDEN);
}

static void wifi_summary(char *text, size_t size)
{
  const char *status = "Not Connected";
  if (s_settings.draft.wifi_ssid[0]
      && strcmp(s_settings.draft.wifi_ssid, s_settings.applied.wifi_ssid) == 0) {
    switch (connectivity_get_wifi_state()) {
      case CONNECTIVITY_WIFI_CONNECTED: status = "Connected"; break;
      case CONNECTIVITY_WIFI_CONNECTING: status = "Connecting"; break;
      case CONNECTIVITY_WIFI_RETRYING: status = "Retrying"; break;
      default: break;
    }
  }
  if (s_settings.draft.wifi_ssid[0])
    snprintf(text, size, "%s · %s", s_settings.draft.wifi_ssid, status);
  else snprintf(text, size, "Not Connected");
}

static void repeat_summary(char *text, size_t size, uint8_t days, bool compact)
{
  if (days == 0x7f) snprintf(text, size, "Every Day");
  else if (days == 0x3e) snprintf(text, size, "Mon–Fri");
  else if (days == 0x41) snprintf(text, size, "Sat–Sun");
  else if (!days) snprintf(text, size, "None");
  else {
    static const char *names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    unsigned count = 0;
    text[0] = '\0';
    for (int i = 0; i < 7; ++i) {
      if (!(days & (1 << i))) continue;
      size_t used = strlen(text);
      snprintf(text + used, size - used, "%s%s", count ? ", " : "", names[i]);
      ++count;
    }
    if (compact && strlen(text) > 14) snprintf(text, size, "%u days", count);
  }
}

static void navigate(wac_settings_view_t view)
{
  stop_preview();
  s_settings.preview_submission_error = ESP_OK;
  ++s_settings.route;
  s_settings.notice[0] = '\0';
  s_settings.notice_kind = NOTICE_GENERAL;
  s_settings.field_error = false;
  wac_ui_settings_navigate(view);
}

static void back(void)
{
  switch (wac_ui_settings_view()) {
    case WAC_SETTINGS_ENTRY: wac_ui_settings_close(); break;
    case WAC_SETTINGS_MACHINE:
      if (s_settings.machine_return == WAC_SETTINGS_NONE) wac_ui_settings_close();
      else navigate(s_settings.machine_return);
      break;
    case WAC_SETTINGS_MACHINE_SOUND: case WAC_SETTINGS_MACHINE_TIMER: navigate(WAC_SETTINGS_MACHINE); break;
    case WAC_SETTINGS_DEVICE_SETUP:
      s_settings.draft = s_settings.baseline;
      navigate(WAC_SETTINGS_ENTRY); break;
    case WAC_SETTINGS_WEATHER:
      s_settings.draft = s_settings.holding_device_draft
        ? s_settings.held_device_draft : s_settings.baseline;
      s_settings.holding_device_draft = false;
      navigate(WAC_SETTINGS_DEVICE_SETUP); break;
    case WAC_SETTINGS_DISPLAY:
      s_settings.draft = s_settings.baseline;
      navigate(WAC_SETTINGS_ENTRY); break;
    case WAC_SETTINGS_ALARM:
      s_settings.draft = s_settings.baseline;
      navigate(WAC_SETTINGS_ENTRY); break;
    case WAC_SETTINGS_DEVICE: case WAC_SETTINGS_WIFI: case WAC_SETTINGS_NIGHT:
      navigate(WAC_SETTINGS_DEVICE_SETUP); break;
    case WAC_SETTINGS_CREDENTIALS: navigate(WAC_SETTINGS_WIFI); break;
    case WAC_SETTINGS_ALARM_TIME: case WAC_SETTINGS_REPEAT: case WAC_SETTINGS_SOUND:
      navigate(WAC_SETTINGS_ALARM); break;
    case WAC_SETTINGS_REPEAT_CUSTOM: navigate(WAC_SETTINGS_REPEAT); break;
    case WAC_SETTINGS_SOUND_PICKER: case WAC_SETTINGS_SNOOZE:
      navigate(WAC_SETTINGS_SOUND); break;
    case WAC_SETTINGS_NIGHT_START: case WAC_SETTINGS_NIGHT_END:
      navigate(WAC_SETTINGS_NIGHT_FIXED); break;
    case WAC_SETTINGS_SUNSET_OFFSET: case WAC_SETTINGS_SUNRISE_OFFSET:
      navigate(WAC_SETTINGS_NIGHT_SOLAR); break;
    case WAC_SETTINGS_BRIGHTNESS: navigate(WAC_SETTINGS_DISPLAY); break;
    default: navigate(WAC_SETTINGS_NIGHT); break;
  }
}

static void action_event(lv_event_t *event)
{
  int action = (int)(intptr_t)lv_event_get_user_data(event);
  if (s_settings.restarting || (s_settings.saving && !s_settings.saving_machine)) return;
  if (action == ACTION_MACHINE_PLAY) {
    wac_audio_copy(&s_settings.audio);
    s_settings.machine_submission_error = (s_settings.audio.machine_active || s_settings.audio.machine_pending)
      ? wac_audio_sound_machine_stop() : wac_audio_sound_machine_play();
    wac_audio_copy(&s_settings.audio);
    refresh_machine(); return;
  }
  if (action == ACTION_MACHINE_RETRY) { submit_save(SAVE_MACHINE); return; }
  if (action >= ACTION_MACHINE_SOUND_BASE && action < ACTION_MACHINE_SOUND_BASE + 12) {
    s_settings.machine_desired.sound = action - ACTION_MACHINE_SOUND_BASE;
    submit_save(SAVE_MACHINE); wac_settings_render(wac_ui_settings_view()); return;
  }
  if (action >= ACTION_MACHINE_TIMER_BASE && action < ACTION_MACHINE_TIMER_BASE + 5) {
    s_settings.machine_desired.timer = machine_timers[action - ACTION_MACHINE_TIMER_BASE];
    submit_save(SAVE_MACHINE); wac_settings_render(wac_ui_settings_view()); return;
  }
  if (action == ACTION_PREVIEW) {
    wac_audio_copy(&s_settings.audio);
    if (!s_settings.audio.ready && !s_settings.audio.preview_active) return;
    if (s_settings.audio.preview_active) stop_preview();
    else {
      s_settings.preview_submission_error = wac_audio_preview(
        s_settings.draft.alarm_sound, s_settings.draft.alarm_volume);
      wac_audio_copy(&s_settings.audio);
      refresh_audio();
    }
    return;
  }
  stop_preview();
  s_settings.preview_submission_error = ESP_OK;
  if (action > WAC_SETTINGS_NONE && action <= WAC_SETTINGS_MACHINE_TIMER) {
    if (action == WAC_SETTINGS_MACHINE) { wac_settings_start_machine(false); return; }
    if (action == WAC_SETTINGS_WEATHER && wac_ui_settings_view() == WAC_SETTINGS_DEVICE_SETUP) {
      s_settings.held_device_draft = s_settings.draft;
      s_settings.holding_device_draft = true;
      s_settings.draft = s_settings.applied;
    } else if (action == WAC_SETTINGS_DEVICE_SETUP || action == WAC_SETTINGS_DISPLAY
        || action == WAC_SETTINGS_WEATHER || action == WAC_SETTINGS_ALARM) {
      s_settings.holding_device_draft = false;
      s_settings.baseline = s_settings.draft = s_settings.applied;
    }
    navigate(action);
    if (action == WAC_SETTINGS_WIFI) scan_networks();
    return;
  }
  if (action == ACTION_BACK || action == ACTION_DONE || action == ACTION_CANCEL) { back(); return; }
  if (action == ACTION_SAVE) {
    submit_save(wac_ui_settings_view() == WAC_SETTINGS_WEATHER ? SAVE_WEATHER : wac_ui_settings_view() == WAC_SETTINGS_ALARM ? SAVE_ALARM : wac_ui_settings_view() == WAC_SETTINGS_DISPLAY ? SAVE_DISPLAY : SAVE_DEVICE);
    return;
  }
  if (action == ACTION_ALARM_TOGGLE) { submit_save(SAVE_TOGGLE); return; }
  if (action == ACTION_CLEAR_KEY) {
    lv_textarea_set_text(s_settings.fields[FIELD_API_KEY].input, "");
    return;
  }
  if (action == ACTION_SCAN) { scan_networks(); return; }
  if (action == ACTION_MANUAL) {
    s_settings.network_note = "Enter the network name and password.";
    navigate(WAC_SETTINGS_CREDENTIALS); return;
  }
  if (action >= ACTION_NETWORK_BASE && action < ACTION_NETWORK_BASE + (int)s_settings.network_count) {
    connectivity_scan_result_t *network = &s_settings.networks[action - ACTION_NETWORK_BASE];
    if (strcmp(network->ssid, s_settings.draft.wifi_ssid) != 0) s_settings.draft.wifi_password[0] = '\0';
    snprintf(s_settings.draft.wifi_ssid, sizeof(s_settings.draft.wifi_ssid), "%s", network->ssid);
    s_settings.network_note = network->requires_password
      ? "Enter the password to join this network." : "Open network. No password is required.";
    navigate(WAC_SETTINGS_CREDENTIALS); return;
  }
  device_config_t *draft = &s_settings.draft;
  if (action >= ACTION_HOUR_UP && action <= ACTION_PM) change_time(action);
  else if (action == ACTION_REPEAT_NONE) draft->alarm_repeat_days = 0;
  else if (action == ACTION_WEEKDAYS) draft->alarm_repeat_days = 0x3e;
  else if (action == ACTION_EVERY_DAY) draft->alarm_repeat_days = 0x7f;
  else if (action >= ACTION_OFFSET_BEFORE && action <= ACTION_OFFSET_PLUS) {
    int value = wac_ui_settings_view() == WAC_SETTINGS_SUNSET_OFFSET
      ? draft->night_sunset_offset_minutes : draft->night_sunrise_offset_minutes;
    int amount = abs(value);
    if (action == ACTION_OFFSET_AT) value = 0;
    else if (action == ACTION_OFFSET_BEFORE || action == ACTION_OFFSET_AFTER)
      value = (action == ACTION_OFFSET_BEFORE ? -1 : 1) * (amount ? amount : 15);
    else {
      amount += action == ACTION_OFFSET_PLUS ? 5 : -5;
      if (amount < 0) amount = 0;
      if (amount > 120) amount = 120;
      value = (value < 0 ? -1 : 1) * amount;
    }
    if (wac_ui_settings_view() == WAC_SETTINGS_SUNSET_OFFSET) draft->night_sunset_offset_minutes = value;
    else draft->night_sunrise_offset_minutes = value;
  }
  else if (action == ACTION_FACE) snprintf(draft->default_clock_face, sizeof(draft->default_clock_face), "%s", strcmp(draft->default_clock_face, "analog") == 0 ? "digital" : "analog");
  else if (action == ACTION_FORMAT) snprintf(draft->time_format, sizeof(draft->time_format), "%s", strcmp(draft->time_format, "24") == 0 ? "12" : "24");
  else if (action == ACTION_ZERO && strcmp(draft->time_format, "24") != 0)
    draft->leading_zero_12h = !draft->leading_zero_12h;
  else if (action == ACTION_UNITS) snprintf(s_settings.draft.units, sizeof(s_settings.draft.units), "%s", strcmp(s_settings.draft.units, "metric") == 0 ? "imperial" : "metric");
  else if (action == ACTION_PROVIDER) {
    draft->weather_provider = draft->weather_provider == DEVICE_CONFIG_WEATHER_VISUAL_CROSSING
      ? DEVICE_CONFIG_WEATHER_OPEN_METEO : DEVICE_CONFIG_WEATHER_VISUAL_CROSSING;
    show_notice("");
  }
  else if (action == ACTION_SHARING) snprintf(draft->message_sharing, sizeof(draft->message_sharing), "%s", strcmp(draft->message_sharing, "shared") == 0 ? "single" : "shared");
  else if (action >= ACTION_DAY_BASE && action < ACTION_DAY_BASE + 7) s_settings.draft.alarm_repeat_days ^= 1 << (action - ACTION_DAY_BASE);
  else if (action >= ACTION_SOUND_BASE && action < ACTION_SOUND_BASE + 3) s_settings.draft.alarm_sound = action - ACTION_SOUND_BASE;
  else if (action >= ACTION_SNOOZE_BASE && action < ACTION_SNOOZE_BASE + 3) s_settings.draft.alarm_snooze_minutes = 5 * (action - ACTION_SNOOZE_BASE + 1);
  else if (action >= ACTION_MODE_BASE && action < ACTION_MODE_BASE + 4) draft->night_shift_mode = action - ACTION_MODE_BASE;
  wac_settings_render(wac_ui_settings_view());
}

void wac_settings_poll(void)
{
  if (s_settings.wifi_summary) {
    char text[160];
    wifi_summary(text, sizeof(text));
    if (strcmp(lv_label_get_text(s_settings.wifi_summary), text) != 0)
      lv_label_set_text(s_settings.wifi_summary, text);
  }
  save_completion_t save;
  scan_completion_t scan;
  portENTER_CRITICAL(&s_completion_lock);
  save = s_save_completion;
  scan = s_scan_completion;
  s_save_completion.ready = s_scan_completion.ready = false;
  portEXIT_CRITICAL(&s_completion_lock);
  if (save.ready) {
    s_settings.saving = false;
    bool saved_editor = s_settings.root && save.route == s_settings.route;
    if (save.error == ESP_OK) {
      device_config_copy(&s_settings.applied);
      if (save.kind == SAVE_DISPLAY) wac_ui_apply_display(&s_settings.applied);
    }
    if (save.kind == SAVE_MACHINE) {
      s_settings.saving_machine = false;
      if (save.error == ESP_OK) {
        copy_machine_preferences(&s_settings.baseline, &s_settings.applied);
        /* A drag or another choice may have changed the desired preferences
         * while the ordinary background save was in flight. */
        if (s_settings.machine_changed_while_saving && !machine_desired_equals(&save.draft)) submit_save(SAVE_MACHINE);
        else {
          copy_machine_preferences(&s_settings.draft, &s_settings.applied);
          if (machine_view(wac_ui_settings_view())) show_notice("");
        }
      } else {
        s_settings.machine_save_failed = true;
        if (machine_view(wac_ui_settings_view())) {
          show_notice("Could not save. Retry your choice.");
          s_settings.notice_kind = NOTICE_SAVE_ERROR;
        }
      }
      if (!s_settings.saving && !s_settings.restarting) {
        if (s_settings.save_button) lv_obj_remove_state(s_settings.save_button, LV_STATE_DISABLED);
        if (s_settings.toggle_button) lv_obj_remove_state(s_settings.toggle_button, LV_STATE_DISABLED);
      }
      wac_settings_view_t current_view = wac_ui_settings_view();
      if (s_settings.root && (machine_view(current_view) || current_view == WAC_SETTINGS_ENTRY)
          && (save.error != ESP_OK || current_view != WAC_SETTINGS_MACHINE || s_settings.machine_retry))
        wac_settings_render(wac_ui_settings_view());
    } else if (save.error == ESP_OK && (save.kind == SAVE_DEVICE || save.kind == SAVE_WEATHER)) {
      /* Save & Restart completes even if alarm navigation replaced its editor.
       * Only the original editor may show location/network-specific success. */
      s_settings.restarting = true;
      show_notice("Settings Saved. Restarting...");
      s_settings.notice_kind = saved_editor ? NOTICE_RESTART_SAVED_EDITOR : NOTICE_GENERAL;
      lv_timer_t *timer = lv_timer_create(restart_timer, 900, NULL);
      if (timer) lv_timer_set_repeat_count(timer, 1);
      else esp_restart();
      if (s_settings.root) wac_settings_render(wac_ui_settings_view());
    } else if (saved_editor) {
      if (save.error == ESP_OK) {
        if (save.kind == SAVE_ALARM || save.kind == SAVE_DISPLAY) {
          s_settings.baseline = s_settings.draft = s_settings.applied;
          navigate(WAC_SETTINGS_ENTRY);
          show_notice(save.kind == SAVE_DISPLAY ? "Display settings saved." : "Alarm settings saved.");
        } else {
          /* On/Off is already applied. Cancel and a later full Save must keep
           * it while the other alarm edits remain in this editor's draft. */
          s_settings.baseline.alarm_enabled = s_settings.applied.alarm_enabled;
          s_settings.draft.alarm_enabled = s_settings.applied.alarm_enabled;
          show_notice("Alarm setting saved.");
        }
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
        if (save.kind == SAVE_WEATHER && save.error == ESP_ERR_NOT_FOUND)
          s_settings.notice_kind = NOTICE_LOCATION_NOT_FOUND;
        else if (save.kind == SAVE_WEATHER && save.lookup_failed
            && save.error != ESP_ERR_WIFI_NOT_CONNECT)
          s_settings.notice_kind = NOTICE_LOCATION_UNAVAILABLE;
        else if (save.kind == SAVE_WEATHER && save.lookup_failed)
          s_settings.notice_kind = NOTICE_LOCATION_OFFLINE;
        else s_settings.notice_kind = save.provider_failed ? NOTICE_PROVIDER_ERROR : NOTICE_SAVE_ERROR;
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
      s_settings.scan_error = scan.error;
      show_notice("");
      wac_settings_render(WAC_SETTINGS_WIFI);
    } else if (s_settings.root && wac_ui_settings_view() == WAC_SETTINGS_WIFI) {
      wac_settings_render(WAC_SETTINGS_WIFI);
    }
  }
}

void wac_settings_render(wac_settings_view_t view)
{
  if (!s_settings.root || view == WAC_SETTINGS_NONE) return;
  if (view != WAC_SETTINGS_SOUND_PICKER) stop_preview();
  lv_obj_clean(s_settings.root);
  s_settings.status = s_settings.keyboard = s_settings.wifi_summary = NULL;
  s_settings.preview_button = s_settings.preview_caption = NULL;
  s_settings.machine_button = s_settings.machine_status = s_settings.machine_volume = NULL;
  s_settings.machine_list = s_settings.machine_bar = s_settings.machine_thumb = s_settings.machine_retry = NULL;
  s_settings.normal_note = s_settings.active_input = NULL;
  s_settings.save_button = s_settings.toggle_button = NULL;
  s_settings.solar_note = s_settings.solar_description = NULL;
  memset(s_settings.solar_times, 0, sizeof(s_settings.solar_times));
  memset(s_settings.fields, 0, sizeof(s_settings.fields));
  memset(s_settings.percent_values, 0, sizeof(s_settings.percent_values));
  s_settings.body = s_settings.root;
  s_settings.backdrop = lv_image_create(s_settings.root);
  lv_image_set_src(s_settings.backdrop, s_night ? &round_stage_night : &round_stage_day);
  lv_obj_remove_flag(s_settings.backdrop, LV_OBJ_FLAG_CLICKABLE);
  const char *titles[] = {"", "Settings", "Device Setup", "Display", "Messaging", "Weather", "Wi-Fi", "Network Details", "Alarm", "Alarm Time", "Repeat", "Custom Days", "Sound & Snooze", "Alarm Sound", "Snooze", "Night Shift", "Night Shift Mode", "Fixed Schedule", "Night Starts", "Night Ends", "Sunrise/Sunset", "Sunset Offset", "Sunrise Offset", "Brightness", "Sound Machine", "Sound", "Sleep Timer"};
  s_settings.heading = label(s_settings.body, titles[view], 245, 128, 320, &lv_font_montserrat_36);
  lv_obj_set_style_text_letter_space(s_settings.heading, 1, 0);
  s_settings.back_button = button_box(ACTION_BACK, 145, 104, 88, 88, false);
  chevron(s_settings.back_button, true);
  if (s_settings.restarting && s_settings.notice_kind == NOTICE_RESTART_SAVED_EDITOR
      && view == WAC_SETTINGS_WEATHER) {
    const device_config_t *saved = &s_settings.applied;
    bool location_changed = !s_settings.baseline.location_ready
      || strcmp(saved->location, s_settings.baseline.location) != 0;
    status_mark(&wac_icon_circle_check_mark);
    label(s_settings.body, location_changed ? "Location Saved" : "Settings Saved", 140, 368, 520, &lv_font_montserrat_36);
    lv_obj_t *location = label(s_settings.body, saved->location, 140, 432, 520, &lv_font_montserrat_28);
    lv_label_set_long_mode(location, LV_LABEL_LONG_DOT);
    char zone[160];
    snprintf(zone, sizeof(zone), "Time Zone · %s", saved->timezone);
    lv_obj_t *zone_label = label(s_settings.body, zone, 150, 486, 500, &wac_font_settings_26);
    lv_obj_add_style(zone_label, &s_palette[STYLE_MUTED], 0);
    lv_obj_t *detail = label(s_settings.body, "Restarting for local weather…", 150, 554, 500, &wac_font_settings_26);
    lv_obj_add_style(detail, &s_palette[STYLE_MUTED], 0);
    return;
  }
  if (s_settings.restarting && s_settings.notice_kind == NOTICE_RESTART_SAVED_EDITOR
      && view == WAC_SETTINGS_CREDENTIALS) {
    lv_label_set_text(s_settings.heading, "Wi-Fi");
    lv_obj_t *network = label(s_settings.body, s_settings.applied.wifi_ssid, 150, 202, 500, &lv_font_montserrat_28);
    lv_obj_add_style(network, &s_palette[STYLE_MUTED], 0);
    lv_label_set_long_mode(network, LV_LABEL_LONG_DOT);
    status_mark(&wac_icon_circle_check_mark);
    label(s_settings.body, "Settings Saved", 140, 392, 520, &lv_font_montserrat_36);
    lv_obj_t *detail = label(s_settings.body, "Restarting to join Wi-Fi…", 150, 456, 500, &wac_font_settings_26);
    lv_obj_add_style(detail, &s_palette[STYLE_MUTED], 0);
    return;
  }
  if (s_settings.restarting || (s_settings.saving && !s_settings.saving_machine && view != WAC_SETTINGS_WEATHER && view != WAC_SETTINGS_ALARM)) {
    s_settings.status = label(s_settings.root, s_settings.restarting
      ? "Settings Saved. Restarting..." : s_settings.notice, 155, 368, 490, &wac_font_settings_24);
    return;
  }
  char text[160];
  device_config_t *d = &s_settings.draft;
  static const char *modes[] = {"Off", "Always On", "Fixed Schedule", "Sunset to Sunrise"};
  static const char *sounds[] = {"Classic Beep", "Classic Bell", "Rising Tones"};
  switch (view) {
    case WAC_SETTINGS_ENTRY: {
      entry_row("Sound Machine", machine_sounds[s_settings.applied.sound_machine_sound], WAC_SETTINGS_MACHINE, 212);
      char time[32], repeat[48];
      format_time(time, sizeof(time), s_settings.applied.alarm_hour,
        s_settings.applied.alarm_minute, &s_settings.applied);
      repeat_summary(repeat, sizeof(repeat), s_settings.applied.alarm_repeat_days, true);
      snprintf(text, sizeof(text), "%s · %s", time, repeat);
      entry_row("Alarm Settings", text, WAC_SETTINGS_ALARM, 320);
      snprintf(text, sizeof(text), "%s · %s-Hour", strcmp(s_settings.applied.default_clock_face, "analog") == 0 ? "Analog" : "Digital", s_settings.applied.time_format);
      entry_row("Display Settings", text, WAC_SETTINGS_DISPLAY, 428);
      entry_row("Device Setup", NULL, WAC_SETTINGS_DEVICE_SETUP, 536);
      button("Back to Clock", ACTION_BACK, 230, 646, 340, 88, false);
      break;
    }
    case WAC_SETTINGS_MACHINE: {
      value_entry("Sound", machine_sounds[s_settings.machine_desired.sound], WAC_SETTINGS_MACHINE_SOUND, 212, 96);
      if (s_settings.machine_desired.timer) snprintf(text, sizeof(text), "%u Minutes", s_settings.machine_desired.timer);
      else snprintf(text, sizeof(text), "Continuous");
      value_entry("Sleep Timer", text, WAC_SETTINGS_MACHINE_TIMER, 320, 96);
      label(s_settings.body, "Volume", 150, 452, 270, &lv_font_montserrat_32);
      snprintf(text, sizeof(text), "%u%%", s_settings.machine_desired.volume);
      s_settings.machine_volume = label(s_settings.body, text, 498, 452, 150, &lv_font_montserrat_32);
      lv_obj_t *range = lv_slider_create(s_settings.body);
      lv_obj_remove_style_all(range);
      lv_obj_set_pos(range, 175, 532); lv_obj_set_size(range, 450, 8);
      lv_obj_set_ext_click_area(range, 20);
      lv_obj_add_style(range, &s_palette[STYLE_BUTTON], LV_PART_MAIN);
      lv_obj_add_style(range, &s_palette[STYLE_RANGE], LV_PART_INDICATOR);
      lv_obj_add_style(range, &s_palette[STYLE_RANGE], LV_PART_KNOB);
      for (int part = 0; part < 3; ++part) {
        lv_part_t target = part == 0 ? LV_PART_MAIN : part == 1 ? LV_PART_INDICATOR : LV_PART_KNOB;
        lv_obj_set_style_bg_opa(range, LV_OPA_COVER, target);
        lv_obj_set_style_radius(range, LV_RADIUS_CIRCLE, target);
      }
      lv_obj_set_style_pad_all(range, 14, LV_PART_KNOB);
      lv_slider_set_range(range, 0, 100);
      lv_slider_set_value(range, s_settings.machine_desired.volume, LV_ANIM_OFF);
      lv_obj_add_event_cb(range, machine_slider_changed, LV_EVENT_VALUE_CHANGED, NULL);
      s_settings.machine_status = label(s_settings.body, "Stopped", 150, 598, 500, &wac_font_settings_26);
      lv_obj_add_style(s_settings.machine_status, &s_palette[STYLE_MUTED], 0);
      s_settings.machine_button = icon_button("Play", ACTION_MACHINE_PLAY, 270, 676, 260, 80, &wac_icon_play, true, true);
      refresh_machine();
      break;
    }
    case WAC_SETTINGS_MACHINE_SOUND: {
      snprintf(text, sizeof(text), "Volume · %u%%", s_settings.machine_desired.volume);
      lv_obj_t *caption = label(s_settings.body, text, 150, 174, 500, &wac_font_settings_28);
      lv_obj_add_style(caption, &s_palette[STYLE_MUTED], 0);
      if (s_settings.machine_save_failed) lv_obj_add_flag(caption, LV_OBJ_FLAG_HIDDEN);
      s_settings.machine_list = lv_obj_create(s_settings.body);
      lv_obj_remove_style_all(s_settings.machine_list);
      lv_obj_set_pos(s_settings.machine_list, 140, 212); lv_obj_set_size(s_settings.machine_list, 520, 420);
      lv_obj_set_scroll_dir(s_settings.machine_list, LV_DIR_VER);
      lv_obj_set_scrollbar_mode(s_settings.machine_list, LV_SCROLLBAR_MODE_OFF);
      s_settings.body = s_settings.machine_list;
      for (int i = 0; i < 12; ++i) {
        bool selected = s_settings.machine_desired.sound == (device_config_sound_machine_sound_t)i;
        lv_obj_t *obj = entry_box(machine_sounds[i], machine_descriptions[i], ACTION_MACHINE_SOUND_BASE + i, i * 108, selected);
        lv_obj_set_x(obj, 0);
        if (selected) check_icon(obj);
      }
      s_settings.body = s_settings.root;
      s_settings.machine_bar = lv_obj_create(s_settings.body);
      lv_obj_remove_style_all(s_settings.machine_bar);
      lv_obj_set_pos(s_settings.machine_bar, 672, 212); lv_obj_set_size(s_settings.machine_bar, 10, 420);
      lv_obj_set_style_radius(s_settings.machine_bar, LV_RADIUS_CIRCLE, 0);
      lv_obj_add_style(s_settings.machine_bar, &s_palette[STYLE_SCROLL_TRACK], 0);
      lv_obj_set_style_bg_opa(s_settings.machine_bar, 97, 0);
      lv_obj_add_flag(s_settings.machine_bar, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(s_settings.machine_bar, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_ext_click_area(s_settings.machine_bar, 12);
      lv_obj_add_event_cb(s_settings.machine_bar, machine_bar_event, LV_EVENT_ALL, NULL);
      s_settings.machine_thumb = lv_obj_create(s_settings.machine_bar);
      lv_obj_remove_style_all(s_settings.machine_thumb);
      lv_obj_set_size(s_settings.machine_thumb, 10, 137);
      lv_obj_set_style_radius(s_settings.machine_thumb, LV_RADIUS_CIRCLE, 0);
      lv_obj_add_style(s_settings.machine_thumb, &s_palette[STYLE_SCROLL_THUMB], 0);
      lv_obj_set_style_bg_opa(s_settings.machine_thumb, LV_OPA_COVER, 0);
      lv_obj_remove_flag(s_settings.machine_thumb, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_event_cb(s_settings.machine_list, machine_scroll_event, LV_EVENT_SCROLL, NULL);
      lv_obj_update_layout(s_settings.machine_list);
      lv_obj_scroll_to_y(s_settings.machine_list, s_settings.machine_scroll, LV_ANIM_OFF);
      machine_scroll_event(NULL);
      button("Done", ACTION_DONE, 270, 676, 260, 80, true);
      break;
    }
    case WAC_SETTINGS_MACHINE_TIMER:
      for (int i = 0; i < 5; ++i) {
        if (machine_timers[i]) snprintf(text, sizeof(text), "%u Minutes", machine_timers[i]);
        else snprintf(text, sizeof(text), "Continuous");
        bool selected = s_settings.machine_desired.timer == machine_timers[i];
        if (selected) icon_button(text, ACTION_MACHINE_TIMER_BASE + i, 170, 212 + i * 92, 460, 80, &wac_icon_check, false, true);
        else button(text, ACTION_MACHINE_TIMER_BASE + i, 170, 212 + i * 92, 460, 80, false);
      }
      button("Done", ACTION_DONE, 270, 676, 260, 80, true);
      break;
    case WAC_SETTINGS_DEVICE_SETUP:
      wifi_summary(text, sizeof(text));
      entry_row("Wi-Fi", text, WAC_SETTINGS_WIFI, 212);
      entry_row("Weather", s_settings.applied.location, WAC_SETTINGS_WEATHER, 320);
      entry_row("Night Shift", modes[d->night_shift_mode], WAC_SETTINGS_NIGHT, 428);
      entry_row("Messaging", d->room_name, WAC_SETTINGS_DEVICE, 536);
      button("Cancel", ACTION_CANCEL, 174, 646, 162, 88, false);
      button("Save & Restart", ACTION_SAVE, 358, 646, 268, 88, true);
      break;
    case WAC_SETTINGS_DISPLAY: {
      bool time_24h = strcmp(d->time_format, "24") == 0;
      value_row("Home Screen", strcmp(d->default_clock_face, "analog") == 0 ? "Analog" : "Digital", ACTION_FACE, 212);
      snprintf(text, sizeof(text), "Day %u%% · Night %u%%", d->day_brightness, d->night_brightness);
      entry_row("Brightness", text, WAC_SETTINGS_BRIGHTNESS, 320);
      value_row("Time Format", strcmp(d->time_format, "24") == 0 ? "24-Hour" : "12-Hour", ACTION_FORMAT, 428);
      lv_obj_t *leading_zero = switch_row("Leading Zero", time_24h || d->leading_zero_12h, ACTION_ZERO, 536);
      if (time_24h) lv_obj_add_state(leading_zero, LV_STATE_DISABLED);
      button("Cancel", ACTION_CANCEL, 178, 646, 206, 88, false);
      button("Save", ACTION_SAVE, 412, 646, 210, 88, true); break;
    }
    case WAC_SETTINGS_DEVICE:
      field("Room Name", d->room_name, FIELD_ROOM, sizeof(d->room_name), 212, false);
      field("Device ID", d->device_id, FIELD_ID, sizeof(d->device_id), 320, false);
      value_row("House Messages", strcmp(d->message_sharing, "shared") == 0 ? "Shared" : "Single", ACTION_SHARING, 428);
      s_settings.normal_note = label(s_settings.body, "Each shared clock needs a distinct Device ID.", 140, 555, 520, &wac_font_settings_26);
      button("Done", ACTION_DONE, 270, 664, 260, 80, true); break;
    case WAC_SETTINGS_WEATHER:
      field("Weather Location", d->location, FIELD_LOCATION, sizeof(d->location), 212, false);
      value_row("Provider", (s_settings.draft.weather_provider == DEVICE_CONFIG_WEATHER_VISUAL_CROSSING) ? "Visual Crossing" : "Open-Meteo", ACTION_PROVIDER, 320);
      value_row("Units", strcmp(d->units, "metric") == 0 ? "Metric" : "Imperial", ACTION_UNITS, 428);
      field("API Key", s_settings.draft.weather_api_key, FIELD_API_KEY, sizeof(s_settings.draft.weather_api_key), 536, true);
      s_settings.normal_note = label(s_settings.body, "Time zone is set automatically from this location.", 140, 359, 520, &lv_font_montserrat_22);
      /* Keep the nearest existing font on one line in the illustrated band. */
      lv_obj_set_style_text_letter_space(s_settings.normal_note, -1, 0);
      button("Cancel", ACTION_CANCEL, 178, 646, 206, 88, false);
      lv_obj_t *save_button = button(s_settings.saving && s_settings.notice_kind == NOTICE_LOCATION_CHECKING
        ? "Checking…" : "Save", ACTION_SAVE, 412, 646, 210, 88, true);
      lv_obj_set_style_text_font(lv_obj_get_child(save_button, 0), &wac_font_settings_28, 0);
      break;
    case WAC_SETTINGS_WIFI: {
      if (!s_settings.scanning && s_settings.scan_error == ESP_OK && s_settings.network_count) {
        lv_obj_t *subtitle = label(s_settings.body, "Choose a Network", 150, 175, 500, &lv_font_montserrat_28);
        lv_obj_add_style(subtitle, &s_palette[STYLE_MUTED], 0);
        lv_obj_t *list = lv_obj_create(s_settings.body);
        lv_obj_remove_style_all(list);
        lv_obj_set_pos(list, 140, 220);
        lv_obj_set_size(list, 520, 312);
        lv_obj_set_scroll_dir(list, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
        s_settings.body = list;
        for (size_t i = 0; i < s_settings.network_count; ++i) network_row(&s_settings.networks[i], i);
        s_settings.body = s_settings.root;
      } else {
        status_mark(s_settings.scanning ? &wac_icon_wifi_mark : &wac_icon_wifi_off_mark);
        const char *title = s_settings.scanning ? "Scanning for Networks…"
          : s_settings.scan_error != ESP_OK ? "Couldn’t Scan for Networks" : "No Networks Found";
        label(s_settings.body, title, 140, 380, 520, &wac_font_settings_34);
        lv_obj_t *detail = label(s_settings.body, s_settings.scanning
          ? "Looking for nearby Wi-Fi networks." : "Refresh the scan or enter\na network manually.",
          150, 444, 500, &wac_font_settings_26);
        lv_obj_add_style(detail, &s_palette[STYLE_MUTED], 0);
      }
      button("Manual Network", ACTION_MANUAL, 140, 544, 520, 96, false);
      refresh_button();
      return;
    }
    case WAC_SETTINGS_CREDENTIALS:
      field("Network Name", d->wifi_ssid, FIELD_SSID, sizeof(d->wifi_ssid), 204, false);
      field("Password", d->wifi_password, FIELD_PASSWORD, sizeof(d->wifi_password), 372, true);
      s_settings.normal_note = label(s_settings.body, s_settings.network_note
        ? s_settings.network_note : "Enter the network name and password.", 160, 548, 480, &wac_font_settings_24);
      button("Back", ACTION_BACK, 187, 646, 136, 88, false);
      button("Save & Restart", ACTION_SAVE, 345, 646, 268, 88, true); break;
    case WAC_SETTINGS_ALARM:
      s_settings.status = label(s_settings.body, "", 180, 169, 440, &wac_font_settings_28);
      lv_obj_add_style(s_settings.status, &s_palette[STYLE_MUTED], 0);
      refresh_notice();
      switch_row("Alarm", d->alarm_enabled, ACTION_ALARM_TOGGLE, 212);
      format_time(text, sizeof(text), d->alarm_hour, d->alarm_minute, d);
      value_entry("Alarm Time", text, WAC_SETTINGS_ALARM_TIME, 320, 96);
      repeat_summary(text, sizeof(text), d->alarm_repeat_days, true);
      value_entry("Repeat", text, WAC_SETTINGS_REPEAT, 428, 96);
      entry_row("Sound & Snooze", NULL, WAC_SETTINGS_SOUND, 536);
      button("Cancel", ACTION_CANCEL, 178, 644, 206, 88, false);
      button("Save", ACTION_SAVE, 412, 644, 210, 88, true);
      return;
    case WAC_SETTINGS_ALARM_TIME: time_picker(d->alarm_hour, d->alarm_minute, d); return;
    case WAC_SETTINGS_REPEAT: {
      static const char *presets[] = {"None", "Every Day", "Weekdays", "Custom Days"};
      static const int actions[] = {ACTION_REPEAT_NONE, ACTION_EVERY_DAY, ACTION_WEEKDAYS, WAC_SETTINGS_REPEAT_CUSTOM};
      static const uint8_t masks[] = {0, 0x7f, 0x3e};
      for (unsigned i = 0; i < 4; ++i) {
        bool selected = i < 3 ? d->alarm_repeat_days == masks[i]
          : d->alarm_repeat_days && d->alarm_repeat_days != 0x7f && d->alarm_repeat_days != 0x3e;
        lv_obj_t *obj = entry_box(presets[i], NULL, actions[i], 212 + i * 108, selected);
        if (i == 3) {
          chevron(obj, false);
          lv_obj_add_style(lv_obj_get_child(obj, -1), &s_palette[STYLE_ACCENT], 0);
        }
        if (selected)
          lv_obj_align(image_icon(obj, &wac_icon_check, STYLE_ACCENT), LV_ALIGN_RIGHT_MID, i == 3 ? -40 : 0, 0);
      }
      if (d->alarm_repeat_days) repeat_summary(text, sizeof(text), d->alarm_repeat_days, true);
      else snprintf(text, sizeof(text), "Rings once");
      lv_obj_t *caption = label(s_settings.body, text, 155, 646, 490, &wac_font_settings_28);
      lv_obj_add_style(caption, &s_palette[STYLE_MUTED], 0);
      button("Done", ACTION_DONE, 270, 682, 260, 88, true);
      return;
    }
    case WAC_SETTINGS_REPEAT_CUSTOM: {
      lv_obj_t *caption = label(s_settings.body, "Choose alarm days", 150, 174, 500, &wac_font_settings_28);
      lv_obj_add_style(caption, &s_palette[STYLE_MUTED], 0);
      static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
      /* Round both mockup half-pixel rows up; their gap stays unchanged. */
      static const int layout[][3] = {{1,86,296},{2,214,296},{3,342,296},{4,470,296},{5,598,296},{6,278,409},{0,406,409}};
      for (size_t i = 0; i < sizeof(layout) / sizeof(layout[0]); ++i) {
        int day = layout[i][0];
        bool selected = d->alarm_repeat_days & (1 << day);
        lv_obj_t *obj = button_box(ACTION_DAY_BASE + day, layout[i][1], layout[i][2], 116, 96, selected);
        lv_obj_t *title = label(obj, days[day], 0, 0, 74, &lv_font_montserrat_28);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, selected ? -18 : 0);
        if (selected) {
          image_icon(obj, &wac_icon_check, STYLE_PRIMARY);
          lv_obj_align(lv_obj_get_child(obj, -1), LV_ALIGN_CENTER, 0, 17);
        }
      }
      if (d->alarm_repeat_days) repeat_summary(text, sizeof(text), d->alarm_repeat_days, false);
      else snprintf(text, sizeof(text), "Rings once");
      caption = label(s_settings.body, text, 155, 542, 490, &wac_font_settings_28);
      lv_obj_add_style(caption, &s_palette[STYLE_MUTED], 0);
      button("Done", ACTION_DONE, 270, 682, 260, 88, true);
      return;
    }
    case WAC_SETTINGS_SOUND: {
      value_entry("Alarm Sound", sounds[d->alarm_sound], WAC_SETTINGS_SOUND_PICKER, 212, 96);
      snprintf(text, sizeof(text), "%u Minutes", d->alarm_snooze_minutes);
      value_entry("Snooze", text, WAC_SETTINGS_SNOOZE, 320, 96);
      slider("Volume", FIELD_VOLUME, d->alarm_volume, 452);
      lv_obj_t *hint = label(s_settings.body, "Sound rises gently to this level", 150, 598, 500, &wac_font_settings_26);
      lv_obj_add_style(hint, &s_palette[STYLE_MUTED], 0);
      button("Done", ACTION_DONE, 270, 676, 260, 80, true);
      return;
    }
    case WAC_SETTINGS_SOUND_PICKER: {
      snprintf(text, sizeof(text), "Volume · %u%%", d->alarm_volume);
      s_settings.preview_caption = label(s_settings.body, text, 150, 174, 500, &wac_font_settings_28);
      lv_obj_add_style(s_settings.preview_caption, &s_palette[STYLE_MUTED], 0);
      static const char *descriptions[] = {"A clear, repeating beep.", "Fast, alternating bell strikes.", "A sequence of ascending tones."};
      for (int i = 0; i < 3; ++i) {
        bool selected = d->alarm_sound == (device_config_alarm_sound_t)i;
        lv_obj_t *obj = entry_box(sounds[i], descriptions[i], ACTION_SOUND_BASE + i, 212 + i * 108, selected);
        if (selected) check_icon(obj);
      }
      s_settings.preview_button = icon_button("Preview Sound", ACTION_PREVIEW, 140, 552, 520, 96,
        &wac_icon_volume_2, true, false);
      /* Lucide's outlined square uses the existing 32px icon slot. */
      lv_obj_t *slot = lv_obj_create(s_settings.preview_button);
      lv_obj_remove_style_all(slot);
      lv_obj_set_size(slot, 32, 32);
      lv_obj_remove_flag(slot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_t *square = lv_obj_create(slot);
      lv_obj_remove_style_all(square);
      lv_obj_set_size(square, 26, 26);
      lv_obj_set_pos(square, 3, 3);
      lv_obj_set_style_border_width(square, 3, 0);
      lv_obj_set_style_radius(square, 4, 0);
      lv_obj_add_style(square, &s_palette[STYLE_PRIMARY], 0);
      lv_obj_remove_flag(square, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
      refresh_audio();
      button("Done", ACTION_DONE, 270, 676, 260, 80, true);
      return;
    }
    case WAC_SETTINGS_SNOOZE:
      for (int i = 0; i < 3; ++i) {
        int minutes = 5 * (i + 1);
        snprintf(text, sizeof(text), "%d Minutes", minutes);
        if (d->alarm_snooze_minutes == minutes)
          icon_button(text, ACTION_SNOOZE_BASE + i, 170, 244 + i * 122, 460, 104, &wac_icon_check, false, true);
        else button(text, ACTION_SNOOZE_BASE + i, 170, 244 + i * 122, 460, 104, false);
      }
      button("Done", ACTION_DONE, 270, 662, 260, 96, true);
      return;
    case WAC_SETTINGS_NIGHT: {
      entry_row("Mode", modes[d->night_shift_mode], WAC_SETTINGS_NIGHT_MODE, 212);
      char start[24], end[24];
      format_fixed_time(start, sizeof(start), d->night_shift_start, d);
      format_fixed_time(end, sizeof(end), d->night_shift_end, d);
      snprintf(text, sizeof(text), "%s – %s", start, end);
      entry_row("Fixed Schedule", text, WAC_SETTINGS_NIGHT_FIXED, 320);
      snprintf(text, sizeof(text), "Sunset %s%d min · Sunrise %s%d min",
        d->night_sunset_offset_minutes > 0 ? "+" : "", d->night_sunset_offset_minutes,
        d->night_sunrise_offset_minutes > 0 ? "+" : "", d->night_sunrise_offset_minutes);
      entry_row("Sunrise/Sunset Offsets", text, WAC_SETTINGS_NIGHT_SOLAR, 428);
      button("Done", ACTION_DONE, 270, 664, 260, 80, true);
      return;
    }
    case WAC_SETTINGS_NIGHT_MODE: {
      static const char *descriptions[] = {"Use the day appearance", "Use the night appearance", "Switch at your set times", "Follow your weather location"};
      for (int i = 0; i < 4; ++i) {
        bool selected = d->night_shift_mode == (device_config_night_mode_t)i;
        lv_obj_t *obj = entry_box(modes[i], descriptions[i], ACTION_MODE_BASE + i, 212 + i * 108, selected);
        if (selected) check_icon(obj);
      }
      button("Done", ACTION_DONE, 270, 664, 260, 80, true);
      return;
    }
    case WAC_SETTINGS_NIGHT_FIXED: {
      lv_obj_t *caption = label(s_settings.body, "Every Day", 150, 175, 500, &lv_font_montserrat_28);
      lv_obj_add_style(caption, &s_palette[STYLE_MUTED], 0);
      format_fixed_time(text, sizeof(text), d->night_shift_start, d);
      value_entry("Night Starts", text, WAC_SETTINGS_NIGHT_START, 244, 104);
      format_fixed_time(text, sizeof(text), d->night_shift_end, d);
      value_entry("Night Ends", text, WAC_SETTINGS_NIGHT_END, 364, 104);
      const char *note = strcmp(d->night_shift_start, d->night_shift_end) == 0
        ? "Night Shift stays on all day." : strcmp(d->night_shift_end, d->night_shift_start) < 0
        ? "Ends the next day." : "Starts and ends on the same day.";
      caption = label(s_settings.body, note, 160, 510, 480, &lv_font_montserrat_28);
      lv_obj_add_style(caption, &s_palette[STYLE_MUTED], 0);
      caption = label(s_settings.body, d->night_shift_mode == DEVICE_CONFIG_NIGHT_FIXED
        ? "Fixed Schedule is selected." : "Choose Fixed Schedule in Mode to use these times.",
        150, 564, 500, &lv_font_montserrat_28);
      lv_obj_add_style(caption, &s_palette[STYLE_MUTED], 0);
      button("Done", ACTION_DONE, 270, 664, 260, 80, true);
      return;
    }
    case WAC_SETTINGS_NIGHT_START: case WAC_SETTINGS_NIGHT_END: {
      int hour = 0, minute = 0;
      sscanf(view == WAC_SETTINGS_NIGHT_START ? d->night_shift_start : d->night_shift_end, "%d:%d", &hour, &minute);
      time_picker(hour, minute, d); return;
    }
    case WAC_SETTINGS_NIGHT_SOLAR: {
      lv_obj_t *caption = label(s_settings.body, "Weather Location", 150, 174, 500, &lv_font_montserrat_24);
      lv_obj_add_style(caption, &s_palette[STYLE_MUTED], 0);
      lv_obj_t *location = label(s_settings.body, s_settings.applied.location_ready && has_text(s_settings.applied.location)
        ? s_settings.applied.location : "Not available", 150, 210, 500, &lv_font_montserrat_28);
      lv_label_set_long_mode(location, LV_LABEL_LONG_DOT);
      lv_obj_set_style_max_height(location, 2 * lv_font_get_line_height(&lv_font_montserrat_28), 0);
      for (int i = 0; i < 2; ++i) {
        offset_label(text, sizeof(text), i == 0 ? d->night_sunset_offset_minutes : d->night_sunrise_offset_minutes, i == 0);
        lv_obj_t *obj = entry_box(i == 0 ? "Night Starts" : "Night Ends", text,
          i == 0 ? WAC_SETTINGS_SUNSET_OFFSET : WAC_SETTINGS_SUNRISE_OFFSET, 280 + i * 108, false);
        lv_obj_t *time = label(obj, "--:--", 0, 0, LV_SIZE_CONTENT, &lv_font_montserrat_28);
        s_settings.solar_times[i] = time;
        lv_obj_add_style(time, &s_palette[STYLE_ACCENT], 0);
        lv_obj_align(time, LV_ALIGN_RIGHT_MID, -40, 0);
        chevron(obj, false);
        lv_obj_add_style(lv_obj_get_child(obj, -1), &s_palette[STYLE_ACCENT], 0);
      }
      s_settings.solar_note = label(s_settings.body, "", 150, 518, 500, &lv_font_montserrat_28);
      s_settings.solar_description = label(s_settings.body, "", 140, 566, 520, &lv_font_montserrat_24);
      lv_obj_add_style(s_settings.solar_description, &s_palette[STYLE_MUTED], 0);
      refresh_solar();
      button("Done", ACTION_DONE, 270, 664, 260, 80, true);
      return;
    }
    case WAC_SETTINGS_SUNSET_OFFSET: case WAC_SETTINGS_SUNRISE_OFFSET:
      offset_picker(view == WAC_SETTINGS_SUNSET_OFFSET ? d->night_sunset_offset_minutes : d->night_sunrise_offset_minutes,
        view == WAC_SETTINGS_SUNSET_OFFSET);
      return;
    case WAC_SETTINGS_BRIGHTNESS: {
      slider("Day Brightness", FIELD_DAY_BRIGHTNESS, d->day_brightness, 224);
      slider("Night Brightness", FIELD_NIGHT_BRIGHTNESS, d->night_brightness, 412);
      lv_obj_t *caption = label(s_settings.body, "Uses the brightness for the active\nday or night mode.", 150, 570, 500, &lv_font_montserrat_28);
      lv_obj_add_style(caption, &s_palette[STYLE_MUTED], 0);
      button("Done", ACTION_DONE, 270, 664, 260, 80, true);
      return;
    }
    default: break;
  }
  if (machine_view(view) && s_settings.machine_save_failed) {
    lv_obj_t *retry = s_settings.machine_retry = button("Retry Save", ACTION_MACHINE_RETRY, 245, 176, 310, 28, false);
    lv_obj_set_style_text_font(lv_obj_get_child(retry, 0), &lv_font_montserrat_18, 0);
    return;
  }
  int notice_y = machine_view(view) ? 642 : (view == WAC_SETTINGS_DEVICE_SETUP || view == WAC_SETTINGS_ENTRY) ? 170
    : (view == WAC_SETTINGS_WEATHER || view == WAC_SETTINGS_CREDENTIALS) ? 548
    : view == WAC_SETTINGS_DEVICE ? 574 : 620;
  s_settings.notice_y = notice_y;
  s_settings.status = label(s_settings.root, s_settings.notice, 155, notice_y, 490, &lv_font_montserrat_18);
  lv_obj_add_style(s_settings.status, &s_palette[STYLE_MUTED], 0);
  if (s_settings.normal_note) {
    lv_obj_add_style(s_settings.normal_note, &s_palette[STYLE_MUTED], 0);
    if (s_settings.notice[0]) lv_obj_add_flag(s_settings.normal_note, LV_OBJ_FLAG_HIDDEN);
  }
  if (view != WAC_SETTINGS_DEVICE && view != WAC_SETTINGS_WEATHER
      && view != WAC_SETTINGS_CREDENTIALS) return;
  s_settings.keyboard = lv_keyboard_create(s_settings.root);
  lv_obj_remove_style_all(s_settings.keyboard);
  lv_obj_remove_event_cb(s_settings.keyboard, lv_keyboard_def_event_cb);
  lv_keyboard_set_map(s_settings.keyboard, LV_KEYBOARD_MODE_TEXT_LOWER,
                      SETTINGS_KEYBOARD_LOWER_MAP, SETTINGS_KEYBOARD_LETTER_CTRL);
  lv_keyboard_set_map(s_settings.keyboard, LV_KEYBOARD_MODE_TEXT_UPPER,
                      SETTINGS_KEYBOARD_UPPER_MAP, SETTINGS_KEYBOARD_LETTER_CTRL);
  lv_keyboard_set_map(s_settings.keyboard, LV_KEYBOARD_MODE_SPECIAL,
                      SETTINGS_KEYBOARD_NUMBERS_MAP, SETTINGS_KEYBOARD_LETTER_CTRL);
  lv_keyboard_set_map(s_settings.keyboard, LV_KEYBOARD_MODE_USER_1,
                      SETTINGS_KEYBOARD_SYMBOLS_MAP, SETTINGS_KEYBOARD_SYMBOLS_CTRL);
  lv_keyboard_set_popovers(s_settings.keyboard, true);
  lv_obj_set_size(s_settings.keyboard, 560, 280);
  lv_obj_set_pos(s_settings.keyboard, 120, 390);
  lv_obj_set_style_pad_all(s_settings.keyboard, 0, 0);
  lv_obj_set_style_pad_row(s_settings.keyboard, 8, 0);
  lv_obj_set_style_pad_column(s_settings.keyboard, 6, 0);
  lv_obj_add_style(s_settings.keyboard, &s_palette[STYLE_BUTTON], LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(s_settings.keyboard, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_border_width(s_settings.keyboard, 1, LV_PART_ITEMS);
  lv_obj_set_style_radius(s_settings.keyboard, 14, LV_PART_ITEMS);
  lv_obj_set_style_text_font(s_settings.keyboard, &lv_font_montserrat_24, LV_PART_ITEMS);
  lv_obj_add_flag(s_settings.keyboard, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
  lv_obj_add_event_cb(s_settings.keyboard, keyboard_draw, LV_EVENT_DRAW_TASK_ADDED, NULL);
  lv_obj_add_event_cb(s_settings.keyboard, keyboard_key_event, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(s_settings.keyboard, keyboard_event, LV_EVENT_READY, NULL);
  lv_obj_add_event_cb(s_settings.keyboard, keyboard_event, LV_EVENT_CANCEL, NULL);
  lv_obj_add_flag(s_settings.keyboard, LV_OBJ_FLAG_HIDDEN);
  typing_layout(NULL);
}
