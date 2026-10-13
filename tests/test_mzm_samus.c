/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free regression tests for the native Zero Mission pose controller. */
#include "mzm_effects.h"
#include "mzm_projectiles.h"
#include "mzm_samus.h"
#include "mzm_sfx.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define GRID_W 20
#define GRID_H 15

/* Solid blocks and, where `slope` is nonzero, a floor slope ClipdataType
 * (2 left steep, 3 right steep). */
typedef struct { bool solid[GRID_H][GRID_W]; uint8_t slope[GRID_H][GRID_W]; } Grid;

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

/* ClipdataProcessForSamus on the grid: solid blocks, solid columns past the
 * sides, air below. */
static uint32_t grid_clipdata(void *context, uint16_t y, uint16_t x) {
    const Grid *g = context;
    int col = x / 64, row = y / 64;
    if (col >= GRID_W) return 1 | MZM_SAMUS_CLIP_SOLID_FLAG;
    if (row >= GRID_H) return 0;
    unsigned sx = x % 64, sy = y % 64;
    /* ClipdataConvertToCollision for the steep slopes. */
    if (g->slope[row][col] == 2) return sy >= sx ? 2 | MZM_SAMUS_CLIP_SOLID_FLAG : 2;
    if (g->slope[row][col] == 3) return sy >= 63 - sx ? 3 | MZM_SAMUS_CLIP_SOLID_FLAG : 3;
    return g->solid[row][col] ? 1 | MZM_SAMUS_CLIP_SOLID_FLAG : 0;
}

static void fill(Grid *g, int col, int row, int w, int h) {
    for (int r = row; r < row + h; ++r)
        for (int c = col; c < col + w; ++c) g->solid[r][c] = true;
}

/* Two-tick frames by default; MidAir and the ledge pulls use the frame
 * counts and durations of the native Power Suit sequences. */
static int test_durations(void *context, const char *key,
                          uint8_t *durations, int max) {
    (void)context;
    int count = 2, ticks = 2;
    if (strstr(key, "/SPOSE_MIDAIR/")) count = 5;
    if (strstr(key, "/SPOSE_PULLING_YOURSELF_UP_FROM_HANGING/") ||
        strstr(key, "/SPOSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL/")) { count = 3; ticks = 3; }
    if (strstr(key, "/SPOSE_PULLING_YOURSELF_FORWARD_FROM_HANGING/")) { count = 4; ticks = 3; }
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
    w->collision = (MzmCollision){.context = &w->grid, .blocked = grid_blocked,
                                  .clipdata = grid_clipdata};
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
    MzmInput input = {held, (uint16_t)(held & ~w->previous), false, 0, 0};
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
    /* lastWallTouchedMidAir: the native KEY_LEFT bit, opposite the facing. */
    assert(w.samus.last_wall == (1u << 5) && box_clear(&w));
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
    assert(w.samus.pose == MZM_POSE_PULLING_YOURSELF_UP_FROM_HANGING);
    steps(&w, MZM_KEY_RIGHT, 9);
    assert(w.samus.pose == MZM_POSE_PULLING_YOURSELF_FORWARD_FROM_HANGING);
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
    /* No energy: SamusChangeToHurtPose sets the drift toward the GBA
     * screen's centre (gBg1X + 480, gBg1Y + 400 subpixels); already there,
     * Samus stays put whatever the input. */
    w.samus.camera_x = w.samus.x - 480;
    w.samus.camera_y = (w.samus.y - 1) - 400;
    mzm_samus_hurt(&w.samus, &w.collision, true);
    assert(w.samus.pose == MZM_POSE_DYING && w.samus.x_velocity == 0 && w.samus.y_velocity == 0 &&
           w.samus.monochrome == MZM_MONOCHROME_BLACK);
    w.samus.monochrome = MZM_MONOCHROME_NONE;    /* the game state takes it */
    int32_t x = w.samus.x, y = w.samus.y;
    steps(&w, MZM_KEY_RIGHT | MZM_KEY_A, 20);
    assert(w.samus.pose == MZM_POSE_DYING && w.samus.x == x && w.samus.y == y);
    /* After the animation's frame 23 the timer runs; at 120 the game over
     * starts (gMainGameMode = GM_GAMEOVER). */
    int dying = 0;
    w.samus.anim_frame = 24;        /* the test world has no animation durations */
    while (w.samus.timer < 5 && dying++ < 2000) step(&w, 0);
    /* The white fading starts with the first flash. */
    assert(w.samus.timer == 5 && w.samus.monochrome == MZM_MONOCHROME_WHITE && w.samus.walljump_timer == 1);
    while (!w.samus.game_over && dying++ < 2000) step(&w, 0);
    assert(w.samus.game_over && w.samus.pose == MZM_POSE_DYING && w.samus.timer == 121 &&
           w.samus.walljump_timer == 8);
    /* Left of the centre: half the distance as velocity, a stop past it. */
    world_init(&w);
    place(&w, 64, 224);
    step(&w, 0);
    w.samus.camera_x = 0;
    w.samus.camera_y = (w.samus.y - 1) - 400;
    mzm_samus_hurt(&w.samus, &w.collision, true);
    assert(w.samus.x_velocity == (480 - 256) / 2 && w.samus.y_velocity == 0);
    step(&w, 0);
    assert(w.samus.x == 256 + 14);
    steps(&w, 0, 40);
    assert(w.samus.x_velocity == 0 && w.samus.x >= 480 && w.samus.x < 480 + 14);
}

/* Random input over mixed geometry never keeps Samus inside collision for
 * a second nor takes her out of the room. The native probes allow overlaps:
 * a spin broken under a low ceiling moves Samus down half a block, her
 * upward velocity then puts the head in the ceiling, and with no vertical
 * motion (A released at the apex) no probe runs until she falls out. */
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
    int embedded = 0;
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
        embedded = box_clear(&w) ? 0 : embedded + 1;
        assert(embedded < 60);
        assert(w.samus.x > 0 && w.samus.x < GRID_W * 64 && w.samus.y > 0 && w.samus.y <= GRID_H * 64);
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

static int affected_damage;
static void record_affect(void *context, int32_t x, int32_t y, int damage) {
    (void)context;
    (void)x;
    (void)y;
    affected_damage |= damage;
}

static void test_projectiles_affect_blocks(void) {
    World w;
    world_init(&w);
    fill(&w.grid, 6, 0, 1, GRID_H);
    w.collision.affect = record_affect;
    place(&w, 64, 224);
    step(&w, 0);
    MzmWeapons weapons;
    mzm_weapons_init(&weapons);
    affected_damage = 0;
    assert(mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, MZM_KEY_B,
                                   &w.equipment));
    for (int i = 0; i < 10; ++i)
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 18, -26, &w.collision, NULL);
    assert(affected_damage == MZM_DAMAGE_BEAM);
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
    MzmInput input = {MZM_KEY_RIGHT | MZM_KEY_A | MZM_KEY_B, MZM_KEY_B, false, 0, 0};
    input.new_projectile = mzm_weapons_begin_frame(&weapons, &w.samus, input.held,
                                                   input.pressed, &w.equipment);
    mzm_samus_update(&w.samus, &input, &w.equipment, &w.collision, &w.animation);
    assert(input.new_projectile && w.samus.pose == MZM_POSE_MIDAIR);
}


/* The recorded requests other than SamusCheckShinesparking's per-frame
 * SoundStop(SOUND_SPEEDBOOSTER_START). */
static int sfx_count(void) {
    int count = 0;
    for (int i = 0; i < mzm_sfx_count(); ++i) {
        MzmSfxRequest r = mzm_sfx_get(i);
        if (!(r.action == MZM_SFX_STOP && r.sound == MZM_SOUND_SPEEDBOOSTER_START)) count++;
    }
    return count;
}

static MzmSfxRequest sfx_get(int index) {
    for (int i = 0; i < mzm_sfx_count(); ++i) {
        MzmSfxRequest r = mzm_sfx_get(i);
        if (r.action == MZM_SFX_STOP && r.sound == MZM_SOUND_SPEEDBOOSTER_START) continue;
        if (index-- == 0) return r;
    }
    return (MzmSfxRequest){MZM_SFX_PLAY, 0, 0};
}

