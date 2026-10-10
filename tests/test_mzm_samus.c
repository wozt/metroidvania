/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free regression tests for the native Zero Mission pose controller. */
#include "mzm_projectiles.h"
#include "mzm_samus.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define GRID_W 20
#define GRID_H 15

typedef struct { bool solid[GRID_H][GRID_W]; } Grid;

static bool grid_blocked(void *context, float x, float y, float w, float h) {
    const Grid *g = context;
    if (x < 0.f || y < 0.f || x + w > GRID_W * 16.f || y + h > GRID_H * 16.f)
        return true;
    for (int row = 0; row < GRID_H; ++row)
        for (int col = 0; col < GRID_W; ++col)
            if (g->solid[row][col] && x < (float)(col * 16 + 16) &&
                x + w > (float)(col * 16) && y < (float)(row * 16 + 16) &&
                y + h > (float)(row * 16)) return true;
    return false;
}

static void fill(Grid *g, int col, int row, int w, int h) {
    for (int r = row; r < row + h; ++r)
        for (int c = col; c < col + w; ++c) g->solid[r][c] = true;
}

/* Two-tick frames by default; MidAir and the ledge pulls use the frame
 * counts and durations of the native Power Suit sequences. */
static int test_durations(void *context, const MzmSamus *samus,
                          uint8_t *durations, int max) {
    (void)context;
    int count = 2, ticks = 2;
    if (samus->pose == MZM_POSE_MIDAIR) count = 5;
    if (samus->pose == MZM_POSE_PULLING_UP) { count = 3; ticks = 3; }
    if (samus->pose == MZM_POSE_PULLING_FORWARD) { count = 4; ticks = 3; }
    if (count > max) count = max;
    for (int i = 0; i < count; ++i) durations[i] = (uint8_t)ticks;
    return count;
}

typedef struct {
    Grid grid;
    MzmCollision collision;
    MzmAnimationSource animation;
    MzmEquipment equipment;
    MzmSamus samus;
    uint16_t previous;
} World;

static void world_init(World *w) {
    memset(w, 0, sizeof *w);
    w->collision = (MzmCollision){.context = &w->grid, .blocked = grid_blocked};
    w->animation = (MzmAnimationSource){NULL, test_durations};
    w->equipment = (MzmEquipment){.suit = MZM_SUIT_NORMAL,
        .items = MZM_ITEM_MORPH_BALL | MZM_ITEM_POWER_GRIP,
        .energy = 99, .max_energy = 99};
    /* Floor along the bottom row. */
    fill(&w->grid, 0, GRID_H - 1, GRID_W, 1);
}

static void place(World *w, int pixel_x, int feet_y) {
    mzm_samus_init(&w->samus, pixel_x * 4, feet_y * 4, 1);
}

static void step(World *w, uint16_t held) {
    MzmInput input = {held, (uint16_t)(held & ~w->previous), false};
    w->previous = held;
    mzm_samus_update(&w->samus, &input, &w->equipment, &w->collision,
                     &w->animation);
}

static void steps(World *w, uint16_t held, int count) {
    for (int i = 0; i < count; ++i) step(w, held);
}

static bool box_clear(World *w) {
    float x, y, bw, bh;
    mzm_samus_box(&w->samus, &x, &y, &bw, &bh);
    return !grid_blocked(&w->grid, x, y, bw, bh);
}

static void test_native_tables(void) {
    const MzmBlockHitbox *standing = mzm_block_hitbox(MZM_HITBOX_STANDING);
    assert(standing->left == -28 && standing->right == 28 && standing->top == -124);
    assert(mzm_block_hitbox(MZM_HITBOX_CROUCHED)->top == -92);
    assert(mzm_block_hitbox(MZM_HITBOX_MORPHED)->top == -60);
    assert(mzm_pose_hitbox(MZM_POSE_SPINNING) == MZM_HITBOX_CROUCHED);
    assert(mzm_pose_hitbox(MZM_POSE_SCREW_ATTACKING) == MZM_HITBOX_CROUCHED);
    assert(mzm_pose_hitbox(MZM_POSE_MIDAIR) == MZM_HITBOX_STANDING);
    assert(mzm_pose_hitbox(MZM_POSE_ROLLING) == MZM_HITBOX_MORPHED);
    assert(mzm_pose_standing(MZM_POSE_HANGING_ON_LEDGE) == MZM_STANDING_HANGING);

    assert(mzm_apply_x_acceleration(90, 1, 8, 96) == 96);
    assert(mzm_apply_x_acceleration(-90, -1, 8, 96) == -96);
    assert(mzm_apply_x_acceleration(0, -1, 8, 64) == -8);

    int16_t velocity = 192;
    assert(mzm_integrate_y(&velocity) == -24 && velocity == 182);
    velocity = 232;
    assert(mzm_integrate_y(&velocity) == -24 && velocity == 222);
    velocity = -200;
    assert(mzm_integrate_y(&velocity) == 16 && velocity == -210);
    velocity = -235;
    assert(mzm_integrate_y(&velocity) == 16 && velocity == -235);

    MzmEquipment e = {.suit = MZM_SUIT_NORMAL, .energy = 99, .max_energy = 99};
    assert(mzm_jump_velocity(&e) == 192);
    e.items = MZM_ITEM_HIGH_JUMP;
    assert(mzm_jump_velocity(&e) == 232);
    e.suit = MZM_SUIT_SUITLESS;
    assert(mzm_jump_velocity(&e) == 212);
}

