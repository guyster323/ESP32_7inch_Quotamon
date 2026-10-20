#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <deque>
#include <vector>
#include <map>
#include <string>
#include <stdexcept>
#include <cassert>
#define CONFIG_BT_NIMBLE_MAX_BONDS 2
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 1
#define ESP_ERR_NVS_NOT_FOUND 2
#define NVS_READWRITE 1
#define NVS_READONLY 0
#define ESP_ERROR_CHECK(x) assert((x)==0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
#define portMAX_DELAY 0xffffffff
#define BLE_HS_CONN_HANDLE_NONE 0xffff
#define BLE_HS_EALREADY 3
#define BLE_HS_ESTORE_CAP 4
#define BLE_ERR_REM_USER_CONN_TERM 0x13
#define BLE_ATT_ERR_INSUFFICIENT_AUTHEN 5
#define BLE_ATT_ERR_READ_NOT_PERMITTED 2
#define BLE_ATT_ERR_WRITE_NOT_PERMITTED 3
#define BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN 13
#define BLE_ATT_ERR_UNLIKELY 14
#define BLE_ATT_ERR_INSUFFICIENT_RES 17
#define BLE_ATT_ERR_VALUE_NOT_ALLOWED 19
#define BLE_ATT_F_READ 1
#define BLE_ATT_F_READ_ENC 4
#define BLE_ATT_F_READ_AUTHEN 16
#define BLE_GATT_ACCESS_OP_READ_CHR 1
#define BLE_GATT_ACCESS_OP_WRITE_CHR 2
#define BLE_GATT_ACCESS_OP_READ_DSC 3
#define BLE_GATT_CHR_F_WRITE 1
#define BLE_GATT_CHR_F_WRITE_ENC 2
#define BLE_GATT_CHR_F_WRITE_AUTHEN 4
#define BLE_GATT_CHR_F_NOTIFY 8
#define BLE_GATT_CHR_F_READ 16
#define BLE_GATT_CHR_F_READ_ENC 32
#define BLE_GATT_CHR_F_READ_AUTHEN 64
#define BLE_GATT_CHR_F_WRITE_NO_RSP 128
#define BLE_GATT_SVC_TYPE_PRIMARY 1
#define BLE_HS_ADV_F_DISC_GEN 2
#define BLE_HS_ADV_F_BREDR_UNSUP 4
#define BLE_GAP_CONN_MODE_UND 2
#define BLE_GAP_DISC_MODE_GEN 2
#define BLE_HS_FOREVER 0
#define BLE_SM_IOACT_NUMCMP 4
#define BLE_HS_IO_DISPLAY_YESNO 1
#define BLE_SM_PAIR_KEY_DIST_ENC 1
#define BLE_SM_PAIR_KEY_DIST_ID 2
#define BLE_GAP_REPEAT_PAIRING_IGNORE 0
enum {BLE_GAP_EVENT_CONNECT=1,BLE_GAP_EVENT_DISCONNECT,BLE_GAP_EVENT_ENC_CHANGE,
 BLE_GAP_EVENT_SUBSCRIBE,BLE_GAP_EVENT_PASSKEY_ACTION,BLE_GAP_EVENT_PARING_COMPLETE,
 BLE_GAP_EVENT_CONN_UPDATE,BLE_GAP_EVENT_REPEAT_PAIRING,BLE_GAP_EVENT_ADV_COMPLETE};