/* The native SoundPlay points of the ported poses and weapons. */
static void test_sounds(void) {
    World w;
    world_init(&w);
    place(&w, 64, 224);
    step(&w, 0);
    mzm_sfx_clear();
    /* SamusSetMidAir: a standing jump plays the low jump sound. */
    step(&w, MZM_KEY_A);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_LOW_JUMP));
    assert(!mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_LANDING));
    while (w.samus.pose == MZM_POSE_MIDAIR) step(&w, MZM_KEY_A);
    /* SamusSetLandingPose: the landing effect on plain ground. */
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_LANDING));
    steps(&w, 0, 4);

    /* SamusRunningGfx: footsteps on animation frame 1. */
    mzm_sfx_clear();
    steps(&w, MZM_KEY_RIGHT, 6);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_FOOTSTEPS_1));
    step(&w, 0);

    /* Crouching, morphing and unmorphing. */
    mzm_sfx_clear();
    step(&w, MZM_KEY_DOWN);
    assert(w.samus.pose == MZM_POSE_CROUCHING);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_CROUCHING));
    step(&w, 0);
    step(&w, MZM_KEY_DOWN);
    assert(w.samus.pose == MZM_POSE_MORPHING);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_MORPHING));
    steps(&w, 0, 8);
    assert(w.samus.pose == MZM_POSE_MORPH_BALL);
    mzm_sfx_clear();
    step(&w, MZM_KEY_UP);
    assert(w.samus.pose == MZM_POSE_UNMORPHING);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_UNMORPHING));
    steps(&w, 0, 8);

    /* SamusSpinningGfx: the spin sound on the spin's first frame. */
    world_init(&w);
    place(&w, 40, 224);
    step(&w, 0);
    step(&w, MZM_KEY_RIGHT);
    mzm_sfx_clear();
    step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_STARTING_SPIN_JUMP);
    /* A running jump starts a spin: no straight jump sound. */
    assert(!mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_LOW_JUMP));
    steps(&w, MZM_KEY_RIGHT | MZM_KEY_A, 6);
    assert(w.samus.pose == MZM_POSE_SPINNING);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SPIN_JUMP));

    /* SamusChangeToHurtPose, then SamusDying: the music stops on the
     * first dying frame and the dying sound plays on the second. */
    world_init(&w);
    place(&w, 64, 224);
    step(&w, 0);
    mzm_sfx_clear();
    mzm_samus_hurt(&w.samus, &w.collision, false);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_HURT));
    steps(&w, 0, 40);
    mzm_sfx_clear();
    mzm_samus_hurt(&w.samus, &w.collision, true);
    assert(sfx_count() == 0);
    step(&w, 0);
    assert(sfx_count() == 1 && sfx_get(0).action == MZM_SFX_STOP_ALL);
    step(&w, 0);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_DYING));
    step(&w, 0);
    assert(sfx_count() == 2);

    /* SamusSetHighlightedWeapon and ProjectileUpdate. */
    world_init(&w);
    place(&w, 64, 224);
    step(&w, 0);
    MzmWeapons weapons;
    mzm_weapons_init(&weapons);
    w.equipment.missiles = 5;
    w.equipment.max_missiles = 5;
    w.equipment.super_missiles = 5;
    w.equipment.max_super_missiles = 5;
    mzm_sfx_clear();
    mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R, 0, &w.equipment);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_ARMING_WEAPON));
    mzm_sfx_clear();
    mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R, 0, &w.equipment);
    assert(sfx_count() == 0);
    mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R, MZM_KEY_SELECT, &w.equipment);
    assert(sfx_count() == 1 && sfx_get(0).sound == MZM_SOUND_MISSILE_TOGGLE);
    mzm_sfx_clear();
    mzm_weapons_begin_frame(&weapons, &w.samus, 0, 0, &w.equipment);
    while (weapons.cooldown) mzm_weapons_begin_frame(&weapons, &w.samus, 0, 0, &w.equipment);
    assert(mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, MZM_KEY_B, &w.equipment));
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -20, &w.collision, NULL);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_BEAM_SHOT));
    mzm_sfx_clear();
    while (weapons.cooldown) mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R, 0, &w.equipment);
    assert(mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R | MZM_KEY_B, MZM_KEY_B,
                                   &w.equipment));
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -20, &w.collision, NULL);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SUPER_MISSILE_SHOT));
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SUPER_MISSILE_THRUST));
}

/* The nine CAA_BOMB_PISTOL points of an explosion, in order. */
static int bomb_points;
static int32_t bomb_point_x[16], bomb_point_y[16];
static void record_bomb_points(void *context, int32_t x, int32_t y, int damage) {
    (void)context;
    if (damage == MZM_DAMAGE_BOMB_PISTOL && bomb_points < 16) {
        bomb_point_x[bomb_points] = x;
        bomb_point_y[bomb_points] = y;
        bomb_points++;
    }
}

/* ProjectileProcessBomb, ProjectileCheckSamusBombBounce, SamusBombBounce. */
static void test_bombs(void) {
    World w;
    world_init(&w);
    w.equipment.items |= MZM_ITEM_BOMBS;
    w.collision.affect = record_bomb_points;
    place(&w, 64, 224);
    step(&w, 0);
    step(&w, MZM_KEY_DOWN);
    step(&w, 0);
    step(&w, MZM_KEY_DOWN);
    steps(&w, 0, 8);
    assert(w.samus.pose == MZM_POSE_MORPH_BALL);
    MzmWeapons weapons;
    mzm_weapons_init(&weapons);
    /* Without the item, B does nothing in the Morph Ball. */
    w.equipment.items &= ~(uint32_t)MZM_ITEM_BOMBS;
    assert(!mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, MZM_KEY_B, &w.equipment));
    assert(!weapons.pending);
    w.equipment.items |= MZM_ITEM_BOMBS;
    mzm_sfx_clear();
    /* No hasNewProjectile: the Morph Ball pose goes on. */
    assert(!mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, MZM_KEY_B, &w.equipment));
    weapons.samus_previous_x = w.samus.x;
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    const MzmProjectile *bomb = first_active(&weapons);
    assert(bomb && bomb->type == MZM_PROJECTILE_BOMB && weapons.cooldown == 7);
    assert(bomb->x == w.samus.x && bomb->y == w.samus.y - 1 && !bomb->x_flip);
    assert(bomb->bomb_stage == MZM_BOMB_FIRST_SPIN && !bomb->fast);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_BOMB_SET));
    /* 16 slow frames, 16 fast frames, then the explosion particle. */
    int frames = 0;
    while (!bomb->fast) {
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
        frames++;
    }
    assert(frames == 16);
    frames = 0;
    while (bomb->bomb_stage != MZM_BOMB_EXPLODING) {
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
        frames++;
    }
    assert(frames == 16 && bomb->hidden && !bomb->hits_sprites);
    assert(weapons.impact_count == 1 && weapons.impacts[0].type == MZM_PROJECTILE_BOMB &&
           weapons.impacts[0].y == bomb->y - 16 && bomb_points == 0);
    /* The first explosion frame: the block centre, the sprite pass and the
     * bounce of Samus sitting on the bomb (straight up). */
    weapons.samus_previous_x = w.samus.x;
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    assert(bomb_points == 1 && bomb_point_x[0] == bomb->x && bomb_point_y[0] == bomb->y - 8);
    assert(bomb->hits_sprites && weapons.bomb_bounce == MZM_FORCED_BOMB_JUMP_UP);
    assert(mzm_samus_bomb_bounce(&w.samus, &w.equipment, &w.collision, weapons.bomb_bounce));
    assert(w.samus.pose == MZM_POSE_MORPH_BALL_MIDAIR &&
           w.samus.y_velocity == MZM_BOMB_BOUNCE_Y_VELOCITY && w.samus.x_velocity == 0);
    int32_t ground = w.samus.y;
    /* The other eight points, one a frame, then the bomb is gone. */
    for (int i = 0; i < 8; ++i)
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    assert(bomb_points == 9);
    assert(bomb_point_y[1] == bomb->y + 56 && bomb_point_x[1] == bomb->x);
    assert(bomb_point_x[2] == bomb->x + 48 && bomb_point_x[3] == bomb->x - 48);
    assert(bomb_point_x[4] == bomb->x + 36 && bomb_point_y[4] == bomb->y + 56);
    assert(bomb_point_y[6] == bomb->y - 72 && bomb_point_x[8] == bomb->x - 36);
    for (int i = 0; i < 6; ++i)
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    assert(bomb->active);
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    assert(!bomb->active && bomb_points == 9);
    /* The bounce rises without A held (the mid-air jump forced movement). */
    step(&w, 0);
    step(&w, 0);
    assert(w.samus.y < ground);

    /* Beside the bomb: a sideways bounce away from it. */
    assert(mzm_samus_bomb_bounce_allowed(&w.samus, MZM_FORCED_BOMB_JUMP_LEFT) ==
           (w.samus.y_velocity <= 0));
    MzmSamus rolling = w.samus;
    rolling.pose = MZM_POSE_ROLLING;
    assert(mzm_samus_bomb_bounce(&rolling, &w.equipment, &w.collision,
                                 MZM_FORCED_BOMB_JUMP_RIGHT));
    assert(rolling.x_velocity == MZM_BOMB_BOUNCE_X_VELOCITY && rolling.facing == 1);
    /* Falling in the air: only from a bomb that is not above. */
    MzmSamus falling = w.samus;
    falling.pose = MZM_POSE_MORPH_BALL_MIDAIR;
    falling.y_velocity = -8;
    assert(mzm_samus_bomb_bounce_allowed(&falling, MZM_FORCED_BOMB_JUMP_UP));
    assert(!mzm_samus_bomb_bounce_allowed(&falling, MZM_FORCED_BOMB_JUMP_UP | MZM_BOMB_JUMP_ABOVE));
    falling.pose = MZM_POSE_STANDING;
    assert(!mzm_samus_bomb_bounce_allowed(&falling, MZM_FORCED_BOMB_JUMP_UP));

    /* Four bombs at most, one every 7 frames. */
    mzm_weapons_init(&weapons);
    w.samus.pose = MZM_POSE_MORPH_BALL;
    int placed = 0;
    for (int frame = 0; frame < 40; ++frame) {
        mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, frame & 1 ? 0 : MZM_KEY_B,
                                &w.equipment);
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
        int count = mzm_projectile_count(&weapons, MZM_PROJECTILE_BOMB);
        if (count > placed) placed = count;
    }
    assert(placed == MZM_PROJECTILE_LIMIT_BOMB);
}

