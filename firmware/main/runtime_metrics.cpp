#include "runtime_metrics.h"
#include "sdkconfig.h"
#if CONFIG_QUOTAMON_RUNTIME_METRICS
#include "ble_transport.h"
#include "ui.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>
#include <cstdio>

namespace diagnostics {
namespace {
// Fixed storage: collecting counters never allocates a per-task array.
TaskStatus_t tasks[48];
configRUN_TIME_COUNTER_TYPE previousIdle[2]{}, previousTotal=0;
bool haveCpuBaseline=false;
int64_t previousUs=0;
uint32_t frames=0, renderMs=0, maxRenderMs=0;
uint64_t pixels=0;
void (*previousMonitor)(lv_disp_drv_t*,uint32_t,uint32_t)=nullptr;
void onRefreshCompleted(lv_disp_drv_t* driver,uint32_t millis,uint32_t count) {
    ++frames;renderMs+=millis;maxRenderMs=std::max(maxRenderMs,millis);pixels+=count;
    if(previousMonitor)previousMonitor(driver,millis,count);
}
bool cpu(double (&percent)[2]) {
    configRUN_TIME_COUNTER_TYPE total=0,idle[2]{};
    bool found[2]{};
    const auto count=uxTaskGetSystemState(tasks,48,&total);
    for(UBaseType_t i=0;i<count;i++) for(int core=0;core<2;core++)
        if(tasks[i].xHandle==xTaskGetIdleTaskHandleForCore(core)) {idle[core]=tasks[i].ulRunTimeCounter;found[core]=true;}
    const auto delta=static_cast<configRUN_TIME_COUNTER_TYPE>(total-previousTotal);
    const bool valid=count>0&&found[0]&&found[1]&&haveCpuBaseline&&delta>0;
    if(valid) for(int core=0;core<2;core++) {
        const auto rest=static_cast<configRUN_TIME_COUNTER_TYPE>(idle[core]-previousIdle[core]);
        percent[core]=std::clamp(100.0*(1.0-static_cast<double>(rest)/delta),0.0,100.0);
    }
    previousTotal=total;previousIdle[0]=idle[0];previousIdle[1]=idle[1];
    haveCpuBaseline=count>0&&found[0]&&found[1];
    return valid;
}
}
void start(lv_disp_t* display) {
    previousMonitor=display->driver->monitor_cb;
    display->driver->monitor_cb=onRefreshCompleted;
    double ignored[2]{};cpu(ignored);previousUs=esp_timer_get_time();
}
void tick() {
    const auto now=esp_timer_get_time(), elapsed=now-previousUs;
    if(elapsed<5000000) return;
    double usage[2]{};const bool valid=cpu(usage);
    multi_heap_info_t external{},internal{};
    heap_caps_get_info(&external,MALLOC_CAP_SPIRAM);
    heap_caps_get_info(&internal,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    int visible=monitor::selectedAccount()>=0?1:0;
    if(monitor::selectedAccount()==-1)for(int i=0;i<5;i++)if(monitor::cardObject(i))++visible;
    char json[1024];
    const int length=std::snprintf(json,sizeof(json),
        "{\"type\":\"runtime_stats\",\"schema\":1,\"uptime_ms\":%llu,\"window_ms\":%llu,"
        "\"page\":%d,\"visible_characters\":%d,\"ble_connected\":%s,\"cpu_valid\":%s,"
        "\"cpu_percent\":[%.2f,%.2f],\"cpu_average_percent\":%.2f,"
        "\"refresh_frames\":%lu,\"lvgl_refresh_fps\":%.3f,\"render_mean_ms\":%.3f,\"render_max_ms\":%lu,"
        "\"pixels_per_refresh\":%llu,\"psram_free_bytes\":%u,\"psram_minimum_free_bytes\":%u,"
        "\"psram_largest_free_block_bytes\":%u,\"internal_free_bytes\":%u,\"internal_minimum_free_bytes\":%u}",
        static_cast<unsigned long long>(now/1000),static_cast<unsigned long long>(elapsed/1000),
        monitor::selectedAccount(),visible,transport::isConnected()?"true":"false",valid?"true":"false",
        usage[0],usage[1],(usage[0]+usage[1])/2,static_cast<unsigned long>(frames),
        frames*1000000.0/elapsed,frames?static_cast<double>(renderMs)/frames:0.0,
        static_cast<unsigned long>(maxRenderMs),static_cast<unsigned long long>(frames?pixels/frames:0),
        static_cast<unsigned>(external.total_free_bytes),static_cast<unsigned>(external.minimum_free_bytes),
        static_cast<unsigned>(external.largest_free_block),static_cast<unsigned>(internal.total_free_bytes),
        static_cast<unsigned>(internal.minimum_free_bytes));
    if(length>0&&static_cast<size_t>(length)<sizeof(json)) {
        if(transport::isConnected())transport::sendControl(json);
    }
    frames=renderMs=maxRenderMs=0;pixels=0;previousUs=now;
}
}
#else
namespace diagnostics {
void start(lv_disp_t*) {}
void tick() {}
}
#endif
