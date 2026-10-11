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
    static MzmSpriteKind kind;
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
    mzm_sprite_update(&z, 0, &world, &tables, &kind, 0, 0);
    assert(z.status & MZM_SPRITE_STATUS_EXISTS && z.health == 3 && z.anim == MZM_ZOOMER_ON_GROUND);
    assert(z.status & MZM_SPRITE_STATUS_FACING_RIGHT && !(z.status & MZM_SPRITE_STATUS_FACING_DOWN));
    assert(z.hitbox_top == -52 && z.hitbox_bottom == 4 && z.hitbox_left == -28);
    uint16_t x0 = z.x;
    for (int i = 0; i < 10; ++i) mzm_sprite_update(&z, 0, &world, &tables, &kind, (uint8_t)i, 0);
    assert(z.x == x0 + 20 && z.y == 640);

    /* The animation: 2 frames of 4 ticks, looping. */
    assert(z.anim == MZM_ZOOMER_ON_GROUND && z.frame <= 1);

    /* A wall (column 8): the zoomer turns and climbs it, facing down along
     * the wall with X flip, its x on the wall's left edge. */
    for (int y = 0; y < 10; ++y) grid[y][8] = MZM_CLIPDATA_SOLID;
    int frames = 0;
    while (!(z.status & MZM_SPRITE_STATUS_FACING_DOWN) && frames < 500) {
        mzm_sprite_update(&z, 0, &world, &tables, &kind, (uint8_t)frames, 0);
        ++frames;
    }
    assert(z.status & MZM_SPRITE_STATUS_FACING_DOWN && z.status & MZM_SPRITE_STATUS_X_FLIP);
    assert(z.x == 8 * 64 && z.anim == MZM_ZOOMER_ON_WALL);
    uint16_t y0 = z.y;
    mzm_sprite_update(&z, 0, &world, &tables, &kind, 0, 0);
    assert(z.y == y0 - 2);

    /* With the wall gone, it falls (4, 4, 4, 4, 8...) back to the floor. */
    for (int y = 0; y < 10; ++y) grid[y][8] = MZM_CLIPDATA_AIR;
    mzm_sprite_update(&z, 0, &world, &tables, &kind, 0, 0);
    assert(z.pose == 0x1E);     /* the next update starts the fall */
    mzm_sprite_update(&z, 0, &world, &tables, &kind, 0, 0);
    assert(z.pose == 0x1F && z.anim == MZM_ZOOMER_FALLING);
    frames = 0;
    while (z.pose == 0x1F && frames < 200) {
        mzm_sprite_update(&z, 0, &world, &tables, &kind, 0, 0);
        ++frames;
    }
    assert(z.pose == 0xF && z.y == 640);

    /* Nothing around it: the zoomer is removed at init. */
    clear();
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_RED, 0, 4, 4, 0);
    mzm_sprite_update(&z, 0, &world, &tables, &kind, 0, 0);
    assert(z.status == 0);

    /* A red zoomer under a ceiling hangs from it (Y flip) with palette 1 and
     * crawls at 3 subpixels per frame. */
    for (int x = 0; x < COLUMNS; ++x) grid[3][x] = MZM_CLIPDATA_SOLID;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_RED, 0, 4, 4, 0);
    mzm_sprite_update(&z, 0, &world, &tables, &kind, 0, 0);
    assert(z.status & MZM_SPRITE_STATUS_Y_FLIP && z.palette_row == 1 && z.y == 5 * 64 - 64);
    x0 = z.x;
    mzm_sprite_update(&z, 0, &world, &tables, &kind, 0, 0);
    assert(z.x == x0 + 3);

    /* On screen within the camera window plus the draw distances. */
    world.camera_x = 0;
    world.camera_y = 0;
    mzm_sprite_update(&z, 0, &world, &tables, &kind, 0, 0);
    assert(z.status & MZM_SPRITE_STATUS_ONSCREEN);
    world.camera_x = 4000;
    mzm_sprite_update(&z, 0, &world, &tables, &kind, 0, 0);
    assert(!(z.status & MZM_SPRITE_STATUS_ONSCREEN));

    /* Combat. A yellow zoomer crawling on the floor, on screen. */
    tables.damage[MZM_PSPRITE_ZOOMER_YELLOW] = 8;
    tables.weakness[MZM_PSPRITE_ZOOMER_YELLOW] =
        MZM_WEAKNESS_BEAM_BOMBS | MZM_WEAKNESS_MISSILES | MZM_WEAKNESS_SPEEDBOOSTER_SCREW_ATTACK;
    clear();
    for (int x = 0; x < COLUMNS; ++x) grid[10][x] = MZM_CLIPDATA_SOLID;
    world.camera_x = world.camera_y = 0;
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0);
    mzm_sprite_update(&z, 0, &world, &tables, &kind, 0, 0);
    assert(z.status & MZM_SPRITE_STATUS_ONSCREEN && z.ignore_samus_timer == 0);

    /* Samus standing to its left, overlapping its hitbox: hurt once with
     * the stats damage, pushed away (the sprite is on her right, "on left"
     * in the native flag naming), then 15 frames of ignored contact. */
    MzmSpriteSamus samus = {NULL, (uint16_t)(z.x - 40), 640, (uint16_t)(z.x - 40),
                            -96, 0, -24, 24, false, false, false, hurt};
    mzm_sprites_samus_collision(&z, 1, &samus, &tables);
    assert(hurts == 1 && last_damage == 8 && last_on_left && samus.invincible);
    assert(z.ignore_samus_timer == 15);
    mzm_sprites_samus_collision(&z, 1, &samus, &tables);
    assert(hurts == 1);
    /* While invincible, contact still sets the ignore timer but no damage. */
    z.ignore_samus_timer = 0;
    mzm_sprites_samus_collision(&z, 1, &samus, &tables);
    assert(hurts == 1 && z.ignore_samus_timer == 15);
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
    mzm_sprite_update(&screwed, 0, &world, &tables, &kind, 0, 0);
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
    mzm_sprite_update(&z, 0, &world, &tables, &kind, 0, 0);
    assert(z.x == x0);  /* stunned */
    for (int i = 0; i < 6; ++i) mzm_sprite_update(&z, 0, &world, &tables, &kind, 0, 0);
    assert(z.x == x0 && (z.isft & 0x7F) == 10);
    mzm_sprite_update(&z, 0, &world, &tables, &kind, 0, 0);
    assert(z.x == x0 + 2);  /* moving again below the stun threshold */
    weapons.list[1].x = z.x;
    mzm_sprites_projectile_hits(&z, 1, &weapons, &tables);
    assert(z.health == 0 && z.pose == 0x62 && !weapons.list[1].active);
    mzm_sprite_update(&z, 0, &world, &tables, &kind, 0, 0);
    assert(z.status == 0);

    /* Missiles deal 20; a sprite not weak to beams only flashes. */
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_RED, 0, 9, 6, 0);
    mzm_sprite_update(&z, 0, &world, &tables, &kind, 0, 0);
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
    puts("mzm_sprites: ok");
    return 0;
}
