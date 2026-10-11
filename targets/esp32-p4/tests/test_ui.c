#ifndef WAC_TOUCH_TEST
#include "test_support.h"
#include "lvgl.h"
#include "assets/wac_icons.h"
#include "assets/wac_fonts.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* These are actual LVGL boxes and resolved styles. One-pixel image substitutes
 * establish stage selection, not artwork/raster parity or physical touch. */
static lv_obj_t *menu_label(lv_obj_t *root, const char *text)
{
  if (lv_obj_check_type(root, &lv_label_class)
      && !strcmp(lv_label_get_text(root), text)) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *found = menu_label(lv_obj_get_child(root, i), text);
    if (found) return found;
  }
  return NULL;
}

static lv_obj_t *find_menu_button(lv_obj_t *root, const char *title, const char *detail, bool title_font)
{
  if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), title)) {
    const lv_font_t *font = lv_obj_get_style_text_font(root, 0);
    /* Titles are 28px; a matching 24px subtitle belongs to another row. */
    if (!title_font || font->line_height == lv_font_montserrat_28.line_height) {
      lv_obj_t *button = root;
      while (button && !lv_obj_check_type(button, &lv_button_class)) button = lv_obj_get_parent(button);
      if (button && (!detail || menu_label(button, detail))) return button;
    }
  }
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *button = find_menu_button(lv_obj_get_child(root, i), title, detail, title_font);
    if (button) return button;
  }
  return NULL;
}

static lv_obj_t *menu_button(const char *title, const char *detail)
{
  lv_obj_t *button = find_menu_button(lv_screen_active(), title, detail, true);
  if (!button) button = find_menu_button(lv_screen_active(), title, detail, false);
  if (!button) fprintf(stderr, "Missing exact button %s\n", title);
  assert(button);
  return button;
}

static void menu_box(lv_obj_t *obj, int x, int y, int width, int height)
{
  assert(obj);
  lv_obj_update_layout(lv_screen_active());
  lv_area_t area;
  lv_obj_get_coords(obj, &area);
  if (area.x1 != x || area.y1 != y || (width >= 0 && lv_obj_get_width(obj) != width)
      || (height >= 0 && lv_obj_get_height(obj) != height))
    fprintf(stderr, "Menu box (%d,%d,%d,%d), expected (%d,%d,%d,%d)\n",
      area.x1, area.y1, (int)lv_obj_get_width(obj), (int)lv_obj_get_height(obj),
      x, y, width, height);
  assert(area.x1 == x && area.y1 == y);
  if (width >= 0) assert(lv_obj_get_width(obj) == width);
  if (height >= 0) assert(lv_obj_get_height(obj) == height);
}

static lv_obj_t *find_menu_back(lv_obj_t *root)
{
  if (lv_obj_check_type(root, &lv_button_class) && lv_obj_get_x(root) == 145
      && lv_obj_get_y(root) == 88) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *found = find_menu_back(lv_obj_get_child(root, i));
    if (found) return found;
  }
  return NULL;
}

static lv_obj_t *menu_back(lv_obj_t *root)
{
  lv_obj_update_layout(lv_screen_active());
  return find_menu_back(root);
}

static bool menu_visible(lv_obj_t *obj)
{
  if (!obj) return false;
  for (; obj; obj = lv_obj_get_parent(obj))
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return false;
  return true;
}

static lv_obj_t *menu_child_type(lv_obj_t *root, const lv_obj_class_t *type)
{
  if (lv_obj_check_type(root, type)) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *found = menu_child_type(lv_obj_get_child(root, i), type);
    if (found) return found;
  }
  return NULL;
}

static lv_obj_t *menu_field(const char *title)
{
  lv_obj_t *caption = menu_label(lv_screen_active(), title);
  assert(caption);
  lv_obj_t *input = menu_child_type(lv_obj_get_parent(caption), &lv_textarea_class);
  assert(input);
  return input;
}

static void menu_color(lv_obj_t *obj, uint32_t hex, bool background)
{
  assert(obj);
  lv_color_t actual = background ? lv_obj_get_style_bg_color(obj, 0)
                                : lv_obj_get_style_text_color(obj, 0);
  assert(lv_color_eq(actual, lv_color_hex(hex)));
}

static void menu_button_style(lv_obj_t *button, bool selected, bool night)
{
  assert(lv_obj_get_style_radius(button, 0) == 24);
  assert(lv_obj_get_style_border_width(button, 0) == 1);
  menu_color(button, selected ? (night ? 0x643437 : 0x245289)
                             : (night ? 0x321c20 : 0x17263b), true);
  assert(lv_color_eq(lv_obj_get_style_border_color(button, 0),
    lv_color_hex(selected ? (night ? 0xe1b6b6 : 0x93d1ff)
                          : (night ? 0x633737 : 0x253a57))));
  bool disabled = lv_obj_has_state(button, LV_STATE_DISABLED);
  lv_obj_add_state(button, LV_STATE_DISABLED);
  assert(lv_obj_get_style_opa(button, 0) == LV_OPA_40);
  if (!disabled) lv_obj_remove_state(button, LV_STATE_DISABLED);
}

static lv_obj_t *image_source(lv_obj_t *root, const lv_image_dsc_t *source)
{
  if (lv_obj_check_type(root, &lv_image_class) && lv_image_get_src(root) == source) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *image = image_source(lv_obj_get_child(root, i), source);
    if (image) return image;
  }
  return NULL;
}

static void menu_heading(const char *title, bool night)
{
  lv_obj_t *heading = menu_label(lv_screen_active(), title);
  lv_obj_update_layout(lv_screen_active());
  lv_obj_t *back = menu_back(lv_obj_get_parent(heading));
  menu_box(back, 145, 88, 88, 88);
  lv_obj_t *arrow = image_source(back, &wac_icon_chevron_left);
  menu_box(arrow, 173, 116, 32, 32);
  assert(lv_color_eq(lv_obj_get_style_image_recolor(arrow, 0),
                    lv_color_hex(night ? 0xecc0c0 : 0xf5f8ff)));
  menu_box(heading, 245, !strcmp(title, "Weather Location") ? 94 : 112, 320, -1);
  assert(lv_obj_get_style_text_font(heading, 0) == &lv_font_montserrat_36);
  assert(lv_obj_get_style_text_letter_space(heading, 0) == 1);
  assert(lv_obj_get_style_text_align(heading, 0) == LV_TEXT_ALIGN_CENTER);
  menu_color(heading, night ? 0xecc0c0 : 0xf5f8ff, false);
  menu_color(lv_obj_get_parent(heading), night ? 0x18090c : 0x07101c, true);
}

static void menu_row(const char *title, const char *detail, int y, bool entry, bool night)
{
  lv_obj_t *button = menu_button(title, detail);
  menu_box(button, 140, y, 520, 96);
  menu_button_style(button, false, night);
  lv_obj_t *title_label = menu_label(button, title);
  assert(lv_obj_get_style_text_font(title_label, 0) == &lv_font_montserrat_28);
  assert(lv_obj_get_style_text_align(title_label, 0) == LV_TEXT_ALIGN_LEFT);
  menu_color(title_label, night ? 0xecc0c0 : 0xf5f8ff, false);
  if (detail) {
    lv_obj_t *detail_label = menu_label(button, detail);
    assert(detail_label != title_label);
    const lv_font_t *font = lv_obj_get_style_text_font(detail_label, 0);
    assert(font->line_height == (entry ? lv_font_montserrat_24.line_height
                                      : lv_font_montserrat_28.line_height));
    assert(font->base_line == (entry ? lv_font_montserrat_24.base_line
                                    : lv_font_montserrat_28.base_line));
    if (entry) {
      lv_font_glyph_dsc_t glyph;
      assert(lv_font_get_glyph_dsc(font, &glyph, 'A', 'l'));
      assert(!glyph.is_placeholder && glyph.resolved_font == &lv_font_montserrat_24);
    }
    if (strstr(detail, "·")) {
      lv_font_glyph_dsc_t glyph;
      assert(lv_font_get_glyph_dsc(font, &glyph, 0x00b7, ' '));
      assert(!glyph.is_placeholder && glyph.adv_w > 0 && glyph.box_w > 0 && glyph.box_h > 0);
    }
    if (strstr(detail, "–")) {
      lv_font_glyph_dsc_t glyph;
      assert(lv_font_get_glyph_dsc(font, &glyph, 0x2013, 'F'));
      assert(!glyph.is_placeholder && glyph.adv_w > 0 && glyph.box_w > 0 && glyph.box_h > 0);
    }
    menu_color(detail_label, entry ? (night ? 0xc17a7a : 0xa6b6cf)
                                 : (night ? 0xe1b6b6 : 0x93d1ff), false);
    lv_area_t title_area, detail_area;
    lv_obj_get_coords(title_label, &title_area);
    lv_obj_get_coords(detail_label, &detail_area);
    if (entry) {
      assert(detail_area.x1 == title_area.x1 && detail_area.y1 > title_area.y2);
    } else {
      assert(detail_area.x1 > title_area.x1);
      assert(lv_obj_get_style_text_align(detail_label, 0) == LV_TEXT_ALIGN_RIGHT);
    }
  }
}

static lv_obj_t *menu_sized_child(lv_obj_t *root, int width, int height)
{
  if (lv_obj_get_width(root) == width && lv_obj_get_height(root) == height) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *found = menu_sized_child(lv_obj_get_child(root, i), width, height);
    if (found) return found;
  }
  return NULL;
}

static void menu_keyboard_assets(void)
{
  const lv_image_dsc_t *icons[] = {&wac_icon_keyboard_delete, &wac_icon_keyboard_shift};
  for (unsigned i = 0; i < sizeof(icons) / sizeof(*icons); ++i) {
    const lv_image_dsc_t *icon = icons[i];
    assert(icon->header.magic == LV_IMAGE_HEADER_MAGIC
           && icon->header.cf == LV_COLOR_FORMAT_A8);
    assert(icon->header.w == 24 && icon->header.h == 24 && icon->header.stride == 24);
    assert(icon->data_size == 24 * 24);
    unsigned clear = 0, covered = 0, softened = 0;
    for (uint32_t pixel = 0; pixel < icon->data_size; ++pixel) {
      clear += icon->data[pixel] == 0;
      covered += icon->data[pixel] == 255;
      softened += icon->data[pixel] > 0 && icon->data[pixel] < 255;
    }
    assert(clear && covered && softened);
  }
}

static void menu_keyboard(lv_obj_t *keyboard, bool night)
{
  menu_box(keyboard, 120, 390, 560, 280);
  assert(lv_obj_get_style_bg_opa(keyboard, 0) == LV_OPA_TRANSP);
  assert(lv_obj_get_style_pad_left(keyboard, 0) == 0
         && lv_obj_get_style_pad_right(keyboard, 0) == 0
         && lv_obj_get_style_pad_top(keyboard, 0) == 0
         && lv_obj_get_style_pad_bottom(keyboard, 0) == 0);
  assert(lv_obj_get_style_pad_row(keyboard, 0) == 8
         && lv_obj_get_style_pad_column(keyboard, 0) == 6);
  assert(lv_obj_get_style_radius(keyboard, LV_PART_ITEMS) == 14);
  assert(lv_obj_get_style_border_width(keyboard, LV_PART_ITEMS) == 1);
  assert(lv_obj_get_style_text_font(keyboard, LV_PART_ITEMS) == &lv_font_montserrat_24);
  assert(lv_color_eq(lv_obj_get_style_text_color(keyboard, LV_PART_ITEMS),
    lv_color_hex(night ? 0xecc0c0 : 0xf5f8ff)));
  assert(lv_color_eq(lv_obj_get_style_bg_color(keyboard, LV_PART_ITEMS),
    lv_color_hex(night ? 0x321c20 : 0x17263b)));
  assert(lv_color_eq(lv_obj_get_style_border_color(keyboard, LV_PART_ITEMS),
    lv_color_hex(night ? 0x633737 : 0x253a57)));
}

static void menu_field_geometry(const char *title, int y, bool night)
{
  lv_obj_t *caption = menu_label(lv_screen_active(), title);
  lv_obj_t *input = menu_field(title);
  lv_obj_t *block = lv_obj_get_parent(caption);
  menu_box(block, 140, y, 520, 140);
  menu_box(caption, 140, y, -1, -1);
  menu_box(input, 140, y + 44, 520, 96);
  assert(lv_obj_get_style_text_font(caption, 0) == &lv_font_montserrat_28);
  assert(lv_obj_get_style_text_align(caption, 0) == LV_TEXT_ALIGN_LEFT);
  menu_color(caption, night ? 0xc17a7a : 0xa6b6cf, false);
  assert(lv_obj_get_style_text_font(input, 0) == &lv_font_montserrat_28);
  assert(lv_obj_get_style_radius(input, 0) == 24
         && lv_obj_get_style_border_width(input, 0) == 1);
  assert(lv_obj_get_style_pad_left(input, 0) == 20
         && lv_obj_get_style_pad_right(input, 0) == 20);
  menu_color(input, night ? 0x321c20 : 0x17263b, true);
  menu_color(input, night ? 0xecc0c0 : 0xf5f8ff, false);
  assert(lv_color_eq(lv_obj_get_style_border_color(input, 0),
    lv_color_hex(night ? 0x633737 : 0x253a57)));
  const char *placeholder = lv_textarea_get_placeholder_text(input);
  assert(placeholder && !strcmp(placeholder, title));
}

static void menu_switch(const char *title, bool on, bool night)
{
  lv_obj_t *button = menu_button(title, on ? "On" : "Off");
  lv_obj_update_layout(button);
  lv_obj_t *track = menu_sized_child(button, 100, 56);
  assert(track && lv_obj_get_style_radius(track, 0) == 28);
  lv_obj_t *knob = menu_sized_child(track, 44, 44);
  assert(knob && lv_obj_get_parent(knob) == track);
  lv_area_t track_area, knob_area;
  lv_obj_get_coords(track, &track_area);
  lv_obj_get_coords(knob, &knob_area);
  /* CSS absolute offsets begin inside the track's one-pixel border. */
  assert(knob_area.x1 - track_area.x1 == (on ? 50 : 6)
         && knob_area.y1 - track_area.y1 == 6);
  menu_color(track, on ? (night ? 0x643437 : 0x245289)
                      : (night ? 0x633737 : 0x253a57), true);
  assert(lv_obj_get_style_border_width(track, 0) == 1);
  assert(lv_color_eq(lv_obj_get_style_border_color(track, 0),
    lv_color_hex(on ? (night ? 0xe1b6b6 : 0x93d1ff)
                   : (night ? 0xc17a7a : 0xa6b6cf))));
  menu_color(knob, night ? 0xecc0c0 : 0xf5f8ff, true);
}
#endif

#ifdef WAC_TOUCH_TEST
#include "test_support.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define ESP_LOGE(tag, ...) do { (void)(tag); if (0) printf(__VA_ARGS__); } while (0)
#include "touch_startup.inc"

/* Controlled read results model the retained GT911 cache. They do not model
 * the board's I2C interrupt ordering or establish physical recovery. */
static esp_lcd_touch_t touch = {.data.lock = portMUX_INITIALIZER_UNLOCKED};
static esp_err_t read_result = ESP_OK;
static unsigned reads, data_gets;
static lv_display_t *display;
static lv_indev_t *pointer;
static unsigned pressed, lost, clicked;

esp_err_t esp_lcd_touch_read_data(esp_lcd_touch_handle_t handle)
{
  assert(handle == &touch);
  ++reads;
  return read_result;
}

esp_err_t esp_lcd_touch_get_data(esp_lcd_touch_handle_t handle,
  esp_lcd_touch_point_data_t *points, uint8_t *count, uint8_t maximum)
{
  assert(handle == &touch && points && count && maximum);
  ++data_gets;
  portENTER_CRITICAL(&touch.data.lock);
  *count = touch.data.points < maximum ? touch.data.points : maximum;
  memcpy(points, touch.data.coords, *count * sizeof(*points));
  portEXIT_CRITICAL(&touch.data.lock);
  return ESP_OK;
}

static void read_pointer(lv_indev_t *input, lv_indev_data_t *data)
{
  assert(input == pointer);
  esp_lcd_touch_point_data_t point = {0};
  uint8_t count = 0;
  esp_err_t result = read_touch(&touch, &point, &count, 1, NULL);
  /* The adapter's documented custom-read contract supplies released state on
   * an error. LVGL itself decides whether that release can generate a click. */
  data->state = result == ESP_OK && count ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  if (result == ESP_OK && count) data->point = (lv_point_t){point.x, point.y};
}

static void flush(lv_display_t *screen, const lv_area_t *area, uint8_t *pixels)
{
  (void)area; (void)pixels;
  lv_display_flush_ready(screen);
}

/* A real LVGL pointer/button verifies press cancellation. Board startup,
 * task ordering and callback registration require separate firmware checks. */
static void create_pointer(void)
{
  display = lv_display_create(800, 800);
  static uint8_t buffer[800 * 32 * 4];
  lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);
  pointer = lv_indev_create();
  lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(pointer, read_pointer);
}

static void test_read_results(void)
{
  const esp_lcd_touch_point_data_t expected[] = {
    {.track_id = 2, .x = 101, .y = 202, .strength = 9},
    {.track_id = 7, .x = 303, .y = 404, .strength = 12},
  };
  esp_lcd_touch_point_data_t points[2] = {0};
  uint8_t count = 0;
  touch.data.points = 2;
  memcpy(touch.data.coords, expected, sizeof(expected));
  assert(read_touch(&touch, points, &count, 2, NULL) == ESP_OK);
  assert(count == 2 && touch.data.points == 2 && !memcmp(points, expected, sizeof(expected)));
  /* No new data keeps a held touch, as it does in the retained driver. */
  assert(read_touch(&touch, points, &count, 2, NULL) == ESP_OK);
  assert(count == 2 && touch.data.points == 2 && !memcmp(points, expected, sizeof(expected)));
  assert(read_touch(&touch, points, &count, 1, NULL) == ESP_OK && count == 1);
  assert(touch.data.points == 2 && !memcmp(points, expected, sizeof(*points)));
  const esp_err_t errors[] = {ESP_FAIL, ESP_ERR_TIMEOUT, ESP_ERR_INVALID_STATE,
    ESP_ERR_INVALID_ARG, ESP_ERR_NO_MEM, ESP_ERR_INVALID_RESPONSE, -129};
  for (unsigned i = 0; i < sizeof(errors) / sizeof(*errors); ++i) {
    read_result = errors[i];
    touch.data.points = 2;
    count = 255;
    unsigned previous_gets = data_gets, previous_reads = reads;
    assert(read_touch(&touch, points, &count, 2, NULL) == errors[i]);
    assert(count == 0 && touch.data.points == 0 && data_gets == previous_gets && reads == previous_reads + 1);
    assert(!memcmp(touch.data.coords, expected, sizeof(expected)));
    read_result = ESP_OK;
    assert(read_touch(&touch, points, &count, 2, NULL) == ESP_OK && count == 0);
    assert(touch.data.points == 0); /* A not-ready poll cannot replay old points. */
  }
}

static void button_event(lv_event_t *event)
{
  switch (lv_event_get_code(event)) {
    case LV_EVENT_PRESSED: ++pressed; break;
    case LV_EVENT_PRESS_LOST: ++lost; break;
    case LV_EVENT_CLICKED: ++clicked; break;
    default: break;
  }
}

static void test_interrupted_press(void)
{
  lv_obj_t *button = lv_button_create(lv_screen_active());
  lv_obj_set_size(button, 120, 70);
  lv_obj_center(button);
  lv_obj_add_event_cb(button, button_event, LV_EVENT_ALL, NULL);
  lv_obj_update_layout(lv_screen_active());
  touch.data.coords[0] = (esp_lcd_touch_point_data_t){.x = 400, .y = 400, .strength = 10};
  touch.data.points = 1;
  lv_indev_read(pointer);
  assert(pressed == 1 && lv_obj_has_state(button, LV_STATE_PRESSED));
  lv_tick_inc(30);
  lv_indev_read(pointer); /* Held points remain pressed. */
  assert(pressed == 1 && lost == 0 && clicked == 0 && touch.data.points == 1);
  read_result = ESP_ERR_INVALID_STATE;
  lv_indev_read(pointer);
  assert(lost == 1 && clicked == 0 && !lv_obj_has_state(button, LV_STATE_PRESSED));
  assert(touch.data.points == 0);
  read_result = ESP_OK;
  lv_indev_read(pointer); /* Successful no-new-data poll after the failure. */
  assert(pressed == 1 && clicked == 0 && lost == 1);
  touch.data.points = 1;
  lv_tick_inc(30);
  lv_indev_read(pointer);
  assert(pressed == 2 && lv_obj_has_state(button, LV_STATE_PRESSED));
  touch.data.points = 0; /* Fresh real release after the new press. */
  lv_tick_inc(30);
  lv_indev_read(pointer);
  assert(clicked == 1 && lost == 1 && !lv_obj_has_state(button, LV_STATE_PRESSED));
}

int main(void)
{
  lv_init();
  create_pointer();
  test_read_results();
  test_interrupted_press();
  lv_indev_delete(pointer);
  lv_display_delete(display);
  lv_deinit();
  puts("Production touch callback checks passed: held points, returned read errors, cache clearing, interrupted press cancellation and fresh click recovery.");
  return 0;
}

