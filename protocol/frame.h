#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <stdexcept>
#include <algorithm>

namespace quotamon {
constexpr size_t MaxMessage = 32768;
constexpr size_t HeaderSize = 8;
enum Kind : uint8_t { Snapshot = 1, Control = 2, Heartbeat = 3 };
inline uint32_t crc32(const uint8_t* bytes, size_t length) {
    uint32_t crc = 0xffffffff;
    for(size_t i=0;i<length;++i) { crc ^= bytes[i]; for(int j=0;j<8;++j) crc=(crc>>1)^(0xedb88320u & (0u-(crc&1))); }
    return ~crc;
}
inline uint16_t read16(const uint8_t* b) { return uint16_t(b[0] | (uint16_t(b[1])<<8)); }
struct Message { uint8_t kind=0; uint16_t sequence=0; std::vector<uint8_t> body; };
// One ordered, bounded message per connection; interrupted messages never become snapshots.
class Reassembler {
    std::vector<uint8_t> bytes;
    uint16_t seq=0, total=0;
    uint8_t kind=0;
    bool active=false;
public:
    void reset() { bytes.clear(); active=false; }
    bool push(const uint8_t* frame, size_t length, Message& out) {
        auto fail=[this]() { reset(); throw std::runtime_error("Invalid BLE frame"); };
        if(length<=HeaderSize || (frame[0]>>4)!=1 || (frame[0]&15)<1 || (frame[0]&15)>3 || (frame[1]&~3)) fail();
        auto s=read16(frame+2), offset=read16(frame+4), count=read16(frame+6);
        if(count<4 || count>MaxMessage || size_t(offset)+length-HeaderSize>count) fail();
        if(frame[1]&1) {
            if(offset!=0) fail();
            reset(); active=true; seq=s; total=count; kind=frame[0]&15; bytes.reserve(count);
        }
        if(!active || seq!=s || total!=count || kind!=(frame[0]&15) || offset!=bytes.size()) fail();
        bytes.insert(bytes.end(),frame+HeaderSize,frame+length);
        if(!(frame[1]&2)) { if(bytes.size()==total) fail(); return false; }
        if(bytes.size()!=total) fail();
        size_t n=bytes.size()-4;
        uint32_t expected=uint32_t(bytes[n])|(uint32_t(bytes[n+1])<<8)|(uint32_t(bytes[n+2])<<16)|(uint32_t(bytes[n+3])<<24);
        if(crc32(bytes.data(),n)!=expected) fail();
        out.kind=kind; out.sequence=seq; out.body.assign(bytes.begin(),bytes.begin()+n); reset(); return true;
    }
};
inline std::vector<std::vector<uint8_t>> fragment(uint8_t kind, uint16_t seq, const std::vector<uint8_t>& body, size_t valueSize) {
    if(kind<1||kind>3||body.size()+4>MaxMessage||valueSize<=HeaderSize) throw std::runtime_error("BLE message bounds");
    std::vector<uint8_t> bytes=body; uint32_t crc=crc32(body.data(),body.size());
    for(int i=0;i<4;++i) bytes.push_back(uint8_t(crc>>(8*i)));
    std::vector<std::vector<uint8_t>> result;
    for(size_t off=0;off<bytes.size();) {
        size_t take=std::min(valueSize-HeaderSize,bytes.size()-off);
        std::vector<uint8_t> f={uint8_t(0x10|kind),uint8_t((off==0?1:0)|(off+take==bytes.size()?2:0)),uint8_t(seq),uint8_t(seq>>8),uint8_t(off),uint8_t(off>>8),uint8_t(bytes.size()),uint8_t(bytes.size()>>8)};
        f.insert(f.end(),bytes.begin()+off,bytes.begin()+off+take); result.push_back(std::move(f)); off+=take;
    }
    return result;
}
}
