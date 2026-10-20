#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ble_stubs/ble_test_platform.h"
#include "../firmware/main/ble_transport.cpp"
#include <iostream>
extern "C" void ble_store_config_init(void) {}
using namespace transport;
static ble_addr_t peer(uint8_t seed) {ble_addr_t a{};a.type=1;for(int i=0;i<6;i++)a.val[i]=seed+i;return a;}
static void emit(uint16_t handle,ble_gap_event e,void* generation=nullptr) {
    auto cb=fake::callbacks.at(handle);cb.first(&e,generation?generation:cb.second);
}
static void connectPeer(uint16_t handle,ble_addr_t addr) {
    fake::connections[handle]={};auto& d=fake::connections[handle];d.conn_handle=handle;d.peer_id_addr=addr;
    fake::advertising=false;ble_gap_event e{};e.type=BLE_GAP_EVENT_CONNECT;e.connect.conn_handle=handle;transport::gap(&e,nullptr);
}
static void encrypt(uint16_t handle,bool valid=true) {
    auto& d=fake::connections[handle];d.sec_state={valid,valid,valid};
    ble_gap_event e{};e.type=BLE_GAP_EVENT_ENC_CHANGE;e.enc_change.conn_handle=handle;emit(handle,e);
}
static void subscribe(uint16_t handle) {
    ble_gap_event e{};e.type=BLE_GAP_EVENT_SUBSCRIBE;e.subscribe.conn_handle=handle;e.subscribe.attr_handle=42;e.subscribe.cur_notify=true;emit(handle,e);
}
static void drop(uint16_t handle) {
    ble_gap_event e{};e.type=BLE_GAP_EVENT_DISCONNECT;e.disconnect.conn=fake::connections.at(handle);emit(handle,e);fake::connections.erase(handle);
}
static Event challenge(uint16_t handle) {
    ble_gap_event e{};e.type=BLE_GAP_EVENT_PASSKEY_ACTION;e.passkey.conn_handle=handle;emit(handle,e);
    std::unique_ptr<Event> result(poll(0));assert(result&&result->type==EventType::Pairing);return *result;
}
static void drain() {for(int i=0;i<20&&!events->items.empty();i++) {std::unique_ptr<Event> e(poll(0));}tick(nullptr);}
static int writeHeartbeat(uint16_t handle) {
    auto frames=quotamon::fragment(quotamon::Heartbeat,1,std::vector<uint8_t>(8),244);os_mbuf buffer{frames[0]};
    ble_gatt_access_ctxt context{};context.om=&buffer;return transport::access(handle,0,&context,nullptr);
}
static int readProfile(uint16_t handle,ProfileValue value,std::vector<uint8_t>& result) {
    os_mbuf buffer{};ble_gatt_access_ctxt context{};context.op=BLE_GATT_ACCESS_OP_READ_CHR;context.om=&buffer;
    const int rc=profileAccess(handle,0,&context,reinterpret_cast<void*>(static_cast<uintptr_t>(value)));
    result=buffer.bytes;return rc;
}
static int writeHidControl(uint16_t handle,std::vector<uint8_t> value) {
    os_mbuf buffer{value};ble_gatt_access_ctxt context{};context.om=&buffer;
    return profileAccess(handle,0,&context,reinterpret_cast<void*>(static_cast<uintptr_t>(ProfileValue::Control)));
}
int main() {
    const auto pc=peer(10),phone=peer(30),stranger=peer(50);
    fake::saved={pc};start();
    // Failed controller setup must retry, without flooding commands every tick.
    fake::advDataRc=12;sync();assert(!fake::advertising);
    const auto failedSetupCalls=fake::advDataCalls;
    tick(nullptr);assert(fake::advDataCalls==failedSetupCalls);
    fake::now+=1000;fake::advDataRc=0;fake::advResponseRc=12;tick(nullptr);
    assert(!fake::advertising&&!advertisingConfigured);
    fake::now+=1000;fake::advResponseRc=0;fake::advStartRc=12;tick(nullptr);
    assert(!fake::advertising&&advertisingConfigured);
    const auto configuredCalls=fake::advDataCalls;
    fake::now+=1000;fake::advStartRc=0;tick(nullptr);assert(fake::advertising);
    assert(fake::advertisedName=="Quotamon"&&fake::advertisedBytes==21&&fake::scanResponseBytes==18);
    assert(fake::advertisedService==0x1812&&fake::appearance==0x03c0&&fake::pcServiceInScanResponse);
    assert(services[0].uuid==&serviceUuid.u&&controlHandle==42&&hidReportHandle==77);
    assert(!hidCharacteristics[4].uuid&&hidCharacteristics[2].descriptors==reportDescriptors);
    assert((hidCharacteristics[2].flags&BLE_GATT_CHR_F_WRITE)==0);
    // Parse the actual HID short items: only Consumer usage page, input bits,
    // one report ID, no output/feature items or keyboard/mouse usage page.
    int reportSize=0,reportCount=0,inputBits=0,reportIds=0;
    for(size_t offset=0;offset<sizeof(hidReportMap);) {
        auto prefix=hidReportMap[offset++];size_t n=prefix&3;if(n==3)n=4;
        assert(prefix!=0xfe&&offset+n<=sizeof(hidReportMap));
        unsigned value=0;for(size_t i=0;i<n;++i)value|=unsigned(hidReportMap[offset+i])<<(8*i);
        offset+=n;const auto item=prefix&0xfc;
        if(item==0x04)assert(value==0x0c);
        if(item==0x74)reportSize=value;
        if(item==0x94)reportCount=value;
        if(item==0x84){assert(value==1);reportIds++;}
        if(item==0x80)inputBits+=reportSize*reportCount;
        assert(item!=0x90&&item!=0xb0); // no Output or Feature reports
    }
    assert(inputBits==8&&reportIds==1);
    assert(bonds[0].valid&&!bonds[1].valid&&fake::saved.size()==1); // single legacy PC migration
    assert(fake::nvs.count("quota-roles/pc")&&fake::nvs.count("quota-roles/schema"));
    connectPeer(7,pc);assert(fake::advertising); // second peer can still discover board
    assert(fake::advDataCalls==configuredCalls); // stable payload is not rewritten while PC is connected
    assert(writeHeartbeat(7)==BLE_ATT_ERR_INSUFFICIENT_AUTHEN);
    openPhonePairing();tick(nullptr);assert(fake::terminated.empty()); // enrollment must not interrupt a saved PC reconnect
    cancelPairing();tick(nullptr);assert(fake::terminated.empty());
    encrypt(7);subscribe(7);assert(isConnected()&&writeHeartbeat(7)==0);
    std::vector<uint8_t> profile;
    assert(readProfile(7,ProfileValue::Report,profile)==BLE_ATT_ERR_READ_NOT_PERMITTED);
    assert(writeHidControl(7,{0})==BLE_ATT_ERR_WRITE_NOT_PERMITTED);
    assert(readProfile(99,ProfileValue::Map,profile)==BLE_ATT_ERR_INSUFFICIENT_AUTHEN);
    const auto pcSession=pcKey();
    openPhonePairing();tick(nullptr);connectPeer(9,phone);
    assert(sessions.count()==2&&!fake::advertising);
    auto phoneChallenge=challenge(9);assert(phoneChallenge.session.role==Role::Phone&&isCurrent(phoneChallenge));
    confirmPairing(phoneChallenge,true);tick(nullptr);assert(fake::compared.back()==9);
    fake::saved.push_back(phone);encrypt(9);
    assert(!pairedStatus().phoneConnected&&!sessions.find(9)->authenticated); // wait for main-stack persistence
    assert(writeHeartbeat(9)==BLE_ATT_ERR_INSUFFICIENT_AUTHEN);
    assert(poll(0)==nullptr);tick(nullptr);
    assert(pairedStatus().phoneBonded&&pairedStatus().phoneConnected&&isConnected());
    const auto phoneTargetRevision=pairedStatus().phoneTargetRevision;
    assert(phoneTargetRevision>0&&!pairedStatus().phonePairing);
    const auto peerId=pairedStatus().phonePeerId;
    assert(peerId.size()==32&&fake::nvs.at("quota-roles/phone_id").size()==16);
    assert(readProfile(9,ProfileValue::Map,profile)==0&&profile==std::vector<uint8_t>(std::begin(hidReportMap),std::end(hidReportMap)));
    assert(readProfile(9,ProfileValue::Information,profile)==0&&profile==std::vector<uint8_t>({0x11,1,0,2}));
    assert(readProfile(9,ProfileValue::Reference,profile)==0&&profile==std::vector<uint8_t>({1,1}));
    assert(readProfile(9,ProfileValue::Report,profile)==0&&profile==std::vector<uint8_t>({0}));
    assert(readProfile(9,ProfileValue::Battery,profile)==0x80&&profile.empty());
    assert(writeHidControl(9,{0})==0&&sessions.find(9)->hidSuspended&&pairedStatus().phoneConnected);
    assert(writeHidControl(9,{1})==0&&!sessions.find(9)->hidSuspended&&pairedStatus().phoneConnected);
    assert(writeHidControl(9,{2})==BLE_ATT_ERR_VALUE_NOT_ALLOWED);
    assert(writeHidControl(9,{})==BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN);
    ble_gap_event hidSubscribe{};hidSubscribe.type=BLE_GAP_EVENT_SUBSCRIBE;
    hidSubscribe.subscribe.attr_handle=hidReportHandle;hidSubscribe.subscribe.cur_notify=true;
    hidSubscribe.subscribe.conn_handle=9;emit(9,hidSubscribe);assert(sessions.find(9)->hidSubscribed);
    hidSubscribe.subscribe.conn_handle=7;emit(7,hidSubscribe);assert(!sessions.find(7)->hidSubscribed&&sessions.find(7)->subscribed);
    const auto replacementIdentity=identityOf(stranger);
    assert(!saveBond(Role::Phone,&replacementIdentity)); // late completion cannot overwrite an occupied role
    assert(fake::nvs.at("quota-roles/phone")==std::vector<uint8_t>(bonds[1].identity.begin(),bonds[1].identity.end()));
    subscribe(9);assert(!sessions.find(9)->subscribed&&writeHeartbeat(9)==BLE_ATT_ERR_INSUFFICIENT_AUTHEN);
    assert(sendControl("{\"type\":\"ack\"}"));for(int i=0;i<30;i++)tick(nullptr);
    assert(!fake::notified.empty());for(auto h:fake::notified)assert(h==7);
    for(auto attr:fake::notifiedAttributes)assert(attr==controlHandle); // HID never sends input
    const auto phoneGeneration=fake::callbacks[9].second;
    drop(9);assert(isConnected()&&!pairedStatus().phoneConnected&&pairedStatus().phoneBonded&&fake::advertising);
    assert(pcKey()==pcSession&&sessions.find(7)->subscribed); // phone disconnect preserves PC state
    connectPeer(9,phone);encrypt(9);assert(pairedStatus().phoneConnected);
    assert(pairedStatus().phoneTargetRevision==phoneTargetRevision); // reconnect never changes target
    assert(pairedStatus().phonePeerId==peerId);
    ble_gap_event late{};late.type=BLE_GAP_EVENT_DISCONNECT;late.disconnect.conn=fake::connections[9];emit(9,late,phoneGeneration);
    late.type=BLE_GAP_EVENT_ENC_CHANGE;late.enc_change.conn_handle=9;late.enc_change.status=1;emit(9,late,phoneGeneration);
    late.type=BLE_GAP_EVENT_SUBSCRIBE;late.subscribe.conn_handle=9;late.subscribe.attr_handle=42;late.subscribe.cur_notify=true;emit(9,late,phoneGeneration);
    assert(pairedStatus().phoneConnected&&isConnected()&&!sessions.find(9)->subscribed);
    confirmPairing(phoneChallenge,true);tick(nullptr);assert(fake::compared.size()==1); // late approval rejected
    connectPeer(11,stranger);assert(fake::terminated.back()==11&&isConnected()&&pairedStatus().phoneConnected);
    assert(storeStatus(nullptr,nullptr)==BLE_HS_ESTORE_CAP&&fake::saved.size()==2);
    // Existing PC reset UI queues forget+open together; reopen only after durable deletion.
    forgetPairing();openPairing();tick(nullptr);assert(!isConnected()&&pairedStatus().phoneConnected);
    assert(poll(0)==nullptr);tick(nullptr);assert(pairingRole==Role::Pc&&!bonds[0].valid&&bonds[1].valid);
    assert(fake::saved.size()==1);drop(7);drain();connectPeer(7,pc);
    auto pcChallenge=challenge(7);confirmPairing(pcChallenge,true);tick(nullptr);fake::saved.push_back(pc);encrypt(7);
    subscribe(7);assert(!isConnected()&&!sendControl("pending role cannot receive data"));
    assert(poll(0)==nullptr);tick(nullptr);assert(isConnected()&&sessions.find(7)->subscribed&&pairedStatus().phoneConnected);
    // Queued PC output cannot follow a reused handle; phone survives PC reconnect.
    assert(sendControl("queued old PC message"));const auto sent=fake::notified.size();
    const auto oldPcCallback=fake::callbacks[7].second;
    drop(7);std::unique_ptr<Event> disconnected(poll(0));assert(disconnected&&isCurrent(*disconnected));
    assert(pairedStatus().phoneConnected&&!isConnected());
    connectPeer(7,pc);encrypt(7);subscribe(7);tick(nullptr);
    assert(!isCurrent(*disconnected)&&fake::notified.size()==sent&&pairedStatus().phoneConnected);
    late={};late.type=BLE_GAP_EVENT_DISCONNECT;late.disconnect.conn=fake::connections[7];emit(7,late,oldPcCallback);assert(isConnected());
    // A failed phone-role erase preserves both persisted bonds and the PC.
    fake::failStorage=true;forgetPhonePairing();forgetPhonePairing();tick(nullptr);
    assert(events->items.size()==1);assert(poll(0)==nullptr);tick(nullptr);drain();
    assert(bonds[1].valid&&fake::saved.size()==2&&isConnected());fake::failStorage=false;
    forgetPhonePairing();tick(nullptr);assert(poll(0)==nullptr);tick(nullptr);
    assert(!bonds[1].valid&&bonds[0].valid&&fake::saved.size()==1&&isConnected());drop(9);
    assert(!pairedStatus().phoneBonded&&pairedStatus().phoneTargetRevision>phoneTargetRevision);
    assert(pairedStatus().phonePeerId.empty()&&!fake::nvs.count("quota-roles/phone_id"));
    // Registration storage failure never reports a phone as connected/registered.
    openPhonePairing();tick(nullptr);connectPeer(9,phone);auto second=challenge(9);
    confirmPairing(second,true);tick(nullptr);fake::saved.push_back(phone);encrypt(9);
    fake::failStorage=true;assert(poll(0)==nullptr);tick(nullptr);fake::failStorage=false;drain();
    assert(!pairedStatus().phoneBonded&&!pairedStatus().phoneConnected&&fake::saved.size()==1&&isConnected());drop(9);
    // Forget during the main-stack registration transaction must win over its queued completion.
    openPhonePairing();tick(nullptr);connectPeer(9,phone);auto pending=challenge(9);
    confirmPairing(pending,true);tick(nullptr);fake::saved.push_back(phone);encrypt(9);
    forgetPhonePairing();tick(nullptr);drain();drain();
    assert(!pairedStatus().phoneBonded&&!pairedStatus().phoneConnected&&fake::saved.size()==1&&isConnected());drop(9);
    // Numeric comparison timeout and explicit cancellation cannot approve another link.
    openPhonePairing();tick(nullptr);connectPeer(9,phone);auto expired=challenge(9);
    fake::now+=60000;confirmPairing(expired,true);tick(nullptr);assert(!sessions.find(9)->approved);drop(9);
    cancelPairing();tick(nullptr);connectPeer(9,stranger);assert(fake::terminated.back()==9&&isConnected());
    // Encryption loss revokes PC access immediately.
    encrypt(7,false);assert(!isConnected()&&writeHeartbeat(7)==BLE_ATT_ERR_INSUFFICIENT_AUTHEN);
    reset(1);assert(!pairedStatus().available);
    // An existing schema prevents accidental promotion of an unassigned bond.
    bonds={};fake::saved={phone};loadBonds();assert(!bonds[0].valid&&!bonds[1].valid);
    // A C installation has two durable roles but no display ID. Migrate only
    // metadata; retain both secure identities and reuse the ID on later boots.
    fake::saved={pc,phone};
    const auto pcIdentity=identityOf(pc),phoneIdentity=identityOf(phone);
    fake::nvs["quota-roles/schema"]={1};
    fake::nvs["quota-roles/pc"]={pcIdentity.begin(),pcIdentity.end()};
    fake::nvs["quota-roles/phone"]={phoneIdentity.begin(),phoneIdentity.end()};
    fake::nvs.erase("quota-roles/phone_id");bonds={};phonePeerId.clear();loadBonds();
    assert(bonds[0].valid&&bonds[0].identity==pcIdentity&&bonds[1].valid&&bonds[1].identity==phoneIdentity);
    const auto migratedId=phonePeerId;assert(migratedId.size()==32&&fake::saved.size()==2);
    bonds={};phonePeerId.clear();loadBonds();assert(phonePeerId==migratedId);
    // Display metadata failure cannot reclassify or erase the saved bonds.
    fake::failStorage=true;bonds={};phonePeerId.clear();loadBonds();fake::failStorage=false;
    assert(bonds[0].valid&&bonds[1].valid&&phonePeerId.empty()&&fake::saved.size()==2);
    std::cout<<"Actual BLE transport with simulated host/storage: security and multi-link regressions passed\n";
}