#elif !defined(WAC_UI_SHELL_TEST)
#include "test_support.h"
#include "ui/ui_private.h"
#include "connectivity.h"
#include "location_lookup.h"
#include "assets/lv_font_clock_time_144.h"
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"

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
const lv_image_dsc_t round_stage_night = {
  .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_ARGB8888,
    .w = 1, .h = 1, .stride = 4},
  .data_size = sizeof(background_pixel), .data = background_pixel
};
static wac_settings_view_t view;
static unsigned restart_count, task_count, lookup_count;
static bool fail_task, wifi_connected = true;
static connectivity_wifi_state_t wifi_state = CONNECTIVITY_WIFI_CONNECTED;
static const char *lookup_timezone = "America/New_York";
static esp_err_t lookup_result = ESP_OK;
/* Controlled owner results exercise actual LVGL scan presentation, not radio traffic. */
static connectivity_scan_result_t scan_results[10];
static size_t scan_result_count;
static esp_err_t scan_result_error;
static unsigned scan_count;
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
  return err == ESP_ERR_INVALID_ARG ? "ESP_ERR_INVALID_ARG"
    : err == ESP_ERR_TIMEOUT ? "ESP_ERR_TIMEOUT" : "ESP_FAIL";
}
esp_err_t bsp_display_brightness_set(int percent)
{ (void)percent; assert(!"Settings drafts must not set the backlight"); return ESP_FAIL; }
bool connectivity_is_wifi_connected(void) { return wifi_connected; }
connectivity_wifi_state_t connectivity_get_wifi_state(void)
{ return wifi_connected ? wifi_state : CONNECTIVITY_WIFI_DISCONNECTED; }
esp_err_t connectivity_scan_networks(connectivity_scan_result_t *networks,
                                    size_t capacity, size_t *count)
{
  ++scan_count;
  assert(capacity >= scan_result_count);
  memcpy(networks, scan_results, scan_result_count * sizeof(*networks));
  *count = scan_result_count;
  return scan_result_error;
}
esp_err_t location_lookup_resolve(const char *query, location_lookup_result_t *result)
{
  ++lookup_count;
  snprintf(last_query, sizeof(last_query), "%s", query);
  if (lookup_result != ESP_OK) return lookup_result;
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
  lookup_result = ESP_OK;
  wifi_state = CONNECTIVITY_WIFI_CONNECTED;
  scan_result_error = ESP_OK;
  scan_result_count = 1;
  memset(scan_results, 0, sizeof(scan_results));
  strcpy(scan_results[0].ssid, "Home network");
  scan_results[0].requires_password = true;
  scan_results[0].rssi = -42;
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
  lv_obj_t *button = find_menu_button(lv_screen_active(), prefix, NULL, true);
  if (!button) button = find_button(lv_screen_active(), prefix);
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

static lv_indev_data_t pointer_data;
static lv_indev_t *pointer;
static void read_pointer(lv_indev_t *indev, lv_indev_data_t *data)
{ (void)indev; *data = pointer_data; }

static void press(lv_obj_t *obj)
{
  assert(obj);
  lv_obj_update_layout(lv_screen_active());
  lv_area_t area;
  lv_obj_get_coords(obj, &area);
  pointer_data.point = (lv_point_t){(area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2};
  pointer_data.state = LV_INDEV_STATE_PRESSED;
  lv_indev_read(pointer);
}

static void release(void)
{
  lv_tick_inc(30);
  pointer_data.state = LV_INDEV_STATE_RELEASED;
  lv_indev_read(pointer);
}

static void tap(lv_obj_t *obj) { press(obj); release(); }

static lv_obj_t *find_box(lv_obj_t *root, const lv_obj_class_t *type,
                          int x, int y, int width, int height, const char *text)
{
  lv_area_t area;
  lv_obj_get_coords(root, &area);
  if (lv_obj_check_type(root, type) && area.x1 == x && area.y1 == y
      && lv_obj_get_width(root) == width && (height < 0 || lv_obj_get_height(root) == height)
      && (!text || (menu_visible(root) && !strcmp(lv_label_get_text(root), text)))) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *obj = find_box(lv_obj_get_child(root, i), type, x, y, width, height, text);
    if (obj) return obj;
  }
  return NULL;
}

static lv_obj_t *box_button(int x, int y, int width, int height)
{
  lv_obj_update_layout(lv_screen_active());
  lv_obj_t *button = find_box(lv_screen_active(), &lv_button_class, x, y, width, height, NULL);
  assert(button);
  return button;
}

static lv_obj_t *box_label(int x, int y, int width)
{
  lv_obj_update_layout(lv_screen_active());
  lv_obj_t *label = find_box(lv_screen_active(), &lv_label_class, x, y, width, -1, NULL);
  if (!label) fprintf(stderr, "Missing label at %d,%d width%d in view%d\n", x, y, width, view);
  assert(label);
  return label;
}

static void picker_set(int hour, int minute, bool twelve)
{
  assert(hour >= 0 && hour < 24 && minute >= 0 && minute < 60);
  int shown_minute = atoi(lv_label_get_text(box_label(450, 355, 170)));
  int minute_delta = (minute - shown_minute + 60) % 60;
  assert(minute_delta % 5 == 0);
  unsigned steps = minute_delta / 5;
  for (unsigned i = 0; i < steps; ++i) tap(box_button(470, 240, 130, 96));
  /* Minute arrows can carry into another hour, so set the hour afterward. */
  int shown_hour = atoi(lv_label_get_text(box_label(180, 355, 170)));
  int target_hour = twelve ? (hour % 12 ? hour % 12 : 12) : hour;
  steps = (target_hour - shown_hour + (twelve ? 12 : 24)) % (twelve ? 12 : 24);
  for (unsigned i = 0; i < steps; ++i) tap(box_button(200, 240, 130, 96));
  if (twelve) tap(menu_button(hour < 12 ? "AM" : "PM", NULL));
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

static void header_back(void)
{
  lv_obj_update_layout(lv_screen_active());
  lv_obj_t *button = menu_back(lv_screen_active());
  assert(button && !lv_obj_has_state(button, LV_STATE_DISABLED));
  tap(button);
}

static lv_obj_t *operation_label(const char *text, int x, int y, int width,
                                const lv_font_t *base, bool muted, bool night)
{
  lv_obj_update_layout(lv_screen_active());
  lv_obj_t *obj = find_box(lv_screen_active(), &lv_label_class, x, y, width, -1, text);
  if (!obj) fprintf(stderr, "Missing literal text %s at %d,%d in view%d\n", text, x, y, view);
  assert(menu_visible(obj) && !strcmp(lv_label_get_text(obj), text));
  menu_box(obj, x, y, width, -1);
  const lv_font_t *font = lv_obj_get_style_text_font(obj, 0);
  assert(font->line_height == base->line_height && font->base_line == base->base_line);
  lv_font_glyph_dsc_t glyph;
  assert(lv_font_get_glyph_dsc(font, &glyph, 'A', 0));
  assert(!glyph.is_placeholder && glyph.resolved_font == base);
  const struct { const char *utf8; uint32_t codepoint; } punctuation[] = {
    {"·", 0x00b7}, {"’", 0x2019}, {"…", 0x2026}
  };
  for (unsigned i = 0; i < sizeof(punctuation) / sizeof(*punctuation); ++i) {
    if (!strstr(text, punctuation[i].utf8)) continue;
    assert(lv_font_get_glyph_dsc(font, &glyph, punctuation[i].codepoint, 0));
    assert(!glyph.is_placeholder && glyph.adv_w && glyph.box_w && glyph.box_h);
  }
  menu_color(obj, muted ? (night ? 0xc17a7a : 0xa6b6cf)
                       : (night ? 0xecc0c0 : 0xf5f8ff), false);
  return obj;
}

static void operation_assets(void)
{
  const lv_image_dsc_t *icons[] = {&wac_icon_wifi, &wac_icon_wifi_high,
    &wac_icon_wifi_low, &wac_icon_refresh, &wac_icon_chevron_up, &wac_icon_chevron_down,
    &wac_icon_chevron_left, &wac_icon_chevron_right, &wac_icon_check,
    &wac_icon_plus, &wac_icon_minus, &wac_icon_volume_2, &wac_icon_wifi_mark,
    &wac_icon_wifi_off_mark, &wac_icon_circle_check_mark};
  for (unsigned i = 0; i < sizeof(icons) / sizeof(*icons); ++i) {
    unsigned size = i < 12 ? 32 : 80;
    const lv_image_dsc_t *icon = icons[i];
    assert(icon->header.magic == LV_IMAGE_HEADER_MAGIC
           && icon->header.cf == LV_COLOR_FORMAT_A8);
    assert(icon->header.w == size && icon->header.h == size && icon->header.stride == size);
    assert(icon->data_size == size * size);
    unsigned clear = 0, covered = 0, softened = 0;
    for (uint32_t pixel = 0; pixel < icon->data_size; ++pixel) {
      clear += icon->data[pixel] == 0;
      covered += icon->data[pixel] == 255;
      softened += icon->data[pixel] > 0 && icon->data[pixel] < 255;
    }
    assert(clear && covered && softened);
  }
}

static void operation_mark(const lv_image_dsc_t *source, bool night)
{
  lv_obj_t *image = image_source(lv_screen_active(), source);
  assert(image && menu_visible(image));
  menu_box(lv_obj_get_parent(image), 352, 254, 96, 96);
  menu_box(image, 360, 262, 80, 80);
  assert(lv_obj_get_style_image_recolor_opa(image, 0) == LV_OPA_COVER);
  assert(lv_color_eq(lv_obj_get_style_image_recolor(image, 0),
                    lv_color_hex(night ? 0xe1b6b6 : 0x93d1ff)));
}

static void wifi_controls(bool scanning, bool night)
{
  menu_heading("Wi-Fi", night);
  lv_obj_t *manual = menu_button("Manual Network", NULL);
  menu_box(manual, 140, 544, 520, 96);
  menu_button_style(manual, false, night);
  assert(!lv_obj_has_state(manual, LV_STATE_DISABLED));
  lv_obj_t *refresh = menu_button("Refresh", NULL);
  menu_box(refresh, 270, 664, 260, 80);
  assert(lv_obj_get_style_bg_opa(refresh, 0) == LV_OPA_TRANSP);
  assert(lv_obj_get_style_border_width(refresh, 0) == 0);
  assert(lv_obj_has_state(refresh, LV_STATE_DISABLED) == scanning);
  assert(lv_obj_get_style_opa(refresh, 0) == (scanning ? LV_OPA_40 : LV_OPA_COVER));
  lv_obj_t *caption = menu_label(refresh, "Refresh");
  assert(lv_obj_get_style_text_font(caption, 0) == &lv_font_montserrat_28);
  menu_color(caption, night ? 0xe1b6b6 : 0x93d1ff, false);
  lv_obj_t *icon = image_source(refresh, &wac_icon_refresh);
  assert(icon && lv_obj_get_width(icon) == 32 && lv_obj_get_height(icon) == 32);
  lv_area_t icon_area, caption_area;
  lv_obj_get_coords(icon, &icon_area);
  lv_obj_get_coords(caption, &caption_area);
  assert(caption_area.x1 - icon_area.x2 - 1 == 14);
  assert(lv_color_eq(lv_obj_get_style_image_recolor(icon, 0),
                    lv_color_hex(night ? 0xe1b6b6 : 0x93d1ff)));
  assert(!lv_obj_has_state(menu_back(lv_screen_active()), LV_STATE_DISABLED));
  assert(!menu_label(lv_screen_active(), "Done"));
}

static void wifi_phase(const char *title, const char *detail, bool scanning, bool night)
{
  wifi_controls(scanning, night);
  operation_mark(scanning ? &wac_icon_wifi_mark : &wac_icon_wifi_off_mark, night);
  operation_label(title, 140, 380, 520, &lv_font_montserrat_34, false, night);
  operation_label(detail, 150, 444, 500, &lv_font_montserrat_26, true, night);
  assert(!menu_label(lv_screen_active(), "Choose a Network"));
  assert(!menu_label(lv_screen_active(), "Home network"));
}

static void test_wifi_scan_states_and_retained_drafts(void)
{
  for (unsigned night = 0; night < 2; ++night) {
    fresh_editor();
    wac_settings_apply_night(night);
    click("Device Setup");
    click("Device Settings");
    field(0, "Pending room");
    click("Done");
    tap(menu_button("Wi-Fi", NULL));
    assert(task_count == 1 && fake_nvs_calls(FAKE_SET) == 0);
    wifi_phase("Scanning for Networks…", "Looking for nearby Wi-Fi networks.", true, night);
    lv_obj_t *pending_title = menu_label(lv_screen_active(), "Scanning for Networks…");
    lv_obj_t *pending_icon = image_source(lv_screen_active(), &wac_icon_wifi_mark);
    lv_obj_t *pending_refresh = menu_button("Refresh", NULL);
    wac_settings_apply_night(!night);
    wifi_phase("Scanning for Networks…", "Looking for nearby Wi-Fi networks.", true, !night);
    assert(menu_label(lv_screen_active(), "Scanning for Networks…") == pending_title);
    assert(image_source(lv_screen_active(), &wac_icon_wifi_mark) == pending_icon);
    assert(menu_button("Refresh", NULL) == pending_refresh && task_count == 1);
    wac_settings_apply_night(night);
    unsigned scans = scan_count;
    lv_obj_send_event(menu_button("Refresh", NULL), LV_EVENT_CLICKED, NULL);
    assert(task_count == 1 && scan_count == scans);
    run_task();
    wifi_controls(false, night);
    operation_label("Choose a Network", 150, 175, 500, &lv_font_montserrat_28, true, night);
    menu_row("Home network", "Secured · -42 dBm", 220, true, night);
    tap(menu_button("Refresh", NULL));
    wifi_phase("Scanning for Networks…", "Looking for nearby Wi-Fi networks.", true, night);
    scan_result_count = 0;
    run_task();
    wifi_phase("No Networks Found", "Refresh the scan or enter\na network manually.", false, night);
    tap(menu_button("Refresh", NULL));
    scan_result_error = ESP_ERR_TIMEOUT;
    run_task();
    wifi_phase("Couldn’t Scan for Networks", "Refresh the scan or enter\na network manually.", false, night);
    fail_task = true;
    tap(menu_button("Refresh", NULL));
    assert(task_count == 0);
    wifi_phase("Couldn’t Scan for Networks", "Refresh the scan or enter\na network manually.", false, night);
    tap(menu_button("Manual Network", NULL));
    field(0, "Manual draft");
    field(1, "retained-password");
    click("Back");
    header_back();
    assert(view == WAC_SETTINGS_DEVICE_SETUP);
    assert(menu_button("Device Settings", "Pending room"));
    assert(menu_button("Wi-Fi", "Manual draft · Not Connected"));
    assert(fake_nvs_calls(FAKE_SET) == 0);
    click("Cancel");
  }
  wac_settings_apply_night(false);

  /* Manual entry remains usable during a real pending scan. Its old result
   * must not replace the current editor or its changed values. */
  fresh_editor();
  click("Device Setup");
  click("Wi-Fi");
  tap(menu_button("Manual Network", NULL));
  field(0, "New draft during scan");
  field(1, "new-password");
  run_task();
  assert(view == WAC_SETTINGS_CREDENTIALS);
  assert(!strcmp(lv_textarea_get_text(menu_field("Network Name")), "New draft during scan"));
  assert(!strcmp(lv_textarea_get_text(menu_field("Password")), "new-password"));
  assert(fake_nvs_calls(FAKE_SET) == 0);
  click("Back");
  header_back();
  click("Cancel");

  /* Returning to Device Setup during a scan keeps its existing draft. */
  fresh_editor();
  click("Device Setup");
  click("Device Settings");
  field(0, "Room retained through Back");
  click("Done");
  click("Wi-Fi");
  header_back();
  assert(view == WAC_SETTINGS_DEVICE_SETUP && task_count == 1);
  run_task();
  assert(view == WAC_SETTINGS_DEVICE_SETUP);
  assert(menu_button("Device Settings", "Room retained through Back"));
  assert(menu_button("Wi-Fi", "Not Connected"));
  assert(fake_nvs_calls(FAKE_SET) == 0);
  click("Cancel");

  /* A completion from a closed editor cannot navigate the reopened editor. */
  fresh_editor();
  click("Device Setup");
  click("Wi-Fi");
  wac_ui_settings_close();
  open_editor();
  click("Device Setup");
  click("Device Settings");
  field(0, "Newer room");
  run_task();
  assert(view == WAC_SETTINGS_DEVICE);
  assert(!strcmp(lv_textarea_get_text(menu_field("Room Name")), "Newer room"));
  assert(fake_nvs_calls(FAKE_SET) == 0);
  click("Done");
  click("Cancel");
}

static void test_wifi_scroll_and_actual_network_content(void)
{
  static const int8_t strengths[] = {-42, -50, -51, -65, -66, -72};
  fresh_editor();
  scan_result_count = sizeof(strengths) / sizeof(*strengths);
  for (size_t i = 0; i < scan_result_count; ++i) {
    snprintf(scan_results[i].ssid, sizeof(scan_results[i].ssid), "Actual network %zu", i + 1);
    scan_results[i].rssi = strengths[i];
    scan_results[i].requires_password = i % 2 == 0;
  }
  click("Device Setup");
  click("Wi-Fi");
  run_task();
  wifi_controls(false, false);
  lv_obj_t *list = lv_obj_get_parent(menu_button("Actual network 1", NULL));
  menu_box(list, 140, 220, 520, 312);
  assert(lv_obj_get_style_bg_opa(list, 0) == LV_OPA_TRANSP
         && lv_obj_get_style_border_width(list, 0) == 0);
  assert(lv_obj_has_flag(list, LV_OBJ_FLAG_SCROLLABLE));
  assert(lv_obj_get_scroll_dir(list) == LV_DIR_VER);
  assert(lv_obj_get_child_count(list) == scan_result_count);
  for (size_t i = 0; i < scan_result_count; ++i) {
    char name[32], detail[48];
    snprintf(name, sizeof(name), "Actual network %zu", i + 1);
    snprintf(detail, sizeof(detail), "%s · %d dBm", i % 2 == 0 ? "Secured" : "Open", strengths[i]);
    lv_obj_t *row = menu_button(name, detail);
    assert(lv_obj_get_parent(row) == list);
    menu_row(name, detail, 220 + (int)i * 108, true, false);
    assert(lv_obj_get_style_text_align(menu_label(row, name), 0) == LV_TEXT_ALIGN_LEFT);
    assert(lv_label_get_long_mode(menu_label(row, name)) == LV_LABEL_LONG_DOT);
    const lv_image_dsc_t *source = strengths[i] >= -50 ? &wac_icon_wifi
      : strengths[i] >= -65 ? &wac_icon_wifi_high : &wac_icon_wifi_low;
    lv_obj_t *icon = image_source(row, source);
    assert(icon && lv_obj_get_width(icon) == 32 && lv_obj_get_height(icon) == 32);
    lv_area_t row_area, icon_area;
    lv_obj_get_coords(row, &row_area);
    lv_obj_get_coords(icon, &icon_area);
    assert(icon_area.x2 == row_area.x2 - 21 && icon_area.y1 == row_area.y1 + 32);
    assert(lv_color_eq(lv_obj_get_style_image_recolor(icon, 0), lv_color_hex(0xf5f8ff)));
  }
  /* LVGL clips extra rows inside the three-row region without dropping data.
   * Scrolling makes the sixth returned network a real pointer target. */
  pointer_data.point = (lv_point_t){400, 490};
  pointer_data.state = LV_INDEV_STATE_PRESSED;
  lv_indev_read(pointer);
  for (unsigned step = 1; step <= 8; ++step) {
    lv_tick_inc(16);
    pointer_data.point.y = 490 - (int)step * 30;
    lv_indev_read(pointer);
  }
  release();
  assert(view == WAC_SETTINGS_WIFI && lv_obj_get_scroll_y(list) > 0);
  assert(lv_obj_get_child_count(list) == scan_result_count);
  lv_obj_scroll_to_y(list, 3 * 108, LV_ANIM_OFF);
  lv_obj_update_layout(lv_screen_active());
  assert(lv_obj_get_scroll_y(list) == 3 * 108);
  lv_obj_t *last = menu_button("Actual network 6", "Open · -72 dBm");
  menu_box(last, 140, 436, 520, 96);
  tap(last);
  assert(view == WAC_SETTINGS_CREDENTIALS);
  assert(!strcmp(lv_textarea_get_text(menu_field("Network Name")), "Actual network 6"));
  assert(menu_visible(menu_label(lv_screen_active(), "Open network. No password is required.")));
  field(1, "open-draft-password");
  click("Back");
  list = lv_obj_get_parent(menu_button("Actual network 1", NULL));
  lv_obj_scroll_to_y(list, 3 * 108, LV_ANIM_OFF);
  lv_obj_update_layout(lv_screen_active());
  tap(menu_button("Actual network 6", NULL));
  assert(!strcmp(lv_textarea_get_text(menu_field("Password")), "open-draft-password"));
  click("Back");
  tap(menu_button("Actual network 1", NULL));
  assert(!lv_textarea_get_text(menu_field("Password"))[0]);
  assert(menu_visible(menu_label(lv_screen_active(), "Enter the password to join this network.")));
  click("Back");
  header_back();
  click("Cancel");
  assert(task_count == 0 && fake_nvs_calls(FAKE_SET) == 0);
}

static void weather_pending(bool lookup, bool night)
{
  menu_heading("Weather Location", night);
  menu_field_geometry("City, State", 212, night);
  menu_row("Units", "Metric", 416, false, night);
  lv_obj_t *input = menu_field("City, State");
  lv_obj_t *units = menu_button("Units", NULL);
  lv_obj_t *cancel = menu_button("Cancel", NULL);
  lv_obj_t *save = menu_button(lookup ? "Checking…" : "Save", NULL);
  menu_box(cancel, 178, 646, 206, 88);
  menu_box(save, 412, 646, 210, 88);
  const lv_font_t *save_font = lv_obj_get_style_text_font(menu_label(save, lookup ? "Checking…" : "Save"), 0);
  assert(save_font->line_height == lv_font_montserrat_28.line_height
         && save_font->base_line == lv_font_montserrat_28.base_line);
  lv_font_glyph_dsc_t glyph;
  assert(lv_font_get_glyph_dsc(save_font, &glyph, 'A', 0)
         && !glyph.is_placeholder && glyph.resolved_font == &lv_font_montserrat_28);
  if (lookup) assert(lv_font_get_glyph_dsc(save_font, &glyph, 0x2026, 0)
                     && !glyph.is_placeholder && glyph.box_w && glyph.box_h);
  lv_obj_t *controls[] = {menu_back(lv_screen_active()), input, units, cancel, save};
  for (unsigned i = 0; i < sizeof(controls) / sizeof(*controls); ++i)
    assert(lv_obj_has_state(controls[i], LV_STATE_DISABLED));
  operation_label(lookup ? "Checking Location…" : "Saving settings…", 150, 548, 500,
                  &lv_font_montserrat_26, true, night);
  unsigned index = 0;
  lv_obj_t *keyboard = find_type(lv_screen_active(), &lv_keyboard_class, &index);
  assert(keyboard && !menu_visible(keyboard));
  unsigned pending = task_count;
  lv_obj_send_event(save, LV_EVENT_CLICKED, NULL);
  lv_obj_send_event(cancel, LV_EVENT_CLICKED, NULL);
  lv_obj_send_event(menu_back(lv_screen_active()), LV_EVENT_CLICKED, NULL);
  assert(task_count == pending && view == WAC_SETTINGS_WEATHER);
}

static void weather_saved(const char *title, bool night)
{
  menu_heading("Weather Location", night);
  assert(lv_obj_has_state(menu_back(lv_screen_active()), LV_STATE_DISABLED));
  operation_mark(&wac_icon_circle_check_mark, night);
  operation_label(title, 140, 368, 520, &lv_font_montserrat_36, false, night);
  lv_obj_t *location = operation_label("Cincinnati, Ohio, US", 140, 432, 520,
                                      &lv_font_montserrat_28, false, night);
  assert(lv_label_get_long_mode(location) == LV_LABEL_LONG_DOT);
  operation_label("Time Zone · America/New_York", 150, 486, 500,
                  &lv_font_montserrat_26, true, night);
  operation_label("Restarting for local weather…", 150, 554, 500,
                  &lv_font_montserrat_26, true, night);
  unsigned index = 0;
  assert(!find_type(lv_screen_active(), &lv_textarea_class, &index));
  index = 0;
  assert(!find_type(lv_screen_active(), &lv_keyboard_class, &index));
  assert(!menu_label(lv_screen_active(), "Cancel") && !menu_label(lv_screen_active(), "Save"));
  assert(!menu_label(lv_screen_active(), "Back to Clock"));
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
  lv_obj_update_layout(lv_screen_active());
  uint32_t id = keyboard_button(keyboard, text);
  if (preview) {
    assert(lv_keyboard_get_popovers(keyboard));
    assert(lv_buttonmatrix_has_button_ctrl(keyboard, id, LV_BUTTONMATRIX_CTRL_POPOVER));
  }
  /* Read LVGL's actual key rectangles rather than duplicate its layout. */
  lv_area_t area;
  lv_obj_get_coords(keyboard, &area);
  const lv_area_t *key = &((lv_buttonmatrix_t *)keyboard)->button_areas[id];
  pointer_data.point = (lv_point_t){area.x1 + (key->x1 + key->x2) / 2,
                                  area.y1 + (key->y1 + key->y2) / 2};
  assert(pointer_data.point.x >= 0 && pointer_data.point.x < 800
         && pointer_data.point.y >= 0 && pointer_data.point.y < 800);
  pointer_data.state = LV_INDEV_STATE_PRESSED;
  lv_indev_read(pointer);
  assert(lv_obj_has_state(keyboard, LV_STATE_PRESSED));
  assert(lv_keyboard_get_selected_button(keyboard) == id);
  /* Exercise the actual software draw path while the character preview and
   * utility vectors are visible. This is not a raster-quality assertion. */
  lv_refr_now(NULL);
  release();
  assert(!lv_obj_has_state(keyboard, LV_STATE_PRESSED));
}

static void assert_inside_round_panel(const lv_area_t *area)
{
  int x[] = {area->x1, area->x2}, y[] = {area->y1, area->y2};
  for (unsigned i = 0; i < 2; ++i) {
    for (unsigned j = 0; j < 2; ++j) {
      int dx = x[i] - 400, dy = y[j] - 400;
      if (dx * dx + dy * dy > 400 * 400)
        fprintf(stderr, "Keyboard corner (%d,%d) is outside the round panel\n", x[i], y[j]);
      assert(dx * dx + dy * dy <= 400 * 400);
    }
  }
}

static void test_settings_foundation(void)
{
  /* Deliberate saved-value snapshots check content rather than preview samples. */
  for (unsigned night = 0; night < 2; ++night) {
    fresh_editor();
    device_config_t snapshot = current();
    snapshot.alarm_hour = 18;
    snapshot.alarm_minute = 45;
    snapshot.alarm_repeat_days = 0x7f;
    snprintf(snapshot.location, sizeof(snapshot.location), "Cincinnati, Ohio, US");
    snprintf(snapshot.wifi_ssid, sizeof(snapshot.wifi_ssid), "Saved network");
    snapshot.wifi_ready = true;
    wac_ui_settings_close();
    wac_settings_apply_night(night);
    wac_settings_begin(lv_screen_active(), &snapshot);
    wac_ui_settings_navigate(WAC_SETTINGS_ENTRY);
    menu_heading("Settings", night);
    menu_row("Alarm", "Off", 212, false, night);
    menu_switch("Alarm", false, night);
    menu_row("Alarm Settings", "6:45 PM · Every Day", 320, true, night);
    menu_row("Weather Location", snapshot.location, 428, true, night);
    menu_row("Device Setup", NULL, 536, false, night);
    lv_obj_t *footer = menu_button("Back to Clock", NULL);
    menu_box(footer, 230, 646, 340, 88);
    menu_button_style(footer, false, night);
    assert(lv_obj_get_style_text_font(menu_label(footer, "Back to Clock"), 0)
           == &lv_font_montserrat_28);
    tap(menu_button("Device Setup", NULL));
    assert(view == WAC_SETTINGS_DEVICE_SETUP);
    menu_heading("Device Setup", night);
    menu_row("Display Settings", "Digital · 12-Hour", 212, true, night);
    menu_row("Night Shift", "Off", 320, true, night);
    menu_row("Device Settings", "Clock", 428, true, night);
    menu_row("Wi-Fi", "Saved network · Connected", 536, true, night);
    menu_box(menu_button("Cancel", NULL), 174, 646, 162, 88);
    menu_box(menu_button("Save & Restart", NULL), 358, 646, 268, 88);
    menu_button_style(menu_button("Save & Restart", NULL), true, night);
    lv_obj_t *summary = menu_label(menu_button("Wi-Fi", NULL), "Saved network · Connected");
    wifi_connected = false;
    wac_settings_poll();
    assert(menu_button("Wi-Fi", "Saved network · Not Connected"));
    assert(summary == menu_label(lv_screen_active(), "Saved network · Not Connected"));
    wifi_connected = true;
    wac_settings_poll();
    assert(summary == menu_label(lv_screen_active(), "Saved network · Connected"));
    tap(menu_button("Display Settings", NULL));
    assert(view == WAC_SETTINGS_DISPLAY);
    menu_heading("Display", night);
    menu_row("Home Screen", "Digital", 212, false, night);
    menu_row("Time Format", "12-Hour", 320, false, night);
    menu_row("Leading Zero", "Off", 428, false, night);
    menu_switch("Leading Zero", false, night);
    footer = menu_button("Done", NULL);
    menu_box(footer, 270, 664, 260, 80);
    menu_button_style(footer, true, night);
    tap(menu_button("Home Screen", "Digital"));
    assert(menu_button("Home Screen", "Analog"));
    tap(menu_button("Time Format", "12-Hour"));
    assert(menu_button("Time Format", "24-Hour"));
    lv_obj_update_layout(lv_screen_active());
    lv_obj_t *track = menu_sized_child(menu_button("Leading Zero", "Off"), 100, 56);
    tap(track); /* Decoration forwards a real pointer tap to the owning row. */
    menu_switch("Leading Zero", true, night);
    assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
    tap(menu_button("Done", NULL));
    assert(view == WAC_SETTINGS_DEVICE_SETUP);
    assert(menu_button("Display Settings", "Analog · 24-Hour"));
    tap(menu_button("Display Settings", NULL));
    menu_switch("Leading Zero", true, night);
    tap(menu_button("Done", NULL));
    tap(menu_button("Cancel", NULL));
    assert(view == WAC_SETTINGS_ENTRY);
    tap(menu_button("Device Setup", NULL));
    assert(menu_button("Display Settings", "Digital · 12-Hour"));
    tap(menu_button("Display Settings", NULL));
    menu_switch("Leading Zero", false, night);
    tap(menu_button("Done", NULL));
    tap(menu_button("Cancel", NULL));
    tap(menu_button("Back to Clock", NULL));
    assert(view == WAC_SETTINGS_NONE);
    device_config_t applied = current();
    assert(!strcmp(applied.default_clock_face, "digital")
           && !strcmp(applied.time_format, "12") && !applied.leading_zero_12h);
    assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
  }
  wac_settings_apply_night(false);
}

static void test_entry_saved_time_caption(void)
{
  fresh_editor();
  device_config_t snapshot = current();
  snapshot.leading_zero_12h = true;
  wac_ui_settings_close();
  wac_settings_begin(lv_screen_active(), &snapshot);
  wac_ui_settings_navigate(WAC_SETTINGS_ENTRY);
  menu_row("Alarm Settings", "7:00 AM · Mon–Fri", 320, true, false);
  wac_ui_settings_close();
  snprintf(snapshot.time_format, sizeof(snapshot.time_format), "24");
  wac_settings_begin(lv_screen_active(), &snapshot);
  wac_ui_settings_navigate(WAC_SETTINGS_ENTRY);
  menu_row("Alarm Settings", "07:00 · Mon–Fri", 320, true, false);
  assert(fake_nvs_calls(FAKE_SET) == 0);
}

static void test_display_save_and_truthful_wifi_draft(void)
{
  fresh_editor();
  device_config_t baseline = current(), saved = baseline;
  snprintf(saved.wifi_ssid, sizeof(saved.wifi_ssid), "Saved network");
  saved.wifi_ready = true;
  assert(device_config_save_device(&baseline, &saved) == ESP_OK);
  wac_ui_settings_close();
  open_editor();
  fake_nvs_clear_activity();
  tap(menu_button("Device Setup", NULL));
  assert(menu_button("Wi-Fi", "Saved network · Connected"));
  const struct { connectivity_wifi_state_t state; const char *caption; } states[] = {
    {CONNECTIVITY_WIFI_DISCONNECTED, "Saved network · Not Connected"},
    {CONNECTIVITY_WIFI_CONNECTING, "Saved network · Connecting"},
    {CONNECTIVITY_WIFI_RETRYING, "Saved network · Retrying"},
    {CONNECTIVITY_WIFI_CONNECTED, "Saved network · Connected"},
  };
  lv_obj_t *summary = menu_label(menu_button("Wi-Fi", NULL), "Saved network · Connected");
  for (unsigned i = 0; i < sizeof(states) / sizeof(*states); ++i) {
    wifi_state = states[i].state;
    wac_settings_poll();
    assert(menu_label(menu_button("Wi-Fi", NULL), states[i].caption) == summary);
    assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
  }
  tap(menu_button("Display Settings", NULL));
  tap(menu_button("Home Screen", "Digital"));
  tap(menu_button("Time Format", "12-Hour"));
  tap(menu_button("Leading Zero", "Off"));
  tap(menu_button("Done", NULL));
  tap(menu_button("Wi-Fi", NULL));
  run_task();
  click("Home network");
  field(1, "draft-password");
  click("Back");
  lv_obj_update_layout(lv_screen_active());
  tap(menu_back(lv_screen_active()));
  /* Still attached to the applied network: the pending network is not connected. */
  for (unsigned i = 0; i < sizeof(states) / sizeof(*states); ++i) {
    wifi_state = states[i].state;
    wac_settings_poll();
    assert(menu_button("Wi-Fi", "Home network · Not Connected"));
  }
  assert(menu_button("Display Settings", "Analog · 24-Hour"));
  assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
  tap(menu_button("Save & Restart", NULL));
  assert(task_count == 1 && fake_nvs_calls(FAKE_SET) == 0);
  /* Closing during work is an existing supported route; completion still saves
   * the copied edit, without restarting a later screen. */
  wac_ui_settings_close();
  run_task();
  device_config_t applied = current();
  assert(!strcmp(applied.default_clock_face, "analog")
         && !strcmp(applied.time_format, "24") && applied.leading_zero_12h);
  assert(!strcmp(applied.wifi_ssid, "Home network")
         && !strcmp(applied.wifi_password, "draft-password"));
  assert(applied.alarm_hour == saved.alarm_hour
         && applied.alarm_repeat_days == saved.alarm_repeat_days
         && !applied.alarm_enabled && !strcmp(applied.units, saved.units)
         && !strcmp(applied.location, saved.location));
  assert(restart_count == 0 && fake_nvs_calls(FAKE_SET) > 0);
}

static void keyboard_layout(lv_obj_t *keyboard, lv_keyboard_mode_t mode)
{
  static const char * const lower[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", LV_SYMBOL_BACKSPACE, "\n",
    LV_SYMBOL_UP, "z", "x", "c", "v", "b", "n", "m", ".", ",", "\n",
    "1#", LV_SYMBOL_CLOSE, " ", LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
  };
  static const char * const upper[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", LV_SYMBOL_BACKSPACE, "\n",
    LV_SYMBOL_UP, "Z", "X", "C", "V", "B", "N", "M", ".", ",", "\n",
    "1#", LV_SYMBOL_CLOSE, " ", LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
  };
  static const char * const numbers[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "@", "#", "$", "%", "&", "*", "(", ")", "-", LV_SYMBOL_BACKSPACE, "\n",
    "2/2", "?", "!", "\"", "'", ":", ";", "/", "_", ".", "\n",
    "ABC", LV_SYMBOL_CLOSE, " ", LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
  };
  static const char * const symbols[] = {
    "+", "=", "[", "]", "{", "}", "<", ">", "\\", LV_SYMBOL_BACKSPACE, "\n",
    "^", "~", "|", "`", ",", ".", ":", ";", "_", "/", "\n",
    "1/2", "!", "?", "@", "#", "$", "%", "&", "(", ")", "\n",
    "ABC", LV_SYMBOL_CLOSE, " ", LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
  };
  const char * const *expected = mode == LV_KEYBOARD_MODE_TEXT_LOWER ? lower
    : mode == LV_KEYBOARD_MODE_TEXT_UPPER ? upper
    : mode == LV_KEYBOARD_MODE_SPECIAL ? numbers : symbols;
  assert(lv_keyboard_get_mode(keyboard) == mode);
  const char * const *actual = lv_keyboard_get_map_array(keyboard);
  lv_obj_update_layout(keyboard);
  unsigned row = 0, column = 0, id = 0;
  lv_area_t previous = {0};
  const lv_area_t *areas = ((lv_buttonmatrix_t *)keyboard)->button_areas;
  for (unsigned i = 0; ; ++i) {
    assert(!strcmp(actual[i], expected[i]));
    if (!expected[i][0]) break;
    if (!strcmp(expected[i], "\n")) { ++row; column = 0; continue; }
    const lv_area_t *area = areas + id;
    assert(area->y1 == (int)row * 72 && area->y2 - area->y1 + 1 == 64);
    if (!column) assert(area->x1 == 0);
    else assert(area->x1 - previous.x2 - 1 == 6);
    if ((row < 3 && column == 9) || (row == 3 && column == 5)) assert(area->x2 == 559);
    bool wide = !strcmp(expected[i], "1#") || !strcmp(expected[i], "ABC");
    bool space = !strcmp(expected[i], " ");
    bool utility = wide || space || !strcmp(expected[i], "1/2")
      || !strcmp(expected[i], "2/2") || !strcmp(expected[i], LV_SYMBOL_UP)
      || !strcmp(expected[i], LV_SYMBOL_CLOSE) || !strcmp(expected[i], LV_SYMBOL_LEFT)
      || !strcmp(expected[i], LV_SYMBOL_RIGHT) || !strcmp(expected[i], LV_SYMBOL_BACKSPACE)
      || !strcmp(expected[i], LV_SYMBOL_OK);
    keyboard_control(keyboard, expected[i], wide ? 2 : space ? 6 : 1, !utility);
    if (row == 3) {
      int width = area->x2 - area->x1 + 1;
      int weight = wide ? 2 : space ? 6 : 1;
      assert(abs(width * 12 - 530 * weight) <= 12);
    }
    previous = *area;
    ++column;
    ++id;
  }
  assert(row == 3 && id == 36);
}

static void test_credentials_keyboard(void)
{
  fresh_editor();
  click("Device Setup");
  click("Wi-Fi");
  run_task();
  click("Home network");
  assert(view == WAC_SETTINGS_CREDENTIALS);
  lv_obj_t *note = menu_label(lv_screen_active(), "Enter the password to join this network.");
  menu_box(note, 160, 548, 480, -1);
  lv_obj_t *password = menu_field("Password");
  unsigned index = 0;
  lv_obj_t *keyboard = find_type(lv_screen_active(), &lv_keyboard_class, &index);
  assert(password && keyboard && lv_textarea_get_password_mode(password));
  assert(lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN));
  assert(lv_keyboard_get_textarea(keyboard) == NULL);
  tap(password);
  assert(lv_obj_has_state(password, LV_STATE_FOCUSED));
  assert(!lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN));
  assert(lv_keyboard_get_textarea(keyboard) == password);
  menu_keyboard(keyboard, false);
  menu_field_geometry("Password", 224, false);
  assert(!menu_visible(note));
  lv_area_t area;
  lv_obj_get_coords(keyboard, &area);
  assert_inside_round_panel(&area);
  keyboard_layout(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);

  /* Pointer presses type a deliberate password fixture through all four maps. */
  keyboard_key(keyboard, "a", true);
  keyboard_key(keyboard, "b", true);
  keyboard_key(keyboard, "c", true);
  keyboard_key(keyboard, LV_SYMBOL_UP, false);
  keyboard_layout(keyboard, LV_KEYBOARD_MODE_TEXT_UPPER);
  keyboard_key(keyboard, "A", true);
  keyboard_key(keyboard, "Z", true);
  keyboard_key(keyboard, LV_SYMBOL_UP, false);
  keyboard_layout(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
  keyboard_key(keyboard, "1#", false);
  keyboard_layout(keyboard, LV_KEYBOARD_MODE_SPECIAL);
  keyboard_key(keyboard, "1", true);
  keyboard_key(keyboard, "9", true);
  keyboard_key(keyboard, ":", true);
  keyboard_key(keyboard, "2/2", false);
  keyboard_layout(keyboard, LV_KEYBOARD_MODE_USER_1);
  const char *symbols[] = {"^", "~", "|", "`", "\\", "+", "=", "[", "]", "{", "}", "<", ">"};
  for (unsigned i = 0; i < sizeof(symbols) / sizeof(*symbols); ++i)
    keyboard_key(keyboard, symbols[i], true);
  keyboard_key(keyboard, "1/2", false);
  keyboard_layout(keyboard, LV_KEYBOARD_MODE_SPECIAL);
  keyboard_key(keyboard, ";", true);
  keyboard_key(keyboard, "ABC", false);
  keyboard_layout(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
  assert(!strcmp(lv_textarea_get_text(password), "abcAZ19:^~|`\\+=[]{}<>;"));

  keyboard_key(keyboard, LV_SYMBOL_LEFT, false);
  assert(lv_textarea_get_cursor_pos(password) == 21);
  keyboard_key(keyboard, " ", false);
  assert(!strcmp(lv_textarea_get_text(password), "abcAZ19:^~|`\\+=[]{}<> ;"));
  keyboard_key(keyboard, LV_SYMBOL_RIGHT, false);
  assert(lv_textarea_get_cursor_pos(password) == 23);
  keyboard_key(keyboard, LV_SYMBOL_BACKSPACE, false);
  keyboard_key(keyboard, LV_SYMBOL_BACKSPACE, false);
  assert(!strcmp(lv_textarea_get_text(password), "abcAZ19:^~|`\\+=[]{}<>"));

  keyboard_key(keyboard, LV_SYMBOL_OK, false);
  assert(lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN));
  assert(lv_keyboard_get_textarea(keyboard) == NULL);
  menu_field_geometry("Password", 372, false);
  assert(menu_visible(note));
  assert(view == WAC_SETTINGS_CREDENTIALS && task_count == 0);
  tap(password);
  assert(lv_keyboard_get_textarea(keyboard) == password);
  keyboard_key(keyboard, LV_SYMBOL_CLOSE, false);
  assert(lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN));
  assert(lv_keyboard_get_textarea(keyboard) == NULL);
  assert(!strcmp(lv_textarea_get_text(password), "abcAZ19:^~|`\\+=[]{}<>"));
  assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);

  /* Reopening reads the draft, so a textarea-only update cannot pass. */
  click("Back");
  click("Home network");
  password = menu_field("Password");
  assert(!strcmp(lv_textarea_get_text(password), "abcAZ19:^~|`\\+=[]{}<>"));
  click("Back");
  lv_obj_update_layout(lv_screen_active());
  tap(menu_back(lv_screen_active()));
  click("Cancel");
  device_config_t value = current();
  assert(!value.wifi_ssid[0] && !value.wifi_password[0]);
  assert(fake_nvs_calls(FAKE_SET) == 0);
}

