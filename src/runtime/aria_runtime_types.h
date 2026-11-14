/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared type and constant definitions of the Aria of Sorrow room
 * runtime (aria_runtime.c): the room/entity model, the sprite library
 * frames and the limits and kind ids every runtime cluster agrees on.
 * Extracted so the draw, entity and asset modules can share one types
 * home (AGENTS.md rule 8). Pure declarations only; the lookup tables
 * (enemy_names, skill_art_names, ...) and file-scope state stay in
 * aria_runtime.c. */
#ifndef ARIA_RUNTIME_TYPES_H
#define ARIA_RUNTIME_TYPES_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aos/core/aos_anim.h"
#include "aos/core/aos_damage_popup.h"
#include "aos/core/aos_effect.h"
#include "aos/core/aos_enemy.h"
#include "aos/core/aos_wall.h"
#include "aos/game/aos_bestiary.h"
#include "aos/game/aos_door.h"
#include "aos/game/aos_object01.h"
#include "aos/game/aos_object02.h"
#include "aos/game/aos_object03.h"
#include "aos/game/aos_object04.h"
#include "aos/game/aos_object05.h"
#include "aos/game/aos_object06.h"
#include "aos/game/aos_object07.h"
#include "aos/game/aos_object0c.h"
#include "aos/game/aos_object0f.h"
#include "aos/game/aos_object0d.h"
#include "aos/game/aos_object0e.h"
#include "aos/game/aos_object10.h"
#include "aos/game/aos_object11.h"
#include "aos/game/aos_object12.h"
#include "aos/game/aos_object13.h"
#include "aos/game/aos_object14.h"
#include "aos/game/aos_object19.h"
#include "aos/game/aos_object1a.h"
#include "aos/game/aos_object1b.h"
#include "aos/game/aos_object1e.h"
#include "aos/game/aos_object15.h"
#include "aos/game/aos_object16.h"
#include "aos/game/aos_object24.h"
#include "aos/game/aos_object26.h"
#include "aos/game/aos_object27.h"
#include "aos/game/aos_object2a.h"
#include "aos/game/aos_object2b.h"
#include "aos/game/aos_object23.h"
#include "aos/game/aos_object2d.h"
#include "aos/game/aos_object2e.h"
#include "aos/game/aos_object2f.h"
#include "aos/game/aos_object36.h"
#include "aos/game/aos_object34.h"
#include "aos/game/aos_object2c.h"
#include "aos/game/aos_object17.h"
#include "aos/game/aos_object18.h"
#include "aos/game/aos_object30.h"
#include "aos/game/aos_object21.h"
#include "aos/game/aos_object25.h"
#include "aos/game/aos_object31.h"
#include "aos/game/aos_object33.h"
#include "aos/game/aos_object22.h"
#include "aos/game/aos_object29.h"
#include "aos/game/aos_object32.h"
#include "aos/game/aos_object35.h"
#include "aos/game/aos_object37.h"
#include "aos/game/aos_object1d.h"
#include "aos/game/aos_object1f.h"
#include "aos/game/aos_object1c.h"
#include "aos/game/aos_object28.h"
#include "aos/game/aos_object20.h"
#include "aos/game/aos_pickup.h"
#include "aos/game/aos_room.h"
#include "aos/game/aos_room_overlay.h"

#define VIEW_W 240
#define VIEW_H 160
#define MAX_FILE_BYTES (8L * 1024 * 1024)
#define MAX_ANIMS 256
#define MAX_FRAMES 80
/* Opening actors and SPECIAL objects share one art struct; the shared
 * flame descriptor 0x0820ED60 exports 23 animations, past the enemy
 * AOS_ENEMY_MAX_ANIMS of 64. */
#define OBJECT_MAX_ANIMS 24
/* Object20's private sprite records (Enemy/object20_actor_N) export
 * up to 31 animation sequences (actors 6 and 7 reach anim_30), past
 * OBJECT_MAX_ANIMS. */
