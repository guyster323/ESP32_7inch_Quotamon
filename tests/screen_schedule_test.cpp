#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../native/screen_schedule.h"
#include <cassert>
#include <iostream>
using monitor::screenShouldSleep;
bool off(int minute,int start=1380,int end=420,int offset=0) {
    return screenShouldSleep(true,start,end,minute*60,offset,true,1000,0);
}
int main() {
    assert(!off(1379)&&off(1380)&&off(1439)&&off(0)&&off(419)&&!off(420));
    assert(!off(599,600,1080)&&off(600,600,1080)&&off(1079,600,1080)&&!off(1080,600,1080));
    assert(off(14*60,1380,420,540)); // 14:00 UTC -> 23:00 Korea
    assert(!off(22*60,1380,420,540)); // 22:00 UTC -> 07:00 next day
    assert(off(4*60,1380,420,-300)); // 04:00 UTC -> 23:00 previous day
    assert(!screenShouldSleep(false,1380,420,0,0,true,1000,0));
    assert(!screenShouldSleep(true,1380,420,0,0,false,1000,0));
    assert(!screenShouldSleep(true,0,0,0,0,true,1000,0));
    assert(!screenShouldSleep(true,1380,420,0,0,true,1000,2000));
    assert(screenShouldSleep(true,1380,420,0,0,true,2000,2000));
    std::cout<<"Screen schedule: overnight/daytime boundaries, UTC offsets, unknown clock, touch wake: PASS\n";
}