static void test_text_fields_and_typing_layout(void)
{
  static const struct {
    wac_settings_view_t view;
    const char *title, *other;
    int top;
    size_t maximum;
  } cases[] = {
    {WAC_SETTINGS_DEVICE, "Room Name", "Device ID", 204, DEVICE_CONFIG_STR_LEN - 1},
    {WAC_SETTINGS_DEVICE, "Device ID", "Room Name", 372, DEVICE_CONFIG_STR_LEN - 1},
    {WAC_SETTINGS_WEATHER, "City, State", NULL, 212, DEVICE_CONFIG_STR_LEN - 1},
    {WAC_SETTINGS_CREDENTIALS, "Network Name", "Password", 204, DEVICE_CONFIG_WIFI_LEN - 1},
    {WAC_SETTINGS_CREDENTIALS, "Password", "Network Name", 372, DEVICE_CONFIG_WIFI_PASS_LEN - 1},
  };
  for (unsigned night = 0; night < 2; ++night) {
    for (unsigned i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
      fresh_editor();
      wac_settings_apply_night(night);
      if (cases[i].view == WAC_SETTINGS_WEATHER) click("Weather Location");
      else {
        click("Device Setup");
        if (cases[i].view == WAC_SETTINGS_DEVICE) click("Device Settings");
        else {
          click("Wi-Fi");
          run_task();
          click("Manual Network");
        }
      }
      assert(view == cases[i].view);
      const char *heading = cases[i].view == WAC_SETTINGS_DEVICE ? "Device"
        : cases[i].view == WAC_SETTINGS_WEATHER ? "Weather Location" : "Network Details";
      menu_heading(heading, night);
      menu_field_geometry(cases[i].title, cases[i].top, night);
      lv_obj_t *input = menu_field(cases[i].title);
      assert(lv_textarea_get_max_length(input) == cases[i].maximum);
      assert(lv_textarea_get_password_mode(input) == !strcmp(cases[i].title, "Password"));
      const char *footer = cases[i].view == WAC_SETTINGS_DEVICE ? "Done"
        : cases[i].view == WAC_SETTINGS_WEATHER ? "Save" : "Save & Restart";
      lv_obj_t *footer_button = menu_button(footer, NULL);
      lv_obj_t *other_button = cases[i].view == WAC_SETTINGS_WEATHER ? menu_button("Cancel", NULL)
        : cases[i].view == WAC_SETTINGS_CREDENTIALS ? menu_button("Back", NULL)
        : menu_button("House Messages", "Single");
      if (cases[i].view == WAC_SETTINGS_DEVICE) {
        menu_box(footer_button, 270, 664, 260, 80);
        menu_row("House Messages", "Single", 548, false, night);
        assert(menu_visible(menu_label(lv_screen_active(), "Each shared clock needs a distinct Device ID.")));
      } else if (cases[i].view == WAC_SETTINGS_WEATHER) {
        menu_box(footer_button, 412, 646, 210, 88);
        menu_box(other_button, 178, 646, 206, 88);
        lv_obj_t *note = menu_label(lv_screen_active(), "Time zone is set automatically\nfrom this location.");
        menu_box(note, 150, 548, 500, -1);
        assert(lv_obj_get_style_text_font(note, 0)->line_height == lv_font_montserrat_26.line_height);
        menu_color(note, night ? 0xc17a7a : 0xa6b6cf, false);
      } else {
        menu_box(footer_button, 345, 646, 268, 88);
        menu_box(other_button, 187, 646, 136, 88);
        lv_obj_t *note = menu_label(lv_screen_active(), "Enter the network name and password.");
        menu_box(note, 160, 548, 480, -1);
        assert(lv_obj_get_style_text_font(note, 0)->line_height == lv_font_montserrat_24.line_height);
        menu_color(note, night ? 0xc17a7a : 0xa6b6cf, false);
      }
      unsigned index = 0;
      lv_obj_t *keyboard = find_type(lv_screen_active(), &lv_keyboard_class, &index);
      assert(keyboard && !menu_visible(keyboard));
      lv_obj_t *inactive = cases[i].other ? lv_obj_get_parent(menu_label(lv_screen_active(), cases[i].other)) : NULL;
      char original[DEVICE_CONFIG_WIFI_PASS_LEN];
      snprintf(original, sizeof(original), "%s", lv_textarea_get_text(input));
      tap(input);
      assert(menu_visible(keyboard) && lv_keyboard_get_textarea(keyboard) == input);
      menu_field_geometry(cases[i].title, 224, night);
      menu_keyboard(keyboard, night);
      assert(menu_visible(lv_obj_get_parent(input)) && !menu_visible(inactive));
      assert(!menu_visible(footer_button) && !menu_visible(other_button));
      if (cases[i].view == WAC_SETTINGS_DEVICE)
        assert(!menu_visible(menu_label(lv_screen_active(), "Each shared clock needs a distinct Device ID.")));
      if (cases[i].view == WAC_SETTINGS_CREDENTIALS)
        assert(!menu_visible(menu_label(lv_screen_active(), "Enter the network name and password.")));
      if (cases[i].view == WAC_SETTINGS_WEATHER)
        assert(!menu_visible(menu_label(lv_screen_active(), "Time zone is set automatically\nfrom this location.")));
      if (cases[i].view == WAC_SETTINGS_WEATHER)
        assert(!menu_visible(menu_button("Units", "Imperial")));
      assert(menu_visible(menu_label(lv_screen_active(), heading))
             && menu_visible(menu_back(lv_obj_get_parent(menu_label(lv_screen_active(), heading)))));
      assert(lv_obj_has_state(input, LV_STATE_FOCUSED));
      assert(lv_color_eq(lv_obj_get_style_bg_color(input, LV_PART_CURSOR),
                         lv_color_hex(night ? 0xecc0c0 : 0xf5f8ff)));
      assert(!strcmp(lv_textarea_get_text(input), original));
      char maximum[DEVICE_CONFIG_WIFI_PASS_LEN + 1];
      memset(maximum, 'a', cases[i].maximum);
      maximum[cases[i].maximum] = '\0';
      lv_textarea_set_text(input, maximum);
      lv_obj_send_event(input, LV_EVENT_VALUE_CHANGED, NULL);
      keyboard_key(keyboard, "a", true);
      assert(strlen(lv_textarea_get_text(input)) == cases[i].maximum);
      keyboard_key(keyboard, LV_SYMBOL_BACKSPACE, false);
      assert(strlen(lv_textarea_get_text(input)) == cases[i].maximum - 1);
      keyboard_key(keyboard, i % 2 ? LV_SYMBOL_CLOSE : LV_SYMBOL_OK, false);
      assert(!menu_visible(keyboard) && lv_keyboard_get_textarea(keyboard) == NULL);
      assert(input == menu_field(cases[i].title));
      menu_field_geometry(cases[i].title, cases[i].top, night);
      assert(menu_visible(footer_button) && menu_visible(other_button));
      if (cases[i].view == WAC_SETTINGS_DEVICE)
        assert(menu_visible(menu_label(lv_screen_active(), "Each shared clock needs a distinct Device ID.")));
      if (inactive) assert(menu_visible(inactive));
      assert(strlen(lv_textarea_get_text(input)) == cases[i].maximum - 1);
      tap(input); /* A field can reopen after either keyboard close action. */
      assert(menu_visible(keyboard) && lv_keyboard_get_textarea(keyboard) == input);
      keyboard_key(keyboard, LV_SYMBOL_CLOSE, false);
      assert(task_count == 0 && fake_nvs_calls(FAKE_SET) == 0);
    }
  }
  wac_settings_apply_night(false);
}

