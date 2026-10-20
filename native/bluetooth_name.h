#pragma once
#include <cstdint>
#include <string>
namespace monitor {
inline std::string bluetoothName(const uint8_t* bytes,size_t size) {
    std::string out;
    for(size_t i=0;i<size&&out.size()<96;) {
        const uint8_t c=bytes[i];size_t n=c<0x80?1:c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;
        bool valid=n&&i+n<=size;
        for(size_t j=1;valid&&j<n;++j)valid=(bytes[i+j]&0xc0)==0x80;
        if(valid&&n==3)valid=!(c==0xe0&&bytes[i+1]<0xa0)&&!(c==0xed&&bytes[i+1]>=0xa0);
        if(valid&&n==4)valid=!(c==0xf0&&bytes[i+1]<0x90)&&!(c==0xf4&&bytes[i+1]>=0x90);
        if(!valid) {if(out.size()+3>96)break;out+="\xef\xbf\xbd";++i;continue;}
        if(n>1||(c>=32&&c!=127)) {if(out.size()+n>96)break;out.append(reinterpret_cast<const char*>(bytes+i),n);}
        i+=n;
    }
    return out;
}
inline bool validBluetoothName(const std::string& name) {
    return name.size()<=96&&bluetoothName(reinterpret_cast<const uint8_t*>(name.data()),name.size())==name;
}
}
