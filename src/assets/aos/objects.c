/* SPDX-License-Identifier: GPL-3.0-only */
/* Native export of traced Aria object, enemy and shared-effect sprites. */
#include "fusion/assets/aos.h"

#include "fusion/assets/gba.h"
#include "export.h"
#include "sprite_library.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    GBA_BASE = 0x08000000,
    ENEMY_TABLE = 0x080E9644,
    MONEY_TABLE = 0x085250E4,
    ITEM_STATS = 0x08505B3C,
    WEAPON_STATS = 0x08505D3C,
    ARMOR_STATS = 0x085063B0,
    RED_SOULS = 0x080E1598,
    BLUE_SOULS = 0x080E192C,
    YELLOW_SOULS = 0x080E1B08,
    SKILL_THROWS = 0x084F12C8,
    HUD_PALETTE = 0x0820C428,
    HUD_TILES = 0x0827B208,
    HUD_FONT_METRICS = 0x080E2EC0,
    HUD_FONT_GLYPHS = 0x08509876,
    HUD_FONT_GLYPH_COUNT = 0xE0,
    HUD_FONT_METRIC_BYTES = HUD_FONT_GLYPH_COUNT * 4,
    HUD_FONT_BITMAP_BYTES = HUD_FONT_GLYPH_COUNT * 10,
    ENEMY_COUNT = 0x71,
    DEATH_BLINK = 0x08118CE0,
    SCREEN_MARGINS = 0x08118D08,
    BUER_DEATH_OFFSETS = 0x081194DC,
    TILE_BYTES = 32,
    MAX_ANIMATIONS = 256,
    MAX_FRAMES = 128,
    MAX_STATIC_FRAMES = 20,
    CANVAS_SIZE = 512,
    CANVAS_ORIGIN = 256,
};

static const char *const index_prefix =
    "assets/extracted/aria/sprites/objects/runtime";

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} Text;

typedef struct {
    const char *name;
    uint32_t graphics;
    uint32_t palette;
    uint8_t bank;
    uint32_t frames;
    uint32_t probes;
    uint32_t volleys;
    uint8_t volley_count;
    uint8_t static_count;
    uint8_t static_frames[MAX_STATIC_FRAMES];
} EntitySpec;

typedef struct {
    const char *name;
    uint32_t graphics;
    uint32_t palette;
    uint8_t bank;
    uint32_t frames;
    uint8_t animation_count;
    uint8_t animations[2];
    int static_frame;
} EffectSpec;

/* A direct `EwramData_unk54C` type-1 sequence, rather than an entry in an
 * entity descriptor's animation-pointer table. */
typedef struct {
    const char *name;
    uint32_t graphics;
    uint32_t palette;
    uint8_t bank;
    uint32_t frames;
    uint8_t count;
    uint8_t frame[MAX_FRAMES];
    uint8_t ticks[MAX_FRAMES];
} DirectEffectSpec;

typedef struct {
    int8_t x;
    int8_t y;
    uint8_t source_x;
    uint8_t source_y;
    uint8_t width;
    uint8_t height;
    uint32_t flags;
} Component;

typedef struct {
    uint8_t *data;
    size_t size;
    size_t width_tiles;
} TileSheet;

typedef struct {
    uint8_t *bmp;
    size_t bmp_size;
    int left;
    int top;
    uint8_t mode;
    int8_t hurt_x;
    int8_t hurt_y;
    uint8_t hurt_width;
    uint8_t hurt_height;
    int8_t attack_x;
    int8_t attack_y;
    uint8_t attack_width;
    uint8_t attack_height;
    int anchor_x;
    int anchor_y;
    int edge_x;     /* the last component's x + width (sub_0806C3F8) */
    int edge_y;
} RenderedFrame;

#define ENTITY(name_, graphics_, palette_, bank_, frames_) \
    {name_, graphics_, palette_, bank_, frames_, 0, 0, 0, 0, {0}}
#define STATIC(name_, graphics_, palette_, bank_, frames_, count_, ...) \
    {name_, graphics_, palette_, bank_, frames_, 0, 0, 0, count_, {__VA_ARGS__}}
#define PROBES(name_, graphics_, palette_, bank_, frames_, probes_) \
    {name_, graphics_, palette_, bank_, frames_, probes_, 0, 0, 0, {0}}

