/**
 * @file modal_framework.cpp
 * @brief Modal UI framework implementation.
 */

#include "ezmodes/modal_framework.hpp"

#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char* TAG = "modal_framework";

namespace ezmodes {

ModalFramework::ModalFramework()
    : active_mode_(nullptr),
      pending_mode_(nullptr),
      initial_mode_id_(nullptr),
      last_render_ms_(0),
      active_screen_(nullptr),
      shutdown_callback_(nullptr),
      inactivity_timeout_ms_(kDefaultInactivityTimeoutMs),
      last_activity_ms_(0),
      power_off_mode_id_(nullptr) {}

ModalFramework::~ModalFramework() {
  // Clean up owned modes
  for (auto* mode : modes_) {
    delete mode;
  }
  modes_.clear();
}

bool ModalFramework::register_mode(ModeInterface* mode) {
  if (mode == nullptr) {
    ESP_LOGE(TAG, "Cannot register null mode");
    return false;
  }

  // Check for duplicate ID
  const char* id = mode->get_id();
  for (const auto* existing : modes_) {
    if (std::strcmp(existing->get_id(), id) == 0) {
      ESP_LOGE(TAG, "Mode '%s' already registered", id);
      return false;
    }
  }

  modes_.push_back(mode);
  ESP_LOGI(TAG, "Registered mode: %s (%s)", id, mode->get_display_name());
  return true;
}

bool ModalFramework::set_initial_mode(const char* mode_id) {
  if (find_mode(mode_id) == nullptr) {
    ESP_LOGE(TAG, "Cannot set initial mode: '%s' not registered", mode_id);
    return false;
  }
  initial_mode_id_ = mode_id;
  ESP_LOGI(TAG, "Initial mode set to: %s", mode_id);
  return true;
}

bool ModalFramework::start() {
  if (initial_mode_id_ == nullptr) {
    ESP_LOGE(TAG, "Cannot start: no initial mode set");
    return false;
  }

  ModeInterface* initial = find_mode(initial_mode_id_);
  if (initial == nullptr) {
    ESP_LOGE(TAG, "Cannot start: initial mode '%s' not found",
             initial_mode_id_);
    return false;
  }

  ESP_LOGI(TAG, "Starting with mode: %s", initial_mode_id_);

  // Initialize activity tracking
  last_activity_ms_ = xTaskGetTickCount() * portTICK_PERIOD_MS;

  // Create header bar if configured.
  if (header_enabled_) {
    if (lvgl_port_lock(100)) {
      create_header_bar();
      lvgl_port_unlock();
    }
  }

  // Create initial screen (framework-owned)
  lv_obj_t* content_screen = nullptr;
  if (lvgl_port_lock(100)) {
    content_screen = create_content_screen();
    lvgl_port_unlock();
  }

  active_screen_ = content_screen;
  active_mode_ = initial;
  active_mode_->on_enter(this, active_screen_);

  // Load the screen after mode populates it
  if (lvgl_port_lock(100)) {
    lv_screen_load(active_screen_);
    if (header_enabled_) {
      update_header_mode_name();
    }
    lvgl_port_unlock();
  }

  return true;
}

bool ModalFramework::request_transition(const char* mode_id) {
  ModeInterface* target = find_mode(mode_id);
  if (target == nullptr) {
    ESP_LOGE(TAG, "Cannot transition: mode '%s' not found", mode_id);
    return false;
  }

  ESP_LOGI(TAG, "Transition requested: %s -> %s",
           active_mode_ ? active_mode_->get_id() : "(none)",
           mode_id);
  pending_mode_ = target;
  return true;
}

void ModalFramework::handle_button_event(ButtonEvent event) {
  // Button events count as activity
  report_activity();

  if (active_mode_ != nullptr) {
    active_mode_->on_button_event(event);
  }
}

void ModalFramework::tick() {
  // Handle pending shutdown first
  if (shutdown_callback_ != nullptr) {
    ShutdownCallback callback = shutdown_callback_;
    shutdown_callback_ = nullptr;

    ESP_LOGI(TAG, "Executing shutdown sequence");

    // Exit current mode
    if (active_mode_ != nullptr) {
      ESP_LOGD(TAG, "Exiting mode for shutdown: %s", active_mode_->get_id());
      active_mode_->on_exit();
      active_mode_ = nullptr;
    }

    // Clear screen reference (callback is responsible for display cleanup)
    active_screen_ = nullptr;

    // Invoke shutdown callback (should handle display cleanup and not return)
    callback();
    return;  // In case callback returns unexpectedly
  }

  // Handle pending transition
  if (pending_mode_ != nullptr && pending_mode_ != active_mode_) {
    ModeInterface* old_mode = active_mode_;
    lv_obj_t* old_screen = active_screen_;

    // Create new screen before any mode operations
    lv_obj_t* new_screen = nullptr;
    if (lvgl_port_lock(100)) {
      new_screen = create_content_screen();
      lvgl_port_unlock();
    }

    // Exit old mode (screen still valid)
    if (old_mode != nullptr) {
      ESP_LOGD(TAG, "Exiting mode: %s", old_mode->get_id());
      old_mode->on_exit();
    }

    // Enter new mode with new screen
    active_mode_ = pending_mode_;
    active_screen_ = new_screen;
    pending_mode_ = nullptr;

    // Reset inactivity timer when entering a new mode
    report_activity();

    ESP_LOGD(TAG, "Entering mode: %s", active_mode_->get_id());
    active_mode_->on_enter(this, new_screen);

    // Update header bar mode name on transition.
    if (header_enabled_ && header_mode_label_) {
      if (lvgl_port_lock(100)) {
        update_header_mode_name();
        lvgl_port_unlock();
      }
    }

    // Atomically switch screens
    if (lvgl_port_lock(100)) {
      lv_screen_load(new_screen);

      // Delete old screen (after new is loaded)
      if (old_screen != nullptr) {
        lv_obj_del(old_screen);
        ESP_LOGD(TAG, "Old screen deleted");
      }
      lvgl_port_unlock();
    }
  }

  // Call render on active mode with time delta
  if (active_mode_ != nullptr) {
    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t delta_ms = (last_render_ms_ == 0) ? 0 : (now_ms - last_render_ms_);
    last_render_ms_ = now_ms;
    active_mode_->on_render(delta_ms);

    // Check inactivity timeout
    if (power_off_mode_id_ != nullptr && active_mode_ != nullptr) {
      // Don't trigger timeout if we're already in the power-off mode
      if (std::strcmp(active_mode_->get_id(), power_off_mode_id_) != 0) {
        // Get effective timeout for this mode
        uint32_t mode_timeout = active_mode_->get_inactivity_timeout_ms();
        uint32_t effective_timeout;
        if (mode_timeout == ModeInterface::kUseDefaultTimeout) {
          effective_timeout = inactivity_timeout_ms_;
        } else {
          effective_timeout = mode_timeout;
        }

        // Check if timeout has elapsed (0 means disabled)
        if (effective_timeout > 0) {
          uint32_t idle_time = now_ms - last_activity_ms_;
          if (idle_time >= effective_timeout) {
            ESP_LOGI(TAG, "Inactivity timeout (%lu ms), transitioning to %s",
                     static_cast<unsigned long>(idle_time), power_off_mode_id_);
            request_transition(power_off_mode_id_);
          }
        }
      }
    }
  }
}

ModeInterface* ModalFramework::get_active_mode() const {
  return active_mode_;
}

std::vector<ModeInterface*> ModalFramework::get_selectable_modes() const {
  std::vector<ModeInterface*> selectable;
  for (auto* mode : modes_) {
    if (mode->is_selectable()) {
      selectable.push_back(mode);
    }
  }
  return selectable;
}

ModeInterface* ModalFramework::find_mode(const char* mode_id) const {
  for (auto* mode : modes_) {
    if (std::strcmp(mode->get_id(), mode_id) == 0) {
      return mode;
    }
  }
  return nullptr;
}

void ModalFramework::request_shutdown(ShutdownCallback callback) {
  if (callback == nullptr) {
    ESP_LOGE(TAG, "Cannot request shutdown with null callback");
    return;
  }
  ESP_LOGI(TAG, "Shutdown requested");
  shutdown_callback_ = callback;
}

lv_obj_t* ModalFramework::get_active_screen() const {
  return active_screen_;
}

void ModalFramework::report_activity() {
  last_activity_ms_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void ModalFramework::set_inactivity_timeout(uint32_t timeout_ms) {
  inactivity_timeout_ms_ = timeout_ms;
  ESP_LOGI(TAG, "Inactivity timeout set to %lu ms",
           static_cast<unsigned long>(timeout_ms));
}

uint32_t ModalFramework::get_inactivity_timeout() const {
  return inactivity_timeout_ms_;
}

void ModalFramework::set_power_off_mode(const char* mode_id) {
  power_off_mode_id_ = mode_id;
  ESP_LOGI(TAG, "Power-off mode set to: %s", mode_id ? mode_id : "(none)");
}

const char* ModalFramework::get_initial_mode_id() const {
  return initial_mode_id_;
}

void ModalFramework::set_header_config(const HeaderBarConfig& config) {
  header_config_ = config;
  header_enabled_ = true;
  ESP_LOGI(TAG, "Header bar configured (%ld px)",
           static_cast<long>(config.height_px));
}

int32_t ModalFramework::get_content_height() const {
  if (!header_enabled_) {
    lv_display_t* disp = lv_display_get_default();
    return disp ? lv_display_get_vertical_resolution(disp) : 0;
  }
  lv_display_t* disp = lv_display_get_default();
  int32_t full = disp ? lv_display_get_vertical_resolution(disp) : 0;
  return full - header_config_.height_px;
}

lv_obj_t* ModalFramework::get_header_widget_area() const {
  return header_widget_area_;
}

void ModalFramework::create_header_bar() {
  lv_obj_t* top_layer = lv_layer_top();

  header_bar_ = lv_obj_create(top_layer);
  lv_display_t* disp = lv_display_get_default();
  int32_t disp_w = disp ? lv_display_get_horizontal_resolution(disp) : 64;
  lv_obj_set_size(header_bar_, disp_w, header_config_.height_px);
  lv_obj_set_pos(header_bar_, 0, 0);
  lv_obj_set_style_bg_color(header_bar_, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(header_bar_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(header_bar_, 0, 0);
  lv_obj_set_style_pad_all(header_bar_, 0, 0);
  lv_obj_clear_flag(header_bar_, LV_OBJ_FLAG_SCROLLABLE);

  // Left-aligned mode name label.
  header_mode_label_ = lv_label_create(header_bar_);
  lv_obj_set_style_text_color(header_mode_label_, lv_color_white(), 0);
  if (header_config_.font) {
    lv_obj_set_style_text_font(header_mode_label_, header_config_.font, 0);
  }
  lv_obj_align(header_mode_label_, LV_ALIGN_LEFT_MID, 1, 0);
  lv_label_set_text(header_mode_label_, "");

  // Services (e.g. battery indicator) place widgets directly on the
  // header bar to avoid nested-container clipping at small pixel heights.
  // Each service is responsible for its own alignment (e.g. RIGHT_MID).
  header_widget_area_ = header_bar_;
}

void ModalFramework::update_header_mode_name() {
  if (!header_mode_label_ || !active_mode_) {
    return;
  }
  lv_label_set_text(header_mode_label_, active_mode_->get_display_name());
}

lv_obj_t* ModalFramework::create_content_screen() {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

  // Screens cannot be repositioned in LVGL (always render at 0,0).
  // Use pad_top to push content below the header bar instead.
  if (header_enabled_) {
    lv_obj_set_style_pad_top(scr, header_config_.height_px, 0);
  }

  return scr;
}

}  // namespace ezmodes