static void test_missing_field_focus_and_error_clear(void)
{
  fresh_editor();
  click("Device Setup");
  click("Device Settings");
  field(0, "");
  field(1, "");
  click("Done");
  click("Save & Restart");
  assert(view == WAC_SETTINGS_DEVICE && task_count == 0);
  unsigned index = 0;
  lv_obj_t *keyboard = find_type(lv_screen_active(), &lv_keyboard_class, &index);
  lv_obj_t *room = menu_field("Room Name");
  assert(menu_visible(keyboard) && lv_keyboard_get_textarea(keyboard) == room);
  menu_field_geometry("Room Name", 224, false);
  lv_obj_t *error = menu_label(lv_screen_active(), "Enter a room name before saving.");
  menu_box(error, 150, 178, 500, -1);
  assert(menu_visible(error));
  keyboard_key(keyboard, " ", false);
  assert(menu_visible(error));
  keyboard_key(keyboard, "a", true);
  assert(!menu_visible(menu_label(lv_screen_active(), "Enter a room name before saving.")));
  keyboard_key(keyboard, LV_SYMBOL_OK, false);
  click("Done");
  click("Save & Restart");
  assert(view == WAC_SETTINGS_DEVICE && task_count == 0);
  index = 0;
  keyboard = find_type(lv_screen_active(), &lv_keyboard_class, &index);
  lv_obj_t *id = menu_field("Device ID");
  assert(menu_visible(keyboard) && lv_keyboard_get_textarea(keyboard) == id);
  menu_field_geometry("Device ID", 224, false);
  error = menu_label(lv_screen_active(), "Enter a device ID before saving.");
  menu_box(error, 150, 178, 500, -1);
  keyboard_key(keyboard, "b", true);
  assert(!menu_visible(menu_label(lv_screen_active(), "Enter a device ID before saving.")));
  keyboard_key(keyboard, LV_SYMBOL_CLOSE, false);
  click("Done");
  click("Cancel");
  click("Weather Location");
  click("Save");
  assert(view == WAC_SETTINGS_WEATHER && task_count == 0);
  index = 0;
  keyboard = find_type(lv_screen_active(), &lv_keyboard_class, &index);
  lv_obj_t *location = menu_field("City, State");
  assert(menu_visible(keyboard) && lv_keyboard_get_textarea(keyboard) == location);
  error = menu_label(lv_screen_active(), "Enter a city and state before saving.");
  menu_box(error, 150, 178, 500, -1);
  keyboard_key(keyboard, " ", false);
  assert(menu_visible(error));
  keyboard_key(keyboard, "c", true);
  assert(!menu_visible(menu_label(lv_screen_active(), "Enter a city and state before saving.")));
  keyboard_key(keyboard, LV_SYMBOL_OK, false);
  click("Cancel");
  assert(task_count == 0 && fake_nvs_calls(FAKE_SET) == 0);
  device_config_t applied = current();
  assert(!strcmp(applied.room_name, "Clock") && !strcmp(applied.device_id, "clock-esp32-p4")
         && !applied.location[0]);
}

static void test_credentials_missing_network_focus(void)
{
  fresh_editor();
  click("Device Setup");
  click("Wi-Fi");
  run_task();
  click("Manual Network");
  click("Save & Restart");
  assert(view == WAC_SETTINGS_CREDENTIALS && task_count == 0);
  unsigned index = 0;
  lv_obj_t *keyboard = find_type(lv_screen_active(), &lv_keyboard_class, &index);
  lv_obj_t *network = menu_field("Network Name");
  lv_obj_t *error = menu_label(lv_screen_active(), "Enter a network name before saving.");
  assert(menu_visible(keyboard) && lv_keyboard_get_textarea(keyboard) == network);
  menu_field_geometry("Network Name", 224, false);
  menu_box(error, 150, 178, 500, -1);
  assert(menu_visible(error));
  keyboard_key(keyboard, " ", false);
  assert(menu_visible(error));
  keyboard_key(keyboard, LV_SYMBOL_OK, false);
  click("Save & Restart");
  assert(view == WAC_SETTINGS_CREDENTIALS && task_count == 0);
  assert(menu_visible(keyboard) && lv_keyboard_get_textarea(keyboard) == network);
  assert(menu_visible(menu_label(lv_screen_active(), "Enter a network name before saving.")));
  keyboard_key(keyboard, "h", true);
  assert(!menu_visible(menu_label(lv_screen_active(), "Enter a network name before saving.")));
  keyboard_key(keyboard, LV_SYMBOL_CLOSE, false);
  click("Back");
  lv_obj_update_layout(lv_screen_active());
  tap(menu_back(lv_screen_active()));
  click("Cancel");
  assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
}

static void digit_label(int x, int y, int width, const char *text, const lv_font_t *font, bool night)
{
  lv_obj_t *obj = box_label(x, y, width);
  assert(!strcmp(lv_label_get_text(obj), text));
  assert(lv_obj_get_style_text_font(obj, 0) == font);
  menu_color(obj, night ? 0xecc0c0 : 0xf5f8ff, false);
  assert(font->fallback == NULL);
  assert(font->line_height == (font == &wac_font_settings_104 ? 118 : 108)
         && font->base_line == (font == &wac_font_settings_104 ? 22 : 19));
  assert(lv_obj_get_height(obj) == font->line_height);
  lv_area_t area; lv_obj_get_coords(obj, &area);
  int baseline = area.y1 + font->line_height - font->base_line;
  assert(baseline == y + (font == &wac_font_settings_104 ? 96 : 89));
  for (const char *p = text; *p; ++p) {
    lv_font_glyph_dsc_t glyph;
    assert(lv_font_get_glyph_dsc(font, &glyph, (unsigned char)*p, 0));
    assert(!glyph.is_placeholder && glyph.resolved_font == font && glyph.box_w && glyph.box_h);
    /* Montserrat's real glyph ink sits inside the approved CSS line box.
     * Sparse font conversion must not move those digits 21px upward. */
    int ink_top = baseline - glyph.ofs_y - glyph.box_h;
    if (*p == ':') assert(ink_top == y + 40);
    else assert(ink_top >= y + 20 && ink_top <= y + 22);
  }
}

static void picker_layout(const char *title, int hour, int minute, bool twelve, bool night)
{
  menu_heading(title, night);
  operation_label("Hour", 170, 196, 170, &lv_font_montserrat_28, true, night);
  operation_label("Minute", 460, 196, 170, &lv_font_montserrat_28, true, night);
  char text[8];
  if (twelve) snprintf(text, sizeof(text), "%d", hour % 12 ? hour % 12 : 12);
  else snprintf(text, sizeof(text), "%02d", hour);
  digit_label(180, 355, 170, text, &wac_font_settings_104, night);
  snprintf(text, sizeof(text), "%02d", minute);
  digit_label(450, 355, 170, text, &wac_font_settings_104, night);
  digit_label(365, 357, 70, ":", &wac_font_settings_104, night);
  const int boxes[][2] = {{200,240},{200,472},{470,240},{470,472}};
  for (unsigned i = 0; i < 4; ++i) {
    lv_obj_t *button = box_button(boxes[i][0], boxes[i][1], 130, 96);
    menu_button_style(button, false, night);
    lv_obj_t *arrow = image_source(button, i % 2 == 0 ? &wac_icon_chevron_up : &wac_icon_chevron_down);
    assert(arrow && lv_obj_get_width(arrow) == 32 && lv_obj_get_height(arrow) == 32);
    assert(lv_color_eq(lv_obj_get_style_image_recolor(arrow, 0),
                      lv_color_hex(night ? 0xecc0c0 : 0xf5f8ff)));
  }
  unsigned index = 0;
  assert(!find_type(lv_screen_active(), &lv_roller_class, &index));
  if (twelve) {
    lv_obj_t *am = menu_button("AM", NULL), *pm = menu_button("PM", NULL);
    menu_box(am, 270, 588, 120, 72);
    menu_box(pm, 410, 588, 120, 72);
    menu_button_style(am, hour < 12, night);
    menu_button_style(pm, hour >= 12, night);
  } else assert(!menu_label(lv_screen_active(), "AM") && !menu_label(lv_screen_active(), "PM"));
  lv_obj_t *done = menu_button("Done", NULL);
  menu_box(done, 270, 682, 260, 88);
  menu_button_style(done, true, night);
}

static void test_shared_time_arrow_controls(void)
{
  static const wac_settings_view_t routes[] = {
    WAC_SETTINGS_ALARM_TIME, WAC_SETTINGS_NIGHT_START, WAC_SETTINGS_NIGHT_END
  };
  static const char *titles[] = {"Alarm Time", "Night Starts", "Night Ends"};
  static const struct {
    int hour, minute, expected_hour, expected_minute;
    bool up;
  } cases[] = {
    {7, 0, 7, 5, true},       /* ordinary five-minute step */
    {7, 55, 8, 0, true},     /* carry to the next hour */
    {11, 55, 12, 0, true},   /* AM to PM at noon */
    {0, 0, 23, 55, false},   /* borrow from midnight */
    {23, 55, 0, 0, true},    /* wrap to the next day */
    {23, 59, 0, 4, true},    /* preserve an arbitrary saved minute */
  };

  for (unsigned twelve = 0; twelve < 2; ++twelve) {
    for (unsigned route = 0; route < sizeof(routes) / sizeof(*routes); ++route) {
      for (unsigned case_index = 0; case_index < sizeof(cases) / sizeof(*cases); ++case_index) {
        const int start_hour = cases[case_index].hour;
        const int start_minute = cases[case_index].minute;
        const int changed_hour = cases[case_index].expected_hour;
        const int changed_minute = cases[case_index].expected_minute;
        const int first_arrow_y = cases[case_index].up ? 240 : 472;
        const int inverse_arrow_y = cases[case_index].up ? 472 : 240;
        const bool night = route == 1;

        fresh_editor();
        device_config_t baseline = current(), saved = baseline;
        strcpy(saved.time_format, twelve ? "12" : "24");
        saved.leading_zero_12h = true;
        saved.alarm_hour = start_hour;
        saved.alarm_minute = start_minute;
        saved.alarm_repeat_days = (1u << 1) | (1u << 3);
        strcpy(saved.room_name, "Saved room");
        saved.day_brightness = 65;
        snprintf(saved.night_shift_start, sizeof(saved.night_shift_start), "%02d:%02d",
                 start_hour, start_minute);
        snprintf(saved.night_shift_end, sizeof(saved.night_shift_end), "%02d:%02d",
                 start_hour, start_minute);
        assert(device_config_save_device(&baseline, &saved) == ESP_OK);
        assert(device_config_save_alarm(&baseline, &saved) == ESP_OK);
        wac_ui_settings_close();
        open_editor();
        fake_nvs_clear_activity();
        wac_settings_apply_night(night);

        if (!route) { click("Alarm Settings"); click("Alarm Time"); }
        else { click("Device Setup"); click("Night Shift"); click("Fixed Schedule"); click(titles[route]); }
        assert(view == routes[route]);
        picker_layout(titles[route], start_hour, start_minute, twelve, night);

        if (case_index == 5) {
          tap(menu_button("Done", NULL));
          assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
          if (!route) click("Alarm Time");
          else click(titles[route]);
          picker_layout(titles[route], start_hour, start_minute, twelve, night);
        }

        tap(box_button(470, first_arrow_y, 130, 96));
        picker_layout(titles[route], changed_hour, changed_minute, twelve, night);
        tap(box_button(470, inverse_arrow_y, 130, 96));
        picker_layout(titles[route], start_hour, start_minute, twelve, night);

        /* Keep the separate hour and period buttons' established behavior. */
        if (case_index == 0) {
          if (twelve) {
            picker_set(11, 0, true);
            tap(box_button(200, 240, 130, 96));
            picker_layout(titles[route], 0, 0, true, night);
            tap(box_button(200, 472, 130, 96));
            picker_layout(titles[route], 11, 0, true, night);
            tap(menu_button("PM", NULL));
            picker_layout(titles[route], 23, 0, true, night);
            tap(box_button(200, 240, 130, 96));
            picker_layout(titles[route], 12, 0, true, night);
            tap(box_button(200, 472, 130, 96));
            picker_layout(titles[route], 23, 0, true, night);
            tap(menu_button("AM", NULL));
            picker_layout(titles[route], 11, 0, true, night);
          } else {
            picker_set(23, 0, false);
            tap(box_button(200, 240, 130, 96));
            picker_layout(titles[route], 0, 0, false, night);
            tap(box_button(200, 472, 130, 96));
            picker_layout(titles[route], 23, 0, false, night);
          }
          picker_set(start_hour, start_minute, twelve);
          picker_layout(titles[route], start_hour, start_minute, twelve, night);
        }

        /* Leave an edited draft, then verify Done and Cancel never write it. */
        tap(box_button(470, first_arrow_y, 130, 96));
        picker_layout(titles[route], changed_hour, changed_minute, twelve, night);
        tap(menu_button("Done", NULL));
        assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
        char detail[24];
        if (twelve)
          snprintf(detail, sizeof(detail), "%d:%02d %s",
                   changed_hour % 12 ? changed_hour % 12 : 12, changed_minute,
                   changed_hour < 12 ? "AM" : "PM");
        else snprintf(detail, sizeof(detail), "%02d:%02d", changed_hour, changed_minute);
        assert(menu_button(titles[route], detail));
        click(titles[route]);
        picker_layout(titles[route], changed_hour, changed_minute, twelve, night);
        header_back();
        if (!route) click("Cancel");
        else { click("Done"); click("Done"); click("Cancel"); }

        device_config_t applied = current();
        assert(applied.alarm_hour == saved.alarm_hour && applied.alarm_minute == saved.alarm_minute);
        assert(!strcmp(applied.night_shift_start, saved.night_shift_start)
               && !strcmp(applied.night_shift_end, saved.night_shift_end));
        assert(applied.alarm_repeat_days == saved.alarm_repeat_days && applied.leading_zero_12h
               && !strcmp(applied.room_name, "Saved room") && applied.day_brightness == 65);
        assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);

        /* Alarm Save applies the carry and keeps unrelated saved preferences. */
        if (!route && (case_index == 1 || case_index == 5)) {
          click("Alarm Settings");
          click("Alarm Time");
          picker_layout(titles[route], start_hour, start_minute, twelve, false);
          tap(box_button(470, 240, 130, 96));
          picker_layout(titles[route], changed_hour, changed_minute, twelve, false);
          tap(menu_button("Done", NULL));
          tap(menu_button("Save", NULL));
          assert(task_count == 1 && fake_nvs_calls(FAKE_SET) == 0);
          run_task();
          applied = current();
          assert(applied.alarm_hour == changed_hour && applied.alarm_minute == changed_minute);
          assert(applied.alarm_repeat_days == saved.alarm_repeat_days
                 && applied.leading_zero_12h == saved.leading_zero_12h
                 && !strcmp(applied.time_format, saved.time_format));
          assert(!strcmp(applied.room_name, "Saved room") && applied.day_brightness == 65);
          assert(!strcmp(applied.night_shift_start, saved.night_shift_start)
                 && !strcmp(applied.night_shift_end, saved.night_shift_end));
          assert(fake_nvs_calls(FAKE_SET) > 0);
        }
      }
    }
  }
  wac_settings_apply_night(false);
}

static void repeat_selection(unsigned mask, bool night)
{
  static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const int positions[][2] = {{454,380},{136,267},{263,267},{390,267},{517,267},{200,380},{327,380}};
  for (unsigned day = 0; day < 7; ++day) {
    lv_obj_t *button = menu_button(days[day], NULL);
    menu_box(button, positions[day][0], positions[day][1], 116, 96);
    bool selected = (mask & (1u << day)) != 0;
    menu_button_style(button, selected, night);
    lv_obj_t *check = image_source(button, &wac_icon_check);
    assert((check != NULL) == selected);
    lv_obj_t *title = menu_label(button, days[day]);
    assert(lv_obj_get_style_text_font(title, 0) == &lv_font_montserrat_28);
    if (selected) {
      assert(lv_obj_get_width(check) == 32 && lv_obj_get_height(check) == 32);
      lv_area_t title_area, check_area;
      lv_obj_get_coords(title, &title_area); lv_obj_get_coords(check, &check_area);
      assert(check_area.y1 - title_area.y2 - 1 == 4);
    }
  }
  assert(lv_obj_has_state(menu_button("Done", NULL), LV_STATE_DISABLED) == !mask);
}

