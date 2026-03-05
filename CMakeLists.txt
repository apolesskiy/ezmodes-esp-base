# ezmodes ESP Base Component
idf_component_register(
  SRCS
    "src/modal_framework.cpp"
  INCLUDE_DIRS
    "include"
  REQUIRES
    log
    freertos
    esp_lvgl_port
)
