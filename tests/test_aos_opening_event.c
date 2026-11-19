/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos/title/aos_opening_event.h"
#include "aos/core/aos_effect.h"
#include "aos/intro/aos_intro_resources.h"
#include "aos/core/aos_soma.h"

#include <assert.h>
#include <string.h>

#include "aos/core/aos_input.h"

static uint32_t zero_random(void *context) {
    (void)context;
    return 0;
}

static uint32_t one_random(void *context) {
    (void)context;
    return 1;
}

static int32_t full_sine(uint32_t angle, void *context) {
    (void)angle;
    (void)context;
    return 0x10000;
}

static uint16_t zero_angle(int16_t x, int16_t y, void *context) {
    (void)x;
    (void)y;
    (void)context;
    return 0;
}

typedef struct {
    int32_t last_probe;
    int wall_push;
    bool moving_left;
} TestCollision;

static int test_floor(int32_t x, int32_t y, void *context) {
    TestCollision *collision = context;
    collision->last_probe = (x << 16) | (y & 0xFFFF);
    return y >= 100 ? -2 : 0;
}

static int test_wall(int32_t x, int32_t y, bool moving_left, void *context) {
    (void)x;
    (void)y;
    TestCollision *collision = context;
    collision->moving_left = moving_left;
    return collision->wall_push;
}

static void test_emitted_motion(void) {
    AosOpeningActors actors;
    aos_opening_actors_init(&actors);
    aos_opening_actors_set_motion(&actors, zero_random, full_sine,
                                  zero_angle, NULL, NULL, NULL);
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_ENEMY_APPEARS);
    assert(actors.emitted_count == 6);
    int32_t knight_y = actors.emitted[0].y;
    int32_t winged_y = actors.emitted[5].y;
    assert(actors.emitted[5].phase == 0);
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(actors.emitted[0].state == 1);
    assert(actors.emitted[5].state == 1 &&
           actors.emitted[5].y == winged_y + 0x4000 &&
           actors.emitted[5].phase == 0x200);
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(actors.emitted[0].state == 1 && actors.emitted[0].step == 1 &&
           actors.emitted[0].timer == 16);
    assert(actors.emitted[5].state == 2 &&
           actors.emitted[5].y == winged_y + 0x8000);
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(actors.emitted[0].timer == 15 &&
           actors.emitted[0].y == knight_y - 0x8000);
    assert(actors.emitted[2].state == 0);
}

/* sub_0806AF98's floor snap for type-10 children: a flat floor 2 pixels
 * below y=200 reports -2 inside it and 0 above it. The distant batch's
 * first child spawns at y=0x29F inside the floor, so the +0x46 -= 8 walk
 * climbs to y=199, the +7 back-off lands at 206 and the final probe adds
 * -2, snapping the spawn to y=204 before the parameter-bit-2 +8 drop. */
static int snap_floor(int32_t x, int32_t y, void *context) {
    (void)x;
    (void)context;
    return y > 200 ? -2 : 0;
}

static void test_emitted_floor_snap(void) {
    AosOpeningActors actors;
    aos_opening_actors_init(&actors);
    aos_opening_actors_set_motion(&actors, zero_random, full_sine,
                                  zero_angle, snap_floor, NULL, NULL);
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_ENEMY_APPEARS);
    assert(actors.emitted_count == 6);
    /* emitted[0] is distant record 0: type 10, parameter 4. */
    assert(actors.emitted[0].entity_type == 10);
    assert(actors.emitted[0].y == (204 + 8) << 16);
    /* The type-8 child never gets the sub_0806AF98 snap. */
    assert(actors.emitted[5].entity_type == 8);
    assert(actors.emitted[5].y == (0x288) << 16);
}

static void test_skeleton_collision_flight(void) {
    AosOpeningActors actors;
    aos_opening_actors_init(&actors);
    TestCollision collision = {0};
    aos_opening_actors_set_motion(&actors, zero_random, full_sine,
                                  zero_angle, test_floor, test_wall,
                                  &collision);
    actors.emitted_count = 1;
    actors.emitted[0] = (AosOpeningEntity){
        .x = 50 << 16,
        .y = 96 << 16,
        .entity_type = 10,
        .parameter = 8,
        .state = 2,
        .active = true,
    };
    AosOpeningEntity *entity = &actors.emitted[0];

    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(entity->animation == 1 && entity->step == 1 &&
           entity->timer == 0x9F && entity->x == (50 << 16) &&
           entity->y == (98 << 16));
    assert(collision.last_probe == ((49 << 16) | 98) &&
           collision.moving_left);

    collision.wall_push = 1;
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(entity->timer == 0x9E && entity->x == (50 << 16) + 0xC000 &&
           entity->y == (98 << 16));
    assert(collision.last_probe == ((50 << 16) | 100));

    entity->mirrored = true;
    collision.wall_push = -1;
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(entity->timer == 0x9D && entity->x == (50 << 16) &&
           entity->y == (98 << 16));
    assert(!collision.moving_left);

    actors.random = one_random;
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(entity->timer == 0x9D && entity->x == (50 << 16) &&
           entity->y == (98 << 16));
}