/* Run right with the position kept in the grid; returns the frames run. */
static int run_until(World *w, bool (*done)(const MzmSamus *), int limit) {
    int frames = 0;
    while (!done(&w->samus) && frames < limit) {
        w->samus.x = 160 * 4;
        step(w, MZM_KEY_RIGHT);
        frames++;
    }
    return frames;
}
static bool is_speedboosting(const MzmSamus *s) { return s->speedboosting; }

/* SamusRunning's charge, SamusCheckShinesparking, SamusSkidding. */
static void test_speed_booster(void) {
    World w;
    world_init(&w);
    place(&w, 160, 224);
    step(&w, 0);
    /* Without the item the run stays at the normal cap. */
    run_until(&w, is_speedboosting, 300);
    assert(!w.samus.speedboosting && w.samus.x_velocity == MZM_X_VELOCITY_CAP);
    step(&w, 0);
    steps(&w, 0, 4);

    w.equipment.items |= MZM_ITEM_SPEED_BOOSTER;
    mzm_sfx_clear();
    int frames = run_until(&w, is_speedboosting, 400);
    /* 12 frames to reach 96, the timer counts from there: 140 at 120
     * frames, 160 at 140, then 8 a frame up to 160. */
    assert(w.samus.speedboosting && w.samus.x_velocity >= 160);
    assert(frames > 140 && frames < 175);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SPEEDBOOSTER_START));
    assert(w.samus.x_velocity == MZM_X_SPEEDBOOST_VELOCITY_CAP);

    /* A jump keeps the speed boost and its mid-air cap. */
    MzmSamus saved = w.samus;
    step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_STARTING_SPIN_JUMP && w.samus.speedboosting);
    w.samus = saved;

    /* Releasing the direction skids: 10 subpixels a frame less. */
    mzm_sfx_clear();
    step(&w, 0);
    assert(w.samus.pose == MZM_POSE_SKIDDING && !w.samus.speedboosting);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SKIDDING));
    int16_t velocity = w.samus.x_velocity;
    step(&w, 0);
    assert(w.samus.x_velocity == velocity - 10 && !w.samus.speedboosting);
    /* Down alone stores a shinespark: the glow and the charge sound. */
    step(&w, MZM_KEY_DOWN);
    assert(w.samus.pose == MZM_POSE_CROUCHING &&
           w.samus.shinespark_timer == MZM_SHINESPARK_TIMER_MAX);
    mzm_sfx_clear();
    steps(&w, MZM_KEY_DOWN, 20);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SPEEDBOOSTER_CHARGE));
    steps(&w, MZM_KEY_DOWN, 200);
    assert(w.samus.shinespark_timer == 0);

    /* Without skidding input the skid ends standing. */
    world_init(&w);
    w.equipment.items |= MZM_ITEM_SPEED_BOOSTER;
    place(&w, 160, 224);
    step(&w, 0);
    run_until(&w, is_speedboosting, 400);
    step(&w, 0);
    frames = 0;
    while (w.samus.pose == MZM_POSE_SKIDDING && frames < 40) {
        w.samus.x = 160 * 4;
        step(&w, 0);
        frames++;
    }
    assert(w.samus.pose == MZM_POSE_STANDING && frames == 16);
}

/* SamusDelayBeforeShinesparking, SamusShinesparking, the collision and the
 * delay after, and the ballspark. */
static void test_shinespark(void) {
    World w;
    world_init(&w);
    fill(&w.grid, 0, 2, GRID_W, 1);            /* a ceiling */
    fill(&w.grid, 17, 0, 1, GRID_H);           /* a wall on the right */
    place(&w, 64, 224);
    step(&w, 0);
    /* A stored shinespark: A without a direction starts it, half a block up. */
    w.samus.shinespark_timer = 100;
    int32_t y = w.samus.y;
    mzm_sfx_clear();
    step(&w, MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_DELAY_BEFORE_SHINESPARKING && w.samus.y == y - 32);
    assert(w.samus.shinespark_timer == 50 &&
           mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SHINESPARKING));
    /* No gravity while charging; the stored timer does not run. */
    step(&w, 0);
    assert(w.samus.y == y - 32 && w.samus.shinespark_timer == 50);
    int frames = 0;
    while (w.samus.pose == MZM_POSE_DELAY_BEFORE_SHINESPARKING && frames++ < 20) step(&w, 0);
    /* Upwards by default, at 192 (24 subpixels a frame), speed boosting. */
    assert(w.samus.pose == MZM_POSE_SHINESPARKING && w.samus.forced == MZM_FORCED_UPWARDS_SHINESPARK);
    assert(w.samus.y_velocity == MZM_SHINESPARK_VELOCITY && w.samus.speedboosting &&
           w.samus.shinespark_timer == 0);
    y = w.samus.y;
    step(&w, 0);
    assert(w.samus.y == y - 24);
    mzm_sfx_clear();
    frames = 0;
    while (w.samus.pose == MZM_POSE_SHINESPARKING && frames++ < 60) step(&w, 0);
    /* The ceiling stops it: the collision, invincible, then the delay. */
    assert(w.samus.pose == MZM_POSE_SHINESPARK_COLLISION && w.samus.invincibility == 48);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SHINESPARK_COLLISION));
    assert(!mzm_sfx_requested(MZM_SFX_STOP, MZM_SOUND_SHINESPARKING));
    y = w.samus.y;
    frames = 0;
    while (w.samus.pose == MZM_POSE_SHINESPARK_COLLISION && frames++ < 40) step(&w, 0);
    assert(w.samus.pose == MZM_POSE_DELAY_AFTER_SHINESPARKING && frames == 17 && w.samus.y == y);
    while (w.samus.pose == MZM_POSE_DELAY_AFTER_SHINESPARKING && frames++ < 80) step(&w, 0);
    /* SamusSetPose only stops the sound when a charge or a spark is cut
     * short, not after the collision. */
    assert(w.samus.pose == MZM_POSE_MIDAIR && !mzm_sfx_requested(MZM_SFX_STOP, MZM_SOUND_SHINESPARKING));
    while (w.samus.pose != MZM_POSE_LANDING && frames++ < 200) step(&w, 0);

    /* Sideways: the direction held when the charge ends, until the wall. */
    steps(&w, 0, 6);
    w.samus.shinespark_timer = 100;
    step(&w, MZM_KEY_A);
    frames = 0;
    while (w.samus.pose == MZM_POSE_DELAY_BEFORE_SHINESPARKING && frames++ < 20)
        step(&w, MZM_KEY_RIGHT);
    assert(w.samus.pose == MZM_POSE_SHINESPARKING && w.samus.forced == MZM_FORCED_SIDEWARDS_SHINESPARK &&
           w.samus.x_velocity == MZM_SHINESPARK_VELOCITY && w.samus.y_velocity == 0);
    y = w.samus.y;
    while (w.samus.pose == MZM_POSE_SHINESPARKING && frames++ < 80) step(&w, MZM_KEY_RIGHT);
    assert(w.samus.pose == MZM_POSE_SHINESPARK_COLLISION && w.samus.y == y);
    assert(box_clear(&w) && w.samus.x > 15 * 64);

    /* Diagonal with L. */
    world_init(&w);
    place(&w, 64, 224);
    step(&w, 0);
    w.samus.shinespark_timer = 100;
    step(&w, MZM_KEY_A);
    while (w.samus.pose == MZM_POSE_DELAY_BEFORE_SHINESPARKING) step(&w, MZM_KEY_L);
    assert(w.samus.forced == MZM_FORCED_DIAGONAL_SHINESPARK &&
           w.samus.x_velocity == MZM_DIAGONAL_SHINESPARK_X_VELOCITY &&
           w.samus.y_velocity == MZM_DIAGONAL_SHINESPARK_Y_VELOCITY);

    /* A ballspark: High Jump, A in the Morph Ball, 60 frames of charge with
     * a hop, then upwards. */
    world_init(&w);
    w.equipment.items |= MZM_ITEM_HIGH_JUMP;
    fill(&w.grid, 0, 0, GRID_W, 1);     /* a ceiling: above the room is air */
    place(&w, 64, 224);
    step(&w, 0);
    step(&w, MZM_KEY_DOWN);
    step(&w, 0);
    step(&w, MZM_KEY_DOWN);
    steps(&w, 0, 8);
    assert(w.samus.pose == MZM_POSE_MORPH_BALL);
    w.samus.shinespark_timer = 100;
    y = w.samus.y;
    mzm_sfx_clear();
    step(&w, MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_DELAY_BEFORE_BALLSPARKING && w.samus.y == y - 32);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_BALLSPARKING));
    step(&w, 0);
    assert(w.samus.y == y - 64);
    frames = 1;
    while (w.samus.pose == MZM_POSE_DELAY_BEFORE_BALLSPARKING && frames++ < 100) step(&w, 0);
    /* The hop frame, then CONVERT_SECONDS(1.f) of charge. */
    assert(w.samus.pose == MZM_POSE_BALLSPARKING && frames == 61 &&
           w.samus.y_velocity == MZM_SHINESPARK_VELOCITY);
    while (w.samus.pose == MZM_POSE_BALLSPARKING && frames++ < 200) step(&w, 0);
    assert(w.samus.pose == MZM_POSE_BALLSPARK_COLLISION);
    while (w.samus.pose == MZM_POSE_BALLSPARK_COLLISION && frames++ < 300) step(&w, 0);
    assert(w.samus.pose == MZM_POSE_MORPH_BALL_MIDAIR);
}

