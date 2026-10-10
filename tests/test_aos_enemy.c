/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Aria of Sorrow enemy framework, the bat, the zombie
 * and the blue crow. */
#include "aos_enemy.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 1, W, H, cells, NULL, false};

static uint32_t never_zero(void) { return 1; }
static uint32_t always_zero(void) { return 0; }

static void zombie_tests(void) {
    /* Floor at cell row 20 (top 160), a wall at cells x 40.. */
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    for (int y = 0; y < 20; ++y) cells[y * W + 40] = 0x03;
    static const uint8_t rise[] = {2, 2}, walk[] = {6, 6}, sink[] = {2, 2}, death[] = {200};
    const AosAnimDef defs[5] = {{2, rise}, {2, walk}, {2, sink}, {1, death}, {2, walk}};
    const AosAnimSet anims = {defs, 5};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int a = 0; a < 5; ++a)
        for (int f = 0; f < 2; ++f) {
            kind.modes[a][f] = a == 3 ? 0 : 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-5, -31, 8, 31};
        }
    kind.margins[4][0] = kind.margins[4][1] = 48;
    const AosEnemyStats stats = {18, 9, 1, 0x0021, 0x0010};

    /* A zombie record snaps onto the floor and rises, facing the player. */
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(159), NULL);
    AosEnemy z;
    assert(aos_enemy_create(&z, AOS_ENEMY_ZOMBIE, 250, 140, 0, 0, &soma, &layer, &kind, &stats));
    assert(z.state == 0 && z.step == 0 && (z.y >> 16) == 159 && !z.mirrored);
    int frames = 0;
    while (z.step == 0 && frames < 20) {
        aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 48, always_zero);
        ++frames;
    }
    assert(z.step == 1 && z.anim.id == 4 && z.walk_timer == 600);
    /* It walks left in bursts of 2.0 that decay by 3/4 per frame. */
    int32_t x0 = z.x;
    aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 48, always_zero);
    assert(z.x < x0 && (z.y >> 16) == 159);
    /* Turned toward a wall on the right, it stops there and sinks, then
     * vanishes after the 60-frame pause and the sink animation. */
    z.mirrored = true;
    z.x = AOS_FIXED(300);
    frames = 0;
    while (z.step == 1 && frames < 300) {
        aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 48, always_zero);
        ++frames;
    }
    assert(z.step == 2 && (z.x >> 16) <= 320 - 8);
    frames = 0;
    while (!z.removed && frames < 200) {
        aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 48, always_zero);
        ++frames;
    }
    assert(z.removed && frames > 60);

    /* A spawner (param1 != 0) is invisible and creates zombies away from
     * the player, up to param0 of them. */
    AosEnemy spawner;
    assert(aos_enemy_create(&spawner, AOS_ENEMY_ZOMBIE, 120, 159, 2, 1, &soma, &layer, &kind,
                            &stats));
    assert(spawner.state == 3 && spawner.hidden);
    int spawns = 0;
    for (int i = 0; i < 10; ++i) {
        AosHitReport r = aos_enemy_update(&spawner, &soma, &layer, &kind, NULL, NULL, 10, 4, 100,
                                          48, always_zero);
        if (r.spawn) {
            ++spawns;
            int dx = r.spawn_x - (soma.x >> 16);
            assert((dx < 0 ? -dx : dx) > 32 && r.spawn_x >= 100 && r.spawn_x < 340);
        }
    }
    assert(spawns == 2 && spawner.spawned_count == 2);

    /* Far outside the screen margins a zombie vanishes. */
    assert(aos_enemy_create(&z, AOS_ENEMY_ZOMBIE, 250, 140, 0, 0, &soma, &layer, &kind, &stats));
    aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 1000, 48, always_zero);
    assert(z.state == 1 && z.hp == 0);

    /* The weapon kills a walking zombie: death animation 3, 40 frames. */
    assert(aos_enemy_create(&z, AOS_ENEMY_ZOMBIE, 250, 140, 0, 0, &soma, &layer, &kind, &stats));
    z.step = 1;
    z.walk_timer = 600;
    z.hp = 1;
    soma.x = AOS_FIXED(240);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15};
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{0, -30, 30, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    AosHitReport hit = aos_enemy_update(&z, &soma, &layer, &kind, &weapon, &blade, 10, 4, 100,
                                        48, always_zero);
    assert(hit.enemy_hit && hit.killed && z.state == 1 && z.anim.id == 3 && z.timer == 0x28);
    frames = 0;
    while (!z.removed && frames < 100) {
        aos_enemy_update(&z, &soma, &layer, &kind, &weapon, &blade, 10, 4, 100, 48, always_zero);
        ++frames;
    }
    assert(frames == 40);
}

