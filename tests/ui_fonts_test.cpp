#ifdef NDEBUG
#undef NDEBUG
#endif
#include "lvgl.h"
#include "generated/translations.h"
#include <cassert>
#include <iostream>
extern "C" {
LV_FONT_DECLARE(font_ko14);
LV_FONT_DECLARE(font_ko16);
LV_FONT_DECLARE(font_ko20);
}
void check(const char* text) {
    uint32_t index=0;
    while(text[index]) {
        const auto code=_lv_txt_encoded_next(text,&index);
        if(code<32)continue;
        for(const auto* font:{&font_ko14,&font_ko16,&font_ko20}) {
            lv_font_glyph_dsc_t glyph{};
            if(!lv_font_get_glyph_dsc(font,&glyph,code,0)) {
                std::cerr<<"Missing UI glyph: U+"<<std::hex<<code<<"\n";
                assert(false);
            }
        }
    }
}
int main() {
    for(const auto& row:monitor::i18n::messages)for(const auto* text:row)check(text);
    for(const auto* text:monitor::i18n::names)check(text);
    for(const auto* text:{"선우의 Z Fold7","山田の携帯電話","张伟的手机","Jörgs Gerät","Zoë’s Phone"})check(text);
    check("齋藤の髙橋 龍龘 㐀 㐁 ｶﾀｶﾅ \ufffd");
    std::cout<<"All five-language LCD messages and language names at 14/16/20px: PASS\n";
}