static void test_emitted_attack_tails(void) {
    AosOpeningActors actors;
    aos_opening_actors_init(&actors);
    aos_opening_actors_set_motion(&actors, zero_random, full_sine,
                                  zero_angle, NULL, NULL, NULL);
    actors.camera_x = 0xDF << 16;
    actors.emitted_count = 1;
    actors.emitted[0] = (AosOpeningEntity){
        .x = 0x158 << 16,
        .y = 0x288 << 16,
        .entity_type = 8,
        .variant = 0,
        .state = 4,
        .active = true,
        .disappearing = true,
    };
    AosOpeningEntity *entity = &actors.emitted[0];
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(entity->step == 1 && entity->timer == 0x20);
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(!entity->hidden && entity->timer == 0x1F);
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(entity->hidden && entity->timer == 0x1E);
    /* +0xB==1's blink runs exactly 0x20 frames (+0xD 0x20..1); the
     * Winged Skeleton then reloads +0xD=0x10 for +0xB==2's countdown
     * before +0xB==3's selector gate fires the margin-checked death. */
    unsigned tail_guard = 0;
    while (entity->step < 2 && tail_guard++ < 64)
        aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(entity->step == 2 && entity->timer == 0x10 && !entity->hidden);
    tail_guard = 0;
    while (entity->step < 3 && tail_guard++ < 64)
        aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(entity->step == 3 && entity->active);
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(entity->state == 5 && entity->active &&
           entity->events == (AOS_OPENING_ENTITY_EVENT_SOUND_0X6B |
                              AOS_OPENING_ENTITY_EVENT_DEBRIS));
    assert(actors.debris_count == 7);
    assert(actors.debris[0].active && actors.debris[0].source_type == 8 &&
           actors.debris[0].frame == 0x24 &&
           actors.debris[0].x == entity->x &&
           actors.debris[0].y == entity->y - 30 * 0x10000 &&
           actors.debris[0].vx == 16 * 0x1000 &&
           actors.debris[0].vy == -64 * 0x1000 &&
           actors.debris[0].timer == 60);
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(!entity->active && entity->events == AOS_OPENING_ENTITY_EVENT_NONE);

    actors.emitted[0] = (AosOpeningEntity){
        .x = 0x400 << 16,
        .y = 0x288 << 16,
        .entity_type = 10,
        .variant = 0,
        .state = 3,
        .active = true,
        .disappearing = true,
    };
    entity = &actors.emitted[0];
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    /* The Skeleton Knight shares the 0x20->0xFF blink — 33 frames —
     * but its +0xB==2 is a single selector gate with no second
     * countdown. */
    for (unsigned frame = 0; frame < 33; ++frame)
        aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(entity->step == 2 && entity->active);
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(entity->step == 3 && entity->events == AOS_OPENING_ENTITY_EVENT_NONE);
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(entity->state == 4 && entity->active &&
           entity->events == AOS_OPENING_ENTITY_EVENT_PROJECTILE);
    assert(actors.projectile_count == 1 &&
           actors.projectiles[0].active &&
           actors.projectiles[0].x == entity->x &&
           actors.projectiles[0].y == entity->y - 0x200000 &&
           actors.projectiles[0].heading == 0x8000 &&
           actors.projectiles[0].speed == 0x30000 &&
           actors.projectiles[0].timer == 0xC8);
    int32_t projectile_x = actors.projectiles[0].x;
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(!entity->active && actors.projectiles[0].active &&
           actors.projectiles[0].speed == 0x31800 &&
           actors.projectiles[0].turn == 0x2A &&
           actors.projectiles[0].x == projectile_x + 0x31800 &&
           actors.projectiles[0].timer == 0xC7);

    actors.emitted[0] = (AosOpeningEntity){
        .x = 0x158 << 16,
        .y = 0x288 << 16,
        .entity_type = 10,
        .variant = 0,
        .state = 3,
        .active = true,
        .disappearing = true,
    };
    for (unsigned frame = 0; frame < 36; ++frame)
        aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    /* sub_0805E5D4's +0xB==3 death gates only SE 0x6B and the flame on
     * the impact margin; the bolt fires unconditionally and Skeleton
     * Knights never emit debris, so the count stays at the Winged
     * Skeleton's seven. */
    assert(actors.emitted[0].state == 4 && actors.debris_count == 7 &&
           actors.flame_count > 0 && actors.projectile_count == 2);

    actors.projectile_count = 1;
    /* sub_0805EE88's margin is on Arikado (Object20's +0x18 entity),
     * not the scripted skeleton. */
    actors.projectiles[0] = (AosOpeningProjectile){
        .x = actors.x[1], .y = actors.y[1], .state = 2, .active = true};
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(!actors.projectiles[0].active &&
           actors.projectiles[0].events ==
               AOS_OPENING_ENTITY_EVENT_SOUND_0XBC);
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(actors.projectiles[0].events == AOS_OPENING_ENTITY_EVENT_NONE);
}

static void test_skeleton_approach(void) {
    /* sub_0805DAA0 +0xB==1: the stop guard compares entity +0x42 —
     * the high word of the same +0x40 sub_0806D430 integrates —
     * against 0xCA minus the live camera word, so the camera terms
     * cancel and the walk ends at world x 0xCA. The move is
     * sub_0806E140(0x30000, 0x200000, 0x14000, 1): in open air
     * sub_080698A8's single integrations are restored and the
     * caller's own adds net -0x30000 in x and +0x14000 in y. */
    AosOpeningActors actors;
    aos_opening_actors_init(&actors);
    actors.state = 2;
    actors.camera_active = false;
    actors.skeleton_state = 2;
    actors.skeleton_step = 2;
    actors.x[2] = AOS_OPENING_SKELETON_X << 16;
    actors.y[2] = AOS_OPENING_SKELETON_Y << 16;
    int32_t camera_x = 0x30;
    aos_opening_actors_update(&actors, NULL, &camera_x, 0);
    assert(actors.x[2] == ((AOS_OPENING_SKELETON_X << 16) - 0x30000));
    assert(actors.y[2] == ((AOS_OPENING_SKELETON_Y << 16) + 0x14000));
    /* One frame from 0xCB still walks (0xCB > 0xCA); the next update
     * sees 0xC8 <= 0xCA and sub_0805DBFC selects animation 2 with
     * +0xA=3 — regardless of where the camera panned. */
    actors.x[2] = 0xCB << 16;
    aos_opening_actors_update(&actors, NULL, &camera_x, 0);
    assert(actors.skeleton_state == 2);
    aos_opening_actors_update(&actors, NULL, &camera_x, 0);
    assert(actors.skeleton_state == 3 && actors.animation[2] == 2 &&
           actors.anim_loop[2]);
    /* The guard is camera-independent: a camera that panned away
     * does not move the 0xCA world-space stop. */
    AosOpeningActors moved;
    aos_opening_actors_init(&moved);
    moved.state = 2;
    moved.camera_active = false;
    moved.skeleton_state = 2;
    moved.skeleton_step = 2;
    moved.x[2] = 0xCB << 16;
    moved.y[2] = AOS_OPENING_SKELETON_Y << 16;
    int32_t moved_camera = 0x50;
    aos_opening_actors_update(&moved, NULL, &moved_camera, 0);
    assert(moved.skeleton_state == 2);
    moved_camera = 0x40;
    aos_opening_actors_update(&moved, NULL, &moved_camera, 0);
    assert(moved.skeleton_state == 3);
}

