#include "ble_transport.h"
#include <atomic>
#include <string>
#include <new>
#include <memory>
#include <cstring>
#include "sdkconfig.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
extern "C" {
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
void ble_store_config_init(void);
}
namespace transport {
namespace {
const char* TAG="quotamon-ble";
const ble_uuid128_t serviceUuid=BLE_UUID128_INIT(0x74,0x69,0x6e,0x6f,0x6d,0x79,0x9b,0x9d,0x75,0x4b,0xf0,0x9d,0x01,0xe0,0x55,0x71);
const ble_uuid128_t snapshotUuid=BLE_UUID128_INIT(0x74,0x69,0x6e,0x6f,0x6d,0x79,0x9b,0x9d,0x75,0x4b,0xf0,0x9d,0x02,0xe0,0x55,0x71);
const ble_uuid128_t controlUuid=BLE_UUID128_INIT(0x74,0x69,0x6e,0x6f,0x6d,0x79,0x9b,0x9d,0x75,0x4b,0xf0,0x9d,0x03,0xe0,0x55,0x71);
QueueHandle_t events;
QueueHandle_t outgoing;
struct ControlRequest {std::string json;uint32_t epoch;};
std::atomic<uint32_t> epoch{0};
quotamon::Reassembler receive;
std::atomic<uint16_t> connection{BLE_HS_CONN_HANDLE_NONE};
std::atomic<bool> authenticated{false}, subscribed{false};
std::atomic<int64_t> contact{0}, pairUntil{0}, challengeUntil{0}, connectedAt{0};
uint8_t addressType;
uint16_t controlHandle,txSequence=0;
bool post(Event* e) { if(!e)return false; if(xQueueSend(events,&e,0)!=pdTRUE) {delete e;return false;}return true; }
int access(uint16_t conn,uint16_t handle,ble_gatt_access_ctxt* ctx,void*) {
    if(!authenticated || connection!=conn) return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    if(ctx->op!=BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_READ_NOT_PERMITTED;
    auto length=OS_MBUF_PKTLEN(ctx->om); if(length<9||length>512)return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    uint8_t frame[512];if(os_mbuf_copydata(ctx->om,0,length,frame)!=0)return BLE_ATT_ERR_UNLIKELY;
    try {
        quotamon::Message message;
        bool complete=receive.push(frame,length,message);
        contact=esp_timer_get_time()/1000;
        if(complete) {
            if(message.kind==quotamon::Heartbeat) {if(message.body.size()!=8)return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;}
            else if(message.kind==quotamon::Snapshot) {
                auto* event=new(std::nothrow) Event{EventType::Message,std::move(message),0,epoch.load()};
                if(!post(event))return BLE_ATT_ERR_INSUFFICIENT_RES;
            } else return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        }
    } catch(const std::exception&) {return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;}
    return 0;
}
// C arrays avoid lifetime issues in NimBLE's service registration.
ble_gatt_chr_def characteristics[3]{};
ble_gatt_svc_def services[2]{};
void advertise();
int gap(ble_gap_event* e,void*) {
    switch(e->type) {
        case BLE_GAP_EVENT_CONNECT:
            if(e->connect.status==0) {
                ++epoch;connection=e->connect.conn_handle;authenticated=false;subscribed=false;receive.reset();connectedAt=esp_timer_get_time()/1000;
                ble_gap_conn_desc desc{};ble_addr_t peers[CONFIG_BT_NIMBLE_MAX_BONDS]{};int peerCount=0;bool known=false;
                if(ble_gap_conn_find(connection,&desc)==0&&ble_store_util_bonded_peers(peers,&peerCount,CONFIG_BT_NIMBLE_MAX_BONDS)==0)
                    for(int i=0;i<peerCount;++i)
                        if(desc.peer_id_addr.type==peers[i].type&&std::memcmp(desc.peer_id_addr.val,peers[i].val,6)==0) {known=true;break;}
                if(connectedAt>pairUntil&&!known) {ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);}
                else {ble_gap_security_initiate(connection);}
            }
            else {advertise();} break;
        case BLE_GAP_EVENT_DISCONNECT:
            ++epoch;connection=BLE_HS_CONN_HANDLE_NONE;authenticated=false;subscribed=false;challengeUntil=0;receive.reset();
            post(new(std::nothrow) Event{EventType::Disconnected,{}});advertise();break;
        case BLE_GAP_EVENT_ENC_CHANGE: {
            ble_gap_conn_desc desc{};
            authenticated=e->enc_change.status==0&&ble_gap_conn_find(e->enc_change.conn_handle,&desc)==0&&desc.sec_state.encrypted&&desc.sec_state.authenticated;
            if(!authenticated) {ble_gap_terminate(e->enc_change.conn_handle,BLE_ERR_REM_USER_CONN_TERM);} break;
        }
        case BLE_GAP_EVENT_SUBSCRIBE:
            if(e->subscribe.attr_handle==controlHandle) {subscribed=e->subscribe.cur_notify;} break;
        case BLE_GAP_EVENT_PASSKEY_ACTION: {
            if(e->passkey.params.action!=BLE_SM_IOACT_NUMCMP||esp_timer_get_time()/1000>pairUntil) {ble_gap_terminate(e->passkey.conn_handle,BLE_ERR_REM_USER_CONN_TERM);break;}
            challengeUntil=esp_timer_get_time()/1000+60000;
            if(!post(new(std::nothrow) Event{EventType::Pairing,{},e->passkey.params.numcmp,epoch.load()}))ble_gap_terminate(e->passkey.conn_handle,BLE_ERR_REM_USER_CONN_TERM);
            break;
        }
        case BLE_GAP_EVENT_REPEAT_PAIRING:
            // Never discard a saved bond merely because an unknown client asks to re-pair.
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        case BLE_GAP_EVENT_ADV_COMPLETE:advertise();break;
        default:break;
    }
    return 0;
}
void advertise() {
    ble_hs_adv_fields fields{}; fields.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128=const_cast<ble_uuid128_t*>(&serviceUuid);fields.num_uuids128=1;fields.uuids128_is_complete=1;
    if(ble_gap_adv_set_fields(&fields)!=0)return;
    ble_hs_adv_fields scan{};const char* name=ble_svc_gap_device_name();scan.name=reinterpret_cast<const uint8_t*>(name);scan.name_len=strlen(name);scan.name_is_complete=1;
    if(ble_gap_adv_rsp_set_fields(&scan)!=0)return;
    ble_gap_adv_params params{};params.conn_mode=BLE_GAP_CONN_MODE_UND;params.disc_mode=BLE_GAP_DISC_MODE_GEN;
    params.itvl_min=160;params.itvl_max=240;
    ble_gap_adv_start(addressType,nullptr,BLE_HS_FOREVER,&params,gap,nullptr);
}
void sync() {if(ble_hs_util_ensure_addr(0)!=0)return;if(ble_hs_id_infer_auto(0,&addressType)!=0)return;advertise();}
void host(void*) {nimble_port_run();nimble_port_freertos_deinit();}
void transmit(void*) {
    for(;;) {
        ControlRequest* request=nullptr;xQueueReceive(outgoing,&request,portMAX_DELAY);
        std::unique_ptr<ControlRequest> pending(request);uint16_t conn=connection;
        if(pending->epoch!=epoch||conn==BLE_HS_CONN_HANDLE_NONE||!authenticated||!subscribed)continue;
        std::vector<uint8_t> body(pending->json.begin(),pending->json.end());
        bool failed=false;
        for(const auto& frame:quotamon::fragment(quotamon::Control,++txSequence,body,std::min(512,int(ble_att_mtu(conn))-3))) {
            bool accepted=false;
            for(int attempt=0;attempt<20&&!accepted;++attempt) {
                if(pending->epoch!=epoch||connection!=conn||!authenticated)break;
                auto* buffer=ble_hs_mbuf_from_flat(frame.data(),frame.size());
                if(buffer&&ble_gatts_notify_custom(conn,controlHandle,buffer)==0)accepted=true;
                else vTaskDelay(pdMS_TO_TICKS(10));
            }
            if(!accepted){failed=true;break;}
        }
        if(failed)ESP_LOGW(TAG,"Control notification interrupted");
    }
}
}
void start() {
    events=xQueueCreate(4,sizeof(Event*));ESP_ERROR_CHECK(events?ESP_OK:ESP_ERR_NO_MEM);
    outgoing=xQueueCreate(4,sizeof(ControlRequest*));ESP_ERROR_CHECK(outgoing?ESP_OK:ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(nvs_flash_init());ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb=sync;
    ble_hs_cfg.sm_io_cap=BLE_HS_IO_DISPLAY_YESNO;
    ble_hs_cfg.sm_bonding=1;ble_hs_cfg.sm_mitm=1;ble_hs_cfg.sm_sc=1;
    ble_hs_cfg.sm_our_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
    ble_svc_gap_init();ble_svc_gatt_init();ble_svc_gap_device_name_set("Quotamon");
    characteristics[0].uuid=&snapshotUuid.u;characteristics[0].access_cb=access;
    characteristics[0].flags=BLE_GATT_CHR_F_WRITE|BLE_GATT_CHR_F_WRITE_ENC|BLE_GATT_CHR_F_WRITE_AUTHEN;
    characteristics[1].uuid=&controlUuid.u;characteristics[1].access_cb=access;characteristics[1].flags=BLE_GATT_CHR_F_NOTIFY;characteristics[1].val_handle=&controlHandle;
    services[0].type=BLE_GATT_SVC_TYPE_PRIMARY;services[0].uuid=&serviceUuid.u;services[0].characteristics=characteristics;
    ESP_ERROR_CHECK(ble_gatts_count_cfg(services));ESP_ERROR_CHECK(ble_gatts_add_svcs(services));
    ble_store_config_init();nimble_port_freertos_init(host);
    ESP_ERROR_CHECK(xTaskCreate(transmit,"quota-tx",4096,nullptr,4,nullptr)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
    ESP_LOGI(TAG,"BLE peripheral ready; open pairing from the touchscreen");
}
Event* poll(int milliseconds) {Event* e=nullptr;xQueueReceive(events,&e,pdMS_TO_TICKS(milliseconds));if(!authenticated&&connection!=BLE_HS_CONN_HANDLE_NONE&&esp_timer_get_time()/1000-connectedAt>65000)disconnect();return e;}
void openPairing() {pairUntil=esp_timer_get_time()/1000+120000;}
void confirmPairing(bool accept) {
    if(connection==BLE_HS_CONN_HANDLE_NONE||esp_timer_get_time()/1000>challengeUntil)return;
    ble_sm_io io{};io.action=BLE_SM_IOACT_NUMCMP;io.numcmp_accept=accept?1:0;
    ble_sm_inject_io(connection,&io);challengeUntil=0;
}
void forgetPairing() {disconnect();ble_store_clear();pairUntil=0;challengeUntil=0;}
bool sendControl(const std::string& json) {
    if(connection==BLE_HS_CONN_HANDLE_NONE||!authenticated||!subscribed||json.size()>4096)return false;
    auto* copy=new(std::nothrow)ControlRequest{json,epoch.load()};if(!copy)return false;
    if(xQueueSend(outgoing,&copy,0)!=pdTRUE){delete copy;return false;}return true;
}
void disconnect() {uint16_t conn=connection;if(conn!=BLE_HS_CONN_HANDLE_NONE)ble_gap_terminate(conn,BLE_ERR_REM_USER_CONN_TERM);}
int64_t lastContactMs() {return contact;}
bool isConnected() {return connection!=BLE_HS_CONN_HANDLE_NONE&&authenticated;}
bool isCurrent(const Event& event) {return event.epoch==epoch;}
}
