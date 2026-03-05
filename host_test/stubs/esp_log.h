/**
 * @file esp_log.h
 * @brief Stub for ESP-IDF logging for host testing.
 *
 * Maps ESP_LOGx macros to printf for host test output.
 */

#pragma once

#include <cstdio>

#define ESP_LOGE(tag, fmt, ...) printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) (void)0
#define ESP_LOGD(tag, fmt, ...) (void)0
#define ESP_LOGV(tag, fmt, ...) (void)0
