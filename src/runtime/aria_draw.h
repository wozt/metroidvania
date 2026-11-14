/* SPDX-License-Identifier: GPL-3.0-only */
/* World-draw cluster of the Aria of Sorrow room runtime, extracted from
 * aria_runtime.c (AGENTS.md rule 8): the per-frame world pass — doors,
 * breakable walls, enemies (with variant palette banks and affine child
 * sprites), SPECIAL objects (Object20's private sprite table, the
 * param0-driven variants, the opening-gate families), the first-room
 * opening actors, shared effects, the level-up display and the damage
 * popups — plus the opening-actor motion callbacks and the still-frame
 * pickup/skill-art lookups the render loop needs. Pure drawing: no
 * state is owned here, every function reads the room/entity/art tables
 * it is handed. */
#ifndef ARIA_DRAW_H
#define ARIA_DRAW_H

/* aos_opening_event.h first: aria_runtime_types.h's AriaObjects uses
 * AOS_OPENING_ACTOR_COUNT from it (same include order as
 * aria_runtime.c). */
#include "aos/title/aos_opening_event.h"

#include "aria_runtime_types.h"

#include "aos/core/aos_soma.h"
#include "aos/intro/aos_levelup.h"

/* The skill-art record of AriaObjects.skills (enemies/variants share the
 * same anonymous layout; blade_frame only ever reads static_ids/statics). */
typedef __typeof__(((AriaObjects *)0)->skills[0]) AriaSkillArt;

/* The sprite art of a pickup, or -1 (items). */
int pickup_art(const AosPickup *pickup);
void draw_pickup_art(SDL_Renderer *renderer, const AriaObjects *objects, int index,
                     const AosPickup *pickup, int cam_x, int cam_y);
/* An art's exported still frame `id`, or NULL. */
const AriaFrame *blade_frame(const AriaSkillArt *art, int id);

/* Doors use their native sprite (frame 0 or 5 by parameter 0) and palette
 * cycle; without the object library an outline marks the blocked area.
 * `objects_ptr` is the AriaObjects table or NULL when it failed to load. */
void draw_doors(SDL_Renderer *renderer, const AriaRoom *room, const void *objects_ptr,
                long frame_count, int cam_x, int cam_y);
/* The breakable walls: their entry's frame at their position. */
void draw_walls(SDL_Renderer *renderer, const AriaRoom *room, const AriaObjects *objects,
                int cam_x, int cam_y);
/* Enemies: sprites face left; mirrored ones are flipped around x. */
void draw_enemies(SDL_Renderer *renderer, const AriaRoom *room, const AriaObjects *objects,
                  int cam_x, int cam_y);
/* Room SPECIAL objects (kind 2): drawn from their exported sprite sheets. */
void draw_special_objects(SDL_Renderer *renderer, const AriaRoom *room,
                          const AriaObjects *objects, int cam_x, int cam_y,
                          unsigned step);
/* Object20's first-room actors (sub_0805D198's Mina/Arikado/skeleton). */
void draw_opening_actors(SDL_Renderer *renderer, const AriaObjects *objects,
                         const AosOpeningActors *actors, int cam_x, int cam_y);
/* Per-actor animation durations in ticks for the opening actors. */
void opening_actor_durations(const AriaObjects *objects,
                             const AosOpeningActors *actors,
                             uint16_t durations[AOS_OPENING_ACTOR_COUNT]);
/* The motion callbacks aos_opening_actors_set_motion consumes. */
uint32_t opening_random(void *context);
int32_t opening_sine(uint32_t angle, void *context);
uint16_t opening_angle(int16_t x, int16_t y, void *context);
int opening_floor_depth(int32_t x, int32_t y, void *context);
int opening_wall_push(int32_t x, int32_t y, bool moving_left, void *context);
/* Shared effects of one kind (flames/smoke in front, blood behind). */
void draw_effects(SDL_Renderer *renderer, const AriaRoom *room,
                  const AriaObjects *objects, int kind, int cam_x, int cam_y);
/* sub_080455BC's level-up display entity (AosLevelUp). */
void draw_levelup(SDL_Renderer *renderer, const AriaObjects *objects,
                  const AosLevelUp *levelup, const AosSoma *soma,
                  int cam_x, int cam_y);
/* sub_0804301C's damage popup glyphs. */
void draw_damage_popup(SDL_Renderer *renderer, const AriaObjects *objects,
                       const AosDamagePopup *popup, int cam_x, int cam_y);

/* Shared still-frame lookup, defined in aria_runtime.c: the soul/skill
 * draw paths there and blade_frame here both resolve exported static
 * frames through it. */
int static_index(const int *ids, int count, int id);

#endif
