/* SPDX-License-Identifier: GPL-3.0-only */
/* Entity-update cluster of the Aria of Sorrow room runtime, extracted
 * from aria_runtime.c (AGENTS.md rule 8, decomposition wave 3): the
 * per-frame SPECIAL/kind entity dispatch (update_entities), the
 * camera-window spawn probe, the Object20 host glue (flag12078 bit
 * reads, In/Out marshalling, the whiteout spawn-child pool) and the
 * Object1C engine probes. The room-dialogue state and host adapters
 * stay in aria_runtime.c — the main loop renders and locks the same
 * dialogue — so this header only exposes what the frame loop calls
 * plus the runtime-side helpers the entity pass borrows. */
#ifndef ARIA_ENTITIES_H
#define ARIA_ENTITIES_H

/* Same include order as aria_runtime.c: the opening-event header
 * precedes the shared types (AOS_OPENING_ACTOR_COUNT). */
#include "aos/title/aos_opening_event.h"

#include "aria_runtime_types.h"

#include "aos/core/aos_collision.h"
#include "aos/core/aos_soma.h"
#include "aos/core/aos_stats.h"
#include "aos/weapons/aos_weapon.h"
#include "aos/game/aos_pickup.h"
#include "aos/game/aos_recent_items.h"
#include "aos/menus/aos_hud.h"
#include "aos/intro/aos_levelup.h"

/* The player's combat stats: ATK and DEF come from aos_stats_refresh
 * (sub_0804AD9C) unless --atk / --def override them; HP from the file. */
typedef struct {
    int atk, def, hp, max_hp;
} AriaPlayerStats;

/* Everything the enemies need from the frame. */
typedef struct {
    const AosEnemyKind *kinds[ENEMY_KINDS];     /* NULL when not loaded */
    const AosEnemyStats *stats[ENEMY_KINDS];
    const AosEnemyDrops *drops[ENEMY_KINDS];
    const AosWeaponEntity *weapon;
    const AosWeaponFrames *weapon_frames;
    AriaPlayerStats *player;
    const AosAnimSet *effects[AOS_EFFECT_KINDS];   /* Effect/<name>, NULL when not loaded */
    AosStats *progress;                          /* level, experience, bestiary */
    const AosMoneyKind *money;                   /* NULL without metadata/money.tsv */
    const AosEquipmentTables *equipment;         /* NULL without metadata/equipment.tsv */
    const uint8_t *item_icons;                    /* item, weapon, armor -> icon id */
    AosPickupRecent *recent_items;                /* sub_0804AE3C's three caches */
    AosRecentSouls *recent_souls;                 /* sub_08049E64's soul caches */
    AosHud *hud;                                 /* sub_0800EA98 enemy-name notice */
    const AosHudFont *hud_font;
    bool derive_atk, derive_def;                 /* false under --atk / --def */
    /* sub_0800E708 on every soul collection (sub_080441F8 tail): the
     * obtained-soul banner plus sub_0800E540's description when the
     * soul is new (sub_080441F8's GetSoulTotal == 0 branch). */
    void (*soul_notice)(void *ctx, unsigned category, unsigned soul_index,
                        bool player_low, bool fresh);
    void *soul_notice_ctx;
    uint8_t soul_storm;         /* sub_08045350's remaining spawn frames */
    uint8_t soul_storm_kind;    /* the new soul's kind, for the palette */
    AosLevelUp levelup;         /* sub_080455BC's display entity */
} AriaCombat;

/* The room's entity pass: SPECIAL/kind dispatch, pickups, walls,
 * Object20 dialogue/shop glue and the deferred sparkle sound. Called
 * once per frame from each room loop in aria_runtime.c. */
int update_entities(AriaRoom *room, AosCollision *layer, AosSoma *soma,
                    int cam_x, int cam_y, AosForcedInput *input,
                    AriaCombat *combat, uint16_t new_input);

/* The BG1 scroll offset accumulated by the ported sub_0800ED24
 * transactions (aos/core/aos_scroll), as a delta in integer pixels
 * from the camera position the layer was bound at — 0,0 before the
 * room's first transaction. The draw path adds it to cam_x/cam_y
 * for the BG1 source rect only; entities keep cam_x/cam_y. */
void aria_scroll_bg1_offset(const AriaRoom *room, int32_t *x, int32_t *y);

/* Helpers the entity pass borrows from aria_runtime.c (their state —
 * the homing list, the published save context, the shared room
 * dialogue — stays owned there). */
void homing_register(int index);
bool object10_item_owned(void *ctx, uint16_t item);
void room_dialogue_bind(void);

/* Object1C child animation table (sub_0803F17C's injected sequences):
 * rebuilt from the loaded special_28_v<bank> art on each objects
 * load; NULL clears it (children freeze on their spawn frame). */
void object1c_anim_bind(const AriaObjects *objects);

#endif /* ARIA_ENTITIES_H */