#define OBJECT20_ACTOR_ANIMS 32
#define INDEX_SCHEMA "schema\tmetroidvania-sprite-index-v1"
#define DEFAULT_ROOMS "assets/extracted/aria/rooms/runtime"
#define DEFAULT_LIBRARY "assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv"
#define DEFAULT_OBJECTS "assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv"
#define DEFAULT_WEAPONS "assets/extracted/aria/metadata/weapons.tsv"
#define DEFAULT_WEAPON_FRAMES "assets/extracted/aria/metadata/weapon_frames.tsv"
#define DEFAULT_WEAPON_SPRITES "assets/extracted/aria/sprites/weapons/runtime/runtime_index.tsv"
#define DEFAULT_ENEMY_FRAMES "assets/extracted/aria/metadata/enemy_frames.tsv"
#define DEFAULT_ENEMY_STATS "assets/extracted/aria/metadata/enemies.tsv"
#define DEFAULT_MONEY "assets/extracted/aria/metadata/money.tsv"
#define DEFAULT_ITEM_USE "assets/extracted/aria/metadata/item_use.tsv"
#define DEFAULT_EQUIPMENT "assets/extracted/aria/metadata/equipment.tsv"
#define DEFAULT_RED_SOULS "assets/extracted/aria/metadata/red_souls.tsv"
#define DEFAULT_BLUE_SOULS "assets/extracted/aria/metadata/blue_souls.tsv"
#define DEFAULT_YELLOW_SOULS "assets/extracted/aria/metadata/yellow_souls.tsv"
#define DEFAULT_SKILL_THROWS "assets/extracted/aria/metadata/skill_throws.tsv"
#define DEFAULT_BURSTS "assets/extracted/aria/metadata/bursts.tsv"
#define DEFAULT_AUDIO_TABLE "assets/extracted/aria/sound/runtime/song_table.tsv"
#define DEFAULT_AUDIO_BANK "assets/extracted/aria/sound/runtime/sound_bank.bin"
#define DEFAULT_ROOM_MUSIC "assets/extracted/aria/sound/runtime/room_music.tsv"
#define DEFAULT_HUD "assets/extracted/aria/metadata/hud.bin"
#define DEFAULT_ITEM_ICONS "assets/extracted/aria/metadata/item_icons.tsv"
#define ITEM_ICONS 193     /* icon numbers 1-192 of sub_0801232C */
#define MAX_PICKUPS 16      /* dropped pickups alive at once (of entities 0x49-0xCF) */
/* The pickup sprites of the object export (coins 0-3, bags 4-6, hearts,
 * souls by type, then the four primary and four follower soul containers);
 * item icons are not exported. */
#define PICKUP_ARTS 21
#define PICKUP_ART_SOUL_CONTAINER 13
#define PICKUP_ART_SOUL_CONTAINER_FOLLOWER 17
#define DOOR_STYLES 2
#define ENEMY_KINDS AOS_BESTIARY_NATIVE_RECORDS
/* Other palette banks of a kind, exported as Enemy/<name>_bank<N>: the gold
 * Medusa head (parameter 1 bit 0) and the Ghost Dancer (parameter 0). */
#define VARIANT_ARTS 5
/* Object/special_NN_vP records: the export currently emits 31 distinct
 * (id, param0) pairs; 40 leaves headroom. */
#define SPECIAL_VARIANTS 40

