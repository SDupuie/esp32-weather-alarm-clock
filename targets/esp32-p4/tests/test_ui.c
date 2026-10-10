#ifndef WAC_UI_SHELL_TEST
#include "test_support.h"
#include "ui/ui_private.h"
#include "connectivity.h"
#include "location_lookup.h"
#include "assets/lv_font_clock_time_144.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Use real LVGL objects/events, with an irrelevant one-pixel background.
 * These checks establish draft/save behavior, not device rendering. */
static const uint8_t background_pixel[] = {0, 0, 0, 255};
const lv_image_dsc_t round_stage_day = {
  .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_ARGB8888,
    .w = 1, .h = 1, .stride = 4},
  .data_size = sizeof(background_pixel), .data = background_pixel
};
static wac_settings_view_t view;
static unsigned restart_count, task_count, lookup_count;
static bool fail_task, wifi_connected = true;
static const char *lookup_timezone = "America/New_York";
static char last_query[DEVICE_CONFIG_STR_LEN];
static struct { void (*run)(void *); void *arg; } tasks[8];

BaseType_t xTaskCreate(void (*run)(void *), const char *name, uint32_t stack,
                     void *arg, UBaseType_t priority, TaskHandle_t *handle)
{
  (void)name; (void)stack; (void)priority; (void)handle;
  if (fail_task) { fail_task = false; return pdFALSE; }
  assert(task_count < 8);
  tasks[task_count].run = run;
  tasks[task_count++].arg = arg;
  return pdPASS;
}
void vTaskDelete(TaskHandle_t task) { assert(task == NULL); }
void esp_restart(void) { ++restart_count; }
const char *esp_err_to_name(esp_err_t err)
{
  return err == ESP_ERR_INVALID_ARG ? "ESP_ERR_INVALID_ARG" : "ESP_FAIL";
}
bool connectivity_is_wifi_connected(void) { return wifi_connected; }
esp_err_t connectivity_scan_networks(connectivity_scan_result_t *networks,
                                    size_t capacity, size_t *count)
{
  assert(capacity >= 1);
  memset(networks, 0, sizeof(*networks));
  strcpy(networks[0].ssid, "Home network");
  networks[0].requires_password = true;
  *count = 1;
  return ESP_OK;
}
esp_err_t location_lookup_resolve(const char *query, location_lookup_result_t *result)
{
  ++lookup_count;
  snprintf(last_query, sizeof(last_query), "%s", query);
  strcpy(result->location, "Cincinnati, Ohio, US");
  strcpy(result->latitude, "39.1031");
  strcpy(result->longitude, "-84.5120");
  snprintf(result->timezone, sizeof(result->timezone), "%s", lookup_timezone);
  return ESP_OK;
}

void wac_ui_settings_navigate(wac_settings_view_t next)
{
  view = next;
  wac_settings_render(view);
}
wac_settings_view_t wac_ui_settings_view(void) { return view; }
void wac_ui_settings_close(void)
{
  wac_settings_close();
  view = WAC_SETTINGS_NONE;
}

static device_config_t current(void)
{
  device_config_t value;
  assert(device_config_copy(&value) == ESP_OK);
  return value;
}

static void open_editor(void)
{
  device_config_t value = current();
  wac_settings_begin(lv_screen_active(), &value);
  wac_ui_settings_navigate(WAC_SETTINGS_ENTRY);
}

static void fresh_editor(void)
{
  assert(task_count == 0);
  wac_ui_settings_close();
  fake_nvs_reset();
  assert(device_config_init() == ESP_OK);
  fake_nvs_clear_activity();
  open_editor();
}

static lv_obj_t *find_label(lv_obj_t *root, const char *prefix)
{
  if (lv_obj_check_type(root, &lv_label_class)
      && !strncmp(lv_label_get_text(root), prefix, strlen(prefix))) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *found = find_label(lv_obj_get_child(root, i), prefix);
    if (found) return found;
  }
  return NULL;
}

static lv_obj_t *find_button(lv_obj_t *root, const char *prefix)
{
  if (lv_obj_check_type(root, &lv_button_class) && find_label(root, prefix)) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *found = find_button(lv_obj_get_child(root, i), prefix);
    if (found) return found;
  }
  return NULL;
}

static void click(const char *prefix)
{
  lv_obj_t *button = find_button(lv_screen_active(), prefix);
  if (!button) fprintf(stderr, "Missing button '%s' in view %d\n", prefix, view);
  assert(button && !lv_obj_has_state(button, LV_STATE_DISABLED));
  /* Navigation may delete this button during its event; LVGL then reports
   * the deleted object, while the requested navigation has succeeded. */
  lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
}

static lv_obj_t *find_type(lv_obj_t *root, const lv_obj_class_t *type, unsigned *index)
{
  if (lv_obj_check_type(root, type)) {
    if (*index == 0) return root;
    --*index;
  }
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *found = find_type(lv_obj_get_child(root, i), type, index);
    if (found) return found;
  }
  return NULL;
}

static void field(unsigned index, const char *text)
{
  lv_obj_t *obj = find_type(lv_screen_active(), &lv_textarea_class, &index);
  assert(obj);
  lv_textarea_set_text(obj, text);
  lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
}

static void select_roller(unsigned index, unsigned selected)
{
  lv_obj_t *obj = find_type(lv_screen_active(), &lv_roller_class, &index);
  assert(obj);
  lv_roller_set_selected(obj, selected, LV_ANIM_OFF);
  lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
}

static void run_task(void)
{
  assert(task_count > 0);
  void (*run)(void *) = tasks[0].run;
  void *arg = tasks[0].arg;
  --task_count;
  memmove(tasks, tasks + 1, task_count * sizeof(tasks[0]));
  run(arg);
  wac_settings_poll();
}

static uint32_t keyboard_button(lv_obj_t *keyboard, const char *text)
{
  uint32_t id = 0;
  const char *label;
  while ((label = lv_keyboard_get_button_text(keyboard, id)) != NULL) {
    if (!strcmp(label, text)) break;
    ++id;
  }
  if (!label) fprintf(stderr, "Missing keyboard key '%s' in mode %d\n", text,
                      lv_keyboard_get_mode(keyboard));
  assert(label);
  return id;
}

static void keyboard_control(lv_obj_t *keyboard, const char *text,
                             unsigned width, bool preview)
{
  uint32_t id = keyboard_button(keyboard, text);
  for (unsigned bit = 1; bit <= 8; bit <<= 1) {
    assert(lv_buttonmatrix_has_button_ctrl(keyboard, id, (lv_buttonmatrix_ctrl_t)bit)
           == ((width & bit) != 0));
  }
  assert(lv_buttonmatrix_has_button_ctrl(keyboard, id, LV_BUTTONMATRIX_CTRL_POPOVER)
         == preview);
}

static void keyboard_key(lv_obj_t *keyboard, const char *text, bool preview)
{
  uint32_t id = keyboard_button(keyboard, text);
  if (preview) {
    assert(lv_keyboard_get_popovers(keyboard));
    assert(lv_buttonmatrix_has_button_ctrl(keyboard, id, LV_BUTTONMATRIX_CTRL_POPOVER));
  }
  lv_buttonmatrix_set_selected_button(keyboard, id);
  assert(lv_obj_send_event(keyboard, LV_EVENT_VALUE_CHANGED, NULL) == LV_RESULT_OK);
}

static void test_credentials_keyboard(void)
{
  fresh_editor();
  click("Device Setup");
  click("Wi-Fi");
  run_task();
  click("Home network");
  assert(view == WAC_SETTINGS_CREDENTIALS);
  unsigned index = 1;
  lv_obj_t *password = find_type(lv_screen_active(), &lv_textarea_class, &index);
  index = 0;
  lv_obj_t *keyboard = find_type(lv_screen_active(), &lv_keyboard_class, &index);
  assert(password && keyboard && lv_textarea_get_password_mode(password));
  assert(lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN));
  assert(lv_keyboard_get_textarea(keyboard) == NULL);
  lv_obj_send_event(password, LV_EVENT_CLICKED, NULL);
  assert(!lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN));
  assert(lv_keyboard_get_textarea(keyboard) == password);
  lv_obj_update_layout(lv_screen_active());
  assert(lv_obj_get_width(keyboard) == 680 && lv_obj_get_height(keyboard) == 280);
  assert(lv_obj_get_style_x(keyboard, LV_PART_MAIN) == 60
         && lv_obj_get_style_y(keyboard, LV_PART_MAIN) == 480);
  assert(lv_keyboard_get_mode(keyboard) == LV_KEYBOARD_MODE_TEXT_LOWER);
  keyboard_control(keyboard, LV_SYMBOL_KEYBOARD, 2, false);
  keyboard_control(keyboard, LV_SYMBOL_OK, 2, false);

  /* Enter a test password through the real keyboard's default event handler. */
  keyboard_key(keyboard, "a", true);
  keyboard_key(keyboard, "b", true);
  keyboard_key(keyboard, "c", true);
  keyboard_key(keyboard, "^", true);
  keyboard_key(keyboard, "ABC", false);
  assert(lv_keyboard_get_mode(keyboard) == LV_KEYBOARD_MODE_TEXT_UPPER);
  keyboard_control(keyboard, LV_SYMBOL_CLOSE, 2, false);
  keyboard_control(keyboard, LV_SYMBOL_OK, 2, false);
  keyboard_key(keyboard, "A", true);
  keyboard_key(keyboard, "^", true);
  keyboard_key(keyboard, "abc", false);
  assert(lv_keyboard_get_mode(keyboard) == LV_KEYBOARD_MODE_TEXT_LOWER);
  keyboard_key(keyboard, "1#", false);
  assert(lv_keyboard_get_mode(keyboard) == LV_KEYBOARD_MODE_SPECIAL);
  keyboard_control(keyboard, LV_SYMBOL_KEYBOARD, 2, false);
  keyboard_control(keyboard, LV_SYMBOL_OK, 2, false);
  keyboard_control(keyboard, "^", 1, true);
  keyboard_control(keyboard, ":", 1, true);
  keyboard_control(keyboard, " ", 4, false);
  keyboard_key(keyboard, ":", true);
  keyboard_key(keyboard, "^", true);
  assert(!strcmp(lv_textarea_get_text(password), "abc^A^:^"));

  keyboard_key(keyboard, LV_SYMBOL_LEFT, false);
  assert(lv_textarea_get_cursor_pos(password) == 7);
  keyboard_key(keyboard, " ", false);
  assert(!strcmp(lv_textarea_get_text(password), "abc^A^: ^"));
  keyboard_key(keyboard, LV_SYMBOL_RIGHT, false);
  assert(lv_textarea_get_cursor_pos(password) == 9);
  keyboard_key(keyboard, LV_SYMBOL_BACKSPACE, false);
  assert(!strcmp(lv_textarea_get_text(password), "abc^A^: "));
  keyboard_key(keyboard, LV_SYMBOL_BACKSPACE, false);
  keyboard_key(keyboard, "^", true);
  assert(!strcmp(lv_textarea_get_text(password), "abc^A^:^"));

  keyboard_key(keyboard, LV_SYMBOL_OK, false);
  assert(lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN));
  assert(lv_keyboard_get_textarea(keyboard) == NULL);
  assert(view == WAC_SETTINGS_CREDENTIALS && task_count == 0);
  lv_obj_send_event(password, LV_EVENT_CLICKED, NULL);
  assert(lv_keyboard_get_textarea(keyboard) == password);
  keyboard_key(keyboard, LV_SYMBOL_KEYBOARD, false);
  assert(lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN));
  assert(lv_keyboard_get_textarea(keyboard) == NULL);
  assert(!strcmp(lv_textarea_get_text(password), "abc^A^:^"));
  assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);

  /* Reopening reads the draft, so a textarea-only update cannot pass. */
  click("Back");
  click("Home network");
  index = 1;
  password = find_type(lv_screen_active(), &lv_textarea_class, &index);
  assert(password && !strcmp(lv_textarea_get_text(password), "abc^A^:^"));
  click("Back");
  click("Done");
  click("Cancel");
  device_config_t value = current();
  assert(!value.wifi_ssid[0] && !value.wifi_password[0]);
  assert(fake_nvs_calls(FAKE_SET) == 0);
}