static void test_trigger(void) {
    assert(AOS_OPENING_SOMA_X == 168 && AOS_OPENING_SOMA_Y == 704);
    assert(AOS_OPENING_ACTOR_COUNT == 3);
    /* sub_0805D198 writes record-0 child's +0x42 as the player's own
     * +0x42 minus 0x18 — Mina stands one step left of Soma. */
    assert(AOS_OPENING_MINA_X == AOS_OPENING_SOMA_X - 0x18 &&
           AOS_OPENING_MINA_Y == AOS_OPENING_SOMA_Y);
    assert(AOS_OPENING_ARIKADO_X == 0x119 && AOS_OPENING_ARIKADO_Y == 0x2C0);
    assert(AOS_OPENING_SKELETON_X == 0x16F && AOS_OPENING_SKELETON_Y == 0x268);
    AosOpeningActors actors;
    aos_opening_actors_init(&actors);
    /* The emitted Skeleton Knights' state machine needs the native RNG;
     * sine/angle/floor stay NULL so the soul orb uses its fallback
     * lerp and the scripted skeleton simply falls while walking. */
    aos_opening_actors_set_motion(&actors, zero_random, NULL, NULL,
                                  NULL, NULL, NULL);
    assert(actors.active && actors.state == 0 && actors.timer == 0x40);
    assert(actors.x[0] == (AOS_OPENING_MINA_X << 16));
    assert(actors.x[1] == (AOS_OPENING_ARIKADO_X << 16));
    assert(actors.animation[0] == 4 && actors.animation[1] == 1 && actors.animation[2] == 4);
    for (unsigned frame = 0; frame < 64; ++frame)
        aos_opening_actors_update(&actors, NULL, NULL, 0);
    /* sub_0805D198's +0x48 store of -0x6000 has no consumer in the
     * dump: every opening child runs the animation-only sub_0805B748
     * (only the skeleton child gets sub_0805DAA0), sub_0805D410 never
     * writes child18's position fields, and native WRAM captures keep
     * his entity +0x40 world x at 0x119 for the whole scene. The
     * timer therefore goes straight to the post-suspend idle and he
     * never translates; the perceived approach is the camera pan. */
    assert(actors.state == 2 && actors.timer == 0 && actors.x[1] == (0x119 << 16));
    assert(actors.animation[1] == 1);
    for (unsigned frame = 0; frame < 200; ++frame)
        aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(actors.x[1] == (0x119 << 16) && actors.animation[1] == 1);
    assert(actors.visible[0] && actors.visible[1] && actors.visible[2]);

    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_CONFRONTATION);
    /* sub_0805D410 +0xB==2's sub-state-0 attribute writes: child0's
     * +0x58 loses bits 0 and 6, child1's gains bit 6 — Genya flips
     * toward the incoming threat while Mina does not. */
    assert(actors.actor_flags58[0] == 0 && actors.actor_flags58[1] == 0x40);
    assert(aos_opening_actors_update(&actors, NULL, NULL, 0) ==
           AOS_OPENING_CUE_CONFRONTATION);
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_ENEMY_APPEARS);
    assert(actors.emitted_batches == AOS_OPENING_EMISSION_DISTANT);
    assert(actors.emitted_count == 6);
    assert(actors.emitted[0].active && actors.emitted[0].entity_type == 10 &&
           actors.emitted[0].animation == 0 &&
           actors.emitted[0].y == (0x2A7 << 16));
    assert(actors.emitted[5].active && actors.emitted[5].entity_type == 8 &&
           actors.emitted[5].animation == 4);
    int32_t camera_x = 48;
    for (unsigned frame = 0; frame < 49; ++frame)
        assert(aos_opening_actors_update(&actors, NULL, &camera_x, 0) ==
               AOS_OPENING_CUE_NONE);
    assert(camera_x == 219 && actors.camera_active);
    assert(aos_opening_actors_update(&actors, NULL, &camera_x, 0) ==
           AOS_OPENING_CUE_NONE);
    assert(camera_x == 223 && !actors.camera_active);
    /* +0xB==3 sub-state 2's +0xD=0x20 wrap wait plus sub-state 3's
     * next-frame emission: 0x22 portable frames. */
    for (unsigned frame = 0; frame < 33; ++frame)
        assert(aos_opening_actors_update(&actors, NULL, &camera_x, 0) == AOS_OPENING_CUE_NONE);
    assert(aos_opening_actors_update(&actors, NULL, &camera_x, 0) ==
           AOS_OPENING_CUE_ENEMY_APPEARS);
    assert(actors.animation[0] == 5 && actors.animation_age[0] == 0);
    /* +0xB==3 sub-state 3's child18 +0x58 rewrite toggles bit 6 back
     * off (and clears bit 0) as the near batch lands — Genya faces
     * Soma again. */
    assert(actors.actor_flags58[1] == 0);
    assert(actors.emitted_batches ==
           (AOS_OPENING_EMISSION_DISTANT | AOS_OPENING_EMISSION_NEAR_SOMA));
    assert(actors.emitted_count == AOS_OPENING_EMITTED_CAPACITY);
    assert(actors.emitted[6].animation == 0 && !actors.emitted[6].mirrored);
    assert(actors.emitted[7].animation == 0 && actors.emitted[7].mirrored);
    assert(actors.emitted[8].entity_type == 8 && actors.emitted[8].mirrored);
    assert(actors.emitted[9].entity_type == 8 && !actors.emitted[9].mirrored);

    AosOpeningSpawn spawns[AOS_OPENING_EMISSION_MAX_SPAWNS];
    assert(aos_opening_emission_spawns(AOS_OPENING_EMISSION_DISTANT,
                                       AOS_OPENING_SOMA_X, spawns,
                                       AOS_OPENING_EMISSION_MAX_SPAWNS) == 6);
    assert(spawns[0].x == 0x158 && spawns[0].y == 0x29F &&
           spawns[0].entity_type == 10 && spawns[0].parameter == 4 &&
           spawns[0].variant == 0);
    assert(spawns[5].x == 0x14F && spawns[5].y == 0x288 &&
           spawns[5].entity_type == 8);
    assert(aos_opening_emission_spawns(AOS_OPENING_EMISSION_NEAR_SOMA,
                                       AOS_OPENING_SOMA_X, spawns,
                                       AOS_OPENING_EMISSION_MAX_SPAWNS) == 4);
    assert(spawns[0].x == 0xC0 && spawns[0].parameter == 0x12 &&
           spawns[0].variant == 5);
    assert(spawns[1].x == 0x78 && spawns[1].parameter == 2 &&
           spawns[1].variant == 6);
    assert(spawns[2].x == 0x7C && spawns[3].x == 0xC4);

    const uint16_t durations[AOS_OPENING_ACTOR_COUNT] = {1, 3, 1};

    /* +0xB==3's sub-state-4 tail auto-advances into +0xB==4 without a
     * script beat: the second 0x21-frame wait runs, then entity+0x38=3's
     * mode-3 return pan and sub-state 2's SE 0x1DF + animation 2. */
    for (unsigned frame = 0; frame < 0x21; ++frame)
        assert(aos_opening_actors_update(&actors, durations, &camera_x, 0) ==
               AOS_OPENING_CUE_NONE);
    assert(actors.scene_state == 4 && actors.scene_step == 0 &&
           actors.scene_timer == 0 && actors.camera_mode == 3);
    unsigned guard = 0;
    while (actors.scene_step == 0 && guard++ < 200)
        aos_opening_actors_update(&actors, durations, &camera_x, 0);
    assert(actors.scene_step == 1 && camera_x <= 0x60);
    assert(aos_opening_actors_update(&actors, durations, &camera_x, 0) ==
           AOS_OPENING_CUE_ENEMY_ATTACKS);
    assert(actors.animation[1] == 2 && actors.scene_step == 2);
    /* child18's +0x59 animation-finished flag raises parent +0x20 bit
     * 1; emitted children poll it only from their own state 2, so the
     * disappearance tail begins a frame after the flag, not at the
     * flag itself. */
    guard = 0;
    while (!actors.emitted[0].disappearing && guard++ < 200)
        aos_opening_actors_update(&actors, durations, &camera_x, 0);
    assert(actors.emitted[0].disappearing && actors.emitted[0].state == 3 &&
           actors.scene_step == 3);
    /* sub-state 4's 0x10->0xFF tail is 0x11 frames, then SE 0x1E0 and
     * animation 3; the flag-poll wait above already consumed a frame
     * of that tail, so wait on the step boundary instead of a fixed
     * count. */
    guard = 0;
    int tail_cue = AOS_OPENING_CUE_NONE;
    while (actors.scene_step == 3 && guard++ < 64)
        tail_cue = aos_opening_actors_update(&actors, durations,
                                             &camera_x, 0);
    assert(tail_cue == AOS_OPENING_CUE_ENEMY_ATTACKS_AGAIN);
    assert(actors.animation[1] == 3 && actors.animation_age[1] == 0 &&
           actors.scene_step == 4);
    for (unsigned frame = 0; frame < 3; ++frame)
        aos_opening_actors_update(&actors, durations, &camera_x, 0);
    assert(actors.scene_step == 5 && actors.scene_timer == 0x59);
    for (unsigned frame = 0; frame < 0x58; ++frame)
        aos_opening_actors_update(&actors, durations, &camera_x, 0);
    assert(aos_opening_actors_update(&actors, durations, &camera_x, 0) ==
           AOS_OPENING_CUE_NONE);
    assert(actors.scene_step == 6 && !aos_opening_actors_blocks_script(&actors));

    /* Script beat 4 lands on +0xB==5, the soul battle: a 0x12-frame
     * preamble, then the 0xD2 countdown whose 0xD0 and 0x8F events fire
     * on the countdown's third and 0x44th checks. */
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_ENEMY_ATTACKS);
    assert(aos_opening_actors_blocks_script(&actors));
    assert(actors.scene_state == 5 && actors.scene_step == 0 &&
           actors.scene_timer == 0x12);
    for (unsigned frame = 0; frame < 20; ++frame)
        assert(aos_opening_actors_update(&actors, durations, &camera_x, 0) ==
               AOS_OPENING_CUE_NONE);
    /* The 0xD2 -> 0x90 countdown still sits above the +0xD<=0x8F
     * window, so sub_0801D028(0x52) has not run yet: Soma keeps the
     * spawn pose 0x51 and the +0x5A visibility bit stays set — no
     * absorb-pose or blink during the early countdown. */
    assert(actors.soma_animation == 0x51 && !actors.soma_hidden);
    /* Countdown update 3 sees +0xD==0xD0: sub_0805E9F0's seven
     * pieces (the sb=6..0 loop consumes all of 0x08526802),
     * sub_0806D578's flame, sub_0805EAFC's soul orb, SE 0x6B and
     * the 0x28-frame blink death all start; the body is not
     * removed instantly. */
    assert(aos_opening_actors_update(&actors, durations, &camera_x, 0) ==
           AOS_OPENING_CUE_ENEMY_DEFEATED);
    assert(actors.skeleton_death == 0x28 && actors.soul_active &&
           !actors.soul_absorbed);
    unsigned death_pieces = 0;
    for (unsigned piece = 0; piece < actors.debris_count; ++piece)
        if (actors.debris[piece].active &&
            actors.debris[piece].source_type == 8) ++death_pieces;
    assert(death_pieces == 7);
    int32_t soul_start_x = actors.soul_x;
    int32_t soul_start_y = actors.soul_y;
    for (unsigned frame = 0; frame < 64; ++frame)
        assert(aos_opening_actors_update(&actors, durations, &camera_x, 0) == AOS_OPENING_CUE_NONE);
    assert(!actors.visible[2] && actors.skeleton_death == 0);
    assert(actors.soul_x != soul_start_x || actors.soul_y != soul_start_y);
    /* sub_0805F0B0's touch callback removes the orb on arrival — well
     * before the parent's +0xD==0x8F absorption beat — so by now the
     * soul has landed on Soma under its own lifetime, not a snap. */
    assert(actors.soul_arrived && !actors.soul_active);
    assert(aos_opening_actors_update(&actors, durations, &camera_x, 0) ==
           AOS_OPENING_CUE_SOUL_ABSORBED);
    assert(actors.soul_absorbed && actors.soma_animation == 0x52);
    /* The script stays blocked only while the window is open or waiting;
     * native state 5 resumes as soon as sub_0800F0AC starts the close. */
    assert(aos_opening_actors_blocks_script(&actors));
    assert(aos_opening_actors_update(&actors, durations, &camera_x, 0) ==
           AOS_OPENING_CUE_NONE);
    assert(actors.soul_notice == 2);
    aos_opening_actors_input(&actors, AOS_KEY_A);
    assert(aos_opening_actors_update(&actors, durations, &camera_x, 0) ==
           AOS_OPENING_CUE_SOUL_NOTICE_DISMISS);
    assert(actors.soul_notice == 3 && actors.soul_notice_timer == 0x20);
    assert(!aos_opening_actors_blocks_script(&actors));
    /* sub-state 5 keeps pose 0x52 once, without the absorption blink. */
    assert(aos_opening_actors_update(&actors, durations, &camera_x, 0) ==
           AOS_OPENING_CUE_NONE);
    assert(!actors.soma_hidden && actors.soma_animation == 0x52);
    for (unsigned frame = 0; frame < 0x1F; ++frame)
        aos_opening_actors_update(&actors, durations, &camera_x, 0);
    assert(actors.soul_notice == 0);
    assert(!aos_opening_actors_blocks_script(&actors));

    /* Script beat 5 lands on +0xB==6: sub_08013CF0(0x17)'s music change
     * (the reused AFTER_BATTLE audio label) under a 0x21-frame suspend
     * hold, then the script is released. */
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_SOUL_ABSORBED);
    assert(aos_opening_actors_blocks_script(&actors));
    assert(aos_opening_actors_update(&actors, durations, &camera_x, 0) ==
           AOS_OPENING_CUE_AFTER_BATTLE);
    for (unsigned frame = 0; frame < 0x20; ++frame)
        assert(aos_opening_actors_update(&actors, durations, &camera_x, 0) ==
               AOS_OPENING_CUE_NONE && aos_opening_actors_blocks_script(&actors));
    aos_opening_actors_update(&actors, durations, &camera_x, 0);
    assert(!aos_opening_actors_blocks_script(&actors));

    /* Script beat 6 lands on +0xB==7: sub_0805B878(0x1041)'s one-shot
     * (the reused COMPLETE audio label), then +0xC=1 idles. The
     * absorbed pose 0x52 remains until the +0xA==0x80 teardown. */
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_AFTER_BATTLE);
    assert(!actors.soul_active && !actors.soma_hidden &&
           actors.soma_animation == 0x52 && actors.scene_state == 7);
    assert(aos_opening_actors_update(&actors, durations, &camera_x, 0) ==
           AOS_OPENING_CUE_COMPLETE);
    assert(!aos_opening_actors_blocks_script(&actors));

    /* Script beat 7 lands on +0xB==8: the mode-4 return pan holds the
     * suspend until sub_0805CCE4 reports Soma recentered. */
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_RETURN_PAN);
    assert(aos_opening_actors_blocks_script(&actors));
    assert(actors.scene_state == 8 && actors.camera_mode == 4);
    guard = 0;
    while (aos_opening_actors_blocks_script(&actors) && guard++ < 400) {
        assert(aos_opening_actors_update(&actors, durations, &camera_x, 0) ==
               AOS_OPENING_CUE_NONE);
    }
    assert(actors.scene_step == 1 && !aos_opening_actors_blocks_script(&actors));

    /* sub_08067A04's end command finishes the script; the +0xA==0x80
     * tail's sub_0805F008 then rebinds the actors: Mina keeps
     * animation 5 through the +0x6D guard (no chain reset), Arikado
     * unconditionally returns to looping animation 1, and the scripted
     * skeleton is deleted. This is the pose the post-scene proximity
     * conversations (states 10/11) leave on screen. */
    uint16_t mina_age = actors.animation_age[0];
    assert(actors.animation[0] == 5 && actors.animation[1] == 3);
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_COMPLETE);
    assert(actors.animation[0] == 5 && actors.animation_age[0] == mina_age &&
           !actors.anim_loop[0]);
    assert(actors.animation[1] == 1 && actors.anim_loop[1] &&
           actors.animation_age[1] == 0);
    assert(!actors.visible[2] && actors.skeleton_state == 5);
    /* Every state-11 talk end reruns the same tail: Arikado's loop
     * rebinds again while Mina's guard still skips. */
    for (unsigned frame = 0; frame < 8; ++frame)
        aos_opening_actors_update(&actors, durations, &camera_x, 0);
    actors.animation_age[1] = 7;
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_COMPLETE);
    assert(actors.animation[1] == 1 && actors.animation_age[1] == 0);
    assert(AOS_SOMA_ANIM_OPENING_LYING == 0x51);
    assert(!strcmp(aos_opening_event_speaker_name(1), "Soma Cruz"));
    assert(!strcmp(aos_opening_event_speaker_name(2), "Mina Hakuba"));
    assert(!strcmp(aos_opening_event_speaker_name(7), "Genya Arikado"));
    assert(!strcmp(aos_opening_event_speaker_name(3), "Genya Arikado"));
    assert(!strcmp(aos_opening_event_speaker_name(0), ""));
    uint32_t events[8] = {0};
    assert(aos_opening_event_should_start(0, 0, 0, events, true));
    assert(!aos_opening_event_should_start(1, 0, 0, events, true));
    assert(!aos_opening_event_should_start(0, 1, 0, events, true));
    assert(!aos_opening_event_should_start(0, 0, 1, events, true));
    assert(!aos_opening_event_should_start(0, 0, 0, events, false));
    events[1] = 1;
    assert(!aos_opening_event_should_start(0, 0, 0, events, true));
}

