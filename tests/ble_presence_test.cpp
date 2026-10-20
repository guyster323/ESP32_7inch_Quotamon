#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../native/bluetooth_name.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace monitor;
    for(const auto* name:{"선우의 Z Fold7","山田の携帯電話","张伟的手机","Jörgs Gerät","Zoë’s Phone"}) {
        const std::string text=name;
        assert(bluetoothName(reinterpret_cast<const uint8_t*>(text.data()),text.size())==text);
        assert(validBluetoothName(text));
    }
    const uint8_t broken[]={0xed,0xa0,0x80,0xc0,0xaf,0xf4,0x90,0x80,0x80,0,10,'A'};
    const auto cleaned=bluetoothName(broken,sizeof(broken));assert(validBluetoothName(cleaned)&&cleaned.back()=='A');
    const std::string cut=std::string(95,'x')+"한";
    assert(bluetoothName(reinterpret_cast<const uint8_t*>(cut.data()),cut.size())==std::string(95,'x'));
    assert(!validBluetoothName(std::string("name\n"))&&!validBluetoothName(std::string(97,'x')));
    std::cout<<"Shared UTF-8 Bluetooth labels: multilingual, malformed input, codepoint limits PASS\n";
}