static void test_damage(void) {
    MzmEquipment e = {.suit = MZM_SUIT_NORMAL, .items = MZM_ITEM_VARIA_SUIT,
                      .energy = 99, .max_energy = 99};
    assert(mzm_equipment_damage(&e, 20) && e.energy == 83);
    e.items = MZM_ITEM_GRAVITY_SUIT;
    assert(mzm_equipment_damage(&e, 20) && e.energy == 69);
    e.items = MZM_ITEM_VARIA_SUIT | MZM_ITEM_GRAVITY_SUIT;
    assert(mzm_equipment_damage(&e, 20) && e.energy == 59);
    assert(mzm_equipment_damage(&e, 1) && e.energy == 58);
    e.items = 0;
    e.energy = 20;
    assert(!mzm_equipment_damage(&e, 20) && e.energy == 0);
}

static void test_running_and_jumping(void) {
    World w;
    world_init(&w);
    place(&w, 64, 224);
    step(&w, 0);
    assert(w.samus.pose == MZM_POSE_STANDING);
    step(&w, MZM_KEY_RIGHT);
    assert(w.samus.pose == MZM_POSE_RUNNING && w.samus.x_velocity == 0);
    steps(&w, MZM_KEY_RIGHT, 12);
    assert(w.samus.x_velocity == MZM_X_VELOCITY_CAP);
    int32_t x = w.samus.x;
    step(&w, MZM_KEY_RIGHT);
    assert(w.samus.x - x == MZM_X_VELOCITY_CAP >> 3);
    /* Releasing the direction stops immediately: no invented skid. */
    step(&w, 0);
    assert(w.samus.pose == MZM_POSE_STANDING && w.samus.x_velocity == 0);

    /* A standing jump is a straight MidAir jump with the low velocity. */
    step(&w, MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_MIDAIR);
    int32_t ground = 224 * 4, apex = w.samus.y;
    while (w.samus.pose == MZM_POSE_MIDAIR) {
        if (w.samus.y < apex) apex = w.samus.y;
        step(&w, MZM_KEY_A);
    }
    /* Sum of (v >> 3) for v = 192, 182, ... > 7 under native gravity. */
    int expected = 0;
    for (int v = 192; v > 0; v -= 10) expected += v >> 3;
    assert(ground - apex == expected);
    assert(w.samus.pose == MZM_POSE_LANDING && w.samus.y == ground);
    steps(&w, 0, 4);
    assert(w.samus.pose == MZM_POSE_STANDING);

    /* Releasing A while rising cuts the upward velocity. */
    step(&w, MZM_KEY_A);
    step(&w, MZM_KEY_A);
    assert(w.samus.y_velocity > 0);
    step(&w, 0);
    assert(w.samus.y_velocity <= 0);
    steps(&w, 0, 30);
    assert(w.samus.pose == MZM_POSE_LANDING || w.samus.pose == MZM_POSE_STANDING);
}

