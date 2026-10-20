#pragma once
#include <algorithm>
#include <cstdint>

namespace monitor {
enum class PairedPresenceState { Disabled, Unpaired, Pairing, Connected, DisconnectedGrace, Absent, Unavailable };
inline const char* pairedPresenceStateName(PairedPresenceState state) {
    switch(state) {
        case PairedPresenceState::Disabled:return "disabled";
        case PairedPresenceState::Unpaired:return "unpaired";
        case PairedPresenceState::Pairing:return "pairing";
        case PairedPresenceState::Connected:return "connected";
        case PairedPresenceState::DisconnectedGrace:return "disconnected_grace";
        case PairedPresenceState::Absent:return "absent";
        default:return "unavailable";
    }
}
struct PairedPresenceInput {
    bool enabled=false,available=false,bonded=false,connected=false,pairing=false;
    uint32_t targetRevision=0;
    int64_t changedMs=0;
    uint32_t radioRevision=0;
};
struct PairedPresenceStatus {
    PairedPresenceState state=PairedPresenceState::Disabled;
    bool enabled=false,available=false,bonded=false,connected=false;
    int graceRemainingSeconds=0;
    bool shouldSleep() const {return state==PairedPresenceState::Absent;}
};
// Main-task owned. Only the authenticated PHONE link and radio health are inputs;
// PC snapshots, subscriptions, heartbeats and UI refreshes cannot reset grace.
class PairedPresenceTracker {
    bool initialized_=false;
    PairedPresenceInput previous_;
    int64_t graceStartMs_=0,lastNowMs_=0;
public:
    static constexpr int64_t graceMs=60000;
    PairedPresenceStatus update(const PairedPresenceInput& input,int64_t nowMs) {
        const bool restart=!initialized_||nowMs<lastNowMs_||!previous_.enabled||
            !previous_.available||!previous_.bonded||previous_.targetRevision!=input.targetRevision||
            previous_.radioRevision!=input.radioRevision;
        if(restart)graceStartMs_=nowMs;
        else if(!input.connected&&(previous_.connected||previous_.changedMs!=input.changedMs)) {
            // The host records when authentication/link loss actually happened.
            // Account for main-task delays instead of starting another 60s here.
            graceStartMs_=std::clamp(input.changedMs,int64_t(0),nowMs);
        }
        PairedPresenceStatus out;
        out.enabled=input.enabled&&input.bonded;
        out.available=input.available;out.bonded=input.bonded;out.connected=input.connected&&input.available;
        if(!input.enabled)out.state=PairedPresenceState::Disabled;
        else if(!input.available)out.state=PairedPresenceState::Unavailable;
        else if(!input.bonded)out.state=input.pairing?PairedPresenceState::Pairing:PairedPresenceState::Unpaired;
        else if(input.connected)out.state=PairedPresenceState::Connected;
        else {
            const auto remaining=graceMs-std::max(int64_t(0),nowMs-graceStartMs_);
            if(remaining>0) {
                out.state=PairedPresenceState::DisconnectedGrace;
                out.graceRemainingSeconds=static_cast<int>((remaining+999)/1000);
            } else out.state=PairedPresenceState::Absent;
        }
        initialized_=true;previous_=input;lastNowMs_=nowMs;return out;
    }
};
}
