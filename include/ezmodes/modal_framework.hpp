#pragma once

/**
 * @file modal_framework.hpp
 * @brief Modal UI framework for managing application modes.
 *
 * The modal framework manages mode registration, transitions, lifecycle, and
 * LVGL screen ownership. It provides a simple interface for building modal UIs
 * with single-button navigation.
 *
 * See mode_interface.hpp for detailed lifecycle documentation.
 */

#include <vector>
#include <string>

#include "lvgl.h"

#include "ezmodes/mode_interface.hpp"

namespace ezmodes {

/**
 * @brief Callback type for shutdown operations.
 *
 * Called by the framework after mode cleanup. The callback is responsible for:
 * 1. Deinitializing the display (call display_deinit())
 * 2. Any other hardware cleanup
 * 3. Entering the final state (deep sleep, restart, etc.)
 *
 * The callback should not return.
 */
using ShutdownCallback = void (*)();

/// Configuration for the optional persistent header bar.
struct HeaderBarConfig {
  int32_t height_px;
  const lv_font_t* font;
};

/**
 * @brief Modal UI framework managing modes and transitions.
 *
 * The framework maintains a registry of modes, owns LVGL screens, and handles
 * transitions between modes. Only one mode can be active at a time.
 */
class ModalFramework {
 public:
  /**
   * @brief Construct a new Modal Framework.
   */
  ModalFramework();

  /**
   * @brief Destroy the Modal Framework.
   */
  ~ModalFramework();

  // Non-copyable
  ModalFramework(const ModalFramework&) = delete;
  ModalFramework& operator=(const ModalFramework&) = delete;

  /**
   * @brief Register a mode with the framework.
   *
   * The framework takes ownership of the mode pointer.
   *
   * @param mode Pointer to mode instance (ownership transferred).
   * @return true if registration successful.
   * @return false if mode with same ID already registered.
   */
  bool register_mode(ModeInterface* mode);

  /**
   * @brief Set the initial mode to activate on start.
   *
   * @param mode_id The ID of the mode to start with.
   * @return true if mode found and set as initial.
   * @return false if mode not registered.
   */
  bool set_initial_mode(const char* mode_id);

  /**
   * @brief Start the framework, activating the initial mode.
   *
   * Must be called after registering modes and setting initial mode.
   *
   * @return true if started successfully.
   * @return false if no initial mode set or mode not found.
   */
  bool start();

  /**
   * @brief Request transition to another mode.
   *
   * The transition happens at the next tick to ensure clean state.
   *
   * @param mode_id The ID of the mode to transition to.
   * @return true if mode found and transition scheduled.
   * @return false if mode not registered.
   */
  bool request_transition(const char* mode_id);

  /**
   * @brief Send a button event to the current mode.
   *
   * @param event The button event type.
   */
  void handle_button_event(ButtonEvent event);

  /**
   * @brief Process framework updates (call from main loop).
   *
   * Handles pending transitions and calls on_tick for the active mode.
   */
  void tick();

  /**
   * @brief Get the currently active mode.
   *
   * @return ModeInterface* Pointer to active mode, or nullptr if none.
   */
  ModeInterface* get_active_mode() const;

  /**
   * @brief Get list of all selectable modes.
   *
   * @return std::vector<ModeInterface*> Vector of selectable modes.
   */
  std::vector<ModeInterface*> get_selectable_modes() const;

  /**
   * @brief Find a mode by ID.
   *
   * @param mode_id The mode ID to find.
   * @return ModeInterface* Pointer to mode, or nullptr if not found.
   */
  ModeInterface* find_mode(const char* mode_id) const;

  /**
   * @brief Request system shutdown.
   *
   * Schedules a shutdown sequence: calls active mode's on_exit(), then invokes
   * the provided callback. The callback is responsible for display cleanup and
   * the final shutdown action (deep sleep, restart, etc.).
   *
   * @param callback Function to call after mode cleanup. Should deinitialize
   *                 display and enter final state. Should not return.
   */
  void request_shutdown(ShutdownCallback callback);

  /**
   * @brief Get the active LVGL screen.
   *
   * @return lv_obj_t* Pointer to the active screen, or nullptr if none.
   */
  lv_obj_t* get_active_screen() const;

  /**
   * @brief Report user/system activity to reset the inactivity timer.
   *
   * Modes should call this when activity occurs that should prevent
   * automatic power-off. Button events automatically count as activity.
   *
   * Examples of mode-specific activity:
   * - IR signal received/transmitted
   * - Recording or playback in progress
   * - Animation or timed operation running
   */
  void report_activity();

  /**
   * @brief Set the global inactivity timeout.
   *
   * @param timeout_ms Timeout in milliseconds. Use 0 to disable.
   */
  void set_inactivity_timeout(uint32_t timeout_ms);

  /**
   * @brief Get the current global inactivity timeout.
   *
   * @return uint32_t Timeout in milliseconds, 0 if disabled.
   */
  uint32_t get_inactivity_timeout() const;

  /**
   * @brief Set the mode to transition to on inactivity timeout.
   *
   * @param mode_id Mode ID to transition to (typically "standby").
   */
  void set_power_off_mode(const char* mode_id);

  /// Get the initial (primary) mode ID set via set_initial_mode().
  const char* get_initial_mode_id() const;

  /// Enable the persistent header bar. Must be called before start().
  void set_header_config(const HeaderBarConfig& config);

  /// Content area height available to modes. Full display height when
  /// header is disabled, (display_height - header_height) when enabled.
  int32_t get_content_height() const;

  /// Right-side widget container in the header bar. Returns nullptr
  /// if the header is disabled.
  lv_obj_t* get_header_widget_area() const;

  /// Default inactivity timeout (2 minutes).
  static constexpr uint32_t kDefaultInactivityTimeoutMs = 120000;

 private:
  std::vector<ModeInterface*> modes_;
  ModeInterface* active_mode_;
  ModeInterface* pending_mode_;
  const char* initial_mode_id_;
  uint32_t last_render_ms_;  ///< Time of last render call for delta calculation
  lv_obj_t* active_screen_;  ///< Framework-owned LVGL screen for active mode
  ShutdownCallback shutdown_callback_;  ///< Pending shutdown callback

  // Inactivity timeout
  uint32_t inactivity_timeout_ms_;  ///< Global inactivity timeout setting
  uint32_t last_activity_ms_;       ///< Timestamp of last activity
  const char* power_off_mode_id_;   ///< Mode to transition to on timeout

  // Header bar
  bool header_enabled_ = false;
  HeaderBarConfig header_config_ = {};
  lv_obj_t* header_bar_ = nullptr;
  lv_obj_t* header_mode_label_ = nullptr;
  lv_obj_t* header_widget_area_ = nullptr;

  void create_header_bar();
  void update_header_mode_name();
  lv_obj_t* create_content_screen();
};

}  // namespace ezmodes
