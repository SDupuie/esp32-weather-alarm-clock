#include "test_support.h"
#include "wac/ui/ui_private.h"
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

int main(void)
{
  lv_init();
  test_clock_time_font();
  lv_display_t *display = lv_display_create(800, 800);
  static uint8_t buffer[800 * 32 * 4];
  lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);
  test_credentials_keyboard();
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