static const EntitySpec entities[] = {
    /* Object20's first-room actors. The table at 0x081186DC stores these
     * records as palette, frame descriptor, tile sheet; the exporter keeps
     * their original resources separate so the scene can draw them without
     * conflating them with enemy kinds. */
    ENTITY("opening_actor_0", 0x08208304, 0x0820C598, 0, 0x082589FC),
    ENTITY("opening_actor_1", 0x082052F8, 0x0820C52C, 0, 0x08258050),
    ENTITY("opening_actor_8", 0x081CBF64, 0x0820A518, 0, 0x08215CBC),
    ENTITY("bat", 0x081F422C, 0x0820BD4C, 0, 0x0824B2C4),
    STATIC("zombie", 0x081CBF8C, 0x0820A62C, 0, 0x082178B8, 2, 0, 13),
    /* The Skeleton: its walker probes 0x08528948 (sub_0806CAF8 mode 12), the
     * bone (0x12) and head (0x1B) projectiles and its death pieces
     * (0x15-0x1B, 0x08528952). */
    {"skeleton", 0x081F424C, 0x0820BE3C, 0, 0x0824C910, 0x08528948, 0, 0, 8,
     {0x12, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B}},
    ENTITY("merman", 0x081CBF3C, 0x0820A464, 0, 0x08212FB4),
    /* The Peeping Eye's direct frames: the eyelid (0-10, + 0x19), the
     * segments (11-14), the eyeball (15) and the pupil (16). */
    STATIC("peeping_eye", 0x081CBFEC, 0x0820A86C, 0, 0x0821D7B4, 17, 0, 1, 2, 3, 4, 5, 6, 7, 8,
           9, 10, 11, 12, 13, 14, 15, 16),
    /* The Killer Fish (the Merman's descriptor): its bubbles show sprite
     * frames 15 and 16, bursting one frame lower (14). */
    STATIC("killer_fish", 0x081CBF3C, 0x0820A464, 0, 0x08212FB4, 3, 14, 15, 16),
    ENTITY("bone_pillar", 0x081E41D4, 0x0820B8E8, 0, 0x08242F8C),
    /* EnemyFishHeadCreate (0x080B3F34) calls sub_080B3BAC with variant
     * 1: the resource row of 0x081193DC + 16 — same tiles and palette,
     * frames 0x08243830. */
    ENTITY("fish_head", 0x081E41D4, 0x0820B8E8, 0, 0x08243830),
    /* EnemyUkobackCreate (0x080C5BDC) through sub_080C5C58: tiles
     * 0x081F425C, palette 0x0820BEA4, frames 0x0824D198. Its peeked
     * creature (sub_080C5FA0) and lid (sub_080C6624) share the same
     * resources; the sub_080C6310/sub_080C639C shard entities use the
     * shared special-15 row-7 art, emitted as burst-15 effects. */
    ENTITY("ukoback", 0x081F425C, 0x0820BEA4, 0, 0x0824D198),
    /* EnemyStudentWitchCreate (0x080B8CA0) through sub_0806E0D0 and
     * the sub_080B8668 bolt, which share the Witch's resources: tiles
     * 0x081E41C4, palette 0x0820B880, frames 0x08241790. */
    ENTITY("student_witch", 0x081E41C4, 0x0820B880, 0, 0x08241790),
    ENTITY("witch", 0x081E41C4, 0x0820B880, 0, 0x08241790),
    ENTITY("mudman", 0x081CBF4C, 0x0820A4AC, 0, 0x082138AC),
    /* EnemyWerejaguarCreate (0x080796F0) / EnemyWeretigerCreate
     * (0x08079874) share sub_0807968C: the 0x081CBF7C record, frames
     * 0x08216A40 and palette 0x0820A5C4 — bank 0 for id 0x4B, bank 1
     * for id 0x54. */
    /* EnemyWerewolfCreate through sub_0806F818: the 0x081CBF34
     * record, palette 0x0820A440 bank 0, frames 0x08212C64. */
    ENTITY("werewolf", 0x081CBF34, 0x0820A440, 0, 0x08212C64),
    /* EnemySlimeCreate 0x080C11C0 feeds sub_0806E0D0 the tile record
     * 0x08260320, palette descriptor 0x082664DC (two banks, the spawn
     * record's + 0x32 byte picks one — 0 in every room export) and the
     * animation record 0x0826798C. */
    ENTITY("slime", 0x08260320, 0x082664DC, 0, 0x0826798C),
    ENTITY("kyoma_demon", 0x081CC0A4, 0x0820AF74, 0, 0x08226EAC),
    /* Altair (func_080C1C54): enemy record 0x081F4234, palette
     * descriptor 0x0820BD70 (one bank), animation record 0x0824B660 —
     * shared with its blade children of sub_080C1D88. */
    ENTITY("altair", 0x081F4234, 0x0820BD70, 0, 0x0824B660),
    /* Bael (EnemyBaelCreate 0x080CC624 through the shared loader sub_0806E0D0):
     * graphics 0x081F428C, palette 0x0820BF9C, frames 0x0824FDB0 —
     * shared with its companion of func_080CC744. The create's rng
     * & 0x1F + 0x58 variant picks one of eight palette schemes at
     * draw time natively; bank 0 stands in (documented). */
    ENTITY("bael", 0x081F428C, 0x0820BF9C, 0, 0x0824FDB0),
    /* Nightmare (EnemyNightmareCreate 0x080B839C through the shared
     * loader sub_0806E0D0): tiles 0x081EC1FC, palette 0x0820BBF8,
     * frames 0x08247488; its drift walks the 0x08528878 probe
     * descriptor (sub_0806CAF8, mode 4). */
    PROBES("nightmare", 0x081EC1FC, 0x0820BBF8, 0, 0x08247488,
           0x08528878),
    /* Une (EnemyUneCreate 0x0807427C through sub_08074874): tiles
     * 0x081CBF5C, palette 0x0820A4F4, frames 0x08214C38 — shared
     * with its split children and debris (sub_080747B4). */
    ENTITY("une", 0x081CBF5C, 0x0820A4F4, 0, 0x08214C38),
    /* Arachne (EnemyArachneCreate 0x080799FC through sub_0807A970):
     * tiles 0x081CBF84, palette 0x0820A608, frames 0x08217464 —
     * shared with its five legs and the web projectiles. */
    ENTITY("arachne", 0x081CBF84, 0x0820A608, 0, 0x08217464),
    ENTITY("persephone", 0x081CBF94, 0x0820A650, 0, 0x08217D48),
    /* Lilith (EnemyLilithCreate 0x08094818 through sub_080952A0):
     * tiles 0x081CC03C, palette 0x0820AAF4, frames 0x08222EFC —
     * shared with the breath bolt children (static frame 0x36). */
    ENTITY("lilith", 0x081CC03C, 0x0820AAF4, 0, 0x08222EFC),
    /* Succubus (sub_080946C4): the same tiles 0x081CC03C, palette
     * 0x0820AAF4 and frames 0x08222EFC as Lilith. */
    ENTITY("succubus", 0x081CC03C, 0x0820AAF4, 0, 0x08222EFC),
    /* Triton (sub_08087D24): tiles 0x081CBFE4, palette 0x0820A848,
     * frames 0x0821D4E8 — shared with the whirlpool head, the bubble,
     * the streak jets and the splashes. */
    ENTITY("triton", 0x081CBFE4, 0x0820A848, 0, 0x0821D4E8),
    /* Devil family (sub_08091430): tiles 0x081CC014, frames 0x08220A38,
     * palettes 0x0820A9A0 banks 0-3 — the + 0x36 variant byte (the id)
     * picks the bank and the per-slot child frame descriptor. */
    ENTITY("devil", 0x081CC014, 0x0820A9A0, 2, 0x08220A38),
    ENTITY("arc_demon", 0x081CC014, 0x0820A9A0, 0, 0x08220A38),
    ENTITY("demon_lord", 0x081CC014, 0x0820A9A0, 1, 0x08220A38),
    ENTITY("flame_demon", 0x081CC014, 0x0820A9A0, 3, 0x08220A38),
    /* Quezlcoatl (sub_080786B0): tiles 0x081CBF74, palette 0x0820A580
     * bank 1, frames 0x0821631C — shared by the head, its fifteen
     * segments and the death gore. */
    ENTITY("quezlcoatl", 0x081CBF74, 0x0820A580, 1, 0x0821631C),
    /* Alastor (sub_080CD544): tiles 0x081F4294, palette 0x0820BFE0,
     * frames 0x0824FEE0 — shared by the head, the partner body and
     * the state-6 muzzle shots. */
    ENTITY("alastor", 0x081F4294, 0x0820BFE0, 0, 0x0824FEE0),
    /* Man Eater (sub_0806E0D0 in EnemyManEaterCreate): tiles
     * 0x081F4224, palette 0x0820BD28, frames 0x0824B178 — shared by
     * the head, the three level children, the volley entities, the
     * bolts and the death particles. */
    ENTITY("man_eater", 0x081F4224, 0x0820BD28, 0, 0x0824B178),
    /* Gladiator (sub_080A1BEC in EnemyGladiatorCreate): tiles
     * 0x081CC0AC, frames 0x08229104, palette bank 0x0820AF98 —
     * shared by the body, the three limbs, the shots, the trails
     * and the death chunks. */
    ENTITY("gladiator", 0x081CC0AC, 0x0820AF98, 0, 0x08229104),
    /* Golem family (sub_08081128 in EnemyGolemCreate): tiles
     * 0x081CBFAC (0x081CBFB4 for Flesh Golem), palette 0x0820A6FC,
     * frames 0x08219804 — shared by the body and the ten limbs. */
    ENTITY("wooden_golem", 0x081CBFAC, 0x0820A6FC, 0, 0x08219804),
    ENTITY("golem", 0x081CBFAC, 0x0820A6FC, 0, 0x08219804),
    ENTITY("flesh_golem", 0x081CBFB4, 0x0820A6FC, 0, 0x08219804),
    ENTITY("iron_golem", 0x081CBFAC, 0x0820A6FC, 0, 0x08219804),
    /* Stolas (sub_0809C4E4 in EnemyStolasCreate): tiles 0x081CC064,
     * palette bank 0x0820AC28, frames 0x08225224 — shared by the
     * body, the six bones, the shot/fan/wave projectiles and the
     * death gore. */
    ENTITY("stolas", 0x081CC064, 0x0820AC28, 0, 0x08225224),
    ENTITY("death", 0x08119498, 0x0820BC1C, 0, 0x08249C48),
    ENTITY("manticore", 0x081CBF54, 0x0820A4D0, 0, 0x082145BC),
    ENTITY("balore", 0x081193FC, 0x0820B92C, 7, 0x08244220),
    ENTITY("creaking_skull", 0x081CC044, 0x0820AB38, 0, 0x082232B8),
    ENTITY("giant_skeleton", 0x081CC044, 0x0820AB38, 0, 0x082232B8),
    ENTITY("legion", 0x081190C0, 0x0820AC4C, 0, 0x082258FC),
    /* Julius Belmont EnemyBelmontCreate 0x080CFAB0: tiles
     * 0x08119520, palette 0x0820C028, frames 0x08252748 — shared by
     * the body and the sub_080D1BB4/sub_080D1ADC projectiles. */
    ENTITY("belmont", 0x08119520, 0x0820C028, 0, 0x08252748),
    /* EnemyBigGolemCreate 0x080A8328 / sub_080AD1D0: tiles
     * 0x0825EB04, palette 0x0826642C, frames 0x08266E08; the four
     * sub_0806DFF8 children of sub_080AC6E8 read tiles 0x0825EB0C
     * and frames 0x08266E78 under the other variant byte. */
    ENTITY("big_golem", 0x0825EB04, 0x0826642C, 0, 0x08266E08),
    /* EnemyGreatArmorCreate 0x0808A43C / sub_0808E024: tiles
     * 0x081CC004 through sub_0803B800, palette bank of the
     * 0x0820A8F8 table picked by the + 0x36 id byte, frames
     * 0x0821F240 through sub_0803B924; the fourteen sub_0806DFF8
     * children of sub_0808E164 share the sheet. */
    ENTITY("great_armor", 0x081CC004, 0x0820A8F8, 0, 0x0821F240),
    /* EnemyGrahamCreate 0x080D2424: the conductor entity carries no
     * graphics of its own; the visible fighter is the sub_080D4168
     * shadow clone — tiles 0x081FB2BC, palette 0x0820C090, frames
     * 0x08253D60 through sub_0806E0D0 — and the sub_080D31D0 weak
     * point reads tiles 0x081FD2C0, palette 0x0820C158, frames
     * 0x08254D84. The clone sheet stands in as the kind art. */
    ENTITY("graham", 0x081FB2BC, 0x0820C090, 0, 0x08253D60),
    /* EnemyHeadhunterCreate/sub_08083A40: tiles 0x08118F00 through
     * sub_0803B800, palette bank 0x0820A780, frames 0x0821C190; the
     * three sub_08083FD0 heads read the 0x08118F14 sub-sheet of the
     * same container and share the frame record. */
    ENTITY("headhunter", 0x08118F00, 0x0820A780, 0, 0x0821C190),
    /* EnemyChaosCreate/sub_080A42D4: tiles 0x081191CC through
     * sub_0803B800, palette bank 0x0820AD70, frames 0x08225EF8.
     * EnemyChaos2Create reads tiles 0x08119224, the same palette
     * bank and frames 0x08226B58 — a second kind the runtime cannot
     * select per variant, so the phase-2 form shares this art. */
    ENTITY("chaos", 0x081191CC, 0x0820AD70, 0, 0x08225EF8),
    ENTITY("werejaguar", 0x081CBF7C, 0x0820A5C4, 0, 0x08216A40),
    ENTITY("weretiger", 0x081CBF7C, 0x0820A5C4, 1, 0x08216A40),
    STATIC("blue_crow", 0x081F4274, 0x0820BF10, 0, 0x0824EC04, 1, 8),
    /* Buer sub_0806CAF8: the shared 0x08528B64 s16 probe descriptor
     * has wall count, ceiling, floor, half-width and wall Y offsets.
     * Export the descriptor from the user's ROM, preserving both still frames. */
    {"buer", 0x081F428C, 0x0820BF9C, 0, 0x0824FDB0,
     0x08528B64, 0, 0, 2, {0, 1}},
    /* The White Dragon's direct frames: the head (0-2), a segment piece
     * (3), its fireball (5-9, frame 9 also the flames and sparks) and the
     * head's two death pieces (10, 11). */
    STATIC("white_dragon", 0x081CBF74, 0x0820A580, 0, 0x0821631C, 12, 0, 1, 2, 3, 4, 5, 6, 7, 8,
           9, 10, 11),
    ENTITY("ghost", 0x081CBFFC, 0x0820A8D4, 0, 0x0821EE0C),
    ENTITY("rock_armor", 0x081E41BC, 0x0820B83C, 1, 0x0823FFD8),
    /* Bomber Armor shares the Rock Armor's tiles and palette bank 0 but
     * has its own frame set (0x0823EC00: walk 0, throw 1, death 2, bomb 3,
     * fuse spark 4-6). */
    ENTITY("bomber_armor", 0x081E41BC, 0x0820B83C, 0, 0x0823EC00),
    /* Sky Fish (sub_080A8194): tiles 0x081CC0BC, palette 0x0820B000 bank 0,
     * frame table 0x0822976C shared with its splash effect entities. */
    ENTITY("sky_fish", 0x081CC0BC, 0x0820B000, 0, 0x0822976C),
    /* Ectoplasm (sub_080999F0): tiles 0x081CC054, palette 0x0820ABE0
     * bank 0, frames 0x0822388C. */
    ENTITY("ectoplasm", 0x081CC054, 0x0820ABE0, 0, 0x0822388C),
    ENTITY("giant_ghost", 0x081CBFA4, 0x0820A698, 0, 0x08219140),
    /* The Winged Skeleton and its death pieces (frames 0x24-0x2A of
     * sub_0807540C). */
    {"winged_skeleton", 0x081CBF64, 0x0820A518, 0, 0x08215CBC, 0, 0, 0, 7,
     {0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A}},
    STATIC("red_crow", 0x081F4274, 0x0820BF10, 1, 0x0824EC04, 1, 8),
    PROBES("axe_armor", 0x081E41AC, 0x0820B7F4, 0, 0x0823C290, 0x08528708),
    {"skull_archer", 0x081E419C, 0x0820B78C, 0, 0x0823A52C,
     0x08528620, 0x08528614, 3, 1, {25}},
    ENTITY("fleaman", 0x081F423C, 0x0820BD94, 0, 0x0824B92C),
    ENTITY("ripper", 0x081F423C, 0x0820BD94, 1, 0x0824B92C),
    STATIC("harpy", 0x081CBF6C, 0x0820A53C, 0, 0x08216060, 7,
           8, 9, 10, 11, 12, 13, 14),
    STATIC("siren", 0x081CBF6C, 0x0820A53C, 1, 0x08216060, 7,
           8, 9, 10, 11, 12, 13, 14),
    STATIC("gorgon", 0x081CBFF4, 0x0820A890, 0, 0x0821ECE8, 10,
           17, 18, 19, 20, 21, 22, 23, 24, 25, 26),
    STATIC("catoblepas", 0x081CBFF4, 0x0820A890, 1, 0x0821ECE8, 10,
           17, 18, 19, 20, 21, 22, 23, 24, 25, 26),
    ENTITY("tiny_devil", 0x081F4254, 0x0820BE60, 1, 0x0824CF0C),
    STATIC("imp", 0x081F4254, 0x0820BE60, 0, 0x0824CF0C, 1, 21),
    PROBES("dead_warrior", 0x081EC1F4, 0x0820BBD4, 0, 0x08246F00, 0x08528830),
    ENTITY("skeleton_knight", 0x081CC00C, 0x0820A95C, 0, 0x0821FA5C),
    ENTITY("skull_millione", 0x081F4214, 0x0820BCA0, 2, 0x0824A8E0),
    STATIC("cagnazzo", 0x081F4214, 0x0820BCA0, 0, 0x0824A334, 1, 13),
    STATIC("lubicant", 0x081F4214, 0x0820BCA0, 1, 0x0824A334, 1, 13),
    STATIC("killer_doll", 0x081F426C, 0x0820BEEC, 0, 0x0824DB0C, 5,
           13, 14, 15, 16, 17),
    STATIC("lightning_doll", 0x081F426C, 0x0820BEEC, 0, 0x0824E7F4, 5,
           20, 21, 22, 23, 24),
    STATIC("nemesis", 0x081F4264, 0x0820BEC8, 0, 0x0824D2D8, 4,
           0, 1, 2, 3),
    {"beam_skeleton", 0x081F427C, 0x0820BF54, 0, 0x0824F9AC,
     0x08528B04, 0, 0, 10, {21, 22, 23, 24, 25, 26, 27, 28, 29, 30}},
    ENTITY("dead_crusader", 0x081CC00C, 0x0820A95C, 1, 0x0821FA5C),
    ENTITY("valkyrie", 0x081CBF2C, 0x0820A3FC, 0, 0x0821285C),
    STATIC("mimic", 0x081CC04C, 0x0820AB7C, 0, 0x08223698, 1, 0),
    ENTITY("erinys", 0x081CBF2C, 0x0820A3FC, 1, 0x0821285C),
    ENTITY("durga", 0x081CC02C, 0x0820AA6C, 0, 0x0822184C),
    ENTITY("curly", 0x081CC02C, 0x0820AA6C, 1, 0x0822184C),
    ENTITY("ghost_dancer", 0x081F4244, 0x0820BDD8, 0, 0x0824BC84),
    ENTITY("ghost_dancer_bank1", 0x081F4244, 0x0820BDD8, 1, 0x0824BC84),
    ENTITY("ghost_dancer_bank2", 0x081F4244, 0x0820BDD8, 2, 0x0824BC84),
    ENTITY("gargoyle", 0x081CBFDC, 0x0820A804, 0, 0x0821C688),
    ENTITY("gremlin", 0x081CBFDC, 0x0820A804, 1, 0x0821C688),
    STATIC("zombie_officer", 0x081CC01C, 0x0820AA24, 0, 0x08220F84, 3,
           15, 16, 17),
    ENTITY("medusa_head", 0x081CC034, 0x0820AAB0, 0, 0x08221978),
    ENTITY("medusa_head_bank1", 0x081CC034, 0x0820AAB0, 1, 0x08221978),
    STATIC("needles", 0x081F4284, 0x0820BF78, 0, 0x0824FB10, 4,
           2, 3, 4, 5),
    STATIC("zombie_soldier", 0x081CC024, 0x0820AA48, 0, 0x082215C4, 3,
           18, 19, 20),
    /* SkillBatUse: entry 3 of the skill resources 0x080E13BC (tiles,
     * palette row 0, frames), exported with its per-frame attack boxes. */
    ENTITY("skill_bat", 0x081CD8D0, 0x0820B0B0, 0, 0x082311F8),
    /* SkillSkullArcherUse and its arrow sub_08028BC4: entry 18 of 0x080E13BC
     * (the bow plays sequence 0, the arrow shows frame 1). */
    STATIC("skill_archer", 0x081D510C, 0x0820B2EC, 0, 0x08233BCC, 1, 1),
    /* Resource 0 of 0x080E13BC, shared by several souls: the Merman's spout
     * and drops (sequences 0, 6), the Winged Skeleton's spear (still frame
     * 0), the Siren's notes (2-3) and the Axe Armor's axe (6). */
    STATIC("skill_shared", 0x081CC0C4, 0x0820B044, 0, 0x082308E8, 5, 0, 2, 3, 6, 13),
    /* Resource 24 of 0x080E13BC: the Gladiator soul's frame 0 and walker
     * probes 0x084F13C8, the Harpy soul's dagger (still frame 1) and its
     * trail (3-5), and the Durga soul's sword (2). */
    {"skill_blades", 0x081D8124, 0x0820B3C4, 0, 0x082347BC, 0x084F13C8,
     0, 0, 5, {1, 2, 3, 4, 5}},
    /* Resource 21 of 0x080E13BC: the Tiny Devil soul's fire (sequence 0,
     * then 16 still frames, one per direction). */
    STATIC("skill_devil", 0x081D6918, 0x0820B358, 0, 0x0823445C, 16, 0, 1, 2, 3, 4, 5, 6, 7, 8,
           9, 10, 11, 12, 13, 14, 15),
    /* Resource 31 of 0x080E13BC: the Blue Crow soul's crow (sequence 2). */
    ENTITY("skill_crow", 0x081DB940, 0x0820B4C0, 0, 0x08235F04),
    /* Resource 20 of 0x080E13BC: the Fleaman soul's flea (still frames 0-2,
     * sequence 1, the walker probes 0x084F13BE). */
    {"skill_flea", 0x081D6114, 0x0820B334, 0, 0x0823415C, 0x084F13BE, 0, 0, 3, {0, 1, 2}},
    /* Resource 19 of 0x080E13BC: the Beam Skeleton soul's skull (sequence 0). */
    ENTITY("skill_skull", 0x081D5910, 0x0820B310, 0, 0x08233FA8),
    /* Resource 2 of 0x080E13BC: the Valkyrie soul's warrior (sequence 0). */
    ENTITY("skill_valkyrie", 0x081CD0CC, 0x0820B08C, 0, 0x08230F88),
    /* Resource 12 of 0x080E13BC: the Needles soul's urchin (sequence 0)
     * and its four burst pieces (still frames 0-3). */
    STATIC("skill_needles", 0x081D20F4, 0x0820B214, 0, 0x08232300, 4, 0, 1, 2, 3),
    /* The two entries of 0x084F128C select still frame 0 (Evil Butcher) or
     * 1 (Ripper). Waiter Skeleton's shared throw uses the same resource's
     * frame 11 and its distinct probe table 0x084F139C. */
    {"skill_ripper", 0x081CC8C8, 0x0820B068, 0, 0x08230AE0, 0x084F139C,
     0, 0, 3, {0, 1, 11}},
    /* Resource 11 of 0x080E13BC: Killer Fish's appearance sequence 1 and
     * looping swim/fall sequence 0. */
    ENTITY("skill_killer_fish", 0x081D18F0, 0x0820B1F0, 0, 0x082321E4),
    /* Resource 17 of 0x080E13BC: Fish Head's accelerating sequence 0. */
    ENTITY("skill_fish_head", 0x081D4908, 0x0820B2C8, 0, 0x082338A4),
    /* Resource 14 of 0x080E13BC: SkillWerewolfUse's werewolf (sequence 0
     * once) with its 0x084F13B4 walker probes. */
    {"skill_werewolf", 0x081D30FC, 0x0820B25C, 0, 0x082331BC, 0x084F13B4,
     0, 0, 0, {0}},
    /* Resource 0x138 of 0x080E13BC: SkillKillerMantleUse's spectral knight
     * (attack box preset 5 through sub_0804277C). */
    {"skill_killer_mantle", 0x081D912C, 0x0820B40C, 0, 0x08234CDC,
     0, 0, 0, 0, {0}},
    /* Entry 0x60 of 0x080E13BC shared by Skill_Werejaguar_Weretiger_Use
     * (attack box preset 5 through sub_0804277C). */
    {"skill_were_beast", 0x081D00E4, 0x0820B164, 0, 0x0823199C,
     0, 0, 0, 0, {0}},
    /* Entry 0x150 of 0x080E13BC shared by SkillDemonLordUse and
     * SkillFlameDemonUpdate (attack box preset 5). */
    {"skill_demon_lord", 0x081DA134, 0x0820B454, 0, 0x0823512C,
     0, 0, 0, 0, {0}},
    /* Entry 0xB4 of 0x080E13BC: SkillBiphronUse's walker and its
     * sub_080286BC totems (0x084F13B4 probes, attack box preset 5). */
    {"skill_biphron", 0x081D3900, 0x0820B280, 0, 0x082331BC, 0x084F13B4,
     0, 0, 0, {0}},
    /* Entry 0x168 of 0x080E13BC: SkillBaloreUse's glued familiar
     * (attack box preset 5, no walker probes). */
    {"skill_balore", 0x081DB13C, 0x0820B49C, 0, 0x08235C8C, 0,
     0, 0, 0, {0}},
    /* Entry 0x144 of 0x080E13BC: SkillKyomaDemonUse's glued familiar
     * (no walker probes; + 0x58 bits 0-4 carry the palette variant). */
    {"skill_kyoma_demon", 0x081D9930, 0x0820B430, 0, 0x08234F90, 0,
     0, 0, 0, {0}},
    /* Entry 0x15C of 0x080E13BC: SkillLegionUse's glued familiar
     * (attack box preset 5, no walker probes). */
    {"skill_legion", 0x081DA938, 0x0820B478, 0, 0x08235684, 0,
     0, 0, 0, {0}},
    /* Entry 0x30 of 0x080E13BC: SkillStudentWitchUse's running ghost,
     * shared by red soul 15 (Student Witch and Basilisk). */
    {"skill_student_witch", 0x081CE0D4, 0x0820B0D4, 0, 0x08231350,
     0x084F12BC, 0, 0, 0, {0}},
    /* Entry 0x18C of 0x080E13BC: SkillSlimeUse's orbiting blob and its
     * sub_0802C9E8 trail child (compressed resource container). */
    {"skill_slime", 0x0825F318, 0x08266494, 0, 0x08267154, 0,
     0, 0, 0, {0}},
    /* Entry 0x108 of 0x080E13BC: SkillUkobackUse's lingering ghost
     * (no walker probes; palette variant through + 0x58 bits 0-4). */
    {"skill_ukoback", 0x081D711C, 0x0820B37C, 0, 0x0823469C, 0,
     0, 0, 0, {0}},
    /* Entry 0x3C of 0x080E13BC: SkillAltairUse's spirit (the shared
     * sub_0803F2C8 animation sequences carry the flicker variants). */
    {"skill_altair", 0x081D10EC, 0x0820B1CC, 0, 0x08231F30, 0,
     0, 0, 0, {0}},
    /* Resource 6 of 0x080E13BC: the Killer Doll soul's decoy (looping
     * sequence 0 or 1). */
    ENTITY("skill_killer_doll", 0x081CF0DC, 0x0820B11C, 0, 0x08231578),
    /* Resource 9 of 0x080E13BC: the Lightning Doll soul's bolt (sequence 0
     * once). */
    ENTITY("skill_lightning", 0x081D08E8, 0x0820B188, 0, 0x08231B64),
    /* SkillCagnazzoUse's own resource (graphics 0x081DC144, palette row 3):
     * Cagnazzo's guardian (looping sequence 0). */
    ENTITY("skill_cagnazzo", 0x081DC144, 0x0820B4E4, 0, 0x082361A4),
    /* The Devil and Manticore souls' sparks (sub_0802ECF4: sequences 0-3,
     * palette bank 0 into OBJ row 3). */
    ENTITY("skill_devil_spark", 0x081DD14C, 0x0820B52C, 0, 0x0823641C),
    /* SkillBigGolemUse's golem (sequences 0 and 1, palette bank 0 into OBJ
     * row 3). */
    ENTITY("skill_big_golem", 0x0825FB1C, 0x082664B8, 0, 0x082677B8),
    /* SkillAluraUneUse's fairy (palette bank 0 into OBJ row 3). */
    ENTITY("skill_alura_une", 0x081DE154, 0x0820B574, 0, 0x082368A8),
    /* SkillShadowKnightUse's knight (sequences 0 and 1, palette bank 0 into
     * OBJ row 3). */
    ENTITY("skill_shadow_knight", 0x081DF15C, 0x0820B5BC, 0, 0x0823717C),
    /* SkillGreatArmorUse's armor (sequence 1, palette bank 0 into OBJ row 3). */
    ENTITY("skill_great_armor", 0x081E116C, 0x0820B64C, 0, 0x0823760C),
    /* SkillPersephoneUse's maid (palette bank 0 into OBJ row 3). */
    ENTITY("skill_persephone", 0x081E0968, 0x0820B628, 0, 0x08237464),
    /* SkillDeathUse's sickle (sub_08030238, palette bank 0 into OBJ row 3). */
    ENTITY("skill_death", 0x081E2174, 0x0820B694, 0, 0x08237770),
    /* SkillFlyingArmorUse's armor (palette bank 0 loaded into OBJ row 3). */
    ENTITY("skill_flying_armor", 0x0825E08C, 0x0825E914, 0, 0x0825EAC4),
    /* Skill_Witch_GiantGhost_Use's own resources (palette row 3): the
     * Giant Ghost (looping sequence 1) and the Student Witch (sequence 0). */
    ENTITY("skill_giant_ghost", 0x081E0968, 0x0820B628, 0, 0x08237464),
    ENTITY("skill_witch", 0x081DE958, 0x0820B598, 0, 0x0823693C),
    /* SkillBonePillarUse's own resource (palette row 3): its fires
     * (sub_0802E638: sequence 0 once). */
    ENTITY("skill_bone_pillar", 0x081DC948, 0x0820B508, 0, 0x082362B8),
    /* sub_0802FDE4's own resource (graphics 0x081E0164, palette row 3):
     * the Buer soul's flames (looping sequence 0). */
    ENTITY("skill_buer", 0x081E0164, 0x0820B604, 0, 0x08237304),
    /* SkillCatoblepasUse's resource (graphics 0x081E2978, palette row 3):
     * the gas clouds of sub_08030D6C (sequence 0 once). */
    ENTITY("skill_catoblepas", 0x081E2978, 0x0820B6B8, 0, 0x08237884),
    /* SkillImpUse's resource (graphics 0x081E317C, palette row 3): the
     * Imp familiar (looping sequence 0 and its frame attack boxes). */
    ENTITY("skill_imp", 0x081E317C, 0x0820B6DC, 0, 0x08237AC4),
    /* sub_0802EE40's resource (graphics 0x081E116C, palette row 3): the
     * Curly soul's shell around the transformed Soma (looping sequence 0). */
    ENTITY("skill_curly", 0x081E116C, 0x0820B64C, 0, 0x0823760C),
    /* Resource 7 of 0x080E13BC: the Skeleton soul's bone. */
    /* Its still frames 0 (the bone) and 3 (the grenade of parameter 5), with
     * the walker probes 0x084F1388 of the bouncing throws (flag 4). */
    {"skill_bone", 0x081CF8E0, 0x0820B140, 0, 0x08231740, 0x084F1388, 0, 0, 2, {0, 3}},
    /* The breakable walls (objects 0x08 and 0x09): entry n of 0x08526064
     * shows one frame of descriptor 0x08526004[+ 0] with palette bank
     * + 2 (sub_0806E0D0). */
    STATIC("wall_0", 0x081CBE1C, 0x08209C88, 0, 0x0820F314, 1, 0),
    STATIC("wall_1", 0x081CBE1C, 0x08209C88, 1, 0x0820F314, 1, 1),
    STATIC("wall_2", 0x081CBE1C, 0x08209C88, 2, 0x0820F314, 1, 2),
    STATIC("wall_3", 0x081CBE24, 0x08209D6C, 0, 0x0820F3E8, 1, 0),
    STATIC("wall_4", 0x081CBE24, 0x08209D6C, 1, 0x0820F3E8, 1, 1),
    STATIC("wall_5", 0x081CBE24, 0x08209D6C, 2, 0x0820F3E8, 1, 1),
    STATIC("wall_6", 0x081CBE2C, 0x08209DF0, 0, 0x0820F4D4, 1, 0),
    STATIC("wall_7", 0x081CBE2C, 0x08209DF0, 1, 0x0820F4D4, 1, 1),
    STATIC("wall_8", 0x081CBEC4, 0x0820A100, 4, 0x082112E8, 1, 0),
    STATIC("wall_9", 0x082591D4, 0x0825C224, 0, 0x0825C3F4, 1, 0),
    STATIC("wall_10", 0x082591D4, 0x0825C224, 0, 0x0825C3F4, 1, 1),
    STATIC("wall_11", 0x082591CC, 0x08209C88, 4, 0x0820F314, 1, 3),
    STATIC("wall_12", 0x082591CC, 0x08209C88, 5, 0x0820F314, 1, 4),
    STATIC("wall_13", 0x082591CC, 0x08209C88, 3, 0x0820F314, 1, 5),
    STATIC("wall_14", 0x081CBE24, 0x08209D6C, 2, 0x0820F3E8, 1, 3),
    STATIC("wall_15", 0x0825E07C, 0x08209D6C, 3, 0x082669A8, 1, 1),
    STATIC("wall_16", 0x0825E07C, 0x08209D6C, 3, 0x082669A8, 1, 0),
    STATIC("wall_17", 0x0825EADC, 0x08209C88, 6, 0x0820F314, 1, 6),
    /* EnemyMinotaurCreate/EnemyRedMinotaurCreate (0x080B0D2C/0x080B0D44)
     * share func_080B0AD4, which loads graphics 0x081E41A4, palette
     * 0x0820B7B0 (two banks: bank 0 brown, bank 1 red) and frames
     * 0x0823AE10 through func_0806E0D0. */
    PROBES("minotaur", 0x081E41A4, 0x0820B7B0, 0, 0x0823AE10, 0x085286F8),
    PROBES("red_minotaur", 0x081E41A4, 0x0820B7B0, 1, 0x0823AE10, 0x085286F8),
    /* EnemyGiantWormCreate / EnemyPoisonWormCreate share
     * sub_080AD7F0: graphics 0x081E4184, palette 0x0820B724 with the
     * variant as bank (sub_08068264), frames 0x08237C44. */
    PROBES("giant_worm", 0x081E4184, 0x0820B724, 0, 0x08237C44, 0x085286F8),
    PROBES("poison_worm", 0x081E4184, 0x0820B724, 1, 0x08237C44, 0x085286F8),
    /* EnemyBiphronCreate / sub_0809A3D8: graphics 0x081CC05C, palette
     * 0x0820AC04, frames 0x08223FF8. Its sub_08069A00 hover walker
     * reads the entity box record, not a probe table; the shared
     * 0x085286F8 descriptor stands in for the floor snap. */
    PROBES("biphron", 0x081CC05C, 0x0820AC04, 0, 0x08223FF8, 0x085286F8),
    /* EnemyEvilButcherCreate / the cleaver, gore and particle children
     * (sub_080BF738 / sub_080BF990 / sub_080BF6B8): graphics 0x082052D0,
     * palette 0x0820C27C with parameter 1 as bank (sub_08068264), frames
     * 0x08256720, the state-0 walker probes 0x085288FC (sub_0806CAF8
     * mode 0xC). Still frames 6 (the death pose), 8 (cleaver and gore)
     * and 9 (particle) are the children's + 0x65 direct frames. */
    {"evil_butcher", 0x082052D0, 0x0820C27C, 0, 0x08256720, 0x085288FC,
     0, 0, 3, {6, 8, 9}},
    ENTITY("evil_butcher_bank1", 0x082052D0, 0x0820C27C, 1, 0x08256720),
    /* EnemyKillerMantleCreate / the afterimage and orbit children
     * (sub_080BB054 / sub_080BB0F0): graphics 0x081F421C, palette
     * 0x0820BD04, frames 0x0824AE0C, the walker probes 0x085288B4
     * (sub_0806CAF8 mode 0). Still frame 0x12 is the orbit blade's
     * + 0x65 direct frame. */
    {"killer_mantle", 0x081F421C, 0x0820BD04, 0, 0x0824AE0C, 0x085288B4,
     0, 0, 1, {0x12}},
    /* EnemyDryadCreate / the seed child (sub_080AE96C / sub_080AE7E4):
     * tiles 0x081E418C, palette 0x0820B768, frames 0x082388AC. The Dryad
     * never walks — no probe table; the seed shares the same resources
     * and its landed animation table is 0x08528604. */
    {"dryad", 0x081E418C, 0x0820B768, 0, 0x082388AC, 0,
     0, 0, 0, {0}},
    /* EnemyTsuchinokoCreate / the spit and rock children
     * (sub_080A776C / sub_080A78A4): tiles 0x081CC0B4, palette
     * 0x0820AFDC bank 0, frames 0x0822952C — the children share the
     * same resources. The snake never walks a probe table (its hop
     * uses sub_08069A00 collision probes instead). */
    {"tsuchinoko", 0x081CC0B4, 0x0820AFDC, 0, 0x0822952C, 0,
     0, 0, 0, {0}},
    /* EnemyKickerSkeletonCreate (sub_080C0324): tiles 0x08259214,
     * palette 0x0825C320, frames 0x0825DA58; its attack walks the
     * 0x08528908 probe table (sub_0806CAF8 mode 0). */
    {"kicker_skeleton", 0x08259214, 0x0825C320, 0, 0x0825DA58,
     0x08528908, 0, 0, 0, {0}},
    /* EnemyWaiterSkeletonCreate (sub_080AF574) and its companion
     * (sub_080AEC0C) and tray (sub_080AED28) children, which share the
     * same resources: tiles 0x081E4194, palette 0x0820BE3C, frames
     * 0x082393E0; the state-2 walker probes 0x08528608
     * (sub_0806CAF8 mode 0xC). */
    {"waiter_skeleton", 0x081E4194, 0x0820BE3C, 0, 0x082393E0,
     0x08528608, 0, 0, 0, {0}},
    /* sub_080BBFF0 / the shadow (sub_080BB924) and rock (sub_080BBAC4)
     * children, which share the same resources: tiles 0x081E41CC,
     * palette 0x0820B8A4, frames 0x08242994; the state-0/1 walker
     * probes 0x085288C0 (sub_0806CAF8 mode 0x14) and the rock's direct
     * frame 1 (+ 0x65). */
    {"cockatrice", 0x081E41CC, 0x0820B8A4, 0, 0x08242994,
     0x085288C0, 0, 0, 1, {1}},
    /* EnemyBasiliskCreate (0x080BC1F8) is a thunk to sub_080BBFF0 with
     * variant 1: the same tiles 0x081E41CC, frames 0x08242994 and
     * probes 0x085288C0, but palette bank 1 (the sub_08068264 variant
     * slot) of 0x0820B8A4; EnemyBasiliskUpdate (0x080BC204) is
     * byte-identical to EnemyCockatriceUpdate (0x080BC164) and shares
     * its state handlers, shadow and rock children. */
    {"basilisk", 0x081E41CC, 0x0820B8A4, 1, 0x08242994,
     0x085288C0, 0, 0, 1, {1}},
    /* EnemyChronomageCreate (0x080B712C) and its sub_080B6C3C wisp
     * child, which share the same resources: tiles 0x081EC1EC,
     * palette 0x0820BBB0, frames 0x08245CA8; the state-2 chase probes
     * 0x08528824 (sub_0806CAF8 mode 2). */
    {"chronomage", 0x081EC1EC, 0x0820BBB0, 0, 0x08245CA8,
     0x08528824, 0, 0, 0, {0}},
    /* EnemyMandragoraCreate (0x080CEF34) and its sub_080CEFE8 body /
     * sub_080CF2B8 tail children plus the sub_080CF804 seed volley,
     * which share the same resources: tiles 0x081F429C, palette
     * 0x0820C004, frames 0x08250E94; the head's + 0x65 = 0xA attack
     * frame and the seeds' static frames 0x1F-0x23. */
    {"mandragora", 0x081F429C, 0x0820C004, 0, 0x08250E94,
     0, 0, 0, 6, {0x0A, 0x1F, 0x20, 0x21, 0x22, 0x23}},
    /* EnemyDiscArmorCreate (0x080B2228) and its sub_080B1678 disc,
     * sub_080B1428 shards and sub_080B14B4 glow, which share the same
     * resources: tiles 0x081E41B4, palette 0x0820B818, frames
     * 0x0823D78C; the disc's animation 6, the glow's 7 and the shards'
     * 4-5. */
    {"disc_armor", 0x081E41B4, 0x0820B818, 0, 0x0823D78C,
     0, 0, 0, 0, {0}},
    /* EnemyAluraUneCreate (0x0807C0A0) through sub_0807CF20, shared by
     * the boss and the sub_0807C9A4 scythes and sub_0807CC9C orbit
     * blades: tiles 0x081CBF9C, palette 0x0820A674, frames
     * 0x08218D3C. */
    {"alura_une", 0x081CBF9C, 0x0820A674, 0, 0x08218D3C,
     0, 0, 0, 0, {0}},
};