/* metadata room_music.tsv: sub_08013960's flags per (area, room). */
#define MUSIC_AREAS 16
#define MUSIC_ROOMS 256
#define ENEMY_STATIC_FRAMES 20 /* single sprite frames of child entities per kind */
/* The red soul skills' sprites (Enemy/skill_<name>). */
enum { SKILL_ART_BAT, SKILL_ART_ARCHER, SKILL_ART_BONE, SKILL_ART_SHARED, SKILL_ART_BLADES,
       SKILL_ART_DEVIL, SKILL_ART_CROW, SKILL_ART_FLEA, SKILL_ART_SKULL, SKILL_ART_VALKYRIE,
       SKILL_ART_NEEDLES, SKILL_ART_RIPPER, SKILL_ART_KILLER_FISH, SKILL_ART_FISH_HEAD,
       SKILL_ART_CAGNAZZO, SKILL_ART_BUER, SKILL_ART_CATOBLEPAS, SKILL_ART_IMP,
       SKILL_ART_LIGHTNING, SKILL_ART_KILLER_DOLL, SKILL_ART_CURLY, SKILL_ART_GIANT_GHOST,
       SKILL_ART_WITCH, SKILL_ART_PILLAR, SKILL_ART_FLYING_ARMOR, SKILL_ART_DEATH,
       SKILL_ART_PERSEPHONE, SKILL_ART_GREAT_ARMOR, SKILL_ART_DEVIL_SPARK,
       SKILL_ART_SHADOW_KNIGHT, SKILL_ART_ALURA_UNE, SKILL_ART_BIG_GOLEM,
       SKILL_ART_WEREWOLF, SKILL_ART_KILLER_MANTLE, SKILL_ART_WERE_BEAST, SKILL_ART_DEMON_LORD,
       SKILL_ART_BIPHRON, SKILL_ART_BALORE, SKILL_ART_KYOMA_DEMON, SKILL_ART_LEGION, SKILL_ART_STUDENT_WITCH, SKILL_ART_SLIME, SKILL_ART_UKOBACK, SKILL_ART_ALTAIR, SKILL_ART_UNE, SKILL_ART_MANDRAGORA, SKILL_ART_CHRONOMAGE, SKILL_ART_DISC_ARMOR, SKILL_ART_COCKATRICE, SKILL_ART_DRYAD, SKILL_ART_ARACHNE, SKILL_ART_RED_MINOTAUR, SKILL_ART_NIGHTMARE, SKILL_ART_MAN_EATER, SKILL_ART_SKY_FISH, SKILL_ART_CREAKING_SKULL, SKILL_ART_ALASTOR, SKILL_ART_FINAL_GUARD, SKILL_ARTS };
#define ARIA_KIND_ENEMY 1



#define MAX_TRANSITIONS 32
#define MAX_ENTITIES 128
#define MAX_EFFECTS 32      /* shared effects (flames, blood) alive at once */
#define MAX_DAMAGE_POPUPS 32
#define ARIA_KIND_SPECIAL 2
#define ARIA_KIND_PICKUP 4
#define ARIA_OBJECT_WOODEN_DOOR 0x00
#define ARIA_OBJECT_WALL 0x08           /* Object08: a breakable wall */
#define ARIA_OBJECT_EVENT_WALL 0x09     /* Object09: a wall its event bit breaks */

