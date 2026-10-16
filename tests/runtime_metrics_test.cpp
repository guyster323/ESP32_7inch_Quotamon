#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../firmware/main/runtime_metrics.h"
#include "esp_heap_caps.h"
#include "esp_lv_adapter.h"
#include "freertos/task.h"
#include "ui.h"
#include "json.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

namespace {
int64_t nowUs=1000000;
uint32_t idle[2]={800000,900000};
bool locked=false, busy=false, connected=true;
unsigned idleReads=0, heapReads=0, chainedFrames=0;
int page=-1;
std::string sent;
nlohmann::json report() {return nlohmann::json::parse(sent);}
void near(double actual,double expected) {assert(std::abs(actual-expected)<0.001);}
}
int64_t esp_timer_get_time() {return nowUs;}
configRUN_TIME_COUNTER_TYPE ulTaskGetIdleRunTimeCounterForCore(int core) {
    assert(core==0||core==1);++idleReads;return idle[core];
}
unsigned uxTaskGetSystemState(TaskStatus_t*,unsigned,configRUN_TIME_COUNTER_TYPE*) {
    assert(false&&"Full task/stack scans block RGB refill interrupts");return 0;
}
void* xTaskGetIdleTaskHandleForCore(int) {return nullptr;}
esp_err_t esp_lv_adapter_lock(int32_t timeout) {
    assert(timeout==0&&!locked);
    if(busy)return ESP_ERR_TIMEOUT;
    locked=true;return ESP_OK;
}
void esp_lv_adapter_unlock() {assert(locked);locked=false;}
void heap_caps_get_info(multi_heap_info_t* info,unsigned) {
    assert(!locked&&"Heap traversal must not hold the UI lock");
    ++heapReads;*info={900000,800000,750000};
}
namespace monitor {
int selectedAccount() {assert(locked);return page;}
lv_obj_t* cardObject(int index) {assert(locked);return index<3?reinterpret_cast<lv_obj_t*>(1):nullptr;}
}
namespace transport {
bool isConnected() {assert(!locked);return connected;}
bool sendControl(const std::string& json) {assert(!locked);sent=json;return true;}
}
int main() {
    lv_disp_drv_t driver{};lv_disp_t display{};display.driver=&driver;
    driver.monitor_cb=[](lv_disp_drv_t*,uint32_t,uint32_t){++chainedFrames;};
    locked=true;diagnostics::start(&display);locked=false;
    assert(idleReads==2);
    nowUs+=4999999;diagnostics::tick();assert(sent.empty()&&heapReads==0);
    // A busy renderer must keep its counters and CPU baseline until a sample
    // can be copied, without blocking or doing heap/BLE work under its lock.
    driver.monitor_cb(&driver,10,100);driver.monitor_cb(&driver,30,300);
    ++nowUs;busy=true;diagnostics::tick();assert(sent.empty()&&idleReads==2);
    busy=false;idle[0]+=4000000;idle[1]+=4500000;diagnostics::tick();
    auto first=report();
    assert(first["cpu_valid"]==true&&first["page"]==-1&&first["visible_characters"]==3);
    near(first["cpu_percent"][0],20);near(first["cpu_percent"][1],10);
    near(first["cpu_average_percent"],15);near(first["lvgl_refresh_fps"],0.4);
    assert(first["refresh_frames"]==2&&first["render_mean_ms"]==20&&first["render_max_ms"]==30);
    assert(first["pixels_per_refresh"]==200&&chainedFrames==2&&heapReads==2);
    nowUs+=5000000;idle[0]+=5000000;idle[1]+=5000000;page=-2;diagnostics::tick();
    auto quiet=report();assert(quiet["refresh_frames"]==0&&quiet["render_mean_ms"]==0);
    assert(quiet["render_max_ms"]==0&&quiet["pixels_per_refresh"]==0&&quiet["visible_characters"]==0);
    near(quiet["cpu_average_percent"],0);
    // Cross the 32-bit microsecond runtime-counter rollover (~71.6 minutes).
    nowUs=(int64_t{1}<<32)-2000000;idle[0]=0xfff00000u;idle[1]=0xffe00000u;
    diagnostics::tick();
    nowUs+=5000000;idle[0]+=3750000;idle[1]+=2500000;page=0;
    driver.monitor_cb(&driver,7,600);diagnostics::tick();
    auto wrapped=report();near(wrapped["cpu_percent"][0],25);near(wrapped["cpu_percent"][1],50);
    assert(wrapped["visible_characters"]==1&&wrapped["refresh_frames"]==1);
    sent.clear();connected=false;nowUs+=5000000;diagnostics::tick();assert(sent.empty());
    assert(!locked);
    std::cout<<"Idle-only CPU sampling, counter wrap, refresh windows, busy UI, unlocked heap/BLE: PASS\n";
}
