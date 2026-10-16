#include "sprite_decoder.h"
#include "lvgl.h"
#include <cstring>

// Shared LVGL draws synchronously with its image cache disabled on both targets.
// One fixed RGB565+alpha scratch buffer supports zoom without heap churn. Cached
// decoder entries would require separate decoded buffers, so fail that build.
#if LV_IMG_CACHE_DEF_SIZE != 0
#error "Quotamon palette decoder requires LV_IMG_CACHE_DEF_SIZE=0"
#endif
#if LV_COLOR_DEPTH != 16 || LV_COLOR_16_SWAP
#error "Quotamon sprites require little-endian RGB565 (LV_COLOR_DEPTH=16, SWAP=0)"
#endif

namespace monitor {
namespace {
constexpr unsigned Width=88, Height=108, Pixels=Width*Height;
alignas(4) uint8_t scratch[Pixels*3];
const lv_img_dsc_t* packedSprite(const void* source) {
    if(lv_img_src_get_type(source)!=LV_IMG_SRC_VARIABLE) return nullptr;
    const auto* image=static_cast<const lv_img_dsc_t*>(source);
    if(image->header.cf!=LV_IMG_CF_RAW_ALPHA||image->header.w!=Width||image->header.h!=Height||
       image->data_size<8||!image->data||std::memcmp(image->data,"QPA1",4)!=0) return nullptr;
    const unsigned colors=image->data[4]|(static_cast<unsigned>(image->data[5])<<8);
    if(!colors||colors>256||image->data_size!=8+colors*3+Pixels) return nullptr;
    return image;
}
lv_res_t info(lv_img_decoder_t*,const void* source,lv_img_header_t* header) {
    const auto* image=packedSprite(source);
    if(!image) return LV_RES_INV;
    *header=image->header;
    header->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;
    return LV_RES_OK;
}
lv_res_t open(lv_img_decoder_t*,lv_img_decoder_dsc_t* descriptor) {
    const auto* image=packedSprite(descriptor->src);
    if(!image) return LV_RES_INV;
    const unsigned colors=image->data[4]|(static_cast<unsigned>(image->data[5])<<8);
    const auto* palette=image->data+8;
    const auto* indices=palette+colors*3;
    for(unsigned i=0;i<Pixels;i++) {
        if(indices[i]>=colors) return LV_RES_INV;
        const auto* entry=palette+indices[i]*3;
        scratch[i*3]=entry[0];scratch[i*3+1]=entry[1];scratch[i*3+2]=entry[2];
    }
    descriptor->img_data=scratch;
    return LV_RES_OK;
}
void close(lv_img_decoder_t*,lv_img_decoder_dsc_t* descriptor) { descriptor->img_data=nullptr; }
}
void initSpriteDecoder() {
    static bool initialized=false;
    if(initialized) return;
    auto* decoder=lv_img_decoder_create();
    lv_img_decoder_set_info_cb(decoder,info);
    lv_img_decoder_set_open_cb(decoder,open);
    lv_img_decoder_set_close_cb(decoder,close);
    initialized=true;
}
}
