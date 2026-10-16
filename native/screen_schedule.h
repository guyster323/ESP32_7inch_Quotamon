#pragma once
#include <cstdint>
namespace monitor {
// Half-open local-time interval [start,end), including overnight ranges.
// No synchronized clock means stay on. Touch wake uses monotonic time.
inline bool screenShouldSleep(bool enabled,int start,int end,int64_t unixSeconds,int utcOffsetMinutes,
                              bool clockValid,int64_t monotonicUs,int64_t wakeUntilUs) {
    if(!enabled||!clockValid||start==end||monotonicUs<wakeUntilUs)return false;
    const int64_t shifted=unixSeconds/60+utcOffsetMinutes;
    const int minute=static_cast<int>((shifted%1440+1440)%1440);
    return start<end?(minute>=start&&minute<end):(minute>=start||minute<end);
}
}
