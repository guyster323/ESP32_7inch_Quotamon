#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../native/ble_sessions.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace transport;
    Sessions peers;
    auto* pc=peers.connect(7,Role::Pc,1000);assert(pc);
    const auto pcKey=pc->key;
    assert(!pc->pcAccess()); // enrollment alone grants no access
    pc->authenticated=true;pc->subscribed=true;
    auto* phone=peers.connect(9,Role::Phone,2000);assert(phone);
    auto phoneKey=phone->key;
    phone->authenticated=true;
    assert(!phone->pcAccess()&&pc->pcAccess());
    assert(peers.count()==2&&!peers.connect(11,Role::Unknown,3000));
    assert(!peers.connect(7,Role::Pc,3000));
    assert(peers.disconnect(phoneKey));
    assert(peers.find(pcKey)->authenticated&&peers.find(pcKey)->subscribed);
    phone=peers.connect(9,Role::Phone,4000);assert(phone);
    const auto replacement=phone->key;
    assert(replacement.generation!=phoneKey.generation);
    assert(!peers.find(phoneKey)&&!peers.disconnect(phoneKey));
    phone->challengeUntil=64000;
    assert(!peers.confirm(phoneKey,true,5000)); // stale UI approval, reused handle
    assert(!peers.confirm({9,replacement.generation,Role::Pc},true,5000));
    assert(peers.confirm(replacement,false,5000)&&!phone->approved);
    assert(!peers.confirm(replacement,true,5001)); // one-use challenge
    phone->challengeUntil=64000;
    assert(!peers.confirm(replacement,true,64000)); // exact timeout boundary
    phone->challengeUntil=65000;
    assert(peers.confirm(replacement,true,64999)&&phone->approved);
    assert(!phone->pcAccess()); // approval is not authenticated registration
    assert(peers.disconnect(pcKey));
    assert(peers.find(replacement)&&phone->approved);
    pc=peers.connect(7,Role::Pc,70000);assert(pc);
    assert(!peers.find(pcKey)&&!pc->subscribed&&!pc->authenticated);
    assert(!peers.disconnect(pcKey)); // late PC event cannot clear new PC
    assert(peers.find(replacement)); // PC reconnection does not clear phone
    assert(peers.disconnect(replacement));
    assert(peers.find(pc->key));
    std::cout<<"BLE role/session isolation and comparison tokens passed\n";
}
