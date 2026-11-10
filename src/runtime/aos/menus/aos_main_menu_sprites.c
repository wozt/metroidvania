/* SPDX-License-Identifier: GPL-3.0-only */
/* The main menu's objects: see aos_main_menu_sprites.h. */
#include "aos_main_menu_sprites.h"

#include <string.h>

/* sUnk_080E0CC0 (the English rows; the other languages use the same
 * rows with their own sheets). */
enum { STYLE_CURSOR, STYLE_ICONS, STYLE_FONT, STYLE_LABEL_ON, STYLE_LABEL_OFF, STYLE_ORNAMENT };
static const struct { uint8_t sheet, row; } styles[6] = {
    {AOS_MENU_SHEET_ICONS, 1}, {AOS_MENU_SHEET_ICONS, 2}, {AOS_MENU_SHEET_FONT, 7},
    {AOS_MENU_SHEET_LABELS, 5}, {AOS_MENU_SHEET_LABELS, 15}, {AOS_MENU_SHEET_ICONS, 14},
};

typedef struct {
    AosObjSprite *out;
    int count;
    int style;
    uint32_t frame;
} List;

/* sub_0803AC40 for entity 0 (layer 0: priority 0, OBJ mode normal). */
static void put(List *l, uint8_t record, int x, int y) {
    if (x < -0x40 || x > 0x130 || y < -0x20 || y > 0xE0 || l->count >= AOS_MENU_MAX_SPRITES) return;
    l->out[l->count++] = (AosObjSprite){styles[l->style].sheet, record, styles[l->style].row, 0, 0,
                                        false, 0x100, 0, 0, 0x100, (int16_t)x, (int16_t)y};
}

/* The cursor's wobble: three pixels each way, frameCounter / 2. */
static int wobble(uint32_t frame) {
    uint32_t t = frame >> 1;
    return (t & 8) ? -(int)((t & 7) - 3) : (int)(t & 7) - 3;
}

/* sub_0803F17C on animation 0: the current step's frame, then the step. */
static uint8_t ornament_frame(AosMainMenuSpriteState *state, const AosObjSheet *icons) {
    if (!icons || !icons->anim_count || !icons->anim_length[0]) return 0;
    AosAnimDef def = {icons->anim_length[0], icons->anim_duration[0]};
    AosAnimSet set = {&def, 1};
    if (!state->started) {
        /* sub_08006BC0: sub_0803F2C8(0, loop), then one sub_0803F17C. */
        aos_anim_start(&state->ornament, &set, 0, true);
        aos_anim_step(&state->ornament, &set);
        state->started = true;
    }
    uint8_t step = state->ornament.frame < def.count ? state->ornament.frame : 0;
    uint8_t record = icons->anim_frame[0][step];
    aos_anim_step(&state->ornament, &set);
    return record;
}

/* sub_08006910: the ornament at both ends of a row. */
static void ornament(List *l, uint8_t record, int x, int y) {
    l->style = STYLE_ORNAMENT;
    put(l, record, x - 1, y);
    put(l, record, x - 1 + 0x62, y);
}

/* sub_08005928_inline: a name, a glyph every 12 pixels (codes 2-0x22). */
static void name(List *l, const uint8_t text[8], int x, int y) {
    for (int k = 0; k < 8; ++k, x += 0xC)
        if (text[k] >= 2 && text[k] <= 0x22) put(l, text[k], x, y);
}

static const AosMenuFileSummary *file(const AosMenuFileSummary files[AOS_MENU_FILES], unsigned slot) {
    static const AosMenuFileSummary none;
    return files && slot < AOS_MENU_FILES ? &files[slot] : &none;
}

/* The file cursor's place for PLAY and DELETE (4F9 below 6: the files and
 * their quicksaves; from 6: the special entry above a file). */
static void file_place(int cursor, int *x, int *y) {
    if (cursor < 6) {
        *x = cursor * 0x10 + 0x10;
        *y = 0x48;
        if (cursor & 1) {
            *x -= 0x10;
            *y += 0x18;
        }
    } else {
        *x = cursor * 0x10 - 0x50;
        *y = 0x30;
    }
}