static void test_spin_jump(void) {
    World w;
    world_init(&w);
    place(&w, 40, 224);
    step(&w, 0);
    step(&w, MZM_KEY_RIGHT);
    step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_STARTING_SPIN_JUMP);
    assert(w.samus.y_velocity < MZM_LOW_JUMP_VELOCITY &&
           w.samus.y_velocity >= MZM_LOW_JUMP_VELOCITY - 10);
    steps(&w, MZM_KEY_RIGHT | MZM_KEY_A, 4);
    assert(w.samus.pose == MZM_POSE_SPINNING);
    /* A spin keeps moving forward without any direction held. */
    int16_t speed = w.samus.x_velocity;
    step(&w, MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_SPINNING && w.samus.x_velocity > speed);
    /* Holding up without a direction breaks the spin and cancels X. */
    step(&w, MZM_KEY_UP | MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_MIDAIR && w.samus.x_velocity == 0);
    /* A midair press starts spinning again without extra height. */
    int16_t rising = w.samus.y_velocity;
    step(&w, 0);
    step(&w, MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_SPINNING);
    assert(w.samus.y_velocity < rising);
    for (int i = 0; i < 80 && w.samus.pose == MZM_POSE_SPINNING; ++i)
        step(&w, MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_STANDING && w.samus.y == 224 * 4);

    /* Equipment upgrades the spin; Space Jump renews only while falling. */
    w.equipment.items |= MZM_ITEM_SPACE_JUMP;
    step(&w, 0);
    step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    steps(&w, MZM_KEY_RIGHT | MZM_KEY_A, 5);
    assert(w.samus.pose == MZM_POSE_SPACE_JUMPING);
    step(&w, MZM_KEY_RIGHT);
    step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    assert(w.samus.y_velocity < MZM_LOW_JUMP_VELOCITY - 10);
    while (w.samus.y_velocity > -8 * MZM_EIGHTH_BLOCK_SIZE)
        step(&w, MZM_KEY_RIGHT);
    step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    assert(w.samus.y_velocity == MZM_LOW_JUMP_VELOCITY - 10);
    w.equipment.items |= MZM_ITEM_SCREW_ATTACK;
    step(&w, MZM_KEY_RIGHT);
    assert(w.samus.pose == MZM_POSE_SCREW_ATTACKING);
}

static void test_wall_jump(void) {
    World w;
    world_init(&w);
    fill(&w.grid, 10, 0, 1, GRID_H);
    place(&w, 140, 224);
    step(&w, 0);
    step(&w, MZM_KEY_RIGHT);
    step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    int frames = 0;
    while (!(w.samus.pose == MZM_POSE_SPINNING && w.samus.walljump_timer) &&
           frames++ < 60) step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    assert(w.samus.walljump_timer == MZM_WALL_JUMP_TICKS);
    assert(w.samus.last_wall == -1 && box_clear(&w));
    /* Turning away first, then A, starts the native wall jump. */
    step(&w, MZM_KEY_LEFT | MZM_KEY_A);
    assert(w.samus.facing == -1);
    step(&w, MZM_KEY_LEFT);
    step(&w, MZM_KEY_LEFT | MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_STARTING_WALL_JUMP);
    int32_t y = w.samus.y;
    step(&w, MZM_KEY_LEFT | MZM_KEY_A);
    assert(w.samus.y == y);
    steps(&w, MZM_KEY_LEFT | MZM_KEY_A, 4);
    assert(w.samus.pose == MZM_POSE_SPINNING);
    assert(w.samus.y_velocity > MZM_LOW_JUMP_VELOCITY - 40);
    steps(&w, MZM_KEY_LEFT | MZM_KEY_A, 6);
    assert(w.samus.x_velocity < 0 && w.samus.y < y);

    /* Without a wall the same input never starts a wall jump. */
    world_init(&w);
    place(&w, 140, 224);
    step(&w, 0);
    step(&w, MZM_KEY_RIGHT);
    step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    steps(&w, MZM_KEY_RIGHT | MZM_KEY_A, 10);
    step(&w, MZM_KEY_LEFT | MZM_KEY_A);
    step(&w, MZM_KEY_LEFT);
    step(&w, MZM_KEY_LEFT | MZM_KEY_A);
    assert(w.samus.pose != MZM_POSE_STARTING_WALL_JUMP);
}

