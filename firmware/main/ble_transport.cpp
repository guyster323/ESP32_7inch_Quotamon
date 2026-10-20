#include "ble_transport.h"
#include <atomic>
#include <memory>
#include <cstring>
#include <algorithm>
#include "sdkconfig.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
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
const ble_uuid16_t hidUuid=BLE_UUID16_INIT(0x1812),disUuid=BLE_UUID16_INIT(0x180a),basUuid=BLE_UUID16_INIT(0x180f);
const ble_uuid16_t hidInfoUuid=BLE_UUID16_INIT(0x2a4a),hidMapUuid=BLE_UUID16_INIT(0x2a4b),
    hidReportUuid=BLE_UUID16_INIT(0x2a4d),hidControlUuid=BLE_UUID16_INIT(0x2a4c),
    hidReferenceUuid=BLE_UUID16_INIT(0x2908),modelUuid=BLE_UUID16_INIT(0x2a24),
    pnpUuid=BLE_UUID16_INIT(0x2a50),batteryUuid=BLE_UUID16_INIT(0x2a19);
// Consumer Control only: report ID 1, two volume bits and six constant bits.
// There is no keyboard/mouse collection, output report, or input-send path.
constexpr uint8_t hidReportMap[]={0x05,0x0c,0x09,0x01,0xa1,0x01,0x85,0x01,
    0x15,0x00,0x25,0x01,0x09,0xe9,0x09,0xea,0x75,0x01,0x95,0x02,0x81,0x02,
    0x75,0x06,0x95,0x01,0x81,0x03,0xc0};
constexpr uint8_t hidInformation[]={0x11,0x01,0x00,0x02}; // normally connectable; no remote wake
constexpr uint8_t hidReference[]={1,1}; // report ID 1, Input
// Experimental, unassigned vendor/product identity; not another vendor's VID.
constexpr uint8_t hidPnp[]={1,0xff,0xff,1,0,0,1};
enum class ProfileValue:uintptr_t { Information=1,Map,Report,Control,Reference,Model,Pnp,Battery };

