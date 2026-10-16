#include "assets.h"
const lv_img_dsc_t* const grok_sprites[6] = {&sprite_grok_idle,&sprite_grok_caution,&sprite_grok_critical,&sprite_grok_exhausted,&sprite_grok_offline,&sprite_grok_stale};
const lv_img_dsc_t* const grok_animation[6][2] = {{&sprite_grok_idle,&sprite_grok_idle_01},{&sprite_grok_caution,&sprite_grok_caution_01},{&sprite_grok_critical,&sprite_grok_critical_01},{&sprite_grok_exhausted,&sprite_grok_exhausted_01},{&sprite_grok_offline,&sprite_grok_offline_01},{&sprite_grok_stale,&sprite_grok_stale_01}};
