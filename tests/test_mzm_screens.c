/* SPDX-License-Identifier: GPL-3.0-only */
/* The shared non-gameplay screen routines (menu objects, palette fades) on
 * synthetic ROM data, and the title screen on the exported bank. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "gba_ppu.h"
#include "mzm_rom.h"
#include "mzm_screen.h"
#include "mzm_sfx.h"
#include "mzm_title_screen.h"
#include "mzm_hud.h"
#include "mzm_pause_screen.h"
#include "mzm_file_select.h"
#include "mzm_game_over.h"
#include "mzm_intro.h"
#include "mzm_cutscene.h"
#include "mzm_door_fade.h"
#include "mzm_tourian_escape.h"

static GbaPpu ppu;
static MzmScreen screen;

static void put16(uint8_t *rom, uint32_t at, uint16_t v) { rom[at] = (uint8_t)v; rom[at + 1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *rom, uint32_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) rom[at + (uint32_t)i] = (uint8_t)(v >> (8 * i));
}

static void test_fading(void) {
    uint16_t src[256], dst[256];
    for (int i = 0; i < 256; ++i) src[i] = (uint16_t)(20 | 10 << 5 | 31 << 10);
    memset(dst, 0, sizeof dst);
    /* Only row 1: the fade in at 16/32 halves the channels. */
    mzm_apply_fading_color(MZM_FADING_IN, 16, src, dst, 1 << 1);
    assert(dst[0] == 0 && dst[16] == (10 | 5 << 5 | 15 << 10) && dst[32] == 0);
    mzm_apply_fading_color(MZM_FADING_OUT, 16, src, dst, 0xFFFF);
    assert(dst[0] == (10 | 5 << 5 | 16 << 10));
    mzm_apply_fading_color(MZM_FADING_FLASH, 0, src, dst, 1);
    assert(dst[0] == 0x7FFF);
    mzm_apply_fading_color(MZM_FADING_UNK, 32, src, dst, 1);
    assert(dst[0] == 0x7FFF);
}

/* A synthetic OamArray at 0x08000000: entry 1 has two frames (timers 2
 * and 1) of one 8x8 part and changes frames; entry 2 kills itself after
 * its one frame. */
static void test_menu_oam(void) {
    static uint8_t rom[0x200];
    memset(rom, 0, sizeof rom);
    const uint32_t base = 0x08000000;
    /* OamArray: {FrameData*, preAction} x 3 at 0x00 */
    put32(rom, 1 * 8, base + 0x40);
    rom[1 * 8 + 4] = MZM_OAM_PRE_CHANGE_FRAME;
    put32(rom, 2 * 8, base + 0x80);
    rom[2 * 8 + 4] = MZM_OAM_PRE_KILL_AFTER_END;
    /* FrameData of entry 1 at 0x40: two frames then the terminator. */
    put32(rom, 0x40, base + 0x100);
    rom[0x44] = 2;
    put32(rom, 0x48, base + 0x110);
    rom[0x4C] = 1;
    /* Entry 2 at 0x80: one frame. */
    put32(rom, 0x80, base + 0x100);
    rom[0x84] = 1;
    /* Frame OAM: part count, then attr0, attr1, attr2. */
    put16(rom, 0x100, 1);
    put16(rom, 0x102, 0x0005);              /* y 5 */
    put16(rom, 0x104, 0x01FE);              /* x -2 */
    put16(rom, 0x106, 0x0007);              /* tile 7 */
    put16(rom, 0x110, 1);
    put16(rom, 0x112, 0x00FF);
    put16(rom, 0x114, 0);
    put16(rom, 0x116, 0x0009);
    /* A MenuOamData: y 0x100, x 0x80, oamId 1, priority 2, exists 1,
     * bound to BG 1. */
    put16(rom, 0x180, 0x100);
    put16(rom, 0x182, 0x80);
    rom[0x18A] = 1;
    rom[0x18B] = 2 | 1 << 6;
    rom[0x18C] = 1;
    assert(mzm_rom_open_memory(base, rom, sizeof rom));
    MzmMenuOam o[2];
    o[0] = mzm_menu_oam_from_rom(base + 0x180);
    assert(o[0].y == 0x100 && o[0].x == 0x80 && o[0].oam_id == 1 && o[0].priority == 2 &&
           o[0].exists == 1 && o[0].bound_background == 1 && !o[0].not_drawn);
    o[1] = o[0];
    mzm_menu_oam_set_id(&o[1], 2);
    assert(o[1].exists == 2);
    o[1].bound_background = 4;
    screen.bg_hofs[1] = 16;                 /* 4 pixels */
    screen.bg_vofs[1] = 0;
    screen.next_oam_slot = 0;
    mzm_menu_oam_process(&screen, 2, o, base);
    /* Object 0: 0x100 / 4 + 5 = 69, 0x80 / 4 - 4 - 2 = 26; tile 7 with
     * priority 2. Object 1 (no background): y 69, x 30. */
    assert(screen.next_oam_slot == 2);
    assert(screen.oam[0] == 69 && screen.oam[1] == 26 && screen.oam[2] == (7 | 2 << 10));
    assert(screen.oam[4] == 69 && screen.oam[5] == 30 && o[1].exists == 1);
    /* The first frame lasts two updates, then the second shows. */
    screen.next_oam_slot = 0;
    mzm_menu_oam_process(&screen, 2, o, base);
    assert(o[0].current_animation_frame == 0 && o[1].oam_id == 0 && !o[1].exists);
    screen.next_oam_slot = 0;
    mzm_menu_oam_process(&screen, 2, o, base);
    assert(o[0].current_animation_frame == 1 && screen.oam[2] == (9 | 2 << 10));
    /* After the last frame it loops back to the first. */
    screen.next_oam_slot = 0;
    mzm_menu_oam_process(&screen, 2, o, base);
    assert(o[0].current_animation_frame == 0);
    /* ResetFreeOam hides the rest of the buffer. */
    mzm_reset_free_oam(&screen);
    assert(screen.oam[4] == 0xFF && screen.oam[5] == 0xFF && screen.oam[6] == 0);
    mzm_rom_close();
}

static int count_requests(MzmSfxAction action, uint16_t sound) {
    int n = 0;
    for (int i = 0; i < mzm_sfx_count(); ++i) {
        MzmSfxRequest r = mzm_sfx_get(i);
        n += r.action == action && r.sound == sound;
    }
    return n;
}

/* With the exported bank: the comets, the scrolling down to the ground,
 * the title fading in, then START. */
static void test_title_bank(void) {
    if (!mzm_rom_open("assets/extracted/metroid/rom/rom_bank.bin",
                      "assets/extracted/metroid/rom/rom_symbols.tsv")) {
        printf("mzm_screens: no ROM bank, the title screen checks are skipped\n");
        return;
    }
    static MzmTitleScreen title;
    mzm_sfx_clear();
    mzm_title_enter(&title, false);
    int f;
    for (f = 0; f < 700; ++f) {
        mzm_screen_input(&screen, 0);
        assert(!mzm_title_update(&title, &screen));
    }
    /* The title music starts once faded in, with priority 2. */
    assert(count_requests(MZM_SFX_MUSIC_PLAY, 0x2) == 1 && count_requests(MZM_SFX_MUSIC_PRIORITY, 0) == 1);
    assert(title.oam_timings[2].stage == 2 && screen.bg_vofs[3] == 64 * 12 + 32);
    assert(ppu.io[0] & GBA_DCNT_OBJ && (ppu.io[0] & 0x0F00) != 0);
    assert(screen.bldalpha_l == 16 && screen.bldalpha_h == 0);
    /* The screen shows something besides the backdrop. */
    uint16_t line[GBA_PPU_MAX_W];
    int drawn = 0;
    for (int y = 0; y < GBA_SCREEN_H; y += 8) {
        gba_ppu_render_line(&ppu, y, GBA_SCREEN_W, line);
        for (int x = 0; x < GBA_SCREEN_W; ++x) drawn += (line[x] & 0x7FFF) != 0;
    }
    assert(drawn > GBA_SCREEN_W * 10);
    /* START: the pressing sound, then the fade out ends the screen. */
    mzm_screen_input(&screen, 1 << 3);
    assert(!mzm_title_update(&title, &screen));
    assert(title.sub_mode2 == 1 && count_requests(MZM_SFX_PLAY, 0x213) == 1);
    bool left = false;
    for (f = 0; f < 200 && !left; ++f) {
        mzm_screen_input(&screen, 0);
        left = mzm_title_update(&title, &screen);
    }
    assert(left && title.sub_mode2 == 1);
    for (int i = 0; i < 512; ++i) assert(ppu.palette[i] == 0);
    /* Idle long enough, a demo would start (sub mode 2). */
    mzm_title_enter(&title, true);
    left = false;
    for (f = 0; f < 60 * 18 + 100 && !left; ++f) {
        mzm_screen_input(&screen, 0);
        left = mzm_title_update(&title, &screen);
    }
    assert(left && title.sub_mode2 == 2);
    mzm_rom_close();
}

static bool pause_event(void *context, uint8_t event) {
    (void)context;
    (void)event;
    return false;
}

/* With the exported bank: the pause screen's map, the areas and the world
 * map, then START. */