/* Room SPECIAL objects (kind 2). Each ObjectXXCreate feeds sub_0806E0D0
 * hardcoded {graphics, palette, bank, frames} resources; param0 selects the
 * behavior table, not the sprite. Only ids whose create resolves cleanly are
 * listed; the rest use the +0x30 packed-record path and are not exported yet. */
/* SPECIAL object ids are the create/update table indices (kind-2 entity
 * id). Resources were verified against the ROM table sUnk_084F0DF8: the
 * create address of each id was matched to its ObjectXXCreate label, so
 * the decompiler's hex suffix IS the entity id (Object13Create = id 0x13
 * = 19). Ids 7, 10, 14, 15, 17 and 18 do not pass hard-coded resources:
 * 14 and 18 index param0-driven tables (0x085262BC/0x08526214 and
 * 0x085263A8), 15 uses the shared flame frames 0x0820ED60, 17 is a
 * spawner and 7/10 dispatch indirectly; they are decoded separately. */
static const EntitySpec special_objects[] = {
    ENTITY("special_13", 0x081CBE74, 0x08209F54, 0, 0x0821017C),
    ENTITY("special_16", 0x08268450, 0x082097B0, 0, 0x08268648),
    ENTITY("special_19", 0x081CBE7C, 0x08209FD8, 0, 0x08210264),
    ENTITY("special_20", 0x081CBEA4, 0x0820A0DC, 0, 0x08210B60),
    ENTITY("special_21", 0x081CBEE4, 0x0820A1C4, 8, 0x082115F8),
    ENTITY("special_23", 0x081CBE9C, 0x08209FD8, 4, 0x08210A90),
    ENTITY("special_24", 0x0825EAF4, 0x082663E4, 0, 0x08266C00),
    ENTITY("special_25", 0x0825EAFC, 0x08266408, 0, 0x08266C50),
    ENTITY("special_27", 0x081CBE84, 0x08209FD8, 1, 0x08210318),
};

