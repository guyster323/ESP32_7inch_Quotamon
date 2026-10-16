#pragma once
#include <cstdint>
using esp_err_t=int;
constexpr esp_err_t ESP_OK=0, ESP_ERR_TIMEOUT=0x107;
esp_err_t esp_lv_adapter_lock(int32_t);
void esp_lv_adapter_unlock();