static void test_intro_transition(void) {
    /* sub_0805CED0: once case 0's reveal pan reports done, case 1 runs
     * sub_0805CB74's six-stage MOSAIC transition once per frame; only
     * the frame after it returns 0 does sub_08067934 open the WIN0
     * window (+0xA==2). The stage timers make that exactly 110
     * transition frames plus the +0xC handoff frame. */
    AosOpeningActors actors;
    aos_opening_actors_init(&actors);
    int32_t camera_x = 0;
    unsigned frame = 0;
    AosOpeningCue reveal_cue = AOS_OPENING_CUE_NONE;
    while (!actors.reveal_done && frame < 64) {
        AosOpeningCue cue =
            aos_opening_actors_update(&actors, NULL, &camera_x, 0);
        if (cue != AOS_OPENING_CUE_NONE) reveal_cue = cue;
        ++frame;
    }
    /* Fresh spawn: camera word 0 remaps mode 5 to mode 0, which pans
     * right 8 pixels per frame until it passes 0x1F. The done frame
     * carries sub_0805CED0's case-0 -> case-1 sub_08013CF0(0x1C)
     * handoff as AOS_OPENING_CUE_REVEAL_PAN. */
    assert(actors.reveal_done && !actors.window_open && camera_x == 32);
    assert(reveal_cue == AOS_OPENING_CUE_REVEAL_PAN);
    for (unsigned i = 0; i < 110; ++i) {
        aos_opening_actors_update(&actors, NULL, &camera_x, 0);
        assert(!actors.window_open);
        if (i == 50) assert(actors.mosaic != 0);
    }
    assert(actors.transition_step == 6 && actors.mosaic == 0 &&
           actors.transition_wait_open);
    aos_opening_actors_update(&actors, NULL, &camera_x, 0);
    assert(actors.window_open);
    /* +0xA==2's first frame raises D410 +0xB==0's suspend flag with a
     * 0x40 reload that wraps to 0xFF on the 65th decrement; the script
     * stays blocked and the window chrome stays hidden until
     * sub_08067A04's state 2 can run. */
    assert(actors.text_hold == 0x42 && !actors.window_shown);
    unsigned held = 0;
    while (aos_opening_actors_blocks_script(&actors) && held < 80) {
        assert(!actors.window_shown);
        aos_opening_actors_update(&actors, NULL, &camera_x, 0);
        ++held;
    }
    /* +0xD=0x40 wraps to 0xFF on the 65th decrement and +0xC==2 clears
     * the suspend the next frame: 66 portable hold frames. */
    assert(held == 66 && actors.text_hold == 0 && actors.window_shown);
    assert(!aos_opening_actors_blocks_script(&actors));
    aos_opening_actors_update(&actors, NULL, &camera_x, 0);
    assert(actors.mosaic == 0 && actors.window_open);
}