typedef struct {
    int kind, id, x, y, persistent_index, param0, param1, flags;
    bool spawned;           /* gEwramData + 0x3D0 bit of the record */
    int spawner;            /* entity index of a zombie's spawner, or -1 */
    AosDoor door;
    AosWall wall;           /* objects 0x08 and 0x09 */
    AosObject02 object02;   /* object 0x02: the cutscene director */
    AosObject03 object03;   /* object 0x03: escort director */
    AosObject01 object01;   /* object 0x01: conditional static children */
    AosObject04 object04;   /* object 0x04: directional cutscene director */
    AosObject05 object05;   /* object 0x05: proximity trigger */
    AosObject06 object06;   /* object 0x06: cinematic trigger */
    AosObject07 object07;   /* object 0x07: ambient motes */
    AosObject0c object0c;   /* object 0x0C: room-region triggers */
    AosObject0f object0f;   /* object 0x0F: destructible prop */
    AosObject0d object0d;   /* object 0x0D: swinging object */
    AosObject0e object0e;   /* object 0x0E: breakable prop variants */
    AosObject10 object10;   /* object 0x10: save point statue */
    AosObject11 object11;   /* object 0x11: save-point remover marker */
    AosObject12 object12;   /* object 0x12: param-driven animated sprite */
    AosObject13 object13;   /* object 0x13: swinging chandelier trigger */
    AosObject14 object14;   /* object 0x14: param0 probe/emitter */
    AosObject20 object20;   /* object 0x20: SPECIAL id 32 dispatcher */
    int object20_anim;      /* last sub_0803F2C8 sequence pick, -1 none —
                             * the draw pass's +0x6D approximation */
    AosWoBar wo_bars[AOS_WO_BAR_COUNT]; /* param0-30 whiteout
                             * (sub_08064518) per-scanline BgAffineSet
                             * records from the entity's last update —
                             * object20_apply_out persists them for
                             * draw_object20_wo_bars */
    uint8_t  wo_bars_count; /* 0 or AOS_WO_BAR_COUNT; rewritten every
                             * update so the bars drop the frame the
                             * cinematic stops emitting them */
    /* sub_08064518 spawn-child pool: one AosWoSpawnChild per
     * AOS_WO_SPAWN_TABLE_118B34 event (aux = 0x08118B34 word
     * index). wo_child_count live entries; the children's RNG
     * draws share the object20_wo_rng gEwramData+8 channel. */
    AosWoSpawnChild wo_children[AOS_WO_MAX_EVENTS];
    uint8_t  wo_child_count;
    /* Burst pool for the spawn children's carrier/burst events:
     * sub_08063168/sub_080632B0 (carrier + sub_080630E8
     * particles) and the child machine's direct carrier_spawn.
     * Draw side is unported (frames 0x08267EE4 / graphics
     * 0x08260390 are not exported); the state machines and RNG
     * draws still run natively. */
    AosWoBurst wo_bursts[AOS_WO_MAX_EVENTS];
    uint8_t  wo_burst_count;
    AosObject19 object19;   /* object 0x19: rectangular patrol mover */
    AosObject1b object1b;   /* object 0x1B: spin-synced object */
    AosObject15 object15;   /* object 0x15: story cutscene director */
    AosObject16 object16;   /* object 0x16: boss-intro zoom director */
    AosObject24 object24;   /* object 0x24: ambient flickering sprite */
    AosObject27 object27;   /* object 0x27: spinning attack orb */
    AosObject2A object2a;   /* object 0x2A: screen-space oscillator */
    AosObject2B object2b;   /* object 0x2B: plain animated sprite */
    AosObject2F object2f;   /* object 0x2F: skull-eye flame jets */
    AosObject36 object36;   /* object 0x36: metal gate BG scroller */
    AosObject34 object34;   /* object 0x34: waterfall splash */
    AosObject2C object2c;   /* object 0x2C: fountain droplets */
    AosObject17 object17;   /* object 0x17: vertical pan director */
    AosObject18 object18;   /* object 0x18: breakable scatter */
    AosObject30 object30;   /* object 0x30: sparkle emitter */
    AosObject21 object21;   /* object 0x21: cyclic lift */
    AosObject25 object25;   /* object 0x25: soul carrier spawner */
    AosObject31 object31;   /* object 0x31: delayed-launch spinner */
    AosObject33 object33;   /* object 0x33: dual-burst emitter */
    AosObject22 object22;   /* object 0x22: scripted elevator */
    AosObject29 object29;   /* object 0x29: ping-pong platform */
    AosObject32 object32;   /* object 0x32: cinematic director */
    AosObject35 object35;   /* object 0x35: sequence actor */
    AosObject37 object37;   /* object 0x37: fade director */
    AosObject1A object1a;   /* object 0x1A: inert placeholder */
    AosObject1E object1e;   /* object 0x1E: inert placeholder */
    AosObject1D object1d;   /* object 0x1D: sum-indexed display */
    AosObject1F object1f;   /* object 0x1F: opening gate */
    AosObject1C object1c;   /* object 0x1C: save point */
    int object1c_save_result; /* last SaveData_SaveSlotToSram result
                               * fed back as the next frame's input */
    AosObject23 object23;   /* object 0x23: plain animated sprite */
    AosObject2D object2d;   /* object 0x2D: proximity attack sprite */
    AosObject2E object2e;   /* object 0x2E: one-shot breakable prop */
    AosObject28 object28;   /* object 0x28: Object27 orb sibling */
    AosObject26 object26;   /* object 0x26: chandelier hub counters */
    AosEnemy enemy;
    uint8_t reward_flags;   /* the entity's + 0x3E guard of sub_080683BC */
    bool secret_mark;       /* a broken wall's pending secret-room mark */
    int link_parent;        /* Object26 child: parent entity index + 1, 0 = none */
} AriaEntity;

