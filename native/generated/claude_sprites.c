#include "assets.h"
const lv_img_dsc_t* const claude_sprites[6] = {&sprite_claude_idle,&sprite_claude_caution,&sprite_claude_critical,&sprite_claude_exhausted,&sprite_claude_offline,&sprite_claude_stale};
const lv_img_dsc_t* const claude_animation[6][2] = {{&sprite_claude_idle,&sprite_claude_idle_01},{&sprite_claude_caution,&sprite_claude_caution_01},{&sprite_claude_critical,&sprite_claude_critical_01},{&sprite_claude_exhausted,&sprite_claude_exhausted_01},{&sprite_claude_offline,&sprite_claude_offline_01},{&sprite_claude_stale,&sprite_claude_stale_01}};