static void test_repeat_shortcuts_and_zero_guard(void)
{
  for (unsigned night = 0; night < 2; ++night) {
    fresh_editor();
    wac_settings_apply_night(night);
    click("Alarm Settings"); click("Repeat");
    menu_heading("Repeat", night);
    operation_label("Choose alarm days", 150, 198, 500, &lv_font_montserrat_32, true, night);
    repeat_selection(0x3e, night);
    operation_label("Mon–Fri", 155, 617, 490, &lv_font_montserrat_28, true, night);
    menu_box(menu_button("Weekdays", NULL), 164, 501, 224, 96);
    menu_box(menu_button("Every Day", NULL), 412, 501, 224, 96);
    menu_box(menu_button("Done", NULL), 270, 682, 260, 88);
    tap(menu_button("Every Day", NULL));
    repeat_selection(0x7f, night);
    operation_label("Every Day", 155, 617, 490, &lv_font_montserrat_28, true, night);
    tap(menu_button("Weekdays", NULL));
    repeat_selection(0x3e, night);
    tap(menu_button("Fri", NULL));
    repeat_selection(0x1e, night);
    operation_label("Mon, Tue, Wed, Thu", 155, 617, 490, &lv_font_montserrat_28, true, night);
    tap(menu_button("Fri", NULL));
    const char *weekdays[] = {"Mon", "Tue", "Wed", "Thu", "Fri"};
    for (unsigned day = 0; day < 5; ++day) tap(menu_button(weekdays[day], NULL));
    repeat_selection(0, night);
    operation_label("Choose at least one day", 155, 617, 490, &lv_font_montserrat_28, false, night);
    tap(menu_button("Done", NULL));
    lv_obj_send_event(menu_button("Done", NULL), LV_EVENT_CLICKED, NULL);
    assert(view == WAC_SETTINGS_REPEAT && task_count == 0 && fake_nvs_calls(FAKE_SET) == 0);
    tap(menu_button("Sat", NULL)); tap(menu_button("Sun", NULL));
    repeat_selection(0x41, night);
    operation_label("Sat–Sun", 155, 617, 490, &lv_font_montserrat_28, true, night);
    tap(menu_button("Done", NULL));
    click("Repeat");
    repeat_selection(0x41, night);
    header_back(); click("Cancel");
    assert(current().alarm_repeat_days == 0x3e && fake_nvs_calls(FAKE_SET) == 0);
  }
  wac_settings_apply_night(false);
}

static void test_night_rows_modes_and_fixed_notes(void)
{
  const char *modes[] = {"Off", "Always On", "Fixed Schedule", "Sunset to Sunrise"};
  const char *descriptions[] = {"Use the day appearance", "Use the night appearance", "Switch at your set times", "Follow your weather location"};
  for (unsigned night = 0; night < 2; ++night) {
    fresh_editor(); wac_settings_apply_night(night);
    click("Device Setup"); click("Night Shift");
    menu_heading("Night Shift", night);
    menu_row("Mode", "Off", 212, true, night);
    menu_row("Fixed Schedule", "10:00 PM – 6:00 AM", 320, true, night);
    menu_row("Sunrise/Sunset Offsets", "Sunset 0 min · Sunrise 0 min", 428, true, night);
    menu_row("Brightness", "Day 80% · Night 30%", 536, true, night);
    menu_box(menu_button("Done", NULL), 270, 664, 260, 80);
    tap(menu_button("Mode", NULL));
    for (unsigned selected = 0; selected < 4; ++selected) {
      tap(menu_button(modes[selected], descriptions[selected]));
      menu_heading("Night Shift Mode", night);
      for (unsigned option = 0; option < 4; ++option) {
        lv_obj_t *button = menu_button(modes[option], descriptions[option]);
        menu_box(button, 140, 212 + (int)option * 108, 520, 96);
        menu_button_style(button, option == selected, night);
        assert(lv_obj_get_style_text_font(menu_label(button, modes[option]), 0) == &lv_font_montserrat_28);
        const lv_font_t *font = lv_obj_get_style_text_font(menu_label(button, descriptions[option]), 0);
        assert(font->line_height == lv_font_montserrat_24.line_height);
        menu_color(menu_label(button, descriptions[option]), night ? 0xc17a7a : 0xa6b6cf, false);
        lv_obj_t *check = image_source(button, &wac_icon_check);
        assert((check != NULL) == (option == selected));
        if (check) {
          menu_box(check, 607, 244 + (int)option * 108, 32, 32);
          assert(lv_color_eq(lv_obj_get_style_image_recolor(check, 0), lv_color_hex(night ? 0xecc0c0 : 0xf5f8ff)));
        }
      }
      assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
    }
    tap(menu_button("Fixed Schedule", "Switch at your set times"));
    tap(menu_button("Done", NULL));
    assert(menu_button("Mode", "Fixed Schedule"));
    tap(menu_button("Fixed Schedule", NULL));
    operation_label("Every Day", 150, 175, 500, &lv_font_montserrat_28, true, night);
    const char *fields[] = {"Night Starts", "Night Ends"};
    const char *values[] = {"10:00 PM", "6:00 AM"};
    for (unsigned field_index = 0; field_index < 2; ++field_index) {
      lv_obj_t *button = menu_button(fields[field_index], values[field_index]);
      menu_box(button, 140, 244 + (int)field_index * 120, 520, 104);
      menu_button_style(button, false, night);
      assert(lv_obj_get_style_text_font(menu_label(button, fields[field_index]), 0) == &lv_font_montserrat_28);
      menu_color(menu_label(button, values[field_index]), night ? 0xe1b6b6 : 0x93d1ff, false);
    }
    operation_label("Ends the next day.", 160, 510, 480, &lv_font_montserrat_28, true, night);
    operation_label("Fixed Schedule is selected.", 150, 564, 500, &lv_font_montserrat_28, true, night);
    click("Night Ends"); picker_set(22, 0, true); click("Done");
    operation_label("Night Shift stays on all day.", 160, 510, 480, &lv_font_montserrat_28, true, night);
    click("Night Starts"); picker_set(8, 0, true); click("Done");
    operation_label("Starts and ends on the same day.", 160, 510, 480, &lv_font_montserrat_28, true, night);
    click("Done"); click("Mode"); tap(menu_button("Off", "Use the day appearance")); click("Done");
    click("Fixed Schedule");
    operation_label("Choose Fixed Schedule in Mode to use these times.", 150, 564, 500, &lv_font_montserrat_28, true, night);
    header_back(); click("Done"); click("Cancel");
        device_config_t applied = current();
    assert(applied.night_shift_mode == DEVICE_CONFIG_NIGHT_OFF
           && !strcmp(applied.night_shift_start, "22:00") && !strcmp(applied.night_shift_end, "06:00"));
    assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
  }
  wac_settings_apply_night(false);
}

static void offset_layout(int value, bool sunset, bool night)
{
  menu_heading(sunset ? "Sunset Offset" : "Sunrise Offset", night);
  operation_label(sunset ? "Night starts" : "Night ends", 150, 190, 500, &lv_font_montserrat_28, true, night);
  const char *directions[] = {"Before", "At", "After"};
  for (unsigned dir = 0; dir < 3; ++dir) {
    lv_obj_t *button = menu_button(directions[dir], NULL);
    menu_box(button, 164 + (int)dir * 162, 244, 148, 88);
    menu_button_style(button, dir == (value < 0 ? 0u : value > 0 ? 2u : 1u), night);
  }
  lv_obj_t *minus = box_button(140, 370, 110, 104), *plus = box_button(550, 370, 110, 104);
  menu_button_style(minus, false, night); menu_button_style(plus, false, night);
  assert(image_source(minus, &wac_icon_minus) && image_source(plus, &wac_icon_plus));
  assert(lv_obj_has_state(minus, LV_STATE_DISABLED) == (value == 0));
  assert(lv_obj_has_state(plus, LV_STATE_DISABLED) == (abs(value) == 120));
  char text[48]; snprintf(text, sizeof(text), "%d", abs(value));
  digit_label(270, 366, 260, text, &wac_font_settings_96, night);
  operation_label("minutes", 270, 474, 260, &lv_font_montserrat_28, true, night);
  if (!value) snprintf(text, sizeof(text), "At %s", sunset ? "sunset" : "sunrise");
  else snprintf(text, sizeof(text), "%d min %s %s", abs(value), value < 0 ? "before" : "after", sunset ? "sunset" : "sunrise");
  operation_label(text, 150, 560, 500, &lv_font_montserrat_28, true, night);
  menu_box(menu_button("Done", NULL), 270, 664, 260, 80);
}

static void test_offset_direction_limits_and_drafts(void)
{
  for (unsigned sunset = 0; sunset < 2; ++sunset) {
    fresh_editor(); wac_settings_apply_night(sunset);
    click("Device Setup"); click("Night Shift"); click("Sunrise/Sunset Offsets");
    const char *title = sunset ? "Night Starts" : "Night Ends";
    click(title);
    offset_layout(0, sunset, sunset);
    tap(box_button(140, 370, 110, 104));
    offset_layout(0, sunset, sunset);
    tap(menu_button("Before", NULL));
    offset_layout(-15, sunset, sunset);
    tap(box_button(140, 370, 110, 104));
    offset_layout(-10, sunset, sunset);
    for (unsigned step = 0; step < 22; ++step) tap(box_button(550, 370, 110, 104));
    offset_layout(-120, sunset, sunset);
    tap(box_button(550, 370, 110, 104));
    offset_layout(-120, sunset, sunset);
    tap(menu_button("After", NULL));
    offset_layout(120, sunset, sunset);
    tap(menu_button("At", NULL));
    offset_layout(0, sunset, sunset);
    tap(menu_button("After", NULL));
    offset_layout(15, sunset, sunset);
    for (unsigned step = 0; step < 3; ++step) tap(box_button(140, 370, 110, 104));
    offset_layout(0, sunset, sunset);
    tap(box_button(550, 370, 110, 104));
    offset_layout(5, sunset, sunset);
    click("Done"); click(title);
    offset_layout(5, sunset, sunset);
    header_back(); click("Done");
    assert(menu_button("Sunrise/Sunset Offsets", sunset ? "Sunset +5 min · Sunrise 0 min" : "Sunset 0 min · Sunrise +5 min"));
    click("Done"); click("Cancel");
    device_config_t applied = current();
    assert(applied.night_sunset_offset_minutes == 0 && applied.night_sunrise_offset_minutes == 0);
    assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
  }
  wac_settings_apply_night(false);
}

static void row_arrow(const char *title, bool night)
{
  lv_obj_t *button = menu_button(title, NULL);
  lv_obj_t *arrow = image_source(button, &wac_icon_chevron_right);
  lv_area_t area; lv_obj_get_coords(button, &area);
  menu_box(arrow, 607, area.y1 + 32, 32, 32);
  assert(arrow && lv_obj_get_width(arrow) == 32 && lv_obj_get_height(arrow) == 32);
  bool primary = !strcmp(title, "Sound & Snooze");
  assert(lv_color_eq(lv_obj_get_style_image_recolor(arrow, 0),
                    lv_color_hex(primary ? (night ? 0xecc0c0 : 0xf5f8ff)
                                         : (night ? 0xe1b6b6 : 0x93d1ff))));
}

static lv_obj_t *alarm_volume_slider(void)
{
  unsigned index = 0;
  lv_obj_t *slider = find_type(lv_screen_active(), &lv_slider_class, &index);
  assert(slider && lv_slider_get_min_value(slider) == 2 && lv_slider_get_max_value(slider) == 20);
  return slider;
}

static void slider_geometry(lv_obj_t *slider, int x, int y, bool night)
{
  menu_box(slider, x, y, 450, 8);
  lv_area_t rail, click_area;
  lv_obj_get_coords(slider, &rail);
  lv_obj_get_click_area(slider, &click_area);

  const uint32_t track = night ? 0x321c20 : 0x17263b;
  const uint32_t range = night ? 0xc17a7a : 0x78b8ff;
  assert(lv_obj_get_style_bg_opa(slider, LV_PART_MAIN) == LV_OPA_COVER);
  assert(lv_color_eq(lv_obj_get_style_bg_color(slider, LV_PART_MAIN), lv_color_hex(track)));
  assert(lv_obj_get_style_radius(slider, LV_PART_MAIN) == LV_RADIUS_CIRCLE);
  assert(lv_color_eq(lv_obj_get_style_bg_color(slider, LV_PART_INDICATOR), lv_color_hex(range)));
  assert(lv_color_eq(lv_obj_get_style_bg_color(slider, LV_PART_KNOB), lv_color_hex(range)));
  assert(lv_obj_get_style_radius(slider, LV_PART_INDICATOR) == LV_RADIUS_CIRCLE);
  assert(lv_obj_get_style_radius(slider, LV_PART_KNOB) == LV_RADIUS_CIRCLE);

  int pad_left = lv_obj_get_style_pad_left(slider, LV_PART_KNOB);
  int pad_right = lv_obj_get_style_pad_right(slider, LV_PART_KNOB);
  int pad_top = lv_obj_get_style_pad_top(slider, LV_PART_KNOB);
  int pad_bottom = lv_obj_get_style_pad_bottom(slider, LV_PART_KNOB);
  assert(pad_left == 14 && pad_right == 14 && pad_top == 14 && pad_bottom == 14);
  /* LVGL draws the knob from the rail height plus its per-side padding. */
  assert((int)lv_obj_get_height(slider) + pad_top + pad_bottom == 36);
  assert((int)lv_obj_get_height(slider) + pad_left + pad_right == 36);

  assert(click_area.x1 == rail.x1 - 20 && click_area.x2 == rail.x2 + 20);
  assert(click_area.y1 == rail.y1 - 20 && click_area.y2 == rail.y2 + 20);
  assert(click_area.y2 - click_area.y1 + 1 == 48);
  assert(click_area.y1 + click_area.y2 == rail.y1 + rail.y2);
}

static void slider_pointer_endpoint(lv_obj_t *slider, bool maximum)
{
  lv_obj_update_layout(lv_screen_active());
  lv_area_t area, click_area;
  lv_obj_get_coords(slider, &area);
  lv_obj_get_click_area(slider, &click_area);
  pointer_data.point = (lv_point_t){maximum ? area.x2 : area.x1,
    maximum ? click_area.y2 - 1 : click_area.y1 + 1};
  pointer_data.state = LV_INDEV_STATE_PRESSED;
  lv_indev_read(pointer); release();
}

static int slider_pointer_drag_to(lv_obj_t *slider, bool start_at_maximum, unsigned percent)
{
  assert(percent > 0 && percent < 100);
  lv_obj_update_layout(lv_screen_active());
  lv_area_t area;
  lv_obj_get_coords(slider, &area);
  int original = lv_slider_get_value(slider);
  pointer_data.point = (lv_point_t){start_at_maximum ? area.x2 : area.x1,
                                    (area.y1 + area.y2) / 2};
  pointer_data.state = LV_INDEV_STATE_PRESSED;
  lv_indev_read(pointer);
  pointer_data.point.x = area.x1 + ((area.x2 - area.x1) * (int)percent) / 100;
  lv_tick_inc(30);
  lv_indev_read(pointer);
  assert(lv_slider_is_dragged(slider));
  int value = lv_slider_get_value(slider);
  assert(value != original);
  release();
  assert(!lv_slider_is_dragged(slider));
  return value;
}

