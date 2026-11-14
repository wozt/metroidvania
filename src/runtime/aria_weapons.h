/* SPDX-License-Identifier: GPL-3.0-only */
/* Weapon and equipment loaders of the Aria of Sorrow room runtime,
 * extracted from aria_runtime.c (AGENTS.md rule 8, decomposition wave
 * 5): the weapons.tsv record loader, the weapon entity's sprite
 * sequence and hitbox loader (classes 0, 2, 3), its free helper and
 * the equipment-menu weapon swap that rebinds the live runtime
 * objects. */
#ifndef ARIA_WEAPONS_H
#define ARIA_WEAPONS_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

/* Same include order as aria_entities.h: the opening-event header
 * precedes the shared types (AOS_OPENING_ACTOR_COUNT). */
#include "aos/title/aos_opening_event.h"

#include "aria_runtime_types.h"
#include "aria_entities.h"
#include "aos/weapons/aos_weapon.h"
#include "aos/core/aos_soma.h"

/* The weapon entity's sprite sequence and hitboxes (classes 0, 2,
 * 3): one animation's frames, per-frame ticks and hitboxes plus the
 * AosWeaponFrames view combat reads. */
typedef struct {
    AriaFrame frames[MAX_FRAMES];
    uint8_t ticks[MAX_FRAMES];
    AosHitbox hitboxes[MAX_FRAMES];
    AosAnimDef def;
    AosAnimSet set;
    AosWeaponFrames data;
    int count;
} AriaWeaponSprite;

/* weapons.tsv of src/assets/aos/weapons.c: "none" is the unarmed
 * record. */
bool load_weapon(const char *path, const char *name, AosWeapon *weapon);
/* Weapon/<name> sprite frames of DEFAULT_WEAPON_SPRITES plus the
 * per-frame hitboxes of DEFAULT_WEAPON_FRAMES. */
bool load_weapon_sprite(const char *name, AriaWeaponSprite *out,
                        SDL_Renderer *renderer);
void free_weapon_sprite(AriaWeaponSprite *sprite);
/* Apply the equipment menu's weapon choice to the same runtime
 * objects used at launch. */
bool reload_equipped_weapon(uint8_t item, AosSoma *soma,
                            AriaWeaponSprite *sprite, bool *loaded,
                            AosWeaponEntity *entity, AriaCombat *combat,
                            SDL_Renderer *renderer);

#endif /* ARIA_WEAPONS_H */
