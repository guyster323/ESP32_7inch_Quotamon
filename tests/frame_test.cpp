#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../protocol/frame.h"
#include <cassert>
#include <iostream>
int main() {
    std::vector<uint8_t> body(24000); for(size_t i=0;i<body.size();++i) body[i]=uint8_t(i);
    for(size_t size:{20,244,512}) {
        auto frames=quotamon::fragment(1,65535,body,size); quotamon::Reassembler r; quotamon::Message m;
        for(size_t i=0;i<frames.size();++i) assert(r.push(frames[i].data(),frames[i].size(),m)==(i+1==frames.size()));
        assert(m.body==body && m.sequence==65535 && m.kind==1);
        frames.back().back()^=1; bool rejected=false;
        try { for(auto& f:frames) r.push(f.data(),f.size(),m); } catch(const std::exception&) { rejected=true; } assert(rejected);
        r.reset(); assert(!r.push(frames[0].data(),frames[0].size(),m));
        rejected=false; try { r.push(frames[2].data(),frames[2].size(),m); } catch(const std::exception&) { rejected=true; } assert(rejected);
    }
    assert(quotamon::crc32(reinterpret_cast<const uint8_t*>("123456789"),9)==0xcbf43926);
    assert(quotamon::fragment(1,0x1234,{'a','b','c'},20)[0]==std::vector<uint8_t>({0x11,3,0x34,0x12,0,0,7,0,0x61,0x62,0x63,0xc2,0x41,0x24,0x35}));
    std::cout << "CRC, MTU20/244/512, 24KB roundtrip, corruption, missing fragment: PASS\n";
}
