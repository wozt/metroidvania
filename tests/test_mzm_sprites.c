/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Zero Mission sprite engine and the Zoomer. */
#include "mzm_sfx.h"
#include "mzm_blocks.h"
#include "mzm_items.h"
#include "mzm_message_banner.h"
#include "mzm_projectiles.h"
#include "mzm_sprites.h"
#include "mzm_screen_shake.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { COLUMNS = 24, ROWS = 16 };
static uint8_t grid[ROWS][COLUMNS];

static uint8_t block(void *context, uint16_t y, uint16_t x) {
    (void)context;
    unsigned row = y / 64u, column = x / 64u;
    return row < ROWS && column < COLUMNS ? grid[row][column] : MZM_CLIPDATA_AIR;
}

/* Clipdata movement (zonelines, wall jump points) by block. */
static uint8_t movement_grid[ROWS][COLUMNS];

static uint8_t movement(void *context, uint16_t y, uint16_t x) {
    (void)context;
    unsigned row = y / 64u, column = x / 64u;
    return row < ROWS && column < COLUMNS ? movement_grid[row][column]
                                          : MZM_CLIPDATA_MOVEMENT_NONE;
}

static void clear(void) {
    /* A new room: RoomLoad's sScreenShake_Empty. */
    mzm_screen_shake_clear();
    memset(grid, 0, sizeof grid);
    memset(movement_grid, 0, sizeof movement_grid);
}

static MzmSpriteFrame frame;

static void step(MzmSprite *sprite, int frame8) {
    frame.frame8 = (uint8_t)frame8;
    mzm_sprite_update(sprite, 0, &frame);
}

static uint8_t test_events[32];
static int door_timer_calls, last_door_timer;

static bool test_check_event(void *context, uint8_t event) {
    (void)context;
    return test_events[event / 8] & (1u << (event % 8));
}

static void test_set_event(void *context, uint8_t event) {
    (void)context;
    test_events[event / 8] |= (uint8_t)(1u << (event % 8));
}

static void test_door_timer(void *context, int8_t timer) {
    (void)context;
    ++door_timer_calls;
    last_door_timer = timer;
}

static void test_remove_solid(void *context, uint16_t y, uint16_t x) {
    (void)context;
    if (y / 64u < ROWS && x / 64u < COLUMNS) grid[y / 64u][x / 64u] = MZM_CLIPDATA_AIR;
}

static int affects, last_affect_damage;

static void test_affect(void *context, uint16_t y, uint16_t x, int damage) {
    (void)context;
    (void)y;
    (void)x;
    ++affects;
    last_affect_damage = damage;
}

static int caa_calls, last_caa, messages[4], message_count;

static void test_apply_caa(void *context, uint16_t y, uint16_t x, uint8_t caa) {
    (void)context;
    (void)y;
    (void)x;
    ++caa_calls;
    last_caa = caa;
}

static void test_spawn_message(void *context, uint8_t message) {
    (void)context;
    if (message_count < 4) messages[message_count++] = message;
}

static const MzmSpriteHooks test_hooks = {NULL, test_check_event, test_set_event,
                                          test_door_timer, test_remove_solid, test_affect,
                                          test_apply_caa, test_spawn_message};

static int leeches;

static bool leech(void *context) {
    (void)context;
    ++leeches;
    return true;
}

static int knockbacks;
static bool last_knockback_left;

static void knockback(void *context, bool sprite_on_left) {
    (void)context;
    ++knockbacks;
    last_knockback_left = sprite_on_left;
}

static int hurts, last_damage;
static bool last_on_left;

static void hurt(void *context, uint16_t damage, bool sprite_on_left) {
    (void)context;
    ++hurts;
    last_damage = damage;
    last_on_left = sprite_on_left;
}