/* SPECIAL ids whose create resolves sprites through param0. Names carry
 * "special_<id>_v<param0>" and the runtime maps entity->param0 to the
 * same variant. Id 14: param0 indexes the 12-byte records at 0x085262BC
 * (room-used values 0-14); record byte0 selects a {gfx,palette,frames}
 * triple at 0x08526214 + group*12 (groups 0-13) and byte1 the palette
 * bank argument. Id 18: param0 indexes the 24-byte records at
 * 0x085263A8 (gfx@0, palette@4, frames@8, bank@0xC), values 0-3. */
static const EntitySpec special14_variants[] = {
    ENTITY("special_14_v00", 0x081CBE34, 0x08209E34, 0, 0x0820F64C),
    ENTITY("special_14_v01", 0x081CBE3C, 0x08209E58, 0, 0x0820F7C4),
    ENTITY("special_14_v02", 0x081CBE44, 0x08209E7C, 0, 0x0820F944),
    ENTITY("special_14_v03", 0x081CBE4C, 0x08209EA0, 0, 0x0820FAC4),
    ENTITY("special_14_v04", 0x081CBE54, 0x08209EC4, 0, 0x0820FC3C),
    ENTITY("special_14_v05", 0x082591DC, 0x0825C248, 0, 0x0825C574),
    ENTITY("special_14_v06", 0x082591E4, 0x0825C26C, 0, 0x0825C6F4),
    ENTITY("special_14_v07", 0x082591EC, 0x0825C290, 0, 0x0825C8D0),
    ENTITY("special_14_v08", 0x082591F4, 0x0825C2B4, 0, 0x0825C9A0),
    ENTITY("special_14_v09", 0x082591FC, 0x0825C2D8, 0, 0x0825CA70),
    ENTITY("special_14_v10", 0x08259204, 0x0825C2FC, 0, 0x0825CB40),
    ENTITY("special_14_v11", 0x0825EAE4, 0x0826639C, 0, 0x08266A70),
    ENTITY("special_14_v12", 0x0825EAEC, 0x082663C0, 0, 0x08266B38),
    ENTITY("special_14_v13", 0x0826800C, 0x08268014, 0, 0x0826822C),
    ENTITY("special_14_v14", 0x0826800C, 0x08268014, 1, 0x0826822C),
};

static const EntitySpec special18_variants[] = {
    ENTITY("special_18_v00", 0x081CBE64, 0x08209F0C, 0, 0x0820FFB4),
    ENTITY("special_18_v01", 0x081CBE6C, 0x08209F30, 0, 0x08210028),
    ENTITY("special_18_v02", 0x081CBEAC, 0x0820A100, 0, 0x08210C30),
    ENTITY("special_18_v03", 0x081CBEAC, 0x0820A100, 0, 0x08210C30),
};

/* Id 15 (Object0FCreate): a shared-flame object. sub_0803AFB8 resolves
 * the palette slot of tile sheet 0x081C15F4 to the 16-bank palette
 * 0x0826F8A4 (the state-3 alternative 0x082718A8 is not selected for
 * Soma; divergence noted) and sub_0803B924 draws frames 0x0820ED60 with
 * OBJ palette row param0 >> 8, defaulting to 7 when the high byte is 0.
 * The variant suffix carries the resolved row, not the raw param0: the
 * runtime compares (param0 >> 8) ?: 7 for this id. Rows seen in rooms:
 * 6 (param0 0x600), 7 (param0 0 and low-byte variants) and 9 (0x900). */
static const EntitySpec special15_variants[] = {
    ENTITY("special_15_v6", 0x081C15F4, 0x0826F8A4, 6, 0x0820ED60),
    ENTITY("special_15_v7", 0x081C15F4, 0x0826F8A4, 7, 0x0820ED60),
    ENTITY("special_15_v9", 0x081C15F4, 0x0826F8A4, 9, 0x0820ED60),
};

static const EffectSpec effects[] = {
    {"flame", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 1, {0}, -1},
    {"blood", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 1, {13}, -1},
    {"smoke", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 1, {5}, -1},
    {"dark_smoke", 0x081C15F4, 0x082099FC, 1, 0x0820ED60, 1, {5}, -1},
    {"fire", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 1, {1}, -1},
    {"spark", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 1, {16}, -1},
    /* SkillGreatArmorUse's sparks: animation 16 on OBJ row 7 (bank 3). */
    {"spark_7", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 1, {16}, -1},
    {"dust", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 1, {4}, -1},
    {"impact", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 1, {7}, -1},
    {"ember", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 1, {1}, -1},
    {"casing", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 0, {0}, 53},
    /* The arts of burst list 0 of sub_08045A98 not shared with the above. */
    {"burst_1", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 1, {1}, -1},
    {"burst_12", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 1, {12}, -1},
    {"burst_15", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 1, {15}, -1},
    /* sub_08045B44 type 0: still frame 0x36, OBJ row 6 (bank 2). */
    {"sparkle", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 0x36},
    /* sub_08025540: the two randomly selected still frames of Rock Armor's
     * four landing fragments, both on OBJ row 6 (palette bank 2). */
    {"rock_fragment_0", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 0x40},
    {"rock_fragment_1", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 0x41},
    /* sub_0804FA88: the third still frame of a breakable wall's debris. */
    {"rock_fragment_2", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 0x42},
    /* sub_0804F9B4: the pieces of the object 0x09 grid wall (entry 8 of
     * 0x08526064: descriptor 5 of 0x08526004, bank 4), its frames 1 and 2. */
    {"wall_piece_1", 0x081CBEC4, 0x0820A100, 4, 0x082112E8, 0, {0}, 1},
    {"wall_piece_2", 0x081CBEC4, 0x0820A100, 4, 0x082112E8, 0, {0}, 2},
    /* Object20's sub_0805ED80 attack: common animation 0x16 with palette
     * bank 6 (OBJ row 9), aimed at the principal Winged Skeleton. */
    {"opening_projectile", 0x081C15F4, 0x082099FC, 6, 0x0820ED60, 1, {0x16}, -1},
    /* sub_0806B5DC's elemental hit variants selected by sub_0806B7D8.
     * Types 3 and 5 are the two arts not already exported for another
     * shared effect (animations 15/row 6 and 20/row 7). */
    {"hit_element_3", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 1, {15}, -1},
    {"hit_element_5", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 1, {20}, -1},
    /* sub_08042B8C / sub_0804301C: damage digits 0-9, minus, plus and
     * the zero-damage guard still, all on OBJ row 6 (palette bank 2). */
    {"damage_0", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 0},
    {"damage_1", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 1},
    {"damage_2", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 2},
    {"damage_3", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 3},
    {"damage_4", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 4},
    {"damage_5", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 5},
    {"damage_6", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 6},
    {"damage_7", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 7},
    {"damage_8", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 8},
    {"damage_9", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 9},
    {"damage_10", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 10},
    {"damage_11", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 11},
    {"damage_guard", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 0x58},
    /* Pickups. sub_0803B924's attribute bits are the OBJ palette row,
     * which sub_0800C5D8 fills from bank sUnk_084F0DD8[row] of 0x082099FC
     * (rows 6, 7, 8, 9 -> banks 2, 3, 4, 6). Small coins (money table rows
     * 0-3: rows 6, 7, 9, 8) loop sequence 21 (sub_0803F2C8), bags (rows 4-6:
     * rows 6, 7, 8) show frame 33, hearts (row 7) frames 40 (small) and 39
     * (big), souls (rows of 0x080E1CD0: 7, 6, 8, 9) loop sequence 6.
     * sub_08044980's fixed soul containers use the same four rows: primary
     * sequence 19 and follower sequence 18. */
    {"coin0", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 1, {21}, -1},
    {"coin1", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 1, {21}, -1},
    {"coin2", 0x081C15F4, 0x082099FC, 6, 0x0820ED60, 1, {21}, -1},
    {"coin3", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 1, {21}, -1},
    {"bag4", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 33},
    {"bag5", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 0, {0}, 33},
    {"bag6", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 0, {0}, 33},
    {"heart_small", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 0, {0}, 40},
    {"heart_big", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 0, {0}, 39},
    {"soul0", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 1, {6}, -1},
    {"soul1", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 1, {6}, -1},
    {"soul2", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 1, {6}, -1},
    {"soul3", 0x081C15F4, 0x082099FC, 6, 0x0820ED60, 1, {6}, -1},
    {"soul_container0", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 1, {19}, -1},
    {"soul_container1", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 1, {19}, -1},
    {"soul_container2", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 1, {19}, -1},
    {"soul_container3", 0x081C15F4, 0x082099FC, 6, 0x0820ED60, 1, {19}, -1},
    {"soul_container_follower0", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 1, {18}, -1},
    {"soul_container_follower1", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 1, {18}, -1},
    {"soul_container_follower2", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 1, {18}, -1},
    {"soul_container_follower3", 0x081C15F4, 0x082099FC, 6, 0x0820ED60, 1, {18}, -1},
    /* sub_08045BC8/sub_08045164: the soul-absorbed sparkle pinned above
     * Soma, animation sequence 12, palette nibble 6 (OBJ row 9). */
    {"soul_held", 0x081C15F4, 0x082099FC, 6, 0x0820ED60, 1, {12}, -1},
    /* sub_0801B03C's corpse sparkle: sub_08045CEC(x, y, 0xD, 8) plays
     * animation 13 of 0x0820ED60; arg3's nibble 8 selects the OBJ palette
     * row while the colours come from the tileset's palette bank (4, the
     * same sequence-13 bank as blood). */
    {"death_sparkle", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 1, {13}, -1},
    /* sub_080455BC's level-up display: entity +0x65 = record byte 0
     * (0x085251F0) = still frame 0x48; sub_0803B924's nibble 6 selects
     * OBJ row 6, palette bank 2 through sUnk_084F0DD8. */
    {"level_up", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 0x48},
};

/* 0x0852513C, selected by sub_08045B44 type 4: its four type-1 records are
 * (frame, ticks) = (0x49,3), (0x36,7), (0x35,7), (0x34,3). */
