/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Zero Mission sprite engine and the Zoomer. */
#include "mzm_sprites.h"

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

static void clear(void) {
    memset(grid, 0, sizeof grid);
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

static const MzmSpriteHooks test_hooks = {NULL, test_check_event, test_set_event,
                                          test_door_timer, test_remove_solid};

static int hurts, last_damage;
static bool last_on_left;

static void hurt(void *context, uint16_t damage, bool sprite_on_left) {
    (void)context;
    ++hurts;
    last_damage = damage;
    last_on_left = sprite_on_left;
}

int main(void) {
    MzmSpriteWorld world = {NULL, block, 0, 0};

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
    static MzmSpriteKind ripper_kind, skree_kind, zeela_kind, waver_kind, hopper_kind, viola_kind, sova_kind, bug_kind, reo_kind, swoop_kind, rinka_kind;
    frame = (MzmSpriteFrame){&world, &tables,
                             {&kind, &zeela_kind, &ripper_kind, &skree_kind, &waver_kind,
                              &drop_kind, &hopper_kind, &hopper_kind, &viola_kind, &sova_kind, &bug_kind, &bug_kind, &bug_kind, &reo_kind, &swoop_kind, &swoop_kind,
                              &swoop_kind, &rinka_kind},
                             &equipment, 0, 0, 0, 0, 0, NULL, 0, NULL, NULL};
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
    MzmSpriteSamus away = {NULL, 0, 0, 0, -96, 0, -24, 24, false, false, false, hurt};
    mzm_sprites_samus_collision(&z, 1, &away, &tables);
    assert(z.ignore_samus_timer == 0 && hurts == 0);

    /* Samus standing to its left, overlapping its hitbox: hurt once with
     * the stats damage, pushed away (the sprite is on her right, "on left"
     * in the native flag naming), then 15 frames of ignored contact. */
    MzmSpriteSamus samus = {NULL, (uint16_t)(z.x - 40), 640, (uint16_t)(z.x - 40),
                            -96, 0, -24, 24, false, false, false, hurt};
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
                             false, false, false, hurt};
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
    step(&z, 0);
    assert(z.status == 0 && equipment.energy == 95 && hurts == 2);

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
     * for 3 frames and are consumed. */
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM,
                                      .x = r.x, .y = r.y - 20};
    weapons.list[1] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_MISSILE,
                                      .x = r.x, .y = r.y - 20};
    mzm_sprites_projectile_hits(&r, 1, &weapons, &tables, NULL);
    assert(r.health == 20 && (r.isft & 0x7F) == 3 && !weapons.list[0].active &&
           !weapons.list[1].active);
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
                           true, false, false, hurt};
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
    step(&z, 0);
    assert(z.status == 0 && fx.particles[1].effect == MZM_PE_SPRITE_EXPLOSION_MEDIUM &&
           fx.particles[1].x == zx2 && fx.particles[1].y == zy2 - 40);
    /* Not weak: the invincible impact. */
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = 0;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0, 0);
    step(&z, 0);
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM,
                                      .x = z.x, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables, &fx);
    assert(fx.particles[2].effect == MZM_PE_HITTING_SOMETHING_INVINCIBLE);
    /* The screw attack kill: its particle and three debris (frame8 even:
     * types 2, 3 and 4). */
    memset(&fx, 0, sizeof fx);
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] = MZM_WEAKNESS_SPEEDBOOSTER_SCREW_ATTACK;
    MzmSpriteSamus screw = {NULL, z.x, (uint16_t)(z.y + 20), z.x, -96, 0, -24, 24,
                            true, false, false, hurt};
    z.ignore_samus_timer = 0;
    mzm_sprites_samus_collision(&z, 1, &screw, &tables);
    assert(z.pose == 0x65);
    step(&z, 0);
    assert(z.status == 0 && fx.particles[0].effect == MZM_PE_SCREW_ATTACK_DESTROYED);
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
                            false, false, false, hurt};
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
    puts("mzm_sprites: ok");
    return 0;
}