static void test_nested_done_and_cancel(void)
{
  fresh_editor();
  click("Alarm Settings");
  click("Time");
  select_roller(0, 5); /* Real 12-hour roller: 6 AM. */
  select_roller(1, 30);
  click("Done");
  assert(view == WAC_SETTINGS_ALARM);
  assert(find_label(lv_screen_active(), "Time\n6:30 AM"));
  click("Sound & Snooze");
  click("Alarm Sound");
  click("Gentle Chime");
  click("Done");
  click("Done");
  assert(find_label(lv_screen_active(), "Sound & Snooze\nGentle Chime"));
  assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
  click("Cancel");
  click("Alarm Settings");
  assert(find_label(lv_screen_active(), "Time\n7:00 AM"));
  assert(find_label(lv_screen_active(), "Sound & Snooze\nClassic Beep"));
  click("Cancel");
  click("Device Setup");
  click("Device Settings");
  field(0, "Draft room");
  click("Done");
  click("Wi-Fi");
  assert(task_count == 1);
  run_task();
  click("Home network");
  field(1, "draft-password");
  click("Back");
  click("Done");
  assert(find_label(lv_screen_active(), "Device Settings\nDraft room"));
  assert(find_label(lv_screen_active(), "Wi-Fi\nHome network"));
  click("Cancel");
  device_config_t value = current();
  assert(!strcmp(value.room_name, "Clock") && !value.wifi_ssid[0]);
  assert(fake_nvs_calls(FAKE_SET) == 0);
}

static void test_selected_time_format(void)
{
  fresh_editor();
  click("Alarm Settings");
  click("Time");
  select_roller(0, 11); /* 12-hour roller uses index 11 for noon/midnight. */
  select_roller(2, 1);
  click("Done");
  assert(find_label(lv_screen_active(), "Time\n12:00 PM"));
  click("Cancel");
  assert(fake_nvs_calls(FAKE_SET) == 0);
  device_config_t baseline = current(), draft = baseline;
  strcpy(draft.time_format, "24");
  assert(device_config_save_device(&baseline, &draft) == ESP_OK);
  wac_ui_settings_close();
  open_editor();
  click("Alarm Settings");
  click("Time");
  select_roller(0, 18);
  select_roller(1, 45);
  click("Done");
  assert(find_label(lv_screen_active(), "Time\n18:45"));
  click("Cancel");
  baseline = current(); draft = baseline;
  strcpy(draft.time_format, "12");
  draft.leading_zero_12h = true;
  assert(device_config_save_device(&baseline, &draft) == ESP_OK);
  wac_ui_settings_close();
  open_editor();
  click("Alarm Settings");
  assert(find_label(lv_screen_active(), "Time\n07:00 AM"));
  click("Cancel");
}

static void test_alarm_failed_save_retry_and_toggle(void)
{
  fresh_editor();
  click("Alarm Settings");
  click("Time");
  select_roller(0, 5);
  select_roller(1, 30);
  click("Done");
  click("Alarm Off");
  device_config_t baseline = current(), newer = baseline;
  strcpy(newer.room_name, "New room saved elsewhere");
  assert(device_config_save_device(&baseline, &newer) == ESP_OK);
  fake_nvs_clear_activity();
  fail_task = true;
  click("Save");
  assert(task_count == 0 && view == WAC_SETTINGS_ALARM);
  assert(find_label(lv_screen_active(), "Could not start saving"));
  fake_nvs_fail(FAKE_COMMIT, 1);
  click("Save");
  assert(task_count == 1 && fake_nvs_calls(FAKE_SET) == 0);
  assert(!find_button(lv_screen_active(), "Save"));
  lv_obj_t *back = find_button(lv_screen_active(), LV_SYMBOL_LEFT);
  assert(back && lv_obj_has_state(back, LV_STATE_DISABLED));
  lv_obj_send_event(back, LV_EVENT_CLICKED, NULL);
  assert(task_count == 1); /* Callback also rejects duplicate submission. */
  run_task();
  assert(view == WAC_SETTINGS_ALARM && restart_count == 0);
  assert(find_label(lv_screen_active(), "Time\n6:30 AM"));
  assert(find_label(lv_screen_active(), "Could not save"));
  device_config_t value = current();
  assert(value.alarm_hour == 7 && !value.alarm_enabled);
  assert(!strcmp(value.room_name, newer.room_name));
  fake_nvs_clear_activity();
  click("Save");
  run_task();
  value = current();
  assert(value.alarm_hour == 6 && value.alarm_minute == 30 && value.alarm_enabled);
  assert(!strcmp(value.room_name, newer.room_name));
  assert(view == WAC_SETTINGS_ENTRY && restart_count == 0);
  fake_nvs_clear_activity();
  click("Alarm\nOn");
  assert(task_count == 1 && fake_nvs_calls(FAKE_SET) == 0);
  run_task();
  value = current();
  assert(!value.alarm_enabled && value.alarm_hour == 6);
  assert(view == WAC_SETTINGS_ENTRY && restart_count == 0);
}

static void test_required_editor_fields_before_submission(void)
{
  fresh_editor();
  click("Alarm Settings");
  click("Repeat");
  const char *weekdays[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday"};
  for (unsigned i = 0; i < sizeof(weekdays) / sizeof(weekdays[0]); ++i) click(weekdays[i]);
  click("Done");
  click("Save");
  assert(task_count == 0 && view == WAC_SETTINGS_ALARM);
  assert(find_label(lv_screen_active(), "Choose at least one Repeat day"));
  assert(fake_nvs_calls(FAKE_SET) == 0);
  click("Cancel");
  click("Device Setup");
  click("Device Settings");
  click("House Messages");
  click("Done");
  click("Save & Restart");
  assert(task_count == 0 && view == WAC_SETTINGS_DEVICE_SETUP);
  assert(find_label(lv_screen_active(), "Shared clocks need a distinct Device ID"));
  assert(fake_nvs_calls(FAKE_SET) == 0);
  click("Cancel");
}

static void test_closed_editor_ignores_save_navigation(void)
{
  fresh_editor();
  click("Device Setup");
  click("Device Settings");
  field(0, "Saved room");
  click("Done");
  click("Save & Restart");
  assert(task_count == 1 && fake_nvs_calls(FAKE_SET) == 0);
  wac_ui_settings_close();
  open_editor();
  run_task();
  device_config_t value = current();
  assert(!strcmp(value.room_name, "Saved room"));
  assert(view == WAC_SETTINGS_ENTRY && restart_count == 0);
  lv_tick_inc(1000);
  lv_timer_handler();
  assert(restart_count == 0);
}

static void test_weather_failure_retains_draft_and_success_restarts(void)
{
  fresh_editor();
  click("Weather Location");
  click("Save");
  assert(task_count == 0 && fake_nvs_calls(FAKE_SET) == 0);
  assert(find_label(lv_screen_active(), "Enter a City, State"));
  field(0, "Cincinnati,OH,US");
  click("Units");
  wifi_connected = false;
  click("Save");
  run_task();
  assert(find_label(lv_screen_active(), "Wi-Fi Required"));
  wifi_connected = true;
  lookup_timezone = "auto";
  click("Save");
  run_task();
  assert(!strcmp(last_query, "Cincinnati,OH,US"));
  assert(view == WAC_SETTINGS_WEATHER && restart_count == 0);
  assert(find_label(lv_screen_active(), "Units\nMetric"));
  assert(find_label(lv_screen_active(), "Could not save"));
  device_config_t value = current();
  assert(!value.location_ready && !strcmp(value.units, "imperial"));
  assert(fake_nvs_calls(FAKE_SET) == 0);
  lookup_timezone = "America/New_York";
  field(0, "Cincinnati,OH,US");
  fake_nvs_fail(FAKE_COMMIT, 1);
  click("Save");
  run_task();
  assert(view == WAC_SETTINGS_WEATHER && restart_count == 0);
  assert(find_label(lv_screen_active(), "Could not save"));
  value = current();
  assert(!value.location_ready && !strcmp(value.units, "imperial"));
  unsigned resolved_lookups = lookup_count;
  fake_nvs_clear_activity();
  wifi_connected = false;
  click("Save");
  run_task();
  assert(lookup_count == resolved_lookups);
  value = current();
  assert(value.location_ready && !strcmp(value.units, "metric"));
  assert(!strcmp(value.timezone, "America/New_York"));
  assert(find_label(lv_screen_active(), "Settings Saved. Restarting"));
  lv_tick_inc(1000);
  lv_timer_handler();
  assert(restart_count == 1);
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
  (void)area; (void)pixels;
  lv_display_flush_ready(display);
}

static void test_clock_time_font(void)
{
  /* Live time needs all decimal digits; unavailable time uses these exact
   * characters. Resolve them through LVGL against the production font. */
  const char *samples[] = {"0123456789", "--:--"};
  for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
    for (const char *letter = samples[i]; *letter; ++letter) {
      lv_font_glyph_dsc_t glyph;
      assert(lv_font_get_glyph_dsc(&lv_font_clock_time_144, &glyph,
                                   (unsigned char)*letter, (unsigned char)letter[1]));
      assert(!glyph.is_placeholder && glyph.resolved_font == &lv_font_clock_time_144);
      assert(glyph.adv_w > 0 && glyph.box_w > 0 && glyph.box_h > 0);
    }
  }
}