/* SamusSetHighlightedWeapon's power bomb branch, the power bomb spawn and
 * ProjectileProcessPowerBomb. */
static void test_power_bomb(void) {
    World w;
    world_init(&w);
    w.equipment.items |= MZM_ITEM_BOMBS;
    w.equipment.power_bombs = w.equipment.max_power_bombs = 2;
    w.equipment.missiles = w.equipment.max_missiles = 5;
    place(&w, 64, 224);
    step(&w, 0);
    MzmWeapons weapons;
    mzm_weapons_init(&weapons);
    /* Standing, R arms missiles; in the Morph Ball, power bombs. */
    mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R, 0, &w.equipment);
    assert(weapons.highlighted == MZM_WEAPON_MISSILE);
    mzm_weapons_begin_frame(&weapons, &w.samus, 0, 0, &w.equipment);
    step(&w, MZM_KEY_DOWN);
    step(&w, 0);
    step(&w, MZM_KEY_DOWN);
    steps(&w, 0, 8);
    assert(w.samus.pose == MZM_POSE_MORPH_BALL);
    mzm_sfx_clear();
    assert(!mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R | MZM_KEY_B, MZM_KEY_B,
                                    &w.equipment));
    assert(weapons.highlighted == MZM_WEAPON_POWER_BOMB &&
           weapons.pending_type == MZM_PROJECTILE_POWER_BOMB);
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    const MzmProjectile *pb = first_active(&weapons);
    assert(pb && pb->type == MZM_PROJECTILE_POWER_BOMB && weapons.cooldown == 5);
    assert(w.equipment.power_bombs == 1 && weapons.power_bomb_set &&
           weapons.power_bomb.placed);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_POWER_BOMB_SET));
    /* One power bomb at a time. */
    while (weapons.cooldown) mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R, 0, &w.equipment);
    mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R | MZM_KEY_B, MZM_KEY_B, &w.equipment);
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    assert(mzm_projectile_count(&weapons, MZM_PROJECTILE_POWER_BOMB) == 1 &&
           w.equipment.power_bombs == 1);
    /* 54 slow frames (the one just run included), 40 fast, then the
     * explosion at the bomb. */
    int frames = 1;
    while (!pb->fast) {
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
        frames++;
    }
    assert(frames == 54);
    frames = 0;
    while (pb->active) {
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
        frames++;
    }
    assert(frames == 40 && weapons.power_bomb.state == MZM_PB_EXPLODING &&
           weapons.power_bomb.x == pb->x && !weapons.power_bomb.placed);
    /* No power bomb while the explosion runs. */
    mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R | MZM_KEY_B, MZM_KEY_B, &w.equipment);
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    assert(mzm_projectile_count(&weapons, MZM_PROJECTILE_POWER_BOMB) == 0 &&
           w.equipment.power_bombs == 1);
    /* The last power bomb disarms them. */
    weapons.power_bomb.state = MZM_PB_NONE;
    while (weapons.cooldown) mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R, 0, &w.equipment);
    mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_R | MZM_KEY_B, MZM_KEY_B, &w.equipment);
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    assert(w.equipment.power_bombs == 0 && weapons.highlighted == MZM_WEAPON_NONE);
}

/* SamusCheckFireBeamMissile's charge, ParticleChargingBeam's sounds, the
 * charged and Long Beam shots. */