static void test_pause_bank(void) {
    if (!mzm_rom_open("assets/extracted/metroid/rom/rom_bank.bin",
                      "assets/extracted/metroid/rom/rom_symbols.tsv")) {
        printf("mzm_screens: no ROM bank, the pause screen checks are skipped\n");
        return;
    }
    static GbaPpu hud_ppu;
    static MzmHud hud;
    static MzmMinimapProgress progress;
    static MzmPauseScreen pause;
    mzm_hud_init(&hud, &hud_ppu, &progress, 240);
    uint8_t map_x, map_y;
    assert(mzm_hud_room_map(0, 0, &map_x, &map_y));
    MzmEquipment equipment = {.energy = 99, .max_energy = 99, .missiles = 5, .max_missiles = 5,
                              .items = MZM_ITEM_LONG_BEAM | MZM_ITEM_MORPH_BALL | MZM_ITEM_BOMBS};
    MzmHudFrame frame = {&equipment, 0, false, 0, 0, 13 * 64, 31 * 64, NULL, NULL, NULL, NULL};
    MzmHudRoom room = {0, 0, map_x, map_y, 79, 44};
    mzm_hud_room_entered(&hud, &room, &frame);
    MzmPauseGame game = {&hud, 0, &equipment, pause_event, NULL, MZM_PAUSE_FLAG_PAUSE};
    mzm_pause_enter(&pause);
    mzm_sfx_clear();
    int f;
    for (f = 0; f < 40 && pause.sub_stage != 2; ++f) {
        mzm_screen_input(&screen, 0);
        assert(!mzm_pause_update(&pause, &screen, &game));
    }
    /* The fade in takes 17 frames (+2 color steps). */
    assert(pause.sub_stage == 2 && f >= 16 && f <= 20);
    assert(pause.current_area == 0 && pause.areas_viewable == 1 && pause.areas_viewable_total == 1);
    assert(pause.dispcnt == (GBA_DCNT_BG1 | GBA_DCNT_BG2 | GBA_DCNT_BG3 | GBA_DCNT_OBJ));
    assert(ppu.io[GBA_REG_BG0CNT / 2 + 3] == (3 << 14 | 28 << 8 | 2 << 2 | 3));
    /* Samus's map tile is on the map (BG3 at VRAM 0xE000), her icon there. */
    uint32_t at = 0xE000u + (uint32_t)(hud.minimap_y * 32 + hud.minimap_x) * 2;
    assert((ppu.vram[at] | ppu.vram[at + 1] << 8) == hud.minimap_visited[hud.minimap_y * 32 + hud.minimap_x]);
    assert(pause.samus_icon[0].x == hud.minimap_x * 32 && pause.samus_icon[0].exists);
    /* One viewable area: SELECT changes nothing. */
    mzm_screen_input(&screen, 1 << 2);
    mzm_pause_update(&pause, &screen, &game);
    mzm_screen_input(&screen, 0);
    mzm_pause_update(&pause, &screen, &game);
    assert(pause.current_area == 0 && pause.changing_minimap_stage == 0);
    /* A opens the world map (and its sound); B closes it, START leaves. */
    mzm_screen_input(&screen, 1);
    mzm_pause_update(&pause, &screen, &game);
    assert(pause.on_world_map && count_requests(MZM_SFX_PLAY, 514) == 1);
    mzm_screen_input(&screen, 0);
    mzm_pause_update(&pause, &screen, &game);
    mzm_screen_input(&screen, 1 << 1);
    mzm_pause_update(&pause, &screen, &game);
    assert(!pause.on_world_map && pause.sub_stage == 2);
    mzm_screen_input(&screen, 0);
    mzm_pause_update(&pause, &screen, &game);
    /* R opens the status screen: the map fades, the headers slide in, the
     * rows are revealed and the cursor lands on the first beam. */
    mzm_sfx_clear();
    mzm_screen_input(&screen, 1 << 8);
    mzm_pause_update(&pause, &screen, &game);
    for (f = 0; f < 200 && pause.state != 1; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_pause_update(&pause, &screen, &game);
    }
    assert(pause.state == 1 && count_requests(MZM_SFX_PLAY, 500) == 1);
    assert(pause.type_flags & 0x80 && pause.dispcnt & GBA_DCNT_BG0 && pause.dispcnt & GBA_DCNT_OBJWIN);
    assert(screen.bldalpha_h == 0 && screen.bldalpha_l == 16);
    assert(pause.status.current_slot == 1 && pause.status.beam[0] == 1 && pause.status.misc[0] == 0x40);
    assert(pause.status.bomb[0] == 0xF && pause.status.bomb[1] == 0 && pause.status.missiles[0] == 3);
    assert(pause.status.suit[0] == 0 && pause.misc[8].oam_id == 44 && pause.misc[9].obj_mode == 2);
    for (int i = 0; i < 640; ++i)
        assert((ppu.vram[0xC000 + 2 * i] | ppu.vram[0xC001 + 2 * i] << 8) == pause.status_tilemap[i]);
    /* The long beam's description is written into the text tiles. */
    bool text = false;
    for (f = 0; f < 120 && !text; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_pause_update(&pause, &screen, &game);
        for (int i = 0; i < 0x800 && !text; ++i) text = ppu.vram[0x7800 + i] != 0;
    }
    assert(text && pause.current_equipment == 0);
    for (f = 0; f < 60 && pause.misc[0].oam_id == 6; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_pause_update(&pause, &screen, &game);
    }
    /* DOWN wraps to the missiles, RIGHT jumps to the morph ball. */
    mzm_screen_input(&screen, 1 << 7);
    mzm_pause_update(&pause, &screen, &game);
    assert(pause.status.current_slot == 6 && count_requests(MZM_SFX_PLAY, 502) == 1);
    for (f = 0; f < 60 && pause.misc[0].oam_id == 6; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_pause_update(&pause, &screen, &game);
    }
    mzm_screen_input(&screen, 1 << 4);
    mzm_pause_update(&pause, &screen, &game);
    assert(pause.status.current_slot == 12 && pause.status.previous_left_slot == 6);
    /* B goes back to the map with its blend. */
    mzm_screen_input(&screen, 0);
    mzm_pause_update(&pause, &screen, &game);
    mzm_screen_input(&screen, 1 << 1);
    mzm_pause_update(&pause, &screen, &game);
    for (f = 0; f < 200 && pause.state != 0; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_pause_update(&pause, &screen, &game);
    }
    assert(pause.state == 0 && !(pause.type_flags & 0x80) && count_requests(MZM_SFX_PLAY, 501) == 1);
    assert(screen.bldalpha_h == 6 && screen.bldalpha_l == 10 && !(pause.dispcnt & GBA_DCNT_BG0));
    assert(pause.dispcnt == (GBA_DCNT_BG1 | GBA_DCNT_BG2 | GBA_DCNT_BG3 | GBA_DCNT_OBJ));
    /* L opens easy sleep: its text replaces the equipment names. */
    static uint8_t names[0x2000];
    memcpy(names, ppu.vram + 0x6000, sizeof names);
    mzm_sfx_clear();
    mzm_screen_input(&screen, 0);
    mzm_pause_update(&pause, &screen, &game);
    mzm_screen_input(&screen, 1 << 9);
    mzm_pause_update(&pause, &screen, &game);
    for (f = 0; f < 200 && pause.state != 10; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_pause_update(&pause, &screen, &game);
    }
    assert(pause.state == 10 && pause.stage == 0 && count_requests(MZM_SFX_PLAY, 517) == 1);
    assert(pause.easy_sleep_text_state < 0 && memcmp(names, ppu.vram + 0x6000, sizeof names) != 0);
    assert(pause.misc[1].oam_id == 2 && pause.misc[1].x == 64 * 8 + 16);
    /* LEFT to YES, A: half a second later the screen blanks and sleeps
     * until SELECT, L and R are held together. */
    mzm_screen_input(&screen, 1 << 5);
    mzm_pause_update(&pause, &screen, &game);
    assert(pause.stage == 1 && pause.misc[1].x == 64 * 3 + 16 && count_requests(MZM_SFX_PLAY, 519) == 1);
    mzm_screen_input(&screen, 0);
    mzm_pause_update(&pause, &screen, &game);
    mzm_screen_input(&screen, 1);
    mzm_pause_update(&pause, &screen, &game);
    assert(pause.stage == 2 && count_requests(MZM_SFX_PLAY, 520) == 1);
    for (f = 0; f < 60 && !pause.sleeping; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_pause_update(&pause, &screen, &game);
    }
    assert(pause.sleeping && f > 30 && pause.dispcnt & GBA_DCNT_BLANK);
    mzm_screen_input(&screen, 1 << 9 | 1 << 8);
    mzm_pause_update(&pause, &screen, &game);
    assert(pause.sleeping);
    mzm_screen_input(&screen, 1 << 9 | 1 << 8 | 1 << 2);
    mzm_pause_update(&pause, &screen, &game);
    assert(!pause.sleeping && !(pause.dispcnt & GBA_DCNT_BLANK) && pause.stage == 5);
    mzm_screen_input(&screen, 0);
    mzm_pause_update(&pause, &screen, &game);
    assert(pause.stage == 0 && pause.misc[1].x == 64 * 8 + 16);
    /* B closes it and restores the equipment names. */
    mzm_screen_input(&screen, 1 << 1);
    mzm_pause_update(&pause, &screen, &game);
    for (f = 0; f < 200 && pause.state != 0; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_pause_update(&pause, &screen, &game);
    }
    assert(pause.state == 0 && count_requests(MZM_SFX_PLAY, 518) == 1);
    assert(!memcmp(names, ppu.vram + 0x6000, sizeof names) && screen.bldalpha_h == 6 && screen.bldalpha_l == 10);
    mzm_screen_input(&screen, 0);
    mzm_pause_update(&pause, &screen, &game);
    mzm_screen_input(&screen, 1 << 3);
    bool left = mzm_pause_update(&pause, &screen, &game);
    for (f = 0; f < 40 && !left; ++f) {
        mzm_screen_input(&screen, 0);
        left = mzm_pause_update(&pause, &screen, &game);
    }
    assert(left);
    for (int i = 0; i < 512; ++i) assert(ppu.palette[i] == 0);
    /* The map download: the area's map is marked downloaded, a line reveals
     * it row by row, then A leaves. */
    game.flag = MZM_PAUSE_FLAG_MAP_DOWNLOAD;
    mzm_pause_enter(&pause);
    for (f = 0; f < 40 && pause.sub_stage != 2; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_pause_update(&pause, &screen, &game);
    }
    assert(pause.type_flags == (1 << 1 | 1 << 5) && progress.downloaded_maps == 1 && pause.state == 9);
    assert(pause.dispcnt & GBA_DCNT_BG0 && !pause.arrows[0].exists && pause.overlay[0].oam_id != 0);
    for (f = 0; f < 400 && pause.state != 3; ++f) {
        mzm_screen_input(&screen, 1);
        assert(!mzm_pause_update(&pause, &screen, &game));
    }
    assert(pause.state == 3 && f > 21 * 8 && !pause.misc[2].exists);
    for (int i = 0; i < 1024; ++i)
        assert((ppu.vram[0xE000 + 2 * i] | ppu.vram[0xE001 + 2 * i] << 8) == hud.minimap_visited[i]);
    mzm_screen_input(&screen, 0);
    mzm_pause_update(&pause, &screen, &game);
    mzm_screen_input(&screen, 1);
    left = mzm_pause_update(&pause, &screen, &game);
    for (f = 0; f < 40 && !left; ++f) {
        mzm_screen_input(&screen, 0);
        left = mzm_pause_update(&pause, &screen, &game);
    }
    assert(left);
    mzm_rom_close();
}

