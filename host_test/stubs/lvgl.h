/**
 * @file lvgl.h
 * @brief Stub for LVGL for host testing.
 *
 * Provides minimal type definitions and function stubs to allow
 * modal framework code to compile for host-based unit testing.
 */

#pragma once

#include <cstdint>
#include <cstdio>

// Basic LVGL types
typedef struct _lv_obj_t {
  int id;
  struct _lv_obj_t* parent;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
  char text[128];
  bool deleted;
} lv_obj_t;

typedef struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} lv_color_t;

typedef struct {
  int32_t line_height;
} lv_font_t;

typedef struct {
  int32_t hor_res;
  int32_t ver_res;
} lv_display_t;

// Global font stubs
static lv_font_t lv_font_montserrat_10 = {10};

// Opacity
#define LV_OPA_TRANSP 0
#define LV_OPA_COVER 255

// Alignment
#define LV_ALIGN_LEFT_MID 0
#define LV_ALIGN_CENTER 1

// Object flags
#define LV_OBJ_FLAG_SCROLLABLE (1 << 0)

// Object creation — declarations only, defined in test file
lv_obj_t* lv_obj_create(lv_obj_t* parent);
lv_obj_t* lv_label_create(lv_obj_t* parent);

// Object deletion
inline void lv_obj_del(lv_obj_t* obj) {
  if (obj != nullptr) {
    obj->deleted = true;
  }
}

// Screen management
inline void lv_screen_load(lv_obj_t*) {}

// Layer
lv_obj_t* lv_layer_top();

// Display
inline lv_display_t* lv_display_get_default() {
  static lv_display_t disp = {64, 128};
  return &disp;
}
inline int32_t lv_display_get_horizontal_resolution(lv_display_t* d) {
  return d ? d->hor_res : 64;
}
inline int32_t lv_display_get_vertical_resolution(lv_display_t* d) {
  return d ? d->ver_res : 128;
}

// Color helpers
inline lv_color_t lv_color_white() { return {255, 255, 255}; }
inline lv_color_t lv_color_black() { return {0, 0, 0}; }

// Position and size
inline void lv_obj_set_pos(lv_obj_t* obj, int32_t x, int32_t y) {
  if (obj) { obj->x = x; obj->y = y; }
}
inline void lv_obj_set_size(lv_obj_t* obj, int32_t w, int32_t h) {
  if (obj) { obj->width = w; obj->height = h; }
}

// Label
inline void lv_label_set_text(lv_obj_t* obj, const char* text) {
  if (obj && text) {
    snprintf(obj->text, sizeof(obj->text), "%s", text);
  }
}

// Alignment
inline void lv_obj_align(lv_obj_t*, int32_t, int32_t, int32_t) {}
inline void lv_obj_center(lv_obj_t*) {}

// Styling (no-ops for testing)
inline void lv_obj_set_style_pad_all(lv_obj_t*, int32_t, int32_t) {}
inline void lv_obj_set_style_pad_top(lv_obj_t*, int32_t, int32_t) {}
inline void lv_obj_set_style_border_width(lv_obj_t*, int32_t, int32_t) {}
inline void lv_obj_set_style_bg_opa(lv_obj_t*, uint8_t, int32_t) {}
inline void lv_obj_set_style_bg_color(lv_obj_t*, lv_color_t, int32_t) {}
inline void lv_obj_set_style_text_color(lv_obj_t*, lv_color_t, int32_t) {}
inline void lv_obj_set_style_text_font(lv_obj_t*, const lv_font_t*, int32_t) {}

// Object flags
inline void lv_obj_add_flag(lv_obj_t*, uint32_t) {}
inline void lv_obj_clear_flag(lv_obj_t*, uint32_t) {}