static void test_charge_beam(void) {
    World w;
    world_init(&w);
    place(&w, 64, 224);
    step(&w, 0);
    MzmWeapons weapons;
    mzm_weapons_init(&weapons);
    /* Without the Charge Beam, holding B never charges. */
    assert(mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, MZM_KEY_B, &w.equipment));
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -20, &w.collision, NULL);
    assert(weapons.shot_flash && weapons.release_palette_timer == 4);
    for (int i = 0; i < 80; ++i) {
        mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, 0, &w.equipment);
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -20, &w.collision, NULL);
    }
    assert(weapons.charge_counter == 0);
    assert(!mzm_weapons_begin_frame(&weapons, &w.samus, 0, 0, &w.equipment));

    /* With it: the start threshold (16) starts the particle and its sound,
     * a full charge at 64, then 79 wraps to 64. */
    w.equipment.items |= MZM_ITEM_CHARGE_BEAM;
    mzm_weapons_init(&weapons);
    mzm_sfx_clear();
    for (int i = 0; i < 16; ++i) {
        assert(!mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, 0, &w.equipment));
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -20, &w.collision, NULL);
    }
    assert(weapons.charge_counter == 16 && weapons.charge_particle.stage == 1);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SHORT_CHARGE_INIT));
    for (int i = 0; i < 48; ++i) {
        mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, 0, &w.equipment);
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -20, &w.collision, NULL);
    }
    assert(weapons.charge_counter == MZM_CHARGE_FULL && weapons.charge_particle.stage == 2);
    mzm_sfx_clear();
    for (int i = 0; i < 16; ++i) {
        mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, 0, &w.equipment);
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -20, &w.collision, NULL);
    }
    assert(weapons.charge_counter == MZM_CHARGE_FULL);  /* 79, then back to 64 */
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SHORT_CHARGE_LOOP));
    /* Releasing B fires the charged beam. */
    mzm_sfx_clear();
    assert(mzm_weapons_begin_frame(&weapons, &w.samus, 0, 0, &w.equipment));
    assert(weapons.charge_counter == 0 && weapons.pending_type == MZM_PROJECTILE_CHARGED_BEAM);
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -20, &w.collision, NULL);
    const MzmProjectile *shot = first_active(&weapons);
    assert(shot && shot->type == MZM_PROJECTILE_CHARGED_BEAM && weapons.shot_flash);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_CHARGED_BEAM_SHOT));
    assert(weapons.charge_particle.stage == 0 &&
           mzm_sfx_requested(MZM_SFX_STOP, MZM_SOUND_SHORT_CHARGE_INIT));
    /* A partial charge (above 25) fires a normal beam, a short one nothing. */
    mzm_weapons_init(&weapons);
    for (int i = 0; i < 30; ++i) mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, 0, &w.equipment);
    assert(mzm_weapons_begin_frame(&weapons, &w.samus, 0, 0, &w.equipment) &&
           weapons.pending_type == MZM_PROJECTILE_BEAM);
    mzm_weapons_init(&weapons);
    for (int i = 0; i < 20; ++i) mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, 0, &w.equipment);
    assert(!mzm_weapons_begin_frame(&weapons, &w.samus, 0, 0, &w.equipment));
    /* Arming missiles drops the charge. */
    w.equipment.missiles = w.equipment.max_missiles = 5;
    for (int i = 0; i < 70; ++i) mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, 0, &w.equipment);
    assert(weapons.charge_counter >= MZM_CHARGE_FULL);
    mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B | MZM_KEY_R, 0, &w.equipment);
    assert(weapons.charge_counter == 0);

    /* Long Beam: its own type, 24 subpixels a frame, no short lifetime. */
    w.equipment.items |= MZM_ITEM_LONG_BEAM;
    world_init(&w);
    w.equipment.items |= MZM_ITEM_LONG_BEAM | MZM_ITEM_CHARGE_BEAM;
    place(&w, 32, 224);
    step(&w, 0);
    mzm_weapons_init(&weapons);
    mzm_sfx_clear();
    assert(mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, MZM_KEY_B, &w.equipment));
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -20, &w.collision, NULL);
    shot = first_active(&weapons);
    assert(shot && shot->type == MZM_PROJECTILE_LONG_BEAM &&
           mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_BEAM_LONG_SHOT));
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -20, &w.collision, NULL);
    int32_t x = shot->x;
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -20, &w.collision, NULL);
    assert(shot->x == x + 24);
    for (int i = 0; i < 14; ++i)
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -20, &w.collision, NULL);
    assert(shot->active);           /* a normal beam is gone after 12 frames */

    /* Ice Beam (over the Long Beam): 26 subpixels a frame, its long shot
     * sound, and a trailing particle half a block behind on the frames
     * where gFrameCounter8Bit & 3 is 0. */
    w.equipment.items |= MZM_ITEM_ICE_BEAM;
    mzm_weapons_init(&weapons);
    mzm_sfx_clear();
    assert(mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, MZM_KEY_B, &w.equipment));
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -20, &w.collision, NULL);
    shot = first_active(&weapons);
    assert(shot && shot->type == MZM_PROJECTILE_ICE_BEAM &&
           mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_BEAM_LONG_ICE_SHOT));
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -20, &w.collision, NULL);
    weapons.frame8 = 1;
    x = shot->x;
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -20, &w.collision, NULL);
    assert(shot->x == x + 26 && weapons.trail_count == 0);
    weapons.frame8 = 4;
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, -20, &w.collision, NULL);
    assert(weapons.trail_count == 1 && weapons.trails[0].y == shot->y &&
           weapons.trails[0].x == shot->x + (shot->x_flip ? -32 : 32) &&
           weapons.trails[0].effect ==
               (shot->x_flip ? MZM_PE_BEAM_TRAILING_LEFT : MZM_PE_BEAM_TRAILING_RIGHT));
    weapons.frame8 = 0;
    w.equipment.items &= ~MZM_ITEM_ICE_BEAM;

    /* A full charge is released when the Morph Ball forms. */
    mzm_weapons_init(&weapons);
    for (int i = 0; i < 70; ++i) mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, 0, &w.equipment);
    w.samus.pose = MZM_POSE_MORPHING;
    assert(!mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, 0, &w.equipment));
    assert(weapons.pending && weapons.pending_type == MZM_PROJECTILE_CHARGED_BEAM &&
           weapons.charge_counter == 0);
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    assert(mzm_projectile_count(&weapons, MZM_PROJECTILE_CHARGED_LONG_BEAM) == 1);
}

/* SamusCheckNewProjectile's beam types and shot sounds, the Wave Beam
 * through walls, the Plasma Beam's walls, the trails and the hitboxes. */
static void test_wave_and_plasma(void) {
    uint16_t sound = 0;
    assert(mzm_beam_type(MZM_ITEM_WAVE_BEAM | MZM_ITEM_ICE_BEAM, false, &sound) ==
               MZM_PROJECTILE_WAVE_BEAM && sound == MZM_SOUND_BEAM_WAVE_ICE_SHOT);
    assert(mzm_beam_type(MZM_ITEM_PLASMA_BEAM | MZM_ITEM_WAVE_BEAM | MZM_ITEM_LONG_BEAM |
                             MZM_ITEM_ICE_BEAM, true, &sound) ==
               MZM_PROJECTILE_CHARGED_PLASMA_BEAM &&
           sound == MZM_SOUND_CHARGED_BEAM_PLASMA_WAVE_LONG_ICE_SHOT);
    assert(mzm_beam_type(MZM_ITEM_LONG_BEAM | MZM_ITEM_ICE_BEAM, true, &sound) ==
               MZM_PROJECTILE_CHARGED_ICE_BEAM && sound == MZM_SOUND_CHARGED_BEAM_LONG_ICE_SHOT);
    assert(mzm_beam_type(0, false, &sound) == MZM_PROJECTILE_BEAM && sound == MZM_SOUND_BEAM_SHOT);
    int16_t t, b, l, r;
    mzm_projectile_hitbox(MZM_PROJECTILE_WAVE_BEAM, MZM_AIM_FORWARD, 0, &t, &b, &l, &r);
    assert(t == -64 && b == 64 && l == -20 && r == 20);
    mzm_projectile_hitbox(MZM_PROJECTILE_CHARGED_WAVE_BEAM, MZM_AIM_DIAGONAL_DOWN, 0, &t, &b, &l, &r);
    assert(t == -16 && b == 72 && l == -48 && r == 48);
    mzm_projectile_hitbox(MZM_PROJECTILE_PLASMA_BEAM, MZM_AIM_UP, 0, &t, &b, &l, &r);
    assert(t == -20 && b == 20 && l == -20 && r == 20);
    mzm_projectile_hitbox(MZM_PROJECTILE_CHARGED_PLASMA_BEAM, MZM_AIM_UP, MZM_ITEM_WAVE_BEAM,
                          &t, &b, &l, &r);
    assert(t == -36 && b == 36 && l == -72 && r == 72);

    /* The Wave Beam (with Long Beam): 28 subpixels a frame, through the
     * wall, which still gets CAA_BEAM. */
    World w;
    world_init(&w);
    fill(&w.grid, 8, 0, 1, GRID_H);
    w.collision.affect = record_affect;
    w.equipment.items |= MZM_ITEM_WAVE_BEAM | MZM_ITEM_LONG_BEAM;
    place(&w, 64, 224);
    step(&w, 0);
    MzmWeapons weapons;
    mzm_weapons_init(&weapons);
    affected_damage = 0;
    mzm_sfx_clear();
    assert(mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, MZM_KEY_B, &w.equipment));
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 18, -26, &w.collision, NULL);
    const MzmProjectile *shot = first_active(&weapons);
    assert(shot && shot->type == MZM_PROJECTILE_WAVE_BEAM &&
           mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_BEAM_WAVE_LONG_SHOT));
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 18, -26, &w.collision, NULL);
    int32_t x = shot->x;
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 18, -26, &w.collision, NULL);
    assert(shot->x == x + 28);
    for (int i = 0; i < 10; ++i)
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 18, -26, &w.collision, NULL);
    assert(shot->active && shot->x > 9 * 16 * 4 && affected_damage == MZM_DAMAGE_BEAM &&
           weapons.impact_count == 0);

    /* The Plasma Beam without Wave stops at the wall with its impact; 32
     * subpixels a frame. */
    w.equipment.items = (w.equipment.items & ~(uint32_t)MZM_ITEM_WAVE_BEAM) | MZM_ITEM_PLASMA_BEAM;
    mzm_weapons_init(&weapons);
    assert(mzm_weapons_begin_frame(&weapons, &w.samus, MZM_KEY_B, MZM_KEY_B, &w.equipment));
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 18, -26, &w.collision, NULL);
    shot = first_active(&weapons);
    assert(shot && shot->type == MZM_PROJECTILE_PLASMA_BEAM && !shot->wave_oam);
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 18, -26, &w.collision, NULL);
    x = shot->x;
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 18, -26, &w.collision, NULL);
    assert(shot->x == x + 32);
    bool impact = false;
    for (int i = 0; i < 8 && shot->active; ++i) {
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 18, -26, &w.collision, NULL);
        for (int k = 0; k < weapons.impact_count; ++k)
            impact |= weapons.impacts[k].type == MZM_PROJECTILE_PLASMA_BEAM;
    }
    assert(!shot->active && impact);

    /* The charged Wave Beam's trail (PE_CHARGED_WAVE_BEAM_TRAIL when
     * gFrameCounter8Bit & 7 is 0) and the missile trail. */
    w.equipment.items = (w.equipment.items & ~(uint32_t)MZM_ITEM_PLASMA_BEAM) | MZM_ITEM_WAVE_BEAM;
    for (int r2 = 0; r2 < GRID_H - 1; ++r2) w.grid.solid[r2][8] = false;
    mzm_weapons_init(&weapons);
    weapons.pending = true;
    weapons.pending_type = MZM_PROJECTILE_CHARGED_BEAM;
    weapons.frame8 = 1;
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 18, -26, &w.collision, NULL);
    shot = first_active(&weapons);
    assert(shot && shot->type == MZM_PROJECTILE_CHARGED_WAVE_BEAM);
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 18, -26, &w.collision, NULL);
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 18, -26, &w.collision, NULL);
    assert(weapons.trail_count == 0);
    weapons.frame8 = 8;
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 18, -26, &w.collision, NULL);
    assert(weapons.trail_count == 1 && weapons.trails[0].effect == MZM_PE_CHARGED_WAVE_BEAM_TRAIL);
    weapons.frame8 = 0;
}

