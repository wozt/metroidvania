/* SPDX-License-Identifier: GPL-3.0-only */
/* The file select screen (see mzm_file_select.h); comments name the pinned
 * decompilation's routines. */
#include "mzm_file_select.h"

#include <string.h>

#include "mzm_rom.h"
#include "mzm_sfx.h"

enum {
    BLOCK = 64, HALF_BLOCK = 32, QUARTER_BLOCK = 16,
    /* FileSelectOam */
    OAM_0 = 0, OAM_1, OAM_2, OAM_3, OAM_MEDIUM_PANEL, OAM_COPY_ARROW, OAM_SUB_CURSOR, OAM_CURSOR,
    OAM_FILE_A_MARKER, OAM_FILE_B_MARKER, OAM_FILE_C_MARKER, OAM_SMALL_PANEL, OAM_FILE_A_LOGO,
    OAM_FILE_B_LOGO, OAM_FILE_C_LOGO, OAM_COUNT,
    /* FileSelectOamId */
    ID_FILE_A_MARKER_IDLE = 13, ID_FILE_A_MARKER_SELECTED_RED = 16, ID_CURSOR_IDLE = 23,
    ID_CURSOR_SELECTED = 24, ID_METROID_LOGO = 33, ID_SMALL_PANEL_OPEN = 34, ID_SMALL_PANEL_CLOSE = 35,
    ID_LARGE_PANEL_OPEN = 36, ID_LARGE_PANEL_CLOSE = 37,
    /* FileSelectCursorPosition */
    POSITION_FILE_A = 0, POSITION_FILE_B, POSITION_FILE_C, POSITION_COPY, POSITION_ERASE,
    POSITION_OPTIONS,
    /* CursorPose */
    CURSOR_DEFAULT = 0, CURSOR_MOVING = 1, CURSOR_OPENING_OPTIONS = 2, CURSOR_SELECTING_FILE = 5,
    CURSOR_DESELECTING_FILE = 6, CURSOR_STARTING_GAME = 7, CURSOR_CANCEL_START = 8,
    /* FileSelectSubMenu */
    SUB_MENU_MAIN = 0, SUB_MENU_FILE, SUB_MENU_COPY, SUB_MENU_ERASE,
    /* CursorCopyPose / CursorErasePose, ArrowCopyPose */
    SUB_CURSOR_DEFAULT = 0, SUB_CURSOR_MOVING, SUB_CURSOR_SELECTING_FILE, SUB_CURSOR_DONE,
    ARROW_DEFAULT = 0, ARROW_MOVING, ARROW_COPYING, ARROW_KILL,
    ID_FILE_A_MARKER_SELECTED_GREEN = 19, ID_MEDIUM_PANEL_OPEN = 38, ID_MEDIUM_PANEL_CLOSE = 39,
    /* FileSelectSelectionSet */
    SET_ALL_FILES = 0, SET_ONLY_FILES, SET_ALL_FILES_NO_COPY,
    /* FileScreenProcessTextStage (US) */
    TEXT_NONE = 0, TEXT_INIT, TEXT_LINE_1, TEXT_LINE_4 = TEXT_LINE_1 + 3, TEXT_UPDATE_QUEUE,
    /* FileScreenMessageInfoId */
    MESSAGE_START_GAME = 0, MESSAGE_COPY_SOURCE_CHOOSE = 1, MESSAGE_COPY_DESTINATION_CHOOSE = 2,
    MESSAGE_COPY_CONFIRM = 3, MESSAGE_CANT_COPY_TIME_ATTACK = 4, MESSAGE_ERASE_CHOOSE = 5,
    MESSAGE_ERASE_CONFIRM = 6, MESSAGE_CONTINUE_NEW = 15, MESSAGE_CONTINUE_NEW_TIME_ATTACK = 16,
    MESSAGE_DIFFICULTY = 18, MESSAGE_DIFFICULTY_HARD = 19, MESSAGE_ERASE_FILE_CONFIRM = 20,
    /* MenuSoundRequest */
    SOUND_REQUEST_SUB_MENU_CURSOR = 0, SOUND_REQUEST_ACCEPT_CONFIRM_MENU, SOUND_REQUEST_CURSOR,
    SOUND_REQUEST_FILE_SELECT, SOUND_REQUEST_START_GAME, SOUND_REQUEST_OPEN_SUB_MENU,
    SOUND_REQUEST_CLOSE_SUB_MENU, SOUND_REQUEST_CLOSE_SUB_MENU2, SOUND_REQUEST_COPY_DELETE,
    SOUND_REQUEST_COPY_DELETE_MOVING, SOUND_REQUEST_COPY_CONFIRM, SOUND_REQUEST_COUNT = 13,
    SOUND_REFUSE_MENU = 0x209, SOUND_FILE_DELETE = 0x20D, SOUND_SELECTING_FILE_TO_DELETE = 0x210,
    MUSIC_FILE_SELECT = 0x9,
    /* FileSelectFadingStage */
    FADING_IN = 0, FADING_CHECK_IN, FADING_OUT, FADING_CHECK_OUT,
    /* Tilemap requests (TilemapRequest values) */
    TILEMAP_ERASE_SPAWN_INIT = 10, TILEMAP_ERASE_SPAWN, TILEMAP_ERASE_DESPAWN_INIT, TILEMAP_ERASE_DESPAWN,
    TILEMAP_ERASE_YES_NO_SPAWN_INIT, TILEMAP_ERASE_YES_NO_SPAWN, TILEMAP_ERASE_YES_NO_DESPAWN_INIT,
    TILEMAP_ERASE_YES_NO_DESPAWN, TILEMAP_COPY_SPAWN_INIT, TILEMAP_COPY_SPAWN, TILEMAP_COPY_DESPAWN_INIT,
    TILEMAP_COPY_DESPAWN, TILEMAP_COPY_OVERRIDE_SPAWN_INIT, TILEMAP_COPY_OVERRIDE_SPAWN,
    TILEMAP_COPY_OVERRIDE_DESPAWN_INIT, TILEMAP_COPY_OVERRIDE_DESPAWN, TILEMAP_COPY_DESTINATION_DESPAWN,
    TILEMAP_COPY_DESTINATION_SPAWN, TILEMAP_28,
    TILEMAP_START_GAME_INIT = 0, TILEMAP_START_GAME = 1, TILEMAP_DIFFICULTY_SPAWN_INIT = 6,
    TILEMAP_DIFFICULTY_SPAWN = 7, TILEMAP_DIFFICULTY_DESPAWN_INIT = 8, TILEMAP_DIFFICULTY_DESPAWN = 9,
    TILEMAP_29 = 29, TILEMAP_30, TILEMAP_31, TILEMAP_32, TILEMAP_34 = 34, TILEMAP_35, TILEMAP_36,
    TILEMAP_37,
    /* GBA keys */
    KEY_A = 1 << 0, KEY_B = 1 << 1, KEY_RIGHT = 1 << 4, KEY_LEFT = 1 << 5, KEY_UP = 1 << 6,
    KEY_DOWN = 1 << 7, KEY_DIRECTIONS = 0xF0,
    /* BLDCNT */
    BLDCNT_BG1_FIRST = 1 << 1, BLDCNT_ALPHA = 1 << 6, BLDCNT_SCREEN_SECOND = 0x3F << 8,
    BLDCNT_SCREEN_FIRST = 0x3F, BLDCNT_BRIGHTNESS_DOWN = 3 << 6,
    /* FILE_SELECT_TILE_* */
    TILE_TWO_DOTS_HIGH = 0x1CA, TILE_TWO_DOTS_LOW = 0x1EA, TILE_MINUS_HIGH = 0x1CB,
    TILE_MINUS_LOW = 0x1EB, TILE_NUMBER_HIGH = 0x1CC, TILE_NUMBER_LOW = 0x1EC,
    NON_GAMEPLAY_START_BG_POS = BLOCK * 32,
};

static uint32_t sym(const char *name) { return mzm_rom_symbol(name, NULL); }

/* The EWRAM buffers of sEwramPointer, as halfwords. */
static uint16_t *ew16(MzmScreen *s, uint32_t offset) { return (uint16_t *)(s->ewram + offset); }

static void ewram_lz77(MzmScreen *s, const char *name, uint32_t offset) {
    mzm_lz77_from_rom(sym(name), s->ewram + offset, MZM_EWRAM_SIZE - offset);
}

static void vram_lz77(MzmScreen *s, const char *name, uint32_t offset) {
    mzm_lz77_from_rom(sym(name), s->ppu->vram + offset, GBA_VRAM_SIZE - offset);
}

/* CREATE_BGCNT */
static uint16_t bgcnt(int char_base, int screen_base, int priority) {
    return (uint16_t)(screen_base << 8 | char_base << 2 | priority);
}

/* FileSelectPlayMenuSound (sMenuSounds) */
static void play_menu_sound(int request) {
    if (request < SOUND_REQUEST_COUNT) mzm_sfx_play(mzm_rom_u16(sym("sMenuSounds") + (uint32_t)request * 2u));
}

/* sFileSelectCursorOamData[position]: xPosition, yPosition,
 * oamFileMarkerId, oamIds[suit] */
static uint32_t cursor_data(int position) { return sym("sFileSelectCursorOamData") + (uint32_t)position * 8u; }
static int16_t cursor_x(int position) { return (int16_t)mzm_rom_u16(cursor_data(position)); }
static int16_t cursor_y(int position) { return (int16_t)mzm_rom_u16(cursor_data(position) + 2); }
static uint8_t cursor_marker(int position) { return mzm_rom_u8(cursor_data(position) + 4); }
static uint8_t cursor_oam_id(int position, int suit) { return mzm_rom_u8(cursor_data(position) + 5 + (uint32_t)suit); }

/* sFileScreenMessagesInfo[id]: text, buffer, kind */
static uint8_t message_info(uint8_t id, int field) {
    return mzm_rom_u8(sym("sFileScreenMessagesInfo") + id * 3u + (uint32_t)field);
}

/* sFileSelect_760bdc */
static const uint32_t TEXT_BUFFERS[4] = {0x5800, 0x6000, 0x6200, 0x7000};

static void fill(MzmScreen *s, uint32_t vram, uint32_t bytes, uint8_t value) {
    memset(s->ppu->vram + vram, value, bytes);
}

