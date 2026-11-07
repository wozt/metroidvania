/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_PAUSE_GRAPHICS_H
#define AOS_PAUSE_GRAPHICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aos_button_config.h"
#include "aos_intro_font.h"
#include "aos_intro_resources.h"
#include "fusion/assets/aos.h"

typedef struct {
    FusionAosRenderedBackground background;
    uint16_t bg0_map[32 * 32];
    uint8_t bg0_base_chars[0x8000];
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

enum {
    AOS_PAUSE_ENEMY_RECORDS = 112,
    AOS_PAUSE_ENEMY_PAGE = 10,
};

typedef struct {
    uint32_t defeated[4]; /* stats->defeated: word enemy>>5, bit enemy&31 */
    uint8_t top;          /* first enemy index shown (multiple of 10) */
    uint8_t cursor;       /* selected enemy index */
} AosPauseEnemyValues;

enum {
    AOS_PAUSE_ITEM_USE_RECORDS = 32,
    AOS_PAUSE_ITEM_USE_VISIBLE = 8,
};

typedef struct {
    uint8_t counts[AOS_PAUSE_ITEM_USE_RECORDS]; /* item_counts slot order */
    uint8_t selected;                           /* item index or 0xFF */
    uint8_t top;                                /* first visible owned ordinal */
} AosPauseItemUseValues;

enum {
    AOS_PAUSE_ABILITY_COUNT = 6,
};

typedef struct {
    uint8_t owned[AOS_PAUSE_ABILITY_COUNT];   /* SoulInventory_GetSoulTotal(3,i) > 0 */
    uint8_t enabled[AOS_PAUSE_ABILITY_COUNT]; /* sub_08032AB8 bit i set */
    uint8_t cursor;
} AosPauseAbilityValues;

enum {
    AOS_PAUSE_EQUIP_SLOTS = 3,
    AOS_PAUSE_EQUIP_VISIBLE = 8,
    AOS_PAUSE_EQUIP_STATS = 6,
    AOS_PAUSE_EQUIP_NONE = 0xFF,
};

typedef struct {
    int16_t current[AOS_PAUSE_EQUIP_STATS]; /* ATK, DEF, STR, CON, INT, LCK. */
    int16_t preview[AOS_PAUSE_EQUIP_STATS];
    uint8_t equipped[AOS_PAUSE_EQUIP_SLOTS];
    uint8_t items[AOS_PAUSE_EQUIP_VISIBLE];
    uint8_t quantities[AOS_PAUSE_EQUIP_VISIBLE];
    uint8_t slot;
    uint8_t item_count;
    uint8_t selected_item;
    bool preview_active;
} AosPauseEquipValues;

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
/* sub_0804A95C/sub_08049F34/sub_0804A2A4/sub_0804A00C: reconstruct the
 * Equip BG0 page and localized ROM text. Item icons and cursor OAM remain a
 * separate compositing layer. */
bool aos_pause_graphics_render_equip(AosPauseGraphics *graphics,
                                     const AosPauseEquipValues *values,
                                     uint8_t *rgba, size_t rgba_size);
/* sub_0804BF8C/sub_0804C264/sub_0804C3AC: the button-configuration page.
 * The four action rows reuse the baked label strips at tilemap entries
 * 0x3359+row*0x10 and the per-button name strips at 0x32A0+index*3 from
 * the language bank. Rows follow the native 0x08525510 order {0,1,3,2};
 * the cursor and button icons are OBJ sprites and remain unrendered. */
bool aos_pause_graphics_render_config(AosPauseGraphics *graphics,
                                      const uint8_t config[AOS_BUTTON_ACTIONS],
                                      uint8_t *rgba, size_t rgba_size);
/* sub_0804D2C8: the Enemy list page. Ten slots in two columns of five;
 * defeated enemies show name text 0x25D+enemy through the context-B
 * cursor, undiscovered ones 0x34E "????????". Row strips use entries
 * 0xF320+slot*0x10 and the enemy numbers use the two-row digit path.
 * The header/footer strips, button hints and the detail view remain
 * unported; the detail page keeps the diagnostic presentation. */
bool aos_pause_graphics_render_enemy(AosPauseGraphics *graphics,
                                     const AosPauseEnemyValues *values,
                                     uint8_t *rgba, size_t rgba_size);
/* sub_0804B6C4/sub_0804B494: the Item Use page. Four rows of two slots
 * reuse the baked strips at columns 2/0x11 rows 11-14, the bottom
 * description bars sit at column 5 rows 16-18, names are text IDs
 * 0x5B+item through the context-B cursor, quantities are 0x3300 digit
 * tiles ending at column 0xD/0x1C and empty slots get two 0x330D dashes.
 * Item icons are OBJ sprites and the description popup's exact window
 * placement is approximate; both remain documented gaps. */
bool aos_pause_graphics_render_item_use(AosPauseGraphics *graphics,
                                        const AosPauseItemUseValues *values,
                                        uint8_t *rgba, size_t rgba_size);
/* sub_0804BA18/sub_0804BB40: the Ability page. The bottom message window
 * (sub_080472B0), the twelve baked row strips, the per-ability ON/OFF tile
 * pairs and the localized names (0x156-0x15B, or 0x34F "---") are BG0. The
 * selection arrow, window frame and description popup remain OBJ/other
 * layers and are unrendered. */
bool aos_pause_graphics_render_ability(AosPauseGraphics *graphics,
                                       const AosPauseAbilityValues *values,
                                       uint8_t *rgba, size_t rgba_size);
/* sub_0804BD38/sub_0804BE0C: the Sleep confirm page's native strings
 * (0x36A "Pause the game?", 0x34B "Yes", 0x34D "No") or the 0x361
 * cannot-save message. The selection arrow and window frame are OBJ and
 * remain unrendered. */
bool aos_pause_graphics_render_sleep(AosPauseGraphics *graphics,
                                     bool can_sleep, uint8_t *rgba,
                                     size_t rgba_size);
void aos_pause_graphics_free(AosPauseGraphics *graphics);

#endif /* AOS_PAUSE_GRAPHICS_H */