static int copies, copied_from, copied_to, erases, erased;
static bool test_copy_file(void *context, int source, int destination) {
    (void)context;
    copies++;
    copied_from = source;
    copied_to = destination;
    return true;
}
static bool test_erase_file(void *context, int file) {
    (void)context;
    erases++;
    erased = file;
    return true;
}

/* One frame with keys pressed, then one released. */
static void press(MzmFileSelect *fs, uint16_t keys) {
    mzm_screen_input(&screen, keys);
    mzm_file_select_update(fs, &screen);
    mzm_screen_input(&screen, 0);
    mzm_file_select_update(fs, &screen);
}

static void run_until_stage(MzmFileSelect *fs, uint8_t stage) {
    for (int f = 0; f < 200 && fs->sub_menu_stage != stage; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_file_select_update(fs, &screen);
    }
    assert(fs->sub_menu_stage == stage);
}

/* The file select screen on the exported bank: file A saved, B and C
 * empty; a new game on B with the difficulty panel, then B back to the
 * title. */
static void test_file_select_bank(void) {
    if (!mzm_rom_open("assets/extracted/metroid/rom/rom_bank.bin",
                      "assets/extracted/metroid/rom/rom_symbols.tsv")) {
        printf("mzm_screens: no ROM bank, the file select checks are skipped\n");
        return;
    }
    static MzmFileSelect fs;
    MzmSaveFileInfo files[3];
    memset(files, 0, sizeof files);
    files[0].exists = true;
    files[0].energy = 99;
    files[0].max_energy = 199;
    files[0].igt_hours = 1;
    files[0].igt_minutes = 23;
    files[0].igt_seconds = 45;
    files[0].difficulty = 1;
    files[1].difficulty = files[2].difficulty = 1;
    mzm_file_select_enter(&fs, files, 0);
    mzm_sfx_clear();
    int f, result = 0;
    for (f = 0; f < 60 && fs.sub_mode1 != 2; ++f) {
        mzm_screen_input(&screen, 0);
        result = mzm_file_select_update(&fs, &screen);
    }
    assert(fs.sub_mode1 == 2 && result == MZM_FILE_SELECT_STAY && count_requests(MZM_SFX_MUSIC_PLAY, 0x9) == 1);
    /* File A and copy, erase and options are enabled. */
    assert(fs.enabled_menus == (1 << 0 | 1 << 3 | 1 << 4 | 1 << 5));
    /* File A's timer 01:23:45 in the menu tilemap (0x1CC + digit), B's
     * --:--:--; its energy tiles are drawn. */
    const uint16_t *menu = (const uint16_t *)(screen.ewram + 0x800);
    assert((menu[0x6E] & 0x3FF) == 0x1CC + 0 && (menu[0x6E + 1] & 0x3FF) == 0x1CC + 1 &&
           (menu[0x6E + 3] & 0x3FF) == 0x1CC + 2 && (menu[0x6E + 7] & 0x3FF) == 0x1CC + 5);
    assert((menu[96 + 0x6E] & 0x3FF) == 0x1CB);
    bool energy = false;
    for (int i = 0; i < 0xE0 && !energy; ++i) energy = ppu.vram[0x20 + i] != 0;
    assert(energy);
    for (int i = 0; i < 0xE0; ++i) assert(ppu.vram[0x20 + 0xE0 + i] == 0);
    /* DOWN to file B, A opens its "Start game" panel and its text. */
    mzm_screen_input(&screen, 1 << 7);
    mzm_file_select_update(&fs, &screen);
    assert(fs.cursor_position == 1 && count_requests(MZM_SFX_PLAY, 0x1F9) == 1);
    mzm_screen_input(&screen, 0);
    mzm_file_select_update(&fs, &screen);
    mzm_screen_input(&screen, 1);
    mzm_file_select_update(&fs, &screen);
    assert(fs.current_sub_menu == 1 && count_requests(MZM_SFX_PLAY, 0x1FB) == 1);
    for (f = 0; f < 200 && fs.sub_menu_stage != 7; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_file_select_update(&fs, &screen);
    }
    assert(fs.sub_menu_stage == 7 && fs.most_recent == 1 && fs.message_queue[0] == 0xFF);
    bool text = false;
    for (int i = 0; i < 0x800 && !text; ++i) text = ppu.vram[0x5800 + i] != 0xFF;
    assert(text);
    /* A: the new file asks the difficulty (normal by default). */
    mzm_screen_input(&screen, 1);
    mzm_file_select_update(&fs, &screen);
    for (f = 0; f < 200 && fs.sub_menu_stage != 30; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_file_select_update(&fs, &screen);
    }
    assert(fs.sub_menu_stage == 30 && fs.cursors.difficulty == 1 && fs.difficulty_message == 18);
    mzm_screen_input(&screen, 1 << 6);
    mzm_file_select_update(&fs, &screen);
    assert(fs.cursors.difficulty == 0);
    mzm_screen_input(&screen, 0);
    mzm_file_select_update(&fs, &screen);
    mzm_screen_input(&screen, 1 << 7);
    mzm_file_select_update(&fs, &screen);
    mzm_screen_input(&screen, 0);
    mzm_file_select_update(&fs, &screen);
    mzm_screen_input(&screen, 1 << 7);
    mzm_file_select_update(&fs, &screen);
    assert(fs.cursors.difficulty == 1);
    mzm_screen_input(&screen, 0);
    mzm_file_select_update(&fs, &screen);
    mzm_sfx_clear();
    mzm_screen_input(&screen, 1);
    mzm_file_select_update(&fs, &screen);
    for (f = 0; f < 300 && result == 0; ++f) {
        mzm_screen_input(&screen, 0);
        result = mzm_file_select_update(&fs, &screen);
    }
    assert(result == MZM_FILE_SELECT_NEW_GAME && fs.most_recent == 1 && fs.cursors.difficulty == 1 &&
           count_requests(MZM_SFX_PLAY, 0x1FF) == 1 && count_requests(MZM_SFX_MUSIC_FADE, 0) == 1);
    /* B on the files goes back to the title after the fade. */
    mzm_file_select_enter(&fs, files, 0);
    for (f = 0; f < 60 && fs.sub_mode1 != 2; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_file_select_update(&fs, &screen);
    }
    mzm_screen_input(&screen, 1 << 1);
    result = mzm_file_select_update(&fs, &screen);
    for (f = 0; f < 60 && result == 0; ++f) {
        mzm_screen_input(&screen, 0);
        result = mzm_file_select_update(&fs, &screen);
    }
    assert(result == MZM_FILE_SELECT_TITLE);
    /* A on the saved file A continues it. */
    mzm_file_select_enter(&fs, files, 0);
    for (f = 0; f < 60 && fs.sub_mode1 != 2; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_file_select_update(&fs, &screen);
    }
    mzm_screen_input(&screen, 1);
    result = mzm_file_select_update(&fs, &screen);
    for (f = 0; f < 200 && fs.sub_menu_stage != 7; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_file_select_update(&fs, &screen);
    }
    mzm_screen_input(&screen, 1);
    result = mzm_file_select_update(&fs, &screen);
    for (f = 0; f < 300 && result == 0; ++f) {
        mzm_screen_input(&screen, 0);
        result = mzm_file_select_update(&fs, &screen);
    }
    assert(result == MZM_FILE_SELECT_CONTINUE && fs.most_recent == 0);
    /* Copy file A to the first empty file (B), then erase B. */
    mzm_file_select_enter(&fs, files, 0);
    fs.copy_file = test_copy_file;
    fs.erase_file_callback = test_erase_file;
    fs.context = NULL;
    for (f = 0; f < 60 && fs.sub_mode1 != 2; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_file_select_update(&fs, &screen);
    }
    for (int i = 0; i < 3; ++i) press(&fs, 1 << 7);
    assert(fs.cursor_position == 3);
    press(&fs, 1);
    assert(fs.current_sub_menu == 2);
    run_until_stage(&fs, 3);
    press(&fs, 1);
    assert(fs.sub_menu_stage == 4 && fs.copy_source == 0 && fs.current_file == 1);
    press(&fs, 1);
    for (f = 0; f < 200 && fs.current_sub_menu != 0; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_file_select_update(&fs, &screen);
    }
    assert(fs.current_sub_menu == 0 && copies == 1 && copied_from == 0 && copied_to == 1);
    assert(fs.files[1].exists && fs.files[1].igt_minutes == 23 && fs.enabled_menus & 1 << 1);
    press(&fs, 1 << 7);
    assert(fs.cursor_position == 4);
    press(&fs, 1);
    assert(fs.current_sub_menu == 3);
    run_until_stage(&fs, 2);
    press(&fs, 1 << 7);
    assert(fs.erase_file == 1);
    press(&fs, 1);
    run_until_stage(&fs, 4);
    assert(fs.cursors.confirm_erase == 1);
    press(&fs, 1 << 5);
    assert(fs.cursors.confirm_erase == 0);
    press(&fs, 1);
    for (f = 0; f < 200 && fs.current_sub_menu != 0; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_file_select_update(&fs, &screen);
    }
    assert(erases == 1 && erased == 1 && !fs.files[1].exists && !(fs.enabled_menus & 1 << 1));
    mzm_rom_close();
}

/* The game over screen on the exported bank: the fade, the music, DOWN to
 * quit, A; then A on "try again". */
