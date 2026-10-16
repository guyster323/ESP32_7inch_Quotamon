#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lv_adapter.h"
extern "C" {
#include "waveshare_rgb_lcd_port.h"
}
#include "ble_transport.h"
#include "runtime_metrics.h"
#include "../../native/snapshot_json.h"
#include <memory>
#include <sys/time.h>
namespace {
monitor::Snapshot current{false,false,{}};
int revision=0;
int editRevision=0;
monitor::Settings confirmedSettings;
lv_obj_t* dimmer=nullptr;
void brightness(int value) {
    if(!dimmer) {dimmer=lv_obj_create(lv_layer_top());lv_obj_remove_style_all(dimmer);lv_obj_set_size(dimmer,800,480);lv_obj_set_style_bg_color(dimmer,lv_color_black(),0);lv_obj_clear_flag(dimmer,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);}
    lv_obj_set_style_bg_opa(dimmer,static_cast<lv_opa_t>((100-value)*255/100),0);
}
void clear() {bool pairing=monitor::selectedAccount()==-4;current.connected=false;current.accounts.clear();monitor::apply(current);if(pairing)monitor::showPairingWait();else monitor::showHome();}
void settings(const monitor::Settings& s,const monitor::Snapshot& data) {
    if(editRevision!=revision) {monitor::setSettings(confirmedSettings);monitor::apply(current);monitor::notify("설정이 변경되어 저장하지 않았습니다. 다시 확인하세요.");return;}
    brightness(s.brightnessPercent);
    nlohmann::json rows=nlohmann::json::array();for(const auto& a:data.accounts)rows.push_back({{"id",a.id},{"enabled",a.enabled}});
    nlohmann::json request={{"type","settings"},{"base_revision",editRevision},{"accounts",rows},{"display",{
        {"poll_seconds",s.pollSeconds},{"brightness_percent",s.brightnessPercent},{"show_reset",s.showReset},{"show_updated",s.showUpdated},{"show_credits",s.showCredits},{"show_session",s.showSession}}}};
    monitor::notify(transport::sendControl(request.dump())?"Orca에 설정 저장 요청됨":"기기에 임시 적용됨 · Orca 연결 후 다시 저장하세요.");
}
void refresh() {transport::sendControl("{\"type\":\"refresh\"}");}
void beginEditing() {editRevision=revision;}
}
extern "C" void app_main() {
    esp_lcd_panel_handle_t panel=nullptr;esp_lcd_touch_handle_t touch=nullptr;
    const auto rotation=ESP_LV_ADAPTER_ROTATE_0;
    const auto tearMode=ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT;
    ESP_ERROR_CHECK(waveshare_esp32_s3_rgb_lcd_init(tearMode,rotation,&panel,&touch));
    ESP_ERROR_CHECK(waveshare_rgb_lcd_backlight_on());
    esp_lv_adapter_config_t adapter=ESP_LV_ADAPTER_DEFAULT_CONFIG();adapter.task_stack_size=16*1024;adapter.stack_in_psram=true;
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter));
    esp_lv_adapter_display_config_t config=ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(panel,nullptr,800,480,rotation);config.profile.use_psram=true;
    config.tear_avoid_mode=tearMode;
    auto* display=esp_lv_adapter_register_display(&config);ESP_ERROR_CHECK(display?ESP_OK:ESP_FAIL);
    if(touch) {esp_lv_adapter_touch_config_t tc=ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(display,touch);auto* input=esp_lv_adapter_register_touch(&tc);ESP_ERROR_CHECK(input?ESP_OK:ESP_FAIL);}
    ESP_ERROR_CHECK(esp_lv_adapter_start());
    esp_lv_adapter_lock(-1);
    diagnostics::start(display);
    monitor::init(current);monitor::setCommitCallback(settings);monitor::setRefreshCallback(refresh);
    monitor::setEditingCallback(beginEditing);
    monitor::setPairingCallbacks(transport::openPairing,transport::confirmPairing,transport::forgetPairing);brightness(70);
    esp_lv_adapter_unlock();transport::start();
    for(;;) {
        std::unique_ptr<transport::Event> event(transport::poll(100));
        esp_lv_adapter_lock(-1);
        if(event) {
            if(event->type==transport::EventType::Disconnected)clear();
            else if(event->type==transport::EventType::Pairing&&transport::isCurrent(*event))monitor::showPairing(event->passkey);
            else if(event->type==transport::EventType::Message&&transport::isCurrent(*event)&&transport::isConnected()) {
                try {
                    auto json=nlohmann::json::parse(event->message.body.begin(),event->message.body.end());
                    auto decoded=monitor::decode(json);
                    if(!decoded.data.connected||decoded.data.mock)throw std::runtime_error("Live snapshot required");
                    bool firstSnapshot=!current.connected;
                    current=std::move(decoded.data);revision=decoded.revision;confirmedSettings=decoded.settings;
                    timeval clock{static_cast<time_t>(decoded.producedAt),0};settimeofday(&clock,nullptr);
                    monitor::setSettings(decoded.settings);brightness(decoded.settings.brightnessPercent);monitor::apply(current);
                    if(firstSnapshot&&(monitor::selectedAccount()==-4||monitor::selectedAccount()==-5))monitor::showHome();
                    transport::sendControl(nlohmann::json({{"type","ack"},{"sequence",event->message.sequence}}).dump());
                } catch(const std::exception&) {ESP_LOGW("quotamon","Rejected incomplete or invalid snapshot");}
            }
        }
        if(current.connected&&(!transport::isConnected()||esp_timer_get_time()/1000-transport::lastContactMs()>35000)) {clear();transport::disconnect();}
        diagnostics::tick();
        esp_lv_adapter_unlock();
    }
}
