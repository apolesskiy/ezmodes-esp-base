/**
 * @file freertos/FreeRTOS.h
 * @brief Stub for FreeRTOS for host testing.
 */

#pragma once

#include <cstdint>

#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(ms) ((uint32_t)(ms))
