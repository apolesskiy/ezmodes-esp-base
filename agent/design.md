# ezmodes-esp-base — Architecture & Design

## Overview

ezmodes-esp-base provides a modal UI framework for single-button ESP32 applications.
It manages mode lifecycle, screen ownership, transitions, and cross-cutting services
(inactivity timeout, shutdown, header bar).

## Architecture

### Three-Layer Model

The framework follows a three-layer architecture:

1. **UI Layer (LVGL)**: Screen and widget ownership. The framework creates and
   destroys LVGL screens; modes populate them with widgets during `on_enter()`.
2. **Application Layer (Modes)**: Business logic lives in `ModeInterface`
   implementations. Each mode handles its own button events and state.
3. **Framework Layer (ModalFramework)**: Orchestrates mode lifecycle, routes input,
   manages transitions, and provides shared services.

### Mode Lifecycle

Modes follow a strict lifecycle managed by the framework:

- **Registration**: `register_mode()` transfers ownership to the framework.
- **Activation**: `on_enter(framework, screen)` — mode populates the given screen.
- **Active**: `on_render(delta_ms)` called each tick; `on_button_event()` routes input.
- **Deactivation**: `on_exit()` — mode cleans up non-UI state. Framework deletes the screen.

### Transition Sequence (Mode A → Mode B)

1. Framework creates a new LVGL screen for Mode B.
2. `A.on_exit()` — Mode A cleans up.
3. `B.on_enter(framework, new_screen)` — Mode B populates screen.
4. Framework atomically switches screens via `lv_screen_load()`.
5. Framework deletes Mode A's old screen.

This ensures no overlap between modes, always-valid LVGL state, and clean ownership.

### Shutdown Sequence

1. Application calls `request_shutdown(callback)`.
2. On next `tick()`, framework calls `on_exit()` on active mode.
3. Framework invokes the callback (responsible for display cleanup, deep sleep, etc.).

## Key Design Decisions

### Framework Owns Screens
Modes never call `lv_screen_load()` or delete screens. The framework manages all
LVGL screen lifecycle, preventing resource leaks and ensuring atomic transitions.

### Single Active Mode
Only one mode is active at any time. Transitions are deferred to `tick()` to ensure
clean state during event handling.

### Ownership Transfer
`register_mode()` takes ownership of mode pointers. The framework destructor deletes
all registered modes.

### Inactivity Timeout
- Global default: 2 minutes (configurable via `set_inactivity_timeout()`).
- Per-mode override: modes return custom timeout from `get_inactivity_timeout_ms()`.
- Disabled by returning 0; uses default by returning `kUseDefaultTimeout`.
- Button events automatically reset the timer; modes can call `report_activity()` for
  other activity types (e.g., IR reception).

### Header Bar
Optional persistent bar at the top of the display. Enabled via `set_header_config()`
before `start()`. Uses LVGL's top layer so it persists across screen switches.
Content area height is reduced by header height.

### Thread Safety
All LVGL operations are wrapped in `lvgl_port_lock()`/`lvgl_port_unlock()` pairs
for thread-safe access from the main loop or FreeRTOS tasks.

## API Surface

| Component | Public Methods/Members |
|-----------|----------------------|
| `ModalFramework` | 19 public methods + constructor/destructor |
| `ModeInterface` | 8 virtual methods (3 pure, 5 with defaults) |
| `ButtonEvent` | 3 values: kShortPress, kLongPress, kKeyUp |
| `HeaderBarConfig` | height_px, font |
| `ShutdownCallback` | Function pointer type |

## Dependencies

- ESP-IDF ≥ 5.5.0 (`log`, `freertos`)
- LVGL via `esp_lvgl_port`