static void test_keyboard_only_on_text_editors(void)
{
  fresh_editor();
  for (wac_settings_view_t next = WAC_SETTINGS_ENTRY; next <= WAC_SETTINGS_BRIGHTNESS; ++next) {
    /* Begin with an allocated keyboard so each nontext view must remove it. */
    wac_ui_settings_navigate(WAC_SETTINGS_DEVICE);
    unsigned index = 0;
    assert(find_type(lv_screen_active(), &lv_keyboard_class, &index));
    wac_ui_settings_navigate(next);
    bool text_editor = next == WAC_SETTINGS_DEVICE || next == WAC_SETTINGS_WEATHER
      || next == WAC_SETTINGS_CREDENTIALS;
    index = 0;
    lv_obj_t *keyboard = find_type(lv_screen_active(), &lv_keyboard_class, &index);
    assert((keyboard != NULL) == text_editor);
    index = 1;
    assert(find_type(lv_screen_active(), &lv_keyboard_class, &index) == NULL);
    if (text_editor) {
      assert(lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN));
      assert(lv_keyboard_get_textarea(keyboard) == NULL);
      index = 0;
      lv_obj_t *textarea = find_type(lv_screen_active(), &lv_textarea_class, &index);
      assert(textarea);
      lv_obj_send_event(textarea, LV_EVENT_CLICKED, NULL);
      assert(!lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN));
      assert(lv_keyboard_get_textarea(keyboard) == textarea);
    }
  }
  wac_ui_settings_close();
}

int main(void)
{
  lv_init();
  test_clock_time_font();
  lv_display_t *display = lv_display_create(800, 800);
  static uint8_t buffer[800 * 32 * 4];
  lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);
  test_credentials_keyboard();
  test_keyboard_only_on_text_editors();
  test_nested_done_and_cancel();
  test_selected_time_format();
  test_alarm_failed_save_retry_and_toggle();
  test_required_editor_fields_before_submission();
  test_closed_editor_ignores_save_navigation();
  test_weather_failure_retains_draft_and_success_restarts();
  assert(task_count == 0);
  wac_ui_settings_close();
  lv_display_delete(display);
  lv_deinit();
  puts("Production UI checks passed: clock time font, credentials keyboard, nested drafts, cancellation, deferred saves, errors, retry, toggle, stale completion, location restart.");
  return 0;
}

#else
#include "test_support.h"
#include "app_ui.h"
#include "ui/ui_private.h"
#include "connectivity.h"
#include "location_lookup.h"
#include "message_service.h"
#include "assets/weather_icons.h"
#include "assets/lv_font_clock_time_144.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Production navigation, rendering and LVGL events run unchanged. Image
 * substitutes keep these behavior checks small; they prove no visual parity. */
static const uint8_t image_pixel[] = {0, 0, 0, 255};
#define TEST_IMAGE(name) const lv_image_dsc_t name = { \
  .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_ARGB8888, \
    .w = 1, .h = 1, .stride = 4}, .data_size = 4, .data = image_pixel }
TEST_IMAGE(analog_center_cap);
TEST_IMAGE(analog_edge_indicator);
TEST_IMAGE(analog_hour_hand);
TEST_IMAGE(analog_minute_hand);
TEST_IMAGE(analog_second_hand);
TEST_IMAGE(analog_stage_day);
TEST_IMAGE(analog_stage_night);
TEST_IMAGE(conditions_edge_indicator);
TEST_IMAGE(conditions_icon_pressure);
TEST_IMAGE(conditions_icon_rain);
TEST_IMAGE(conditions_icon_snow);
TEST_IMAGE(conditions_icon_wind);
TEST_IMAGE(conditions_wind_arrow);
TEST_IMAGE(digital_stage_day);
TEST_IMAGE(digital_stage_night);
TEST_IMAGE(forecast_tomorrow_frame_day);
TEST_IMAGE(forecast_tomorrow_frame_night);
TEST_IMAGE(round_stage_day);
TEST_IMAGE(round_stage_night);
TEST_IMAGE(weather_icon_clear_day);
TEST_IMAGE(test_forecast_cloudy);
TEST_IMAGE(test_forecast_rain);
TEST_IMAGE(test_forecast_snow);
TEST_IMAGE(test_forecast_fog);
TEST_IMAGE(test_forecast_thunder);
const lv_image_dsc_t *weather_icon_image_for_name(const char *name)
{
  if (name && !strcmp(name, "cloudy")) return &test_forecast_cloudy;
  if (name && !strcmp(name, "rain")) return &test_forecast_rain;
  if (name && !strcmp(name, "snow")) return &test_forecast_snow;
  if (name && !strcmp(name, "fog")) return &test_forecast_fog;
  if (name && !strcmp(name, "thunderstorm")) return &test_forecast_thunder;
  return &weather_icon_clear_day;
}

const lv_image_dsc_t *weather_icon_image_for_snapshot(const app_weather_snapshot_t *snapshot)
{ (void)snapshot; return &weather_icon_clear_day; }

BaseType_t xTaskCreate(void (*run)(void *), const char *name, uint32_t stack,
                      void *arg, UBaseType_t priority, TaskHandle_t *handle)
{
  (void)run; (void)name; (void)stack; (void)arg; (void)priority; (void)handle;
  assert(!"No background settings work expected in these checks");
  return pdFALSE;
}

void vTaskDelete(TaskHandle_t task) { (void)task; }
void vTaskDelay(TickType_t ticks) { (void)ticks; }
void esp_restart(void) { assert(!"No restart expected"); }
const char *esp_err_to_name(esp_err_t error)
{ return error == ESP_ERR_TIMEOUT ? "ESP_ERR_TIMEOUT" : "ESP_ERR_INVALID_STATE"; }
int64_t esp_timer_get_time(void) { return (int64_t)lv_tick_get() * 1000; }
bool connectivity_is_wifi_connected(void) { return true; }
esp_err_t connectivity_scan_networks(connectivity_scan_result_t *networks,
                                    size_t capacity, size_t *count)
{ (void)networks; (void)capacity; *count = 0; return ESP_OK; }
esp_err_t location_lookup_resolve(const char *query, location_lookup_result_t *result)
{ (void)query; (void)result; return ESP_FAIL; }
static unsigned message_submissions;
static esp_err_t message_submission_result;
static char submitted_message[APP_MESSAGE_ID_LEN], submitted_device[DEVICE_CONFIG_STR_LEN];
esp_err_t message_service_submit_acknowledgment(const char *device, const char *id)
{
  ++message_submissions;
  snprintf(submitted_device, sizeof(submitted_device), "%s", device);
  snprintf(submitted_message, sizeof(submitted_message), "%s", id);
  return message_submission_result;
}

static unsigned dismissal_count;
static esp_err_t dismissal_result;
static wac_time_warning_t dismissed_warning;
static esp_err_t submit_dismissal(wac_time_warning_t warning)
{ ++dismissal_count; dismissed_warning = warning; return dismissal_result; }

static lv_obj_t *find_label(lv_obj_t *root, const char *text)
{
  if (lv_obj_check_type(root, &lv_label_class)
      && !strcmp(lv_label_get_text(root), text)) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *found = find_label(lv_obj_get_child(root, i), text);
    if (found) return found;
  }
  return NULL;
}
static bool visible(lv_obj_t *object)
{
  if (!object) return false;
  for (; object; object = lv_obj_get_parent(object))
    if (lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) return false;
  return true;
}
static lv_obj_t *find_visible_label(lv_obj_t *root, const char *text)
{
  if (!visible(root)) return NULL;
  if (lv_obj_check_type(root, &lv_label_class)
      && !strcmp(lv_label_get_text(root), text)) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *found = find_visible_label(lv_obj_get_child(root, i), text);
    if (found) return found;
  }
  return NULL;
}
static lv_obj_t *find_image_source(lv_obj_t *root, const void *source)
{
  if (lv_obj_check_type(root, &lv_image_class) && lv_image_get_src(root) == source) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *found = find_image_source(lv_obj_get_child(root, i), source);
    if (found) return found;
  }
  return NULL;
}
static unsigned visible_image_count(lv_obj_t *root, const void *source)
{
  if (!visible(root)) return 0;
  unsigned count = lv_obj_check_type(root, &lv_image_class) && lv_image_get_src(root) == source;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
    count += visible_image_count(lv_obj_get_child(root, i), source);
  return count;
}
static void assert_home_edges(void)
{
  assert(visible_image_count(lv_screen_active(), &analog_edge_indicator) == 1);
  assert(visible_image_count(lv_screen_active(), &conditions_edge_indicator) == 1);
}
static lv_obj_t *label(const char *text)
{ lv_obj_t *found = find_label(lv_screen_active(), text);
  if (!found) fprintf(stderr, "Missing shell label: %s\n", text);
  assert(found); return found; }
