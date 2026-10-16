#pragma once
#include "lvgl.h"
namespace diagnostics {
// Called with the adapter's LVGL lock held, on the application/UI thread.
void start(lv_disp_t* display);
void tick();
}