static void test_crouch_and_morph(void) {
    World w;
    world_init(&w);
    place(&w, 64, 224);
    step(&w, 0);
    step(&w, MZM_KEY_DOWN);
    assert(w.samus.pose == MZM_POSE_CROUCHING);
    step(&w, 0);
    step(&w, MZM_KEY_DOWN);
    assert(w.samus.pose == MZM_POSE_MORPHING);
    steps(&w, 0, 5);
    assert(w.samus.pose == MZM_POSE_MORPH_BALL && w.samus.y == 224 * 4);
    step(&w, MZM_KEY_UP);
    assert(w.samus.pose == MZM_POSE_UNMORPHING);
    steps(&w, 0, 5);
    assert(w.samus.pose == MZM_POSE_CROUCHING);
    step(&w, MZM_KEY_UP);
    assert(w.samus.pose == MZM_POSE_STANDING);

    /* Inside a one-block tunnel Up cannot unmorph and rolling stops at the
     * wall without entering it. */
    world_init(&w);
    fill(&w.grid, 3, GRID_H - 2 - 1, 8, 1);
    fill(&w.grid, 11, GRID_H - 2, 1, 1);
    place(&w, 24, 224);
    step(&w, 0);
    step(&w, MZM_KEY_DOWN);
    step(&w, 0);
    step(&w, MZM_KEY_DOWN);
    steps(&w, 0, 5);
    assert(w.samus.pose == MZM_POSE_MORPH_BALL);
    for (int i = 0; i < 200; ++i) {
        step(&w, MZM_KEY_RIGHT);
        assert(box_clear(&w));
    }
    assert(w.samus.pose == MZM_POSE_ROLLING || w.samus.pose == MZM_POSE_MORPH_BALL);
    assert(w.samus.x + 28 <= 11 * 16 * 4);
    step(&w, 0);
    step(&w, MZM_KEY_UP);
    assert(w.samus.pose == MZM_POSE_MORPH_BALL);
    step(&w, 0);
    step(&w, MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_MORPH_BALL);
}

static void test_ledge(void) {
    World w;
    world_init(&w);
    /* A platform whose top is at y=160 starting at x=160. */
    fill(&w.grid, 10, 10, 10, GRID_H - 10);
    place(&w, 150, 224);
    step(&w, 0);
    step(&w, MZM_KEY_RIGHT);
    step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    int frames = 0;
    while (w.samus.pose != MZM_POSE_HANGING_ON_LEDGE && frames++ < 120)
        step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_HANGING_ON_LEDGE);
    /* Native alignment: feet 34 pixels below the ledge top. */
    assert(w.samus.y == 160 * 4 + 2 * MZM_BLOCK_SIZE + MZM_EIGHTH_BLOCK_SIZE + 1);
    assert(box_clear(&w));
    steps(&w, 0, 5);
    assert(w.samus.pose == MZM_POSE_HANGING_ON_LEDGE);
    step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_PULLING_UP);
    steps(&w, MZM_KEY_RIGHT, 9);
    assert(w.samus.pose == MZM_POSE_PULLING_FORWARD);
    assert(w.samus.y == 160 * 4);
    int32_t hang_x = w.samus.x;
    steps(&w, 0, 12);
    /* One pixel forward per frame for the 12-tick native pull. */
    assert(w.samus.pose == MZM_POSE_STANDING && w.samus.y == 160 * 4);
    assert(w.samus.x == hang_x + 12 * MZM_PIXEL_SIZE);
    assert(box_clear(&w) && w.samus.x >= 160 * 4);

    /* Without Power Grip Samus lands below instead of hanging. */
    world_init(&w);
    w.equipment.items &= ~(uint32_t)MZM_ITEM_POWER_GRIP;
    fill(&w.grid, 10, 10, 10, GRID_H - 10);
    place(&w, 150, 224);
    step(&w, 0);
    step(&w, MZM_KEY_RIGHT);
    step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    for (int i = 0; i < 120; ++i) {
        step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
        assert(w.samus.pose != MZM_POSE_HANGING_ON_LEDGE);
    }
}

static void test_hurt_and_death(void) {
    World w;
    world_init(&w);
    place(&w, 64, 224);
    step(&w, 0);
    mzm_samus_hurt(&w.samus, &w.collision, false);
    assert(w.samus.pose == MZM_POSE_GETTING_HURT);
    assert(w.samus.y_velocity == 112 && w.samus.invincibility == 48);
    assert(w.samus.x_velocity == 0);
    steps(&w, 0, 40);
    assert(w.samus.pose != MZM_POSE_GETTING_HURT);
    assert(w.samus.invincibility < 48);
    step(&w, MZM_KEY_A);
    steps(&w, MZM_KEY_A, 3);
    assert(mzm_pose_standing(w.samus.pose) == MZM_STANDING_MIDAIR);
    mzm_samus_hurt(&w.samus, &w.collision, false);
    assert(w.samus.y_velocity == 56);
    mzm_samus_hurt(&w.samus, &w.collision, true);
    assert(w.samus.pose == MZM_POSE_DYING);
    int32_t x = w.samus.x, y = w.samus.y;
    steps(&w, MZM_KEY_RIGHT | MZM_KEY_A, 20);
    assert(w.samus.pose == MZM_POSE_DYING && w.samus.x == x && w.samus.y == y);
}