static lv_obj_t *find_warning_button(lv_obj_t *root)
{
  if (lv_obj_check_type(root, &lv_button_class) && find_label(root, LV_SYMBOL_WARNING)) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *found = find_warning_button(lv_obj_get_child(root, i));
    if (found) return found;
  }
  return NULL;
}
static lv_obj_t *indicator(void)
{ lv_obj_t *button = find_warning_button(lv_screen_active()); assert(button); return button; }
static void click(const char *text)
{ lv_obj_send_event(!strcmp(text, LV_SYMBOL_WARNING) ? indicator() : lv_obj_get_parent(label(text)),
                    LV_EVENT_CLICKED, NULL); }
static lv_obj_t *overlay(void)
{ return lv_obj_get_parent(lv_obj_get_parent(lv_obj_get_parent(label("DISMISS")))); }
static lv_obj_t *find_textarea(lv_obj_t *root)
{
  if (lv_obj_check_type(root, &lv_textarea_class)) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *found = find_textarea(lv_obj_get_child(root, i));
    if (found) return found;
  }
  return NULL;
}
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{ (void)area; (void)pixels; lv_display_flush_ready(display); }

static lv_indev_data_t pointer_data;
static void read_pointer(lv_indev_t *indev, lv_indev_data_t *data)
{ (void)indev; *data = pointer_data; }
static void swipe(lv_indev_t *pointer, int x, int y, int end_x, int end_y)
{
  lv_obj_update_layout(lv_screen_active());
  pointer_data.point = (lv_point_t){x, y};
  pointer_data.state = LV_INDEV_STATE_PRESSED;
  lv_indev_read(pointer);
  pointer_data.point = (lv_point_t){end_x, end_y};
  lv_tick_inc(30);
  lv_indev_read(pointer);
  pointer_data.state = LV_INDEV_STATE_RELEASED;
  lv_tick_inc(30);
  lv_indev_read(pointer);
}

static lv_obj_t *forecast_image(lv_obj_t *root, int width)
{
  if (lv_obj_check_type(root, &lv_image_class) && lv_obj_get_width(root) == width) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *found = forecast_image(lv_obj_get_child(root, i), width);
    if (found) return found;
  }
  return NULL;
}

static unsigned direct_label_count(lv_obj_t *parent, const char *text)
{
  unsigned count = 0;
  for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i) {
    lv_obj_t *child = lv_obj_get_child(parent, i);
    if (lv_obj_check_type(child, &lv_label_class) && !strcmp(lv_label_get_text(child), text)) ++count;
  }
  return count;
}

static void assert_pending_forecast(lv_obj_t *root, unsigned counts[3])
{
  if (lv_obj_check_type(root, &lv_image_class)) {
    int width = lv_obj_get_width(root);
    int index = width == 75 ? 0 : width == 90 ? 1 : width == 364 ? 2 : -1;
    if (index >= 0) {
      ++counts[index];
      assert(direct_label_count(lv_obj_get_parent(root), "--") == 2);
    }
  }
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
    assert_pending_forecast(lv_obj_get_child(root, i), counts);
}

static time_t local_epoch(int year, int month, int day, int hour, int minute)
{
  struct tm local = {.tm_year = year - 1900, .tm_mon = month - 1,
    .tm_mday = day, .tm_hour = hour, .tm_min = minute, .tm_isdst = -1};
  time_t epoch = mktime(&local);
  assert(epoch != (time_t)-1);
  return epoch;
}

static void set_forecast_dates(app_weather_snapshot_t *weather, const char *const dates[5])
{
  for (unsigned i = 0; i < APP_FORECAST_DAYS; ++i)
    snprintf(weather->forecast[i].date, sizeof(weather->forecast[i].date), "%s", dates[i]);
}

static void assert_forecast_pairing(const app_weather_snapshot_t *weather,
                                    const char *const weekdays[5], const char *caption)
{
  for (unsigned i = 0; i < APP_FORECAST_DAYS; ++i) {
    char high[16], upper[8], range[24];
    snprintf(high, sizeof(high), "%d°", weather->forecast[i].high);
    snprintf(upper, sizeof(upper), "%s", weekdays[i]);
    for (char *p = upper; *p; ++p) if (*p >= 'a' && *p <= 'z') *p -= 'a' - 'A';
    lv_obj_t *item = lv_obj_get_parent(label(high));
    assert(find_label(item, upper));
    assert(lv_image_get_src(forecast_image(item, 75))
      == weather_icon_image_for_name(weather->forecast[i].icon_name));
    snprintf(range, sizeof(range), "%d° / %d°", weather->forecast[i].high, weather->forecast[i].low);
    lv_obj_t *row = lv_obj_get_parent(label(range));
    assert(direct_label_count(row, i == 0 ? caption : weekdays[i]) == 1);
    assert(lv_image_get_src(forecast_image(row, i == 0 ? 364 : 90))
      == weather_icon_image_for_name(weather->forecast[i].icon_name));
  }
}

static void test_provider_forecast_dates(lv_indev_t *pointer)
{
  fake_nvs_reset();
  assert(device_config_init() == ESP_OK);
  device_config_t config;
  assert(device_config_copy(&config) == ESP_OK);
  config.location_ready = true;
  strcpy(config.default_clock_face, "digital");
  app_runtime_state_t runtime = {0};
  assert(app_ui_show_boot_screen(&config, &runtime, true) == ESP_OK);
  /* No provider payload: both forecasts have unknown days even with a clock. */
  unsigned pending_counts[3] = {0};
  assert_pending_forecast(lv_screen_active(), pending_counts);
  assert(pending_counts[0] == 5 && pending_counts[1] == 4 && pending_counts[2] == 1);
  assert(!find_label(lv_screen_active(), "Tomorrow"));
  runtime.clock.usable = true;
  runtime.clock.source = WAC_TIME_SOURCE_NETWORK;
  runtime.clock.utc_now = local_epoch(2026, 10, 9, 12, 0);
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  memset(pending_counts, 0, sizeof(pending_counts));
  assert_pending_forecast(lv_screen_active(), pending_counts);
  assert(pending_counts[0] == 5 && pending_counts[1] == 4 && pending_counts[2] == 1);
  assert(!find_label(lv_screen_active(), "SAT"));
  assert(!find_label(lv_screen_active(), "Tomorrow"));

  /* Illustrative provider entries, with distinct values and image identities,
   * exercise actual LVGL rendering; these are not downloaded observations. */
  const char *const dates[] = {"2026-10-10", "2026-10-11", "2026-10-12", "2026-10-13", "2026-10-14"};
  const char *const days[] = {"Sat", "Sun", "Mon", "Tue", "Wed"};
  const char *const icons[] = {"cloudy", "rain", "snow", "fog", "thunderstorm"};
  runtime.weather.valid = true;
  runtime.weather.temp = 32;
  runtime.weather.updated_at = runtime.clock.utc_now;
  set_forecast_dates(&runtime.weather, dates);
  for (unsigned i = 0; i < APP_FORECAST_DAYS; ++i) {
    runtime.weather.forecast[i].high = 71 + i;
    runtime.weather.forecast[i].low = 41 + i;
    strcpy(runtime.weather.forecast[i].icon_name, icons[i]);
  }
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert_forecast_pairing(&runtime.weather, days, "Tomorrow"); /* Fresh labels are unchanged. */
  swipe(pointer, 600, 200, 300, 200);
  assert(visible(label("Tomorrow")) && visible(label("71° / 41°")));

  runtime.clock.usable = false;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert_forecast_pairing(&runtime.weather, days, "Sat");
  runtime.clock.usable = true;
  runtime.clock.source = WAC_TIME_SOURCE_RTC;
  runtime.clock.utc_now = local_epoch(2026, 10, 8, 12, 0); /* Wrong clock before correction. */
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert_forecast_pairing(&runtime.weather, days, "Sat");
  runtime.clock.source = WAC_TIME_SOURCE_NETWORK;
  runtime.clock.utc_now = local_epoch(2026, 10, 9, 23, 59);
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert_forecast_pairing(&runtime.weather, days, "Tomorrow");
  runtime.clock.utc_now += 60; /* Same payload across midnight and failed refresh. */
  runtime.weather.stale = true;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert_forecast_pairing(&runtime.weather, days, "Sat");
  app_weather_snapshot_t retained = runtime.weather;
  runtime.weather.valid = false; /* UI retains the last valid snapshot too. */
  runtime.clock.utc_now += 24 * 60 * 60;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert_forecast_pairing(&retained, days, "Sat");

  runtime.weather = retained;
  const char *const invalid[] = {"", "2026-02-30", "2026-13-12", "2026-10-1x", "0000-10-14"};
  const char *const unknown[] = {"--", "--", "--", "--", "--"};
  set_forecast_dates(&runtime.weather, invalid);
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert_forecast_pairing(&runtime.weather, unknown, "--");

  /* Calendar boundaries include Gregorian leap rules and DST transitions.
   * Spring/fall checks use local late-night time, where adding 24 hours
   * instead of advancing the calendar can produce the wrong date. */
  const struct {
    const char *timezone, *first_date, *weekday;
    int year, month, day, hour, minute;
  } boundaries[] = {
    {"UTC0", "2026-11-01", "Sun", 2026, 10, 31, 23, 59},
    {"UTC0", "2027-01-01", "Fri", 2026, 12, 31, 23, 59},
    {"UTC0", "2024-02-29", "Thu", 2024, 2, 28, 23, 59},
    {"UTC0", "2024-03-01", "Fri", 2024, 2, 29, 23, 59},
    {"UTC0", "2100-03-01", "Mon", 2100, 2, 28, 23, 59},
    {"UTC0", "2000-02-29", "Tue", 2000, 2, 28, 23, 59},
    {"EST5EDT,M3.2.0/2,M11.1.0/2", "2026-03-08", "Sun", 2026, 3, 7, 23, 30},
    {"EST5EDT,M3.2.0/2,M11.1.0/2", "2026-11-01", "Sun", 2026, 10, 31, 23, 30},
    {"EST5EDT,M3.2.0/2,M11.1.0/2", "2026-03-09", "Mon", 2026, 3, 8, 0, 30},
    {"EST5EDT,M3.2.0/2,M11.1.0/2", "2026-11-02", "Mon", 2026, 11, 1, 0, 30},
  };
  for (unsigned i = 0; i < sizeof(boundaries) / sizeof(boundaries[0]); ++i) {
    setenv("TZ", boundaries[i].timezone, 1);
    tzset();
    strcpy(runtime.weather.forecast[0].date, boundaries[i].first_date);
    runtime.clock.utc_now = local_epoch(boundaries[i].year, boundaries[i].month,
      boundaries[i].day, boundaries[i].hour, boundaries[i].minute);
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    const char *case_days[] = {boundaries[i].weekday, "--", "--", "--", "--"};
    assert_forecast_pairing(&runtime.weather, case_days, "Tomorrow");
    runtime.clock.usable = false;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert_forecast_pairing(&runtime.weather, case_days, boundaries[i].weekday);
    runtime.clock.usable = true;
  }
  setenv("TZ", "UTC0", 1);
  tzset();
}