static void test_alarm_sound_snooze_layout_and_drafts(void)
{
  const char *sounds[] = {"Classic Beep", "Gentle Chime", "Rising Tones"};
  const char *descriptions[] = {"A clear, repeating beep.", "A short musical chime.", "A sequence of ascending tones."};
  for (unsigned night = 0; night < 2; ++night) {
    fresh_editor(); wac_settings_apply_night(night);
    click("Alarm Settings");
    menu_heading("Alarm", night);
    operation_label("Alarm off", 180, 169, 440, &lv_font_montserrat_28, true, night);
    menu_row("Alarm", "Off", 212, false, night); menu_switch("Alarm", false, night);
    menu_row("Alarm Time", "7:00 AM", 320, false, night); row_arrow("Alarm Time", night);
    menu_row("Repeat", "Mon–Fri", 428, false, night); row_arrow("Repeat", night);
    menu_row("Sound & Snooze", NULL, 536, false, night); row_arrow("Sound & Snooze", night);
    menu_box(menu_button("Cancel", NULL), 178, 644, 206, 88);
    menu_box(menu_button("Save", NULL), 412, 644, 210, 88);
    tap(menu_button("Alarm", "Off"));
    menu_switch("Alarm", true, night);
    operation_label("Next alarm unavailable", 180, 169, 440, &lv_font_montserrat_28, true, night);
    assert(!current().alarm_enabled && fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
    click("Sound & Snooze");
    menu_heading("Sound & Snooze", night);
    menu_row("Alarm Sound", "Classic Beep", 212, false, night); row_arrow("Alarm Sound", night);
    menu_row("Snooze", "10 Minutes", 320, false, night); row_arrow("Snooze", night);
    operation_label("Volume", 150, 452, 270, &lv_font_montserrat_32, false, night);
    operation_label("50%", 498, 452, 150, &lv_font_montserrat_32, false, night);
    operation_label("Sound rises gently to this level", 150, 598, 500, &lv_font_montserrat_26, true, night);
    lv_obj_t *volume_slider = alarm_volume_slider();
    slider_geometry(volume_slider, 175, 532, night);
    menu_box(menu_button("Done", NULL), 270, 676, 260, 80);
    click("Alarm Sound");
    menu_heading("Alarm Sound", night);
    operation_label("Volume · 50%", 150, 174, 500, &lv_font_montserrat_28, true, night);
    for (unsigned selected = 0; selected < 3; ++selected) {
      tap(menu_button(sounds[selected], descriptions[selected]));
      for (unsigned option = 0; option < 3; ++option) {
        lv_obj_t *button = menu_button(sounds[option], descriptions[option]);
        menu_box(button, 140, 212 + (int)option * 108, 520, 96);
        menu_button_style(button, option == selected, night);
        assert(lv_obj_get_style_text_font(menu_label(button, sounds[option]), 0) == &lv_font_montserrat_28);
        const lv_font_t *font = lv_obj_get_style_text_font(menu_label(button, descriptions[option]), 0);
        assert(font->line_height == lv_font_montserrat_24.line_height && font->base_line == lv_font_montserrat_24.base_line);
        menu_color(menu_label(button, descriptions[option]), night ? 0xc17a7a : 0xa6b6cf, false);
        assert((image_source(button, &wac_icon_check) != NULL) == (option == selected));
      }
    }
    lv_obj_t *preview = menu_button("Preview Sound", NULL);
    menu_box(preview, 140, 552, 520, 96);
    assert(lv_obj_has_state(preview, LV_STATE_DISABLED));
    assert(lv_obj_get_style_opa(preview, 0) == LV_OPA_40);
    lv_obj_t *icon = image_source(preview, &wac_icon_volume_2);
    assert(icon && lv_obj_get_width(icon) == 32 && lv_obj_get_height(icon) == 32);
    lv_area_t preview_text, preview_icon;
    lv_obj_get_coords(menu_label(preview, "Preview Sound"), &preview_text); lv_obj_get_coords(icon, &preview_icon);
    assert(preview_text.x1 - preview_icon.x2 - 1 == 14);
    assert(abs(preview_icon.x1 + preview_text.x2 - 799) <= 1);
    assert(lv_color_eq(lv_obj_get_style_image_recolor(icon, 0), lv_color_hex(night ? 0xecc0c0 : 0xf5f8ff)));
    tap(preview); lv_obj_send_event(preview, LV_EVENT_CLICKED, NULL);
    assert(!menu_label(lv_screen_active(), "Stop Preview") && task_count == 0 && fake_nvs_calls(FAKE_SET) == 0);
    menu_box(menu_button("Done", NULL), 270, 676, 260, 80);
    click("Done");
    assert(menu_button("Alarm Sound", "Rising Tones"));
    click("Snooze"); menu_heading("Snooze", night);
    for (unsigned selected = 0; selected < 3; ++selected) {
      char chosen[24]; snprintf(chosen, sizeof(chosen), "%u Minutes", 5 * (selected + 1));
      tap(menu_button(chosen, NULL));
      for (unsigned option = 0; option < 3; ++option) {
        char label[24]; snprintf(label, sizeof(label), "%u Minutes", 5 * (option + 1));
        lv_obj_t *button = menu_button(label, NULL);
        menu_box(button, 170, 244 + (int)option * 122, 460, 104);
        menu_button_style(button, option == selected, night);
        lv_obj_t *check = image_source(button, &wac_icon_check);
        assert((check != NULL) == (option == selected));
        if (check) {
          lv_area_t text_area, icon_area;
          lv_obj_get_coords(menu_label(button, label), &text_area); lv_obj_get_coords(check, &icon_area);
          assert(lv_obj_get_width(check) == 32 && lv_obj_get_height(check) == 32);
          if (icon_area.x1 - text_area.x2 - 1 != 14)
            fprintf(stderr, "Snooze icon gap: text %d..%d icon %d..%d\n", text_area.x1, text_area.x2, icon_area.x1, icon_area.x2);
          assert(icon_area.x1 - text_area.x2 - 1 == 14);
          assert(abs((text_area.x1 + icon_area.x2) - (170 + 629)) <= 1);
        }
      }
    }
    menu_box(menu_button("Done", NULL), 270, 662, 260, 96);
    click("Done"); assert(menu_button("Snooze", "15 Minutes"));
    lv_obj_t *slider = alarm_volume_slider(), *value = box_label(498, 452, 150);
    lv_slider_set_value(slider, 13, LV_ANIM_OFF);
    assert(!strcmp(lv_label_get_text(value), "50%"));
    slider_pointer_endpoint(slider, false);
    assert(lv_slider_get_value(slider) == 2 && !strcmp(lv_label_get_text(value), "10%"));
    slider_pointer_endpoint(slider, true);
    assert(lv_slider_get_value(slider) == 20 && !strcmp(lv_label_get_text(value), "100%"));
    int dragged_volume = slider_pointer_drag_to(slider, true, 65);
    char dragged_text[8]; snprintf(dragged_text, sizeof(dragged_text), "%d%%", dragged_volume * 5);
    assert(!strcmp(lv_label_get_text(value), dragged_text));
    slider_pointer_endpoint(slider, true);
    assert(lv_slider_get_value(slider) == 20 && !strcmp(lv_label_get_text(value), "100%"));
    uint32_t key = LV_KEY_LEFT;
    lv_obj_send_event(slider, LV_EVENT_KEY, &key);
    assert(lv_slider_get_value(slider) == 19 && !strcmp(lv_label_get_text(value), "95%"));
    key = LV_KEY_RIGHT; lv_obj_send_event(slider, LV_EVENT_KEY, &key);
    assert(lv_slider_get_value(slider) == 20 && !strcmp(lv_label_get_text(value), "100%"));
    assert(box_label(498, 452, 150) == value && alarm_volume_slider() == slider);
    assert(lv_color_eq(lv_obj_get_style_bg_color(slider, LV_PART_INDICATOR), lv_color_hex(night ? 0xc17a7a : 0x78b8ff)));
    assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
    click("Done"); click("Sound & Snooze");
    assert(menu_button("Alarm Sound", "Rising Tones") && menu_button("Snooze", "15 Minutes"));
    assert(!strcmp(lv_label_get_text(box_label(498, 452, 150)), "100%"));
    header_back(); click("Cancel");
    device_config_t applied = current();
    assert(!applied.alarm_enabled && applied.alarm_sound == DEVICE_CONFIG_ALARM_CLASSIC_BEEP
           && applied.alarm_snooze_minutes == 10 && applied.alarm_volume == 50);
    assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
  }
  wac_settings_apply_night(false);
}

static void test_saved_volume_is_preserved_until_input(void)
{
  const unsigned levels[] = {0, 7, 37};
  for (unsigned i = 0; i < sizeof(levels) / sizeof(*levels); ++i) {
    fresh_editor();
    device_config_t baseline = current(), saved = baseline;
    saved.alarm_volume = levels[i];
    assert(device_config_save_alarm(&baseline, &saved) == ESP_OK);
    wac_ui_settings_close(); open_editor(); fake_nvs_clear_activity();
    click("Alarm Settings"); click("Sound & Snooze");
    char text[16]; snprintf(text, sizeof(text), "%u%%", levels[i]);
    operation_label(text, 498, 452, 150, &lv_font_montserrat_32, false, false);
    alarm_volume_slider();
    click("Alarm Sound"); click("Done"); click("Done");
    assert(task_count == 0 && fake_nvs_calls(FAKE_SET) == 0);
    click("Cancel");
    assert(current().alarm_volume == levels[i]);
    click("Alarm Settings"); click("Sound & Snooze"); click("Alarm Sound"); click("Done"); click("Done");
    click("Alarm Time"); picker_set(8, 5, true); click("Done");
    click("Save"); assert(task_count == 1); run_task();
    device_config_t applied = current();
    assert(applied.alarm_volume == levels[i] && applied.alarm_hour == 8 && applied.alarm_minute == 5);
    assert(view == WAC_SETTINGS_ENTRY && restart_count == 0);
  }
}

static void test_solar_layout_and_actual_location(void)
{
  for (unsigned ready = 0; ready < 2; ++ready) {
    fresh_editor();
    if (ready) {
      device_config_t baseline = current(), saved = baseline;
      strcpy(saved.location, "Cincinnati, Ohio, US");
      strcpy(saved.latitude, "39.1031"); strcpy(saved.longitude, "-84.5120");
      strcpy(saved.timezone, "America/New_York"); saved.location_ready = true;
      assert(device_config_save_weather(&baseline, &saved) == ESP_OK);
      wac_ui_settings_close(); open_editor();
    }
    fake_nvs_clear_activity();
    click("Device Setup"); click("Night Shift"); click("Sunrise/Sunset Offsets");
    lv_obj_t *location = menu_label(lv_screen_active(), ready ? "Cincinnati, Ohio, US" : "Not available");
    assert(location);
    for (unsigned night = 0; night < 2; ++night) {
      wac_settings_apply_night(night);
      menu_heading("Sunrise/Sunset", night);
      operation_label("Weather Location", 150, 174, 500, &lv_font_montserrat_24, true, night);
      operation_label(ready ? "Cincinnati, Ohio, US" : "Not available", 150, 210, 500, &lv_font_montserrat_28, false, night);
      assert(menu_label(lv_screen_active(), ready ? "Cincinnati, Ohio, US" : "Not available") == location);
      assert(lv_label_get_long_mode(location) == LV_LABEL_LONG_DOT);
      assert(lv_obj_get_height(location) <= 2 * lv_font_montserrat_28.line_height);
      const char *titles[] = {"Night Starts", "Night Ends"};
      const char *details[] = {"At sunset", "At sunrise"};
      for (unsigned row = 0; row < 2; ++row) {
        lv_obj_t *button = menu_button(titles[row], details[row]);
        menu_box(button, 140, 280 + (int)row * 108, 520, 96);
        menu_button_style(button, false, night);
        lv_obj_t *title = menu_label(button, titles[row]), *detail = menu_label(button, details[row]);
        assert(lv_obj_get_style_text_font(title, 0) == &lv_font_montserrat_28);
        assert(lv_obj_get_style_text_font(detail, 0) == &lv_font_montserrat_22);
        assert(lv_label_get_long_mode(detail) == LV_LABEL_LONG_CLIP);
        menu_color(title, night ? 0xecc0c0 : 0xf5f8ff, false);
        menu_color(detail, night ? 0xc17a7a : 0xa6b6cf, false);
        lv_area_t title_area, detail_area; lv_obj_get_coords(title, &title_area); lv_obj_get_coords(detail, &detail_area);
        assert(title_area.x1 == detail_area.x1 && detail_area.y1 - title_area.y2 - 1 == 6);
        lv_obj_t *time = menu_label(button, "--:--");
        assert(lv_obj_get_style_text_font(time, 0) == &lv_font_montserrat_28);
        menu_color(time, night ? 0xe1b6b6 : 0x93d1ff, false);
        lv_area_t time_area; lv_obj_get_coords(time, &time_area);
        assert(time_area.x1 > detail_area.x1 && time_area.y1 > title_area.y1);
        row_arrow(titles[row], night);
      }
      operation_label("Sunrise/sunset times unavailable", 150, 518, 500, &lv_font_montserrat_28, false, night);
      operation_label("Keeps the current appearance and\nbrightness until times are available.", 140, 566, 520, &lv_font_montserrat_24, true, night);
      menu_box(menu_button("Done", NULL), 270, 664, 260, 80);
    }
    tap(menu_button("Night Starts", "At sunset")); tap(menu_button("Before", NULL)); click("Done");
    assert(menu_button("Night Starts", "15 min before sunset"));
    tap(menu_button("Night Ends", "At sunrise")); tap(menu_button("After", NULL)); click("Done");
    assert(menu_button("Night Ends", "15 min after sunrise"));
    assert(task_count == 0 && fake_nvs_calls(FAKE_SET) == 0);
    click("Done"); header_back(); click("Cancel");
    device_config_t applied = current();
    assert(applied.night_sunset_offset_minutes == 0 && applied.night_sunrise_offset_minutes == 0);
    assert(applied.location_ready == (bool)ready);
    assert(task_count == 0 && fake_nvs_calls(FAKE_SET) == 0);
  }
  wac_settings_apply_night(false);
}

static void test_nested_done_and_cancel(void)
{
  fresh_editor();
  click("Alarm Settings");
  click("Alarm Time");
  picker_set(6, 30, true);
  click("Done");
  assert(view == WAC_SETTINGS_ALARM);
  assert(menu_button("Alarm Time", "6:30 AM"));
  click("Sound & Snooze");
  click("Alarm Sound");
  click("Gentle Chime");
  click("Done");
  click("Done");
  click("Sound & Snooze");
  assert(menu_button("Alarm Sound", "Gentle Chime"));
  header_back();
  assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
  click("Cancel");
  click("Alarm Settings");
  assert(menu_button("Alarm Time", "7:00 AM"));
  click("Sound & Snooze");
  assert(menu_button("Alarm Sound", "Classic Beep"));
  header_back();
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
  lv_obj_update_layout(lv_screen_active());
  tap(menu_back(lv_screen_active()));
  assert(menu_button("Device Settings", "Draft room"));
  assert(menu_button("Wi-Fi", "Home network · Not Connected"));
  click("Cancel");
  device_config_t value = current();
  assert(!strcmp(value.room_name, "Clock") && !value.wifi_ssid[0]);
  assert(fake_nvs_calls(FAKE_SET) == 0);
}

static void test_brightness_limits_and_cancel(void)
{
  const int endpoints[] = {20, 100};
  for (unsigned endpoint = 0; endpoint < 2; ++endpoint) {
    fresh_editor();
    click("Device Setup");
    click("Night Shift");
    click("Brightness");
    bool night = endpoint != 0;
    wac_settings_apply_night(night);
    menu_heading("Brightness", night);
    operation_label("Uses the brightness for the active\nday or night mode.", 150, 570, 500, &lv_font_montserrat_28, true, night);
    menu_box(menu_button("Done", NULL), 270, 664, 260, 80);
    for (unsigned index = 0; index < 2; ++index) {
      int top = index ? 412 : 224;
      operation_label(index ? "Night Brightness" : "Day Brightness", 150, top, 350,
                      &lv_font_montserrat_32, false, night);
      lv_obj_t *value_label = box_label(510, top, 140);
      assert(!strcmp(lv_label_get_text(value_label), index ? "30%" : "80%"));
      unsigned slider_index = index;
      lv_obj_t *slider = find_type(lv_screen_active(), &lv_slider_class, &slider_index);
      assert(slider && lv_slider_get_min_value(slider) == 20
             && lv_slider_get_max_value(slider) == 100);
      slider_geometry(slider, 175, top + 88, night);
      slider_pointer_endpoint(slider, endpoint != 0);
      assert(lv_slider_get_value(slider) == endpoints[endpoint]);
      assert(box_label(510, top, 140) == value_label);
      int dragged_brightness = slider_pointer_drag_to(slider, endpoint != 0, 65);
      char dragged_text[8]; snprintf(dragged_text, sizeof(dragged_text), "%d%%", dragged_brightness);
      assert(!strcmp(lv_label_get_text(value_label), dragged_text));
      slider_pointer_endpoint(slider, endpoint != 0);
      assert(lv_slider_get_value(slider) == endpoints[endpoint]);
      lv_slider_set_value(slider, endpoint == 0 ? 19 : 100, LV_ANIM_OFF);
      assert(lv_slider_get_value(slider) == endpoints[endpoint]);
      lv_obj_send_event(slider, LV_EVENT_VALUE_CHANGED, NULL);
      assert(box_label(510, top, 140) == value_label);
      assert(!strcmp(lv_label_get_text(value_label), endpoint ? "100%" : "20%"));
    }
    assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
    click("Done");
    char value[48];
    snprintf(value, sizeof(value), "Day %d%% · Night %d%%",
             endpoints[endpoint], endpoints[endpoint]);
    assert(menu_button("Brightness", value));
    click("Brightness");
    assert(!strcmp(lv_label_get_text(box_label(510, 224, 140)), endpoint ? "100%" : "20%"));
    assert(!strcmp(lv_label_get_text(box_label(510, 412, 140)), endpoint ? "100%" : "20%"));
    header_back();
    click("Done");
    click("Cancel");
    device_config_t applied = current();
    assert(applied.day_brightness == 80 && applied.night_brightness == 30);
    assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
  }
  wac_settings_apply_night(false);
}

static void test_selected_time_format(void)
{
  fresh_editor();
  click("Alarm Settings");
  click("Alarm Time");
  picker_set(12, 0, true);
  click("Done");
  assert(menu_button("Alarm Time", "12:00 PM"));
  click("Cancel");
  assert(fake_nvs_calls(FAKE_SET) == 0);
  device_config_t baseline = current(), draft = baseline;
  strcpy(draft.time_format, "24");
  assert(device_config_save_device(&baseline, &draft) == ESP_OK);
  wac_ui_settings_close();
  open_editor();
  click("Alarm Settings");
  click("Alarm Time");
  picker_set(18, 45, false);
  click("Done");
  assert(menu_button("Alarm Time", "18:45"));
  click("Cancel");
  baseline = current(); draft = baseline;
  strcpy(draft.time_format, "12");
  draft.leading_zero_12h = true;
  assert(device_config_save_device(&baseline, &draft) == ESP_OK);
  wac_ui_settings_close();
  open_editor();
  click("Alarm Settings");
  assert(menu_button("Alarm Time", "7:00 AM"));
  click("Cancel");
}

static void test_alarm_failed_save_retry_and_toggle(void)
{
  fresh_editor();
  click("Alarm Settings");
  click("Alarm Time");
  picker_set(6, 30, true);
  click("Done");
  click("Repeat"); tap(menu_button("Every Day", NULL));
  const char *weekdays[] = {"Mon", "Tue", "Wed", "Thu", "Fri"};
  for (unsigned i = 0; i < 5; ++i) tap(menu_button(weekdays[i], NULL));
  click("Done");
  tap(menu_button("Alarm", "Off"));
  click("Sound & Snooze"); click("Alarm Sound"); tap(menu_button("Rising Tones", NULL)); click("Done");
  click("Snooze"); tap(menu_button("15 Minutes", NULL)); click("Done");
  lv_obj_t *volume = alarm_volume_slider();
  lv_slider_set_value(volume, 13, LV_ANIM_OFF); lv_obj_send_event(volume, LV_EVENT_VALUE_CHANGED, NULL);
  assert(!strcmp(lv_label_get_text(box_label(498, 452, 150)), "65%")); click("Done");
  device_config_t baseline = current(), newer = baseline;
  strcpy(newer.room_name, "New room saved elsewhere");
  assert(device_config_save_device(&baseline, &newer) == ESP_OK);
  fake_nvs_clear_activity();
  fail_task = true;
  click("Save");
  assert(task_count == 0 && view == WAC_SETTINGS_ALARM);
  assert(menu_label(lv_screen_active(), "Could not save. Try again."));
  fake_nvs_fail(FAKE_COMMIT, 1);
  click("Save");
  assert(task_count == 1 && fake_nvs_calls(FAKE_SET) == 0);
  operation_label("Saving settings...", 180, 169, 440, &lv_font_montserrat_28, true, false);
  const char *pending_controls[] = {"Alarm", "Alarm Time", "Repeat", "Sound & Snooze", "Cancel", "Save"};
  for (unsigned i = 0; i < sizeof(pending_controls) / sizeof(*pending_controls); ++i)
    assert(lv_obj_has_state(menu_button(pending_controls[i], NULL), LV_STATE_DISABLED));
  lv_obj_update_layout(lv_screen_active());
  lv_obj_t *back = menu_back(lv_screen_active());
  assert(back && lv_obj_has_state(back, LV_STATE_DISABLED));
  lv_obj_send_event(back, LV_EVENT_CLICKED, NULL);
  assert(task_count == 1); /* Callback also rejects duplicate submission. */
  run_task();
  assert(view == WAC_SETTINGS_ALARM && restart_count == 0);
  assert(menu_button("Alarm Time", "6:30 AM"));
  operation_label("Could not save. Try again.", 180, 169, 440, &lv_font_montserrat_28, true, false);
  device_config_t value = current();
  assert(value.alarm_hour == 7 && !value.alarm_enabled && value.alarm_repeat_days == 0x3e
         && value.alarm_sound == DEVICE_CONFIG_ALARM_CLASSIC_BEEP && value.alarm_snooze_minutes == 10 && value.alarm_volume == 50);
  assert(!strcmp(value.room_name, newer.room_name));
  fake_nvs_clear_activity();
  click("Save");
  run_task();
  value = current();
  assert(value.alarm_hour == 6 && value.alarm_minute == 30 && value.alarm_enabled
         && value.alarm_repeat_days == 0x41 && value.alarm_sound == DEVICE_CONFIG_ALARM_RISING_TONES
         && value.alarm_snooze_minutes == 15 && value.alarm_volume == 65);
  assert(!strcmp(value.room_name, newer.room_name));
  assert(view == WAC_SETTINGS_ENTRY && restart_count == 0);
  fake_nvs_clear_activity();
  menu_switch("Alarm", true, false);
  tap(menu_button("Alarm", "On"));
  assert(task_count == 1 && fake_nvs_calls(FAKE_SET) == 0);
  run_task();
  value = current();
  assert(!value.alarm_enabled && value.alarm_hour == 6);
  assert(view == WAC_SETTINGS_ENTRY && restart_count == 0);
  menu_switch("Alarm", false, false);
}

static void test_required_editor_fields_before_submission(void)
{
  fresh_editor();
  click("Alarm Settings");
  click("Repeat");
  const char *weekdays[] = {"Mon", "Tue", "Wed", "Thu", "Fri"};
  for (unsigned i = 0; i < sizeof(weekdays) / sizeof(weekdays[0]); ++i) tap(menu_button(weekdays[i], NULL));
  assert(lv_obj_has_state(menu_button("Done", NULL), LV_STATE_DISABLED));
  tap(menu_button("Done", NULL));
  assert(view == WAC_SETTINGS_REPEAT);
  header_back();
  click("Save");
  assert(task_count == 0 && view == WAC_SETTINGS_ALARM);
  assert(menu_label(lv_screen_active(), "Choose at least one day."));
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
  assert(!value.wifi_ssid[0] && !value.wifi_ready);
  assert(view == WAC_SETTINGS_ENTRY && restart_count == 0);
  lv_tick_inc(1000);
  lv_timer_handler();
  assert(restart_count == 0);
}

static void test_weather_error_typing_notice(void)
{
  static const struct {
    esp_err_t lookup_error;
    bool offline, fail_save;
    const char *normal_prefix, *typing_notice;
  } cases[] = {
    {ESP_ERR_NOT_FOUND, false, false, "Location Not Found\nTry a city and state.", "Location not found. Try City, State."},
    {ESP_ERR_TIMEOUT, false, false, "Couldn’t check this location.\nTry Save again in a moment.", "Couldn’t check location. Try again."},
    {ESP_OK, true, false, "Wi-Fi Required.\nConnect before saving a location.", NULL},
    {ESP_OK, false, true, "Couldn’t save settings.\nTry Save again.", NULL},
  };
  for (unsigned i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
    fresh_editor();
    click("Weather Location");
    field(0, "Cincinnati,OH,US");
    lookup_result = cases[i].lookup_error;
    wifi_connected = !cases[i].offline;
    if (cases[i].fail_save) fake_nvs_fail(FAKE_COMMIT, 1);
    tap(menu_button("Save", NULL));
    assert(task_count == 1);
    run_task();
    assert(view == WAC_SETTINGS_WEATHER && restart_count == 0);
    lv_obj_t *normal = operation_label(cases[i].normal_prefix, 150, 548, 500,
                                       &lv_font_montserrat_26, true, false);
    char retained[192];
    snprintf(retained, sizeof(retained), "%s", lv_label_get_text(normal));
    unsigned writes = fake_nvs_calls(FAKE_SET), lookups = lookup_count;
    lv_obj_t *input = menu_field("City, State");
    char draft[DEVICE_CONFIG_STR_LEN];
    snprintf(draft, sizeof(draft), "%s", lv_textarea_get_text(input));
    tap(input);
    unsigned index = 0;
    lv_obj_t *keyboard = find_type(lv_screen_active(), &lv_keyboard_class, &index);
    assert(menu_visible(keyboard) && lv_keyboard_get_textarea(keyboard) == input);
    menu_field_geometry("City, State", 224, false);
    if (cases[i].typing_notice) {
      lv_obj_t *notice = menu_label(lv_screen_active(), cases[i].typing_notice);
      assert(notice && menu_visible(notice));
      menu_box(notice, 150, 178, 500, -1);
      lv_area_t area;
      lv_obj_get_coords(notice, &area);
      assert(area.y2 < 224);
      if (strstr(cases[i].typing_notice, "’")) {
        lv_font_glyph_dsc_t glyph;
        assert(lv_font_get_glyph_dsc(lv_obj_get_style_text_font(notice, 0),
                                     &glyph, 0x2019, 't'));
        assert(!glyph.is_placeholder && glyph.adv_w > 0 && glyph.box_w > 0);
      }
    } else {
      assert(!menu_visible(menu_label(lv_screen_active(), retained)));
      /* Hiding a normal error must not leave any other visible wrapping
       * label in the gap occupied by the field caption. */
      lv_obj_t *root = lv_obj_get_parent(menu_label(lv_screen_active(), "Weather Location"));
      for (uint32_t child = 0; child < lv_obj_get_child_count(root); ++child) {
        lv_obj_t *obj = lv_obj_get_child(root, child);
        if (!menu_visible(obj) || !lv_obj_check_type(obj, &lv_label_class)
            || !lv_label_get_text(obj)[0]) continue;
        lv_area_t area;
        lv_obj_get_coords(obj, &area);
        assert(area.y1 >= 224 || area.y2 < 224);
      }
    }
    keyboard_key(keyboard, LV_SYMBOL_CLOSE, false);
    normal = menu_label(lv_screen_active(), retained);
    assert(normal && menu_visible(normal));
    menu_box(normal, 150, 548, 500, -1);
    assert(!strcmp(lv_textarea_get_text(input), draft));
    assert(fake_nvs_calls(FAKE_SET) == writes && lookup_count == lookups && task_count == 0);
    lookup_result = ESP_OK;
    wifi_connected = true;
    click("Cancel");
  }
}

static void test_credentials_save_failure_and_restart(void)
{
  fresh_editor();
  click("Device Setup");
  click("Device Settings");
  field(0, "Kitchen clock");
  click("Done");
  click("Display Settings");
  click("Home Screen");
  click("Done");
  click("Night Shift"); click("Mode");
  tap(menu_button("Fixed Schedule", "Switch at your set times")); click("Done");
  click("Fixed Schedule"); click("Night Starts"); picker_set(23, 55, true); click("Done");
  click("Night Ends"); picker_set(0, 5, true); click("Done"); click("Done");
  click("Sunrise/Sunset Offsets"); click("Night Starts");
  tap(menu_button("Before", NULL)); tap(box_button(550, 370, 110, 104)); click("Done");
  click("Night Ends"); tap(menu_button("After", NULL));
  tap(box_button(140, 370, 110, 104)); click("Done"); click("Done");
  click("Brightness");
  for (unsigned i = 0; i < 2; ++i) {
    unsigned index = i;
    lv_obj_t *slider = find_type(lv_screen_active(), &lv_slider_class, &index);
    lv_slider_set_value(slider, i ? 99 : 21, LV_ANIM_OFF);
    lv_obj_send_event(slider, LV_EVENT_VALUE_CHANGED, NULL);
  }
  click("Done"); click("Done");
  assert(fake_nvs_calls(FAKE_SET) == 0 && task_count == 0);
  click("Wi-Fi");
  run_task();
  click("Manual Network");
  field(0, "Saved manual network");
  field(1, "actual-password");
  fake_nvs_fail(FAKE_COMMIT, 1);
  tap(menu_button("Save & Restart", NULL));
  assert(task_count == 1);
  assert(lv_obj_has_state(menu_back(lv_screen_active()), LV_STATE_DISABLED));
  lv_obj_send_event(menu_back(lv_screen_active()), LV_EVENT_CLICKED, NULL);
  assert(view == WAC_SETTINGS_CREDENTIALS && task_count == 1);
  run_task();
  assert(view == WAC_SETTINGS_CREDENTIALS && restart_count == 0);
  operation_label("Couldn’t save settings.\nTry Save & Restart again.", 160, 548, 480,
                  &lv_font_montserrat_24, true, false);
  assert(!strcmp(lv_textarea_get_text(menu_field("Network Name")), "Saved manual network"));
  assert(!strcmp(lv_textarea_get_text(menu_field("Password")), "actual-password"));
  assert(fake_nvs_calls(FAKE_SET) > 0);
  fake_nvs_clear_activity();
  tap(menu_button("Save & Restart", NULL));
  assert(task_count == 1 && fake_nvs_calls(FAKE_SET) == 0);
  run_task();
  menu_heading("Wi-Fi", false);
  assert(lv_obj_has_state(menu_back(lv_screen_active()), LV_STATE_DISABLED));
  lv_obj_t *network = operation_label("Saved manual network", 150, 202, 500,
                                     &lv_font_montserrat_28, true, false);
  assert(lv_label_get_long_mode(network) == LV_LABEL_LONG_DOT);
  operation_mark(&wac_icon_circle_check_mark, false);
  operation_label("Settings Saved", 140, 392, 520, &lv_font_montserrat_36, false, false);
  operation_label("Restarting to join Wi-Fi…", 150, 456, 500,
                  &lv_font_montserrat_26, true, false);
  unsigned index = 0;
  assert(!find_type(lv_screen_active(), &lv_textarea_class, &index));
  index = 0;
  assert(!find_type(lv_screen_active(), &lv_keyboard_class, &index));
  assert(!menu_label(lv_screen_active(), "Back to Clock"));
  assert(!menu_label(lv_screen_active(), "Save & Restart"));
  device_config_t value = current();
  assert(!strcmp(value.room_name, "Kitchen clock") && !strcmp(value.default_clock_face, "analog"));
  assert(value.wifi_ready && !strcmp(value.wifi_ssid, "Saved manual network")
         && !strcmp(value.wifi_password, "actual-password"));
  assert(!value.location_ready && !value.alarm_enabled);
  assert(value.night_shift_mode == DEVICE_CONFIG_NIGHT_FIXED);
  assert(!strcmp(value.night_shift_start, "23:55") && !strcmp(value.night_shift_end, "00:05"));
  assert(value.night_sunset_offset_minutes == -20 && value.night_sunrise_offset_minutes == 10);
  assert(value.day_brightness == 21 && value.night_brightness == 99);
  unsigned writes = fake_nvs_calls(FAKE_SET);
  wac_settings_apply_night(true);
  menu_heading("Wi-Fi", true);
  operation_mark(&wac_icon_circle_check_mark, true);
  operation_label("Restarting to join Wi-Fi…", 150, 456, 500,
                  &lv_font_montserrat_26, true, true);
  assert(fake_nvs_calls(FAKE_SET) == writes && restart_count == 0);
  lv_tick_inc(1000);
  lv_timer_handler();
  assert(restart_count == 1);
}

static void test_weather_unchanged_location_saved(void)
{
  fresh_editor();
  device_config_t baseline = current(), saved = baseline;
  strcpy(saved.location, "Cincinnati, Ohio, US");
  strcpy(saved.latitude, "39.1031");
  strcpy(saved.longitude, "-84.5120");
  strcpy(saved.timezone, "America/New_York");
  saved.location_ready = true;
  strcpy(saved.units, "metric");
  assert(device_config_save_weather(&baseline, &saved) == ESP_OK);
  wac_ui_settings_close();
  open_editor();
  wac_settings_apply_night(true);
  fake_nvs_clear_activity();
  unsigned lookups = lookup_count;
  click("Weather Location");
  tap(menu_button("Save", NULL));
  weather_pending(false, true);
  run_task();
  assert(lookup_count == lookups);
  weather_saved("Settings Saved", true);
  unsigned writes = fake_nvs_calls(FAKE_SET);
  wac_settings_apply_night(false);
  weather_saved("Settings Saved", false);
  assert(fake_nvs_calls(FAKE_SET) == writes && restart_count == 0);
  lv_tick_inc(1000);
  lv_timer_handler();
  assert(restart_count == 1);
}

static void test_weather_failure_retains_draft_and_success_restarts(void)
{
  fresh_editor();
  click("Weather Location");
  click("Save");
  assert(task_count == 0 && fake_nvs_calls(FAKE_SET) == 0);
  assert(menu_visible(menu_label(lv_screen_active(), "Enter a city and state before saving.")));
  field(0, "Cincinnati,OH,US");
  unsigned keyboard_index = 0;
  lv_obj_t *keyboard = find_type(lv_screen_active(), &lv_keyboard_class, &keyboard_index);
  assert(keyboard && menu_visible(keyboard));
  keyboard_key(keyboard, LV_SYMBOL_OK, false);
  click("Units");
  wifi_connected = false;
  click("Save");
  weather_pending(true, false);
  run_task();
  assert(find_label(lv_screen_active(), "Wi-Fi Required"));
  wifi_connected = true;
  lookup_timezone = "auto";
  click("Save");
  weather_pending(true, false);
  run_task();
  assert(!strcmp(last_query, "Cincinnati,OH,US"));
  assert(view == WAC_SETTINGS_WEATHER && restart_count == 0);
  assert(menu_button("Units", "Metric"));
  assert(find_label(lv_screen_active(), "Couldn’t save settings."));
  device_config_t value = current();
  assert(!value.location_ready && !strcmp(value.units, "imperial"));
  assert(fake_nvs_calls(FAKE_SET) == 0);
  lookup_timezone = "America/New_York";
  field(0, "Cincinnati,OH,US");
  fake_nvs_fail(FAKE_COMMIT, 1);
  click("Save");
  weather_pending(true, false);
  run_task();
  assert(view == WAC_SETTINGS_WEATHER && restart_count == 0);
  assert(find_label(lv_screen_active(), "Couldn’t save settings."));
  value = current();
  assert(!value.location_ready && !strcmp(value.units, "imperial"));
  unsigned resolved_lookups = lookup_count;
  fake_nvs_clear_activity();
  wifi_connected = false;
  click("Save");
  weather_pending(false, false);
  run_task();
  assert(lookup_count == resolved_lookups);
  value = current();
  assert(value.location_ready && !strcmp(value.units, "metric"));
  assert(!strcmp(value.timezone, "America/New_York"));
  weather_saved("Location Saved", false);
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
      unsigned fields = next == WAC_SETTINGS_WEATHER ? 1 : 2;
      for (unsigned field_index = 0; field_index < fields; ++field_index) {
        index = field_index;
        lv_obj_t *textarea = find_type(lv_screen_active(), &lv_textarea_class, &index);
        assert(textarea);
        tap(textarea);
        assert(lv_obj_has_state(textarea, LV_STATE_FOCUSED));
        assert(!lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN));
        assert(lv_keyboard_get_textarea(keyboard) == textarea);
        lv_area_t area;
        lv_obj_get_coords(keyboard, &area);
        assert_inside_round_panel(&area);
        keyboard_key(keyboard, LV_SYMBOL_OK, false);
        assert(lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN));
        tap(textarea); /* Reopen the same field without another focus event. */
        assert(!lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN));
        assert(lv_keyboard_get_textarea(keyboard) == textarea);
        keyboard_key(keyboard, LV_SYMBOL_CLOSE, false);
      }
    }
  }
  wac_ui_settings_close();
}

