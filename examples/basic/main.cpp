/**
 * @file main.cpp
 * @brief ezmodes_esp_base example: two-mode application.
 *
 * Demonstrates a minimal two-mode setup with transitions triggered
 * by button events.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ezmodes/modal_framework.hpp"

static const char* TAG = "ezmodes_example";

/// A simple "Hello" mode that transitions on long press.
class HelloMode : public ezmodes::ModeInterface {
 public:
  const char* get_id() const override { return "hello"; }
  const char* get_display_name() const override { return "Hello"; }

  void on_enter(ezmodes::ModalFramework* fw, lv_obj_t* screen) override {
    framework_ = fw;
    lv_obj_t* label = lv_label_create(screen);
    lv_label_set_text(label, "Hello!\nShort: count\nLong: switch");
    lv_obj_center(label);
  }

  void on_exit() override {
    count_ = 0;
  }

  void on_button_event(ezmodes::ButtonEvent event) override {
    if (event == ezmodes::ButtonEvent::kShortPress) {
      count_++;
      ESP_LOGI(TAG, "Hello count: %d", count_);
    } else if (event == ezmodes::ButtonEvent::kLongPress) {
      framework_->request_transition("world");
    }
  }

 private:
  ezmodes::ModalFramework* framework_ = nullptr;
  int count_ = 0;
};

/// A simple "World" mode that transitions back on long press.
class WorldMode : public ezmodes::ModeInterface {
 public:
  const char* get_id() const override { return "world"; }
  const char* get_display_name() const override { return "World"; }

  void on_enter(ezmodes::ModalFramework* fw, lv_obj_t* screen) override {
    framework_ = fw;
    lv_obj_t* label = lv_label_create(screen);
    lv_label_set_text(label, "World!\nLong: back");
    lv_obj_center(label);
  }

  void on_exit() override {}

  void on_button_event(ezmodes::ButtonEvent event) override {
    if (event == ezmodes::ButtonEvent::kLongPress) {
      framework_->request_transition("hello");
    }
  }

 private:
  ezmodes::ModalFramework* framework_ = nullptr;
};

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "ezmodes example starting");

  // NOTE: LVGL display must be initialized before starting the framework.
  // See your board's display driver documentation for initialization.

  ezmodes::ModalFramework framework;

  framework.register_mode(new HelloMode());
  framework.register_mode(new WorldMode());
  framework.set_initial_mode("hello");
  framework.start();

  while (true) {
    framework.tick();
    vTaskDelay(pdMS_TO_TICKS(16));
  }
}
