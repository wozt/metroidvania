/* SPDX-License-Identifier: GPL-3.0-only */
/* Menus/debug cluster of the Aria of Sorrow room runtime, extracted
 * from aria_runtime.c (AGENTS.md rule 8, decomposition wave 4): the
 * debug-menu actions and draw-side helpers the frame loop calls —
 * debug teleport/spawn, the native pause-graphics page draws, the
 * draw_soul_menu fallback and the draw_debug overlay. Pure drawing
 * and debug actions: the menu/soul/progress state stays owned by the
 * caller; the only runtime state touched is the shared room_dialogue
 * the teleport closes. */
#ifndef ARIA_MENUS_DEBUG_H
#define ARIA_MENUS_DEBUG_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* aos_opening_event.h first: aria_runtime_types.h's AriaObjects uses
 * AOS_OPENING_ACTOR_COUNT from it (same include order as
 * aria_draw.h / aria_runtime.c). */
#include "aos/title/aos_opening_event.h"

#include "aria_runtime_types.h"
#include "aria_entities.h"

#include "aos/core/aos_collision.h"
#include "aos/core/aos_soma.h"
#include "aos/core/aos_stats.h"
#include "aos/game/aos_pickup.h"
#include "aos/menus/aos_pause_graphics.h"
#include "aos/menus/aos_quick_map.h"
#include "aos/souls/aos_soul_menu.h"

#include "debug_menu.h"

/* Debug menu (F1, or the gamepad chord): every item edits the running
 * engine's state; nothing here exists in the original game. */
enum {
    DEBUG_PAUSE = 1, DEBUG_STEP, DEBUG_HITBOXES, DEBUG_HP, DEBUG_MAX_HP, DEBUG_REFILL,
    DEBUG_ATK, DEBUG_DEF, DEBUG_MOVES, DEBUG_ENEMY, DEBUG_SPAWN, DEBUG_CLEAR,
    DEBUG_AREA, DEBUG_ROOM, DEBUG_TELEPORT,
};

/* The five debug move toggles' labels and AOS_MOVE_* bits; the frame
 * loop mirrors soma.moves into debug_moves[] through them. */
extern const char *const debug_move_labels[5];
extern const uint32_t debug_move_bits[5];

/* An exported room at a floor found by find_spawn; Soma keeps HP, moves,
 * weapon and facing, and restarts in his normal state. */
bool debug_teleport(SDL_Renderer *renderer, AriaRoom *room, SDL_Texture **background,
                    AosSoma *soma, const AosAnimSet *anims, int area, int number);
/* A ported enemy 48 pixels in front of Soma, created as a room record. */
bool debug_spawn(AriaRoom *room, const AriaCombat *combat, const AosSoma *soma,
                 const AosCollision *layer, int id);

/* The native pause pages rendered into the shared pause texture; each
 * returns false when its page is not the open one so the caller can
 * chain them into the draw_soul_menu fallback. */
bool draw_native_pause_hub(SDL_Renderer *renderer, SDL_Texture *texture,
                           AosPauseGraphics *graphics,
                           const AosSoulMenu *menu, const AosStats *stats,
                           const AosSoma *soma, uint32_t play_frames);
bool draw_native_soul_set(SDL_Renderer *renderer, SDL_Texture *texture,
                          AosPauseGraphics *graphics,
                          const AosSoulMenu *menu, const AosStats *stats);
bool draw_native_equip(SDL_Renderer *renderer, SDL_Texture *texture,
                       AosPauseGraphics *graphics,
                       const AosSoulMenu *menu, const AosStats *stats,
                       const AosEquipmentTables *equipment);
bool draw_native_config(SDL_Renderer *renderer, SDL_Texture *texture,
                        AosPauseGraphics *graphics,
                        const AosSoulMenu *menu);
bool draw_native_sleep(SDL_Renderer *renderer, SDL_Texture *texture,
                       AosPauseGraphics *graphics,
                       const AosSoulMenu *menu);
bool draw_native_ability(SDL_Renderer *renderer, SDL_Texture *texture,
                         AosPauseGraphics *graphics,
                         const AosSoulMenu *menu, const AosStats *stats);
bool draw_native_item_use(SDL_Renderer *renderer, SDL_Texture *texture,
                          AosPauseGraphics *graphics,
                          const AosSoulMenu *menu, const AosStats *stats,
                          const AosItemUseRecord items[AOS_ITEM_RECORDS]);
bool draw_native_enemy(SDL_Renderer *renderer, SDL_Texture *texture,
                       AosPauseGraphics *graphics,
                       const AosSoulMenu *menu, const AosStats *stats,
                       AriaObjects *objects);
bool draw_native_quick_map(SDL_Renderer *renderer, SDL_Texture *texture,
                           AosQuickMap *map, const AosStats *stats,
                           uint32_t animation_frame);

/* Source-adapted slices of sub_080473F4 / sub_08047764. Native tile, icon,
 * description and transition rendering remain separate future work. */
void draw_soul_menu(SDL_Renderer *renderer, const AosSoulMenu *menu,
                    const AosStats *stats, const AosEquipmentTables *equipment,
                    const AosItemUseRecord items[AOS_ITEM_RECORDS],
                    const AosSoma *soma, const AriaObjects *objects);
/* The debug overlay: the paused strip or the F1 menu panel. */
void draw_debug(SDL_Renderer *renderer, const DebugMenu *menu, bool paused,
                const AriaRoom *room, const AosSoma *soma, long step);

/* Room-loading helpers owned by aria_runtime.c that debug_teleport
 * borrows (same borrowed-helper pattern as aria_entities.h). */
void free_room(AriaRoom *room);
AosCollision room_layer(const AriaRoom *room);
void room_folder(char *out, size_t size, int area, int number);
bool load_room_folder(const char *folder, AriaRoom *room);
SDL_Texture *load_background(SDL_Renderer *renderer, const char *folder);
bool find_spawn(const AosCollision *layer, int *out_x, int *out_y);

#endif /* ARIA_MENUS_DEBUG_H */