static void test_normal_clock_surfaces(lv_indev_t *pointer)
{
  fake_nvs_reset();
  assert(device_config_init() == ESP_OK);
  device_config_t config;
  assert(device_config_copy(&config) == ESP_OK);
  config.location_ready = true;
  config.wifi_ready = true;
  fake_nvs_clear_activity();

  /* Controlled owner snapshots exercise rendering; these do not establish
   * whether a real RTC or network supplied trustworthy time. */
  for (unsigned analog = 0; analog < 2; ++analog) {
    snprintf(config.default_clock_face, sizeof(config.default_clock_face), "%s",
             analog ? "analog" : "digital");
    snprintf(config.time_format, sizeof(config.time_format), "12");
    config.leading_zero_12h = false;
    app_runtime_state_t runtime = {0};
    runtime.clock.usable = true;
    runtime.clock.source = WAC_TIME_SOURCE_RTC;
    runtime.clock.utc_now = local_epoch(2026, 10, 9, 7, 6) + 30;
    runtime.unread_message_count = 1;
    runtime.has_important_message = true;
    runtime.weather.valid = true;
    runtime.weather.has_conditions = true;
    runtime.weather.conditions.alert = true;
    snprintf(runtime.weather.icon_name, sizeof(runtime.weather.icon_name), "clear-day");
    snprintf(runtime.weather_status, sizeof(runtime.weather_status), "Weather test status");
    assert(app_ui_show_boot_screen(&config, &runtime, true) == ESP_OK);
    assert_home_edges();
    assert(find_visible_label(lv_screen_active(), analog ? "Friday" : "FRIDAY"));
    assert(find_visible_label(lv_screen_active(), analog ? "Oct 9" : "OCT 9"));
    assert(find_visible_label(lv_screen_active(), "Weather test status"));
    lv_obj_t *hour = find_image_source(lv_screen_active(), &analog_hour_hand);
    lv_obj_t *minute = find_image_source(lv_screen_active(), &analog_minute_hand);
    lv_obj_t *second = find_image_source(lv_screen_active(), &analog_second_hand);
    assert(hour && minute && second);
    if (analog) {
      assert(visible(hour) && visible(minute) && visible(second));
      assert(lv_image_get_rotation(hour) == 2130);
      assert(lv_image_get_rotation(minute) == 360);
      assert(lv_image_get_rotation(second) == 1800);
    } else {
      assert(find_visible_label(lv_screen_active(), "7:06"));
      assert(find_visible_label(lv_screen_active(), "AM"));
      config.leading_zero_12h = true;
      assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
      assert(find_visible_label(lv_screen_active(), "07:06"));
      runtime.clock.utc_now = local_epoch(2026, 10, 9, 0, 0);
      assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
      assert(find_visible_label(lv_screen_active(), "12:00"));
      assert(find_visible_label(lv_screen_active(), "AM"));
      runtime.clock.utc_now = local_epoch(2026, 10, 9, 12, 0);
      assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
      assert(find_visible_label(lv_screen_active(), "12:00"));
      assert(find_visible_label(lv_screen_active(), "PM"));
      snprintf(config.time_format, sizeof(config.time_format), "24");
      runtime.clock.utc_now = local_epoch(2026, 10, 9, 0, 6);
      assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
      assert(find_visible_label(lv_screen_active(), "00:06"));
      assert(!find_visible_label(lv_screen_active(), "AM"));
      assert(!find_visible_label(lv_screen_active(), "PM"));
      runtime.clock.utc_now = local_epoch(2026, 10, 9, 23, 59);
      assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
      assert(find_visible_label(lv_screen_active(), "23:59"));
    }

    /* The owner selects appearance even when the saved mode says Day. */
    config.night_shift_mode = DEVICE_CONFIG_NIGHT_OFF;
    runtime.night_active = true;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert_home_edges();
    assert(find_visible_label(lv_screen_active(), analog ? "Friday" : "FRIDAY"));
    assert(find_visible_label(lv_screen_active(), analog ? "Oct 9" : "OCT 9"));
    assert(!find_visible_label(lv_screen_active(), "Weather test status"));
    assert(visible(find_image_source(lv_screen_active(),
                                  analog ? &analog_stage_night : &digital_stage_night)));
    config.night_shift_mode = DEVICE_CONFIG_NIGHT_ALWAYS_ON;
    runtime.night_active = false;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert_home_edges();
    assert(find_visible_label(lv_screen_active(), "Weather test status"));
    assert(visible(find_image_source(lv_screen_active(),
                                  analog ? &analog_stage_day : &digital_stage_day)));

    /* Updating an unchanged minute must keep the inherited pause diagnostic.
     * Seconds advancing alone are deliberately insufficient to clear it. */
    runtime.clock.source = WAC_TIME_SOURCE_NETWORK;
    runtime.clock.utc_now = local_epoch(2026, 10, 9, 7, 6);
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    for (unsigned i = 0; i < 135; ++i) {
      lv_tick_inc(1000);
      runtime.clock.utc_now = local_epoch(2026, 10, 9, 7, 6) + i % 60;
      assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
      assert(!find_visible_label(lv_screen_active(), "Clock paused"));
    }
    lv_tick_inc(1); /* Preserves the existing strictly-after-135-seconds edge. */
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    lv_obj_t *paused = find_visible_label(lv_screen_active(), "Clock paused");
    assert(paused);
    assert(lv_obj_get_style_text_font(paused, 0) == &lv_font_montserrat_28);
    lv_obj_update_layout(lv_screen_active());
    lv_area_t notice;
    lv_obj_get_coords(lv_obj_get_parent(paused), &notice);
    assert(notice.x1 == (analog ? 220 : 170) && notice.y1 == (analog ? 220 : 642));
    assert(lv_area_get_width(&notice) == (analog ? 360 : 460));
    assert(lv_area_get_height(&notice) == (analog ? 72 : 80));
    runtime.clock.utc_now = local_epoch(2026, 10, 9, 7, 7);
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert(!find_visible_label(lv_screen_active(), "Clock paused"));

    /* A plausible epoch with unusable trust must never appear as real time. */
    runtime.clock.usable = false;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert_home_edges();
    const char *unavailable_text[] = {"Time unavailable", "--:--", "Waiting for time sync"};
    const int x[] = {140, 140, 120}, y[] = {140, 228, 405}, width[] = {520, 520, 560};
    for (unsigned i = 0; i < 3; ++i) {
      lv_obj_t *text = find_visible_label(lv_screen_active(), unavailable_text[i]);
      assert(text);
      lv_obj_update_layout(lv_screen_active());
      lv_area_t coords;
      lv_obj_get_coords(text, &coords);
      assert(coords.x1 == x[i] && coords.y1 == y[i]);
      assert(lv_area_get_width(&coords) == width[i]);
      assert(lv_obj_get_style_text_font(text, 0) == (i == 1 ? &lv_font_clock_time_144 : &lv_font_montserrat_36));
    }
    assert(!find_visible_label(lv_screen_active(), "Clock pending network sync"));
    assert(!find_visible_label(lv_screen_active(), "Weather test status"));
    assert(!find_visible_label(lv_screen_active(), "Alarm Settings"));
    assert(!visible(hour) && !visible(minute) && !visible(second));
    runtime.night_active = true;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert_home_edges();
    assert(find_visible_label(lv_screen_active(), "Time unavailable"));
    assert(find_visible_label(lv_screen_active(), "--:--"));
    assert(find_visible_label(lv_screen_active(), "Waiting for time sync"));
    swipe(pointer, 600, 200, 300, 200);
    assert(!find_visible_label(lv_screen_active(), "Time unavailable"));
    swipe(pointer, 300, 200, 600, 200);
    assert(find_visible_label(lv_screen_active(), "Time unavailable"));
    runtime.night_active = false;
    runtime.clock.usable = true;
    runtime.clock.source = WAC_TIME_SOURCE_RTC;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert_home_edges();
    assert(!find_visible_label(lv_screen_active(), "Clock pending network sync"));
    assert(find_visible_label(lv_screen_active(), analog ? "Friday" : "FRIDAY"));
    if (analog) assert(visible(hour) && visible(minute) && visible(second));

    /* Both configured faces retain the real-pointer navigation map. */
    swipe(pointer, 600, 200, 300, 200);
    assert(!find_visible_label(lv_screen_active(), analog ? "Friday" : "FRIDAY"));
    swipe(pointer, 300, 200, 600, 200);
    assert(find_visible_label(lv_screen_active(), analog ? "Friday" : "FRIDAY"));
    swipe(pointer, 300, 200, 600, 200);
    assert(!find_visible_label(lv_screen_active(), analog ? "Friday" : "FRIDAY"));
    swipe(pointer, 600, 200, 300, 200);
    assert(find_visible_label(lv_screen_active(), analog ? "Friday" : "FRIDAY"));
    swipe(pointer, 600, 400, 600, 200); /* Up remains reserved. */
    assert(find_visible_label(lv_screen_active(), analog ? "Friday" : "FRIDAY"));
    swipe(pointer, 600, 200, 600, 400);
    assert(wac_ui_settings_view() == WAC_SETTINGS_ENTRY);
    wac_ui_settings_close();
    assert(find_visible_label(lv_screen_active(), analog ? "Friday" : "FRIDAY"));
  }
  assert(fake_nvs_calls(FAKE_SET) == 0);
}