static void test_script_and_commit(void) {
    static const uint8_t script[] = {
        0, 0, 3, 4, 7, 2, 'A', 'B', 6, 'C', 5,
        4, 3, 5, 7, 8, 'D', 2, 9, 5, 9, 'E', 10,
    };
    AosOpeningEvent event;
    assert(aos_opening_event_init(&event, script, sizeof script));
    aos_opening_event_update(&event, 0);
    assert(event.portrait == 4 && event.speaker == 0);
    assert(event.slide_dir == 1 && event.slide_pos == 0);
    for (unsigned frame = 0; frame < 12; ++frame)
        aos_opening_event_update(&event, 0);
    assert(event.slide_dir == 0 && event.slide_pos == 0x24);
    aos_opening_event_update(&event, 0);
    assert(event.speaker == 2);
    assert(event.glyph_counts[0] == 1 && event.glyphs[0][0] == 'A');
    for (unsigned frame = 0; frame < 16 && !event.waiting; ++frame)
        aos_opening_event_update(&event, 0);
    assert(event.waiting && event.line == 1);
    assert(event.glyph_counts[0] == 2 && event.glyph_counts[1] == 1);
    aos_opening_event_update(&event, AOS_KEY_A);
    /* Command 4 holds parsing through a twelve-frame retract and a
     * twelve-frame extension. Its page turn retains the current speaker. */
    assert(!event.waiting && event.slide_dir == -1 &&
           event.glyph_counts[0] == 0 && event.speaker == 2);
    for (unsigned frame = 0; frame < 12; ++frame)
        aos_opening_event_update(&event, 0);
    assert(event.slide_dir == 1 && event.slide_pos == 0);
    for (unsigned frame = 0; frame < 12; ++frame)
        aos_opening_event_update(&event, 0);
    assert(event.slide_dir == 0 && event.slide_pos == 0x24 &&
           event.speaker == 2);
    aos_opening_event_update(&event, 0);
    assert(event.portrait == 5 && event.slide_dir == 1 &&
           event.slide_pos == 0 && event.speaker == 0);
    for (unsigned frame = 0; frame < 12; ++frame)
        aos_opening_event_update(&event, 0);
    assert(event.slide_dir == 0 && event.slide_pos == 0x24 &&
           event.speaker == 0);
    for (unsigned frame = 0; frame < 16 && !event.waiting; ++frame)
        aos_opening_event_update(&event, 0);
    assert(event.waiting && event.beat == 9 && event.speaker == 8);
    assert(event.beat_count == 1 && event.cue == AOS_OPENING_CUE_ENTRY_PAN);
    assert(event.glyph_counts[0] == 1 && event.glyphs[0][0] == 'D');
    aos_opening_event_update(&event, AOS_KEY_A);
    /* Command 9 repeats the same 24-frame clear slide before 'E'. */
    for (unsigned frame = 0; frame < 48 && event.active; ++frame)
        aos_opening_event_update(&event, 0);
    assert(event.complete && !event.active && !event.malformed);
    assert(event.cue == AOS_OPENING_CUE_COMPLETE);
    assert(event.glyph_counts[0] == 1 && event.glyphs[0][0] == 'E');

    uint32_t events[8] = {0};
    AosStats stats = {0};
    assert(aos_opening_event_commit(&event, events, &stats));
    assert(events[1] == 1);
    assert(aos_souls_total(&stats, AOS_SOUL_RED, 0) == 1);
    assert(stats.red_soul == 1);
    assert(!aos_opening_event_should_start(0, 0, 0, events, true));
    assert(!aos_opening_event_commit(&event, events, &stats));
    assert(aos_souls_total(&stats, AOS_SOUL_RED, 0) == 1);
}

