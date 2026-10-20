#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lv_adapter.h"
extern "C" {
#include "waveshare_rgb_lcd_port.h"
}
#include "ble_transport.h"
#include "runtime_metrics.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "../../native/snapshot_json.h"
#include "../../native/screen_schedule.h"
#include "../../native/paired_presence.h"
#include <memory>
#include <ctime>
#include <sys/time.h>
namespace {
monitor::Snapshot current{false,false,{}};
int revision=0;
int editRevision=0;
monitor::Settings confirmedSettings;
lv_obj_t* dimmer=nullptr;
bool screenOff=false, backlightOff=false, clockValid=false;
int utcOffsetMinutes=0;
int64_t wakeUntilUs=0;
monitor::Settings localDisplay;
monitor::PairedPresenceTracker pairedPresence;
bool pairedPresenceEnabled=true; // Explicit phone registration enables paired sensing; legacy scan settings are ignored.
int pendingPairedPresence=-1;
void setPairedPresenceEnabled(bool enabled) {pendingPairedPresence=enabled?1:0;}
transport::Event shownChallenge{transport::EventType::Pairing};
void confirmShownPairing(bool accept) {transport::confirmPairing(shownChallenge,accept);}
struct PendingSettings {
    monitor::Settings value;
    int baseRevision;
    nlohmann::json accounts;
};
// LVGL callbacks run on a PSRAM stack. NVS partition protection can freeze
// the cache even with XIP enabled, so flash writes must run on main's DRAM stack.
// Both producer and consumer hold the LVGL lock.
std::unique_ptr<PendingSettings> pendingSettings;
const char* tr(const char* key) {return monitor::i18n::text(key,monitor::settings().language);}
void loadDisplay() {
    ESP_ERROR_CHECK(nvs_flash_init());
    nvs_handle_t handle;
    if(nvs_open("quotamon-ui",NVS_READONLY,&handle)!=ESP_OK)return;
    char language[12];size_t size=sizeof(language);
    if(nvs_get_str(handle,"language",language,&size)==ESP_OK&&monitor::i18n::valid(language))localDisplay.language=language;
    uint8_t enabled=0;uint16_t start=1380,end=420;
    nvs_get_u8(handle,"sleep_on",&enabled);nvs_get_u16(handle,"sleep_start",&start);nvs_get_u16(handle,"sleep_end",&end);
    uint8_t pairedOn=1;
    if(nvs_get_u8(handle,"paired_on",&pairedOn)==ESP_OK)pairedPresenceEnabled=pairedOn==1;
    uint8_t savedBrightness=70;
    if(nvs_get_u8(handle,"brightness",&savedBrightness)==ESP_OK&&savedBrightness>=10&&savedBrightness<=100)
        localDisplay.brightnessPercent=savedBrightness;
    if(start<1440&&end<1440&&start!=end) {localDisplay.sleepEnabled=enabled==1;localDisplay.sleepStartMinute=start;localDisplay.sleepEndMinute=end;}
    // Never import old advertising names/addresses/enablement into paired sensing.
    // Legacy scan metadata is retired; paired_on and secure phone roles survive.
    nvs_close(handle);
    // Retire the old scan blob on main's DRAM stack; never import its enablement.
    if(nvs_open("quotamon-ui",NVS_READWRITE,&handle)==ESP_OK) {
        const auto erased=nvs_erase_key(handle,"presence");
        if(erased==ESP_OK)nvs_commit(handle);
        nvs_close(handle);
    }
}
bool persistDisplay(const monitor::Settings& s) {
    if(s.language==localDisplay.language&&s.sleepEnabled==localDisplay.sleepEnabled&&s.brightnessPercent==localDisplay.brightnessPercent&&
       s.sleepStartMinute==localDisplay.sleepStartMinute&&s.sleepEndMinute==localDisplay.sleepEndMinute)return false;
    nvs_handle_t handle;
    if(nvs_open("quotamon-ui",NVS_READWRITE,&handle)!=ESP_OK)return false;
    esp_err_t result=nvs_set_str(handle,"language",s.language.c_str());
    if(result==ESP_OK)result=nvs_set_u8(handle,"brightness",static_cast<uint8_t>(s.brightnessPercent));
    if(result==ESP_OK)result=nvs_set_u8(handle,"sleep_on",s.sleepEnabled);
    if(result==ESP_OK)result=nvs_set_u16(handle,"sleep_start",static_cast<uint16_t>(s.sleepStartMinute));
    if(result==ESP_OK)result=nvs_set_u16(handle,"sleep_end",static_cast<uint16_t>(s.sleepEndMinute));
    if(result==ESP_OK)result=nvs_commit(handle);
    nvs_close(handle);
    if(result==ESP_OK) {
        localDisplay=s;
        ESP_LOGI("quotamon-ui","Display saved: language=%s schedule=%d start=%d end=%d",
            s.language.c_str(),s.sleepEnabled,s.sleepStartMinute,s.sleepEndMinute);
    } else ESP_LOGW("quotamon-ui","Display save failed: %s",esp_err_to_name(result));
    return result==ESP_OK;
}
void commitPairedPresence() {
    if(pendingPairedPresence<0)return;
    const bool enabled=pendingPairedPresence!=0;pendingPairedPresence=-1;
    if(enabled==pairedPresenceEnabled)return;
    nvs_handle_t handle;
    auto rc=nvs_open("quotamon-ui",NVS_READWRITE,&handle);
    if(rc==ESP_OK) {
        rc=nvs_set_u8(handle,"paired_on",enabled?1:0);
        if(rc==ESP_OK)rc=nvs_commit(handle);
        nvs_close(handle);
    }
    if(rc==ESP_OK) {
        pairedPresenceEnabled=enabled;
        // Changing sensing does not cancel a valid five-minute touch wake.
        ESP_LOGI("quotamon-ui","Paired presence enabled=%d saved",enabled);
    } else monitor::notify(tr("감지 설정을 저장하지 못했습니다. 다시 시도하세요."));
}
void brightness(int value) {
    if(!dimmer) {
        dimmer=lv_obj_create(lv_layer_top());lv_obj_remove_style_all(dimmer);lv_obj_set_size(dimmer,800,480);lv_obj_set_style_bg_color(dimmer,lv_color_black(),0);lv_obj_clear_flag(dimmer,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(dimmer,[](lv_event_t*) {wakeUntilUs=esp_timer_get_time()+5LL*60*1000000;},LV_EVENT_PRESSED,nullptr);
    }
    if(screenOff)lv_obj_add_flag(dimmer,LV_OBJ_FLAG_CLICKABLE);else lv_obj_clear_flag(dimmer,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(dimmer,screenOff?static_cast<lv_opa_t>(LV_OPA_COVER):static_cast<lv_opa_t>((100-value)*255/100),0);
}
void clear() {const auto page=monitor::selectedAccount();current.connected=false;current.accounts.clear();monitor::apply(current);if(page==-1||page>=0)monitor::showHome();}
void settings(const monitor::Settings& s,const monitor::Snapshot& data) {
    if(editRevision!=revision) {monitor::setSettings(confirmedSettings);monitor::apply(current);monitor::notify(tr("설정이 변경되어 저장하지 않았습니다. 다시 확인하세요."));return;}
    auto pending=std::make_unique<PendingSettings>();
    pending->value=s;pending->baseRevision=editRevision;
    pending->accounts=nlohmann::json::array();
    for(const auto& a:data.accounts)pending->accounts.push_back({{"id",a.id},{"enabled",a.enabled}});
    pendingSettings=std::move(pending);
}
void commitPendingSettings() {
    if(!pendingSettings)return;
    auto pending=std::move(pendingSettings);
    if(pending->baseRevision!=revision) {monitor::setSettings(confirmedSettings);monitor::apply(current);monitor::notify(tr("설정이 변경되어 저장하지 않았습니다. 다시 확인하세요."));return;}
    const auto& s=pending->value;
    persistDisplay(s);wakeUntilUs=0;
    brightness(s.brightnessPercent);
    nlohmann::json request={{"type","settings"},{"base_revision",pending->baseRevision},{"accounts",std::move(pending->accounts)},{"display",{
        {"language",s.language},{"sleep_enabled",s.sleepEnabled},{"sleep_start_minute",s.sleepStartMinute},{"sleep_end_minute",s.sleepEndMinute},
        {"poll_seconds",s.pollSeconds},{"brightness_percent",s.brightnessPercent},{"show_reset",s.showReset},{"show_updated",s.showUpdated},{"show_credits",s.showCredits},{"show_session",s.showSession}}}};
    monitor::notify(tr(transport::sendControl(request.dump())?"Orca에 설정 저장 요청됨":"기기에 임시 적용됨 · Orca 연결 후 다시 저장하세요."));
}
void refresh() {transport::sendControl("{\"type\":\"refresh\"}");}
void beginEditing() {editRevision=revision;}
}
extern "C" void app_main() {
    loadDisplay();
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
    monitor::init(current);monitor::setSettings(localDisplay);confirmedSettings=localDisplay;
    monitor::setCommitCallback(settings);monitor::setRefreshCallback(refresh);
    monitor::setEditingCallback(beginEditing);
    monitor::setPairingCallbacks(transport::openPairing,confirmShownPairing,transport::forgetPairing);
    monitor::setPairedTrialCallbacks(transport::openPhonePairing,transport::forgetPhonePairing,transport::cancelPairing);
    monitor::setPairedPresenceControl(setPairedPresenceEnabled);brightness(localDisplay.brightnessPercent);
    esp_lv_adapter_unlock();transport::start();
    for(;;) {
        std::unique_ptr<transport::Event> event(transport::poll(100));
        const auto paired=transport::pairedStatus();
        esp_lv_adapter_lock(-1);
        commitPairedPresence();
        const auto pairedState=pairedPresence.update({pairedPresenceEnabled,paired.available,paired.phoneBonded,
            paired.phoneConnected,paired.phonePairing,paired.phoneTargetRevision,paired.phoneChangedMs,paired.radioRevision},esp_timer_get_time()/1000);
        static int64_t lastPresenceUiMs=0;
        if(esp_timer_get_time()/1000-lastPresenceUiMs>=1000) {
            lastPresenceUiMs=esp_timer_get_time()/1000;monitor::updatePairedPresence(pairedState);
        }
        commitPendingSettings();
        if(event) {
            if(event->type==transport::EventType::Notice)monitor::notify(tr("페어링 저장에 실패했습니다. 다시 시도하세요."));
            if(event->type==transport::EventType::Disconnected&&transport::isCurrent(*event))clear();
            else if(event->type==transport::EventType::Pairing&&transport::isCurrent(*event)) {
                shownChallenge=*event;
                if(event->session.role==transport::Role::Phone)monitor::showPhonePairing(event->passkey);
                else monitor::showPairing(event->passkey);
            }
            else if(event->type==transport::EventType::Message&&transport::isCurrent(*event)&&transport::isConnected()) {
                try {
                    auto json=nlohmann::json::parse(event->message.body.begin(),event->message.body.end());
                    if(event->message.kind==quotamon::Control) {
                        transport::sendControl("{\"type\":\"unsupported_control\",\"capability\":\"paired-presence\"}");
                    } else {
                    auto decoded=monitor::decode(json);
                    if(!decoded.data.connected||decoded.data.mock)throw std::runtime_error("Live snapshot required");
                    current=std::move(decoded.data);revision=decoded.revision;confirmedSettings=decoded.settings;
                    timeval clock{static_cast<time_t>(decoded.producedAt),0};settimeofday(&clock,nullptr);
                    utcOffsetMinutes=decoded.utcOffsetMinutes;clockValid=true;
                    static int64_t lastPersistUs=0;
                    const int64_t nowUs=esp_timer_get_time();
                    if((!lastPersistUs||nowUs-lastPersistUs>=60LL*1000000)&&persistDisplay(decoded.settings))lastPersistUs=nowUs;
                    monitor::setSettings(decoded.settings);brightness(decoded.settings.brightnessPercent);monitor::apply(current);
                    transport::sendControl(nlohmann::json({{"type","ack"},{"sequence",event->message.sequence}}).dump());
                    }
                } catch(const std::exception&) {ESP_LOGW("quotamon","Rejected incomplete or invalid snapshot");}
            }
        }
        if(current.connected&&(!transport::isConnected()||esp_timer_get_time()/1000-transport::lastContactMs()>35000)) {clear();transport::disconnect();}
        const auto& displaySettings=monitor::settings();
        const bool scheduleOff=monitor::screenShouldSleep(displaySettings.sleepEnabled,displaySettings.sleepStartMinute,
            displaySettings.sleepEndMinute,time(nullptr),utcOffsetMinutes,clockValid,esp_timer_get_time(),wakeUntilUs);
        const bool off=monitor::screenShouldBeOff(scheduleOff,pairedState.shouldSleep(),esp_timer_get_time(),wakeUntilUs);
        const int64_t nowMs=esp_timer_get_time()/1000;
        static int64_t lastPairedReportMs=0;
        if(transport::isConnected()&&nowMs-lastPairedReportMs>=5000) {
            if(transport::sendControl(nlohmann::json({{"type","paired_presence_status"},{"schema",2},{"capability","paired-presence"},
                {"phone_profile","hid-consumer-trial"},{"automatic_input_enabled",false},
                {"available",paired.available},{"phone_bonded",paired.phoneBonded},{"phone_connected",paired.phoneConnected},
                {"phone_changed_ms",paired.phoneChangedMs},{"enabled",pairedState.enabled},
                {"peer_id",paired.phonePeerId},{"name",paired.phoneBonded?"Phone":""},
                {"presence_state",monitor::pairedPresenceStateName(pairedState.state)},
                {"grace_remaining_seconds",pairedState.graceRemainingSeconds}}).dump()))lastPairedReportMs=nowMs;
        }
        if(off!=screenOff) {
            screenOff=off;monitor::setDisplayAwake(!off);brightness(displaySettings.brightnessPercent);
            ESP_LOGI("quotamon-ui","Screen %s t=%lld: schedule_off=%d presence=%s grace_s=%d touch_wake=%d",
                off?"off":"on",nowMs,scheduleOff,monitor::pairedPresenceStateName(pairedState.state),
                pairedState.graceRemainingSeconds,esp_timer_get_time()<wakeUntilUs);
        }
        static monitor::PairedPresenceState lastState=monitor::PairedPresenceState::Disabled;
        if(lastState!=pairedState.state) {
            lastState=pairedState.state;
            ESP_LOGI("quotamon-ui","Phone presence t=%lld state=%s enabled=%d bonded=%d connected=%d grace_s=%d",
                nowMs,monitor::pairedPresenceStateName(pairedState.state),pairedState.enabled,pairedState.bonded,
                pairedState.connected,pairedState.graceRemainingSeconds);
        }
        esp_lv_adapter_unlock();
        if(off!=backlightOff) {
            const auto result=off?waveshare_rgb_lcd_backlight_off():waveshare_rgb_lcd_backlight_on();
            ESP_LOGI("quotamon-ui","Backlight %s: %s",off?"off":"on",esp_err_to_name(result));
            if(result==ESP_OK)backlightOff=off;
        }
        diagnostics::tick();
    }
}