static void test_digital_night_layout(void)
{
  fake_nvs_reset();
  assert(device_config_init() == ESP_OK);
  device_config_t config;
  assert(device_config_copy(&config) == ESP_OK);
  config.location_ready = true;
  config.wifi_ready = true;
  snprintf(config.default_clock_face, sizeof(config.default_clock_face), "digital");
  snprintf(config.time_format, sizeof(config.time_format), "12");
  config.leading_zero_12h = false;
  app_runtime_state_t runtime = {0};
  runtime.clock.usable = true;
  runtime.clock.source = WAC_TIME_SOURCE_NETWORK;
  runtime.clock.utc_now = local_epoch(2026, 10, 9, 7, 6);
  assert(app_ui_show_boot_screen(&config, &runtime, true) == ESP_OK);
  lv_obj_t *time = find_visible_label(lv_screen_active(), "7:06");
  assert(time);
  lv_obj_update_layout(lv_screen_active());
  lv_area_t day_coords;
  lv_obj_get_coords(time, &day_coords);
  assert(lv_obj_get_style_transform_scale_x_safe(time, 0) == 256);

  runtime.night_active = true;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  lv_obj_update_layout(lv_screen_active());
  lv_area_t reserved;
  lv_obj_get_coords(time, &reserved);
  int scale = lv_obj_get_style_transform_scale_x_safe(time, 0);
  assert(scale > 256);
  assert(scale == lv_obj_get_style_transform_scale_y_safe(time, 0));
  int pivot_x = lv_obj_get_style_transform_pivot_x(time, 0);
  int pivot_y = lv_obj_get_style_transform_pivot_y(time, 0);
  int center_x = reserved.x1 + pivot_x
    + ((lv_area_get_width(&reserved) / 2 - pivot_x) * scale) / 256;
  int center_y = reserved.y1 + pivot_y
    + ((lv_area_get_height(&reserved) / 2 - pivot_y) * scale) / 256;
  assert(abs(center_x - 400) <= 2 && abs(center_y - 400) <= 2);
  assert(find_visible_label(lv_screen_active(), "FRIDAY"));
  assert(find_visible_label(lv_screen_active(), "OCT 9"));

  const int hours[] = {8, 11, 12, 23};
  const int minutes[] = {8, 59, 0, 59};
  const char *texts[] = {"8:08", "11:59", "12:00", "11:59"};
  for (unsigned i = 0; i < 4; ++i) {
    runtime.clock.utc_now = local_epoch(2026, 10, 9, hours[i], minutes[i]);
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    lv_obj_update_layout(lv_screen_active());
    assert(find_visible_label(lv_screen_active(), texts[i]) == time);
    assert(lv_obj_get_style_transform_scale_x_safe(time, 0) == scale);
    lv_area_t now;
    lv_obj_get_coords(time, &now);
    center_x = now.x1 + pivot_x
      + ((lv_area_get_width(&now) / 2 - pivot_x) * scale) / 256;
    center_y = now.y1 + pivot_y
      + ((lv_area_get_height(&now) / 2 - pivot_y) * scale) / 256;
    assert(abs(center_x - 400) <= 2 && abs(center_y - 400) <= 2);
    lv_obj_t *period = find_visible_label(lv_screen_active(), i < 2 ? "AM" : "PM");
    assert(period);
    lv_area_t period_coords;
    lv_obj_get_coords(period, &period_coords);
    /* The right-hand period must fit the round display as well as the time.
     * This checks LVGL layout bounds; small image substitutes cannot prove
     * raster-font sharpness or a physical screen's clipping. */
    int dx = period_coords.x2 - 400, dy = period_coords.y2 - 400;
    assert(dx * dx + dy * dy <= 390 * 390);
    const lv_font_t *font = lv_obj_get_style_text_font(period, 0);
    assert(font == &lv_font_montserrat_24);
    int digit_baseline = now.y1 + pivot_y
      + ((lv_font_clock_time_144.line_height - lv_font_clock_time_144.base_line - pivot_y) * scale) / 256;
    int period_baseline = period_coords.y1 + font->line_height - font->base_line;
    assert(abs(digit_baseline - period_baseline) <= 3);
  }

  runtime.night_active = false;
  runtime.clock.utc_now = local_epoch(2026, 10, 9, 7, 6);
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  lv_obj_update_layout(lv_screen_active());
  lv_area_t restored;
  lv_obj_get_coords(time, &restored);
  assert(lv_obj_get_style_transform_scale_x_safe(time, 0) == 256);
  assert(restored.x1 == day_coords.x1 && restored.y1 == day_coords.y1
    && restored.x2 == day_coords.x2 && restored.y2 == day_coords.y2);

  /* 24-hour Night reserves its widest five-character time as well. */
  snprintf(config.time_format, sizeof(config.time_format), "24");
  runtime.night_active = true;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  lv_obj_update_layout(lv_screen_active());
  assert(find_visible_label(lv_screen_active(), "07:06") == time);
  lv_obj_get_coords(time, &reserved);
  scale = lv_obj_get_style_transform_scale_x_safe(time, 0);
  runtime.clock.utc_now = local_epoch(2026, 10, 9, 23, 59);
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  lv_obj_update_layout(lv_screen_active());
  assert(find_visible_label(lv_screen_active(), "23:59") == time);
  lv_obj_get_coords(time, &restored);
  assert(lv_obj_get_style_transform_scale_x_safe(time, 0) == scale);
  assert(abs(restored.x1 + lv_area_get_width(&restored) * scale / 512 - 400) <= 2);
  assert(restored.y1 == reserved.y1 && restored.y2 == reserved.y2);
  assert(!find_visible_label(lv_screen_active(), "AM"));
  assert(!find_visible_label(lv_screen_active(), "PM"));
}