int main(int argc, char **argv)
{
  lv_init();
  menu_keyboard_assets();
  test_clock_time_font();
  lv_display_t *display = lv_display_create(800, 800);
  static uint8_t buffer[800 * 32 * 4];
  lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);
  pointer = lv_indev_create();
  lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(pointer, read_pointer);
  operation_assets();
  if (argc == 2) {
    if (!strcmp(argv[1], "wifi_restart")) test_credentials_save_failure_and_restart();
    else if (!strcmp(argv[1], "weather_saved")) test_weather_unchanged_location_saved();
    else assert(!"Unknown settings scenario");
  } else {
    assert(argc == 1);
    test_settings_foundation();
    test_entry_saved_time_caption();
    test_display_save_and_truthful_wifi_draft();
    test_wifi_scan_states_and_retained_drafts();
    test_wifi_scroll_and_actual_network_content();
    test_credentials_keyboard();
    test_text_fields_and_typing_layout();
    test_missing_field_focus_and_error_clear();
    test_credentials_missing_network_focus();
    test_keyboard_only_on_text_editors();
    test_shared_time_arrow_controls();
    test_repeat_shortcuts_and_zero_guard();
    test_night_rows_modes_and_fixed_notes();
    test_offset_direction_limits_and_drafts();
    test_alarm_sound_snooze_layout_and_drafts();
    test_saved_volume_is_preserved_until_input();
    test_solar_layout_and_actual_location();
    test_nested_done_and_cancel();
    test_brightness_limits_and_cancel();
    test_selected_time_format();
    test_alarm_failed_save_retry_and_toggle();
    test_required_editor_fields_before_submission();
    test_closed_editor_ignores_save_navigation();
    test_weather_error_typing_notice();
    test_weather_failure_retains_draft_and_success_restarts();
  }
  assert(task_count == 0);
  wac_ui_settings_close();
  lv_indev_delete(pointer);
  lv_display_delete(display);
  lv_deinit();
  if (argc == 2) printf("Production settings operation checks passed: %s.\n", argv[1]);
  else puts("Production UI checks passed: settings geometry/styles, actual Wi-Fi results/scan states/scrolling, truthful captions and glyphs, keyboard maps/text fields, validation, Weather operations, drafts, cancellation, deferred saves, retry, toggle, stale completion and restart.");
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
connectivity_wifi_state_t connectivity_get_wifi_state(void)
{ return CONNECTIVITY_WIFI_CONNECTED; }
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
static bool visible(lv_obj_t *object) { return menu_visible(object); }
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
{ lv_obj_send_event(!strcmp(text, LV_SYMBOL_WARNING) ? indicator() : menu_button(text, NULL),
                    LV_EVENT_CLICKED, NULL); }
