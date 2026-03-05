/**
 * @file freertos/task.h
 * @brief Stub for FreeRTOS task API for host testing.
 *
 * Provides a controllable tick counter for testing time-dependent logic.
 */

#pragma once

#include <cstdint>

namespace freertos_stub {

/// Set the mock tick count (call from tests to simulate time progression).
void set_tick_count(uint32_t ticks);

/// Get the current mock tick count.
uint32_t get_tick_count();

}  // namespace freertos_stub

inline uint32_t xTaskGetTickCount() {
  return freertos_stub::get_tick_count();
}

inline void vTaskDelay(uint32_t) {}