/* Host model of one gEwramData + 0x4E4 entityData[] record
 * (EwramData_EntityData, third_party/cvaos/include/structs/
 * ewram.h, native stride 0x84, entityData[0xE0]). Only the
 * fields ported consumers read are kept; offsets noted are
 * record-relative. Table index 0 is the player entity (native
 * slot 0 — sub_0801487C's EntityCreateInRange(0,0)); indices
 * 1..MAX_ENTITIES mirror room->entities[0..MAX_ENTITIES-1] in
 * room order. The native 0x1B-based slot numbering and the
 * +0x10/+0x11 byte values are approximated by the host indices:
 * slot10/slot11 name table indices, where 0 is the player
 * record — matching the native post-clear +0x11 == 0 default
 * that resolves to entityData[0]. */
typedef struct {
    bool active;            /* updateFunc != NULL natively */
    uint8_t state;          /* +0xA state byte */
    int32_t pos40;          /* +0x40 fine X, 16.16 (unk_524) */
    int32_t pos44;          /* +0x44 fine Y, 16.16 (unk_528) */
    uint8_t flags20;        /* +0x20 record flags byte
                             * (unk_504_8.unk_504) */
    uint8_t flags58;        /* +0x58 flags byte (unk_53C):
                             * bits 0-4 (unk_53C_0:5) are the
                             * sub_0803E058/sub_0803E6BC effect
                             * parameter, bit 5 (unk_53C_5) an
                             * effect-active flag read by
                             * sub_08039340, bit 6 (unk_53C_6) the
                             * facing/horizontal-flip bit consumed
                             * by the sub_08014A04-family collision
                             * paths and the sprite-flip argument,
                             * bit 7 (unk_53C_7) a gate in the same
                             * routine family. Only bit 6 has
                             * ported consumers so far; the other
                             * bits round-trip untouched. */
    uint8_t flags59;        /* +0x59 flags byte (unk_53D): bit 1
                             * (unk_53D_1) marks the screen-locked
                             * entities sub_08000F60 compensates
                             * during BG scroll transactions; no
                             * ported writer sets it yet */
    uint8_t slot10;         /* +0x10 own slot index */
    uint8_t slot11;         /* +0x11 bound-record slot index */
} AriaEntityDataRecord;

#define ARIA_ENTITY_DATA_RECORDS (MAX_ENTITIES + 1)

typedef struct {
    int area, number;
    int width_screens, height_screens, width_cells, height_cells;
    uint8_t room_flags;
    uint8_t *cells;
    uint8_t *blocks;        /* gEwramData + 0xF0C0, cleared on every load */
    AosTransition transitions[MAX_TRANSITIONS];
    size_t transition_count;
    AriaEntity entities[MAX_ENTITIES];
    size_t entity_count;
    AosEffect effects[MAX_EFFECTS];
    size_t effect_count;
    AosDamagePopup damage_popups[MAX_DAMAGE_POPUPS];
    size_t damage_popup_count;
    AosPickup pickups[MAX_PICKUPS];
    size_t pickup_count;
    AosPickupIconSlots pickup_icons;
    /* The map editor's transitions (aos_room_overlay): new doors, and the
     * spawn points of the native entries they redirect. */
    AosProjectDoor project_doors[MAX_TRANSITIONS];
    size_t project_door_count;
    struct { bool set; int x, y; } arrivals[MAX_TRANSITIONS];
} AriaRoom;

typedef struct {
    SDL_Texture *texture;
    int offset_x, offset_y;
    float w, h;
} AriaFrame;

typedef struct {
    AosAnimDef defs[MAX_ANIMS];
    uint8_t durations[MAX_ANIMS][MAX_FRAMES];
    AriaFrame frames[MAX_ANIMS][MAX_FRAMES];
    AosAnimSet set;
} AriaLibrary;

/* Object20's private 12-record sprite table at 0x081186DC: records
 * 0/1/8 are the shared Enemy/opening_actor_* exports, records
 * 2-7/9-11 are Enemy/object20_actor_N. Same layout as the shared
 * object art but with OBJECT20_ACTOR_ANIMS sequences. */
typedef struct {
    AriaFrame frames[OBJECT20_ACTOR_ANIMS][MAX_FRAMES];
    uint8_t ticks[OBJECT20_ACTOR_ANIMS][MAX_FRAMES];
    AosAnimDef defs[OBJECT20_ACTOR_ANIMS];
    AosAnimSet set;
    bool loaded;
} AriaObject20ActorArt;

