#pragma once
#include "FreeRTOS.h"
configRUN_TIME_COUNTER_TYPE ulTaskGetIdleRunTimeCounterForCore(int);
// Keep the old, disruptive API available so a regression fails at runtime.
struct TaskStatus_t { void* xHandle; configRUN_TIME_COUNTER_TYPE ulRunTimeCounter; };
unsigned uxTaskGetSystemState(TaskStatus_t*, unsigned, configRUN_TIME_COUNTER_TYPE*);
void* xTaskGetIdleTaskHandleForCore(int);
