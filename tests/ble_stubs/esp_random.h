#pragma once
#include "ble_test_platform.h"
inline void esp_fill_random(void* out,size_t length) {
    static uint8_t seed=1;auto* bytes=static_cast<uint8_t*>(out);
    for(size_t i=0;i<length;++i)bytes[i]=seed++;
}