static lv_obj_t *overlay(void)
{ return lv_obj_get_parent(lv_obj_get_parent(lv_obj_get_parent(label("DISMISS")))); }
static lv_obj_t *find_widget(lv_obj_t *root, const lv_obj_class_t *type)
{
  if (lv_obj_check_type(root, type)) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t *found = find_widget(lv_obj_get_child(root, i), type);
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

/* Synthetic owner publications exercise the first download and realistic
 * retry sequence; they are not observations from a provider or the board. */
static void assert_forecast_artwork(lv_obj_t *root, bool loaded, unsigned *count)
{
  if (lv_obj_check_type(root, &lv_image_class)) {
    int width = lv_obj_get_width(root);
    if (width == 364 || width == 90) {
      ++*count;
      assert(visible(root));
      assert((lv_obj_get_style_opa(root, 0) != LV_OPA_TRANSP) == loaded);
      if (!loaded) assert(direct_label_count(lv_obj_get_parent(root), "--") == 2);
    }
  }
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
    assert_forecast_artwork(lv_obj_get_child(root, i), loaded, count);
}

static void assert_data_notice(const char *text, bool conditions)
{
  lv_obj_t *notice = find_visible_label(lv_screen_active(), text);
  assert(notice);
  menu_box(notice, conditions ? 150 : 180, conditions ? 680 : 48,
           conditions ? 500 : 440, -1);
  assert(lv_obj_get_style_text_font(notice, 0) == &lv_font_montserrat_22);
  assert(lv_obj_get_style_text_opa(notice, 0) == LV_OPA_COVER);
  assert(lv_obj_get_style_opa(notice, 0) == LV_OPA_COVER);
  assert(lv_obj_get_style_text_letter_space(notice, 0) == 0);
  assert(lv_obj_get_style_text_align(notice, 0) == LV_TEXT_ALIGN_CENTER);
  menu_color(notice, 0xa6b6cf, false);
}

static unsigned assert_section_dividers(lv_obj_t *root, bool night, bool digital)
{
  unsigned count = 0;
  lv_obj_update_layout(lv_screen_active());
  if (lv_obj_get_height(root) == 1 && lv_obj_get_width(root) > 20
      && lv_obj_get_style_bg_opa(root, 0) != LV_OPA_TRANSP) {
    lv_area_t box;
    lv_obj_get_coords(root, &box);
    bool candidate = digital ? lv_obj_get_width(root) == 500
      : (box.y1 == 500 && lv_obj_get_width(lv_obj_get_parent(root)) == 440);
    if (candidate) {
      ++count;
      assert(lv_color_eq(lv_obj_get_style_bg_color(root, 0),
                        lv_color_hex(night ? 0xe1b6b6 : 0x93d1ff)));
      assert(lv_obj_get_style_bg_opa(root, 0) == 89);
      if (digital) {
        assert(box.x1 == 150 && (box.y1 == 310 || box.y1 == 496));
        assert(visible(root) == !night);
      } else {
        lv_obj_t *row = lv_obj_get_parent(root);
        menu_box(row, 180, 490, 440, -1);
        assert(lv_obj_get_style_pad_column(row, 0) == 14);
        assert(lv_obj_get_width(lv_obj_get_child(row, 0))
          == lv_obj_get_width(lv_obj_get_child(row, 2)));
      }
    }
  }
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
    count += assert_section_dividers(lv_obj_get_child(root, i), night, digital);
  return count;
}

static void test_weather_screen_transitions(lv_indev_t *pointer)
{
  fake_nvs_reset();
  assert(device_config_init() == ESP_OK);
  device_config_t config;
  assert(device_config_copy(&config) == ESP_OK);
  config.location_ready = true;
  config.wifi_ready = true;
  fake_nvs_clear_activity();
  for (unsigned analog = 0; analog < 2; ++analog) {
    strcpy(config.default_clock_face, analog ? "analog" : "digital");
    app_runtime_state_t runtime = {0};
    runtime.clock.usable = true;
    runtime.clock.source = WAC_TIME_SOURCE_NETWORK;
    runtime.clock.utc_now = local_epoch(2026, 10, 9, 7, 6);
    assert(app_ui_show_boot_screen(&config, &runtime, true) == ESP_OK);
    lv_obj_t *waiting = find_visible_label(lv_screen_active(), "Waiting for weather…");
    assert(waiting);
    menu_box(waiting, 150, analog ? 550 : 390, 500, -1);
    const lv_font_t *waiting_font = lv_obj_get_style_text_font(waiting, 0);
    assert(waiting_font->line_height == lv_font_montserrat_28.line_height);
    lv_font_glyph_dsc_t ellipsis;
    assert(lv_font_get_glyph_dsc(waiting_font, &ellipsis, 0x2026, 0));
    assert(!ellipsis.is_placeholder && ellipsis.box_w > 0);
    assert(find_visible_label(lv_screen_active(), analog ? "Oct 9" : "OCT 9"));
    assert(!find_visible_label(lv_screen_active(), "Loading Weather"));
    assert(!find_visible_label(lv_screen_active(), "Please wait"));
    assert(!find_visible_label(lv_screen_active(), "--"));
    assert(visible_image_count(lv_screen_active(), &weather_icon_clear_day) == 0);
    if (analog) {
      assert(visible(find_image_source(lv_screen_active(), &analog_hour_hand)));
      assert(visible(find_image_source(lv_screen_active(), &analog_minute_hand)));
      assert(visible(find_image_source(lv_screen_active(), &analog_second_hand)));
    } else {
      assert(find_visible_label(lv_screen_active(), "7:06"));
    }
    runtime.night_active = true;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert(!find_visible_label(lv_screen_active(), "Waiting for weather…"));
    runtime.night_active = false;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert(find_visible_label(lv_screen_active(), "Waiting for weather…"));
    strcpy(runtime.weather_status, "Weather request failed. Retrying...");
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    swipe(pointer, 600, 200, 300, 200);
    assert_data_notice("Weather update failed.", false); /* Before another heartbeat. */
    for (unsigned night = 0; night < 2; ++night) {
      runtime.night_active = night;
      assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
      lv_obj_t *updating = find_visible_label(lv_screen_active(), "Updating…");
      assert(updating);
      menu_box(updating, 200, 290, 400, -1);
      const lv_font_t *updating_font = lv_obj_get_style_text_font(updating, 0);
      assert(updating_font->line_height == lv_font_montserrat_32.line_height);
      assert(lv_font_get_glyph_dsc(updating_font, &ellipsis, 0x2026, 0));
      assert(!ellipsis.is_placeholder && ellipsis.box_w > 0);
      menu_color(updating, night ? 0xc17a7a : 0xa6b6cf, false);
      unsigned icons = 0;
      lv_obj_t *forecast = lv_obj_get_parent(updating);
      assert_forecast_artwork(forecast, false, &icons);
      assert(icons == 5);
      if (!night) assert_data_notice("Weather update failed.", false);
      else assert(!find_visible_label(lv_screen_active(), "Weather update failed."));
    }
    runtime.night_active = false;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    swipe(pointer, 600, 200, 300, 200);
    assert_data_notice("Weather update failed.", true);
    assert(find_visible_label(lv_screen_active(), "N"));
    assert(!visible(find_image_source(lv_screen_active(), &conditions_wind_arrow)));
    runtime.night_active = true;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert(!find_visible_label(lv_screen_active(), "Weather update failed."));
    runtime.night_active = false;

    runtime.weather.valid = true;
    runtime.weather.has_conditions = true;
    runtime.weather.temp = 63;
    runtime.weather.high = 71;
    runtime.weather.low = 41;
    runtime.weather.updated_at = runtime.clock.utc_now;
    strcpy(runtime.weather.icon_name, "cloudy");
    strcpy(runtime.weather.summary, "Cloudy");
    strcpy(runtime.weather.conditions.headline, "Calm and dry");
    runtime.weather.conditions.has_speed = true;
    runtime.weather.conditions.speed = 8;
    strcpy(runtime.weather.conditions.wind_unit, "mph");
    strcpy(runtime.weather.conditions.change_label, "Conditions steady");
    const char *const dates[] = {"2026-10-10", "2026-10-11", "2026-10-12", "2026-10-13", "2026-10-14"};
    set_forecast_dates(&runtime.weather, dates);
    for (unsigned i = 0; i < APP_FORECAST_DAYS; ++i) {
      runtime.weather.forecast[i].high = 71 + i;
      runtime.weather.forecast[i].low = 41 + i;
      strcpy(runtime.weather.forecast[i].icon_name, "cloudy");
    }
    runtime.weather_status[0] = '\0';
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert(!find_visible_label(lv_screen_active(), "Weather update failed."));
    assert(find_visible_label(lv_screen_active(), "8"));
    assert(find_visible_label(lv_screen_active(), "Conditions steady"));
    assert(assert_section_dividers(lv_screen_active(), false, false) == 2);
    runtime.night_active = true;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert(assert_section_dividers(lv_screen_active(), true, false) == 2);
    runtime.night_active = false;
    runtime.weather.stale = true;
    strcpy(runtime.weather_status, "Weather refresh failed. Showing last update until retry.");
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert_data_notice("Weather data stale", true);
    assert(find_visible_label(lv_screen_active(), "8"));
    assert(find_visible_label(lv_screen_active(), "Conditions steady"));
    swipe(pointer, 300, 200, 600, 200);
    assert_data_notice("Weather data stale", false);
    assert(find_visible_label(lv_screen_active(), "71° / 41°"));
    lv_obj_t *forecast = lv_obj_get_parent(label("71° / 41°"));
    unsigned icons = 0;
    assert_forecast_artwork(forecast, true, &icons);
    assert(icons == 5 && !find_visible_label(lv_screen_active(), "Updating…"));
    runtime.night_active = true;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    icons = 0;
    assert_forecast_artwork(forecast, true, &icons);
    assert(icons == 5 && !find_visible_label(lv_screen_active(), "Weather data stale"));
    runtime.night_active = false;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    swipe(pointer, 300, 200, 600, 200);
    lv_obj_t *home_status = find_visible_label(lv_screen_active(),
      "Weather refresh failed. Showing last update until retry.");
    assert(home_status && !find_visible_label(lv_screen_active(), "Weather data stale"));
    assert(lv_obj_get_style_text_font(home_status, 0) == &lv_font_montserrat_18);
    assert(lv_obj_get_style_text_opa(home_status, 0) == LV_OPA_70);
    assert(lv_obj_get_style_text_letter_space(home_status, 0) == 1);
    assert(find_visible_label(lv_screen_active(), "63°"));
    swipe(pointer, 600, 200, 300, 200);
    assert_data_notice("Weather data stale", false);
    runtime.weather.stale = false;
    runtime.weather.updated_at += 600;
    runtime.weather_status[0] = '\0';
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert(!find_visible_label(lv_screen_active(), "Weather data stale"));
    swipe(pointer, 300, 200, 600, 200);
    assert(find_visible_label(lv_screen_active(), "63°"));
    assert(find_visible_label(lv_screen_active(), "H: 71°  L: 41°"));
    assert(!find_visible_label(lv_screen_active(), "Waiting for weather…"));
    if (!analog) {
      lv_obj_t *temp = find_visible_label(lv_screen_active(), "63°");
      lv_area_t box;
      lv_obj_update_layout(lv_screen_active());
      lv_obj_get_coords(temp, &box);
      assert(box.x1 == 135);
      lv_obj_get_coords(label("AM"), &box);
      assert(box.y1 == 267);
      assert(lv_obj_get_style_text_font(label("AM"), 0) == &lv_font_montserrat_24);
      lv_obj_get_coords(label("Cloudy"), &box);
      assert(box.x1 == 500 && lv_obj_get_width(label("Cloudy")) == 270);
      assert(assert_section_dividers(lv_screen_active(), false, true) == 2);
    }
    runtime.night_active = true;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert(!find_visible_label(lv_screen_active(), "63°"));
    assert(!find_visible_label(lv_screen_active(), "H: 71°  L: 41°"));
    if (!analog) assert(assert_section_dividers(lv_screen_active(), true, true) == 2);
    runtime.night_active = false;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert(find_visible_label(lv_screen_active(), "63°"));
  }
  assert(fake_nvs_calls(FAKE_SET) == 0);
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

static void assert_waiting_sync(bool analog, bool night)
{
  lv_obj_t *text = find_visible_label(lv_screen_active(), "Waiting for sync");
  assert(text);
  lv_obj_t *container = lv_obj_get_parent(text);
  menu_box(container, analog ? 220 : 170, analog ? 220 : 642,
           analog ? 360 : 460, analog ? 72 : 80);
  assert(lv_obj_get_style_text_font(text, 0) == &lv_font_montserrat_28);
  menu_color(text, night ? 0xc17a7a : 0xa6b6cf, false);
  lv_obj_t *icon = find_image_source(container, &wac_icon_clock);
  assert(icon && visible(icon));
  assert(lv_obj_get_width(icon) == 28 && lv_obj_get_height(icon) == 28);
  assert(lv_color_eq(lv_obj_get_style_image_recolor(icon, 0),
                    lv_color_hex(night ? 0xc17a7a : 0xa6b6cf)));
  assert(lv_obj_get_style_image_recolor_opa(icon, 0) == LV_OPA_COVER);
  assert(!find_visible_label(container, LV_SYMBOL_WARNING));
}

static void test_waiting_sync_transitions(lv_indev_t *pointer)
{
  fake_nvs_reset();
  assert(device_config_init() == ESP_OK);
  device_config_t config;
  assert(device_config_copy(&config) == ESP_OK);
  config.location_ready = true;
  config.wifi_ready = true;
  fake_nvs_clear_activity();
  for (unsigned analog = 0; analog < 2; ++analog) {
    strcpy(config.default_clock_face, analog ? "analog" : "digital");
    app_runtime_state_t runtime = {0};
    runtime.clock.usable = true;
    runtime.clock.source = WAC_TIME_SOURCE_RTC;
    runtime.clock.utc_now = local_epoch(2026, 10, 9, 7, 6);
    assert(app_ui_show_boot_screen(&config, &runtime, true) == ESP_OK);
    assert_waiting_sync(analog, false);
    runtime.night_active = true;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert_waiting_sync(analog, true);
    runtime.night_active = false;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    for (unsigned second = 0; second < 135; ++second) {
      lv_tick_inc(1000);
      runtime.clock.utc_now = local_epoch(2026, 10, 9, 7, 6) + second % 60;
      assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
      assert(!find_visible_label(lv_screen_active(), "Clock paused"));
      assert_waiting_sync(analog, false);
    }
    lv_tick_inc(1);
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    lv_obj_t *paused = find_visible_label(lv_screen_active(), "Clock paused");
    assert(paused && !find_visible_label(lv_screen_active(), "Waiting for sync"));
    assert(find_visible_label(lv_obj_get_parent(paused), LV_SYMBOL_WARNING));
    assert(!visible(find_image_source(lv_obj_get_parent(paused), &wac_icon_clock)));
    swipe(pointer, 600, 200, 300, 200);
    assert(!find_visible_label(lv_screen_active(), "Clock paused"));
    assert(!find_visible_label(lv_screen_active(), "Waiting for sync"));
    lv_tick_inc(136000);
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    swipe(pointer, 300, 200, 600, 200);
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert_waiting_sync(analog, false); /* Away from home resets paused-minute health. */
    swipe(pointer, 600, 200, 600, 400);
    assert(wac_ui_settings_view() == WAC_SETTINGS_ENTRY);
    assert(!find_visible_label(lv_screen_active(), "Waiting for sync"));
    wac_ui_settings_close();
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert_waiting_sync(analog, false);
    runtime.clock.source = WAC_TIME_SOURCE_NETWORK;
    runtime.clock.warning = WAC_TIME_WARNING_CONNECTION;
    runtime.clock.utc_now += 60;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert(!find_visible_label(lv_screen_active(), "Waiting for sync"));
    assert(visible(indicator())); /* RTC correction warning does not delay clearing. */
    runtime.clock.usable = false;
    runtime.clock.source = WAC_TIME_SOURCE_UNAVAILABLE;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert(find_visible_label(lv_screen_active(), "Waiting for time sync"));
    assert(!find_visible_label(lv_screen_active(), "Waiting for sync"));
    assert(!find_visible_label(lv_screen_active(), "Clock paused"));
  }
  assert(fake_nvs_calls(FAKE_SET) == 0);
}

/* Inspect actual LVGL transformed outer bounds, including all four corners.
 * These checks establish layout fit, not raster sharpness or physical clipping. */
static lv_area_t digital_night_bounds(lv_obj_t *obj)
{
  lv_area_t area;
  lv_obj_get_coords(obj, &area);
  lv_obj_get_transformed_area(obj, &area, LV_OBJ_POINT_TRANSFORM_FLAG_RECURSIVE);
  const int xs[] = {area.x1, area.x2};
  const int ys[] = {area.y1, area.y2};
  for (unsigned x = 0; x < 2; ++x) {
    for (unsigned y = 0; y < 2; ++y) {
      int dx = xs[x] - 400, dy = ys[y] - 400;
      assert(dx * dx + dy * dy <= 390 * 390);
    }
  }
  return area;
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
  app_runtime_state_t runtime = {0};
  runtime.clock.usable = true;
  runtime.clock.source = WAC_TIME_SOURCE_NETWORK;

  /* Check every real time in 12-hour unpadded, 12-hour padded and 24-hour
   * formats. The padded format retains its existing fitted size; the other
   * two use nominal 250px (144px font at LVGL's quantized 444/256 scale). */
  for (unsigned format = 0; format < 3; ++format) {
    bool show_period = format != 2;
    config.leading_zero_12h = format == 1;
    snprintf(config.time_format, sizeof(config.time_format), show_period ? "12" : "24");
    runtime.night_active = false;
    runtime.clock.utc_now = local_epoch(2026, 10, 9, 7, 6);
    assert(app_ui_show_boot_screen(&config, &runtime, true) == ESP_OK);
    lv_obj_t *time = find_visible_label(lv_screen_active(), format ? "07:06" : "7:06");
    assert(time);
    lv_obj_update_layout(lv_screen_active());
    lv_area_t day_coords;
    lv_obj_get_coords(time, &day_coords);
    assert(lv_obj_get_style_transform_scale_x_safe(time, 0) == 256);
    assert(lv_obj_get_style_transform_scale_y_safe(time, 0) == 256);

    runtime.night_active = true;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    lv_obj_update_layout(lv_screen_active());
    assert(lv_obj_get_style_text_font(time, 0) == &lv_font_clock_time_144);
    assert(lv_obj_get_style_text_letter_space(time, 0) == -2);
    menu_color(time, 0xecc0c0, false);
    if (show_period) {
      lv_obj_t *period = find_visible_label(lv_screen_active(), "AM");
      assert(period);
      menu_color(period, 0xc17a7a, false);
      assert(lv_obj_get_style_text_font(period, 0) == &lv_font_montserrat_24);
      assert(lv_obj_get_style_text_letter_space(period, 0) == 2);
      assert(lv_obj_get_style_transform_scale_x_safe(period, 0) == 256);
      assert(lv_obj_get_style_transform_scale_y_safe(period, 0) == 256);
    }
    assert(find_visible_label(lv_screen_active(), "FRIDAY"));
    assert(find_visible_label(lv_screen_active(), "OCT 9"));

    const int expected_scale = format == 1 ? 408 : 444;
    for (int hour = 0; hour < 24; ++hour) {
      for (int minute = 0; minute < 60; ++minute) {
        runtime.clock.utc_now = local_epoch(2026, 10, 9, hour, minute);
        assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
        lv_obj_update_layout(lv_screen_active());
        int display_hour = show_period ? (hour % 12 ? hour % 12 : 12) : hour;
        char expected_text[6];
        snprintf(expected_text, sizeof(expected_text), format ? "%02d:%02d" : "%d:%02d",
                 display_hour, minute);
        assert(!strcmp(lv_label_get_text(time), expected_text));
        assert(visible(time));
        assert(lv_obj_get_style_transform_scale_x_safe(time, 0) == expected_scale);
        assert(lv_obj_get_style_transform_scale_y_safe(time, 0) == expected_scale);
        lv_area_t bounds = digital_night_bounds(time);
        assert(abs((bounds.x1 + bounds.x2) / 2 - 400) <= 2);
        assert(abs((bounds.y1 + bounds.y2) / 2 - 400) <= 2);
        lv_obj_t *period = find_visible_label(lv_screen_active(), hour < 12 ? "AM" : "PM");
        assert(!find_visible_label(lv_screen_active(), hour < 12 ? "PM" : "AM"));
        if (show_period) {
          assert(period);
          lv_area_t period_bounds = digital_night_bounds(period);
          assert(abs(period_bounds.x1 - bounds.x2 - 8) <= 2);
          lv_area_t coords;
          lv_obj_get_coords(time, &coords);
          int pivot_y = lv_obj_get_style_transform_pivot_y(time, 0);
          int digit_baseline = coords.y1 + pivot_y
            + ((lv_font_clock_time_144.line_height - lv_font_clock_time_144.base_line - pivot_y)
               * expected_scale) / 256;
          int period_baseline = period_bounds.y1 + lv_font_montserrat_24.line_height
            - lv_font_montserrat_24.base_line;
          assert(abs(digit_baseline - period_baseline) <= 3);
        } else {
          assert(!period);
        }
      }
    }

    runtime.night_active = false;
    runtime.clock.utc_now = local_epoch(2026, 10, 9, 7, 6);
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    lv_obj_update_layout(lv_screen_active());
    lv_area_t restored;
    lv_obj_get_coords(time, &restored);
    assert(lv_obj_get_style_transform_scale_x_safe(time, 0) == 256);
    assert(lv_obj_get_style_transform_scale_y_safe(time, 0) == 256);
    assert(restored.x1 == day_coords.x1 && restored.y1 == day_coords.y1
      && restored.x2 == day_coords.x2 && restored.y2 == day_coords.y2);
  }
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
  lv_obj_t *dismiss = lv_obj_get_parent(label("DISMISS"));
  assert(lv_obj_get_style_radius(dismiss, 0) == 32);
  menu_box(dismiss, 225, 480, 350, 96);
  swipe(pointer, 600, 200, 300, 200);
  assert(visible(overlay()) && find_visible_label(lv_screen_active(), "--:--"));
  swipe(pointer, 600, 200, 600, 400);
  assert(wac_ui_settings_view() == WAC_SETTINGS_NONE);

  dismissal_result = ESP_ERR_TIMEOUT;
  swipe(pointer, 400, 528, 400, 528); /* Actual tap on the unchanged action area. */
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
  lv_obj_t *room = find_widget(lv_screen_active(), &lv_textarea_class);
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
  click("Device Setup");
  click("Device Settings");
  lv_obj_t *room = find_widget(lv_screen_active(), &lv_textarea_class);
  assert(room);
  lv_textarea_set_text(room, "Message-waiting draft");
  lv_obj_send_event(room, LV_EVENT_VALUE_CHANGED, NULL);
  app_ui_complete_message_acknowledgment(ESP_OK);
  assert(wac_ui_settings_view() == WAC_SETTINGS_DEVICE);
  assert(!strcmp(lv_textarea_get_text(room), "Message-waiting draft"));
  wac_ui_settings_close();
  assert(fake_nvs_calls(FAKE_SET) == 0);
}

static void test_settings_runtime_appearance(lv_indev_t *pointer)
{
  fake_nvs_reset();
  assert(device_config_init() == ESP_OK);
  device_config_t config;
  assert(device_config_copy(&config) == ESP_OK);
  config.location_ready = true;
  fake_nvs_clear_activity();
  app_runtime_state_t runtime = {0};
  runtime.clock.usable = true;
  runtime.clock.source = WAC_TIME_SOURCE_NETWORK;
  runtime.clock.utc_now = 1577862360;
  assert(app_ui_show_boot_screen(&config, &runtime, true) == ESP_OK);
  swipe(pointer, 600, 200, 600, 400);
  assert(wac_ui_settings_view() == WAC_SETTINGS_ENTRY);
  menu_heading("Settings", false);
  menu_row("Alarm Settings", "7:00 AM · Mon–Fri", 320, true, false);
  lv_obj_t *heading = menu_label(lv_screen_active(), "Settings");
  lv_obj_t *root = lv_obj_get_parent(heading);
  assert(find_image_source(root, &round_stage_day));
  runtime.night_active = true;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert(heading == menu_label(lv_screen_active(), "Settings"));
  menu_heading("Settings", true);
  menu_row("Alarm Settings", "7:00 AM · Mon–Fri", 320, true, true);
  menu_switch("Alarm", false, true);
  assert(find_image_source(root, &round_stage_night)
         && !find_image_source(root, &round_stage_day));
  click("Device Setup");
  click("Display Settings");
  menu_heading("Display", true);
  menu_row("Leading Zero", "Off", 428, false, true);
  click("Home Screen");
  click("Leading Zero");
  menu_switch("Leading Zero", true, true);
  click("Done");
  assert(menu_button("Display Settings", "Analog · 12-Hour"));
  click("Device Settings");
  lv_obj_t *room = find_widget(lv_screen_active(), &lv_textarea_class);
  lv_obj_t *keyboard = find_widget(lv_screen_active(), &lv_keyboard_class);
  assert(room && keyboard);
  lv_textarea_set_text(room, "Runtime draft");
  lv_obj_send_event(room, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_update_layout(lv_screen_active());
  lv_area_t area;
  lv_obj_get_coords(room, &area);
  int x = (area.x1 + area.x2) / 2, y = (area.y1 + area.y2) / 2;
  swipe(pointer, x, y, x, y);
  assert(visible(keyboard) && lv_keyboard_get_textarea(keyboard) == room
         && lv_obj_has_state(room, LV_STATE_FOCUSED));
  lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_UPPER);
  lv_textarea_set_cursor_pos(room, 3);
  heading = menu_label(lv_screen_active(), "Device");
  root = lv_obj_get_parent(heading);
  for (unsigned night = 0; night < 2; ++night) {
    runtime.night_active = night;
    ++runtime.clock.utc_now;
    assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
    assert(wac_ui_settings_view() == WAC_SETTINGS_DEVICE);
    assert(room == find_widget(root, &lv_textarea_class));
    assert(keyboard == find_widget(root, &lv_keyboard_class));
    assert(heading == menu_label(root, "Device"));
    assert(visible(keyboard) && lv_keyboard_get_textarea(keyboard) == room);
    assert(lv_obj_has_state(room, LV_STATE_FOCUSED));
    assert(lv_keyboard_get_mode(keyboard) == LV_KEYBOARD_MODE_TEXT_UPPER);
    assert(lv_textarea_get_cursor_pos(room) == 3);
    assert(!strcmp(lv_textarea_get_text(room), "Runtime draft"));
    menu_heading("Device", night);
    menu_field_geometry("Room Name", 224, night);
    menu_keyboard(keyboard, night);
    assert(lv_color_eq(lv_obj_get_style_bg_color(room, LV_PART_CURSOR),
                       lv_color_hex(night ? 0xecc0c0 : 0xf5f8ff)));
    assert(find_image_source(root, night ? &round_stage_night : &round_stage_day));
  }
  runtime.night_active = false; /* Return to Day while the keyboard remains active. */
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert(visible(keyboard) && lv_keyboard_get_textarea(keyboard) == room);
  menu_heading("Device", false);
  menu_field_geometry("Room Name", 224, false);
  menu_keyboard(keyboard, false);
  lv_obj_send_event(keyboard, LV_EVENT_READY, NULL);
  menu_field_geometry("Room Name", 204, false);
  click("Done");
  assert(menu_button("Device Settings", "Runtime draft"));
  assert(menu_button("Display Settings", "Analog · 12-Hour"));
  click("Display Settings");
  menu_switch("Leading Zero", true, false);
  menu_row("Home Screen", "Analog", 212, false, false);
  menu_box(menu_button("Done", NULL), 270, 664, 260, 80);
  click("Done");
  click("Cancel");
  click("Device Setup");
  assert(menu_button("Device Settings", "Clock"));
  assert(menu_button("Display Settings", "Digital · 12-Hour"));
  click("Cancel");
  lv_obj_t *footer = menu_button("Back to Clock", NULL);
  menu_box(footer, 230, 646, 340, 88);
  menu_button_style(footer, false, false);
  swipe(pointer, 400, 690, 400, 690);
  assert(wac_ui_settings_view() == WAC_SETTINGS_NONE);
  assert(fake_nvs_calls(FAKE_SET) == 0);
  /* A fresh settings opening consumes the current owner snapshot immediately. */
  runtime.night_active = true;
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  swipe(pointer, 600, 200, 600, 400);
  menu_heading("Settings", true);
  assert(find_image_source(lv_obj_get_parent(menu_label(lv_screen_active(), "Settings")),
                           &round_stage_night));
  wac_ui_settings_close();
}

static void test_first_use_boot(lv_indev_t *pointer)
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
  assert(visible(label("Device Setup")) && menu_button("Wi-Fi", "Not Connected"));
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
  click("Cancel");
  assert(wac_ui_settings_view() == WAC_SETTINGS_ENTRY);
  swipe(pointer, 400, 690, 400, 690); /* Back to Clock cannot bypass setup. */
  assert(wac_ui_settings_view() == WAC_SETTINGS_WEATHER);
  click("Cancel");
  assert(wac_ui_settings_view() == WAC_SETTINGS_ENTRY);
  click("Back to Clock");
  assert(wac_ui_settings_view() == WAC_SETTINGS_WEATHER);
  assert(fake_nvs_calls(FAKE_SET) == 0);
  wac_settings_close(); /* Teardown before a separate simulated boot. */

  config.wifi_ready = true; /* Saved Wi-Fi is ready; location still uses defaults. */
  assert(app_ui_show_boot_screen(&config, &runtime, true) == ESP_OK);
  assert(wac_ui_settings_view() == WAC_SETTINGS_WEATHER);
  assert(visible(label("Weather Location")) && visible(label("City, State")));
  lv_obj_t *location = find_widget(lv_screen_active(), &lv_textarea_class);
  assert(location && !lv_textarea_get_text(location)[0]);
  assert(!visible(indicator()) && !visible(overlay()));
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert(wac_ui_settings_view() == WAC_SETTINGS_WEATHER);
  assert(!lv_textarea_get_text(location)[0]);
  assert(dismissal_count == submissions_before_boot);
  assert(runtime.clock.warning == WAC_TIME_WARNING_LOST_TIME);
  assert(fake_nvs_calls(FAKE_SET) == 0);
  lv_obj_t *keyboard = find_widget(lv_screen_active(), &lv_keyboard_class);
  assert(keyboard && lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN));
  lv_obj_update_layout(lv_screen_active());
  lv_area_t area;
  lv_obj_get_coords(location, &area);
  int x = (area.x1 + area.x2) / 2, y = (area.y1 + area.y2) / 2;
  swipe(pointer, x, y, x, y);
  assert(visible(keyboard) && lv_keyboard_get_textarea(keyboard) == location);
  menu_field_geometry("City, State", 224, false);
  menu_keyboard(keyboard, false);
  lv_obj_get_coords(keyboard, &area);
  assert(area.x1 == 120 && area.y1 == 390 && area.x2 == 679 && area.y2 == 669);
  assert(app_ui_update_runtime(&config, &runtime) == ESP_OK);
  assert(visible(keyboard) && lv_keyboard_get_textarea(keyboard) == location);
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
  menu_keyboard_assets();
  lv_display_t *display = lv_display_create(800, 800);
  static uint8_t buffer[800 * 32 * 4];
  lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);
  lv_indev_t *pointer = lv_indev_create();
  lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(pointer, read_pointer);
  test_provider_forecast_dates(pointer);
  test_weather_screen_transitions(pointer);
  test_normal_clock_surfaces(pointer);
  test_waiting_sync_transitions(pointer);
  test_digital_night_layout();
  test_backup_clock_ui(pointer);
  test_background_message_ui(pointer, false);
  test_background_message_ui(pointer, true);
  test_settings_runtime_appearance(pointer);
  test_first_use_boot(pointer);
  test_indicator_pulses();
  lv_indev_delete(pointer);
  lv_display_delete(display);
  lv_deinit();
  puts("Production clock UI checks passed: owner time, warning kinds, navigation, draft and active keyboard retention through Day/Night changes, dismissal errors/pending/acknowledgment, settings styles and first-use guard, uninterrupted indicator pulses, provider forecast dates and calendar captions, approved weather loading/retention/failure/success transitions and divider styles, sync-status source/priority/navigation transitions.");
  return 0;
}
#endif