static void crow_tests(void) {
    /* ArcTan2 (BIOS 0x0A): quadrants and an exact atan(2). */
    assert(aos_arctan2(1, 0) == 0 && aos_arctan2(0, 1) == 0x4000);
    assert(aos_arctan2(-1, 0) == 0x8000 && aos_arctan2(0, -1) == 0xC000);
    assert(aos_arctan2(1, 1) == 0x2000 && aos_arctan2(1, -1) == 0xE000);
    assert(aos_arctan2(1, 2) == 0x2D1C);

    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    static const uint8_t flap[] = {4, 4}, death[] = {200};
    const AosAnimDef defs[4] = {{2, flap}, {2, flap}, {2, flap}, {1, death}};
    const AosAnimSet anims = {defs, 4};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int a = 0; a < 4; ++a)
        for (int f = 0; f < 2; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-6, -6, 12, 12};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {6, 8, 0, 0, 0};

    /* The crow perches (animation 0) until the player is within 60 pixels. */
    AosSoma soma = aos_soma_spawn(AOS_FIXED(300), AOS_FIXED(159), NULL);
    AosEnemy crow;
    assert(aos_enemy_create(&crow, AOS_ENEMY_BLUE_CROW, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(crow.state == 1 && crow.anim.id == 0 && crow.mirrored);
    aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(crow.step == 1);
    soma.x = AOS_FIXED(160);
    soma.y = AOS_FIXED(160);
    aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(crow.step == 3 && crow.substep == 0);
    /* It flaps in place (animation 2) for 33 frames. */
    int frames = 0;
    while (crow.substep < 2 && frames < 100) {
        aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    assert(frames == 33 && crow.anim.id == 2 && crow.x == AOS_FIXED(100));
    /* Within 72 pixels horizontally, it stays. */
    aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(crow.substep == 2);
    /* Farther, it flies at 1.625 to 42 pixels above and 32 behind him. */
    soma.x = AOS_FIXED(300);
    aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(crow.substep == 3);
    aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(crow.anim.id == 1 && crow.vx > 0x18000 && crow.vy > 0x2800 && crow.vy < 0x3000);
    frames = 0;
    while (crow.substep == 3 && frames < 300) {
        aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    int ex = (crow.x >> 16) - (300 - 32), ey = (crow.y >> 16) - (160 - 42);
    assert(crow.substep == 2 && crow.anim.id == 2 && crow.vx == 0 && crow.vy == 0);
    assert(ex >= -2 && ex <= 2 && ey >= -2 && ey <= 2);
    /* Facing left, the point is on his right. */
    soma.facing_left = true;
    soma.x = AOS_FIXED(100);
    while (crow.substep != 3 && frames < 600) {
        aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    while (crow.substep == 3 && frames < 900) {
        aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    ex = (crow.x >> 16) - (100 + 32);
    assert(ex >= -2 && ex <= 2);

    /* A killing blow: state 2, a 64-frame fall at 0.5, then the deletion. */
    soma.facing_left = false;
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15};
    soma.x = crow.x;
    soma.y = crow.y + AOS_FIXED(30);
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-10, -40, 20, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    AosHitReport hit = aos_enemy_update(&crow, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0,
                                        never_zero);
    assert(hit.killed && crow.state == 2 && crow.step == 0);
    int32_t y0 = crow.y;
    int dying = 0;
    while (!crow.removed && dying < 100) {
        aos_enemy_update(&crow, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, never_zero);
        ++dying;
    }
    assert(dying == 64 && crow.anim.id == 3 && crow.y == y0 + 64 * 0x8000);
}

static void soldier_tests(void) {
    /* Floor at cell row 20 (top 160). */
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t walk[] = {6, 6, 6, 6, 6, 6, 6, 6}, attack[] = {2, 7, 3, 5},
                         throw_ticks[] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 6, 6}, death[] = {200};
    const AosAnimDef defs[4] = {{8, walk}, {4, attack}, {13, throw_ticks}, {1, death}};
    const AosAnimSet anims = {defs, 4};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int a = 0; a < 4; ++a)
        for (int f = 0; f < 13; ++f) {
            kind.modes[a][f] = 2;
            kind.hurt[a][f] = (AosBox){-5, -32, 8, 32};
            kind.attack[a][f] = (AosBox){-18, -24, 8, 8};
        }
    kind.margins[2][0] = kind.margins[2][1] = 32;
    const AosEnemyStats stats = {46, 14, 5, 0x0121, 0x0010};

    /* Snapped to the floor; far from Soma it steps toward him at 0.25. */
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(159), NULL);
    soma.max_hp = soma.hp = 320;
    AosEnemy z;
    assert(aos_enemy_create(&z, AOS_ENEMY_ZOMBIE_SOLDIER, 100, 140, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(z.state == 0 && (z.y >> 16) == 159 && z.static_frame == -1);
    int32_t x0 = z.x;
    for (int i = 0; i < 60; ++i)
        aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(z.state == 0 && z.mirrored && z.x > x0 && z.x - x0 < 0x100000);

    /* Within 220 x 70 but not 80 x 70: a grenade (one chance in four). */
    soma.x = AOS_FIXED(z.x >> 16) + AOS_FIXED(84);
    AosHitReport hit = {0};
    int frames = 0;
    while (z.state == 0 && frames < 100) {
        aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(z.state == 1 && z.step == 0xA && z.anim.id == 2);
    while (!hit.spawn_child && frames < 300) {
        hit = aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(hit.spawn_child && z.step == 0xB && z.anim.frame == 10);
    AosEnemy g = hit.child;
    assert(g.role == AOS_ROLE_GRENADE && g.static_frame == 18 && g.own_boxes);
    assert(g.x == z.x + 0x100000 && g.y == z.y - 0x250000);
    assert(g.vx == 0x14000 && g.vy == -0x20000 && g.ay == 0x1800);
    /* The throw animation ends: back to walking. */
    while (z.state == 1 && frames < 400) {
        aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    assert(z.state == 0 && z.anim.id == 0);

    /* Away from Soma, the grenade bounces once at a third of its speed,
     * explodes on the second floor hit, and its blast grows then ends. */
    soma.x = AOS_FIXED(600);
    AosEnemy flying = g;
    int bounces = 0, guard = 0;
    int32_t vy_before = 0;
    while (flying.state == 0 && guard++ < 300) {
        vy_before = flying.vy;      /* sub_08092CCC passes vy / 3 before gravity */
        aos_enemy_update(&flying, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        if (flying.state == 0 && flying.timer > bounces) {
            ++bounces;
            assert(flying.vy == -(vy_before / 3));
        }
    }
    assert(bounces == 1 && flying.state == 2 && flying.timer == 2);
    int blast = 0;
    while (!flying.removed && blast < 100) {
        aos_enemy_update(&flying, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++blast;
        if (flying.timer == 14) assert(flying.own_attack.x == -9 && flying.own_attack.w == 18);
    }
    assert(flying.hidden && blast == 0x18 - 2);

    /* A grenade reaching Soma knocks him back (type 1) and bursts. */
    AosEnemy direct = g;
    soma.x = direct.x;
    soma.y = direct.y + AOS_FIXED(10);
    soma.combat = (AosCombat){.type = AOS_TYPE_PLAYER};
    hit = aos_enemy_update(&direct, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(hit.soma_hit && hit.soma_damage == aos_player_damage(14, 4));
    assert(soma.pending_type == 1 && direct.state == 1);
    for (int i = 0; i < 7; ++i)
        aos_enemy_update(&direct, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(direct.removed);

    /* A killing blow: death animation 3, state 2 and the generic death. */
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15};
    soma.x = z.x;
    soma.y = z.y + AOS_FIXED(20);
    soma.facing_left = false;
    z.hp = 1;
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-10, -40, 20, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    /* The camera follows it: off-screen, the activity window pauses it. */
    int cam = (z.x >> 16) - 120;
    hit = aos_enemy_update(&z, &soma, &layer, &kind, &weapon, &blade, 10, 4, cam, 0, never_zero);
    assert(hit.killed && z.state == 2 && z.anim.id == 3 && z.timer <= 0x28);
    int dying = 0;
    while (!z.removed && dying < 100) {
        aos_enemy_update(&z, &soma, &layer, &kind, &weapon, &blade, 10, 4, cam, 0, never_zero);
        ++dying;
    }
    assert(z.removed && dying == 0x28);
}

static uint32_t always_one(void) { return 1; }

static void armor_tests(void) {
    /* Floor at cell row 20 (top 160) from cell 4 to 49, walls at cells 3
     * and 50 (x 24..31 and 400..407). */
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 4; x < 50; ++x) cells[20 * W + x] = 0x03;
    for (int y = 0; y < 21; ++y) cells[y * W + 3] = cells[y * W + 50] = 0x03;
    static uint8_t walk[18], once[15], axe_ticks[] = {4}, death[11];
    for (int i = 0; i < 18; ++i) walk[i] = 4;
    for (int i = 0; i < 15; ++i) once[i] = 3;
    for (int i = 0; i < 11; ++i) death[i] = 5;
    const AosAnimDef defs[5] = {{18, walk}, {15, once}, {15, once}, {1, axe_ticks}, {11, death}};
    const AosAnimSet anims = {defs, 5};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int a = 0; a < 5; ++a)
        for (int f = 0; f < 18; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-13, -48, 24, 48};
        }
    kind.probes = (AosProbes){2, -40, -1, 6, {-7, -24}};
    kind.margins[4][0] = kind.margins[4][1] = 48;
    const AosEnemyStats stats = {60, 15, 8, 0x0108, 0};

    /* Snapped onto the floor plus one pixel; walking left, it turns at the
     * wall (no patrol length). */
    AosSoma soma = aos_soma_spawn(AOS_FIXED(600), AOS_FIXED(400), NULL);
    soma.max_hp = soma.hp = 320;
    AosEnemy a;
    assert(aos_enemy_create(&a, AOS_ENEMY_AXE_ARMOR, 80, 150, 0, 0, &soma, &layer, &kind, &stats));
    assert((a.y >> 16) == 160 && a.state == 0);
    a.mirrored = false;
    int frames = 0;
    while (!a.mirrored && frames < 2000) {
        aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    /* The wall probes run at the position before the move, so the last
     * accepted step may end one pixel inside the probe distance. */
    assert(a.mirrored && (a.x >> 16) >= 32 + 6 - 1 && (a.y >> 16) == 160);
    /* With a patrol of 1 step it turns after two walk cycles instead. */
    AosEnemy patrol;
    assert(aos_enemy_create(&patrol, AOS_ENEMY_AXE_ARMOR, 200, 150, 1, 0, &soma, &layer, &kind,
                            &stats));
    bool facing = patrol.mirrored;
    int turned_at = -1;
    for (int i = 0; i < 400 && turned_at < 0; ++i) {
        aos_enemy_update(&patrol, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        if (patrol.mirrored != facing) turned_at = i;
    }
    assert(turned_at > 0 && patrol.anim.frame == 0x11);

    /* The player 60 pixels ahead at frame 17: a high or low throw. */
    assert(aos_enemy_create(&a, AOS_ENEMY_AXE_ARMOR, 200, 150, 0, 0, &soma, &layer, &kind, &stats));
    a.mirrored = true;
    soma.x = a.x + AOS_FIXED(60);
    soma.y = a.y;
    frames = 0;
    while (a.state == 0 && frames < 200) {
        aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_one);
        ++frames;
    }
    assert(a.state == 1);
    AosHitReport hit = {0};
    while (!hit.spawn_child && frames < 400) {
        hit = aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_one);
        ++frames;
    }
    /* Thrown at frame 12, tick 2 (the last of these 3-tick test frames):
     * the animation step that follows reaches frame 13. */
    assert(a.anim.id == 1 && a.substep == 0 && a.anim.frame == 13 && a.anim.tick == 0);
    AosEnemy axe = hit.child;
    assert(axe.role == AOS_ROLE_AXE && axe.combat.type == 0x0A);
    assert(axe.x == a.x + 0x180000 && axe.y == a.y - 0x100000);

    /* Its first update starts the flight: 2.5 toward the throw, pulled
     * back by 0x800 per frame, spinning 0x800 per frame. */
    AosSoma away = soma;
    away.x = AOS_FIXED(5000);
    aos_enemy_update(&axe, &away, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(axe.state == 1 && axe.vx == 0x28000 && axe.ax == -0x800 && axe.spin == -0x800);
    int32_t start = axe.x, farthest = axe.x;
    /* From +2.5 to -2.5 at 0x800 per frame takes 160 frames. */
    for (int i = 0; i < 200 && !axe.removed; ++i) {
        aos_enemy_update(&axe, &away, &layer, &kind, NULL, NULL, 10, 4, (axe.x >> 16) - 120, 0,
                         never_zero);
        if (axe.x > farthest) farthest = axe.x;
    }
    assert(farthest > start + AOS_FIXED(60) && axe.x < farthest && axe.vx == -0x28000);

    /* The axe hits Soma through its own type (0xA): an enemy-body hit does
     * not shield him from it. */
    AosEnemy second = hit.child;
    aos_enemy_update(&second, &away, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    soma.x = second.x;
    soma.y = second.y + AOS_FIXED(20);
    soma.combat = (AosCombat){.type = AOS_TYPE_PLAYER};
    aos_combat_take(&soma.combat, AOS_TYPE_ENEMY, 81);
    hit = aos_enemy_update(&second, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(hit.soma_hit && hit.soma_damage == aos_player_damage(15, 4) && soma.pending_type == 0);

    /* A weapon destroys it. */
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15};
    soma.facing_left = false;
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-10, -40, 20, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    AosEnemy struck = hit.child;
    struck = second;
    struck.combat.recent[0] = struck.combat.recent[1] = struck.combat.recent[2] = 0;
    soma.x = struck.x;
    soma.y = struck.y + AOS_FIXED(30);
    hit = aos_enemy_update(&struck, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, never_zero);
    assert(hit.enemy_hit && struck.removed);

    /* A killing blow: the armor turns to the attacker, plays its death
     * animation and is deleted when it ends. */
    soma.x = a.x - AOS_FIXED(5);
    soma.y = a.y + AOS_FIXED(20);
    soma.combat = (AosCombat){.type = AOS_TYPE_PLAYER};
    a.hp = 1;
    a.mirrored = true;
    hit = aos_enemy_update(&a, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, never_zero);
    assert(hit.killed && a.state == 2 && a.defeated && !a.mirrored);
    int dying = 0;
    while (!a.removed && dying < 200) {
        aos_enemy_update(&a, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, never_zero);
        ++dying;
    }
    assert(a.removed && a.anim.id == 4 && dying >= 11 * 5);
}

static void archer_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 4; x < 60; ++x) cells[20 * W + x] = 0x03;
    static uint8_t idle[4], walk[4], shot[13], triple[16], piece[1];
    for (int i = 0; i < 4; ++i) idle[i] = walk[i] = 6;
    for (int i = 0; i < 13; ++i) shot[i] = 3;
    for (int i = 0; i < 16; ++i) triple[i] = 3;
    piece[0] = 9;
    const AosAnimDef defs[6] = {{4, idle}, {4, walk}, {13, shot}, {13, shot}, {16, triple}, {1, piece}};
    const AosAnimSet anims = {defs, 6};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int a = 0; a < 6; ++a)
        for (int f = 0; f < 16; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -32, 16, 32};
        }
    kind.probes = (AosProbes){1, -30, -1, 8, {-15}};
    kind.margins[4][0] = kind.margins[4][1] = 48;
    kind.volleys[0][0][0] = kind.volleys[1][0][0] = 3;
    kind.volley_sizes[0] = kind.volley_sizes[1] = 1;
    const int8_t triple_shots[3][2] = {{3, 0}, {9, 1}, {14, 2}};
    memcpy(kind.volleys[2], triple_shots, sizeof triple_shots);
    kind.volley_sizes[2] = 3;
    const AosEnemyStats stats = {42, 13, 5, 0x0020, 0x0010};

    /* Standing: it faces the player and shoots once he is in the 240 x 35
     * box; the volleys go 0, 0, 1, 1, 2 (animations 2, 2, 3, 3, 4). */
    AosSoma soma = aos_soma_spawn(AOS_FIXED(500), AOS_FIXED(159), NULL);
    soma.max_hp = soma.hp = 320;
    AosEnemy a;
    assert(aos_enemy_create(&a, AOS_ENEMY_SKULL_ARCHER, 200, 150, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(a.state == 0 && (a.y >> 16) == 160);
    soma.y = a.y;
    aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 0, never_zero);
    assert(a.state == 0 && a.mirrored);
    soma.x = a.x + AOS_FIXED(100);
    AosSoma far_soma = soma;
    far_soma.x = AOS_FIXED(5000);
    const unsigned expected[5] = {2, 2, 3, 3, 4};
    for (int volley = 0; volley < 5; ++volley) {
        int arrows = 0, frames = 0;
        AosEnemy first = {0};
        while (a.state != 1 && frames < 500) {
            aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 0, never_zero);
            ++frames;
        }
        assert(a.state == 1 && a.anim.id == expected[volley]);
        /* Shooting does not need the player in range any more. */
        while (a.state == 1 && frames < 1000) {
            AosHitReport hit = aos_enemy_update(&a, &far_soma, &layer, &kind, NULL, NULL, 10, 4,
                                                100, 0, never_zero);
            if (hit.spawn_child && arrows++ == 0) first = hit.child;
            ++frames;
        }
        assert(arrows == (volley == 4 ? 3 : 1));
        assert(first.role == AOS_ROLE_ARROW && first.static_frame == 25 && first.x == a.x &&
               first.y == a.y && first.combat.type == 0x0A && first.mirrored);
    }

    /* An arrow flies at 3.0, knocks Soma back (type 1) and sticks to him
     * for 30 frames. */
    AosHitReport hit = {0};
    int frames = 0;
    while (a.state != 1 && frames < 500) {
        aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 0, never_zero);
        ++frames;
    }
    while (!hit.spawn_child && frames < 1000) {
        hit = aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 0, never_zero);
        ++frames;
    }
    AosEnemy arrow = hit.child;
    int32_t x0 = arrow.x;
    soma.combat = (AosCombat){.type = AOS_TYPE_PLAYER};
    int flight = 0;
    hit = (AosHitReport){0};
    while (!hit.soma_hit && flight < 100) {
        hit = aos_enemy_update(&arrow, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 0,
                               never_zero);
        ++flight;
    }
    assert(hit.soma_hit && arrow.x == x0 + flight * 0x30000);
    assert(hit.soma_damage == aos_player_damage(13, 4) && soma.pending_type == 1);
    assert(arrow.state == 1 && arrow.combat.attack_off);
    soma.x += AOS_FIXED(5);
    aos_enemy_update(&arrow, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 0, never_zero);
    assert(arrow.x == soma.x + arrow.ax);
    int stuck = 1;
    while (!arrow.removed && stuck < 100) {
        aos_enemy_update(&arrow, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 0, never_zero);
        ++stuck;
    }
    assert(stuck == 31);

    /* A patrolling archer walks at 0.25, backs away at 0.75 from the
     * player ahead within 79 pixels and shoots once he is beyond 99. */
    AosEnemy p;
    soma.x = AOS_FIXED(600);
    assert(aos_enemy_create(&p, AOS_ENEMY_SKULL_ARCHER, 200, 150, 1, 0, &soma, &layer, &kind,
                            &stats));
    assert(p.state == 2 && p.anim.id == 1);
    p.mirrored = true;
    int32_t px = p.x;
    aos_enemy_update(&p, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 0, never_zero);
    assert(p.x == px + 0x4000 && p.step == 0);
    soma.x = p.x + AOS_FIXED(60);
    aos_enemy_update(&p, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 0, never_zero);
    assert(p.step == 1);
    px = p.x;
    aos_enemy_update(&p, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 0, never_zero);
    assert(p.x == px - 0xC000 && p.state == 2);
    soma.x = p.x + AOS_FIXED(120);
    aos_enemy_update(&p, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 0, never_zero);
    assert(p.state == 1 && p.anim.id == 2);

    /* Killed, it shatters on the next update. */
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15};
    soma.facing_left = false;
    soma.x = a.x - AOS_FIXED(5);
    soma.y = a.y + AOS_FIXED(20);
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-10, -40, 20, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    a.hp = 1;
    hit = aos_enemy_update(&a, &soma, &layer, &kind, &weapon, &blade, 10, 4, 100, 0, never_zero);
    assert(hit.killed && a.state == 3 && !a.removed);
    aos_enemy_update(&a, &soma, &layer, &kind, &weapon, &blade, 10, 4, 100, 0, never_zero);
    assert(a.removed);
}

int main(void) {
    /* sub_080009E4 */
    assert(aos_sine(0) == 0 && aos_sine(0x4000) == 0x10000 && aos_sine(0xC000) == -0x10000);
    assert(aos_sine(0x2000) == 46340 && aos_sine(0xA000) == -46340);

    for (int x = 0; x < W; ++x) cells[1 * W + x] = 0x03;   /* ceiling bottom at y 15 */
    static const uint8_t one[] = {4}, three[] = {3, 3, 3};
    const AosAnimDef defs[3] = {{1, one}, {1, one}, {3, three}};
    const AosAnimSet anims = {defs, 3};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int a = 0; a < 3; ++a)
        for (int f = 0; f < 3; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-2, 0, 4, 9};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {10, 6, 0, 0x0001, 0};

    /* The bat climbs from its record to the ceiling and hangs there. */
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(159), NULL);
    AosEnemy bat;
    assert(!aos_enemy_create(&bat, 0x0D, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    assert(aos_enemy_create(&bat, 0x00, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    assert((bat.y >> 16) == 16 && bat.state == 1 && bat.mirrored);

    /* Soma far away: it hangs (animation 0). */
    for (int i = 0; i < 10; ++i) aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
    assert(bat.state == 1 && bat.anim.id == 0);
    /* Within 0xE0 x 0xA0 but not 0xC0 x 0x70: it notices him (animation 1). */
    soma.x = AOS_FIXED(100 + 0x68);
    aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
    assert(bat.state == 1 && bat.anim.id == 1);
    /* Within 0xC0 x 0x70: it attacks. */
    soma.x = AOS_FIXED(150);
    soma.y = AOS_FIXED(100);
    aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
    assert(bat.state == 2 && bat.step == 0);

    /* Backward hop: 0.5 away from Soma, then a dive at 0.375 toward him
     * once 33 frames have passed. */
    int32_t x0 = bat.x;
    aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
    assert(bat.anim.id == 2 && bat.vx == -0x8000 && bat.x == x0 - 0x8000);
    int frames = 1;
    while (bat.step == 0 && frames < 100) {
        aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
        ++frames;
    }
    assert(frames == 33 && bat.step == 1);
    aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
    assert(bat.vx == 0x6000 && bat.vy == 0x6000);
    /* It dives until within 39 pixels of Soma's height, then flies off on a
     * sine wave with acceleration and is deleted 240 pixels on screen. */
    frames = 0;
    while (bat.step == 1 && frames < 400) {
        aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
        ++frames;
    }
    assert(bat.step == 2);
    int dy = (bat.y >> 16) - (soma.y >> 16);
    assert(dy >= -0x27 && dy <= 0x27);
    frames = 0;
    while (!bat.removed && frames < 2000) {
        aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
        ++frames;
    }
    assert(bat.removed && (bat.x >> 16) > 0xF0 && bat.vx > 0xC000);

    /* Outside the activity window nothing runs. */
    assert(aos_enemy_create(&bat, 0x00, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    soma.x = AOS_FIXED(150);
    aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 1000, 0, never_zero);
    assert(bat.state == 1 && bat.anim.tick == 1);
    /* Combat: a swooping bat touching Soma's hurtbox damages him once,
     * then type-8 attacks are ignored for 81 frames. */
    soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(60), NULL);
    assert(aos_enemy_create(&bat, 0x00, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    bat.state = 9;          /* no AI: only the collision pass runs */
    bat.y = AOS_FIXED(40);
    AosHitReport hit = aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(hit.soma_hit && hit.soma_damage == (6 * 4 - 4 * 2) / 2);
    int frames_to_next = 0;
    do {
        aos_combat_tick(&soma.combat);
        hit = aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames_to_next;
    } while (!hit.soma_hit && frames_to_next < 200);
    assert(frames_to_next == 81);

    /* The weapon's hitbox kills it: power 10 * 1.25 (weak to element 1),
     * defence 0 -> 12 damage against 10 HP; it falls, blinks and goes. */
    soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(200), NULL);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15};
    assert(aos_enemy_create(&bat, 0x00, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    bat.y = AOS_FIXED(170);
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-10, -40, 20, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    hit = aos_enemy_update(&bat, &soma, &layer, &kind, &weapon, &blade, 10, 0, 0, 0, never_zero);
    assert(hit.enemy_hit && hit.enemy_damage == 12 && hit.killed && bat.state == 3);
    assert(soma.flags & AOS_FLAG_KICK_HIT);
    int dying = 0;
    while (!bat.removed && dying < 100) {
        aos_enemy_update(&bat, &soma, &layer, &kind, &weapon, &blade, 10, 0, 0, 0, never_zero);
        ++dying;
    }
    assert(dying == 64 && bat.vflip);

    /* Damage formulas. */
    assert(aos_enemy_damage(10, 0, 0x0001, 0, 0x0001) == 12);   /* weak, element 1 */
    assert(aos_enemy_damage(10, 0, 0x0002, 0, 0x0002) == 20);   /* weak, other */
    assert(aos_enemy_damage(10, 0, 0, 0x0001, 0x0001) == 5);    /* resisted */
    assert(aos_enemy_damage(10, 4, 0, 0, 0x0001) == 7);         /* (10 - 2) * 252 >> 8 */
    assert(aos_enemy_damage(1, 8, 0, 0, 1) == 1);
    assert(aos_player_damage(6, 10) == 2 && aos_player_damage(1, 50) == 1);
    zombie_tests();
    crow_tests();
    soldier_tests();
    armor_tests();
    archer_tests();
    puts("aos_enemy: ok");
    return 0;
}