struct ble_addr_t {uint8_t type=0;uint8_t val[6]{};};
struct ble_gap_conn_desc {
 uint16_t conn_handle=0,conn_itvl=24,conn_latency=0,supervision_timeout=500;
 ble_addr_t peer_id_addr;
 struct {bool encrypted=false,authenticated=false,bonded=false;} sec_state;
};
struct ble_gap_event {
 int type=0;
 struct {int status=0;uint16_t conn_handle=0;} connect,enc_change,pairing_complete,conn_update;
 struct {ble_gap_conn_desc conn;int reason=19;} disconnect;
 struct {uint16_t conn_handle=0,attr_handle=0;bool cur_notify=false;} subscribe;
 struct {uint16_t conn_handle=0;struct {int action=BLE_SM_IOACT_NUMCMP;uint32_t numcmp=123456;} params;} passkey;
};
using ble_gap_event_fn=int(ble_gap_event*,void*);
struct os_mbuf {std::vector<uint8_t> bytes;};
#define OS_MBUF_PKTLEN(b) ((b)->bytes.size())
struct ble_gatt_access_ctxt {int op=BLE_GATT_ACCESS_OP_WRITE_CHR;os_mbuf* om=nullptr;};
struct ble_uuid_t {int type=128;};
struct ble_uuid128_t {ble_uuid_t u;uint8_t value[16];};
struct ble_uuid16_t {ble_uuid_t u;uint16_t value;};
#define BLE_UUID128_INIT(...) {{128},{__VA_ARGS__}}
#define BLE_UUID16_INIT(x) {{16},x}
using access_fn=int(uint16_t,uint16_t,ble_gatt_access_ctxt*,void*);
struct ble_gatt_dsc_def {const ble_uuid_t* uuid=nullptr;int att_flags=0;access_fn* access_cb=nullptr;void* arg=nullptr;};
struct ble_gatt_chr_def {const ble_uuid_t* uuid=nullptr;access_fn* access_cb=nullptr;int flags=0;uint16_t* val_handle=nullptr;void* arg=nullptr;ble_gatt_dsc_def* descriptors=nullptr;};
struct ble_gatt_svc_def {int type=0;const ble_uuid_t* uuid=nullptr;ble_gatt_chr_def* characteristics=nullptr;};
struct ble_hs_adv_fields {int flags=0;ble_uuid128_t* uuids128=nullptr;int num_uuids128=0,uuids128_is_complete=0;ble_uuid16_t* uuids16=nullptr;int num_uuids16=0,uuids16_is_complete=0;uint16_t appearance=0;bool appearance_is_present=false;const uint8_t* name=nullptr;size_t name_len=0;int name_is_complete=0;};
struct ble_gap_adv_params {int conn_mode=0,disc_mode=0,itvl_min=0,itvl_max=0;};
struct ble_sm_io {int action=0,numcmp_accept=0;};
struct ble_store_status_event {};
struct ble_npl_event {};
struct ble_npl_callout {};
using esp_err_t=int;
using nvs_handle_t=std::string*;
struct FakeQueue {size_t limit=0;std::deque<void*> items;};
using QueueHandle_t=FakeQueue*;
using SemaphoreHandle_t=int;
namespace fake {
inline int64_t now=1000;
inline bool advertising=false,failStorage=false;
inline int advDataRc=0,advResponseRc=0,advStartRc=0,advDataCalls=0,advStartCalls=0;
inline std::string advertisedName;
inline int advertisedBytes=0;
inline int scanResponseBytes=0;inline uint16_t advertisedService=0,appearance=0;
inline bool pcServiceInScanResponse=false;
inline std::map<uint16_t,ble_gap_conn_desc> connections;
inline std::map<uint16_t,std::pair<ble_gap_event_fn*,void*>> callbacks;
inline std::vector<ble_addr_t> saved;
inline std::vector<uint16_t> terminated,notified,compared;
inline std::vector<uint16_t> notifiedAttributes;
inline std::map<std::string,std::vector<uint8_t>> nvs;
}
inline int64_t esp_timer_get_time() {return fake::now*1000;}
inline int nvs_flash_init() {return 0;}
inline int nvs_open(const char* ns,int,nvs_handle_t* h) {*h=new std::string(ns);return 0;}
inline void nvs_close(nvs_handle_t h) {delete h;}
inline int nvs_get_blob(nvs_handle_t h,const char* k,void* out,size_t* size) {
 auto it=fake::nvs.find(*h+"/"+k);if(it==fake::nvs.end())return ESP_ERR_NVS_NOT_FOUND;
 if(*size<it->second.size())return 1;*size=it->second.size();std::memcpy(out,it->second.data(),*size);return 0;
}
inline int nvs_get_u8(nvs_handle_t h,const char* k,uint8_t* out) {size_t n=1;return nvs_get_blob(h,k,out,&n);}
inline int nvs_set_blob(nvs_handle_t h,const char* k,const void* data,size_t n) {
 if(fake::failStorage)return 1;
 auto p=static_cast<const uint8_t*>(data);fake::nvs[*h+"/"+k]={p,p+n};return 0;
}
inline int nvs_set_u8(nvs_handle_t h,const char* k,uint8_t value) {return nvs_set_blob(h,k,&value,1);}
inline int nvs_erase_key(nvs_handle_t h,const char* k) {if(fake::failStorage)return 1;return fake::nvs.erase(*h+"/"+k)?0:ESP_ERR_NVS_NOT_FOUND;}
inline int nvs_commit(nvs_handle_t) {return fake::failStorage?1:0;}
inline QueueHandle_t xQueueCreate(size_t n,size_t) {return new FakeQueue{n,{}};}
inline int xQueueSend(QueueHandle_t q,const void* p,int) {if(q->items.size()>=q->limit)return 0;q->items.push_back(*static_cast<void* const*>(p));return 1;}
inline int xQueueSendToFront(QueueHandle_t q,const void* p,int) {if(q->items.size()>=q->limit)return 0;q->items.push_front(*static_cast<void* const*>(p));return 1;}
inline int xQueueReceive(QueueHandle_t q,void* p,int) {if(q->items.empty())return 0;*static_cast<void**>(p)=q->items.front();q->items.pop_front();return 1;}
inline SemaphoreHandle_t xSemaphoreCreateMutex() {return 1;}
inline int xSemaphoreTake(SemaphoreHandle_t,int) {return 1;}
inline void xSemaphoreGive(SemaphoreHandle_t) {}
inline int ble_store_util_bonded_peers(ble_addr_t* out,int* n,int capacity) {*n=static_cast<int>(fake::saved.size());if(*n>capacity)return 1;for(int i=0;i<*n;i++)out[i]=fake::saved[i];return 0;}
inline int ble_store_util_delete_peer(const ble_addr_t* addr) {
 for(auto it=fake::saved.begin();it!=fake::saved.end();++it)if(it->type==addr->type&&!std::memcmp(it->val,addr->val,6)) {fake::saved.erase(it);return 0;}return 1;
}
inline int ble_gap_conn_find(uint16_t h,ble_gap_conn_desc* out) {auto it=fake::connections.find(h);if(it==fake::connections.end())return 1;*out=it->second;return 0;}
inline int ble_gap_set_event_cb(uint16_t h,ble_gap_event_fn* fn,void* arg) {fake::callbacks[h]={fn,arg};return 0;}
inline int ble_gap_security_initiate(uint16_t) {return 0;}
inline int ble_gap_terminate(uint16_t h,int) {fake::terminated.push_back(h);return 0;}
inline int ble_sm_inject_io(uint16_t h,ble_sm_io*) {fake::compared.push_back(h);return 0;}
inline bool ble_gap_adv_active() {return fake::advertising;}
inline int ble_gap_adv_set_fields(ble_hs_adv_fields* f) {
 fake::advDataCalls++;fake::advertisedName=f->name?std::string(reinterpret_cast<const char*>(f->name),f->name_len):"";
 fake::advertisedBytes=(f->flags?3:0)+(f->num_uuids128?2+16*f->num_uuids128:0)+(f->num_uuids16?2+2*f->num_uuids16:0)+(f->appearance_is_present?4:0)+(f->name_len?2+static_cast<int>(f->name_len):0);
 fake::advertisedService=f->num_uuids16?f->uuids16[0].value:0;fake::appearance=f->appearance;
 return fake::advDataRc;
}
inline int ble_gap_adv_rsp_set_fields(ble_hs_adv_fields* f) {
 fake::scanResponseBytes=f->num_uuids128?2+16*f->num_uuids128:0;
 fake::pcServiceInScanResponse=f->num_uuids128==1&&f->uuids128_is_complete;
 return fake::advResponseRc;
}
inline int ble_gap_adv_start(uint8_t,void*,int,ble_gap_adv_params*,ble_gap_event_fn*,void*) {
 fake::advStartCalls++;if(fake::advStartRc)return fake::advStartRc;fake::advertising=true;return 0;
}
inline int ble_hs_util_ensure_addr(int) {return 0;}
inline int ble_hs_id_infer_auto(int,uint8_t* type) {*type=0;return 0;}
inline int os_mbuf_copydata(os_mbuf* p,int offset,int n,void* out) {std::memcpy(out,p->bytes.data()+offset,n);return 0;}
inline int os_mbuf_append(os_mbuf* p,const void* data,size_t n) {auto b=static_cast<const uint8_t*>(data);p->bytes.insert(p->bytes.end(),b,b+n);return 0;}
inline os_mbuf* ble_hs_mbuf_from_flat(const void* data,size_t n) {auto p=static_cast<const uint8_t*>(data);return new os_mbuf{{p,p+n}};}
inline int ble_gatts_notify_custom(uint16_t h,uint16_t attr,os_mbuf* p) {fake::notified.push_back(h);fake::notifiedAttributes.push_back(attr);delete p;return 0;}
inline uint16_t ble_att_mtu(uint16_t) {return 247;}
inline int ble_gatts_count_cfg(ble_gatt_svc_def*) {return 0;}
inline int ble_gatts_add_svcs(ble_gatt_svc_def* s) {*s[0].characteristics[1].val_handle=42;*s[1].characteristics[2].val_handle=77;return 0;}
inline void ble_svc_gap_init() {}
inline void ble_svc_gatt_init() {}
inline void ble_svc_gap_device_name_set(const char*) {}
inline void ble_svc_gap_device_appearance_set(uint16_t) {}
inline const char* ble_svc_gap_device_name() {return "Quotamon";}
inline int nimble_port_init() {return 0;}
inline void nimble_port_run() {}
inline void nimble_port_freertos_init(void(*)(void*)) {}
inline void nimble_port_freertos_deinit() {}
inline void* nimble_port_get_dflt_eventq() {return nullptr;}
inline void ble_npl_callout_init(ble_npl_callout*,void*,void(*)(ble_npl_event*),void*) {}
inline void ble_npl_callout_reset(ble_npl_callout*,int) {}
inline int ble_npl_time_ms_to_ticks32(int n) {return n;}
inline struct FakeHsConfig {
 void(*sync_cb)()=nullptr;void(*reset_cb)(int)=nullptr;
 int sm_io_cap=0,sm_bonding=0,sm_mitm=0,sm_sc=0,sm_our_key_dist=0,sm_their_key_dist=0;
 int(*store_status_cb)(ble_store_status_event*,void*)=nullptr;
} ble_hs_cfg;