/* ProjectileMoveTumbling: up then down along sTumblingMissileSpeed (the
 * last speed kept), a pixel a frame sideways, no wall collision, removed
 * off screen; ProjectileCheckDespawn keeps on-screen projectiles. */
static void test_tumbling_missile(void) {
    World w;
    world_init(&w);
    fill(&w.grid, 0, 0, GRID_W, GRID_H);    /* solid everywhere */
    place(&w, 64, 224);
    MzmWeapons weapons;
    mzm_weapons_init(&weapons);
    weapons.has_camera = true;
    weapons.camera_x = weapons.camera_y = 0;
    MzmProjectile *p = &weapons.list[0];
    *p = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_MISSILE, .x = 400, .y = 400,
                         .x_flip = true, .on_screen = true};
    mzm_projectile_start_tumbling(p, 380);
    assert(p->stage == MZM_STAGE_TUMBLING && p->x_flip);
    static const int16_t speeds[] = {-12, -10, -8, -6, -4, -2, 0, 0, 1, 2, 4, 8, 12, 16, 20, 20, 20};
    int32_t y = 400, x = 400;
    for (int i = 0; i < 17; ++i) {
        mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
        y += speeds[i];
        x += 4;
        assert(p->active && p->y == y && p->x == x && p->on_screen);
    }
    /* Leaving the screen (plus its draw distance and padding): removed on
     * the next update. */
    p->y = 160 * 4 + 64 + 1;
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    assert(p->active && !p->on_screen);
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    assert(!p->active);
    /* Without the camera every projectile counts as off screen. */
    weapons.has_camera = false;
    *p = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_SUPER_MISSILE, .x = 400, .y = 400,
                         .on_screen = true};
    mzm_projectile_start_tumbling(p, 500);
    assert(!p->x_flip);
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    assert(p->active && p->x == 396 && !p->on_screen);
    mzm_weapons_update(&weapons, &w.samus, &w.equipment, 0, 0, &w.collision, NULL);
    assert(!p->active);
}

/* Standing on a solid (frozen) sprite: the landing request from
 * SpriteUtilSamusStandingOnSprite lands a midair Samus, and STANDING_ENEMY
 * keeps a ground pose over air until it is released. */
static void test_standing_on_sprite(void) {
    World w;
    world_init(&w);
    place(&w, 64, 96);              /* feet over air */
    w.samus.y_velocity = -20;
    w.samus.pose = MZM_POSE_MIDAIR;
    MzmInput none = {0, 0, false, 0, 0};
    mzm_samus_land_on_sprite(&w.samus, &none, &w.equipment, &w.collision);
    assert(w.samus.pose == MZM_POSE_LANDING && w.samus.y_velocity == 0 &&
           w.samus.copy_y == 96 * 4);
    w.samus.on_enemy = true;
    steps(&w, 0, 10);
    assert(mzm_pose_standing(w.samus.pose) == MZM_STANDING_GROUND && w.samus.y == 96 * 4);
    /* Walking on it keeps the ground pose. */
    steps(&w, MZM_KEY_RIGHT, 10);
    assert(w.samus.pose == MZM_POSE_RUNNING && w.samus.y == 96 * 4);
    /* Released: the next ground check finds air. */
    w.samus.on_enemy = false;
    step(&w, 0);
    assert(w.samus.pose == MZM_POSE_MIDAIR);
}

static void graphics_key(const World *w, uint8_t highlighted, char *key) {
    mzm_samus_graphics_key(&w->samus, &w->equipment, highlighted, key, 160);
}

static void step_input(World *w, uint16_t held, bool new_projectile, uint8_t charge) {
    MzmInput input = {held, (uint16_t)(held & ~w->previous), new_projectile, 0, charge};
    w->previous = held;
    mzm_samus_update(&w->samus, &input, &w->equipment, &w->collision, &w->animation);
}

/* Reported bug 4: the diagonals while running (SamusAimCannon's running
 * case: Up or Down with the direction aims diagonally; no direction pad
 * aim leaves the arm down, ACD_NONE, with its own Running row). */
static void test_running_aim(void) {
    World w;
    world_init(&w);
    place(&w, 64, 224);
    step(&w, 0);
    steps(&w, MZM_KEY_RIGHT, 3);
    char key[160];
    assert(w.samus.pose == MZM_POSE_RUNNING && w.samus.aim == MZM_AIM_NONE);
    graphics_key(&w, 0, key);
    assert(strcmp(key, "PowerSuit/Running/ACD_NONE/right") == 0);
    /* In the gunship (lastWallTouchedMidAir), the saving pose draws the
     * InShip table; else the pose's own. */
    MzmSamus in_ship = w.samus;
    in_ship.pose = MZM_POSE_SAVING_LOADING_GAME;
    in_ship.last_wall = 1;
    mzm_samus_graphics_key(&in_ship, &w.equipment, 0, key, 160);
    assert(strcmp(key, "PowerSuit/InShip/SPOSE_SAVING_LOADING_GAME/right") == 0);
    in_ship.last_wall = 0;
    mzm_samus_graphics_key(&in_ship, &w.equipment, 0, key, 160);
    assert(strcmp(key, "PowerSuit/pose/SPOSE_SAVING_LOADING_GAME/right") == 0);
    step(&w, MZM_KEY_RIGHT | MZM_KEY_UP);
    assert(w.samus.pose == MZM_POSE_RUNNING && w.samus.aim == MZM_AIM_DIAGONAL_UP);
    graphics_key(&w, 0, key);
    assert(strcmp(key, "PowerSuit/Running/ACD_DIAGONALLY_UP/right") == 0);
    step(&w, MZM_KEY_RIGHT | MZM_KEY_DOWN);
    assert(w.samus.aim == MZM_AIM_DIAGONAL_DOWN);
    step(&w, MZM_KEY_RIGHT);
    assert(w.samus.aim == MZM_AIM_FORWARD);    /* aiming released: forward */
    /* Leftward, the same diagonals. */
    steps(&w, MZM_KEY_LEFT, 12);
    assert(w.samus.facing == -1);
    steps(&w, MZM_KEY_LEFT | MZM_KEY_UP, 2);
    assert(w.samus.pose == MZM_POSE_RUNNING && w.samus.aim == MZM_AIM_DIAGONAL_UP);
    graphics_key(&w, 0, key);
    assert(strcmp(key, "PowerSuit/Running/ACD_DIAGONALLY_UP/left") == 0);
    /* L alone aims diagonally up, L with Down diagonally down. */
    steps(&w, MZM_KEY_LEFT | MZM_KEY_L, 2);
    assert(w.samus.aim == MZM_AIM_DIAGONAL_UP);
    step(&w, MZM_KEY_LEFT | MZM_KEY_L | MZM_KEY_DOWN);
    assert(w.samus.aim == MZM_AIM_DIAGONAL_DOWN);
    /* A charging beam keeps the cannon forward. */
    world_init(&w);
    place(&w, 64, 224);
    step(&w, 0);
    for (int i = 0; i < 3; ++i) step_input(&w, MZM_KEY_RIGHT, false, 20);
    assert(w.samus.pose == MZM_POSE_RUNNING && w.samus.aim == MZM_AIM_FORWARD);
    /* Landing while aiming up keeps a diagonal for the run. */
    world_init(&w);
    place(&w, 64, 224);
    step(&w, 0);
    step(&w, MZM_KEY_A);
    steps(&w, MZM_KEY_A | MZM_KEY_UP, 6);
    assert(w.samus.pose == MZM_POSE_MIDAIR && w.samus.aim == MZM_AIM_UP);
}