static void test_backup_clock_ui(lv_indev_t *pointer)
{
  fake_nvs_reset();
  assert(device_config_init() == ESP_OK);
  device_config_t config;
  assert(device_config_copy(&config) == ESP_OK);
  fake_nvs_clear_activity();
  config.location_ready = true;
  config.wifi_ready = true;
  app_runtime_state_t runtime = {0};
  runtime.clock.warning = WAC_TIME_WARNING_LOST_TIME;
  runtime.clock.source = WAC_TIME_SOURCE_UNAVAILABLE;
  /* Registration must survive boot's UI-state reset. */
  app_ui_set_clock_warning_dismiss_handler(submit_dismissal);
  assert(app_ui_show_boot_screen(&config, &runtime, true) == ESP_OK);
  assert(find_visible_label(lv_screen_active(), "--:--") && visible(indicator()));
  lv_obj_update_layout(lv_screen_active());
  assert(lv_obj_get_x(indicator()) == 464 && lv_obj_get_y(indicator()) == 56);
  assert(!visible(overlay()));
  click(LV_SYMBOL_WARNING);
  assert(visible(overlay()) && visible(label("Backup Clock Lost Time")));
  assert(visible(label("Check Battery.")));
  lv_obj_t *card = lv_obj_get_parent(label("Backup Clock Lost Time"));
  lv_obj_update_layout(lv_screen_active());
  assert(lv_obj_get_x(card) == 140 && lv_obj_get_y(card) == 318);
  assert(lv_obj_get_width(card) == 520 && lv_obj_get_height(card) == 290);
  assert(lv_obj_get_style_text_font(label("Check Battery."), 0)->line_height
    == lv_font_montserrat_28.line_height);
  assert(lv_obj_get_style_text_font(label("DISMISS"), 0) == &lv_font_montserrat_36);
  swipe(pointer, 600, 200, 300, 200);
  assert(visible(overlay()) && find_visible_label(lv_screen_active(), "--:--"));
  swipe(pointer, 600, 200, 600, 400);
  assert(wac_ui_settings_view() == WAC_SETTINGS_NONE);

  dismissal_result = ESP_ERR_TIMEOUT;
  click("DISMISS");
  assert(dismissal_count == 1 && visible(overlay()));
  assert(visible(label("Dismiss failed (ESP_ERR_TIMEOUT).")));
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert(visible(label("Dismiss failed (ESP_ERR_TIMEOUT).")));
  assert(!lv_obj_has_state(lv_obj_get_parent(label("DISMISS")), LV_STATE_DISABLED));
  dismissal_result = ESP_OK;
  click("DISMISS");
  assert(dismissal_count == 2 && visible(overlay()));
  assert(lv_obj_has_state(lv_obj_get_parent(label("DISMISS")), LV_STATE_DISABLED));
  click("DISMISS"); /* Even direct repeated events cannot enqueue twice. */
  assert(dismissal_count == 2);

  runtime.clock.usable = true;
  runtime.clock.source = WAC_TIME_SOURCE_NETWORK;
  runtime.clock.utc_now = 1577862360; /* Made-up 2020-01-01 07:06 UTC, below old date threshold. */
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert(visible(label("7:06")) && visible(overlay()));
  assert(lv_obj_has_state(lv_obj_get_parent(label("DISMISS")), LV_STATE_DISABLED));
  runtime.clock.warning = WAC_TIME_WARNING_NONE;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert(!visible(overlay()) && !visible(indicator()));

  runtime.clock.source = WAC_TIME_SOURCE_RTC;
  runtime.clock.warning = WAC_TIME_WARNING_CONNECTION;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  lv_obj_update_layout(lv_screen_active());
  assert(visible(indicator()) && lv_obj_get_y(indicator()) == 98);
  click(LV_SYMBOL_WARNING);
  assert(visible(label("Backup Clock Unavailable")) && visible(label("Check Connection.")));
  assert(!lv_obj_has_state(lv_obj_get_parent(label("DISMISS")), LV_STATE_DISABLED));
  /* The owner can acknowledge one warning and reveal a different one before
   * the heartbeat sees NONE. These are made-up snapshots of that real path. */
  click("DISMISS");
  assert(dismissal_count == 3 && dismissed_warning == WAC_TIME_WARNING_CONNECTION);
  assert(lv_obj_has_state(lv_obj_get_parent(label("DISMISS")), LV_STATE_DISABLED));
  runtime.clock.warning = WAC_TIME_WARNING_LOST_TIME;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert(visible(label("Backup Clock Lost Time")) && visible(label("Check Battery.")));
  assert(!lv_obj_has_state(lv_obj_get_parent(label("DISMISS")), LV_STATE_DISABLED));
  click("DISMISS");
  assert(dismissal_count == 4 && dismissed_warning == WAC_TIME_WARNING_LOST_TIME);
  click("DISMISS");
  assert(dismissal_count == 4);
  runtime.clock.warning = WAC_TIME_WARNING_CONNECTION;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert(visible(label("Backup Clock Unavailable")) && visible(label("Check Connection.")));
  assert(!lv_obj_has_state(lv_obj_get_parent(label("DISMISS")), LV_STATE_DISABLED));
  click("DISMISS");
  assert(dismissal_count == 5 && dismissed_warning == WAC_TIME_WARNING_CONNECTION);
  runtime.clock.warning = WAC_TIME_WARNING_NONE;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  runtime.clock.warning = WAC_TIME_WARNING_CONNECTION;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  click(LV_SYMBOL_WARNING);
  /* Opening settings closes the overlay without acknowledging the warning. */
  wac_settings_begin(lv_obj_get_parent(indicator()), &config);
  wac_ui_settings_navigate(WAC_SETTINGS_DEVICE);
  assert(!visible(overlay()) && !visible(indicator()));
  lv_obj_t *room = find_textarea(lv_screen_active());
  assert(room);
  lv_textarea_set_text(room, "Draft room");
  lv_obj_send_event(room, LV_EVENT_VALUE_CHANGED, NULL);
  runtime.clock.source = WAC_TIME_SOURCE_NETWORK;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert(!strcmp(lv_textarea_get_text(room), "Draft room"));
  assert(wac_ui_settings_view() == WAC_SETTINGS_DEVICE);
  assert(dismissal_count == 5);
  wac_ui_settings_close();
  assert(visible(indicator()) && !visible(overlay()));
  swipe(pointer, 600, 200, 300, 200);
  assert(!visible(indicator()));
  swipe(pointer, 300, 200, 600, 200);
  assert(visible(indicator()));

  config.night_shift_mode = DEVICE_CONFIG_NIGHT_ALWAYS_ON;
  /* Appearance is supplied by the owner even if settings suggest otherwise. */
  runtime.night_active = false;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert(lv_color_eq(lv_obj_get_style_text_color(indicator(), 0), lv_color_hex(0xefb965)));
  runtime.night_active = true;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert(lv_color_eq(lv_obj_get_style_text_color(indicator(), 0), lv_color_hex(0xd29191)));
  click(LV_SYMBOL_WARNING);
  assert(visible(overlay()));
  assert(lv_color_eq(lv_obj_get_style_bg_color(lv_obj_get_parent(label("Check Connection.")), 0),
                     lv_color_hex(0x261315)));

  /* Rebuilding the analog shell consumes an authoritative usable RTC snapshot. */
  runtime.clock.source = WAC_TIME_SOURCE_RTC;
  snprintf(config.default_clock_face, sizeof(config.default_clock_face), "analog");
  assert(app_ui_show_boot_screen(&config, &runtime, true) == ESP_OK);
  assert(visible(indicator()) && !visible(overlay()));
  lv_obj_update_layout(lv_screen_active());
  assert(lv_obj_get_y(indicator()) == 126);
  assert(visible(label("Wednesday")));
  runtime.clock.usable = false;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  lv_obj_update_layout(lv_screen_active());
  assert(find_visible_label(lv_screen_active(), "Time unavailable") && lv_obj_get_y(indicator()) == 56);
  assert(fake_nvs_calls(FAKE_SET) == 0);
}

static void test_background_message_ui(lv_indev_t *pointer, bool night_active)
{
  fprintf(stderr, "starting background message case night_active=%d\n", night_active);
  fake_nvs_reset();
  assert(device_config_init() == ESP_OK);
  device_config_t config;
  assert(device_config_copy(&config) == ESP_OK);
  fake_nvs_clear_activity();
  config.location_ready = true;
  snprintf(config.default_clock_face, sizeof(config.default_clock_face), "digital");
  app_runtime_state_t runtime = {0};
  runtime.clock.usable = true;
  runtime.clock.source = WAC_TIME_SOURCE_NETWORK;
  runtime.clock.utc_now = 1577862360;
  runtime.night_active = night_active;
  runtime.unread_message_count = 1;
  runtime.message = (app_message_snapshot_t){.available = true, .id = "displayed",
    .text = "Dinner is ready", .meta = "Alex"};
  message_submissions = 0;
  message_submission_result = ESP_OK;
  assert(app_ui_show_boot_screen(&config, &runtime, true) == ESP_OK);
  swipe(pointer, 300, 200, 600, 200);
  assert(visible(label("Dinner is ready")));
  swipe(pointer, 600, 200, 600, 200); /* Actual pointer tap. */
  assert(message_submissions == 1);
  assert(!strcmp(submitted_message, "displayed"));
  assert(!strcmp(submitted_device, config.device_id));
  assert(visible(label("Dismissing...")));
  swipe(pointer, 600, 200, 600, 200);
  assert(message_submissions == 1);
  /* A later arrival must not replace the message awaiting its result. */
  runtime.message = (app_message_snapshot_t){.available = true,
    .id = "new-arrival", .text = "New important message", .meta = "Sam"};
  runtime.clock.utc_now += 60;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert(visible(label("Dinner is ready")) && !visible(label("7:07")));
  app_ui_complete_message_acknowledgment(ESP_FAIL);
  assert(visible(label("Could not dismiss. Tap to retry.")));
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert(visible(label("Dinner is ready")));
  swipe(pointer, 600, 200, 600, 200);
  assert(message_submissions == 2 && !strcmp(submitted_message, "displayed"));
  app_ui_complete_message_acknowledgment(ESP_OK);
  assert(visible(label("7:07")));
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);

  /* Submission failure is shown, keeps the message, and permits retry. */
  swipe(pointer, 300, 200, 600, 200);
  message_submission_result = ESP_ERR_TIMEOUT;
  swipe(pointer, 600, 200, 600, 200);
  assert(message_submissions == 3);
  assert(visible(label("Could not dismiss. Tap to retry.")));
  message_submission_result = ESP_OK;
  swipe(pointer, 600, 200, 600, 200);
  assert(message_submissions == 4 && !strcmp(submitted_message, "new-arrival"));
  /* Leave and reopen before completion: returning must not revive old navigation. */
  swipe(pointer, 600, 200, 300, 200);
  assert(visible(label("7:07")));
  swipe(pointer, 300, 200, 600, 200);
  app_ui_complete_message_acknowledgment(ESP_OK);
  assert(visible(label("New important message")) && !visible(label("7:07")));

  /* A completion cannot close settings or discard an editor's draft. */
  swipe(pointer, 600, 200, 600, 200);
  assert(message_submissions == 5);
  swipe(pointer, 600, 200, 300, 200);
  swipe(pointer, 600, 200, 600, 400);
  assert(wac_ui_settings_view() == WAC_SETTINGS_ENTRY);
  click("Device Setup\nClock");
  click("Device Settings\nClock");
  lv_obj_t *room = find_textarea(lv_screen_active());
  assert(room);
  lv_textarea_set_text(room, "Message-waiting draft");
  lv_obj_send_event(room, LV_EVENT_VALUE_CHANGED, NULL);
  app_ui_complete_message_acknowledgment(ESP_OK);
  assert(wac_ui_settings_view() == WAC_SETTINGS_DEVICE);
  assert(!strcmp(lv_textarea_get_text(room), "Message-waiting draft"));
  wac_ui_settings_close();
  assert(fake_nvs_calls(FAKE_SET) == 0);
}

