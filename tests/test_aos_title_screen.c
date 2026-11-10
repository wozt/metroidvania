/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for Aria's title screen (aos_title_screen). */
#include "aos/title/aos_title_screen.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aos/game/aos_game_mode.h"
#include "aos/core/aos_input.h"

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
    graphics.palette[0x14] = 0x03E0;   /* BG1 */
    graphics.palette[0x15] = 0x001F;   /* BG2 */
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
    /* sub_08004160 / sub_08003DBC: the disc's four quarters (layer 0x11:
     * priority 2, rows 4-7), the bats and the swooping bat hidden, ten
     * random values a bat from the seed 0x1967; each bat drifts toward
     * (0x78, 0x60). */
    static AosTitleSprite sprites[AOS_TITLE_MAX_SPRITES];
    assert(aos_title_screen_sprites(&t, sprites) == 4);
    assert(sprites[0].sheet == AOS_TITLE_SHEET_DISC && sprites[0].record == 0 &&
           sprites[0].palette_row == 4 && sprites[0].priority == 2 && sprites[0].mode == 0 &&
           sprites[0].x == 0x98 - 32 && sprites[0].y == 0x70 - 32 && !sprites[0].affine &&
           sprites[3].record == 3 && sprites[3].palette_row == 7 && sprites[3].x == 0x98 + 32);
    uint32_t r = 0x1967;
    for (int k = 0; k < AOS_TITLE_BATS * 10; ++k) r = (r >> 8) * 0x3243F6ADu + 0x1B0CB175u;
    assert(t.random == r && t.swoop.active && t.swoop.hidden);
    for (int k = 0; k < AOS_TITLE_BATS; ++k) {
        const AosTitleBat *b = &t.bats[k];
        assert(b->active && b->hidden && b->semi && b->ax >= 0x400 && b->ax <= 0x13F0);
        assert(b->x < 0x780000 ? b->vx >= 0 : b->vx <= 0);
        assert(b->y < 0x600000 ? b->vy >= 0 : b->vy <= 0);
    }
    AosTitleAffine zoom = aos_title_affine(0xC8, 0);
    assert(t.affine.pa == zoom.pa && zoom.pa == 0xC8);
    assert(run_until_stage(&t, 3, 100) == 32 && t.bldy == 0 && t.bldcnt == 0x3F40);
    /* Stage 3: the disc drifts in, 0x2000 a frame, to below 0x30000; at
     * 0x40000 come the two sparkles (sub_08003F08): affine, priority 0,
     * semi-transparent, scaled 1.5 and 2. */
    int to_sparkle = 0;
    while (!t.sparkles[0].active && to_sparkle < 400) {
        aos_title_screen_update(&t, 0, 0);
        ++to_sparkle;
    }
    assert(to_sparkle == (0x200000 - 0x40000) / 0x2000 && t.sparkles[1].active && t.bldy == 16);
    int n = aos_title_screen_sprites(&t, sprites);
    assert(n == 6 && sprites[0].sheet == AOS_TITLE_SHEET_DISC && sprites[0].affine &&
           sprites[0].priority == 0 && sprites[0].mode == 1 && sprites[0].palette_row == 0xB &&
           sprites[0].x == 0x4E && sprites[0].y == 0x26 && abs(sprites[0].pa - 0xAA) <= 1 &&
           abs(sprites[1].pa - 0x80) <= 1 && sprites[2].priority == 2);
    assert(to_sparkle + run_until_stage(&t, 4, 400) == (0x200000 - 0x2E000) / 0x2000 &&
           (t.e12_flags & 1));
    /* Stage 4 waits for entity 0x12's palette sweep (three times 15 rows),
     * then stage 5's 64 frames. */
    int sweep = run_until_stage(&t, 5, 200);
    assert(sweep == 45 && (t.e12_flags & 2) && t.e12_blend == 2);
    /* The sparkles live 87 frames (+ 0x4F8 from 0x8000 by 0x180). */
    assert(t.sparkles[0].active);
    assert(run_until_stage(&t, 6, 100) == 64 && (t.e13_flags & 1));
    assert(!t.sparkles[0].active && !t.sparkles[1].active);
    /* The disc is now an OBJ window; the bats show (layer 7: priority 1),
     * semi-transparent until the end of stage 6. */
    n = aos_title_screen_sprites(&t, sprites);
    int bats = 0;
    for (int k = 0; k < n; ++k)
        if (sprites[k].sheet == AOS_TITLE_SHEET_BATS && sprites[k].priority == 1 &&
            sprites[k].mode == 1 && sprites[k].palette_row == 0xD)
            ++bats;
    /* Layer 7 comes before layer 0x11 in the OAM. */
    assert(bats >= 30 && sprites[n - 1].sheet == AOS_TITLE_SHEET_DISC && sprites[n - 1].mode == 2);
    /* Stage 6: 0x88 frames of the bats' alpha, then BG1 (the logo). */
    assert(run_until_stage(&t, 7, 200) == 0x89 && (t.dispcnt & 0x200) && t.bldcnt == 0x3F42);
    for (int k = 0; k < AOS_TITLE_BATS; ++k) assert(!t.bats[k].semi);
    /* Stage 7: below 0x380 of + 0x504 the swooping bat spirals in to
     * (0x7A, 0x1A), affine on layer 6. */
    while (t.swoop.state == 0 && t.stage == 7) aos_title_screen_update(&t, 0, 0);
    assert(t.stage == 7 && !t.swoop.hidden && t.reveal <= 0x37F && t.swoop.radius < 0xC00000);
    /* It starts above the screen (culled by sub_0803AC40) and comes in. */
    bool swoop = false;
    for (int frame = 0; frame < 60 && !swoop; ++frame) {
        n = aos_title_screen_sprites(&t, sprites);
        for (int k = 0; k < n; ++k)
            swoop |= sprites[k].affine && sprites[k].priority == 0 && sprites[k].palette_row == 0xC;
        if (!swoop) aos_title_screen_update(&t, 0, 0);
    }
    assert(swoop && t.stage == 7);
    while (t.swoop.radius != 0 && t.stage == 7) aos_title_screen_update(&t, 0, 0);
    assert(t.stage == 7 && t.swoop.x == 0x7A0000 && t.swoop.y == 0x1A0000);
    /* Stage 7: the logo's wave fades out; then the castle (BG0), the menu,
     * the white flash and stage 8. */
    run_until_stage(&t, 8, 800);
    assert(t.stage == 8 && (t.dispcnt & 0x100) && t.menu_shown && t.copyright_shown &&
           t.counter == 0x258 && t.reveal == 0 && t.bg.fx.weight[16] < 0x10000);
    for (int k = 0; k < AOS_TITLE_WAVE_LINES; ++k) assert(t.wave[k] == 0);
    /* sub_08000EA8(1) deleted the bats, the swooping bat and the sparkles;
     * the disc is hidden; the menu (row 0x0B selected) and entity 0x15's
     * ring and copyright remain. */
    assert(!t.swoop.active && !t.bats[0].active);
    n = aos_title_screen_sprites(&t, sprites);
    assert(n == 6 && sprites[0].sheet == AOS_TITLE_SHEET_MENU && sprites[0].record == 0 &&
           sprites[0].palette_row == 0xB && sprites[0].x == 120 && sprites[0].y == 0x8C - 4 &&
           sprites[0].priority == 2 && sprites[5].record == 0xB && sprites[5].x == 120 &&
           sprites[5].y == 160 && sprites[1].record == 0xC && sprites[1].x == 120 - 32);

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
    assert(t.stage == 9 && t.chosen && t.song == 0x1010 && t.bldcnt == 0xFF);
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

    /* The compositor: BG0 over the backdrop, BG1 over BG0 (sub_0803FD60
     * gives BG1 priority 0), the menu sprite (row 0x0B: resource row 2). */
    static uint8_t rgba[AOS_TITLE_W * AOS_TITLE_H * 4];
    graphics.text[0][10 * AOS_TITLE_LAYER_SIZE + 30] = 0x13;
    graphics.text[1][10 * AOS_TITLE_LAYER_SIZE + 30] = 0x14;
    memset(graphics.affine, 0x15, sizeof graphics.affine);
    for (int s = 0; s < AOS_TITLE_SHEETS; ++s) {
        AosObjSheet *sheet = &graphics.sheets[s];
        sheet->frame_count = 1;
        sheet->frames[0].count = 1;
        sheet->frames[0].pieces[0] = (AosObjPiece){0, 0, 8, 8, 0, 0, false, false};
        for (int k = 0; k < 64; ++k) sheet->pixels[(k / 8) * AOS_OBJ_SHEET_SIZE + k % 8] = 1;
    }
    aos_title_screen_render(&p, rgba);
    const uint8_t *px = rgba + (10 * AOS_TITLE_W + 20) * 4;
    assert(px[0] == 0 && px[2] == 255);
    assert(rgba[(10 * AOS_TITLE_W + 30) * 4 + 1] == 255 && rgba[(10 * AOS_TITLE_W + 30) * 4 + 2] == 0);
    const uint8_t *menu = rgba + ((0x8C - 4) * AOS_TITLE_W + 121) * 4;
    assert(menu[0] == 8 && menu[1] == 16 && menu[2] == 0);
    assert(rgba[(60 * AOS_TITLE_W + 60) * 4] == 255);   /* BG2 everywhere else */
    /* Entity 0x12's disc as an OBJ window hides BG2 (the backdrop shows);
     * semi-transparent, it blends with BG2 by BLDALPHA. */
    static AosTitleScreen q;
    q = p;
    q.e12_hidden = false;
    q.e12_blend = 2;
    q.e12_x = 80 << 16;
    q.e12_y = 48 << 16;
    aos_title_screen_render(&q, rgba);
    const uint8_t *in = rgba + (18 * AOS_TITLE_W + 50) * 4, *out = rgba + (18 * AOS_TITLE_W + 60) * 4;
    assert(in[0] == 20 * 255 / 31 && out[0] == 255);   /* backdrop 0x1234, BG2 red */
    q.e12_blend = 1;
    q.bldalpha = 8 | 8 << 8;
    aos_title_screen_render(&q, rgba);
    assert(in[0] == 16 * 255 / 31 && in[1] == 1 * 255 / 31 && in[2] == 0);
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