/* A ledge to hang from: the platform's top at y=160, starting at x=160. */
static void hang(World *w) {
    world_init(w);
    fill(&w->grid, 10, 10, 10, GRID_H - 10);
    place(w, 150, 224);
    step(w, 0);
    step(w, MZM_KEY_RIGHT);
    step(w, MZM_KEY_RIGHT | MZM_KEY_A);
    int frames = 0;
    while (w->samus.pose != MZM_POSE_HANGING_ON_LEDGE && frames++ < 120)
        step(w, MZM_KEY_RIGHT | MZM_KEY_A);
    assert(w->samus.pose == MZM_POSE_HANGING_ON_LEDGE && w->samus.facing == 1);
    steps(w, 0, 3);
}

/* Reported bug 2: climbing with the diagonal toward the ledge, no A
 * (SamusHangingOnLedge turns to aim on Up, SamusAimingWhileHanging pulls
 * Samus up after ten frames of Up without the outward direction). */
static void test_ledge_climb_without_a(void) {
    World w;
    hang(&w);
    step(&w, MZM_KEY_UP | MZM_KEY_RIGHT);
    assert(w.samus.pose == MZM_POSE_TURNING_TO_AIM_WHILE_HANGING && w.samus.facing == -1);
    int frames = 0;
    while (w.samus.pose == MZM_POSE_TURNING_TO_AIM_WHILE_HANGING && frames++ < 20)
        step(&w, MZM_KEY_UP | MZM_KEY_RIGHT);
    assert(w.samus.pose == MZM_POSE_AIMING_WHILE_HANGING);
    frames = 0;
    while (w.samus.pose == MZM_POSE_AIMING_WHILE_HANGING && frames++ < 30)
        step(&w, MZM_KEY_UP | MZM_KEY_RIGHT);
    /* Eleven frames: the timer passes 9. */
    assert(w.samus.pose == MZM_POSE_PULLING_YOURSELF_UP_FROM_HANGING && frames == 11 &&
           w.samus.facing == 1);
    frames = 0;
    while (w.samus.pose != MZM_POSE_STANDING && frames++ < 60) step(&w, MZM_KEY_UP | MZM_KEY_RIGHT);
    assert(w.samus.pose == MZM_POSE_STANDING && w.samus.y == 160 * 4 && w.samus.x >= 160 * 4);

    /* Up alone climbs too; the outward direction with Up does not. */
    hang(&w);
    steps(&w, MZM_KEY_UP | MZM_KEY_LEFT, 40);
    assert(w.samus.pose == MZM_POSE_AIMING_WHILE_HANGING && w.samus.aim == MZM_AIM_DIAGONAL_UP);
    steps(&w, MZM_KEY_UP, 12);
    assert(w.samus.pose == MZM_POSE_PULLING_YOURSELF_UP_FROM_HANGING);

    /* Pressing toward the ledge again hides the arm cannon and hangs. */
    hang(&w);
    steps(&w, MZM_KEY_DOWN, 10);
    assert(w.samus.pose == MZM_POSE_AIMING_WHILE_HANGING && w.samus.aim == MZM_AIM_DOWN);
    step(&w, MZM_KEY_RIGHT);
    assert(w.samus.pose == MZM_POSE_HIDING_ARM_CANNON_WHILE_HANGING);
    frames = 0;
    while (w.samus.pose == MZM_POSE_HIDING_ARM_CANNON_WHILE_HANGING && frames++ < 20) step(&w, 0);
    assert(w.samus.pose == MZM_POSE_HANGING_ON_LEDGE && w.samus.facing == 1);

    /* A block above the ledge: the climb pulls into a morph ball tunnel. */
    hang(&w);
    fill(&w.grid, 10, 8, 1, 1);
    step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL);
    frames = 0;
    while (w.samus.pose != MZM_POSE_MORPH_BALL && frames++ < 60) step(&w, MZM_KEY_RIGHT);
    assert(w.samus.pose == MZM_POSE_MORPH_BALL && w.samus.y == 160 * 4);
}

/* Reported bug 3: firing while hanging turns away from the ledge and aims
 * up (or down) with the hanging arm cannon tables, whose offsets the
 * projectiles use (no more shots from the feet). */
static void test_shooting_while_hanging(void) {
    World w;
    hang(&w);
    step_input(&w, MZM_KEY_B, true, 0);
    assert(w.samus.pose == MZM_POSE_SHOOTING_WHILE_HANGING && w.samus.facing == -1 &&
           w.samus.aim == MZM_AIM_UP);
    char key[160];
    graphics_key(&w, 0, key);
    assert(strcmp(key, "PowerSuit/ShootingWhileHanging/ACD_UP/left") == 0);
    int frames = 0;
    while (w.samus.pose == MZM_POSE_SHOOTING_WHILE_HANGING && frames++ < 20) step(&w, 0);
    assert(w.samus.pose == MZM_POSE_AIMING_WHILE_HANGING);
    /* Aiming: Up and Down, the diagonals with the outward direction. */
    step(&w, MZM_KEY_DOWN);
    assert(w.samus.aim == MZM_AIM_DOWN);
    step(&w, MZM_KEY_DOWN | MZM_KEY_LEFT);
    assert(w.samus.aim == MZM_AIM_DIAGONAL_DOWN);
    step(&w, MZM_KEY_LEFT);
    assert(w.samus.aim == MZM_AIM_FORWARD);
    step_input(&w, MZM_KEY_LEFT | MZM_KEY_B, true, 0);
    assert(w.samus.pose == MZM_POSE_SHOOTING_WHILE_HANGING && w.samus.aim == MZM_AIM_FORWARD);
    graphics_key(&w, MZM_WH_MISSILE, key);
    assert(strcmp(key, "PowerSuit/ShootingWhileHanging/ACD_FORWARD/left") == 0);
    /* Down held: the first shot aims down. */
    hang(&w);
    step_input(&w, MZM_KEY_DOWN | MZM_KEY_B, true, 0);
    assert(w.samus.pose == MZM_POSE_SHOOTING_WHILE_HANGING && w.samus.aim == MZM_AIM_DOWN);
    /* A charging beam turns to aim while hanging. */
    hang(&w);
    step_input(&w, MZM_KEY_B, false, 10);
    assert(w.samus.pose == MZM_POSE_AIMING_WHILE_HANGING && w.samus.aim == MZM_AIM_UP);
    /* A jumps away from the ledge, spinning when holding outward. */
    step(&w, MZM_KEY_LEFT | MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_STARTING_SPIN_JUMP || w.samus.pose == MZM_POSE_SPINNING);
}

/* Reported bug 1: the native corner rules of the probes. */
static void test_corners(void) {
    World w;
    /* A ceiling block whose edge only covers Samus's leftmost head probe:
     * SamusCheckTopCollision moves her out of it and she keeps rising. */
    world_init(&w);
    fill(&w.grid, 3, 9, 1, 1);
    place(&w, 64 + 8, 224);          /* left probe at x = 72 - 7 = 65: in column 4 */
    w.samus.x = 4 * 64 + 28 - 4;     /* the left probe 4 subpixels inside column 3 */
    step(&w, 0);
    step(&w, MZM_KEY_A);
    int32_t lowest = w.samus.y;
    int frames = 0;
    while (w.samus.pose == MZM_POSE_MIDAIR && w.samus.y_velocity > 0 && frames++ < 60) {
        step(&w, MZM_KEY_A);
        if (w.samus.y < lowest) lowest = w.samus.y;
    }
    /* Pushed right to clear the block's edge, then on up past its bottom. */
    assert(w.samus.x == 4 * 64 + 28);
    assert(lowest - 1 + mzm_block_hitbox(MZM_HITBOX_STANDING)->top < 10 * 64);
    /* A block covering a middle probe stops the rise under it. */
    world_init(&w);
    fill(&w.grid, 4, 9, 1, 1);
    place(&w, 64 + 8, 224);
    w.samus.x = 4 * 64 + 32;
    step(&w, 0);
    step(&w, MZM_KEY_A);
    frames = 0;
    while (w.samus.pose == MZM_POSE_MIDAIR && frames++ < 60) {
        step(&w, MZM_KEY_A);
        assert(w.samus.y - 1 + mzm_block_hitbox(MZM_HITBOX_STANDING)->top >= 10 * 64);
    }
    /* Running into a wall stops at the block edge minus the hitbox. */
    world_init(&w);
    fill(&w.grid, 10, 0, 1, GRID_H);
    place(&w, 100, 224);
    step(&w, 0);
    steps(&w, MZM_KEY_RIGHT, 80);
    assert(w.samus.x == 10 * 64 - 28 - 1 && w.samus.x_velocity == 0);
    assert(box_clear(&w));
    /* Walking off a ledge drops at the native halfway point of her block. */
    world_init(&w);
    memset(w.grid.solid, 0, sizeof w.grid.solid);
    fill(&w.grid, 0, GRID_H - 1, 5, 1);
    place(&w, 40, 224);
    step(&w, 0);
    frames = 0;
    while (w.samus.pose != MZM_POSE_MIDAIR && frames++ < 200) step(&w, MZM_KEY_RIGHT);
    assert(w.samus.pose == MZM_POSE_MIDAIR);
    assert(w.samus.x >= 5 * 64 + 28 && w.samus.x < 5 * 64 + 28 + 14);
}

