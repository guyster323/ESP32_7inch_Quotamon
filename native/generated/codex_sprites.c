#include "assets.h"
const lv_img_dsc_t* const codex_sprites[6] = {&sprite_codex_idle,&sprite_codex_caution,&sprite_codex_critical,&sprite_codex_exhausted,&sprite_codex_offline,&sprite_codex_stale};
const lv_img_dsc_t* const codex_animation[6][2] = {{&sprite_codex_idle,&sprite_codex_idle_01},{&sprite_codex_caution,&sprite_codex_caution_01},{&sprite_codex_critical,&sprite_codex_critical_01},{&sprite_codex_exhausted,&sprite_codex_exhausted_01},{&sprite_codex_offline,&sprite_codex_offline_01},{&sprite_codex_stale,&sprite_codex_stale_01}};