static void test_window_vertical(void) {
    uint8_t top = 0, bottom = 0;
    aos_opening_window_vertical(0, &top, &bottom);
    assert(top == 124 && bottom == 124);
    aos_opening_window_vertical(3, &top, &bottom);
    assert(top == 122 && bottom == 126);
    aos_opening_window_vertical(18, &top, &bottom);
    assert(top == 108 && bottom == 140);
    aos_opening_window_vertical(0x24, &top, &bottom);
    assert(top == 92 && bottom == 156);
    aos_opening_window_vertical(0xFF, &top, &bottom);
    assert(top == 92 && bottom == 156);
}

static bool resolve_insert(void *context, uint8_t id, const uint8_t **script,
                           size_t *script_size) {
    static const uint8_t insert[] = {0, 0, 'X', 10};
    (void)context;
    if (id != 3) return false;
    *script = insert;
    *script_size = sizeof insert;
    return true;
}

static void test_insert_and_malformed(void) {
    static const uint8_t insert_command[] = {0, 0, 1, 3, 10};
    AosOpeningEvent event;
    assert(aos_opening_event_init(&event, insert_command,
                                  sizeof insert_command));
    aos_opening_event_update(&event, 0);
    assert(event.malformed && !event.active && !event.complete);
    assert(aos_opening_event_init(&event, insert_command,
                                  sizeof insert_command));
    aos_opening_event_set_resolver(&event, resolve_insert, NULL);
    aos_opening_event_update(&event, AOS_KEY_A);
    assert(event.active && event.glyph_counts[0] == 1 &&
           event.glyphs[0][0] == 'X');
    aos_opening_event_update(&event, 0);
    assert(event.complete && event.glyph_counts[0] == 1 &&
           event.glyphs[0][0] == 'X');
    assert(!aos_opening_event_init(&event, insert_command, 2));
}

/* The host-side beat -> player +0x6D map (aos_opening_soma_anim):
 * the native sequence 0x51 -> 0x0D -> 0x00 -> 0x04 -> 0x05 -> 0x52 ->
 * post window, read back out of the serialized +0xB/+0xC/+0xD beat
 * state. States 6-8 hand the already-started post-absorb choreography to
 * the normal player animation path. */
static void test_soma_anim_map(void) {
    AosOpeningActors actors;
    aos_opening_actors_init(&actors);
    /* +0xB==0 reveal/MOSAIC keeps the unconscious floor pose. */
    assert(aos_opening_soma_anim(&actors) == 0x51);
    /* +0xB==1's entry pan plays LAND (0x0D); the released idle
     * sub-state and the +0xB==2,3,4 dialogue beats play idle. */
    actors.scene_state = 1;
    actors.beat1_step = 0;
    assert(aos_opening_soma_anim(&actors) == 0x0D);
    actors.beat1_step = 1;
    assert(aos_opening_soma_anim(&actors) == 0x00);
    actors.beat1_step = 2;
    assert(aos_opening_soma_anim(&actors) == 0x00);
    actors.scene_state = 2;
    assert(aos_opening_soma_anim(&actors) == 0x00);
    actors.scene_state = 4;
    assert(aos_opening_soma_anim(&actors) == 0x00);
    /* +0xB==5's absorb wind-up: sub-state 0 -> 0x04, sub-state 1 ->
     * 0x05, and the obtained-soul window (+0xC==2 tail / soul_notice)
     * binds 0x52 like sub_0805D410's sub_0801D028 calls. */
    actors.scene_state = 5;
    actors.scene_step = 0;
    assert(aos_opening_soma_anim(&actors) == 0x04);
    actors.scene_step = 1;
    assert(aos_opening_soma_anim(&actors) == 0x05);
    actors.scene_step = 2;
    assert(aos_opening_soma_anim(&actors) == 0x52);
    actors.scene_step = 1;
    actors.soul_notice = 1;
    assert(aos_opening_soma_anim(&actors) == 0x52);
    actors.soul_notice = 0;
    /* +0xB==6 holds the post-absorb crouch choreography. The following
     * dialogue beat requests the stand-up sequence, exactly where the native
     * +0x6D trace changes from 0x02 through 0x0A to 0x00. */
    actors.scene_state = 6;
    assert(aos_opening_soma_anim(&actors) == AOS_OPENING_SOMA_ANIM_POST);
    actors.scene_state = 7;
    assert(aos_opening_soma_anim(&actors) ==
           AOS_OPENING_SOMA_ANIM_RELEASE);
    actors.scene_state = 8;
    assert(aos_opening_soma_anim(&actors) == 0x00);
    assert(aos_opening_soma_anim(NULL) == 0x00);
}

static void test_opening_actor_room_lifetime(void) {
    assert(!aos_opening_actors_should_retire(false, 0, 3));
    assert(!aos_opening_actors_should_retire(true, 0, 0));
    assert(aos_opening_actors_should_retire(true, 0, 3));
    assert(aos_opening_actors_should_retire(true, 1, 0));
}

static void test_actor_anim_ticks(void) {
    AosOpeningActors actors;
    aos_opening_actors_init(&actors);
    /* Actor 0's spawn animation 4 is non-looping; bind a two-entry
     * flattened sequence {ticks 2,3} = 5 update frames to finish. */
    static const uint8_t seq_a[] = {2, 3};
    actors.anim_seqs[0][4].ticks = seq_a;
    actors.anim_seqs[0][4].count = 2;
    /* Actor 1's spawn animation 1 loops; entries {1,2} wrap every 3
     * frames and never latch the end bit. */
    static const uint8_t seq_b[] = {1, 2};
    actors.anim_seqs[1][1].ticks = seq_b;
    actors.anim_seqs[1][1].count = 2;

    for (unsigned frame = 0; frame < 4; ++frame)
        assert(aos_opening_actors_update(&actors, NULL, NULL, 0) ==
               AOS_OPENING_CUE_NONE);
    /* +0x6E sits on entry 1 with +0x6F=2 of 3; not finished yet. */
    assert(actors.anim_subframe[0] == 1 && actors.anim_tick[0] == 2 &&
           !actors.anim_finished[0] && !actors.anim_ended[0]);
    /* Actor 1 wrapped once at update 3 and re-entered entry 1 at
     * update 4: sub_0803F17C sets +0x59 bit 0 at every sequence end,
     * but +0x6C bit 2 stays clear for loops. */
    assert(actors.anim_subframe[1] == 1 && actors.anim_finished[1] &&
           !actors.anim_ended[1]);
    assert(aos_opening_actors_update(&actors, NULL, NULL, 0) ==
           AOS_OPENING_CUE_NONE);
    assert(actors.anim_finished[0] && actors.anim_ended[0] &&
           actors.anim_subframe[0] == 1);
    /* The +0x6C bit 2 early-out pins the last entry: later updates do
     * not move the tick chain. */
    for (unsigned frame = 0; frame < 4; ++frame)
        aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(actors.anim_subframe[0] == 1 && actors.anim_tick[0] == 0);

    /* actor_animation (sub_0803F2C8) resets the chain for the new
     * sequence: bind actor 2's beat animation 5, let the chain run,
     * then the ENEMY_ATTACKS cue re-arms it. */
    static const uint8_t seq_c[] = {4, 4};
    actors.anim_seqs[2][5].ticks = seq_c;
    actors.anim_seqs[2][5].count = 2;
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_ENEMY_ATTACKS);
    assert(actors.animation[2] == 5);
    for (unsigned frame = 0; frame < 3; ++frame)
        aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(actors.anim_tick[2] == 3 && actors.anim_subframe[2] == 0);
    /* The skeleton-state machine re-arms animation 5 on its own; drive
     * the chain again and force one more actor_animation through the
     * scene by re-triggering the same cue is not possible, so verify
     * the reset on actor 0 via ENTRY_PAN instead. */
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_ENTRY_PAN);
    assert(actors.animation[0] == 1);
    assert(actors.anim_subframe[0] == 0 && actors.anim_tick[0] == 0 &&
           !actors.anim_finished[0] && !actors.anim_ended[0]);

    /* No bound sequence -> the duration fallback still applies: actor 2
     * keeps only its age counter (its spawn animation loops anyway). */
    assert(!actors.anim_seqs[2][4].ticks &&
           actors.anim_seqs[2][4].count == 0);
}