/* FileScreenProcessText (US) */
static void process_text(MzmFileSelect *f, MzmScreen *s) {
    uint8_t id = f->message_queue[0];
    switch (f->process_text_stage) {
    case TEXT_NONE:
        break;
    case TEXT_INIT: {
        f->message = mzm_text_message_from_rom(sym("sFileScreenMessage_Empty"));
        uint8_t value = f->message.is_message ? 0xFF : 0;
        uint8_t buffer = message_info(id, 1);
        uint32_t base = TEXT_BUFFERS[buffer & 3];
        if (buffer == 3 || buffer == 1) {
            fill(s, base, 0x1000, value);
        } else if (buffer == 2) {
            /* The US code adds these offsets in bytes (the decompiled
             * source's u32 pointers would multiply them by 4). */
            fill(s, base, 0x200, value);
            fill(s, base + 0x400, 0x200, value);
            fill(s, base + 0x600, 0x800, value);
        } else {
            fill(s, base, 0x800, value);
            if (message_info(id, 2) == 3) {
                fill(s, base + 0x800, 0x200, value);
                fill(s, base + 0xC00, 0x200, value);
            }
        }
        f->process_text_stage++;
        break;
    }
    case TEXT_LINE_1:
    case TEXT_LINE_1 + 1:
    case TEXT_LINE_1 + 2:
    case TEXT_LINE_4: {
        uint8_t buffer = message_info(id, 1);
        uint32_t dst = TEXT_BUFFERS[buffer & 3];
        int line = f->process_text_stage - TEXT_LINE_1;
        if (line & 2) dst += 0x800;
        if (line & 1) {
            dst += 0x200;
            if (buffer == 2) dst += 0x400;
        }
        f->message.indent = 0;
        uint32_t text = mzm_rom_u32(sym("sEnglishTextPointers_FileScreen") + message_info(id, 0) * 4u);
        for (int guard = 0; guard < 4096; ++guard) {
            int state = mzm_text_process_message(&f->message, text, s->ppu->vram + dst, s->button_input);
            if (state == MZM_TEXT_ENDED) {
                f->process_text_stage = TEXT_UPDATE_QUEUE;
                break;
            }
            if (state == MZM_TEXT_NEW_LINE) {
                f->process_text_stage++;
                break;
            }
        }
        break;
    }
    case TEXT_UPDATE_QUEUE: {
        uint8_t next[2] = {0xFF, 0xFF};
        int count = 0;
        f->message_queue[0] = 0xFF;
        if (f->message_queue[1] != 0xFF) {
            next[0] = f->message_queue[1];
            f->message_queue[1] = 0xFF;
            count++;
        }
        if (f->message_queue[2] != 0xFF) {
            next[count] = f->message_queue[2];
            f->message_queue[2] = 0xFF;
        }
        f->message_queue[0] = next[0];
        f->message_queue[1] = next[1];
        f->process_text_stage = f->message_queue[0] != 0xFF;
        break;
    }
    default:
        break;
    }
}

/* FileScreenUpdateMessageInfoIdQueue: 0 queues, 1 checks the message is
 * no longer queued, 2 clears. */
static bool update_message_queue(MzmFileSelect *f, int option, uint8_t id) {
    if (option == 0) {
        for (int i = 0; i < 3; ++i) {
            if (f->message_queue[i] != 0xFF) continue;
            f->message_queue[i] = id;
            if (i == 0) f->process_text_stage = TEXT_INIT;
            return true;
        }
        return false;
    }
    if (option == 1) {
        for (int i = 0; i < 3; ++i)
            if (f->message_queue[i] == id) return false;
        return true;
    }
    memset(f->message_queue, 0xFF, sizeof f->message_queue);
    f->process_text_stage = TEXT_NONE;
    return true;
}

/* FileSelectUpdateCursor */
static void update_cursor(MzmFileSelect *f, int pose, int position) {
    int suit = position < POSITION_COPY ? f->files[position].suit : 0;
    uint8_t oam_id = cursor_oam_id(position, suit < 3 ? suit : 0);
    MzmMenuOam *cursor = &f->oam[OAM_CURSOR];
    switch (pose) {
    case CURSOR_DEFAULT:
        oam_id++;
        /* fall through */
    case CURSOR_MOVING:
        mzm_menu_oam_set_id(cursor, oam_id);
        cursor->x = cursor_x(position);
        cursor->y = cursor_y(position);
        for (int file = 0; file < 3; ++file)
            mzm_menu_oam_set_id(&f->oam[OAM_FILE_A_MARKER + file],
                                position != file ? cursor_marker(file)
                                                 : (uint8_t)(ID_FILE_A_MARKER_SELECTED_RED + file));
        break;
    case CURSOR_OPENING_OPTIONS:
        mzm_menu_oam_set_id(cursor, (uint8_t)(oam_id + 2));
        break;
    case CURSOR_SELECTING_FILE:
        mzm_menu_oam_set_id(cursor, (uint8_t)(oam_id + 2));
        for (int file = 0; file < 3; ++file) {
            if (position == file) continue;
            f->oam[OAM_FILE_A_MARKER + file].obj_mode = 1;
            f->oam[OAM_FILE_A_LOGO + file].obj_mode = 1;
        }
        break;
    case CURSOR_DESELECTING_FILE:
        mzm_menu_oam_set_id(cursor, (uint8_t)(oam_id + 1));
        for (int file = 0; file < 3; ++file) {
            f->oam[OAM_FILE_A_MARKER + file].obj_mode = 0;
            f->oam[OAM_FILE_A_LOGO + file].obj_mode = 0;
        }
        break;
    case CURSOR_STARTING_GAME:
        mzm_menu_oam_set_id(cursor, (uint8_t)(oam_id + 5));
        break;
    case CURSOR_CANCEL_START:
        cursor->oam_id = (uint8_t)(oam_id + 4);
        break;
    default:
        break;
    }
}

/* unk_7e3fc for the sub cursors of the panels ported here: 0 the "Start
 * game" cursor, 2 the difficulty, 5 continue / new game, 6 the overwrite
 * confirmation. */
static void sub_cursor(MzmFileSelect *f, int menu, uint8_t value) {
    MzmMenuOam *o = menu == 0 ? &f->oam[OAM_SUB_CURSOR] : &f->oam[OAM_3];
    if (value == 0x80) {
        mzm_menu_oam_set_id(o, ID_CURSOR_SELECTED);
        return;
    }
    if (value == 0x81) {
        o->oam_id = 0;
        o->exists = 0;
        return;
    }
    switch (menu) {
    case 0:
        if (value != 0) return;
        f->oam[OAM_3].priority = 1;
        o->bound_background = 1;
        o->x = BLOCK * 2;
        o->y = BLOCK * 2 - 8;
        break;
    case 2:
        if (value > 2) return;
        o->priority = 0;
        o->bound_background = 0;
        o->x = BLOCK * 2;
        o->y = (int16_t)(BLOCK * (2 + value) + 24);
        break;
    case 3:
    case 4:
        if (value > 1) return;
        o->priority = 0;
        o->bound_background = 0;
        o->y = BLOCK * 3 + 24;
        o->x = value == 0 ? BLOCK * 3 + QUARTER_BLOCK : BLOCK * 6 - QUARTER_BLOCK;
        break;
    case 5:
        if (value > 2) return;
        o->priority = 0;
        o->bound_background = 0;
        o->x = BLOCK + HALF_BLOCK + 12;
        o->y = (int16_t)(BLOCK * (1 + value) + 24);
        break;
    case 6:
        if (value > 1) return;
        o->priority = 0;
        o->bound_background = 0;
        o->y = BLOCK * 4 + HALF_BLOCK - 8;
        o->x = value == 0 ? BLOCK * 3 - 8 : BLOCK * 6 - QUARTER_BLOCK;
        if (f->oam[OAM_0].oam_id != ID_METROID_LOGO) {
            mzm_menu_oam_set_id(&f->oam[OAM_0], ID_METROID_LOGO);
            f->oam[OAM_0].x = BLOCK * 2 - 8;
            f->oam[OAM_0].y = BLOCK * 3 + QUARTER_BLOCK;
        }
        break;
    default:
        return;
    }
    if (o->oam_id != ID_CURSOR_IDLE) mzm_menu_oam_set_id(o, ID_CURSOR_IDLE);
}

/* unk_7eedc: the difficulty rows the file completed are highlighted. */
static void highlight_completed(MzmFileSelect *f, uint16_t *tilemap) {
    uint8_t flags = f->files[f->cursor_position].completed_game;
    uint16_t completion = (uint16_t)((flags & 7) | (flags >> 3 & 7));
    tilemap += 128;
    for (int i = 0; i < 3; ++i) {
        uint16_t base = completion >> i & 1 ? 14 << 12 : 15 << 12;
        for (int j = 2; j < 18; ++j) {
            tilemap[i * 64 + j] = (uint16_t)((tilemap[i * 64 + j] & 0x3FF) | base);
            tilemap[i * 64 + 32 + j] = (uint16_t)((tilemap[i * 64 + 32 + j] & 0x3FF) | base);
        }
    }
}

static void copy_ewram_to_vram(MzmScreen *s, uint32_t ewram, uint32_t vram, uint32_t bytes) {
    memcpy(s->ppu->vram + vram, s->ewram + ewram, bytes);
}

static bool panel_closed(const MzmFileSelect *f) { return f->oam[OAM_MEDIUM_PANEL].oam_id == 0; }

