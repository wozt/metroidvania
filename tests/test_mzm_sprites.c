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
    static MzmSpriteKind ripper_kind, skree_kind;
    frame = (MzmSpriteFrame){&world, &tables, {&kind, &ripper_kind, &skree_kind, &drop_kind},
                             &equipment, 0, 0, 0, 0, NULL, 0, NULL};
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
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_RED, 0, 9, 6, 0);
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
        mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0);
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
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0);
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
    mzm_sprite_init_primary(&r, MZM_PSPRITE_RIPPER_BROWN, 0, 6, 7, 0);
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
    mzm_sprite_init_primary(&r, MZM_PSPRITE_RIPPER_PURPLE, 0, 6, 7, 0);
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
    mzm_sprite_init_primary(k, MZM_PSPRITE_SKREE_GREEN, 0, 3, 8, 0);
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
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0);
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
    mzm_sprite_init_primary(&z, MZM_PSPRITE_ZOOMER_YELLOW, 0, 9, 6, 0);
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
    puts("mzm_sprites: ok");
    return 0;
}
