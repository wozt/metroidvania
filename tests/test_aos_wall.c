/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Aria of Sorrow breakable walls (objects 0x08 and
 * 0x09). */
#include "aos/core/aos_wall.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static uint8_t blocks[(W / 2) * (H / 2)];
static AosCollision layer = {2, 1, W, H, cells, blocks, false};

static uint32_t always_zero(void) { return 0; }
static const uint32_t *script;
static int script_at;
static uint32_t scripted(void) { return script[script_at++]; }

static bool solid(int x, int y) { return aos_collision_cell(&layer, x, y) != 0; }

static void reset(void) {
    memset(cells, 0, sizeof cells);
    memset(blocks, 0, sizeof blocks);
    layer.blocks_active = false;
}

/* The blocks of each shape (sub_0804FCA0) and their clearing. */
static void shape_tests(void) {
    uint32_t events[AOS_WALL_EVENT_WORDS] = {0};
    AosWall wall;
    /* Entry 0: a column of four blocks (64 high) on x 200, at y - dy for
     * dy from -32 below 32: 152 up to 104. */
    reset();
    assert(aos_wall_create(&wall, &layer, 200, 120, 0, 34, false, events));
    assert(solid(200, 104) && solid(200, 120) && solid(200, 136) && solid(200, 152) &&
           !solid(200, 88) && !solid(200, 168) && !solid(184, 120));
    /* Entry 1: the same column 16 pixels left. */
    reset();
    assert(aos_wall_create(&wall, &layer, 200, 120, 1, 34, false, events));
    assert(solid(184, 120) && !solid(200, 120));
    /* Entry 3: a row of five blocks (80 wide), 16 lower, at x - dx: 240
     * down to 176. */
    reset();
    assert(aos_wall_create(&wall, &layer, 200, 120, 3, 34, false, events));
    assert(solid(176, 136) && solid(240, 136) && !solid(160, 136) && !solid(200, 120));
    /* Entry 8: a grid of 2 x 7 blocks, 48 higher. */
    reset();
    assert(aos_wall_create(&wall, &layer, 200, 120, 8, 34, true, events));
    assert(solid(184, 16) && solid(200, 112) && !solid(216, 72) && !solid(200, 128));
    /* An entry out of range, or a recorded break, deletes it. */
    assert(!aos_wall_create(&wall, &layer, 200, 120, AOS_WALL_ENTRIES, 34, false, events));
    events[1] = 1u << 2;     /* bit 34 */
    reset();
    assert(!aos_wall_create(&wall, &layer, 200, 120, 0, 34, false, events));
    assert(!solid(200, 120));
}

/* Object08Update: three blows (0xD3), the fourth breaks it (0xAB): dust,
 * nine debris, the blocks cleared, the event bit set. */
static void break_tests(void) {
    reset();
    uint32_t events[AOS_WALL_EVENT_WORDS] = {0};
    AosWall wall;
    assert(aos_wall_create(&wall, &layer, 200, 120, 0, 34, false, events) && wall.hp == 3);
    AosWallReport report = {0};
    for (int blow = 0; blow < 3; ++blow) {
        report = (AosWallReport){0};
        aos_wall_update(&wall, &layer, events, 0, true, always_zero, &report);
        assert(report.struck && report.sound == 0xD3 && !report.broke && wall.hp == 2 - blow);
        /* The dust spawner raises its first dust at once. */
        assert(report.effect_count >= 1 && report.effects[report.effect_count - 1].rise);
    }
    report = (AosWallReport){0};
    aos_wall_update(&wall, &layer, events, 0, true, always_zero, &report);
    int debris = 0;
    for (int k = 0; k < report.effect_count; ++k) {
        if (!report.effects[k].debris) continue;
        assert(report.effects[k].translucent && report.effects[k].x == wall.x);
        ++debris;
    }
    assert(report.broke && report.sound == 0xAB && debris == 9 && wall.broken &&
           !solid(200, 120) && aos_wall_event(events, 34));
    assert(report.effects[0].y == wall.y - 0x100000);
    /* Broken, it only runs its spawners: 32 frames, a dust every fourth. */
    int frames = 0, dusts = 0;
    while (aos_wall_alive(&wall) && frames < 100) {
        report = (AosWallReport){0};
        aos_wall_update(&wall, &layer, events, 0, true, always_zero, &report);
        dusts += report.effect_count;
        assert(!report.struck);
        ++frames;
    }
    assert(frames == 32 && dusts == 4 * 8);   /* the four spawners' remaining dusts */

    /* Without an event bit it does nothing (no blow, no sparkle). */
    reset();
    assert(aos_wall_create(&wall, &layer, 200, 120, 15, 0, false, events));
    report = (AosWallReport){0};
    for (int k = 0; k < 20; ++k)
        aos_wall_update(&wall, &layer, events, AOS_WALL_PEEPING_EYE_FLAG, true, always_zero,
                        &report);
    assert(report.effect_count == 0 && !report.struck && wall.hp == 3 && solid(200, 120));
}