static void test_first_use_boot(void)
{
  fake_nvs_reset();
  assert(device_config_init() == ESP_OK);
  device_config_t config;
  assert(device_config_copy(&config) == ESP_OK);
  assert(!config.location_ready && !config.location[0] && !config.wifi_ready);
  fake_nvs_clear_activity();
  app_runtime_state_t runtime = {0};
  runtime.clock.warning = WAC_TIME_WARNING_LOST_TIME;
  unsigned submissions_before_boot = dismissal_count;
  assert(app_ui_show_boot_screen(&config, &runtime, true) == ESP_OK);
  assert(wac_ui_settings_view() == WAC_SETTINGS_DEVICE_SETUP);
  assert(visible(label("Device Setup")) && visible(label("Wi-Fi\nChoose a Network")));
  assert(!visible(indicator()) && !visible(overlay()));

  /* A usable network snapshot must not bypass first-use setup or clear the
   * pending warning. This is an illustrative heartbeat, not a network test. */
  runtime.clock.usable = true;
  runtime.clock.source = WAC_TIME_SOURCE_NETWORK;
  runtime.clock.utc_now = 1577862360;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert(wac_ui_settings_view() == WAC_SETTINGS_DEVICE_SETUP);
  assert(!visible(indicator()) && !visible(overlay()));
  assert(dismissal_count == submissions_before_boot);
  assert(runtime.clock.warning == WAC_TIME_WARNING_LOST_TIME);
  wac_settings_close(); /* Teardown before a separate simulated boot. */

  config.wifi_ready = true; /* Saved Wi-Fi is ready; location still uses defaults. */
  assert(app_ui_show_boot_screen(&config, &runtime, true) == ESP_OK);
  assert(wac_ui_settings_view() == WAC_SETTINGS_WEATHER);
  assert(visible(label("Weather Location")) && visible(label("City, State")));
  lv_obj_t *location = find_textarea(lv_screen_active());
  assert(location && !lv_textarea_get_text(location)[0]);
  assert(!visible(indicator()) && !visible(overlay()));
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert(wac_ui_settings_view() == WAC_SETTINGS_WEATHER);
  assert(!lv_textarea_get_text(location)[0]);
  assert(dismissal_count == submissions_before_boot);
  assert(runtime.clock.warning == WAC_TIME_WARNING_LOST_TIME);
  assert(fake_nvs_calls(FAKE_SET) == 0);
  wac_settings_close();
}

static void find_edge_indicators(lv_obj_t *root, const lv_image_dsc_t *source,
                                 lv_obj_t **objects, unsigned *count)
{
  if (lv_obj_check_type(root, &lv_image_class) && lv_image_get_src(root) == source) {
    assert(*count < 2);
    objects[(*count)++] = root;
  }
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
    find_edge_indicators(lv_obj_get_child(root, i), source, objects, count);
}

static void test_indicator_pulses(void)
{
  fake_nvs_reset();
  assert(device_config_init() == ESP_OK);
  device_config_t config;
  assert(device_config_copy(&config) == ESP_OK);
  config.location_ready = true;
  app_runtime_state_t runtime = {0};
  runtime.clock.usable = true;
  runtime.clock.source = WAC_TIME_SOURCE_NETWORK;
  runtime.clock.utc_now = 1577862360;
  runtime.unread_message_count = 1;
  runtime.has_important_message = true;
  runtime.message = (app_message_snapshot_t){.available = true,
    .id = "important", .text = "Please call home", .meta = "Alex"};
  /* Illustrative service snapshots exercise the ordinary one-second refresh;
   * they do not perform message polling or fetch real weather. */
  runtime.weather.valid = true;
  runtime.weather.has_conditions = true;
  runtime.weather.updated_at = runtime.clock.utc_now;
  strcpy(runtime.weather.icon_name, "cloudy");
  runtime.weather.conditions.alert = true;
  strcpy(runtime.weather.conditions.headline, "Strong winds");
  assert(app_ui_show_boot_screen(&config, &runtime, true) == ESP_OK);

  lv_obj_t *edges[4]; /* Both clock faces: messages first, weather second. */
  unsigned count = 0;
  find_edge_indicators(lv_screen_active(), &analog_edge_indicator, edges, &count);
  assert(count == 2);
  count = 0;
  find_edge_indicators(lv_screen_active(), &conditions_edge_indicator, edges + 2, &count);
  assert(count == 2);
  lv_opa_t previous[4], low[4], high[4];
  unsigned rises[4] = {0}, falls[4] = {0};
  for (unsigned i = 0; i < 4; ++i) {
    assert(!lv_obj_has_flag(edges[i], LV_OBJ_FLAG_HIDDEN));
    assert(lv_anim_get(edges[i], NULL) != NULL);
    previous[i] = low[i] = high[i] = lv_obj_get_style_opa(edges[i], 0);
    assert(previous[i] == 184);
  }
  for (unsigned step = 1; step <= 40; ++step) {
    lv_tick_inc(100);
    lv_timer_handler();
    for (unsigned i = 0; i < 4; ++i) {
      lv_opa_t opacity = lv_obj_get_style_opa(edges[i], 0);
      assert(opacity >= 184 && opacity <= 255);
      rises[i] += opacity > previous[i];
      falls[i] += opacity < previous[i];
      if (opacity < low[i]) low[i] = opacity;
      if (opacity > high[i]) high[i] = opacity;
      previous[i] = opacity;
    }
    if (step % 10 == 0) {
      assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
      for (unsigned i = 0; i < 4; ++i) {
        assert(lv_anim_get(edges[i], NULL) != NULL);
        assert(lv_obj_get_style_opa(edges[i], 0) == previous[i]);
      }
    }
  }
  for (unsigned i = 0; i < 4; ++i) {
    assert(low[i] == 184 && high[i] == 255);
    assert(rises[i] >= 16 && falls[i] >= 16); /* Two complete fade cycles. */
  }

  config.night_shift_mode = DEVICE_CONFIG_NIGHT_ALWAYS_ON;
  runtime.night_active = true;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  for (unsigned i = 0; i < 4; ++i) {
    assert(lv_obj_get_style_opa(edges[i], 0) == previous[i]);
    assert(lv_anim_get(edges[i], NULL) != NULL);
    assert(lv_color_eq(lv_obj_get_style_image_recolor(edges[i], 0),
      lv_color_hex(i < 2 ? 0xd9a0a0 : 0xec9682)));
    assert(lv_obj_get_style_image_recolor_opa(edges[i], 0) == (i < 2 ? 66 : LV_OPA_COVER));
  }

  runtime.has_important_message = false;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  for (unsigned i = 0; i < 2; ++i) {
    assert(!lv_obj_has_flag(edges[i], LV_OBJ_FLAG_HIDDEN));
    assert(lv_anim_get(edges[i], NULL) == NULL);
    assert(lv_obj_get_style_opa(edges[i], 0) == LV_OPA_80);
    assert(lv_color_eq(lv_obj_get_style_image_recolor(edges[i], 0), lv_color_hex(0xbf8888)));
    assert(lv_obj_get_style_image_recolor_opa(edges[i], 0) == 46);
  }
  lv_tick_inc(500);
  lv_timer_handler();
  for (unsigned i = 0; i < 2; ++i)
    assert(lv_obj_get_style_opa(edges[i], 0) == LV_OPA_80);

  config.night_shift_mode = DEVICE_CONFIG_NIGHT_OFF;
  runtime.night_active = false;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  for (unsigned i = 0; i < 2; ++i) {
    assert(lv_obj_get_style_opa(edges[i], 0) == LV_OPA_COVER);
    assert(lv_obj_get_style_image_recolor_opa(edges[i], 0) == LV_OPA_TRANSP);
  }
  for (unsigned i = 2; i < 4; ++i) {
    assert(lv_anim_get(edges[i], NULL) != NULL);
    assert(lv_color_eq(lv_obj_get_style_image_recolor(edges[i], 0), lv_color_hex(0xffb040)));
  }

  runtime.unread_message_count = 0;
  runtime.message.available = false;
  runtime.weather.conditions.alert = false;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  for (unsigned i = 0; i < 4; ++i) {
    assert(lv_obj_has_flag(edges[i], LV_OBJ_FLAG_HIDDEN));
    assert(lv_anim_get(edges[i], NULL) == NULL);
    previous[i] = lv_obj_get_style_opa(edges[i], 0);
  }
  lv_tick_inc(1000);
  lv_timer_handler();
  for (unsigned i = 0; i < 4; ++i)
    assert(lv_obj_get_style_opa(edges[i], 0) == previous[i]);

  runtime.unread_message_count = 1;
  runtime.has_important_message = true;
  runtime.message.available = true;
  runtime.weather.conditions.alert = true;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  for (unsigned i = 0; i < 4; ++i) {
    assert(!lv_obj_has_flag(edges[i], LV_OBJ_FLAG_HIDDEN));
    assert(lv_anim_get(edges[i], NULL) != NULL);
    assert(lv_obj_get_style_opa(edges[i], 0) == 184);
  }
  lv_tick_inc(500);
  lv_timer_handler();
  for (unsigned i = 0; i < 4; ++i)
    assert(lv_obj_get_style_opa(edges[i], 0) > 184);
}

int main(void)
{
  setenv("TZ", "UTC0", 1);
  tzset();
  lv_init();
  lv_display_t *display = lv_display_create(800, 800);
  static uint8_t buffer[800 * 32 * 4];
  lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);
  lv_indev_t *pointer = lv_indev_create();
  lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(pointer, read_pointer);
  test_provider_forecast_dates(pointer);
  test_normal_clock_surfaces(pointer);
  test_digital_night_layout();
  test_backup_clock_ui(pointer);
  test_background_message_ui(pointer, false);
  test_background_message_ui(pointer, true);
  test_first_use_boot();
  test_indicator_pulses();
  lv_indev_delete(pointer);
  lv_display_delete(display);
  lv_deinit();
  puts("Production clock UI checks passed: owner time, warning kinds, navigation, draft retention, dismissal errors/pending/acknowledgment, day/night styles, first-use boot routing, uninterrupted indicator pulses, provider forecast dates and calendar captions.");
  return 0;
}
#endif
