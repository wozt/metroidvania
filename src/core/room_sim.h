#ifndef FUSION_ROOM_SIM_H
#define FUSION_ROOM_SIM_H

#include <SDL3/SDL.h>

#include "core/types.h"
#include "core/room_sprite.h"
#include "core/tilemap.h"

#define ROOM_MAX_SOLIDS 12

typedef struct {
    const char *title;
    SDL_Color background;
    SDL_Color accent;
    SDL_FRect solids[ROOM_MAX_SOLIDS];
    int solid_count;
    SDL_FRect hazard;
    SDL_FRect target;
    SDL_FRect portal;
    float gravity;
    float move_speed[2];
    float jump_speed[2];
    float actor_width[2];
    float actor_height[2];
    float attack_range[2];
} RoomDefinition;

typedef struct {
    RoomDefinition definition;
    bool active;
    RoomSprites sprites;
    FusionTilemap tilemap;
    bool tilemap_loaded;
    float animation_clock;
    int animation_state;
    CharacterKind animation_character;
    bool facing_left;
    bool on_ground;
    float damage_cooldown;
    float attack_flash;
    float attack_animation_time;
    char notice[128];
    float notice_time;
} RoomRuntime;

void room_runtime_init(RoomRuntime *runtime, const RoomDefinition *definition);
bool room_runtime_load_tiles(RoomRuntime *runtime, const char *path,
                             WorldKind world);
void room_enter(RoomRuntime *runtime, WorldState *world);
void room_leave(RoomRuntime *runtime, WorldState *world);
void room_runtime_shutdown(RoomRuntime *runtime);
void room_tick(RoomRuntime *runtime, SessionState *session,
               const FusionInput *input, float dt);
void room_render(RoomRuntime *runtime, const SessionState *session,
                 SDL_Renderer *renderer, bool debug_overlay, float fps);

#endif
