file(GLOB ASSETS "../../native/generated/*.c")
idf_component_register(SRCS "main.cpp" "ble_transport.cpp" "../vendor/waveshare_rgb_lcd_port.c" "../../native/ui.cpp" ${ASSETS}
 INCLUDE_DIRS "." ".." "../vendor" "../../native" "../../third_party/nlohmann" "../../protocol"
 REQUIRES bt nvs_flash esp_timer esp_lcd driver)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-format -Wno-missing-field-initializers)
