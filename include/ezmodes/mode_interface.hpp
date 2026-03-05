#pragma once

/**
 * @file mode_interface.hpp
 * @brief Abstract interface for modal system modes.
 *
 * Each mode in the modal UI system must implement this interface.
 *
 * ## Mode Lifecycle
 *
 * The modal framework manages mode lifecycle with clear, non-overlapping phases:
 *
 * 1. **Activation**: Framework creates a screen, then calls `on_enter(framework, screen)`.
 *    The mode populates the screen with LVGL widgets. Do NOT call `lv_screen_load()` -
 *    the framework handles screen switching.
 *
 * 2. **Active**: Framework calls `on_render(delta_ms)` each frame and routes button
 *    events via `on_button_event()`. Mode remains active until a transition or shutdown.
 *
 * 3. **Deactivation**: Framework calls `on_exit()`. The mode should clean up non-UI
 *    state (timers, saved data, etc.). Do NOT delete LVGL objects - the framework
 *    owns the screen and will delete it after `on_exit()` returns.
 *
 * ## Transition Sequence (Mode A → Mode B)
 *
 * 1. Framework creates new screen for Mode B
 * 2. Framework calls `A.on_exit()` (Mode A cleans up non-UI state)
 * 3. Framework calls `B.on_enter(framework, new_screen)` (Mode B populates screen)
 * 4. Framework atomically switches to new screen (`lv_screen_load`)
 * 5. Framework deletes old screen (Mode A's widgets are gone)
 *
 * This ensures: (a) modes never coexist during transitions, (b) LVGL always has
 * a valid screen, (c) modes don't manage screen lifecycle.
 *
 * ## Shutdown
 *
 * For system shutdown (deep sleep, restart), call `framework->request_shutdown()`.
 * The framework will call `on_exit()`, deinitialize the display, then invoke
 * the registered shutdown callback.
 */

#include <cstdint>

#include "lvgl.h"

namespace ezmodes {

// Forward declaration
class ModalFramework;

/**
 * @brief Button event types for mode input handling.
 */
enum class ButtonEvent {
  kShortPress,  ///< Short button press (< long press threshold)
  kLongPress,   ///< Long button press (>= long press threshold)
  kKeyUp,       ///< Button released (any press type)
};

/**
 * @brief Abstract base class for all modes in the modal system.
 *
 * Modes handle UI state and respond to button events. The modal framework
 * manages mode lifecycle, screen ownership, and transitions.
 */
class ModeInterface {
 public:
  virtual ~ModeInterface() = default;

  /**
   * @brief Get the unique identifier for this mode.
   *
   * @return const char* Mode identifier string (e.g., "mode_selection").
   */
  virtual const char* get_id() const = 0;

  /**
   * @brief Get the human-readable display name for this mode.
   *
   * @return const char* Mode display name (e.g., "Mode Selection").
   */
  virtual const char* get_display_name() const = 0;

  /**
   * @brief Called when the mode becomes active.
   *
   * Populate the provided screen with LVGL widgets. The screen is owned by
   * the framework - do NOT delete it or call `lv_screen_load()`.
   *
   * @param framework Pointer to the modal framework for requesting transitions.
   * @param screen Framework-owned LVGL screen to populate with widgets.
   */
  virtual void on_enter(ModalFramework* framework, lv_obj_t* screen) = 0;

  /**
   * @brief Called when leaving this mode.
   *
   * Clean up non-UI state (timers, saved data, etc.). Do NOT delete LVGL
   * objects - the framework owns the screen and handles cleanup.
   */
  virtual void on_exit() = 0;

  /**
   * @brief Called periodically while the mode is active for rendering updates.
   *
   * Use this for animations, timeouts, or other time-based updates.
   * Called from the main loop.
   *
   * @param delta_ms Time in milliseconds since the last render call.
   */
  virtual void on_render(uint32_t delta_ms) { (void)delta_ms; }

  /**
   * @brief Handle a button event.
   *
   * @param event The type of button event received.
   */
  virtual void on_button_event(ButtonEvent event) = 0;

  /**
   * @brief Check if this mode should appear in the mode selection menu.
   *
   * @return true if the mode should be listed in mode selection.
   * @return false if the mode is internal/hidden (e.g., Mode Selection itself).
   */
  virtual bool is_selectable() const { return true; }

  /**
   * @brief Get the inactivity timeout for this mode.
   *
   * Modes can override to customize inactivity behavior:
   * - Return 0 to disable inactivity timeout for this mode
   * - Return a positive value for a mode-specific timeout (milliseconds)
   * - Return kUseDefaultTimeout to use the framework's global setting
   *
   * Default implementation returns kUseDefaultTimeout.
   *
   * @return uint32_t Timeout in milliseconds, 0 to disable, or kUseDefaultTimeout.
   */
  virtual uint32_t get_inactivity_timeout_ms() const { return kUseDefaultTimeout; }

  /// Special value indicating the mode should use the framework's default timeout.
  static constexpr uint32_t kUseDefaultTimeout = UINT32_MAX;
};

}  // namespace ezmodes
