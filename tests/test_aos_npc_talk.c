#include "../src/runtime/aos/game/aos_npc_talk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    AosNpcTalk t;

    memset(&t, 0, sizeof t);
    aos_npc_talk_17_update(&t, false);
    assert(t.arm_42c_1 && t.window_text == 0x17 && t.state == 1);
    aos_npc_talk_17_update(&t, false);
    assert(!t.entity_release);
    aos_npc_talk_17_update(&t, true);
    assert(t.entity_release);

    memset(&t, 0, sizeof t);
    aos_npc_talk_18_update(&t, false);
    assert(t.window_text == 0x18 && t.state == 1);
    aos_npc_talk_18_update(&t, true);
    assert(t.window_close && (t.flags59 & 0x8));
    t.state = 0x80;
    aos_npc_talk_18_update(&t, false);
    assert(t.entity_release);

    assert(aos_npc_talk_arm_flag());

    /* misc Object20 callbacks. */
    uint8_t a5c = 0; bool del = false;
    assert(aos_npc_talk_sprite_gate(&a5c, &del, true) == 1 &&
           a5c == 0x11 && !del);
    a5c = 0; del = false;
    assert(aos_npc_talk_sprite_gate(&a5c, &del, false) == 0 &&
           del);
    aos_npc_talk_release_frames();
    aos_npc_talk_arm_fade();
    uint8_t f59 = 0;
    aos_npc_talk_mark_done(&f59);
    assert(f59 & 0x8);
    assert(aos_npc_talk_sound_release() == 0);
    uint8_t cc = 0, cd = 0; bool w417;
    assert(aos_npc_talk_counter_tick(&cc, &cd, 5, &w417) &&
           w417 && cd == 5 && cc == 1);
    assert(aos_npc_talk_counter_tick(&cc, &cd, 0, &w417) &&
           !w417 && cd == 4);
    cd = 0;
    assert(aos_npc_talk_counter_tick(&cc, &cd, 0, &w417) && cc == 2);
    assert(!aos_npc_talk_counter_tick(&cc, &cd, 0, &w417));

    puts("aos_npc_talk: ok");
    return 0;
}

/* appended coverage */