/* Random input over mixed geometry must never leave the hitbox embedded
 * in collision, whatever pose or hitbox size the controller selects. */
static void test_random_input_never_embeds(void) {
    World w;
    world_init(&w);
    w.equipment.items |= MZM_ITEM_HIGH_JUMP;
    fill(&w.grid, 0, 0, 1, GRID_H);
    fill(&w.grid, GRID_W - 1, 0, 1, GRID_H);
    fill(&w.grid, 0, 0, GRID_W, 1);
    fill(&w.grid, 4, 10, 5, 1);
    fill(&w.grid, 12, 8, 4, 6);
    fill(&w.grid, 3, 12, 6, 1);
    fill(&w.grid, 9, 13, 1, 1);
    place(&w, 40, 224);
    uint32_t seed = 12345u;
    uint16_t held = 0;
    static const uint32_t item_sets[] = {
        0, MZM_ITEM_SPACE_JUMP, MZM_ITEM_SCREW_ATTACK,
        MZM_ITEM_SPACE_JUMP | MZM_ITEM_SCREW_ATTACK
    };
    for (int frame = 0; frame < 40000; ++frame) {
        seed = seed * 1103515245u + 12345u;
        if ((seed >> 16) % 6 == 0) held = (uint16_t)((seed >> 8) & 0x7f);
        if (frame % 5000 == 0) {
            w.equipment.items = MZM_ITEM_MORPH_BALL | MZM_ITEM_POWER_GRIP |
                MZM_ITEM_HIGH_JUMP | item_sets[(frame / 5000) % 4];
        }
        step(&w, held);
        assert(box_clear(&w));
        assert(w.samus.pose < MZM_POSE_COUNT);
    }
}

static int two_frames(void *context, const MzmProjectile *projectile,
                      uint8_t *durations, int max) {
    (void)context;
    (void)projectile;
    (void)max;
    durations[0] = 3;
    durations[1] = 3;
    return 2;
}

static const MzmProjectile *first_active(const MzmWeapons *weapons) {
    for (int i = 0; i < MZM_MAX_PROJECTILES; ++i)
        if (weapons->list[i].active) return &weapons->list[i];
    return NULL;
}

static void test_weapon_selection(void) {
    World w;
    world_init(&w);
    place(&w, 64, 224);
    step(&w, 0);
    MzmWeapons weapons;
    mzm_weapons_init(&weapons);
    w.equipment.missiles = 1;
    w.equipment.super_missiles = 1;
    mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R, 0, &w.equipment);
    assert(weapons.highlighted == MZM_WEAPON_MISSILE);
    mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R, MZM_KEY_SELECT,
                            &w.equipment);
    assert(weapons.highlighted == MZM_WEAPON_SUPER_MISSILE);
    w.equipment.super_missiles = 0;
    mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R, 0, &w.equipment);
    assert(weapons.highlighted == MZM_WEAPON_MISSILE);
    w.equipment.missiles = 0;
    mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R, 0, &w.equipment);
    assert(weapons.highlighted == MZM_WEAPON_NONE);
    /* No weapon can be armed or fired in Morph Ball. */
    w.samus.pose = MZM_POSE_MORPH_BALL;
    w.equipment.missiles = 3;
    assert(!mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R | MZM_KEY_B,
                                    MZM_KEY_B, &w.equipment));
    assert(weapons.highlighted == MZM_WEAPON_NONE);
}

