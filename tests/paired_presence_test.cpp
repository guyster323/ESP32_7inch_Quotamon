#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../native/paired_presence.h"
#include "../native/screen_schedule.h"
#include <cassert>
#include <iostream>
using namespace monitor;
int main() {
    PairedPresenceTracker tracker;
    PairedPresenceInput in{true,true,true,false,false,1,0};
    auto state=tracker.update(in,1000);
    assert(state.state==PairedPresenceState::DisconnectedGrace&&state.graceRemainingSeconds==60);
    assert(!tracker.update(in,60999).shouldSleep());
    assert(tracker.update(in,61000).shouldSleep());
    in.connected=true;in.changedMs=62000;
    assert(tracker.update(in,62000).state==PairedPresenceState::Connected);
    in.connected=false;in.changedMs=70000;
    state=tracker.update(in,71000);assert(state.graceRemainingSeconds==59);
    // Repeated updates (including PC traffic) cannot extend the deadline.
    for(int64_t now=71000;now<130000;now+=1000)assert(!tracker.update(in,now).shouldSleep());
    assert(tracker.update(in,130000).shouldSleep());
    in.connected=true;in.changedMs=130001;
    assert(!tracker.update(in,130001).shouldSleep());
    in.connected=false;in.changedMs=140000;
    // Main stalled over a full grace: sleep immediately using host event time.
    assert(tracker.update(in,201000).shouldSleep());
    in.available=false;
    assert(tracker.update(in,202000).state==PairedPresenceState::Unavailable);
    assert(!tracker.update(in,500000).shouldSleep());
    in.available=true;
    assert(tracker.update(in,500001).graceRemainingSeconds==60);
    assert(!tracker.update(in,560000).shouldSleep());
    assert(tracker.update(in,560001).shouldSleep());
    ++in.targetRevision;
    assert(tracker.update(in,600000).graceRemainingSeconds==60);
    in.enabled=false;assert(tracker.update(in,700000).state==PairedPresenceState::Disabled);
    in.enabled=true;assert(tracker.update(in,700001).graceRemainingSeconds==60);
    in.bonded=false;
    state=tracker.update(in,800000);assert(state.state==PairedPresenceState::Unpaired&&!state.enabled&&!state.shouldSleep());
    in.pairing=true;assert(tracker.update(in,800001).state==PairedPresenceState::Pairing);
    in.bonded=true;in.pairing=false;
    assert(tracker.update(in,900000).graceRemainingSeconds==60);
    assert(tracker.update(in,960000).shouldSleep());
    // Touch overrides both reasons for five minutes, then conditions reapply.
    constexpr int64_t touch=1000000,wakeUntil=touch+300000000;
    assert(!screenShouldBeOff(true,true,touch,wakeUntil));
    assert(!screenShouldBeOff(true,true,wakeUntil-1,wakeUntil));
    assert(screenShouldBeOff(false,true,wakeUntil,wakeUntil));
    assert(screenShouldBeOff(true,false,wakeUntil,wakeUntil));
    assert(!screenShouldBeOff(false,false,wakeUntil,wakeUntil));
    // Even with the phone present, the schedule still turns the screen off.
    in.connected=true;
    assert(!tracker.update(in,970000).shouldSleep());
    assert(screenShouldBeOff(true,tracker.update(in,970001).shouldSleep(),400000000,0));
    PairedPresenceTracker skipped;
    in.connected=false;in.changedMs=0;
    skipped.update(in,1000);
    // Host sees a brief return and another drop between two main polls.
    in.changedMs=1800;
    assert(skipped.update(in,2000).graceRemainingSeconds==60);
    assert(!skipped.update(in,61799).shouldSleep());
    assert(skipped.update(in,61800).shouldSleep());
    assert(skipped.update(in,1000).graceRemainingSeconds==60); // monotonic rollback fails open
    assert(skipped.update(in,61000).shouldSleep());
    // A full host reset/sync can happen between main polls: the epoch preserves
    // the recovery event even if the unavailable boolean was never sampled.
    ++in.radioRevision;
    assert(skipped.update(in,62000).graceRemainingSeconds==60);
    assert(!skipped.update(in,121999).shouldSleep());
    assert(skipped.update(in,122000).shouldSleep());
    std::cout<<"Paired presence: event deadlines, reconnect, fail-open recovery, deletion, target/enable changes, schedule/touch priority PASS\n";
}
