#pragma once
#include "frame.h"
#include <cstdint>
#include <string>
namespace transport {
enum class EventType { Message, Disconnected, Pairing };
struct Event { EventType type; quotamon::Message message; uint32_t passkey=0; uint32_t epoch=0; };
void start();
Event* poll(int milliseconds);
void openPairing();
void confirmPairing(bool accept);
void forgetPairing();
bool sendControl(const std::string& json);
void disconnect();
int64_t lastContactMs();
bool isConnected();
bool isCurrent(const Event& event);
}