static const DirectEffectSpec direct_effects[] = {
    {"soul_container_spark0", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 4,
     {0x49, 0x36, 0x35, 0x34}, {3, 7, 7, 3}},
    {"soul_container_spark1", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 4,
     {0x49, 0x36, 0x35, 0x34}, {3, 7, 7, 3}},
    {"soul_container_spark2", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 4,
     {0x49, 0x36, 0x35, 0x34}, {3, 7, 7, 3}},
    {"soul_container_spark3", 0x081C15F4, 0x082099FC, 6, 0x0820ED60, 4,
     {0x49, 0x36, 0x35, 0x34}, {3, 7, 7, 3}},
    /* sub_08045350's storm sparkles (sub_080451F4): frames {0x49,0x36,
     * 0x35} held 8/8/5 updates — the 21-frame life ends before the table's
     * fourth frame — one variant per soul-kind palette bank. */
    {"soul_storm0", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 3,
     {0x49, 0x36, 0x35}, {8, 8, 5}},
    {"soul_storm1", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 3,
     {0x49, 0x36, 0x35}, {8, 8, 5}},
    {"soul_storm2", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 3,
     {0x49, 0x36, 0x35}, {8, 8, 5}},
    {"soul_storm3", 0x081C15F4, 0x082099FC, 6, 0x0820ED60, 3,
     {0x49, 0x36, 0x35}, {8, 8, 5}},
};

#undef ENTITY
#undef STATIC
#undef PROBES

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static bool text_append(Text *text, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    va_list copy;
    va_copy(copy, arguments);
    int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(arguments);
        return false;
    }
    size_t total = text->size + (size_t)needed + 1;
    if (total > text->capacity) {
        size_t next = text->capacity ? text->capacity : 4096;
        while (next < total) {
            if (next > SIZE_MAX / 2) {
                va_end(arguments);
                return false;
            }
            next *= 2;
        }
        char *grown = realloc(text->data, next);
        if (!grown) {
            va_end(arguments);
            return false;
        }
        text->data = grown;
        text->capacity = next;
    }
    vsnprintf(text->data + text->size, text->capacity - text->size,
              format, arguments);
    va_end(arguments);
    text->size += (size_t)needed;
    return true;
}

static bool pointer_offset(const FusionRom *rom, uint32_t pointer, size_t size,
                           uint32_t *offset, const char *label,
                           char *error, size_t error_size) {
    if (!rom || !rom->data || pointer < GBA_BASE ||
        (uint64_t)pointer - GBA_BASE > rom->size ||
        size > rom->size - ((size_t)pointer - GBA_BASE)) {
        char message[160];
        snprintf(message, sizeof message, "truncated %s", label);
        set_error(error, error_size, message);
        return false;
    }
    *offset = pointer - GBA_BASE;
    return true;
}

static const uint8_t *slice(const FusionRom *rom, uint32_t pointer, size_t size,
                            const char *label, char *error, size_t error_size) {
    uint32_t offset = 0;
    if (!pointer_offset(rom, pointer, size, &offset, label, error, error_size))
        return NULL;
    return rom->data + offset;
}

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | (uint16_t)bytes[1] << 8);
}

static int16_t read_s16(const uint8_t *bytes) {
    return (int16_t)read_u16(bytes);
}

