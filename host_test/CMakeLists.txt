cmake_minimum_required(VERSION 3.16)
project(ezmodes_base_host_tests C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# Stub ESP-IDF / LVGL / FreeRTOS headers
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_executable(test_modal_framework
  test_modal_framework.cpp
  # Source under test
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/modal_framework.cpp
)

add_test(NAME modal_framework_tests COMMAND test_modal_framework)
