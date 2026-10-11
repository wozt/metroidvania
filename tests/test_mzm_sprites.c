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
    frame = (MzmSpriteFrame){&world, &tables, &kind, &drop_kind, &equipment, 0, 0};
    for (int a = 0; a < MZM_ZOOMER_ANIMS; ++a) {
        kind.counts[a] = 2;
        kind.timers[a][0] = kind.timers[a][1] = 4;
    }

    /* A zoomer placed above a floor (row 10) starts crawling on it, facing
     * right, at 2 subpixels per frame. */
    clear();
    for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
    MzmSprite z;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 3, 0);
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
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_RED, 0, 4, 4, 0);
    step(&z, 0);
    assert(z.status == 0);

    /* A red zoomer under a ceiling hangs from it (Y flip) with palette 1 and
     * crawls at 3 subpixels per frame. */
    for (int x = 0; x < COLUMNS; ++x) grid[3][x] = MZM_CLIPDATA_SOLID;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_RED, 0, 4, 4, 0);
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
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0);
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
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables);
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
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables);
    assert(z.health == 0 && z.pose == 0x62 && !weapons.list[1].active);
    step(&z, 0);
    assert(z.status == 0);

    /* Missiles deal 20; a sprite not weak to beams only flashes. */
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_RED, 0, 9, 6, 0);
    step(&z, 0);
    tables.health[MZM_PSPRITE_ZOOMER_RED] = 30;
    z.health = 30;
    weapons.list[2] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM,
                                      .x = z.x, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables);
    assert(z.health == 30 && (z.isft & 0x7F) == 3 && !weapons.list[2].active);
    tables.weakness[MZM_PSPRITE_ZOOMER_RED] = MZM_WEAKNESS_MISSILES;
    weapons.list[3] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_MISSILE,
                                      .x = z.x + 30, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables);
    assert(z.health == 10 && (z.isft & 0x7F) == 17 && !weapons.list[3].active);
    /* The super missile's larger box (12 subpixels) reaches the hitbox
     * right edge (28) from 39 subpixels away; at 40 the strict test fails. */
    weapons.list[4] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_SUPER_MISSILE,
                                      .x = z.x + 40, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables);
    assert(z.health == 10 && weapons.list[4].active);
    weapons.list[4] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_SUPER_MISSILE,
                                      .x = z.x + 39, .y = z.y - 20};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables);
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
        mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0);
        frame.frame16 = 0;
        step(&z, 0);
        uint16_t zx = z.x, zy = z.y;
        weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM,
                                          .x = z.x, .y = z.y - 20};
        z.health = 1;
        mzm_sprites_projectile_hits(&z, 1, &weapons, &tables);
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
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0);
    z = drop;
    step(&z, 0);
    assert(z.pose == 9 && z.ignore_samus_timer == 20 && z.anim == MZM_DROP_SMALL_ENERGY);
    assert(z.status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES && z.hitbox_top == -36);
    /* Projectiles pass through drops. */
    weapons.list[0] = (MzmProjectile){.active = true, .type = MZM_PROJECTILE_BEAM, .x = z.x, .y = z.y};
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables);
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
    puts("mzm_sprites: ok");
    return 0;
}