/* Object sequences of src/assets/aos/objects.c: WoodenDoor/style_N
 * (the door frame under each step of its palette cycle). */
typedef struct {
    AriaFrame frames[DOOR_STYLES][MAX_FRAMES];
    uint8_t ticks[DOOR_STYLES][MAX_FRAMES];
    int counts[DOOR_STYLES];
    struct {
        AriaFrame frames[AOS_ENEMY_MAX_ANIMS][MAX_FRAMES];
        uint8_t ticks[AOS_ENEMY_MAX_ANIMS][MAX_FRAMES];
        AosAnimDef defs[AOS_ENEMY_MAX_ANIMS];
        AosAnimSet set;
        AosEnemyKind kind;
        AosEnemyStats stats;
        AosEnemyDrops drops;
        bool loaded;
        int static_ids[ENEMY_STATIC_FRAMES];
        AriaFrame statics[ENEMY_STATIC_FRAMES];
        AosBox static_attack[64];       /* by sprite frame: static_attack rows */
        int static_count;
        /* Bestiary detail portrait (sub_0804CA18): the exported BMP path of
         * each frame, kept so the single menu frame can be decoded to RGBA
         * without holding every frame twice. */
        char *paths[AOS_ENEMY_MAX_ANIMS][MAX_FRAMES];
        char *static_paths[ENEMY_STATIC_FRAMES];
        int16_t portrait_x, portrait_y; /* enemy record + 0x1E / + 0x20 */
        const AriaFrame *portrait_frame;
        uint8_t *portrait;              /* RGBA pixels of the menu frame */
        int portrait_w, portrait_h;
        int8_t portrait_state;          /* 0 untried, 1 ready, -1 unavailable */
        bool portrait_flip;
    } enemies[ENEMY_KINDS], variants[VARIANT_ARTS], skills[SKILL_ARTS]; /* Enemy/skill_* */
    struct {
        AriaFrame frames[OBJECT_MAX_ANIMS][MAX_FRAMES];
        uint8_t ticks[OBJECT_MAX_ANIMS][MAX_FRAMES];
        AosAnimDef defs[OBJECT_MAX_ANIMS];
        AosAnimSet set;
        bool loaded;
        int id, param0; /* special_variants only: -1 id = free slot */
    } opening_actors[AOS_OPENING_ACTOR_COUNT], /* Enemy/opening_actor_N */
      special_objects[64],                     /* Object/special_NN */
      /* Object/special_NN_vP: param0-driven variants of SPECIAL objects
       * (ids 14 and 18 resolve their sprite through entity->param0). */
      special_variants[SPECIAL_VARIANTS],
      /* Object/whiteout_burst_carrier: the sub_08063168/sub_080632B0
       * carrier's four variant animations (0x08267EE4 descriptor,
       * anim index = the sub_0803F2C8 variant arg). */
      wo_burst_carrier,
      /* Object/whiteout_burst_particle: still frames of the same
       * descriptor stored at their native +0x65 record index
       * (frames 0xB/0xC only are exported). */
      wo_burst_particle;
    struct {
        AriaFrame frames[MAX_FRAMES];
        uint8_t ticks[MAX_FRAMES];
        AosAnimDef def;
        AosAnimSet set;
    } effects[AOS_EFFECT_KINDS],    /* Effect/<effect_names[k]>/anim_N */
      pickup_arts[PICKUP_ARTS];     /* Effect/<pickup_art_names[k]>/anim_N */
    struct {
        AriaFrame frames[MAX_FRAMES];
        uint8_t ticks[MAX_FRAMES];
        AosAnimDef def;
        AosAnimSet set;
    } damage[AOS_DAMAGE_POPUP_GLYPHS], damage_guard;
    AriaFrame icons[ITEM_ICONS];    /* Icon/i<n>/anim_0 */
    AriaFrame walls[AOS_WALL_ENTRIES]; /* Enemy/wall_<n>/frame_<f>: an entry's frame */
    AriaObject20ActorArt object20_actors[12]; /* Enemy/object20_actor_N,
                                               * indexed by native record */
    uint8_t item_icon[3][59];       /* item, weapon, armor record -> icon */
    AosMoneyKind money[AOS_MONEY_KINDS];
    bool money_loaded;
} AriaObjects;

#endif