/* FileSelectUpdateTilemap (the requests of the ported panels) */
static bool update_tilemap(MzmFileSelect *f, MzmScreen *s, int request) {
    MzmMenuOam *small = &f->oam[OAM_SMALL_PANEL], *medium = &f->oam[OAM_MEDIUM_PANEL];
    switch (request) {
    case TILEMAP_START_GAME_INIT:
        small->x = BLOCK * 4;
        small->y = BLOCK * 2;
        mzm_menu_oam_set_id(small, ID_SMALL_PANEL_OPEN);
        play_menu_sound(SOUND_REQUEST_OPEN_SUB_MENU);
        s->bg_hofs[1] = BLOCK * 31;
        s->bg_vofs[1] = BLOCK * 29 + HALF_BLOCK + 8;
        copy_ewram_to_vram(s, 0x3000, 0xE800, 0x300);
        return true;
    case TILEMAP_START_GAME:
        if (small->ended && update_message_queue(f, 1, MESSAGE_START_GAME)) {
            f->bldcnt = 0;
            f->bg1cnt = f->unk_1c;
            return true;
        }
        return false;
    case TILEMAP_DIFFICULTY_SPAWN_INIT:
        medium->x = BLOCK * 5;
        medium->y = BLOCK * 3;
        mzm_menu_oam_set_id(medium, ID_LARGE_PANEL_OPEN);
        play_menu_sound(SOUND_REQUEST_OPEN_SUB_MENU);
        s->bg_hofs[0] = BLOCK * 28;
        s->bg_vofs[0] = BLOCK * 28 + HALF_BLOCK;
        return true;
    case TILEMAP_DIFFICULTY_SPAWN:
        if (medium->ended && update_message_queue(f, 1, f->difficulty_message)) {
            highlight_completed(f, ew16(s, 0x3C00));
            copy_ewram_to_vram(s, 0x3C00, 0xE000, 0x300);
            f->bg0cnt = f->unk_1e;
            f->dispcnt |= GBA_DCNT_BG0;
            return true;
        }
        return false;
    case TILEMAP_DIFFICULTY_DESPAWN_INIT:
    case TILEMAP_31:
        f->dispcnt &= (uint16_t)~GBA_DCNT_BG0;
        medium->oam_id = ID_LARGE_PANEL_CLOSE;
        play_menu_sound(SOUND_REQUEST_CLOSE_SUB_MENU);
        return true;
    case TILEMAP_29:
        medium->x = BLOCK * 5;
        medium->y = BLOCK * 3;
        mzm_menu_oam_set_id(medium, ID_LARGE_PANEL_OPEN);
        play_menu_sound(SOUND_REQUEST_OPEN_SUB_MENU);
        s->bg_hofs[0] = BLOCK * 28;
        s->bg_vofs[0] = BLOCK * 28 + HALF_BLOCK;
        copy_ewram_to_vram(s, 0x3300, 0xE000, 0x300);
        return true;
    case TILEMAP_30:
        if (medium->ended && update_message_queue(f, 1, f->continue_new_message)) {
            f->bg0cnt = f->unk_1e;
            f->dispcnt |= GBA_DCNT_BG0;
            return true;
        }
        return false;
    case TILEMAP_34:
        if (update_message_queue(f, 1, f->continue_new_message)) {
            copy_ewram_to_vram(s, 0x3600, 0xE000, 0x300);
            return true;
        }
        return false;
    case TILEMAP_35:
        f->dispcnt &= (uint16_t)~GBA_DCNT_BG0;
        f->oam[OAM_0].oam_id = 0;
        f->oam[OAM_0].exists = 0;
        medium->oam_id = ID_LARGE_PANEL_CLOSE;
        play_menu_sound(SOUND_REQUEST_CLOSE_SUB_MENU);
        return true;
    case TILEMAP_37:
        f->oam[OAM_0].oam_id = 0;
        f->oam[OAM_0].exists = 0;
        copy_ewram_to_vram(s, 0x3300, 0xE000, 0x300);
        return true;
    case TILEMAP_ERASE_SPAWN_INIT:
    case TILEMAP_COPY_SPAWN_INIT:
        small->x = BLOCK * 5;
        small->y = BLOCK * 2 + HALF_BLOCK;
        mzm_menu_oam_set_id(small, ID_MEDIUM_PANEL_OPEN);
        play_menu_sound(SOUND_REQUEST_OPEN_SUB_MENU);
        s->bg_hofs[1] = BLOCK * 27 + 8;
        s->bg_vofs[1] = BLOCK * 26 + HALF_BLOCK + 8;
        copy_ewram_to_vram(s, 0x3F00, 0xE800, 0x300);
        f->bg1cnt = f->unk_1c;
        f->dispcnt &= (uint16_t)~GBA_DCNT_BG1;
        f->bg2cnt = f->unk_1a;
        f->dispcnt |= GBA_DCNT_BG2;
        return true;
    case TILEMAP_ERASE_SPAWN:
    case TILEMAP_COPY_SPAWN:
        if (small->ended && update_message_queue(f, 1, request == TILEMAP_ERASE_SPAWN ? MESSAGE_ERASE_CHOOSE
                                                                                      : MESSAGE_COPY_SOURCE_CHOOSE)) {
            f->bg1cnt = f->unk_1c;
            f->dispcnt |= GBA_DCNT_BG1;
            return true;
        }
        return false;
    case TILEMAP_ERASE_DESPAWN_INIT:
    case TILEMAP_COPY_DESPAWN_INIT:
        f->dispcnt &= (uint16_t)~GBA_DCNT_BG1;
        small->oam_id = ID_MEDIUM_PANEL_CLOSE;
        play_menu_sound(SOUND_REQUEST_CLOSE_SUB_MENU);
        return true;
    case TILEMAP_ERASE_DESPAWN:
    case TILEMAP_COPY_DESPAWN:
        if (small->oam_id != 0) return false;
        f->bg1cnt = f->unk_18;
        f->bg2cnt = f->unk_16;
        f->dispcnt &= (uint16_t)~GBA_DCNT_BG2;
        f->dispcnt |= GBA_DCNT_BG1;
        s->bg_hofs[1] = BLOCK * 32;
        s->bg_vofs[1] = BLOCK * 32;
        return true;
    case TILEMAP_ERASE_YES_NO_SPAWN_INIT:
    case TILEMAP_COPY_OVERRIDE_SPAWN_INIT:
        s->bg_hofs[0] = BLOCK * 27 + 8;
        s->bg_vofs[0] = BLOCK * 26 + HALF_BLOCK + 8;
        copy_ewram_to_vram(s, 0x4200, 0xE000, 0x300);
        return true;
    case TILEMAP_ERASE_YES_NO_SPAWN:
    case TILEMAP_COPY_OVERRIDE_SPAWN:
        if (update_message_queue(f, 1, request == TILEMAP_ERASE_YES_NO_SPAWN ? MESSAGE_ERASE_CONFIRM
                                                                              : MESSAGE_COPY_CONFIRM)) {
            f->bg0cnt = f->unk_1e;
            f->dispcnt |= GBA_DCNT_BG0;
            f->dispcnt &= (uint16_t)~GBA_DCNT_BG1;
            return true;
        }
        return false;
    case TILEMAP_ERASE_YES_NO_DESPAWN_INIT:
    case TILEMAP_COPY_OVERRIDE_DESPAWN_INIT:
        f->dispcnt &= (uint16_t)~GBA_DCNT_BG0;
        f->dispcnt |= GBA_DCNT_BG1;
        return true;
    case TILEMAP_COPY_DESTINATION_DESPAWN:
        copy_ewram_to_vram(s, 0x3F00, 0xE800, 0x300);
        return true;
    case TILEMAP_COPY_DESTINATION_SPAWN:
        copy_ewram_to_vram(s, 0x4500, 0xE800, 0x300);
        return true;
    case TILEMAP_28:
        copy_ewram_to_vram(s, 0x4200, 0xE800, 0x300);
        return true;
    case TILEMAP_DIFFICULTY_DESPAWN:
    case TILEMAP_32:
    case TILEMAP_36:
        return panel_closed(f);
    default:
        return true;
    }
}

/* HudDrawEnergy(file + 1): the energy digits and tanks into the file's
 * BG tiles. */
static void draw_energy(MzmScreen *s, int file, uint16_t energy, uint16_t max_energy) {
    static const char *const tanks_gfx[13] = {
        "sEnergyTanksGfx_Zero", "sEnergyTanksGfx_One", "sEnergyTanksGfx_Two",
        "sEnergyTanksGfx_Three", "sEnergyTanksGfx_Four", "sEnergyTanksGfx_Five",
        "sEnergyTanksGfx_Six", "sEnergyTanksGfx_Seven", "sEnergyTanksGfx_Eight",
        "sEnergyTanksGfx_Nine", "sEnergyTanksGfx_Ten", "sEnergyTanksGfx_Eleven",
        "sEnergyTanksGfx_Twelve"};
    uint32_t dst = 0x20u + (uint32_t)file * 0xE0u;
    uint8_t tanks = (uint8_t)(max_energy / 100), full = (uint8_t)(energy / 100);
    mzm_copy_from_rom(s->ppu->vram + dst, sym("sEnergyDigitsTensGfx") + (energy / 10 % 10) * 32u, 32);
    mzm_copy_from_rom(s->ppu->vram + dst + 32, sym("sEnergyDigitsOnesGfx") + (energy % 10) * 32u, 32);
    if (tanks == 0) mzm_copy_from_rom(s->ppu->vram + dst + 64, sym(tanks_gfx[0]), 160);
    else if (tanks <= 12) mzm_copy_from_rom(s->ppu->vram + dst + 64, sym(tanks_gfx[tanks]) + full * 160u, 160);
}

/* FileSelectDisplaySaveFileHealth */
static void display_health(MzmFileSelect *f, MzmScreen *s, int file) {
    const MzmSaveFileInfo *info = &f->files[file];
    if ((info->exists || info->intro_played) && !info->corrupted)
        draw_energy(s, file, info->energy, info->max_energy);
    else
        fill(s, 0x20u + (uint32_t)file * 0xE0u, 0xE0, 0);
}

/* FileSelectDisplaySaveFileTimer: HH:MM:SS or --:--:-- */
static void display_timer(MzmFileSelect *f, MzmScreen *s, int file) {
    const MzmSaveFileInfo *info = &f->files[file];
    uint16_t *dst = ew16(s, 0x800) + file * 96 + 0x6E;
    uint16_t base = info->time_attack ? 6 << 12 : 5 << 12;
    bool shown = info->exists && !info->corrupted;
    uint8_t values[3] = {info->igt_hours, info->igt_minutes, info->igt_seconds};
    for (int part = 0; part < 3; ++part) {
        for (int digit = 0; digit < 2; ++digit) {
            int i = part * 3 + digit;
            uint8_t number = digit == 0 ? values[part] / 10 : values[part] % 10;
            dst[i] = (uint16_t)(base | (shown ? TILE_NUMBER_HIGH + number : TILE_MINUS_HIGH));
            dst[i + 32] = (uint16_t)(base | (shown ? TILE_NUMBER_LOW + number : TILE_MINUS_LOW));
        }
        if (part < 2) {
            dst[part * 3 + 2] = (uint16_t)(base | TILE_TWO_DOTS_HIGH);
            dst[part * 3 + 2 + 32] = (uint16_t)(base | TILE_TWO_DOTS_LOW);
        }
    }
}

/* FileSelectDisplaySaveFileMiscInfo: the difficulty (or time attack) and
 * the area. */
static void display_misc(MzmFileSelect *f, MzmScreen *s, int file) {
    const MzmSaveFileInfo *info = &f->files[file];
    uint16_t palette = info->time_attack ? 6 << 12 : 5 << 12;
    uint16_t *dst = ew16(s, 0x800) + file * 96 + 102;
    bool shown = (info->exists || info->intro_played) && !info->corrupted;
    uint16_t tile = info->time_attack ? 0x1AF : (uint16_t)(0x1A0 + info->difficulty * 5);
    for (int i = 0; i < 5; ++i) dst[i] = shown ? (uint16_t)(palette | (tile + i)) : 0;
    dst = ew16(s, 0x800) + file * 96 + 150;
    int area = info->area < 12 ? (int8_t)mzm_rom_u8(sym("sSaveFileAreasId") + info->area) : -1;
    tile = (uint16_t)(area * 6 + 0x176);
    for (int i = 0; i < 6; ++i) dst[i] = shown && area >= 0 ? (uint16_t)(palette | (tile + i)) : 0;
}

