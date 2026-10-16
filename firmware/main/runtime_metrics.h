#pragma once
#include "lvgl.h"
namespace diagnostics {
// start: called once with the adapter's LVGL lock held.
void start(lv_disp_t* display);
// tick: application thread, without the LVGL lock. Takes a brief, nonblocking
// lock to copy UI counters; samples CPU/heaps and sends BLE outside that lock.
void tick();
}