/* The steep slopes: walked up and down with the native slope speeds. */
static void test_steep_slope(void) {
    World w;
    world_init(&w);
    /* A right steep slope rising from the floor (row 13) to row 12. */
    w.grid.slope[13][6] = 3;
    fill(&w.grid, 7, 13, 13, 1);
    place(&w, 64, 224);
    step(&w, 0);
    int frames = 0;
    while (w.samus.x < 7 * 64 + 32 && frames++ < 300) {
        step(&w, MZM_KEY_RIGHT);
        assert(w.samus.pose == MZM_POSE_RUNNING || w.samus.pose == MZM_POSE_STANDING);
    }
    /* Up the slope onto the higher floor, never airborne. */
    assert(w.samus.y == 13 * 64 && w.samus.current_slope == 0);
    /* On the slope the speed is three fifths going up. */
    world_init(&w);
    w.grid.slope[13][6] = 3;
    fill(&w.grid, 7, 13, 13, 1);
    place(&w, 6 * 16 + 4, 224);
    step(&w, 0);
    for (int i = 0; i < 30 && !(w.samus.current_slope & 1); ++i) step(&w, MZM_KEY_RIGHT);
    assert(w.samus.current_slope == (1 | (1u << 4)));
    w.samus.x_velocity = 80;
    int32_t x = w.samus.x;
    step(&w, MZM_KEY_RIGHT);
    assert(w.samus.x - x == ((80 + 8) * 3 / 5) >> 3);
}

/* Suitless crawling: the run into a low passage crouches to crawl. */
static void test_suitless(void) {
    World w;
    world_init(&w);
    w.equipment.suit = MZM_SUIT_SUITLESS;
    w.equipment.items = 0;
    place(&w, 64, 224);
    step(&w, 0);
    step(&w, MZM_KEY_DOWN);
    assert(w.samus.pose == MZM_POSE_CROUCHING_SUITLESS);
    char key[160];
    graphics_key(&w, 0, key);
    assert(strcmp(key, "Suitless/CrouchingSuitless/ACD_FORWARD/right") == 0);
    int frames = 0;
    while (w.samus.pose != MZM_POSE_CROUCHING && frames++ < 20) step(&w, 0);
    assert(w.samus.pose == MZM_POSE_CROUCHING);
    step(&w, MZM_KEY_UP);
    assert(w.samus.pose == MZM_POSE_UNCROUCHING_SUITLESS);
    /* The suitless jump. */
    frames = 0;
    while (w.samus.pose != MZM_POSE_STANDING && frames++ < 20) step(&w, 0);
    step(&w, MZM_KEY_A);
    assert(w.samus.pose == MZM_POSE_MIDAIR && w.samus.y_velocity == MZM_SUITLESS_JUMP_VELOCITY - 10);
}

/* ClipdataCheckCurrentAffectingAtPosition's hazards on the grid: water
 * below row 10, lava below row 12. */
static uint32_t grid_affecting(void *context, uint16_t y, uint16_t x) {
    (void)context;
    (void)x;
    if (y / 64 >= 12) return MZM_HAZARD_LAVA;
    return y / 64 >= 10 ? MZM_HAZARD_WATER : MZM_HAZARD_NONE;
}

/* SamusUpdatePhysics in water halves the speeds; SamusTakeHazardDamage. */
static void test_liquids(void) {
    World w;
    world_init(&w);                 /* no liquid */
    place(&w, 64, 224);
    step(&w, 0);
    steps(&w, MZM_KEY_RIGHT, 30);
    assert(!w.samus.slowed && w.samus.x_velocity == MZM_X_VELOCITY_CAP);
    world_init(&w);
    w.collision.affecting = grid_affecting;
    fill(&w.grid, 0, 11, GRID_W, 1);
    place(&w, 64, 176);             /* standing in the water (row 10) */
    step(&w, 0);
    steps(&w, MZM_KEY_RIGHT, 30);
    assert(w.samus.slowed && w.samus.x_velocity == MZM_X_VELOCITY_CAP / 2);
    /* Slowed jumps: the gravity halved. */
    step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    int16_t v = w.samus.y_velocity;
    step(&w, MZM_KEY_RIGHT | MZM_KEY_A);
    assert(v - w.samus.y_velocity == MZM_Y_ACCELERATION / 2);
    /* The Gravity Suit is not slowed. */
    w.equipment.items |= MZM_ITEM_GRAVITY_SUIT;
    steps(&w, 0, 40);
    assert(!w.samus.slowed);
    /* Lava: a point of energy every three frames without a suit, every five
     * with the Varia Suit. */
    world_init(&w);
    w.collision.affecting = grid_affecting;
    memset(w.grid.solid, 0, sizeof w.grid.solid);
    fill(&w.grid, 0, 13, GRID_W, 2);
    place(&w, 64, 13 * 16);
    step(&w, 0);
    int energy = w.equipment.energy;
    steps(&w, 0, 30);
    assert(w.equipment.energy == energy - 10);
    w.equipment.items |= MZM_ITEM_VARIA_SUIT;
    energy = w.equipment.energy;
    steps(&w, 0, 30);
    assert(w.equipment.energy == energy - 6);
    /* Down to zero: SamusChangeToHurtPose's dying branch. */
    w.equipment.energy = 1;
    steps(&w, 0, 6);
    assert(w.equipment.energy == 0 && w.samus.pose == MZM_POSE_DYING);
}

/* The pose table: every pose's hitbox and standing status, and the
 * requests outside it. */
static void test_pose_table(void) {
    assert(mzm_pose_standing(MZM_POSE_AIMING_WHILE_HANGING) == MZM_STANDING_HANGING);
    assert(mzm_pose_standing(MZM_POSE_PULLING_YOURSELF_UP_FROM_HANGING) == MZM_STANDING_FORCED);
    assert(mzm_pose_standing(MZM_POSE_PULLING_YOURSELF_FORWARD_FROM_HANGING) == MZM_STANDING_GROUND);
    assert(mzm_pose_standing(MZM_POSE_ON_ZIPLINE) == MZM_STANDING_NOT_IN_CONTROL);
    assert(mzm_pose_hitbox(MZM_POSE_CRAWLING) == MZM_HITBOX_MORPHED);
    assert(mzm_pose_hitbox(MZM_POSE_ON_ZIPLINE) == MZM_HITBOX_STANDING);
    assert(strcmp(mzm_pose_name(MZM_POSE_TURNING_TO_ENTER_ESCAPE_SHIP),
                  "SPOSE_TURNING_TO_ENTER_ESCAPE_SHIP") == 0);
    assert(MZM_POSE_COUNT == 67 && MZM_POSE_MID_AIR_REQUEST == 0xFE);
    /* SamusSetPose(SPOSE_KNOCKBACK_REQUEST) */
    World w;
    world_init(&w);
    place(&w, 64, 224);
    step(&w, 0);
    mzm_samus_knockback(&w.samus, &w.collision);
    assert(w.samus.pose == MZM_POSE_GETTING_KNOCKED_BACK && w.samus.y_velocity == 112);
    int frames = 0;
    while (w.samus.pose == MZM_POSE_GETTING_KNOCKED_BACK && frames++ < 60) step(&w, 0);
    assert(w.samus.pose == MZM_POSE_MIDAIR && frames > 13);
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
    test_projectiles_affect_blocks();
    test_firing_breaks_a_spin();
    test_sounds();
    test_bombs();
    test_speed_booster();
    test_shinespark();
    test_power_bomb();
    test_charge_beam();
    test_standing_on_sprite();
    test_wave_and_plasma();
    test_tumbling_missile();
    test_running_aim();
    test_ledge_climb_without_a();
    test_shooting_while_hanging();
    test_corners();
    test_steep_slope();
    test_suitless();
    test_pose_table();
    test_liquids();
    puts("MZM Samus controller tests passed");
    return 0;
}