static void test_game_over_bank(void) {
    if (!mzm_rom_open("assets/extracted/metroid/rom/rom_bank.bin",
                      "assets/extracted/metroid/rom/rom_symbols.tsv")) {
        printf("mzm_screens: no ROM bank, the game over checks are skipped\n");
        return;
    }
    static MzmGameOver go;
    mzm_game_over_enter(&go, 0);
    mzm_sfx_clear();
    int f, result = 0;
    for (f = 0; f < 60 && go.sub_mode1 != 3; ++f) {
        mzm_screen_input(&screen, 0);
        result = mzm_game_over_update(&go, &screen);
    }
    assert(go.sub_mode1 == 3 && result == 0 && screen.bldy == 6 && count_requests(MZM_SFX_MUSIC_PLAY, 0x53) == 1);
    assert(go.dispcnt & GBA_DCNT_WIN1 && go.win1v == (102 << 8 | 122) && go.oam[0].x == 200 && go.oam[0].y == 448);
    /* The letters' palette breathes. */
    uint16_t letters[6];
    memcpy(letters, &ppu.palette[4], sizeof letters);
    bool changed = false;
    for (f = 0; f < 60 && !changed; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_game_over_update(&go, &screen);
        changed = memcmp(letters, &ppu.palette[4], sizeof letters) != 0;
    }
    assert(changed);
    /* DOWN selects quit (the text palettes swap, the window follows). */
    uint16_t top = (uint16_t)(ppu.vram[0x340] | ppu.vram[0x341] << 8);
    mzm_screen_input(&screen, 1 << 7);
    mzm_game_over_update(&go, &screen);
    uint16_t moved = (uint16_t)(ppu.vram[0x340] | ppu.vram[0x341] << 8);
    assert(go.option_selected == 1 && go.win1v == (126 << 8 | 146) && go.oam[0].y == 544 &&
           (top >> 12) == 2 && (moved >> 12) == 3 && count_requests(MZM_SFX_PLAY, 0x1F9) == 1);
    mzm_screen_input(&screen, 0);
    mzm_game_over_update(&go, &screen);
    mzm_screen_input(&screen, 1);
    mzm_game_over_update(&go, &screen);
    assert(go.sub_mode1 == 4 && count_requests(MZM_SFX_PLAY, 0x1FF) == 1);
    for (f = 0; f < 200 && result == 0; ++f) {
        mzm_screen_input(&screen, 0);
        result = mzm_game_over_update(&go, &screen);
    }
    assert(result == MZM_GAME_OVER_QUIT && screen.bldy == 16 && count_requests(MZM_SFX_MUSIC_FADE, 0) == 1);
    mzm_game_over_enter(&go, 2);
    result = 0;
    for (f = 0; f < 60 && go.sub_mode1 != 3; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_game_over_update(&go, &screen);
    }
    mzm_screen_input(&screen, 1 << 3);
    mzm_game_over_update(&go, &screen);
    for (f = 0; f < 200 && result == 0; ++f) {
        mzm_screen_input(&screen, 0);
        result = mzm_game_over_update(&go, &screen);
    }
    assert(result == MZM_GAME_OVER_CONTINUE);
    mzm_rom_close();
}

/* The intro on the exported bank: the eight stages in order with their
 * sounds, the typed text's letters, the ship's affine parameters, the fuzz
 * palette, the end; then a skip with START. */
static void test_intro_bank(void) {
    if (!mzm_rom_open("assets/extracted/metroid/rom/rom_bank.bin",
                      "assets/extracted/metroid/rom/rom_symbols.tsv")) {
        printf("mzm_screens: no ROM bank, the intro checks are skipped\n");
        return;
    }
    static MzmIntro in;
    mzm_intro_enter(&in);
    mzm_sfx_clear();
    int f, stage_start[9] = {0};
    uint8_t stage = 0;
    bool ended = false, affine = false, double_size = false, letters = false, fuzz = false;
    for (f = 0; f < 3000 && !ended; ++f) {
        mzm_screen_input(&screen, 0);
        ended = mzm_intro_update(&in, &screen);
        if (in.stage != stage) stage_start[stage = in.stage] = f;
        if (in.stage == 1 && screen.next_oam_slot != 0) {
            affine |= screen.oam[3] != 0;
            double_size |= (screen.oam[0] & 0x300) == 0x300;
        }
        if (in.stage == 0 && in.final_character > 0) letters = true;
        if (in.stage == 7 && in.fuzz_vblank && ppu.palette[256 + 1] != 0) fuzz = true;
    }
    assert(ended && in.sub_mode2 == 0 && in.stage == 7);
    assert(letters && affine && double_size && fuzz);
    for (int i = 2; i < 8; ++i) assert(stage_start[i] > stage_start[i - 1]);
    /* The timed stages: 30 + 2, 60 + 20 (twice) and 192 frames, plus the
     * frame resetting their timer; the fuzz ends at 96. */
    assert(stage_start[2] - stage_start[1] == 33 && stage_start[3] - stage_start[2] == 81 &&
           stage_start[5] - stage_start[4] == 81 && stage_start[7] - stage_start[6] == 193 &&
           f - stage_start[7] == 99);
    assert(count_requests(MZM_SFX_PLAY, 0x4) == 1 && count_requests(MZM_SFX_PLAY, 0x39) == 1);
    for (uint16_t sound = 0x227; sound <= 0x22B; ++sound) assert(count_requests(MZM_SFX_PLAY, sound) == 1);
    assert(count_requests(MZM_SFX_PLAY, 0x226) > 40);
    /* START skips it: the sounds and the music fade, gSubGameMode2 1. */
    mzm_intro_enter(&in);
    mzm_sfx_clear();
    ended = false;
    for (f = 0; f < 100; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_intro_update(&in, &screen);
    }
    mzm_screen_input(&screen, 1 << 3);
    mzm_intro_update(&in, &screen);
    for (f = 0; f < 3 && !ended; ++f) {
        mzm_screen_input(&screen, 0);
        ended = mzm_intro_update(&in, &screen);
    }
    assert(ended && in.sub_mode2 == 1 && count_requests(MZM_SFX_FADE_ALL, 0) == 1 &&
           count_requests(MZM_SFX_MUSIC_FADE, 0) == 1);
    mzm_rom_close();
}

/* The intro text cutscene on the exported bank: the text drawn before the
 * fade in, the landing sound, the wait for A, the fade out and the end. */
static void test_cutscene_bank(void) {
    if (!mzm_rom_open("assets/extracted/metroid/rom/rom_bank.bin",
                      "assets/extracted/metroid/rom/rom_symbols.tsv")) {
        printf("mzm_screens: no ROM bank, the cutscene checks are skipped\n");
        return;
    }
    static MzmCutscene c;
    assert(!mzm_cutscene_enter(&c, 3) && mzm_cutscene_enter(&c, MZM_CUTSCENE_INTRO_TEXT));
    mzm_sfx_clear();
    int f;
    for (f = 0; f < 400 && !(c.stage == 4 && c.sub_stage == 1); ++f) {
        mzm_screen_input(&screen, 0);
        assert(!mzm_cutscene_update(&c, &screen));
    }
    /* Six lines of text in the BG0 graphics, faded in, BG0 shown. */
    assert(c.stage == 4 && c.message.message_ended && c.message.line == 5);
    bool drawn = false;
    for (int at = 0xB000; at < 0xB000 + 6 * 0x800; ++at) drawn |= ppu.vram[at] != 0;
    assert(drawn && screen.bldalpha_l == 16 && screen.bldalpha_h == 0 && c.dispcnt == GBA_DCNT_BG0);
    assert(c.bgcnt[0] == (0x14 << 8 | 2 << 2) && c.bg_vofs[0] == (2048 - 2 * 64 - 32) / 4);
    assert(count_requests(MZM_SFX_PLAY, 0x233) == 1);
    /* It waits for A or B. */
    for (f = 0; f < 60; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_cutscene_update(&c, &screen);
    }
    assert(c.stage == 4);
    mzm_screen_input(&screen, 1);
    mzm_cutscene_update(&c, &screen);
    bool ended = false;
    for (f = 0; f < 200 && !ended; ++f) {
        mzm_screen_input(&screen, 0);
        ended = mzm_cutscene_update(&c, &screen);
    }
    assert(ended && c.cutscene == MZM_CUTSCENE_NONE && screen.bldalpha_l == 0 && ppu.palette[15 * 16 + 1] == 0);
    mzm_rom_close();
}

/* GM_TOURIAN_ESCAPE on the exported bank: the three exploding-room views,
 * the native Zebes background, then Samus in and looking around her ship. */
