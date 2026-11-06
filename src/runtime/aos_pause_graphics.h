/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_PAUSE_GRAPHICS_H
#define AOS_PAUSE_GRAPHICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aos_intro_font.h"
#include "aos_intro_resources.h"
#include "fusion/assets/aos.h"

typedef struct {
    FusionAosRenderedBackground background;
    uint16_t bg0_map[32 * 32];
    uint8_t bg0_chars[0x8000];
    uint8_t palette[0x200];
    uint8_t backdrop_rgb[3];
    uint8_t language;
    AosIntroResources text;
    AosIntroFont font;
    AosIntroTextTiles text_tiles;
    bool text_ready;
} AosPauseGraphics;

typedef struct {
    int16_t hp;
    int16_t mp;
    uint16_t max_hp;
    uint16_t max_mp;
    int16_t stats[4];
    int16_t attack;
    int16_t defence;
    uint8_t level;
    uint32_t experience;
    uint32_t next_experience;
    uint32_t gold;
    uint32_t play_frames;
    uint16_t status;
} AosPauseHubValues;

enum {
    AOS_PAUSE_SOUL_FAMILIES = 3,
    AOS_PAUSE_SOUL_RECORDS = 56,
};

typedef struct {
    int16_t stats[4];
    int16_t attack;
    int16_t defence;
    uint16_t status;
    uint8_t category;
    uint8_t cursor[AOS_PAUSE_SOUL_FAMILIES];
    uint8_t equipped[AOS_PAUSE_SOUL_FAMILIES];
    uint8_t counts[AOS_PAUSE_SOUL_FAMILIES][AOS_PAUSE_SOUL_RECORDS];
} AosPauseSoulSetValues;

/* sub_08047684 and sub_08047520: decode the pause hub's actual BG1 tiles,
 * palette and block map from the verified Aria ROM. Dynamic BG0 text and OBJ
 * layers are deliberately separate so the runtime and editor can share this
 * source representation. */
bool aos_pause_graphics_init(AosPauseGraphics *graphics, const FusionRom *rom,
                             uint8_t language,
                             char *error, size_t error_size);
/* sub_08047520/sub_08047C4C: rebuild the hub's BG0 map, then composite it
 * over the source-decoded BG1. `rgba` is a 240 x 160 RGBA buffer. */
bool aos_pause_graphics_render_hub(AosPauseGraphics *graphics,
                                   const AosPauseHubValues *values,
                                   uint8_t selected, uint8_t *rgba,
                                   size_t rgba_size);
/* sub_08048D84/sub_080493E8/sub_080494F8/sub_08049DB4: reconstruct the
 * Soul Set BG0 page with the ROM's localized names, descriptions and native
 * proportional fonts. Sprite icons and the animated OBJ cursor remain a
 * separate layer. */
bool aos_pause_graphics_render_soul_set(
    AosPauseGraphics *graphics, const AosPauseSoulSetValues *values,
    uint8_t *rgba, size_t rgba_size);
void aos_pause_graphics_free(AosPauseGraphics *graphics);

#endif /* AOS_PAUSE_GRAPHICS_H */