int main(void) {
    MzmSpriteWorld world = {NULL, block, 0, 0, movement, 0, NULL, 0};

    /* ClipdataConvertToCollision */
    clear();
    grid[1][1] = MZM_CLIPDATA_SOLID;
    grid[1][2] = MZM_CLIPDATA_LEFT_STEEP;
    grid[1][3] = MZM_CLIPDATA_ENEMY_ONLY;
    grid[1][4] = MZM_CLIPDATA_STOP_ENEMY;
    grid[1][5] = MZM_CLIPDATA_DOOR;
    assert(mzm_clipdata_process(&world, 64 + 5, 64 + 5, MZM_ACTOR_SPRITE) ==
           (MZM_CLIPDATA_SOLID | MZM_CLIPDATA_SOLID_FLAG));
    /* Left steep slope: solid where the subpixel y >= x. */
    assert(mzm_clipdata_process(&world, 64 + 40, 128 + 20, MZM_ACTOR_SPRITE) ==
           (MZM_CLIPDATA_LEFT_STEEP | MZM_CLIPDATA_SOLID_FLAG));
    assert(mzm_clipdata_process(&world, 64 + 10, 128 + 20, MZM_ACTOR_SPRITE) ==
           MZM_CLIPDATA_LEFT_STEEP);
    /* Enemy-only blocks stop Samus, stop-enemy blocks stop sprites. */
    assert(mzm_clipdata_process(&world, 70, 3 * 64 + 1, MZM_ACTOR_SPRITE) == MZM_CLIPDATA_AIR);
    assert(mzm_clipdata_process(&world, 70, 3 * 64 + 1, MZM_ACTOR_SAMUS) & MZM_CLIPDATA_SOLID_FLAG);
    assert(mzm_clipdata_process(&world, 70, 4 * 64 + 1, MZM_ACTOR_SPRITE) & MZM_CLIPDATA_SOLID_FLAG);
    assert(mzm_clipdata_process(&world, 70, 4 * 64 + 1, MZM_ACTOR_SAMUS) == MZM_CLIPDATA_AIR);
    assert(mzm_clipdata_process(&world, 70, 5 * 64 + 1, MZM_ACTOR_SAMUS) & MZM_CLIPDATA_SOLID_FLAG);
    assert(mzm_clipdata_process(&world, 9999, 9999, MZM_ACTOR_SPRITE) == MZM_CLIPDATA_AIR);

    static MzmSpriteTables tables;
    memset(tables.random, 0, sizeof tables.random);     /* rng & 1 = 0: facing right */
    const int16_t falling[16] = {4, 4, 4, 4, 8, 8, 8, 8, 12, 12, 12, 16, 16, 16, 20, 32767};
    memcpy(tables.falling, falling, sizeof falling);
    tables.health[MZM_PSPRITE_ZOOMER_YELLOW] = 3;
    tables.health[MZM_PSPRITE_ZOOMER_RED] = 8;
    static MzmSpriteKind kind, drop_kind;
    static MzmEquipment equipment = {.energy = 99, .max_energy = 99};
    static MzmSpriteKind ripper_kind, skree_kind, zeela_kind, waver_kind, hopper_kind, viola_kind, sova_kind, bug_kind, reo_kind, swoop_kind, rinka_kind, pirate_kind, security_kind;
    static MzmSpriteGlobals globals;
    frame = (MzmSpriteFrame){&world, &tables,
                             {&kind, &zeela_kind, &ripper_kind, &skree_kind, &waver_kind,
                              &drop_kind, &hopper_kind, &hopper_kind, &viola_kind, &sova_kind, &bug_kind, &bug_kind, &bug_kind, &reo_kind, &swoop_kind, &swoop_kind,
                              &swoop_kind, &rinka_kind, &pirate_kind, &security_kind},
                             &equipment, 0, 0, 0, 0, 0, NULL, 0, NULL, NULL, &globals, false, 0,
                             0, 0, NULL, false, NULL, NULL, false, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 1};
    for (int a = 0; a < MZM_ZOOMER_ANIMS; ++a) {
        kind.counts[a] = 2;
        kind.timers[a][0] = kind.timers[a][1] = 4;
    }

    /* A zoomer placed above a floor (row 10) starts crawling on it, facing
     * right, at 2 subpixels per frame. */
    clear();
    for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
    MzmSprite z;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 3, 0, 0);
    assert(z.y == 640 && z.x == 3 * 64 + 32);
    step(&z, 0);
    assert(z.status & MZM_SPRITE_STATUS_EXISTS && z.health == 3 && z.anim == MZM_ZOOMER_ON_GROUND);
    assert(z.status & MZM_SPRITE_STATUS_FACING_RIGHT && !(z.status & MZM_SPRITE_STATUS_FACING_DOWN));
    assert(z.hitbox_top == -52 && z.hitbox_bottom == 4 && z.hitbox_left == -28);
    uint16_t x0 = z.x;
    for (int i = 0; i < 10; ++i) step(&z, (uint8_t)i);
    assert(z.x == x0 + 20 && z.y == 640);

    /* The animation: 2 frames of 4 ticks, looping. */
    assert(z.anim == MZM_ZOOMER_ON_GROUND && z.frame <= 1);

    /* ScreenShakeUpdateVertical: an offset every third frame, up then down
     * (intensity bit 0) or up then nothing, halved in the last 15 frames;
     * only a longer shake replaces the current one. */
    {
        mzm_screen_shake_clear();
        assert(mzm_screen_shake_start_vertical(30, 1) == 30);
        assert(mzm_screen_shake_start_vertical(10, 0x81) == 30);
        int offsets[30];
        for (int i = 0; i < 30; ++i) offsets[i] = mzm_screen_shake_update_vertical();
        assert(offsets[0] == 0 && offsets[1] == 0 && offsets[2] == -2 && offsets[5] == 2 &&
               offsets[8] == -2 && offsets[11] == 2 && offsets[14] == -1 && offsets[17] == 1 &&
               offsets[29] == 1 && mzm_screen_shake.y.timer == 0);
        assert(mzm_screen_shake_update_vertical() == 0);
        mzm_screen_shake_start_horizontal(120, 0x80);
        for (int i = 0; i < 2; ++i) mzm_screen_shake_update_horizontal();
        assert(mzm_screen_shake_update_horizontal() == -2);
        for (int i = 0; i < 2; ++i) mzm_screen_shake_update_horizontal();
        assert(mzm_screen_shake_update_horizontal() == 0);
        mzm_screen_shake_clear();
    }
    /* SpriteUtilShouldFall: during a screen shake, a crawler on the ground
     * stops; one facing down (on a wall or ceiling) drops. */
    {
        MzmSprite shaken = z;
        mzm_screen_shake_start_vertical(10, 0x81);
        step(&shaken, 0);
        assert(shaken.x == z.x && shaken.pose == 9);
        shaken.status |= MZM_SPRITE_STATUS_FACING_DOWN;
        step(&shaken, 0);
        assert(shaken.pose == 0x1E);              /* ZOOMER_POSE_FALLING_INIT */
        mzm_screen_shake_clear();
    }

    /* A wall (column 8): the zoomer turns and climbs it, facing down along
     * the wall with X flip, its x on the wall's left edge. */
    for (int y = 0; y < 10; ++y) grid[y][8] = MZM_CLIPDATA_SOLID;
    int frames = 0;
    while (!(z.status & MZM_SPRITE_STATUS_FACING_DOWN) && frames < 500) {
        step(&z, (uint8_t)frames);
        ++frames;
    }
    assert(z.status & MZM_SPRITE_STATUS_FACING_DOWN && z.status & MZM_SPRITE_STATUS_X_FLIP);
    assert(z.x == 8 * 64 && z.anim == MZM_ZOOMER_ON_WALL);
    uint16_t y0 = z.y;
    step(&z, 0);
    assert(z.y == y0 - 2);

    /* With the wall gone, it falls (4, 4, 4, 4, 8...) back to the floor. */
    for (int y = 0; y < 10; ++y) grid[y][8] = MZM_CLIPDATA_AIR;
    step(&z, 0);
    assert(z.pose == 0x1E);     /* the next update starts the fall */
    step(&z, 0);
    assert(z.pose == 0x1F && z.anim == MZM_ZOOMER_FALLING);
    frames = 0;
    while (z.pose == 0x1F && frames < 200) {
        step(&z, 0);
        ++frames;
    }
    assert(z.pose == 0xF && z.y == 640);

    /* Nothing around it: the zoomer is removed at init. */
    clear();
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_RED, 0, 4, 4, 0, 0);
    step(&z, 0);
    assert(z.status == 0);

    /* A red zoomer under a ceiling hangs from it (Y flip) with palette 1 and
     * crawls at 3 subpixels per frame. */
    for (int x = 0; x < COLUMNS; ++x) grid[3][x] = MZM_CLIPDATA_SOLID;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_RED, 0, 4, 4, 0, 0);
    step(&z, 0);
    assert(z.status & MZM_SPRITE_STATUS_Y_FLIP && z.palette_row == 1 && z.y == 5 * 64 - 64);
    x0 = z.x;
    step(&z, 0);
    assert(z.x == x0 + 3);

    /* On screen within the camera window plus the draw distances. */
    world.camera_x = 0;
    world.camera_y = 0;
    step(&z, 0);
    assert(z.status & MZM_SPRITE_STATUS_ONSCREEN);
    world.camera_x = 4000;
    step(&z, 0);
    assert(!(z.status & MZM_SPRITE_STATUS_ONSCREEN));

    /* Combat. A yellow zoomer crawling on the floor, on screen. */
    tables.damage[MZM_PSPRITE_ZOOMER_YELLOW] = 8;
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] =
        MZM_WEAKNESS_BEAM_BOMBS | MZM_WEAKNESS_MISSILES | MZM_WEAKNESS_SPEEDBOOSTER_SCREW_ATTACK;
    clear();
    for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
    world.camera_x = world.camera_y = 0;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
    step(&z, 0);
    /* SpriteInitPrimary's one-frame ignore timer runs down in the collision
     * routine, not in the sprite's update. */
    assert(z.status & MZM_SPRITE_STATUS_ONSCREEN && z.ignore_samus_timer == 1);
    MzmSpriteSamus away = {NULL, 0, 0, 0, -96, 0, -24, 24, false, false, false, hurt, false, false, 0, NULL, NULL, 0, false, false, 1, true, false, false, MZM_POSE_STANDING, MZM_POSE_NONE, false, NULL, NULL, false, NULL};
    mzm_sprites_samus_collision(&z, 1, &away, &tables);
    assert(z.ignore_samus_timer == 0 && hurts == 0);

    /* Samus standing to its left, overlapping its hitbox: hurt once with
     * the stats damage, pushed away (the sprite is on her right, "on left"
     * in the native flag naming), then 15 frames of ignored contact. */
    MzmSpriteSamus samus = {NULL, (uint16_t)(z.x - 40), 640, (uint16_t)(z.x - 40),
                            -96, 0, -24, 24, false, false, false, hurt, false, false, 0, NULL, NULL, 0, false, false, 1, true, false, false, MZM_POSE_STANDING, MZM_POSE_NONE, false, NULL, NULL, false, NULL};
    mzm_sprites_samus_collision(&z, 1, &samus, &tables);
    assert(hurts == 1 && last_damage == 8 && last_on_left && samus.invincible);
    /* Set to 15, then run down once at the end of the routine. */
    assert(z.ignore_samus_timer == 14);
    mzm_sprites_samus_collision(&z, 1, &samus, &tables);
    assert(hurts == 1);
    /* While invincible, contact still sets the ignore timer but no damage. */
    z.ignore_samus_timer = 0;
    mzm_sprites_samus_collision(&z, 1, &samus, &tables);
    assert(hurts == 1 && z.ignore_samus_timer == 14);
    /* Only the first hurting contact of a frame counts: a second zoomer
     * behind the first keeps its timer. */
    MzmSprite pair[2] = {z, z};
    pair[0].ignore_samus_timer = pair[1].ignore_samus_timer = 0;
    samus.invincible = false;
    mzm_sprites_samus_collision(pair, 2, &samus, &tables);
    assert(hurts == 2 && pair[0].ignore_samus_timer == 14 && pair[1].ignore_samus_timer == 0);
    hurts = 1;
    /* Out of reach: nothing. */
    samus.invincible = false;
    z.ignore_samus_timer = 0;
    samus.x = (uint16_t)(z.x - 200);
    mzm_sprites_samus_collision(&z, 1, &samus, &tables);
    assert(hurts == 1 && z.ignore_samus_timer == 0);

    /* The screw attack destroys it on contact without hurting Samus; the
     * next update removes it (SpriteUtilSpriteDeath). */
    MzmSprite screwed = z;
    samus.x = samus.previous_x = (uint16_t)(z.x + 30);
    samus.screw_attacking = true;
    mzm_sprites_samus_collision(&screwed, 1, &samus, &tables);
    assert(hurts == 1 && screwed.health == 0 && screwed.pose == 0x65 &&
           (screwed.isft & 0x7F) == 17);
    step(&screwed, 0);
    assert(screwed.status == 0);
    /* Not weak to it: a short stun, then the normal hurt. */
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] &= (uint8_t)~MZM_WEAKNESS_SPEEDBOOSTER_SCREW_ATTACK;
    screwed = z;
    mzm_sprites_samus_collision(&screwed, 1, &samus, &tables);
    assert(hurts == 2 && !last_on_left && (screwed.isft & 0x7F) == 3 && screwed.health == 3);
    samus.screw_attacking = false;

    /* Beams: 2 damage, a 17-frame stun that freezes the AI, the beam is
     * consumed; the second shot kills (3 health). */
    static MzmWeapons weapons;
    memset(&weapons, 0, sizeof weapons);
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM,
                                      .x = z.x, .y = z.y - 20};
    weapons.list[1] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM,
                                      .x = z.x + 400, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, NULL);
    assert(z.health == 1 && (z.isft & 0x7F) == 17 && !weapons.list[0].active &&
           weapons.list[1].active);
    x0 = z.x;
    step(&z, 0);
    assert(z.x == x0);  /* stunned */
    for (int i = 0; i < 6; ++i) step(&z, 0);
    assert(z.x == x0 && (z.isft & 0x7F) == 10);
    step(&z, 0);
    assert(z.x == x0 + 2);  /* moving again below the stun threshold */
    weapons.list[1].x = z.x;
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, NULL);
    assert(z.health == 0 && z.pose == 0x62 && !weapons.list[1].active);
    step(&z, 0);
    assert(z.status == 0);

    /* Missiles deal 20; a sprite not weak to beams only flashes. */
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_RED, 0, 9, 6, 0, 0);
    step(&z, 0);
    tables.health[MZM_PSPRITE_ZOOMER_RED] = 30;
    z.health = 30;
    weapons.list[2] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM,
                                      .x = z.x, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, NULL);
    assert(z.health == 30 && (z.isft & 0x7F) == 3 && !weapons.list[2].active);
    tables.weakness[MZM_PSPRITE_ZOOMER_RED] = MZM_WEAKNESS_MISSILES;
    weapons.list[3] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_MISSILE,
                                      .x = z.x + 30, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, NULL);
    assert(z.health == 10 && (z.isft & 0x7F) == 17 && !weapons.list[3].active);
    /* The super missile's larger box (12 subpixels) reaches the hitbox
     * right edge (28) from 39 subpixels away; at 40 the strict test fails. */
    weapons.list[4] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_SUPER_MISSILE,
                                      .x = z.x + 40, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, NULL);
    assert(z.health == 10 && weapons.list[4].active);
    weapons.list[4] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_SUPER_MISSILE,
                                      .x = z.x + 39, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, NULL);
    assert(z.health == 0 && z.pose == 0x62);

    /* Drops. The yellow zoomer's ROM probabilities out of 1024: small energy
     * 300, large 20, missile 224, super missile 50, power bomb 50, stacked
     * from the top: power (974, 1024], super (924, 974], missile (700, 924],
     * large (680, 700], small (380, 680]. With a zero sprite RNG the roll is
     * frame8 + frame16. */
    const uint16_t probs[5] = {300, 20, 224, 50, 50};
    memcpy(tables.drops[MZM_PSPRITE_ZOOMER_YELLOW], probs, sizeof probs);
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = MZM_WEAKNESS_BEAM_BOMBS;
    for (int a = 0; a < MZM_DROP_ANIMS; ++a) {
        drop_kind.counts[a] = 2;
        drop_kind.timers[a][0] = drop_kind.timers[a][1] = 8;
    }
    struct { uint16_t roll; int energy, missiles, max_missiles; uint8_t drop; } rolls[] = {
        {500, 99, 0, 0, MZM_PSPRITE_SMALL_ENERGY_DROP},
        {500, 99, 3, 10, MZM_PSPRITE_MISSILE_DROP},     /* full life: missiles instead */
        {690, 50, 0, 0, MZM_PSPRITE_LARGE_ENERGY_DROP},
        {800, 50, 3, 10, MZM_PSPRITE_MISSILE_DROP},
        {800, 50, 10, 10, MZM_PSPRITE_LARGE_ENERGY_DROP}, /* full missiles, not full life */
        {800, 99, 0, 0, MZM_PSPRITE_SMALL_ENERGY_DROP},   /* no missiles yet, full life */
        {1000, 99, 0, 0, MZM_PSPRITE_SMALL_ENERGY_DROP},  /* power bombs not unlocked */
    };
    for (size_t r = 0; r < sizeof rolls / sizeof rolls[0]; ++r) {
        mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
        frame.frame16 = 0;
        step(&z, 0);
        uint16_t zx = z.x, zy = z.y;
        weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM,
                                          .x = z.x, .y = z.y - 20};
        z.health = 1;
        mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, NULL);
        assert(z.pose == 0x62);
        equipment.energy = rolls[r].energy;
        equipment.missiles = rolls[r].missiles;
        equipment.max_missiles = rolls[r].max_missiles;
        frame.frame16 = rolls[r].roll;
        step(&z, 0);
        if (rolls[r].drop == MZM_PSPRITE_UNUSED0) {
            assert(z.status == 0);
            continue;
        }
        /* The drop replaces the zoomer at the middle of its body, hidden. */
        assert(z.sprite_id == rolls[r].drop && z.pose == 0 && z.x == zx && z.y == zy - 40);
        assert(z.status & MZM_SPRITE_STATUS_NOT_DRAWN);
    }
    frame.frame16 = 0;

    /* The last drop (small energy, 99/99): init, 20 frames ignoring Samus,
     * shown when the ignore timer drops below 2. */
    MzmSprite drop = z;
    equipment.energy = 90;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
    z = drop;
    step(&z, 0);
    assert(z.pose == 9 && z.ignore_samus_timer == 20 && z.anim == MZM_DROP_SMALL_ENERGY);
    assert(z.status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES && z.hitbox_top == -36);
    /* Projectiles pass through drops. */
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM, .x = z.x, .y = z.y};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, NULL);
    assert(weapons.list[0].active);
    weapons.list[0].active = false;
    samus = (MzmSpriteSamus){NULL, z.x, (uint16_t)(z.y + 40), z.x, -96, 0, -24, 24,
                             false, false, false, hurt, false, false, 0, NULL, NULL, 0, false, false, 1, true, false, false, MZM_POSE_STANDING, MZM_POSE_NONE, false, NULL, NULL, false, NULL};
    for (int i = 0; i < 19; ++i) {
        mzm_sprites_samus_collision(&z, 1, &samus, &tables);
        step(&z, 0);
    }
    assert(z.ignore_samus_timer == 1 && !(z.status & MZM_SPRITE_STATUS_NOT_DRAWN));
    assert(!(z.status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) && equipment.energy == 90);
    /* Touched: collected (+5 energy) on the next update. */
    mzm_sprites_samus_collision(&z, 1, &samus, &tables);
    step(&z, 0);
    mzm_sprites_samus_collision(&z, 1, &samus, &tables);
    assert(z.status & MZM_SPRITE_STATUS_SAMUS_COLLIDING);
    mzm_sfx_clear();
    step(&z, 0);
    assert(z.status == 0 && equipment.energy == 95 && hurts == 2);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_GETTING_SMALL_ENERGY_DROP));

    /* Left alone it blinks during its last 80 timer ticks and disappears
     * after 200 ticks counted every other frame. */
    z = drop;
    step(&z, 0);
    int shown_changes = 0, frames_alive = 1;
    bool hidden = z.status & MZM_SPRITE_STATUS_NOT_DRAWN;
    while (z.status && frames_alive < 1000) {
        if (z.ignore_samus_timer) z.ignore_samus_timer--;  /* the collision routine */
        step(&z, 0);
        ++frames_alive;
        bool now = z.status & MZM_SPRITE_STATUS_NOT_DRAWN;
        if (z.status && now != hidden) ++shown_changes;
        hidden = now;
    }
    assert(z.status == 0 && frames_alive == 1 + 19 + 1 + 400 - 1);
    assert(shown_changes == 1 + 79);

    /* Rippers: a corridor between walls at columns 2 and 12 over a floor at
     * row 10. Init raises it an eighth of a block and picks a direction
     * from the RNG (0: X flip, moving right). */
    for (int a = 0; a < MZM_RIPPER_ANIMS; ++a) {
        ripper_kind.counts[a] = 3;
        ripper_kind.timers[a][0] = ripper_kind.timers[a][1] = ripper_kind.timers[a][2] = 2;
    }
    tables.health[MZM_PSPRITE_RIPPER_BROWN] = 20;
    tables.health[MZM_PSPRITE_RIPPER_PURPLE] = 40;
    tables.weakness[MZM_PSPRITE_RIPPER_BROWN] =
        MZM_WEAKNESS_SUPER_MISSILES | MZM_WEAKNESS_SPEEDBOOSTER_SCREW_ATTACK;
    clear();
    for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
    for (int y = 0; y < 10; ++y) grid[y][2] = grid[y][12] = MZM_CLIPDATA_SOLID;
    MzmSprite r;
    mzm_sprite_init_primary(&r, MZM_PSPRITE_RIPPER_BROWN, 0, 6, 7, 0, 0);
    step(&r, 0);
    assert(r.pose == 8 && r.health == 20 && r.y == 7 * 64 - 8 && r.hitbox_top == -36);
    assert(r.status & MZM_SPRITE_STATUS_X_FLIP && r.anim == MZM_RIPPER_MOVING);
    x0 = r.x;
    step(&r, 0);
    assert(r.pose == 9 && r.x == x0 + 2);
    /* It turns when the wall is 36 subpixels ahead: x + 36 reaches column
     * 12 (768) once x >= 732. */
    frames = 0;
    while (r.pose == 9 && frames < 500) {
        step(&r, 0);
        ++frames;
    }
    assert(r.pose == 0xA && r.x == 732);
    /* Turn: the X flip changes when the first table ends, then the second
     * table nearly ends and it moves left. */
    frames = 0;
    while (r.status & MZM_SPRITE_STATUS_X_FLIP && frames < 50) {
        step(&r, 0);
        ++frames;
    }
    assert(r.pose == 0xC && r.anim == MZM_RIPPER_TURNING_AROUND_PART2);
    frames = 0;
    while (r.pose != 9 && frames < 50) {
        step(&r, 0);
        ++frames;
    }
    x0 = r.x;
    step(&r, 0);
    assert(r.x == x0 - 2);
    /* Brown rippers only take super missiles: beams and missiles stun them
     * for 3 frames; the beam is consumed, the missile tumbles off it
     * (ProjectileStartTumblingMissile: away from the sprite's x, no
     * longer hitting sprites) with its thrust sound stopped. */
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM,
                                      .x = r.x, .y = r.y - 20};
    weapons.list[1] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_MISSILE,
                                      .x = r.x, .y = r.y - 20};
    weapons.list[1].x += 4;
    mzm_sfx_clear();
    mzm_sprites_projectile_hits(&r, 1, &weapons, &tables, NULL);
    assert(r.health == 20 && (r.isft & 0x7F) == 3 && !weapons.list[0].active &&
           weapons.list[1].active && weapons.list[1].stage == MZM_STAGE_TUMBLING &&
           weapons.list[1].x_flip && weapons.list[1].timer == 0 &&
           mzm_sfx_requested(MZM_SFX_STOP, MZM_SOUND_MISSILE_THRUST));
    r.isft = 0;
    mzm_sprites_projectile_hits(&r, 1, &weapons, &tables, NULL);
    assert(r.isft == 0);
    weapons.list[1].active = false;
    weapons.list[2] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_SUPER_MISSILE,
                                      .x = r.x, .y = r.y - 20};
    mzm_sprites_projectile_hits(&r, 1, &weapons, &tables, NULL);
    assert(r.health == 0 && r.pose == 0x62);
    /* Its death point is 24 subpixels above its feet (no drop: zero
     * probabilities). */
    uint16_t ry = r.y;
    tables.drops[MZM_PSPRITE_RIPPER_BROWN][MZM_DROP_PROB_SMALL_ENERGY] = 1024;
    equipment.energy = 50;
    step(&r, 0);
    assert(r.sprite_id == MZM_PSPRITE_SMALL_ENERGY_DROP && r.y == ry - 24);

    /* Purple rippers move a pixel per frame. */
    mzm_sprite_init_primary(&r, MZM_PSPRITE_RIPPER_PURPLE, 0, 6, 7, 0, 0);
    step(&r, 0);
    x0 = r.x;
    step(&r, 0);
    assert(r.health == 40 && r.x == x0 + 4);

    /* Skrees: hanging from a ceiling at row 2 above a floor at row 10. */
    for (int a = 0; a < MZM_SKREE_ANIMS; ++a) {
        skree_kind.counts[a] = 4;
        for (int f = 0; f < 4; ++f) skree_kind.timers[a][f] = 3;
    }
    const int16_t skree_falling[8] = {2, 4, 6, 8, 12, 14, 16, 32767};
    memcpy(tables.skree_falling, skree_falling, sizeof skree_falling);
    tables.health[MZM_PSPRITE_SKREE_GREEN] = 3;
    tables.secondary_damage[MZM_SSPRITE_SKREE_EXPLOSION] = 2;
    tables.drops[MZM_PSPRITE_SKREE_GREEN][MZM_DROP_PROB_SMALL_ENERGY] = 0;
    clear();
    for (int x = 0; x < COLUMNS; ++x) grid[2][x] = grid[10][x] = MZM_CLIPDATA_SOLID;
    static MzmSprite group[8];
    memset(group, 0, sizeof group);
    frame.sprites = group;
    frame.sprite_count = 8;
    world.camera_x = world.camera_y = 0;
    MzmSprite *k = &group[0];
    mzm_sprite_init_primary(k, MZM_PSPRITE_SKREE_GREEN, 0, 3, 8, 0, 0);
    /* Samus far to the right: it waits, one block above its placement. */
    frame.samus_x = 20 * 64;
    frame.samus_y = 640;
    step(k, 0);
    assert(k->pose == 9 && k->y == 3 * 64 && k->health == 3 && k->hitbox_bottom == 96);
    step(k, 0);
    assert(k->pose == 9);
    /* Within 150 subpixels to its left: it spins, then dives. */
    frame.samus_x = (uint16_t)(k->x - 149);
    step(k, 0);
    assert(k->pose == 0x22);
    frames = 0;
    while (k->pose != 0x35 && frames < 50) {
        step(k, 0);
        ++frames;
    }
    assert(k->pose == 0x35 && !(k->status & MZM_SPRITE_STATUS_FACING_RIGHT));
    /* The first frame falls 2 with no drift, then the drift grows by one
     * subpixel every four frames. */
    uint16_t kx = k->x;
    frames = 0;
    while (k->pose == 0x35 && frames < 200) {
        step(k, 0);
        ++frames;
    }
    /* Lands with its hitbox bottom (96) on the floor top (640). */
    assert(k->pose == 0x37 && k->y == 640 - 96 && k->x < kx);
    /* 60 frames later: four explosion parts, the skree removed. */
    for (int i = 0; i < 60; ++i) {
        for (int j = 0; j < 8; ++j) step(&group[j], 0);
    }
    assert(k->status == 0);
    int parts = 0;
    for (int j = 1; j < 8; ++j) {
        if (!(group[j].status & MZM_SPRITE_STATUS_EXISTS)) continue;
        ++parts;
        assert(group[j].secondary && group[j].sprite_id == MZM_SSPRITE_SKREE_EXPLOSION);
        assert(group[j].status & MZM_SPRITE_STATUS_ROTATION_SCALING_WHOLE);
        assert(mzm_sprite_graphics_owner(&group[j]) == MZM_PSPRITE_SKREE_GREEN);
    }
    assert(parts == 4);
    /* Slot 1 is the first going-up part (not flipped): it was updated in the
     * same frame (a later slot), moving left and up, rotating backwards. */
    MzmSprite *part = &group[1];
    assert(part->room_slot == 0 && !(part->status & MZM_SPRITE_STATUS_X_FLIP));
    assert(part->y == 640 - 96 - 8 + 40 - 4 && part->x == kx - (kx - k->x) - 8);
    assert(part->rotation == (uint16_t)-32);
    /* It hurts Samus with the secondary damage, but not the screw attack. */
    hurts = 0;
    MzmSpriteSamus near = {NULL, part->x, (uint16_t)(part->y + 20), part->x, -96, 0, -24, 24,
                           true, false, false, hurt, false, false, 0, NULL, NULL, 0, false, false, 1, true, false, false, MZM_POSE_STANDING, MZM_POSE_NONE, false, NULL, NULL, false, NULL};
    part->ignore_samus_timer = 0;
    mzm_sprites_samus_collision(group, 8, &near, &tables);
    assert(hurts == 1 && last_damage == 2);
    /* Projectiles ignore it. */
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM,
                                      .x = part->x, .y = part->y};
    mzm_sprites_projectile_hits(group, 8, &weapons, &tables, NULL);
    assert(weapons.list[0].active);
    weapons.list[0].active = false;
    /* After its second frame it is harmless; it ends with its table. */
    for (int i = 0; i < 7; ++i) step(part, 0);
    assert(part->frame > 1 && part->ignore_samus_timer == 1);
    frames = 0;
    while (part->status && frames < 50) {
        step(part, 0);
        ++frames;
    }
    assert(part->status == 0);
    /* Far off screen, parts are removed. */
    MzmSprite *far = &group[2];
    world.camera_x = 4000;
    step(far, 0);
    assert(far->status == 0);
    world.camera_x = 0;
    frame.sprites = NULL;
    frame.sprite_count = 0;

    /* Particles. ParticleSet takes the first free slot, then the particle
     * with the highest frame counter. */
    static MzmEffects fx;
    static MzmEffectKinds fx_kinds;
    memset(&fx, 0, sizeof fx);
    fx_kinds.counts[MZM_PE_SPRITE_EXPLOSION_MEDIUM] = 3;
    fx_kinds.counts[MZM_PE_HITTING_SOMETHING_WITH_NORMAL_BEAM] = 2;
    for (int f = 0; f < 3; ++f) {
        fx_kinds.timers[MZM_PE_SPRITE_EXPLOSION_MEDIUM][f] = 2;
        fx_kinds.timers[MZM_PE_HITTING_SOMETHING_WITH_NORMAL_BEAM][f] = 2;
    }
    fx_kinds.debris_count = 4;
    for (int f = 0; f < 4; ++f) fx_kinds.debris_timers[f] = 3;
    mzm_particle_set(&fx, 300, 400, MZM_PE_SPRITE_EXPLOSION_MEDIUM);
    MzmParticle *pe = &fx.particles[0];
    assert(pe->status == MZM_PARTICLE_EXISTS && pe->effect == MZM_PE_SPRITE_EXPLOSION_MEDIUM);
    /* First update: high OAM priority, on screen; a frame lasts until its
     * counter reaches the timer; the table wrapping removes it. */
    mzm_particles_process(&fx, &fx_kinds, 0, 0);
    assert(pe->status & MZM_PARTICLE_HIGH_OAM_PRIORITY && pe->status & MZM_PARTICLE_ONSCREEN);
    assert(pe->frame == 0 && pe->adc == 1);
    mzm_particles_process(&fx, &fx_kinds, 0, 0);
    assert(pe->frame == 1 && pe->adc == 0);
    for (int i = 0; i < 3; ++i) mzm_particles_process(&fx, &fx_kinds, 0, 0);
    assert(pe->status && pe->frame == 2);
    mzm_particles_process(&fx, &fx_kinds, 0, 0);
    assert(pe->status == 0);
    /* ParticleFreezingSpriteWithIce: high priority, drawn every other
     * frame. ParticleBeamTrailingRight / Left drift one subpixel sideways
     * and one pixel down a frame after their first. */
    fx_kinds.counts[MZM_PE_FREEZING_SPRITE_WITH_ICE] = 1;
    fx_kinds.timers[MZM_PE_FREEZING_SPRITE_WITH_ICE][0] = 8;
    fx_kinds.counts[MZM_PE_BEAM_TRAILING_LEFT] = 1;
    fx_kinds.timers[MZM_PE_BEAM_TRAILING_LEFT][0] = 8;
    mzm_particle_set(&fx, 300, 400, MZM_PE_FREEZING_SPRITE_WITH_ICE);
    mzm_particle_set(&fx, 300, 400, MZM_PE_BEAM_TRAILING_LEFT);
    mzm_particles_process(&fx, &fx_kinds, 0, 0);
    assert(fx.particles[0].status & MZM_PARTICLE_NOT_DRAWN &&
           fx.particles[0].status & MZM_PARTICLE_HIGH_OAM_PRIORITY);
    assert(fx.particles[1].x == 400 && fx.particles[1].y == 300);
    mzm_particles_process(&fx, &fx_kinds, 0, 0);
    assert(!(fx.particles[0].status & MZM_PARTICLE_NOT_DRAWN));
    assert(fx.particles[1].x == 399 && fx.particles[1].y == 304);
    /* The Wave, Plasma and full beam impacts play their sounds on their
     * first update (the full beams the ice impact). */
    memset(&fx, 0, sizeof fx);
    const uint8_t impacts[4] = {MZM_PE_HITTING_SOMETHING_WITH_WAVE_BEAM,
                                MZM_PE_HITTING_SOMETHING_WITH_PLASMA_BEAM,
                                MZM_PE_HITTING_SOMETHING_WITH_FULL_BEAM_NO_PLASMA,
                                MZM_PE_HITTING_SOMETHING_WITH_FULL_BEAM};
    const uint16_t impact_sounds[4] = {MZM_SOUND_WAVE_BEAM_IMPACT, MZM_SOUND_PLASMA_BEAM_IMPACT,
                                       MZM_SOUND_ICE_BEAM_IMPACT, MZM_SOUND_ICE_BEAM_IMPACT};
    for (int k = 0; k < 4; ++k) {
        fx_kinds.counts[impacts[k]] = 1;
        fx_kinds.timers[impacts[k]][0] = 4;
        mzm_sfx_clear();
        mzm_particle_set(&fx, 300, 400, impacts[k]);
        mzm_particles_process(&fx, &fx_kinds, 0, 0);
        assert(mzm_sfx_requested(MZM_SFX_PLAY, impact_sounds[k]));
        memset(&fx, 0, sizeof fx);
    }
    /* Off screen (the native window), removed after its update. */
    mzm_particle_set(&fx, 300, 3000, MZM_PE_HITTING_SOMETHING_WITH_NORMAL_BEAM);
    mzm_particles_process(&fx, &fx_kinds, 0, 0);
    assert(fx.particles[0].status == 0);
    /* Full: the oldest particle is replaced. */
    for (int i = 0; i < MZM_MAX_PARTICLES; ++i) {
        mzm_particle_set(&fx, 300, 400, MZM_PE_SPRITE_EXPLOSION_MEDIUM);
        fx.particles[i].frame_counter = (uint8_t)(i == 5 ? 9 : 1);
    }
    mzm_particle_set(&fx, 100, 100, MZM_PE_HITTING_SOMETHING_WITH_NORMAL_BEAM);
    assert(fx.particles[5].effect == MZM_PE_HITTING_SOMETHING_WITH_NORMAL_BEAM &&
           fx.particles[5].x == 100);
    memset(&fx, 0, sizeof fx);

    /* Debris type 0x11 arcs left with sDebris11YVelocity (each value twice)
     * onto the floor (row 10), then bounces with the bouncing table. */
    const int16_t debris_tables[MZM_DEBRIS_TABLES][9] = {
        {-4, -8, -4, 0, 4, 8, 12, 16, 20}, {-4, -8, -6, -4, 0, 4, 8, 8, 12},
        {-4, -8, -4, -2, 0, 4, 8, 12, 12}, {-6, -8, -10, -6, 0, 4, 8, 12, 16},
        {-5, -13, -7, -3, 0, 6, 10, 16, 20}, {4, 8, 12, 16, 20, 24, 28, 28, 28},
        {8, 10, 12, 14, 16, 18, 20, 22, 22}, {2, 4, 6, 8, 10, 12, 14, 16, 16},
        {8, 16, 20, 24, 28, 32, 36, 36, 36},
    };
    memcpy(tables.debris, debris_tables, sizeof debris_tables);
    clear();
    for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
    world.camera_x = world.camera_y = 0;
    mzm_sprite_debris_init(&fx, 0x11, 600, 400);
    MzmSpriteDebris *db = &fx.debris[0];
    mzm_sprite_debris_process(&fx, &world, &tables, &fx_kinds);
    assert(db->exists && db->y == 596 && db->x == 399 && db->frame_counter == 1);
    mzm_sprite_debris_process(&fx, &world, &tables, &fx_kinds);
    assert(db->y == 592 && db->x == 398);
    frames = 2;
    while (db->type == 0x11 && frames < 100) {
        mzm_sprite_debris_process(&fx, &world, &tables, &fx_kinds);
        ++frames;
    }
    assert(db->type == 0x81 && db->y == 640 && db->offset == 1);
    mzm_sprite_debris_process(&fx, &world, &tables, &fx_kinds);
    assert(db->y == 636);       /* the bounce starts upward */
    /* Animation: the counter starts at 0, so the first frame lasts timer + 1
     * updates and the next ones timer updates (it restarts at 1). */
    int calls = frames + 1;
    assert(db->frame == (unsigned)((calls < 4 ? 0 : 1 + (calls - 4) / 3) % 4));
    /* Delayed types wait: type 1 for 8 frames. */
    mzm_sprite_debris_init(&fx, 1, 600, 400);
    for (int i = 0; i < 8; ++i) mzm_sprite_debris_process(&fx, &world, &tables, &fx_kinds);
    assert(fx.debris[1].y == 600 && fx.debris[1].frame_counter == 8);
    mzm_sprite_debris_process(&fx, &world, &tables, &fx_kinds);
    assert(fx.debris[1].y == 596);
    /* Far below the camera: removed by the draw range check. */
    world.camera_y = 2000;
    mzm_sprite_debris_process(&fx, &world, &tables, &fx_kinds);
    assert(!fx.debris[0].exists && !fx.debris[1].exists);
    world.camera_y = 0;
    /* Eight slots at most. */
    for (int i = 0; i < 9; ++i) mzm_sprite_debris_init(&fx, 4, 600, 400);
    for (int i = 0; i < MZM_MAX_SPRITE_DEBRIS; ++i) assert(fx.debris[i].exists);
    memset(&fx, 0, sizeof fx);

    /* Deaths set their particle: a beam kill leaves the medium explosion at
     * the zoomer's body and the beam impact at the projectile. */
    frame.effects = &fx;
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] =
        MZM_WEAKNESS_BEAM_BOMBS | MZM_WEAKNESS_SPEEDBOOSTER_SCREW_ATTACK;
    memset(tables.drops[MZM_PSPRITE_ZOOMER_YELLOW], 0, sizeof tables.drops[0]);
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
    step(&z, 0);
    z.health = 1;
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM,
                                      .x = z.x + 4, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(fx.particles[0].effect == MZM_PE_HITTING_SOMETHING_WITH_NORMAL_BEAM &&
           fx.particles[0].x == z.x + 4 && fx.particles[0].y == z.y - 20);
    uint16_t zx2 = z.x, zy2 = z.y;
    assert(z.damaged);
    bool z_onscreen = (z.status & MZM_SPRITE_STATUS_ONSCREEN) != 0;
    assert(z_onscreen);
    mzm_sfx_clear();
    step(&z, 0);
    assert(z.status == 0 && fx.particles[1].effect == MZM_PE_SPRITE_EXPLOSION_MEDIUM &&
           fx.particles[1].x == zx2 && fx.particles[1].y == zy2 - 40);
    /* Zoomer: SP_DAMAGED first (on screen only), then SpriteUtilSpriteDeath. */
    assert(mzm_sfx_requested(MZM_SFX_PLAY_NOT_PLAYING, MZM_SOUND_ZOOMER_DAMAGED) == z_onscreen);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SPRITE_EXPLOSION_MEDIUM));
    /* ProjectileBombHitSprite: an exploding bomb (its sprite pass) deals 8,
     * leaves no particle and stays; outside its pass it does nothing. */
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = MZM_WEAKNESS_BEAM_BOMBS;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
    step(&z, 0);
    z.health = 20;
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BOMB,
                                      .x = z.x + 40, .y = z.y - 8,
                                      .bomb_stage = MZM_BOMB_EXPLODING, .hidden = true};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(z.health == 20);
    weapons.list[0].hits_sprites = true;
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(z.health == 12 && z.damaged && weapons.list[0].active &&
           !weapons.list[0].hits_sprites && !(fx.particles[2].status & MZM_PARTICLE_EXISTS));
    weapons.list[0].active = false;
    /* ProjectilePowerBombDealDamage: 50 once per power bomb (the stunned
     * flag), cleared when the next one is set. */
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = MZM_WEAKNESS_POWER_BOMB;
    z.health = 120;
    z.isft = 0;
    weapons.power_bomb = (MzmPowerBomb){.state = MZM_PB_EXPLODING, .x = z.x, .y = z.y,
                                        .hitbox_top = -40, .hitbox_bottom = 40,
                                        .hitbox_left = -80, .hitbox_right = 80};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(z.health == 70 && z.isft & 0x80 && (z.isft & 0x7F) == 17);
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(z.health == 70);
    weapons.power_bomb_set = true;
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(z.health == 20);
    weapons.power_bomb_set = false;
    weapons.power_bomb.state = MZM_PB_NONE;
    /* The charged beam (8) and charged Long Beam (12) hit sprites weak to
     * the charge beam or beams; the Long Beam deals 3 with its impact. */
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = MZM_WEAKNESS_CHARGE_BEAM_PISTOL;
    z.health = 40;
    z.isft = 0;
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_CHARGED_BEAM,
                                      .x = z.x, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, NULL);
    assert(z.health == 32 && !weapons.list[0].active);
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_CHARGED_LONG_BEAM,
                                      .x = z.x, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, NULL);
    assert(z.health == 20);
    /* Not weak to beams: the Long Beam only stuns. */
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_LONG_BEAM,
                                      .x = z.x, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, NULL);
    assert(z.health == 20 && !weapons.list[0].active);
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = MZM_WEAKNESS_BEAM_BOMBS;
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_LONG_BEAM,
                                      .x = z.x, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, NULL);
    assert(z.health == 17);
    z.isft = 0;
    /* Not weak: the invincible impact. */
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = 0;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
    step(&z, 0);
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM,
                                      .x = z.x, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(fx.particles[2].effect == MZM_PE_HITTING_SOMETHING_INVINCIBLE);
    /* ProjectileIceBeamHittingSprite: weak and freezable with frame8 & flag
     * clear: 3 damage, frozen for 240 with the frozen row, the ice impact,
     * then the freezing particle and SOUND_FREEZING_SPRITE. */
    memset(&fx, 0, sizeof fx);
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = MZM_WEAKNESS_BEAM_BOMBS | MZM_WEAKNESS_CAN_BE_FROZEN;
    tables.freeze[MZM_PSPRITE_ZOOMER_YELLOW] = 1;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
    step(&z, 0);
    z.health = 20;
    weapons.frame8 = 1;
    weapons.items = 0;
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_ICE_BEAM,
                                      .x = z.x, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    /* frame8 & flag set: a plain hit, no freeze. */
    assert(z.health == 17 && !z.freeze_timer && !weapons.list[0].active &&
           fx.particles[0].effect == MZM_PE_HITTING_SOMETHING_WITH_ICE_BEAM &&
           !(fx.particles[1].status & MZM_PARTICLE_EXISTS));
    z.isft = 0;
    mzm_sfx_clear();
    weapons.frame8 = 2;
    weapons.items = MZM_ITEM_LONG_BEAM;
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_ICE_BEAM,
                                      .x = z.x, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(z.health == 13 && z.freeze_timer == 240 && z.palette_row == 15 - z.gfx_slot);
    assert(fx.particles[2].effect == MZM_PE_FREEZING_SPRITE_WITH_ICE);
    assert(mzm_sfx_requested(MZM_SFX_PLAY_NOT_PLAYING, MZM_SOUND_FREEZING_SPRITE));
    /* SpriteUtilUpdateFreezeTimer: frozen in place, one tick every odd
     * frame, the palette flickers in the last 45 ticks. */
    uint16_t frozen_x = z.x;
    uint8_t frozen_pose = z.pose;
    for (int i = 0; i < 10; ++i) step(&z, i);
    assert(z.freeze_timer == 235 && z.x == frozen_x && z.pose == frozen_pose);
    z.freeze_timer = 45;
    step(&z, 1);
    assert(z.freeze_timer == 44 && z.palette_row == z.absolute_palette_row);
    step(&z, 1);
    assert(z.freeze_timer == 43 && z.palette_row == 15 - z.gfx_slot);
    z.freeze_timer = 1;
    step(&z, 1);
    assert(z.freeze_timer == 0 && z.palette_row == z.absolute_palette_row);
    /* The charged Ice Beam (16 with the Long Beam) freezes regardless of
     * frame8; it kills here, which clears the freeze. */
    memset(&fx, 0, sizeof fx);
    z.isft = 0;
    z.health = 30;
    weapons.frame8 = 1;
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_CHARGED_ICE_BEAM,
                                      .x = z.x, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(z.health == 14 && z.freeze_timer == 240 &&
           fx.particles[1].effect == MZM_PE_FREEZING_SPRITE_WITH_CHARGED_ICE);
    z.isft = 0;
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_CHARGED_ICE_BEAM,
                                      .x = z.x, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(z.health == 0 && z.freeze_timer == 0 && z.palette_row == 0 &&
           z.pose == 0x62);
    /* Freezable but not weak: frozen with no damage; neither: the solid
     * hit and the invincible particle. */
    memset(&fx, 0, sizeof fx);
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = MZM_WEAKNESS_CAN_BE_FROZEN;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
    step(&z, 0);
    z.health = 20;
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_ICE_BEAM,
                                      .x = z.x, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(z.health == 20 && z.freeze_timer == 240);
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = 0;
    z.freeze_timer = 0;
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_ICE_BEAM,
                                      .x = z.x, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(!z.freeze_timer && fx.particles[2].effect == MZM_PE_HITTING_SOMETHING_INVINCIBLE);
    tables.freeze[MZM_PSPRITE_ZOOMER_YELLOW] = 0;
    weapons.frame8 = 0;
    weapons.items = 0;
    /* The Wave Beam: WAVE_LONG_BEAM_DAMAGE (4) and its impact; with the Ice
     * Beam an ice hit (WAVE_ICE_BEAM_DAMAGE) with the full beam particle. */
    memset(&fx, 0, sizeof fx);
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = MZM_WEAKNESS_BEAM_BOMBS | MZM_WEAKNESS_CAN_BE_FROZEN;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
    step(&z, 0);
    z.health = 40;
    weapons.items = MZM_ITEM_WAVE_BEAM | MZM_ITEM_LONG_BEAM;
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_WAVE_BEAM,
                                      .x = z.x, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(z.health == 36 && !weapons.list[0].active && !z.freeze_timer &&
           fx.particles[0].effect == MZM_PE_HITTING_SOMETHING_WITH_WAVE_BEAM);
    z.isft = 0;
    weapons.items = MZM_ITEM_WAVE_BEAM | MZM_ITEM_ICE_BEAM;
    weapons.frame8 = 0;
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_WAVE_BEAM,
                                      .x = z.x, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(z.health == 32 && z.freeze_timer == 240 &&
           fx.particles[1].effect == MZM_PE_HITTING_SOMETHING_WITH_FULL_BEAM_NO_PLASMA &&
           fx.particles[2].effect == MZM_PE_FREEZING_SPRITE_WITH_ICE);
    /* The Plasma Beam goes on after its hit (PLASMA_WAVE_BEAM_DAMAGE 4). */
    z.freeze_timer = 0;
    z.isft = 0;
    weapons.items = MZM_ITEM_PLASMA_BEAM | MZM_ITEM_WAVE_BEAM;
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_PLASMA_BEAM,
                                      .x = z.x, .y = z.y - 20, .wave_oam = true};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(z.health == 28 && weapons.list[0].active &&
           fx.particles[3].effect == MZM_PE_HITTING_SOMETHING_WITH_PLASMA_BEAM);
    /* The fully charged beam: CHARGED_PLASMA_WAVE_ICE_LONG_BEAM_DAMAGE (24),
     * always freezing, the full beam particle. */
    memset(&fx, 0, sizeof fx);
    z.isft = 0;
    z.health = 30;
    weapons.items = MZM_ITEM_PLASMA_BEAM | MZM_ITEM_WAVE_BEAM | MZM_ITEM_ICE_BEAM | MZM_ITEM_LONG_BEAM;
    weapons.frame8 = 1;
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_CHARGED_PLASMA_BEAM,
                                      .x = z.x, .y = z.y - 20, .wave_oam = true};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(z.health == 6 && z.freeze_timer == 240 && weapons.list[0].active &&
           fx.particles[0].effect == MZM_PE_HITTING_SOMETHING_WITH_FULL_BEAM &&
           fx.particles[1].effect == MZM_PE_FREEZING_SPRITE_WITH_CHARGED_ICE);
    weapons.list[0].active = false;
    weapons.items = 0;
    weapons.frame8 = 0;
    z.freeze_timer = 0;
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = MZM_WEAKNESS_BEAM_BOMBS;
    /* SpriteUtilUpdateStunTimer: the stun row when the decremented timer
     * is 12 or 4, the sprite's own row at 16, 8 and 0. */
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = MZM_WEAKNESS_BEAM_BOMBS;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
    step(&z, 0);
    z.health = 20;
    z.isft = 17;
    step(&z, 0);
    assert(z.isft == 16 && z.palette_row == z.absolute_palette_row);
    for (int i = 0; i < 4; ++i) step(&z, 0);
    assert(z.isft == 12 && z.palette_row == 14 - z.gfx_slot);
    for (int i = 0; i < 4; ++i) step(&z, 0);
    assert(z.isft == 8 && z.palette_row == z.absolute_palette_row);
    z.isft = 0;
    /* Frozen sprites are solid (SpriteUtilSamusAndSpriteCollision): a
     * falling Samus lands on top (y = top + 1) without being hurt. */
    clear();
    for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = MZM_WEAKNESS_BEAM_BOMBS;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
    step(&z, 0);
    z.freeze_timer = 200;
    z.ignore_samus_timer = 0;
    z.isft = 0;
    const uint16_t ztop = (uint16_t)(z.y + z.hitbox_top), zbottom = (uint16_t)(z.y + z.hitbox_bottom);
    const uint16_t zleft = (uint16_t)(z.x + z.hitbox_left);
    MzmSpriteSamus solid = {NULL, z.x, (uint16_t)(ztop + 8), z.x, -124, 0, -28, 28,
                            false, false, false, hurt, false, false, 0, NULL,
                            &world, -10, false, false, 1, true, false, false, MZM_POSE_STANDING, MZM_POSE_NONE, false, NULL, NULL, false, NULL};
    hurts = 0;
    mzm_sprites_samus_collision(&z, 1, &solid, &tables);
    assert(hurts == 0 && solid.y == ztop + 1 && z.status & MZM_SPRITE_STATUS_SAMUS_ON_TOP &&
           z.standing_on == MZM_STANDING_ON_SPRITE_ON);
    /* Rising: not set on top. */
    MzmSpriteSamus rising = solid;
    rising.y = (uint16_t)(ztop + 8);
    rising.y_velocity = 30;
    z.status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_ON_TOP;
    mzm_sprites_samus_collision(&z, 1, &rising, &tables);
    assert(rising.y == ztop + 8 && !(z.status & MZM_SPRITE_STATUS_SAMUS_ON_TOP));
    z.status |= MZM_SPRITE_STATUS_SAMUS_ON_TOP;
    /* SpriteUtilSamusStandingOnSprite: a midair Samus gets the landing
     * request and STANDING_ENEMY; the flag is cleared every frame. */
    MzmSpriteStanding standing = {false, true, solid.y, solid.y, false, false};
    frame.standing = &standing;
    step(&z, 1);
    assert(standing.landing_request && standing.on_enemy && !standing.midair &&
           !(z.status & MZM_SPRITE_STATUS_SAMUS_ON_TOP));
    /* Off the top: releasing (Samus kept on its top when she did not rise
     * since her last pose change), then STANDING_MIDAIR. */
    standing.landing_request = false;
    standing.y = standing.copy_y = (uint16_t)(ztop + 3);
    step(&z, 1);
    assert(z.standing_on == MZM_STANDING_ON_SPRITE_RELEASING && standing.y_set &&
           standing.y == ztop + 1 && standing.on_enemy);
    standing.y_set = false;
    step(&z, 1);
    assert(z.standing_on == MZM_STANDING_ON_SPRITE_OFF && !standing.on_enemy && standing.midair &&
           !standing.y_set);
    frame.standing = NULL;
    /* Below it while rising: pushed under it, the upward velocity zeroed. */
    MzmSpriteSamus below = solid;
    below.y = (uint16_t)(zbottom + 124 - 8);
    below.y_velocity = 20;
    mzm_sprites_samus_collision(&z, 1, &below, &tables);
    assert(below.y == zbottom + 124 && below.stop_y && hurts == 0);
    /* Beside it: stopped at its left side, the velocity zeroed when facing
     * it; pulling up ignores it. */
    MzmSpriteSamus side = solid;
    side.y = (uint16_t)(zbottom - 14);
    side.x = side.previous_x = (uint16_t)(zleft - 20);
    MzmSpriteSamus pulling = side;
    pulling.pulling_up = true;
    mzm_sprites_samus_collision(&z, 1, &pulling, &tables);
    assert(pulling.x == zleft - 20 && !pulling.stop_x);
    mzm_sprites_samus_collision(&z, 1, &side, &tables);
    assert(side.x == zleft - 28 && side.stop_x && hurts == 0);
    /* A kill drops Samus: the screw attack on a frozen weak sprite. */
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = MZM_WEAKNESS_SPEEDBOOSTER_SCREW_ATTACK;
    z.standing_on = MZM_STANDING_ON_SPRITE_ON;
    MzmSpriteSamus screwing = solid;
    screwing.screw_attacking = true;
    mzm_sprites_samus_collision(&z, 1, &screwing, &tables);
    assert(z.pose == 0x65 && z.standing_on == MZM_STANDING_ON_SPRITE_DROPPED &&
           screwing.y == solid.y);
    assert(mzm_sprites_samus_dropped(&z, 1) && z.standing_on == MZM_STANDING_ON_SPRITE_OFF &&
           !mzm_sprites_samus_dropped(&z, 1));
    /* The screw attack kill: its particle and three debris (frame8 even:
     * types 2, 3 and 4). */
    memset(&fx, 0, sizeof fx);
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = MZM_WEAKNESS_SPEEDBOOSTER_SCREW_ATTACK;
    MzmSpriteSamus screw = {NULL, z.x, (uint16_t)(z.y + 20), z.x, -96, 0, -24, 24,
                            true, false, false, hurt, false, false, 0, NULL, NULL, 0, false, false, 1, true, false, false, MZM_POSE_STANDING, MZM_POSE_NONE, false, NULL, NULL, false, NULL};
    z.ignore_samus_timer = 0;
    mzm_sprites_samus_collision(&z, 1, &screw, &tables);
    assert(z.pose == 0x65 && z.damaged);
    mzm_sfx_clear();
    step(&z, 0);
    assert(z.status == 0 && fx.particles[0].effect == MZM_PE_SCREW_ATTACK_DESTROYED);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SCREW_ATTACK_DESTROYED));
    assert(!mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SPRITE_EXPLOSION_MEDIUM));
    /* Speed boosting (any pose) kills the same weak sprites as
     * DCT_SPEEDBOOSTER: its own particle and sound. */
    memset(&fx, 0, sizeof fx);
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
    step(&z, 0);
    MzmSpriteSamus speed = screw;
    speed.screw_attacking = false;
    speed.speedboosting = true;
    speed.x = z.x;
    speed.y = (uint16_t)(z.y + 20);
    z.ignore_samus_timer = 0;
    mzm_sprites_samus_collision(&z, 1, &speed, &tables);
    assert(z.pose == 0x64 && z.damaged);
    mzm_sfx_clear();
    step(&z, 0);
    assert(z.status == 0 && fx.particles[0].effect == MZM_PE_SPEEDBOOSTER_DESTROYED);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SPEEDBOOSTER_DESTROYED));
    /* The pseudo screw: a charged spin spends the charge on a weak sprite;
     * a kill is PSEUDO_SCREW_DESTROYED, a survivor still hurts Samus. */
    memset(&fx, 0, sizeof fx);
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
    step(&z, 0);
    /* Not weak to beams: nothing happens to the sprite or the charge. */
    uint8_t charge = 70;
    MzmSpriteSamus pseudo = speed;
    pseudo.speedboosting = pseudo.shinesparking = false;
    pseudo.pseudo_screw_damage = 8;
    pseudo.charge_counter = &charge;
    pseudo.x = z.x;
    pseudo.y = (uint16_t)(z.y + 20);
    z.ignore_samus_timer = 0;
    mzm_sprites_samus_collision(&z, 1, &pseudo, &tables);
    assert(charge == 70 && z.health == 3);
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] |= MZM_WEAKNESS_BEAM_BOMBS;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
    step(&z, 0);
    pseudo.speedboosting = pseudo.shinesparking = false;
    pseudo.pseudo_screw_damage = 8;
    pseudo.charge_counter = &charge;
    pseudo.x = z.x;
    pseudo.y = (uint16_t)(z.y + 20);
    z.health = 20;
    z.ignore_samus_timer = 0;
    pseudo.invincible = false;
    int hurts_before = hurts;
    mzm_sprites_samus_collision(&z, 1, &pseudo, &tables);
    assert(charge == 0 && z.health == 12 && hurts == hurts_before + 1);
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
    step(&z, 0);
    z.health = 8;
    z.ignore_samus_timer = 0;
    charge = 70;
    pseudo.invincible = false;
    hurts_before = hurts;
    mzm_sprites_samus_collision(&z, 1, &pseudo, &tables);
    assert(z.pose == 0x66 && hurts == hurts_before);
    mzm_sfx_clear();
    step(&z, 0);
    assert(z.status == 0 && fx.particles[0].effect == MZM_PE_PSEUDO_SCREW_DESTROYED &&
           mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_PSEUDO_SCREW_DESTROYED));
    /* Shinesparking: DCT_SHINESPARK. */
    memset(&fx, 0, sizeof fx);
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
    step(&z, 0);
    speed.shinesparking = true;
    speed.x = z.x;
    speed.y = (uint16_t)(z.y + 20);
    z.ignore_samus_timer = 0;
    mzm_sprites_samus_collision(&z, 1, &speed, &tables);
    assert(z.pose == 0x63);
    mzm_sfx_clear();
    step(&z, 0);
    assert(z.status == 0 && fx.particles[0].effect == MZM_PE_SHINESPARK_DESTROYED);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SHINESPARK_DESTROYED));
    assert(fx.debris[0].exists && fx.debris[0].type == 2 && fx.debris[1].type == 3 &&
           fx.debris[2].type == 4 && !fx.debris[3].exists);
    frame.effects = NULL;

    /* Zeelas crawl like Zoomers at one subpixel per frame; on the ground the
     * X flip follows the facing, and the hitbox has its own sizes. */
    for (int a = 0; a < MZM_ZEELA_ANIMS; ++a) {
        zeela_kind.counts[a] = 2;
        zeela_kind.timers[a][0] = zeela_kind.timers[a][1] = 4;
    }
    tables.health[MZM_PSPRITE_ZEELA] = 6;
    tables.health[MZM_PSPRITE_ZEELA_RED] = 12;
    const int16_t eyes_up[16] = {-4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 32767};
    const int16_t eyes_bottom[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 32767};
    memcpy(tables.zeela_eyes_up, eyes_up, sizeof eyes_up);
    memcpy(tables.zeela_eyes_bottom, eyes_bottom, sizeof eyes_bottom);
    tables.secondary_damage[MZM_SSPRITE_ZEELA_EYES] = 3;
    memset(tables.random, 0, sizeof tables.random);
    clear();
    for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
    memset(group, 0, sizeof group);
    frame.sprites = group;
    frame.sprite_count = 8;
    MzmSprite *zl = &group[0];
    mzm_sprite_init_primary(zl, MZM_PSPRITE_ZEELA, 0, 9, 6, 0, 0);
    step(zl, 0);
    assert(zl->health == 6 && zl->status & MZM_SPRITE_STATUS_FACING_RIGHT &&
           zl->status & MZM_SPRITE_STATUS_X_FLIP && zl->anim == MZM_ZEELA_ON_GROUND);
    assert(zl->hitbox_top == -56 && zl->hitbox_bottom == 4 && zl->hitbox_left == -24 &&
           zl->hitbox_right == 24);
    x0 = zl->x;
    for (int i = 0; i < 10; ++i) step(zl, 0);
    assert(zl->x == x0 + 10);
    /* A red Zeela (palette row 1) killed on the floor throws two eyes from
     * three quarters of a block above it, a part-2 pair moving 3 subpixels a
     * frame outward and up first. */
    mzm_sprite_init_primary(zl, MZM_PSPRITE_ZEELA_RED, 0, 9, 6, 0, 0);
    step(zl, 0);
    assert(zl->health == 12 && zl->palette_row == 1);
    uint16_t zlx = zl->x, zly = zl->y;
    zl->pose = 0x62;
    step(zl, 0);
    assert(zl->status == 0);
    MzmSprite *eye_l = &group[1], *eye_r = &group[2];
    assert(eye_l->secondary && eye_l->sprite_id == MZM_SSPRITE_ZEELA_EYES && eye_l->room_slot == 2);
    assert(mzm_sprite_graphics_owner(eye_l) == MZM_PSPRITE_ZEELA_RED);
    /* Their first update (in the game, the same frame, as later slots). */
    assert(eye_l->x == zlx - 24 && eye_r->x == zlx + 24 && eye_l->pose == 0);
    step(eye_l, 0);
    step(eye_r, 0);
    assert(eye_l->x == zlx - 24 - 3 && eye_r->x == zlx + 24 + 3);
    assert(eye_l->y == zly - 48 - 4 && eye_r->status & MZM_SPRITE_STATUS_X_FLIP);
    assert(eye_l->status & MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE && eye_l->rotation == 20);
    step(eye_l, 0);
    assert(eye_l->y == zly - 48 - 4 - 3 && eye_l->rotation == 40);
    /* Touching Samus: hurt with the eyes' damage, then stopped, exploding,
     * harmless, and gone when the table ends. */
    hurts = 0;
    MzmSpriteSamus touch = {NULL, eye_l->x, (uint16_t)(eye_l->y + 20), eye_l->x, -96, 0, -24, 24,
                            false, false, false, hurt, false, false, 0, NULL, NULL, 0, false, false, 1, true, false, false, MZM_POSE_STANDING, MZM_POSE_NONE, false, NULL, NULL, false, NULL};
    eye_l->ignore_samus_timer = 0;
    mzm_sprites_samus_collision(eye_l, 1, &touch, &tables);
    assert(hurts == 1 && last_damage == 3 && eye_l->pose == 66);
    step(eye_l, 0);
    assert(eye_l->pose == 0x43 && eye_l->anim == MZM_ZEELA_EYES_EXPLODING &&
           eye_l->samus_collision == 0);
    frames = 0;
    while (eye_l->status && frames < 50) {
        step(eye_l, 0);
        ++frames;
    }
    assert(eye_l->status == 0 && frames == 8);
    /* The other eye falls until it reaches the floor, then explodes. */
    frames = 0;
    while (eye_r->pose == 9 && frames < 200) {
        step(eye_r, 0);
        ++frames;
    }
    assert(eye_r->pose == 66 && eye_r->y >= 640);
    frame.sprites = NULL;
    frame.sprite_count = 0;

    /* Wavers start toward Samus and bounce between surfaces: one pixel a
     * frame across, half a pixel up or down. */
    waver_kind.counts[0] = 2;
    waver_kind.timers[0][0] = waver_kind.timers[0][1] = 4;
    tables.health[MZM_PSPRITE_WAVER] = 8;
    clear();
    for (int x = 0; x < COLUMNS; ++x) grid[2][x] = grid[10][x] = MZM_CLIPDATA_SOLID;
    for (int y = 0; y < ROWS; ++y) grid[y][2] = grid[y][12] = MZM_CLIPDATA_SOLID;
    MzmSprite w;
    mzm_sprite_init_primary(&w, MZM_PSPRITE_WAVER, 0, 5, 7, 0, 0);
    frame.samus_x = 0;
    step(&w, 0);
    assert(w.pose == 9 && w.health == 8 && !(w.status & MZM_SPRITE_STATUS_X_FLIP));
    x0 = w.x;
    uint16_t wy = w.y;
    step(&w, 0);
    assert(w.x == x0 - 4 && w.y == wy + 2);
    /* The left wall (column 2 ends at 192) turns it when its hitbox left
     * edge (32 subpixels) touches solid. */
    frames = 0;
    while (!(w.status & MZM_SPRITE_STATUS_X_FLIP) && frames < 300) {
        step(&w, 0);
        ++frames;
    }
    assert(w.x - 32 < 192 && w.x - 32 + 4 >= 192);
    /* The floor (row 10 at 640) turns it up (FACING_DOWN). */
    frames = 0;
    while (!(w.status & MZM_SPRITE_STATUS_FACING_DOWN) && frames < 500) {
        step(&w, 0);
        ++frames;
    }
    assert(w.y + 32 >= 640 && w.y + 32 - 2 < 640);
    wy = w.y;
    step(&w, 0);
    assert(w.y == wy - 2);

    /* Sidehoppers and Dessgeegas, on a floor at row 10 between walls at
     * columns 1 and 20, Samus far away. */
    for (int a = 0; a < MZM_SIDEHOPPER_ANIMS; ++a) {
        hopper_kind.counts[a] = 2;
        hopper_kind.timers[a][0] = hopper_kind.timers[a][1] = 2;
    }
    const int16_t high[10] = {-8, -16, -16, -8, -4, 0, 4, 8, 16, 16};
    const int16_t low[10] = {-4, -8, -4, -2, 0, 4, 8, 16, 16, 16};
    const int16_t ceiling_fall[16] = {-4, -4, -4, -4, -4, -4, -8, -8, -8, -12, -12, -12, -12,
                                      -12, -16, 32767};
    memcpy(tables.sidehopper_high, high, sizeof high);
    memcpy(tables.sidehopper_low, low, sizeof low);
    memcpy(tables.dessgeega_high, high, sizeof high);
    memcpy(tables.dessgeega_low, low, sizeof low);
    memcpy(tables.falling_ceiling, ceiling_fall, sizeof ceiling_fall);
    tables.health[MZM_PSPRITE_SIDEHOPPER] = 40;
    tables.health[MZM_PSPRITE_DESSGEEGA] = 30;
    tables.health[MZM_PSPRITE_DESSGEEGA_AFTER_LONG_BEAM] = 30;
    memset(tables.random, 0, sizeof tables.random);
    clear();
    for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
    for (int y = 0; y < ROWS; ++y) grid[y][1] = grid[y][20] = MZM_CLIPDATA_SOLID;
    frame.samus_x = 60000;
    frame.samus_y = 0;
    frame.samus_hitbox_top = -128;
    MzmSprite h;
    mzm_sprite_init_primary(&h, MZM_PSPRITE_SIDEHOPPER, 0, 9, 8, 0, 0);
    step(&h, 0);
    /* Init: idle on the floor, head hitbox 92, sides 72, facing right
     * (RNG 0); without BUGFIX work1 keeps 0, so it jumps after one loop. */
    assert(h.pose == 0xF && h.health == 40 && h.hitbox_top == -92 && h.hitbox_bottom == 0);
    assert(h.hitbox_left == -72 && h.hitbox_right == 72 && h.work1 == 0);
    assert(h.status & MZM_SPRITE_STATUS_FACING_RIGHT && h.anim == MZM_SIDEHOPPER_IDLE);
    frames = 0;
    while (h.pose != 0x23 && frames < 100) {
        step(&h, 0);
        ++frames;
    }
    assert(h.pose == 0x23 && h.hitbox_top == -0x70 && h.work2 == 0);   /* high jump */
    /* High jump: 20 frames up 208 subpixels and 4 level frames at 5
     * subpixels across, then down at 4: it lands back after 42 frames,
     * 192 subpixels further, its head hitbox back to 92. */
    uint16_t hx = h.x, hy = h.y, peak = h.y;
    frames = 0;
    while (h.pose == 0x23 && frames < 100) {
        step(&h, 0);
        ++frames;
        if (h.y < peak) peak = h.y;
    }
    assert(frames == 42 && hy == 640 && peak == 640 - 208);
    assert(h.pose == 0x25 && h.y == 640 && h.x == hx + 192 && h.hitbox_top == -92);
    /* The landing table ends: idle again. */
    frames = 0;
    while (h.pose == 0x25 && frames < 50) {
        step(&h, 0);
        ++frames;
    }
    assert(h.pose == 0xF);
    /* Samus within five blocks: it faces her and warns at once. */
    frame.samus_x = (uint16_t)(h.x - 200);
    frame.samus_y = (uint16_t)(h.y + 64);
    step(&h, 0);
    assert(h.pose == 9 && !(h.status & MZM_SPRITE_STATUS_FACING_RIGHT));
    frame.samus_x = 60000;
    /* Against the left wall (column 1, probed 76 subpixels to its left) it
     * is pushed back 6 subpixels and turns (facing right again). */
    h.x = 2 * 64 + 72;
    h.pose = 0x23;
    h.work3 = 0;
    h.work2 = 0;
    step(&h, 0);
    assert(h.status & MZM_SPRITE_STATUS_FACING_RIGHT && h.x == 2 * 64 + 72 + 6);
    /* Unsupported on both sides: it falls (the floor removed). */
    mzm_sprite_init_primary(&h, MZM_PSPRITE_SIDEHOPPER, 0, 4, 8, 0, 0);
    step(&h, 0);
    assert(h.pose == 0xF);
    step(&h, 0);
    assert(h.pose == 0x1F && h.anim == MZM_SIDEHOPPER_JUMPING);
    frames = 0;
    while (h.pose == 0x1F && frames < 200) {
        step(&h, 0);
        ++frames;
    }
    assert(h.pose == 0x25 && h.y == 640);
    /* On a ceiling (row 3): Y flipped, head hitbox below. */
    for (int x = 0; x < COLUMNS; ++x) grid[3][x] = MZM_CLIPDATA_SOLID;
    mzm_sprite_init_primary(&h, MZM_PSPRITE_SIDEHOPPER, 0, 4, 8, 0, 0);
    step(&h, 0);
    assert(h.status & MZM_SPRITE_STATUS_Y_FLIP && h.y == 4 * 64 && h.hitbox_top == 0 &&
           h.hitbox_bottom == 92);
    for (int x = 0; x < COLUMNS; ++x) grid[3][x] = MZM_CLIPDATA_AIR;
    /* Death: the single-then-big explosion 52 subpixels above its feet. */
    memset(&fx, 0, sizeof fx);
    frame.effects = &fx;
    mzm_sprite_init_primary(&h, MZM_PSPRITE_SIDEHOPPER, 0, 9, 8, 0, 0);
    step(&h, 0);
    h.pose = 0x62;
    step(&h, 0);
    assert(fx.particles[0].effect == MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG &&
           fx.particles[0].y == 640 - 52);

    /* Dessgeega: head hitbox 88, sides 64; every ground Dessgeega death sets
     * the Long Beam event and unlocks the doors (the game's bug). */
    frame.hooks = &test_hooks;
    memset(test_events, 0, sizeof test_events);
    door_timer_calls = 0;
    MzmSprite d;
    mzm_sprite_init_primary(&d, MZM_PSPRITE_DESSGEEGA, 0, 9, 8, 0, 0);
    step(&d, 0);
    assert(d.pose == 0xF && d.hitbox_top == -88 && d.hitbox_left == -64 && d.health == 30);
    d.pose = 0x62;
    step(&d, 0);
    assert(test_check_event(NULL, MZM_EVENT_LONG_BEAM_DESSGEEGA_KILLED));
    assert(door_timer_calls == 1 && last_door_timer == -20);
    assert(fx.particles[1].effect == MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG &&
           fx.particles[1].y == 640 - 48);
    /* Its jump keeps the head hitbox. */
    mzm_sprite_init_primary(&d, MZM_PSPRITE_DESSGEEGA, 0, 9, 8, 0, 0);
    step(&d, 0);
    d.pose = 8;
    frames = 0;
    while (d.pose != 0x23 && frames < 50) {
        step(&d, 0);
        ++frames;
    }
    assert(d.pose == 0x23 && d.hitbox_top == -88);

    /* The Long Beam Dessgeega: removed unless the statue room was viewed
     * and it was not killed yet. */
    memset(test_events, 0, sizeof test_events);
    door_timer_calls = 0;
    mzm_sprite_init_primary(&d, MZM_PSPRITE_DESSGEEGA_AFTER_LONG_BEAM, 0, 2, 8, 0, 0);
    step(&d, 0);
    assert(d.status == 0 && door_timer_calls == 0);
    test_set_event(NULL, MZM_EVENT_VIEWED_STATUE_ROOM);
    mzm_sprite_init_primary(&d, MZM_PSPRITE_DESSGEEGA_AFTER_LONG_BEAM, 0, 2, 8, 0, 0);
    step(&d, 0);
    /* Doors locked, waiting hidden from Samus and projectiles. */
    assert(d.pose == 0x11 && door_timer_calls == 1 && last_door_timer == 1);
    step(&d, 0);
    assert(d.status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES && d.ignore_samus_timer == 1);
    /* Samus below within 12 blocks and 3.75 across, on screen: it drops
     * with four debris, at 24 subpixels a frame. */
    memset(&fx, 0, sizeof fx);
    world.camera_x = world.camera_y = 0;
    frame.samus_x = (uint16_t)(d.x + 200);
    frame.samus_y = 640;
    step(&d, 0);
    assert(d.pose == 0x33 && !(d.status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES));
    assert(fx.debris[0].type == 5 && fx.debris[1].type == 7 && fx.debris[2].type == 8 &&
           fx.debris[3].type == 6);
    uint16_t dy = d.y;
    step(&d, 0);
    assert(d.y == dy + 24);
    /* The floor loses its three blocks under it, then the one below (row
     * 13) stops it. */
    for (int x = 0; x < COLUMNS; ++x) grid[13][x] = MZM_CLIPDATA_SOLID;
    uint16_t dx = d.x;
    frames = 0;
    while (grid[10][dx / 64] == MZM_CLIPDATA_SOLID && frames < 100) {
        step(&d, 0);
        ++frames;
    }
    assert(grid[10][dx / 64 - 1] == MZM_CLIPDATA_AIR && grid[10][dx / 64 + 1] == MZM_CLIPDATA_AIR);
    assert(grid[10][dx / 64 + 2] == MZM_CLIPDATA_SOLID && d.work3 == 1);
    assert(fx.particles[0].effect == MZM_PE_TWO_MEDIUM_DUST &&
           fx.particles[1].effect == MZM_PE_MEDIUM_DUST && fx.debris[4].type == 0x11);
    frames = 0;
    while (d.pose == 0x33 && frames < 100) {
        step(&d, 0);
        ++frames;
    }
    assert(d.pose == 0x25 && d.y == 13 * 64);
    /* Once killed, it does not come back. */
    test_set_event(NULL, MZM_EVENT_LONG_BEAM_DESSGEEGA_KILLED);
    mzm_sprite_init_primary(&d, MZM_PSPRITE_DESSGEEGA_AFTER_LONG_BEAM, 0, 2, 8, 0, 0);
    step(&d, 0);
    assert(d.status == 0);
    frame.hooks = NULL;
    frame.effects = NULL;

    /* Violas crawl around a closed box (walls at columns 2 and 9, ceiling
     * row 3, floor row 10) along its floor, walls and ceiling. */
    viola_kind.counts[0] = 2;
    viola_kind.timers[0][0] = viola_kind.timers[0][1] = 4;
    tables.health[MZM_PSPRITE_VIOLA_BLUE] = 12;
    tables.health[MZM_PSPRITE_VIOLA_ORANGE] = 20;
    clear();
    for (int x = 2; x <= 9; ++x) grid[3][x] = grid[10][x] = MZM_CLIPDATA_SOLID;
    for (int y = 3; y <= 10; ++y) grid[y][2] = grid[y][9] = MZM_CLIPDATA_SOLID;
    MzmSprite v;
    mzm_sprite_init_primary(&v, MZM_PSPRITE_VIOLA_BLUE, 0, 9, 5, 0, 0);
    step(&v, 0);
    /* Half a block up, then an eighth down onto the floor: centre 24
     * subpixels above it. */
    assert(v.pose == 9 && v.health == 12 && v.y == 640 - 24 && v.work2 == 2 &&
           v.hitbox_top == -20 && v.status & MZM_SPRITE_STATUS_FACING_RIGHT);
    x0 = v.x;
    step(&v, 0);
    assert(v.x == x0 + 2 && v.y == 640 - 24);
    bool seen[0x28] = {false};
    uint16_t min_x = 0xFFFF, max_x = 0, min_y = 0xFFFF, max_y = 0;
    for (int i = 0; i < 3000; ++i) {
        step(&v, 0);
        seen[v.pose] = true;
        if (v.x < min_x) min_x = v.x;
        if (v.x > max_x) max_x = v.x;
        if (v.y < min_y) min_y = v.y;
        if (v.y > max_y) max_y = v.y;
    }
    /* It visits the four surfaces, never falls and stays inside the box
     * (centres 24 subpixels from each surface). */
    assert(seen[9] && seen[0x23] && seen[0x25] && seen[0x27] && !seen[0x1F]);
    assert(min_x == 3 * 64 + 24 && max_x == 9 * 64 - 24 && min_y == 4 * 64 + 24 &&
           max_y == 640 - 24);
    /* Orange Violas are twice as fast. */
    mzm_sprite_init_primary(&v, MZM_PSPRITE_VIOLA_ORANGE, 0, 9, 5, 0, 0);
    step(&v, 0);
    x0 = v.x;
    step(&v, 0);
    assert(v.health == 20 && v.work2 == 4 && v.x == x0 + 4);
    /* Without a surface it falls onto the floor below. */
    clear();
    for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
    mzm_sprite_init_primary(&v, MZM_PSPRITE_VIOLA_BLUE, 0, 9, 5, 0, 0);
    step(&v, 0);
    grid[10][5] = grid[10][4] = grid[10][6] = MZM_CLIPDATA_AIR;
    grid[12][5] = MZM_CLIPDATA_SOLID;
    step(&v, 0);
    assert(v.pose == 0x1E);
    frames = 0;
    while (v.pose != 9 && frames < 100) {
        step(&v, 0);
        ++frames;
    }
    assert(v.pose == 9 && v.y == 12 * 64 - 24);

    /* Sovas: crawlers whose speed follows the animation frame (0, 0, 1, 2,
     * 3, 1 subpixels; orange: one more when moving) and whose ceiling state
     * is work2, not a Y flip. */
    for (int a = 0; a < MZM_SOVA_ANIMS; ++a) {
        sova_kind.counts[a] = 6;
        for (int f = 0; f < 6; ++f) sova_kind.timers[a][f] = 1;
    }
    tables.health[MZM_PSPRITE_SOVA_PURPLE] = 4;
    tables.health[MZM_PSPRITE_SOVA_ORANGE] = 12;
    clear();
    for (int x = 2; x <= 12; ++x) grid[3][x] = grid[10][x] = MZM_CLIPDATA_SOLID;
    for (int y = 3; y <= 10; ++y) grid[y][2] = grid[y][12] = MZM_CLIPDATA_SOLID;
    MzmSprite so;
    mzm_sprite_init_primary(&so, MZM_PSPRITE_SOVA_PURPLE, 0, 9, 6, 0, 0);
    step(&so, 0);
    assert(so.pose == 9 && so.health == 4 && so.work2 == 0 && so.anim == MZM_SOVA_ON_GROUND);
    assert(so.status & MZM_SPRITE_STATUS_FACING_RIGHT && so.status & MZM_SPRITE_STATUS_X_FLIP);
    assert(so.hitbox_top == -52 && so.hitbox_bottom == 4 && so.hitbox_left == -28);
    /* Each frame moves by the speed of the frame the animation shows: with
     * one-tick timers the frames go 0, 1, 1, 2, 2, ... */
    int moved = 0;
    for (int i = 0; i < 24; ++i) {
        uint16_t before = so.x;
        uint8_t shown = so.frame;
        step(&so, 0);
        static const int speeds[6] = {0, 0, 1, 2, 3, 1};
        assert(so.x - before == speeds[shown]);
        moved += so.x - before;
    }
    assert(moved > 0);
    /* Around the closed box: floor, both walls and the ceiling (work2,
     * never Y flipped while crawling), without falling. */
    bool floor_seen = false, ceiling_seen = false, left_seen = false, right_seen = false;
    for (int i = 0; i < 20000 && !(floor_seen && ceiling_seen && left_seen && right_seen); ++i) {
        step(&so, 0);
        assert(so.pose != 0x1F && so.status);
        if (so.pose != 9) continue;
        assert(!(so.status & MZM_SPRITE_STATUS_Y_FLIP));
        if (so.status & MZM_SPRITE_STATUS_FACING_DOWN) {
            if (so.status & MZM_SPRITE_STATUS_X_FLIP) right_seen = true;
            else left_seen = true;
        } else if (so.work2) {
            ceiling_seen = true;
            assert(so.anim == MZM_SOVA_WALKING_ON_CEILING && so.hitbox_bottom == 52);
        } else {
            floor_seen = true;
        }
    }
    assert(floor_seen && ceiling_seen && left_seen && right_seen);
    /* Orange Sovas use palette row 1 and move one subpixel more. */
    mzm_sprite_init_primary(&so, MZM_PSPRITE_SOVA_ORANGE, 0, 9, 6, 0, 0);
    step(&so, 0);
    assert(so.palette_row == 1 && so.health == 12);
    step(&so, 0);
    step(&so, 0);
    uint16_t before = so.x;
    assert(so.frame == 2);
    step(&so, 0);
    assert(so.x - before == 2);

    /* Zebs and Geegas: pipe bugs. A Zeb in a pipe at block (9, 10) waits
     * hidden from Samus and projectiles. */
    for (int a = 0; a < MZM_PIPE_BUG_ANIMS; ++a) {
        bug_kind.counts[a] = 2;
        bug_kind.timers[a][0] = bug_kind.timers[a][1] = 4;
    }
    tables.health[MZM_PSPRITE_ZEB] = 2;
    tables.health[MZM_PSPRITE_GEEGA_LEADER] = 4;
    tables.health[MZM_PSPRITE_GEEGA_FOLLOWER] = 4;
    tables.weakness[MZM_PSPRITE_ZEB] = MZM_WEAKNESS_BEAM_BOMBS;
    clear();
    memset(group, 0, sizeof group);
    frame.sprites = group;
    frame.sprite_count = 8;
    frame.effects = NULL;
    frame.samus_hitbox_top = -128;
    MzmSprite *zb = &group[0];
    mzm_sprite_init_primary(zb, MZM_PSPRITE_ZEB, 0, 9, 10, 0, 0);
    frame.samus_x = 60000;
    frame.samus_y = 100;
    step(zb, 0);
    assert(zb->pose == 9 && zb->y == 640 - 32 && zb->x == 10 * 64 + 64 && zb->work1 == 0);
    assert(zb->status & MZM_SPRITE_STATUS_NOT_DRAWN &&
           zb->status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES && zb->samus_collision == 0);
    /* Samus beside it but too close horizontally (36 subpixels) or out of
     * the five-block range: nothing. */
    frame.samus_x = (uint16_t)(zb->x - 36);
    frame.samus_y = (uint16_t)(zb->y - 100);
    for (int i = 0; i < 5; ++i) step(zb, 0);
    assert(zb->pose == 9);
    /* Above and to its left within range: it rises toward her, facing
     * left, harmful after two frames. */
    frame.samus_x = (uint16_t)(zb->x - 120);
    frame.samus_y = (uint16_t)(zb->y - 120);
    uint16_t zby = zb->y;
    step(zb, 0);
    assert(zb->pose == 0x23 && zb->scaling == frame.samus_y && zb->work0 == 2 &&
           !(zb->status & MZM_SPRITE_STATUS_NOT_DRAWN) && !(zb->status & MZM_SPRITE_STATUS_X_FLIP));
    step(zb, 0);
    assert(zb->y == zby - 8 && zb->samus_collision == 0);
    step(zb, 0);
    assert(zb->y == zby - 16 && zb->samus_collision == 4);
    /* It stops 100 subpixels above her saved height, waits 10 frames, then
     * flies left 12 subpixels a frame. */
    frames = 0;
    while (zb->pose == 0x23 && frames < 100) {
        step(zb, 0);
        ++frames;
    }
    assert(zb->pose == 0x25 && zb->y < frame.samus_y - 100 && zb->y + 8 >= frame.samus_y - 100);
    for (int i = 0; i < 10; ++i) step(zb, 0);
    uint16_t zbx = zb->x;
    step(zb, 0);
    assert(zb->x == zbx - 12 && zb->anim == MZM_PIPE_BUG_MOVING);
    /* Sixteen blocks past Samus it returns to its pipe and waits a second. */
    frames = 0;
    while (zb->pose == 0x25 && frames < 300) {
        step(zb, 0);
        ++frames;
    }
    assert(zb->pose == 9 && zb->y == zby && zb->work1 == 60 &&
           zb->status & MZM_SPRITE_STATUS_NOT_DRAWN);
    /* Killed: its drop follows its slot and the pipe waits while it
     * exists (DEATH_RESPAWNING). */
    tables.drops[MZM_PSPRITE_ZEB][MZM_DROP_PROB_SMALL_ENERGY] = 1024;
    equipment.energy = 50;
    zb->pose = 0x62;
    step(zb, 0);
    MzmSprite *zdrop = &group[1];
    assert(zb->pose == 9 && zb->health == 2 && zb->x == zb->x_spawn);
    assert(zdrop->status & MZM_SPRITE_STATUS_EXISTS &&
           zdrop->sprite_id == MZM_PSPRITE_SMALL_ENERGY_DROP && zdrop->ram_slot == 0);
    step(zdrop, 0);
    zb->work1 = 0;
    frame.samus_x = (uint16_t)(zb->x - 120);
    frame.samus_y = (uint16_t)(zb->y - 120);
    step(zb, 0);
    assert(zb->pose == 9);  /* has drops */
    zdrop->status = 0;
    step(zb, 0);
    assert(zb->pose == 0x23);

    /* A Geega leader rises with a follower two blocks and a half below it,
     * and waits in its pipe while the follower exists. */
    memset(group, 0, sizeof group);
    MzmSprite *gl = &group[0];
    mzm_sprite_init_primary(gl, MZM_PSPRITE_GEEGA_LEADER, 0, 9, 10, 0, 0);
    frame.samus_x = 60000;
    step(gl, 0);
    assert(gl->hitbox_top == -28 && gl->hitbox_left == -24);
    gl->work1 = 0;
    frame.samus_x = (uint16_t)(gl->x + 120);
    frame.samus_y = (uint16_t)(gl->y - 120);
    step(gl, 0);
    MzmSprite *gf = &group[1];
    assert(gl->pose == 0x23 && gl->status & MZM_SPRITE_STATUS_X_FLIP);
    assert(gf->sprite_id == MZM_PSPRITE_GEEGA_FOLLOWER && gf->ram_slot == 0 && gf->work0 == 18 &&
           gf->scaling == gl->scaling && gf->status & MZM_SPRITE_STATUS_X_FLIP);
    step(gf, 0);
    /* GeegaInit moved it under the leader, two blocks below. */
    assert(gf->pose == 0x23 && gf->x == gl->x && gf->y == gl->y + 128 &&
           !(gf->status & MZM_SPRITE_STATUS_NOT_DRAWN));
    /* The follower is removed when it would respawn; the leader waits for
     * it before rising again. */
    gl->pose = 9;
    gl->work1 = 0;
    step(gl, 0);
    assert(gl->pose == 9);
    gf->pose = 0x62;
    step(gf, 0);
    assert(gf->status == 0);
    step(gl, 0);
    assert(gl->pose == 0x23);

    /* A Zebbo leader spawns two followers (1.5 and 2.5 blocks below, 10 and
     * 18 frames before they turn harmful) and waits for both. */
    tables.health[MZM_PSPRITE_ZEBBO_GREEN_LEADER] = 6;
    tables.health[MZM_PSPRITE_ZEBBO_GREEN_FOLLOWER] = 6;
    memset(group, 0, sizeof group);
    MzmSprite *zl2 = &group[0];
    mzm_sprite_init_primary(zl2, MZM_PSPRITE_ZEBBO_GREEN_LEADER, 0, 9, 10, 0, 0);
    frame.samus_x = 60000;
    step(zl2, 0);
    assert(zl2->hitbox_top == -24 && zl2->hitbox_right == 24 && zl2->health == 6);
    frame.samus_x = (uint16_t)(zl2->x - 120);
    frame.samus_y = (uint16_t)(zl2->y - 120);
    step(zl2, 0);
    assert(zl2->pose == 0x23);
    assert(group[1].sprite_id == MZM_PSPRITE_ZEBBO_GREEN_FOLLOWER && group[1].work0 == 10 &&
           group[1].y == zl2->y + 96);
    assert(group[2].sprite_id == MZM_PSPRITE_ZEBBO_GREEN_FOLLOWER && group[2].work0 == 18 &&
           group[2].y == zl2->y + 160);
    zl2->pose = 9;
    zl2->work1 = 0;
    group[1].status = 0;
    step(zl2, 0);
    assert(zl2->pose == 9);     /* one follower left */
    group[2].status = 0;
    step(zl2, 0);
    assert(zl2->pose == 0x23);

    /* A blue Gamet leader (the pipe bug AI with its own hitbox and sounds)
     * spawns two followers like the Zebbo leader and waits for both. */
    frame.kinds[MZM_FAMILY_GAMET] = &bug_kind;
    tables.health[MZM_PSPRITE_GAMET_BLUE_LEADER] = 4;
    tables.health[MZM_PSPRITE_GAMET_BLUE_FOLLOWER] = 4;
    memset(group, 0, sizeof group);
    MzmSprite *ga = &group[0];
    mzm_sprite_init_primary(ga, MZM_PSPRITE_GAMET_BLUE_LEADER, 0, 9, 10, 0, 0);
    ga->status |= MZM_SPRITE_STATUS_ONSCREEN;
    frame.samus_x = 60000;
    step(ga, 0);
    assert(ga->hitbox_top == -28 && ga->hitbox_bottom == 28 && ga->hitbox_left == -24 &&
           ga->hitbox_right == 24 && ga->health == 4 && ga->status & MZM_SPRITE_STATUS_NOT_DRAWN);
    frame.samus_x = (uint16_t)(ga->x - 120);
    frame.samus_y = (uint16_t)(ga->y - 120);
    mzm_sfx_clear();
    step(ga, 0);
    assert(ga->pose == 0x23 && mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_GAMET_RISING));
    assert(group[1].sprite_id == MZM_PSPRITE_GAMET_BLUE_FOLLOWER && group[1].work0 == 10 &&
           group[1].y == ga->y + 96);
    assert(group[2].sprite_id == MZM_PSPRITE_GAMET_BLUE_FOLLOWER && group[2].work0 == 18 &&
           group[2].y == ga->y + 160);
    /* Up to Samus's height, then 3 pixels a frame toward her. */
    frames = 0;
    while (ga->pose == 0x23 && frames < 100) {
        step(ga, 0);
        ++frames;
    }
    assert(ga->pose == 0x25 && ga->anim == MZM_PIPE_BUG_MOVING && ga->samus_collision == 4);  /* SSC_HURTS_SAMUS */
    for (int i = 0; i < 10; ++i) step(ga, 0);
    mzm_sfx_clear();
    uint16_t gax = ga->x;
    step(ga, 0);
    assert(ga->x == gax - 12);
    ga->pose = 9;
    ga->work1 = 0;
    ga->x = ga->x_spawn;
    ga->y = ga->y_spawn;
    group[1].status = 0;
    step(ga, 0);
    assert(ga->pose == 9);      /* one follower left */
    group[2].status = 0;
    step(ga, 0);
    assert(ga->pose == 0x23);

    /* Reos hover with the ROM idle tables, then home on a point 72
     * subpixels above Samus's feet, accelerating up to their cap and
     * braking past it. */
    for (int a = 0; a < MZM_REO_ANIMS; ++a) {
        reo_kind.counts[a] = 2;
        reo_kind.timers[a][0] = reo_kind.timers[a][1] = 4;
    }
    const int16_t reo_y[65] = {0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 2, 2, 1, 1, 1, 0, 1,
                               0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, -1, 0, -1,
                               0, -1, -1, -1, -2, -2, -1, -1, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, 0,
                               0, 0, 32767};
    const int16_t reo_x[81] = {0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2,
                               2, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0,
                               0, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, -1, -1, -1, -1, -1, -2, -2,
                               -2, -2, -1, -1, -1, -1, -1, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, 0,
                               0, 0, 32767};
    memcpy(tables.reo_idle_y, reo_y, sizeof reo_y);
    memcpy(tables.reo_idle_x, reo_x, sizeof reo_x);
    tables.health[MZM_PSPRITE_REO_GREEN_WINGS] = 6;
    clear();
    memset(group, 0, sizeof group);
    frame.sprites = group;
    frame.sprite_count = 8;
    frame.samus_x = 60000;
    frame.samus_y = 60000;
    MzmSprite *re = &group[0];
    mzm_sprite_init_primary(re, MZM_PSPRITE_REO_GREEN_WINGS, 0, 5, 5, 0, 0);
    step(re, 0);
    assert(re->pose == 8 && re->health == 6 && re->hitbox_left == -56 && re->hitbox_top == -32);
    step(re, 0);
    assert(re->pose == 9 && re->work2 == 0 && re->work3 == 0);  /* RNG 0 */
    /* A full idle cycle of the Y table (64 frames) returns it to its
     * height (the table sums to zero). */
    uint16_t ry0 = re->y, rx0 = re->x;
    for (int i = 0; i < 64; ++i) step(re, 0);
    assert(re->y == ry0 && re->work3 == 64);
    for (int i = 0; i < 16; ++i) step(re, 0);
    assert(re->x == rx0);           /* the X table (80 entries) too */
    /* Samus within range, below-right: it turns toward her and dives. */
    frame.samus_x = (uint16_t)(re->x + 300);
    frame.samus_y = (uint16_t)(re->y + 400);
    frame.samus_hitbox_top = -128;
    step(re, 0);
    assert(re->pose == 0x22);       /* detected; the dive starts next frame */
    step(re, 0);
    assert(re->pose == 0x23 && re->status & MZM_SPRITE_STATUS_FACING_RIGHT &&
           re->status & MZM_SPRITE_STATUS_FACING_DOWN && re->anim == MZM_REO_MOVING);
    /* The speed grows by one unit a frame up to the green cap (24 across,
     * 16 down) and moves a quarter of it. */
    for (int i = 0; i < 40; ++i) step(re, 0);
    assert(re->work2 == 24 && re->work3 == 16);
    uint16_t rx = re->x, rey = re->y;
    step(re, 0);
    assert(re->x == rx + 6 && re->y == rey + 4);
    /* It passes the target point, brakes, turns, and keeps circling it. */
    bool turned = false;
    for (int i = 0; i < 400; ++i) {
        step(re, 0);
        if (!(re->status & MZM_SPRITE_STATUS_FACING_RIGHT)) turned = true;
    }
    assert(turned && re->pose == 0x23);
    assert(re->x > frame.samus_x - 400 && re->x < frame.samus_x + 400);
    /* A second Reo overlapping it is pushed away by a pixel. */
    MzmSprite *re2 = &group[1];
    *re2 = *re;
    re2->ram_slot = 1;
    re2->x = (uint16_t)(re->x + 10);
    re2->y = (uint16_t)(re->y + 10);
    uint16_t re2x = re2->x, re2y = re2->y;
    step(re, 0);
    assert(re2->x == re2x + 4 && re2->y == re2y + 4);

    /* Mellas and Holtzes swoop from a ceiling (row 2) to the floor (row 10)
     * and climb back. */
    for (int a = 0; a < MZM_HOLTZ_ANIMS; ++a) {
        swoop_kind.counts[a] = 4;
        for (int f = 0; f < 4; ++f) swoop_kind.timers[a][f] = 3;
    }
    const int16_t mella_idle_y[65] = {0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 2, 2, 1, 1, 1,
                                      0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1,
                                      0, -1, 0, -1, 0, -1, -1, -1, -2, -2, -1, -1, -1, 0, -1, 0,
                                      -1, 0, -1, 0, -1, 0, 0, 0, 0, 32767};
    const int16_t mella_idle_x[49] = {1, 0, 1, 0, 1, 1, 1, 2, 2, 1, 1, 1, 0, 1, 0, 1, 0, 1,
                                      0, 1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, -1, 0, -1,
                                      0, -1, -1, -1, -2, -2, -1, -1, -1, 0, -1, 0, -1, 32767};
    const int16_t mella_down[6] = {1, 2, 3, 4, 5, 6};
    const int16_t mella_up[7] = {0, 1, 2, 3, 4, 5, 6};
    const int16_t mella_x[7] = {0, 1, 2, 3, 4, 5, 6};
    memcpy(tables.mella_idle_y, mella_idle_y, sizeof mella_idle_y);
    memcpy(tables.mella_idle_x, mella_idle_x, sizeof mella_idle_x);
    memcpy(tables.mella_down, mella_down, sizeof mella_down);
    memcpy(tables.mella_up, mella_up, sizeof mella_up);
    memcpy(tables.mella_x, mella_x, sizeof mella_x);
    memcpy(tables.holtz_idle_y, mella_idle_y, sizeof mella_idle_y);
    tables.health[MZM_PSPRITE_MELLA] = 3;
    tables.health[MZM_PSPRITE_HOLTZ] = 10;
    tables.health[MZM_PSPRITE_MULTIVIOLA] = 10;
    clear();
    for (int x = 0; x < COLUMNS; ++x) grid[2][x] = grid[10][x] = MZM_CLIPDATA_SOLID;
    MzmSprite me;
    mzm_sprite_init_primary(&me, MZM_PSPRITE_MELLA, 0, 3, 10, 0, 0);
    frame.samus_x = 60000;
    frame.samus_y = 640;
    step(&me, 0);
    step(&me, 0);
    assert(me.pose == 9 && me.health == 3 && me.hitbox_left == -32);
    /* Samus low and within 3 blocks: a 7-frame delay, then down toward her
     * (the going down flag is SAMUS_COLLIDING). */
    frame.samus_x = (uint16_t)(me.x + 150);
    step(&me, 0);
    assert(me.pose == 0x32);
    step(&me, 0);
    assert(me.pose == 0x33 && me.anim == MZM_MELLA_MOVING && me.work0 == 6);
    for (int i = 0; i < 6; ++i) step(&me, 0);
    assert(me.pose == 0x35 && me.status & MZM_SPRITE_STATUS_SAMUS_COLLIDING &&
           me.status & MZM_SPRITE_STATUS_FACING_RIGHT);
    frames = 0;
    while (me.pose == 0x35 && frames < 300) {
        step(&me, 0);
        ++frames;
    }
    /* It stops when the floor is a block below it, then climbs back to
     * half a block under the ceiling. */
    assert(me.pose == 0x37 && me.y + 64 >= 640 && !(me.status & MZM_SPRITE_STATUS_SAMUS_COLLIDING));
    frames = 0;
    while (me.pose == 0x37 && frames < 300) {
        step(&me, 0);
        ++frames;
    }
    assert(me.pose == 8 && me.y == 3 * 64 + 32);

    MzmSprite ho;
    mzm_sprite_init_primary(&ho, MZM_PSPRITE_HOLTZ, 0, 3, 10, 0, 0);
    frame.samus_x = 60000;
    step(&ho, 0);
    step(&ho, 0);
    assert(ho.pose == 9 && ho.anim == MZM_HOLTZ_IDLE && ho.hitbox_left == -40);  /* Samus well below */
    frame.samus_x = (uint16_t)(ho.x + 250);
    step(&ho, 0);
    assert(ho.pose == 0x32);
    frames = 0;
    while (ho.pose != 0x35 && frames < 100) {
        step(&ho, 0);
        ++frames;
    }
    /* Down at 3 pixels a frame with the horizontal speed growing every
     * other frame from 2 to 12 subpixels. */
    assert(ho.work2 == 2 && ho.status & MZM_SPRITE_STATUS_SAMUS_COLLIDING);
    uint16_t hoy = ho.y;
    step(&ho, 0);
    assert(ho.y == hoy + 12);
    frames = 0;
    while (ho.pose == 0x35 && frames < 300) {
        step(&ho, 0);
        ++frames;
    }
    assert(ho.pose == 0x37);
    frames = 0;
    while (ho.pose != 0x3B && frames < 300) {
        step(&ho, 0);
        ++frames;
    }
    /* Back under its ceiling, 40 subpixels below the block edge. */
    assert(ho.pose == 0x3B && ho.y == 3 * 64 + 40 && ho.anim == MZM_HOLTZ_BONKING_ON_CEILING);
    frames = 0;
    while (ho.pose != 9 && frames < 100) {
        step(&ho, 0);
        ++frames;
    }
    assert(ho.pose == 9);

    /* Multiviolas bounce at 3 subpixels a frame, their animation starting
     * at twice their slot. */
    swoop_kind.counts[0] = 56;
    for (int f = 0; f < 56; ++f) swoop_kind.timers[0][f] = 2;
    MzmSprite mv;
    mzm_sprite_init_primary(&mv, MZM_PSPRITE_MULTIVIOLA, 0, 5, 5, 0, 7);
    frame.samus_x = 0;
    step(&mv, 0);
    assert(mv.pose == 9 && mv.frame == 14 && !(mv.status & MZM_SPRITE_STATUS_X_FLIP));
    uint16_t mvx = mv.x, mvy = mv.y;
    step(&mv, 0);
    assert(mv.x == mvx - 3 && mv.y == mvy + 3);
    frames = 0;
    while (!(mv.status & MZM_SPRITE_STATUS_FACING_DOWN) && frames < 300) {
        step(&mv, 0);
        ++frames;
    }
    assert(mv.y + 32 >= 640);   /* bounced off the floor */

    /* Gerutas wait under a ceiling (row 2), warn when Samus is low and
     * within 5 blocks, dive at 3 pixels a frame drifting a pixel toward her,
     * bounce off the floor (row 10) and climb back. */
    static MzmSpriteKind geruta_kind, ripper2_kind;
    for (int a = 0; a < MZM_GERUTA_ANIMS; ++a) {
        geruta_kind.counts[a] = 2;
        geruta_kind.timers[a][0] = geruta_kind.timers[a][1] = 3;
    }
    for (int a = 0; a < MZM_RIPPER2_ANIMS; ++a) {
        ripper2_kind.counts[a] = 2;
        ripper2_kind.timers[a][0] = ripper2_kind.timers[a][1] = 3;
    }
    frame.kinds[MZM_FAMILY_GERUTA] = &geruta_kind;
    frame.kinds[MZM_FAMILY_RIPPER2] = &ripper2_kind;
    tables.health[MZM_PSPRITE_GERUTA_RED] = 10;
    tables.health[MZM_PSPRITE_RIPPERII] = 30;
    clear();
    for (int x = 0; x < COLUMNS; ++x) grid[2][x] = grid[10][x] = MZM_CLIPDATA_SOLID;
    MzmSprite ge;
    mzm_sprite_init_primary(&ge, MZM_PSPRITE_GERUTA_RED, 0, 3, 10, 0, 0);
    frame.samus_x = 60000;
    frame.samus_y = 500;
    step(&ge, 0);
    assert(ge.pose == 8 && ge.y == 256 - 12 && ge.health == 10 && ge.hitbox_left == -48 &&
           ge.hitbox_top == -32);
    step(&ge, 0);
    assert(ge.pose == 9 && ge.anim == MZM_GERUTA_IDLE);
    /* Too high: Samus's feet less than 72 subpixels below it. */
    frame.samus_x = (uint16_t)(ge.x + 200);
    frame.samus_y = (uint16_t)(ge.y + 71);
    step(&ge, 0);
    assert(ge.pose == 9);
    frame.samus_y = 500;
    step(&ge, 0);
    assert(ge.pose == 0x32);
    step(&ge, 0);
    assert(ge.pose == 0x33 && ge.anim == MZM_GERUTA_WARNING);
    frames = 0;
    while (ge.pose != 0x37 && frames < 100) {
        step(&ge, 0);
        ++frames;
    }
    assert(ge.anim == MZM_GERUTA_GOING_DOWN && ge.status & MZM_SPRITE_STATUS_SAMUS_COLLIDING &&
           ge.status & MZM_SPRITE_STATUS_FACING_RIGHT);
    uint16_t gex = ge.x, gey = ge.y;
    step(&ge, 0);
    assert(ge.x == gex + 4 && ge.y == gey + 12);
    frames = 0;
    while (ge.pose == 0x37 && frames < 300) {
        step(&ge, 0);
        ++frames;
    }
    /* It bounces a block above the floor. */
    assert(ge.pose == 0x39 && ge.anim == MZM_GERUTA_BOUNCING && ge.y + 64 >= 640 && ge.y + 52 < 640);
    frames = 0;
    while (ge.pose != 0x3B && frames < 100) {
        step(&ge, 0);
        ++frames;
    }
    assert(!(ge.status & MZM_SPRITE_STATUS_SAMUS_COLLIDING));
    frames = 0;
    while (ge.pose == 0x3B && frames < 300) {
        step(&ge, 0);
        ++frames;
    }
    /* Back under its ceiling, 52 subpixels below the block edge. */
    assert(ge.pose == 0x3D && ge.y == 3 * 64 + 52 && ge.anim == MZM_GERUTA_BOUNCING_ON_CEILING);
    frames = 0;
    while (ge.pose != 9 && ge.pose != 0x32 && frames < 100) {
        step(&ge, 0);
        ++frames;
    }
    assert(ge.pose == 0x32);    /* Samus still in range */
    /* Without a ceiling, it attacks at once. */
    clear();
    for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
    MzmSprite ge2;
    mzm_sprite_init_primary(&ge2, MZM_PSPRITE_GERUTA_RED, 0, 3, 10, 0, 0);
    frame.samus_x = 60000;
    step(&ge2, 0);
    step(&ge2, 0);
    assert(ge2.pose == 0x32);

    /* Ripper IIs cross at two pixels a frame and turn at walls. */
    clear();
    for (int y = 0; y < ROWS; ++y) grid[y][2] = grid[y][12] = MZM_CLIPDATA_SOLID;
    MzmSprite r2;
    mzm_sprite_init_primary(&r2, MZM_PSPRITE_RIPPERII, 0, 5, 7, 0, 0);
    step(&r2, 0);
    assert(r2.pose == 8 && r2.y == 384 - 8 && r2.hitbox_right == 40 && r2.hitbox_bottom == 8 &&
           r2.status & MZM_SPRITE_STATUS_X_FLIP && r2.health == 30);
    uint16_t r2x = r2.x;
    step(&r2, 0);
    assert(r2.pose == 9 && r2.x == r2x + 8);
    frames = 0;
    while (r2.pose == 9 && frames < 200) {
        step(&r2, 0);
        ++frames;
    }
    /* The wall's edge is within 44 subpixels. */
    assert(r2.pose == 0xA && r2.x + 44 >= 12 * 64 && r2.x + 36 < 12 * 64);
    frames = 0;
    while (r2.pose != 9 && frames < 100) {
        step(&r2, 0);
        ++frames;
    }
    assert(!(r2.status & MZM_SPRITE_STATUS_X_FLIP));
    r2x = r2.x;
    step(&r2, 0);
    assert(r2.x == r2x - 8);

    /* Squeepts hide at their spawn until Samus is above them, jump with the
     * rising table (leaving the lava surface plays its sound), turn at its
     * end and fall back into hiding. */
    static MzmSpriteKind squeept_kind;
    for (int a = 0; a < MZM_SQUEEPT_ANIMS; ++a) {
        squeept_kind.counts[a] = 2;
        squeept_kind.timers[a][0] = squeept_kind.timers[a][1] = 3;
    }
    frame.kinds[MZM_FAMILY_SQUEEPT] = &squeept_kind;
    tables.health[MZM_PSPRITE_SQUEEPT] = 8;
    for (int i = 0; i < 64; ++i) tables.squeept_rising[i] = -8;
    tables.squeept_rising[64] = 32767;
    const int16_t squeept_falling[16] = {4, 4, 4, 6, 6, 8, 8, 10, 10, 12, 12, 14, 14, 16, 16, 32767};
    memcpy(tables.squeept_falling, squeept_falling, sizeof squeept_falling);
    clear();
    world.effect_y = 11 * 64;
    MzmSprite sq;
    mzm_sprite_init_primary(&sq, MZM_PSPRITE_SQUEEPT, 0, 12, 7, 0, 0);
    sq.status |= MZM_SPRITE_STATUS_ONSCREEN;
    frame.samus_x = 60000;
    frame.samus_y = 600;
    step(&sq, 0);
    /* SqueeptInit falls through SqueeptIdleInit into SqueeptIdle. */
    assert(sq.pose == 0xF && sq.y == 13 * 64 + 4 && sq.y_spawn == sq.y && sq.work0 == 29 &&
           sq.status & MZM_SPRITE_STATUS_NOT_DRAWN && sq.hitbox_top == -44);
    frame.samus_x = (uint16_t)(sq.x + 300);
    for (int i = 0; i < 29; ++i) step(&sq, 0);
    assert(sq.pose == 0xF && sq.work0 == 0);
    step(&sq, 0);
    assert(sq.pose == 0x35 && !(sq.status & MZM_SPRITE_STATUS_NOT_DRAWN));
    mzm_sfx_clear();
    uint16_t sqy = sq.y;
    frames = 0;
    while (sq.pose == 0x35 && frames < 200) {
        step(&sq, 0);
        ++frames;
    }
    /* 64 entries of 8 subpixels, then the turn. */
    assert(frames == 65 && sq.y == sqy - 64 * 8 && sq.hitbox_top == -16 &&
           mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SQUEEPT_LEAVING_LAVA));
    frames = 0;
    while (sq.pose == 0x37 && frames < 100) {
        step(&sq, 0);
        ++frames;
    }
    assert(sq.pose == 0x39 && sq.hitbox_top == 0 && sq.hitbox_bottom == 40);
    mzm_sfx_clear();
    frames = 0;
    while (sq.pose == 0x39 && frames < 300) {
        step(&sq, 0);
        ++frames;
    }
    /* Back at its spawn, hidden, after entering the lava. */
    assert(sq.pose == 0xF && sq.y == sq.y_spawn && sq.status & MZM_SPRITE_STATUS_NOT_DRAWN &&
           sq.work0 == 30 && mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_SQUEEPT_ENTERING_LAVA));
    /* A ceiling more than 2.5 blocks above its spawn turns it early. */
    for (int x = 0; x < COLUMNS; ++x) grid[7][x] = MZM_CLIPDATA_SOLID;
    for (int i = 0; i < 31; ++i) step(&sq, 0);
    assert(sq.pose == 0x35);
    frames = 0;
    while (sq.pose == 0x35 && frames < 200) {
        step(&sq, 0);
        ++frames;
    }
    assert(sq.pose == 0x37 && sq.y - 48 < 8 * 64 && sq.y - 48 + 8 >= 8 * 64);
    world.effect_y = 0;

    /* Rinkas. The first spawn is visible at once (SpriteInitPrimary sets
     * no NOT_DRAWN) and flies when its spawning table ends; respawns are
     * hidden, wait, show once on screen and turn vulnerable 11 frames
     * later. */
    for (int a = 0; a < MZM_RINKA_ANIMS; ++a) {
        rinka_kind.counts[a] = 2;
        rinka_kind.timers[a][0] = rinka_kind.timers[a][1] = 2;
    }
    tables.health[MZM_PSPRITE_RINKA_ORANGE] = 2;
    clear();
    world.camera_x = world.camera_y = 0;
    MzmSprite rk;
    mzm_sprite_init_primary(&rk, MZM_PSPRITE_RINKA_ORANGE, 0, 5, 5, 0, 0);
    frame.samus_x = 60000;
    frame.samus_y = 60000;
    step(&rk, 0);
    /* Init: half a block up, spawn block (5, 5), hitbox 20 subpixels. */
    assert(rk.pose == 9 && rk.y == 5 * 64 + 32 && rk.work3 == 5 && rk.work2 == 5 &&
           rk.hitbox_right == 20 && rk.anim == MZM_RINKA_ORANGE_SPAWNING);
    assert(!(rk.status & MZM_SPRITE_STATUS_NOT_DRAWN) &&
           !(rk.status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES));
    /* Samus to its right, her y 60 subpixels below its centre: a level
     * flight, 4, 8, 12... subpixels from the spawn point. */
    frame.samus_x = (uint16_t)(rk.x + 600);
    frame.samus_y = (uint16_t)(5 * 64 + 32 + 60);
    frames = 0;
    while (rk.pose != 0x23 && frames < 50) {
        step(&rk, 0);
        ++frames;
    }
    assert(rk.pose == 0x23 && rk.status & MZM_SPRITE_STATUS_FACING_RIGHT &&
           rk.anim == MZM_RINKA_ORANGE_MOVING);
    uint16_t spawn_x = 5 * 64 + 32;
    step(&rk, 0);
    assert(rk.x == spawn_x + 4 && rk.y == 5 * 64 + 32);
    step(&rk, 0);
    assert(rk.x == spawn_x + 8);
    step(&rk, 0);
    assert(rk.x == spawn_x + 12);
    /* Sixteen blocks past Samus it respawns, hidden and harmless, after 60
     * frames. */
    frames = 0;
    while (rk.pose == 0x23 && frames < 1000) {
        step(&rk, 0);
        ++frames;
    }
    assert(rk.pose == 9 && rk.x == spawn_x && rk.work1 == 60 &&
           rk.status & MZM_SPRITE_STATUS_NOT_DRAWN &&
           rk.status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
    for (int i = 0; i < 60; ++i) step(&rk, 0);
    assert(rk.work1 == 0 && rk.status & MZM_SPRITE_STATUS_NOT_DRAWN);
    step(&rk, 0);                   /* on screen: shown */
    assert(!(rk.status & MZM_SPRITE_STATUS_NOT_DRAWN) && rk.work0 == 11 &&
           rk.status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
    for (int i = 0; i < 11; ++i) step(&rk, 0);
    assert(!(rk.status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES));
    /* A diagonal flight follows the unit vector (BIOS Sqrt). */
    frame.samus_x = (uint16_t)(spawn_x + 300);
    frame.samus_y = (uint16_t)(5 * 64 + 32 + 60 + 400);
    rk.work1 = 0;
    frames = 0;
    while (rk.pose != 0x23 && frames < 50) {
        step(&rk, 0);
        ++frames;
    }
    step(&rk, 0);
    step(&rk, 0);
    /* velocity 8, unit (0.6, 0.8): 4 across, 6 down (10-bit truncation). */
    assert(rk.x == spawn_x + 4 && rk.y == 5 * 64 + 32 + 6);

    /* Space Pirates: one frame of the sprite array, then the end of
     * SpriteUpdate (alarm). */
    {
        static MzmSprite ps[8];
        const int16_t wall_jump[8] = {-2, -1, 0, 4, 8, 12, 16, 20};
        const int16_t jump_low[11] = {-8, -4, 0, 4, 8, 12, 16, 20, 20, 20, 20};
        const int16_t jump[10] = {-12, -8, -4, 0, 4, 8, 12, 16, 20, 20};
        const int16_t jump_high[11] = {-16, -12, -8, -4, -2, 0, 4, 8, 12, 16, 20};
        memcpy(tables.pirate_wall_jump, wall_jump, sizeof wall_jump);
        memcpy(tables.pirate_jump_low, jump_low, sizeof jump_low);
        memcpy(tables.pirate_jump, jump, sizeof jump);
        memcpy(tables.pirate_jump_high, jump_high, sizeof jump_high);
        tables.health[MZM_PSPRITE_SPACE_PIRATE] = 30;
        tables.damage[MZM_PSPRITE_SPACE_PIRATE] = 100;
        tables.secondary_damage[MZM_SSPRITE_SPACE_PIRATE_LASER] = 100;
        for (int a = 0; a < MZM_PIRATE_ANIMS; ++a) {
            pirate_kind.counts[a] = a == MZM_PIRATE_WALKING ? 8 : 4;
            for (int f = 0; f < pirate_kind.counts[a]; ++f) pirate_kind.timers[a][f] = 4;
        }
        frame.sprites = ps;
        frame.sprite_count = 8;
        frame.hooks = &test_hooks;
        frame.samus_hitbox_top = -100;
#define PIRATE_FRAME()                                                           \
        do {                                                                     \
            for (int i_ = 0; i_ < 8; ++i_) mzm_sprite_update(&ps[i_], (uint8_t)i_, &frame); \
            mzm_sprites_end_frame(&globals, &test_hooks);                        \
        } while (0)

        /* A pirate on a floor, Samus out of range: it walks toward her,
         * accelerating 2 subpixels a frame (a quarter of that per frame). */
        clear();
        memset(ps, 0, sizeof ps);
        memset(&globals, 0, sizeof globals);
        for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
        mzm_sprite_init_primary(&ps[0], MZM_PSPRITE_SPACE_PIRATE, 0, 9, 5, 0, 0);
        frame.samus_x = 23 * 64;
        frame.samus_y = 640;
        PIRATE_FRAME();
        assert(ps[0].pose == 0x9 && ps[0].health == 30 && ps[0].samus_collision == 20 &&
               ps[0].status & MZM_SPRITE_STATUS_FACING_RIGHT &&
               ps[0].status & MZM_SPRITE_STATUS_X_FLIP && ps[0].hitbox_left == -24 &&
               ps[0].hitbox_right == 64 && ps[0].hitbox_top == -160 &&
               ps[0].anim == MZM_PIRATE_WALKING && ps[0].scaling == 0x100);
        uint16_t px = ps[0].x;
        for (int i = 0; i < 6; ++i) PIRATE_FRAME();
        /* work2 0, 2, 4, 6, 8, 10: moves 0 + 0 + 1 + 1 + 2 + 2. */
        assert(ps[0].x == px + 6 && ps[0].work2 == 12 && globals.alarm_timer == 0);
        for (int i = 0; i < 40; ++i) PIRATE_FRAME();
        assert(ps[0].work2 == 32);  /* top speed: 8 subpixels a frame */
        px = ps[0].x;
        PIRATE_FRAME();
        assert(ps[0].x == px + 8);

        /* Samus four blocks ahead in its line of sight: alerted, the alarm
         * rings, it charges for 24 frames and fires two lasers forward. */
        clear();
        memset(ps, 0, sizeof ps);
        for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
        mzm_sprite_init_primary(&ps[0], MZM_PSPRITE_SPACE_PIRATE, 0, 9, 5, 0, 0);
        frame.samus_x = (uint16_t)(5 * 64 + 32 + 4 * 64);
        PIRATE_FRAME();
        PIRATE_FRAME();
        assert(ps[0].status & MZM_SPRITE_STATUS_FACING_DOWN && ps[0].pose == 0x22 &&
               globals.draw_order[0] == 0);   /* cleared by the end of the frame */
        PIRATE_FRAME();
        assert(globals.alarm_timer == MZM_ALARM_TIMER_ACTIVE - 1 && ps[0].pose == 0x34 &&
               ps[0].work1 == 0);
        PIRATE_FRAME();
        assert(ps[0].pose == 0x35 && ps[0].work0 == 23 &&
               ps[0].anim == MZM_PIRATE_CHARGING_LASER_FORWARD);
        int frames = 0;
        while (!(ps[1].status & MZM_SPRITE_STATUS_EXISTS) && frames < 100) {
            PIRATE_FRAME();
            ++frames;
        }
        assert(ps[0].pose == 0x37 && ps[0].anim == MZM_PIRATE_SHOOTING_FORWARD &&
               ps[0].frame == 2);
        uint16_t x0 = ps[0].x, y0 = ps[0].y;
        /* Spawned behind the pirate's slot: initialised and moved once. */
        assert(ps[1].secondary && ps[1].sprite_id == MZM_SSPRITE_SPACE_PIRATE_LASER &&
               ps[1].samus_collision == 19 && ps[1].pose == 0x9 &&
               ps[1].status & MZM_SPRITE_STATUS_X_FLIP && ps[1].y == y0 - 52 &&
               ps[1].x == x0 + 68 && ps[2].y == y0 - 120 && ps[2].x == x0 + 120 &&
               ps[1].ram_slot == 0 && ps[1].work2 == 10);
        affects = 0;
        PIRATE_FRAME();
        assert(ps[1].x == x0 + 68 + 20 && ps[1].work2 == 9 && affects == 2 &&
               last_affect_damage == MZM_DAMAGE_BOMB_PISTOL);
        /* Samus touches the laser: it stops (SPRITE_POSE_STOPPED), hurts her
         * with the laser's damage, then explodes. */
        hurts = 0;
        MzmSpriteSamus laser_hit = {NULL, ps[1].x, (uint16_t)(ps[1].y + 20), ps[1].x,
                                    -40, 0, -20, 20, false, false, false, hurt, false, false, 0, NULL, NULL, 0, false, false, 1, true, false, false, MZM_POSE_STANDING, MZM_POSE_NONE, false, NULL, NULL, false, NULL};
        /* The spawn's ignore timer runs down on the first collision pass. */
        MzmSpriteSamus far_away = laser_hit;
        far_away.x = 60000;
        mzm_sprites_samus_collision(ps, 8, &far_away, &tables);
        assert(hurts == 0 && ps[1].ignore_samus_timer == 0);
        mzm_sprites_samus_collision(ps, 8, &laser_hit, &tables);
        assert(hurts == 1 && last_damage == 100 && ps[1].pose == 0x42);
        PIRATE_FRAME();
        assert(ps[1].pose == 0x43 && ps[1].anim == MZM_PIRATE_LASER_EXPLODING &&
               ps[1].samus_collision == 0);
        for (int i = 0; i < 16; ++i) PIRATE_FRAME();
        assert(!(ps[1].status & MZM_SPRITE_STATUS_EXISTS));
        /* The other laser explodes on a wall (on screen). */
        for (int r = 0; r < ROWS; ++r) grid[r][15] = MZM_CLIPDATA_SOLID;
        frames = 0;
        while (ps[2].pose == 0x9 && frames < 50) {
            PIRATE_FRAME();
            ++frames;
        }
        assert(ps[2].pose == 0x43 && ps[2].x >= 15 * 64);

        /* A laser out of its gun hits a pirate: frozen, flashing, then dead;
         * the last pirate's death stops the alarm. */
        clear();
        memset(ps, 0, sizeof ps);
        for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
        mzm_sprite_init_primary(&ps[0], MZM_PSPRITE_SPACE_PIRATE, 0, 9, 5, 0, 0);
        frame.samus_x = 23 * 64;
        PIRATE_FRAME();
        ps[0].status |= MZM_SPRITE_STATUS_ONSCREEN;
        ps[1] = (MzmSprite){.status = MZM_SPRITE_STATUS_EXISTS, .secondary = true,
                            .sprite_id = MZM_SSPRITE_SPACE_PIRATE_LASER, .samus_collision = 19,
                            .pose = 0x9, .x = ps[0].x, .y = (uint16_t)(ps[0].y - 50)};
        globals.alarm_timer = 300;
        PIRATE_FRAME();
        assert(ps[0].pose == 0x6C && ps[0].health == 0 && ps[0].samus_collision == 0 &&
               ps[0].status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES && ps[0].work0 == 32);
        frames = 0;
        while (ps[0].status & MZM_SPRITE_STATUS_EXISTS && frames < 100) {
            PIRATE_FRAME();
            ++frames;
        }
        assert(frames == 32 && globals.alarm_timer == 0);
        /* With Mecha Ridley dead the alarm never stops. */
        test_events[MZM_EVENT_MECHA_RIDLEY_KILLED / 8] |= 1u << (MZM_EVENT_MECHA_RIDLEY_KILLED % 8);
        globals.alarm_timer = 1;
        mzm_sprites_end_frame(&globals, &test_hooks);
        assert(globals.alarm_timer == MZM_ALARM_TIMER_ACTIVE);
        memset(test_events, 0, sizeof test_events);
        globals.alarm_timer = 0;

        /* A pirate that already reacted (MOSAIC) leaves at a zoneline when
         * the alarm is off. */
        clear();
        memset(ps, 0, sizeof ps);
        for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
        for (int r = 0; r < 10; ++r) grid[r][9] = MZM_CLIPDATA_SOLID;
        movement_grid[9][9] = MZM_CLIPDATA_MOVEMENT_SPACE_PIRATE_ZONELINE;
        mzm_sprite_init_primary(&ps[0], MZM_PSPRITE_SPACE_PIRATE, 0, 9, 5, 0, 0);
        PIRATE_FRAME();
        ps[0].status |= MZM_SPRITE_STATUS_MOSAIC;
        frames = 0;
        while (ps[0].status & MZM_SPRITE_STATUS_EXISTS && frames < 200) {
            PIRATE_FRAME();
            ++frames;
        }
        /* Removed when its probe 80 subpixels ahead reached the zoneline
         * (x 496); the rest of the walk then ran with a cleared status,
         * facing left, and moved it back 8 subpixels as natively. */
        assert(!(ps[0].status & MZM_SPRITE_STATUS_EXISTS) && ps[0].x == 9 * 64 - 80 - 8);

        /* In the air beside a wall on its left (sprite RNG 0): it climbs
         * down the wall a pixel a frame, then turns at the floor and jumps
         * off the wall away from it. */
        clear();
        memset(ps, 0, sizeof ps);
        for (int x = 0; x < COLUMNS; ++x) grid[12][x] = MZM_CLIPDATA_SOLID;
        for (int r = 0; r < 12; ++r) grid[r][4] = MZM_CLIPDATA_SOLID;
        mzm_sprite_init_primary(&ps[0], MZM_PSPRITE_SPACE_PIRATE, 0, 5, 5, 0, 0);
        frame.samus_x = 23 * 64;
        frame.samus_y = 2 * 64;
        PIRATE_FRAME();
        assert(ps[0].pose == 0x5F && ps[0].anim == MZM_PIRATE_CLIMBING_DOWN &&
               !(ps[0].status & MZM_SPRITE_STATUS_FACING_RIGHT) && ps[0].x == 5 * 64 + 32);
        uint16_t py = ps[0].y;
        PIRATE_FRAME();
        assert(ps[0].y == py + 4);
        frames = 0;
        while (ps[0].pose != 0x4D && frames < 400) {
            PIRATE_FRAME();
            ++frames;
        }
        assert(ps[0].pose == 0x4D && ps[0].status & MZM_SPRITE_STATUS_FACING_RIGHT &&
               ps[0].anim == MZM_PIRATE_JUMPING_FROM_WALL);
        px = ps[0].x;
        PIRATE_FRAME();
        assert(ps[0].x == px + 12);  /* the wall jump's horizontal speed */

        /* A wall jump point beside a climbing pirate makes it leave the
         * wall at once. */
        clear();
        memset(ps, 0, sizeof ps);
        for (int x = 0; x < COLUMNS; ++x) grid[12][x] = MZM_CLIPDATA_SOLID;
        for (int r = 0; r < 12; ++r) grid[r][4] = MZM_CLIPDATA_SOLID;
        for (int r = 0; r < 12; ++r)
            movement_grid[r][4] = MZM_CLIPDATA_MOVEMENT_SPACE_PIRATE_WALLJUMP_POINT;
        mzm_sprite_init_primary(&ps[0], MZM_PSPRITE_SPACE_PIRATE, 0, 5, 5, 0, 0);
        PIRATE_FRAME();
        PIRATE_FRAME();
        assert(ps[0].pose == 0x12 && ps[0].work0 == 1);
        PIRATE_FRAME();
        assert(ps[0].pose == 0x13 && ps[0].work0 == 1 &&
               ps[0].anim == MZM_PIRATE_TURNING_TO_AIM_WHILE_CLIMBING);

        /* Waiting pirates hide at their door until the alarm rings. */
        clear();
        memset(ps, 0, sizeof ps);
        for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
        for (int r = 0; r < 10; ++r) grid[r][3] = MZM_CLIPDATA_SOLID;
        mzm_sprite_init_primary(&ps[0], MZM_PSPRITE_SPACE_PIRATE_WAITING1, 0, 9, 5, 0, 0);
        PIRATE_FRAME();
        assert(ps[0].pose == 0x28 && ps[0].samus_collision == 0 &&
               ps[0].status & MZM_SPRITE_STATUS_NOT_DRAWN &&
               ps[0].status & MZM_SPRITE_STATUS_FACING_RIGHT && ps[0].x == 5 * 64 + 32 - 40);
        mzm_sprite_init_primary(&ps[1], MZM_PSPRITE_SPACE_PIRATE_WAITING2, 0, 9, 8, 1, 1);
        PIRATE_FRAME();
        assert(!(ps[1].status & MZM_SPRITE_STATUS_EXISTS));
        globals.alarm_timer = MZM_ALARM_TIMER_ACTIVE;
        PIRATE_FRAME();
        assert(ps[0].pose == 0x24 && ps[0].work0 == 20);
        frames = 0;
        while (ps[0].pose != 0x23 && frames < 100) {
            PIRATE_FRAME();
            ++frames;
        }
        assert(ps[0].pose == 0x23 && ps[0].samus_collision == 20 &&
               !(ps[0].status & MZM_SPRITE_STATUS_NOT_DRAWN) && ps[0].work2 >= 32);

        /* SpawnWaitingPirates: nothing without the alarm; with it, a waiting
         * pirate at the arrival point that comes out of the door. */
        clear();
        memset(ps, 0, sizeof ps);
        for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
        for (int r = 6; r < 10; ++r) grid[r][2] = MZM_CLIPDATA_DOOR;    /* the hatch */
        const uint8_t pirate_set[] = {MZM_PSPRITE_ZOOMER_YELLOW, MZM_PSPRITE_SPACE_PIRATE};
        const uint8_t no_pirate_set[] = {MZM_PSPRITE_ZOOMER_YELLOW};
        globals.alarm_timer = 0;
        mzm_spawn_waiting_pirates(ps, 8, &globals, &test_hooks, pirate_set, 2, 53, 639, 224,
                                  false);
        assert(!(ps[0].status & MZM_SPRITE_STATUS_EXISTS));
        globals.alarm_timer = 200;
        mzm_spawn_waiting_pirates(ps, 8, &globals, &test_hooks, no_pirate_set, 1, 53, 639, 224,
                                  false);
        assert(globals.alarm_timer == 0 && !(ps[0].status & MZM_SPRITE_STATUS_EXISTS));
        globals.alarm_timer = 200;
        ps[0].status = MZM_SPRITE_STATUS_EXISTS;    /* an occupied slot */
        mzm_spawn_waiting_pirates(ps, 8, &globals, &test_hooks, pirate_set, 2, 88, 639, 224,
                                  true);
        assert(ps[1].sprite_id == MZM_PSPRITE_SPACE_PIRATE_WAITING1 && ps[1].room_slot == 0x81 &&
               ps[1].gfx_slot == 5 && ps[1].ram_slot == 1 && ps[1].y == 639 && ps[1].x == 224 &&
               ps[1].status & MZM_SPRITE_STATUS_FACING_DOWN &&
               ps[1].status & MZM_SPRITE_STATUS_NOT_DRAWN);
        memset(ps, 0, sizeof ps);
        frame.samus_x = 224;
        frame.samus_y = 639;
        mzm_spawn_waiting_pirates(ps, 8, &globals, &test_hooks, pirate_set, 2, 53, 639, 224,
                                  false);
        PIRATE_FRAME();
        /* Facing Samus (right, x not above hers), 2 blocks and an eighth
         * back toward the door, 20 frames before it shoots. */
        assert(ps[0].pose == 0x24 && ps[0].status & MZM_SPRITE_STATUS_FACING_RIGHT &&
               ps[0].x == 224 - 136 && ps[0].work0 == 20 &&
               ps[0].status & MZM_SPRITE_STATUS_MOSAIC &&
               ps[0].status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES && ps[0].samus_collision == 0);
        frame.samus_x = 20 * 64;    /* out of its line of sight */
        frames = 0;
        while (ps[0].pose == 0x24 && frames < 100) {
            PIRATE_FRAME();
            ++frames;
        }
        /* The hatch 1.5 blocks ahead: it shoots through it with a hidden
         * laser when its timer ends. */
        assert(ps[0].pose == 0x25 && ps[1].secondary && ps[1].room_slot == 0x80 &&
               ps[1].status & MZM_SPRITE_STATUS_NOT_DRAWN && ps[1].samus_collision == 0);
        frames = 0;
        while (ps[0].pose == 0x25 && frames < 100) {
            PIRATE_FRAME();
            ++frames;
        }
        assert(ps[0].pose == 0x23 && frames == 10 && ps[0].samus_collision == 20 &&
               !(ps[0].status & MZM_SPRITE_STATUS_NOT_DRAWN));
        /* After Mecha Ridley the alarm is forced on at every door. */
        memset(ps, 0, sizeof ps);
        globals.alarm_timer = 0;
        test_events[MZM_EVENT_MECHA_RIDLEY_KILLED / 8] |= 1u << (MZM_EVENT_MECHA_RIDLEY_KILLED % 8);
        mzm_spawn_waiting_pirates(ps, 8, &globals, &test_hooks, pirate_set, 2, 53, 639, 224,
                                  false);
        assert(globals.alarm_timer == MZM_ALARM_TIMER_ACTIVE && ps[0].status);
        memset(test_events, 0, sizeof test_events);
        globals.alarm_timer = 0;

        /* Chozodia security: searchlights, security lasers, searchlight
         * eyes. */
        for (int a = 0; a < MZM_SECURITY_ANIMS; ++a) {
            security_kind.counts[a] = 2;
            security_kind.timers[a][0] = security_kind.timers[a][1] = 4;
        }
        tables.health[MZM_PSPRITE_SEARCHLIGHT_EYE] = 1;
        tables.weakness[MZM_PSPRITE_SEARCHLIGHT_EYE] = MZM_WEAKNESS_BEAM_BOMBS;
        tables.secondary_damage[MZM_SSPRITE_SEARCHLIGHT_EYE_PROJECTILE] = 16;
        globals.alarm_timer = 0;

        /* The searchlight sweeps half a pixel a frame on both axes,
         * flickering, and turns at stop-enemy blocks. */
        clear();
        memset(ps, 0, sizeof ps);
        movement_grid[4][7] = MZM_CLIPDATA_MOVEMENT_STOP_ENEMY_BLOCK_SOLID;
        mzm_sprite_init_primary(&ps[0], MZM_PSPRITE_SEARCHLIGHT, 4, 3, 5, 0, 0);
        PIRATE_FRAME();
        assert(ps[0].pose == 0x9 && ps[0].samus_collision == 30 &&
               ps[0].status & MZM_SPRITE_STATUS_FACING_RIGHT &&
               ps[0].status & MZM_SPRITE_STATUS_FACING_DOWN &&
               ps[0].anim == MZM_SECURITY_SEARCHLIGHT_MOVING);
        uint16_t sx = ps[0].x, sy = ps[0].y;
        uint16_t drawn = ps[0].status & MZM_SPRITE_STATUS_NOT_DRAWN;
        PIRATE_FRAME();
        assert(ps[0].x == sx + 2 && ps[0].y == sy + 2 &&
               (ps[0].status & MZM_SPRITE_STATUS_NOT_DRAWN) != drawn);
        frames = 0;
        while (ps[0].status & MZM_SPRITE_STATUS_FACING_RIGHT && frames < 100) {
            PIRATE_FRAME();
            ++frames;
        }
        /* The block a block right of it (column 7) stops it at x 384. */
        assert(ps[0].x == 7 * 64 - 64 && frames < 100);
        /* Samus in its light rings the alarm, then it holds it. */
        hurts = 0;
        MzmSpriteSamus in_light = {NULL, ps[0].x, (uint16_t)(ps[0].y + 20), ps[0].x,
                                   -40, 0, -20, 20, false, false, false, hurt, false, false, 0, NULL, NULL, 0, false, false, 1, true, false, false, MZM_POSE_STANDING, MZM_POSE_NONE, false, NULL, NULL, false, NULL};
        ps[0].status |= MZM_SPRITE_STATUS_ONSCREEN;
        MzmSpriteSamus out_of_light = in_light;
        out_of_light.x = 60000;
        mzm_sprites_samus_collision(ps, 8, &out_of_light, &tables);
        mzm_sprites_samus_collision(ps, 8, &in_light, &tables);
        assert(ps[0].status & MZM_SPRITE_STATUS_SAMUS_COLLIDING && hurts == 0);
        PIRATE_FRAME();
        assert(globals.alarm_timer == MZM_ALARM_TIMER_ACTIVE - 1 && ps[0].pose == 0x23 &&
               ps[0].samus_collision == 0);
        for (int i = 0; i < 10; ++i) PIRATE_FRAME();
        assert(ps[0].pose == 0x25 && ps[0].status & MZM_SPRITE_STATUS_NOT_DRAWN);
        globals.alarm_timer = 5;
        PIRATE_FRAME();
        assert(globals.alarm_timer == MZM_ALARM_TIMER_ACTIVE - 1);
        /* With the alarm on, a new searchlight is removed. */
        mzm_sprite_init_primary(&ps[1], MZM_PSPRITE_SEARCHLIGHT2, 4, 3, 5, 1, 1);
        PIRATE_FRAME();
        assert(!(ps[1].status & MZM_SPRITE_STATUS_EXISTS));

        /* A vertical security laser up to the ceiling two blocks above:
         * tripped, it marks the pirates, fades out and holds the alarm. */
        clear();
        memset(ps, 0, sizeof ps);
        globals.alarm_timer = 0;
        for (int x = 0; x < COLUMNS; ++x) grid[7][x] = MZM_CLIPDATA_SOLID;
        mzm_sprite_init_primary(&ps[0], MZM_PSPRITE_SECURITY_LASER_VERTICAL, 4, 9, 5, 0, 0);
        ps[2] = (MzmSprite){.status = MZM_SPRITE_STATUS_EXISTS, .samus_collision = 20,
                            .pose = 0x27, .x = 2000, .y = 640};
        PIRATE_FRAME();
        assert(ps[0].pose == 0x9 && ps[0].work1 == 0x82 && ps[0].hitbox_top == -100 &&
               ps[0].anim == MZM_SECURITY_LASER_VERTICAL_SMALL && ps[0].work3 == 8);
        ps[0].status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        PIRATE_FRAME();
        assert(ps[0].pose == 0xB && ps[0].anim == MZM_SECURITY_LASER_VERTICAL_SMALL_DISAPPEARING &&
               globals.alarm_timer == MZM_ALARM_TIMER_ACTIVE - 1 &&
               ps[2].status & MZM_SPRITE_STATUS_MOSAIC);
        frames = 0;
        while (ps[0].pose == 0xB && frames < 50) {
            PIRATE_FRAME();
            ++frames;
        }
        assert(ps[0].pose == 0x23 && ps[0].status & MZM_SPRITE_STATUS_NOT_DRAWN &&
               ps[0].anim == MZM_SECURITY_LASER_VERTICAL_SMALL);
        /* The fade toward the full blend, one step per frame. */
        assert(ps[0].work3 > 8 && ps[0].work3 <= 16);

        /* A searchlight eye on a wall to its right: immune before the
         * Fully Powered Suit, its beam seven blocks out; Samus in the beam
         * rings the alarm and the eye shoots. */
        clear();
        memset(ps, 0, sizeof ps);
        globals.alarm_timer = 0;
        for (int r = 0; r < ROWS; ++r) grid[r][9] = MZM_CLIPDATA_SOLID;
        mzm_sprite_init_primary(&ps[0], MZM_PSPRITE_SEARCHLIGHT_EYE, 4, 4, 8, 0, 0);
        PIRATE_FRAME();
        assert(ps[0].pose == 0x9 && ps[0].immune_to_projectiles && ps[0].health == 1 &&
               ps[0].x == 8 * 64 + 64 && !(ps[0].status & MZM_SPRITE_STATUS_X_FLIP) &&
               ps[1].secondary && ps[1].sprite_id == MZM_SSPRITE_SEARCHLIGHT_EYE_BEAM2 &&
               ps[1].x == 8 * 64 + 64 - 448 && ps[1].samus_collision == 30);
        /* Moving down a subpixel a frame; the beam follows. */
        uint16_t ey = ps[0].y;
        PIRATE_FRAME();
        assert(ps[0].y == ey + 1 && ps[1].y == ps[0].y);
        /* A beam shot: the invincible impact, no damage. */
        static MzmWeapons eye_weapons;
        memset(&eye_weapons, 0, sizeof eye_weapons);
        eye_weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM,
                                              .x = ps[0].x, .y = ps[0].y};
        mzm_sprites_projectile_hits(ps, 8, &eye_weapons, &tables, NULL);
        assert(!eye_weapons.list[0].active && ps[0].health == 1 && (ps[0].isft & 0x7F) == 2);
        /* Samus in the beam, in front of the eye. */
        frame.samus_x = (uint16_t)(ps[0].x - 200);
        ps[1].status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        PIRATE_FRAME();
        assert(globals.alarm_timer == MZM_ALARM_TIMER_ACTIVE - 1 && ps[1].pose == 0x23);
        PIRATE_FRAME();
        assert(ps[0].pose == 0x23 && ps[0].work0 == 40);
        frames = 0;
        ps[0].status |= MZM_SPRITE_STATUS_ONSCREEN;
        /* The beam disappears after 40 frames; the projectile takes its
         * slot. */
        while (!(ps[1].status & MZM_SPRITE_STATUS_EXISTS &&
                 ps[1].sprite_id == MZM_SSPRITE_SEARCHLIGHT_EYE_PROJECTILE) && frames < 200) {
            ps[0].status |= MZM_SPRITE_STATUS_ONSCREEN;
            PIRATE_FRAME();
            ++frames;
        }
        /* 40 frames getting alerted, one alerted frame, then the 30 frames
         * of the shooting table and the shot. */
        assert(frames == 40 + 1 + 31 && ps[1].samus_collision == 6 && ps[0].pose == 0x25 &&
               ps[0].work0 == 90 && ps[1].x == ps[0].x - 12);
        uint16_t qx = ps[1].x;
        PIRATE_FRAME();
        assert(ps[1].x == qx - 12);
        globals.alarm_timer = 0;
#undef PIRATE_FRAME
        frame.sprites = NULL;
        frame.sprite_count = 0;
        frame.hooks = NULL;
    }
    /* An item Chozo statue (ChozoStatue, ChozoStatuePart, ChozoBall): the
     * Long Beam statue faces right, sits (CAA_MAKE_NON_POWER_GRIP on four
     * blocks), holds the ball and spawns its eye, arm, leg and glow. */
    {
        static MzmSprite room[8];
        static MzmSpriteKind chozo_kind;
        memset(room, 0, sizeof room);
        memset(&chozo_kind, 0, sizeof chozo_kind);
        for (int a = 0; a < MZM_CHOZO_ANIMS; ++a) {
            chozo_kind.counts[a] = 1;
            chozo_kind.timers[a][0] = 4;
        }
        chozo_kind.counts[MZM_CHOZO_BALL_NORMAL_REVEALING] = 3;
        chozo_kind.timers[MZM_CHOZO_BALL_NORMAL_REVEALING][1] = 4;
        chozo_kind.timers[MZM_CHOZO_BALL_NORMAL_REVEALING][2] = 4;
        MzmMultiSprite *seated = &tables.chozo_multi[MZM_CHOZO_MULTI_SEATED];
        const int16_t parts[5][3] = {{8, -48, 0}, {15, -64, -56}, {4, -168, 8}, {2, 4, 0},
                                     {3, -128, 60}};
        seated->timers[0] = 255;
        memcpy(seated->parts[0], parts, sizeof parts);
        MzmSpriteFrame statue_frame = frame;
        uint16_t prevent = 0;
        statue_frame.kinds[MZM_FAMILY_CHOZO_STATUE] = &chozo_kind;
        statue_frame.sprites = room;
        statue_frame.sprite_count = 8;
        statue_frame.hooks = &test_hooks;
        statue_frame.prevent_movement = &prevent;
        MzmEquipment items = {.energy = 99, .max_energy = 99};
        statue_frame.equipment = &items;
        memset(test_events, 0, sizeof test_events);
        caa_calls = 0;
        message_count = 0;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_CHOZO_STATUE_LONG_HINT + 1, 0, 6, 7, 0, 0);
        mzm_sprite_update(&room[0], 0, &statue_frame);
        assert(room[0].pose == 0x29 && room[0].status & MZM_SPRITE_STATUS_X_FLIP &&
               room[0].always_active && room[0].draw_order == 3);
        assert(caa_calls == 4 && last_caa == 4);      /* CAA_MAKE_NON_POWER_GRIP */
        /* gSubSpriteData1 half a block right; the head at its OAM offset. */
        assert(globals.sub1.x == 7 * 64 + 32 + 32 && globals.sub1.multi == MZM_CHOZO_MULTI_SEATED);
        assert(room[0].anim == MZM_CHOZO_IDLE && room[0].y == globals.sub1.y - 128 &&
               room[0].x == globals.sub1.x - 60);
        assert(room[1].secondary && room[1].sprite_id == MZM_SSPRITE_CHOZO_BALL &&
               room[1].y == globals.sub1.y - 96 && room[1].x == globals.sub1.x + 56 &&
               room[1].owner == MZM_PSPRITE_CHOZO_STATUE_LONG_HINT + 1);
        assert(room[0].work1 == 2 && room[2].room_slot == 2 && room[5].room_slot == 1 &&
               room[5].work1 == 3);
        for (int i = 1; i < 6; ++i) mzm_sprite_update(&room[i], (uint8_t)i, &statue_frame);
        assert(room[1].samus_collision == 1 && room[1].anim == MZM_CHOZO_BALL_NORMAL_CLOSED);
        assert(room[3].anim == MZM_CHOZO_ARM_IDLE && room[3].draw_order == 2 &&
               room[5].status & MZM_SPRITE_STATUS_NOT_DRAWN);
        /* Shot ("destroyed"): the ball reveals its item, immune from then on. */
        mzm_sfx_clear();
        room[1].pose = 0x62;
        mzm_sprite_update(&room[1], 1, &statue_frame);
        assert(room[1].pose == 0x67 && room[1].immune_to_projectiles &&
               room[1].samus_collision == 30 &&
               mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_CHOZO_BALL_REVEALING));
        for (int i = 0; i < 12 && room[1].pose == 0x67; ++i) mzm_sprite_update(&room[1], 1, &statue_frame);
        assert(room[1].pose == 0x9 && room[1].anim == MZM_CHOZO_BALL_NORMAL_REVEALED);
        /* Touched: the item, its events and its banner; Samus stops. */
        room[1].status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        mzm_sprite_update(&room[1], 1, &statue_frame);
        assert(room[1].pose == 0x23 && prevent == 1000 &&
               items.items & MZM_ITEM_LONG_BEAM && test_check_event(NULL, 8) &&
               message_count == 1 && messages[0] == 8);
        /* The statue notices the item is gone, waits 40 frames, idles. */
        mzm_sprite_update(&room[0], 0, &statue_frame);
        assert(room[0].pose == 0x2B && room[0].work0 == 40);
        for (int i = 0; i < 40; ++i) mzm_sprite_update(&room[0], 0, &statue_frame);
        assert(room[0].pose == 0x9);
        /* The ball flickers while movement stays prevented, then goes; the
         * arm glows once it is free. */
        mzm_sprite_update(&room[1], 1, &statue_frame);
        assert(room[1].status & MZM_SPRITE_STATUS_EXISTS);
        prevent = 0;
        mzm_sprite_update(&room[1], 1, &statue_frame);
        assert(!(room[1].status & MZM_SPRITE_STATUS_EXISTS));
        mzm_sprite_update(&room[3], 3, &statue_frame);
        assert(room[3].anim == MZM_CHOZO_ARM_GLOW);
        mzm_sprite_update(&room[5], 5, &statue_frame);
        assert(!(room[5].status & MZM_SPRITE_STATUS_NOT_DRAWN));
        /* Reloaded with the Long Beam: a refill statue without a ball. */
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_CHOZO_STATUE_LONG_HINT + 1, 0, 6, 7, 0, 0);
        memset(room + 1, 0, sizeof room - sizeof room[0]);
        mzm_sprite_update(&room[0], 0, &statue_frame);
        assert(room[0].pose == 0x9 && room[1].sprite_id == MZM_SSPRITE_CHOZO_STATUE_PART);
        /* The refill: Samus in the Morph Ball resting in the glowing hand
         * is grabbed and refilled, then the statue sleeps, the banner opens
         * and the hand lets her go once she may move. */
        for (int i = 1; i < 8; ++i)
            if (room[i].status & MZM_SPRITE_STATUS_EXISTS)
                mzm_sprite_update(&room[i], (uint8_t)i, &statue_frame);
        int arm_slot = -1;
        for (int i = 1; i < 8; ++i)
            if (room[i].sprite_id == MZM_SSPRITE_CHOZO_STATUE_PART && room[i].room_slot == 0)
                arm_slot = i;
        assert(arm_slot > 0 && room[arm_slot].pose == 0x25 &&
               room[arm_slot].anim == MZM_CHOZO_ARM_GLOW);
        MzmSprite *arm = &room[arm_slot];
        uint16_t hand = (uint16_t)(arm->x + 56);
        MzmSpriteSamusLink link = {MZM_LINK_POSE_MORPH_BALL, (uint16_t)(arm->y - 17), hand,
                                   false, false, false, false, MZM_POSE_NONE, false, false, false, 0, MZM_POSE_STANDING, false, false, 0};
        statue_frame.samus_link = &link;
        /* Off by one subpixel in height: no grab. */
        link.y++;
        mzm_sprite_update(arm, (uint8_t)arm_slot, &statue_frame);
        assert(!link.grab);
        link.y--;
        mzm_sprite_update(arm, (uint8_t)arm_slot, &statue_frame);
        assert(link.grab && link.pose == MZM_LINK_POSE_GRABBED && room[0].pose == 0x2E &&
               arm->pose == 0x27 && arm->anim == MZM_CHOZO_ARM_SAMUS_GRABBED);
        items.energy = 10;
        items.missiles = 0;
        items.max_missiles = 0;
        mzm_sfx_clear();
        mzm_sprite_update(&room[0], 0, &statue_frame);
        assert(room[0].pose == 0x2F);
        int glow = -1;
        for (int i = 1; i < 8; ++i)
            if (room[i].sprite_id == 2 && room[i].status & MZM_SPRITE_STATUS_EXISTS) glow = i;
        assert(glow > 0);
        mzm_sprite_update(&room[glow], (uint8_t)glow, &statue_frame);
        assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_CHOZO_STATUE_REFILL) &&
               room[glow].draw_order == 1);
        /* SpriteUtilRefillEnergy: (99 - 10) / 32 = 2 a frame at first. */
        mzm_sprite_update(arm, (uint8_t)arm_slot, &statue_frame);
        assert(items.energy == 12);
        message_count = 0;
        for (int i = 0; i < 200 && room[0].pose == 0x2F; ++i) {
            mzm_sprite_update(&room[0], 0, &statue_frame);
            mzm_sprite_update(arm, (uint8_t)arm_slot, &statue_frame);
        }
        assert(items.energy == 99 && room[0].pose == 0x30 && arm->pose == 0x29 &&
               message_count == 1 && messages[0] == 25);
        /* The flashing rows run 1, 2, 3, 2 during the refill. */
        mzm_sprite_update(&room[0], 0, &statue_frame);
        assert(room[0].pose == 0x31 && room[0].palette_row == 0);
        mzm_sprite_update(arm, (uint8_t)arm_slot, &statue_frame);
        assert(arm->pose == 0x2B);
        prevent = 5;
        mzm_sprite_update(arm, (uint8_t)arm_slot, &statue_frame);
        assert(!link.release);
        prevent = 0;
        mzm_sprite_update(arm, (uint8_t)arm_slot, &statue_frame);
        assert(link.release && link.pose == MZM_LINK_POSE_MORPH_BALL &&
               arm->anim == MZM_CHOZO_ARM_IDLE);
        mzm_sprite_update(&room[glow], (uint8_t)glow, &statue_frame);
        assert(!(room[glow].status & MZM_SPRITE_STATUS_EXISTS));
        statue_frame.samus_link = NULL;
        /* The hint statue stands (the hand made grippable) until its event. */
        caa_calls = 0;
        memset(room, 0, sizeof room);
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_CHOZO_STATUE_LONG_HINT, 0, 6, 7, 0, 0);
        memset(test_events, 0, sizeof test_events);
        mzm_sprite_update(&room[0], 0, &statue_frame);
        assert(globals.sub1.multi == MZM_CHOZO_MULTI_STANDING && globals.sub1.work3 &&
               caa_calls == 8 && !(room[0].status & MZM_SPRITE_STATUS_X_FLIP));
        /* Grabbed in the hand while rolling: the hint registers, the eye
         * opens, the statue flashes, then sits down (the multi-sprite
         * Sitting table and its echoes) and seats. */
        MzmMultiSprite *standing = &tables.chozo_multi[MZM_CHOZO_MULTI_STANDING];
        standing->timers[0] = 255;
        memcpy(standing->parts[0], parts, sizeof parts);
        MzmMultiSprite *sitting = &tables.chozo_multi[MZM_CHOZO_MULTI_SITTING];
        sitting->timers[0] = sitting->timers[1] = 2;
        memcpy(sitting->parts[0], parts, sizeof parts);
        memcpy(sitting->parts[1], parts, sizeof parts);
        for (int i = 1; i < 8; ++i)
            if (room[i].status & MZM_SPRITE_STATUS_EXISTS)
                mzm_sprite_update(&room[i], (uint8_t)i, &statue_frame);
        MzmSprite *hint_arm = NULL;
        for (int i = 1; i < 8; ++i)
            if (room[i].sprite_id == MZM_SSPRITE_CHOZO_STATUE_PART && room[i].room_slot == 0)
                hint_arm = &room[i];
        assert(hint_arm && hint_arm->pose == 0x9);
        MzmSpriteSamusLink hint_link = {MZM_LINK_POSE_ROLLING, (uint16_t)(hint_arm->y - 17),
                                        (uint16_t)(hint_arm->x - 56), false, false, false,
                                        false, MZM_POSE_NONE, false, false, false, 0, MZM_POSE_STANDING, false, false, 0};
        statue_frame.samus_link = &hint_link;
        mzm_sprite_update(hint_arm, 0, &statue_frame);
        assert(hint_link.grab && room[0].pose == 0x22 && hint_arm->pose == 0x23);
        mzm_sprite_update(&room[0], 0, &statue_frame);
        assert(room[0].pose == 0x23 && test_check_event(NULL, 8) && room[0].work0 == 160 &&
               mzm_sfx_requested(MZM_SFX_MUSIC_FADE, 0));
        MzmSprite *eye = &room[room[0].work1];
        mzm_sprite_update(eye, room[0].work1, &statue_frame);
        assert(eye->pose == 0x43);
        for (int i = 0; i < 8 && eye->pose != 0x61; ++i) mzm_sprite_update(eye, room[0].work1, &statue_frame);
        assert(eye->pose == 0x61 && eye->anim == MZM_CHOZO_EYE_OPENED);
        mzm_sfx_clear();
        int flashes = 0;
        for (int i = 0; i < 200 && room[0].pose == 0x23; ++i) {
            mzm_sprite_update(&room[0], 0, &statue_frame);
            flashes += room[0].palette_row != 0;
        }
        assert(room[0].pose == 0x24 && flashes > 0 && room[0].palette_row == 0 &&
               mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_CHOZO_STATUE_HINT) &&
               mzm_sfx_requested(MZM_SFX_MUSIC_PLAY, MZM_MUSIC_CHOZO_STATUE_HINT));
        mzm_sprite_update(&room[0], 0, &statue_frame);
        assert(room[0].pose == 0x25 && globals.sub1.multi == MZM_CHOZO_MULTI_SITTING &&
               mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_CHOZO_STATUE_SITTING_DOWN));
        for (int i = 0; i < 20 && room[0].pose == 0x25; ++i) mzm_sprite_update(&room[0], 0, &statue_frame);
        assert(room[0].pose == 0x27 && globals.sub1.multi == MZM_CHOZO_MULTI_SEATED);
        /* ChozoStatueSleeping: a hint statue replays the room track. */
        {
            MzmSpriteWorld track_world = *statue_frame.world;
            track_world.room_track = MZM_MUSIC_BRINSTAR;
            MzmSpriteFrame sleep_frame = statue_frame;
            sleep_frame.world = &track_world;
            MzmSprite awake = room[0];
            room[0].pose = 0x30;
            mzm_sfx_clear();
            for (int i = 0; i < 80 && room[0].pose != 0xF; ++i) mzm_sprite_update(&room[0], 0, &sleep_frame);
            assert(room[0].pose == 0xF && mzm_sfx_requested(MZM_SFX_MUSIC_PLAY, MZM_MUSIC_BRINSTAR));
            room[0] = awake;
        }
        statue_frame.samus_link = NULL;
        /* An unknown item statue (UnknownItemChozoStatue): its own parts,
         * OAM and multi-sprite tables; its item is obtained but not
         * activated. */
        memset(room, 0, sizeof room);
        MzmMultiSprite *unknown_seated =
            &tables.chozo_multi[MZM_CHOZO_MULTI_UNKNOWN + MZM_CHOZO_MULTI_SEATED];
        unknown_seated->timers[0] = 255;
        memcpy(unknown_seated->parts[0], parts, sizeof parts);
        MzmEquipment plain = {.energy = 99, .max_energy = 99};
        statue_frame.equipment = &plain;
        message_count = 0;
        memset(test_events, 0, sizeof test_events);
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_CHOZO_STATUE_PLASMA_BEAM, 0, 6, 7, 0, 0);
        mzm_sprite_update(&room[0], 0, &statue_frame);
        assert(room[0].pose == 0x29 && !(room[0].status & MZM_SPRITE_STATUS_X_FLIP) &&
               globals.sub1.multi == MZM_CHOZO_MULTI_UNKNOWN + MZM_CHOZO_MULTI_SEATED &&
               room[0].anim == MZM_CHOZO_UNKNOWN_BASE + MZM_CHOZO_IDLE);
        assert(room[1].sprite_id == MZM_SSPRITE_CHOZO_BALL &&
               room[2].sprite_id == MZM_SSPRITE_UNKNOWN_ITEM_CHOZO_STATUE_PART &&
               mzm_sprite_graphics_owner(&room[2]) == MZM_PSPRITE_CHOZO_STATUE_PLASMA_BEAM);
        mzm_sprite_update(&room[1], 1, &statue_frame);
        assert(room[1].anim == MZM_CHOZO_UNKNOWN_BASE + MZM_CHOZO_BALL_NORMAL_CLOSED);
        room[1].pose = 0x9;
        room[1].status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        mzm_sprite_update(&room[1], 1, &statue_frame);
        assert(plain.unknown_items & MZM_ITEM_PLASMA_BEAM && !(plain.items & MZM_ITEM_PLASMA_BEAM) &&
               test_check_event(NULL, 24) && message_count == 1 && messages[0] == 12);
        mzm_sprite_update(&room[0], 0, &statue_frame);
        assert(room[0].pose == 0x2B);
        statue_frame.equipment = &items;
    }
    /* Skulteras patrol a pixel every three frames, chase Samus every two
     * and rise only in water. */
    {
        static MzmSpriteKind skultera_kind;
        for (int a = 0; a < MZM_SKULTERA_ANIMS; ++a) {
            skultera_kind.counts[a] = 2;
            skultera_kind.timers[a][0] = skultera_kind.timers[a][1] = 3;
        }
        frame.kinds[MZM_FAMILY_SKULTERA] = &skultera_kind;
        tables.health[MZM_PSPRITE_SKULTERA] = 20;
        clear();
        for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
        MzmSprite sk;
        mzm_sprite_init_primary(&sk, MZM_PSPRITE_SKULTERA, 0, 9, 10, 0, 0);
        frame.samus_x = 60000;
        frame.samus_y = 0;
        step(&sk, 0);
        /* rng & 1 = 0: X flip, head to the right. */
        assert(sk.pose == 8 && sk.status & MZM_SPRITE_STATUS_X_FLIP && sk.hitbox_right == 40 &&
               sk.hitbox_left == -24 && sk.hitbox_top == -96 && sk.health == 20);
        step(&sk, 0);
        assert(sk.pose == 9 && sk.x_spawn == sk.x && sk.work0 == 3);
        uint16_t sx = sk.x;
        for (int i = 0; i < 9; ++i) step(&sk, 0);
        assert(sk.x == sx + 3 * 4);
        /* Samus behind it within 5 blocks: it turns. */
        frame.samus_x = (uint16_t)(sk.x - 200);
        frame.samus_y = (uint16_t)(sk.y - 32);
        step(&sk, 0);
        assert(sk.pose == 0xA);
        step(&sk, 0);
        assert(sk.pose == 0xB && sk.anim == MZM_SKULTERA_TURNING_AROUND);
        frames = 0;
        while (sk.pose == 0xB && frames++ < 20) step(&sk, 0);
        assert(sk.pose == 9 && !(sk.status & MZM_SPRITE_STATUS_X_FLIP) && sk.hitbox_left == -40);
        /* Ahead of it now: the chase, two frames a pixel. */
        step(&sk, 0);
        assert(sk.pose == 0x34);
        step(&sk, 0);
        assert(sk.pose == 0x35 && sk.anim == MZM_SKULTERA_CHASING_SAMUS);
        sx = sk.x;
        for (int i = 0; i < 6; ++i) step(&sk, 0);
        assert(sk.x <= sx - 2 * 4);
        /* Samus far above: no rise out of the water (no hazard here). */
        uint16_t sy = sk.y;
        frame.samus_y = (uint16_t)(sk.y - 200);
        step(&sk, 0);
        assert(sk.y == sy);
        frame.samus_x = 60000;
    }

    /* A Dragon rises toward Samus above it, spits a fireball after its
     * warning, and the fireball arcs at 3 pixels a frame. */
    {
        static MzmSprite room[4];
        static MzmSpriteKind dragon_kind;
        memset(room, 0, sizeof room);
        for (int a = 0; a < MZM_DRAGON_ANIMS; ++a) {
            dragon_kind.counts[a] = 2;
            dragon_kind.timers[a][0] = dragon_kind.timers[a][1] = 3;
        }
        MzmSpriteFrame dragon_frame = frame;
        dragon_frame.kinds[MZM_FAMILY_DRAGON] = &dragon_kind;
        dragon_frame.sprites = room;
        dragon_frame.sprite_count = 4;
        tables.health[MZM_PSPRITE_DRAGON] = 18;
        tables.secondary_health[MZM_SSPRITE_DRAGON_FIREBALL] = 1;
        for (int i = 0; i < 39; ++i) tables.dragon_fireball_y[i] = (int16_t)(i < 20 ? -4 : 4);
        tables.dragon_fireball_y[39] = 32767;
        clear();
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_DRAGON, 0, 9, 5, 0, 0);
        dragon_frame.samus_x = (uint16_t)(room[0].x + 200);   /* to its right */
        dragon_frame.samus_y = (uint16_t)(room[0].y - 100);
        mzm_sprite_update(&room[0], 0, &dragon_frame);
        assert(room[0].pose == 8 && room[0].status & MZM_SPRITE_STATUS_X_FLIP &&
               room[0].y_spawn == room[0].y && room[0].health == 18);
        uint16_t spawn = room[0].y;
        int frames = 0;
        while (room[0].pose != 0x35 && frames++ < 200) mzm_sprite_update(&room[0], 0, &dragon_frame);
        /* The 100-frame spit interval while rising two blocks at most. */
        assert(room[0].pose == 0x35 && frames >= 99 && room[0].y < spawn &&
               room[0].y >= spawn - (2 * 64 - 1));
        frames = 0;
        while (room[1].status == 0 && frames++ < 60) mzm_sprite_update(&room[0], 0, &dragon_frame);
        /* The rest of the half-second warning, then 8 frames of spitting. */
        assert(frames == 29 + 8 && room[0].pose == 9 && room[0].work0 == 100);
        assert(room[1].secondary && room[1].sprite_id == MZM_SSPRITE_DRAGON_FIREBALL);
        mzm_sfx_clear();
        mzm_sprite_update(&room[1], 1, &dragon_frame);
        assert(room[1].status & MZM_SPRITE_STATUS_FACING_RIGHT && room[1].health == 1 &&
               room[1].status & MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE &&
               mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_DRAGON_SPITTING));
        uint16_t fx = room[1].x, fy = room[1].y;
        mzm_sprite_update(&room[1], 1, &dragon_frame);
        assert(room[1].x == fx + 12 && room[1].y == fy - 4);
        /* Into a wall: it explodes. */
        for (int y = 0; y < ROWS; ++y) grid[y][(room[1].x + 64) / 64] = MZM_CLIPDATA_SOLID;
        frames = 0;
        while (room[1].pose == 9 && frames++ < 20) mzm_sprite_update(&room[1], 1, &dragon_frame);
        mzm_sprite_update(&room[1], 1, &dragon_frame);
        assert(room[1].pose == 0x43 && room[1].anim == MZM_DRAGON_FIREBALL_EXPLODING);
    }

    /* The room gadgets: the elevator pad rides with Samus, steam vents sit
     * on their surface, water drops fall and splash, Gerons and the unknown
     * item blocks fill their blocks. */
    {
        static MzmSprite room[4];
        static MzmSpriteKind gadget_kind;
        memset(room, 0, sizeof room);
        memset(&gadget_kind, 0, sizeof gadget_kind);
        for (int a = 0; a < MZM_GADGET_ANIMS; ++a) {
            gadget_kind.counts[a] = 10;
            for (int f = 0; f < 10; ++f) gadget_kind.timers[a][f] = 2;
        }
        MzmSpriteFrame g = frame;
        g.kinds[MZM_FAMILY_GADGET] = &gadget_kind;
        g.sprites = room;
        g.sprite_count = 4;
        g.hooks = &test_hooks;
        clear();
        /* ElevatorPad */
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_ELEVATOR_PAD, 0, 5, 5, 0, 0);
        uint16_t pad_y = room[0].y;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 9 && room[0].y == pad_y - 8 && room[0].always_active &&
               room[0].anim == MZM_GADGET_ELEVATOR_PAD_IDLE);
        g.samus_using_elevator = true;
        g.samus_x = 1000;
        g.samus_y = 600;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].x == 1000 && room[0].y == 600 && room[0].anim == MZM_GADGET_ELEVATOR_PAD_MOVING);
        g.samus_using_elevator = false;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].anim == MZM_GADGET_ELEVATOR_PAD_IDLE);
        /* Steam on the ground (block row 6 under the sprite of row 5). */
        for (int x = 0; x < COLUMNS; ++x) grid[6][x] = MZM_CLIPDATA_SOLID;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_STEAM_LARGE, 0, 5, 5, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 9 && room[0].anim == MZM_GADGET_STEAM_VERTICAL_LARGE &&
               !(room[0].status & MZM_SPRITE_STATUS_Y_FLIP) && room[0].work1 &&
               room[0].draw_top == (64 * 4 + 32) / 4 && room[0].samus_collision == 0);
        /* Its sound on the first frame, on screen. */
        mzm_sfx_clear();
        room[0].frame = 0;
        room[0].adc = 2;
        room[0].status |= MZM_SPRITE_STATUS_ONSCREEN;
        mzm_sprite_update(&room[0], 0, &g);
        assert(mzm_sfx_requested(MZM_SFX_PLAY_NOT_PLAYING, MZM_SOUND_STEAM_LARGE));
        /* On the ceiling: flipped and a block up. */
        clear();
        for (int x = 0; x < COLUMNS; ++x) grid[4][x] = MZM_CLIPDATA_SOLID;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_STEAM_SMALL, 0, 5, 5, 0, 0);
        uint16_t steam_y = room[0].y;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].status & MZM_SPRITE_STATUS_Y_FLIP && room[0].y == steam_y - 64 &&
               room[0].anim == MZM_GADGET_STEAM_VERTICAL_SMALL && !room[0].work1);
        /* On a left wall: horizontal, half a block up and left. */
        clear();
        grid[5][4] = MZM_CLIPDATA_SOLID;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_STEAM_SMALL, 0, 5, 5, 0, 0);
        uint16_t steam_x = room[0].x;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].status & MZM_SPRITE_STATUS_FACING_DOWN &&
               room[0].anim == MZM_GADGET_STEAM_HORIZONTAL_SMALL && room[0].x == steam_x - 32 &&
               room[0].y == steam_y - 32 && !(room[0].status & MZM_SPRITE_STATUS_X_FLIP));
        /* A diagonal vent against that wall: not flipped in X, down. */
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_STEAM_LARGE_DIAGONAL_DOWN, 0, 5, 5, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(!(room[0].status & MZM_SPRITE_STATUS_X_FLIP) && room[0].status & MZM_SPRITE_STATUS_Y_FLIP &&
               room[0].anim == MZM_GADGET_STEAM_DIAGONAL_LARGE && room[0].work1);
        /* In the air, steam does not exist. */
        clear();
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_STEAM_SMALL, 0, 5, 5, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].status == 0);
        /* A water drop: hidden a random while, falls and splashes on the
         * ground, then waits hidden again. */
        clear();
        for (int x = 0; x < COLUMNS; ++x) grid[9][x] = MZM_CLIPDATA_SOLID;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_WATER_DROP, 0, 3, 5, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x11 && room[0].status & MZM_SPRITE_STATUS_NOT_DRAWN &&
               room[0].y == 4 * 64 - 64 && room[0].y_spawn == room[0].y && room[0].draw_order == 1);
        int frames = 0;
        while (room[0].pose != 0x1F && frames++ < 400) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x1F && !(room[0].status & MZM_SPRITE_STATUS_NOT_DRAWN) &&
               room[0].anim == MZM_GADGET_WATER_DROP_FALLING);
        frames = 0;
        while (room[0].pose == 0x1F && frames++ < 120) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].y == 9 * 64 && (room[0].pose == 0xE || room[0].pose == 0xF));
        frames = 0;
        while (room[0].pose != 0x11 && frames++ < 60) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x11 && room[0].status & MZM_SPRITE_STATUS_NOT_DRAWN &&
               room[0].work0 >= 100);
        /* A Geron makes its three blocks solid but not grippable; hit, it
         * shakes; destroyed earlier, it stays an empty stump. */
        clear();
        memset(test_events, 0, sizeof test_events);
        caa_calls = 0;
        mzm_sfx_clear();
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_GERON_BRINSTAR_ROOM_15, 0, 5, 5, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(caa_calls == 3 && last_caa == MZM_CAA_MAKE_NON_POWER_GRIP && room[0].pose == 9 &&
               room[0].anim == MZM_GADGET_GERON_IDLE && room[0].health == 1 &&
               room[0].hitbox_top == -192 && room[0].draw_order == 5);
        room[0].isft = 3;               /* SpriteUtilUpdateStunTimer runs first */
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].anim == MZM_GADGET_GERON_SHAKING &&
               mzm_sfx_requested(MZM_SFX_PLAY_NOT_PLAYING, MZM_SOUND_GERON_SHAKING));
        test_events[47 / 8] |= 1u << (47 % 8);      /* EVENT_PLANT_DESTROYED_LAVA */
        caa_calls = 0;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_GERON_BRINSTAR_ROOM_15, 0, 5, 5, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x25 && room[0].anim == MZM_GADGET_GERON_DESTROYED && caa_calls == 0 &&
               room[0].status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
        /* The post-Varia Geron only after High Jump; the Norfair one goes
         * with the Power Grip. */
        memset(test_events, 0, sizeof test_events);
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_GERON_BRINSTAR_ROOM_1C, 0, 5, 5, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].status == 0);
        caa_calls = 0;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_GERON_NORFAIR, 0, 5, 5, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(caa_calls == 3 && room[0].anim == MZM_GADGET_GERON_NORFAIR_IDLE);
        test_events[16 / 8] |= 1u << (16 % 8);      /* EVENT_POWER_GRIP_OBTAINED */
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_GERON_NORFAIR, 0, 5, 5, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].status == 0);
        /* An unknown item block: 2x2 solid blocks, shots pass through until
         * its item is obtained, then a hit blows it up with its light. */
        memset(test_events, 0, sizeof test_events);
        caa_calls = 0;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_SPACE_JUMP_BLOCK, 0, 5, 5, 0, 0);
        uint16_t block_y = room[0].y, block_x = room[0].x;
        mzm_sprite_update(&room[0], 0, &g);
        assert(caa_calls == 4 && last_caa == MZM_CAA_MAKE_SOLID_GRIPPABLE && room[0].pose == 9 &&
               room[0].status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES && room[0].y == block_y - 64 &&
               room[0].x == block_x + 32);
        test_events[22 / 8] |= 1u << (22 % 8);      /* EVENT_SPACE_JUMP_OBTAINED */
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0xB && room[0].anim == MZM_GADGET_UNKNOWN_ITEM_BLOCK_ACTIVATED &&
               !(room[0].status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES));
        room[0].pose = 0x62;
        caa_calls = 0;
        mzm_sfx_clear();
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x67 && room[0].anim == MZM_GADGET_UNKNOWN_ITEM_BLOCK_EXPLODING &&
               caa_calls == 4 && last_caa == MZM_CAA_REMOVE_SOLID &&
               mzm_sfx_requested(MZM_SFX_PLAY_NOT_PLAYING, MZM_SOUND_UNKNOWN_ITEM_BLOCK_BREAKING));
        room[0].frame = 7;
        room[0].adc = 2;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[1].status & MZM_SPRITE_STATUS_EXISTS && room[1].secondary &&
               room[1].sprite_id == MZM_SSPRITE_UNKNOWN_ITEM_BLOCK_LIGHT &&
               room[1].anim == MZM_GADGET_UNKNOWN_ITEM_BLOCK_LIGHT && room[1].draw_bottom == 80);
        mzm_sprite_update(&room[1], 1, &g);
        assert(room[1].draw_top == 0);
        /* A map station: inactive once the area's map is downloaded; else
         * it grabs Samus standing on its platform, downloads (the pause
         * flag), shows the map message, waits for the banner, retracts. */
        memset(room, 0, sizeof room);
        static MzmSpriteGlobals station_globals;
        memset(&station_globals, 0, sizeof station_globals);
        g.globals = &station_globals;
        g.area = 0;
        g.downloaded_maps = 1;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_MAP_STATION, 0, 5, 5, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0xF && room[0].anim == MZM_GADGET_MAP_STATION_INACTIVE && room[0].frame == 9 &&
               !(room[1].status & MZM_SPRITE_STATUS_EXISTS));
        memset(room, 0, sizeof room);
        g.downloaded_maps = 0;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_MAP_STATION, 0, 5, 5, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 9 && room[1].sprite_id == MZM_SSPRITE_MAP_STATION_PART &&
               room[2].sprite_id == MZM_SSPRITE_MAP_STATION_PART && room[2].room_slot == 1);
        mzm_sprite_update(&room[1], 1, &g);
        assert(room[1].anim == MZM_GADGET_MAP_STATION_BACK_SCREEN_IDLE);
        uint16_t station_x = (uint16_t)(room[0].x + 96), station_y = (uint16_t)(room[0].y + 128 - 1);
        MzmSpriteSamusLink station_link = {MZM_LINK_POSE_OTHER, station_y, (uint16_t)(station_x + 64),
                                           false, false, false, true, MZM_POSE_NONE, false, false, false, 0, MZM_POSE_STANDING, false, false, 0};
        g.samus_link = &station_link;
        mzm_sfx_clear();
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 9 && station_link.set_pose == MZM_POSE_NONE);
        station_link.x = (uint16_t)(station_x + 20);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x23 && station_link.moved && station_link.x == station_x &&
               station_link.clear_invincibility &&
               station_link.set_pose == MZM_POSE_TURNING_AROUND_TO_DOWNLOAD_MAP_DATA &&
               mzm_sfx_requested(MZM_SFX_PLAY, 0x123));
        int station_frames = 0;
        while (room[0].pose == 0x23 && station_frames++ < 40) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x25 && station_link.set_timer && station_link.timer == 0);
        mzm_sprite_update(&room[1], 1, &g);
        assert(room[1].anim == MZM_GADGET_MAP_STATION_BACK_SCREEN_DOWNLOADING);
        for (station_frames = 0; station_frames < 59; ++station_frames) mzm_sprite_update(&room[0], 0, &g);
        assert(station_globals.pause_screen_flag == 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(station_globals.pause_screen_flag == 5 && room[0].pose == 0x25);
        for (station_frames = 0; station_frames < 10; ++station_frames) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x27 && room[0].anim == MZM_GADGET_MAP_STATION_RETRACTING);
        message_count = 0;
        station_link.set_timer = station_link.reset_frame = false;
        for (station_frames = 0; station_frames < 10; ++station_frames) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x29 && message_count == 1 && messages[0] == 27 && station_link.set_timer &&
               station_link.timer == 1 && station_link.reset_frame && mzm_sfx_requested(MZM_SFX_FADE, 0x123));
        g.banner_pose = MZM_BANNER_POSE_REMOVAL_ANIMATION;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x2B);
        station_link.set_pose = MZM_POSE_NONE;
        for (station_frames = 0; station_frames < 11; ++station_frames) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x2F && room[0].anim == MZM_GADGET_MAP_STATION_INACTIVE &&
               station_link.set_pose == MZM_POSE_STANDING && mzm_sfx_requested(MZM_SFX_PLAY, 0x124));
        /* A save platform: opens under Samus standing still, grabs her for
         * the save prompt, saves (tube, shadow, ray, flashing), shows the
         * save complete message and releases her. */
        memset(room, 0, sizeof room);
        memset(&station_globals, 0, sizeof station_globals);
        g.banner_pose = 0;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_SAVE_PLATFORM, 0, 5, 5, 0, 0);
        g.samus_link = NULL;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 9 && room[0].anim == MZM_GADGET_SAVE_PLATFORM_IDLE && room[0].work3 == 1 &&
               room[1].sprite_id == MZM_SSPRITE_SAVE_PLATFORM_PART && room[1].room_slot == 3 &&
               room[1].y == (uint16_t)(room[0].y - 512));
        mzm_sprite_update(&room[1], 1, &g);
        assert(room[1].pose == 0x43 && room[1].anim == MZM_GADGET_SAVE_PLATFORM_TOP_IDLE);
        MzmSpriteSamusLink save_link = {MZM_LINK_POSE_OTHER, (uint16_t)(room[0].y - 65), (uint16_t)(room[0].x + 20),
                                        false, false, false, false, MZM_POSE_NONE, false, false, false, 0, MZM_POSE_STANDING, false, false, 0};
        g.samus_link = &save_link;
        mzm_sfx_clear();
        for (int i = 0; i < 9; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 9);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x23 && mzm_sfx_requested(MZM_SFX_PLAY, 0x112));
        int save_frames = 0;
        while (room[0].pose == 0x23 && save_frames++ < 40) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x25);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x42 && save_link.moved && save_link.x == room[0].x &&
               save_link.set_pose == MZM_POSE_TURNING_FROM_FACING_THE_FOREGROUND && save_link.set_timer &&
               save_link.timer == 1 && station_globals.disable_pause);
        message_count = 0;
        for (int i = 0; i < 4; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(message_count == 1 && messages[0] == 22);
        g.banner_pose = MZM_BANNER_POSE_REMOVAL_ANIMATION;
        g.banner_yes = true;
        save_link.set_timer = false;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x43 && room[0].anim == MZM_GADGET_SAVE_PLATFORM_SAVING && room[1].pose == 0x44 &&
               room[2].sprite_id == MZM_SSPRITE_SAVE_PLATFORM_PART && room[2].room_slot == 0 &&
               save_link.set_timer && save_link.timer == 0 && mzm_sfx_requested(MZM_SFX_PLAY, 0x114));
        g.banner_pose = 0;
        /* SavePlatformSaving: rows 2 and 0 alternate with the frames. */
        room[0].frame = 0;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].palette_row == 2 && room[1].palette_row == 2);
        /* The tube spawns its shadow, then the ray once the shadow is gone;
         * the ray rises and turns the platform off. */
        mzm_sprite_update(&room[2], 2, &g);
        assert(room[2].pose == 9 && room[2].anim == MZM_GADGET_SAVE_PLATFORM_TUBE_SPAWNING);
        save_frames = 0;
        while (room[2].pose == 9 && save_frames++ < 40) mzm_sprite_update(&room[2], 2, &g);
        assert(room[2].pose == 0x23 && room[3].room_slot == 1);
        room[3].status = 0;
        mzm_sprite_update(&room[2], 2, &g);
        assert(room[2].pose == 0x25 && room[2].y_spawn == 120 && room[3].room_slot == 2);
        mzm_sprite_update(&room[3], 3, &g);
        uint16_t ray_y = room[3].y;
        for (int i = 0; i < 96; ++i) mzm_sprite_update(&room[3], 3, &g);
        assert(!(room[3].status & MZM_SPRITE_STATUS_EXISTS) && room[3].y == (uint16_t)(ray_y - 96 * 4) &&
               room[0].pose == 0x45);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x47 && room[0].palette_row == 0 && room[0].anim == MZM_GADGET_SAVE_PLATFORM_OPENED_OFF);
        message_count = 0;
        for (int i = 0; i < 60; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x49 && message_count == 1 && messages[0] == 23);
        g.banner_pose = MZM_BANNER_POSE_REMOVAL_ANIMATION;
        mzm_sprite_update(&room[0], 0, &g);
        g.banner_pose = 0;
        save_link.set_pose = MZM_POSE_NONE;
        for (int i = 0; i < 11; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x29 && save_link.set_pose == MZM_POSE_FACING_THE_FOREGROUND &&
               !station_globals.disable_pause);
        /* Samus leaves: it closes. */
        save_link.x = (uint16_t)(room[0].x + 200);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x27 && mzm_sfx_requested(MZM_SFX_PLAY, 0x113));
        /* A loaded file: the platform releases Samus from its tube. */
        memset(room, 0, sizeof room);
        station_globals.loading_file = true;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_SAVE_PLATFORM, 0, 5, 5, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x28 && room[0].y_spawn == 340 && room[1].room_slot == 0 && room[2].room_slot == 3);
        mzm_sprite_update(&room[1], 1, &g);
        mzm_sprite_update(&room[2], 2, &g);
        assert(room[1].pose == 0x25 && room[1].y_spawn == 310 && room[2].anim == MZM_GADGET_SAVE_PLATFORM_TOP_EXTENDED);
        /* The gunship at a new game's start: six blocks up, it lands with
         * Samus inside (hidden HUD, the vertical flames), and starts the
         * intro text half a second before touching down. */
        memset(room, 0, sizeof room);
        memset(&station_globals, 0, sizeof station_globals);
        station_globals.landing = true;
        g.banner_pose = 0;
        g.banner_yes = false;
        MzmSpriteSamusLink ship_link = {MZM_LINK_POSE_OTHER, 0, 0, false, false, false, false, MZM_POSE_NONE,
                                        false, false, false, 0, MZM_POSE_STANDING, false, false, 0};
        g.samus_link = &ship_link;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_GUNSHIP, 0, 5, 5, 0, 0);
        uint16_t ship_ground = room[0].y;
        mzm_sfx_clear();
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x55 && room[0].y == (uint16_t)(ship_ground - 384) && room[0].work0 == 192 &&
               room[0].anim == MZM_GADGET_GUNSHIP_IDLE && station_globals.hide_hud &&
               ship_link.set_pose == MZM_POSE_SAVING_LOADING_GAME && ship_link.set_last_wall &&
               ship_link.last_wall == 1 && mzm_sfx_requested(MZM_SFX_PLAY, 0x232));
        assert(room[1].sprite_id == MZM_SSPRITE_GUNSHIP_PART && room[1].room_slot == 1 && room[2].room_slot == 2 &&
               room[3].room_slot == 5);
        mzm_sprite_update(&room[1], 1, &g);
        mzm_sprite_update(&room[3], 3, &g);
        assert(room[1].pose == 0x9 && room[3].pose == 0x34 && room[3].anim == MZM_GADGET_GUNSHIP_FLAMES_VERTICAL &&
               room[3].draw_order == 15);
        for (int i = 0; i < 161; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].work0 == 31 && station_globals.cutscene_effect == 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].work0 == 30 && station_globals.cutscene_effect == 3 &&
               room[0].status & MZM_SPRITE_STATUS_MOSAIC && ship_link.moved && ship_link.x == room[0].x &&
               ship_link.y == (uint16_t)(room[0].y - 4));
        for (int i = 0; i < 40; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].y == ship_ground && room[0].work0 == 0);
        /* Idle on the ground: Samus standing on top enters it, sinks into
         * it, and is refilled; the banner then asks to save. */
        memset(room, 0, sizeof room);
        memset(&station_globals, 0, sizeof station_globals);
        ship_link.samus_pose = MZM_POSE_STANDING;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_GUNSHIP, 0, 5, 5, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x9 && room[0].samus_collision == 2 && room[0].draw_order == 12 &&
               room[3].room_slot == 3 && room[3].y == (uint16_t)(room[0].y - 192 + 32));
        static MzmEquipment ship_items;
        memset(&ship_items, 0, sizeof ship_items);
        ship_items.energy = 30;
        ship_items.max_energy = 99;
        g.equipment = &ship_items;
        ship_link = (MzmSpriteSamusLink){MZM_LINK_POSE_OTHER, (uint16_t)(room[0].y - 196), (uint16_t)(room[0].x + 20),
                                         false, false, false, false, MZM_POSE_NONE, false, false, false, 0,
                                         MZM_POSE_STANDING, false, false, 0};
        room[0].status |= MZM_SPRITE_STATUS_SAMUS_ON_TOP;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x22 && room[0].draw_order == 4 && room[0].samus_collision == 0 &&
               ship_link.set_pose == MZM_POSE_TURNING_FROM_FACING_THE_FOREGROUND && ship_link.standing_enemy &&
               ship_link.x == room[0].x && room[0].work0 == 56);
        uint16_t ship_samus_y = ship_link.y;
        int ship_frames = 0;
        while (room[0].pose == 0x22 && ship_frames++ < 80) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x23 && ship_link.y == (uint16_t)(ship_samus_y + 43 * 4) && room[0].work0 == 65);
        message_count = 0;
        while (room[0].pose == 0x23 && ship_frames++ < 400) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x24 && ship_items.energy == 99 && message_count == 1 && messages[0] == 26 &&
               mzm_sfx_requested(MZM_SFX_PLAY, 0x21E) && mzm_sfx_requested(MZM_SFX_FADE, 0x21E));
        g.banner_pose = MZM_BANNER_POSE_REMOVAL_ANIMATION;
        for (int i = 0; i < 30; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(message_count == 2 && messages[1] == 22);
        g.banner_yes = true;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x42 && mzm_sfx_requested(MZM_SFX_PLAY, 0x21F));
        g.banner_pose = 0;
        for (int i = 0; i < 30; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x43 && message_count == 3 && messages[2] == 23);
        g.banner_pose = MZM_BANNER_POSE_REMOVAL_ANIMATION;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x25);
        /* Samus leaves: the platform rises with her, then she faces the
         * screen. */
        ship_samus_y = ship_link.y;
        while (room[0].pose != 0x29 && ship_frames++ < 600) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x29 && ship_link.y == (uint16_t)(ship_samus_y - 43 * 4) &&
               ship_link.set_pose == MZM_POSE_FACING_THE_FOREGROUND);
        g.equipment = frame.equipment;
        /* The area banner: an area's name at the bottom of the GBA screen
         * for 2 seconds, a room's name in the middle (the save room's
         * shorter), none for the save room while the alarm rings. */
        memset(room, 0, sizeof room);
        memset(&station_globals, 0, sizeof station_globals);
        room[0].status = MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_NOT_DRAWN;
        room[0].sprite_id = MZM_PSPRITE_AREA_BANNER;
        room[0].absolute = true;
        room[0].room_slot = 0;                  /* LT_BRINSTAR */
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x8 && room[0].anim == MZM_GADGET_AREA_BANNER_SPAWN_BOTTOM && room[0].x == 120 &&
               room[0].y == 152 && room[0].always_active);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x9 && !(room[0].status & MZM_SPRITE_STATUS_NOT_DRAWN));
        int banner_frames = 0;
        while (room[0].pose == 0x9 && banner_frames++ < 200) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x23 && room[0].anim == MZM_GADGET_AREA_BANNER_STATIC_BOTTOM && room[0].work0 == 120);
        for (int i = 0; i < 120; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x25 && room[0].anim == MZM_GADGET_AREA_BANNER_REMOVING_BOTTOM);
        banner_frames = 0;
        while (room[0].status && banner_frames++ < 200) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].status == 0);
        memset(room, 0, sizeof room);
        room[0].status = MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_NOT_DRAWN;
        room[0].sprite_id = MZM_PSPRITE_AREA_BANNER;
        room[0].room_slot = 9;                  /* LT_SAVE_ROOM */
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].anim == MZM_GADGET_AREA_BANNER_SPAWN_MIDDLE && room[0].y == 160 / 3 + 1);
        mzm_sprite_update(&room[0], 0, &g);
        banner_frames = 0;
        while (room[0].pose == 0x9 && banner_frames++ < 200) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].anim == MZM_GADGET_AREA_BANNER_STATIC_MIDDLE && room[0].work0 == 50);
        memset(room, 0, sizeof room);
        station_globals.alarm_timer = 100;
        room[0].status = MZM_SPRITE_STATUS_EXISTS;
        room[0].sprite_id = MZM_PSPRITE_AREA_BANNER;
        room[0].room_slot = 9;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].status == 0);
        /* A morph ball launcher: its back, its blocks made non power grip;
         * a fresh bomb set in it is held, then the launch spawns the energy
         * around Samus. */
        memset(room, 0, sizeof room);
        static MzmWeapons launcher_weapons;
        mzm_weapons_init(&launcher_weapons);
        g.weapons = &launcher_weapons;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_MORPH_BALL_LAUNCHER, 0, 5, 5, 0, 0);
        uint16_t launcher_y = room[0].y;
        caa_calls = 0;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x9 && room[0].y == (uint16_t)(launcher_y - 32) && caa_calls == 5 &&
               last_caa == MZM_CAA_MAKE_NON_POWER_GRIP && room[1].sprite_id == MZM_SSPRITE_MORPH_BALL_LAUNCHER_PART &&
               room[1].room_slot == 0);
        launcher_weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BOMB,
                                                   .bomb_stage = MZM_BOMB_FIRST_SPIN,
                                                   .x = room[0].x, .y = room[0].y + 30};
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0xB && launcher_weapons.list[0].bomb_stage == MZM_BOMB_PLACED_ON_LAUNCHER &&
               room[0].work0 == 32);
        for (int i = 0; i < 32; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0xC && room[0].anim == MZM_GADGET_MORPH_BALL_LAUNCHER_LAUNCHING);
        MzmSpriteSamusLink launch_link = {MZM_LINK_POSE_OTHER, (uint16_t)(room[0].y + 30), room[0].x,
                                          false, false, false, false, MZM_POSE_NONE, false, false, false, 0,
                                          MZM_POSE_DELAY_BEFORE_BALLSPARKING, false, false, 0};
        g.samus_link = &launch_link;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].work1 && room[2].sprite_id == MZM_SSPRITE_MORPH_BALL_LAUNCHER_PART && room[2].room_slot == 1);
        for (int i = 0; i < 59; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x9);
        /* A zipline between two stop-enemy blocks (initial direction away
         * from the one beside it), grabbed from below while mid-air: Samus
         * hangs under it; once the ziplines are activated it carries her
         * to the other block, snaps to its middle, drops her and turns. */
        memset(room, 0, sizeof room);
        memset(test_events, 0, sizeof test_events);
        clear();
        movement_grid[5][6] = movement_grid[5][2] = MZM_CLIPDATA_MOVEMENT_STOP_ENEMY_BLOCK_SOLID;
        g.weapons = NULL;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_ZIPLINE, 0, 5, 5, 0, 0);
        room[0].status |= MZM_SPRITE_STATUS_FACING_RIGHT;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x9 && !(room[0].status & MZM_SPRITE_STATUS_FACING_RIGHT) &&
               room[0].anim == MZM_GADGET_ZIPLINE_OFF_IDLE && room[0].samus_collision == 26 /* SSC_ZIPLINE */ &&
               room[0].solid_for_projectiles && room[0].health == 1 && room[0].draw_order == 3);
        MzmSpriteSamus zip = {.x = 340, .y = (uint16_t)(room[0].y + 60), .previous_x = 340,
                              .hitbox_top = -70, .hitbox_left = -24, .hitbox_right = 24,
                              .hurt = hurt, .has_energy = true, .facing = 1,
                              .pose = MZM_POSE_STANDING, .set_pose = MZM_POSE_NONE};
        mzm_sprites_samus_collision(room, 4, &zip, g.tables);
        mzm_sprites_samus_collision(room, 4, &zip, g.tables);
        /* Not mid-air: only the contact (and its ignore timer). */
        assert(zip.set_pose == MZM_POSE_NONE && room[0].ignore_samus_timer == 14);
        room[0].ignore_samus_timer = 0;
        zip.pose = MZM_POSE_MIDAIR;
        mzm_sprites_samus_collision(room, 4, &zip, g.tables);
        assert(zip.set_pose == MZM_POSE_ON_ZIPLINE && zip.y == room[0].y + 168 && zip.x == 352 &&
               zip.previous_x == 352 && room[0].status & MZM_SPRITE_STATUS_SAMUS_COLLIDING);
        MzmSpriteSamusLink zip_link = {MZM_LINK_POSE_OTHER, zip.y, zip.x, false, false, false, false,
                                       MZM_POSE_NONE, false, false, false, 0, MZM_POSE_ON_ZIPLINE,
                                       false, false, 0};
        g.samus_link = &zip_link;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].work1 == 1 && room[0].anim == MZM_GADGET_ZIPLINE_OFF_GRABBING_SAMUS);
        for (int i = 0; i < 30; ++i) mzm_sprite_update(&room[0], 0, &g);
        /* Off, it holds her without moving. */
        assert(room[0].work1 == 2 && room[0].anim == MZM_GADGET_ZIPLINE_OFF_SAMUS_GRABBED &&
               room[0].health == 1 && room[0].x == 352);
        /* Let go (a jump): it releases her and ignores her for a while. */
        zip_link.samus_pose = MZM_POSE_MIDAIR;
        mzm_sprite_update(&room[0], 0, &g);
        assert(!(room[0].status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) && room[0].ignore_samus_timer == 15 &&
               room[0].anim == MZM_GADGET_ZIPLINE_OFF_RELEASING_SAMUS);
        for (int i = 0; i < 30; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].work1 == 0 && room[0].anim == MZM_GADGET_ZIPLINE_OFF_IDLE);
        test_set_event(NULL, MZM_EVENT_ZIPLINES_ACTIVATED);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].anim == MZM_GADGET_ZIPLINE_ON_IDLE);
        room[0].status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        zip_link.samus_pose = MZM_POSE_ON_ZIPLINE;
        int zip_frames = 0;
        while (room[0].health != 2 && zip_frames++ < 40) mzm_sprite_update(&room[0], 0, &g);
        /* It moves in the frame it starts. */
        assert(room[0].anim == MZM_GADGET_ZIPLINE_ON_SAMUS_GRABBED && room[0].work1 == 2);
        assert(room[0].x == 340 && zip_link.x == 340 && zip_link.moved && room[0].work0 == 1);
        zip_frames = 0;
        while (room[0].health == 2 && zip_frames++ < 40) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].x == 224 && zip_link.x == 224 && zip_link.set_pose == MZM_POSE_MID_AIR_REQUEST &&
               room[0].status & MZM_SPRITE_STATUS_FACING_RIGHT && room[0].work0 == 0 &&
               !(room[0].status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) && room[0].ignore_samus_timer == 15);
        /* In morph ball, she is held inside it. */
        room[0].ignore_samus_timer = 0;
        zip.x = zip.previous_x = 224;
        zip.y = (uint16_t)(room[0].y + 60);
        zip.pose = MZM_POSE_MORPH_BALL_MIDAIR;
        zip.set_pose = MZM_POSE_NONE;
        mzm_sprites_samus_collision(room, 4, &zip, g.tables);
        assert(zip.set_pose == MZM_POSE_MORPH_BALL_ON_ZIPLINE && zip.y == room[0].y + 60);
        /* A frozen sprite drops Samus from a zipline. */
        room[1] = room[0];
        room[1].samus_collision = 4;            /* SSC_HURTS_SAMUS */
        room[1].freeze_timer = 100;
        room[1].ignore_samus_timer = 0;
        room[0].ignore_samus_timer = 10;
        zip.pose = MZM_POSE_ON_ZIPLINE;
        zip.set_pose = MZM_POSE_NONE;
        MzmSpriteWorld zip_world = *g.world;
        zip.world = &zip_world;
        mzm_sprites_samus_collision(room, 4, &zip, g.tables);
        assert(zip.set_pose == MZM_POSE_MID_AIR_REQUEST);
        memset(&room[1], 0, sizeof room[1]);
        /* The button: bound to the zipline, lit; a shot (solid for
         * projectiles: no damage) calls it, until it stops. */
        mzm_sprite_init_primary(&room[2], MZM_PSPRITE_ZIPLINE_BUTTON, 0, 5, 8, 0, 2);
        uint16_t button_y = room[2].y;
        mzm_sprite_update(&room[2], 2, &g);
        assert(room[2].pose == 0x1 && room[2].y == button_y - 128 && room[2].solid_for_projectiles &&
               room[2].hitbox_bottom == 80 && room[2].hitbox_left == -20);
        mzm_sprite_update(&room[2], 2, &g);
        assert(room[2].pose == 0x9 && room[2].work1 == 0 && room[2].anim == MZM_GADGET_ZIPLINE_BUTTON_ON_IDLE);
        static MzmWeapons zip_weapons;
        mzm_weapons_init(&zip_weapons);
        zip_weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM,
                                              .x = room[2].x, .y = room[2].y + 40};
        mzm_sprites_projectile_hits(room, 4, &zip_weapons, g.tables, NULL);
        assert(!zip_weapons.list[0].active && room[2].health == 1 && (room[2].isft & 0x7F) == 3);
        mzm_sprite_update(&room[2], 2, &g);
        assert(room[2].pose == 0x23 && room[2].anim == MZM_GADGET_ZIPLINE_BUTTON_ACTIVE && room[0].health == 2);
        zip_frames = 0;
        while (room[0].health == 2 && zip_frames++ < 40) mzm_sprite_update(&room[0], 0, &g);
        /* Without Samus it moves a quarter block a frame. */
        assert(room[0].x == 352 && !(room[0].status & MZM_SPRITE_STATUS_FACING_RIGHT));
        mzm_sprite_update(&room[2], 2, &g);
        assert(room[2].pose == 0x9);
        /* Without a zipline, the button is removed. */
        memset(room, 0, sizeof room);
        mzm_sprite_init_primary(&room[2], MZM_PSPRITE_ZIPLINE_BUTTON, 0, 5, 8, 0, 2);
        mzm_sprite_update(&room[2], 2, &g);
        mzm_sprite_update(&room[2], 2, &g);
        assert(room[2].status == 0);
        /* The generator: its conductor, solid grippable blocks; Samus in
         * morph ball on top starts two seconds of activation (the morph
         * symbol, the electricity, the animated palette back on), then the
         * event and her morph ball. */
        memset(room, 0, sizeof room);
        memset(test_events, 0, sizeof test_events);
        caa_calls = 0;
        MzmSpriteGlobals zip_globals = {0};
        g.globals = &zip_globals;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_ZIPLINE_GENERATOR, 0, 7, 3, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x9 && room[0].anim == MZM_GADGET_ZIPLINE_GENERATOR_DEACTIVATED &&
               room[1].sprite_id == MZM_SSPRITE_ZIPLINE_GENERATOR_PART &&
               room[1].anim == MZM_GADGET_ZIPLINE_GENERATOR_CONDUCTOR_DEACTIVATED && caa_calls == 4 &&
               last_caa == MZM_CAA_MAKE_SOLID_GRIPPABLE && room[0].status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
        mzm_sprite_update(&room[1], 1, &g);
        assert(room[1].pose == 0x61 && room[1].draw_order == 13 && !(room[1].status & MZM_SPRITE_STATUS_NOT_DRAWN));
        MzmSpriteSamusLink gen_link = {MZM_LINK_POSE_MORPH_BALL, (uint16_t)(room[0].y - 129), room[0].x,
                                       false, false, false, false, MZM_POSE_NONE, false, false, false, 0,
                                       MZM_POSE_STANDING, false, false, 0};
        g.samus_link = &gen_link;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x9);
        gen_link.samus_pose = MZM_POSE_MORPH_BALL;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0xB && room[0].work0 == 120 && gen_link.set_pose == MZM_POSE_ACTIVATING_ZIPLINES &&
               room[1].anim == MZM_GADGET_ZIPLINE_GENERATOR_CONDUCTOR_ACTIVATING && room[0].work2 == 2 &&
               room[2].room_slot == 1 && room[2].y == gen_link.y - 24);
        mzm_sprite_update(&room[2], 2, &g);
        assert(room[2].anim == MZM_GADGET_ZIPLINE_GENERATOR_MORPH_SYMBOL_ACTIVATING && room[2].always_active);
        for (int i = 0; i < 30; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].work3 == 3 && room[3].room_slot == 2 && room[3].y == room[0].y - 384);
        for (int i = 0; i < 74; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].work0 == 16 && zip_globals.enable_animated_palette && room[2].pose == 0xB &&
               room[2].anim == MZM_GADGET_ZIPLINE_GENERATOR_MORPH_SYMBOL_ACTIVATED);
        for (int i = 0; i < 16; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0xC && gen_link.set_pose == MZM_POSE_MORPH_BALL && room[3].status == 0 &&
               test_check_event(NULL, MZM_EVENT_ZIPLINES_ACTIVATED) &&
               room[1].anim == MZM_GADGET_ZIPLINE_GENERATOR_CONDUCTOR_ACTIVATED);
        /* Already activated. */
        memset(room, 0, sizeof room);
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_ZIPLINE_GENERATOR, 0, 7, 3, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0xF && room[0].anim == MZM_GADGET_ZIPLINE_GENERATOR_ACTIVATED);
        /* Security gates: open, they close when the alarm rings, become
         * solid once Samus is out of them, and open two thirds of a second
         * after it stops; the default closed ones do the opposite. */
        memset(room, 0, sizeof room);
        memset(&zip_globals, 0, sizeof zip_globals);
        g.samus_link = NULL;
        g.samus_x = 100;
        g.samus_y = 100;
        g.samus_hitbox_top = -40;
        g.samus_hitbox_left = -20;
        g.samus_hitbox_right = 20;
        caa_calls = 0;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_SECURITY_GATE_DEFAULT_OPEN, 0, 7, 6, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x9 && room[0].anim == MZM_GADGET_SECURITY_GATE_OPENED && caa_calls == 0 &&
               room[0].immune_to_projectiles && room[0].status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES &&
               room[0].hitbox_top == -256 && room[0].draw_order == 3);
        zip_globals.alarm_timer = 500;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x23 && room[0].anim == MZM_GADGET_SECURITY_GATE_CLOSING &&
               !(room[0].status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES));
        int gate_frames = 0;
        while (room[0].pose == 0x23 && gate_frames++ < 40) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x25 && room[0].anim == MZM_GADGET_SECURITY_GATE_CLOSED && !room[0].work0);
        /* Samus inside the gate: not solid yet. */
        g.samus_x = room[0].x;
        g.samus_y = (uint16_t)(room[0].y - 10);
        mzm_sprite_update(&room[0], 0, &g);
        assert(!room[0].work0 && caa_calls == 0);
        g.samus_x = 100;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].work0 == 1 && caa_calls == 4 && last_caa == MZM_CAA_MAKE_NON_POWER_GRIP);
        zip_globals.alarm_timer = 0;
        for (int i = 0; i < 40; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x25 && room[0].work3 == 40);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x27 && caa_calls == 8 && last_caa == MZM_CAA_REMOVE_SOLID &&
               room[0].anim == MZM_GADGET_SECURITY_GATE_OPENING);
        gate_frames = 0;
        while (room[0].pose == 0x27 && gate_frames++ < 40) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x9 && room[0].anim == MZM_GADGET_SECURITY_GATE_OPENED);
        memset(room, 0, sizeof room);
        caa_calls = 0;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_SECURITY_GATE_DEFAULT_CLOSED, 0, 7, 6, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x25 && room[0].work0 && caa_calls == 4);
        zip_globals.alarm_timer = 500;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x27 && last_caa == MZM_CAA_REMOVE_SOLID);
        g.samus_hitbox_top = g.samus_hitbox_left = g.samus_hitbox_right = 0;
        g.globals = frame.globals;
        /* A piston: its four body blocks, then its bottom block once Samus
         * is away; a damaging hit on its head opens it (the bottom block
         * removed), then its top block turns solid. */
        memset(room, 0, sizeof room);
        caa_calls = 0;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_PISTON, 0, 7, 6, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x9 && caa_calls == 5 && last_caa == MZM_CAA_MAKE_NON_POWER_GRIP &&
               room[0].status & MZM_SPRITE_STATUS_SAMUS_COLLIDING && room[0].health == 256 &&
               !(room[0].status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES) && room[0].hitbox_top == -116 &&
               room[0].hitbox_bottom == -108 && room[0].anim == MZM_GADGET_PISTON_IDLE);
        /* A beam hit (ISFT_DAMAGED: sPrimarySpriteStats gives it
         * WEAKNESS_BEAM_BOMBS), seen a frame later. */
        tables.weakness[MZM_PSPRITE_PISTON] = MZM_WEAKNESS_BEAM_BOMBS;
        static MzmWeapons piston_weapons;
        mzm_weapons_init(&piston_weapons);
        piston_weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM,
                                                 .x = room[0].x, .y = room[0].y - 112};
        mzm_sprites_projectile_hits(room, 4, &piston_weapons, g.tables, NULL);
        assert((room[0].isft & 0x7F) == 17 && room[0].health < 256);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x23 && room[0].health == 256 && (room[0].isft & 0x7F) == 0 &&
               room[0].status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES &&
               room[0].anim == MZM_GADGET_PISTON_OPENING);
        int piston_frames = 0;
        while (room[0].pose == 0x23 && piston_frames++ < 40) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x25 && caa_calls == 6 && room[0].hitbox_top == -180 &&
               room[0].anim == MZM_GADGET_PISTON_OPENED);
        mzm_sprite_update(&room[0], 0, &g);
        assert(caa_calls == 7 && last_caa == MZM_CAA_MAKE_NON_POWER_GRIP &&
               room[0].status & MZM_SPRITE_STATUS_SAMUS_COLLIDING);
        /* The glass tube: intact until a power bomb touches it after the
         * Fully Powered Suit, then two seconds, the cracking, the opening of
         * its rows with a shake both ways, and the broken tube. */
        memset(room, 0, sizeof room);
        memset(test_events, 0, sizeof test_events);
        caa_calls = 0;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_GLASS_TUBE, 0, 7, 6, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x9 && room[0].anim == MZM_GADGET_GLASS_TUBE_INTACT && caa_calls == 0);
        static MzmWeapons tube_weapons;
        mzm_weapons_init(&tube_weapons);
        MzmEquipment tube_equipment = *frame.equipment;
        tube_equipment.max_power_bombs = 2;
        g.equipment = &tube_equipment;
        g.weapons = &tube_weapons;
        tube_weapons.power_bomb.state = MZM_PB_EXPLODING;
        tube_weapons.power_bomb.x = room[0].x;
        tube_weapons.power_bomb.y = room[0].y;
        tube_weapons.power_bomb.hitbox_left = tube_weapons.power_bomb.hitbox_top = -64;
        tube_weapons.power_bomb.hitbox_right = tube_weapons.power_bomb.hitbox_bottom = 64;
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x9);            /* not before the Fully Powered Suit */
        test_set_event(NULL, MZM_EVENT_FULLY_POWERED_SUIT_OBTAINED);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x23 && room[0].work0 == 120 &&
               test_check_event(NULL, MZM_EVENT_GLASS_TUBE_BROKEN));
        for (int i = 0; i < 120; ++i) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x25 && room[0].anim == MZM_GADGET_GLASS_TUBE_CRACKING);
        mzm_screen_shake_clear();
        int tube_frames = 0;
        while (room[0].pose == 0x25 && tube_frames++ < 40) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0x27 && caa_calls == 22 && last_caa == MZM_CAA_REMOVE_SOLID &&
               mzm_screen_shake.x.timer == 30 && mzm_screen_shake.y.timer == 30);
        tube_frames = 0;
        while (room[0].pose == 0x27 && tube_frames++ < 40) mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0xF && room[0].anim == MZM_GADGET_GLASS_TUBE_BROKEN);
        /* Broken already: its rows removed at once. */
        memset(room, 0, sizeof room);
        caa_calls = 0;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_GLASS_TUBE, 0, 7, 6, 0, 0);
        mzm_sprite_update(&room[0], 0, &g);
        assert(room[0].pose == 0xF && caa_calls == 22);
        /* The rising Chozo pillar: waits for the Power Grip, rises BG2 by
         * half a pixel a frame for 704 frames, then spawns its 3, 2 and 1
         * platforms (two grippable blocks each) and sets its event; later
         * the platforms come at once, the shadowed ones with their shadow. */
        {
            static MzmSprite pillar_room[12];
            memset(pillar_room, 0, sizeof pillar_room);
            MzmSpriteFrame pg = g;
            MzmSpriteGlobals pillar_globals = {0};
            pg.sprites = pillar_room;
            pg.sprite_count = 12;
            pg.globals = &pillar_globals;
            caa_calls = 0;
            mzm_sprite_init_primary(&pillar_room[0], MZM_PSPRITE_RISING_CHOZO_PILLAR, 0, 30, 3, 0, 0);
            mzm_sprite_update(&pillar_room[0], 0, &pg);
            assert(pillar_room[0].pose == 0x8 && pillar_room[0].status & MZM_SPRITE_STATUS_NOT_DRAWN);
            mzm_sprite_update(&pillar_room[0], 0, &pg);
            assert(pillar_room[0].pose == 0x8);
            test_set_event(NULL, MZM_EVENT_POWER_GRIP_OBTAINED);
            mzm_sprite_update(&pillar_room[0], 0, &pg);
            assert(pillar_room[0].pose == 0x9 && pillar_room[0].scaling == 704);
            for (int i = 0; i < 703; ++i) mzm_sprite_update(&pillar_room[0], 0, &pg);
            assert(pillar_globals.bg2_offset_y == 703 * 2 && pillar_room[0].pose == 0x9);
            mzm_sprite_update(&pillar_room[0], 0, &pg);
            assert(pillar_room[0].pose == 0x22 && pillar_globals.bg2_offset_y == 703 * 2);
            int debris = caa_calls;
            assert(debris == 0);
            mzm_sprite_update(&pillar_room[0], 0, &pg);
            assert(pillar_room[0].pose == 0x23 && caa_calls == 6 && last_caa == MZM_CAA_MAKE_SOLID_GRIPPABLE);
            mzm_sprite_update(&pillar_room[0], 0, &pg);
            mzm_sprite_update(&pillar_room[0], 0, &pg);
            assert(pillar_room[0].pose == 0x25 && caa_calls == 12);
            int platforms = 0;
            for (int i = 1; i < 12; ++i)
                if (pillar_room[i].status && pillar_room[i].sprite_id == MZM_SSPRITE_CHOZO_PILLAR_PLATFORM)
                    ++platforms;
            assert(platforms == 6 && pillar_room[1].room_slot == 1 && pillar_room[4].room_slot == 0);
            mzm_sprite_update(&pillar_room[0], 0, &pg);
            mzm_sprite_update(&pillar_room[0], 0, &pg);
            assert(pillar_room[0].status == 0 && test_check_event(NULL, MZM_EVENT_CHOZO_PILLAR_FULLY_EXTENDED));
            /* A platform updated after the event: four pixels down, already
             * spawned. */
            uint16_t platform_y = pillar_room[1].y;
            mzm_sprite_update(&pillar_room[1], 1, &pg);
            assert(pillar_room[1].y == platform_y + 4 && pillar_room[1].pose == 0x9 &&
                   pillar_room[1].anim == MZM_GADGET_CHOZO_PILLAR_PLATFORM_SHADOW_SPAWNED);
            /* On a later visit the platforms come at once, the shadowed ones
             * with their shadow behind Samus. */
            memset(pillar_room, 0, sizeof pillar_room);
            caa_calls = 0;
            mzm_sprite_init_primary(&pillar_room[0], MZM_PSPRITE_RISING_CHOZO_PILLAR, 0, 30, 3, 0, 0);
            mzm_sprite_update(&pillar_room[0], 0, &pg);
            assert(pillar_room[0].status == 0 && caa_calls == 12);
            mzm_sprite_update(&pillar_room[1], 1, &pg);
            /* (the shadow takes the pillar's freed slot) */
            assert(pillar_room[1].pose == 0x9 && pillar_room[0].sprite_id == MZM_SSPRITE_CHOZO_PILLAR_PLATFORM_SHADOW);
            mzm_sprite_update(&pillar_room[0], 0, &pg);
            assert(pillar_room[0].draw_order == 12 && pillar_room[0].anim == MZM_GADGET_CHOZO_PILLAR_PLATFORM_SHADOW);
            memset(test_events, 0, sizeof test_events);
        }
        /* The Chozodia save platform refills Samus before its save prompt:
         * energy, missiles, super missiles and power bombs with their
         * counters' refill animation, the light, then "weapons and energy
         * restored" (suitless: the energy and "energy tank recharge
         * complete"). */
        {
            static MzmSprite pads[8];
            memset(pads, 0, sizeof pads);
            MzmSpriteFrame zf = g;
            MzmSpriteGlobals pad_globals = {0};
            MzmEquipment pad_equipment = *frame.equipment;
            pad_equipment.suit = MZM_SUIT_NORMAL;
            pad_equipment.energy = 30;
            pad_equipment.max_energy = 99;
            pad_equipment.missiles = 0;
            pad_equipment.max_missiles = 10;
            pad_equipment.super_missiles = pad_equipment.max_super_missiles = 2;
            pad_equipment.power_bombs = 1;
            pad_equipment.max_power_bombs = 2;
            MzmSpriteSamusLink pad_link = {0};
            zf.sprites = pads;
            zf.sprite_count = 8;
            zf.globals = &pad_globals;
            zf.equipment = &pad_equipment;
            zf.samus_link = &pad_link;
            zf.banner_pose = 0;
            mzm_sprite_init_primary(&pads[0], MZM_PSPRITE_SAVE_PLATFORM_CHOZODIA, 0, 7, 4, 0, 0);
            mzm_sprite_update(&pads[0], 0, &zf);
            assert(pads[0].pose == 0x9 && pads[0].anim == MZM_GADGET_SAVE_PLATFORM_CHOZODIA_IDLE &&
                   pads[1].sprite_id == MZM_SSPRITE_CHOZODIA_SAVE_PLATFORM_PART && pads[1].room_slot == 3);
            pads[0].pose = 0x54;                /* CHECK_REFILL */
            pads[0].work0 = 0;
            message_count = 0;
            mzm_sprite_update(&pads[0], 0, &zf);
            assert(pads[0].pose == 0x55 && pads[0].work0 == 5 && pad_link.set_timer && pad_link.timer == 0 &&
                   pads[2].sprite_id == MZM_SSPRITE_CHOZODIA_SAVE_PLATFORM_PART && pads[2].room_slot == 4);
            mzm_sprite_update(&pads[2], 2, &zf);
            assert(pads[2].pose == 0x33 && pads[2].anim == MZM_GADGET_SAVE_PLATFORM_CHOZODIA_REFILL_LIGHT);
            int pad_frames = 0;
            while (pads[0].pose == 0x55 && pad_frames++ < 400) mzm_sprite_update(&pads[0], 0, &zf);
            assert(pads[0].pose == 0x56 && pad_equipment.energy == 99 && pad_equipment.missiles == 10 &&
                   pad_equipment.power_bombs == 2 && message_count == 1 && messages[0] == 26 &&
                   pad_link.timer == 1);
            mzm_sprite_update(&pads[2], 2, &zf);
            assert(pads[2].status == 0);
            zf.banner_pose = MZM_BANNER_POSE_REMOVAL_ANIMATION;
            mzm_sprite_update(&pads[0], 0, &zf);
            assert(pads[0].pose == 0x42 && pads[0].work0 == 20);
            /* Full: straight to the prompt. */
            pads[0].pose = 0x54;
            pads[0].work0 = 0;
            mzm_sprite_update(&pads[0], 0, &zf);
            assert(pads[0].pose == 0x42 && pads[0].work0 == 10);
            /* Suitless: only the energy counts. */
            pad_equipment.suit = MZM_SUIT_SUITLESS;
            pad_equipment.energy = 50;
            pad_equipment.missiles = 0;
            pads[0].pose = 0x54;
            pads[0].work0 = 0;
            message_count = 0;
            zf.banner_pose = 0;
            mzm_sprite_update(&pads[0], 0, &zf);
            pad_frames = 0;
            while (pads[0].pose == 0x55 && pad_frames++ < 400) mzm_sprite_update(&pads[0], 0, &zf);
            assert(pads[0].pose == 0x56 && pad_equipment.energy == 99 && pad_equipment.missiles == 0 &&
                   message_count == 1 && messages[0] == 25);
            /* Off during the alarm: its off table (the normal one's opened
             * off) and palette row 3. */
            memset(pads, 0, sizeof pads);
            pad_globals.alarm_timer = 100;
            mzm_sprite_init_primary(&pads[0], MZM_PSPRITE_SAVE_PLATFORM_CHOZODIA, 0, 7, 4, 0, 0);
            mzm_sprite_update(&pads[0], 0, &zf);
            assert(pads[0].pose == 0x51 && pads[0].anim == MZM_GADGET_SAVE_PLATFORM_CHOZODIA_OPENED_OFF &&
                   pads[0].palette_row == 3 && pads[1].palette_row == 3);
        }
        /* Atomics: none on easy; idle hovering, the charge (palette
         * flashing) ending in the electricity shot at Samus (SSC_HURTS_SAMUS_A_LOT:
         * five times the damage), chasing the arm cannon while she charges. */
        {
            static MzmSprite atoms[8];
            static MzmSpriteKind atomic_kind;
            memset(atoms, 0, sizeof atoms);
            memset(&atomic_kind, 0, sizeof atomic_kind);
            for (int a = 0; a < MZM_ATOMIC_ANIMS; ++a) {
                atomic_kind.counts[a] = 3;
                for (int f = 0; f < 3; ++f) atomic_kind.timers[a][f] = 2;
            }
            MzmSpriteFrame af = g;
            af.kinds[MZM_FAMILY_ATOMIC] = &atomic_kind;
            af.sprites = atoms;
            af.sprite_count = 8;
            af.samus_link = NULL;
            af.samus_x = 2000;
            af.samus_y = 2000;
            af.difficulty = 0;
            clear();
            tables.health[MZM_PSPRITE_ATOMIC] = 40;
            tables.damage[MZM_PSPRITE_ATOMIC] = 30;
            mzm_sprite_init_primary(&atoms[0], MZM_PSPRITE_ATOMIC, 0, 5, 5, 0, 0);
            mzm_sprite_update(&atoms[0], 0, &af);
            assert(atoms[0].status == 0);
            af.difficulty = 1;
            mzm_sprite_init_primary(&atoms[0], MZM_PSPRITE_ATOMIC, 0, 5, 5, 0, 0);
            mzm_sprite_update(&atoms[0], 0, &af);
            assert(atoms[0].pose == 0x8 && atoms[0].health == 40 && atoms[0].samus_collision == 4 &&
                   atoms[0].hitbox_top == -32 && atoms[0].y_spawn == atoms[0].y);
            mzm_sprite_update(&atoms[0], 0, &af);
            assert(atoms[0].pose == 0x9);
            /* The charge: the electricity spawns, the palette flashes through
             * rows 0, 2 and 3, then the shot. */
            atoms[0].scaling = 1;
            mzm_sprite_update(&atoms[0], 0, &af);
            assert(atoms[1].sprite_id == MZM_SSPRITE_ATOMIC_ELECTRICITY && atoms[1].secondary);
            mzm_sprite_update(&atoms[1], 1, &af);
            assert(atoms[1].pose == 0x8 && atoms[1].anim == MZM_ATOMIC_ELECTRICITY_CHARGING);
            bool row2 = false, row3 = false;
            int atom_frames = 0;
            while (atoms[0].scaling == 0 && atom_frames++ < 300) {
                mzm_sprite_update(&atoms[0], 0, &af);
                row2 |= atoms[0].palette_row == 2;
                row3 |= atoms[0].palette_row == 3;
            }
            assert(row2 && row3 && atoms[0].scaling == 200 && atoms[0].palette_row == 0);
            af.samus_x = atoms[0].x + 300;
            af.samus_y = (uint16_t)(atoms[0].y + 64 + 8);
            mzm_sprite_update(&atoms[1], 1, &af);
            assert(atoms[1].pose == 0x9 && atoms[1].samus_collision == 5 &&
                   atoms[1].anim == MZM_ATOMIC_ELECTRICITY_MOVING_HORIZONTAL &&
                   atoms[1].status & MZM_SPRITE_STATUS_X_FLIP && atoms[1].hitbox_right == 160);
            uint16_t shot_x = atoms[1].x;
            mzm_sprite_update(&atoms[1], 1, &af);
            assert(atoms[1].x == shot_x + 20);
            /* SSC_HURTS_SAMUS_A_LOT: five times the stats damage. */
            tables.secondary_damage[MZM_SSPRITE_ATOMIC_ELECTRICITY] = 10;
            atoms[1].ignore_samus_timer = 0;
            hurts = 0;
            MzmSpriteSamus zapped = {.x = (uint16_t)(atoms[1].x + 40), .y = (uint16_t)(atoms[1].y + 20),
                                     .previous_x = (uint16_t)(atoms[1].x + 40), .hitbox_top = -96,
                                     .hitbox_left = -24, .hitbox_right = 24, .hurt = hurt,
                                     .has_energy = true, .facing = 1, .pose = MZM_POSE_STANDING,
                                     .set_pose = MZM_POSE_NONE};
            mzm_sprites_samus_collision(atoms, 8, &zapped, g.tables);
            assert(hurts == 1 && last_damage == 50 && !zapped.hurts_a_lot);
            /* Chasing the arm cannon while Samus charges. */
            af.charge_counter = 64;
            af.arm_cannon_x = (uint16_t)(atoms[0].x + 200);
            af.arm_cannon_y = atoms[0].y;
            atoms[0].pose = 0x9;
            mzm_sprite_update(&atoms[0], 0, &af);
            assert(atoms[0].pose == 0x26);
            mzm_sprite_update(&atoms[0], 0, &af);
            assert(atoms[0].pose == 0x27);
            af.charge_counter = 0;
            mzm_sprite_update(&atoms[0], 0, &af);
            assert(atoms[0].pose == 0x8);
            clear();
        }
        /* Polyps: a knockback on contact (no damage), a projectile spat
         * every two seconds on screen, arcing towards Samus and exploding
         * on a block. */
        {
            static MzmSprite plants[6];
            static MzmSpriteKind polyp_kind;
            memset(plants, 0, sizeof plants);
            memset(&polyp_kind, 0, sizeof polyp_kind);
            for (int a = 0; a < MZM_POLYP_ANIMS; ++a) {
                polyp_kind.counts[a] = 3;
                for (int f = 0; f < 3; ++f) polyp_kind.timers[a][f] = 2;
            }
            const int16_t arc[44] = {-8, -8, -6, -4, -2, 0, 2, 4, 6, 8, 32767};
            memcpy(tables.polyp_projectile_y, arc, sizeof arc);
            MzmSpriteFrame pp = g;
            pp.kinds[MZM_FAMILY_POLYP] = &polyp_kind;
            pp.sprites = plants;
            pp.sprite_count = 6;
            pp.samus_x = (uint16_t)(5 * 64 + 32 + 200);
            pp.samus_y = 6 * 64;
            clear();
            for (int x = 0; x < COLUMNS; ++x) grid[9][x] = MZM_CLIPDATA_SOLID;
            mzm_sprite_init_primary(&plants[0], MZM_PSPRITE_POLYP, 0, 5, 5, 0, 0);
            mzm_sprite_update(&plants[0], 0, &pp);
            assert(plants[0].pose == 0x9 && plants[0].samus_collision == 7 && plants[0].work0 == 119);
            int polyp_frames = 0;
            while (plants[0].pose == 0x9 && polyp_frames++ < 200) mzm_sprite_update(&plants[0], 0, &pp);
            assert(plants[0].pose == 0x23 && plants[0].anim == MZM_POLYP_WARNING);
            polyp_frames = 0;
            while (!plants[1].status && polyp_frames++ < 100) mzm_sprite_update(&plants[0], 0, &pp);
            assert(plants[1].sprite_id == MZM_SSPRITE_POLYP_PROJECTILE && plants[0].pose == 0x25 &&
                   plants[0].work0 == 4 && plants[1].y == plants[0].y - 32);
            mzm_sprite_update(&plants[1], 1, &pp);
            assert(plants[1].pose == 0x8 && plants[1].status & MZM_SPRITE_STATUS_FACING_RIGHT &&
                   plants[1].anim == MZM_POLYP_PROJECTILE_RIGHT && plants[1].kill_off_screen);
            for (int i = 0; i < 4; ++i) mzm_sprite_update(&plants[1], 1, &pp);
            assert(plants[1].pose == 0x9 && plants[1].samus_collision == 6);
            uint16_t px = plants[1].x, py = plants[1].y;
            mzm_sprite_update(&plants[1], 1, &pp);
            assert(plants[1].x == px + 4 && plants[1].y == py - 8);
            polyp_frames = 0;
            while (plants[1].pose == 0x9 && polyp_frames++ < 400) mzm_sprite_update(&plants[1], 1, &pp);
            assert(plants[1].pose == 0x42);         /* SPRITE_POSE_STOPPED on the floor */
            mzm_sprite_update(&plants[1], 1, &pp);
            assert(plants[1].pose == 0x43 && plants[1].anim == MZM_POLYP_PROJECTILE_EXPLODING);
            /* While its projectile is out, the Polyp waits. */
            plants[0].pose = 0x9;
            plants[0].work0 = 1;
            mzm_sprite_update(&plants[0], 0, &pp);
            assert(plants[0].pose == 0x9 && plants[0].work0 == 120);
            /* SSC_KNOCKS_BACK_SAMUS */
            knockbacks = 0;
            hurts = 0;
            plants[0].ignore_samus_timer = 0;
            MzmSpriteSamus pushed = {.x = (uint16_t)(plants[0].x - 40), .y = (uint16_t)(plants[0].y + 30),
                                     .previous_x = (uint16_t)(plants[0].x - 40), .hitbox_top = -96,
                                     .hitbox_left = -24, .hitbox_right = 24, .hurt = hurt,
                                     .has_energy = true, .facing = 1, .pose = MZM_POSE_STANDING,
                                     .set_pose = MZM_POSE_NONE, .knockback = knockback};
            mzm_sprites_samus_collision(plants, 1, &pushed, g.tables);
            assert(knockbacks == 1 && hurts == 0 && last_knockback_left);
            clear();
        }
        /* Tourian (zebetite_and_cannon.c): Zebetites heal a phase every
         * half second when not hurt and come back once 20 blocks left;
         * cannons sweep their aims and their bullets fly until a block. */
        {
            static MzmSprite defences[6];
            static MzmSpriteKind tourian_kind;
            memset(defences, 0, sizeof defences);
            memset(&tourian_kind, 0, sizeof tourian_kind);
            for (int a = 0; a < MZM_TOURIAN_ANIMS; ++a) {
                tourian_kind.counts[a] = 3;
                for (int f = 0; f < 3; ++f) tourian_kind.timers[a][f] = 2;
            }
            MzmSpriteFrame tf = g;
            tf.kinds[MZM_FAMILY_TOURIAN] = &tourian_kind;
            tf.sprites = defences;
            tf.sprite_count = 6;
            tf.hooks = &test_hooks;
            tf.difficulty = 1;
            tables.health[MZM_PSPRITE_ZEBETITE_ONE_AND_THREE] = 140;
            clear();
            MzmSprite *z = &defences[0];
            mzm_sprite_init_primary(z, MZM_PSPRITE_ZEBETITE_ONE_AND_THREE, 0, 5, 30, 0, 0);
            uint16_t zx = z->x;
            mzm_sprite_update(z, 0, &tf);
            assert(z->pose == 0x9 && z->health == 140 && z->samus_collision == 12 &&
                   z->anim == MZM_TOURIAN_ZEBETITE_PHASE1);
            z->health = 100;                        /* phase 2 */
            mzm_sprite_update(z, 0, &tf);
            assert(z->anim == MZM_TOURIAN_ZEBETITE_PHASE2 && z->work0 == 30 && z->scaling == 100);
            int zebetite_frames = 0;
            while (z->health == 100 && zebetite_frames++ < 100) mzm_sprite_update(z, 0, &tf);
            assert(z->health == 120 && zebetite_frames == 30);
            mzm_sprite_update(z, 0, &tf);
            assert(z->anim == MZM_TOURIAN_ZEBETITE_PHASE2 && z->scaling == 120);
            /* Destroyed: event one, then the same sprite 20 blocks left. */
            z->health = 0;
            mzm_sprite_update(z, 0, &tf);
            assert(test_check_event(NULL, MZM_EVENT_ZEBETITE_ONE_DESTROYED) &&
                   z->status & MZM_SPRITE_STATUS_NOT_DRAWN && z->pose == 0);
            mzm_sprite_update(z, 0, &tf);
            assert(z->x == zx - 20 * 64 && z->health == 140 && z->pose == 0x9);
            z->health = 0;
            mzm_sprite_update(z, 0, &tf);
            assert(z->status == 0 && test_check_event(NULL, MZM_EVENT_ZEBETITE_ONE_DESTROYED + 2));
            /* Both destroyed: it does not come back. */
            mzm_sprite_init_primary(z, MZM_PSPRITE_ZEBETITE_ONE_AND_THREE, 0, 5, 30, 0, 0);
            mzm_sprite_update(z, 0, &tf);
            assert(z->status == 0);
            /* Easy heals half as fast. */
            tf.difficulty = 0;
            mzm_sprite_init_primary(z, MZM_PSPRITE_ZEBETITE_TWO_AND_FOUR, 0, 5, 30, 0, 0);
            tables.health[MZM_PSPRITE_ZEBETITE_TWO_AND_FOUR] = 140;
            mzm_sprite_update(z, 0, &tf);
            assert(z->work0 == 60 && z->x == zx);
            assert(mzm_sprite_graphics_owner(z) == MZM_PSPRITE_ZEBETITE_ONE_AND_THREE);
            tf.difficulty = 1;
            for (int e = 0; e < 4; ++e)
                test_events[(MZM_EVENT_ZEBETITE_ONE_DESTROYED + e) / 8] &=
                    (uint8_t)~(1u << ((MZM_EVENT_ZEBETITE_ONE_DESTROYED + e) % 8));
            /* A cannon against a wall on its right looks left, then sweeps
             * through the transitions to the left and back. */
            MzmSprite *gun = &defences[0];
            grid[3][11] = MZM_CLIPDATA_SOLID;
            mzm_sprite_init_primary(gun, MZM_PSPRITE_CANNON, 0, 3, 10, 0, 0);
            gun->status |= MZM_SPRITE_STATUS_ONSCREEN;
            tf.samus_x = gun->x;
            uint16_t gy = gun->y;
            mzm_sprite_update(gun, 0, &tf);
            assert(gun->pose == 0x9 && gun->y == gy - 32 && gun->work1 == 2 && gun->work3 == 2 &&
                   gun->anim == MZM_TOURIAN_CANNON_AIMING_DOWN_LEFT &&
                   !(gun->status & MZM_SPRITE_STATUS_FACING_RIGHT));
            memset(tables.random, 15, sizeof tables.random);    /* every arrival shoots */
            int cannon_frames = 0, aims_seen = 0, last_aim = gun->work1;
            bool fired = false;
            while (cannon_frames++ < 5000 && (aims_seen < 8 || !fired)) {
                tf.frame8 = (uint8_t)cannon_frames;
                mzm_sprite_update(gun, 0, &tf);
                if (gun->work1 != last_aim) {
                    assert(gun->work1 == last_aim + 1 || gun->work1 + 1 == last_aim);
                    assert(gun->work1 <= 8);
                    last_aim = gun->work1;
                    ++aims_seen;
                }
                for (int i = 1; i < 6; ++i)
                    if (defences[i].status && defences[i].sprite_id == MZM_SSPRITE_CANNON_BULLET) fired = true;
            }
            assert(aims_seen >= 8 && fired);
            memset(tables.random, 0, sizeof tables.random);
            assert(mzm_sprite_graphics_owner(gun) == MZM_PSPRITE_CANNON);
            /* A bullet aimed down-right: from the mouth, diagonally, until
             * the floor. */
            memset(defences, 0, sizeof defences);
            clear();
            for (int x = 0; x < COLUMNS; ++x) grid[9][x] = MZM_CLIPDATA_SOLID;
            MzmSprite *bullet = &defences[1];
            bullet->status = MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_ONSCREEN;
            bullet->secondary = true;
            bullet->sprite_id = MZM_SSPRITE_CANNON_BULLET;
            bullet->room_slot = 6;                  /* CANNON_AIM_DOWN_RIGHT */
            bullet->y = 300;
            bullet->x = 300;
            mzm_sprite_update(bullet, 1, &tf);
            assert(bullet->pose == 0x9 && bullet->x == 348 && bullet->y == 348 &&
                   bullet->anim == MZM_TOURIAN_CANNON_BULLET_DOWN_RIGHT && bullet->samus_collision == 6 &&
                   mzm_sprite_graphics_owner(bullet) == MZM_PSPRITE_CANNON);
            mzm_sprite_update(bullet, 1, &tf);
            assert(bullet->x == 351 && bullet->y == 351);
            int bullet_frames = 0;
            while (bullet->pose == 0x9 && bullet_frames++ < 400) mzm_sprite_update(bullet, 1, &tf);
            assert(bullet->pose == 0x42 && bullet->y >= 9 * 64);
            mzm_sprite_update(bullet, 1, &tf);
            assert(bullet->status == 0);
            /* Mother Brain dead: no more cannons. */
            test_set_event(NULL, MZM_EVENT_MOTHER_BRAIN_KILLED);
            mzm_sprite_init_primary(gun, MZM_PSPRITE_CANNON2, 0, 3, 10, 0, 0);
            mzm_sprite_update(gun, 0, &tf);
            assert(gun->status == 0);
            test_events[MZM_EVENT_MOTHER_BRAIN_KILLED / 8] &= (uint8_t)~(1u << (MZM_EVENT_MOTHER_BRAIN_KILLED % 8));
            clear();
        }
        /* Worker Robots (worker_robot.c): asleep until hit, then standing
         * half a second and walking until Samus is in front; a missile on
         * the side pushes them away, and they fall when the floor ends. */
        {
            static MzmSprite bots[4];
            static MzmSpriteKind robot_kind;
            static MzmWeapons robot_weapons;
            memset(bots, 0, sizeof bots);
            memset(&robot_kind, 0, sizeof robot_kind);
            for (int a = 0; a < MZM_WORKER_ROBOT_ANIMS; ++a) {
                robot_kind.counts[a] = 3;
                for (int f = 0; f < 3; ++f) robot_kind.timers[a][f] = 2;
            }
            mzm_weapons_init(&robot_weapons);
            MzmSpriteFrame rf = g;
            rf.kinds[MZM_FAMILY_WORKER_ROBOT] = &robot_kind;
            rf.sprites = bots;
            rf.sprite_count = 4;
            rf.weapons = &robot_weapons;
            rf.samus_link = NULL;
            rf.samus_x = 2 * 64;
            rf.samus_y = 9 * 64;
            rf.samus_hitbox_top = -96;
            rf.samus_hitbox_bottom = 0;
            rf.samus_hitbox_left = -24;
            rf.samus_hitbox_right = 24;
            tables.health[MZM_PSPRITE_WORKER_ROBOT] = 1;
            clear();
            for (int x = 0; x < 16; ++x) grid[9][x] = MZM_CLIPDATA_SOLID;
            MzmSprite *bot = &bots[0];
            mzm_sprite_init_primary(bot, MZM_PSPRITE_WORKER_ROBOT, 0, 8, 8, 0, 0);
            bot->status |= MZM_SPRITE_STATUS_ONSCREEN;
            mzm_sprite_update(bot, 0, &rf);
            /* Samus on its left: it looks right (X flip) and sleeps. */
            assert(bot->pose == 0x11 && bot->samus_collision == 1 && bot->immune_to_projectiles &&
                   bot->status & MZM_SPRITE_STATUS_X_FLIP && bot->status & MZM_SPRITE_STATUS_FACING_RIGHT &&
                   bot->anim == MZM_WORKER_ROBOT_SLEEPING);
            for (int i = 0; i < 20; ++i) mzm_sprite_update(bot, 0, &rf);
            assert(bot->pose == 0x11);
            /* A hit's stun flash wakes it. */
            mzm_sfx_clear();
            bot->isft = 2;
            mzm_sprite_update(bot, 0, &rf);
            assert(bot->pose == 0x12);
            mzm_sprite_update(bot, 0, &rf);
            assert(bot->pose == 0x13 && bot->anim == MZM_WORKER_ROBOT_WAKING_UP);
            assert(mzm_sfx_requested(MZM_SFX_PLAY_NOT_PLAYING, MZM_SOUND_WORKER_ROBOT_WAKING_UP));
            bot->isft = 0;
            int robot_frames = 0;
            while (bot->pose != 0xF && robot_frames++ < 100) mzm_sprite_update(bot, 0, &rf);
            assert(bot->pose == 0xF && bot->work0 == 29 && bot->hitbox_top == -132);
            robot_frames = 0;
            while (bot->pose != 0x9 && robot_frames++ < 100) mzm_sprite_update(bot, 0, &rf);
            assert(robot_frames == 30 && bot->anim == MZM_WORKER_ROBOT_WALKING);
            /* A subpixel a frame to the right. */
            uint16_t bx = bot->x;
            mzm_sprite_update(bot, 0, &rf);
            assert(bot->x == bx + 1);
            /* A missile from the left pushes it right, faster first. */
            robot_weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_MISSILE,
                                                    .stage = MZM_STAGE_MOVING,
                                                    .direction = MZM_AIM_FORWARD, .x_flip = true,
                                                    .x = bot->x - 20, .y = bot->y - 64};
            bx = bot->x;
            mzm_sprite_update(bot, 0, &rf);
            assert(!robot_weapons.list[0].active && bot->work1 == 29 && bot->x == bx + 7 &&
                   bot->anim == MZM_WORKER_ROBOT_WALKING);
            robot_frames = 0;
            while (bot->work1 && robot_frames++ < 100) mzm_sprite_update(bot, 0, &rf);
            assert(bot->pose == 0xE || bot->pose == 0xF);
            assert(bot->status & MZM_SPRITE_STATUS_FACING_RIGHT);
            /* A super missile from the right: a second backwards. */
            mzm_sprite_update(bot, 0, &rf);
            robot_weapons.list[1] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_SUPER_MISSILE,
                                                    .stage = MZM_STAGE_MOVING,
                                                    .direction = MZM_AIM_FORWARD, .x_flip = false,
                                                    .x = bot->x + 20, .y = bot->y - 64};
            mzm_sprite_update(bot, 0, &rf);
            assert(!robot_weapons.list[1].active && bot->pose == 0x9 && bot->work1 == 60 &&
                   !(bot->status & MZM_SPRITE_STATUS_FACING_RIGHT) &&
                   bot->anim == MZM_WORKER_ROBOT_WALKING_BACKWARDS);
            /* A tumbling missile is ignored. */
            robot_weapons.list[2] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_MISSILE,
                                                    .stage = MZM_STAGE_TUMBLING, .x = bot->x,
                                                    .y = bot->y - 64};
            mzm_sprite_update(bot, 0, &rf);
            assert(robot_weapons.list[2].active);
            robot_weapons.list[2].active = false;
            robot_frames = 0;
            while (bot->work1 && robot_frames++ < 100) mzm_sprite_update(bot, 0, &rf);
            /* Walking on, it meets Samus in front: back to sleep, then it
             * lies down the other way. */
            while (bot->pose != 0x9 && robot_frames++ < 300) mzm_sprite_update(bot, 0, &rf);
            rf.samus_x = (uint16_t)(bot->x + 80);
            mzm_sprite_update(bot, 0, &rf);
            assert(bot->pose == 0xA);
            mzm_sprite_update(bot, 0, &rf);
            assert(bot->pose == 0xB && bot->anim == MZM_WORKER_ROBOT_BACK_TO_SLEEP);
            robot_frames = 0;
            while (bot->pose != 0x11 && robot_frames++ < 100) mzm_sprite_update(bot, 0, &rf);
            assert(bot->pose == 0x11 && !(bot->status & MZM_SPRITE_STATUS_X_FLIP) &&
                   !(bot->status & MZM_SPRITE_STATUS_FACING_RIGHT) && bot->hitbox_top == -116);
            /* No floor: it falls asleep with its own table, and lands. */
            mzm_sprite_init_primary(&bots[1], MZM_PSPRITE_WORKER_ROBOT, 0, 4, 20, 0, 0);
            const int16_t sleep_fall[10] = {4, 8, 8, 12, 12, 16, 16, 20, 24, 32767};
            memcpy(tables.worker_robot_sleeping_falling, sleep_fall, sizeof sleep_fall);
            for (int x = 16; x < COLUMNS; ++x) grid[9][x] = MZM_CLIPDATA_SOLID;
            uint16_t by = bots[1].y;
            mzm_sprite_update(&bots[1], 1, &rf);
            assert(bots[1].pose == 0x20);
            mzm_sprite_update(&bots[1], 1, &rf);
            assert(bots[1].pose == 0x21 && bots[1].y == by + 4);
            robot_frames = 0;
            while (bots[1].pose == 0x21 && robot_frames++ < 200) mzm_sprite_update(&bots[1], 1, &rf);
            assert(bots[1].pose == 0x10 && bots[1].y == 9 * 64);
            assert(mzm_sprite_graphics_owner(&bots[1]) == MZM_PSPRITE_WORKER_ROBOT);
            clear();
        }
        /* Parasites (parasite.c). */
        {
            static MzmSprite bugs[12];
            static MzmSpriteKind parasite_kind;
            memset(bugs, 0, sizeof bugs);
            memset(&parasite_kind, 0, sizeof parasite_kind);
            for (int a = 0; a < MZM_PARASITE_ANIMS; ++a) {
                parasite_kind.counts[a] = 3;
                for (int f = 0; f < 3; ++f) parasite_kind.timers[a][f] = 2;
            }
            MzmSpriteFrame pf = g;
            MzmSpriteGlobals bug_globals = {0};
            pf.kinds[MZM_FAMILY_PARASITE] = &parasite_kind;
            pf.kinds[MZM_FAMILY_GADGET] = &gadget_kind;
            pf.sprites = bugs;
            pf.sprite_count = 12;
            pf.globals = &bug_globals;
            pf.samus_link = NULL;
            pf.weapons = NULL;
            pf.samus_x = 2000;
            pf.samus_y = 2000;
            clear();
            for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
            /* A single Parasite: its point hitbox, the leech collision, idle
             * on the ground. */
            mzm_sprite_init_primary(&bugs[0], MZM_PSPRITE_PARASITE, 0, 9, 4, 0, 0);
            mzm_sprite_update(&bugs[0], 0, &pf);
            assert(bugs[0].pose == 0x8 && bugs[0].samus_collision == 23 /* SSC_PARASITE */ &&
                   bugs[0].health == 1 && bugs[0].hitbox_top == -4 && bugs[0].hitbox_right == 4 &&
                   bugs[0].anim == MZM_PARASITE_IDLE);
            mzm_sprite_update(&bugs[0], 0, &pf);
            assert(bugs[0].pose == 0x9 && bugs[0].work2 >= 6);
            /* Samus touching it: grabbed, its offsets from her top left
             * corner kept. */
            MzmSpriteSamus held = {.x = bugs[0].x, .y = bugs[0].y, .previous_x = bugs[0].x,
                                   .hitbox_top = -96, .hitbox_left = -24, .hitbox_right = 24,
                                   .hurt = hurt, .has_energy = true, .facing = 1,
                                   .pose = MZM_POSE_STANDING, .set_pose = MZM_POSE_NONE,
                                   .leech = leech, .leech_count = &bug_globals.sub1.work2};
            bugs[0].ignore_samus_timer = 0;
            mzm_sprites_samus_collision(bugs, 12, &held, g.tables);
            assert(bugs[0].status & MZM_SPRITE_STATUS_SAMUS_COLLIDING && leeches == 0 &&
                   bugs[0].ignore_samus_timer == 14);
            pf.samus_x = held.x;
            pf.samus_y = held.y;
            pf.samus_hitbox_top = -96;
            pf.samus_hitbox_left = -24;
            pf.samus_hitbox_right = 24;
            mzm_sprite_update(&bugs[0], 0, &pf);
            assert(bugs[0].pose == 0x42);            /* PARASITE_POSE_SAMUS_GRABBED_INIT */
            mzm_sprite_update(&bugs[0], 0, &pf);
            assert(bugs[0].pose == 0x43 && bugs[0].anim == MZM_PARASITE_ATTACHED &&
                   bug_globals.parasite_related == 90 && !(bugs[0].status & MZM_SPRITE_STATUS_SAMUS_COLLIDING));
            /* Four on her: the leech (1 energy, then invincibility), its
             * sound and every fourth time the hurt sound; three do not. */
            for (int i = 1; i < 4; ++i) {
                bugs[i] = bugs[0];
                bugs[i].ram_slot = (uint8_t)i;
            }
            bugs[3].pose = 0x9;
            for (int i = 0; i < 4; ++i) bugs[i].ignore_samus_timer = 0;
            mzm_sprites_samus_collision(bugs, 12, &held, g.tables);
            assert(leeches == 0);
            bugs[3].pose = 0x43;
            for (int i = 0; i < 4; ++i) bugs[i].ignore_samus_timer = 0;
            mzm_sprites_samus_collision(bugs, 12, &held, g.tables);
            assert(leeches == 1 && held.invincible && bug_globals.sub1.work2 == 1);
            /* The Varia or Gravity Suit is immune. */
            held.invincible = false;
            held.suited = true;
            for (int i = 0; i < 4; ++i) bugs[i].ignore_samus_timer = 0;
            mzm_sprites_samus_collision(bugs, 12, &held, g.tables);
            assert(leeches == 1);
            /* The screw attack throws them off. */
            MzmSpriteSamusLink screw = {0};
            screw.samus_pose = MZM_POSE_SCREW_ATTACKING;
            pf.samus_link = &screw;
            mzm_sprite_update(&bugs[0], 0, &pf);
            assert(bugs[0].pose == 0x44 && bugs[0].work2 >= 6);
            mzm_sprite_update(&bugs[0], 0, &pf);
            assert(bugs[0].pose == 0x45 && bugs[0].anim == MZM_PARASITE_EXPELLED);
            pf.samus_link = NULL;
            /* A beam "kills" it (health 1) but only throws it away; an
             * exploding bomb on it kills it. */
            memset(bugs, 0, sizeof bugs);
            mzm_sprite_init_primary(&bugs[0], MZM_PSPRITE_PARASITE, 0, 9, 4, 0, 0);
            mzm_sprite_update(&bugs[0], 0, &pf);
            mzm_sprite_update(&bugs[0], 0, &pf);
            tables.weakness[MZM_PSPRITE_PARASITE] = MZM_WEAKNESS_BEAM_BOMBS | MZM_WEAKNESS_MISSILES;
            static MzmWeapons bug_weapons;
            mzm_weapons_init(&bug_weapons);
            bug_weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM,
                                                  .x = bugs[0].x, .y = bugs[0].y - 2};
            mzm_sprites_projectile_hits(bugs, 12, &bug_weapons, g.tables, NULL);
            assert(bugs[0].pose == 0x62 && bugs[0].health == 0);
            mzm_sprite_update(&bugs[0], 0, &pf);
            assert(bugs[0].pose == 0x45 && bugs[0].health == 1 && bugs[0].isft == 0 && bugs[0].work2 >= 9);
            bug_weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BOMB,
                                                  .bomb_stage = MZM_BOMB_EXPLODING,
                                                  .x = bugs[0].x, .y = bugs[0].y};
            pf.weapons = &bug_weapons;
            bugs[0].isft = 3;
            mzm_sprite_update(&bugs[0], 0, &pf);
            assert(bugs[0].pose == 0x67 && bugs[0].anim == MZM_PARASITE_DYING);
            int bug_frames = 0;
            while (bugs[0].status && bug_frames++ < 20) mzm_sprite_update(&bugs[0], 0, &pf);
            assert(bugs[0].status == 0);
            pf.weapons = NULL;
            /* Four Parasites on a Geron destroy it; its destruction throws
             * them off. */
            memset(bugs, 0, sizeof bugs);
            memset(test_events, 0, sizeof test_events);
            mzm_sprite_init_primary(&bugs[0], MZM_PSPRITE_GERON_BRINSTAR_ROOM_15, 0, 9, 6, 0, 0);
            mzm_sprite_update(&bugs[0], 0, &pf);
            assert(bugs[0].pose == 0x9);
            for (int i = 1; i <= 4; ++i) {
                mzm_sprite_init_primary(&bugs[i], MZM_PSPRITE_PARASITE, 0, 9, 6, 0, (uint8_t)i);
                mzm_sprite_update(&bugs[i], (uint8_t)i, &pf);
                bugs[i].y = (uint16_t)(bugs[0].y - 40);
                bugs[i].x = (uint16_t)(bugs[0].x + i);
            }
            mzm_sprite_update(&bugs[0], 0, &pf);
            assert(bugs[0].work3 == 4 && bugs[0].pose == 0x23 && bugs[1].pose == 0x4C &&
                   bugs[1].samus_collision == 0 && bugs[1].work1 == 0 &&
                   bugs[1].status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
            mzm_sprite_update(&bugs[1], 1, &pf);
            assert(bugs[1].pose == 0x4D && bugs[1].anim == MZM_PARASITE_ATTACHED);
            mzm_sprite_update(&bugs[1], 1, &pf);
            assert(bugs[1].pose == 0x4D);
            bugs[0].pose = 0x24;                /* GERON_POSE_DELAY_BEFORE_DESTROYED */
            mzm_sprite_update(&bugs[1], 1, &pf);
            assert(bugs[1].pose == 0x44 && bugs[1].samus_collision == 23 &&
                   !(bugs[1].status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES));
            /* The multiple Parasites: the main one spawns five more and locks
             * the doors (with the Bombs); the last to die sets
             * EVENT_BUGS_KILLED and unlocks them. */
            memset(bugs, 0, sizeof bugs);
            MzmEquipment bug_equipment = *frame.equipment;
            bug_equipment.items |= MZM_ITEM_BOMBS;
            pf.equipment = &bug_equipment;
            door_timer_calls = 0;
            mzm_sprite_init_primary(&bugs[0], MZM_PSPRITE_PARASITE_MULTIPLE, 0, 9, 4, 0, 0);
            mzm_sprite_update(&bugs[0], 0, &pf);
            assert(door_timer_calls == 1 && last_door_timer == 1);
            int bug_count = 0;
            for (int i = 0; i < 12; ++i) bug_count += bugs[i].sprite_id == MZM_PSPRITE_PARASITE_MULTIPLE;
            assert(bug_count == 6 && bugs[1].status & MZM_SPRITE_STATUS_NOT_DRAWN && bugs[1].ram_slot == 1);
            mzm_sprite_update(&bugs[1], 1, &pf);
            assert(!(bugs[1].status & MZM_SPRITE_STATUS_NOT_DRAWN));
            for (int i = 0; i < 6; ++i) {
                bugs[i].pose = 0x62;
                if (i < 5) {
                    bug_frames = 0;
                    while (bugs[i].status && bug_frames++ < 20) mzm_sprite_update(&bugs[i], (uint8_t)i, &pf);
                }
            }
            assert(!test_check_event(NULL, 45));
            bug_frames = 0;
            while (bugs[5].status && bug_frames++ < 20) mzm_sprite_update(&bugs[5], 5, &pf);
            assert(test_check_event(NULL, 45) && last_door_timer == -20);
            memset(test_events, 0, sizeof test_events);
            clear();
        }
        mzm_screen_shake_clear();
        memset(test_events, 0, sizeof test_events);
        g.equipment = frame.equipment;
        memset(test_events, 0, sizeof test_events);
        clear();
        g.weapons = NULL;
        g.banner_pose = 0;
        g.banner_yes = false;
        g.samus_link = NULL;
        g.globals = frame.globals;
    }

    /* The Morph Ball, the Power Grip and the Charge Beam: touched, they give
     * their item, hold Samus with the banner and flicker until it lets her
     * go; their secondary sprites follow. */
    {
        static MzmSprite room[6];
        static MzmSpriteKind item_kind;
        memset(room, 0, sizeof room);
        memset(&item_kind, 0, sizeof item_kind);
        for (int a = 0; a < MZM_ITEM_SPRITE_ANIMS; ++a) {
            item_kind.counts[a] = 2;
            item_kind.timers[a][0] = item_kind.timers[a][1] = 3;
        }
        MzmSpriteFrame item_frame = frame;
        uint16_t prevent = 0;
        item_frame.kinds[MZM_FAMILY_ITEM_SPRITE] = &item_kind;
        item_frame.sprites = room;
        item_frame.sprite_count = 6;
        item_frame.hooks = &test_hooks;
        item_frame.prevent_movement = &prevent;
        MzmEquipment items = {.energy = 99, .max_energy = 99};
        item_frame.equipment = &items;
        memset(test_events, 0, sizeof test_events);
        message_count = 0;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_MORPH_BALL, 0, 6, 7, 0, 0);
        mzm_sprite_update(&room[0], 0, &item_frame);
        assert(room[0].pose == 9 && room[0].y == 7 * 64 - 32 && room[0].samus_collision == 30 &&
               room[0].immune_to_projectiles && room[0].draw_order == 3);
        assert(room[1].secondary && room[1].sprite_id == MZM_SSPRITE_MORPH_BALL_OUTSIDE);
        mzm_sprite_update(&room[1], 1, &item_frame);
        assert(room[1].draw_order == 2 && room[1].status == room[0].status &&
               mzm_sprite_graphics_owner(&room[1]) == MZM_PSPRITE_MORPH_BALL);
        room[0].status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        mzm_sprite_update(&room[0], 0, &item_frame);
        assert(room[0].pose == 0x23 && items.items & MZM_ITEM_MORPH_BALL && room[0].always_active &&
               prevent == MZM_SAMUS_ITEM_PMT && message_count == 1 && messages[0] == 16);
        uint16_t before = room[0].status;
        mzm_sprite_update(&room[0], 0, &item_frame);
        assert((room[0].status ^ before) & MZM_SPRITE_STATUS_NOT_DRAWN);
        mzm_sprite_update(&room[1], 1, &item_frame);
        assert(room[1].always_active && room[1].status == room[0].status);
        prevent = MZM_SAMUS_ITEM_PMT - 2;
        mzm_sprite_update(&room[0], 0, &item_frame);
        assert(room[0].status == 0);
        /* Already obtained: removed at once. */
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_MORPH_BALL, 0, 6, 7, 0, 0);
        mzm_sprite_update(&room[0], 0, &item_frame);
        assert(room[0].status == 0);

        /* The Power Grip a block up, its glow gone once taken, its event. */
        memset(room, 0, sizeof room);
        message_count = 0;
        prevent = 0;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_POWER_GRIP, 0, 6, 7, 0, 0);
        mzm_sprite_update(&room[0], 0, &item_frame);
        assert(room[0].pose == 9 && room[0].y == 7 * 64 - 64 &&
               room[1].sprite_id == MZM_SSPRITE_POWER_GRIP_GLOW);
        mzm_sprite_update(&room[1], 1, &item_frame);
        assert(room[1].draw_order == 5 && room[1].pose == 9);
        room[0].status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        mzm_sprite_update(&room[0], 0, &item_frame);
        assert(items.items & MZM_ITEM_POWER_GRIP && messages[0] == 21 &&
               test_events[16 / 8] & (1u << (16 % 8)));
        mzm_sprite_update(&room[1], 1, &item_frame);
        assert(room[1].status == 0);

        /* The Charge Beam: its spawning table, then the glow and the float
         * with sChargeBeamIdleYMovement. */
        memset(room, 0, sizeof room);
        message_count = 0;
        prevent = 0;
        for (int i = 0; i < 65; ++i) tables.charge_beam_idle_y[i] = (int16_t)(i % 2);
        tables.charge_beam_idle_y[65] = 32767;
        mzm_sprite_init_primary(&room[0], MZM_PSPRITE_CHARGE_BEAM, 0, 6, 7, 0, 0);
        mzm_sprite_update(&room[0], 0, &item_frame);
        assert(room[0].pose == 0xA && room[0].samus_collision == 0 &&
               room[0].status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
        int frames = 0;
        while (room[0].pose == 0xA && frames++ < 20) mzm_sprite_update(&room[0], 0, &item_frame);
        assert(room[0].pose == 0xB && room[0].work1 == 1 &&
               room[1].sprite_id == MZM_SSPRITE_CHARGE_BEAM_GLOW);
        mzm_sprite_update(&room[0], 0, &item_frame);
        assert(room[0].pose == 9 && room[0].samus_collision == 30 && room[0].anim ==
               MZM_ITEM_SPRITE_CHARGE_BEAM_VISIBLE);
        uint16_t cy = room[0].y;
        mzm_sprite_update(&room[0], 0, &item_frame);
        mzm_sprite_update(&room[0], 0, &item_frame);
        assert(room[0].y == cy + 1);
        mzm_sprite_update(&room[1], 1, &item_frame);
        assert(room[1].y == room[0].y && room[1].draw_order == 3);
        room[0].status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        mzm_sprite_update(&room[0], 0, &item_frame);
        assert(room[0].pose == 0x23 && room[1].status == 0 && items.items & MZM_ITEM_CHARGE_BEAM &&
               messages[0] == 9 && test_events[20 / 8] & (1u << (20 % 8)));
    }
    puts("mzm_sprites: ok");
    return 0;
}
