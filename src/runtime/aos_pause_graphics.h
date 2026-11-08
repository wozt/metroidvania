/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_PAUSE_GRAPHICS_H
#define AOS_PAUSE_GRAPHICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aos_button_config.h"
#include "aos_intro_font.h"
#include "aos_intro_resources.h"
#include "aos_menu_cells.h"
#include "aos_obj.h"

struct AosRecentSouls;
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
    /* sub_08048C74/sub_08048B0C: the selection hand's sheet, frame records
     * and its OBJ palette row (0x0820C2C0 blocks 1-2). */
    AosObjSheet cursor_sheet;
    uint16_t cursor_palette[16];
    bool cursor_ready;
    uint32_t cursor_tick;      /* frame counter driving bob and frame cycle */
    /* The native software cell renderer: fb is the 32x32-cell buffer
     * (each cell already a 32-byte 4bpp tile), and the four pointers
     * address the ROM char/glyph metric tables. */
    AosMenuCells cells;
    uint32_t cells_fb[AOS_MENU_CELLS_ROW_WORDS * 32];
    const uint8_t *cells_char_a;   /* 0x080E2B4C, stride 4 */
    const uint8_t *cells_glyph_a;  /* 0x0850A136, stride 14 */
    const uint8_t *cells_char_b;   /* 0x080E2EC0, stride 4 */
    const uint8_t *cells_glyph_b;  /* 0x08509876, stride 10 */
    /* sub_08048804/sub_080488FC icon entities: the three 16x16-tile
     * sheets of sub_0801232C (0x081C5E00/0x081C7E04/0x081C9E08, 8192
     * bytes of 4bpp each) and the 16 BGR555 colours of palette bank 2
     * of 0x082099FC. Icon n is the 16x16 block (n-1)%64 of sheet
     * (n-1)/64, drawn at the entity anchor + the frame's (-8,-16). */
    uint8_t icon_tiles[3][0x2000];
    uint16_t icon_palette[16];
    bool icons_ready;
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
    /* sub_08049E64's per-family recent soul lists; sub_080494F8 lists the
     * four most recently acquired owned souls before the rest. */
    const struct AosRecentSouls *recent_souls;
} AosPauseSoulSetValues;

enum {
    AOS_PAUSE_ENEMY_RECORDS = 112,
    AOS_PAUSE_ENEMY_PAGE = 10,
};

typedef struct {
    uint32_t defeated[4];          /* stats->defeated: word enemy>>5, bit enemy&31 */
    uint32_t dropped[2][4];        /* stats->dropped: [0] common 0x133B0, [1] rare 0x133C0 */
    uint8_t top;                   /* first enemy index shown (multiple of 10) */
    uint8_t cursor;                /* selected enemy index */
    uint8_t detail;                /* nonzero renders the detail view for cursor */
    /* Detail record fields (enemy == cursor), from the 0x080E9644 row. */
    int16_t hp;                    /* +0x0C */
    uint16_t experience;           /* +0x10 */
    uint16_t weak, resist;         /* +0x1A, +0x1C element bitmasks */
    uint16_t common_item;          /* +0x08, 0 = no common drop */
    uint16_t rare_item;            /* +0x0A, 0 = no rare drop */
    uint8_t soul_type;             /* +0x17 */
    uint8_t soul_index;            /* +0x18, index + 1; 0 = no soul */
    uint8_t soul_owned;            /* SoulInventory_GetSoulTotal != 0 */
    uint8_t soul_view;             /* menu soul-view toggle */
} AosPauseEnemyValues;

enum {
    AOS_PAUSE_ITEM_USE_RECORDS = 32,
    AOS_PAUSE_ITEM_USE_VISIBLE = 8,
};

typedef struct {
    uint8_t counts[AOS_PAUSE_ITEM_USE_RECORDS]; /* item_counts slot order */
    uint8_t icons[AOS_PAUSE_ITEM_USE_RECORDS];  /* item_use.tsv icon column */
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
    bool selecting;               /* item list open (sub_0804AFD4 state) */
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
                                      uint8_t cursor,
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
/* sub_0804CDF4 + sub-draws: the enemy detail view. Draws the enemy
 * number, HP and experience through sub_08046FFC, the common/rare
 * drop names (0x34F/0xA/0x5A+item gated by the drop bitsets), the
 * name with its 0x1310 underline, the soul-type icon when the soul
 * view is active and owned, the three soul-bar strips and the owned
 * markers. The portrait, the soul name/description message window,
 * weakness labels and transitions remain unported; the drop rows are
 * pure context-B text (sub_0804090C is the pixel cursor setter). */
bool aos_pause_graphics_render_enemy_detail(AosPauseGraphics *graphics,
                                            const AosPauseEnemyValues *values,
                                            uint8_t *rgba, size_t rgba_size);
/* sub_0804B6C4/sub_0804B494: the Item Use page. Four rows of two slots
 * reuse the baked strips at columns 2/0x11 rows 11-14, the bottom
 * description bars sit at column 5 rows 16-18, names are text IDs
 * 0x5B+item through the context-B cursor, quantities are 0x3300 digit
 * tiles ending at column 0xD/0x1C and empty slots get two 0x330D dashes.
 * The selected item's icon draws through the sub_08048804 entity at
 * pixel (0x1C,0x94) with the frame's (-8,-16) offset; the description
 * popup's exact window placement remains approximate. */
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
