#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace transport {
enum class Role : uint8_t { Unknown, Pc, Phone };
struct SessionKey {
    uint16_t handle=0xffff;
    uint32_t generation=0;
    Role role=Role::Unknown;
    bool operator==(const SessionKey& b) const {
        return handle==b.handle&&generation==b.generation&&role==b.role;
    }
};
struct Session {
    SessionKey key;
    bool active=false, authenticated=false, subscribed=false, approved=false, registering=false, enrolling=false;
    bool hidSubscribed=false,hidSuspended=false;
    int64_t connectedAt=0, challengeUntil=0;
    bool pcAccess() const {return active&&authenticated&&key.role==Role::Pc;}
};
// Host-task owned. Cross-task commands always carry the complete session key.
class Sessions {
    std::array<Session,2> slots_{};
    uint32_t generation_=0;
public:
    Session* find(uint16_t handle) {
        for(auto& s:slots_)if(s.active&&s.key.handle==handle)return &s;
        return nullptr;
    }
    Session* find(SessionKey key) {
        auto* s=find(key.handle);return s&&s->key==key?s:nullptr;
    }
    Session* connect(uint16_t handle,Role role,int64_t now) {
        if(find(handle))return nullptr;
        for(auto& s:slots_)if(!s.active) {
            s=Session{};s.active=true;s.key={handle,++generation_,role};s.connectedAt=now;return &s;
        }
        return nullptr;
    }
    bool confirm(SessionKey key,bool accept,int64_t now) {
        auto* s=find(key);
        if(!s||!s->challengeUntil||now>=s->challengeUntil)return false;
        s->challengeUntil=0;s->approved=accept;return true;
    }
    bool disconnect(SessionKey key) {
        auto* s=find(key);if(!s)return false;*s=Session{};return true;
    }
    auto& all() {return slots_;}
    size_t count() const {size_t n=0;for(const auto& s:slots_)n+=s.active;return n;}
};
}
