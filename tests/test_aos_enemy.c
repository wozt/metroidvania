/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Aria of Sorrow enemy framework and its ported enemy
 * state machines, including Nemesis. */
#include "aos_enemy.h"
#include "aos_effect.h"
#include "aos_audio.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 1, W, H, cells, NULL, false};

static uint32_t never_zero(void) { return 1; }
static uint32_t always_zero(void) { return 0; }
static uint32_t always_one_hundred(void) { return 100; }
/* A scripted RandomNumberGenerator. */
static const uint32_t *script;
static int script_at;
static uint32_t scripted(void) { return script[script_at++]; }

/* Skeleton: the native attack's third animation frame launches a single
 * bone, using source-derived OAM IDs, sound, launch trajectory and boxes.
 * The regular kill path is deliberately still used for XP and soul drops. */
/* EnemyWingedSkeletonUpdate from its traced routines. */
static void winged_skeleton_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;      /* floor top 160 */
    static const uint8_t ticks[] = {2, 2, 2, 2};
    static AosAnimDef defs[9];
    for (int a = 0; a < 9; ++a) defs[a] = (AosAnimDef){4, ticks};
    const AosAnimSet anims = {defs, 9};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int k = 0; k < 14; ++k) kind.margins[k / 2][k % 2] = 64;
    for (int a = 0; a < 9; ++a)
        for (int f = 0; f < 4; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -8, 16, 16};
        }
    const AosEnemyStats stats = {30, 12, 2, 0, 0, 10};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(100), NULL);
    AosEnemy w;
    /* Facing Soma on its right, it flies level at 0.75 + (r & 0x3FFF). */
    assert(aos_enemy_create(&w, AOS_ENEMY_WINGED_SKELETON, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(w.state == 0 && w.mirrored && w.vx >= 0xC000 && w.vx <= 0xFFFF && w.vy == 0);
    int32_t y0 = w.y;
    for (int k = 0; k < 10; ++k)
        aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_one_hundred);
    assert(w.y == y0 && w.state == 0);
    /* Aiming: Soma 40 right and 32 below (a diagonal), the point ahead in
     * the air: + 0x1C, then at an animation start the attack (step 10). */
    w.step = 3;
    w.rest = 0;
    w.vx = 0;
    soma.x = w.x + AOS_FIXED(40);
    soma.y = w.y + AOS_FIXED(32);
    for (int k = 0; k < 20 && w.state == 0; ++k)
        aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_one_hundred);
    assert(w.state == 1 && w.step == 10);
    /* Animation 7; past its frame 2 the bone (0x85), then it flies on. */
    aos_audio_clear();
    AosEnemy bone = {0};
    for (int k = 0; k < 40 && w.state == 1; ++k) {
        AosHitReport r = aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_one_hundred);
        for (int c = 0; c < r.child_count; ++c)
            if (r.children[c].role == AOS_ROLE_WINGED_BONE) bone = r.children[c];
    }
    assert(bone.role == AOS_ROLE_WINGED_BONE && bone.vx == 0x20000 && bone.vy == 0x20000 &&
           (bone.x >> 16) == (w.x >> 16) + 0x15 && (bone.y >> 16) == (w.y >> 16) - 9);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x85) == 1);
    assert(w.state == 0 && w.step == 4 && w.anim.id == 1);
    /* The bone lands on the floor, rests 60 frames, then goes. */
    int frames = 0;
    while (bone.state == 0 && frames < 100) {
        aos_enemy_update(&bone, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    assert(bone.state == 1 && bone.combat.attack_off);
    frames = 0;
    while (!bone.removed && frames < 100) {
        aos_enemy_update(&bone, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    assert(bone.removed && frames == 0x3C);
    /* A killing blow: 0x6B, then seven pieces (frames 0x24-0x2A). */
    aos_audio_clear();
    AosSoulAttack hit = {{0, 0, 2000, 2000}, 255, 0, false, NULL, AOS_TYPE_SOUL};
    aos_enemy_set_soul_attacks(&hit, 1);
    aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_one_hundred);
    aos_enemy_set_soul_attacks(NULL, 0);
    assert(w.state == 2 && aos_audio_requested(AOS_AUDIO_START_SONG, 0x6B) == 1);
    w.hit_flash = 0;
    AosHitReport death = aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_one_hundred);
    int pieces = 0;
    for (int c = 0; c < death.child_count; ++c)
        if (death.children[c].role == AOS_ROLE_WINGED_PIECE) {
            assert(death.children[c].static_frame == 0x24 + pieces);
            ++pieces;
        }
    assert(pieces == 7);
}

static void skeleton_bone_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {1, 1, 1, 1};
    static AosAnimDef defs[0x11];
    for (int a = 0; a < 0x11; ++a) defs[a] = (AosAnimDef){4, ticks};
    const AosAnimSet anims = {defs, 0x11};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 0x11; ++a)
        for (int f = 0; f < 4; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -16, 16, 16};
            kind.edges[a][f][0] = -3;
            kind.edges[a][f][1] = -27;
        }
    const AosEnemyStats stats = {20, 10, 1, 0, 0, 15};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(110), AOS_FIXED(150), NULL);
    AosEnemy e;
    /* EnemySkeletonCreate: head on and bone in hand, home +/- 32, state 1. */
    assert(aos_enemy_create(&e, AOS_ENEMY_SKELETON, 100, 100, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 1 && e.skel_flags == 5 && e.home == 100 && e.home_left == 68 &&
           e.home_right == 132 && e.mirrored);
    /* The first update creates the head and the bone in hand; on screen the
     * body is shown again (sub_0806E4F8). */
    AosHitReport report = aos_enemy_update(&e, &soma, &layer, &kind,
                                           NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(report.child_count >= 2 && report.children[0].role == AOS_ROLE_SKELETON_HEAD &&
           report.children[1].role == AOS_ROLE_SKELETON_HELD && !e.hidden);
    /* The head sits at the corner of the body's last component, mirrored. */
    AosEnemy head = report.children[0];
    head.parent = &e;
    aos_enemy_update(&head, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(head.state == 1 && head.anim.id == 0x10 && (head.x >> 16) == (e.x >> 16) + 3 &&
           (head.y >> 16) == (e.y >> 16) - 27);
    /* The throw (state 4): at animation 4's frame 2 the bone flies. */
    e.state = 4;
    e.step = 0;
    aos_audio_clear();
    AosEnemy bone = {0};
    int launches = 0;
    for (int frame = 0; frame < 20 && e.state == 4; ++frame) {
        report = aos_enemy_update(&e, &soma, &layer, &kind,
                                  NULL, NULL, 10, 4, 0, 0, never_zero);
        for (int k = 0; k < report.child_count; ++k)
            if (report.children[k].role == AOS_ROLE_SKELETON_BONE) {
                bone = report.children[k];
                ++launches;
            }
    }
    assert(launches == 1 && e.state == 1 && !(e.skel_flags & 4) && e.spawned_count == 1);
    assert(bone.static_frame == 0x12 && (bone.y >> 16) == (e.y >> 16) - 30 && bone.spinning);
    assert(bone.vx == 0x8AAA && bone.vy == -0x30000);
    assert(bone.own_hurt.w == 16 && bone.own_attack.w == 2);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x88) == 1);
    /* It falls with 0x1800 of gravity and is gone below screen row 0xAA. */
    bone.parent = &e;
    int32_t x0 = bone.x, y0 = bone.y;
    aos_enemy_update(&bone, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(bone.x == x0 + 0x8AAA && bone.y == y0 - 0x30000 && bone.vy == -0x30000 + 0x1800);
    for (int frame = 0; frame < 200 && !bone.removed; ++frame)
        aos_enemy_update(&bone, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(bone.removed && e.spawned_count == 0);
    /* With the head in hand (+ 0x1A bit 1) the head flies (0x1B, 0x89). */
    e.skel_flags = 2;
    e.state = 4;
    e.step = 0;
    aos_audio_clear();
    bone = (AosEnemy){0};
    for (int frame = 0; frame < 20 && e.state == 4; ++frame) {
        report = aos_enemy_update(&e, &soma, &layer, &kind,
                                  NULL, NULL, 10, 4, 0, 0, never_zero);
        for (int k = 0; k < report.child_count; ++k)
            if (report.children[k].role == AOS_ROLE_SKELETON_BONE) bone = report.children[k];
    }
    assert(bone.static_frame == 0x1B && bone.variant && e.skel_flags == 0 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x89) == 1);
    /* sub_080C3B2C: a struck head flies back twelve times faster (0x15D),
     * a struck bone drops (vy 0.25, 0x151); neither collides again. */
    for (int k = 0; k < 2; ++k) {
        AosEnemy struck = bone;
        struck.parent = &e;
        struck.variant = (uint8_t)(k == 0);
        struck.combat = (AosCombat){.type = AOS_TYPE_ENEMY};
        AosSoulAttack hit = {{0, 0, 2000, 2000}, 50, 0, false, NULL, AOS_TYPE_SOUL};
        aos_enemy_set_soul_attacks(&hit, 1);
        aos_audio_clear();
        int32_t vx = struck.vx;
        aos_enemy_update(&struck, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        aos_enemy_set_soul_attacks(NULL, 0);
        assert(struck.state == 3 && !struck.spinning && struck.hit_flash == 8);
        if (k == 0) {
            assert(abs(struck.vx) == abs(vx) * 12 &&
                   aos_audio_requested(AOS_AUDIO_START_SONG, 0x15D) == 1);
        } else {
            assert(struck.vx == 0 && struck.vy == 0x4000 &&
                   aos_audio_requested(AOS_AUDIO_START_SONG, 0x151) == 1);
        }
    }
    /* sub_080C4624: 0x6B, then six pieces (no head left), 256 frames. */
    e.skel_flags = 0;
    e.state = 1;
    e.step = 0;
    e.hit_flash = 0;
    aos_audio_clear();
    AosSoulAttack soul = {{e.x >> 16, (e.y >> 16) - 16, e.x >> 16,
                           (e.y >> 16) - 16}, 255, 0, false, NULL, AOS_TYPE_SOUL};
    aos_enemy_set_soul_attacks(&soul, 1);
    report = aos_enemy_update(&e, &soma, &layer, &kind,
                              NULL, NULL, 10, 4, 0, 0, never_zero);
    aos_enemy_set_soul_attacks(NULL, 0);
    int pieces = 0;
    for (int k = 0; k < report.child_count; ++k) pieces += report.children[k].role == AOS_ROLE_DEBRIS;
    assert(report.killed && e.defeated && e.state == 0xFF && e.hidden && pieces == 6);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x6B) == 1);
    int frames = 0;
    while (!e.removed && frames < 300) {
        aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    assert(e.removed && frames == 256);
    memset(cells, 0, sizeof cells);
}


/* sub_0803E86C as the runtime computes it (aos_enemy.c fixed_mul). */
static int32_t fixed_mul_of(int32_t a, int32_t b) {
    if (!a || !b) return 0;
    if (a == 0x10000) return b;
    if (b == 0x10000) return a;
    int32_t a8 = a < 0 ? -((-a) >> 8) : a >> 8;
    int32_t b8 = b < 0 ? -((-b) >> 8) : b >> 8;
    int32_t p = a8 * b8;
    if (p < 0) p += 0xFF;
    return (int32_t)((uint32_t)(p >> 8) << 8);
}

/* Peeping Eye from its traced routines (EnemyPeepingEyeCreate / Update,
 * sub_08088168, sub_08088638, sub_08088724, sub_080888D4, sub_08088A64). */
static void peeping_eye_tests(void) {
    aos_audio_clear();
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;     /* floor top 160 */
    static const uint8_t ticks[] = {200};
    const AosAnimDef defs[2] = {{1, ticks}, {1, ticks}};
    const AosAnimSet anims = {defs, 2};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    kind.margins[0][0] = kind.margins[0][1] = 32;
    kind.margins[3][0] = kind.margins[3][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 4};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(150), NULL);

    /* Creation: animation 0, boxes (-7, -7, 14, 14), on the floor and 8
     * pixels up, state 0. */
    AosEnemy eye;
    aos_random_seed(0);
    assert(aos_enemy_create(&eye, AOS_ENEMY_PEEPING_EYE, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(eye.state == 0 && eye.step == 0 && eye.anim.id == 0 && eye.own_boxes &&
           eye.own_hurt.x == -7 && eye.own_hurt.w == 14 && eye.y == AOS_FIXED(151));
    eye.pe_reverse = false;

    /* The first update makes, in order, the eyeball (frame 0xF), the pupil
     * (0x10) and six hidden segments. Soma outside (x - 90, y - 100, 180,
     * 200): it waits, showing its animation. */
    AosHitReport first = aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          never_zero);
    assert(first.child_count == 8 && eye.step == 0 && eye.static_frame == -1);
    static const int16_t frames[6] = {0xB, 0xB, 0xC, 0xC, 0xD, 0xE};
    static const int32_t reach[6] = {0xC0000, 0xA0000, 0x80000, 0x80000, 0x70000, 0x30000};
    assert(first.children[0].static_frame == 0xF && first.children[0].variant == 6 &&
           first.children[1].static_frame == 0x10 && first.children[1].variant == 7);
    AosEnemy part[8];
    for (int c = 0; c < 8; ++c) {
        part[c] = first.children[c];
        part[c].parent = &eye;
        assert(part[c].role == AOS_ROLE_PEEPING_SEGMENT && part[c].combat.hurt_off);
        if (c < 2) continue;
        int k = c - 2;
        assert(part[c].variant == k && part[c].static_frame == frames[k] && part[c].hidden &&
               part[c].angle == (uint32_t)(k + 1) * 5000 && part[c].timer == 6 * (k + 1) &&
               part[c].pe_radius == 0 && part[c].pe_radius_target == reach[k]);
    }
    /* Closed, the segments stay hidden and the pupil centred. */
    for (int c = 0; c < 8; ++c)
        aos_enemy_update(&part[c], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(part[2].hidden && part[2].x == eye.x && part[1].x == eye.x && !part[0].hidden);

    /* Soma within the box: the opening (40 frames of 0x08527400), then
     * facing him, a point 64 pixels short of him and 64 above. */
    soma.x = AOS_FIXED(160);
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.step == 1 && eye.pe_tick == 0);
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.static_frame == 3 && eye.pe_tick == 1);
    int count = 1;
    while (eye.step == 1 && count < 100) {
        aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++count;
    }
    assert(count == 40 && eye.step == 3 && (eye.pe_flags & 2) && eye.static_frame == 0 &&
           eye.mirrored && eye.target_x == soma.x - AOS_FIXED(64) &&
           eye.target_y == soma.y - AOS_FIXED(64) && eye.pe_tick == 1);

    /* Open, a segment sways in from its angle (the first pulled a quarter
     * toward 0x4000) and grows by a 64th of its reach; the pupil moves its
     * offset 0x4000 toward 3 * (cos, sin) of its aim. */
    aos_enemy_update(&part[2], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    int32_t r0 = reach[0] / 64, bent = 5000 - (5000 - 0x4000) / 4;
    assert(!part[2].hidden && part[2].step == 1 && part[2].angle == 5000 &&
           part[2].pe_radius == r0 &&
           part[2].x == eye.x + fixed_mul_of(aos_sine((uint32_t)bent + 0x4000u), r0) &&
           part[2].y == eye.y + fixed_mul_of(aos_sine((uint32_t)bent), r0) &&
           eye.pe_seg[0] == &part[2]);
    aos_enemy_update(&part[2], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(part[2].angle == 5500 && part[2].pe_radius == 2 * r0);
    aos_enemy_update(&part[3], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(part[3].x == part[2].x + fixed_mul_of(aos_sine(10000u + 0x4000u), reach[1] / 64));
    aos_enemy_update(&part[1], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(part[1].pe_off_x == 0x4000 && part[1].pe_off_y == -0x4000 &&
           part[1].x == eye.x + 0x4000 && part[1].mirrored == eye.mirrored);
    /* The tip turns by its angle plus half its distance from 0x4000. */
    aos_enemy_update(&part[7], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(part[7].pe_rotation == (uint16_t)(30000 + (30000 - 0x4000) / 2));

    /* Steering: no push within 4 pixels (x), -0x800 beyond (y). */
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.ax == 0 && eye.ay == -0x800 && eye.vy == -0x800 && eye.timer == 1 &&
           eye.static_frame == 0);
    /* Speeds keep 31/32 before the push. */
    eye.vx = 0x20000;
    eye.target_x = eye.x + AOS_FIXED(40);
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.vx == 0x20000 * 31 / 32 + 0x800 && eye.ax == 0x800);
    /* Within 2.0 (sub_0806D3D8). */
    eye.vx = 0x21000;
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.vx == 0x20000);
    /* Within 4 pixels it picks again; so does the 120-frame timeout. */
    eye.target_x = eye.x + 0x3FFFF;
    eye.target_y = eye.y - 0x3FFFF;
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.step == 2);
    /* Step 2: 16 + r % 48 pixels to a side of Soma (r & 1), 32 + r % 48
     * above him; the three draws in order. */
    const uint32_t picks[] = {0x10, 1, 0x20, 0xFF};
    script = picks;
    script_at = 0;
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, scripted);
    assert(script_at == 3 && eye.step == 3 && eye.timer == 1 &&
           eye.target_x == soma.x + 0x100010 && eye.target_y == soma.y - 0x200020);
    eye.timer = 0x78;
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.step == 2);

    /* A finished blink with Soma below looks down (look 2, frame 10). */
    eye.step = 3;
    eye.pe_seq = 0;
    eye.pe_index = 6;
    eye.pe_tick = 19;
    soma.x = eye.x + AOS_FIXED(1);
    soma.y = eye.y + AOS_FIXED(80);
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.pe_seq == 2 && eye.pe_index == 0 && eye.pe_tick == 1 && eye.static_frame == 10);
    /* A finished other look blinks again. */
    eye.pe_index = 1;
    eye.pe_tick = 19;
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.pe_seq == 0 && eye.static_frame == 0);

    /* A blow: the hurt flag and a 2.0 push away from Soma; next frame the
     * hurt look (frame 8), facing him. */
    soma.x = eye.x - AOS_FIXED(20);
    soma.y = eye.y;
    eye.vx = eye.vy = 0;
    AosSoulAttack blow = {{0, 0, 2000, 2000}, 20, 0, false, NULL, AOS_TYPE_SOUL};
    aos_enemy_set_soul_attacks(&blow, 1);
    AosHitReport hit = aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                        never_zero);
    aos_enemy_set_soul_attacks(NULL, 0);
    assert(hit.enemy_hit && !hit.killed && eye.pe_hurt && eye.hit_flash == 8 &&
           eye.vx == fixed_mul_of(aos_sine(0x4000), 0x20000) && eye.vy == 0);
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(!eye.pe_hurt && eye.pe_seq == 1 && eye.static_frame == 8 && !eye.mirrored &&
           eye.hit_flash == 7);
    /* While it flashes the segments hold their sway. */
    uint32_t held = part[2].angle;
    aos_enemy_update(&part[2], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(part[2].angle == held);

    /* A lethal blow: sound 0x6E, state 2 for 45 frames (hidden, frame 0,
     * one flame), state 3 for four, then gone. */
    eye.hit_flash = 0;
    memset(&eye.combat, 0, sizeof eye.combat);
    eye.combat.type = AOS_TYPE_ENEMY;
    aos_audio_clear();
    blow.power = 255;
    aos_enemy_set_soul_attacks(&blow, 1);
    hit = aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    aos_enemy_set_soul_attacks(NULL, 0);
    assert(hit.killed && eye.defeated && eye.state == 2 && eye.timer == 0x2D &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E) == 1);
    hit = aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.hidden && eye.static_frame == 0 && hit.effect_count == 1 &&
           hit.effects[0].kind == AOS_EFFECT_FLAME && hit.effects[0].x == eye.x);
    /* The eyeball and pupil go; each segment flames after 6 (k + 1) + 1
     * frames and again 4 later, 4 pixels down. */
    aos_enemy_update(&part[0], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    aos_enemy_update(&part[1], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(part[0].removed && part[1].removed);
    int flames[2] = {0}, lit = 0;
    for (int f = 1; f <= 20; ++f) {
        AosHitReport r =
            aos_enemy_update(&part[2], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        if (r.effect_count) {
            assert(r.effects[0].y == part[2].y + 0x40000 && lit < 2);
            flames[lit++] = f;
        }
        if (f == 1) assert(part[2].state == 1 && !part[2].hidden);
    }
    assert(flames[0] == 7 && flames[1] == 11 && part[2].hidden && !part[2].removed);
    count = 1;
    while (eye.state == 2 && count < 100) {
        aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++count;
    }
    assert(count == 45 && eye.state == 3);
    aos_enemy_update(&part[2], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(part[2].removed);
    count = 0;
    while (!eye.removed && count < 10) {
        aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++count;
    }
    assert(count == 4);

    /* Under the global 0x8E & 0x40: state 4, 48 pixels up, no boxes; 31
     * frames, the opening, then the looks. */
    aos_enemy_set_flag_8e_40(true);
    AosEnemy shown;
    assert(aos_enemy_create(&shown, AOS_ENEMY_PEEPING_EYE, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    count = 0;
    while (shown.step == 0 && count < 100) {
        aos_enemy_update(&shown, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++count;
    }
    assert(shown.state == 4 && shown.y == AOS_FIXED(52) && shown.combat.hurt_off && count == 31);
    count = 0;
    while (shown.step == 1 && count < 100) {
        aos_enemy_update(&shown, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++count;
    }
    assert(count == 40 && shown.step == 2 && (shown.pe_flags & 2));
    aos_enemy_set_flag_8e_40(false);
    memset(cells, 0, sizeof cells);
}
static void bone_pillar_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[25 * W + x] = 0x03;      /* floor top 200 */
    /* The exported frame durations of its animations 0-5. */
    static const uint8_t idle[1] = {200}, open_jaw[15] = {2, 1, 3, 3, 3, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3},
                         turn[4] = {12, 12, 12, 12}, spit[5] = {12, 12, 48, 14, 12},
                         breath[8] = {4, 4, 4, 4, 4, 4, 4, 4}, ball[3] = {2, 2, 3};
    const AosAnimDef defs[6] = {{1, idle}, {15, open_jaw}, {4, turn},
                                {5, spit}, {8, breath},    {3, ball}};
    const AosAnimSet anims = {defs, 6};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 32;
    const AosEnemyStats stats = {80, 20, 4, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(199), NULL);

    /* + 0x30 = 3: the unseen controller 22 below the floor's bottom skull;
     * its first update makes the three skulls, facing alternately. */
    AosEnemy b;
    assert(aos_enemy_create(&b, AOS_ENEMY_BONE_PILLAR, 100, 150, 3, 0, &soma, &layer, &kind,
                            &stats));
    int floor_y = (b.y >> 16) - 22;
    assert(b.param0 == 3 && b.hidden && b.combat.attack_off && floor_y >= 198 &&
           floor_y <= 201);
    AosHitReport first = aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_zero);
    assert(first.child_count == 3 && first.children[0].variant == 0 &&
           (first.children[0].y >> 16) == floor_y && (first.children[2].y >> 16) == floor_y - 44 &&
           first.children[0].mirrored != first.children[1].mirrored &&
           first.children[2].hp == 80);
    AosEnemy s[3];
    for (int i = 0; i < 3; ++i) {
        s[i] = first.children[i];
        s[i].parent = &b;
    }
    /* Active, the skulls register; the top one is the homing target. */
    aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    AosHitReport r[3];
    for (int i = 0; i < 3; ++i)
        r[i] = aos_enemy_update(&s[i], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                always_zero);
    assert(b.bp_active && b.bp_skull[2] == &s[2] && !r[0].targetable && !r[1].targetable);
    aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    r[2] = aos_enemy_update(&s[2], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(b.bp_top == 2 && r[2].targetable && (s[0].y >> 16) == floor_y);

    /* Soma within the 160 x 120 box: the attack. The even bottom skull
     * breathes fire (0x6E) at frame 2, the odd one spits a fireball (0x7A). */
    soma.x = s[0].x + AOS_FIXED(40);
    soma.y = s[0].y;
    aos_audio_clear();
    int breaths = 0, balls = 0, smokes = 0;
    for (int f = 0; f < 120; ++f) {
        aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        for (int i = 0; i < 2; ++i) {
            AosHitReport h = aos_enemy_update(&s[i], &soma, &layer, &kind, NULL, NULL, 10, 4, 0,
                                              0, always_zero);
            for (int c = 0; c < h.child_count; ++c) {
                breaths += h.children[c].role == AOS_ROLE_PILLAR_BREATH;
                balls += h.children[c].role == AOS_ROLE_PILLAR_FIREBALL;
                smokes += h.children[c].role == AOS_ROLE_PILLAR_SMOKE;
            }
        }
    }
    assert(breaths == 1 && balls == 1 && smokes == 3 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E) == 1 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x7A) == 1);

    /* A lethal blow on the bottom skull: 0x6B, three pieces, then gone; the
     * skulls above fall 22 pixels onto the floor. */
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    soma.x = s[0].x;
    soma.y = s[0].y + AOS_FIXED(10);
    aos_audio_clear();
    AosHitReport hit = aos_enemy_update(&s[0], &soma, &layer, &kind, &weapon, &blade, 9999, 4,
                                        0, 0, always_zero);
    assert(hit.killed && s[0].state == 2);
    AosHitReport pieces = aos_enemy_update(&s[0], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                           always_zero);
    assert(pieces.child_count == 3 && pieces.children[0].static_frame == 0x17 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x6B) == 1);
    int frames = 0;
    while (!s[0].removed && frames < 30) {
        aos_enemy_update(&s[0], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(s[0].removed && frames == 9);
    soma.x = AOS_FIXED(900);
    for (int f = 0; f < 30; ++f) {
        aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        aos_enemy_update(&s[1], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    }
    assert((s[1].y >> 16) == floor_y);
}

static void giant_ghost_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    static const uint8_t ticks[2] = {6, 6};
    AosAnimDef defs[9];
    for (int i = 0; i < 9; ++i) defs[i] = (AosAnimDef){2, ticks};
    const AosAnimSet anims = {defs, 9};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 9; ++a)
        for (int f = 0; f < 2; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -40, 32, 40};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {300, 30, 4, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(300), AOS_FIXED(100), NULL);

    /* Creation: animation 0, contact element 0x10, state 0; the first
     * update makes the aura (animation 6) and the two eyes (8, 7). */
    AosEnemy g;
    assert(aos_enemy_create(&g, AOS_ENEMY_GIANT_GHOST, 100, 120, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(g.state == 0 && g.contact_elements == 0x10 && g.anim.id == 0);
    /* The zero random picks the charge, but Soma is outside the 160 x 160
     * box: it drifts (step 1, animation 3). */
    AosHitReport first = aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_zero);
    assert(first.child_count == 3 && first.children[0].anim.id == 6 &&
           first.children[1].anim.id == 8 && first.children[2].anim.id == 7 &&
           first.children[0].translucent && g.step == 1 && g.anim.id == 3);
    AosEnemy aura = first.children[0], eye = first.children[1];
    aura.parent = eye.parent = &g;
    /* It speeds up by 0x1000 a frame toward 48 pixels over Soma for 21
     * frames, coasts 21, slows by 0x800 for 31, then picks again. */
    aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(g.speed == 0x1000 && g.vx > 0 && g.vy < 0);
    int frames = 1;
    while (g.step != 0 && frames < 200) {
        aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(frames == 21 + 21 + 31 && g.speed == 0x1000);   /* capped 0xC000, floored 0x1000 */
    /* The eye looks at Soma (to its right): 3 pixels out, less 1. */
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(eye.vx > 0x10000 && eye.x == g.x + eye.vx);
    /* The aura sits 4 pixels behind (it faces Soma, to its right) and 18
     * up, pulsing. */
    aos_enemy_update(&aura, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(g.mirrored && (aura.x >> 16) == (g.x >> 16) - 4 && (aura.y >> 16) == (g.y >> 16) - 18 &&
           aura.affine_sx == (aos_sine(0x400) >> 4) + 0x1B000);

    /* Soma within its box: the charge. Back away 21 + 21 + 31 frames, face
     * him 21, then (0x87) swoop at 2.0 and drift 46 frames, rest 31. */
    soma.x = g.x + AOS_FIXED(40);
    soma.y = g.y;
    aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(g.state == 1 && g.step == 0 && g.speed == 0);
    aos_audio_clear();
    frames = 0;
    while (g.step != 4 && frames < 300) {
        aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(frames == 21 + 21 + 31 + 21 && aos_audio_requested(AOS_AUDIO_START_SONG, 0x87) == 1 &&
           g.anim.id == 3);
    while (g.state == 1 && frames < 600) {
        aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(g.state == 0 && g.step == 0);

    /* Touching Soma (contact) flags it: next drift step it flees upward. */
    g.step = 1;
    g.gg_touched = true;
    aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(g.step == 10 && !g.gg_touched && g.anim.id == 2);
    aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(g.step == 11 && g.vy == -0x1000);

    /* A lethal blow: 0x6B, animation 5, translucent, four wisps, then 90
     * frames blinking out with a wisp every eighth past 40; the parts go. */
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    g.state = 0;
    g.step = 2;
    soma.x = g.x;
    soma.y = g.y;
    aos_audio_clear();
    AosHitReport hit = aos_enemy_update(&g, &soma, &layer, &kind, &weapon, &blade, 9999, 4, 0,
                                        0, always_zero);
    assert(hit.killed && g.state == 2 && g.timer == 0x5A && g.anim.id == 5 && g.translucent &&
           hit.effect_count == 5 && hit.effects[0].wisp &&
           hit.effects[4].kind == AOS_EFFECT_BLOOD &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x6B) == 1);
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    aos_enemy_update(&aura, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(eye.removed && aura.state == 1 && aura.timer == 0x6E);
    int wisps = 0;
    frames = 0;
    while (!g.removed && frames < 200) {
        AosHitReport r = aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_zero);
        wisps += r.effect_count;
        ++frames;
    }
    assert(frames == 0x5A && wisps == 7);   /* timer 89, 81, ..., 41 */
    /* A wisp falls with gravity 0x2000 for 200 frames. */
    AosEffect w = hit.effects[0];
    const AosEffectWorld world = {&layer, 0, 0, always_zero, 0, 0};
    int life = 0;
    while (aos_effect_update(&w, &anims, NULL, &world) && life < 300) ++life;
    assert(life == 199);

    /* The global 0x8E & 0x40: state 3, its eyes sweep a point. */
    aos_enemy_set_flag_8e_40(true);
    assert(aos_enemy_create(&g, AOS_ENEMY_GIANT_GHOST, 100, 120, 0, 0, &soma, &layer, &kind,
                            &stats));
    aos_enemy_set_flag_8e_40(false);
    first = aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(g.state == 3 && first.child_count == 3 && first.children[1].target_x == g.x - 0x200000);
    eye = first.children[1];
    eye.parent = &g;
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(eye.target_x == g.x - 0x1F0000 && eye.gg_scan == 1);
}

static void white_dragon_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    static const uint8_t ticks[2] = {4, 4};
    const AosAnimDef defs[2] = {{2, ticks}, {2, ticks}};
    const AosAnimSet anims = {defs, 2};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {200, 20, 4, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(500), AOS_FIXED(100), NULL);

    /* Facing right (parameter 0 set): heading 0, frame 0, its root at the
     * record; the first two updates make the ten segments. */
    AosEnemy d;
    assert(aos_enemy_create(&d, AOS_ENEMY_WHITE_DRAGON, 100, 100, 1, 0, &soma, &layer, &kind,
                            &stats));
    assert(d.mirrored && d.wd_heading == 0 && d.static_frame == 0 && d.wd_home_x == 100 &&
           d.state == 0 && d.own_hurt.h == 12);
    AosHitReport first = aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_zero);
    assert(first.child_count == 8 && first.children[0].variant == 9 &&
           first.children[0].role == AOS_ROLE_WHITE_DRAGON_PART);
    AosEnemy seg0 = first.children[7];
    AosHitReport second = aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                           always_zero);
    assert(second.child_count == 2 && second.children[1].variant == 0);
    AosEnemy seg_last = second.children[1];
    /* Two updates: neck 2, rate 0x20, swing 0x30, phase 0x280, amplitude 2;
     * the head at its neck length along the heading. */
    assert(d.wd_radius == 2 && d.wd_rate == 0x20 && d.wd_angle == 0x30 &&
           d.wd_phase == 0x280 && d.wd_swing == 2 && d.wd_history[0] == 0x30 &&
           d.wd_history[1] == 0x10);
    /* The neck grows to 72 pixels, the rate to 0x400, the amplitude to 0x80. */
    for (int i = 0; i < 200; ++i)
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(d.wd_radius == 0x48 && d.wd_rate == 0x400 && d.wd_swing == 0x80);
    int dx = (d.x >> 16) - 100, dy = (d.y >> 16) - 100;
    assert(dx * dx + dy * dy <= 73 * 73 && dx * dx + dy * dy >= 70 * 70 && dx > 0);
    /* Segment 0 sits at 9 / 10 of the neck, segment 9 at the root. */
    seg_last.parent = &d;
    aos_enemy_update(&seg_last, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    int sdx = (seg_last.x >> 16) - 100, sdy = (seg_last.y >> 16) - 100;
    assert(sdx * sdx + sdy * sdy >= 62 * 62 && sdx * sdx + sdy * sdy <= 66 * 66);
    AosEnemy root = first.children[0];
    root.parent = &d;
    aos_enemy_update(&root, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(root.x == AOS_FIXED(100) && root.y == AOS_FIXED(100));
    (void)seg0;

    /* A target in the bite box (84 x 200, 40 pixels ahead of the root):
     * the bite. The neck draws in, the heading turns to the target, then
     * the lunge (0x87) grows it to 100. */
    soma.x = AOS_FIXED(100 + 0x28 + 0x20);
    soma.y = d.y;
    d.wd_cool_a = 0;
    aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.state == 1 && d.step == 0 && d.wd_cool_a == 100 + 0x48);
    aos_audio_clear();
    int frames = 0;
    while (d.step == 0 && frames < 200) {
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(d.step == 1 && d.wd_radius == 0x20 && d.wd_rate == 0x800 && d.wd_swing == 0 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x87) == 1);
    while (d.state == 1 && frames < 400) {
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(d.state == 0 && d.wd_radius >= 0x60 && d.wd_cool_a == 0x3C && d.wd_cool_b == 0);

    /* The fireball breath (step 20): 60 frames of flames (0x77, frame 1)
     * and sparks, one opening frame (frame 2), then a fireball every fourth
     * of 30 frames (eight, 0x77 each) sweeping by 0x300. */
    d.state = 1;
    d.step = 20;
    d.timer = 0;
    d.wd_aim = 0;
    aos_audio_clear();
    int flames = 0, sparks = 0, balls = 0;
    AosEnemy ball = {0};
    frames = 0;
    while (d.state == 1 && frames < 300) {
        AosHitReport r = aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_zero);
        for (int c = 0; c < r.child_count; ++c) {
            flames += r.children[c].role == AOS_ROLE_DRAGON_FLAME;
            sparks += r.children[c].role == AOS_ROLE_DRAGON_SPARK;
            if (r.children[c].role == AOS_ROLE_DRAGON_FIREBALL) {
                if (!balls) ball = r.children[c];
                balls++;
            }
        }
        ++frames;
    }
    assert(flames == 4 + 8 && sparks == 8 && balls == 8 && frames == 61 + 1 + 61 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x77) == 2 + 8 && d.static_frame == 0);
    assert(ball.static_frame == 5 && ball.contact_elements == 2 && ball.vx == 0x40000);
    /* The fireball: 7 frames moving at 4.0, growing through frames 6-9. */
    int32_t bx = ball.x;
    for (frames = 0; !ball.removed && frames < 60; ++frames)
        aos_enemy_update(&ball, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(frames == 7 + 7 + 15 + 5 + 5 && ball.static_frame == 9 && ball.x > bx + AOS_FIXED(50));

    /* Struck by the weapon while idle: the recoil (state 3, frame 1) for
     * 31 frames (its flash does not hold state 3). */
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    d.state = d.step = 0;
    d.wd_cool_a = d.wd_cool_b = 100;
    soma.x = d.x;
    soma.y = d.y + AOS_FIXED(16);
    const AosCombat fresh = d.combat;
    AosHitReport hit = aos_enemy_update(&d, &soma, &layer, &kind, &weapon, &blade, 30, 4, 0, 0,
                                        always_zero);
    assert(hit.enemy_hit && d.state == 3 && d.static_frame == 1 && d.hit_flash == 8);
    for (frames = 0; d.state == 3 && frames < 100; ++frames)
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.state == 0 && d.static_frame == 0 && frames == 31);

    /* A lethal blow: 0x6E, 120 frames dying with flames, then the head goes
     * (0x6B, two pieces) and the segments fall apart; gone 50 frames on. */
    d.combat = fresh;
    d.hit_flash = 0;
    soma.x = d.x;
    soma.y = d.y + AOS_FIXED(16);
    aos_audio_clear();
    hit = aos_enemy_update(&d, &soma, &layer, &kind, &weapon, &blade, 9999, 4, 0, 0,
                           always_zero);
    assert(hit.killed && d.state == 2 && d.timer == 0x78 && d.static_frame == 1);
    for (frames = 0; d.step == 0 && frames < 300; ++frames)
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(frames == 0x78 && d.hidden && aos_audio_requested(AOS_AUDIO_START_SONG, 0x6B) == 1);
    seg_last.parent = &d;
    AosHitReport fall = {0};
    for (frames = 0; !seg_last.removed && frames < 60; ++frames) {
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        fall = aos_enemy_update(&seg_last, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                always_zero);
    }
    assert(seg_last.removed && d.timer == 4 && fall.child_count >= 1 &&
           fall.children[fall.child_count - 1].role == AOS_ROLE_DRAGON_PIECE);
    while (!d.removed && frames < 200) {
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(d.removed && d.timer == 0x33);

    /* The global 0x8E & 0x40: state 4, it only swings. */
    aos_enemy_set_flag_8e_40(true);
    assert(aos_enemy_create(&d, AOS_ENEMY_WHITE_DRAGON, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(d.state == 4 && d.wd_rate == 0x400 && d.wd_heading == 0x8000);
    aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.state == 4 && d.wd_rate == 0x400 && d.wd_angle == 0x400);
}

static void early_enemy_batch_tests(void) {
    assert(AOS_ENEMY_SKELETON == 0x02 && AOS_ENEMY_PEEPING_EYE == 0x06 &&
           AOS_ENEMY_KILLER_FISH == 0x07 && AOS_ENEMY_BONE_PILLAR == 0x08 &&
           AOS_ENEMY_BUER == 0x0A && AOS_ENEMY_WHITE_DRAGON == 0x0B &&
           AOS_ENEMY_GHOST == 0x0E && AOS_ENEMY_ROCK_ARMOR == 0x12 &&
           AOS_ENEMY_GIANT_GHOST == 0x13 && AOS_ENEMY_WINGED_SKELETON == 0x14);
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2, 2};
    const AosAnimDef defs[4] = {{2, ticks}, {2, ticks}, {2, ticks}, {2, ticks}};
    const AosAnimSet anims = {defs, 4};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    for (int a = 0; a < 4; ++a)
        for (int f = 0; f < 2; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -16, 16, 16};
        }
    const AosEnemyStats stats = {30, 16, 2, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(220), AOS_FIXED(120), NULL);
    const uint8_t ids[] = {AOS_ENEMY_SKELETON,
                           AOS_ENEMY_KILLER_FISH,
                           AOS_ENEMY_GHOST,
                           AOS_ENEMY_ROCK_ARMOR,
                           AOS_ENEMY_WINGED_SKELETON};
    for (unsigned i = 0; i < sizeof ids; ++i) {
        AosEnemy e;
        assert(aos_enemy_create(&e, ids[i], 100, 100, ids[i] == AOS_ENEMY_BONE_PILLAR ? 4 : 0,
                                0, &soma, &layer, &kind, &stats));
        assert(e.id == ids[i] && e.hp == stats.hp);
        int32_t x0 = e.x, y0 = e.y;
        uint8_t frame0 = e.anim.frame, tick0 = e.anim.tick;
        aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        assert(e.x != x0 || e.y != y0 || e.anim.frame != frame0 || e.anim.tick != tick0);
    }

    /* Source-derived Winged Skeleton producer: 1/32, cap, right-side
     * entry when Soma is near the left camera border. */
    AosEnemy winged_producer;
    AosSoma near_left = aos_soma_spawn(AOS_FIXED(10), AOS_FIXED(100), NULL);
    assert(aos_enemy_create(&winged_producer, AOS_ENEMY_WINGED_SKELETON,
                            100, 100, 2, 1, &near_left, &layer, &kind, &stats));
    assert(winged_producer.state == 3 && winged_producer.hidden);
    AosHitReport winged_spawn = aos_enemy_update(&winged_producer, &near_left, &layer,
        &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(winged_spawn.spawn && winged_spawn.spawn_x == 256 &&
           winged_spawn.spawn_y == 48 && winged_producer.spawned_count == 1);
    winged_spawn = aos_enemy_update(&winged_producer, &near_left, &layer,
        &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(winged_spawn.spawn && winged_producer.spawned_count == 2);
    winged_spawn = aos_enemy_update(&winged_producer, &near_left, &layer,
        &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(!winged_spawn.spawn);

    AosEnemy producer;
    assert(aos_enemy_create(&producer, AOS_ENEMY_GHOST, 100, 100, 0, 1, &soma, &layer, &kind,
                            &stats));
    assert(producer.state == 3 && producer.hidden && producer.combat.attack_off &&
           producer.combat.hurt_off);
    AosSoma distant = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(400), NULL);
    assert(aos_enemy_create(&producer, AOS_ENEMY_GHOST, 0, 0, 2, 1, &distant, &layer, &kind,
                            &stats));
    AosHitReport ghost_spawn = aos_enemy_update(&producer, &distant, &layer, &kind, NULL, NULL,
                                                 10, 4, 0, 0, always_zero);
    assert(!ghost_spawn.spawn && producer.producer_timer == 1);
    static const uint32_t ghost_spawn_draws[] = {0, 10, 20};
    script = ghost_spawn_draws;
    script_at = 0;
    ghost_spawn = aos_enemy_update(&producer, &distant, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                   scripted);
    assert(ghost_spawn.spawn && ghost_spawn.spawn_x == 10 && ghost_spawn.spawn_y == 36 &&
           producer.spawned_count == 1 && script_at == 3 && !ghost_spawn.targetable);

    AosEnemy ghost;
    soma = aos_soma_spawn(AOS_FIXED(140), AOS_FIXED(120), NULL);
    assert(aos_enemy_create(&ghost, AOS_ENEMY_GHOST, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(ghost.state == 0 && ghost.step == 0 && ghost.combat.attack_off &&
           ghost.combat.hurt_off && ghost.anim.id == 0);
    for (int frame = 0; frame < 8 && ghost.step == 0; ++frame)
        aos_enemy_update(&ghost, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(ghost.step == 1 && ghost.anim.id == 1 && !ghost.combat.attack_off &&
           !ghost.combat.hurt_off);
    int32_t ghost_x = ghost.x;
    aos_enemy_update(&ghost, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(ghost.ax == 0x400 && ghost.vx > 0 && ghost.x > ghost_x);
    ghost.x = soma.x;
    ghost.y = soma.y - AOS_FIXED(16);
    ghost.vx = ghost.vy = ghost.speed = 0;
    aos_enemy_update(&ghost, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(ghost.vx || ghost.vy);

    aos_enemy_set_flag_8e_40(true);
    assert(aos_enemy_create(&ghost, AOS_ENEMY_GHOST, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(ghost.state == 2 && ghost.y == AOS_FIXED(64));
    for (int frame = 0; frame < 8 && ghost.step == 0; ++frame)
        aos_enemy_update(&ghost, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(ghost.step == 1 && ghost.anim.id == 1 && ghost.x == AOS_FIXED(100));

    assert(aos_enemy_create(&producer, AOS_ENEMY_WINGED_SKELETON, 100, 100, 0, 1, &soma,
                            &layer, &kind, &stats));
    assert(producer.state == 3 && producer.hidden);

    aos_enemy_set_flag_8e_40(true);
    AosEnemy inert;
    assert(aos_enemy_create(&inert, AOS_ENEMY_SKELETON, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(inert.state == 5);
    int32_t inert_x = inert.x;
    aos_enemy_update(&inert, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(inert.x == inert_x);

    AosEnemy buer;
    assert(aos_enemy_create(&buer, AOS_ENEMY_BUER, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    AosHitReport buer_parts = aos_enemy_update(&buer, &soma, &layer, &kind, NULL, NULL, 10, 4,
                                               0, 0, never_zero);
    assert(buer_parts.child_count == 1 && buer_parts.children[0].role == AOS_ROLE_BUER_FIRE &&
           buer_parts.children[0].static_frame == 1);
    AosEnemy buer_fire = buer_parts.children[0];
    buer_fire.parent = &buer;
    aos_enemy_update(&buer_fire, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(buer_fire.x == buer.x && buer_fire.y == buer.y);
}

static void buer_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;      /* floor top 160 */
    static const uint8_t ticks[2] = {3, 3};
    const AosAnimDef defs[2] = {{2, ticks}, {2, ticks}};
    const AosAnimSet anims = {defs, 2};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    kind.probes = (AosProbes){1, -24, 23, 20, {0}};     /* 0x08528B64 */
    for (int i = 0; i < 8; ++i) {
        kind.buer_death_offsets[i][0] = (int16_t)(i * 4 - 16);
        kind.buer_death_offsets[i][1] = -8;
    }
    kind.buer_death_offsets_valid = true;
    const AosEnemyStats stats = {120, 30, 6, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(500), AOS_FIXED(159), NULL);

    /* Creation: frame 0, spin 0.5, gravity 0x4000, rolling state 1 on the
     * floor 24 pixels up, the (-20, -20, 40, 40) boxes; its fire faces
     * right. */
    AosEnemy b;
    assert(aos_enemy_create(&b, AOS_ENEMY_BUER, 100, 150, 0, 0, &soma, &layer, &kind, &stats));
    int32_t y0 = b.y;
    assert(b.state == 1 && b.spin == 0x8000 && b.ay == 0x4000 && b.static_frame == 0 &&
           b.own_hurt.w == 40 && b.contact_elements == 1 && b.buer_fire_mirrored);
    /* First update: the fire; it rolls 0.5 right, the spin decays by 0x40,
     * the angle turns by spin / 64; 92 pixels below it leave the room
     * (26 cells): state 2. */
    AosHitReport first = aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_zero);
    assert(first.child_count == 1 && first.children[0].role == AOS_ROLE_BUER_FIRE &&
           first.children[0].static_frame == 1);
    assert(b.x == AOS_FIXED(100) + 0x8000 && b.spin == 0x7FC0 && b.angle == 0x7FC0 / 64 &&
           b.state == 2 && b.step == 0);
    AosEnemy fire = first.children[0];
    fire.parent = &b;
    aos_enemy_update(&fire, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(fire.x == b.x && fire.y == b.y && fire.mirrored);
    /* Its spin keeps decaying to 0x4000 while it rolls. */
    for (int i = 0; i < 300; ++i)
        aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(b.spin == 0x4000 && b.state == 2);
    (void)y0;

    /* Soma in its 192 x 48 box on the fire's side (right): the attack. 31
     * frames spinning up by 0xCCC (capped at 2.0, which ends the stage),
     * ten pairs of bursts and three 0x84, then 33 frames rolling on. */
    soma.x = b.x + AOS_FIXED(60);
    soma.y = b.y;
    b.step = 0;
    b.timer = 0;
    aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(b.step == 1 && b.substep == 0);
    b.spin = 0x8000;
    aos_audio_clear();
    int bursts = 0, frames = 0;
    do {
        AosHitReport r = aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          never_zero);
        for (int c = 0; c < r.child_count; ++c)
            bursts += r.children[c].role == AOS_ROLE_BUER_PROJECTILE;
        ++frames;
    } while (b.substep != 2 && frames < 100);
    assert(frames == 31 && b.spin == 0x20000 && b.nem_passes == 1 && b.buer_fire_mirrored &&
           bursts == 20 && aos_audio_requested(AOS_AUDIO_START_SONG, 0x84) == 3 &&
           b.timer == 0x20);
    frames = 0;
    while (b.substep == 2 && frames < 100) {
        aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    assert(frames == 33 && b.substep == 0 && b.step == 1);
    /* A burst plays its animation once in place. */
    AosEnemy burst = {0};
    burst.id = AOS_ENEMY_BUER;
    burst.role = AOS_ROLE_BUER_PROJECTILE;
    burst.variant = 1;
    for (frames = 0; !burst.removed && frames < 20; ++frames)
        aos_enemy_update(&burst, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(burst.removed && burst.anim.id == 1 && frames == 6);

    /* The wait (step 2): 32 + (r & 31) frames, the fire turned round. */
    b.step = 2;
    b.substep = 0;
    soma.x = AOS_FIXED(900);
    bool facing = b.buer_fire_mirrored;
    aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(b.timer == 0x1F && b.buer_fire_mirrored != facing && b.substep == 2);
    for (frames = 0; b.step == 2 && frames < 100; ++frames)
        aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(frames == 32 && b.step == 0 && (b.spin < 0) == !b.buer_fire_mirrored);

    /* A surviving blow while it rolls starts the attack. */
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    soma.x = b.x;
    soma.y = b.y + AOS_FIXED(16);
    b.step = 0;
    const AosCombat fresh = b.combat;
    AosHitReport hit = aos_enemy_update(&b, &soma, &layer, &kind, &weapon, &blade, 40, 4, 0, 0,
                                        always_zero);
    assert(hit.enemy_hit && !hit.killed && b.step == 1 && b.substep == 0 && b.hit_flash == 8);

    /* A lethal blow: state 3. 97 frames with the fire wobbling and the
     * spin from 2.0, then the fire goes and 129 frames of 32 x 32 flames,
     * then eight flames and it is gone; 0x6E at the start, five times in
     * stage two and at the end. */
    b.combat = fresh;
    b.hit_flash = 0;
    aos_audio_clear();
    hit = aos_enemy_update(&b, &soma, &layer, &kind, &weapon, &blade, 9999, 4, 0, 0,
                           always_zero);
    assert(hit.killed && b.state == 3 && b.step == 0);
    aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(b.step == 1 && b.combat.attack_off && !b.buer_fire_following &&
           (b.spin == 0x20000 - 0x380 || b.spin == -0x20000 + 0x380));
    frames = 1;
    while (b.step == 1 && frames < 200) {
        aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(frames == 97 && b.buer_fire_gone);
    aos_enemy_update(&fire, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(fire.removed);
    int flames = 0;
    frames = 0;
    while (!b.removed && frames < 300) {
        AosHitReport r = aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_zero);
        flames += r.effect_count;
        ++frames;
    }
    assert(frames == 130 && flames == 65 + 8 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E) == 7);

    /* The global 0x8E & 0x40: state 4, its fire still made; it rises 32
     * pixels once and spins by 0x100 a frame. */
    aos_enemy_set_flag_8e_40(true);
    assert(aos_enemy_create(&b, AOS_ENEMY_BUER, 100, 150, 0, 0, &soma, &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    y0 = b.y;
    AosHitReport inert = aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_zero);
    aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(b.state == 4 && inert.child_count == 1 && b.y == y0 - AOS_FIXED(32) &&
           b.angle == 0x200);

    /* Out of the room it is deleted with its fire (sub_080CC55C). */
    assert(aos_enemy_create(&b, AOS_ENEMY_BUER, 100, 150, 1, 0, &soma, &layer, &kind, &stats));
    b.y = AOS_FIXED(300);
    aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(b.removed);
}

static void batch_enemy_tests(void) {
    assert(AOS_ENEMY_MERMAN == 0x03 && AOS_ENEMY_SIREN == 0x0F &&
           AOS_ENEMY_TINY_DEVIL == 0x10 && AOS_ENEMY_CATOBLEPAS == 0x1C &&
           AOS_ENEMY_KILLER_DOLL == 0x1F && AOS_ENEMY_NEMESIS == 0x26 &&
           AOS_ENEMY_IMP == 0x31 && AOS_ENEMY_GORGON == 0x33 &&
           AOS_ENEMY_HARPY == 0x38 && AOS_ENEMY_RIPPER == 0x4A);
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2, 2};
    const AosAnimDef defs[7] = {{2, ticks}, {2, ticks}, {2, ticks}, {2, ticks},
                                {2, ticks}, {2, ticks}, {2, ticks}};
    const AosAnimSet anims = {defs, 7};
    AosEnemyKind kind = {.anims = &anims};
    for (int a = 0; a < 7; ++a)
        for (int f = 0; f < 2; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -16, 16, 16};
        }
    const AosEnemyStats stats = {30, 16, 2, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(220), AOS_FIXED(159), NULL);
    const uint8_t ids[] = {AOS_ENEMY_MERMAN, AOS_ENEMY_SIREN, AOS_ENEMY_TINY_DEVIL,
                           AOS_ENEMY_CATOBLEPAS, AOS_ENEMY_IMP, AOS_ENEMY_GORGON,
                           AOS_ENEMY_HARPY, AOS_ENEMY_RIPPER};
    for (unsigned i = 0; i < sizeof ids; ++i) {
        AosEnemy e;
        int y = ids[i] == AOS_ENEMY_GORGON || ids[i] == AOS_ENEMY_CATOBLEPAS ||
                        ids[i] == AOS_ENEMY_RIPPER
                    ? 140
                    : 100;
        assert(aos_enemy_create(&e, ids[i], 100, y, 0, 0, &soma, &layer, &kind, &stats));
        assert(e.id == ids[i] && e.hp == stats.hp);
        int32_t x0 = e.x, y0 = e.y;
        aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        assert(e.x != x0 || e.y != y0);
    }

    /* Ripper uses the native six-way jump table: draw 0 is the steep
     * animation-0 hop, draw 3 is the long shallow attack hop. Its one-shot
     * animation creates the linked accelerating blade near the apex. */
    AosEnemy ripper;
    assert(aos_enemy_create(&ripper, AOS_ENEMY_RIPPER, 100, 140, 0, 0, &soma, &layer, &kind,
                            &stats));
    aos_enemy_update(&ripper, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(ripper.step == 6 && ripper.vx == 0xD000 && ripper.vy == -0x30000 &&
           ripper.ay == 0xA000 && ripper.anim.id == 0);
    assert(aos_enemy_create(&ripper, AOS_ENEMY_RIPPER, 100, 140, 0, 0, &soma, &layer, &kind,
                            &stats));
    static const uint32_t ripper_draws[] = {3};
    script = ripper_draws;
    script_at = 0;
    AosHitReport hit = aos_enemy_update(&ripper, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                        scripted);
    assert(ripper.step == 5 && ripper.vx == 0x1A000 && ripper.ay == 0x2800 && script_at == 1);
    AosEnemy blade = {0};
    bool got_blade = false;
    for (int frame = 0; frame < 80 && !got_blade; ++frame) {
        hit = aos_enemy_update(&ripper, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                               never_zero);
        if (hit.child_count) {
            assert(hit.child_count == 1 && hit.children[0].role == AOS_ROLE_RIPPER_BLADE);
            blade = hit.children[0];
            got_blade = true;
        }
    }
    assert(got_blade && blade.parent == &ripper && blade.vx == 0x30000);
    aos_audio_clear();
    int32_t blade_x = blade.x;
    aos_enemy_update(&blade, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(blade.x > blade_x && blade.vx > 0x30000);
    /* sub_080C2B74: sound once at the start of flight. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x89) == 1);
    soma.x = blade.x + blade.vx;
    soma.y = blade.y;
    hit = aos_enemy_update(&blade, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(hit.soma_hit && blade.state == 6 && blade.vy == 0x8000 && blade.ay == 0x1000);
    /* sub_080C2DC4: blade contact, without repeating the flight sound. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x13A) == 1);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x89) == 1);
    aos_audio_clear();
    soma = aos_soma_spawn(AOS_FIXED(220), AOS_FIXED(159), NULL);
    ripper.state = 0xFF;
    ripper.step = 0;
    aos_enemy_update(&ripper, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(ripper.step == 1 && ripper.anim.id == 2 && ripper.timer == 49 && ripper.ay == 0x2000);
    /* sub_080C2A18: play once on the first death update. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x72) == 1);
    aos_enemy_update(&ripper, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                     never_zero);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x72) == 1);

    /* Merman's hidden form produces at most three waiting bodies. The body
     * attack plays animation 3 and launches its animation-6 horizontal shot;
     * state 2 is its 40-frame falling death. */
    AosEnemy spawner;
    assert(aos_enemy_create(&spawner, AOS_ENEMY_MERMAN, 100, 100, 1, 1, &soma, &layer, &kind,
                            &stats));
    hit = aos_enemy_update(&spawner, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(hit.spawn && hit.spawn_x == 0x90 && hit.spawn_y == 132 &&
           spawner.spawned_count == 1);
    hit = aos_enemy_update(&spawner, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(hit.spawn && spawner.spawned_count == 2);
    hit = aos_enemy_update(&spawner, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(hit.spawn && spawner.spawned_count == 3);
    hit = aos_enemy_update(&spawner, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(!hit.spawn && spawner.spawned_count == 3);
    AosEnemy merman;
    assert(aos_enemy_create(&merman, AOS_ENEMY_MERMAN, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    merman.state = 1;
    merman.step = 0;
    AosEnemy merman_projectile = {0};
    bool got_projectile = false;
    aos_audio_clear();
    for (int frame = 0; frame < 20 && !got_projectile; ++frame) {
        hit = aos_enemy_update(&merman, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                               never_zero);
        if (hit.child_count) {
            assert(hit.child_count == 1 && hit.children[0].role == AOS_ROLE_MERMAN_SHOT);
            merman_projectile = hit.children[0];
            got_projectile = true;
            /* sub_080714A4 plays 0x7A with the shot. */
            assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x7A));
        }
    }
    assert(got_projectile && merman_projectile.anim.id == 6 && merman_projectile.vx == 0x20000);
    int32_t shot_x = merman_projectile.x;
    aos_enemy_update(&merman_projectile, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                     never_zero);
    assert(merman_projectile.x > shot_x);
    merman.state = 2;
    merman.timer = 40;
    merman.vy = 0;
    merman.ay = 0x2000;
    aos_enemy_update(&merman, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(merman.timer == 39 && merman.vy == 0x2000);
    /* A killing hit plays its death sound 0x6E (sub_080715B4). */
    AosEnemy doomed;
    assert(aos_enemy_create(&doomed, AOS_ENEMY_MERMAN, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    AosSoulAttack blast = {{0, 0, 300, 300}, 999, 0, false, NULL, AOS_TYPE_SOUL};
    aos_enemy_set_soul_attacks(&blast, 1);
    aos_audio_clear();
    for (int frame = 0; frame < 4 && !doomed.defeated; ++frame)
        aos_enemy_update(&doomed, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    aos_enemy_set_soul_attacks(NULL, 0);
    assert(doomed.defeated && doomed.state == 2 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E));

    /* The two native aliases execute the same locomotion from equal state. */
    AosEnemy harpy, siren, gorgon, catoblepas;
    assert(aos_enemy_create(&harpy, AOS_ENEMY_HARPY, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(aos_enemy_create(&siren, AOS_ENEMY_SIREN, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    siren.phase = harpy.phase;
    aos_enemy_update(&harpy, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    aos_enemy_update(&siren, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(harpy.x == siren.x && harpy.y == siren.y && harpy.vx == siren.vx);
    assert(aos_enemy_create(&gorgon, AOS_ENEMY_GORGON, 100, 140, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(aos_enemy_create(&catoblepas, AOS_ENEMY_CATOBLEPAS, 100, 140, 0, 0, &soma, &layer,
                            &kind, &stats));
    aos_enemy_update(&gorgon, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    aos_enemy_update(&catoblepas, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(gorgon.x == catoblepas.x && gorgon.y == catoblepas.y);

    /* Harpy's two native volleys are frames 12/10/8 then 11/9. Siren
     * aliases the body state machine but fires its three-note 14/13/14 fan. */
    soma.x = AOS_FIXED(140);
    AosEnemy flyer;
    assert(aos_enemy_create(&flyer, AOS_ENEMY_HARPY, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    int harpy_shots = 0;
    aos_audio_clear();
    for (int frame = 0; frame < 60; ++frame) {
        AosHitReport hit = aos_enemy_update(&flyer, &soma, &layer, &kind, NULL, NULL, 10, 4, 0,
                                            0, always_zero);
        for (int i = 0; i < hit.child_count; ++i) {
            assert(hit.children[i].role == AOS_ROLE_HARPY_SHOT);
            assert(hit.children[i].static_frame >= 8 && hit.children[i].static_frame <= 12);
            ++harpy_shots;
        }
    }
    assert(harpy_shots == 5 && flyer.state == 0);
    /* sub_0807594C: 0x89 after the three feathers and after the two. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x89) == 2);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x8B) >= 1);
    assert(aos_enemy_create(&flyer, AOS_ENEMY_HARPY, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    for (int frame = 0; frame < 30; ++frame)
        aos_enemy_update(&flyer, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                         always_zero);
    int32_t dive_x = flyer.x;
    aos_audio_clear();
    aos_enemy_update(&flyer, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(flyer.state == 1 && flyer.step == 1 && flyer.anim.id == 2);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x87) == 1);
    aos_enemy_update(&flyer, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(flyer.x != dive_x);
    assert(aos_enemy_create(&flyer, AOS_ENEMY_SIREN, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    int siren_shots = 0;
    AosEnemy note = {0};
    aos_audio_clear();
    for (int frame = 0; frame < 60; ++frame) {
        AosHitReport hit = aos_enemy_update(&flyer, &soma, &layer, &kind, NULL, NULL, 10, 4, 0,
                                            0, always_zero);
        for (int i = 0; i < hit.child_count; ++i) {
            assert(hit.children[i].role == AOS_ROLE_SIREN_SHOT);
            assert(hit.children[i].static_frame == 13 || hit.children[i].static_frame == 14);
            note = hit.children[i];
            ++siren_shots;
        }
    }
    assert(siren_shots == 3 && flyer.state == 0);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x89) == 1);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x8B) >= 1);
    assert(aos_enemy_create(&flyer, AOS_ENEMY_SIREN, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    for (int frame = 0; frame < 30; ++frame)
        aos_enemy_update(&flyer, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                         always_zero);
    aos_audio_clear();
    aos_enemy_update(&flyer, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(flyer.state == 1 && flyer.step == 1 && flyer.anim.id == 2);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x87) == 1);
    /* A killing hit: sub_08075CAC's sound 0x1C3. */
    AosEnemy doomed_harpy;
    assert(aos_enemy_create(&doomed_harpy, AOS_ENEMY_HARPY, 100, 100, 0, 0, &soma, &layer,
                            &kind, &stats));
    AosSoulAttack harpy_blast = {{0, 0, 300, 300}, 999, 0, false, NULL, AOS_TYPE_SOUL};
    aos_enemy_set_soul_attacks(&harpy_blast, 1);
    aos_audio_clear();
    for (int frame = 0; frame < 4 && !doomed_harpy.defeated; ++frame)
        aos_enemy_update(&doomed_harpy, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                         always_zero);
    aos_enemy_set_soul_attacks(NULL, 0);
    assert(doomed_harpy.defeated && aos_audio_requested(AOS_AUDIO_START_SONG, 0x1C3) == 1);
    AosEnemy doomed_siren;
    assert(aos_enemy_create(&doomed_siren, AOS_ENEMY_SIREN, 100, 100, 0, 0, &soma, &layer,
                            &kind, &stats));
    aos_enemy_set_soul_attacks(&harpy_blast, 1);
    aos_audio_clear();
    for (int frame = 0; frame < 4 && !doomed_siren.defeated; ++frame)
        aos_enemy_update(&doomed_siren, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                         always_zero);
    aos_enemy_set_soul_attacks(NULL, 0);
    assert(doomed_siren.defeated && aos_audio_requested(AOS_AUDIO_START_SONG, 0x1C3) == 1);
    uint32_t note_angle = note.angle;
    int32_t note_x = note.x;
    aos_enemy_update(&note, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(note.angle == (uint16_t)(note_angle + 0x400) && note.x != note_x);

    /* Gorgon and Catoblepas share the animation-2 wind-up and the native
     * 30..70-frame breath stream (one child every even frame). */
    assert(aos_enemy_create(&gorgon, AOS_ENEMY_GORGON, 100, 140, 0, 0, &soma, &layer, &kind,
                            &stats));
    int breaths = 0, breaths_at_song[2] = {-1, -1}, songs = 0;
    AosEnemy cloud = {0};
    aos_audio_clear();
    for (int frame = 0; frame < 180 && breaths < 21; ++frame) {
        AosHitReport hit = aos_enemy_update(&gorgon, &soma, &layer, &kind, NULL, NULL, 10, 4, 0,
                                            0, always_zero);
        int heard = aos_audio_requested(AOS_AUDIO_START_SONG, 0x67);
        for (int i = 0; i < hit.child_count; ++i) {
            assert(hit.children[i].role == AOS_ROLE_GORGON_BREATH);
            assert(hit.children[i].static_frame == 25);
            cloud = hit.children[i];
            ++breaths;
        }
        if (heard > songs) breaths_at_song[songs++] = breaths;
    }
    assert(breaths == 21 && gorgon.state == 1);
    /* sub_08089560: 0x67 with the first breath (+ 0x0D 0x1E) and the
     * eleventh (+ 0x0D 0x32). */
    assert(songs == 2 && breaths_at_song[0] == 1 && breaths_at_song[1] == 11);
    for (int frame = 0; frame < 16 && gorgon.state == 1; ++frame)
        aos_enemy_update(&gorgon, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                         always_zero);
    assert(gorgon.state == 0);
    for (int i = 0; i < 25; ++i)
        aos_enemy_update(&cloud, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(cloud.static_frame == 26 && !cloud.removed);
    for (int i = 0; i < 16; ++i)
        aos_enemy_update(&cloud, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(cloud.removed);

    /* Tiny Devil waits through its native-range cooldown, performs the
     * 31-frame approach and creates one aimed animation-4 orb. The orb
     * charges through animations 5/6, travels and expires after 60 frames. */
    AosEnemy devil;
    assert(aos_enemy_create(&devil, AOS_ENEMY_TINY_DEVIL, 100, 100, 0, 0, &soma, &layer,
                            &kind, &stats));
    AosEnemy orb = {0};
    bool got_orb = false;
    for (int frame = 0; frame < 220 && !got_orb; ++frame) {
        AosHitReport hit = aos_enemy_update(&devil, &soma, &layer, &kind, NULL, NULL, 10, 4, 0,
                                            0, always_zero);
        for (int i = 0; i < hit.child_count; ++i)
            if (hit.children[i].role == AOS_ROLE_TINY_DEVIL_ORB) {
                orb = hit.children[i];
                got_orb = true;
            }
    }
    assert(got_orb && orb.anim.id == 4 && orb.own_attack.w == 10 && orb.own_attack.h == 10);
    int orb_frames = 0;
    bool launched = false;
    aos_audio_clear();
    while (!orb.removed && orb_frames < 200) {
        aos_enemy_update(&orb, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        if (orb.state == 2) launched = true;
        ++orb_frames;
    }
    assert(launched && orb.removed && orb_frames > 60);
    /* sub_080C5A44: the launch's sound 0x89, once. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x89) == 1);

    /* Imp creates its hidden linked helper on the first update, then moves
     * through the approach and possession states. At Soma it reproduces
     * SetPlayerInput(1, 0/B) on alternating frames. */
    AosEnemy imp;
    assert(aos_enemy_create(&imp, AOS_ENEMY_IMP, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    AosHitReport imp_hit = aos_enemy_update(&imp, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                            always_zero);
    assert(imp_hit.child_count == 1 &&
           imp_hit.children[0].role == AOS_ROLE_IMP_HELPER && imp_hit.children[0].hidden);
    AosEnemy helper = imp_hit.children[0];
    bool forced_attack = false, forced_release = false, helper_visible = false;
    for (int frame = 0; frame < 420 && !(forced_attack && forced_release); ++frame) {
        imp_hit = aos_enemy_update(&imp, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                   always_zero);
        if (imp_hit.force_input) {
            forced_attack |= imp_hit.forced_held == AOS_KEY_ATTACK;
            forced_release |= imp_hit.forced_held == 0;
        }
        helper.parent = &imp;
        aos_enemy_update(&helper, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        helper_visible |= !helper.hidden;
    }
    assert(forced_attack && forced_release && helper_visible);
    assert(imp.state == 3 && imp.step == 1 && imp.patience == 0);
    soma.state = 12;
    for (int frame = 0; frame < 30 && imp.step == 1; ++frame)
        aos_enemy_update(&imp, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(imp.step == 2 && imp.patience == 21);
    for (int frame = 0; frame < 50 && imp.state == 3; ++frame)
        aos_enemy_update(&imp, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(imp.state == 1);
    soma.state = 0;

    /* Both bodies use the 50-frame falling death; Imp also emits direct
     * frame 0x15 as its separate falling child. */
    devil.defeated = true;
    devil.state = 0xFF;
    devil.step = 0;
    devil.timer = 0x32;
    devil.vy = 0x8000;
    aos_audio_clear();
    for (int frame = 0; frame < 50 && !devil.removed; ++frame) {
        AosHitReport devil_death = aos_enemy_update(&devil, &soma, &layer, &kind, NULL, NULL,
                                                    10, 4, 0, 0, always_zero);
        assert(devil_death.child_count == 0);
    }
    assert(devil.removed);
    /* sub_080C580C step 0: sound 0x72, once. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x72) == 1);
    imp.defeated = true;
    imp.state = 0xFF;
    imp.step = 0;
    imp.timer = 0x32;
    aos_audio_clear();
    imp.vy = 0x8000;
    imp.helper_spawned = true;
    AosHitReport imp_death = aos_enemy_update(&imp, &soma, &layer, &kind, NULL, NULL, 10, 4, 0,
                                              0, always_zero);
    assert(imp_death.child_count == 1 &&
           imp_death.children[0].role == AOS_ROLE_IMP_DEATH &&
           imp_death.children[0].static_frame == 0x15);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x72) == 1);     /* sub_080C4ED4 */
    AosEnemy imp_piece = imp_death.children[0];
    for (int frame = 0; frame < 50 && !imp_piece.removed; ++frame)
        aos_enemy_update(&imp_piece, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                         always_zero);
    assert(imp_piece.removed);

    /* The specialized deaths emit four Harpy fragments or six Gorgon
     * frames before their generic flame/blink lifetime. */
    flyer.state = 2;
    flyer.step = 0;
    flyer.timer = 0x28;
    flyer.defeated = true;
    AosHitReport death = aos_enemy_update(&flyer, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_zero);
    assert(death.child_count == 4);
    for (int i = 0; i < 4; ++i)
        assert(death.children[i].role == AOS_ROLE_GORGON_DEBRIS &&
               death.children[i].static_frame == 8);
    gorgon.state = 2;
    gorgon.step = 0;
    gorgon.timer = 0x3C;
    gorgon.defeated = true;
    death = aos_enemy_update(&gorgon, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                             always_zero);
    assert(death.child_count == 6);
    for (int i = 0; i < 6; ++i)
        assert(death.children[i].role == AOS_ROLE_GORGON_DEBRIS &&
               death.children[i].static_frame == 17 + i);

    aos_enemy_set_flag_8e_40(true);
    AosEnemy frozen;
    assert(aos_enemy_create(&frozen, AOS_ENEMY_HARPY, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    aos_enemy_set_flag_8e_40(false);
    int32_t x0 = frozen.x, y0 = frozen.y;
    aos_enemy_update(&frozen, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(frozen.state == 4 && frozen.x == x0 && frozen.y == y0);
}

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
    const AosEnemyStats stats = {18, 9, 1, 0x0021, 0x0010, 0};

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
    /* It walks left in bursts of 2.0 that decay by 3/4 per frame; on walk
     * frame 0 it drops a drip at (+9, -24) (animation 4, unmirrored) and a
     * puddle at its feet, once each. */
    int32_t x0 = z.x;
    AosHitReport walk_hit = aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 48,
                                             always_zero);
    assert(z.x < x0 && (z.y >> 16) == 159);
    assert(walk_hit.child_count == 2);
    AosEnemy drip = walk_hit.children[0], puddle = walk_hit.children[1];
    assert(drip.role == AOS_ROLE_DRIP && drip.static_frame == 0xD && drip.x == z.x + 0x90000);
    assert(drip.y == z.y - 0x180000 && drip.timer == 0x1E && drip.ay == 0xA00);
    assert(puddle.role == AOS_ROLE_PUDDLE && puddle.static_frame == 0 && puddle.x == z.x);
    walk_hit = aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 48, always_zero);
    assert(walk_hit.child_count == 0);
    /* sub_0807B310: the drip falls for 31 updates; sub_0807B3C8: the puddle
     * stays 121. */
    int life = 0;
    int32_t dy0 = drip.y;
    while (!drip.removed && life < 100) {
        aos_enemy_update(&drip, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 48, always_zero);
        ++life;
    }
    assert(life == 31 && drip.y > dy0 && drip.vy == 31 * 0xA00);
    life = 0;
    while (!puddle.removed && life < 300) {
        aos_enemy_update(&puddle, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 48, always_zero);
        ++life;
    }
    assert(life == 121);
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{0, -30, 30, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    /* A blow it survives: 8 frames frozen (sub_0806AD24), the collision
     * pass still running. */
    z.hp = 30;
    AosEnemy before = z;
    AosHitReport hit = aos_enemy_update(&z, &soma, &layer, &kind, &weapon, &blade, 10, 4, 100,
                                        48, always_zero);
    assert(hit.enemy_hit && !hit.killed && z.hit_flash == 8);
    AosEnemy frozen = z;
    for (int i = 0; i < 8; ++i) {
        aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 48, always_zero);
        assert(z.x == frozen.x && z.anim.tick == frozen.anim.tick && z.hit_flash == 7 - i);
    }
    z = before;
    z.hp = 1;
    /* The killing blow: sub_0807B258 throws two pieces (frame 0xD) from
     * x - 4 and y - 26 (random() = 0). */
    aos_audio_clear();
    hit = aos_enemy_update(&z, &soma, &layer, &kind, &weapon, &blade, 10, 4, 100, 48, always_zero);
    assert(hit.enemy_hit && hit.killed && z.state == 1 && z.anim.id == 3 && z.timer == 0x28);
    /* sub_0807B0DC: sound 0x70. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x70) == 1);
    /* (The walk of that frame may also drop a drip and a puddle first.) */
    assert(hit.child_count >= 2);
    const AosEnemy *gore = &hit.children[hit.child_count - 2];
    assert(gore[0].role == AOS_ROLE_GIB && gore[1].role == AOS_ROLE_GIB);
    assert(gore[0].static_frame == 0xD && gore[0].x == z.x - 0x40000);
    assert(gore[0].y == z.y - 0x1A0000 && gore[0].vx == -0x10000);
    assert(gore[0].vy == -0x10000 && gore[0].walk_timer == z.y);
    /* sub_0806AEAC: a flame every 4 frames (timer 40, 36 .. 4), over 16 x
     * 12 pixels centred 6 pixels above it. */
    frames = 0;
    int flames = 0;
    while (!z.removed && frames < 100) {
        int32_t zx = z.x, zy = z.y;
        hit = aos_enemy_update(&z, &soma, &layer, &kind, &weapon, &blade, 10, 4, 100, 48,
                               always_zero);
        if (hit.effect_count) {
            assert(hit.effect_count == 1 && hit.effects[0].x == zx - 0x80000);
            assert(hit.effects[0].y == zy - 0x60000 - 0x60000 && !hit.effects[0].mirrored);
            ++flames;
        }
        ++frames;
    }
    assert(frames == 40 && flames == 10);

    /* Under gEwramData + 0x8E bit 0x40: state 2 (sub_0807B034), no floor
     * snap; once the rise ends it walks in place with a drip at the start
     * of each cycle, without a collision pass. */
    aos_enemy_set_flag_8e_40(true);
    AosEnemy idle;
    assert(aos_enemy_create(&idle, AOS_ENEMY_ZOMBIE, 250, 140, 0, 0, &soma, &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(idle.state == 2 && (idle.y >> 16) == 140);
    int drips = 0;
    soma.x = idle.x;
    soma.y = idle.y;
    for (int i = 0; i < 40; ++i) {
        hit = aos_enemy_update(&idle, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 48, always_zero);
        assert(!hit.soma_hit && idle.x == AOS_FIXED(250));
        for (int c = 0; c < hit.child_count; ++c) {
            assert(hit.children[c].role == AOS_ROLE_DRIP);
            assert(hit.children[c].x == idle.x + 0x90000);
            ++drips;
        }
    }
    /* The walk starts already stepped (tick 1) on the frame it is played,
     * so only the later returns to frame 0, tick 0 drip within 40 frames
     * of a 12-tick cycle. */
    assert(idle.step == 1 && idle.anim.id == 4 && drips == 2);
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
    const AosEnemyStats stats = {6, 8, 0, 0, 0, 0};

    /* The crow perches (animation 0) until the player is within 60 pixels. */
    AosSoma soma = aos_soma_spawn(AOS_FIXED(300), AOS_FIXED(159), NULL);
    AosEnemy crow;
    assert(aos_enemy_create(&crow, AOS_ENEMY_BLUE_CROW, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(crow.state == 1 && crow.anim.id == 0 && crow.mirrored);
    /* Under gEwramData + 0x8E bit 0x40: state 3 (sub_080C9FFC), animation
     * 0 stepped in place, no collision pass. */
    aos_enemy_set_flag_8e_40(true);
    AosEnemy idle;
    assert(aos_enemy_create(&idle, AOS_ENEMY_BLUE_CROW, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(idle.state == 3);
    AosSoma near = aos_soma_spawn(idle.x, idle.y, NULL);
    for (int i = 0; i < 10; ++i) {
        AosHitReport idle_hit = aos_enemy_update(&idle, &near, &layer, &kind, NULL, NULL, 10, 4, 0,
                                                 0, never_zero);
        assert(!idle_hit.soma_hit && idle.state == 3 && idle.step == 1 && idle.x == AOS_FIXED(100));
    }
    /* Ten steps of the 4 + 4 tick flap: back on frame 0. */
    assert(idle.anim.frame == 0 && idle.anim.flags & AOS_ANIM_LOOP);
    aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(crow.step == 1);
    soma.x = AOS_FIXED(160);
    soma.y = AOS_FIXED(160);
    aos_audio_clear();
    aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(crow.step == 3 && crow.substep == 0);
    /* sub_080C9AF4: sound 0x8B as it takes off. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x8B) == 1);
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

    soma.facing_left = false;
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    soma.x = crow.x;
    soma.y = crow.y + AOS_FIXED(30);
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-10, -40, 20, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    /* sub_0806AD24: a blow it survives freezes its flight and animation
     * while sub_0806AE54 counts + 0x2D down from 8. */
    const AosCombat unhit = crow.combat;
    AosHitReport hit = aos_enemy_update(&crow, &soma, &layer, &kind, &weapon, &blade, 1, 4, 0, 0,
                                        never_zero);
    assert(hit.enemy_hit && !hit.killed && crow.hp == 5 && crow.hit_flash == 8);
    AosEnemy frozen = crow;
    for (int i = 0; i < 8; ++i) {
        aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 1, 4, 0, 0, never_zero);
        assert(crow.x == frozen.x && crow.y == frozen.y && crow.anim.tick == frozen.anim.tick);
        assert(crow.hit_flash == 7 - i);
    }
    aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 1, 4, 0, 0, never_zero);
    assert(crow.anim.tick != frozen.anim.tick || crow.anim.frame != frozen.anim.frame);
    crow.combat = unhit;            /* clear the weapon's recent-hit slot */
    soma.x = crow.x;
    soma.y = crow.y + AOS_FIXED(30);

    /* A killing blow: state 2, a 64-frame fall at 0.5, then the deletion. */
    hit = aos_enemy_update(&crow, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, never_zero);
    assert(hit.killed && crow.state == 2 && crow.step == 0);
    int32_t x0 = crow.x, y0 = crow.y;
    int dying = 0, blood = 0, flames = 0;
    AosEnemy feathers[2];
    aos_audio_clear();
    while (!crow.removed && dying < 100) {
        hit = aos_enemy_update(&crow, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0,
                               never_zero);
        if (dying == 0) {
            /* sub_080C9E2C: two feathers; sub_0806E664 at the crow, then 4
             * pixels below its new position (timer 0x40). */
            assert(hit.child_count == 2 && hit.effect_count == 2);
            feathers[0] = hit.children[0];
            feathers[1] = hit.children[1];
            assert(hit.effects[0].kind == AOS_EFFECT_BLOOD && hit.effects[0].y == (y0 >> 16) << 16);
            assert(hit.effects[1].y == (((y0 + 0x8000) >> 16) + 4) << 16 && hit.effects[1].mirrored);
        }
        for (int i = 0; i < hit.effect_count; ++i) {
            blood += hit.effects[i].kind == AOS_EFFECT_BLOOD;
            flames += hit.effects[i].kind == AOS_EFFECT_FLAME;
        }
        ++dying;
    }
    assert(dying == 64 && crow.anim.id == 3 && crow.y == y0 + 64 * 0x8000);
    assert(blood == 5 && flames == 10);
    /* sub_080C9D68 step 0: sound 0x72, once. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x72) == 1);

    /* sub_080C9ED4: frame 8, (x + 8, y + 4) drifting right and (x - 8,
     * y - 4) mirrored drifting left; 49 + 25 frames, blinking on bit 0. */
    assert(feathers[0].role == AOS_ROLE_FEATHER && feathers[0].static_frame == 8);
    assert(feathers[0].x == ((x0 >> 16) + 8) << 16 && feathers[0].y == ((y0 >> 16) + 4) << 16);
    assert(feathers[1].x == ((x0 >> 16) - 8) << 16 && feathers[1].y == ((y0 >> 16) - 4) << 16);
    AosEnemy right = feathers[0], left = feathers[1];
    aos_enemy_update(&right, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    aos_enemy_update(&left, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(right.x == feathers[0].x + 0x4000 && right.y == feathers[0].y + 0x2000);
    assert(right.vx == 0x4000 - 0x300 && right.hidden && !right.mirrored);
    assert(left.x == feathers[1].x - 0x4000 && left.vx == -0x4000 + 0x300 && left.mirrored);
    int life = 1;
    while (!right.removed && life < 200) {
        hit = aos_enemy_update(&right, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        assert(!hit.soma_hit);
        if (life == 48) assert(right.state == 2 && right.timer == 0x18);
        ++life;
    }
    assert(life == 74);
}

static void red_crow_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    static const uint8_t flap[] = {4, 4}, land[] = {5, 5}, death[] = {200};
    const AosAnimDef defs[5] = {{2, flap}, {2, flap}, {2, flap}, {1, death}, {2, land}};
    const AosAnimSet anims = {defs, 5};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {30, 20, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(159), NULL);

    /* It perches until the player is within 120 pixels, then hovers 128
     * frames (animation 2) and swoops 64 (animation 1). */
    AosEnemy c;
    assert(aos_enemy_create(&c, AOS_ENEMY_RED_CROW, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    assert(c.state == 1);
    aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(c.step == 1);
    soma.x = AOS_FIXED(200);
    aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(c.step == 3);
    int frames = 0;
    while (c.step == 3 && frames < 300) {
        aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    assert(frames == 129 && c.step == 2 && c.volley_count == 1);
    aos_audio_clear();
    aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(c.anim.id == 1 && c.vy == 0x20000 - 0x1000 && c.timer == 63);
    /* sub_080CA0D4: the swoop's sound 0x8B. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x8B) == 1);
    int32_t y0 = c.y;
    frames = 0;
    int32_t low = c.y;
    while (c.step == 2 && frames < 100) {
        aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        if (c.y > low) low = c.y;
        ++frames;
    }
    /* A V-shaped swoop: down, then back up. */
    assert(c.step == 3 && low > y0 && c.y < low);
    /* An odd round other than the first with the player away: it lands. */
    soma.x = AOS_FIXED(1000);
    frames = 0;
    while (c.step != 4 && frames < 1000) {
        aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    assert(c.step == 4 && c.volley_count == 3);
    frames = 0;
    while (c.step != 1 && frames < 100) {
        aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    assert(c.step == 1 && c.anim.id == 0);
}

static void mimic_tests(void) {
    /* Floor at cell row 20 (top 160). */
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t open_ticks[] = {2, 4, 5}, bite[] = {7, 3}, death[] = {200};
    const AosAnimDef defs[4] = {{3, open_ticks}, {2, bite}, {1, death}, {1, death}};
    const AosAnimSet anims = {defs, 4};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int a = 0; a < 4; ++a)
        for (int f = 0; f < 3; ++f) {
            kind.modes[a][f] = 2;
            kind.hurt[a][f] = (AosBox){-14, -32, 28, 32};
            kind.attack[a][f] = (AosBox){-14, -32, 28, 32};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {30, 20, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(159), NULL);

    /* A placed chest (parameter 0 set) sits closed on the floor: sprite
     * frame 0, its own hurtbox, no attack box. */
    AosEnemy m;
    assert(aos_enemy_create(&m, AOS_ENEMY_MIMIC, 100, 150, 1, 0, &soma, &layer, &kind, &stats));
    assert(m.state == 0 && m.step == 3 && m.static_frame == 0 && (m.y >> 16) == 159);
    assert(m.own_boxes && m.combat.attack_off);
    for (int i = 0; i < 10; ++i)
        aos_enemy_update(&m, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(m.step == 3 && m.static_frame == 0);
    /* The player in (x - 16, y - 20, 32, 20) opens it: animation 0 once,
     * then animation 1 with the frame boxes. */
    soma.x = AOS_FIXED(110);
    soma.y = AOS_FIXED(159);
    aos_enemy_update(&m, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(m.step == 4 && m.static_frame < 0 && !m.own_boxes && !m.combat.attack_off);
    int frames = 0;
    while (m.step == 4 && frames < 50) {
        aos_enemy_update(&m, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    assert(m.step == 5 && m.anim.id == 1 && frames == 11);

    /* A struck chest opens with animation 3 (2 once open) into the generic
     * death; its contact is a knockback. */
    soma.x = AOS_FIXED(400);
    AosEnemy c;
    assert(aos_enemy_create(&c, AOS_ENEMY_MIMIC, 100, 150, 1, 0, &soma, &layer, &kind, &stats));
    soma.facing_left = false;
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    soma.x = AOS_FIXED(70);
    soma.y = c.y + AOS_FIXED(30);
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{20, -40, 20, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    AosHitReport hit = aos_enemy_update(&c, &soma, &layer, &kind, &weapon, &blade, 50, 4, 0, 0,
                                        never_zero);
    assert(hit.enemy_hit && hit.killed && c.state == 1 && c.anim.id == 3 && c.timer == 0x28);

    /* A hidden Mimic (parameter 0 clear) waits in step 0 behind its
     * trigger, created on its first update. */
    soma.x = AOS_FIXED(400);
    soma.y = AOS_FIXED(159);
    AosEnemy h;
    assert(aos_enemy_create(&h, AOS_ENEMY_MIMIC, 100, 60, 0, 0, &soma, &layer, &kind, &stats));
    assert(h.step == 0 && h.hidden);
    hit = aos_enemy_update(&h, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(hit.child_count == 1 && hit.children[0].role == AOS_ROLE_MIMIC_TRIGGER);
    AosEnemy trigger = hit.children[0];
    assert(trigger.hidden && trigger.own_boxes && trigger.combat.attack_off);
    assert(trigger.x == h.x && trigger.y == h.y);
    hit = aos_enemy_update(&h, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(hit.child_count == 0 && h.step == 0);

    /* Striking the trigger reports the wake and bursts: flames every 4
     * frames above 30, deleted after 41 frames. */
    soma.x = AOS_FIXED(70);
    soma.y = trigger.y + AOS_FIXED(30);
    hit = aos_enemy_update(&trigger, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0,
                           never_zero);
    assert(hit.wake_spawner && trigger.state == 1 && trigger.timer == 0x28);
    int flames = 0;
    frames = 0;
    while (!trigger.removed && frames < 100) {
        hit = aos_enemy_update(&trigger, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                               never_zero);
        flames += hit.effect_count;
        ++frames;
    }
    assert(frames == 41 && flames == 3);

    /* Woken, it falls as a closed chest and lands in step 3. */
    h.step = 1;
    aos_enemy_update(&h, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(h.step == 2 && !h.hidden && h.vy == 0x2000 && h.static_frame == 0);
    frames = 0;
    while (h.step == 2 && frames < 200) {
        aos_enemy_update(&h, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    assert(h.step == 3 && (h.y >> 16) == 159 && h.vy == 0x10000 && h.own_boxes);
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
    const AosEnemyStats stats = {46, 14, 5, 0x0121, 0x0010, 0};

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
    while (!hit.child_count && frames < 300) {
        hit = aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(hit.child_count && z.step == 0xB && z.anim.frame == 10);
    AosEnemy g = hit.children[0];
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
    aos_audio_clear();
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
    /* sub_08092CCC: 0x11B at the bounce, 0x76 when it explodes. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x11B) == 1 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x76) == 1);
    /* sub_08092A28 step 1: the close attack's 0x85 at the start of frame 2. */
    AosEnemy biter = z;
    biter.state = 1;
    biter.step = 1;
    biter.anim = (AosAnimState){.id = 1, .frame = 2, .tick = 0};
    aos_audio_clear();
    aos_enemy_update(&biter, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x85) == 1);
    int blast = 0, kinds[AOS_EFFECT_KINDS] = {0};
    while (!flying.removed && blast < 100) {
        hit = aos_enemy_update(&flying, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++blast;
        if (flying.timer == 14) assert(flying.own_attack.x == -9 && flying.own_attack.w == 18);
        for (int i = 0; i < hit.effect_count; ++i) {
            const AosEffect *e = &hit.effects[i];
            assert(e->particle && e->x == flying.x && e->y == flying.y);
            ++kinds[e->kind];
            if (e->kind == AOS_EFFECT_SMOKE) {
                /* A random angle (1): (cos, sin) at 1.0, + 0x18 = 7. */
                assert(e->flags == 7 && e->vx == aos_sine(0x4001) && e->vy == aos_sine(1));
            }
            if (e->kind == AOS_EFFECT_DARK_SMOKE) assert(flying.timer == 3 && e->vx == 0);
            if (e->kind == AOS_EFFECT_FLAME) assert(e->flags == 3 && flying.timer & 1);
        }
    }
    assert(flying.hidden && blast == 0x18 - 2);
    /* Timers 3 to 8: smoke each frame, dark smoke at 3, flames at 3, 5, 7. */
    assert(kinds[AOS_EFFECT_SMOKE] == 6 && kinds[AOS_EFFECT_DARK_SMOKE] == 1 &&
           kinds[AOS_EFFECT_FLAME] == 3 && kinds[AOS_EFFECT_BLOOD] == 0);

    /* A grenade reaching Soma knocks him back (type 1) and bursts. */
    AosEnemy direct = g;
    soma.x = direct.x;
    soma.y = direct.y + AOS_FIXED(10);
    soma.combat = (AosCombat){.type = AOS_TYPE_PLAYER};
    hit = aos_enemy_update(&direct, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(hit.soma_hit && hit.soma_damage == aos_player_damage(14, 4));
    assert(soma.pending_type == 1 && direct.state == 1);
    int burst = 0;
    for (int i = 0; i < 7; ++i) {
        hit = aos_enemy_update(&direct, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        burst += hit.effect_count;
    }
    /* The burst's timers 1 to 7: 7 smoke, 1 dark smoke, 4 flames. */
    assert(direct.removed && burst == 12);

    /* A killing blow: death animation 3, state 2 and the generic death. */
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
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
    aos_audio_clear();
    hit = aos_enemy_update(&z, &soma, &layer, &kind, &weapon, &blade, 10, 4, cam, 0, never_zero);
    assert(hit.killed && z.state == 2 && z.anim.id == 3 && z.timer == 0x28);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x70) == 1);      /* sub_08092B38 */
    /* First death frame: two gibs from the draws dx, dy, vx, vy each, then
     * the generic death's flame (timer 0x28). */
    static const uint32_t gib_draws[] = {0x90000, 0x30000, 0x1234, 0x10000,
                                         0x7FFFF, 0x200000, 0xFFFFF, 0x2FFFF, 0, 0, 0};
    script = gib_draws;
    script_at = 0;
    int32_t zx = z.x, zy = z.y;
    hit = aos_enemy_update(&z, &soma, &layer, &kind, &weapon, &blade, 10, 4, cam, 0, scripted);
    assert(hit.child_count == 2 && hit.effect_count == 1 && script_at == 11);
    AosEnemy g0 = hit.children[0], g1 = hit.children[1];
    int32_t back = z.mirrored ? -1 : 1;
    assert(g0.role == AOS_ROLE_GIB && g0.static_frame == 0x13 && g1.static_frame == 0x14);
    assert(g0.x == zx + 0x10000 && g0.y == zy - 0x10000 - 0x30000);
    assert(g0.vx == back * (0x8000 + 0x1234));
    assert(g0.vy == -0x10000 - 0x18000 && g0.ay == 0x1800 && g0.mirrored == z.mirrored);
    assert(g1.x == zx - 1 && g1.y == zy - 0x80000 && g1.vy == -0x17FFF - 0x18000);
    assert(g1.vx == back * (0x8000 + 0x7FFF));
    /* sub_08093078: it rises, falls back and goes once below the soldier. */
    int gib_frames = 0;
    while (!g0.removed && gib_frames < 200) {
        aos_enemy_update(&g0, &soma, &layer, &kind, NULL, NULL, 10, 4, cam, 0, never_zero);
        if (!g0.removed) assert(g0.y <= zy);
        ++gib_frames;
    }
    assert(g0.removed && gib_frames > 10 && g0.y > zy);
    int dying = 1;
    while (!z.removed && dying < 100) {
        aos_enemy_update(&z, &soma, &layer, &kind, &weapon, &blade, 10, 4, cam, 0, never_zero);
        ++dying;
    }
    assert(z.removed && dying == 0x28);

    /* Under gEwramData + 0x8E bit 0x40: state 3, no floor snap, only the
     * animation step. */
    aos_enemy_set_flag_8e_40(true);
    AosEnemy idle;
    assert(aos_enemy_create(&idle, AOS_ENEMY_ZOMBIE_SOLDIER, 60, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(idle.state == 3 && idle.y == AOS_FIXED(100));
    for (int i = 0; i < 20; ++i) {
        hit = aos_enemy_update(&idle, &soma, &layer, &kind, NULL, NULL, 10, 4, cam, 0, never_zero);
        assert(!hit.soma_hit && idle.state == 3 && idle.x == AOS_FIXED(60));
    }
    assert(idle.anim.frame == 3);
}

static void rock_armor_tests(void) {
    aos_audio_clear();
    /* Floor at cell row 20 (top 160) from cell 4 to 49, walls at cells 3
     * and 50. */
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 4; x < 50; ++x) cells[20 * W + x] = 0x03;
    for (int y = 0; y < 21; ++y) cells[y * W + 3] = cells[y * W + 50] = 0x03;
    /* The exported frame durations of animations 0 (walk), 1 (throw) and
     * 2 (death); 3 the rock, 4-6 its pieces. */
    static const uint8_t walk[10] = {25, 5, 5, 5, 4, 22, 4, 5, 4, 4};
    static const uint8_t throw_ticks[33] = {2, 4, 3, 7, 9, 3, 2, 2, 2, 1, 1, 1, 2, 8, 31, 5, 6,
                                            3, 3, 3, 8, 8, 8, 8, 8, 8, 8, 8, 6, 9, 9, 7, 8};
    static const uint8_t death[11] = {8, 6, 6, 6, 8, 6, 6, 4, 4, 4, 11};
    static const uint8_t one[1] = {1};
    const AosAnimDef defs[7] = {{10, walk}, {33, throw_ticks}, {11, death}, {1, one},
                                {1, one},   {1, one},          {1, one}};
    const AosAnimSet anims = {defs, 7};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 3; ++a)
        for (int f = 0; f < 33; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-13, -48, 24, 48};
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    const AosEnemyStats stats = {60, 16, 8, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(600), AOS_FIXED(400), NULL);
    soma.max_hp = soma.hp = 320;

    /* Snapped onto the floor plus one pixel; frame 0 walks 0.125 a frame,
     * frame 1 0.375 (frame % 5 * 0.25 + 0.125, none at frame % 5 = 4). */
    AosEnemy a;
    assert(aos_enemy_create(&a, AOS_ENEMY_ROCK_ARMOR, 200, 150, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert((a.y >> 16) == 160 && a.state == 0);
    a.mirrored = false;
    int32_t x0 = a.x;
    aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(a.x == x0 - 0x2000);
    /* With a patrol of 1 it turns on the third update of frame 9. */
    AosEnemy patrol;
    assert(aos_enemy_create(&patrol, AOS_ENEMY_ROCK_ARMOR, 200, 150, 1, 0, &soma, &layer, &kind,
                            &stats));
    bool facing = patrol.mirrored;
    int frame9 = 0;
    for (int i = 0; i < 200 && patrol.mirrored == facing; ++i) {
        if (patrol.anim.frame == 9) ++frame9;
        aos_enemy_update(&patrol, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    }
    assert(patrol.mirrored != facing && frame9 == 3);

    /* The player 60 pixels ahead at the start of frame 9: the throw; its
     * frame 7 lasts two updates, each throwing a rock (sound 0x88). */
    assert(aos_enemy_create(&a, AOS_ENEMY_ROCK_ARMOR, 200, 150, 0, 0, &soma, &layer, &kind,
                            &stats));
    a.mirrored = true;
    soma.x = a.x + AOS_FIXED(60);
    soma.y = a.y;
    int frames = 0;
    while (a.state == 0 && frames < 200) {
        aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    assert(a.state == 1 && a.anim.frame == 9);
    aos_audio_clear();
    AosEnemy rock = {0};
    int rocks = 0;
    while (a.state == 1 && frames < 600) {
        AosHitReport r = aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          never_zero);
        for (int c = 0; c < r.child_count; ++c)
            if (r.children[c].role == AOS_ROLE_ROCK) {
                rock = r.children[c];
                ++rocks;
            }
        ++frames;
        if (a.step == 0 && a.anim.id == 1 && rocks) break;
    }
    assert(rocks == 2 && aos_audio_requested(AOS_AUDIO_START_SONG, 0x88) == 2 &&
           rock.mirrored && rock.combat.type == 0x0A);

    /* The rock: 58 pixels up, up at 2.0, right at 0.125 + distance / 64, power
     * + 1 / 4; it lands, then bursts into eight pieces (sound 0x80). */
    int32_t ry = rock.y, distance = (soma.x - rock.x) >> 16;
    aos_enemy_update(&rock, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(rock.state == 1 && rock.speed == 0x2000 + distance * 0x400 &&
           rock.stats.contact == 20 && rock.y == ry - 0x3A0000 - 0x20000 + 0x2000 && rock.own_hurt.w == 32);
    frames = 0;
    while (rock.state == 1 && frames < 200) {
        aos_enemy_update(&rock, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(rock.state == 2 && (rock.y >> 16) == 151);   /* its floor probe on row 159 */
    aos_audio_clear();
    AosHitReport burst = aos_enemy_update(&rock, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_zero);
    assert(rock.removed && burst.child_count == 8 &&
           burst.children[0].role == AOS_ROLE_ROCK_PIECE &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x80) == 1);
    /* A piece (zero random: animation 4, vx -0.75, up at 1.0) falls with
     * gravity 0.25 and bounces once off the floor. */
    AosEnemy piece = burst.children[0];
    assert(piece.x == rock.x - 0x80000 && piece.y == rock.y - 0x100000 && !piece.mirrored);
    aos_enemy_update(&piece, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(piece.anim.id == 4 && piece.vx == -0xC000 && piece.vy == -0x10000 &&
           piece.state == 1);
    frames = 0;
    while (piece.state == 1 && frames < 100) {
        aos_enemy_update(&piece, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(piece.state == 2 && piece.vy < 0);

    /* A rock struck by the weapon loses its hit points and bursts. */
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 40, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    AosEnemy struck = rock;
    struck.removed = false;
    struck.state = 1;
    struck.hp = 60;
    struck.x = soma.x;
    struck.y = soma.y - AOS_FIXED(16);
    AosHitReport hit = aos_enemy_update(&struck, &soma, &layer, &kind, &weapon, &blade, 10, 4,
                                        0, 0, always_zero);
    assert(hit.enemy_hit && struck.hp == 0 && struck.state == 2);

    /* A lethal blow: it faces the player, death animation 2 (0x6F), a
     * burst every fourth frame, a spark and 0x72 every eighth, gone when the
     * animation ends. */
    assert(aos_enemy_create(&a, AOS_ENEMY_ROCK_ARMOR, 200, 150, 0, 0, &soma, &layer, &kind,
                            &stats));
    a.mirrored = false;
    soma.x = a.x + AOS_FIXED(4);
    soma.y = a.y;
    aos_audio_clear();
    hit = aos_enemy_update(&a, &soma, &layer, &kind, &weapon, &blade, 999, 4, 0, 0,
                           always_zero);
    assert(hit.killed && a.state == 2 && a.mirrored);
    frames = 0;
    int bursts = 0, sparks = 0;
    while (!a.removed && frames < 200) {
        AosHitReport r = aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_zero);
        for (int e = 0; e < r.effect_count; ++e) {
            bursts += r.effects[e].kind == AOS_EFFECT_BURST_15;
            sparks += r.effects[e].kind == AOS_EFFECT_SPARK;
        }
        ++frames;
    }
    assert(a.removed && frames == 70 && bursts == 17 && sparks == 8 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x6F) == 1 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x72) == 8);
}

static void killer_fish_tests(void) {
    aos_audio_clear();
    /* Water (cells 8) on rows 10-19 (y 80..159) over a floor at row 20. */
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int y = 10; y < 20; ++y)
        for (int x = 0; x < W; ++x) cells[y * W + x] = 0x08;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t fry_ticks[8] = {8, 8, 8, 8, 8, 8, 8, 8};
    static const uint8_t swim_ticks[6] = {12, 12, 12, 12, 12, 12};
    static const uint8_t spit_ticks[2] = {8, 30};
    const AosAnimDef defs[3] = {{8, fry_ticks}, {6, swim_ticks}, {2, spit_ticks}};
    const AosAnimSet anims = {defs, 3};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 3; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-12, -8, 24, 16};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 0x100;
    const AosEnemyStats stats = {80, 24, 4, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(40), NULL);

    /* Out of water: upside down on the floor, 8 pixels up, inert. */
    AosEnemy f;
    assert(aos_enemy_create(&f, AOS_ENEMY_KILLER_FISH, 100, 40, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(f.step == 0x63 && f.vflip && (f.y >> 16) < 160 && f.state == 0 && f.anim.id == 1);
    AosHitReport none = aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                         always_zero);
    assert(none.child_count == 0 && f.step == 0x63);

    /* In water with + 0x32 = 3: three fry; it swims at 0.25 (facing left). */
    assert(aos_enemy_create(&f, AOS_ENEMY_KILLER_FISH, 100, 120, 0, 3, &soma, &layer, &kind,
                            &stats));
    f.mirrored = false;
    assert(f.step == 0);
    AosHitReport first = aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_zero);
    assert(first.child_count == 3 && first.children[0].role == AOS_ROLE_FRY &&
           first.children[0].stats.contact == 6 && f.step == 1 && f.vx == -0x4000 &&
           f.ay == 0x400);
    int32_t x0 = f.x;
    /* The bubble countdown starts at 0: a bubble 16 pixels ahead (frame 16),
     * then 120 + r % 120 frames. */
    AosHitReport bubbles = aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                            always_zero);
    assert(f.x == x0 - 0x4000 && bubbles.child_count == 1 &&
           bubbles.children[0].role == AOS_ROLE_BUBBLE &&
           bubbles.children[0].static_frame == 16 && bubbles.children[0].x == f.x - 0x100000 &&
           f.fish_bubble == 0x78);
    /* Its bob: ay flips at +/- 0.25. */
    int frames = 0;
    while (f.ay > 0 && frames < 100) {
        aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(f.ay == -0x400 && f.vy > 0x4000 && f.state == 0);

    /* Soma in water within 80 pixels: the attack. It faces him, picks a
     * point 16 above him (within 4), spits after 40 frames four bubbles (the
     * first frame 15) and darts at the point at 2.0. */
    soma.x = f.x + AOS_FIXED(60);
    soma.y = f.y;
    aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(f.state == 1 && f.step == 0);
    aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(f.mirrored && f.vx == 0 && f.fish_tx == (soma.x >> 16) - 4 &&
           f.fish_ty == (soma.y >> 16) - 16 && f.step == 1);
    frames = 0;
    while (f.step == 1 && frames < 100) {
        aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(frames == 41 && f.step == 2 && f.anim.id == 2);
    AosHitReport spit = {0};
    while (f.step == 2 && frames < 200)
        spit = aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                always_zero);
    assert(f.step == 3 && spit.child_count == 4 && spit.children[0].static_frame == 15 &&
           spit.children[1].static_frame == 16 && spit.children[0].vx == 0x30000 &&
           spit.children[0].vy == -0x6000 && spit.children[1].vy == -0x4000);
    aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(f.vx > 0x10000 && f.vx <= 0x20000);
    frames = 0;
    while (f.state == 1 && frames < 200) {
        aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(f.state == 0 && f.anim.id == 1);

    /* A bubble rises at its vy until its head leaves the water. */
    AosEnemy b = bubbles.children[0];
    frames = 0;
    while (!b.removed && frames < 400) {
        aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(b.removed && (b.y >> 16) - 2 < 80 && (b.y >> 16) - 2 >= 78);
    /* A bursting bubble shows the lower frame from its sixth update and goes
     * after twelve. */
    AosEnemy pop = {0};
    pop.id = AOS_ENEMY_KILLER_FISH;
    pop.role = AOS_ROLE_BUBBLE_POP;
    pop.static_frame = 15;
    pop.x = pop.y = AOS_FIXED(120);
    for (frames = 0; !pop.removed && frames < 20; ++frames)
        aos_enemy_update(&pop, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(frames == 12 && pop.static_frame == 14 && pop.y == AOS_FIXED(120) - 12 * 0x4000);

    /* A fry follows its fish (a point within 8 pixels of it, 2.0 slowed by
     * 1/20); Soma in water in its 160 x 64 box starts the frenzy. */
    AosEnemy fry = first.children[0];
    fry.parent = &f;
    soma.x = AOS_FIXED(600);
    aos_enemy_update(&fry, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(fry.state == 1 && fry.fish_count == 0x14 && fry.anim.id == 0);
    soma.x = fry.x + AOS_FIXED(40);
    soma.y = fry.y;
    aos_enemy_update(&fry, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(fry.state == 3);
    aos_enemy_update(&fry, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(fry.step == 1 && fry.timer == 1 && fry.vx > 0 && fry.mirrored);
    /* Five more dashes, then back to its fish (state 4). */
    frames = 0;
    while (fry.state == 3 && frames < 400) {
        aos_enemy_update(&fry, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(fry.state == 4);
    /* Struck: no damage, a bursting bubble, 40 frames of blinking. */
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    fry.x = soma.x;
    fry.y = soma.y - AOS_FIXED(16);
    AosHitReport hit = aos_enemy_update(&fry, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0,
                                        0, always_zero);
    assert(hit.enemy_hit && !hit.killed && fry.state == 2 && hit.child_count == 1 &&
           hit.children[0].role == AOS_ROLE_BUBBLE_POP);
    for (frames = 0; !fry.removed && frames < 60; ++frames)
        aos_enemy_update(&fry, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(frames == 0x28);

    /* A lethal blow: the fish floats up dead (sound 0x6C at the surface),
     * bobs there and is gone after 70 more frames. */
    f.state = 0;
    f.step = 1;
    f.hit_flash = 0;
    f.x = soma.x;
    f.y = soma.y - AOS_FIXED(8);
    aos_audio_clear();
    hit = aos_enemy_update(&f, &soma, &layer, &kind, &weapon, &blade, 999, 4, 0, 0,
                           always_zero);
    assert(hit.killed && f.state == 2 && f.timer == 0x28);
    frames = 0;
    while (!f.removed && frames < 400) {
        aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(f.removed && f.vflip && aos_audio_requested(AOS_AUDIO_START_SONG, 0x6C) == 1);
}

static uint32_t always_one(void) { return 1; }

static void armor_tests(void) {
    aos_audio_clear();
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
    const AosEnemyStats stats = {60, 15, 8, 0x0108, 0, 0};

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
    while (!hit.child_count && frames < 400) {
        hit = aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_one);
        ++frames;
    }
    /* Thrown at frame 12, tick 2 (the last of these 3-tick test frames):
     * the animation step that follows reaches frame 13. */
    assert(a.anim.id == 1 && a.substep == 0 && a.anim.frame == 13 && a.anim.tick == 0);
    AosEnemy axe = hit.children[0];
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
    AosEnemy second = hit.children[0];
    aos_enemy_update(&second, &away, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    soma.x = second.x;
    soma.y = second.y + AOS_FIXED(20);
    soma.combat = (AosCombat){.type = AOS_TYPE_PLAYER};
    aos_combat_take(&soma.combat, AOS_TYPE_ENEMY, 81);
    hit = aos_enemy_update(&second, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(hit.soma_hit && hit.soma_damage == aos_player_damage(15, 4) && soma.pending_type == 0);

    /* A weapon destroys it. */
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    soma.facing_left = false;
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-10, -40, 20, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    AosEnemy struck = hit.children[0];
    struck = second;
    struck.combat.recent[0] = struck.combat.recent[1] = struck.combat.recent[2] = 0;
    soma.x = struck.x;
    soma.y = struck.y + AOS_FIXED(30);
    hit = aos_enemy_update(&struck, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, never_zero);
    assert(hit.enemy_hit && struck.removed);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x86));
    /* sub_0806D5C0: a flame within 8 x 8 pixels of the struck axe. */
    assert(hit.effect_count == 1 && hit.effects[0].kind == AOS_EFFECT_FLAME);
    assert(hit.effects[0].x == struck.x + 1 - 0x40000 && hit.effects[0].y == struck.y + 1 - 0x40000);

    /* A killing blow: the armor turns to the attacker, plays its death
     * animation and is deleted when it ends. */
    soma.x = a.x - AOS_FIXED(5);
    soma.y = a.y + AOS_FIXED(20);
    soma.combat = (AosCombat){.type = AOS_TYPE_PLAYER};
    aos_audio_clear();
    a.hp = 1;
    a.mirrored = true;
    hit = aos_enemy_update(&a, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, never_zero);
    assert(hit.killed && a.state == 2 && a.defeated && !a.mirrored);
    /* sub_080B11DC: fire on even frames up to 32 (16 x sin(phase +
     * 0x4800) beside it, the height climbing from 40 pixels above), sparks
     * every 8 frames. random() = 1: +/- offsets of 1 - n / 2. */
    int dying = 0, fires = 0, sparks = 0;
    int32_t ax0 = a.x, ay0 = a.y;
    while (!a.removed && dying < 200) {
        hit = aos_enemy_update(&a, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, never_zero);
        ++dying;
        for (int i = 0; i < hit.effect_count; ++i) {
            const AosEffect *e = &hit.effects[i];
            assert(e->drift && dying >= 2 && dying - 1 <= 0x20);
            int t = dying - 1;      /* + 0x0D after the increment */
            int32_t height = ay0 - 0x280000 + (t - 1) * 0x12000;
            if (e->kind == AOS_EFFECT_FIRE) {
                int32_t phase = (t - 1) * 0x5000;
                assert(t % 2 == 0 && e->vy == 0x8000 && e->ay == -0x1000 && e->y == height);
                assert(e->x == ax0 + (aos_sine((uint32_t)(phase + 0x4800)) << 4) + 1 - 0x20000);
                ++fires;
            } else {
                assert(e->kind == AOS_EFFECT_SPARK && t % 8 == 0);
                assert(e->x == ax0 - 7 * 0x10000 && e->y == height - 0x10000);
                ++sparks;
            }
        }
    }
    assert(a.removed && a.anim.id == 4 && dying >= 11 * 5);
    assert(fires == 16 && sparks == 4);
    /* Soma's hurt sound 0xB3 (sub_08021654) aside: 0x6F, then four 0x6E. */
    int logged = 0;
    for (int i = 0; i < aos_audio_count(); ++i) {
        AosAudioRequest request = aos_audio_get(i);
        if (request.action == AOS_AUDIO_START_SONG && request.value == 0xB3) continue;
        if (request.action == AOS_AUDIO_START_SONG && request.value == 0x9E) continue;
        assert(request.action == AOS_AUDIO_START_SONG &&
               request.value == (logged == 0 ? 0x6F : 0x6E));
        ++logged;
    }
    assert(logged == 5);

    /* Under gEwramData + 0x8E bit 0x40: state 3, no floor snap, animation
     * only. */
    aos_enemy_set_flag_8e_40(true);
    AosEnemy idle;
    assert(aos_enemy_create(&idle, AOS_ENEMY_AXE_ARMOR, 60, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(idle.state == 3 && idle.y == AOS_FIXED(100));
    soma.x = idle.x;
    soma.y = idle.y;
    for (int i = 0; i < 10; ++i) {
        hit = aos_enemy_update(&idle, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        assert(!hit.soma_hit && idle.state == 3 && idle.x == AOS_FIXED(60));
    }

    /* sub_08045C9C: position then velocity, the animation step; deleted
     * when it ends. */
    static const uint8_t fire_steps[] = {3, 5};
    const AosAnimDef fire_def = {2, fire_steps};
    const AosAnimSet fire_set = {&fire_def, 1};
    AosEffect fire = {.kind = AOS_EFFECT_FIRE, .drift = true, .y = AOS_FIXED(40), .vy = 0x8000,
                      .ay = -0x1000};
    int life = 1;
    assert(aos_effect_update(&fire, &fire_set, NULL, NULL) && fire.y == AOS_FIXED(40) + 0x8000);
    assert(fire.vy == 0x7000);
    while (aos_effect_update(&fire, &fire_set, NULL, NULL) && life < 50) ++life;
    assert(life == 7);
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
    const AosEnemyStats stats = {42, 13, 5, 0x0020, 0x0010, 0};

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
        aos_audio_clear();
        while (a.state == 1 && frames < 1000) {
            AosHitReport hit = aos_enemy_update(&a, &far_soma, &layer, &kind, NULL, NULL, 10, 4,
                                                100, 0, never_zero);
            if (hit.child_count && arrows++ == 0) first = hit.children[0];
            ++frames;
        }
        assert(arrows == (volley == 4 ? 3 : 1));
        /* sub_080AF934: sound 0x85 with each arrow. */
        assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x85) == arrows);
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
    while (!hit.child_count && frames < 1000) {
        hit = aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 0, never_zero);
        ++frames;
    }
    AosEnemy arrow = hit.children[0];
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
    /* A struck arrow (no hit point) vanishes with a flame of sub_0806D5C0
     * within 4 x 4 pixels of (x + 8 * vx, y - 22). */
    AosEnemy struck_arrow = arrow;
    struck_arrow.removed = false;
    struck_arrow.state = 0;
    struck_arrow.hp = 0;
    struck_arrow.vx = 0x30000;
    hit = aos_enemy_update(&struck_arrow, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 0, never_zero);
    assert(struck_arrow.removed && hit.effect_count == 1 && hit.effects[0].kind == AOS_EFFECT_FLAME);
    assert(hit.effects[0].x == struck_arrow.x + 8 * 0x30000 + 1 - 0x20000);
    assert(hit.effects[0].y == struck_arrow.y - 0x160000 + 1 - 0x20000);

    /* Under gEwramData + 0x8E bit 0x40: state 4, animation only. */
    aos_enemy_set_flag_8e_40(true);
    AosEnemy inert;
    assert(aos_enemy_create(&inert, AOS_ENEMY_SKULL_ARCHER, 200, 150, 1, 0, &soma, &layer, &kind,
                            &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(inert.state == 4 && inert.y == AOS_FIXED(150));
    for (int i = 0; i < 10; ++i) {
        hit = aos_enemy_update(&inert, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 0, never_zero);
        assert(!hit.soma_hit && !hit.child_count && inert.state == 4 && inert.x == AOS_FIXED(200));
    }

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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
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
    assert(hit.killed && a.state == 3 && !a.removed && a.away == 1);
    aos_audio_clear();
    hit = aos_enemy_update(&a, &soma, &layer, &kind, &weapon, &blade, 10, 4, 100, 0, never_zero);
    assert(a.removed && hit.child_count == 7);
    /* sub_080AFB9C plays 0x6B as it shatters. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x6B));
    for (int i = 0; i < 7; ++i) {
        const AosEnemy *piece = &hit.children[i];
        assert(piece->role == AOS_ROLE_DEBRIS && piece->anim.id == 5 + i);
        /* random() = 1: vx = 0x8000 + (1 << 13), vy = -0x38000 + (1 << 12). */
        assert(piece->vx == 0xA000 && piece->vy == (int32_t)0xFFFC9000 && piece->ay == 0x2000);
        assert(piece->combat.attack_off && piece->combat.hurt_off);
    }
    /* A piece rises, falls back and vanishes on the floor (its probe 4
     * pixels below its anchor), never sliding far: vx keeps 79/80. */
    kind.anchors[5][0][0] = -4;
    kind.anchors[5][0][1] = -6;
    AosEnemy bone = hit.children[0];
    int32_t rise = bone.y, top = bone.y;
    int falling = 0;
    aos_audio_clear();
    while (!bone.removed && falling < 200) {
        hit = aos_enemy_update(&bone, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 0,
                               never_zero);
        if (bone.y < top) top = bone.y;
        ++falling;
    }
    assert(bone.removed && top < rise && (bone.y >> 16) - 6 + 4 >= 160);
    /* sub_0806C5AC plays 0x162 at the impact, once. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x162) == 1);
    /* The impact leaves a dust puff at the anchor (sub_08045CEC 4, row 6):
     * random() = 1 gives vy = 1 - 0x800 - 0x4000, vx = 1 - 0x1000 - vx. */
    assert(hit.effect_count == 1 && hit.effects[0].kind == AOS_EFFECT_DUST);
    const AosEffect *dust = &hit.effects[0];
    assert(dust->x == bone.x + (bone.mirrored ? AOS_FIXED(4) : -AOS_FIXED(4)));
    assert(dust->y == bone.y - AOS_FIXED(6));
    assert(dust->vy == 1 - 0x800 - 0x4000 && dust->ay == dust->vy / 32);
    assert(dust->vx == 1 - 0x1000 - bone.vx && dust->ax == -dust->vx / 64);
    assert(bone.x - a.x < AOS_FIXED(40));
}

static void curly_tests(void) {
    /* Floor top 160 from x 0 to 199, a drop beyond. */
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < 25; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t walk[] = {4, 4, 4, 4, 4, 4, 4, 4}, bite[] = {3, 3, 3, 3, 3, 3},
                         death[] = {200};
    const AosAnimDef defs[3] = {{8, walk}, {6, bite}, {1, death}};
    const AosAnimSet anims = {defs, 3};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int a = 0; a < 3; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -16, 16, 16};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {40, 30, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(40), AOS_FIXED(159), NULL);

    /* It snaps to the floor and paces at 0.5 (frames 0-3), still on 4-7;
     * facing right it turns at the drop past x 199. */
    AosEnemy c;
    assert(aos_enemy_create(&c, AOS_ENEMY_CURLY, 150, 140, 0, 0, &soma, &layer, &kind, &stats));
    assert(c.state == 0 && (c.y >> 16) == 159);
    c.mirrored = true;
    soma.x = AOS_FIXED(1000);
    bool turned = false;
    int32_t far = c.x;
    aos_audio_clear();
    for (int i = 0; i < 300 && !turned; ++i) {
        aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        if (c.x > far) far = c.x;
        if (!c.mirrored) turned = true;
    }
    assert(turned && (far >> 16) <= 200 && (c.y >> 16) == 159);
    /* sub_08093218: a step sound 0x85 at the start of walk frame 4. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x85) >= 1);
    /* Unhurt it ignores the player; hurt (hp below its record), step 1
     * faces him at each frame start. */
    assert(c.step == 0);
    c.hp = 10;
    aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(c.step == 1);

    /* With the player within (x - 50, y - 64, 100, 70) on walk frame 1:
     * the attack (animation 1), a lunge at 1.0 for 8 frames. */
    AosEnemy b;
    assert(aos_enemy_create(&b, AOS_ENEMY_CURLY, 100, 140, 0, 0, &soma, &layer, &kind, &stats));
    soma.x = b.x + AOS_FIXED(20);
    b.mirrored = true;
    int frames = 0;
    while (b.state == 0 && frames < 60) {
        aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(b.state == 1 && b.anim.id == 1);
    aos_audio_clear();
    int32_t x0 = b.x;
    aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(b.vx == 0x10000 && b.x == x0 + 0x10000);
    frames = 0;
    while (b.state == 1 && frames < 60) {
        aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(b.state == 0 && b.anim.id == 0 && frames == 17);
    /* sub_08093460: 0x85 on attack frame 4 (this six-frame bite has no 7,
     * 10 or 13). */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x85) == 1);

    /* EnemyDurgaCreate / EnemyDurgaUpdate call Curly's routines. */
    AosEnemy d;
    assert(aos_enemy_create(&d, AOS_ENEMY_DURGA, 100, 140, 0, 0, &soma, &layer, &kind, &stats));
    assert(d.state == 0 && (d.y >> 16) == 159 && d.vy == 0x10000 && d.ay == 0x2800);
    for (int i = 0; i < 20 && d.vx == 0; ++i)
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    /* At the next walk frame start: 0.5 toward its facing. */
    assert(d.vx == (d.mirrored ? 0x8000 : -0x8000));
    /* A killing hit: sub_080935E8's sound 0x1C3. */
    AosSoulAttack curly_blast = {{0, 0, 300, 300}, 999, 0, false, NULL, AOS_TYPE_SOUL};
    aos_enemy_set_soul_attacks(&curly_blast, 1);
    aos_audio_clear();
    for (int frame = 0; frame < 4 && !d.defeated; ++frame)
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    aos_enemy_set_soul_attacks(NULL, 0);
    assert(d.defeated && aos_audio_requested(AOS_AUDIO_START_SONG, 0x1C3) == 1);
}

static void valkyrie_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    static const uint8_t hover[] = {4, 4}, strike[] = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
                         death[] = {200};
    const AosAnimDef defs[3] = {{2, hover}, {10, strike}, {1, death}};
    const AosAnimSet anims = {defs, 3};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int a = 0; a < 3; ++a)
        for (int f = 0; f < 10; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -16, 16, 16};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {60, 40, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(159), NULL);

    /* It waits beyond 120 pixels, then heads at 2.0 for the point 64
     * pixels in front of the player and 48 above. */
    AosEnemy v;
    assert(aos_enemy_create(&v, AOS_ENEMY_VALKYRIE, 100, 60, 0, 0, &soma, &layer, &kind, &stats));
    aos_enemy_update(&v, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(v.state == 0 && v.step == 0);
    soma.x = AOS_FIXED(150);       /* sqrt(50^2 + 99^2) = 110 pixels */
    aos_enemy_update(&v, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(v.step == 1);
    int32_t x0 = v.x;
    aos_enemy_update(&v, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    /* Facing right, its target is 64 pixels left of the player (x 86). */
    assert(v.x < x0 && v.vx < 0 && v.vx >= -0x20000 && v.vy > 0);
    int frames = 0;
    while (v.state == 0 && frames < 200) {
        aos_enemy_update(&v, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(v.state == 1);
    /* The attack: frame 2 held 16 frames, then the 3.0 charge. */
    frames = 0;
    aos_audio_clear();
    while (v.step < 3 && frames < 100) {
        aos_enemy_update(&v, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(v.step == 3 && v.vy > 0 && (v.vx == 0x30000 || v.vx == -0x30000 || v.vx > 0x28000));
    /* sub_0806E8AC: the charge's sound 0x87. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x87) == 1);
    frames = 0;
    while (v.state == 1 && frames < 300) {
        aos_enemy_update(&v, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(v.state == 0 && v.step == 1);

    /* Contact: halved outside the strike frames 4 to 6. */
    soma = aos_soma_spawn(v.x, v.y + AOS_FIXED(8), NULL);
    AosHitReport hit = aos_enemy_update(&v, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                        always_zero);
    if (hit.soma_hit) assert(hit.soma_damage == aos_player_damage(20, 4));

    /* Struck during its attack, it recoils (state 3) up and away. */
    soma = aos_soma_spawn(v.x, v.y + AOS_FIXED(20), NULL);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-20, -60, 40, 60, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    v.state = 1;
    v.step = 4;
    v.hp = 60;
    v.combat = (AosCombat){.type = AOS_TYPE_ENEMY};
    int cam = (v.x >> 16) - 120, cam_y = (v.y >> 16) - 80;
    hit = aos_enemy_update(&v, &soma, &layer, &kind, &weapon, &blade, 10, 4, cam, cam_y,
                           always_zero);
    assert(hit.enemy_hit && !hit.killed && v.state == 3 && v.step == 0);
    v.hit_flash = 0;
    aos_enemy_update(&v, &soma, &layer, &kind, NULL, NULL, 10, 4, cam, cam_y, always_zero);
    assert(v.step == 1 && v.vy == -0x30000);
    frames = 0;
    while (v.state == 3 && frames < 100) {
        aos_enemy_update(&v, &soma, &layer, &kind, NULL, NULL, 10, 4, cam, cam_y, always_zero);
        ++frames;
    }
    assert(v.state == 0 && v.step == 0);

    /* The killing blow: animation 2, the falling generic death. */
    v.hp = 1;
    v.combat = (AosCombat){.type = AOS_TYPE_ENEMY};
    soma = aos_soma_spawn(v.x, v.y + AOS_FIXED(20), NULL);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    cam = (v.x >> 16) - 120;
    cam_y = (v.y >> 16) - 80;
    aos_audio_clear();
    hit = aos_enemy_update(&v, &soma, &layer, &kind, &weapon, &blade, 10, 4, cam, cam_y,
                           always_zero);
    assert(hit.killed && v.state == 2 && v.anim.id == 2 && v.ay == 0x1000);
    /* sub_0806EBDC's death sound. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x1CD) == 1);
    int dying = 0;
    while (!v.removed && dying < 100) {
        aos_enemy_update(&v, &soma, &layer, &kind, NULL, NULL, 10, 4, cam, cam_y, always_zero);
        ++dying;
    }
    assert(dying == 0x28);

    /* Erinys runs the Valkyrie's routines, but sub_0806EBDC's recoil is
     * for id 0x29 only: struck mid-strike it keeps striking. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_ERINYS, 100, 60, 0, 0, &soma, &layer, &kind, &stats));
    assert(e.state == 0);
    e.state = 1;
    e.step = 4;
    e.hp = 60;
    soma = aos_soma_spawn(e.x, e.y + AOS_FIXED(20), NULL);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    cam = (e.x >> 16) - 120;
    cam_y = (e.y >> 16) - 80;
    hit = aos_enemy_update(&e, &soma, &layer, &kind, &weapon, &blade, 10, 4, cam, cam_y,
                           always_zero);
    assert(hit.enemy_hit && !hit.killed && e.state == 1);
}

/* Death sounds of sub_080AD5B8, sub_080C2388 and sub_0808A0C8. */
static void death_sound_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    static const uint8_t two[] = {4, 4}, four[] = {4, 4, 4, 4};
    const AosAnimDef defs[4] = {{2, two}, {2, two}, {4, four}, {4, four}};
    const AosAnimSet anims = {defs, 4};
    static AosEnemyKind kind;
    kind.anims = &anims;
    const AosEnemyStats stats = {20, 15, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(300), AOS_FIXED(100), NULL);
    AosEnemy bat;
    assert(aos_enemy_create(&bat, AOS_ENEMY_BAT, 100, 60, 0, 0, &soma, &layer, &kind, &stats));
    bat.state = 3;
    bat.step = 0;
    aos_audio_clear();
    aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x72) && bat.step == 1);
    /* Only on the death's first frame. */
    aos_audio_clear();
    aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(!aos_audio_requested(AOS_AUDIO_START_SONG, 0x72));
    AosEnemy flea;
    assert(aos_enemy_create(&flea, AOS_ENEMY_FLEAMAN, 100, 60, 0, 0, &soma, &layer, &kind,
                            &stats));
    flea.state = 0xFF;
    flea.step = 0;
    aos_audio_clear();
    aos_enemy_update(&flea, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x72) && flea.step == 2);
    AosEnemy ghost;
    kind.modes[0][0] = 1;
    kind.hurt[0][0] = (AosBox){-8, -8, 16, 16};
    kind.modes[1][0] = kind.modes[1][1] = 1;
    kind.hurt[1][0] = kind.hurt[1][1] = (AosBox){-8, -8, 16, 16};
    assert(aos_enemy_create(&ghost, AOS_ENEMY_GHOST, 100, 60, 0, 0, &soma, &layer, &kind,
                            &stats));
    for (int frame = 0; frame < 24 && ghost.step == 0; ++frame)
        aos_enemy_update(&ghost, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                         always_zero);
    assert(ghost.step == 1);
    AosSoulAttack soul = {{ghost.x >> 16, ghost.y >> 16, ghost.x >> 16, ghost.y >> 16},
                          100, 0, false, NULL, AOS_TYPE_SOUL};
    aos_enemy_set_soul_attacks(&soul, 1);
    aos_audio_clear();
    aos_enemy_update(&ghost, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    aos_enemy_set_soul_attacks(NULL, 0);
    assert(ghost.defeated && aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E) == 1);
    int ghost_flames = 0;
    for (int frame = 0; frame < 8; ++frame) {
        AosHitReport death = aos_enemy_update(&ghost, &soma, &layer, &kind, NULL, NULL, 10, 4,
                                               0, 0, always_zero);
        ghost_flames += death.effect_count;
    }
    assert(ghost.removed && ghost_flames == 2);
    /* The death callbacks' PlaySong: Medusa Head sub_08093AE0, Mimic
     * sub_0809922C, Skeleton Knight sub_0808F0A4 and Dead Crusader
     * sub_0808F848 0x72, Ghost Dancer sub_080C32EC 0x6D, Gargoyle and
     * Gremlin sub_08086878 0x6E, Gorgon and Catoblepas sub_080896D8 0x71. */
    static const struct { uint8_t id; uint16_t song; } deaths[] = {
        {AOS_ENEMY_MEDUSA_HEAD, 0x72}, {AOS_ENEMY_MIMIC, 0x72},
        {AOS_ENEMY_SKELETON_KNIGHT, 0x72}, {AOS_ENEMY_DEAD_CRUSADER, 0x72},
        {AOS_ENEMY_GHOST_DANCER, 0x6D}, {AOS_ENEMY_GARGOYLE, 0x6E},
        {AOS_ENEMY_GREMLIN, 0x6E}, {AOS_ENEMY_GORGON, 0x71}, {AOS_ENEMY_CATOBLEPAS, 0x71},
    };
    for (int k = 0; k < 4; ++k)
        for (int f = 0; f < 4; ++f) {
            kind.modes[k][f] = 1;
            kind.hurt[k][f] = (AosBox){-16, -32, 32, 48};
        }
    for (size_t k = 0; k < sizeof deaths / sizeof deaths[0]; ++k) {
        AosEnemy e;
        /* A placed Mimic (parameter 0 set) is a closed chest that can be hit. */
        bool chest = deaths[k].id == AOS_ENEMY_MIMIC;
        for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
        assert(aos_enemy_create(&e, deaths[k].id, 100, 150, chest ? 1 : 0, 0, &soma, &layer,
                                &kind, &stats));
        AosSoulAttack lethal = {{0, 0, 2000, 2000}, 999, 0, false, NULL, AOS_TYPE_SOUL};
        AosHitReport report = {0};
        aos_audio_clear();
        for (int frame = 0; frame < 4 && !e.defeated; ++frame) {
            aos_enemy_set_soul_attacks(&lethal, 1);
            report = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                      always_zero);
            aos_enemy_set_soul_attacks(NULL, 0);
        }
        (void)report;
        if (!e.defeated || aos_audio_requested(AOS_AUDIO_START_SONG, deaths[k].song) != 1)
            fprintf(stderr, "death sound of enemy 0x%02X: defeated %d, song %d\n",
                    deaths[k].id, e.defeated,
                    aos_audio_requested(AOS_AUDIO_START_SONG, deaths[k].song));
        assert(e.defeated && aos_audio_requested(AOS_AUDIO_START_SONG, deaths[k].song) == 1);
    }
}

static void flea_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;      /* floor top 160 */
    static const uint8_t two[] = {4, 4}, four[] = {4, 4, 4, 4};
    const AosAnimDef defs[4] = {{2, two}, {2, two}, {2, two}, {4, four}};
    const AosAnimSet anims = {defs, 4};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int a = 0; a < 4; ++a)
        for (int f = 0; f < 4; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-6, -12, 12, 12};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {20, 15, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(300), AOS_FIXED(159), NULL);

    /* On the floor, state 1; step 0 then step 4. */
    AosEnemy f;
    assert(aos_enemy_create(&f, AOS_ENEMY_FLEAMAN, 100, 140, 0, 0, &soma, &layer, &kind, &stats));
    assert(f.state == 1 && (f.y >> 16) == 159 && !f.dropped);
    aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(f.step == 4);
    /* random() % 30 = 0: a hop at 0.8125 toward the player, vy -3.0. */
    aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(f.step == 6 && f.mirrored && f.vx == 0xD000 && f.vy == -0x30000 && f.ay == 0xA000);
    int frames = 0;
    int32_t top = f.y;
    while (f.step == 6 && frames < 100) {
        aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        if (f.y < top) top = f.y;
        ++frames;
    }
    assert(f.step == 4 && top < AOS_FIXED(159) && (f.y >> 16) >= 160);
    /* random() % 30 = 14 or more: straight up with animation 3, frame 1,
     * paused; the apex sets frame 2. */
    static const uint32_t up[] = {20};
    script = up;
    script_at = 0;
    aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, scripted);
    assert(f.step == 5 && f.vx == 0 && f.vy == -0x30000 && f.ay == 0x5000 && f.anim.id == 3 &&
           f.anim.frame == 1 && (f.anim.flags & 2));
    frames = 0;
    bool apex = false;
    while (f.step == 5 && frames < 100) {
        aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        if (f.anim.frame == 2) apex = true;
        ++frames;
    }
    assert(apex && f.step == 4 && f.anim.frame == 0);

    /* A carried Fleaman (parameters 0xFF) waits in step 1. */
    AosEnemy carried;
    assert(aos_enemy_create(&carried, AOS_ENEMY_FLEAMAN, 100, 60, 0xFF, 0xFF, &soma, &layer, &kind,
                            &stats));
    for (int i = 0; i < 5; ++i)
        aos_enemy_update(&carried, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(carried.dropped && carried.step == 1 && carried.y == AOS_FIXED(60));

    /* The killing blow: state 0xFF, animation 2, the blink and flames of
     * sub_0806BE74 from timer 50. */
    soma = aos_soma_spawn(f.x, f.y, NULL);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-20, -40, 40, 40, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    f.hp = 1;
    f.step = 4;
    AosHitReport hit;
    frames = 0;
    do {
        hit = aos_enemy_update(&f, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, always_zero);
    } while (!hit.killed && ++frames < 10);
    assert(hit.killed && f.state == 0xFF);
    int dying = 0;
    while (!f.removed && dying < 100) {
        aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++dying;
    }
    assert(dying == 0x32 && f.anim.id == 2);
}

static void warrior_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;      /* floor top 160 */
    static const uint8_t idle[] = {4, 4, 4, 4}, charge[] = {3, 3, 3, 3}, brake[] = {20, 20},
                         death[] = {30, 30};
    const AosAnimDef defs[4] = {{4, idle}, {4, charge}, {2, brake}, {2, death}};
    const AosAnimSet anims = {defs, 4};
    static AosEnemyKind kind;
    kind.anims = &anims;
    kind.probes = (AosProbes){1, -20, -1, 7, {-10}};
    for (int a = 0; a < 4; ++a)
        for (int f = 0; f < 4; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -32, 16, 32};
        }
    const AosEnemyStats stats = {50, 30, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(240), AOS_FIXED(159), NULL);

    /* EnemyDeadWarriorCreate: on the floor plus one pixel, + 0x1C = 50. */
    AosEnemy w;
    assert(aos_enemy_create(&w, AOS_ENEMY_DEAD_WARRIOR, 100, 140, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(w.state == 0 && (w.y >> 16) == 160 && w.shot == 50);
    /* It walks with + 0x18 rising by 0x4000 to 2.0; past + 0x1C 100 it
     * brakes (animation 2). */
    int frames = 0;
    aos_audio_clear();
    while (w.step != 3 && frames < 200) {
        aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(w.step == 3 && frames == 51 && w.anim.id == 2 && w.walk_timer == 0x20000);
    /* sub_080B72F8: 0x7D at + 0x0D 0, 25 and 50 on screen, 0x127 as it
     * brakes. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x7D) == 3 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x127) == 1);
    /* Braking: dust every 4 frames, then it turns around and walks again. */
    bool mirrored = w.mirrored;
    int dust = 0;
    frames = 0;
    while (w.step == 3 && frames < 100) {
        AosHitReport r = aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_zero);
        dust += r.effect_count;
        ++frames;
    }
    assert(w.step == 0 && w.mirrored != mirrored && dust == 10 && w.walk_timer < 0x20000);

    /* The killing blow: thrown back and up, fire every 4 frames. */
    soma = aos_soma_spawn(w.x, w.y, NULL);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-20, -40, 40, 40, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    w.hp = 1;
    AosHitReport hit = aos_enemy_update(&w, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0,
                                        always_zero);
    assert(hit.killed && w.state == 1);
    hit = aos_enemy_update(&w, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, always_zero);
    assert(w.anim.id == 3 && w.vy == -0x28000 + 0x2000 && w.ay == 0x2000);
    int fires = 0, dying = 1;
    aos_audio_clear();
    while (!w.removed && dying < 200) {
        hit = aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        for (int i = 0; i < hit.effect_count; ++i)
            if (hit.effects[i].kind == AOS_EFFECT_EMBER) ++fires;
        ++dying;
    }
    /* Deleted on the update after its 60-tick death animation ends (or off
     * screen first). */
    assert(w.removed && fires >= 10);
    /* sub_080B747C: 0x6E every eighth frame of the fall. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E) == (dying + 1) / 8);
}

static void knight_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;      /* floor top 160 */
    static const uint8_t idle[] = {8}, walk[] = {6, 6, 6, 6}, swing[] = {4, 4, 4};
    const AosAnimDef defs[5] = {{1, idle}, {4, walk}, {3, swing}, {3, swing}, {3, swing}};
    const AosAnimSet anims = {defs, 5};
    static AosEnemyKind kind;
    kind.anims = &anims;
    kind.margins[4][0] = kind.margins[4][1] = 48;
    for (int a = 0; a < 5; ++a)
        for (int f = 0; f < 4; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -40, 16, 40};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {80, 30, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(159), NULL);

    /* It waits beyond (x - 90, y - 48, 180, 96), then walks at 0.5. */
    AosEnemy k;
    assert(aos_enemy_create(&k, AOS_ENEMY_SKELETON_KNIGHT, 150, 140, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(k.state == 0 && (k.y >> 16) == 159);
    aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(k.step == 0);
    soma.x = AOS_FIXED(230);
    aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(k.step == 1 && k.anim.id == 1);
    int32_t x0 = k.x;
    aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(k.x == x0 + 0x8000);
    /* Within (x - 40, y - 40, 80, 80): r & 0x7F <= 0x40 attacks. */
    int frames = 0;
    while (k.state == 0 && frames < 300) {
        aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(k.state == 1 && ((soma.x - k.x) >> 16) <= 40);
    /* 45 + 31 + 21 frames of animations 2, 3 and 4, then walking again;
     * sub_0808EFD8 plays 0x85 once, at + 0x0D 0x0B of animation 3. */
    frames = 0;
    int swing_frame = -1;
    aos_audio_clear();
    while (k.state == 1 && frames < 200) {
        aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
        if (swing_frame < 0 && aos_audio_requested(AOS_AUDIO_START_SONG, 0x85))
            swing_frame = frames;
    }
    assert(frames == 1 + 45 + 31 + 21 && k.step == 1 && k.anim.id == 1);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x85) == 1 && swing_frame == 1 + 45 + 12);
    /* r & 0x7F > 0x40 within reach: it backs away (step 2) facing him. */
    frames = 0;
    while (k.step == 1 && frames < 300) {
        aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_one_hundred);
        ++frames;
    }
    assert(k.step == 2 && k.state == 0 && k.mirrored);
    x0 = k.x;
    aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_one_hundred);
    assert(k.x == x0 - 0x8000);
    /* gEwramData + 0x13168: with the Killer Doll soul's decoy to its right
     * the knight's attack faces the decoy (sub_0806CF2C), not Soma on its
     * left; without it, it faces Soma. */
    soma.x = k.x - AOS_FIXED(30);
    k.state = 1;
    k.step = 0;
    aos_enemy_set_decoy(true, k.x + AOS_FIXED(30), k.y);
    aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(k.mirrored);
    aos_enemy_set_decoy(false, 0, 0);
    k.state = 1;
    k.step = 0;
    aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(!k.mirrored);
}

static void crusader_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;      /* floor top 160 */
    static const uint8_t idle[] = {8}, walk[] = {6, 6, 6, 6}, swing[] = {4, 4, 4};
    const AosAnimDef defs[8] = {{1, idle},  {4, walk},  {3, swing}, {3, swing},
                                {3, swing}, {3, swing}, {3, swing}, {3, swing}};
    const AosAnimSet anims = {defs, 8};
    static AosEnemyKind kind;
    kind.anims = &anims;
    kind.margins[4][0] = kind.margins[4][1] = 48;
    for (int a = 0; a < 8; ++a)
        for (int f = 0; f < 4; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -40, 16, 40};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {180, 30, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(159), NULL);

    /* It waits beyond (x - 90, y - 48, 180, 96), then walks at 0.75. */
    AosEnemy k;
    assert(aos_enemy_create(&k, AOS_ENEMY_DEAD_CRUSADER, 150, 140, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(k.state == 0 && (k.y >> 16) == 159);
    aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(k.step == 0);
    soma.x = AOS_FIXED(230);
    aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(k.step == 1 && k.anim.id == 1);
    int32_t x0 = k.x;
    aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(k.x == x0 + 0xC000);
    /* Within (x - 40, y - 40, 80, 80): r & 0x7F <= 0x40 attacks; r & 0x7F
     * <= 0x3F then thrusts: 39 + 31 + 21 frames of animations 2, 3, 4. */
    int frames = 0;
    while (k.state == 0 && frames < 300) {
        aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(k.state == 1 && ((soma.x - k.x) >> 16) <= 40);
    frames = 0;
    aos_audio_clear();
    while (k.state == 1 && frames < 200) {
        aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        if (k.step == 3) assert(k.anim.id == 4);
        ++frames;
    }
    assert(frames == 1 + 39 + 31 + 21 && k.step == 1 && k.anim.id == 1);
    /* sub_0808F6E8: 0x85 once in the thrust (+ 0x0D 0x0B). */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x85) == 1);
    /* Otherwise it slashes: 13 frames, then animation 6 four times 13. */
    k.state = 1;
    k.step = 0;
    frames = 0;
    bool slashed = false;
    aos_audio_clear();
    while (k.state == 1 && frames < 200) {
        aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_one_hundred);
        slashed |= k.anim.id == 6;
        ++frames;
    }
    assert(slashed && frames == 1 + 39 + 13 + 4 * 13 && k.step == 1);
    /* ... and once in the slash's 13 frames (+ 0x0D 0x0B of step 5). */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x85) == 1);
    /* r & 0x7F > 0x40 within reach: it backs away (step 2) at 0.75. */
    frames = 0;
    while (k.step == 1 && frames < 300) {
        aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_one_hundred);
        ++frames;
    }
    assert(k.step == 2 && k.state == 0);
    x0 = k.x;
    aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_one_hundred);
    assert(k.x == x0 + (k.mirrored ? -0xC000 : 0xC000));

    /* While Soma attacks it guards (animation 7, step 5): his blows deal 0
     * damage; after 61 frames it walks on. */
    soma.flags |= AOS_FLAG_ATTACKING;
    soma.facing_left = false;
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    soma.x = k.x - AOS_FIXED(30);
    soma.y = AOS_FIXED(159);
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{20, -40, 20, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    aos_audio_clear();
    AosHitReport hit = aos_enemy_update(&k, &soma, &layer, &kind, &weapon, &blade, 50, 4, 0, 0,
                                        always_one_hundred);
    assert(k.step == 5 && k.guarding && k.anim.id == 7 && k.vx == 0);
    assert(hit.enemy_hit && hit.enemy_damage == 0 && k.hp == 180);
    /* sub_0808F848: a guarded blow plays 0x69. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x69) == 1);
    soma.flags &= ~(uint32_t)AOS_FLAG_ATTACKING;
    frames = 1;
    while (k.step == 5 && frames < 100) {
        aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_one_hundred);
        ++frames;
    }
    assert(frames == 62 && k.step == 1 && k.anim.id == 1);
    /* Unguarded, the same blow wounds it; a killing one runs the generic
     * death. */
    k.combat = (AosCombat){0};
    soma.x = k.x - AOS_FIXED(30);
    hit = aos_enemy_update(&k, &soma, &layer, &kind, &weapon, &blade, 50, 4, 0, 0,
                           always_one_hundred);
    assert(hit.enemy_hit && hit.enemy_damage > 0 && k.hp < 180 && !k.guarding);
    k.combat = (AosCombat){0};
    soma.x = k.x - AOS_FIXED(30);
    aos_audio_clear();
    hit = aos_enemy_update(&k, &soma, &layer, &kind, &weapon, &blade, 999, 4, 0, 0,
                           always_one_hundred);
    assert(hit.killed && k.state == 2 && k.timer == 0x28);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x72) == 1 &&
           !aos_audio_requested(AOS_AUDIO_START_SONG, 0x69));
}

static void skull_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    static const uint8_t bob[] = {10, 10, 13, 10}, dive[] = {5, 12, 10, 9, 5, 21, 2},
                         death[] = {200}, spin[] = {9, 9, 9, 9};
    const AosAnimDef defs[4] = {{4, bob}, {7, dive}, {1, death}, {4, spin}};
    const AosAnimSet anims = {defs, 4};
    static AosEnemyKind kind;
    kind.anims = &anims;
    kind.margins[4][0] = kind.margins[4][1] = 48;
    for (int a = 0; a < 4; ++a)
        for (int f = 0; f < 7; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-10, -30, 20, 30};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {500, 50, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(600), AOS_FIXED(159), NULL);

    /* A Lubicant bobs in place (vy = sin / 2) with its fin, made on the
     * first update. */
    AosEnemy l;
    assert(aos_enemy_create(&l, AOS_ENEMY_LUBICANT, 100, 60, 0, 0, &soma, &layer, &kind, &stats));
    assert(l.state == 0 && l.variant == 2);
    AosHitReport hit = aos_enemy_update(&l, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                        always_zero);
    assert(hit.child_count == 1 && hit.children[0].role == AOS_ROLE_FIN);
    AosEnemy fin = hit.children[0];
    fin.parent = &l;
    assert(fin.static_frame == 0xD && fin.hidden);
    int32_t low = l.y, high = l.y;
    for (int i = 0; i < 60; ++i) {
        aos_enemy_update(&l, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        if (l.y < low) low = l.y;
        if (l.y > high) high = l.y;
    }
    assert(l.state == 0 && high - low > AOS_FIXED(4) && high - low < AOS_FIXED(10));
    /* The player in the 0xDC x 0xA0 box 40 pixels below: it faces him and
     * chases with animation 3 and its fin turning 12 pixels ahead. */
    soma.x = AOS_FIXED(150);
    soma.y = AOS_FIXED(159);
    aos_enemy_update(&l, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(l.state == 1 && l.anim.id == 3 && l.mirrored);
    aos_enemy_update(&fin, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(!fin.hidden && fin.state == 1 && (fin.x >> 16) == (l.x >> 16) + 12 &&
           (fin.y >> 16) == (l.y >> 16) - 6 && fin.angle == 0x1200);
    /* It speeds up toward 80 pixels above him and dives on arrival. */
    int frames = 0;
    while (l.state == 1 && frames < 600) {
        aos_enemy_update(&l, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(l.state == 2 && ((soma.y - l.y) >> 16) >= 70 && ((soma.y - l.y) >> 16) <= 90);
    aos_enemy_update(&l, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(l.step == 2 && l.anim.id == 1 && l.ay == 0x1000);
    /* Rising, then at animation frame 4 the plunge at 4.0. */
    frames = 0;
    while (l.step == 2 && frames < 100) {
        aos_enemy_update(&l, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(l.step == 3 && l.anim.frame == 4 && l.ay == -0x2000);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x87) == 1);
    int32_t top = l.y;
    frames = 0;
    while (l.state == 2 && frames < 200) {
        aos_enemy_update(&l, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(l.state == 0 && l.anim.id == 0 && l.y > top && l.vy == 0);

    /* A killing blow: animation 2, three blood splashes over 33 frames,
     * the fin flung off, then the sinking generic death. */
    soma.facing_left = false;
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    soma.x = l.x - AOS_FIXED(30);
    soma.y = l.y + AOS_FIXED(30);
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{20, -50, 20, 30, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    hit = aos_enemy_update(&l, &soma, &layer, &kind, &weapon, &blade, 999, 4, 0, 0, always_zero);
    assert(hit.killed && l.state == 3 && l.step == 0);
    fin.state = 1;
    aos_enemy_update(&fin, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(fin.state == 3 && !fin.hidden && fin.vy == -0x38000 - 0x8000 + 0x2000 && fin.ay == 0x2000);
    int blood = 0;
    frames = 0;
    while (!l.removed && frames < 200) {
        hit = aos_enemy_update(&l, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        for (int i = 0; i < hit.effect_count; ++i) blood += hit.effects[i].kind == AOS_EFFECT_BLOOD;
        if (frames == 0) assert(l.anim.id == 2);
        ++frames;
    }
    assert(blood == 3 && l.removed && frames > 33 + 40);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x1BC) == 1);

    /* Cagnazzo circles the player for 64 frames (animation 3) before its
     * dive; Skull Millione has no fin. */
    soma.x = AOS_FIXED(150);
    soma.y = AOS_FIXED(159);
    AosEnemy c;
    assert(aos_enemy_create(&c, AOS_ENEMY_CAGNAZZO, 150, 79, 0, 0, &soma, &layer, &kind, &stats));
    c.state = 2;
    c.step = 0;
    hit = aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(hit.child_count == 1 && c.step == 5 && c.anim.id == 3);
    frames = 1;
    while (c.step == 5 && frames < 100) {
        aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        assert(c.vx >= -0x28000 && c.vx <= 0x28000 && c.vy >= -0x8000 && c.vy <= 0x8000);
        ++frames;
    }
    assert(frames == 66 && c.step == 1);
    aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(c.step == 2 && c.anim.id == 1 && c.vy == -0x20000 + 0x1000);
    frames = 0;
    while (c.step == 2 && frames < 100) {
        aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(c.step == 3 && aos_audio_requested(AOS_AUDIO_START_SONG, 0x1BA) == 1);
    AosEnemy m;
    assert(aos_enemy_create(&m, AOS_ENEMY_SKULL_MILLIONE, 100, 60, 0, 0, &soma, &layer, &kind,
                            &stats));
    hit = aos_enemy_update(&m, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(hit.child_count == 0 && m.variant == 0);
}

static void beam_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;      /* floor top 160 */
    static const uint8_t idle[] = {10, 10, 10}, walk[] = {11, 11, 11, 11}, raise[] = {8, 6, 6},
                         charge[] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
                         shoot[] = {3, 3, 4, 3, 3, 24}, hold[] = {4, 4};
    const AosAnimDef defs[6] = {{3, idle},    {4, walk},  {3, raise},
                                {17, charge}, {6, shoot}, {2, hold}};
    const AosAnimSet anims = {defs, 6};
    static AosEnemyKind kind;
    kind.anims = &anims;
    kind.probes = (AosProbes){1, -30, -1, 8, {-15}};
    for (int a = 0; a < 6; ++a)
        for (int f = 0; f < 17; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -40, 16, 40};
        }
    for (int f = 0; f < 6; ++f) kind.sprites[4][f] = (uint8_t)(14 + f);   /* frame 5: 0x13 */
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {100, 30, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(600), AOS_FIXED(159), NULL);

    /* It patrols at 0.375, turning 40 pixels from its home column. */
    AosEnemy b;
    assert(aos_enemy_create(&b, AOS_ENEMY_BEAM_SKELETON, 200, 140, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(b.state == 0 && (b.y >> 16) == 159 && b.home == 200 && b.home_right == 264 &&
           b.home_left == 136);
    int32_t lo = b.x, hi = b.x;
    for (int i = 0; i < 600; ++i) {
        aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        if (b.x < lo) lo = b.x;
        if (b.x > hi) hi = b.x;
    }
    assert(b.state == 0 && b.anim.id == 1 && (lo >> 16) >= 158 && (lo >> 16) <= 160 &&
           (hi >> 16) >= 240 && (hi >> 16) <= 242);

    /* The player in its 0xC0 x 0x80 box: it closes in at a random pace,
     * backing away when he is near, then fires once its wait is over and
     * he is in its 100 x 40 box. */
    b.x = AOS_FIXED(200);
    soma.x = AOS_FIXED(240);
    aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(b.state == 1 && b.vx == 0);
    aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(b.step == 2 && b.timer == 0x13 && b.mirrored && b.speed == 0x8000 - 0x800);
    int32_t x0 = b.x;
    int frames = 0;
    while (b.state == 1 && frames < 100) {
        aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(b.state == 3 && b.x > x0 && frames == 20);

    /* Animation 2 to its end, 33 frames of animation 3, then animation 4
     * at full scale until its sprite frame 0x13 fires the flash and the
     * beam. */
    AosHitReport hit = {0};
    AosEnemy flash = {0}, beam = {0};
    frames = 0;
    while (b.state == 3 && b.step < 3 && frames < 200) {
        hit = aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        if (b.step == 1) assert(b.scale == 0);
        ++frames;
    }
    assert(b.step == 3 && b.firing && b.scale == 0xFFFF && hit.child_count == 2);
    flash = hit.children[0];
    beam = hit.children[1];
    flash.parent = beam.parent = &b;
    assert(flash.role == AOS_ROLE_BEAM_FLASH && flash.static_frame == 0x15 && flash.translucent);
    assert(beam.role == AOS_ROLE_BEAM && beam.static_frame == 0x16 && beam.stats.contact == 45 &&
           beam.x == b.x + 0xB0000 && beam.y == b.y - 0x110000);
    /* The beam waits at the eye, then shoots at 12.0 and loops back
     * beyond 92 pixels. */
    for (int i = 0; i < 5; ++i)
        aos_enemy_update(&beam, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(beam.state == 3 && beam.x == b.x + 0xB0000 + 0xC0000 && beam.static_frame == 0x16);
    aos_enemy_update(&beam, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(beam.x == b.x + 0xB0000 + 2 * 0xC0000 && beam.static_frame == 0x17);
    int32_t far = 0;
    for (int i = 0; i < 12; ++i) {
        aos_enemy_update(&beam, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        if (beam.x - b.x > far) far = beam.x - b.x;
    }
    assert((far >> 16) <= 0x5C + 12 && (far >> 16) > 0x5C - 12 && beam.x < b.x + far);
    /* Its blast hurts the player in its path at 3/2 of the contact. */
    soma.x = b.x + AOS_FIXED(40);
    soma.y = b.y;
    soma.combat = (AosCombat){.type = AOS_TYPE_PLAYER};
    int hits = 0;
    for (int i = 0; i < 10; ++i) {
        hit = aos_enemy_update(&beam, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        hits += hit.soma_hit;
    }
    assert(hits >= 1);
    soma.x = AOS_FIXED(600);
    /* 43 frames at full scale, then 64 shrinking, the beam harmless and
     * scaled; 9 frames later the beam is gone and it closes in again. */
    frames = 0;
    while (b.scale == 0xFFFF && frames < 100) {
        aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(frames == 44 && b.scale == 0xFFFF - 0x400);
    aos_enemy_update(&beam, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    aos_enemy_update(&flash, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(beam.combat.attack_off && beam.scaled && flash.scaled && !beam.removed);
    frames = 0;
    while (b.state == 3 && frames < 200) {
        aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(frames == 63 + 9 && b.state == 1 && !b.firing);
    aos_enemy_update(&beam, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    aos_enemy_update(&flash, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(beam.removed && flash.removed);

    /* A killing blow: hidden, seven bones flung away from the player (to
     * its right here), deleted on the next update. */
    soma.facing_left = false;
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    soma.x = b.x - AOS_FIXED(30);
    soma.y = b.y;
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{20, -40, 20, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    b.state = 0;
    b.step = 1;
    aos_audio_clear();
    hit = aos_enemy_update(&b, &soma, &layer, &kind, &weapon, &blade, 999, 4, 0, 0, always_zero);
    assert(hit.killed && b.state == 0xFF && b.hidden && hit.child_count == 7);
    /* sub_080CB4F0's sound. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x6B) == 1);
    for (int i = 0; i < 7; ++i) {
        assert(hit.children[i].role == AOS_ROLE_DEBRIS && hit.children[i].vx > 0 &&
               hit.children[i].ay == 0x3000 && hit.children[i].static_frame >= 0x18 &&
               hit.children[i].static_frame <= 0x1E);
    }
    /* The table's offsets as they are: bone 3 at x + 4, bone 5 at x - 4. */
    assert((hit.children[3].x >> 16) == (b.x >> 16) + 4 &&
           (hit.children[5].x >> 16) == (b.x >> 16) - 4);
    assert(hit.children[0].vy == -(50 << 12) - 0x10 && hit.children[0].vx == 20 << 12);
    aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(b.removed);
}

static void killer_doll_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;      /* floor top 160 */
    static const uint8_t rest[] = {200}, wake[] = {2, 2}, rise[] = {20}, pause[] = {2, 2},
                         fly[] = {9, 9}, attack[] = {5, 5}, land[] = {8, 8};
    const AosAnimDef defs[7] = {{1, rest}, {2, wake},   {1, rise}, {2, pause},
                                {2, fly}, {2, attack}, {2, land}};
    const AosAnimSet anims = {defs, 7};
    static const uint8_t flame_ticks[] = {4, 4, 4};
    const AosAnimDef flame_def = {3, flame_ticks};
    const AosAnimSet flame_set = {&flame_def, 1};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    kind.flame_anims = &flame_set;
    for (int a = 0; a < 7; ++a)
        for (int f = 0; f < 2; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -24, 16, 24};
            kind.sprites[a][f] = (uint8_t)(a * 2 + f);
        }
    const AosEnemyStats stats = {200, 40, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(600), AOS_FIXED(159), NULL);

    /* It rests on the floor, then wakes within 60 pixels, rises and
     * pauses before the first flight leg. */
    AosEnemy d;
    assert(aos_enemy_create(&d, AOS_ENEMY_KILLER_DOLL, 100, 140, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(d.state == 1 && (d.y >> 16) == 159 && d.attack_wait >= 0x80 &&
           d.attack_wait <= 0x87F && d.leg >= 0x20 && d.leg <= 0x11F &&
           d.flight >= 0x100 && d.flight <= 0x2FF);
    aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.step == 0 && d.anim.id == 0);
    soma.x = AOS_FIXED(150);
    aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.step == 1 && d.anim.id == 1);
    int frames = 0;
    while (d.step == 1 && frames < 30) {
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(d.step == 2 && d.anim.id == 2 && d.vy == -0x4000);
    while (d.step == 2 && frames < 100) {
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(d.step == 4 && d.anim.id == 4);
    while (d.step == 4 && frames < 150) {
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(d.step == 3 && d.anim.id == 3 && d.leg == 0x20);

    /* Random pattern zero flies horizontally away from the player at
     * 1.0, with the sprite turned away. */
    aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.substep == 1 && d.vx == -0x10000 && d.vy == 0 && !d.mirrored && d.anim.id == 4);

    /* With its attack wait over and the player ahead, it enters animation
     * 5 and flies at 1.0 toward his upper body for the horizontal travel
     * time. */
    d.step = 3;
    d.substep = 1;
    d.vx = d.vy = 0;
    d.attack_wait = 0;
    d.leg = 100;
    d.flight = 100;
    d.mirrored = true;
    soma.x = d.x + AOS_FIXED(15);
    soma.y = d.y;
    aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.step == 5 && d.substep == 0);
    aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.anim.id == 5 && d.substep == 1 && d.vx > 0 && d.timer > 0 && d.timer <= 32);
    frames = 0;
    while (d.step == 5 && frames < 100) {
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert((d.step == 3 || d.step == 4) && d.attack_wait == 0x80 && d.leg > 0 &&
           d.flight > 0);

    /* When its flight expires it sinks to a solid cell, rests in animation
     * 6 and rises again with a new 512-frame minimum flight timer. */
    d.step = 3;
    d.substep = 1;
    d.flight = 1;
    d.leg = 100;
    d.attack_wait = 100;
    d.vx = d.vy = 0;
    d.y = AOS_FIXED(150);
    soma.x = AOS_FIXED(600);
    aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.step == 6);
    frames = 0;
    while (d.step == 6 && d.anim.id != 6 && frames < 100) {
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(d.anim.id == 6 && (d.y >> 16) == 160);
    while (d.step == 6 && frames < 200) {
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(d.step == 1 && d.anim.id == 1 && d.flight == 0x200);

    /* A killing blow hides it and creates the five Killer Doll frames.
     * The shared piece routine keeps the parent until all five leave. */
    AosEnemy dead;
    soma = aos_soma_spawn(AOS_FIXED(70), AOS_FIXED(159), NULL);
    assert(aos_enemy_create(&dead, AOS_ENEMY_KILLER_DOLL, 100, 140, 0, 0, &soma, &layer, &kind,
                            &stats));
    soma.facing_left = false;
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{20, -40, 20, 30, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    aos_audio_clear();
    AosHitReport hit = aos_enemy_update(&dead, &soma, &layer, &kind, &weapon, &blade, 999, 4,
                                        0, 0, always_zero);
    assert(hit.killed && dead.state == 0xFF && dead.hidden && hit.child_count == 5 &&
           dead.spawned_count == 5);
    /* sub_080C81AC: sound 0x6E. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E) == 1);
    for (int i = 0; i < 5; ++i) {
        AosEnemy piece = hit.children[i];
        aos_enemy_update(&piece, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        assert(piece.role == AOS_ROLE_DOLL_PIECE && piece.static_frame == 13 + i &&
               piece.glow.started);
    }
}

static void nemesis_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    static const uint8_t ticks[] = {1};
    const AosAnimDef defs[1] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    const AosEnemyStats stats = {440, 40, 24, 0x0181, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(220), AOS_FIXED(100), NULL);
    AosEnemy n;
    aos_enemy_set_flag_8e_40(false);
    aos_audio_clear();

    /* Creation: never mirrored, the native boxes, state 0 step 1 hidden
     * at angle 0x1000 with its linked sprite hidden in state 5. */
    assert(aos_enemy_create(&n, AOS_ENEMY_NEMESIS, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    assert(n.state == 0 && n.step == 1 && n.hidden && n.angle == 0x1000 && !n.mirrored);
    assert(n.own_boxes && n.own_attack.x == -4 && n.own_attack.y == 4 && n.own_attack.w == 8 &&
           n.own_hurt.x == -8 && n.own_hurt.w == 16 && n.nem_index == 9 && n.nem_hits == 3);
    assert(n.nem_helper.state == 5 && n.nem_helper.hidden && n.nem_helper.mirrored &&
           n.nem_helper.frame == 1 && n.nem_helper.x == n.x);

    /* The first update emits the linked sprite and the three trails; Soma
     * 120 pixels away keeps it waiting. */
    AosHitReport first = aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_zero);
    assert(first.child_count == 4 && first.children[0].role == AOS_ROLE_NEMESIS_HELPER &&
           first.children[1].variant == 9 && first.children[2].variant == 12 &&
           first.children[3].variant == 6 && n.step == 1);
    AosEnemy helper = first.children[0];
    helper.parent = &n;
    AosEnemy effect = first.children[1];
    effect.parent = &n;

    /* Within 80 pixels: the linked sprite 10 pixels right (it faces
     * right) and 16 below, then 128 frames of fading in and out. */
    soma.x = AOS_FIXED(150);
    aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.step == 2 && n.nem_helper.x == n.x + AOS_FIXED(10) &&
           n.nem_helper.y == n.y + AOS_FIXED(16));
    aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.translucent && n.hidden == false && n.timer == 1);
    aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.hidden && n.nem_helper.hidden);
    aos_enemy_update(&helper, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(helper.hidden && helper.translucent && helper.x == n.nem_helper.x &&
           helper.static_frame == 1);
    int frames = 2;
    while (n.state == 0 && frames < 200) {
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(n.state == 1 && frames == 128 && !n.hidden && !n.translucent &&
           n.nem_helper.state == 0 && n.nem_helper.hidden);

    /* State 1: it tilts from 0x1000 to 0xF000 (Soma to its right) by 0x180
     * in 22 frames, then rises half a pixel a frame until 48 pixels above
     * Soma (within 72 horizontally already). */
    aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.step == 2 && n.nem_spin == -100 && n.own_attack.x == -8 && n.own_attack.w == 16);
    frames = 0;
    while (n.step == 2 && frames < 40) {
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(frames == 22 && n.angle == 0xF000 && n.step == 3);
    frames = 0;
    while (n.state == 1 && frames < 100) {
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(n.state == 2 && (n.y >> 16) == 68 && frames == 64 && n.x == AOS_FIXED(100));

    /* State 2 (row 0 drawn by the zero random): it slides away from Soma
     * two pixels a frame while tilting to 0x2800 by 800, the linked
     * sprite shows frame 2 below it, then the first attack (a swoop). */
    aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.nem_row == 0 && n.nem_index == 0 && n.x == AOS_FIXED(98) && n.step == 0);
    frames = 1;
    while (n.step == 0 && frames < 40) {
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(n.angle == 0x2800 && n.nem_helper.state == 2 && n.nem_helper.frame == 2 &&
           !n.nem_helper.mirrored && n.nem_helper.y == n.y + 0x250000 &&
           n.nem_helper.x == n.x + AOS_FIXED(9));
    while (n.state == 2 && frames < 200) {
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(n.state == 4 && n.nem_index == 1);

    /* State 4: six legs over 64 frames (+48, +8 pixels toward Soma less the
     * 8 / 24 truncation, the turns cancelling), sound 0x85 at each leg, 42
     * frames of rest. */
    int32_t x0 = n.x, y0 = n.y;
    uint16_t a0 = (uint16_t)n.angle;
    aos_audio_clear();
    frames = 0;
    while (n.step != 5 && frames < 100) {
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(frames == 64 && n.x == x0 + AOS_FIXED(48) && n.y == y0 + AOS_FIXED(8) - 8 &&
           (uint16_t)n.angle == a0 && aos_audio_requested(AOS_AUDIO_START_SONG, 0x85) == 7);
    while (n.state == 4 && frames < 200) {
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(n.state == 1 && frames == 64 + 42);

    /* State 3: three dashes of 16 frames, the first two followed by a
     * 9-frame recoil, then 42 frames of rest; 0x85 at each dash's end. The
     * tip box follows the angle (0: 16 x 8 at (0, 8)). */
    n.state = 3;
    n.step = 0;
    n.angle = 0;
    aos_audio_clear();
    frames = 0;
    while (n.state == 3 && frames < 200) {
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        if (frames == 0)
            assert(n.own_hurt.x == 0 && n.own_hurt.y == 8 && n.own_hurt.w == 16 &&
                   n.own_hurt.h == 8 && n.nem_spin == -0x320 && n.vy == 0x10000);
        ++frames;
    }
    assert(frames == 25 + 25 + 17 + 42 && aos_audio_requested(AOS_AUDIO_START_SONG, 0x85) == 3);

    /* State 5: 65 frames spinning by 0x1000 (0x85 every 16), then a
     * two-pixel flight at Soma for |dx / vx| frames; the turn frame and the
     * two last steps also move it, two pixels past him. */
    n.state = 5;
    n.step = 0;
    n.x = AOS_FIXED(100);
    n.y = AOS_FIXED(100);
    aos_audio_clear();
    for (int i = 0; i < 66; ++i)
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.step == 2 && n.nem_spin == 0 && n.vx == 0x20000 && n.vy == 0 && n.timer == 25 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x85) == 6);
    while (n.state == 5)
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.state == 1 && n.x == AOS_FIXED(154));

    /* Blows: three only count down + 0x24; the fourth starts the recoil
     * (state 6) with the linked sprite in state 3 behind it. */
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-8, -16, 16, 24, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    n.state = 1;
    n.step = 3;
    n.x = soma.x;
    n.y = soma.y - AOS_FIXED(8);
    n.own_hurt = (AosBox){-8, -8, 16, 16};
    const AosCombat fresh = n.combat;
    for (int i = 0; i < 3; ++i) {
        n.combat = fresh;
        n.hit_flash = 0;
        AosHitReport hit = aos_enemy_update(&n, &soma, &layer, &kind, &weapon, &blade, 60, 4,
                                            0, 0, always_zero);
        assert(hit.enemy_hit && !hit.killed && n.hit_flash == 8 && n.state == 1);
    }
    assert(n.nem_hits == 0);
    n.combat = fresh;
    n.hit_flash = 0;
    AosHitReport hit = aos_enemy_update(&n, &soma, &layer, &kind, &weapon, &blade, 60, 4, 0, 0,
                                        always_zero);
    assert(hit.enemy_hit && n.state == 6 && n.nem_hits == 3 && n.nem_helper.state == 3 &&
           n.nem_helper.frame == 3);
    /* Flashing, it only collides; then, Soma not left of it, it is knocked
     * left until the linked sprite has flickered out. */
    frames = 0;
    while (n.state == 6 && frames < 200) {
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(n.state == 1 && n.x < soma.x);

    /* A lethal blow: sound 0x1C3, the recoil, then the fall (0x72), the
     * trails deleted, the body gone after 66 frames. */
    n.combat = fresh;
    n.hit_flash = 0;
    n.state = 1;
    n.step = 3;
    n.x = soma.x;
    n.y = soma.y - AOS_FIXED(8);
    aos_audio_clear();
    hit = aos_enemy_update(&n, &soma, &layer, &kind, &weapon, &blade, 9999, 4, 0, 0,
                           always_zero);
    assert(hit.killed && n.defeated && n.state == 6 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x1C3) == 1);
    while (n.state == 6)
        aos_enemy_update(&n, &soma, &layer, &kind, &weapon, &blade, 9999, 4, 0, 0, always_zero);
    assert(n.state == 0xFF && (n.nem_flags & 0x20));
    aos_enemy_update(&effect, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(effect.removed);
    frames = 0;
    while (!n.removed && frames < 100) {
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 1000, always_zero);
        ++frames;
    }
    assert(frames == 66 && aos_audio_requested(AOS_AUDIO_START_SONG, 0x72) == 1);
    aos_enemy_update(&helper, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(helper.removed);

    /* With 0x13168 cleared (the Nemesis soul) the target helpers return
     * the enemy's own position: Soma far away, it still appears. */
    soma.x = AOS_FIXED(600);
    assert(aos_enemy_create(&n, AOS_ENEMY_NEMESIS, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.step == 1);
    aos_enemy_set_no_target(true);
    aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    aos_enemy_set_no_target(false);
    assert(n.step == 2);

    aos_enemy_set_flag_8e_40(true);
    assert(aos_enemy_create(&n, AOS_ENEMY_NEMESIS, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(n.state == 7);
    aos_enemy_update(&n, &soma, &layer, &kind, &weapon, &blade, 999, 4, 0, 0, always_zero);
    assert(n.state == 7 && !n.defeated);
}

static void doll_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;      /* floor top 160 */
    static const uint8_t rest[] = {200}, rise[] = {20, 9, 20}, fly[] = {9, 9, 9, 9},
                         charge[] = {12, 12, 8}, spark[] = {1, 2, 1}, land[] = {16, 20};
    const AosAnimDef defs[6] = {{1, rest},   {3, rise},  {4, fly},
                                {3, charge}, {3, spark}, {2, land}};
    const AosAnimSet anims = {defs, 6};
    static const uint8_t flame_ticks[] = {4, 4, 4};
    const AosAnimDef flame_def = {3, flame_ticks};
    const AosAnimSet flame_set = {&flame_def, 1};
    static AosEnemyKind kind;
    kind.anims = &anims;
    kind.flame_anims = &flame_set;
    for (int a = 0; a < 6; ++a)
        for (int f = 0; f < 4; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -24, 16, 24};
            kind.sprites[a][f] = (uint8_t)(a * 4 + f);
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {200, 40, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(600), AOS_FIXED(159), NULL);

    /* It rests on the floor until the player is in its 0xF0 x 0x80 box. */
    AosEnemy d;
    assert(aos_enemy_create(&d, AOS_ENEMY_LIGHTNING_DOLL, 100, 140, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(d.state == 1 && (d.y >> 16) == 159 && d.attack_wait >= 0x10 &&
           d.attack_wait <= 0x2F && d.flight >= 0x200 && d.flight <= 0x5FF);
    aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.step == 0 && d.anim.id == 0);
    soma.x = AOS_FIXED(180);
    aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.step == 1 && d.anim.id == 1);
    d.attack_wait = 0x200;      /* (r & 0x3FF) == 0 would attack at once */
    int frames = 0;
    while (d.step == 1 && frames < 100) {
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(d.step == 2);
    /* r % 6 = 0: 0.5 diagonally toward the player (up and right here) for
     * (r & 0xFF) + 0x80 frames, facing its vx. */
    aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.substep == 1 && d.vx == 0x8000 && d.vy == 0x8000 && d.leg == 0x80 &&
           d.anim.id == 2);
    /* Below him (y 159 against 159: not above), it heads down into the
     * floor and bounces off it (sub_080C9788). */
    int32_t y0 = d.y;
    for (int i = 0; i < 4; ++i)
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.vy < 0 && d.y <= y0 + 0x10000 && d.mirrored);

    /* With its attack wait over and the player ahead within 20 pixels it
     * charges (animation 3), then sparks (animation 4 with the attack box
     * (-14, -32, 30, 28)) for (r & 0xFF) + 0x80 frames. */
    d.attack_wait = 0;
    d.flight = 0x300;
    soma.x = d.x + AOS_FIXED(15);
    soma.y = d.y;
    aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.step == 3 && d.substep == 0);
    aos_audio_clear();
    aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.anim.id == 3 && d.vx == 0 && d.vy == 0 && d.timer == 0x80);
    /* sub_080C8B3C holds the spark loop 0x75 (sub_08013D60). */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x75) == 1);
    frames = 0;
    while (d.anim.id != 4 && frames < 100) {
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    frames = 0;
    bool boxed = false, sparked = false;
    soma.combat = (AosCombat){.type = AOS_TYPE_PLAYER};
    while (d.step == 3 && frames < 400) {
        AosHitReport hit =
            aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        if (d.own_boxes && d.own_attack.x == -14 && d.own_attack.w == 30) boxed = true;
        for (int i = 0; i < hit.effect_count; ++i)
            if (hit.effects[i].kind == AOS_EFFECT_SPARK && hit.effects[i].follow) sparked = true;
        ++frames;
    }
    assert(boxed && sparked && frames == 0x80 + 2 && d.step == 2);
    /* Held again at 0x80, 0x70 ... 0x30 of the countdown, then released
     * (sub_08013E18) when the attack ends. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x75) == 7);
    for (int slot = 0; slot < 4; ++slot) assert(aos_audio_held(slot) != 0x75);

    /* Out of flight it sinks a pixel a frame to a cell, rests (animation 5)
     * and rises again. */
    d.flight = 1;
    d.attack_wait = 0x40;
    d.substep = 1;
    d.leg = 100;
    d.y = AOS_FIXED(150);
    soma.x = AOS_FIXED(600);
    aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.step == 4);
    frames = 0;
    while (d.step == 4 && d.anim.id != 5 && frames < 100) {
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(d.anim.id == 5 && (d.y >> 16) == 160);
    frames = 0;
    while (d.step == 4 && frames < 200) {
        aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(d.step == 1 && d.anim.id == 1 && d.flight == 0x200 && frames == 1 + 0x41);

    /* A killing blow: hidden, five pieces (the doll waits for them). */
    soma.facing_left = false;
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    soma.x = d.x - AOS_FIXED(30);
    soma.y = d.y + AOS_FIXED(10);
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{20, -40, 20, 30, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    d.hit_flash = 0;
    aos_audio_hold(0x75);
    aos_audio_clear();
    AosHitReport hit =
        aos_enemy_update(&d, &soma, &layer, &kind, &weapon, &blade, 999, 4, 0, 0, always_zero);
    assert(hit.killed && d.state == 0xFF && d.hidden && hit.child_count == 5 &&
           d.spawned_count == 5);
    /* sub_080C8FBC: the loop released, then 0x6E. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E) == 1);
    for (int slot = 0; slot < 4; ++slot) assert(aos_audio_held(slot) != 0x75);
    AosEnemy pieces[5];
    for (int i = 0; i < 5; ++i) pieces[i] = hit.children[i];
    /* First update: frame, speed (pointing right: the player is left of
     * the doll), the glow lit 16 pixels up. */
    for (int i = 0; i < 5; ++i) {
        aos_enemy_update(&pieces[i], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        assert(pieces[i].state == 1 && pieces[i].static_frame == 20 + i &&
               pieces[i].vy == -0x18000 && pieces[i].ay == 0x1800 && pieces[i].glow.started);
    }
    assert(pieces[0].vx == 0x8000 && pieces[4].vx == -0x8000 && pieces[1].timer == 0x80);
    /* Piece 1 bounces on the floor and comes to rest; the others fall
     * through. Each ends hidden and goes once its glow has cycled. */
    int flames = 0, dust = 0;
    aos_audio_clear();
    for (int f = 0; f < 200; ++f)
        for (int i = 0; i < 5; ++i) {
            if (pieces[i].removed) continue;
            hit = aos_enemy_update(&pieces[i], &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                   always_zero);
            for (int k = 0; k < hit.effect_count; ++k) {
                flames += hit.effects[k].kind == AOS_EFFECT_FLAME;
                dust += hit.effects[k].kind == AOS_EFFECT_DUST && hit.effects[k].particle;
            }
            if (pieces[i].removed && d.spawned_count) d.spawned_count--;
        }
    for (int i = 0; i < 5; ++i) assert(pieces[i].removed);
    /* Piece 1 rests with its probe (y - 15) on the floor; piece 2 starts
     * below timer 0x30, so four flames and dust puffs. */
    assert((pieces[1].y >> 16) >= 170 && (pieces[1].y >> 16) <= 175 && flames == 4 &&
           dust == 4);
    /* sub_080C9284: 0x153 at each of piece 1's floor bounces. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x153) >= 1);
    aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.removed);

    /* The spark held on the player (sub_080C9198). */
    static const uint8_t spark_ticks[] = {3, 3};
    const AosAnimDef spark_def = {2, spark_ticks};
    const AosAnimSet spark_set = {&spark_def, 1};
    const AosEffectWorld world = {&layer, 0, 0, always_zero, AOS_FIXED(77), 120};
    AosEffect s = {.kind = AOS_EFFECT_SPARK, .follow = true};
    assert(aos_effect_update(&s, &spark_set, NULL, &world));
    frames = 0;
    while (aos_effect_update(&s, &spark_set, NULL, &world) && frames < 50) {
        /* Its integer y is reset to 128 each frame, the fraction kept. */
        assert(s.x == AOS_FIXED(77) && (s.y >> 16) >= 128 + (s.timer - 1) / 2 &&
               (s.y >> 16) <= 129 + (s.timer - 1) / 2);
        ++frames;
    }
    assert(frames == 5);
}

static void ghost_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    static const uint8_t dance[] = {6, 6}, death[] = {5, 5};
    const AosAnimDef defs[3] = {{2, dance}, {2, dance}, {2, death}};
    const AosAnimSet anims = {defs, 3};
    static AosEnemyKind kind;
    kind.anims = &anims;
    kind.margins[4][0] = kind.margins[4][1] = 48;
    for (int a = 0; a < 3; ++a)
        for (int f = 0; f < 2; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -16, 16, 32};
        }
    const AosEnemyStats stats = {25, 15, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(159), NULL);

    /* State 1: step 0 faces the player; then a figure of eight, x +=
     * 2 sin(phase + 0x4000), y += sin(4 phase), phase += 0x80. */
    AosEnemy g;
    assert(aos_enemy_create(&g, AOS_ENEMY_GHOST_DANCER, 100, 80, 1, 0, &soma, &layer, &kind,
                            &stats));
    assert(g.state == 1 && !g.translucent);
    aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(g.step == 1 && g.mirrored);
    int32_t x0 = g.x, y0 = g.y;
    aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(g.x == x0 + 2 * aos_sine(0x4000) && g.y == y0 + aos_sine(0) && g.phase == 0x80);
    /* After half a turn (256 frames) the fade starts: 33 alternating
     * frames, then semi-transparent; a full turn later it fades back. */
    int frames = 0;
    while (g.fade == 0 && frames < 400) {
        aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(frames == 255 && g.fade == 1);
    frames = 0;
    while (g.fade == 1 && frames < 100) {
        aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(frames == 34 && g.fade == 2);
    aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(g.translucent);
    frames = 0;
    while (g.fade != 0 && frames < 600) {
        aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(g.fade == 0 && !g.translucent);

    /* The killing blow: animation 2 once, semi-transparent, deleted when it
     * ends; no flames. */
    soma = aos_soma_spawn(g.x, g.y + AOS_FIXED(20), NULL);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-20, -60, 40, 60, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    g.hp = 1;
    int cam = (g.x >> 16) - 120, cam_y = (g.y >> 16) - 80;
    AosHitReport hit = aos_enemy_update(&g, &soma, &layer, &kind, &weapon, &blade, 10, 4, cam,
                                        cam_y, always_zero);
    assert(hit.killed && g.state == 3 && g.anim.id == 2 && g.translucent);
    int dying = 0;
    while (!g.removed && dying < 50) {
        hit = aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, cam, cam_y,
                               always_zero);
        assert(hit.effect_count == 0);
        ++dying;
    }
    /* 10 ticks of animation 2; the update after it ends deletes it. */
    assert(dying == 11);
}

static void gargoyle_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    static const uint8_t flap[] = {4, 4}, swoop[] = {6, 6}, death[] = {200};
    const AosAnimDef defs[4] = {{2, flap}, {2, swoop}, {2, flap}, {1, death}};
    const AosAnimSet anims = {defs, 4};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {30, 20, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(159), NULL);

    /* State 0 step 0: facing the player (right), 0.5; step 1 chases the
     * point 64 pixels above him by 0x1000 per frame. */
    AosEnemy g;
    assert(aos_enemy_create(&g, AOS_ENEMY_GARGOYLE, 100, 60, 0, 0, &soma, &layer, &kind, &stats));
    assert(g.state == 0);
    aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(g.step == 1 && g.mirrored && g.vx == 0x9000 && g.vy == 0x1000);
    /* It ends level with the point (within 8 pixels) and within 96
     * horizontally: animation 3 and the brake (step 2). */
    int frames = 0;
    while (g.step == 1 && frames < 400) {
        aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    int32_t dy = soma.y - (g.y + 0x400000), dx = soma.x - g.x;
    assert(g.step == 2 && g.anim.id == 3 && dy >= -0x80000 && dy <= 0x80000);
    assert((dx < 0 ? -dx : dx) <= 0x600000);
    /* 21 frames of braking, then the swoop: animation 1, (0xC00, 0x1800)
     * up to 2.0 for 25 frames, then 15/16 of vy for 61 frames, then the
     * climb. */
    frames = 0;
    aos_audio_clear();
    while (g.step == 2 && frames < 100) {
        aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(frames == 21 && g.step == 3 && g.anim.id == 1 && g.ay == 0x1800);
    /* sub_08086534: the swoop's sound 0x87. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x87) == 1);
    assert(g.ax == (g.mirrored ? 0xC00 : -0xC00));
    int32_t y_dive = g.y;
    frames = 0;
    while (g.step == 3 && frames < 100) {
        aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(frames == 25 && g.y > y_dive && g.vy == 0x20000);
    frames = 0;
    while (g.step == 4 && frames < 100) {
        aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(frames == 61 && g.step == 5 && g.anim.id == 0 && g.ay == 0x18000);
    frames = 0;
    while (g.step == 5 && frames < 400) {
        aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(g.step == 1 && g.timer == 0x14 && g.ax == 0x1000 && g.ay == 0x10000);
    assert(soma.y - (g.y + 0x400000) >= 0);

    /* The killing blow: state 1, 9/10 of vx, falling, 40 frames. */
    soma = aos_soma_spawn(g.x, g.y + AOS_FIXED(20), NULL);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-10, -40, 20, 40, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    for (int a = 0; a < 4; ++a)
        for (int f = 0; f < 2; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -8, 16, 16};
        }
    g.hp = 1;
    int cam = (g.x >> 16) - 120;    /* the collision pass needs it on screen */
    AosHitReport hit = aos_enemy_update(&g, &soma, &layer, &kind, &weapon, &blade, 10, 4, cam, 0,
                                        always_zero);
    assert(hit.killed && g.state == 1 && g.ay == 0x1000 && g.timer == 0x28);
    int32_t y_fall = g.y;
    int dying = 0;
    while (!g.removed && dying < 100) {
        aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, cam, 0, always_zero);
        ++dying;
    }
    assert(dying == 0x28 && g.y > y_fall);

    /* The Gremlin runs the gargoyle's routines. */
    AosEnemy gr;
    soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(159), NULL);
    assert(aos_enemy_create(&gr, AOS_ENEMY_GREMLIN, 100, 60, 0, 0, &soma, &layer, &kind, &stats));
    aos_enemy_update(&gr, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(gr.step == 1 && gr.mirrored && gr.vx == 0x9000);
}

static void officer_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;      /* floor top 160 */
    static const uint8_t walk[] = {4, 4, 4, 4, 4, 4, 4, 4}, attack[] = {3, 3, 3, 3, 3, 3},
                         death[] = {200};
    const AosAnimDef defs[3] = {{8, walk}, {6, attack}, {1, death}};
    const AosAnimSet anims = {defs, 3};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int a = 0; a < 3; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -40, 16, 40};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {30, 24, 6, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(300), AOS_FIXED(159), NULL);

    /* EnemyZombieOfficerCreate: snapped to the floor, state 0. At the start
     * of walk frame 3 it faces the player and steps at 0.625. */
    AosEnemy z;
    assert(aos_enemy_create(&z, AOS_ENEMY_ZOMBIE_OFFICER, 100, 140, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(z.state == 0 && (z.y >> 16) == 159);
    int frames = 0;
    while (!(z.anim.frame == 3 && z.anim.tick == 0) && frames < 50) {
        aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    int32_t x0 = z.x;
    aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(z.mirrored && z.vx == 0xA000 && z.x == x0 + 0xA000);

    /* Soma within 50 pixels: at the next frame 3 or 7 it attacks
     * (animation 1 once), then walks again. */
    soma.x = z.x + AOS_FIXED(30);
    frames = 0;
    while (z.state == 0 && frames < 80) {
        aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(z.state == 1 && z.step == 0);
    aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(z.anim.id == 1 && z.step == 1);
    frames = 0;
    aos_audio_clear();
    while (z.state == 1 && frames < 80) {
        aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(z.state == 0 && z.anim.id == 0 && frames == 18);
    /* sub_080924AC: 0x85 at the start of attack frame 4. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x85) == 1);

    /* The killing blow: animation 2, then three gibs (0x10, 0x11, 0x0F at
     * (+6 mirrored, -13)) and the 40-frame generic death. */
    soma = aos_soma_spawn(z.x, z.y, NULL);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-10, -40, 20, 40, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    z.hp = 1;
    aos_audio_clear();
    AosHitReport hit = aos_enemy_update(&z, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0,
                                        always_zero);
    assert(hit.killed && z.state == 2 && z.anim.id == 2 && z.timer == 0x28);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x70) == 1);      /* sub_0809252C */
    hit = aos_enemy_update(&z, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, always_zero);
    assert(hit.child_count == 3 && hit.children[2].static_frame == 0x0F);
    int32_t dx = z.mirrored ? 0x60000 : -0x60000;
    assert(hit.children[2].x == z.x + dx && hit.children[2].y == z.y - 0xD0000);
    assert(hit.children[2].vy == -0x10000 && hit.children[0].static_frame == 0x10);
    int dying = 1;
    while (!z.removed && dying < 100) {
        aos_enemy_update(&z, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, always_zero);
        ++dying;
    }
    assert(dying == 0x28);
}

static void medusa_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    static const uint8_t two[] = {4, 4}, death[] = {200};
    const AosAnimDef defs[4] = {{2, two}, {2, two}, {2, two}, {1, death}};
    const AosAnimSet anims = {defs, 4};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    kind.margins[4][0] = kind.margins[4][1] = 48;
    const AosEnemyStats stats = {12, 20, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(60), AOS_FIXED(159), NULL);

    /* A spawner (parameter 1 bit 1): hidden, state 3. One frame in 32 it
     * spawns at screen X 256 (Soma at screen X 60 <= 0x67), Y 64 + (r &
     * 0x3F), up to parameter 0 heads; gold (bit 0) passes on. */
    AosEnemy spawner;
    assert(aos_enemy_create(&spawner, AOS_ENEMY_MEDUSA_HEAD, 0, 0, 2, 3, &soma, &layer, &kind,
                            &stats));
    assert(spawner.state == 3 && spawner.hidden && !spawner.own_boxes);
    static const uint32_t draws[] = {1, 0, 5, 0, 7, 0};
    script = draws;
    script_at = 0;
    AosHitReport hit = aos_enemy_update(&spawner, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                        scripted);
    assert(!hit.spawn && script_at == 1);
    hit = aos_enemy_update(&spawner, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, scripted);
    assert(hit.spawn && hit.spawn_x == 256 && hit.spawn_y == 64 + 5 && hit.spawn_param1 == 1);
    hit = aos_enemy_update(&spawner, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, scripted);
    assert(hit.spawn && spawner.spawned_count == 2);
    hit = aos_enemy_update(&spawner, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(!hit.spawn);
    /* Soma at screen X 0x89 or facing left between 0x68 and 0x88: X -16. */
    spawner.spawned_count = 0;
    soma.x = AOS_FIXED(0x78);
    soma.facing_left = true;
    hit = aos_enemy_update(&spawner, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(hit.spawn && hit.spawn_x == -16);

    /* A head created at the right edge faces the player (left): vx -0.75 -
     * (r & 0x3FFF), + 0x54 0xC00; vy swings, flipping every + 0x22 frames;
     * rising it plays animation 2. */
    aos_random_seed(0);
    AosEnemy head;
    assert(aos_enemy_create(&head, AOS_ENEMY_MEDUSA_HEAD, 200, 100, 0, 1, &soma, &layer, &kind,
                            &stats));
    assert(head.state == 0 && !head.mirrored && head.vx <= -0xC000 && head.vx > -0x10000);
    assert(head.ay == 0xC00 && head.shot >= 45 && head.shot <= 60 &&
           head.volley_count == head.shot / 2);
    int32_t x0 = head.x;
    bool flipped = false;
    int8_t start = head.volley_count;
    bool down = head.dropped;
    for (int i = 0; i < 40; ++i) {
        aos_enemy_update(&head, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        if (head.dropped != down) flipped = true;
    }
    assert(flipped == (start + 40 >= (int8_t)head.shot) && head.x == x0 + 40 * head.vx);
    if (head.vy < 0) assert(head.anim.id == 2);
    else assert(head.anim.id != 2);

    /* Beyond the screen margins (entry 4) it dies at once. */
    AosEnemy far = head;
    aos_enemy_update(&far, &soma, &layer, &kind, NULL, NULL, 10, 4, 1000, 0, always_zero);
    assert(far.removed);

    /* A blow it survives stuns it 8 frames; the killing blow plays the
     * death animation 3 and the 40-frame generic death. */
    soma = aos_soma_spawn(head.x, head.y + AOS_FIXED(20), NULL);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-10, -40, 20, 40, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    head.hp = 30;
    AosCombat unhit = head.combat;
    hit = aos_enemy_update(&head, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, always_zero);
    assert(hit.enemy_hit && !hit.killed && head.hit_flash == 8);
    int32_t frozen = head.x;
    aos_enemy_update(&head, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(head.x == frozen && head.hit_flash == 7);
    head.combat = unhit;
    head.hit_flash = 0;
    head.hp = 1;
    hit = aos_enemy_update(&head, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, always_zero);
    assert(hit.killed && head.state == 2 && head.anim.id == 3 && head.timer == 0x28);
    int dying = 0;
    while (!head.removed && dying < 100) {
        aos_enemy_update(&head, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++dying;
    }
    assert(dying == 0x28);
}

static void needles_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    static const uint8_t spin[] = {4, 4};
    const AosAnimDef defs[1] = {{2, spin}};
    const AosAnimSet anims = {defs, 1};
    static AosEnemyKind kind;
    kind.anims = &anims;
    const AosEnemyStats stats = {1, 38, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(300), AOS_FIXED(159), NULL);

    /* EnemyNeedlesCreate: state 1, not mirrored, (-8, -8, 16, 16) boxes. */
    AosEnemy n;
    assert(aos_enemy_create(&n, AOS_ENEMY_NEEDLES, 100, 150, 0, 0, &soma, &layer, &kind, &stats));
    assert(n.state == 1 && !n.mirrored && n.own_boxes && n.own_attack.w == 16 &&
           n.own_hurt.x == -8);
    /* sub_080CB5E0: the first update pauses the animation for random() &
     * 0x7F frames (1 here), then a cycle of 8 ticks plays and it pauses
     * again. */
    script = (const uint32_t[]){1, 0x82, 0};
    script_at = 0;
    aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, scripted);
    assert(n.substep == 1 && n.timer == 1 && (n.anim.flags & 2));
    aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, scripted);
    aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, scripted);
    assert(n.substep == 0 && !(n.anim.flags & 2));
    int frames = 0;
    while (n.substep == 0 && frames < 50) {
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, scripted);
        ++frames;
    }
    assert(n.substep == 1 && n.timer == 2 && frames == 8 && n.y == AOS_FIXED(150));

    /* Parameter 0 set: it bobs, y += sin(phase + 0x4000) / 4. */
    AosEnemy bob;
    assert(aos_enemy_create(&bob, AOS_ENEMY_NEEDLES, 100, 150, 1, 0, &soma, &layer, &kind,
                            &stats));
    bob.phase = 0;
    aos_enemy_update(&bob, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(bob.y == AOS_FIXED(150) + 0x4000 && bob.phase == 0x400);

    /* A contact costs 38 against defence 4. */
    soma.x = n.x;
    soma.y = n.y + AOS_FIXED(10);
    AosHitReport hit = aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                        always_zero);
    assert(hit.soma_hit && hit.soma_damage == aos_player_damage(38, 4));

    /* One hit point: the weapon shatters it into five pieces from
     * 0x08528B44, then it waits for them (+ 0x13). */
    soma = aos_soma_spawn(n.x, n.y + AOS_FIXED(10), NULL);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-10, -40, 20, 40, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    hit = aos_enemy_update(&n, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, always_zero);
    assert(hit.killed && n.state == 2);
    aos_audio_clear();
    hit = aos_enemy_update(&n, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, always_zero);
    assert(n.hidden && !n.removed && hit.child_count == 5 && n.spawned_count == 5);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x70) == 1);     /* sub_080CB894 */
    const AosEnemy *p = &hit.children[0];
    assert(p->role == AOS_ROLE_NEEDLE_PIECE && p->static_frame == 5 && p->x == n.x - 0x40000);
    assert(p->y == n.y - 0x40000 && p->vx == -0x8000 && p->vy == -0x30000 && p->timer == 60);
    /* sub_080CB7C8: the second piece (30 frames) falls with vy += 0x3000
     * (camera 100 pixels down: it stays above screen Y 0xAA). */
    AosEnemy piece = hit.children[1];
    int life = 0;
    while (!piece.removed && life < 100) {
        aos_enemy_update(&piece, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 100, always_zero);
        ++life;
    }
    assert(life == 31 && piece.vy == -0x18000 + 30 * 0x3000);
    /* Below screen Y 0xAA a piece goes at once. */
    piece = hit.children[1];
    aos_enemy_update(&piece, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, -30, always_zero);
    assert(piece.removed);
    n.spawned_count = 0;
    aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.removed);
}

/* Type-5 soul attacks reach the enemy hit path after the weapon: damage
 * from their own power and elements, the defeated flag at 0 HP, and no
 * hit when the box misses. */
static void soul_attack_tests(void) {
    static const uint8_t ticks[] = {2, 2};
    const AosAnimDef defs[4] = {{2, ticks}, {2, ticks}, {2, ticks}, {2, ticks}};
    const AosAnimSet anims = {defs, 4};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    for (int a = 0; a < 4; ++a)
        for (int f = 0; f < 2; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -16, 16, 16};
        }
    const AosEnemyStats stats = {30, 16, 2, 0, 0, 0};
    /* A floor under the enemies (cell rows 13 and below: y 104). */
    memset(cells, 0, sizeof cells);
    for (int y = 13; y < H; ++y)
        for (int x = 0; x < W; ++x) cells[y * W + x] = 0x03;
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_SKELETON, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    /* A ring over the enemy (hurt rectangle 92..107 x 84..99). */
    AosSoulAttack ring = {{90, 80, 110, 104}, 26, 0, false, NULL, AOS_TYPE_SOUL};
    aos_enemy_set_soul_attacks(&ring, 1);
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                        never_zero);
    int expected = aos_enemy_damage(26, stats.defence, 0, 0, 0);
    assert(hit.enemy_hit && hit.enemy_damage == expected && e.hp == 30 - expected);
    assert(hit.has_damage_popup && hit.damage_popup.value == expected &&
           hit.damage_popup.x == e.x && hit.damage_popup.y == e.y);
    assert(hit.effect_count == 1 && hit.effects[0].kind == AOS_EFFECT_IMPACT);
    assert((hit.effects[0].x >> 16) >= 90 && (hit.effects[0].x >> 16) <= 110);
    assert((hit.effects[0].y >> 16) >= 80 && (hit.effects[0].y >> 16) <= 104);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xA4));
    /* The skill learns it struck (its hit callback). */
    assert(ring.struck && ring.target == &e);
    /* The weapon's ATK (10) is not used for it. */
    assert(expected != aos_enemy_damage(10, stats.defence, 0, 0, 0));
    /* Elements meet the record's weakness. */
    AosEnemy weak;
    const AosEnemyStats weak_stats = {30, 16, 2, 0x0004, 0, 0};
    assert(aos_enemy_create(&weak, AOS_ENEMY_SKELETON, 100, 100, 0, 0, &soma, &layer, &kind,
                            &weak_stats));
    AosSoulAttack fire = {{90, 80, 110, 104}, 26, 0x0004, false, NULL, AOS_TYPE_SOUL};
    aos_enemy_set_soul_attacks(&fire, 1);
    hit = aos_enemy_update(&weak, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(hit.enemy_damage == aos_enemy_damage(26, 2, 0x0004, 0, 0x0004) &&
           hit.enemy_damage > expected);
    assert(hit.effect_count == 1 && hit.effects[0].kind == AOS_EFFECT_HIT_ELEMENT_3);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xA5));
    /* A normal physical element against a non-weak target selects the
     * shared blood art and its table sound, as a knife hit on a Zombie. */
    AosEnemy blood_target;
    assert(aos_enemy_create(&blood_target, AOS_ENEMY_SKELETON, 100, 100, 0, 0, &soma, &layer,
                            &kind, &stats));
    AosSoulAttack physical = {{90, 80, 110, 104}, 10, 0x0001, false, NULL, AOS_TYPE_SOUL};
    aos_enemy_set_soul_attacks(&physical, 1);
    hit = aos_enemy_update(&blood_target, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           never_zero);
    assert(hit.effect_count == 1 && hit.effects[0].kind == AOS_EFFECT_BLOOD);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x9E));
    /* A box beside the enemy does nothing. */
    AosEnemy apart;
    assert(aos_enemy_create(&apart, AOS_ENEMY_SKELETON, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    AosSoulAttack beside = {{130, 80, 150, 104}, 26, 0, false, NULL, AOS_TYPE_SOUL};
    aos_enemy_set_soul_attacks(&beside, 1);
    hit = aos_enemy_update(&apart, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(!hit.enemy_hit && apart.hp == 30 && !beside.struck);
    /* Enough power kills it. */
    AosSoulAttack strong = {{90, 80, 110, 104}, 200, 0, false, NULL, AOS_TYPE_SOUL};
    aos_enemy_set_soul_attacks(&strong, 1);
    hit = aos_enemy_update(&apart, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(hit.killed && apart.defeated);
    aos_enemy_set_soul_attacks(NULL, 0);
    memset(cells, 0, sizeof cells);
}

int main(void) {
    peeping_eye_tests();
    skeleton_bone_tests();
    soul_attack_tests();
    death_sound_tests();
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
    const AosEnemyStats stats = {10, 6, 0, 0x0001, 0, 0};

    /* The bat climbs from its record to the ceiling and hangs there. */
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(159), NULL);
    AosEnemy bat;
    assert(!aos_enemy_create(&bat, 0x15, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
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
    assert(hit.effect_count == 1 && hit.effects[0].kind == AOS_EFFECT_IMPACT);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xA4));
    assert(soma.flags & AOS_FLAG_KICK_HIT);
    int dying = 0;
    while (!bat.removed && dying < 100) {
        aos_enemy_update(&bat, &soma, &layer, &kind, &weapon, &blade, 10, 0, 0, 0, never_zero);
        ++dying;
    }
    assert(dying == 64 && bat.vflip);

    /* sub_0806BE74(bat, 15, 8.0, 6.0): flames at timer 60, 45, 30 and 15,
     * at x + rand % 8.0 - 4.0 and y - 6 + rand % 6.0 - 3.0, mirrored by a
     * third draw. */
    assert(aos_enemy_create(&bat, 0x00, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    bat.y = AOS_FIXED(170);
    aos_enemy_update(&bat, &soma, &layer, &kind, &weapon, &blade, 10, 0, 0, 0, never_zero);
    assert(bat.state == 3);
    static const uint32_t draws[] = {0x80005, 0x60003, 3, 0x7FFFF, 0, 2, 1, 2, 3, 4, 5, 6};
    script = draws;
    script_at = 0;
    int flame_frames[4], effect_count = 0;
    AosEffect first = {0}, second = {0};
    dying = 0;
    while (!bat.removed && dying < 100) {
        int32_t by = bat.y;
        hit = aos_enemy_update(&bat, &soma, &layer, &kind, &weapon, &blade, 10, 0, 0, 0, scripted);
        if (hit.effect_count) {
            assert(hit.effect_count == 1 && effect_count < 4);
            if (effect_count == 0) {
                first = hit.effects[0];
                assert(first.y == by + 0x8000 - 0x60000 + 3 - 0x30000);
            }
            if (effect_count == 1) second = hit.effects[0];
            flame_frames[effect_count++] = dying;
        }
        ++dying;
    }
    assert(effect_count == 4 && script_at == 12);
    assert(flame_frames[0] == 4 && flame_frames[1] == 19 && flame_frames[3] == 49);
    assert(first.x == bat.x + 5 - 0x40000 && first.mirrored);
    assert(second.x == bat.x + 0x7FFFF - 0x40000 && !second.mirrored);

    /* sub_0806E524: animation 0 played once, rising 0.5 per frame after
     * the first, deleted on the frame the animation ends. */
    static const uint8_t puff[] = {3, 4, 6, 4, 3};
    const AosAnimDef puff_def = {5, puff};
    const AosAnimSet puff_set = {&puff_def, 1};
    AosEffect flame = {.x = AOS_FIXED(50), .y = AOS_FIXED(50)};
    int lifetime = 0;
    while (aos_effect_update(&flame, &puff_set, NULL, NULL) && lifetime < 100) ++lifetime;
    assert(lifetime == 19 && flame.y == AOS_FIXED(50) - 19 * 0x8000);
    /* sub_08068F78: + 0x18 = 7 keeps 7/8 of each speed before moving and
     * blinks from 0x08118CE0[0x28 - t]; deleted on the update after the
     * animation ends. */
    static uint8_t pattern[40];
    pattern[0x27] = 1;
    AosEffect smoke = {.kind = AOS_EFFECT_SMOKE, .particle = true, .flags = 7,
                       .x = AOS_FIXED(50), .y = AOS_FIXED(50), .vx = 0x10000, .vy = -0x8000};
    assert(aos_effect_update(&smoke, &puff_set, pattern, NULL));
    assert(smoke.vx == 0xE000 && smoke.vy == -0x7000 && smoke.x == AOS_FIXED(50) + 0xE000);
    assert(smoke.timer == 1 && smoke.hidden);
    assert(aos_effect_update(&smoke, &puff_set, pattern, NULL) && smoke.vx == 0xC400 && !smoke.hidden);
    lifetime = 2;
    while (aos_effect_update(&smoke, &puff_set, pattern, NULL) && lifetime < 100) ++lifetime;
    /* 20 ticks: two steps on the creation frame, ended on call 19. */
    assert(lifetime == 19);
    AosEffect bare = {0};
    const AosAnimSet empty = {NULL, 0};
    assert(!aos_effect_update(&bare, &empty, NULL, NULL));

    /* Under gEwramData + 0x8E bit 0x40 the bat stays in state 4: no
     * ceiling search, animation 1, no collision pass. */
    aos_enemy_set_flag_8e_40(true);
    soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(60), NULL);
    assert(aos_enemy_create(&bat, 0x00, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(bat.state == 4 && (bat.y >> 16) == 100);
    bat.y = AOS_FIXED(40);
    for (int i = 0; i < 5; ++i) {
        hit = aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        assert(!hit.soma_hit && bat.state == 4 && bat.anim.id == 1 && bat.step == 1);
    }

    /* Damage formulas. */
    assert(aos_enemy_damage(10, 0, 0x0001, 0, 0x0001) == 12);   /* weak, element 1 */
    assert(aos_enemy_damage(10, 0, 0x0002, 0, 0x0002) == 20);   /* weak, other */
    assert(aos_enemy_damage(10, 0, 0, 0x0001, 0x0001) == 5);    /* resisted */
    assert(aos_enemy_damage(10, 4, 0, 0, 0x0001) == 7);         /* (10 - 2) * 252 >> 8 */
    assert(aos_enemy_damage(1, 8, 0, 0, 1) == 1);
    assert(aos_player_damage(6, 10) == 2 && aos_player_damage(1, 50) == 1);
    early_enemy_batch_tests();
    batch_enemy_tests();
    zombie_tests();
    crow_tests();
    red_crow_tests();
    mimic_tests();
    crusader_tests();
    skull_tests();
    beam_tests();
    killer_doll_tests();
    nemesis_tests();
    doll_tests();
    soldier_tests();
    armor_tests();
    rock_armor_tests();
    buer_tests();
    white_dragon_tests();
    giant_ghost_tests();
    bone_pillar_tests();
    killer_fish_tests();
    archer_tests();
    needles_tests();
    officer_tests();
    gargoyle_tests();
    winged_skeleton_tests();
    ghost_tests();
    knight_tests();
    warrior_tests();
    flea_tests();
    valkyrie_tests();
    curly_tests();
    medusa_tests();
    puts("aos_enemy: ok");
    return 0;
}