static void test_beam(void) {
    World w;
    world_init(&w);
    fill(&w.grid, 15, 0, 1, GRID_H);
    place(&w, 64, 224);
    step(&w, 0);
    MzmWeapons weapons;
    mzm_weapons_init(&weapons);
    MzmProjectileAnimation animation = {NULL, two_frames};
    assert(mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, MZM_KEY_B,
                                   &w.equipment));
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 18, -26, &w.collision,
                       &animation);
    const MzmProjectile *beam = first_active(&weapons);
    assert(beam && beam->type == MZM_PROJECTILE_BEAM && beam->x_flip);
    /* gArmCannonX/Y from Samus's native pixel position. */
    assert(beam->x == (64 + 18) * 4 && beam->y == (223 - 26) * 4);
    assert(weapons.cooldown == MZM_BEAM_COOLDOWN);
    assert(!mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, MZM_KEY_B,
                                    &w.equipment));
    int32_t x = beam->x;
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, &animation);
    assert(beam->x == x + 16);
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, &animation);
    assert(beam->x == x + 36 && beam->anim_frame == 0);
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, &animation);
    assert(beam->anim_frame == 1);
    int updates = 4;
    while (beam->active) {
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision,
                           &animation);
        ++updates;
    }
    /* PROJ_SHORT_BEAM_LIFETIME: removed on the thirteenth update. */
    assert(updates == 13);

    /* A wall in the path removes the shot when its point enters solid,
     * well before its lifetime or the distance despawn. */
    mzm_weapons_init(&weapons);
    w.samus.x = 200 * 4;
    assert(mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, MZM_KEY_B,
                                   &w.equipment));
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 18, -26, &w.collision,
                       &animation);
    beam = first_active(&weapons);
    int32_t last_x = beam->x;
    int lifetime = 1;
    while (beam->active) {
        last_x = beam->x;
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision,
                           &animation);
        ++lifetime;
    }
    assert(lifetime < 8 && last_x >= 240 * 4 && last_x < 245 * 4);
    w.samus.x = 64 * 4;

    /* Diagonal shots move 7/10 of the distance on both axes. */
    mzm_weapons_init(&weapons);
    w.samus.aim = MZM_AIM_DIAGONAL_UP;
    w.samus.facing = -1;
    assert(mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, MZM_KEY_B,
                                   &w.equipment));
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, -10, -30, &w.collision,
                       &animation);
    beam = first_active(&weapons);
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, &animation);
    int32_t bx = beam->x, by = beam->y;
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, &animation);
    assert(beam->x == bx - 14 && beam->y == by - 14 && !beam->x_flip);

    /* At most six beams exist at once. */
    mzm_weapons_init(&weapons);
    w.samus.aim = MZM_AIM_UP;
    for (int i = 0; i < 8; ++i) {
        weapons.cooldown = 0;
        mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, MZM_KEY_B, &w.equipment);
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -40, &w.collision,
                           &animation);
    }
    assert(mzm_projectile_count(&weapons, MZM_PROJECTILE_BEAM) == MZM_PROJECTILE_LIMIT_BEAM);
}

static void test_missile(void) {
    World w;
    world_init(&w);
    place(&w, 64, 224);
    step(&w, 0);
    w.equipment.missiles = 1;
    MzmWeapons weapons;
    mzm_weapons_init(&weapons);
    assert(mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R | MZM_KEY_B,
                                   MZM_KEY_R | MZM_KEY_B, &w.equipment));
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 18, -26, &w.collision, NULL);
    const MzmProjectile *missile = first_active(&weapons);
    assert(missile && missile->type == MZM_PROJECTILE_MISSILE);
    assert(weapons.cooldown == MZM_MISSILE_COOLDOWN);
    /* The last missile is spent at launch and disarms missiles. */
    assert(w.equipment.missiles == 0 && weapons.highlighted == MZM_WEAPON_NONE);
    int32_t x = missile->x;
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    assert(missile->x == x + 48);
    x = missile->x;
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    assert(missile->x == x + 8);
    x = missile->x;
    /* Samus's horizontal velocity is added when moving the same way. */
    w.samus.x_velocity = 64;
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    assert(missile->x == x + 9 + 8);
}

static void test_firing_breaks_a_spin(void) {
    World w;
    world_init(&w);
    place(&w, 64, 224);
    step(&w, 0);
    step(&w, MZM_KEY_RIGHT);
    step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    steps(&w, MZM_KEY_RIGHT | MZM_KEY_A, 5);
    assert(w.samus.pose == MZM_POSE_SPINNING);
    MzmWeapons weapons;
    mzm_weapons_init(&weapons);
    MzmInput input = {MZM_KEY_RIGHT | MZM_KEY_A | MZM_KEY_B, MZM_KEY_B, false};
    input.new_projectile = mzm_weapons_begin_frame(&weapons, &w.samus, input.held,
                                                   input.pressed, &w.equipment);
    mzm_samus_update(&w.samus, &input, &w.equipment, &w.collision, &w.animation);
    assert(input.new_projectile && w.samus.pose == MZM_POSE_MIDAIR);
}

int main(void) {
    test_native_tables();
    test_damage();
    test_running_and_jumping();
    test_spin_jump();
    test_wall_jump();
    test_crouch_and_morph();
    test_ledge();
    test_hurt_and_death();
    test_random_input_never_embeds();
    test_weapon_selection();
    test_beam();
    test_missile();
    test_firing_breaks_a_spin();
    puts("MZM Samus controller tests passed");
    return 0;
}