/* The Peeping Eye soul: a burst sparkle (animation 12) every 16 frames. */
static void sparkle_tests(void) {
    reset();
    uint32_t events[AOS_WALL_EVENT_WORDS] = {0};
    AosWall wall;
    assert(aos_wall_create(&wall, &layer, 200, 120, 0, 34, false, events));
    int sparkles = 0;
    for (int frame = 1; frame <= 48; ++frame) {
        AosWallReport report = {0};
        aos_wall_update(&wall, &layer, events, frame <= 32 ? AOS_WALL_PEEPING_EYE_FLAG : 0, false,
                        always_zero, &report);
        if (report.effect_count) {
            assert(report.effects[0].kind == AOS_EFFECT_BURST_12 && report.effects[0].drift &&
                   report.effects[0].x == wall.x && frame % 16 == 0);
            ++sparkles;
        }
    }
    assert(sparkles == 2);
}

/* Object09Update: no blows; it breaks once its bit is set, a grid into 21
 * pieces and debris 32 pixels up. */
static void event_wall_tests(void) {
    reset();
    uint32_t events[AOS_WALL_EVENT_WORDS] = {0};
    AosWall wall;
    assert(aos_wall_create(&wall, &layer, 200, 120, 8, 46, true, events));
    AosWallReport report = {0};
    aos_wall_update(&wall, &layer, events, 0, true, always_zero, &report);
    assert(!report.struck && !wall.broken && report.effect_count == 0);
    events[1] |= 1u << 14;   /* bit 46 */
    report = (AosWallReport){0};
    aos_wall_update(&wall, &layer, events, 0, false, always_zero, &report);
    int pieces = 0, debris = 0;
    for (int k = 0; k < report.effect_count; ++k) {
        pieces += report.effects[k].piece;
        debris += report.effects[k].debris;
    }
    assert(report.broke && report.sound == 0xAB && pieces == 21 && debris == 9 && wall.broken &&
           !solid(200, 112));
    /* The first piece: column -1, row -3: x + 8 - 2, y - (-24 + 32) - 2. */
    assert(report.effects[0].x == AOS_FIXED(206) && report.effects[0].y == AOS_FIXED(110));
    /* A column object 0x09 breaks into debris only. */
    reset();
    events[1] = 0;
    assert(aos_wall_create(&wall, &layer, 200, 120, 4, 45, true, events));
    events[1] |= 1u << 13;
    report = (AosWallReport){0};
    aos_wall_update(&wall, &layer, events, 0, false, always_zero, &report);
    assert(report.broke && report.effect_count == 9);
}