static uint32_t read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
           (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static bool load_tile_sheet(const FusionRom *rom, uint32_t descriptor,
                            TileSheet *sheet, char *error, size_t error_size) {
    *sheet = (TileSheet){0};
    const uint8_t *header = slice(rom, descriptor, 4, "graphics descriptor",
                                  error, error_size);
    /* Encoding 2/3 containers carry a block count in header[1]
     * instead of the depth byte; every other encoding needs depth
     * 4. */
    if (!header ||
        (header[0] != 2 && header[0] != 3 && header[1] != 4) ||
        header[2] == 0 || header[3] == 0) {
        set_error(error, error_size, "unsupported object graphics descriptor");
        return false;
    }
    sheet->width_tiles = header[2];
    sheet->size = (size_t)header[2] * header[3] * TILE_BYTES;
    sheet->data = malloc(sheet->size);
    if (!sheet->data) {
        set_error(error, error_size, "cannot allocate object tile sheet");
        return false;
    }
    if (header[0] == 0) {
        const uint8_t *tiles = slice(rom, descriptor + 4, sheet->size,
                                     "object tiles", error, error_size);
        if (!tiles) goto failed;
        memcpy(sheet->data, tiles, sheet->size);
        return true;
    }
    if (header[0] == 2 || header[0] == 3) {
        /* A multi-sheet container (Death's and Balore's tiles, and
         * Legion's encoding-3 variant): a sequence of blocks
         * `<tag> <count> 04 02` — tag 2 for uncompressed leaves,
         * tag 3 for compressed ones — each followed by count
         * pointers to sub-descriptors: encoding 0 (depth 4, raw
         * tiles inline) or encoding 1 (`01 <depth> <w> <h> <ptr>`,
         * LZ77 tiles), each width_tiles x height_tiles tiles; the
         * game loads them into VRAM contiguously, so the sheets
         * concatenate in order (a shared sub-sheet may repeat). */
        const uint8_t tag = header[0];
        size_t total = 0;
        uint32_t offsets[32];
        uint8_t dims[32][2], comp[32];
        unsigned count = 0;
        uint32_t at = descriptor;
        for (unsigned block = 0; block < 8; ++block) {
            const uint8_t *bh = slice(rom, at, 4, "sheet block header",
                                      error, error_size);
            if (!bh || bh[0] != tag)
                break;
            /* The native loader (sub_0803A1E8) walks the pointer
             * table until the entries stop being sheet descriptors;
             * the count/grid bytes vary per object (Belmont's
             * `02 03 02 01` heads four 16x8 sheets), so scan while
             * each word still points at a valid sub-sheet. */
            unsigned n = 0;
            while (n < 8 && count + n < 32) {
                const uint8_t *we = slice(rom, at + 4 + n * 4, 4,
                                          "sheet pointer", error,
                                          error_size);
                if (!we)
                    break;
                uint32_t sub = read_u32(we);
                const uint8_t *sh;
                if (sub < GBA_BASE ||
                    (uint64_t)sub - GBA_BASE + 8 > rom->size)
                    break;
                sh = rom->data + (sub - GBA_BASE);
                if ((sh[0] != 0 && sh[0] != 1) || sh[1] != 4 ||
                    sh[2] == 0 || sh[3] == 0 || sh[2] > 64 ||
                    sh[3] > 64)
                    break;
                offsets[count] = sub;
                dims[count][0] = sh[2];
                dims[count][1] = sh[3];
                comp[count] = (uint8_t)(sh[0] == 1);
                total += (size_t)sh[2] * sh[3] * TILE_BYTES;
                if (count == 0) {
                    sheet->width_tiles = sh[2];
                } else if (sh[2] != sheet->width_tiles) {
                    set_error(error, error_size,
                              "object sub-sheets differ in width");
                    goto failed;
                }
                count++;
                n++;
            }
            if (n == 0)
                break;
            at += 4 + n * 4;
        }
        if (count == 0) {
            char detail[160];
            snprintf(detail, sizeof detail,
                     "unsupported object sheet count at 0x%08X", descriptor);
            set_error(error, error_size, detail);
            goto failed;
        }
        free(sheet->data);
        sheet->data = malloc(total);
        sheet->size = total;
        if (!sheet->data) {
            set_error(error, error_size, "cannot allocate object tile sheet");
            sheet->data = NULL;
            return false;
        }
        size_t pos = 0;
        for (unsigned i = 0; i < count; ++i) {
            size_t n = (size_t)dims[i][0] * dims[i][1] * TILE_BYTES;
            if (!comp[i]) {
                const uint8_t *tiles = slice(rom, offsets[i] + 4, n,
                                             "sub-sheet tiles", error,
                                             error_size);
                if (!tiles) goto failed;
                memcpy(sheet->data + pos, tiles, n);
            } else {
                const uint8_t *tp = slice(rom, offsets[i] + 4, 4,
                                          "sub-sheet tile pointer",
                                          error, error_size);
                if (!tp) goto failed;
                uint32_t off = 0;
                if (!pointer_offset(rom, read_u32(tp), 4, &off,
                                    "sub-sheet tile data", error,
                                    error_size)) goto failed;
                size_t expanded = fusion_gba_uncompressed_size(
                    rom->data + off, rom->size - off);
                if (expanded < n + 4) {
                    set_error(error, error_size,
                              "compressed sub-sheet tiles are short");
                    goto failed;
                }
                uint8_t *decoded = malloc(expanded);
                size_t written = 0;
                if (!decoded || !fusion_gba_lz77_uncompress(
                        rom->data + off, rom->size - off, decoded,
                        expanded, &written) || written < n + 4) {
                    free(decoded);
                    set_error(error, error_size,
                              "invalid compressed sub-sheet tiles");
                    goto failed;
                }
                size_t skip = written >= n + 4 && decoded[0] == 0 ? 4 : 0;
                if (written - skip < n) {
                    free(decoded);
                    set_error(error, error_size,
                              "compressed sub-sheet tiles are short");
                    goto failed;
                }
                memcpy(sheet->data + pos, decoded + skip, n);
                free(decoded);
            }
            pos += n;
        }
        return true;
    }
    if (header[0] == 1) {
        const uint8_t *entry = slice(rom, descriptor + 4, 4, "tile pointer",
                                     error, error_size);
        uint32_t offset = 0;
        if (!entry || !pointer_offset(rom, read_u32(entry), 4, &offset,
                                      "tile data", error, error_size)) goto failed;
        uint32_t expanded = fusion_gba_uncompressed_size(
            rom->data + offset, rom->size - offset);
        if (expanded < sheet->size) {
            set_error(error, error_size,
                      "compressed object tiles are shorter than their sheet");
            goto failed;
        }
        uint8_t *decoded = malloc(expanded);
        size_t written = 0;
        if (!decoded || !fusion_gba_lz77_uncompress(
                rom->data + offset, rom->size - offset, decoded, expanded,
                &written) || written < sheet->size) {
            free(decoded);
            set_error(error, error_size, "invalid compressed object tiles");
            goto failed;
        }
        /* The decompressed resource starts with the 4-byte header of an
         * uncompressed one (encoding 0, depth, size / 256), which the game
         * skips (aos_rom_load_graphics_resource does the same); copying it
         * shifted every tile down one row. */
        size_t skip = written >= sheet->size + 4 && decoded[0] == 0 ? 4 : 0;
        memcpy(sheet->data, decoded + skip, sheet->size);
        free(decoded);
        return true;
    }
    {
        char message[96];
        snprintf(message, sizeof message,
                 "unsupported object graphics encoding %u", header[0]);
        set_error(error, error_size, message);
    }
failed:
    free(sheet->data);
    *sheet = (TileSheet){0};
    return false;
}

static const uint8_t *load_palette(const FusionRom *rom, uint32_t descriptor,
                                   unsigned bank, char *error,
                                   size_t error_size) {
    const uint8_t *header = slice(rom, descriptor, 4, "palette descriptor",
                                  error, error_size);
    if (!header || header[0] != 0 || bank >= header[2]) {
        set_error(error, error_size, "unsupported object palette bank");
        return NULL;
    }
    return slice(rom, descriptor + 4 + bank * 32, 32, "object palette",
                 error, error_size);
}

static bool read_components(const FusionRom *rom, uint32_t descriptor,
                            uint8_t frame, Component components[256],
                            size_t *count, const uint8_t **record,
                            char *error, size_t error_size) {
    const uint8_t *header = slice(rom, descriptor, 8, "frame descriptor",
                                  error, error_size);
    if (!header) return false;
    uint16_t record_count = read_u16(header);
    uint32_t records = read_u32(header + 4);
    if (frame >= record_count) {
        set_error(error, error_size, "object frame outside its frame list");
        return false;
    }
    *record = slice(rom, records + (uint32_t)frame * 16, 16,
                    "object frame record", error, error_size);
    if (!*record) return false;
    *count = (*record)[5];
    uint32_t pointer = read_u32(*record + 12);
    for (size_t index = 0; index < *count; ++index) {
        const uint8_t *source = slice(rom, pointer + (uint32_t)index * 12, 12,
                                      "OAM component", error, error_size);
        if (!source) return false;
        if (read_u16(source + 2) || source[4] % 8 || source[5] % 8 ||
            source[6] % 8 || source[7] % 8) {
            set_error(error, error_size, "unsupported object OAM component");
            return false;
        }
        components[index] = (Component){
            (int8_t)source[0], (int8_t)source[1], source[4], source[5],
            source[6], source[7], read_u32(source + 8)};
    }
    return true;
}

static uint8_t expand5(unsigned value) { return (uint8_t)(value * 255 / 31); }

static bool render_frame(const FusionRom *rom, uint32_t descriptor,
                         const TileSheet *sheet, const uint8_t *palette,
                         uint8_t frame, RenderedFrame *rendered,
                         char *error, size_t error_size) {
    *rendered = (RenderedFrame){0};
    Component components[256];
    size_t count = 0;
    const uint8_t *record = NULL;
    if (!read_components(rom, descriptor, frame, components, &count, &record,
                         error, error_size)) return false;
    if (count) {
        const Component *last = &components[count - 1];
        rendered->anchor_x = last->x + last->width / 2;
        rendered->anchor_y = last->y + last->height / 2;
        rendered->edge_x = last->x + last->width;
        rendered->edge_y = last->y + last->height;
    }
    rendered->mode = record[4];
    if (rendered->mode) {
        const uint8_t *box = slice(rom, read_u32(record + 8),
                                   rendered->mode == 2 ? 8 : 4,
                                   "frame box", error, error_size);
        if (!box) return false;
        rendered->hurt_x = (int8_t)box[0];
        rendered->hurt_y = (int8_t)box[1];
        rendered->hurt_width = box[2];
        rendered->hurt_height = box[3];
        const uint8_t *attack = rendered->mode == 2 ? box + 4 : box;
        rendered->attack_x = (int8_t)attack[0];
        rendered->attack_y = (int8_t)attack[1];
        rendered->attack_width = attack[2];
        rendered->attack_height = attack[3];
    }
    uint8_t *rgba = calloc((size_t)CANVAS_SIZE * CANVAS_SIZE, 4);
    if (!rgba) {
        set_error(error, error_size, "cannot allocate object sprite canvas");
        return false;
    }
    size_t sheet_height = sheet->size / (sheet->width_tiles * TILE_BYTES) * 8;
    for (size_t order = count; order > 0; --order) {
        const Component *component = &components[order - 1];
        if (((component->flags >> 16) & 0xFF) >= 4) {
            /* Affine (rotated/scaled) components cannot be baked into
             * the sheet; the runtime rotates the whole entity through
             * its own transform.  Skip the component — one sprite of
             * the Gladiator flail is affected (frame 44). */
            continue;
        }
        if ((size_t)component->source_x + component->width >
                sheet->width_tiles * 8 ||
            (size_t)component->source_y + component->height > sheet_height)
            continue;
        bool vertical = (component->flags & (1u << 16)) != 0;
        bool horizontal = (component->flags & (1u << 17)) != 0;
        for (size_t y = 0; y < component->height; ++y) {
            for (size_t x = 0; x < component->width; ++x) {
                size_t source_x = component->source_x +
                    (horizontal ? component->width - 1 - x : x);
                size_t source_y = component->source_y +
                    (vertical ? component->height - 1 - y : y);
                size_t tile = source_y / 8 * sheet->width_tiles + source_x / 8;
                uint8_t packed = sheet->data[tile * TILE_BYTES +
                    source_y % 8 * 4 + source_x % 8 / 2];
                unsigned color_index = (packed >> (4 * (source_x & 1))) & 15;
                if (!color_index) continue;
                int canvas_x = CANVAS_ORIGIN + component->x + (int)x;
                int canvas_y = CANVAS_ORIGIN + component->y + (int)y;
                if (canvas_x < 0 || canvas_x >= CANVAS_SIZE ||
                    canvas_y < 0 || canvas_y >= CANVAS_SIZE) {
                    free(rgba);
                    set_error(error, error_size, "object frame exceeds 512 pixels");
                    return false;
                }
                uint16_t color = read_u16(palette + color_index * 2);
                uint8_t *pixel = rgba +
                    ((size_t)canvas_y * CANVAS_SIZE + (size_t)canvas_x) * 4;
                pixel[0] = expand5(color & 31);
                pixel[1] = expand5((color >> 5) & 31);
                pixel[2] = expand5((color >> 10) & 31);
                pixel[3] = 255;
            }
        }
    }
    bool visible = false;
    for (size_t index = 3; index < (size_t)CANVAS_SIZE * CANVAS_SIZE * 4;
         index += 4) {
        if (rgba[index]) { visible = true; break; }
    }
    if (!visible) {
        free(rgba);
        rgba = calloc(4, 1);
        if (!rgba) {
            set_error(error, error_size, "cannot allocate empty object frame");
            return false;
        }
        rgba[3] = 255;
        bool success = fusion_sprite_bmp32_crop(
            rgba, 1, 1, 0, 0, &rendered->bmp, &rendered->bmp_size,
            &rendered->left, &rendered->top, error, error_size);
        free(rgba);
        return success;
    }
    bool success = fusion_sprite_bmp32_crop(
        rgba, CANVAS_SIZE, CANVAS_SIZE, CANVAS_ORIGIN, CANVAS_ORIGIN,
        &rendered->bmp, &rendered->bmp_size, &rendered->left, &rendered->top,
        error, error_size);
    free(rgba);
    return success;
}

static bool begin_and_add(FusionSpriteLibrary *library, const char *key,
                          size_t index, unsigned ticks,
                          const RenderedFrame *frame,
                          bool begin, char *error, size_t error_size) {
    return (!begin || fusion_sprite_library_begin(library, error, error_size)) &&
           fusion_sprite_library_add(library, key, index, ticks,
                                     frame->left, frame->top,
                                     frame->bmp, frame->bmp_size,
                                     error, error_size);
}

static bool append_frame_row(Text *rows, const char *name, size_t animation,
                             size_t index, uint8_t frame, uint8_t duration,
                             const RenderedFrame *rendered) {
    return text_append(rows,
        "%s\t%zu\t%zu\t%u\t%u\t%u\t%d\t%d\t%u\t%u\t%d\t%d\t%u\t%u\n",
        name, animation, index, frame, duration, rendered->mode,
        rendered->hurt_x, rendered->hurt_y,
        rendered->hurt_width, rendered->hurt_height,
        rendered->attack_x, rendered->attack_y,
        rendered->attack_width, rendered->attack_height);
}

static bool export_animation(const FusionRom *rom,
                             FusionSpriteLibrary *library,
                             const char *key_prefix, const char *name,
                             uint32_t descriptor, const TileSheet *sheet,
                             const uint8_t *palette, size_t animation_number,
                             uint32_t animation_pointer, uint8_t direct_frame,
                             bool direct, bool metadata, Text *rows,
                             char *error, size_t error_size) {
    const uint8_t *descriptor_bytes = slice(
        rom, descriptor, 16, "entity descriptor", error, error_size);
    if (!descriptor_bytes) return false;
    uint16_t record_count = read_u16(descriptor_bytes);
    size_t count = 1;
    const uint8_t *animation = NULL;
    if (!direct) {
        animation = slice(rom, animation_pointer, 4, "animation header",
                          error, error_size);
        if (!animation) return false;
        count = read_u16(animation);
        if (read_u16(animation + 2) != 1 || count > MAX_FRAMES) {
            set_error(error, error_size,
                      "unsupported entity animation encoding");
            return false;
        }
        /* Native descriptors may declare empty animations (count 0): they
         * carry no frames, so there is nothing to export for them. */
        if (count == 0) return true;
    }
    int anchors_x[MAX_FRAMES], anchors_y[MAX_FRAMES], edges_x[MAX_FRAMES], edges_y[MAX_FRAMES];
    char key[96];
    int written = snprintf(key, sizeof key, "%s/%s/anim_%zu",
                           key_prefix, name, animation_number);
    if (written < 0 || (size_t)written >= sizeof key) {
        set_error(error, error_size, "object sprite key is too long");
        return false;
    }
    for (size_t index = 0; index < count; ++index) {
        uint8_t frame = direct_frame, duration = 1;
        if (!direct) {
            const uint8_t *entry = slice(
                rom, animation_pointer + 4 + (uint32_t)index * 4, 2,
                "animation frame", error, error_size);
            if (!entry) return false;
            frame = entry[0]; duration = entry[1];
            if (frame >= record_count || duration == 0) {
                set_error(error, error_size, "invalid entity animation frame");
                return false;
            }
        }
        RenderedFrame rendered = {0};
        if (!render_frame(rom, descriptor, sheet, palette, frame, &rendered,
                          error, error_size)) return false;
        anchors_x[index] = rendered.anchor_x;
        anchors_y[index] = rendered.anchor_y;
        edges_x[index] = rendered.edge_x;
        edges_y[index] = rendered.edge_y;
        bool success = (!metadata || append_frame_row(
                            rows, name, animation_number, index,
                            frame, duration, &rendered)) &&
                       begin_and_add(library, key, index, duration, &rendered,
                                     index == 0, error, error_size);
        free(rendered.bmp);
        if (!success) return false;
    }
    if (metadata) {
        for (size_t index = 0; index < count; ++index) {
            if (!text_append(rows, "anchor\t%s\t%zu\t%zu\t%d\t%d\n",
                             name, animation_number, index,
                             anchors_x[index], anchors_y[index]) ||
                !text_append(rows, "edge\t%s\t%zu\t%zu\t%d\t%d\n",
                             name, animation_number, index,
                             edges_x[index], edges_y[index])) return false;
        }
    }
    return true;
}

static bool append_probes(const FusionRom *rom, const EntitySpec *entity,
                          Text *rows, char *error, size_t error_size) {
    if (!entity->probes) return true;
    const uint8_t *count_bytes = slice(rom, entity->probes, 2, "probe table",
                                       error, error_size);
    if (!count_bytes) return false;
    int count = read_s16(count_bytes);
    if (count < 1 || count > 8) {
        char message[128];
        snprintf(message, sizeof message, "%s: implausible probe count %d",
                 entity->name, count);
        set_error(error, error_size, message);
        return false;
    }
    const uint8_t *table = slice(rom, entity->probes, 8 + (size_t)count * 2,
                                 "probe table", error, error_size);
    if (!table || !text_append(rows, "probes\t%s", entity->name)) return false;
    for (int index = 0; index < 4 + count; ++index)
        if (!text_append(rows, "\t%d", read_s16(table + index * 2))) return false;
    return text_append(rows, "\n");
}

static bool append_volleys(const FusionRom *rom, const EntitySpec *entity,
                           Text *rows, char *error, size_t error_size) {
    for (size_t number = 0; number < entity->volley_count; ++number) {
        const uint8_t *entry = slice(
            rom, entity->volleys + (uint32_t)number * 4, 4, "volley",
            error, error_size);
        if (!entry || !text_append(rows, "volley\t%s\t%zu",
                                   entity->name, number)) return false;
        uint32_t pointer = read_u32(entry);
        for (size_t index = 0; index < 8; ++index) {
            const uint8_t *shot = slice(
                rom, pointer + (uint32_t)index * 4, 2, "volley entry",
                error, error_size);
            if (!shot) return false;
            int frame = (int8_t)shot[0];
            if (frame < 0) break;
            if (!text_append(rows, "\t%d\t%d", frame, (int8_t)shot[1]))
                return false;
        }
        if (!text_append(rows, "\n")) return false;
    }
    return true;
}

/* sub_080CBEA4 final phase reads eight consecutive (X,Y) signed
 * 16-bit pairs at 0x081194DC. Export from the locally supplied ROM only;
 * this metadata is generated under ignored assets/extracted/, not in Git. */
static bool append_buer_death_offsets(const FusionRom *rom, Text *rows,
                                      char *error, size_t error_size) {
    const uint8_t *bytes = slice(rom, BUER_DEATH_OFFSETS, 8 * 4,
                                 "Buer death offsets", error, error_size);
    if (!bytes || !text_append(rows, "buer_death_offsets\tbuer")) return false;
    for (unsigned i = 0; i < 8; ++i) {
        if (!text_append(rows, "\t%d\t%d", read_s16(bytes + 4 * i),
                         read_s16(bytes + 4 * i + 2))) return false;
    }
    return text_append(rows, "\n");
}

static bool export_entity(const FusionRom *rom, FusionSpriteLibrary *library,
                          const EntitySpec *entity, const char *key_prefix,
                          Text *rows, char *error, size_t error_size) {
    TileSheet sheet;
    if (!load_tile_sheet(rom, entity->graphics, &sheet, error, error_size)) {
        char detail[192];
        snprintf(detail, sizeof detail, "%s: %s", entity->name, error);
        set_error(error, error_size, detail);
        return false;
    }
    const uint8_t *palette = load_palette(rom, entity->palette, entity->bank,
                                          error, error_size);
    const uint8_t *descriptor = palette
        ? slice(rom, entity->frames, 16, "entity descriptor", error, error_size)
        : NULL;
    bool success = descriptor != NULL;
    uint16_t record_count = success ? read_u16(descriptor) : 0;
    uint16_t animation_count = success ? read_u16(descriptor + 2) : 0;
    uint32_t table = success ? read_u32(descriptor + 12) : 0;
    if (animation_count > MAX_ANIMATIONS) {
        set_error(error, error_size, "entity animation count exceeds native limit");
        success = false;
    }
    size_t sequence_count = animation_count ? animation_count : record_count;
    for (size_t number = 0; success && number < sequence_count; ++number) {
        uint32_t pointer = 0;
        if (animation_count) {
            const uint8_t *entry = slice(
                rom, table + (uint32_t)number * 4, 4, "animation",
                error, error_size);
            if (!entry) { success = false; break; }
            pointer = read_u32(entry);
        }
        success = export_animation(
            rom, library, key_prefix, entity->name, entity->frames, &sheet,
            palette, number, pointer, (uint8_t)number, animation_count == 0,
            true, rows, error, error_size);
    }
    if (success) success = append_probes(rom, entity, rows, error, error_size) &&
                           append_volleys(rom, entity, rows, error, error_size);
    for (size_t index = 0; success && index < entity->static_count; ++index) {
        uint8_t frame_id = entity->static_frames[index];
        RenderedFrame rendered;
        char key[96];
        snprintf(key, sizeof key, "Enemy/%s/frame_%u", entity->name, frame_id);
        success = render_frame(rom, entity->frames, &sheet, palette, frame_id,
                               &rendered, error, error_size);
        if (!success) break;
        success = begin_and_add(library, key, 0, 1, &rendered, true,
                                error, error_size) &&
                  text_append(rows, "static_anchor\t%s\t%u\t%d\t%d\n",
                              entity->name, frame_id,
                              rendered.anchor_x, rendered.anchor_y) &&
                  /* The frame record's attack box (+ 8), which sub_08042848
                   * installs for entities that use their still frame's. */
                  text_append(rows, "static_attack\t%s\t%u\t%d\t%d\t%u\t%u\n",
                              entity->name, frame_id, rendered.attack_x,
                              rendered.attack_y, rendered.attack_width,
                              rendered.attack_height);
        free(rendered.bmp);
    }
    free(sheet.data);
    return success;
}

static bool export_effect(const FusionRom *rom, FusionSpriteLibrary *library,
                          const EffectSpec *effect,
                          char *error, size_t error_size) {
    TileSheet sheet;
    if (!load_tile_sheet(rom, effect->graphics, &sheet, error, error_size))
        return false;
    const uint8_t *palette = load_palette(rom, effect->palette, effect->bank,
                                          error, error_size);
    bool success = palette != NULL;
    if (success && effect->static_frame >= 0) {
        RenderedFrame rendered;
        char key[96];
        snprintf(key, sizeof key, "Effect/%s/anim_0", effect->name);
        success = render_frame(rom, effect->frames, &sheet, palette,
                               (uint8_t)effect->static_frame, &rendered,
                               error, error_size);
        if (success) {
            success = begin_and_add(library, key, 0, 1, &rendered, true,
                                    error, error_size);
            free(rendered.bmp);
        }
    }
    const uint8_t *descriptor = success && effect->static_frame < 0
        ? slice(rom, effect->frames, 16, "entity descriptor", error, error_size)
        : NULL;
    uint16_t animation_count = descriptor ? read_u16(descriptor + 2) : 0;
    uint32_t table = descriptor ? read_u32(descriptor + 12) : 0;
    for (size_t index = 0;
         success && effect->static_frame < 0 && index < effect->animation_count;
         ++index) {
        unsigned number = effect->animations[index];
        if (number >= animation_count) {
            set_error(error, error_size, "entity animation outside its descriptor");
            success = false;
            break;
        }
        const uint8_t *entry = slice(rom, table + number * 4, 4, "animation",
                                      error, error_size);
        if (!entry) { success = false; break; }
        success = export_animation(
            rom, library, "Effect", effect->name, effect->frames, &sheet,
            palette, number, read_u32(entry), 0, false, false, NULL,
            error, error_size);
    }
    free(sheet.data);
    return success;
}

static bool export_direct_effect(const FusionRom *rom, FusionSpriteLibrary *library,
                                 const DirectEffectSpec *effect,
                                 char *error, size_t error_size) {
    TileSheet sheet;
    if (!load_tile_sheet(rom, effect->graphics, &sheet, error, error_size)) return false;
    const uint8_t *palette = load_palette(rom, effect->palette, effect->bank,
                                          error, error_size);
    bool success = palette != NULL && effect->count > 0 && effect->count <= MAX_FRAMES;
    char key[96];
    if (success && snprintf(key, sizeof key, "Effect/%s/anim_0", effect->name) >=
                       (int)sizeof key) {
        set_error(error, error_size, "direct effect sprite key is too long");
        success = false;
    }
    for (size_t index = 0; success && index < effect->count; ++index) {
        if (effect->ticks[index] == 0) {
            set_error(error, error_size, "direct effect has a zero-duration frame");
            success = false;
            break;
        }
        RenderedFrame rendered = {0};
        success = render_frame(rom, effect->frames, &sheet, palette, effect->frame[index],
                               &rendered, error, error_size);
        if (success)
            success = begin_and_add(library, key, index, effect->ticks[index], &rendered,
                                    index == 0, error, error_size);
        free(rendered.bmp);
    }
    free(sheet.data);
    return success;
}

/* The item icons of sub_0801232C: icon n (a record's + 0x2) is the 16 x 16
 * block (n - 1) % 64 of sheet sUnk_084F1060[(n - 1) / 64] (0x081C5E00,
 * 0x081C7E04, 0x081C9E08; eight per row), shown by the one-component frames
 * of 0x080E10F0 at (-8, -16) on OBJ row + 0x3 (6 for every record), which
 * sub_0800C5D8 fills with bank 2 of 0x082099FC. Each icon a record uses is
 * exported as Icon/i<n>/anim_0. */
static bool export_icons(const FusionRom *rom, FusionSpriteLibrary *library, char *error,
                         size_t error_size) {
    static const uint32_t sheets[3] = {0x081C5E00, 0x081C7E04, 0x081C9E08};
    static const struct { uint32_t table, stride, count; } tables[3] = {
        {0x08505B3C, 16, 32}, {WEAPON_STATS, 28, 59}, {ARMOR_STATS, 20, 45}};
    bool used[193] = {false};
    for (size_t table = 0; table < 3; ++table)
        for (uint32_t index = 0; index < tables[table].count; ++index) {
            const uint8_t *record = slice(rom, tables[table].table + index * tables[table].stride,
                                          4, "item record", error, error_size);
            if (!record) return false;
            if (record[3] != 6) {
                set_error(error, error_size, "item icon outside OBJ palette row 6");
                return false;
            }
            if (record[2] >= 1 && record[2] <= 192) used[record[2]] = true;
        }
    const uint8_t *palette = load_palette(rom, 0x082099FC, 2, error, error_size);
    if (!palette) return false;
    TileSheet sheet[3] = {{0}};
    bool success = true;
    for (size_t s = 0; success && s < 3; ++s)
        success = load_tile_sheet(rom, sheets[s], &sheet[s], error, error_size);
    for (unsigned icon = 1; success && icon <= 192; ++icon) {
        if (!used[icon]) continue;
        const TileSheet *source = &sheet[(icon - 1) / 64];
        unsigned slot = (icon - 1) % 64, origin_x = (slot % 8) * 16, origin_y = (slot / 8) * 16;
        uint8_t rgba[16 * 16 * 4] = {0};
        bool visible = false;
        for (unsigned y = 0; y < 16; ++y)
            for (unsigned x = 0; x < 16; ++x) {
                size_t sx = origin_x + x, sy = origin_y + y;
                size_t tile = sy / 8 * source->width_tiles + sx / 8;
                if ((tile + 1) * TILE_BYTES > source->size) continue;
                uint8_t packed = source->data[tile * TILE_BYTES + sy % 8 * 4 + sx % 8 / 2];
                unsigned color_index = (packed >> (4 * (sx & 1))) & 15;
                if (!color_index) continue;
                uint16_t color = read_u16(palette + color_index * 2);
                uint8_t *pixel = rgba + (y * 16 + x) * 4;
                pixel[0] = expand5(color & 31);
                pixel[1] = expand5((color >> 5) & 31);
                pixel[2] = expand5((color >> 10) & 31);
                pixel[3] = 255;
                visible = true;
            }
        if (!visible) continue;
        RenderedFrame rendered = {0};
        char key[48];
        snprintf(key, sizeof key, "Icon/i%u/anim_0", icon);
        success = fusion_sprite_bmp32_crop(rgba, 16, 16, 8, 16, &rendered.bmp,
                                           &rendered.bmp_size, &rendered.left, &rendered.top,
                                           error, error_size) &&
                  begin_and_add(library, key, 0, 1, &rendered, true, error, error_size);
        free(rendered.bmp);
    }
    for (size_t s = 0; s < 3; ++s) free(sheet[s].data);
    return success;
}

/* The icon each item, weapon and armor record shows (+ 0x2). */
static bool append_item_icons(const FusionRom *rom, Text *icons, char *error,
                              size_t error_size) {
    static const struct { const char *kind; uint32_t table, stride, count; } tables[3] = {
        {"item", 0x08505B3C, 16, 32}, {"weapon", WEAPON_STATS, 28, 59},
        {"armor", ARMOR_STATS, 20, 45}};
    if (!text_append(icons, "schema\tmetroidvania-aos-item-icons-v1\n"
                            "# kind\tindex\ticon\n")) return false;
    for (size_t table = 0; table < 3; ++table)
        for (uint32_t index = 0; index < tables[table].count; ++index) {
            const uint8_t *record = slice(rom, tables[table].table + index * tables[table].stride,
                                          4, "item record", error, error_size);
            if (!record || !text_append(icons, "%s\t%u\t%u\n", tables[table].kind, index,
                                        record[2]))
                return false;
        }
    return true;
}

static bool export_doors(const FusionRom *rom, FusionSpriteLibrary *library,
                         char *error, size_t error_size) {
    static const uint8_t frames[] = {0, 5};
    static const uint32_t scripts[] = {0x08525564, 0x08525574};
    TileSheet sheet;
    if (!load_tile_sheet(rom, 0x081CBE0C, &sheet, error, error_size))
        return false;
    bool success = true;
    for (size_t style = 0; success && style < 2; ++style) {
        const uint8_t *header = slice(rom, scripts[style], 2,
                                      "palette script", error, error_size);
        size_t count = header ? read_u16(header) : 0;
        if (!header || count == 0 || count > 64) {
            set_error(error, error_size, "invalid palette script length");
            success = false;
            break;
        }
        char key[40];
        snprintf(key, sizeof key, "WoodenDoor/style_%zu", style);
        for (size_t index = 0; success && index < count; ++index) {
            const uint8_t *entry = slice(
                rom, scripts[style] + 4 + (uint32_t)index * 4, 2,
                "palette script entries", error, error_size);
            const uint8_t *palette = entry
                ? load_palette(rom, 0x08209AE0, entry[0], error, error_size)
                : NULL;
            RenderedFrame rendered;
            success = palette && render_frame(
                rom, 0x0820F160, &sheet, palette, frames[style], &rendered,
                error, error_size);
            if (!success) break;
            success = begin_and_add(library, key, index, entry[1], &rendered,
                                    index == 0, error, error_size);
            free(rendered.bmp);
        }
    }
    free(sheet.data);
    return success;
}

static bool append_enemy_stats(const FusionRom *rom, Text *stats,
                               char *error, size_t error_size) {
    if (!text_append(stats,
            "schema\tmetroidvania-aos-enemy-stats-v4\n"
            "# id\thp\tfield_e\tcontact\tdefence\tweak\tresist\texperience"
            "\tcommon_item\trare_item\tsoul_rarity\tcommon_rate\trare_rate"
            "\tsoul_type\tsoul_index\tportrait_x\tportrait_y\n")) return false;
    for (size_t enemy = 0; enemy < ENEMY_COUNT; ++enemy) {
        const uint8_t *record = slice(
            rom, ENEMY_TABLE + (uint32_t)enemy * 0x24, 0x24,
            "enemy record", error, error_size);
        /* sub_0804CA18 adds the signed halfwords at + 0x1E / + 0x20 to the
         * portrait entity's 0x40 / 0x58 pixel anchor on the detail page. */
        if (!record || !text_append(
                stats, "%zu\t%u\t%u\t%u\t%u\t0x%04x\t0x%04x\t%u"
                "\t0x%04x\t0x%04x\t%u\t%u\t%u\t%u\t%u\t%d\t%d\n",
                enemy, read_u16(record + 0xC), read_u16(record + 0xE),
                record[0x13], record[0x14], read_u16(record + 0x1A),
                read_u16(record + 0x1C), read_u16(record + 0x10),
                read_u16(record + 0x8), read_u16(record + 0xA), record[0x12],
                record[0x15], record[0x16], record[0x17], record[0x18],
                read_s16(record + 0x1E), read_s16(record + 0x20))) return false;
    }
    return true;
}

/* The money pickup table of sub_08043F70 (0x085250E4, seven 12-byte
 * records): value, message, the bytes at + 0x8 (attribute bits of
 * sub_0803B924) and + 0x9 (a sequence or frame number, not traced) and the
 * lift flag. */
static bool append_money(const FusionRom *rom, Text *money, char *error, size_t error_size) {
    if (!text_append(money,
            "schema\tmetroidvania-aos-money-v1\n"
            "# variant\tvalue\ttext\tfield_8\tfield_9\tlifted\n")) return false;
    for (uint32_t variant = 0; variant < 7; ++variant) {
        const uint8_t *record = slice(rom, MONEY_TABLE + variant * 12, 12, "money record",
                                      error, error_size);
        if (!record || !text_append(money, "%u\t%u\t%u\t%u\t%u\t%u\n", variant,
                                    read_u32(record), read_u32(record + 4), record[8],
                                    record[9], read_u16(record + 10)))
            return false;
    }
    return true;
}

/* The 32 item records consumed by sub_0804B36C (0x08505B3C, 16 bytes):
 * icon (+ 0x2), effect kind (+ 0x8) and its amount or status mask (+ 0xA).
 * Kinds are fixed HP, proportional MP, status cure, harmful HP and key item. */
static bool append_item_use(const FusionRom *rom, Text *items, char *error,
                            size_t error_size) {
    if (!text_append(items, "schema\tmetroidvania-aos-item-use-v1\n"
                            "# item\ticon\teffect\tamount\n"))
        return false;
    for (uint32_t item = 0; item < 32; ++item) {
        const uint8_t *record = slice(rom, ITEM_STATS + item * 16, 16, "item use record",
                                      error, error_size);
        if (!record || !text_append(items, "%u\t%u\t%u\t%u\n", item, record[2],
                                    record[8], read_u16(record + 0xA)))
            return false;
    }
    return true;
}

/* The equipment records sub_0804ACD4 reads: weapons (0x08505D3C, 59
 * records of 28 bytes) and armor/accessories (0x085063B0, 45 records of 20
 * bytes): ATK (+ 0xA), DEF (+ 0xB), STR, CON, INT, LCK (+ 0xC-0xF, signed)
 * and the armor table's resistances (+ 0x10). */
static bool append_equipment(const FusionRom *rom, Text *equipment, char *error,
                             size_t error_size) {
    if (!text_append(equipment,
            "schema\tmetroidvania-aos-equipment-v1\n"
            "# kind\tindex\tatk\tdef\tstr\tcon\tint\tlck\tresist\n")) return false;
    static const struct { const char *kind; uint32_t table, stride, count; } tables[] = {
        {"weapon", WEAPON_STATS, 28, 59}, {"armor", ARMOR_STATS, 20, 45}};
    for (size_t table = 0; table < 2; ++table)
        for (uint32_t index = 0; index < tables[table].count; ++index) {
            const uint8_t *record = slice(rom, tables[table].table + index * tables[table].stride,
                                          tables[table].stride, "equipment record", error,
                                          error_size);
            if (!record || !text_append(equipment, "%s\t%u\t%u\t%u\t%d\t%d\t%d\t%d\t0x%04x\n",
                                        tables[table].kind, index, record[0xA], record[0xB],
                                        (int8_t)record[0xC], (int8_t)record[0xD],
                                        (int8_t)record[0xE], (int8_t)record[0xF],
                                        table ? read_u16(record + 0x10) : 0))
                return false;
        }
    return true;
}

/* Soma's red soul records (0x080E1598, 56 x 16 bytes, one per count
 * nibble of a 0x1C-byte red soul inventory group; record 0 is none):
 * whether a skill entity exists (+ 0x0), the cast pose (+ 0x4), the MP cost
 * (+ 0x6), the most active at once (+ 0x8), the power of sub_08021530(5)
 * (+ 0xA), the parameter given to the skill entity (+ 0xE) and the attack
 * elements of sub_0806B7D8 (+ 0xC). */
static bool append_red_souls(const FusionRom *rom, Text *souls, char *error,
                             size_t error_size) {
    if (!text_append(souls,
            "schema\tmetroidvania-aos-red-souls-v2\n"
            "# soul\tskill\tpose\tmp\tmaximum\tpower\tparameter\telements\n")) return false;
    for (uint32_t soul = 0; soul < 56; ++soul) {
        const uint8_t *record = slice(rom, RED_SOULS + soul * 16, 16, "red soul record",
                                      error, error_size);
        if (!record || !text_append(souls, "%u\t0x%08x\t%u\t%d\t%u\t%u\t%u\t0x%04x\n", soul,
                                    read_u32(record), read_u16(record + 4),
                                    (int16_t)read_u16(record + 6), record[8],
                                    read_u16(record + 0xA), read_u16(record + 0xE),
                                    read_u16(record + 0xC)))
            return false;
    }
    return true;
}

/* Soma's blue soul records (0x080E192C, 25 x 12 bytes indexed by the
 * equipped blue soul; record 0 is none), as sub_0802E0C4 reads them: the
 * guardian entity's routine (+ 0x0), the MP drained per second (+ 0x4, one
 * MP every 60 / it frames), the flags (+ 0x5, bit 1: a press toggles the
 * soul instead of holding it), the entity's + 0x1A (+ 0x6) and + 0x1C
 * (+ 0x8). */
static bool append_blue_souls(const FusionRom *rom, Text *souls, char *error,
                              size_t error_size) {
    if (!text_append(souls,
            "schema\tmetroidvania-aos-blue-souls-v1\n"
            "# soul\troutine\tmp_per_second\tflags\tparameter\targument\n")) return false;
    for (uint32_t soul = 0; soul < 25; ++soul) {
        const uint8_t *record = slice(rom, BLUE_SOULS + soul * 12, 12, "blue soul record",
                                      error, error_size);
        if (!record || !text_append(souls, "%u\t0x%08x\t%u\t0x%02x\t%u\t0x%08x\n", soul,
                                    read_u32(record), record[4], record[5],
                                    read_u16(record + 6), read_u32(record + 8)))
            return false;
    }
    return true;
}

/* Soma's yellow soul records (0x080E1B08, 37 x 12 bytes indexed by the
 * equipped yellow soul; record 0 is none), as sub_08031F3C and
 * sub_08031FC4 read them: the soul entity's routine (+ 0x0), + 0x4, the
 * entity's + 0x1A (+ 0x6) and + 0x1C (+ 0x8). */
static bool append_yellow_souls(const FusionRom *rom, Text *souls, char *error,
                                size_t error_size) {
    if (!text_append(souls,
            "schema\tmetroidvania-aos-yellow-souls-v1\n"
            "# soul\troutine\trefresh\tparameter\targument\n")) return false;
    for (uint32_t soul = 0; soul < 37; ++soul) {
        const uint8_t *record = slice(rom, YELLOW_SOULS + soul * 12, 12, "yellow soul record",
                                      error, error_size);
        if (!record || !text_append(souls, "%u\t0x%08x\t%u\t%u\t0x%08x\n", soul,
                                    read_u32(record), read_u16(record + 4),
                                    read_u16(record + 6), read_u32(record + 8)))
            return false;
    }
    return true;
}

/* The thrown-object records of the shared Skeleton skill routine
 * (0x084F12C8, 8 x 24 bytes, indexed by the red soul's parameter): launch
 * speeds (+ 0x0, + 0x4), gravity (+ 0x8), graphics record of 0x080E13BC
 * (+ 0xC), still frame (+ 0xD), sequence (+ 0xE), palette row (+ 0xF),
 * song (+ 0x10), flags (+ 0x12), spin (+ 0x14) and + 0x16. */
/* Burst list 0 of sub_08045A98 (0x08525498[0], 24-byte entries ended by a
 * row byte 0xFF): animation (+ 0x0), OBJ row (+ 0x1), priority (+ 0x2),
 * delay (+ 0x3), x and y offsets (+ 0x4, + 0x6, s16), vx, vy, ax, ay
 * (+ 0x8 to + 0x14). */
static bool append_bursts(const FusionRom *rom, Text *text, char *error, size_t error_size) {
    const uint8_t *head = slice(rom, 0x08525498, 4, "burst lists", error, error_size);
    if (!head || !text_append(text, "schema\tmetroidvania-aos-bursts-v1\n"
                                    "# list\tanimation\trow\tpriority\tdelay\tdx\tdy\tvx\tvy"
                                    "\tax\tay\n"))
        return false;
    uint32_t pointer = read_u32(head);
    for (unsigned index = 0; index < 64; ++index) {
        const uint8_t *entry = slice(rom, pointer + index * 24, 24, "burst entry", error,
                                     error_size);
        if (!entry) return false;
        if (entry[1] == 0xFF) return true;
        if (!text_append(text, "0\t%u\t%u\t%u\t%u\t%d\t%d\t%d\t%d\t%d\t%d\n", entry[0],
                         entry[1], entry[2], entry[3], (int16_t)read_u16(entry + 4),
                         (int16_t)read_u16(entry + 6), (int32_t)read_u32(entry + 8),
                         (int32_t)read_u32(entry + 12), (int32_t)read_u32(entry + 16),
                         (int32_t)read_u32(entry + 20)))
            return false;
    }
    set_error(error, error_size, "unterminated burst list");
    return false;
}

static bool append_skill_throws(const FusionRom *rom, Text *throws, char *error,
                                size_t error_size) {
    if (!text_append(throws,
            "schema\tmetroidvania-aos-skill-throws-v1\n"
            "# parameter\tvx\tvy\tgravity\tresource\tframe\tsequence\tpalette\tsong\tflags"
            "\tspin\tfield_16\n")) return false;
    for (uint32_t index = 0; index < 8; ++index) {
        const uint8_t *record = slice(rom, SKILL_THROWS + index * 24, 24, "skill throw record",
                                      error, error_size);
        if (!record || !text_append(throws,
                                    "%u\t%d\t%d\t%d\t%u\t%u\t%u\t%u\t0x%04x\t0x%04x\t0x%04x"
                                    "\t0x%04x\n",
                                    index, (int32_t)read_u32(record),
                                    (int32_t)read_u32(record + 4), (int32_t)read_u32(record + 8),
                                    record[0xC], record[0xD], record[0xE], record[0xF],
                                    read_u16(record + 0x10), read_u16(record + 0x12),
                                    read_u16(record + 0x14), read_u16(record + 0x16)))
            return false;
    }
    return true;
}

/* The in-game HUD resources of sub_0800DA50 and sub_0800DE4C: palette rows
 * 0-1 of 0x0820C428 (the initial BG rows 13-14), its rows 4 and 5 (the item-
 * and enemy-name BG row 14), the 16 x 16 tiles of 0x0827B208 (BG tiles
 * 0x200-0x2FF), then context B's metrics and 8-pixel font bitmaps. */
static bool write_hud(const FusionRom *rom, const char *metadata_output, char *error,
                      size_t error_size) {
    const uint8_t *palette = slice(rom, HUD_PALETTE, 4 + 6 * 32, "HUD palette", error,
                                   error_size);
    if (!palette || palette[0] != 0 || palette[2] < 6) {
        if (palette) set_error(error, error_size, "unsupported HUD palette resource");
        return false;
    }
    TileSheet sheet;
    if (!load_tile_sheet(rom, HUD_TILES, &sheet, error, error_size)) return false;
    const uint8_t *metrics = slice(rom, HUD_FONT_METRICS, HUD_FONT_METRIC_BYTES,
                                   "HUD font metrics", error, error_size);
    const uint8_t *glyphs = metrics ? slice(rom, HUD_FONT_GLYPHS, HUD_FONT_BITMAP_BYTES,
                                            "HUD font glyphs", error, error_size)
                                     : NULL;
    bool ok = sheet.size == 0x2000 && metrics && glyphs;
    size_t total = 128 + sheet.size + HUD_FONT_METRIC_BYTES + HUD_FONT_BITMAP_BYTES;
    uint8_t *bytes = ok ? malloc(total) : NULL;
    if (bytes) {
        memcpy(bytes, palette + 4, 64);
        memcpy(bytes + 64, palette + 4 + 4 * 32, 64);
        memcpy(bytes + 128, sheet.data, sheet.size);
        memcpy(bytes + 128 + sheet.size, metrics, HUD_FONT_METRIC_BYTES);
        memcpy(bytes + 128 + sheet.size + HUD_FONT_METRIC_BYTES, glyphs,
               HUD_FONT_BITMAP_BYTES);
        char path[FUSION_EXPORT_PATH_CAPACITY];
        ok = fusion_export_join(path, metadata_output, "hud.bin") &&
             fusion_export_write_file(path, bytes, total, error, error_size);
    } else {
        if (ok) set_error(error, error_size, "cannot allocate the HUD resources");
        else set_error(error, error_size, "unexpected HUD tile sheet size");
        ok = false;
    }
    free(bytes);
    free(sheet.data);
    return ok;
}

static bool append_global_metadata(const FusionRom *rom, Text *rows,
                                   char *error, size_t error_size) {
    const uint8_t *blink = slice(rom, DEATH_BLINK, 40, "death blink pattern",
                                 error, error_size);
    if (!blink || !text_append(rows, "blink\t")) return false;
    for (size_t index = 0; index < 40; ++index)
        if (!text_append(rows, "%u", blink[index] & 1)) return false;
    if (!text_append(rows, "\nmargins")) return false;
    const uint8_t *margins = slice(rom, SCREEN_MARGINS, 28, "screen margins",
                                   error, error_size);
    if (!margins) return false;
    for (size_t index = 0; index < 14; ++index)
        if (!text_append(rows, "\t%d", read_s16(margins + index * 2))) return false;
    return text_append(rows, "\n");
}

static bool write_text_file(const char *directory, const char *name,
                            const Text *text, char *error, size_t error_size) {
    char path[FUSION_EXPORT_PATH_CAPACITY];
    if (!fusion_export_join(path, directory, name)) {
        set_error(error, error_size, "object metadata path is too long");
        return false;
    }
    return fusion_export_write_file(path, (const uint8_t *)text->data,
                                    text->size, error, error_size);
}

bool fusion_aos_export_objects(const FusionRom *rom,
                               const char *metadata_output,
                               const char *runtime_output,
                               FusionAosObjectExportSummary *summary,
                               char *error, size_t error_size) {
    if (!rom || rom->game != FUSION_ASSET_GAME_ARIA || !metadata_output ||
        !runtime_output || !summary) {
        set_error(error, error_size, "invalid Aria object export request");
        return false;
    }
    *summary = (FusionAosObjectExportSummary){0};
    Text rows = {0}, object_rows = {0}, stats = {0}, money = {0}, item_use = {0}, equipment = {0},
         red_souls = {0}, blue_souls = {0}, yellow_souls = {0}, skill_throws = {0}, bursts = {0}, item_icons = {0};
    FusionSpriteLibrary library = {0};
    bool success = fusion_export_ensure_directory(metadata_output, error,
                                                   error_size) &&
        fusion_export_ensure_directory(runtime_output, error, error_size) &&
        fusion_sprite_library_init(&library, runtime_output, index_prefix,
                                   error, error_size) &&
        export_doors(rom, &library, error, error_size) &&
        text_append(&rows,
            "schema\tmetroidvania-aos-enemy-frames-v2\n"
            "# enemy\tanimation\tindex\tframe\tticks\tmode\thurt x\ty\twidth\theight"
            "\tattack x\ty\twidth\theight\n");
    for (size_t index = 0;
         success && index < sizeof entities / sizeof entities[0]; ++index)
        success = export_entity(rom, &library, &entities[index], "Enemy", &rows,
                                error, error_size);
    if (success) success = text_append(&object_rows,
        "schema\tmetroidvania-aos-object-frames-v1\n"
        "# object\tanimation\tindex\tframe\tticks\tmode\thurt x\ty\twidth\theight"
        "\tattack x\ty\twidth\theight\n");
    for (size_t index = 0;
         success && index < sizeof special_objects / sizeof special_objects[0];
         ++index)
        success = export_entity(rom, &library, &special_objects[index], "Object",
                                &object_rows, error, error_size);
    for (size_t index = 0;
         success && index < sizeof special14_variants / sizeof special14_variants[0];
         ++index)
        success = export_entity(rom, &library, &special14_variants[index],
                                "Object", &object_rows, error, error_size);
    for (size_t index = 0;
         success && index < sizeof special18_variants / sizeof special18_variants[0];
         ++index)
        success = export_entity(rom, &library, &special18_variants[index],
                                "Object", &object_rows, error, error_size);
    for (size_t index = 0;
         success && index < sizeof special15_variants / sizeof special15_variants[0];
         ++index)
        success = export_entity(rom, &library, &special15_variants[index],
                                "Object", &object_rows, error, error_size);
    for (size_t index = 0;
         success && index < sizeof effects / sizeof effects[0]; ++index)
        success = export_effect(rom, &library, &effects[index],
                                error, error_size);
    for (size_t index = 0;
         success && index < sizeof direct_effects / sizeof direct_effects[0]; ++index)
        success = export_direct_effect(rom, &library, &direct_effects[index],
                                       error, error_size);
    if (success) success = export_icons(rom, &library, error, error_size);
    if (success) success = append_buer_death_offsets(rom, &rows, error, error_size) &&
                           append_global_metadata(rom, &rows, error, error_size) &&
                           append_enemy_stats(rom, &stats, error, error_size) &&
                           write_text_file(metadata_output, "enemy_frames.tsv",
                                           &rows, error, error_size) &&
                           write_text_file(metadata_output, "object_frames.tsv",
                                           &object_rows, error, error_size) &&
                           write_text_file(metadata_output, "enemies.tsv",
                                           &stats, error, error_size) &&
                           append_money(rom, &money, error, error_size) &&
                           write_text_file(metadata_output, "money.tsv",
                                           &money, error, error_size) &&
                           append_item_use(rom, &item_use, error, error_size) &&
                           write_text_file(metadata_output, "item_use.tsv",
                                           &item_use, error, error_size) &&
                           append_equipment(rom, &equipment, error, error_size) &&
                           write_text_file(metadata_output, "equipment.tsv",
                                           &equipment, error, error_size) &&
                           append_red_souls(rom, &red_souls, error, error_size) &&
                           write_text_file(metadata_output, "red_souls.tsv",
                                           &red_souls, error, error_size) &&
                           append_blue_souls(rom, &blue_souls, error, error_size) &&
                           write_text_file(metadata_output, "blue_souls.tsv",
                                           &blue_souls, error, error_size) &&
                           append_yellow_souls(rom, &yellow_souls, error, error_size) &&
                           write_text_file(metadata_output, "yellow_souls.tsv",
                                           &yellow_souls, error, error_size) &&
                           append_skill_throws(rom, &skill_throws, error, error_size) &&
                           write_text_file(metadata_output, "skill_throws.tsv",
                                           &skill_throws, error, error_size) &&
                           append_bursts(rom, &bursts, error, error_size) &&
                           write_text_file(metadata_output, "bursts.tsv",
                                           &bursts, error, error_size) &&
                           write_hud(rom, metadata_output, error, error_size) &&
                           append_item_icons(rom, &item_icons, error, error_size) &&
                           write_text_file(metadata_output, "item_icons.tsv",
                                           &item_icons, error, error_size);
    FusionSpriteLibrarySummary library_summary = {0};
    if (success) success = fusion_sprite_library_finish(
        &library, &library_summary, error, error_size);
    if (success) {
        summary->sequences = library_summary.sequences;
        summary->frames = library_summary.frames;
        summary->unique_bmps = library_summary.unique_bmps;
        summary->removed_stale_objects = library_summary.removed_stale_objects;
    } else if (!error || error[0] == '\0') {
        set_error(error, error_size, "cannot export Aria object library");
    }
    fusion_sprite_library_free(&library);
    free(rows.data);
    free(object_rows.data);
    free(stats.data);
    free(money.data);
    free(item_use.data);
    free(equipment.data);
    free(red_souls.data);
    free(blue_souls.data);
    free(yellow_souls.data);
    free(skill_throws.data);
    free(bursts.data);
    free(item_icons.data);
    return success;
}

bool fusion_aos_render_object_frame(const FusionRom *rom, uint32_t graphics, uint32_t palette,
                                    uint8_t bank, uint32_t frames, uint8_t frame,
                                    uint8_t **bmp, size_t *bmp_size, int *left, int *top,
                                    char *error, size_t error_size) {
    if (!rom || !bmp || !bmp_size || !left || !top) {
        set_error(error, error_size, "invalid Aria object frame request");
        return false;
    }
    TileSheet sheet;
    if (!load_tile_sheet(rom, graphics, &sheet, error, error_size)) return false;
    const uint8_t *colors = load_palette(rom, palette, bank, error, error_size);
    RenderedFrame rendered = {0};
    bool success = colors && render_frame(rom, frames, &sheet, colors, frame, &rendered,
                                          error, error_size);
    free(sheet.data);
    if (!success) return false;
    *bmp = rendered.bmp;
    *bmp_size = rendered.bmp_size;
    *left = rendered.left;
    *top = rendered.top;
    return true;
}
