#pragma once
#include "esp_timer.h"
#include <cstdint>
using configRUN_TIME_COUNTER_TYPE=uint32_t;
#define portGET_RUN_TIME_COUNTER_VALUE() static_cast<configRUN_TIME_COUNTER_TYPE>(esp_timer_get_time())
