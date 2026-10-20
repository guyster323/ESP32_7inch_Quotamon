#pragma once
#include "frame.h"
#include <cstdint>
#include <string>
#include "../../native/ble_sessions.h"
namespace transport {
enum class EventType { Message, Disconnected, Pairing, Registered, Forget, Notice };
struct Event {
    EventType type;
    quotamon::Message message;
    uint32_t passkey=0;
    SessionKey session;
    std::array<uint8_t,7> identity{}; // private NVS work; never sent over GATT
};
struct PairedStatus {
    bool available=false, phoneBonded=false, phoneConnected=false;
    int64_t phoneChangedMs=0;
    bool phonePairing=false;
    uint32_t phoneTargetRevision=0;
    uint32_t radioRevision=0;
    std::string phonePeerId;
};
void start();
Event* poll(int milliseconds);
void openPairing();
void openPhonePairing();
void cancelPairing();
void confirmPairing(const Event& challenge,bool accept);
void forgetPairing();
void forgetPhonePairing();
bool sendControl(const std::string& json);
void disconnect();
int64_t lastContactMs();
bool isConnected();
bool isCurrent(const Event& event);
PairedStatus pairedStatus();
}
