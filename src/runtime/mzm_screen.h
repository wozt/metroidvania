/* SPDX-License-Identifier: GPL-3.0-only */
/* What the Zero Mission non-gameplay screens (title, file select, pause
 * screen, cutscenes) share: the GBA display they draw on (a GbaPpu), the
 * gOamData buffer and gNextOamSlot, the *_NonGameplay scroll and blending
 * copies their V-blank writes, the EWRAM work area (sEwramPointer), the
 * input, and the routines of the menu objects (ProcessMenuOam,
 * ResetFreeOam, UpdateMenuOamDataId) and of the palette fades
 * (ApplySpecialBackgroundFadingColor). The data come from the ROM bank. */
#ifndef MZM_SCREEN_H
#define MZM_SCREEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gba_ppu.h"

enum {
    MZM_EWRAM_SIZE = 0x40000,
    MZM_OAM_BUFFER = 128
};

/* struct MenuOamData (16 bytes in ROM) */
typedef struct {
    int16_t y, x;
    uint8_t unk_4, unk_5, unk_6, unk_7;
    uint8_t animation_duration_counter, current_animation_frame, oam_id;
    uint8_t priority, obj_mode;
    bool ended, not_drawn;
    uint8_t exists;                 /* 2: OAM_ID_CHANGED_FLAG */
    uint8_t bound_background;       /* 0-3, else none */
    bool rotation_scaling;
    int16_t unk_e;
} MzmMenuOam;

/* OAM_ARRAY_PRE_ACTION_* */
enum {
    MZM_OAM_PRE_NONE, MZM_OAM_PRE_CHANGE_FRAME, MZM_OAM_PRE_RESET_FRAME,
    MZM_OAM_PRE_LOOP_ON_LAST_FRAME, MZM_OAM_PRE_KILL_AFTER_END, MZM_OAM_PRE_INCREMENT_ID_AFTER_END,
    MZM_OAM_PRE_INCREMENT_ID_LOCK, MZM_OAM_PRE_DECREMENT_ID_AFTER_END, MZM_OAM_PRE_PLAY_BACKWARDS,
    MZM_OAM_PRE_PLAY_BACKWARDS_DECREMENT_ID
};

/* ColorFadingType */
enum { MZM_FADING_IN, MZM_FADING_FLASH, MZM_FADING_OUT, MZM_FADING_UNK };

typedef struct {
    GbaPpu *ppu;
    uint16_t oam[(MZM_OAM_BUFFER + 32) * 4];   /* gOamData */
    uint8_t next_oam_slot;                     /* gNextOamSlot */
    /* gBg0HOFS_NonGameplay ... gBg3VOFS_NonGameplay (subpixels) */
    uint16_t bg_hofs[4], bg_vofs[4];
    int16_t oam_x_offset, oam_y_offset;        /* gOamX/YOffset_NonGameplay */
    uint8_t bldy;                              /* gWrittenToBldy_NonGameplay */
    uint8_t bldalpha_l, bldalpha_h;            /* gWrittenToBldalpha_L / _H */
    uint8_t *ewram;                            /* sEwramPointer, MZM_EWRAM_SIZE bytes */
    /* gChangedInput, gButtonInput: the GBA key bits (GBA_KEY_* of
     * gba_input.h) */
    uint16_t changed_input, button_input;
} MzmScreen;

bool mzm_screen_init(MzmScreen *s, GbaPpu *ppu);
void mzm_screen_free(MzmScreen *s);
/* gChangedInput and gButtonInput from the held GBA keys (GBA_KEY_*). */
void mzm_screen_input(MzmScreen *s, uint16_t held);

/* ClearGfxRam */
void mzm_clear_gfx_ram(MzmScreen *s);
/* ResetFreeOam */
void mzm_reset_free_oam(MzmScreen *s);
/* DMA3_COPY_32(gOamData, OAM_BASE, ...) of a V-blank */
void mzm_screen_copy_oam(MzmScreen *s);
/* UpdateMenuOamDataId */
void mzm_menu_oam_set_id(MzmMenuOam *oam, uint8_t oam_id);
/* A MenuOamData of the ROM (sMenuOamData_Empty, the title sparkles...). */
MzmMenuOam mzm_menu_oam_from_rom(uint32_t address);
/* ProcessMenuOam with the struct OamArray table at a ROM address. */
void mzm_menu_oam_process(MzmScreen *s, int length, MzmMenuOam *oam, uint32_t oam_array);

/* ApplySpecialBackgroundFadingColor on 256 colors: rows of 16 whose bit
 * is set in `mask` take `color` (0-31) of the fade from `src` to `dst`. */
void mzm_apply_fading_color(uint8_t type, uint8_t color, const uint16_t *src, uint16_t *dst,
                            uint16_t mask);

/* CallLZ77UncompVram / CallLZ77UncompWram of a ROM address into a buffer;
 * false when the stream is missing or does not fit. */
bool mzm_lz77_from_rom(uint32_t address, uint8_t *dst, size_t dst_size);
/* DmaTransfer of ROM bytes (missing bytes leave `dst` unchanged). */
void mzm_copy_from_rom(void *dst, uint32_t address, size_t size);
/* ProcessComplexOam (double-sized) on gOamData[slot], and
 * CalculateOamPart4's sixteen affine parameters from `slot`. */
void mzm_process_complex_oam(MzmScreen *s, int slot, int16_t x_position, int16_t y_position,
                             uint16_t rotation, int16_t scaling, uint16_t matrix);
void mzm_calculate_oam_part4(MzmScreen *s, uint16_t rotation, int16_t scaling, int slot);
/* A ROM symbol's address, 0 if unknown. */
uint32_t mzm_sym(const char *name);

#endif