static void test_tourian_escape_bank(void) {
    if (!mzm_rom_open("assets/extracted/metroid/rom/rom_bank.bin",
                      "assets/extracted/metroid/rom/rom_symbols.tsv")) {
        printf("mzm_screens: no ROM bank, the Tourian escape checks are skipped\n");
        return;
    }
    static MzmTourianEscape escape;
    mzm_sfx_clear();
    assert(mzm_tourian_escape_enter(&escape, &screen, true));
    assert(escape.ready && escape.stage == 0 && escape.timer == 0);
    assert(escape.room.area == 4 && escape.room.room == 4 && screen.ppu == &escape.room.ppu);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) ==
           (GBA_DCNT_BG1 | GBA_DCNT_BG2 | GBA_DCNT_BG3 | GBA_DCNT_OBJ));
    assert(count_requests(MZM_SFX_PLAY, MZM_SOUND_TOURIAN_ESCAPE_FIRST_ROOM_EXPLODING) == 0);
    mzm_tourian_escape_update(&escape, &screen);
    assert(count_requests(MZM_SFX_PLAY, MZM_SOUND_TOURIAN_ESCAPE_FIRST_ROOM_EXPLODING) == 1);
    assert(screen.next_oam_slot > 0 && screen.ppu->oam[0] != 0x200);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0HOFS + 4) == 82);

    for (int f = 1; f < 249; ++f) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.planet && escape.timer == 249 && escape.room.area == 5 && escape.room.room == 1);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) == (GBA_DCNT_BG0 | GBA_DCNT_OBJ));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDCNT) == 0x3F00);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDY) == 0);
    assert(escape.explosion_timer == 2);
    assert(screen.ppu->oam[0] != 0x200);
    assert(count_requests(MZM_SFX_PLAY, MZM_SOUND_TOURIAN_ESCAPE_OTHER_ROOMS_EXPLODING) == 2);
    assert(count_requests(MZM_SFX_PLAY, MZM_SOUND_TOURIAN_ESCAPE_SAMUS_LEAVING_PLANET) == 1);
    assert(count_requests(MZM_SFX_MUSIC_FADE_AND_QUEUE, MZM_MUSIC_ESCAPING_ZEBES_CUTSCENE) == 1);

    uint16_t line[GBA_SCREEN_W];
    int drawn = 0;
    for (int y = 0; y < GBA_SCREEN_H; y += 8) {
        gba_ppu_render_line(screen.ppu, y, GBA_SCREEN_W, line);
        for (int x = 0; x < GBA_SCREEN_W; ++x) drawn += (line[x] & 0x7FFF) != 0;
    }
    assert(drawn > GBA_SCREEN_W);
    for (int f = 0; f < 300 && escape.stage == 0; ++f)
        mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 1 && escape.timer == 0 && screen.ppu->oam[0] == 0x200);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0HOFS) == 0);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0HOFS + 4) == 0);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0VOFS + 4) == 0);

    /* Samus in her ship: suited image, expanding flash window, suitless
     * cross-fade, opened eyes, then the exact stage boundary. */
    for (int f = 0; f < 4; ++f) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.timer == 4 && gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) ==
           (GBA_DCNT_BG1 | GBA_DCNT_OBJ));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0CNT) == (2 << 2 | 30 << 8));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0CNT + 2) == (1 | 14 << 8));
    assert(screen.ppu->palette[17] ==
           mzm_rom_u16(mzm_sym("sTourianEscapeSamusInHerShipVariaSuitPal") + 34));
    drawn = 0;
    for (int y = 0; y < GBA_SCREEN_H; y += 8) {
        gba_ppu_render_line(screen.ppu, y, GBA_SCREEN_W, line);
        for (int x = 0; x < GBA_SCREEN_W; ++x) drawn += (line[x] & 0x7FFF) != 0;
    }
    assert(drawn > GBA_SCREEN_W);

    while (escape.timer < 57) mzm_tourian_escape_update(&escape, &screen);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) ==
           (GBA_DCNT_BG1 | GBA_DCNT_OBJ | GBA_DCNT_WIN0));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_WIN0H) == (119 << 8 | 121));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_WIN0V) == (76 << 8 | 84));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDY) == 10);
    assert(count_requests(MZM_SFX_PLAY, MZM_SOUND_TOURIAN_ESCAPE_SAMUS_REMOVING_SUIT_1) == 1);
    while (escape.timer < 129) mzm_tourian_escape_update(&escape, &screen);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDY) == 11);
    while (escape.timer < 163) mzm_tourian_escape_update(&escape, &screen);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) ==
           (GBA_DCNT_BG0 | GBA_DCNT_BG1 | GBA_DCNT_OBJ));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDY) == 15);
    assert(count_requests(MZM_SFX_PLAY, MZM_SOUND_TOURIAN_ESCAPE_SAMUS_REMOVING_SUIT_2) == 1);
    while (escape.timer < 201) mzm_tourian_escape_update(&escape, &screen);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDCNT) ==
           (1 | 1 << 6 | 1 << 9));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDALPHA) == 16);
    assert(count_requests(MZM_SFX_PLAY, MZM_SOUND_TOURIAN_ESCAPE_SAMUS_REMOVING_SUIT_3) == 1);
    while (escape.timer < 204) mzm_tourian_escape_update(&escape, &screen);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDALPHA) == (1 << 8 | 15));
    while (escape.timer < 353) mzm_tourian_escape_update(&escape, &screen);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0CNT + 2) == 15 << 8);
    while (escape.stage == 1) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 2 && escape.timer == 0);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) == 0);

    /* Samus looking around: both alpha-blended objects converge on the
     * screen centre while the cockpit switches from its left to right view. */
    for (int f = 0; f < 4; ++f) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.timer == 4);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) ==
           (GBA_DCNT_BG0 | GBA_DCNT_OBJ));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0CNT) == 16 << 8);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDCNT) ==
           (1 << 4 | 1 << 6 | 1 << 8));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDALPHA) == (8 << 8 | 8));
    assert(escape.object_active[0] && escape.object_active[1]);
    assert(escape.object_x[0] == 128 && escape.object_x[1] == 128);
    assert(escape.object_y[0] == 0 && escape.object_y[1] == 160);
    assert(screen.next_oam_slot == 8 && screen.ppu->oam[0] != 0x200);
    assert(screen.ppu->palette[17] ==
           mzm_rom_u16(mzm_sym("sTourianEscapeSamusLookingAroundPal") + 34));
    drawn = 0;
    for (int y = 0; y < GBA_SCREEN_H; y += 8) {
        gba_ppu_render_line(screen.ppu, y, GBA_SCREEN_W, line);
        for (int x = 0; x < GBA_SCREEN_W; ++x) drawn += (line[x] & 0x7FFF) != 0;
    }
    assert(drawn > GBA_SCREEN_W);
    while (escape.timer < 17) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_y[0] == 8 && escape.object_y[1] == 152);
    while (escape.timer < 26) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_y[0] == 80 && escape.object_y[1] == 80);
    while (escape.timer < 33) mzm_tourian_escape_update(&escape, &screen);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0CNT) == 17 << 8);
    while (escape.stage == 2) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 3 && escape.timer == 0);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) == 0);
    assert(screen.ppu->oam[0] == 0x200);

    /* Samus surrounded: BG2 starts magnified over the fixed cockpit and
     * zooms out around the native screen centre from frames 32 through 47. */
    assert(escape.bg2_scale == 1536);
    for (int f = 0; f < 5; ++f) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.timer == 5 && escape.bg2_scale == 1536);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) ==
           (1 | GBA_DCNT_BG0 | GBA_DCNT_BG2 | GBA_DCNT_OBJ));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0CNT) == (1 | 14 << 8));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0CNT + 2) == (2 | 14 << 8));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0CNT + 4) ==
           (2 << 2 | 15 << 8 | 1 << 7 | 1 << 14));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG2PA) == 42);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG2PB) == 0);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG2PC) == 0);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG2PD) == 42);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG2X) == 25680);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG2Y) == 17120);
    assert(screen.ppu->palette[255] ==
           mzm_rom_u16(mzm_sym("sTourianEscapeSamusSurroundedPal") + 510));
    drawn = 0;
    for (int y = 0; y < GBA_SCREEN_H; y += 8) {
        gba_ppu_render_line(screen.ppu, y, GBA_SCREEN_W, line);
        for (int x = 0; x < GBA_SCREEN_W; ++x) drawn += (line[x] & 0x7FFF) != 0;
    }
    assert(drawn > GBA_SCREEN_W);
    while (escape.timer < 33) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.bg2_scale == 1456);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG2PA) == 45);
    while (escape.timer < 48) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.bg2_scale == 256);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG2PA) == 256);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG2X) == 0);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG2Y) == 0);
    while (escape.stage == 3) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 4 && escape.timer == 0);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) == 0);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDCNT) == 0x3F00);

    /* Samus flying in: the intro ship grows through a double-size affine
     * object while its two native OAM frames alternate over the starfield. */
    mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 4 && escape.timer == 1);
    assert(count_requests(MZM_SFX_PLAY,
                          MZM_SOUND_TOURIAN_ESCAPE_SAMUS_FLYING_IN) == 1);
    mzm_tourian_escape_update(&escape, &screen);
    mzm_tourian_escape_update(&escape, &screen);
    assert(escape.timer == 3 && escape.object_active[0]);
    assert(escape.object_x[0] == 120 && escape.object_y[0] == 36);
    assert(escape.object_scaling == 40 && escape.object_timer[0] == 1);
    assert(escape.object_frame[0] == 0 && screen.next_oam_slot == 6);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) ==
           (GBA_DCNT_BG0 | GBA_DCNT_OBJ));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0CNT) == 30 << 8);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDALPHA) == (7 << 8 | 9));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDCNT) == 0x3F00);
    assert((screen.ppu->oam[0] & 3 << 8) == 3 << 8);
    assert(screen.ppu->oam[3] == 1638);
    assert(screen.ppu->palette[96] ==
           mzm_rom_u16(mzm_sym("sIntroTextAndShipPal") + 192));
    assert(screen.ppu->palette[256 + 96] == screen.ppu->palette[96]);
    drawn = 0;
    for (int y = 0; y < GBA_SCREEN_H; y += 8) {
        gba_ppu_render_line(screen.ppu, y, GBA_SCREEN_W, line);
        for (int x = 0; x < GBA_SCREEN_W; ++x) drawn += (line[x] & 0x7FFF) != 0;
    }
    assert(drawn > GBA_SCREEN_W);
    while (escape.timer < 7) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_y[0] == 66 && escape.object_scaling == 72);
    assert(escape.object_frame[0] == 1 && screen.next_oam_slot == 9);
    while (escape.timer < 14) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_y[0] == 96 && escape.object_scaling == 128);
    while (escape.timer < 26) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_y[0] == 108 && escape.object_scaling == 320);
    while (escape.timer < 31) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_y[0] == 104 && escape.object_scaling == 480);
    assert(escape.object_frame[0] == 1 && screen.next_oam_slot == 9);
    while (escape.stage == 4) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 5 && escape.timer == 0);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) == 0);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDCNT) == 0);
    assert(screen.ppu->oam[0] == 0x200);

    /* Samus chased by pirates: six alternating particles cross the scrolling
     * starfield, then Samus and three pirate ships traverse it in both
     * directions with their native poses, timings and sound cues. */
    for (int f = 0; f < 4; ++f) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 5 && escape.timer == 4);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) ==
           (GBA_DCNT_BG0 | GBA_DCNT_OBJ));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0CNT) == 28 << 8);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0HOFS) == 0);
    assert(screen.next_oam_slot == 6);
    assert(!escape.object_active[0] && escape.object_active[4] == 1 &&
           escape.object_active[5] == 2 && escape.object_active[9] == 2);
    assert(escape.object_x[4] == 18 && escape.object_y[4] == 40);
    assert(escape.object_x[5] == 210 && escape.object_y[5] == 136);
    assert(escape.object_x[6] == 140 && escape.object_y[6] == 88);
    assert(escape.object_x[9] == 28 && escape.object_y[9] == 152);
    assert(screen.ppu->palette[79] == mzm_rom_u16(
           mzm_sym("sTourianEscapeSamusChasedBackgroundPal") + 158));
    assert(screen.ppu->palette[256 + 95] == mzm_rom_u16(
           mzm_sym("sTourianEscapeSamusChasedShipsPal") + 190));
    drawn = 0;
    for (int y = 0; y < GBA_SCREEN_H; y += 8) {
        gba_ppu_render_line(screen.ppu, y, GBA_SCREEN_W, line);
        for (int x = 0; x < GBA_SCREEN_W; ++x) drawn += (line[x] & 0x7FFF) != 0;
    }
    assert(drawn > GBA_SCREEN_W);
    while (escape.timer < 8) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.bg0_x == -1 &&
           gba_ppu_read16(screen.ppu, GBA_REG_BG0HOFS) == 511);
    assert(escape.object_timer[4] == 6 && escape.object_x[4] == 22);
    assert(escape.object_oam_address[4] ==
           mzm_rom_u32(mzm_sym("sTourianEscape_47ce00") + 4));

    while (escape.timer < 41) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_active[0] == 1 && escape.object_x[0] == 214 &&
           escape.object_y[0] == 226 && screen.next_oam_slot == 11);
    assert(count_requests(MZM_SFX_PLAY,
                          MZM_SOUND_TOURIAN_ESCAPE_SAMUS_FLEEING_RIGHT_TO_LEFT) == 1);
    while (escape.timer < 51) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_active[5] == 1 && escape.object_frame[5] == 7);
    assert(escape.object_x[5] == 80 && escape.object_y[5] == 72);
    assert(escape.object_oam_address[5] ==
           mzm_rom_u32(mzm_sym("sTourianEscape_47ce10")));
    while (escape.timer < 65) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_x[0] == 70 && escape.object_y[0] == 370);
    assert(escape.object_active[1] == 1 && escape.object_x[1] == 250 &&
           escape.object_y[1] == 246);
    assert(escape.object_x[2] == 174 && escape.object_y[2] == 246);
    assert(escape.object_x[3] == 206 && escape.object_y[3] == 196);
    assert(screen.next_oam_slot == 26);
    assert(count_requests(MZM_SFX_PLAY,
                          MZM_SOUND_TOURIAN_ESCAPE_SAMUS_FLEEING_LEFT_TO_RIGHT) == 1);
    while (escape.timer < 73) mzm_tourian_escape_update(&escape, &screen);
    assert(!escape.object_active[0] && escape.object_x[0] == 464 &&
           escape.object_y[0] == 224);
    assert(escape.object_x[1] == 202 && escape.object_y[1] == 294);
    while (escape.timer < 117) mzm_tourian_escape_update(&escape, &screen);
    assert(!escape.object_active[1] && escape.object_frame[1] == 1 &&
           escape.object_x[1] == 432 && escape.object_y[1] == 224);
    assert(!escape.object_active[2] && escape.object_frame[2] == 1 &&
           escape.object_x[2] == 496 && escape.object_y[2] == 224);
    while (escape.timer < 120) mzm_tourian_escape_update(&escape, &screen);
    assert(!escape.object_active[3] && escape.object_frame[3] == 1 &&
           escape.object_x[3] == 432 && escape.object_y[3] == 224);

    while (escape.timer < 161) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_active[0] == 2 && escape.object_x[0] == 478 &&
           escape.object_y[0] == 210 && escape.object_timer[0] == 1);
    assert(screen.next_oam_slot == 12 &&
           (screen.ppu->oam[2] & 0x3FF) == 0x380);
    assert(count_requests(MZM_SFX_PLAY,
                          MZM_SOUND_TOURIAN_ESCAPE_PIRATES_CHASING_RIGHT_TO_LEFT) == 1);
    while (escape.timer < 165) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_timer[0] == 5 &&
           (screen.ppu->oam[2] & 0x3FF) == 0x386);
    while (escape.timer < 177) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_active[1] == 2 && escape.object_x[1] == 446 &&
           escape.object_y[1] == 210);
    assert(count_requests(MZM_SFX_PLAY,
                          MZM_SOUND_TOURIAN_ESCAPE_PIRATES_CHASING_LEFT_TO_RIGHT) == 1);
    mzm_tourian_escape_update(&escape, &screen);
    assert(!escape.object_active[0] && escape.object_x[1] == 460 &&
           escape.object_y[1] == 196);
    while (escape.timer < 185) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_active[2] == 2 && escape.object_x[2] == 538 &&
           escape.object_y[2] == 182);
    assert(escape.object_active[3] == 2 && escape.object_x[3] == 446 &&
           escape.object_y[3] == 210);
    while (escape.timer < 201) mzm_tourian_escape_update(&escape, &screen);
    assert(!escape.object_active[1] && !escape.object_active[2] &&
           !escape.object_active[3] && screen.next_oam_slot == 6);
    while (escape.stage == 5) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 6 && escape.timer == 0);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) == 0);
    assert(screen.ppu->oam[0] == 0x200);
    assert(escape.object_frame[4] == 1 && escape.object_x[4] == 142 &&
           escape.object_y[4] == 80);
    assert(escape.object_frame[5] == 8 && escape.object_x[5] == 90 &&
           escape.object_y[5] == 16);
    assert(escape.object_frame[9] == 13 && escape.object_x[9] == 121 &&
           escape.object_y[9] == 112);

    /* Pirates firing: Samus follows the eight native acceleration paths while
     * both affine formations animate, shrink and leave the scrolling shot. */
    for (int f = 0; f < 3; ++f) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 6 && escape.timer == 3);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) ==
           (GBA_DCNT_BG0 | GBA_DCNT_OBJ));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0HOFS) == 48);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0VOFS) == 80);
    assert(escape.object_active[0] == 1 && escape.object_x[0] == 108 &&
           escape.object_y[0] == 90 && escape.object_scaling == 193);
    assert(escape.object_active[1] == 1 && escape.secondary_scaling == 254);
    assert(escape.object_velocity[2] == -70 &&
           escape.firing_vertical_velocity == -21);
    assert(escape.object_frame[2] == 0 && escape.object_frame[3] == 0);
    assert(escape.object_y[4] == 78 && escape.object_y[5] == 12 &&
           escape.object_y[9] == 110);
    assert(screen.next_oam_slot == 14);
    assert((screen.ppu->oam[0] & 3 << 8) == 3 << 8);
    assert((screen.ppu->oam[4 * 4] & 3 << 8) == 3 << 8);
    assert((screen.ppu->oam[4 * 4 + 2] & 0x3FF) == 0x100);
    assert(screen.ppu->oam[3] == 339);
    assert(screen.ppu->oam[16 * 4 + 3] == 258);
    assert(screen.ppu->palette[256 + 63] == mzm_rom_u16(
           mzm_sym("sTourianEscapeSamusChasedByPiratesFiringPal") + 126));
    assert(count_requests(MZM_SFX_PLAY,
                          MZM_SOUND_TOURIAN_ESCAPE_SAMUS_DODGING_SHOTS) == 1);
    assert(count_requests(MZM_SFX_PLAY,
                          MZM_SOUND_TOURIAN_ESCAPE_PIRATES_FIRING) == 1);
    drawn = 0;
    for (int y = 0; y < GBA_SCREEN_H; y += 8) {
        gba_ppu_render_line(screen.ppu, y, GBA_SCREEN_W, line);
        for (int x = 0; x < GBA_SCREEN_W; ++x) drawn += (line[x] & 0x7FFF) != 0;
    }
    assert(drawn > GBA_SCREEN_W);
    while (escape.timer < 8) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.bg0_y == 79 &&
           gba_ppu_read16(screen.ppu, GBA_REG_BG0VOFS) == 79);
    assert(escape.object_x[0] == 81 && escape.object_y[0] == 80 &&
           escape.object_scaling == 195);
    assert(escape.object_oam_address[0] ==
           mzm_rom_u32(mzm_sym("sTourianEscape_47cf70") + 4));
    while (escape.timer < 11) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_active[5] == 1 && escape.object_frame[5] == 9 &&
           escape.object_x[5] == 144 && escape.object_y[5] == 96);
    assert(escape.object_oam_address[5] ==
           mzm_rom_u32(mzm_sym("sTourianEscape_47ce10") + 8));

    while (escape.timer < 40) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_x[0] == 130 && escape.object_y[0] == 88 &&
           escape.object_scaling == 211);
    assert(escape.object_frame[2] == 2 && escape.object_frame[3] == 1);
    assert(escape.object_velocity[2] == 30 &&
           escape.firing_vertical_velocity == 3);
    while (escape.timer < 80) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_x[0] == 77 && escape.object_y[0] == 114 &&
           escape.object_scaling == 231);
    assert(escape.object_frame[2] == 1 && escape.object_frame[3] == 2 &&
           escape.object_timer[2] == 4);
    assert(escape.secondary_scaling == 177);
    while (escape.timer < 121) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_active[1] == 2 && escape.secondary_scaling == 135);
    assert(escape.object_x[0] == 120 && escape.object_y[0] == 98 &&
           escape.object_scaling == 252);
    while (escape.timer < 127) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_active[0] == 2 && escape.object_scaling == 254);
    assert(escape.object_x[0] == 120 && escape.object_y[0] == 98);
    while (escape.timer < 161) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_active[0] == 3 && escape.object_y[0] == 102 &&
           escape.object_scaling == 262 && escape.object_timer[0] == 1);
    while (escape.timer < 165) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_y[0] == 116 && escape.object_scaling == 294);
    assert(escape.object_oam_address[0] ==
           mzm_rom_u32(mzm_sym("sTourianEscape_47cf70") + 4));
    while (escape.timer < 185) mzm_tourian_escape_update(&escape, &screen);
    assert(!escape.object_active[1] && escape.secondary_scaling == 7);
    assert(escape.object_y[0] == 139 && escape.object_scaling == 454);
    assert(screen.next_oam_slot == 10);
    while (escape.timer < 193) mzm_tourian_escape_update(&escape, &screen);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) == 0);
    assert(escape.object_y[0] == 147 && escape.object_scaling == 510);
    while (escape.timer < 369) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_y[0] == 323 && escape.bg0_y == 34);
    assert(count_requests(MZM_SFX_PLAY,
                          MZM_SOUND_TOURIAN_ESCAPE_SAMUS_FLYING_AFTER_SHOTS) == 1);
    while (escape.stage == 6) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 7 && escape.timer == 0);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) == 0);
    assert(screen.ppu->oam[0] == 0x200);
    assert(escape.object_frame[4] == 5 && escape.object_x[4] == 64 &&
           escape.object_y[4] == 0 && escape.object_velocity[4] == 2);
    assert(escape.object_frame[5] == 14 && escape.object_x[5] == 136 &&
           escape.object_y[5] == 72 && escape.object_velocity[5] == 2);
    assert(escape.object_frame[9] == 3 && escape.object_x[9] == 168 &&
           escape.object_y[9] == 60 && escape.object_velocity[9] == 1);

    /* Getting shot: the inherited shots cross the darkened starfield while
     * Samus decelerates, oscillates after the hit and disappears on the
     * decisive shot before the crash scene boundary. */
    for (int f = 0; f < 4; ++f) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 7 && escape.timer == 4);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) ==
           (GBA_DCNT_BG0 | GBA_DCNT_OBJ));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0CNT) == 28 << 8);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0HOFS) == 96);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0VOFS) == 48);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDCNT) == 0xFF);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDY) == 16);
    assert(escape.effect_phase == 1 && screen.next_oam_slot == 6);
    assert(escape.object_active[4] == 1 && escape.object_active[5] == 2 &&
           escape.object_active[6] == 2 && escape.object_active[7] == 1 &&
           escape.object_active[8] == 2 && escape.object_active[9] == 1);
    assert(escape.object_x[4] == 66 && escape.object_x[5] == 138 &&
           escape.object_x[9] == 169);
    assert(screen.ppu->palette[256 + 79] == mzm_rom_u16(
           mzm_sym("sTourianEscapeSamusGettingShotPal") + 158));
    drawn = 0;
    for (int at = 0x8000; at < 0xF800; ++at) drawn += screen.ppu->vram[at] != 0;
    assert(drawn > GBA_SCREEN_W);

    while (escape.timer < 8) mzm_tourian_escape_update(&escape, &screen);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDY) == 15 &&
           escape.bg0_x == 95);
    while (escape.timer < 25) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_active[0] == 1 && escape.object_x[0] == 299 &&
           escape.object_y[0] == 173);
    assert(escape.object_x[1] == 9576 && escape.object_x[2] == 5540 &&
           escape.object_velocity[1] == 56 && escape.object_velocity[2] == 28);
    assert(screen.next_oam_slot == 9 &&
           (screen.ppu->oam[2] & 0x3FF) == 0);
    while (escape.timer < 64) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_x[0] == 231 && escape.object_y[0] == 139 &&
           gba_ppu_read16(screen.ppu, GBA_REG_BLDY) == 8 && escape.bg0_x == 88);
    drawn = 0;
    for (int y = 0; y < GBA_SCREEN_H; y += 8) {
        gba_ppu_render_line(screen.ppu, y, GBA_SCREEN_W, line);
        for (int x = 0; x < GBA_SCREEN_W; ++x) drawn += (line[x] & 0x7FFF) != 0;
    }
    assert(drawn > GBA_SCREEN_W);
    while (escape.timer < 112) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_x[0] == 147 && escape.object_y[0] == 97 &&
           escape.object_x[1] == 4705 && escape.object_x[2] == 3104);
    assert(escape.object_velocity[1] == 55 && escape.object_velocity[2] == 28 &&
           gba_ppu_read16(screen.ppu, GBA_REG_BLDY) == 2);
    while (escape.timer < 128) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_x[0] == 123 && escape.object_y[0] == 85 &&
           escape.object_velocity[1] == 39 && escape.object_velocity[2] == 20);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDY) == 0 &&
           escape.effect_phase == 1 && escape.bg0_x == 80);
    mzm_tourian_escape_update(&escape, &screen);
    assert(escape.timer == 129 && escape.object_active[0] == 2 &&
           escape.object_velocity[1] == 0 && escape.object_velocity[2] == 0);
    assert(!escape.effect_phase && gba_ppu_read16(screen.ppu, GBA_REG_BLDCNT) == 0);
    while (escape.timer < 137) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_active[0] == 3 && escape.object_x[0] == 123 &&
           escape.object_y[0] == 85);
    assert(escape.object_x[1] == 3960 && escape.object_x[2] == 2727 &&
           escape.object_velocity[1] == -1 && escape.object_velocity[2] == -1);
    while (escape.timer < 176) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_x[0] == 107 && escape.object_y[0] == 77 &&
           escape.object_velocity[1] == -16 && escape.object_velocity[2] == 0);
    assert(escape.object_frame[1] == 1 && escape.object_frame[2] == 2 &&
           escape.object_timer[4] == 13);
    mzm_tourian_escape_update(&escape, &screen);
    assert(escape.timer == 177 && escape.object_x[0] == 107 &&
           escape.object_y[0] == 77 && escape.object_timer[4] == 0);
    assert(escape.object_frame[1] == 1 && escape.object_frame[2] == 3);
    while (escape.timer < 237) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_x[0] == 120 && escape.object_y[0] == 81 &&
           escape.object_velocity[1] == 40 && escape.object_velocity[2] == -19);
    assert(escape.object_frame[1] == 4 && escape.object_frame[2] == 0);
    while (escape.timer < 320) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_x[0] == 120 && escape.object_y[0] == 81 &&
           escape.object_velocity[1] == -18 && escape.object_velocity[2] == -20);
    assert(escape.object_frame[1] == 1 && escape.object_frame[2] == 1);
    while (escape.timer < 337) mzm_tourian_escape_update(&escape, &screen);
    assert(!escape.object_active[0] && screen.next_oam_slot == 6);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0CNT + 2) ==
           (1 | 2 << 2 | 30 << 8));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) ==
           (GBA_DCNT_BG1 | GBA_DCNT_OBJ));
    assert(escape.bg0_x == 54 && screen.ppu->palette[79] == mzm_rom_u16(
           mzm_sym("sTourianEscapeSamusGettingShotPal") + 158));
    assert(count_requests(MZM_SFX_PLAY,
                          MZM_SOUND_TOURIAN_ESCAPE_DECISIVE_SHOT) == 1);
    drawn = 0;
    for (int y = 0; y < GBA_SCREEN_H; y += 8) {
        gba_ppu_render_line(screen.ppu, y, GBA_SCREEN_W, line);
        for (int x = 0; x < GBA_SCREEN_W; ++x) drawn += (line[x] & 0x7FFF) != 0;
    }
    assert(drawn > GBA_SCREEN_W);
    while (escape.timer < 400) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 7 && escape.bg0_x == 46 &&
           gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) ==
           (GBA_DCNT_BG1 | GBA_DCNT_OBJ));
    mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 8 && escape.timer == 0);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) == 0);
    assert(screen.next_oam_slot == 0 && screen.ppu->oam[0] == 0x200);

    /* Going to crash: the damaged ship and its translucent wake flicker over
     * Zebes while the two smoke trails follow their ROM-authored offsets. */
    for (int f = 0; f < 3; ++f) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 8 && escape.timer == 3);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) == 0);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDALPHA) == (7 << 8 | 9));
    assert(escape.bg0_x == 16 && escape.bg0_y == 0);
    assert(escape.object_active[0] == 1 && escape.object_active[3] == 1);
    assert(escape.object_x[0] == 108 && escape.object_y[0] == 71 &&
           escape.object_x[3] == 136 && escape.object_y[3] == 88);
    assert(screen.next_oam_slot == 10);
    assert((screen.ppu->oam[0] & 3 << 10) == 1 << 10 &&
           (screen.ppu->oam[2 * 4] & 3 << 10) == 1 << 10);
    assert(screen.ppu->palette[143] == mzm_rom_u16(
           mzm_sym("sTourianEscapeExplodingPal") + 286));
    assert(screen.ppu->palette[256 + 111] == mzm_rom_u16(
           mzm_sym("sTourianEscapeSamusGoingToCrashPal") + 222));
    assert(screen.ppu->palette[256 + 5 * 16 + 15] == mzm_rom_u16(
           mzm_sym("sTourianEscape_479ee0") + 30));

    mzm_tourian_escape_update(&escape, &screen);
    assert(escape.timer == 4 && gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) ==
           (GBA_DCNT_BG0 | GBA_DCNT_OBJ));
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BG0CNT) == 30 << 8);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_BLDCNT) == (0x3F << 8 | 1 << 6));
    assert(screen.ppu->palette[256 + 5 * 16 + 15] == mzm_rom_u16(
           mzm_sym("sTourianEscape_479f00") + 30));
    assert(count_requests(MZM_SFX_PLAY,
                          MZM_SOUND_TOURIAN_ESCAPE_SHIP_FREE_FALLING) == 1);
    drawn = 0;
    for (int y = 0; y < GBA_SCREEN_H; y += 8) {
        gba_ppu_render_line(screen.ppu, y, GBA_SCREEN_W, line);
        for (int x = 0; x < GBA_SCREEN_W; ++x) drawn += (line[x] & 0x7FFF) != 0;
    }
    assert(drawn > GBA_SCREEN_W);

    while (escape.timer < 6) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.bg0_x == 15 && gba_ppu_read16(screen.ppu, GBA_REG_BG0HOFS) == 15);
    while (escape.timer < 7) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_active[1] == 1 && escape.object_x[1] == 111 &&
           escape.object_y[1] == 72 && escape.object_timer[1] == 1);
    assert(escape.object_oam_address[1] == mzm_sym("sTourianEscape_47a998"));
    assert(screen.next_oam_slot == 11 &&
           (screen.ppu->oam[2 * 4 + 2] & 0x3FF) == 0x84);
    mzm_tourian_escape_update(&escape, &screen);
    assert(escape.timer == 8 && screen.ppu->palette[256 + 5 * 16 + 15] ==
           mzm_rom_u16(mzm_sym("sTourianEscape_479ee0") + 30));
    while (escape.timer < 13) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_active[2] == 1 && escape.object_x[1] == 126 &&
           escape.object_y[1] == 84 && escape.object_timer[1] == 7);
    assert(escape.object_x[2] == 111 && escape.object_y[2] == 68 &&
           escape.object_timer[2] == 1);
    assert(screen.next_oam_slot == 12);
    while (escape.timer < 39) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.bg0_x == 10 && escape.object_x[1] == 111 &&
           escape.object_y[1] == 72 && escape.object_timer[1] == 0);
    assert(escape.object_x[2] == 241 && escape.object_y[2] == 34 &&
           escape.object_timer[2] == 27);
    while (escape.timer < 45) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.object_x[1] == 125 && escape.object_y[1] == 84 &&
           escape.object_timer[1] == 6);
    assert(escape.object_x[2] == 111 && escape.object_y[2] == 68 &&
           escape.object_timer[2] == 0);
    while (escape.timer < 80) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 8 && escape.bg0_x == 3);
    assert(escape.object_x[0] == 108 && escape.object_y[0] == 71);
    assert(escape.object_x[1] == 131 && escape.object_y[1] == 88 &&
           escape.object_x[2] == 115 && escape.object_y[2] == 64);
    mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 9 && escape.timer == 0 && escape.bg0_x == 3);
    assert(gba_ppu_read16(screen.ppu, GBA_REG_DISPCNT) == 0 &&
           gba_ppu_read16(screen.ppu, GBA_REG_BLDCNT) == 0);
    assert(screen.next_oam_slot == 0 && screen.ppu->oam[0] == 0x200);
    assert(escape.object_x[0] == 109 && escape.object_y[0] == 70 &&
           escape.object_x[1] == 135 && escape.object_y[1] == 90 &&
           escape.object_x[2] == 117 && escape.object_y[2] == 62);

    /* The other equipment branch selects the Power Suit palette. */
    assert(mzm_tourian_escape_enter(&escape, &screen, false));
    for (int f = 0; f < 513 + 3; ++f) mzm_tourian_escape_update(&escape, &screen);
    assert(escape.stage == 1 && escape.timer == 3 && screen.ppu->palette[17] ==
           mzm_rom_u16(mzm_sym("sTourianEscapeSamusInHerShipPowerSuitPal") + 34));
    screen.ppu = &ppu;
    mzm_rom_close();
}