static void test_emitted_anim_ticks(void) {
    AosOpeningActors actors;
    aos_opening_actors_init(&actors);
    aos_opening_actors_set_motion(&actors, zero_random, full_sine,
                                  zero_angle, NULL, NULL, NULL);
    /* Type-10 children tick against emitted_seqs (Skeleton Knight
     * art); type-8 children reuse actor slot 2's anim_seqs binding
     * (the shared opening_actor_8 export). */
    static const uint8_t knight_seq[] = {2, 3};
    actors.emitted_seqs[0].ticks = knight_seq;
    actors.emitted_seqs[0].count = 2;
    static const uint8_t winged_seq[] = {1, 2};
    actors.anim_seqs[2][4].ticks = winged_seq;
    actors.anim_seqs[2][4].count = 2;
    actors.emitted_count = 3;
    /* State 2 with no parameter bits keeps both state machines idle. */
    actors.emitted[0] = (AosOpeningEntity){
        .entity_type = 10, .animation = 0, .state = 2, .active = true};
    actors.emitted[1] = (AosOpeningEntity){
        .entity_type = 8, .animation = 4, .state = 2, .active = true};
    /* Animation 5 has no bound sequence: only the age counter moves. */
    actors.emitted[2] = (AosOpeningEntity){
        .entity_type = 10, .animation = 5, .state = 2, .active = true};
    AosOpeningEntity *knight = &actors.emitted[0];
    AosOpeningEntity *winged = &actors.emitted[1];
    AosOpeningEntity *unbound = &actors.emitted[2];

    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(knight->anim_tick == 1 && knight->anim_subframe == 0);
    /* {1,2}: entry 0's single tick advances to entry 1 immediately. */
    assert(winged->anim_tick == 0 && winged->anim_subframe == 1);
    assert(unbound->anim_subframe == 0 && unbound->anim_tick == 0 &&
           unbound->animation_age == 1);
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(knight->anim_tick == 0 && knight->anim_subframe == 1);
    assert(winged->anim_tick == 1 && winged->anim_subframe == 1);
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(knight->anim_tick == 1 && knight->anim_subframe == 1);
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(knight->anim_tick == 2 && knight->anim_subframe == 1);
    /* {2,3} wraps after five update frames: entry 1's third tick
     * returns to entry 0. */
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(knight->anim_tick == 0 && knight->anim_subframe == 0);
    /* {1,2} wrapped at update 3 already: two more frames put it back
     * on entry 1's first tick. */
    assert(winged->anim_tick == 1 && winged->anim_subframe == 1);
    assert(unbound->animation_age == 5 &&
           unbound->anim_subframe == 0 && unbound->anim_tick == 0);

    /* Parameter bit 4's state-2 tail switches animation 0 -> 2 through
     * the same reset sub_0803F2C8 performs: age, +0x6E and +0x6F all
     * restart on the frame of the switch. */
    static const uint8_t run_seq[] = {4, 4};
    actors.emitted_seqs[2].ticks = run_seq;
    actors.emitted_seqs[2].count = 2;
    knight->parameter = 0x10;
    knight->animation_age = 100;
    knight->anim_subframe = 1;
    knight->anim_tick = 2;
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(knight->animation == 2 && knight->animation_age == 0 &&
           knight->anim_subframe == 0 && knight->anim_tick == 0);
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(knight->anim_tick == 1 && knight->anim_subframe == 0);
}

static void test_opening_sparks(void) {
    /* sub_0805EC20/sub_0805EE88's type-8 sub_08045B44 sparkles: the
     * soul orb emits one every fourth +0xD (read before the tail
     * decrement), the projectile one every second; each sparkle
     * spawns at the emitter's position plus two
     * sub_0802D5EC(0x200000) offsets with velocity -v/4 and
     * acceleration v/64, then lives the 0x08525194 {2,4,4,2} sequence. */
    AosOpeningActors actors;
    aos_opening_actors_init(&actors);
    /* sine/angle stay NULL so the orb takes the fallback lerp and the
     * manually stored velocity survives to the sparkle terms. */
    aos_opening_actors_set_motion(&actors, zero_random, NULL,
                                  NULL, NULL, NULL, NULL);
    /* Drive the soul orb directly: the portable fields mirror the
     * native +0xA/+0xD/+0x40/+0x44/+0x48/+0x4C the routines write. */
    actors.soul_active = true;
    actors.soul_state = 0;
    actors.soul_timer = 0xC8;
    actors.soul_x = 0x100 << 16;
    actors.soul_y = 0x200 << 16;
    actors.soul_vx = 0x40000;
    actors.soul_vy = -0x20000;
    /* First update: +0xD=0xC8 is a multiple of four, so one sparkle
     * appends at the post-lerp position; it gets its first
     * sub_080450C8 pass on the next frame. */
    int32_t spawn_x = actors.soul_x +
        ((AOS_OPENING_SOMA_X << 16) - actors.soul_x) / 8;
    int32_t spawn_y = actors.soul_y +
        (((AOS_OPENING_SOMA_Y - 16) << 16) - actors.soul_y) / 8;
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(actors.spark_count == 1);
    const AosOpeningSpark *spark = &actors.sparks[0];
    /* zero_random's sub_0802D5EC(0x200000) draw yields -0x100000. */
    assert(spark->x == spawn_x - 0x100000 &&
           spark->y == spawn_y - 0x100000);
    assert(spark->vx == -0x40000 / 4 && spark->vy == -(-0x20000) / 4 &&
           spark->ax == 0x40000 / 64 && spark->ay == -0x20000 / 64);
    assert(spark->kind == AOS_EFFECT_CONTAINER_SPARK0);
    assert(!spark->started);
    /* The next three +0xD values do not fire; +0xD=196 does — every
     * fourth countdown frame. */
    for (unsigned frame = 0; frame < 3; ++frame)
        aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(actors.spark_count == 1);
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(actors.spark_count == 2);
    /* The {2,4,4,2} sequence is 12 spark updates long; the first
     * sparkle has had 15 by now and is gone (its slot may already
     * have been recycled by a later emission). */
    for (unsigned frame = 0; frame < 11; ++frame)
        aos_opening_actors_update(&actors, NULL, NULL, 0);
    unsigned live = 0;
    for (unsigned i = 0; i < actors.spark_count; ++i)
        live += actors.sparks[i].active != 0;
    /* Four orb emissions so far (gates at +0xD 200/196/192/188); the
     * first one's 12-update sequence ended on update 13, leaving
     * exactly three live sparkles. */
    assert(live == 3);
    /* The orb's own +0xA/+0xD two-phase lifetime still totals 0x108
     * frames; 16 updates in it sits in state 0's countdown. */
    assert(actors.soul_active && actors.soul_state == 0 &&
           actors.soul_timer == 0xC8 - 16);
    actors.soul_active = false;

    /* The projectile's gate is +0xD&1: even countdown values only,
     * bank-6 spark kind, same velocity terms. */
    actors.projectile_count = 1;
    actors.projectiles[0] = (AosOpeningProjectile){
        .x = 0x80 << 16, .y = 0x90 << 16, .vx = 0x8000, .vy = 0,
        .timer = 0xC8, .state = 1, .active = true};
    unsigned bank6_before = 0;
    for (unsigned i = 0; i < actors.spark_count; ++i)
        bank6_before += actors.sparks[i].active &&
                        actors.sparks[i].kind == AOS_EFFECT_CONTAINER_SPARK3;
    for (unsigned frame = 0; frame < 4; ++frame)
        aos_opening_actors_update(&actors, NULL, NULL, 0);
    /* Gate values read: 0xC8, 0xC7, 0xC6, 0xC5 -> two even hits
     * (0xC8, 0xC6). */
    unsigned bank6 = 0;
    bool saw_terms = false;
    for (unsigned i = 0; i < actors.spark_count; ++i) {
        const AosOpeningSpark *spark = &actors.sparks[i];
        if (!spark->active || spark->kind != AOS_EFFECT_CONTAINER_SPARK3)
            continue;
        ++bank6;
        if (spark->vx == -0x8000 / 4 && spark->ax == 0x8000 / 64)
            saw_terms = true;
    }
    assert(bank6 == bank6_before + 2 && saw_terms);
}

