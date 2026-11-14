/* Tests for the Object20 shop callbacks sub_08061ABC and
 * sub_08061D10 in aos_shop.c. */
#include "../src/runtime/aos/game/aos_shop.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static AosShopInput in0;

int main(void) {
    AosShop s;
    AosShopOut out;
    memset(&s, 0, sizeof s);
    memset(&in0, 0, sizeof in0);

    /* ---- sub_08061ABC (sell/give) ---- */

    /* State 0 arms the sound pair and falls into the state-1
     * gate in the same call. */
    int r = aos_shop_sell_update(&s, &in0, &out);
    assert(s.state == 2 && r == 0);
    assert(out.b878 == 0x1040 && out.sound_2b);
    assert(out.set_42c == 1);

    /* State 2 opens text 0x1F. */
    r = aos_shop_sell_update(&s, &in0, &out);
    assert(s.state == 3 && out.window_text == 0x1F);

    /* State 3 with the window still up and menu 0: only the
     * animation step runs. */
    memset(&in0, 0, sizeof in0);
    in0.text_done = false;
    r = aos_shop_sell_update(&s, &in0, &out);
    assert(s.state == 3 && out.window_poll && out.anim_step);
    assert(!out.transaction && !out.write_417);

    /* Drive the +0xB == 1 purchase: cursor 0 writes the flags
     * and the +0x417/+0x427 markers. */
    s.menu = 1; s.cursor = 0;
    r = aos_shop_sell_update(&s, &in0, &out);
    assert(s.cursor == 1);
    assert(out.andnot_player58 == 0x41 && out.andnot_ent58 == 0x21);
    assert(out.write_417 && out.write_417_val == 1);
    assert(out.write_427 && out.write_427_val == 1);

    /* Cursor 1: transaction with arg2 from A094[6]; probe in
     * the <= 0x11F band arms the 0x28 timer and maybe animates. */
    s.flag6d = 1;
    in0.a094_6 = 0x50;             /* <= 0x70 -> arg2 0x40000 */
    in0.probe = 0x100;
    r = aos_shop_sell_update(&s, &in0, &out);
    assert(out.transaction && out.transaction_arg1 == 0x40000);
    assert(s.cursor == 2 && s.timer == 0x28);
    assert(out.anim_a == 0 && out.anim_b == 1 && out.anim_c == 1);

    /* A094[6] > 0x70 flips arg2 to 0. */
    in0.a094_6 = 0x71;
    r = aos_shop_sell_update(&s, &(AosShopInput){.text_done=false,.a094_6=0x71,.probe=0x100}, &out);
    assert(out.transaction_arg1 == 0);

    /* Middle band: flag6d == 2 suppresses the anim, the slide
     * velocity is armed and pos40 integrates once. */
    s.cursor = 1; s.flag6d = 2; s.vel48 = 0; s.pos40 = 0;
    in0.probe = 0x120;
    r = aos_shop_sell_update(&s, &in0, &out);
    assert(s.vel48 == -0x8000 && s.pos40 == -0x8000);
    assert(out.anim_a == -1);

    /* Far band: only the slide integrates. */
    s.cursor = 1; s.pos40 = 0;
    in0.probe = 0x200;
    r = aos_shop_sell_update(&s, &in0, &out);
    assert(s.pos40 == -0x8000);

    /* Cursor 2 counts the timer down; the wrap advances the
     * menu and clears +0x417. */
    s.cursor = 2; s.menu = 1; s.timer = 1;
    r = aos_shop_sell_update(&s, &in0, &out);
    assert(s.timer == 0 && s.menu == 1 && s.cursor == 2);
    r = aos_shop_sell_update(&s, &in0, &out);
    assert(s.timer == 0xFF && s.menu == 2 && s.cursor == 0);
    assert(out.write_417 && out.write_417_val == 0);

    /* Menu 2 keeps transacting. */
    r = aos_shop_sell_update(&s, &in0, &out);
    assert(out.transaction && out.transaction_arg1 == 0);

    /* Window done: jingle, release held sound, clear +0x42C
     * bit 0x40000, song 0x14, state 0x80. */
    in0.text_done = true;
    r = aos_shop_sell_update(&s, &in0, &out);
    assert(s.state == 0x80);
    assert(out.jingle == 0x29 && out.release_sound_2b);
    assert(out.clear_42c == 0x00040000 && out.song == 0x14);

    /* State 0x80 clears +0x427 and releases the entity. */
    memset(&in0, 0, sizeof in0);
    r = aos_shop_sell_update(&s, &in0, &out);
    assert(out.write_427 && out.write_427_val == 0);
    assert(out.frames_release);

    /* ---- sub_08061D10 (buy/menu) ---- */
    memset(&s, 0, sizeof s);
    memset(&in0, 0, sizeof in0);
    in0.dir = 1;

    /* State 0 opens text 0x20, polls in the same call, and the
     * dir == 1 pad already moves menu 0 to menu 2. */
    r = aos_shop_buy_update(&s, &in0, &out);
    assert(s.state == 1);
    assert(out.window_text == 0x20 && out.write_417 && out.write_417_val == 1);
    assert(out.window_poll && out.side_record_53c);
    assert(s.menu == 2);

    /* Menu 2 keeps dir == 1: entity left of the player gives
     * the 0x20 code, right gives 0x10. */
    in0.ent_left_of_player = true;
    r = aos_shop_buy_update(&s, &in0, &out);
    assert(r == 0x20);
    in0.ent_left_of_player = false;
    r = aos_shop_buy_update(&s, &in0, &out);
    assert(r == 0x10);

    /* Wrong direction drops to menu 4. */
    in0.dir = -1;
    r = aos_shop_buy_update(&s, &in0, &out);
    assert(s.menu == 4);

    /* Menu 3 with dir -1 compares the room position to half
     * the A094[0]-derived limit. */
    s.menu = 3; s.cursor = 0;
    in0.dir = -1; in0.a094_0 = 1; in0.room_q3 = 0x20;
    r = aos_shop_buy_update(&s, &in0, &out);
    /* limit = 0x1E, half = 0xF; room 0x20 > 0xF -> 0x20 */
    assert(r == 0x20 && out.side_record_53c);
    in0.room_q3 = 0x8;
    r = aos_shop_buy_update(&s, &in0, &out);
    assert(r == 0x10);

    /* Menu 4 cursor 0 writes the record byte and, with the
     * player's bit6 set against a clear record side, the
     * conditional player +0x58 sync. */
    s.state = 1; s.menu = 4; s.cursor = 0;
    in0.dir = 0;
    in0.player_flags58 = 0x40;   /* player bit6 set, record side 0 */
    r = aos_shop_buy_update(&s, &in0, &out);
    assert(s.cursor == 1);
    assert(out.side_record_58);
    assert(out.record58_val == 0);        /* (0 & ~0x41) | 0<<6 */
    assert(out.side_player_58);
    assert(out.player58_val == 0);        /* (0x40 & ~0x41) | 0 */
    assert(out.write_417 && out.write_417_val == 0);

    /* When the player bit6 already matches the record side bit
     * the ROM skips the player +0x58 store entirely. */
    s.state = 1; s.menu = 4; s.cursor = 0;
    memset(&in0, 0, sizeof in0);          /* both bit6 clear */
    in0.dir = 0;
    r = aos_shop_buy_update(&s, &in0, &out);
    assert(out.side_record_58 && !out.side_player_58);

    /* Record side bit: record +0x40 left of player +0x40 sets
     * bit6 and preserves the other flag bits. */
    s.state = 1; s.menu = 0; s.cursor = 0;
    memset(&in0, 0, sizeof in0);
    in0.record_pos40 = -0x10000;          /* record left of player */
    in0.player_pos40 = 0;
    in0.record_flags58 = 0x83;            /* bits 0,1,7 set */
    r = aos_shop_buy_update(&s, &in0, &out);
    assert(out.side_record_53c);
    assert(out.record58_val == ((0x83 & ~0x41) | 0x40));

    /* Record right of the player clears bit6 (and bit0). */
    s.state = 1; s.menu = 0; s.cursor = 0;
    in0.record_pos40 = 0x10000;
    in0.player_pos40 = 0;
    r = aos_shop_buy_update(&s, &in0, &out);
    assert(out.record58_val == (0x83 & ~0x41));

    /* sub_0800ED24's +0x427 gate: byte > param0 (1) suppresses
     * the transaction with no side effects. */
    memset(&s, 0, sizeof s);
    memset(&in0, 0, sizeof in0);
    s.state = 3; s.menu = 1; s.cursor = 1;
    in0.byte_427 = 2;
    r = aos_shop_sell_update(&s, &in0, &out);
    assert(!out.transaction);
    s.cursor = 1;                        /* the gated call still
                                             advanced the cursor */
    in0.byte_427 = 1;
    r = aos_shop_sell_update(&s, &in0, &out);
    assert(out.transaction && out.transaction_arg1 == 0x40000);

    /* Input busy in menu 0 does nothing. */
    memset(&s, 0, sizeof s); s.state = 1; s.menu = 0;
    memset(&in0, 0, sizeof in0);
    in0.input_busy = true; in0.dir = 1;
    r = aos_shop_buy_update(&s, &in0, &out);
    assert(s.menu == 0 && r == 0 && !out.side_record_53c);

    /* Text done arms the 0x40 countdown; state 0x7F jingles,
     * sets +0x42C bit 0x40000000 and counts to 0x80. */
    memset(&s, 0, sizeof s); s.state = 1;
    memset(&in0, 0, sizeof in0); in0.text_done = true;
    r = aos_shop_buy_update(&s, &in0, &out);
    assert(s.state == 0x7F && s.timer == 0x40);
    s.timer = 0;                    /* 0 - 1 wraps to 0xFF */
    r = aos_shop_buy_update(&s, &in0, &out);
    assert(s.state == 0x80);
    assert(out.jingle == 0x2A && out.set_42c == 0x40000000);
    r = aos_shop_buy_update(&s, &in0, &out);
    assert(out.release);

    puts("aos_shop: ok");
    return 0;
}
