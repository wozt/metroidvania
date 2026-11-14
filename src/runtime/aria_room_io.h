/* SPDX-License-Identifier: GPL-3.0-only */
/* Room I/O cluster of the Aria of Sorrow room runtime, extracted
 * from aria_runtime.c (AGENTS.md rule 8, decomposition wave 5): room
 * file loading (load_room / load_room_folder and the map editor's
 * overlay pass), the sprite-library loader, the room music flag table,
 * the transition audit, the camera follow and door/spawn probes, and
 * the room background texture load. The entity spawn records, the
 * save-context pointers and the room-dialogue state stay owned by
 * aria_runtime.c / aria_entities.c; this header exposes what the
 * frame loop and the debug teleport borrow. */
#ifndef ARIA_ROOM_IO_H
#define ARIA_ROOM_IO_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* aos_opening_event.h first: aria_runtime_types.h's AriaObjects uses
 * AOS_OPENING_ACTOR_COUNT from it (same include order as
 * aria_draw.h / aria_runtime.c). */
#include "aos/title/aos_opening_event.h"

#include "aria_runtime_types.h"

#include "aos/core/aos_collision.h"
#include "aos/core/aos_room_music.h"
#include "aos/core/aos_audio.h"
#include "aos/game/aos_room.h"
#include "aos/game/aos_room_overlay.h"

/* metadata room_music.tsv flags per (area, room): sub_08013960's
 * save/warp/transition bits, filled by load_room_music. */
extern AosRoomMusicFlags room_music_flags[MUSIC_AREAS][MUSIC_ROOMS];

/* The overlay pass's switches: --no-overlays and --overlay-dir set
 * them from main()'s argument parsing; apply_room_overlay reads them. */
extern bool aria_overlays_enabled;
extern const char *aria_overlay_directory;

bool load_room_music(const char *path);
void start_mp2k_song(void *context, uint16_t song,
                     const AosAudioSongInfo *info);

void free_room(AriaRoom *room);
AosCollision room_layer(const AriaRoom *room);
bool load_room(const char *path, AriaRoom *room);
bool room_has_save_point(const AriaRoom *room);
/* Reads the sprite index; textures are created only with a renderer. */
bool load_library(const char *path, AriaLibrary *lib, SDL_Renderer *renderer);
void room_folder(char *out, size_t size, int area, int number);
void apply_room_overlay(AriaRoom *room);
bool load_room_folder(const char *folder, AriaRoom *room);
int audit_transitions(void);

void follow_camera(const AriaRoom *room, int32_t x, int32_t y,
                   int *cam_x, int *cam_y);
SDL_Texture *load_background(SDL_Renderer *renderer, const char *folder);
const AosProjectDoor *project_door_touched(const AriaRoom *room,
                                           int32_t x, int32_t y);
bool find_spawn(const AosCollision *layer, int *out_x, int *out_y);

#endif /* ARIA_ROOM_IO_H */