/* The weapon test: Soma's blade box over the wall's box, once per swing. */
static void weapon_tests(void) {
    reset();
    uint32_t events[AOS_WALL_EVENT_WORDS] = {0};
    AosWall wall;
    assert(aos_wall_create(&wall, &layer, 200, 120, 0, 34, false, events));
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(190), AOS_FIXED(150), NULL);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    assert(aos_wall_weapon_hit(&wall, &weapon, &soma, &blade));
    assert(!aos_wall_weapon_hit(&wall, &weapon, &soma, &blade));   /* the recent-hit slot */
    for (int k = 0; k < 20; ++k) aos_combat_tick(&wall.combat);
    assert(aos_wall_weapon_hit(&wall, &weapon, &soma, &blade));
    soma.x = AOS_FIXED(100);
    for (int k = 0; k < 20; ++k) aos_combat_tick(&wall.combat);
    assert(!aos_wall_weapon_hit(&wall, &weapon, &soma, &blade));
}

/* The walls' effects: debris (sub_0804F8A0), dust (sub_080503E0) and
 * pieces (sub_0804F9B4). */
static void effect_tests(void) {
    reset();
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;     /* floor top 160 */
    static const uint8_t still_ticks[] = {1}, smoke_ticks[] = {4, 4};
    const AosAnimDef still_def = {1, still_ticks}, smoke_def = {2, smoke_ticks};
    const AosAnimSet still = {&still_def, 1}, smoke = {&smoke_def, 1};
    const AosEffectWorld world = {&layer, 0, 0, scripted, AOS_FIXED(250), 0, 0};
    /* Debris left of the player: thrown left (vx negated), up; it falls by
     * 0x3000 and bounces once 4 pixels over the floor. */
    const uint32_t throw_draws[] = {0x1000, 0x2000, 1};
    script = throw_draws;
    script_at = 0;
    AosEffect d = {.kind = AOS_EFFECT_ROCK_FRAGMENT_2, .debris = true, .x = AOS_FIXED(200),
                   .y = AOS_FIXED(140)};
    assert(aos_effect_update(&d, &still, NULL, &world));
    assert(d.vx == -(0x10000 - 0x8000) && d.vy == -0x20000 && d.x == AOS_FIXED(200));
    int frames = 0;
    while (d.state == 1 && frames < 100) {
        assert(aos_effect_update(&d, &still, NULL, &world));
        ++frames;
    }
    assert(d.state == 2 && d.kind == AOS_EFFECT_ROCK_FRAGMENT_1 && d.vy < 0 && script_at == 3);
    /* Dust: started by its creator, it rises from its next update. */
    const uint32_t rise_draws[] = {0x10};
    script = rise_draws;
    script_at = 0;
    AosEffect dust = {.kind = AOS_EFFECT_SMOKE, .rise = true, .x = AOS_FIXED(200),
                      .y = AOS_FIXED(100)};
    assert(aos_effect_update(&dust, &smoke, NULL, &world));
    assert(dust.vy == -0xC100 && dust.y == AOS_FIXED(100));
    assert(aos_effect_update(&dust, &smoke, NULL, &world) && dust.y == AOS_FIXED(100) - 0xC100);
    frames = 2;
    while (aos_effect_update(&dust, &smoke, NULL, &world)) ++frames;
    assert(frames == 6);   /* eight ticks: one at creation, then one per update */
    /* A piece: frame 2 here, 2.0 + r right, -1.0 - r up, falling. */
    const uint32_t piece_draws[] = {1, 0, 0};
    script = piece_draws;
    script_at = 0;
    AosEffect p = {.kind = AOS_EFFECT_WALL_PIECE_1, .piece = true, .x = AOS_FIXED(100),
                   .y = AOS_FIXED(100)};
    assert(aos_effect_update(&p, &still, NULL, &world));
    assert(p.kind == AOS_EFFECT_WALL_PIECE_2 && p.vx == 0x20000 && p.vy == -0x10000 + 0x3000 &&
           p.x == AOS_FIXED(102));
    reset();
}

int main(void) {
    effect_tests();
    shape_tests();
    break_tests();
    sparkle_tests();
    event_wall_tests();
    weapon_tests();
    printf("aos_wall: ok\n");
    return 0;
}