/* FileScreenSetEnabledMenuFlags */
static void set_enabled_menus(MzmFileSelect *f, MzmScreen *s) {
    f->enabled_menus = 0;
    for (int file = 0; file < 3; ++file)
        if (!f->files[file].corrupted && (f->files[file].exists || f->files[file].intro_played))
            f->enabled_menus |= (uint8_t)(1 << file);
    uint16_t palette = 1;
    if (f->enabled_menus) {
        f->enabled_menus |= 1 << POSITION_COPY | 1 << POSITION_ERASE;
        palette = 0;
    }
    uint16_t *src = ew16(s, 0xB40);
    for (int i = 0; i < 0x80; ++i) src[i] = (uint16_t)((src[i] & 0xFFF) | palette << 12);
    f->enabled_menus |= 1 << POSITION_OPTIONS;
}

/* FileSelectDisplaySaveFileInfo and its panel tilemap copies */
static void display_files(MzmFileSelect *f, MzmScreen *s) {
    for (int file = 0; file < 3; ++file) {
        display_health(f, s, file);
        display_timer(f, s, file);
        display_misc(f, s, file);
    }
    set_enabled_menus(f, s);
    static const struct { uint32_t clear, size, src, bytes; } copies[] = {
        {0x3000, 0x300, 0x1800, 0x200}, {0x3300, 0x300, 0x1A00, 0x300}, {0x3600, 0x300, 0x1D00, 0x300},
        {0x3900, 0x300, 0x2000, 0x280}, {0x3C00, 0x300, 0x2280, 0x300}, {0x3F00, 0x300, 0x2800, 0x280},
        {0x4200, 0x300, 0x2A80, 0x280}, {0x4500, 0x300, 0x2D00, 0x280}, {0x4800, 0x300, 0x2580, 0x200},
    };
    for (size_t i = 0; i < sizeof copies / sizeof copies[0]; ++i) {
        memset(s->ewram + copies[i].clear, 0, copies[i].size);
        memcpy(s->ewram + copies[i].clear, s->ewram + copies[i].src, copies[i].bytes);
    }
    memcpy(s->ewram + 0x4B00, s->ewram + 0x3300, 0x300);
    memcpy(s->ewram + 0x4E00, s->ewram + 0x3600, 0x300);
}

/* FileSelectInitFading */
static void init_fading(MzmFileSelect *f, MzmScreen *s, bool fading_out) {
    f->fading.color = 0;
    f->fading.palette_updated = false;
    f->fading.timer = 0;
    memcpy(ew16(s, 0), s->ppu->palette, 0x400);
    if (!fading_out) {
        memset(s->ppu->palette, 0, sizeof s->ppu->palette);
        memcpy(ew16(s, 0x400), s->ppu->palette, 0x400);
        f->fading.stage = FADING_IN;
    } else {
        f->fading.stage = FADING_OUT;
    }
    f->fading.intensity = 8;
    f->fading.frequency = 0;
}

/* FileSelectApplyFading */
static void apply_fading(MzmFileSelect *f, MzmScreen *s) {
    if (!f->fading.palette_updated) return;
    memcpy(s->ppu->palette, ew16(s, 0x400), 0x400);
    f->fading.palette_updated = false;
}