/* The door fades' state machines (no ROM data). */
static void test_door_fade(void) {
    static MzmDoorFade f;
    /* A hatch: one setup frame, 16 colors out, the frame ending it, BG2/BG3
     * off, the tilemap on BG3, 8 blending frames and one hiding BG1, then
     * the load. */
    mzm_door_fade_start(&f, true, 224, 100);
    assert(f.phase == MZM_DOOR_FADE_OUT && f.bg_mask == 0xFFFD && f.obj_mask == 0xFFC7);
    int frames = 0;
    uint8_t last_color = 0;
    bool increasing = true;
    while (!mzm_door_fade_out(&f) && frames < 100) {
        frames++;
        if (f.faded) {
            increasing &= f.color >= last_color;
            last_color = f.color;
        }
        if (f.stage == 4 && f.blend) assert(f.door_bg3 && f.cover && !f.show_bg[0] && f.door_palette);
    }
    assert(frames == 1 + 16 + 1 + 1 + 1 + 9 && increasing && last_color == 32 && f.fading_type == MZM_FADING_OUT);
    assert(f.sprites_hidden && !f.show_bg[1] && !f.show_bg[2] && f.show_bg[3] && f.eva == 16 && f.evb == 0);
    /* Loaded: the tube slides 3 pixels a frame vertically, waits, then 6 a
     * frame horizontally; the room comes back and fades in over 16 frames. */
    mzm_door_fade_loaded(&f, 560, 40);
    assert(f.phase == MZM_DOOR_FADE_IN && f.faded && f.color == 0 && !f.door_palette);
    frames = 0;
    while (!mzm_door_fade_in(&f) && frames < 400) {
        frames++;
        if (f.stage == 5) assert(f.show_bg[0] && f.show_bg[1] && !f.cover && !f.sprites_hidden);
    }
    assert(f.phase == MZM_DOOR_FADE_NONE && !f.faded && f.door_x == 560 && f.door_y == 40);
    /* 1 + 20 vertical frames + 1, 3 waiting, 56 horizontal + 1, 16 colors + 1, the end */
    assert(frames == 1 + 20 + 1 + 3 + 56 + 1 + 16 + 1);
    /* Another door: 8 colors to black, everything covered, then back. */
    mzm_door_fade_start(&f, false, 0, 0);
    frames = 0;
    while (!mzm_door_fade_out(&f) && frames < 100) frames++;
    assert(frames == 1 + 8 + 1 && f.cover && !f.show_bg[1] && f.bg_mask == 0xFFFF && !f.sprites_hidden);
    mzm_door_fade_loaded(&f, 0, 0);
    frames = 0;
    while (!mzm_door_fade_in(&f) && frames < 100) frames++;
    assert(frames == 1 + 8 && !f.cover && f.show_bg[1]);
}