/* sub_08005928: PLAY (4EF 0) and DELETE (4EF 2). */
static void files_screen(List *l, const AosMainMenu *m, const AosMenuFileSummary *files,
                         uint8_t ornament_record) {
    bool del = m->selected_slot == 2;
    int phase = del ? m->file_phase : 0;               /* + 0x4F0 */
    bool confirm = !del && m->confirm_active;          /* + 0x4FD */
    int cursor = m->file_cursor, source = m->source_slot;
    unsigned current_slot = phase ? (unsigned)source : (unsigned)(cursor >= 6 ? cursor - 6 : cursor);
    const AosMenuFileSummary *current = file(files, current_slot);
    ornament(l, ornament_record, 8, del ? 0x18 : 0x10);
    l->style = STYLE_CURSOR;
    int x, y;
    if (phase == 0) {
        if (confirm) {
            put(l, 0x10, m->confirm_choice * 0x60 + 0x24 + wobble(l->frame), 0x8C);
            if (l->frame & 2) {
                file_place(cursor, &x, &y);
                put(l, 0x10, x, y);
            }
        } else {
            file_place(cursor, &x, &y);
            put(l, 0x10, x + wobble(l->frame), y);
        }
    } else {
        put(l, 0x10, cursor * 0x40 + 0x46 + wobble(l->frame), 0x90);
        if (l->frame & 2) put(l, 0x10, source * 0x10 + 0x10, 0x48);
    }
    if (!confirm) {
        l->style = STYLE_FONT;
        name(l, current->name, 0x80, 0x20);
    }
    l->style = STYLE_ICONS;
    for (int i = 0; i < 3; ++i) {
        const AosMenuFileSummary *regular = file(files, (unsigned)i * 2);
        put(l, (uint8_t)((regular->icon ? 3 : 0) + i), (i << 5) + 0x10, 0x40);
        if (current->play_time == 0)
            put(l, del ? 9 : 0xA, 0x90, 0x38);
        else if (cursor > 5)
            put(l, 0xA, 0x90, 0x38);
        if (!del && (regular->completed & 0xF)) {
            put(l, 8, (i << 5) + 0x10, 0x28);
            put(l, 6, (i << 5) + 0x18, 0x38);
        }
        int v = phase == 0 ? cursor : source;
        if ((current->completed & 0xF) && current->play_time && !(v & 1) && v < 6)
            put(l, 0xB, 0x80, 0x60);
        if (!del && file(files, (unsigned)i * 2 + 1)->play_time) {
            put(l, 6, (i << 5) + 0x18, 0x50);
            put(l, 7, (i << 5) + 0x10, 0x58);
        }
    }
}

/* sub_08005E28: COPY (4F0 the phase, 4F6 the source, 4F7 the target). */
static void copy_screen(List *l, const AosMainMenu *m, const AosMenuFileSummary *files,
                        uint8_t ornament_record) {
    int phase = m->file_phase, cursor = m->file_cursor;
    int source = phase == 0 ? cursor : m->source_slot;
    int target = phase == 1 ? cursor : m->destination_slot;
    const AosMenuFileSummary *from = file(files, (unsigned)source), *to = file(files, (unsigned)target);
    ornament(l, ornament_record, 8, 0x10);
    l->style = STYLE_CURSOR;
    if (phase != 2) {
        put(l, 0x10, cursor * 0x10 + 0x88 + wobble(l->frame), 0x10);
    } else {
        put(l, 0x10, cursor * 0x40 + 0x46 + wobble(l->frame), 0x90);
        if (l->frame & 2) put(l, 0x10, target * 0x10 + 0x88, 0x10);
    }
    l->style = STYLE_FONT;
    name(l, from->name, 8, 0x20);
    if ((from->completed & 0xF) && from->play_time) {
        l->style = STYLE_ICONS;
        put(l, 0xB, 8, 0x60);
    }
    l->style = STYLE_ICONS;
    for (int i = 0; i < 3; ++i)
        put(l, (uint8_t)((file(files, (unsigned)i * 2)->icon ? 3 : 0) + i), i * 0x20 + 0x88, 8);
    if (to->play_time) {
        l->style = STYLE_FONT;
        name(l, to->name, 0x88, 0x20);
        if (to->completed & 0xF) {
            l->style = STYLE_ICONS;
            put(l, 0xB, 0x88, 0x60);
        }
    } else if (phase != 0) {
        l->style = STYLE_ICONS;
        put(l, 9, 0x98, 0x38);
    }
}

/* sub_08006318: name entry (4F9 the symbol, 4FB the position). */
static void name_screen(List *l, const AosMainMenu *m, uint8_t ornament_record) {
    l->style = STYLE_ORNAMENT;
    put(l, ornament_record, 0x37, 0x18);
    put(l, ornament_record, 0x37 + 0x82, 0x18);
    l->style = STYLE_FONT;
    if (l->frame & 0x10) put(l, 0x23, m->name_position * 0xC + 0x48, 0x10);
    put(l, 1, (m->name_cursor % 9) * 0x18 + 0x18, (m->name_cursor / 9) * 0x18 + 0x40);
    name(l, m->name_buffer, 0x48, 0x10);
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 9; ++column) {
            int glyph = row * 9 + column + 2;
            if (glyph != 0x23) put(l, (uint8_t)glyph, column * 0x18 + 0x12, row * 0x18 + 0x38);
        }
}

int aos_main_menu_sprites(const AosMainMenu *menu, bool name_entry,
                          const AosMenuFileSummary files[AOS_MENU_FILES],
                          uint32_t frame_counter, const AosObjSheet *icons,
                          AosMainMenuSpriteState *state,
                          AosObjSprite sprites[AOS_MENU_MAX_SPRITES]) {
    List l = {sprites, 0, STYLE_CURSOR, frame_counter};
    if (name_entry) {
        name_screen(&l, menu, ornament_frame(state, icons));
    } else if (!menu->selected_slot_active) {
        /* sub_080069C4 case 1: the three labels, the selected one lit with
         * the ornament around it. */
        for (int i = 0; i < 3; ++i) {
            bool on = menu->selected_slot == i;
            l.style = on ? STYLE_LABEL_ON : STYLE_LABEL_OFF;
            put(&l, (uint8_t)i, i * 0x30 + 0x20, (i << 5) + 0x10);
            if (on) ornament(&l, ornament_frame(state, icons), i * 0x30 + 0x18, (i << 5) + 0x18);
        }
    } else if (menu->selected_slot == 1) {
        copy_screen(&l, menu, files, ornament_frame(state, icons));
    } else {
        files_screen(&l, menu, files, ornament_frame(state, icons));
    }
    return l.count;
}