/* FileSelectUpdateFading */
static bool update_fading(MzmFileSelect *f, MzmScreen *s) {
    f->fading.timer++;
    switch (f->fading.stage) {
    case FADING_IN:
    case FADING_OUT: {
        if (f->fading.palette_updated || f->fading.timer < f->fading.frequency) break;
        bool out = f->fading.stage == FADING_OUT;
        if (out) f->fading.timer = 0;
        if (f->fading.color < 32) {
            uint8_t type = out ? MZM_FADING_OUT : MZM_FADING_IN;
            mzm_apply_fading_color(type, f->fading.color, ew16(s, 0), ew16(s, 0x400), 0xFFFF);
            mzm_apply_fading_color(type, f->fading.color, ew16(s, 0x200), ew16(s, 0x600), 0xFFFF);
            f->fading.palette_updated = true;
            if (f->fading.color == 31) f->fading.color++;
            else f->fading.color = f->fading.color + f->fading.intensity > 31 ? 31
                                                                           : (uint8_t)(f->fading.color + f->fading.intensity);
            break;
        }
        if (out) memset(ew16(s, 0x400), 0, 0x400);
        else memcpy(ew16(s, 0x400), ew16(s, 0), 0x400);
        f->fading.palette_updated = true;
        f->fading.stage++;
        break;
    }
    case FADING_CHECK_IN:
    case FADING_CHECK_OUT:
        if (!f->fading.palette_updated) {
            f->fading.color = 0;
            f->fading.stage = 0;
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

/* FileSelectResetOam */
static void reset_oam(MzmFileSelect *f) {
    MzmMenuOam empty = mzm_menu_oam_from_rom(sym("sFileSelectOamData_Empty"));
    for (int i = 0; i < OAM_COUNT; ++i) f->oam[i] = empty;
    for (int file = 0; file < 3; ++file) {
        MzmMenuOam *marker = &f->oam[OAM_FILE_A_MARKER + file], *logo = &f->oam[OAM_FILE_A_LOGO + file];
        marker->x = cursor_x(file);
        marker->y = cursor_y(file);
        mzm_menu_oam_set_id(marker, cursor_marker(file));
        logo->priority = 3;
        logo->oam_id = ID_METROID_LOGO;
        if (f->files[file].completed_game) logo->exists = 1;
        if (f->files[file].corrupted) logo->not_drawn = true;
        logo->x = BLOCK * 14;
    }
    f->oam[OAM_FILE_A_LOGO].y = BLOCK * 2;
    f->oam[OAM_FILE_B_LOGO].y = BLOCK * 3 + HALF_BLOCK;
    f->oam[OAM_FILE_C_LOGO].y = BLOCK * 5;
    f->oam[OAM_SMALL_PANEL].bound_background = 1;
    f->oam[OAM_SMALL_PANEL].priority = 2;
    f->oam[OAM_MEDIUM_PANEL].bound_background = 0;
    f->oam[OAM_MEDIUM_PANEL].priority = 1;
    for (int i = OAM_0; i <= OAM_2; ++i) {
        f->oam[i].bound_background = 0;
        f->oam[i].priority = 0;
    }
}

/* FileSelectProcessOam */
static void process_oam(MzmFileSelect *f, MzmScreen *s) {
    s->next_oam_slot = 0;
    mzm_menu_oam_process(s, OAM_COUNT, f->oam, sym("sFileSelectOam"));
    mzm_reset_free_oam(s);
}

/* FileSelectVBlank */
static void vblank(MzmFileSelect *f, MzmScreen *s) {
    GbaPpu *ppu = s->ppu;
    mzm_screen_copy_oam(s);
    for (int bg = 0; bg < 4; ++bg) {
        gba_ppu_write16(ppu, GBA_REG_BG0HOFS + 4u * (unsigned)bg, (uint16_t)(s->bg_hofs[bg] / 4));
        gba_ppu_write16(ppu, GBA_REG_BG0VOFS + 4u * (unsigned)bg, (uint16_t)(s->bg_vofs[bg] / 4));
    }
    gba_ppu_write16(ppu, GBA_REG_DISPCNT, f->dispcnt);
    gba_ppu_write16(ppu, GBA_REG_BLDY, s->bldy);
    gba_ppu_write16(ppu, GBA_REG_BLDALPHA, (uint16_t)(s->bldalpha_h << 8 | s->bldalpha_l));
    gba_ppu_write16(ppu, GBA_REG_BLDCNT, f->bldcnt);
    gba_ppu_write16(ppu, GBA_REG_BG0CNT, f->bg0cnt);
    gba_ppu_write16(ppu, GBA_REG_BG0CNT + 2, f->bg1cnt);
    gba_ppu_write16(ppu, GBA_REG_BG0CNT + 4, f->bg2cnt);
}

/* FileSelectInit (no corrupted file, the options not entered) */
static void init(MzmFileSelect *f, MzmScreen *s) {
    GbaPpu *ppu = s->ppu;
    MzmSaveFileInfo files[3];
    memcpy(files, f->files, sizeof files);
    uint8_t most_recent = f->most_recent;
    bool (*copy_file)(void *, int, int) = f->copy_file;
    bool (*erase_file)(void *, int) = f->erase_file_callback;
    void *context = f->context;
    uint32_t time_attack_unlocked = f->time_attack_unlocked;
    memset(f, 0, sizeof *f);
    memcpy(f->files, files, sizeof files);
    f->most_recent = most_recent;
    f->time_attack_unlocked = time_attack_unlocked;
    f->copy_file = copy_file;
    f->erase_file_callback = erase_file;
    f->context = context;
    f->copy_source = f->current_file = f->erase_file = most_recent;
    gba_ppu_write16(ppu, GBA_REG_DISPCNT, 0);
    s->bldy = 16;
    gba_ppu_write16(ppu, GBA_REG_BLDY, 16);
    f->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN;
    gba_ppu_write16(ppu, GBA_REG_BLDCNT, f->bldcnt);
    s->next_oam_slot = 0;
    mzm_clear_gfx_ram(s);
    mzm_reset_free_oam(s);
    s->oam_x_offset = s->oam_y_offset = 0;
    memset(s->ewram + 0x1000, 0, 0x800);
    f->unk_24 = f->cursor_position = most_recent;
    mzm_copy_from_rom(&ppu->palette[0], sym("sFileSelectPal"), 224);
    mzm_copy_from_rom(&ppu->palette[256 - 32], sym("sFileSelect_4548f8"), 64);
    mzm_copy_from_rom(&ppu->palette[256], sym("sFileSelectIconsPal"), 192);
    ppu->palette[0] = 0;
    vram_lz77(s, "sFileSelectCharactersGfx", 0x400);
    vram_lz77(s, "sFileSelectChozoBackgroundGfx", 0x8000);
    vram_lz77(s, "sFileSelectObjIconsGfx", GBA_OBJ_VRAM);
    /* The Japanese "Copy", "Erase" and "Options" are blanked, the English
     * options text replaces them. */
    fill(s, 0x400, 0x800, 0);
    vram_lz77(s, "sFileSelectOptionsTextEnglishGfx", 0xC00);
    vram_lz77(s, "sFileSelectChozoBackgroundTileTable", 0xF800);
    ewram_lz77(s, "sFileSelectMenuTileTable", 0x800);
    ewram_lz77(s, "sFileSelect3BigPanelsTileTable", 0x2800);
    ewram_lz77(s, "sFileSelect1Small2BigPanelsTileTable", 0x1800);
    ewram_lz77(s, "sFileSelect2Big1SmallPanelsTileTable", 0x2000);
    display_files(f, s);
    copy_ewram_to_vram(s, 0x800, 0xD800, 0x800);
    f->sub_mode1 = 2;
    mzm_sfx_music_play(MUSIC_FILE_SELECT, 2);
    f->current_sub_menu = SUB_MENU_MAIN;
    for (int bg = 0; bg < 4; ++bg) s->bg_hofs[bg] = s->bg_vofs[bg] = NON_GAMEPLAY_START_BG_POS;
    f->dispcnt = GBA_DCNT_BG1 | GBA_DCNT_BG3 | GBA_DCNT_OBJ;
    f->unk_14 = bgcnt(2, 31, 3);
    f->unk_16 = bgcnt(0, 30, 2);
    f->unk_18 = bgcnt(0, 27, 1);
    f->unk_1a = bgcnt(0, 27, 2);
    f->unk_1c = bgcnt(0, 29, 1);
    f->unk_1e = bgcnt(0, 28, 0);
    f->unk_20 = bgcnt(0, 30, 2);
    f->bg0cnt = 0;
    f->bg1cnt = f->unk_18;
    f->bg2cnt = f->unk_16;
    f->bg3cnt = f->unk_14;
    gba_ppu_write16(ppu, GBA_REG_BG0CNT, f->bg0cnt);
    gba_ppu_write16(ppu, GBA_REG_BG0CNT + 2, f->bg1cnt);
    gba_ppu_write16(ppu, GBA_REG_BG0CNT + 4, f->bg2cnt);
    gba_ppu_write16(ppu, GBA_REG_BG0CNT + 6, f->bg3cnt);
    /* sFileSelectMenuCursors_Empty */
    uint32_t cursors = sym("sFileSelectMenuCursors_Empty");
    uint8_t *c = &f->cursors.confirm_copy;
    for (int i = 0; i < 7; ++i) c[i] = mzm_rom_u8(cursors + (uint32_t)i);
    update_message_queue(f, 2, 0);
    reset_oam(f);
    update_cursor(f, CURSOR_DEFAULT, f->unk_24);
    process_oam(f, s);
    init_fading(f, s, false);
    s->bldy = 0;
    f->bldcnt = 0;
    gba_ppu_write16(ppu, GBA_REG_BLDY, 0);
    gba_ppu_write16(ppu, GBA_REG_BLDCNT, 0);
    vblank(f, s);
}

/* CheckForMaintainedInput */
static void maintained_input(MzmFileSelect *f, MzmScreen *s) {
    if (s->button_input & KEY_DIRECTIONS) {
        f->maintained.delay++;
    } else {
        f->maintained.delay = 0;
        f->maintained.set = 0;
    }
    if (f->maintained.delay >= mzm_rom_u8(sym("sMaintainedInputDelays_Fast") + f->maintained.set)) {
        s->changed_input |= s->button_input & KEY_DIRECTIONS;
        f->maintained.delay = 0;
        if (f->maintained.set < 6) f->maintained.set++;
    }
}

/* FileSelectApplyMenuSelectInput */
static bool menu_select_input(MzmFileSelect *f, MzmScreen *s, int set, uint8_t *file) {
    int direction;
    if (s->changed_input & KEY_UP) direction = -1;
    else if (s->changed_input & KEY_DOWN) direction = 1;
    else return false;
    const uint8_t files = 1 << POSITION_FILE_A | 1 << POSITION_FILE_B | 1 << POSITION_FILE_C;
    uint8_t flags = f->enabled_menus;
    if (set == SET_ALL_FILES) flags |= files;
    else if (set == SET_ONLY_FILES) flags &= files;
    else flags = (uint8_t)(files & ~(1 << f->copy_source));
    int position = *file;
    for (;;) {
        if ((unsigned)(position + direction) > POSITION_OPTIONS) return false;
        position += direction;
        if (flags >> position & 1) {
            *file = (uint8_t)position;
            return true;
        }
    }
}

/* FileSelectFindFirstNonEmptyFile */
static void find_first_non_empty_file(MzmFileSelect *f, uint8_t *file) {
    uint8_t flags = f->enabled_menus;
    if (flags >> *file & 1) return;
    for (uint8_t i = 0; i < 3; ++i)
        if (flags & 1 << i) {
            *file = i;
            return;
        }
}

static bool file_used(const MzmSaveFileInfo *file) { return file->exists || file->intro_played; }

/* FileSelectUpdateCopyCursor and FileSelectUpdateEraseCursor (alike) */
static void update_file_cursor(MzmFileSelect *f, int pose, int file) {
    int suit = file < POSITION_COPY ? f->files[file].suit : 0;
    uint8_t oam_id = cursor_oam_id(file, suit < 3 ? suit : 0);
    MzmMenuOam *cursor = &f->oam[OAM_SUB_CURSOR];
    switch (pose) {
    case SUB_CURSOR_DEFAULT:
        oam_id++;
        /* fall through */
    case SUB_CURSOR_MOVING:
        mzm_menu_oam_set_id(cursor, oam_id);
        cursor->x = cursor_x(file);
        cursor->y = cursor_y(file);
        cursor->bound_background = 2;
        for (int i = 0; i < 3; ++i)
            mzm_menu_oam_set_id(&f->oam[OAM_FILE_A_MARKER + i],
                                file == i && file_used(&f->files[i]) ? (uint8_t)(ID_FILE_A_MARKER_SELECTED_RED + i)
                                                                     : cursor_marker(i));
        break;
    case SUB_CURSOR_DONE:
        cursor->exists = 0;
        cursor->oam_id = 0;
        for (int i = 0; i < 3; ++i) mzm_menu_oam_set_id(&f->oam[OAM_FILE_A_MARKER + i], cursor_marker(i));
        break;
    case SUB_CURSOR_SELECTING_FILE:
        mzm_menu_oam_set_id(cursor, (uint8_t)(oam_id + 2));
        break;
    default:
        break;
    }
}

/* FileSelectUpdateCopyArrow */
static void update_copy_arrow(MzmFileSelect *f, int pose, int destination) {
    MzmMenuOam *arrow = &f->oam[OAM_COPY_ARROW];
    arrow->x = cursor_x(f->copy_source);
    arrow->y = cursor_y(f->copy_source);
    switch (pose) {
    case ARROW_DEFAULT:
    case ARROW_MOVING:
        arrow->bound_background = 2;
        mzm_menu_oam_set_id(arrow, mzm_rom_u8(sym("sFileSelectCopyFileArrowsOamIds") + f->copy_source * 3u +
                                              (uint32_t)destination));
        for (int i = 0; i < 3; ++i) {
            if (f->copy_source == i) continue;
            mzm_menu_oam_set_id(&f->oam[OAM_FILE_A_MARKER + i],
                                destination != i ? cursor_marker(i) : (uint8_t)(ID_FILE_A_MARKER_SELECTED_GREEN + i));
        }
        break;
    case ARROW_KILL:
        arrow->exists = 0;
        if (destination >= 0 && destination < 3)
            mzm_menu_oam_set_id(&f->oam[OAM_FILE_A_MARKER + destination], cursor_marker(destination));
        break;
    default:
        break;
    }
}

/* The file's info redrawn after a copy or an erase. */
static void redraw_file(MzmFileSelect *f, MzmScreen *s, int file) {
    display_health(f, s, file);
    display_timer(f, s, file);
    display_misc(f, s, file);
}

/* The yes/no inputs of the erase and copy confirmations: true when A was
 * pressed on YES; `cancel` is set by A on NO or B. */
static uint8_t yes_no_input(MzmScreen *s, uint8_t *choice, bool *cancel, bool *confirm) {
    uint8_t action = 0xFF;
    *cancel = *confirm = false;
    if (!s->changed_input) return action;
    if (s->changed_input & KEY_A) {
        if (*choice) *cancel = true;
        else *confirm = true;
    } else if (s->changed_input & KEY_B) {
        *cancel = true;
    } else if (s->changed_input & KEY_LEFT) {
        if (*choice) {
            action = 0;
            *choice = 0;
            play_menu_sound(SOUND_REQUEST_SUB_MENU_CURSOR);
        }
    } else if (s->changed_input & KEY_RIGHT) {
        if (!*choice) {
            action = 1;
            *choice = 1;
            play_menu_sound(SOUND_REQUEST_SUB_MENU_CURSOR);
        }
    }
    return action;
}

/* FileSelectEraseFileHandler */
static bool erase_file_handler(MzmFileSelect *f, MzmScreen *s) {
    int action;
    bool cancel, confirm;
    f->sub_menu_timer++;
    switch (f->sub_menu_stage) {
    case 0:
        update_message_queue(f, 0, MESSAGE_ERASE_CHOOSE);
        update_message_queue(f, 0, MESSAGE_ERASE_CONFIRM);
        find_first_non_empty_file(f, &f->erase_file);
        update_file_cursor(f, SUB_CURSOR_DEFAULT, f->erase_file);
        update_tilemap(f, s, TILEMAP_ERASE_SPAWN_INIT);
        f->unk_3a = 0;
        f->sub_menu_stage++;
        break;
    case 1:
        if (update_tilemap(f, s, TILEMAP_ERASE_SPAWN)) f->sub_menu_stage++;
        break;
    case 2:
        /* The file to erase */
        action = 0;
        if (s->changed_input) {
            if (menu_select_input(f, s, SET_ONLY_FILES, &f->erase_file)) {
                action = 1;
            } else if (s->changed_input & KEY_B) {
                f->sub_menu_stage = 10;
                break;
            } else if (s->changed_input & KEY_A) {
                action = f->enabled_menus >> f->erase_file & 1 ? 2 : 0;
            }
        }
        if (action == 1) {
            play_menu_sound(SOUND_REQUEST_CURSOR);
            update_file_cursor(f, SUB_CURSOR_MOVING, f->erase_file);
        } else if (action == 2) {
            mzm_sfx_play(SOUND_SELECTING_FILE_TO_DELETE);
            update_file_cursor(f, SUB_CURSOR_SELECTING_FILE, f->erase_file);
            update_tilemap(f, s, TILEMAP_ERASE_YES_NO_SPAWN_INIT);
            f->sub_menu_stage = 3;
        }
        break;
    case 3:
        if (update_tilemap(f, s, TILEMAP_ERASE_YES_NO_SPAWN)) {
            f->cursors.confirm_erase = mzm_rom_u8(sym("sFileSelectMenuCursors_Empty") + 1);
            sub_cursor(f, 3, f->cursors.confirm_erase);
            f->sub_menu_stage++;
        }
        break;
    case 4: {
        /* Erase it? */
        uint8_t moved = yes_no_input(s, &f->cursors.confirm_erase, &cancel, &confirm);
        if (cancel) {
            f->sub_menu_stage = 5;
        } else if (confirm) {
            mzm_sfx_play(SOUND_FILE_DELETE);
            sub_cursor(f, 3, 0x80);
            f->sub_menu_stage = 7;
        } else if (moved != 0xFF) {
            sub_cursor(f, 3, moved);
        }
        break;
    }
    case 5:
        mzm_sfx_play(SOUND_REFUSE_MENU);
        sub_cursor(f, 3, 0x81);
        update_tilemap(f, s, TILEMAP_ERASE_YES_NO_DESPAWN_INIT);
        f->sub_menu_stage++;
        break;
    case 6:
        if (update_tilemap(f, s, TILEMAP_ERASE_YES_NO_DESPAWN)) {
            update_file_cursor(f, SUB_CURSOR_DEFAULT, f->erase_file);
            f->sub_menu_stage = 2;
        }
        break;
    case 7:
        /* SramDeleteFile */
        if (!f->erase_file_callback || f->erase_file_callback(f->context, f->erase_file)) {
            memset(&f->files[f->erase_file], 0, sizeof f->files[f->erase_file]);
            f->files[f->erase_file].difficulty = 1;
            f->most_recent = f->erase_file;
        }
        f->unk_3a = 1;
        f->sub_menu_stage = 8;
        break;
    case 8:
        sub_cursor(f, 3, 0x81);
        update_tilemap(f, s, TILEMAP_ERASE_YES_NO_DESPAWN_INIT);
        f->sub_menu_stage++;
        break;
    case 9:
        if (update_tilemap(f, s, TILEMAP_ERASE_YES_NO_DESPAWN)) f->sub_menu_stage = 10;
        break;
    case 10:
        if (f->unk_3a) {
            redraw_file(f, s, f->erase_file);
            if (!f->files[f->erase_file].completed_game) f->oam[OAM_FILE_A_LOGO + f->erase_file].exists = 0;
            set_enabled_menus(f, s);
            copy_ewram_to_vram(s, 0x800, 0xD800, 0x800);
        }
        update_file_cursor(f, SUB_CURSOR_DONE, f->erase_file);
        update_tilemap(f, s, TILEMAP_ERASE_DESPAWN_INIT);
        f->sub_menu_stage++;
        break;
    case 11:
        if (update_tilemap(f, s, TILEMAP_ERASE_DESPAWN)) {
            update_message_queue(f, 2, 0xFF);
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

/* FileSelectFileCopyChooseBaseDestinationFile */
static void choose_copy_destination(MzmFileSelect *f) {
    int file = -1;
    for (int i = 0; i < 3 && file < 0; ++i)
        if (!file_used(&f->files[i])) file = i;
    if (file < 0) {
        file = f->current_file;
        if (file == 0x80 || f->copy_source == file)
            file = f->copy_source == POSITION_FILE_B ? POSITION_FILE_A : POSITION_FILE_B;
    }
    f->current_file = (uint8_t)file;
}

/* FileSelectCopyFileHandler */
static bool copy_file_handler(MzmFileSelect *f, MzmScreen *s) {
    int action;
    bool cancel, confirm;
    f->sub_menu_timer++;
    switch (f->sub_menu_stage) {
    case 0:
        update_message_queue(f, 0, MESSAGE_COPY_SOURCE_CHOOSE);
        update_message_queue(f, 0, MESSAGE_COPY_DESTINATION_CHOOSE);
        find_first_non_empty_file(f, &f->copy_source);
        update_file_cursor(f, SUB_CURSOR_DEFAULT, f->copy_source);
        update_tilemap(f, s, TILEMAP_COPY_SPAWN_INIT);
        f->cursors.confirm_copy = mzm_rom_u8(sym("sFileSelectMenuCursors_Empty"));
        f->unk_3a = 0;
        f->sub_menu_stage++;
        break;
    case 1:
        if (update_tilemap(f, s, TILEMAP_COPY_SPAWN)) f->sub_menu_stage++;
        break;
    case 2:
        if (update_message_queue(f, 1, MESSAGE_COPY_DESTINATION_CHOOSE)) f->sub_menu_stage++;
        break;
    case 3:
        /* The source */
        action = 0;
        if (s->changed_input) {
            if (menu_select_input(f, s, SET_ONLY_FILES, &f->copy_source)) {
                action = 1;
            } else if (s->changed_input & KEY_B) {
                f->sub_menu_stage = 14;
                break;
            } else if (s->changed_input & KEY_A) {
                action = f->enabled_menus >> f->copy_source & 1 ? 2 : 0;
            }
        }
        if (action == 1) {
            play_menu_sound(SOUND_REQUEST_CURSOR);
            update_file_cursor(f, SUB_CURSOR_MOVING, f->copy_source);
        } else if (action == 2) {
            update_file_cursor(f, SUB_CURSOR_SELECTING_FILE, f->copy_source);
            if (f->files[f->copy_source].time_attack) {
                mzm_sfx_play(SOUND_REFUSE_MENU);
                update_message_queue(f, 0, MESSAGE_CANT_COPY_TIME_ATTACK);
                f->sub_menu_stage = 9;
            } else {
                play_menu_sound(SOUND_REQUEST_COPY_DELETE);
                choose_copy_destination(f);
                update_copy_arrow(f, ARROW_DEFAULT, f->current_file);
                update_tilemap(f, s, TILEMAP_COPY_DESTINATION_SPAWN);
                f->sub_menu_stage++;
            }
        }
        break;
    case 4:
        /* The destination */
        if (!s->changed_input) break;
        action = 0;
        if (menu_select_input(f, s, SET_ALL_FILES_NO_COPY, &f->current_file)) {
            action = 1;
        } else if (s->changed_input & KEY_B) {
            mzm_sfx_play(SOUND_REFUSE_MENU);
            update_file_cursor(f, SUB_CURSOR_DEFAULT, f->copy_source);
            update_copy_arrow(f, ARROW_KILL, f->current_file);
            update_tilemap(f, s, TILEMAP_COPY_DESTINATION_DESPAWN);
            f->sub_menu_stage = 3;
            break;
        } else if (s->changed_input & KEY_A) {
            action = 2;
        }
        if (action == 1) {
            update_copy_arrow(f, ARROW_MOVING, f->current_file);
            play_menu_sound(SOUND_REQUEST_COPY_DELETE_MOVING);
        } else if (action == 2) {
            f->cursors.confirm_copy = mzm_rom_u8(sym("sFileSelectMenuCursors_Empty"));
            update_copy_arrow(f, ARROW_COPYING, f->current_file);
            if (f->enabled_menus >> f->current_file & 1) {
                update_message_queue(f, 0, MESSAGE_COPY_CONFIRM);
                update_tilemap(f, s, TILEMAP_COPY_OVERRIDE_SPAWN_INIT);
                play_menu_sound(SOUND_REQUEST_COPY_DELETE);
                f->sub_menu_stage = 5;
            } else {
                play_menu_sound(SOUND_REQUEST_COPY_CONFIRM);
                f->sub_menu_stage = 11;
            }
        }
        break;
    case 5:
        if (update_tilemap(f, s, TILEMAP_COPY_OVERRIDE_SPAWN)) {
            sub_cursor(f, 4, f->cursors.confirm_copy);
            f->sub_menu_stage++;
        }
        break;
    case 6: {
        /* Overwrite the destination? */
        uint8_t moved = yes_no_input(s, &f->cursors.confirm_copy, &cancel, &confirm);
        if (cancel) {
            f->sub_menu_stage = 7;
        } else if (confirm) {
            play_menu_sound(SOUND_REQUEST_COPY_CONFIRM);
            sub_cursor(f, 4, 0x80);
            f->sub_menu_stage = 11;
        } else if (moved != 0xFF) {
            sub_cursor(f, 4, moved);
        }
        break;
    }
    case 7:
        mzm_sfx_play(SOUND_REFUSE_MENU);
        sub_cursor(f, 4, 0x81);
        update_tilemap(f, s, TILEMAP_COPY_OVERRIDE_DESPAWN_INIT);
        f->sub_menu_stage++;
        break;
    case 8:
        if (update_tilemap(f, s, TILEMAP_COPY_OVERRIDE_DESPAWN)) {
            update_copy_arrow(f, ARROW_DEFAULT, f->current_file);
            f->sub_menu_stage = 4;
        }
        break;
    case 9:
        if (update_message_queue(f, 1, MESSAGE_CANT_COPY_TIME_ATTACK)) {
            update_tilemap(f, s, TILEMAP_28);
            f->sub_menu_stage++;
        }
        break;
    case 10:
        if (s->changed_input & (KEY_A | KEY_B)) {
            update_file_cursor(f, SUB_CURSOR_DEFAULT, f->copy_source);
            update_tilemap(f, s, TILEMAP_COPY_DESTINATION_DESPAWN);
            f->sub_menu_stage = 3;
        }
        break;
    case 11:
        /* SramCopyFile */
        if (!f->copy_file || f->copy_file(f->context, f->copy_source, f->current_file))
            f->files[f->current_file] = f->files[f->copy_source];
        f->unk_3a = 1;
        f->sub_menu_stage = f->cursors.confirm_copy ? 14 : 12;
        break;
    case 12:
        sub_cursor(f, 4, 0x81);
        update_tilemap(f, s, TILEMAP_COPY_OVERRIDE_DESPAWN_INIT);
        f->sub_menu_stage++;
        break;
    case 13:
        if (update_tilemap(f, s, TILEMAP_COPY_OVERRIDE_DESPAWN)) f->sub_menu_stage = 14;
        break;
    case 14:
        if (f->unk_3a) {
            redraw_file(f, s, f->current_file);
            f->oam[OAM_FILE_A_LOGO + f->current_file].exists = f->files[f->current_file].completed_game ? 2 : 0;
            set_enabled_menus(f, s);
            copy_ewram_to_vram(s, 0x800, 0xD800, 0x800);
        }
        update_file_cursor(f, SUB_CURSOR_DONE, f->copy_source);
        update_copy_arrow(f, ARROW_KILL, f->current_file);
        update_tilemap(f, s, TILEMAP_COPY_DESPAWN_INIT);
        f->sub_menu_stage++;
        break;
    case 15:
        if (update_tilemap(f, s, TILEMAP_COPY_DESPAWN)) {
            update_message_queue(f, 2, 0xFF);
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}


static void fade_bldalpha(MzmScreen *s, int target_l, int target_h, bool *done) {
    *done = true;
    if (s->bldalpha_l != target_l) {
        *done = false;
        s->bldalpha_l = (uint8_t)(target_l < s->bldalpha_l ? (s->bldalpha_l - 2 < target_l ? target_l : s->bldalpha_l - 2)
                                                           : (s->bldalpha_l + 2 > target_l ? target_l : s->bldalpha_l + 2));
    }
    if (s->bldalpha_h != target_h) {
        *done = false;
        s->bldalpha_h = (uint8_t)(target_h < s->bldalpha_h ? (s->bldalpha_h - 2 < target_h ? target_h : s->bldalpha_h - 2)
                                                           : (s->bldalpha_h + 2 > target_h ? target_h : s->bldalpha_h + 2));
    }
}

/* FileSelectCheckInputtingTimeAttackCode: sTimeAttackButtonCode (UP, LEFT,
 * RIGHT, DOWN) pressed one key at a time; a wrong key restarts it after half
 * a second. */
static bool check_time_attack_code(MzmFileSelect *f, MzmScreen *s) {
    static const uint16_t code[] = {KEY_UP, KEY_LEFT, KEY_RIGHT, KEY_DOWN, 0xFFFF};
    uint16_t input = s->changed_input;
    if (!f->inputting_time_attack) {
        if (f->time_attack_cooldown != 0) {
            f->time_attack_cooldown--;
        } else if (input == code[0]) {
            f->inputting_time_attack = true;
            f->time_attack_inputs = 1;
        } else if (input) {
            f->time_attack_cooldown = 30;                   /* CONVERT_SECONDS(.5f) */
        }
    } else if (input == code[f->time_attack_inputs]) {
        f->time_attack_inputs++;
        if (code[f->time_attack_inputs] == 0xFFFF) return true;
    } else if (input) {
        f->inputting_time_attack = false;
        f->time_attack_inputs = 0;
        f->time_attack_cooldown = 30;
    }
    return false;
}

/* FileSelectProcessFileSelection (US: no Japanese text menu); returns 1
 * when back on the files, 2 when the game starts. */
static int process_file_selection(MzmFileSelect *f, MzmScreen *s) {
    const MzmSaveFileInfo *file = &f->files[f->cursor_position < 3 ? f->cursor_position : 0];
    bool done;
    int action;
    f->sub_menu_timer++;
    switch (f->sub_menu_stage) {
    case 0: {
        f->most_recent = f->cursor_position;
        uint32_t offset = (f->cursor_position + 1u) * 3u;
        memset(s->ewram + 0x10C0, 0, 0x240);
        memcpy(s->ewram + 0x1000 + offset * 0x40, s->ewram + 0x800 + offset * 0x40, 0xC0);
        copy_ewram_to_vram(s, 0x1000, 0xF000, 0x800);
        f->dispcnt |= GBA_DCNT_BG2 | GBA_DCNT_WIN0;
        gba_ppu_write16(s->ppu, GBA_REG_WIN0H, 70 << 8 | 170);
        gba_ppu_write16(s->ppu, GBA_REG_WIN0V, 0 << 8 | 23);
        gba_ppu_write16(s->ppu, GBA_REG_WINOUT, 0x3F);
        gba_ppu_write16(s->ppu, GBA_REG_WININ, 0x1F);
        s->bldalpha_h = 0;
        s->bldalpha_l = 16;
        f->bldcnt = BLDCNT_BG1_FIRST | BLDCNT_ALPHA | BLDCNT_SCREEN_SECOND;
        f->unk_3a = 0;
        uint32_t cursors = sym("sFileSelectMenuCursors_Empty");
        uint8_t *c = &f->cursors.confirm_copy;
        for (int i = 0; i < 7; ++i) c[i] = mzm_rom_u8(cursors + (uint32_t)i);
        for (int i = 0; i < 3; ++i)
            if (f->cursor_position != i) f->oam[OAM_FILE_A_LOGO + i].not_drawn = true;
        f->sub_menu_stage++;
        break;
    }
    case 1:
        fade_bldalpha(s, 0, 16, &done);
        if (!done) break;
        for (int i = 0; i < 3; ++i)
            if (f->cursor_position != i) f->oam[OAM_FILE_A_MARKER + i].not_drawn = true;
        f->sub_menu_stage++;
        break;
    case 2:
        action = f->cursor_position * 0x60 + 0x800;
        if (action != s->bg_vofs[2]) {
            s->bg_vofs[2] = (uint16_t)(s->bg_vofs[2] + 12 > action ? action : s->bg_vofs[2] + 12);
            break;
        }
        f->sub_menu_stage++;
        break;
    case 3:
        copy_ewram_to_vram(s, 0x800, 0xF000u + f->cursor_position * 0xC0u, 0xC0);
        f->dispcnt &= (uint16_t)~GBA_DCNT_WIN0;
        update_tilemap(f, s, TILEMAP_START_GAME_INIT);
        update_message_queue(f, 0, MESSAGE_START_GAME);
        f->sub_menu_stage = 4;
        break;
    case 4:
        if (!update_tilemap(f, s, TILEMAP_START_GAME)) break;
        /* fall through */
    case 5:
        sub_cursor(f, 0, f->cursors.start_game);
        f->sub_menu_stage = 6;
        break;
    case 6:
        f->inputting_time_attack = false;
        f->time_attack_inputs = 0;
        f->time_attack_cooldown = 0;
        f->inputted_time_attack = false;
        f->sub_menu_stage = 7;
        /* fall through */
    case 7:
        /* "Start game": A, B back, or the Time Attack code on a completed
         * file */
        action = 0xFF;
        if (s->changed_input & KEY_A) {
            action = 0x80;
            play_menu_sound(SOUND_REQUEST_ACCEPT_CONFIRM_MENU);
            if (!file->completed_game && (file->exists || file->intro_played)) {
                f->unk_3a = 0;
                f->sub_menu_stage = 34;
            } else {
                f->sub_menu_stage = 8;
            }
        } else if (s->changed_input & KEY_B) {
            action = 0x81;
            f->sub_menu_stage = 39;
        } else if (f->time_attack_unlocked & 1 && file->completed_game && check_time_attack_code(f, s)) {
            action = 0x80;
            f->cursors.completed_file_options = 2;
            f->inputted_time_attack = true;
            f->sub_menu_stage = 8;
        }
        if (action != 0xFF) sub_cursor(f, 0, (uint8_t)action);
        break;
    case 8:
        if (file->exists || file->intro_played) {
            f->continue_new_message = file->time_attack || f->inputted_time_attack
                                          ? MESSAGE_CONTINUE_NEW_TIME_ATTACK : MESSAGE_CONTINUE_NEW;
            f->sub_menu_stage = 9;
        } else {
            f->sub_menu_stage = 21;
        }
        f->sub_menu_timer = 0;
        break;
    case 9:
        update_message_queue(f, 0, f->continue_new_message);
        update_message_queue(f, 0, MESSAGE_ERASE_FILE_CONFIRM);
        update_tilemap(f, s, TILEMAP_29);
        f->sub_menu_stage++;
        break;
    case 10:
        if (update_tilemap(f, s, TILEMAP_30)) {
            sub_cursor(f, 5, f->cursors.completed_file_options);
            f->sub_menu_stage++;
        }
        break;
    case 11:
        /* Continue / new game (/ time attack) on a completed file */
        action = 0xFF;
        if (s->changed_input & KEY_A) {
            action = 0x80;
            f->sub_menu_timer = 0;
            play_menu_sound(SOUND_REQUEST_ACCEPT_CONFIRM_MENU);
            f->sub_menu_stage = 13;
        } else if (s->changed_input & KEY_B) {
            update_tilemap(f, s, TILEMAP_31);
            action = 0x81;
            f->sub_menu_timer = 0;
            f->sub_menu_stage = 12;
        } else if (s->changed_input & KEY_UP) {
            if (f->cursors.completed_file_options != 0) {
                action = --f->cursors.completed_file_options;
                play_menu_sound(SOUND_REQUEST_SUB_MENU_CURSOR);
            }
        } else if (s->changed_input & KEY_DOWN) {
            uint8_t last = f->continue_new_message == MESSAGE_CONTINUE_NEW_TIME_ATTACK ? 2 : 1;
            if (f->cursors.completed_file_options < last) {
                action = ++f->cursors.completed_file_options;
                play_menu_sound(SOUND_REQUEST_SUB_MENU_CURSOR);
            }
        }
        if (action != 0xFF) sub_cursor(f, 5, (uint8_t)action);
        break;
    case 12:
        if (update_tilemap(f, s, TILEMAP_32)) {
            if (f->cursors.completed_file_options == 2) f->cursors.completed_file_options = 0;
            f->sub_menu_stage = 5;
        }
        break;
    case 13:
        f->sub_menu_timer = 0;
        if (f->cursors.completed_file_options == 0) {
            f->unk_3a = 1;
            f->sub_menu_stage = 34;
        } else {
            f->sub_menu_stage = 14;
        }
        break;
    case 14:
        if (f->sub_menu_timer > 10) f->sub_menu_stage = 15;
        break;
    case 15:
        f->sub_menu_stage++;
        break;
    case 16:
        if (update_tilemap(f, s, TILEMAP_34)) {
            f->cursors.confirm_overwriting_completed = mzm_rom_u8(sym("sFileSelectMenuCursors_Empty") + 4);
            sub_cursor(f, 6, f->cursors.confirm_overwriting_completed);
            f->sub_menu_stage++;
        }
        break;
    case 17:
        /* Overwrite the completed file? */
        action = 0xFF;
        if (s->changed_input & KEY_A) {
            f->sub_menu_timer = 0;
            action = 0x80;
            if (f->cursors.confirm_overwriting_completed) mzm_sfx_play(SOUND_REFUSE_MENU);
            else play_menu_sound(SOUND_REQUEST_ACCEPT_CONFIRM_MENU);
            f->sub_menu_stage = 18;
        } else if (s->changed_input & KEY_B) {
            f->sub_menu_timer = 0;
            action = 0x81;
            mzm_sfx_play(SOUND_REFUSE_MENU);
            f->sub_menu_stage = 19;
        } else if (s->changed_input & KEY_LEFT) {
            if (f->cursors.confirm_overwriting_completed) {
                action = 0;
                f->cursors.confirm_overwriting_completed = 0;
                play_menu_sound(SOUND_REQUEST_SUB_MENU_CURSOR);
            }
        } else if (s->changed_input & KEY_RIGHT) {
            if (!f->cursors.confirm_overwriting_completed) {
                action = 1;
                f->cursors.confirm_overwriting_completed = 1;
                play_menu_sound(SOUND_REQUEST_SUB_MENU_CURSOR);
            }
        }
        if (action != 0xFF) sub_cursor(f, 6, (uint8_t)action);
        break;
    case 18:
        if (f->sub_menu_timer <= 10) break;
        if (f->cursors.confirm_overwriting_completed) {
            f->sub_menu_stage = 19;
            break;
        }
        if (f->cursors.completed_file_options != 2) {
            sub_cursor(f, 6, 0x81);
            update_tilemap(f, s, TILEMAP_35);
            f->sub_menu_stage = 20;
        } else {
            f->unk_3a = 2;
            f->sub_menu_stage = 34;
        }
        break;
    case 19:
        update_tilemap(f, s, TILEMAP_37);
        sub_cursor(f, 5, f->cursors.completed_file_options);
        f->sub_menu_stage = 11;
        break;
    case 20:
        if (update_tilemap(f, s, TILEMAP_36)) f->sub_menu_stage = 21;
        break;
    case 21:
        f->sub_menu_stage = 28;
        if (f->cursors.completed_file_options == 2) {
            f->cursors.difficulty = 1;
        } else {
            f->difficulty_message = file->completed_game & 0x36 ? MESSAGE_DIFFICULTY_HARD : MESSAGE_DIFFICULTY;
            update_message_queue(f, 0, f->difficulty_message);
        }
        break;
    case 28:
        update_tilemap(f, s, TILEMAP_DIFFICULTY_SPAWN_INIT);
        f->sub_menu_stage++;
        break;
    case 29:
        if (update_tilemap(f, s, TILEMAP_DIFFICULTY_SPAWN)) {
            sub_cursor(f, 2, f->cursors.difficulty);
            f->sub_menu_stage++;
        }
        break;
    case 30:
        /* The difficulty (hard once unlocked) */
        action = 0xFF;
        if (s->changed_input & KEY_A) {
            action = 0x80;
            f->sub_menu_timer = 0;
            f->unk_3a = 4;
            f->sub_menu_stage = 34;
        } else if (s->changed_input & KEY_B) {
            update_tilemap(f, s, TILEMAP_DIFFICULTY_DESPAWN_INIT);
            action = 0x81;
            f->sub_menu_timer = 0;
            f->sub_menu_stage = 31;
        } else if (s->changed_input & KEY_UP) {
            if (f->cursors.difficulty != 0) {
                action = --f->cursors.difficulty;
                play_menu_sound(SOUND_REQUEST_SUB_MENU_CURSOR);
            }
        } else if (s->changed_input & KEY_DOWN) {
            uint8_t last = f->difficulty_message == MESSAGE_DIFFICULTY_HARD ? 2 : 1;
            if (f->cursors.difficulty < last) {
                action = ++f->cursors.difficulty;
                play_menu_sound(SOUND_REQUEST_SUB_MENU_CURSOR);
            }
        }
        if (action != 0xFF) sub_cursor(f, 2, (uint8_t)action);
        break;
    case 31:
        if (update_tilemap(f, s, TILEMAP_DIFFICULTY_DESPAWN)) f->sub_menu_stage = file->exists ? 8 : 6;
        break;
    case 34:
        update_cursor(f, CURSOR_STARTING_GAME, f->cursor_position);
        play_menu_sound(SOUND_REQUEST_START_GAME);
        f->sub_menu_stage++;
        break;
    case 35:
        if (s->changed_input & KEY_B) {
            f->sub_menu_stage = 37;
            break;
        }
        if (f->oam[OAM_CURSOR].ended) {
            f->sub_menu_timer = 0;
            f->sub_menu_stage++;
        }
        break;
    case 36:
        if (s->changed_input & KEY_B) {
            f->sub_menu_stage = 37;
            break;
        }
        if (f->sub_menu_timer > 16) return 2;
        break;
    case 37:
        update_cursor(f, CURSOR_CANCEL_START, f->cursor_position);
        play_menu_sound(SOUND_REQUEST_CLOSE_SUB_MENU2);
        f->sub_menu_stage++;
        break;
    case 38:
        if (f->oam[OAM_CURSOR].oam_id != cursor_oam_id(f->cursor_position, file->suit < 3 ? file->suit : 0) + 3)
            break;
        switch (f->unk_3a) {
        case 0: f->sub_menu_stage = 5; break;
        case 1:
            sub_cursor(f, 5, f->cursors.completed_file_options);
            f->sub_menu_stage = 11;
            break;
        case 2:
            sub_cursor(f, 6, f->cursors.confirm_overwriting_completed);
            f->sub_menu_stage = 17;
            break;
        case 4:
            sub_cursor(f, 2, f->cursors.difficulty);
            f->sub_menu_stage = 30;
            break;
        default:
            break;
        }
        break;
    case 39:
        f->bldcnt = BLDCNT_BG1_FIRST | BLDCNT_ALPHA | BLDCNT_SCREEN_SECOND;
        f->oam[OAM_SMALL_PANEL].oam_id = ID_SMALL_PANEL_CLOSE;
        play_menu_sound(SOUND_REQUEST_CLOSE_SUB_MENU);
        f->sub_menu_stage++;
        break;
    case 40:
        if (f->oam[OAM_SMALL_PANEL].oam_id == 0) {
            f->dispcnt |= GBA_DCNT_WIN0;
            f->bg1cnt = f->unk_18;
            s->bg_hofs[1] = BLOCK * 32;
            s->bg_vofs[1] = BLOCK * 32;
            f->sub_menu_stage++;
        }
        break;
    case 41:
        fill(s, 0xF000u + f->cursor_position * 0xC0u, 0xC0, 0);
        f->sub_menu_stage++;
        /* fall through */
    case 42:
        action = BLOCK * 32;
        if (action != s->bg_vofs[2]) {
            s->bg_vofs[2] = (uint16_t)(s->bg_vofs[2] - 12 < action ? action : s->bg_vofs[2] - 12);
            break;
        }
        for (int i = 0; i < 3; ++i) f->oam[OAM_FILE_A_MARKER + i].not_drawn = false;
        f->sub_menu_stage++;
        break;
    case 43:
        fade_bldalpha(s, 16, 0, &done);
        if (!done) break;
        f->dispcnt &= (uint16_t)~GBA_DCNT_BG2;
        for (int i = 0; i < 3; ++i) f->oam[OAM_FILE_A_LOGO + i].not_drawn = false;
        f->sub_menu_stage++;
        break;
    case 44:
        f->dispcnt &= (uint16_t)~GBA_DCNT_WIN0;
        return 1;
    default:
        break;
    }
    return 0;
}

/* FileSelectUpdateSubMenu (the main menu and a file's panels) */
static bool update_sub_menu(MzmFileSelect *f, MzmScreen *s) {
    switch (f->current_sub_menu) {
    case SUB_MENU_MAIN: {
        int result = 0;
        maintained_input(f, s);
        if (s->changed_input) {
            if (menu_select_input(f, s, SET_ALL_FILES, &f->cursor_position)) result = 1;
            else if (s->changed_input & KEY_B) result = 2;
            else if (s->changed_input & KEY_A) result = 3;
        }
        if (result == 1) {
            update_cursor(f, CURSOR_MOVING, f->cursor_position);
            play_menu_sound(f->cursor_position < POSITION_COPY ? SOUND_REQUEST_CURSOR
                                                               : SOUND_REQUEST_SUB_MENU_CURSOR);
        } else if (result == 2) {
            mzm_sfx_music_fade(0);
            f->sub_mode2 = 3;
            return true;
        } else if (result == 3) {
            int pose = -1;
            if (f->cursor_position < POSITION_COPY) {
                play_menu_sound(SOUND_REQUEST_FILE_SELECT);
                pose = CURSOR_SELECTING_FILE;
                f->current_sub_menu = SUB_MENU_FILE;
            } else if (f->cursor_position != POSITION_OPTIONS && f->enabled_menus >> f->cursor_position & 1) {
                /* Copy or erase (the options are not ported) */
                play_menu_sound(SOUND_REQUEST_ACCEPT_CONFIRM_MENU);
                pose = CURSOR_OPENING_OPTIONS;
                f->current_sub_menu = f->cursor_position == POSITION_COPY ? SUB_MENU_COPY : SUB_MENU_ERASE;
            }
            if (pose >= 0) {
                update_cursor(f, pose, f->cursor_position);
                f->sub_menu_stage = 0;
                f->sub_menu_timer = 0;
            }
        }
        break;
    }
    case SUB_MENU_FILE: {
        int result = process_file_selection(f, s);
        if (result == 0) break;
        if (result == 2) {
            MzmSaveFileInfo *file = &f->files[f->most_recent];
            if (f->unk_3a >= 2 && f->unk_3a < 5) {
                file->exists = false;
                file->difficulty = f->cursors.difficulty;
                file->time_attack = f->cursors.completed_file_options == 2;
            }
            f->sub_mode2 = file->exists ? 1 : 2;
            mzm_sfx_music_fade(20);
            return true;
        }
        update_cursor(f, CURSOR_DESELECTING_FILE, f->cursor_position);
        f->current_sub_menu = SUB_MENU_MAIN;
        break;
    }
    case SUB_MENU_COPY:
    case SUB_MENU_ERASE:
        if (f->current_sub_menu == SUB_MENU_COPY ? copy_file_handler(f, s) : erase_file_handler(f, s)) {
            update_cursor(f, CURSOR_DEFAULT, f->cursor_position);
            f->current_sub_menu = SUB_MENU_MAIN;
        }
        break;
    default:
        break;
    }
    process_text(f, s);
    return false;
}

void mzm_file_select_enter(MzmFileSelect *f, const MzmSaveFileInfo files[3], uint8_t most_recent) {
    bool (*copy_file)(void *, int, int) = f->copy_file;
    bool (*erase_file)(void *, int) = f->erase_file_callback;
    void *context = f->context;
    memset(f, 0, sizeof *f);
    f->copy_file = copy_file;
    f->erase_file_callback = erase_file;
    f->context = context;
    memcpy(f->files, files, sizeof f->files);
    f->most_recent = most_recent < 3 ? most_recent : 0;
}

int mzm_file_select_update(MzmFileSelect *f, MzmScreen *s) {
    /* FileSelectMenuHandler */
    f->timer++;
    int result = MZM_FILE_SELECT_STAY;
    switch (f->sub_mode1) {
    case 0:
        f->sub_mode2 = 0;
        init(f, s);
        f->sub_mode1--;
        break;
    case 1:
        apply_fading(f, s);
        if (update_fading(f, s)) {
            f->sub_mode1++;
            f->sub_mode2 = 0;
        }
        break;
    case 2:
        if (update_sub_menu(f, s)) {
            f->timer = 0;
            if (f->sub_mode2 == 1) f->sub_mode1 = 7;
            else if (f->sub_mode2 == 2) f->sub_mode1 = 4;
            else f->sub_mode1 = 3;
            init_fading(f, s, true);
        }
        break;
    case 3:
        apply_fading(f, s);
        if (update_fading(f, s)) result = MZM_FILE_SELECT_TITLE;
        break;
    case 4:
    case 7:
        apply_fading(f, s);
        if (update_fading(f, s)) {
            f->timer = 0;
            f->sub_mode1++;
        }
        break;
    case 5:
        if (f->timer > 30) f->sub_mode1++;
        break;
    case 6:
        result = MZM_FILE_SELECT_NEW_GAME;
        break;
    case 8:
        result = MZM_FILE_SELECT_CONTINUE;
        break;
    default:
        break;
    }
    if (result != MZM_FILE_SELECT_STAY) {
        f->sub_mode1 = 0;
        return result;
    }
    process_oam(f, s);
    vblank(f, s);
    return result;
}