/* EVENT_STATUE_LONG_BEAM_GRABBED only. */
static bool hint_event(void *context, uint8_t event) {
    (void)context;
    return event == 8;
}

/* The Long Beam statue's hint on the exported bank: the hint screen, the
 * target travelling from the statue to the Long Beam, the map following
 * it, A to leave; then the target on the pause screen's map and world map. */
static void test_chozo_hint_bank(void) {
    if (!mzm_rom_open("assets/extracted/metroid/rom/rom_bank.bin",
                      "assets/extracted/metroid/rom/rom_symbols.tsv")) {
        printf("mzm_screens: no ROM bank, the Chozo hint checks are skipped\n");
        return;
    }
    static GbaPpu hud_ppu;
    static MzmHud hud;
    static MzmMinimapProgress progress;
    static MzmPauseScreen pause;
    mzm_hud_init(&hud, &hud_ppu, &progress, 240);
    uint8_t map_x, map_y;
    assert(mzm_hud_room_map(0, 2, &map_x, &map_y));
    MzmEquipment equipment = {.energy = 99, .max_energy = 99, .items = MZM_ITEM_MORPH_BALL};
    MzmHudFrame frame = {&equipment, 0, false, 0, 0, 53 * 64, 9 * 64, NULL, NULL, NULL, NULL};
    MzmHudRoom room = {0, 2, map_x, map_y, 64, 16};
    mzm_hud_room_entered(&hud, &room, &frame);
    hud.minimap_x = 9;
    hud.minimap_y = 15;
    MzmPauseGame game = {&hud, 0, &equipment, hint_event, NULL, MZM_PAUSE_FLAG_CHOZO_HINT};
    mzm_pause_enter(&pause);
    mzm_sfx_clear();
    int f;
    for (f = 0; f < 40 && pause.sub_stage != 2; ++f) {
        mzm_screen_input(&screen, 0);
        assert(!mzm_pause_update(&pause, &screen, &game));
    }
    assert(pause.sub_stage == 2 && pause.state == 15 && pause.type_flags == (1 << 1 | 1 << 4));
    mzm_screen_input(&screen, 0);
    mzm_pause_update(&pause, &screen, &game);
    assert(pause.state == 8);
    assert(pause.hint.index == 0 && pause.hint.activated == 1 && pause.hint_target_x == 6 &&
           pause.hint_target_y == 6);
    assert(pause.targets[0].x == 9 * 32 && pause.targets[0].y == 15 * 32 && pause.targets[1].x == 6 * 32 &&
           pause.targets[1].y == 6 * 32 && pause.dispcnt == (GBA_DCNT_BG1 | GBA_DCNT_BG2 | GBA_DCNT_BG3 | GBA_DCNT_OBJ));
    assert(pause.overlay[0].oam_id == 0x1A && screen.bldalpha_h == 7 && screen.bldalpha_l == 9);
    /* The target travels to the Long Beam; the map follows it. */
    bool moved = false;
    for (f = 0; f < 600 && pause.stage != 9; ++f) {
        mzm_screen_input(&screen, 0);
        assert(!mzm_pause_update(&pause, &screen, &game));
        moved |= pause.hint.movement_stage == 3 && pause.chozo_hint[0].y < 15 * 32;
    }
    assert(pause.stage == 9 && moved && pause.chozo_hint[0].exists == 0 && pause.targets[1].exists);
    assert(screen.bg_vofs[3] == (0x204 - (10 - 6) * 8) * 4 && screen.bg_hofs[3] == (0x204 - (15 - 6) * 8) * 4);
    /* A ends it: the music fades and the screen fades out. */
    mzm_screen_input(&screen, 1);
    mzm_pause_update(&pause, &screen, &game);
    bool left = false;
    for (f = 0; f < 60 && !left; ++f) {
        mzm_screen_input(&screen, 0);
        left = mzm_pause_update(&pause, &screen, &game);
    }
    assert(left && count_requests(MZM_SFX_MUSIC_FADE, 0) == 1);
    /* The pause afterwards: the target on Brinstar's map and world map. */
    game.flag = MZM_PAUSE_FLAG_PAUSE;
    mzm_pause_enter(&pause);
    for (f = 0; f < 40 && pause.sub_stage != 2; ++f) {
        mzm_screen_input(&screen, 0);
        mzm_pause_update(&pause, &screen, &game);
    }
    assert(pause.hint.activated == 1 && pause.targets[0].exists && pause.targets[0].x == 6 * 32 &&
           pause.areas_with_hints == 1 && pause.world_map[8].exists && pause.world_map[8].oam_id == 15);
    mzm_rom_close();
}

int main(void) {
    assert(mzm_screen_init(&screen, &ppu));
    test_fading();
    test_door_fade();
    test_menu_oam();
    test_title_bank();
    test_pause_bank();
    test_chozo_hint_bank();
    test_file_select_bank();
    test_game_over_bank();
    test_intro_bank();
    test_cutscene_bank();
    test_tourian_escape_bank();
    mzm_screen_free(&screen);
    printf("mzm_screens: ok\n");
    return 0;
}
