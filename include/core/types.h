#ifndef FUSION_CORE_TYPES_H
#define FUSION_CORE_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#define FUSION_SAVE_VERSION 1u
#define FUSION_CHARACTER_COUNT 2u
#define FUSION_WORLD_COUNT 2u

typedef enum {
    WORLD_METROID = 0,
    WORLD_CASTLEVANIA = 1
} WorldKind;

typedef enum {
    CHARACTER_SAMUS = 0,
    CHARACTER_SOMA = 1
} CharacterKind;

typedef struct {
    int32_t hp;
    int32_t max_hp;
    uint32_t inventory_flags;
    uint32_t ability_flags;
    bool available;
} CharacterState;

typedef struct {
    float actor_x;
    float actor_y;
    float actor_vx;
    float actor_vy;
    int32_t target_hp;
    int32_t target_max_hp;
    bool door_open;
    bool visited;
} WorldState;

typedef struct {
    uint32_t version;
    WorldKind active_world;
    CharacterKind active_character;
    CharacterState characters[FUSION_CHARACTER_COUNT];
    WorldState worlds[FUSION_WORLD_COUNT];
    uint32_t shared_map_flags;
    uint32_t shared_boss_flags;
    uint32_t synergy_placeholder;
} SessionState;

typedef struct {
    bool left;
    bool right;
    bool jump_pressed;
    bool attack_pressed;
    bool switch_character_pressed;
    bool switch_world_pressed;
    bool debug_pressed;
    bool damage_pressed;
    bool save_pressed;
    bool load_pressed;
} FusionInput;

const char *world_name(WorldKind world);
const char *character_name(CharacterKind character);

#endif
