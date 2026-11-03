/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for Aria's title screen (aos_title_screen). */
#include "aos_title_screen.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "aos_game_mode.h"
#include "aos_input.h"

/* A palette resource: 4-byte header (+ 2: its row count), then the rows;
 * row r, colour i holds r * 32 + i. */
static uint8_t resource[4 + 0x21 * 0x20];

static void make_resource(void) {
    resource[2] = 0x21;
    for (unsigned r = 0; r < 0x21; ++r)
        for (unsigned i = 0; i < 16; ++i) {
            uint16_t c = (uint16_t)(r * 32 + i);
            resource[4 + r * 0x20 + i * 2] = (uint8_t)c;
            resource[5 + r * 0x20 + i * 2] = (uint8_t)(c >> 8);
        }
}

static int run_until_stage(AosTitleScreen *t, uint16_t stage, int limit) {
    int frames = 0;
    while (t->stage != stage && frames < limit) {
        assert(aos_title_screen_update(t, 0, 0) == AOS_MODE_SAME);
        ++frames;
    }
    return frames;
}

int main(void) {
    make_resource();
    static AosTitleGraphics graphics;
    graphics.palette[0] = 0x1234;
    graphics.text[0][10 * AOS_TITLE_LAYER_SIZE + 20] = 0x13;   /* BG0 row 1 colour 3 */
    graphics.palette[0x13] = 0x7C00;
    static AosTitleScreen t;
    aos_title_screen_init(&t, &graphics, resource, resource);

    /* Stage 0, then stage 1 (MUS_27, mode 1 with BG2, OBJ and the OBJ
     * window, the OBJ rows of sub_08004160, the zoomed-in affine scale
     * 0xC8) falling into the 32-frame fade of stage 2. */
    aos_title_screen_update(&t, 0, 0);
    assert(t.stage == 1 && t.bldy == 16 && t.bldcnt == 0xFF);
    aos_title_screen_update(&t, 0, 0);
    assert(t.song == 27 && t.stage == 2 && t.dispcnt == 0x9401 && t.motion == 0x200000);
    assert(t.obj.source[4 * 16] == 3 * 32 && t.obj.source[0xC * 16 + 5] == 32 + 5 &&
           t.obj.source[0xD * 16] == 0 && t.obj.source[0xB * 16] == 2 * 32);
    AosTitleAffine zoom = aos_title_affine(0xC8, 0);
    assert(t.affine.pa == zoom.pa && zoom.pa == 0xC8);
    assert(run_until_stage(&t, 3, 100) == 32 && t.bldy == 0 && t.bldcnt == 0x3F40);
    /* Stage 3: the disc drifts in, 0x2000 a frame, to below 0x30000. */
    assert(run_until_stage(&t, 4, 400) == (0x200000 - 0x2E000) / 0x2000 && (t.e12_flags & 1));
    /* Stage 4 waits for entity 0x12's palette sweep (three times 15 rows),
     * then stage 5's 64 frames. */
    int sweep = run_until_stage(&t, 5, 200);
    assert(sweep == 45 && (t.e12_flags & 2) && t.e12_blend == 2);
    assert(run_until_stage(&t, 6, 100) == 64 && (t.e13_flags & 1));
    /* Stage 6: 0x88 frames of the bats' alpha, then BG1 (the logo). */
    assert(run_until_stage(&t, 7, 200) == 0x89 && (t.dispcnt & 0x200) && t.bldcnt == 0x3F42);
    /* Stage 7: the logo's wave fades out; then the castle (BG0), the menu,
     * the white flash and stage 8. */
    run_until_stage(&t, 8, 800);
    assert(t.stage == 8 && (t.dispcnt & 0x100) && t.menu_shown && t.copyright_shown &&
           t.counter == 0x258 && t.reveal == 0 && t.bg.fx.weight[16] < 0x10000);
    for (int k = 0; k < AOS_TITLE_WAVE_LINES; ++k) assert(t.wave[k] == 0);

    /* No SOUL TRADE: only GAME START, or GAME START and the extra entry. */
    uint8_t entries[3];
    assert(aos_title_screen_entries(&t, entries) == 1 &&
           entries[0] == AOS_TITLE_ENTRY_GAME_START);
    t.unlocked = true;
    assert(aos_title_screen_entries(&t, entries) == 2 && entries[1] == AOS_TITLE_ENTRY_EXTRA);
    aos_title_screen_update(&t, 0, AOS_KEY_DOWN);
    assert(t.cursor == 1);
    /* A on the extra entry slides to the submenu; B slides back. */
    aos_title_screen_update(&t, AOS_KEY_A, 0);
    assert(t.stage == 8 && t.sliding);
    for (int k = 0; k < 20 && t.sliding; ++k) aos_title_screen_update(&t, 0, 0);
    assert(t.first == AOS_TITLE_ENTRY_BOSS_RUSH && aos_title_screen_entries(&t, entries) == 2 &&
           entries[0] == AOS_TITLE_ENTRY_BOSS_RUSH && entries[1] == AOS_TITLE_ENTRY_SOUND_TEST);
    aos_title_screen_update(&t, AOS_KEY_B, 0);
    for (int k = 0; k < 20 && t.sliding; ++k) aos_title_screen_update(&t, 0, 0);
    assert(t.first == 0 && t.cursor == 0);
    t.unlocked = false;

    /* START on GAME START: SONG_MUTE, the 32-frame fade, MAIN_MENU. */
    aos_title_screen_update(&t, AOS_KEY_START, 0);
    assert(t.stage == 9 && t.chosen && t.song == 0x7FFF && t.bldcnt == 0xFF);
    int fade = 0;
    int32_t mode = AOS_MODE_SAME;
    while ((mode = aos_title_screen_update(&t, 0, 0)) == AOS_MODE_SAME && fade < 100) ++fade;
    assert(mode == AOS_MODE_MAIN_MENU && fade == 32);

    /* A new entry starts afresh; START in stages 2-7 reveals at once. */
    t.stage = 0;
    aos_title_screen_update(&t, 0, 0);
    aos_title_screen_update(&t, 0, 0);
    aos_title_screen_update(&t, AOS_KEY_START, 0);
    assert(t.stage == 8 && t.menu_shown && !t.chosen && t.bldalpha == 16 && t.progress == 0x40 &&
           t.e12_x == 0x780000 && (t.dispcnt & 0x300) == 0x300);
    /* Ten idle minutes... ten idle seconds: the introduction. */
    t.counter = 1;
    aos_title_screen_update(&t, 0, 0);
    assert(t.timed_out && t.stage == 9);
    while ((mode = aos_title_screen_update(&t, 0, 0)) == AOS_MODE_SAME) {}
    assert(mode == AOS_MODE_INTRO_CUTSCENE);

    /* The palette effects: a global blend toward a colour from weight 0. */
    AosTitleScreen p;
    aos_title_screen_init(&p, &graphics, resource, resource);
    aos_title_screen_update(&p, 0, 0);
    aos_title_screen_update(&p, 0, 0);
    aos_title_screen_update(&p, AOS_KEY_START, 0);   /* white flash: weight 0 + 1/32 */
    uint16_t c = p.bg.shown[0x13];
    assert(c != 0x7C00 && (c & 0x1F) >= 28);
    for (int k = 0; k < 40; ++k) aos_title_screen_update(&p, 0, 0);
    assert(p.bg.shown[0x13] == 0x7C00 && !(p.bg.fx.running & 0x10000));

    /* The compositor: BG0 over the backdrop; brightness decrease at 16. */
    static uint8_t rgba[AOS_TITLE_W * AOS_TITLE_H * 4];
    aos_title_screen_render(&p, rgba);
    const uint8_t *px = rgba + (10 * AOS_TITLE_W + 20) * 4;
    assert(px[0] == 0 && px[2] == 255);
    p.bldcnt = 0xFF;
    p.bldy = 16;
    aos_title_screen_render(&p, rgba);
    assert(px[0] == 0 && px[1] == 0 && px[2] == 0);

    /* BgAffineSet: unit scale maps (120, 80) to the texture centre. */
    AosTitleAffine unit = aos_title_affine(0x100, 0);
    assert(unit.pa == 0x100 && unit.pb == 0 && unit.pd == 0x100 &&
           ((unit.x + unit.pa * 120) >> 8) == 128 && ((unit.y + unit.pd * 80) >> 8) == 128);
    printf("aos_title_screen: ok\n");
    return 0;
}
