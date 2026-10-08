#include "core/room_sim.h"
#include "core/session.h"

#include <assert.h>
#include <stdio.h>

static RoomDefinition test_room(void)
{
    RoomDefinition definition = {
        .title = "test", .gravity = 0,
        .move_speed = {0, 0}, .jump_speed = {0, 0},
        .actor_width = {10, 30}, .actor_height = {10, 30},
        .attack_range = {50, 50},
        .solid_count = 1,
        .solids = {{45, 75, 10, 20}}
    };
    return definition;
}

static void test_switch_corrects_collision_deterministically(void)
{
    SessionState session;
    RoomRuntime runtime;
    RoomDefinition definition = test_room();
    FusionInput input = {.switch_character_pressed = true};
    session_init(&session);
    session.worlds[WORLD_METROID].actor_x = 50;
    session.worlds[WORLD_METROID].actor_y = 100;
    room_runtime_init(&runtime, &definition);
    room_enter(&runtime, &session.worlds[WORLD_METROID]);
    room_tick(&runtime, &session, &input, 0);
    assert(session.active_character == CHARACTER_SOMA);
    assert(session.worlds[WORLD_METROID].actor_x == 26.0f);
}

static void test_switch_refused_without_safe_position(void)
{
    SessionState session;
    RoomRuntime runtime;
    RoomDefinition definition = test_room();
    FusionInput input = {.switch_character_pressed = true};
    definition.solids[0] = (SDL_FRect){-100, 0, 300, 200};
    session_init(&session);
    session.worlds[WORLD_METROID].actor_x = 50;
    session.worlds[WORLD_METROID].actor_y = 100;
    room_runtime_init(&runtime, &definition);
    room_enter(&runtime, &session.worlds[WORLD_METROID]);
    room_tick(&runtime, &session, &input, 0);
    assert(session.active_character == CHARACTER_SAMUS);
    assert(session.worlds[WORLD_METROID].actor_x == 50.0f);
}

static void test_verified_animation_timing(void)
{
    assert(room_sprite_frame_index(CHARACTER_SAMUS, SPRITE_IDLE, 15.f / 60.f) == 0);
    assert(room_sprite_frame_index(CHARACTER_SAMUS, SPRITE_IDLE, 16.f / 60.f) == 1);
    assert(room_sprite_frame_index(CHARACTER_SAMUS, SPRITE_RUN, 2.f / 60.f) == 1);
    assert(room_sprite_frame_index(CHARACTER_SAMUS, SPRITE_JUMP, 2.f / 60.f) == 1);
    assert(room_sprite_frame_index(CHARACTER_SAMUS, SPRITE_JUMP, 3.f / 60.f) == 2);
    assert(room_sprite_frame_index(CHARACTER_SAMUS, SPRITE_ATTACK, 4.f / 60.f) == 2);
    assert(room_sprite_frame_index(CHARACTER_SAMUS, SPRITE_ATTACK, 8.f / 60.f) == 0);
    assert(room_sprite_frame_index(CHARACTER_SOMA, SPRITE_IDLE, 29.f / 60.f) == 0);
    assert(room_sprite_frame_index(CHARACTER_SOMA, SPRITE_IDLE, 30.f / 60.f) == 1);
    assert(room_sprite_frame_index(CHARACTER_SOMA, SPRITE_IDLE, 41.f / 60.f) == 2);
    assert(room_sprite_frame_index(CHARACTER_SOMA, SPRITE_IDLE, 52.f / 60.f) == 3);
    assert(room_sprite_frame_index(CHARACTER_SOMA, SPRITE_IDLE, 63.f / 60.f) == 0);
}

int main(void)
{
    test_switch_corrects_collision_deterministically();
    test_switch_refused_without_safe_position();
    test_verified_animation_timing();
    puts("Collision and deterministic swap tests passed.");
    return 0;
}
