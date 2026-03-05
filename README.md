# ezmodes_esp_base

Modal UI framework for one-button navigationESP32 with LVGL. Manages application modes,
transitions, screen lifecycle, and shared services like inactivity
timeout and shutdown.

## Features

- **Mode lifecycle management**: Clean enter/exit transitions with
  framework-owned LVGL screens.
- **Single-button navigation**: Routes button events (short press,
  long press, key up) to the active mode.
- **Inactivity timeout**: Configurable per-mode or global timeout
  with automatic transition to a power-off mode.
- **Shutdown sequence**: Graceful shutdown with mode cleanup before
  hardware deinitialization.
- **Header bar**: Optional persistent header with mode name and
  widget area for status indicators.
- **LVGL integration**: Thread-safe screen management via
  `esp_lvgl_port`.

## Installation

### ESP Component Registry

```yaml
# idf_component.yml
dependencies:
  apolesskiy/ezmodes-esp-base:
    git: https://github.com/apolesskiy/ezmodes-esp-base.git
```

### Manual

Clone into your project's `components/` directory:

```bash
cd components
git clone https://github.com/apolesskiy/ezmodes-esp-base.git ezmodes-esp-base
```

## Quick Start

### 1. Implement a Mode

```cpp
#include "ezmodes/mode_interface.hpp"

class MyMode : public ezmodes::ModeInterface {
 public:
  const char* get_id() const override { return "my_mode"; }
  const char* get_display_name() const override { return "My Mode"; }

  void on_enter(ezmodes::ModalFramework* fw, lv_obj_t* screen) override {
    // Create LVGL widgets on the screen
    lv_obj_t* label = lv_label_create(screen);
    lv_label_set_text(label, "Hello!");
    lv_obj_center(label);
  }

  void on_exit() override {
    // Clean up non-UI state (timers, saved data)
    // Do NOT delete LVGL objects — the framework owns the screen
  }

  void on_button_event(ezmodes::ButtonEvent event) override {
    if (event == ezmodes::ButtonEvent::kShortPress) {
      // Handle short press
    }
  }
};
```

### 2. Set Up the Framework

```cpp
#include "ezmodes/modal_framework.hpp"

// Create framework
ezmodes::ModalFramework framework;

// Register modes (framework takes ownership)
framework.register_mode(new MyMode());
framework.register_mode(new AnotherMode());

// Set the initial mode
framework.set_initial_mode("my_mode");

// Optional: configure header bar
ezmodes::HeaderBarConfig header = {
    .height_px = 12,
    .font = &lv_font_montserrat_10,
};
framework.set_header_config(header);

// Optional: configure inactivity timeout
framework.set_inactivity_timeout(120000);  // 2 minutes
framework.set_power_off_mode("standby");

// Start
framework.start();
```

### 3. Main Loop

```cpp
while (true) {
  framework.tick();
  vTaskDelay(pdMS_TO_TICKS(16));  // ~60 FPS
}
```

### 4. Route Button Events

```cpp
// From your button ISR or task:
framework.handle_button_event(ezmodes::ButtonEvent::kShortPress);
```

## Mode Lifecycle

### Activation

1. Framework creates a new LVGL screen.
2. Framework calls `on_enter(framework, screen)`.
3. Mode populates the screen with widgets.
4. Framework loads the screen via `lv_screen_load()`.

### Active

- `on_render(delta_ms)` is called each frame for animations/updates.
- `on_button_event(event)` is called for button input.
- Mode may call `framework->request_transition("other_mode")`.

### Deactivation

1. Framework calls `on_exit()`.
2. Mode cleans up non-UI state.
3. Framework deletes the old screen.

### Transition Sequence (A → B)

1. Framework creates new screen for B.
2. `A.on_exit()` — A cleans up.
3. `B.on_enter(framework, screen)` — B populates screen.
4. Framework atomically switches screens.
5. Framework deletes A's old screen.

### Shutdown

1. `framework->request_shutdown(callback)`.
2. Framework calls `on_exit()` on the active mode.
3. Framework invokes the callback for hardware cleanup.

## API Reference

See header files for full documentation:

- [`mode_interface.hpp`](include/ezmodes/mode_interface.hpp) — `ModeInterface`, `ButtonEvent`
- [`modal_framework.hpp`](include/ezmodes/modal_framework.hpp) — `ModalFramework`, `HeaderBarConfig`, `ShutdownCallback`

## Requirements

- ESP-IDF v5.5.0 or later
- LVGL (via `esp_lvgl_port`)
- ESP32, ESP32-S2, ESP32-S3, or ESP32-C3
