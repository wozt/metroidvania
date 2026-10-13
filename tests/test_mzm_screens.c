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
    MzmHudFrame frame = {&equipment, 0, false, 0, 0, 13 * 64, 31 * 64, NULL, NULL, NULL};
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

int main(void) {
    assert(mzm_screen_init(&screen, &ppu));
    test_fading();
    test_menu_oam();
    test_title_bank();
    test_pause_bank();
    test_file_select_bank();
    mzm_screen_free(&screen);
    printf("mzm_screens: ok\n");
    return 0;
}