using Identity=std::array<uint8_t,7>;
struct Bond {bool valid=false;Identity identity{};};
std::array<Bond,2> bonds{}; // PC, phone; host task owns after start()
std::array<bool,2> blockedRoles{};
std::string phonePeerId;
std::string loadPhonePeerId() {
    nvs_handle_t h;if(nvs_open("quota-roles",NVS_READONLY,&h)!=ESP_OK)return {};
    uint8_t bytes[16]{};size_t size=sizeof(bytes);
    const auto rc=nvs_get_blob(h,"phone_id",bytes,&size);nvs_close(h);
    if(rc!=ESP_OK||size!=sizeof(bytes))return {};
    const char hex[]="0123456789abcdef";std::string out;
    for(const auto byte:bytes) {out+=hex[byte>>4];out+=hex[byte&15];}
    return out;
}
int index(Role role) {return role==Role::Pc?0:1;}
const char* roleName(Role role) {return role==Role::Pc?"pc":role==Role::Phone?"phone":"unknown";}
Identity identityOf(const ble_addr_t& addr) {
    Identity id{};id[0]=addr.type&1;std::memcpy(id.data()+1,addr.val,6);return id;
}
ble_addr_t addressOf(const Identity& id) {
    ble_addr_t addr{};addr.type=id[0];std::memcpy(addr.val,id.data()+1,6);return addr;
}
bool bonded(const Identity& id) {
    ble_addr_t peers[CONFIG_BT_NIMBLE_MAX_BONDS]{};int count=0;
    if(ble_store_util_bonded_peers(peers,&count,CONFIG_BT_NIMBLE_MAX_BONDS))return false;
    for(int i=0;i<count;++i)if(identityOf(peers[i])==id)return true;
    return false;
}
Role knownRole(const Identity& id) {
    if(!bonded(id))return Role::Unknown;
    for(int i=0;i<2;++i)if(!blockedRoles[i]&&bonds[i].valid&&bonds[i].identity==id)return i==0?Role::Pc:Role::Phone;
    return Role::Unknown;
}
// Only start()/poll() (main's DRAM stack) may write application role metadata.
// NimBLE continues to own all LTK/IRK storage.
bool saveBond(Role role,const Identity* id) {
    nvs_handle_t h;esp_err_t rc=nvs_open("quota-roles",NVS_READWRITE,&h);
    if(rc!=ESP_OK)return false;
    const char* key=role==Role::Pc?"pc":"phone";
    if(id) {
        Identity existing{};size_t size=existing.size();
        const auto read=nvs_get_blob(h,key,existing.data(),&size);
        // Enrollment may fill an empty role, never replace a different saved peer.
        // This also serializes overlapping registration completions on main.
        if((read==ESP_OK&&(size!=existing.size()||existing!=*id))||
           (read!=ESP_OK&&read!=ESP_ERR_NVS_NOT_FOUND)) {nvs_close(h);return false;}
    }
    rc=id?nvs_set_blob(h,key,id->data(),id->size()):nvs_erase_key(h,key);
    if(rc==ESP_ERR_NVS_NOT_FOUND)rc=ESP_OK;
    if(rc==ESP_OK&&role==Role::Phone) {
        if(id) {
            uint8_t opaque[16]{};size_t size=sizeof(opaque);
            const auto read=nvs_get_blob(h,"phone_id",opaque,&size);
            if(read==ESP_ERR_NVS_NOT_FOUND) {
                esp_fill_random(opaque,sizeof(opaque));rc=nvs_set_blob(h,"phone_id",opaque,sizeof(opaque));
            } else if(read!=ESP_OK||size!=sizeof(opaque))rc=ESP_FAIL;
        } else {
            rc=nvs_erase_key(h,"phone_id");if(rc==ESP_ERR_NVS_NOT_FOUND)rc=ESP_OK;
        }
    }
    if(rc==ESP_OK)rc=nvs_set_u8(h,"schema",1);
    if(rc==ESP_OK)rc=nvs_commit(h);
    nvs_close(h);return rc==ESP_OK;
}
void loadBonds() {
    nvs_handle_t h;bool schemaPresent=false;
    if(nvs_open("quota-roles",NVS_READONLY,&h)==ESP_OK) {
        uint8_t schema=0;const auto rc=nvs_get_u8(h,"schema",&schema);
        // A corrupt/unknown schema must never trigger legacy role assignment.
        schemaPresent=rc!=ESP_ERR_NVS_NOT_FOUND;
        if(rc==ESP_OK&&schema==1)for(int i=0;i<2;++i) {
            Identity id{};size_t size=id.size();
            if(nvs_get_blob(h,i==0?"pc":"phone",id.data(),&size)==ESP_OK&&size==id.size()&&id[0]<=1&&bonded(id))bonds[i]={true,id};
        }
        nvs_close(h);
    }
    if(!schemaPresent) {
        ble_addr_t peers[CONFIG_BT_NIMBLE_MAX_BONDS]{};int count=0;
        if(ble_store_util_bonded_peers(peers,&count,CONFIG_BT_NIMBLE_MAX_BONDS)==0&&count==1) {
            // This upgrade starts from the PC-only, single-bond firmware.
            auto id=identityOf(peers[0]);
            if(saveBond(Role::Pc,&id)) {bonds[0]={true,id};ESP_LOGI(TAG,"Legacy single PC bond preserved");}
        } else if(count>1)ESP_LOGW(TAG,"Ambiguous legacy bonds; role registration required");
    }
    if(bonds[0].valid&&bonds[1].valid&&bonds[0].identity==bonds[1].identity)bonds={};
    if(bonds[1].valid) {
        // Migrate existing C phone roles without replacing their identity or keys.
        if(saveBond(Role::Phone,&bonds[1].identity))phonePeerId=loadPhonePeerId();
        else ESP_LOGE("quotamon-ble","Phone display identity unavailable; pairing preserved");
    }
}
Sessions sessions;
quotamon::Reassembler receive; // strictly PC-only, reset only on PC transitions
QueueHandle_t events, commands;
SemaphoreHandle_t stateMutex;
SessionKey publicPc,publicChallenge;
bool publicPcReady=false,publicSubscribed=false;
PairedStatus publicStatus;
Bond publishedPhoneBond;
std::atomic<int64_t> contact{0};
uint8_t addressType;
uint16_t controlHandle,hidReportHandle,txSequence=0;
Role pairingRole=Role::Unknown;
Role reopenRole=Role::Unknown;
int64_t pairUntil=0;
bool radioReady=false;
uint32_t radioRevision=0;
bool advertisingConfigured=false;
int64_t advertiseRetryAt=0;
ble_npl_callout pump;
struct Command {
    enum Type { Open,Cancel,Confirm,ForgetRole,Disconnect,Send,Commit,Erased } type;
    SessionKey key;
    bool accepted=false;
    Identity identity{};
    std::string json;
};
struct Transmission {SessionKey key;std::vector<std::vector<uint8_t>> frames;size_t next=0;int retries=0;};
std::unique_ptr<Transmission> sending;
int64_t nowMs() {return esp_timer_get_time()/1000;}
bool post(Event* event) {
    if(!event)return false;
    if(xQueueSend(events,&event,0)!=pdTRUE) {delete event;return false;}return true;
}
bool enqueue(Command* command) {
    if(!command)return false;
    if(xQueueSend(commands,&command,0)!=pdTRUE) {delete command;return false;}return true;
}
void publish() {
    xSemaphoreTake(stateMutex,portMAX_DELAY);
    publicPcReady=false;publicSubscribed=false;publicStatus.available=radioReady;publicStatus.radioRevision=radioRevision;
    bool phone=false;
    for(auto& s:sessions.all())if(s.active) {
        if(s.key.role==Role::Pc) {publicPc=s.key;publicPcReady=s.pcAccess();publicSubscribed=s.subscribed;}
        if(s.key.role==Role::Phone&&s.authenticated)phone=true;
    }
    if(phone!=publicStatus.phoneConnected)publicStatus.phoneChangedMs=nowMs();
    publicStatus.phoneConnected=phone;publicStatus.phoneBonded=bonds[1].valid&&!blockedRoles[1];
    publicStatus.phonePeerId=publicStatus.phoneBonded?phonePeerId:std::string();
    if(publishedPhoneBond.valid!=bonds[1].valid||publishedPhoneBond.identity!=bonds[1].identity) {
        ++publicStatus.phoneTargetRevision;publishedPhoneBond=bonds[1];
    }
    publicStatus.phonePairing=pairingRole==Role::Phone&&nowMs()<pairUntil;
    publicChallenge={};
    for(auto& s:sessions.all())if(s.active&&s.challengeUntil)publicChallenge=s.key;
    xSemaphoreGive(stateMutex);
}
void terminate(Session& s) {s.authenticated=false;s.subscribed=false;s.hidSubscribed=false;s.hidSuspended=false;s.approved=false;s.challengeUntil=0;s.registering=true;ble_gap_terminate(s.key.handle,BLE_ERR_REM_USER_CONN_TERM);}
void advertise();
void reportParameters(const Session& s,const ble_gap_conn_desc& desc) {
    // Deliberately exclude peer addresses, keys and the numeric comparison value.
    ESP_LOGI(TAG,"link t=%lld role=%s handle=%u generation=%lu auth=%d interval_units=%u latency=%u supervision_units=%u",
        nowMs(),roleName(s.key.role),s.key.handle,static_cast<unsigned long>(s.key.generation),s.authenticated,
        desc.conn_itvl,desc.conn_latency,desc.supervision_timeout);
}
int access(uint16_t conn,uint16_t,ble_gatt_access_ctxt* ctx,void*) {
    auto* s=sessions.find(conn);
    if(!s||!s->pcAccess())return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    if(ctx->op!=BLE_GATT_ACCESS_OP_WRITE_CHR)return BLE_ATT_ERR_READ_NOT_PERMITTED;
    auto length=OS_MBUF_PKTLEN(ctx->om);if(length<9||length>512)return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    uint8_t frame[512];if(os_mbuf_copydata(ctx->om,0,length,frame)!=0)return BLE_ATT_ERR_UNLIKELY;
    try {
        quotamon::Message message;const bool complete=receive.push(frame,length,message);
        if(complete) {
            if(message.kind==quotamon::Heartbeat) {if(message.body.size()!=8)return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;}
            else if(message.kind==quotamon::Snapshot||message.kind==quotamon::Control) {
                if(!post(new(std::nothrow) Event{EventType::Message,std::move(message),0,s->key}))return BLE_ATT_ERR_INSUFFICIENT_RES;
            } else return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        }
        contact=nowMs();
    } catch(const std::exception&) {return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;}
    return 0;
}
int profileAccess(uint16_t conn,uint16_t,ble_gatt_access_ctxt* ctx,void* arg) {
    auto* s=sessions.find(conn);
    if(!s||!s->authenticated)return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    const auto value=static_cast<ProfileValue>(reinterpret_cast<uintptr_t>(arg));
    const bool metadata=value==ProfileValue::Model||value==ProfileValue::Pnp;
    if(s->key.role!=Role::Phone&&!metadata)
        return ctx->op==BLE_GATT_ACCESS_OP_WRITE_CHR?BLE_ATT_ERR_WRITE_NOT_PERMITTED:BLE_ATT_ERR_READ_NOT_PERMITTED;
    if(value==ProfileValue::Control) {
        if(ctx->op!=BLE_GATT_ACCESS_OP_WRITE_CHR)return BLE_ATT_ERR_READ_NOT_PERMITTED;
        uint8_t state=0;
        if(OS_MBUF_PKTLEN(ctx->om)!=1)return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        if(os_mbuf_copydata(ctx->om,0,1,&state))return BLE_ATT_ERR_UNLIKELY;
        if(state>1)return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        s->hidSuspended=state==0;
        ESP_LOGI(TAG,"HID control t=%lld suspended=%d; authenticated link retained",nowMs(),s->hidSuspended);
        return 0;
    }
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR&&ctx->op!=BLE_GATT_ACCESS_OP_READ_DSC)return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    const uint8_t* data=nullptr;size_t size=0;
    static const uint8_t neutral=0;
    static const char model[]="Quotamon HID trial";
    switch(value) {
        case ProfileValue::Information:data=hidInformation;size=sizeof(hidInformation);break;
        case ProfileValue::Map:data=hidReportMap;size=sizeof(hidReportMap);break;
        case ProfileValue::Report:data=&neutral;size=1;break;
        case ProfileValue::Reference:data=hidReference;size=sizeof(hidReference);break;
        case ProfileValue::Model:data=reinterpret_cast<const uint8_t*>(model);size=sizeof(model)-1;break;
        case ProfileValue::Pnp:data=hidPnp;size=sizeof(hidPnp);break;
        // USB-powered board has no measured battery. Expose the companion
        // characteristic but report unavailable, never a fictitious percentage.
        case ProfileValue::Battery:return 0x80; // application error: measurement unavailable
        default:return BLE_ATT_ERR_UNLIKELY;
    }
    return os_mbuf_append(ctx->om,data,size)?BLE_ATT_ERR_INSUFFICIENT_RES:0;
}
void tryAuthenticate(Session& s,const ble_gap_conn_desc& desc) {
    if(!desc.sec_state.encrypted||!desc.sec_state.authenticated||!desc.sec_state.bonded)return;
    const auto id=identityOf(desc.peer_id_addr);
    if(s.key.role!=Role::Unknown&&bonds[index(s.key.role)].valid&&bonds[index(s.key.role)].identity!=id) {
        terminate(s);publish();return;
    }
    if(knownRole(id)==s.key.role&&s.key.role!=Role::Unknown) {
        s.authenticated=true;
        if(s.key.role==Role::Pc)contact=nowMs();
    } else if(s.approved&&!s.registering&&s.key.role!=Role::Unknown&&bonded(id)) {
        s.registering=true;
        if(!post(new(std::nothrow) Event{EventType::Registered,{},0,s.key,id}))terminate(s);
    }
    reportParameters(s,desc);publish();
}
int gap(ble_gap_event* e,void* arg) {
    if(e->type==BLE_GAP_EVENT_CONNECT) {
        if(e->connect.status!=0) {advertise();return 0;}
        ble_gap_conn_desc desc{};
        if(ble_gap_conn_find(e->connect.conn_handle,&desc))return 0;
        const auto id=identityOf(desc.peer_id_addr);Role role=knownRole(id);
        const bool enrolling=role==Role::Unknown;
        if(role==Role::Unknown&&nowMs()<pairUntil&&pairingRole!=Role::Unknown&&!bonds[index(pairingRole)].valid)role=pairingRole;
        bool duplicate=false;
        for(const auto& s:sessions.all())if(s.active&&s.key.role==role)duplicate=true;
        auto* s=(role==Role::Unknown||duplicate)?nullptr:sessions.connect(e->connect.conn_handle,role,nowMs());
        if(!s)ble_gap_terminate(e->connect.conn_handle,BLE_ERR_REM_USER_CONN_TERM);
        else {
            s->enrolling=enrolling;
            ble_gap_set_event_cb(s->key.handle,gap,reinterpret_cast<void*>(static_cast<uintptr_t>(s->key.generation)));
            if(role==Role::Pc)receive.reset();
            reportParameters(*s,desc);publish();
            const int rc=ble_gap_security_initiate(s->key.handle);
            if(rc&&rc!=BLE_HS_EALREADY)terminate(*s);
        }
        advertise();return 0;
    }
    if(e->type==BLE_GAP_EVENT_ADV_COMPLETE) {advertise();return 0;}
    uint16_t handle=BLE_HS_CONN_HANDLE_NONE;
    switch(e->type) {
        case BLE_GAP_EVENT_DISCONNECT:handle=e->disconnect.conn.conn_handle;break;
        case BLE_GAP_EVENT_ENC_CHANGE:handle=e->enc_change.conn_handle;break;
        case BLE_GAP_EVENT_SUBSCRIBE:handle=e->subscribe.conn_handle;break;
        case BLE_GAP_EVENT_PASSKEY_ACTION:handle=e->passkey.conn_handle;break;
        case BLE_GAP_EVENT_PARING_COMPLETE:handle=e->pairing_complete.conn_handle;break;
        case BLE_GAP_EVENT_CONN_UPDATE:handle=e->conn_update.conn_handle;break;
        case BLE_GAP_EVENT_REPEAT_PAIRING:return BLE_GAP_REPEAT_PAIRING_IGNORE;
        default:return 0;
    }
    auto* s=sessions.find(handle);
    // The callback carries an immutable generation, including after handle reuse.
    if(!s||s->key.generation!=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg)))return 0;
    switch(e->type) {
        case BLE_GAP_EVENT_DISCONNECT: {
            const auto key=s->key;
            ESP_LOGI(TAG,"disconnect t=%lld role=%s handle=%u generation=%lu reason=%d",nowMs(),roleName(key.role),handle,static_cast<unsigned long>(key.generation),e->disconnect.reason);
            sessions.disconnect(key);
            if(key.role==Role::Pc) {receive.reset();post(new(std::nothrow) Event{EventType::Disconnected,{},0,key});}
            publish();advertise();break;
        }
        case BLE_GAP_EVENT_ENC_CHANGE:
        case BLE_GAP_EVENT_PARING_COMPLETE: {
            const int status=e->type==BLE_GAP_EVENT_ENC_CHANGE?e->enc_change.status:e->pairing_complete.status;
            ble_gap_conn_desc desc{};
            s->authenticated=false;
            if(status||ble_gap_conn_find(handle,&desc)||!desc.sec_state.encrypted||!desc.sec_state.authenticated)terminate(*s);
            else tryAuthenticate(*s,desc);
            publish();break;
        }
        case BLE_GAP_EVENT_SUBSCRIBE:
            // Remember PC subscription intent while main commits its role. Every
            // actual send still requires pcAccess(); phones never get this flag.
            if(e->subscribe.attr_handle==controlHandle)s->subscribed=s->key.role==Role::Pc&&e->subscribe.cur_notify;
            if(e->subscribe.attr_handle==hidReportHandle) {
                s->hidSubscribed=s->key.role==Role::Phone&&e->subscribe.cur_notify;
                ESP_LOGI(TAG,"HID subscription role=%s auth=%d enabled=%d",roleName(s->key.role),s->authenticated,s->hidSubscribed);
            }
            publish();break;
        case BLE_GAP_EVENT_PASSKEY_ACTION: {
            if(e->passkey.params.action!=BLE_SM_IOACT_NUMCMP||nowMs()>=pairUntil||s->key.role!=pairingRole||bonds[index(s->key.role)].valid) {terminate(*s);break;}
            bool other=false;for(const auto& otherSession:sessions.all())if(otherSession.challengeUntil)other=true;
            if(other) {terminate(*s);break;}
            s->challengeUntil=nowMs()+60000;
            if(!post(new(std::nothrow) Event{EventType::Pairing,{},e->passkey.params.numcmp,s->key}))terminate(*s);
            publish();break;
        }
        case BLE_GAP_EVENT_CONN_UPDATE: {
            ble_gap_conn_desc desc{};if(!ble_gap_conn_find(handle,&desc))reportParameters(*s,desc);break;
        }
        default:break;
    }
    return 0;
}
void advertise() {
    if(!radioReady||ble_gap_adv_active())return;
    if(sessions.count()>=2||nowMs()<advertiseRetryAt)return;
    const auto failed=[](const char* stage,int rc) {
        advertiseRetryAt=nowMs()+1000;
        ESP_LOGW(TAG,"advertise failed stage=%s rc=%d sessions=%u",stage,rc,static_cast<unsigned>(sessions.count()));
    };
    if(!advertisingConfigured) {
        // Native phone discovery sees the real HID service and generic HID
        // appearance. Active PC discovery still sees its existing 128-bit UUID
        // in the scan response; the PC service/attribute order is unchanged.
        ble_hs_adv_fields fields{};fields.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
        fields.uuids16=const_cast<ble_uuid16_t*>(&hidUuid);fields.num_uuids16=1;fields.uuids16_is_complete=0;
        fields.appearance=0x03c0;fields.appearance_is_present=1;
        const char* name=ble_svc_gap_device_name();fields.name=reinterpret_cast<const uint8_t*>(name);fields.name_len=strlen(name);fields.name_is_complete=1;
        int rc=ble_gap_adv_set_fields(&fields);
        if(rc) {failed("data",rc);return;}
        ble_hs_adv_fields scan{};
        scan.uuids128=const_cast<ble_uuid128_t*>(&serviceUuid);scan.num_uuids128=1;scan.uuids128_is_complete=1;
        rc=ble_gap_adv_rsp_set_fields(&scan);
        if(rc) {failed("scan-response",rc);return;}
        // The payload is constant: reconnects only need to enable advertising.
        advertisingConfigured=true;
    }
    ble_gap_adv_params params{};params.conn_mode=BLE_GAP_CONN_MODE_UND;params.disc_mode=BLE_GAP_DISC_MODE_GEN;params.itvl_min=160;params.itvl_max=240;
    const int rc=ble_gap_adv_start(addressType,nullptr,BLE_HS_FOREVER,&params,gap,nullptr);
    if(rc)failed("start",rc);
    else ESP_LOGI(TAG,"advertising started sessions=%u",static_cast<unsigned>(sessions.count()));
}
void reset(int reason) {
    ++radioRevision;
    radioReady=false;advertisingConfigured=false;advertiseRetryAt=0;sending.reset();
    for(auto& s:sessions.all())if(s.active) {
        auto key=s.key;sessions.disconnect(key);
        if(key.role==Role::Pc)post(new(std::nothrow) Event{EventType::Disconnected,{},0,key});
    }
    pairingRole=Role::Unknown;pairUntil=0;receive.reset();publish();
    ESP_LOGW(TAG,"host reset reason=%d",reason);
}
void sync() {
    if(ble_hs_util_ensure_addr(0)||ble_hs_id_infer_auto(0,&addressType))return;
    advertisingConfigured=false;advertiseRetryAt=0;
    ++radioRevision;radioReady=true;publish();advertise();
}
void host(void*) {nimble_port_run();nimble_port_freertos_deinit();}
void process(Command& c) {
    auto* s=sessions.find(c.key);
    if(c.type==Command::Open) {
        reopenRole=Role::Unknown;
        if(bonds[index(c.key.role)].valid) {
            if(blockedRoles[index(c.key.role)])reopenRole=c.key.role;
            return;
        }
        // Only one role's enrollment window/challenge may exist at a time.
        for(auto& pending:sessions.all())if(pending.active&&pending.enrolling&&!pending.authenticated)terminate(pending);
        pairingRole=c.key.role;pairUntil=nowMs()+120000;
        ESP_LOGI(TAG,"pairing window role=%s opened; advertising=%d sessions=%u",roleName(pairingRole),ble_gap_adv_active(),static_cast<unsigned>(sessions.count()));advertise();
    } else if(c.type==Command::Cancel) {
        pairingRole=Role::Unknown;reopenRole=Role::Unknown;pairUntil=0;
        for(auto& pending:sessions.all())if(pending.active&&pending.enrolling&&!pending.authenticated)terminate(pending);
    } else if(c.type==Command::Confirm) {
        if(!sessions.confirm(c.key,c.accepted,nowMs()))return;
        ble_sm_io io{};io.action=BLE_SM_IOACT_NUMCMP;io.numcmp_accept=c.accepted;
        if(ble_sm_inject_io(c.key.handle,&io)||!c.accepted)terminate(*s);
    } else if(c.type==Command::Commit) {
        // A saved role is useful on reconnect even if the enrollment link ended.
        if(c.accepted&&bonded(c.identity)) {
            bonds[index(c.key.role)]={true,c.identity};
            if(c.key.role==Role::Phone)phonePeerId=c.json;
        }
        if(!c.accepted&&knownRole(c.identity)==Role::Unknown) {
            const auto addr=addressOf(c.identity);ble_store_util_delete_peer(&addr);
            post(new(std::nothrow) Event{EventType::Notice});
        }
        if(s) {
            ble_gap_conn_desc desc{};
            if(c.accepted&&!ble_gap_conn_find(s->key.handle,&desc)&&identityOf(desc.peer_id_addr)==c.identity)tryAuthenticate(*s,desc);
            else terminate(*s);
        }
        pairingRole=Role::Unknown;pairUntil=0;
        ESP_LOGI(TAG,"registration role=%s saved=%d",roleName(c.key.role),c.accepted);
    } else if(c.type==Command::ForgetRole) {
        if(blockedRoles[index(c.key.role)])return; // one durable deletion transaction per role
        auto& bond=bonds[index(c.key.role)];Identity id=bond.identity;bool stored=bond.valid;
        if(pairingRole==c.key.role) {pairingRole=Role::Unknown;pairUntil=0;}
        for(auto& other:sessions.all())if(other.active&&other.key.role==c.key.role) {
            ble_gap_conn_desc desc{};
            if(!stored&&!ble_gap_conn_find(other.key.handle,&desc)&&bonded(identityOf(desc.peer_id_addr))) {
                id=identityOf(desc.peer_id_addr);stored=true;
            }
            terminate(other);
        }
        if(!stored) {publish();return;}
        blockedRoles[index(c.key.role)]=true; // revoke authority until persistence completes
        if(!post(new(std::nothrow) Event{EventType::Forget,{},0,c.key,id}))blockedRoles[index(c.key.role)]=false;
    } else if(c.type==Command::Erased) {
        if(c.accepted) {
            bonds[index(c.key.role)]={};
            if(c.key.role==Role::Phone)phonePeerId.clear();
            const auto addr=addressOf(c.identity);const int rc=ble_store_util_delete_peer(&addr);
            ESP_LOGI(TAG,"forget role=%s bond_delete_rc=%d",roleName(c.key.role),rc);
        } else {ESP_LOGW(TAG,"Role erase failed; saved pairing preserved");post(new(std::nothrow) Event{EventType::Notice});}
        blockedRoles[index(c.key.role)]=false;
        if(reopenRole==c.key.role) {
            reopenRole=Role::Unknown;
            if(c.accepted) {pairingRole=c.key.role;pairUntil=nowMs()+120000;}
        }
    } else if(c.type==Command::Disconnect) {if(s&&s->key.role==Role::Pc)terminate(*s);}
    else if(c.type==Command::Send&&s&&s->pcAccess()&&s->subscribed&&!sending) {
        std::vector<uint8_t> body(c.json.begin(),c.json.end());
        sending=std::make_unique<Transmission>();sending->key=s->key;
        sending->frames=quotamon::fragment(quotamon::Control,++txSequence,body,std::min(512,int(ble_att_mtu(s->key.handle))-3));
    }
    publish();
}
void tick(ble_npl_event*) {
    // Every BLE operation and mutable session field is owned by the host task.
    for(int i=0;i<8;++i) {
        Command* raw=nullptr;
        if(xQueueReceive(commands,&raw,0)!=pdTRUE)break;
        std::unique_ptr<Command> c(raw);
        // Don't drop control messages while a previous fragmented send is active.
        if(c->type==Command::Send&&sending) {
            raw=c.release();if(xQueueSendToFront(commands,&raw,0)!=pdTRUE)delete raw;break;
        }
        process(*c);
    }
    for(auto& s:sessions.all())if(s.active&&!s.authenticated) {
        if((s.challengeUntil&&nowMs()>=s.challengeUntil)||nowMs()-s.connectedAt>=65000)terminate(s);
        else if(s.approved&&!s.registering) {ble_gap_conn_desc d{};if(!ble_gap_conn_find(s.key.handle,&d))tryAuthenticate(s,d);}
    }
    if(sending) {
        auto* s=sessions.find(sending->key);
        if(!s||!s->pcAccess()||!s->subscribed)sending.reset();
        else {
            const auto& frame=sending->frames[sending->next];auto* buffer=ble_hs_mbuf_from_flat(frame.data(),frame.size());
            if(buffer&&ble_gatts_notify_custom(s->key.handle,controlHandle,buffer)==0) {
                sending->retries=0;if(++sending->next==sending->frames.size())sending.reset();
            } else if(++sending->retries>=20)sending.reset();
        }
    }
    publish();if(radioReady)advertise();
    ble_npl_callout_reset(&pump,ble_npl_time_ms_to_ticks32(10));
}
ble_gatt_chr_def characteristics[3]{};
ble_gatt_chr_def hidCharacteristics[5]{},disCharacteristics[3]{},basCharacteristics[2]{};
ble_gatt_dsc_def reportDescriptors[2]{};
ble_gatt_svc_def services[5]{};
void configureProfile() {
    const auto read=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_READ_ENC|BLE_GATT_CHR_F_READ_AUTHEN;
    const auto characteristic=[](ble_gatt_chr_def& c,const ble_uuid_t* uuid,int flags,ProfileValue value) {
        c.uuid=uuid;c.flags=flags;c.access_cb=profileAccess;c.arg=reinterpret_cast<void*>(static_cast<uintptr_t>(value));
    };
    characteristic(hidCharacteristics[0],&hidInfoUuid.u,read,ProfileValue::Information);
    characteristic(hidCharacteristics[1],&hidMapUuid.u,read,ProfileValue::Map);
    characteristic(hidCharacteristics[2],&hidReportUuid.u,read|BLE_GATT_CHR_F_NOTIFY,ProfileValue::Report);
    hidCharacteristics[2].val_handle=&hidReportHandle;
    reportDescriptors[0].uuid=&hidReferenceUuid.u;
    reportDescriptors[0].att_flags=BLE_ATT_F_READ|BLE_ATT_F_READ_ENC|BLE_ATT_F_READ_AUTHEN;
    reportDescriptors[0].access_cb=profileAccess;
    reportDescriptors[0].arg=reinterpret_cast<void*>(static_cast<uintptr_t>(ProfileValue::Reference));
    hidCharacteristics[2].descriptors=reportDescriptors;
    characteristic(hidCharacteristics[3],&hidControlUuid.u,BLE_GATT_CHR_F_WRITE_NO_RSP|BLE_GATT_CHR_F_WRITE_ENC|BLE_GATT_CHR_F_WRITE_AUTHEN,ProfileValue::Control);
    characteristic(disCharacteristics[0],&modelUuid.u,read,ProfileValue::Model);
    characteristic(disCharacteristics[1],&pnpUuid.u,read,ProfileValue::Pnp);
    characteristic(basCharacteristics[0],&batteryUuid.u,read,ProfileValue::Battery);
    services[1].type=BLE_GATT_SVC_TYPE_PRIMARY;services[1].uuid=&hidUuid.u;services[1].characteristics=hidCharacteristics;
    services[2].type=BLE_GATT_SVC_TYPE_PRIMARY;services[2].uuid=&disUuid.u;services[2].characteristics=disCharacteristics;
    services[3].type=BLE_GATT_SVC_TYPE_PRIMARY;services[3].uuid=&basUuid.u;services[3].characteristics=basCharacteristics;
}
// Refuse storage overflow: never evict the existing PC/phone bond.
int storeStatus(ble_store_status_event*,void*) {ESP_LOGW(TAG,"Bond storage full; no peer evicted");return BLE_HS_ESTORE_CAP;}
SessionKey pcKey() {xSemaphoreTake(stateMutex,portMAX_DELAY);auto key=publicPc;xSemaphoreGive(stateMutex);return key;}
}
void start() {
    stateMutex=xSemaphoreCreateMutex();ESP_ERROR_CHECK(stateMutex?ESP_OK:ESP_ERR_NO_MEM);
    events=xQueueCreate(12,sizeof(Event*));commands=xQueueCreate(16,sizeof(Command*));
    ESP_ERROR_CHECK(events&&commands?ESP_OK:ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(nvs_flash_init());ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb=sync;ble_hs_cfg.reset_cb=reset;
    ble_hs_cfg.sm_io_cap=BLE_HS_IO_DISPLAY_YESNO;ble_hs_cfg.sm_bonding=1;ble_hs_cfg.sm_mitm=1;ble_hs_cfg.sm_sc=1;
    ble_hs_cfg.sm_our_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
    ble_svc_gap_init();ble_svc_gatt_init();ble_svc_gap_device_name_set("Quotamon");
    ble_svc_gap_device_appearance_set(0x03c0);
    characteristics[0].uuid=&snapshotUuid.u;characteristics[0].access_cb=access;
    characteristics[0].flags=BLE_GATT_CHR_F_WRITE|BLE_GATT_CHR_F_WRITE_ENC|BLE_GATT_CHR_F_WRITE_AUTHEN;
    characteristics[1].uuid=&controlUuid.u;characteristics[1].access_cb=access;characteristics[1].flags=BLE_GATT_CHR_F_NOTIFY;characteristics[1].val_handle=&controlHandle;
    services[0].type=BLE_GATT_SVC_TYPE_PRIMARY;services[0].uuid=&serviceUuid.u;services[0].characteristics=characteristics;
    configureProfile();
    ESP_ERROR_CHECK(ble_gatts_count_cfg(services));ESP_ERROR_CHECK(ble_gatts_add_svcs(services));
    ble_store_config_init();ble_hs_cfg.store_status_cb=storeStatus;loadBonds();publish();
    ble_npl_callout_init(&pump,nimble_port_get_dflt_eventq(),tick,nullptr);
    ble_npl_callout_reset(&pump,ble_npl_time_ms_to_ticks32(10));
    nimble_port_freertos_init(host);
    ESP_LOGI(TAG,"Paired presence: 2 connections; PC bond=%d phone bond=%d; scanning disabled",bonds[0].valid,bonds[1].valid);
    ESP_LOGI(TAG,"HID consumer trial: no input transmission; PC control=%u HID input=%u; battery unavailable",controlHandle,hidReportHandle);
}
Event* poll(int milliseconds) {
    Event* raw=nullptr;xQueueReceive(events,&raw,pdMS_TO_TICKS(milliseconds));
    if(raw&&(raw->type==EventType::Registered||raw->type==EventType::Forget)) {
        std::unique_ptr<Event> e(raw);
        bool success=saveBond(e->session.role,e->type==EventType::Registered?&e->identity:nullptr);
        auto* c=new(std::nothrow) Command{e->type==EventType::Registered?Command::Commit:Command::Erased,e->session,success,e->identity};
        if(c&&success&&e->type==EventType::Registered&&e->session.role==Role::Phone)c->json=loadPhonePeerId();
        if(!enqueue(c))ESP_LOGE(TAG,"Persistence completion queue full");
        return nullptr;
    }
    return raw;
}
void openPairing() {enqueue(new(std::nothrow) Command{Command::Open,{0xffff,0,Role::Pc}});}
void openPhonePairing() {enqueue(new(std::nothrow) Command{Command::Open,{0xffff,0,Role::Phone}});}
void cancelPairing() {enqueue(new(std::nothrow) Command{Command::Cancel});}
void confirmPairing(const Event& e,bool accept) {enqueue(new(std::nothrow) Command{Command::Confirm,e.session,accept});}
void forgetPairing() {enqueue(new(std::nothrow) Command{Command::ForgetRole,{0xffff,0,Role::Pc}});}
void forgetPhonePairing() {enqueue(new(std::nothrow) Command{Command::ForgetRole,{0xffff,0,Role::Phone}});}
bool sendControl(const std::string& json) {
    if(json.size()>4096)return false;
    xSemaphoreTake(stateMutex,portMAX_DELAY);const auto key=publicPc;const bool ready=publicPcReady&&publicSubscribed;xSemaphoreGive(stateMutex);
    return ready&&enqueue(new(std::nothrow) Command{Command::Send,key,false,{},json});
}
void disconnect() {enqueue(new(std::nothrow) Command{Command::Disconnect,pcKey()});}
int64_t lastContactMs() {return contact;}
bool isConnected() {xSemaphoreTake(stateMutex,portMAX_DELAY);bool ready=publicPcReady;xSemaphoreGive(stateMutex);return ready;}
bool isCurrent(const Event& e) {
    xSemaphoreTake(stateMutex,portMAX_DELAY);
    const bool current=e.session==(e.type==EventType::Pairing?publicChallenge:publicPc);
    xSemaphoreGive(stateMutex);return current;
}
PairedStatus pairedStatus() {xSemaphoreTake(stateMutex,portMAX_DELAY);auto status=publicStatus;xSemaphoreGive(stateMutex);return status;}
}
