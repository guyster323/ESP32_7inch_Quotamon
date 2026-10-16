#include "assets.h"
const lv_img_dsc_t* const cursor_sprites[6] = {&sprite_cursor_idle,&sprite_cursor_caution,&sprite_cursor_critical,&sprite_cursor_exhausted,&sprite_cursor_offline,&sprite_cursor_stale};
const lv_img_dsc_t* const cursor_animation[6][2] = {{&sprite_cursor_idle,&sprite_cursor_idle_01},{&sprite_cursor_caution,&sprite_cursor_caution_01},{&sprite_cursor_critical,&sprite_cursor_critical_01},{&sprite_cursor_exhausted,&sprite_cursor_exhausted_01},{&sprite_cursor_offline,&sprite_cursor_offline_01},{&sprite_cursor_stale,&sprite_cursor_stale_01}};