static bool resolve_rom_text(void *context, uint8_t id,
                             const uint8_t **script, size_t *script_size) {
    return aos_intro_resources_entry(context, 1, id, script, script_size);
}

/* sub_0805CB74's MOSAIC register values, verified frame by frame
 * against asm/code/code_08050A3C.s: case 0 writes 0xFFFF with a 0x40
 * reload, cases 1-4 write (timer>>2)*0x1111 truncated to 16 bits while
 * the timer walks 0x40->0x20->0x30->0x10->0x20, and case 5 counts down
 * by two to the 0xFF wrap that clears the register. */
static void test_mosaic_register_values(void) {
    AosOpeningActors actors;
    aos_opening_actors_init(&actors);
    int32_t camera_x = 0;
    while (!actors.reveal_done)
        aos_opening_actors_update(&actors, NULL, &camera_x, 0);
    uint16_t sequence[128];
    unsigned count = 0;
    while (actors.transition_step < 6 && count < 128) {
        aos_opening_actors_update(&actors, NULL, &camera_x, 0);
        if (count < sizeof sequence / sizeof sequence[0])
            sequence[count] = actors.mosaic;
        ++count;
    }
    /* transition_started consumes the first post-reveal frame. */
    assert(count >= 105 && count <= 115);
    /* Case 0 writes 0xFFFF; case 1's first timer 0x40 gives level 16
     * -> 0x1110 and the next timer 0x3F gives level 15 -> 0xFFFF
     * again. The transition_started latch may consume one leading
     * frame depending on which update raised reveal_done, so match
     * the triple anywhere in the recorded run. */
    bool saw_full_pixel = false;
    for (unsigned i = 0; i + 2 < count; ++i)
        if (sequence[i] == 0xFFFF && sequence[i + 1] == 0x1110 &&
            sequence[i + 2] == 0xFFFF)
            saw_full_pixel = true;
    assert(saw_full_pixel);
    /* Level 14 (timer 0x3C/0x3D) writes 0xEEEE. */
    bool saw_eeee = false, saw_8888 = false, saw_cccc = false;
    for (unsigned i = 0; i < count; ++i) {
        if (sequence[i] == 0xEEEE) saw_eeee = true;
        if (sequence[i] == 0x8888) saw_8888 = true;
        if (sequence[i] == 0xCCCC) saw_cccc = true;
    }
    assert(saw_eeee && saw_8888 && saw_cccc);
    /* The transition ends with the register cleared. */
    assert(actors.mosaic == 0);
}

int main(int argc, char **argv) {
    assert(AOS_OPENING_TEXT_PANEL_X * 2 +
               AOS_OPENING_TEXT_PANEL_WIDTH == 240);
    assert(AOS_OPENING_TEXT_PANEL_X == 8 &&
           AOS_OPENING_TEXT_PANEL_X + AOS_OPENING_TEXT_PANEL_WIDTH == 232);
    assert(AOS_OPENING_TEXT_PANEL_Y == 92 &&
           AOS_OPENING_TEXT_PANEL_Y + AOS_OPENING_TEXT_PANEL_HEIGHT == 156);
    assert(AOS_OPENING_SPEAKER_X == 40 && AOS_OPENING_SPEAKER_Y == 100);
    assert(AOS_OPENING_TEXT_X == 40 && AOS_OPENING_TEXT_Y == 116);
    assert(AOS_OPENING_PROMPT_X == 216 && AOS_OPENING_PROMPT_Y == 140);
    test_emitted_motion();
    test_emitted_floor_snap();
    test_skeleton_collision_flight();
    test_skeleton_approach();
    test_emitted_attack_tails();
    test_actor_anim_ticks();
    test_emitted_anim_ticks();
    test_opening_sparks();
    test_soma_anim_map();
    test_opening_actor_room_lifetime();
    test_trigger();
    test_intro_transition();
    test_mosaic_register_values();
    test_window_vertical();
    test_script_and_commit();
    test_insert_and_malformed();
    if (argc == 2) {
        FusionRom rom = {0};
        AosIntroResources resources;
        const uint8_t *script = NULL;
        size_t script_size = 0;
        char error[128];
        assert(fusion_rom_open(FUSION_ASSET_GAME_ARIA, argv[1], &rom,
                               error, sizeof error));
        assert(aos_intro_resources_init(&resources, &rom, error, sizeof error));
        assert(aos_intro_resources_entry(&resources, 1, AOS_OPENING_TEXT_ID,
                                         &script, &script_size));
        AosOpeningEvent event;
        assert(aos_opening_event_init(&event, script, script_size));
        aos_opening_event_set_resolver(&event, resolve_rom_text, &resources);
        bool cues[AOS_OPENING_CUE_COMPLETE + 1] = {false};
        bool confrontation_visible = false;
        for (unsigned frame = 0; frame < 10000 && event.active; ++frame) {
            aos_opening_event_update(&event, AOS_KEY_A);
            if (event.cue == AOS_OPENING_CUE_CONFRONTATION)
                confrontation_visible = event.slide_pos == 0x24 &&
                    event.slide_dir == 0 &&
                    (event.speaker == 3 || event.speaker == 7) &&
                    (event.glyph_counts[0] != 0 ||
                     event.glyph_counts[1] != 0);
            cues[event.cue] = true;
        }
        assert(event.complete && !event.malformed);
        assert(event.beat_count == 7);
        assert(cues[AOS_OPENING_CUE_CONFRONTATION]);
        assert(confrontation_visible);
        assert(cues[AOS_OPENING_CUE_ENEMY_APPEARS]);
        assert(cues[AOS_OPENING_CUE_ENEMY_ATTACKS]);
        assert(cues[AOS_OPENING_CUE_SOUL_ABSORBED]);
        assert(cues[AOS_OPENING_CUE_AFTER_BATTLE]);
        assert(cues[AOS_OPENING_CUE_RETURN_PAN]);
        assert(cues[AOS_OPENING_CUE_COMPLETE]);
        fusion_rom_close(&rom);
    }
    return 0;
}
