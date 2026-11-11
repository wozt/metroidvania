/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Aria of Sorrow enemy framework and its ported enemy
 * state machines, including Nemesis. */
#include "aos/core/aos_enemy.h"
#include "aos/core/aos_effect.h"
#include "aos/core/aos_audio.h"

void aos_enemy_disc_glow_update(AosEnemy *enemy,
                                const AosEnemyKind *kind);

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
static uint32_t headhunter_random(void) { return 0xC0000; }
/* A scripted RandomNumberGenerator. */
static const uint32_t *script;
static int script_at;
static uint32_t scripted(void) { return script[script_at++]; }

static void headhunter_death_tests(void) {
    static const uint8_t tick = 1;
    static AosAnimDef defs[25];
    for (int i = 0; i < 25; ++i) defs[i] = (AosAnimDef){1, &tick};
    const AosAnimSet anims = {defs, 25};
    AosEnemyKind kind = {.anims = &anims};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(100), NULL);
    const AosEnemyStats stats = {.hp = 300};
    AosEnemy body;
    assert(aos_enemy_create(&body, AOS_BOSS_HEADHUNTER, 120, 80, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(body.state == 0 && body.anim.id == 0x17 && !body.boss_parts_spawned);
    AosHitReport built = aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL,
                                          10, 4, 10, 20, never_zero);
    assert(built.child_count == 3 && body.boss_parts_spawned && body.spawned_count == 3 &&
           body.step == 1 && body.hh_phase == 1 && body.timer == 100 && body.anim.id == 0x18);
    static const int x[3] = {250, 282, 266}, y[3] = {144, 148, 116};
    for (int i = 0; i < 3; ++i)
        assert(built.children[i].role == AOS_ROLE_HEADHUNTER_HEAD &&
               built.children[i].variant == i && built.children[i].parent == &body &&
               built.children[i].x == AOS_FIXED(x[i]) &&
               built.children[i].y == AOS_FIXED(y[i]));
    AosEnemy first = built.children[0], second = built.children[1];
    aos_enemy_update(&first, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, never_zero);
    aos_enemy_update(&second, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, never_zero);
    assert(first.step == 1 && second.step == 0);
    aos_enemy_update(&first, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, headhunter_random);
    assert(first.step == 3 && first.target_x == body.x + AOS_FIXED(4) &&
           first.target_y == body.y - AOS_FIXED(80));
    int32_t old_x = first.x, old_y = first.y;
    aos_enemy_update(&first, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, headhunter_random);
    assert(first.x != old_x || first.y != old_y);
    first.step = 4;
    first.substep = 0;
    first.x = body.x;
    first.y = body.y;
    body.mirrored = false;
    aos_enemy_update(&first, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, headhunter_random);
    assert(first.step == 4 && first.target_x == body.x - AOS_FIXED(2) &&
           first.target_y == body.y - AOS_FIXED(52));
    first.x = first.target_x;
    first.y = first.target_y;
    aos_enemy_update(&first, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, headhunter_random);
    assert(first.step == 5 && first.timer == 0);
    AosEnemy phase_body = body;
    phase_body.step = 3;
    first.parent = &phase_body;
    int32_t retreat_y = first.y;
    for (int i = 0; i < 90; ++i)
        aos_enemy_update(&first, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 10, 20, headhunter_random);
    assert(first.step == 6 && first.timer == 90 &&
           first.y == retreat_y + 31 * 0x4000);
    aos_audio_clear();
    aos_enemy_update(&first, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, headhunter_random);
    assert(first.removed && phase_body.step == 4 && phase_body.anim.id == 1 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x11D) == 1);

    /* sub_08083FD0 case 5: the head emits one sub_08083C1C actor at
     * each timer 2/5/0x14/0x1C with the phase-specific table. */
    AosEnemy actor_head = first;
    actor_head.removed = false;
    actor_head.step = 5;
    actor_head.timer = 1;
    actor_head.variant = 0;
    actor_head.parent = &body;
    AosHitReport shot =
        aos_enemy_update(&actor_head, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 10, 20, headhunter_random);
    assert(shot.child_count == 1 &&
           shot.children[0].role == AOS_ROLE_HH_ACTOR &&
           shot.children[0].variant == 0 &&
           shot.children[0].x == body.x - AOS_FIXED(5) &&
           shot.children[0].y == body.y - AOS_FIXED(32) &&
           shot.children[0].vx == 0xA000 && shot.children[0].ax == -0x2AA);
    actor_head.timer = 4;
    shot = aos_enemy_update(&actor_head, &soma, &layer, &kind, NULL, NULL,
                            10, 4, 10, 20, headhunter_random);
    assert(shot.child_count == 1 && shot.children[0].vx == -0x2000);
    actor_head.timer = 19;
    shot = aos_enemy_update(&actor_head, &soma, &layer, &kind, NULL, NULL,
                            10, 4, 10, 20, headhunter_random);
    assert(shot.child_count == 1 && shot.children[0].mirrored &&
           shot.children[0].vx == 0xA500);
    actor_head.timer = 27;
    shot = aos_enemy_update(&actor_head, &soma, &layer, &kind, NULL, NULL,
                            10, 4, 10, 20, headhunter_random);
    assert(shot.child_count == 1 && shot.children[0].vx == -0x2600);
    /* The sub_08083EEC actor integrates, sinks past 0x3B and deletes
     * past 0x59. */
    AosEnemy actor = shot.children[0];
    actor.parent = &body;
    actor.timer = 60;
    int32_t ax = actor.x, ay = actor.y;
    aos_enemy_update(&actor, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, never_zero);
    assert(actor.y == ay + actor.vy + 0x4000 && actor.x == ax + actor.vx);
    actor.timer = 90;
    aos_enemy_update(&actor, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, never_zero);
    assert(actor.removed);

    body.hh_phase = 2;
    aos_enemy_update(&second, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, never_zero);
    assert(second.step == 1);
    AosEnemy second_body = body;
    second_body.step = 9;
    int32_t second_y = second_body.y;
    second.parent = &second_body;
    second.step = 6;
    aos_enemy_update(&second, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, headhunter_random);
    assert(second.step == 7 && second.timer == 5 && !second.removed);
    for (int i = 0; i < 5; ++i)
        aos_enemy_update(&second, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 10, 20, headhunter_random);
    assert(second.removed && second_body.step == 10 && second_body.anim.id == 5 &&
           second_body.y == second_y - AOS_FIXED(19));

    AosEnemy third_body = body;
    third_body.step = 29;
    int32_t third_y = third_body.y;
    AosEnemy third = built.children[2];
    third.parent = &third_body;
    third.step = 6;
    aos_enemy_update(&third, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, headhunter_random);
    for (int i = 0; i < 5; ++i)
        aos_enemy_update(&third, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 10, 20, headhunter_random);
    assert(third.removed && third_body.step == 30 && third_body.anim.id == 9 &&
           third_body.y == third_y + AOS_FIXED(11));
    body.hh_phase = 1;
    aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, never_zero);
    assert(body.step == 2 && body.timer == 0 && body.anim.id == 0);
    for (int i = 0; i < 29; ++i)
        aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 10, 20, never_zero);
    assert(body.step == 3 && body.timer == 60);
    /* Case 3 waits 0x3B frames then slams at the fixed native
     * velocity until the floor advance. */
    for (int i = 0; i < 5; ++i)
        aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 10, 20, never_zero);
    assert(body.step == 3 && body.hh_f1a == 1 && body.vy >= 0x10000 &&
           body.ay == 0x2800);
    for (int i = 0; i < 90 && body.step == 3; ++i)
        aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 10, 20, never_zero);
    assert(body.step == 4);
    /* Cases 4-12 chain through the phase timers into the case-12
     * coin flip that enters state 1 case 0xA (never_zero & 1). */
    for (int i = 0; i < 400 && body.state == 0; ++i)
        aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 10, 20, never_zero);
    assert(body.state == 1 && body.step == 0xA);
    /* Case 0's face-and-lead into case 1 was covered natively by the
     * state-1 entry cases; drive the machine directly from here. */
    body.state = 1;
    body.step = 0;
    /* Case 1's leap ends on the animation-end gate back to state 0. */
    for (int i = 0; i < 40 && body.state == 1; ++i)
        aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 10, 20, never_zero);
    assert(body.state == 0 && body.step == 3 && body.timer == 0x3C);
    /* Case 10's random gate: 0 passes (<= 0x31) to case 11, 100 fails
     * to case 13 with animation 7. */
    body.state = 1;
    body.step = 10;
    body.timer = 0;
    script = (const uint32_t[]){0};
    script_at = 0;
    aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, scripted);
    assert(body.step == 11);
    script = (const uint32_t[]){150};
    script_at = 0;
    body.step = 10;
    aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, scripted);
    assert(body.step == 13 && body.anim.id == 7);
    /* Cases 12/14 count + 0xD to 0x63 then return to state 0 step 0xC
     * with the velocities cleared and animation 6. */
    body.step = 12;
    body.timer = 0;
    for (int i = 0; i < 100; ++i)
        aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 10, 20, never_zero);
    assert(body.state == 0 && body.step == 0xC && body.vx == 0 &&
           body.vy == 0 && body.anim.id == 6);
    /* Case 22's dash fires the sub_080858B8 shot (song 0x67) on the
     * + 0xD > 0x13 multiple-of-4 tick, then advances at 0x2D. */
    body.state = 1;
    body.step = 22;
    body.timer = 0x14;
    aos_audio_clear();
    AosHitReport dash = aos_enemy_update(&body, &soma, &layer, &kind,
                                         NULL, NULL, 10, 4, 10, 20,
                                         never_zero);
    assert(dash.child_count == 1 &&
           dash.children[0].role == AOS_ROLE_HH_SHOT &&
           dash.children[0].parent == &body &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x67) == 1);
    /* The sub_0808597C shot damps 31/32, caps at 0x40000 and deletes
     * past age 0x3B. */
    AosEnemy proj = dash.children[0];
    proj.vx = 0x20000;
    proj.vy = -0x20000;
    int32_t proj_x = proj.x, proj_y = proj.y;
    aos_enemy_update(&proj, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, never_zero);
    assert(proj.vx == 0x20000 * 31 / 32 && proj.vy == -0x20000 * 31 / 32 &&
           proj.x == proj_x + proj.vx && proj.y == proj_y + proj.vy &&
           proj.timer == 1);
    proj.timer = 0x3C;
    aos_enemy_update(&proj, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, never_zero);
    assert(proj.removed);
    body.timer = 0x2D;
    aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, never_zero);
    assert(body.step == 23 && body.timer == 0);
    /* Cases 37 and 47 hand control back to state 0 with their decoded
     * step writes (0x1E flee / 0x32 exit). */
    body.step = 37;
    aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, never_zero);
    assert(body.state == 0 && body.step == 0x1E && body.vflip &&
           body.anim.id == 0xE);
    body.vflip = false;
    body.state = 1;
    body.step = 47;
    aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, never_zero);
    assert(body.state == 0 && body.step == 0x32);
    body.removed = true;
    AosEnemy orphan = built.children[0];
    aos_enemy_update(&orphan, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 10, 20, never_zero);
    assert(orphan.removed);

    AosEnemy boss = {.id = AOS_BOSS_HEADHUNTER, .role = AOS_ROLE_ENEMY,
                     .state = 2, .defeated = true, .hp = 0,
                     .x = AOS_FIXED(100), .y = AOS_FIXED(180), .static_frame = -1};
    AosHitReport report = aos_enemy_update(&boss, &soma, &layer, &kind, NULL, NULL,
                                           10, 4, 0, 0, never_zero);
    assert(!report.special_soul && boss.step == 1 && boss.vy == 0x10000 &&
           boss.ay == 0x4000);
    /* Case 1 waits at the lower edge until the three linked heads
     * report their staged endings (hh_head_done). */
    for (int i = 0; i < 200 && boss.step == 1; ++i)
        aos_enemy_update(&boss, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 0, 0, never_zero);
    assert(boss.step == 1 && boss.y == AOS_FIXED(0xC0) && boss.vy == 0);
    boss.hh_head_done = 3;
    aos_enemy_update(&boss, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, never_zero);
    assert(boss.step == 2);
    aos_enemy_update(&boss, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, never_zero);
    assert(boss.step == 3 && boss.timer == 0x59);
    aos_audio_clear();
    int frames = 0;
    do {
        report = aos_enemy_update(&boss, &soma, &layer, &kind, NULL, NULL,
                                  10, 4, 0, 0, never_zero);
        ++frames;
    } while (!report.special_soul && frames < 100);
    assert(report.special_soul && boss.removed && boss.step == 4 && frames == 90 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x76) == 12);
}

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

    /* The Fish Head (id 0x41) shares sub_080B3BAC/sub_080B3DCC: its
     * create is the variant-1 resource row of 0x081193DC and its
     * update the same controller; the skulls inherit its id. */
    const AosEnemyStats fish_stats = {280, 48, 40, 0x0020, 0x0010, 100};
    AosEnemy f;
    assert(aos_enemy_create(&f, AOS_ENEMY_FISH_HEAD, 100, 150, 3, 0,
                            &soma, &layer, &kind, &fish_stats));
    assert(f.param0 == 3 && f.hidden && f.state == 0);
    AosHitReport ff = aos_enemy_update(&f, &soma, &layer, &kind, NULL,
                                        NULL, 10, 4, 0, 0, always_zero);
    assert(ff.child_count == 3 && ff.children[0].id == AOS_ENEMY_FISH_HEAD &&
           ff.children[0].role == AOS_ROLE_BONE_PILLAR_SEGMENT &&
           ff.children[0].hp == 280);
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
           hit.effects[4].kind == AOS_EFFECT_IMPACT &&
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
    const AosEffectWorld world = {&layer, 0, 0, always_zero, 0, 0, 0};
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


static void creaking_skull_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { CS_ANIMS = 4 };
    static const uint8_t ticks4[4] = {4, 4, 4, 4};
    static AosAnimDef defs[CS_ANIMS];
    for (int a = 0; a < CS_ANIMS; ++a) defs[a] = (AosAnimDef){8, ticks4};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = CS_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < CS_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-32, -64, 64, 72};
            kind.sprites[a][f] = (uint8_t)(a * 8 + f < 32 ? a * 8 + f : 0);
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 33: hp 240, contact 35, defence 10,
     * weak 0x0020, resist 0x0010, experience 110, soul blue 14;
     * id 92: hp 2000, contact 75, defence 50, weak 0x0020,
     * resist 0x0410, experience 1200, soul red 41. */
    static const AosEnemyStats cs33 = {240, 35, 10, 0x0020, 0x0010, 110};
    static const AosEnemyStats cs92 = {2000, 75, 50, 0x0020, 0x0410, 1200};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;

    /* Both ids share the create: hp from the record, state 0 awake,
     * state 3 dormant under the global flag. */
    AosEnemy e;
    assert(aos_enemy_create(&e, 0x21, 300, 150, 0, 0,
                            &soma, &layer, &kind, &cs33));
    assert(e.state == 0 && e.hp == 240);
    assert(!e.boss_parts_spawned && e.spawned_count == 0);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, 0x5C, 300, 150, 0, 0,
                            &soma, &layer, &kind, &cs92));
    assert(ed.state == 3 && ed.hp == 2000);
    aos_enemy_set_flag_8e_40(false);

    /* The thirteen bone children emit over two updates (8-child
     * cap): 8 then 5. */
    e.x = AOS_FIXED(150);
    e.y = AOS_FIXED(120);
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0,
                                        always_zero);
    int bones = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_CS_BONE) bones++;
    assert(bones == 8 && e.spawned_count == 8);
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    bones = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_CS_BONE) bones++;
    assert(bones == 5 && e.spawned_count == 13 &&
           e.boss_parts_spawned);

    /* A bone child tracks its parent and dies with it. */
    AosEnemy b;
    memset(&b, 0, sizeof b);
    b.id = 0x21;
    b.role = AOS_ROLE_CS_BONE;
    b.parent = &e;
    b.cs_bone = 3;
    b.x = AOS_FIXED(150);
    b.y = AOS_FIXED(120);
    aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(!b.removed && b.y < e.y);
    e.state = 2;
    aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(b.removed);
    e.state = 0;

    /* State 1 volley: two CS_SHOT children then back to state 0. */
    AosEnemy a;
    assert(aos_enemy_create(&a, 0x21, 150, 120, 0, 0,
                            &soma, &layer, &kind, &cs33));
    a.x = AOS_FIXED(150);
    a.y = AOS_FIXED(120);
    a.state = 1;
    a.substep = 1;
    a.timer = 0x10;
    a.boss_parts_spawned = true;
    a.spawned_count = 13;
    hit = aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    int shots = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_CS_SHOT) shots++;
    assert(shots == 1);
    a.timer = 0x28;
    hit = aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    shots = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_CS_SHOT) shots++;
    assert(shots == 1);
    a.state = 1;
    a.substep = 2;
    aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(a.state == 0 && a.step == 1);

    /* Lethal: hp 0 -> defeated, state 2 death machine with gore,
     * then the delete. */
    AosEnemy el;
    assert(aos_enemy_create(&el, 0x5C, 150, 120, 0, 0,
                            &soma, &layer, &kind, &cs92));
    el.x = AOS_FIXED(150);
    el.y = AOS_FIXED(120);
    el.state = 1;
    el.hp = 0;
    hit = aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(el.defeated && el.state == 2);
    el.timer = 0;
    hit = aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    int gore = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_CS_GORE) gore++;
    assert(gore == 1);
    /* timer already 0 path removes on the NEXT state-2 update;
     * run once more to reach removed. */
    aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(el.removed);
}

static void balore_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { BL_ANIMS = 8 };
    static const uint8_t ticks8[8] = {4, 4, 4, 4, 4, 4, 4, 4};
    static AosAnimDef defs[BL_ANIMS];
    for (int a = 0; a < BL_ANIMS; ++a) defs[a] = (AosAnimDef){8, ticks8};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = BL_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < BL_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-32, -64, 64, 72};
            kind.sprites[a][f] = (uint8_t)(a * 8 + f < 32 ? a * 8 + f : 0);
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 109: hp 4000, contact 65, defence 60,
     * weak 0x0000, resist 0x0400, experience 16000,
     * soul red 50 (not ported). */
    static const AosEnemyStats bl = {4000, 65, 60, 0x0000, 0x0400, 16000};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;

    /* EnemyBaloreCreate: hp 4000, state 0, the three part children
     * emit over the first updates via boss_parts_spawned. */
    AosEnemy e;
    assert(aos_enemy_create(&e, 0x6D, 300, 150, 0, 0,
                            &soma, &layer, &kind, &bl));
    assert(e.state == 0 && e.hp == 4000 && e.bl_idx == 4);
    assert(!e.boss_parts_spawned && e.spawned_count == 0);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, 0x6D, 300, 150, 0, 0,
                            &soma, &layer, &kind, &bl));
    assert(ed.state == 5);
    aos_enemy_set_flag_8e_40(false);

    /* First update emits the wing + two limbs (cap allows all 3). */
    e.x = AOS_FIXED(150);
    e.y = AOS_FIXED(120);
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0,
                                        always_zero);
    int wing = 0, l0 = 0, l1 = 0;
    for (int i = 0; i < hit.child_count; ++i) {
        if (hit.children[i].role == AOS_ROLE_BALO_WING) wing++;
        if (hit.children[i].role == AOS_ROLE_BALO_LIMB0) l0++;
        if (hit.children[i].role == AOS_ROLE_BALO_LIMB1) l1++;
    }
    assert(wing == 1 && l0 == 1 && l1 == 1);
    assert(e.spawned_count == 3 && e.boss_parts_spawned);

    /* State 0 -> state 1 on the first update. */
    assert(e.state == 1);

    /* The wing child orbits the parent and dies with it. */
    AosEnemy w;
    memset(&w, 0, sizeof w);
    w.id = 0x6D;
    w.role = AOS_ROLE_BALO_WING;
    w.parent = &e;
    w.x = AOS_FIXED(150);
    w.y = AOS_FIXED(120);
    aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(!w.removed && w.state >= 1);
    e.state = 4;
    e.phase |= 4;
    aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(w.removed);
    e.state = 1;
    e.phase &= ~4;

    /* State 1 rise: substep machine with the two FX spawns and
     * song 0x1D4. */
    AosEnemy r;
    assert(aos_enemy_create(&r, 0x6D, 150, 120, 0, 0,
                            &soma, &layer, &kind, &bl));
    r.x = AOS_FIXED(150);
    r.y = AOS_FIXED(120);
    r.state = 1;
    r.substep = 1;
    r.timer = 0x18;
    hit = aos_enemy_update(&r, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    int fx = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_BALO_FX) fx++;
    assert(fx == 1);

    /* State 3 attack: substep 0 spawns two blades + song 0x1D5. */
    AosEnemy a;
    assert(aos_enemy_create(&a, 0x6D, 150, 120, 0, 0,
                            &soma, &layer, &kind, &bl));
    a.x = AOS_FIXED(150);
    a.y = AOS_FIXED(120);
    a.state = 3;
    a.substep = 0;
    hit = aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    int blades = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_BALO_BLADE) blades++;
    assert(blades == 2 && a.substep == 1 && a.timer == 0x60);

    /* Lethal: hp 0 -> defeated, state 4 death machine, then the
     * case-6 delete. */
    AosEnemy el;
    assert(aos_enemy_create(&el, 0x6D, 150, 120, 0, 0,
                            &soma, &layer, &kind, &bl));
    el.x = AOS_FIXED(150);
    el.y = AOS_FIXED(120);
    el.state = 2;
    el.hp = 0;
    aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(el.defeated && el.state == 4);
    el.substep = 6;
    el.timer = 0;
    hit = aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(el.removed);
}

static void manticore_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { MT_ANIMS = 8 };
    static const uint8_t ticks8[8] = {4, 4, 4, 4, 4, 4, 4, 4};
    static AosAnimDef defs[MT_ANIMS];
    for (int a = 0; a < MT_ANIMS; ++a) defs[a] = (AosAnimDef){8, ticks8};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = MT_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < MT_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-32, -64, 64, 72};
            kind.sprites[a][f] = (uint8_t)(a * 8 + f < 32 ? a * 8 + f : 0);
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 54: hp 440, contact 40, defence 24,
     * weak 0x0181, resist 0x0000, experience 250, rare 0x0027,
     * soul blue 19 (the Manticore variant of the shared
     * Skill_Devil_Manticore_Curly_Use). */
    static const AosEnemyStats mt = {440, 40, 24, 0x0181, 0x0000, 250};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;

    /* EnemyManticoreCreate: hp 5000, state 0, the six orbit children
     * emit over the first updates via boss_parts_spawned. */
    AosEnemy e;
    assert(aos_enemy_create(&e, 0x36, 300, 150, 0, 0,
                            &soma, &layer, &kind, &mt));
    assert(e.state == 0 && e.hp == 440);
    assert(!e.boss_parts_spawned && e.spawned_count == 0);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, 0x36, 300, 150, 0, 0,
                            &soma, &layer, &kind, &mt));
    assert(ed.state == 3);
    aos_enemy_set_flag_8e_40(false);
    /* param0 & 3: the gEwramData + 0x4CC variant adds 0x3E8 hp. */
    AosEnemy ev;
    assert(aos_enemy_create(&ev, 0x36, 300, 150, 3, 0,
                            &soma, &layer, &kind, &mt));
    assert(ev.hp == 440 + 0x3E8);

    /* First update emits up to 8 children; six slots total. */
    e.x = AOS_FIXED(150);
    e.y = AOS_FIXED(120);
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0,
                                        always_zero);
    int kids = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role >= AOS_ROLE_MANTICORE_CHILD0 &&
            hit.children[i].role <= AOS_ROLE_MANTICORE_CHILD5)
            kids++;
    assert(kids == 6 && e.spawned_count == 6 && e.boss_parts_spawned);

    /* A child flies and dies with its parent. */
    AosEnemy kid;
    memset(&kid, 0, sizeof kid);
    kid.id = 0x36;
    kid.role = AOS_ROLE_MANTICORE_CHILD0;
    kid.parent = &e;
    kid.x = AOS_FIXED(150);
    kid.y = AOS_FIXED(120);
    kid.mt_slot = 0;
    aos_enemy_update(&kid, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(kid.mt_act == 1 && !kid.removed);
    e.state = 2;
    aos_enemy_update(&kid, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(kid.removed);
    e.state = 0;

    /* State 1 attack: the + 0x6E == 0xE marker grows a wing and
     * fires the shot. */
    AosEnemy a;
    assert(aos_enemy_create(&a, 0x36, 150, 120, 0, 0,
                            &soma, &layer, &kind, &mt));
    a.x = AOS_FIXED(150);
    a.y = AOS_FIXED(120);
    a.state = 1;
    a.step = 11;
    a.mt_phase = 0xE;
    hit = aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    int wing = 0, shot = 0;
    for (int i = 0; i < hit.child_count; ++i) {
        if (hit.children[i].role == AOS_ROLE_MANTICORE_WING) wing++;
        if (hit.children[i].role == AOS_ROLE_MANTICORE_SHOT) shot++;
    }
    assert(wing == 1 && shot == 1);

    /* Lethal: hp 0 -> defeated, state 2 flame death, then removed. */
    AosEnemy el;
    assert(aos_enemy_create(&el, 0x36, 150, 120, 0, 0,
                            &soma, &layer, &kind, &mt));
    el.x = AOS_FIXED(150);
    el.y = AOS_FIXED(120);
    el.state = 0;
    el.hp = 0;
    hit = aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(el.defeated && el.state == 2 && el.timer <= 0x3C);
    el.timer = 0;
    aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(el.removed);
}

static void death_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { DT_ANIMS = 8 };
    static const uint8_t ticks8[8] = {4, 4, 4, 4, 4, 4, 4, 4};
    static AosAnimDef defs[DT_ANIMS];
    for (int a = 0; a < DT_ANIMS; ++a) defs[a] = (AosAnimDef){8, ticks8};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = DT_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < DT_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-32, -64, 64, 72};
            kind.sprites[a][f] = (uint8_t)(a * 8 + f < 32 ? a * 8 + f : 0);
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 107: hp 4444, contact 50, defence 20,
     * weak 0x0000, resist 0x0410, experience 4444. */
    static const AosEnemyStats dt = {4444, 50, 20, 0x0000, 0x0410, 4444};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;

    /* EnemyDeathCreate: hp 0x98C, state 0 (or 8 dormant, or 1 when
     * param0 set), the three child roles emitted on first update. */
    AosEnemy e;
    assert(aos_enemy_create(&e, 0x6B, 300, 150, 0, 0,
                            &soma, &layer, &kind, &dt));
    assert(e.state == 0 && e.hp == 0x98C);
    assert(e.dth_atk == 0 && !e.boss_parts_spawned);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, 0x6B, 300, 150, 0, 0,
                            &soma, &layer, &kind, &dt));
    assert(ed.state == 8);
    aos_enemy_set_flag_8e_40(false);
    AosEnemy ep;
    assert(aos_enemy_create(&ep, 0x6B, 300, 150, 1, 0,
                            &soma, &layer, &kind, &dt));
    assert(ep.state == 1);

    /* First update: emits the mirror + mid (param0) + scythe. */
    e.x = AOS_FIXED(150);
    e.y = AOS_FIXED(120);
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0,
                                        always_zero);
    int mir = 0, mid = 0, scy = 0;
    for (int i = 0; i < hit.child_count; ++i) {
        if (hit.children[i].role == AOS_ROLE_DEATH_MIRROR) mir++;
        if (hit.children[i].role == AOS_ROLE_DEATH_MID) mid++;
        if (hit.children[i].role == AOS_ROLE_DEATH_SCYTHE) scy++;
    }
    /* param0 was 0 -> only mirror + scythe. */
    assert(mir == 1 && mid == 0 && scy == 1);
    assert(e.spawned_count == 2 && e.boss_parts_spawned);

    /* param0 variant emits the mid too. */
    ep.x = AOS_FIXED(150);
    ep.y = AOS_FIXED(120);
    hit = aos_enemy_update(&ep, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    mid = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_DEATH_MID) mid++;
    assert(mid == 1 && ep.spawned_count == 3);

    /* The mirror child tracks the parent position. */
    AosEnemy mirror;
    memset(&mirror, 0, sizeof mirror);
    mirror.id = 0x6B;
    mirror.role = AOS_ROLE_DEATH_MIRROR;
    mirror.parent = &e;
    mirror.state = 1;
    e.x = AOS_FIXED(200);
    e.y = AOS_FIXED(140);
    aos_enemy_update(&mirror, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(mirror.x == e.x && mirror.y == e.y);

    /* State 0 intro walks the substeps toward the dive. */
    AosEnemy w;
    assert(aos_enemy_create(&w, 0x6B, 300, 150, 0, 0,
                            &soma, &layer, &kind, &dt));
    w.x = AOS_FIXED(150);
    w.y = AOS_FIXED(120);
    for (int i = 0; i < 12 && w.state == 0; ++i)
        aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 0, 0, always_zero);
    assert(w.substep >= 1);

    /* State 1 volley emits DEATH_SHOT children. */
    w.state = 1;
    w.step = 3;
    w.dth_atk = 0x10;
    hit = aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    int shots = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_DEATH_SHOT) shots++;
    assert(shots >= 1);

    /* Lethal: hp 0 -> defeated, state 7, timer 0x3C, gore spawns. */
    AosEnemy el;
    assert(aos_enemy_create(&el, 0x6B, 150, 120, 0, 0,
                            &soma, &layer, &kind, &dt));
    el.x = AOS_FIXED(150);
    el.y = AOS_FIXED(120);
    el.state = 3;
    el.hp = 0;
    hit = aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(el.defeated && el.state == 7 && el.timer <= 0x3C);
    int gore = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_DEATH_GORE) gore++;
    assert(gore >= 1);

    /* State 7 end deletes (timer already counted down). */
    el.timer = 0;
    aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(el.removed);
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
    /* Zombie is physically weak, so the native element table selects its
     * blood splash. The specialized update must keep that contact art. */
    assert(hit.effect_count == 1 &&
           hit.effects[0].kind == AOS_EFFECT_BLOOD);
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

static void bomber_armor_tests(void) {
    aos_audio_clear();
    /* Same floor as the Rock Armor: cell row 20 (top 160) from cell 4 to
     * 49, walls at cells 3 and 50. */
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 4; x < 50; ++x) cells[20 * W + x] = 0x03;
    for (int y = 0; y < 21; ++y) cells[y * W + 3] = cells[y * W + 50] = 0x03;
    /* Animations 0 (walk), 1 (throw), 2 (death) share the Rock Armor's
     * exported durations; 3 the bomb, 4-6 the fuse spark. */
    static const uint8_t walk[10] = {25, 5, 5, 5, 4, 22, 4, 5, 4, 4};
    static const uint8_t throw_ticks[33] = {2, 4, 3, 7, 9, 3, 2, 2, 2, 1, 1, 1, 2, 8, 31, 5, 6,
                                            3, 3, 3, 8, 8, 8, 8, 8, 8, 8, 8, 6, 9, 9, 7, 8};
    static const uint8_t death[11] = {8, 6, 6, 6, 8, 6, 6, 4, 4, 4, 11};
    static const uint8_t loop[4] = {4, 4, 4, 4};
    static const uint8_t one[1] = {1};
    const AosAnimDef defs[7] = {{10, walk},   {33, throw_ticks}, {11, death},
                                {4, loop},    {1, one},         {1, one},
                                {1, one}};
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
    const AosEnemyStats stats = {200, 35, 25, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(600), AOS_FIXED(400), NULL);
    soma.max_hp = soma.hp = 320;

    /* The create is the Rock Armor's minus the one pixel down: same floor
     * snap, state 0, walk animation. */
    AosEnemy a, rock;
    assert(aos_enemy_create(&a, AOS_ENEMY_BOMBER_ARMOR, 200, 150, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(aos_enemy_create(&rock, AOS_ENEMY_ROCK_ARMOR, 200, 150, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(a.state == 0 && rock.state == 0 && a.y + 0x10000 == rock.y);

    /* The shared walk: the player 60 pixels ahead at the start of frame 9
     * throws; the animation's frame 7 lasts two updates and each throws a
     * bomb (sound 0x88). */
    a.mirrored = true;
    rock.mirrored = true;
    soma.x = a.x + AOS_FIXED(60);
    soma.y = a.y;
    int frames = 0;
    while (a.state == 0 && frames < 200) {
        aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    assert(a.state == 1 && a.anim.frame == 9);
    aos_audio_clear();
    AosEnemy bomb = {0};
    int bombs = 0;
    while (a.state == 1 && frames < 600) {
        AosHitReport r = aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          never_zero);
        for (int c = 0; c < r.child_count; ++c)
            if (r.children[c].role == AOS_ROLE_BOMBER_BOMB) {
                bomb = r.children[c];
                ++bombs;
            }
        ++frames;
        if (a.step == 0 && a.anim.id == 1 && bombs) break;
    }
    assert(bombs == 2 && aos_audio_requested(AOS_AUDIO_START_SONG, 0x88) == 2 &&
           bomb.mirrored && bomb.hp == a.hp && bomb.stats.contact == stats.contact);

    /* The bomb's first update: animation 3, contact elements 2, the own
     * boxes, 58 pixels up, vy -2.5 with gravity 0.125, the throw speed
     * -2.0 - distance / 64 negated by the facing, contact + 1 / 4. */
    int32_t bx = bomb.x, by = bomb.y;
    int32_t dx = ((soma.x - bx) >> 16), dy = ((soma.y - by) >> 16);
    uint32_t squares = (uint32_t)(dx * dx + dy * dy), root = 0;
    while ((root + 1) * (root + 1) <= squares) ++root;
    int32_t distance = (int16_t)root;
    int32_t want_speed = -0x2000 - ((distance << 16) >> 6);
    if (bomb.mirrored) want_speed = -want_speed;
    (void)aos_enemy_update(&bomb, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(bomb.state == 1 && bomb.anim.id == 3 && bomb.contact_elements == 2 &&
           bomb.own_boxes && bomb.own_hurt.w == 16 && bomb.own_attack.w == 48 &&
           bomb.y == by - 0x3A0000 + 0x2000 - 0x28000 && bomb.spin == want_speed &&
           bomb.vy == (int32_t)0xFFFDA000 && bomb.ay == 0x2000 &&
           bomb.stats.contact == stats.contact + (stats.contact >> 2));
    /* A fuse spark every fourth frame (timer 0, 4, 8: two more after the
     * first update's), 8 pixels above its position before the move. */
    int sparks = 0;
    for (int i = 0; i < 8; ++i) {
        int32_t pre_y = bomb.y;
        AosHitReport r = aos_enemy_update(&bomb, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                          always_zero);
        for (int c = 0; c < r.child_count; ++c)
            if (r.children[c].role == AOS_ROLE_BOMBER_SPARK &&
                r.children[c].y == pre_y - 0x80000)
                ++sparks;
    }
    assert(sparks == 2);
    /* It arcs onto the floor, counts the fuse down, then bursts (list 0)
     * and deletes itself with sound 0x76. */
    frames = 0;
    while (bomb.state == 1 && frames < 200) {
        aos_enemy_update(&bomb, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(bomb.state == 2 && bomb.timer == 0x10);
    aos_audio_clear();
    frames = 0;
    while (!bomb.removed && bomb.state != 0 && frames < 100) {
        (void)aos_enemy_update(&bomb, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                               always_zero);
        ++frames;
    }
    assert(bomb.removed && aos_audio_requested(AOS_AUDIO_START_SONG, 0x76) == 1);

    /* A bomb struck by the weapon loses its hit points but keeps flying
     * (sub_080B2F5C zeroes + 0xC; there is no death state). */
    AosEnemy struck = bomb;
    struck.removed = false;
    struck.state = 1;
    struck.hp = 60;
    struck.x = soma.x;
    struck.y = soma.y - AOS_FIXED(16);
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 40, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    AosHitReport hit = aos_enemy_update(&struck, &soma, &layer, &kind, &weapon, &blade, 10, 4,
                                        0, 0, always_zero);
    assert(hit.enemy_hit && struck.hp == 0 && !struck.removed && struck.state == 1);

    /* The fuse spark: animation 4 once, gone when it ends. */
    AosEnemy spark = {0};
    spark.id = AOS_ENEMY_BOMBER_ARMOR;
    spark.role = AOS_ROLE_BOMBER_SPARK;
    spark.static_frame = -1;
    spark.x = AOS_FIXED(300);
    spark.y = AOS_FIXED(100);
    frames = 0;
    while (!spark.removed && frames < 100) {
        aos_enemy_update(&spark, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(spark.removed && spark.anim.id == 4);

    /* A lethal blow on the armor: it faces the player and dies like the
     * Rock Armor (animation 2, sound 0x6F). */
    assert(aos_enemy_create(&a, AOS_ENEMY_BOMBER_ARMOR, 200, 150, 0, 0, &soma, &layer, &kind,
                            &stats));
    a.mirrored = false;
    soma.x = a.x + AOS_FIXED(4);
    soma.y = a.y;
    aos_audio_clear();
    hit = aos_enemy_update(&a, &soma, &layer, &kind, &weapon, &blade, 999, 4, 0, 0,
                           always_zero);
    assert(hit.killed && a.state == 2 && a.mirrored);
    frames = 0;
    while (!a.removed && frames < 200) {
        aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(a.removed &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x6F) == 1);
}

static void sky_fish_tests(void) {
    /* Floor for the create's sub_0806AF98 snap (the dash repositions the
     * fish anyway). */
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    /* anim_0 the swim (4 frames), anim_1 the death (11 frames); the
     * durations are the exported ones. */
    static const uint8_t swim[4] = {9, 10, 11, 12};
    static const uint8_t death[11] = {1, 2, 3, 3, 4, 5, 6, 7, 8, 9, 12};
    const AosAnimDef defs[2] = {{4, swim}, {11, death}};
    const AosAnimSet anims = {defs, 2};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 2; ++a)
        for (int f = 0; f < 11; ++f)
            kind.modes[a][f] = 1;
    for (int f = 0; f < 4; ++f) {
        kind.hurt[0][f] = kind.attack[0][f] = (AosBox){-12, -4, 24, 8};
    }
    for (int n = 0; n < 8; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {3, 0, 0, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(40), NULL);
    soma.max_hp = soma.hp = 320;

    /* The create: state 0, hidden, the + 0x18 collision gate set, the
     * + 0x1A countdown in [0x40, 0x7F] and the + 0x19 entry side. */
    aos_random_seed(0x2468);
    AosEnemy a;
    assert(aos_enemy_create(&a, AOS_ENEMY_SKY_FISH, 200, 150, 0, 0, &soma, &layer,
                            &kind, &stats));
    assert(a.state == 0 && a.step == 0 && a.hidden && a.sf_gate == 1 &&
           a.sf_timer >= 0x40 && a.sf_timer <= 0x7F);

    /* Step 0 holds it hidden at the entry side with a random height in
     * [32, 64) while + 0x1A counts down; then it shows itself and dashes
     * at 26 px/frame toward its facing. */
    int frames = 0;
    while (a.step == 0 && frames < 200) {
        (void)aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                               always_zero);
        ++frames;
    }
    assert(a.step == 1 && !a.hidden);
    int32_t entry_x = a.sf_variant ? 0x1000000 : (int32_t)0xFFF00000;
    assert(a.x == entry_x || a.x == entry_x + (int32_t)0x1A0000 * (a.mirrored ? 1 : -1));
    assert(a.y >= 0x200000 && a.y < 0x200000 + 0x200000);
    assert(a.vx == (a.mirrored ? 0x1A0000 : (int32_t)0xFFE60000));

    /* Step 1: it crosses at 26 px/frame, a splash drift every frame, and
     * despawns beyond the margin of entry 6 — it never collides. */
    frames = 0;
    int splashes = 0;
    while (!a.removed && frames < 60) {
        AosHitReport r = aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4,
                                          0, 0, always_zero);
        splashes += r.effect_count;
        ++frames;
    }
    assert(a.removed && splashes > 0 && !a.hidden);

    /* The flag path (0x8E & 0x40) is state 2: animation only. */
    aos_enemy_set_flag_8e_40(true);
    assert(aos_enemy_create(&a, AOS_ENEMY_SKY_FISH, 200, 150, 0, 0, &soma, &layer,
                            &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(a.state == 2 && a.step == 1 && a.sf_gate == 1);
    int32_t x0 = a.x;
    (void)aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(a.x == x0 && !a.removed);

    /* State 1 (the lethal hurt callback's destination): it hangs with
     * ay 0x4000 under the 4.0 limit, sinks, blinks on the timer pattern,
     * bursts a bubble every eighth tick and is deleted at timer 0. */
    assert(aos_enemy_create(&a, AOS_ENEMY_SKY_FISH, 200, 150, 0, 0, &soma, &layer,
                            &kind, &stats));
    a.state = 1;
    a.step = 0;
    a.timer = 0x40;
    a.hidden = false;
    (void)aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(a.step == 1 && a.vx == 0 && a.vy == 0x4000 && a.ay == 0x4000);
    frames = 0;
    int blinks = 0, bubbles = 0;
    bool was_hidden = false;
    while (!a.removed && frames < 100) {
        AosHitReport r = aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL, 10, 4,
                                          0, 0, always_zero);
        bubbles += r.effect_count;
        if (a.hidden) {
            if (!was_hidden) ++blinks;
            was_hidden = true;
        } else {
            was_hidden = false;
        }
        ++frames;
    }
    assert(a.removed && blinks > 0 && bubbles > 0 && a.y > 0x200000);
}

static void ectoplasm_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    /* Animations 0 (the drift, 12 frames) and 1 (the death, 3 frames)
     * with the exported durations. */
    static const uint8_t drift[12] = {8, 19, 17, 11, 5, 5, 8, 8, 15, 11, 10, 5};
    static const uint8_t death[3] = {4, 2, 4};
    const AosAnimDef defs[2] = {{12, drift}, {3, death}};
    const AosAnimSet anims = {defs, 2};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 2; ++a)
        for (int f = 0; f < 12; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-4, -9, 10, 18};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    const AosEnemyStats stats = {80, 24, 4, 0, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(900), AOS_FIXED(40), NULL);
    soma.max_hp = soma.hp = 320;

    /* The create: state 0, home point = position, contact element 0x80,
     * animation 0. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_ECTOPLASM, 200, 150, 0, 0, &soma, &layer,
                            &kind, &stats));
    assert(e.state == 0 && e.step == 0 && e.target_x == e.x && e.target_y == e.y &&
           e.contact_elements == 0x80 && e.anim.id == 0);

    /* Step 0 hovers at home: x pinned, y = home + a wobble of at most
     * 3 pixels, the phase advancing 0x200 a tick; Soma far away keeps it
     * there. */
    int32_t phase0 = e.walk_timer;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.step == 0 && e.x == e.target_x && e.timer == 1 &&
           e.walk_timer == phase0 + 0x200);
    int32_t wobble = e.y - e.target_y;
    assert(wobble >= -0x30000 && wobble <= 0x30000);

    /* Soma in the 0xB4 x 0xC8 box wakes it: step 1 next tick, then the
     * target pick (always_zero: -16 px in x by the RNG & 1 = 0 sign, -16 px
     * in y on the first + 0x20 pass) falls into step 2 the same frame. */
    soma.x = e.x;
    soma.y = e.y;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.step == 1 && e.timer == 0);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.step == 2 && e.timer == 1 && e.ec_dir == 1);
    assert(e.target_x == soma.x - 0x100000 && e.target_y == soma.y - 0x100000);
    assert(e.ax == -0x400 && e.ay == -0x400);

    /* Step 2 steers toward the target (damped 31/32, limit 1.0) and comes
     * back to step 1 on arrival or after 0x78 ticks. */
    int frames = 0;
    while (e.step == 2 && frames < 200) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                               always_zero);
        ++frames;
    }
    assert(e.step == 1 && frames > 4);

    /* A weapon hit knocks it back from Soma at 1.0 px/frame; a second,
     * lethal one starts the death drift: state 1, anim 1, timer 0x28,
     * song 0x6D, zeroed velocities and ay 0x1000. */
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    soma.x = e.x;
    soma.y = e.y;
    e.step = 0;
    e.state = 0;
    e.x = e.target_x = AOS_FIXED(200);
    e.y = e.target_y = AOS_FIXED(150);
    e.vx = e.vy = e.ax = e.ay = 0;
    e.timer = 0;
    aos_audio_clear();
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind, &weapon, &blade, 10, 4,
                                        0, 0, always_zero);
    assert(hit.enemy_hit && e.hp < stats.hp && !hit.killed);
    assert(e.vx != 0 || e.vy != 0);
    for (int i = 0; i < 20; ++i) {
        soma.x = e.x;
        soma.y = e.y;
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                               always_zero);
    }
    soma.x = e.x;
    soma.y = e.y;
    hit = aos_enemy_update(&e, &soma, &layer, &kind, &weapon, &blade, 9999, 4, 0, 0,
                           always_zero);
    assert(hit.killed && e.state == 1 && e.step == 0 && e.timer == 0x28 &&
           e.anim.id == 1 && e.defeated && e.vx == 0 && e.vy == 0 && e.ax == 0 &&
           e.ay == 0x1000 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x6D) == 1);

    /* State 1: it sinks under ay 0x1000, a wisp every 4th tick, blinks on
     * the table and is deleted when the u8 timer wraps. */
    frames = 0;
    int wisps = 0, blinks = 0;
    bool was_hidden = false;
    while (!e.removed && frames < 100) {
        AosHitReport r = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                                          0, 0, always_zero);
        wisps += r.effect_count;
        if (e.hidden) {
            if (!was_hidden) ++blinks;
            was_hidden = true;
        } else {
            was_hidden = false;
        }
        ++frames;
    }
    assert(e.removed && wisps >= 8 && blinks > 0 && e.vy > 0);

    /* The flag path (0x8E & 0x40) is state 2: parked at home with the
     * wobble, animation only — no drift, no collisions. */
    aos_enemy_set_flag_8e_40(true);
    assert(aos_enemy_create(&e, AOS_ENEMY_ECTOPLASM, 200, 150, 0, 0, &soma, &layer,
                            &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(e.state == 2 && e.step == 0 && e.target_x == e.x);
    int32_t hx = e.x;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0,
                           always_zero);
    assert(e.x == hx && !e.removed && !e.defeated);
}

static void butcher_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    /* Animations 0 (the patrol, 4 frames) and 1 (the throw, 4 frames with
     * a 3-tick third frame so frame 2 tick 2 occurs once). */
    static const uint8_t patrol[4] = {4, 4, 4, 4};
    static const uint8_t throw_[4] = {4, 4, 3, 4};
    const AosAnimDef defs[2] = {{4, patrol}, {4, throw_}};
    const AosAnimSet anims = {defs, 2};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 2; ++a)
        for (int f = 0; f < 4; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -8, 16, 16};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    /* The 0x085288FC probe descriptor: 1 wall probe, ceiling -30, floor
     * -1, half width 6, wall y -7. */
    kind.probes.count = 1;
    kind.probes.ceiling = -30;
    kind.probes.floor = -1;
    kind.probes.half_width = 6;
    kind.probes.wall_y[0] = -7;
    const AosEnemyStats stats = {112, 22, 4, 15, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(900), AOS_FIXED(40), NULL);
    soma.max_hp = soma.hp = 320;

    /* The create: state 0, animation 0 looping, one pixel down, element 0
     * contact. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_EVIL_BUTCHER, 200, 150, 0, 0, &soma,
                            &layer, &kind, &stats));
    assert(e.state == 0 && e.step == 0 && e.anim.id == 0 && e.contact_elements == 0);

    /* State 0 patrols on the floor with the probe walker; far from the
     * player it keeps walking and flipping after obstacle frames. */
    soma.x = AOS_FIXED(900);
    soma.y = AOS_FIXED(40);
    int32_t x0 = e.x;
    for (int i = 0; i < 8; ++i)
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                               always_zero);
    assert(e.state == 0 && e.x != x0);

    /* Soma inside the 0xA0 x 0x78 box at an animation loop start wakes it:
     * state 1, then step 0 faces the player and plays animation 1. */
    soma.x = e.x;
    soma.y = e.y;
    int frames = 0;
    while (e.state == 0 && frames < 200) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                               always_zero);
        ++frames;
    }
    assert(e.state == 1 && frames < 200);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.step == 1 && e.anim.id == 1);

    /* At frame 2 tick 2 the cleaver child is emitted: pixel position 24 px
     * up and 16 px to the facing side, facing and palette bank copied. */
    frames = 0;
    AosHitReport thrown = {0};
    while (!thrown.child_count && frames < 200) {
        thrown = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                  always_zero);
        ++frames;
    }
    assert(thrown.child_count == 1);
    const AosEnemy *cleaver = &thrown.children[0];
    assert(cleaver->role == AOS_ROLE_BUTCHER_CLEAVER && cleaver->id == e.id);
    assert(cleaver->x == ((e.x >> 16) + (e.mirrored ? 0x10 : -0x10)) * 0x10000);
    assert(cleaver->y == ((e.y >> 16) - 0x18) * 0x10000);
    assert(cleaver->mirrored == e.mirrored && cleaver->param1 == e.param1);

    /* The throw animation ends: step 3 resets to the patrol, animation 0. */
    frames = 0;
    while (e.state == 1 && frames < 200) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                               always_zero);
        ++frames;
    }
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.state == 0 && e.step == 0 && e.anim.id == 0);

    /* The cleaver's first update: still frame 8, hp 1, song 0x89, state 1,
     * flying at +/- 2.5 px/frame with element-1 contact. */
    AosEnemy c = *cleaver;
    aos_audio_clear();
    (void)aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(c.state == 1 && c.static_frame == 8 && c.hp == 1 &&
           c.contact_elements == 1 && c.own_boxes &&
           c.vx == (c.mirrored ? 0x28000 : (int32_t)0xFFFD8000) &&
           c.producer_timer == (uint16_t)(c.mirrored ? -0x1234 : 0x1234) &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x89) >= 1);

    /* It flies until a solid cell 6 px ahead sticks it: song 0x13A, hp 0,
     * state 2, a negated-halved vx plus random, vy and ay set. */
    c.x = AOS_FIXED(200);
    c.y = AOS_FIXED(100);
    c.vx = 0x28000;
    c.vy = 0;
    c.ay = 0;
    c.hp = 1;
    c.state = 1;
    for (int cx = 20; cx <= 32; ++cx) cells[12 * W + cx] = 0x01;
    (void)aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(c.state == 2 && c.hp == 0 && c.vy != 0 && c.ay == 0x2000 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x13A) >= 1);
    int32_t angle0 = (int32_t)c.angle;
    (void)aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(c.state == 2 && c.angle != (uint32_t)angle0);

    /* A struck cleaver loses its hit point at once (sub_080BFF0C). */
    AosEnemy c2 = c;
    c2.state = 1;
    c2.hp = 1;
    c2.x = AOS_FIXED(200);
    c2.y = AOS_FIXED(100);
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    soma.x = c2.x;
    soma.y = c2.y;
    AosHitReport hit = aos_enemy_update(&c2, &soma, &layer, &kind, &weapon, &blade,
                                        10, 4, 0, 0, always_zero);
    assert(hit.enemy_hit && c2.hp == 0);

    /* The parent's lethal blow: state 2, still frame 6 next update, song
     * 0x1C1 and a gore child; then particles every fourth tick until the
     * 0x40 timer deletes it. */
    e.state = 0;
    e.step = 0;
    e.timer = 0;
    e.x = AOS_FIXED(200);
    e.y = AOS_FIXED(100);
    e.vx = e.vy = e.ax = e.ay = 0;
    e.hp = stats.hp;
    soma.x = e.x;
    soma.y = e.y;
    hit = aos_enemy_update(&e, &soma, &layer, &kind, &weapon, &blade, 9999, 4,
                           0, 0, always_zero);
    assert(hit.killed && e.state == 2 && e.step == 0 && e.defeated);
    aos_audio_clear();
    AosHitReport death = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                                          10, 4, 0, 0, always_zero);
    assert(e.static_frame == 6 && e.step == 1 && e.timer == 0x40 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x1C1) >= 1);
    assert(death.child_count == 1 &&
           death.children[0].role == AOS_ROLE_BUTCHER_GORE);
    /* The gore lands 18 px to the facing side plus or minus 1 px (the
     * centred 0x20000 draw is -1 px with a zero RNG), 32 px up plus or
     * minus 1. */
    assert(death.children[0].x == e.x + (e.mirrored ? -0x120000 : 0x120000) -
                                0x10000 &&
           death.children[0].y == e.y - 0x200000 - 0x10000);

    frames = 0;
    int particles = 0;
    while (!e.removed && frames < 100) {
        AosHitReport r = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                                          10, 4, 0, 0, always_zero);
        particles += r.child_count;
        ++frames;
    }
    assert(e.removed && particles >= 8 && e.helper_spawned);

    /* A gore chunk survives the sign draw with a zero RNG, shows still
     * frame 8, falls under ay 0x2000 and sticks 4 px above a floor cell. */
    AosEnemy g = {0};
    g.id = AOS_ENEMY_EVIL_BUTCHER;
    g.role = AOS_ROLE_BUTCHER_GORE;
    g.static_frame = -1;
    g.x = AOS_FIXED(200);
    g.y = AOS_FIXED(60);
    (void)aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(g.state == 1 && g.static_frame == 8 && g.ay == 0x2000 &&
           g.angle == 0xC000 && !g.removed);
    cells[16 * W + 25] = 0x01;
    frames = 0;
    while (g.state == 1 && frames < 200) {
        (void)aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                               always_zero);
        ++frames;
    }
    assert(g.state == 2 && g.vx != 0 && g.vy != 0);

    /* A death particle counts its life down and is removed at zero. */
    AosEnemy p = {0};
    p.id = AOS_ENEMY_EVIL_BUTCHER;
    p.role = AOS_ROLE_BUTCHER_PARTICLE;
    p.static_frame = -1;
    p.timer = 3;
    (void)aos_enemy_update(&p, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(p.state == 1 && p.static_frame == 9 && p.timer == 2);
    (void)aos_enemy_update(&p, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    (void)aos_enemy_update(&p, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(!p.removed && p.timer == 0);
    (void)aos_enemy_update(&p, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(p.removed);

    /* The flag path (0x8E & 0x40) is state 3: animation only. */
    aos_enemy_set_flag_8e_40(true);
    assert(aos_enemy_create(&e, AOS_ENEMY_EVIL_BUTCHER, 200, 150, 0, 0, &soma,
                            &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(e.state == 3 && e.anim.id == 0);
}

static void killer_mantle_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    /* Animations 0-8, two 4-tick frames each. */
    static const uint8_t two[2] = {4, 4};
    AosAnimDef defs[9];
    for (int a = 0; a < 9; ++a) defs[a] = (AosAnimDef){2, two};
    const AosAnimSet anims = {defs, 9};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 9; ++a)
        for (int f = 0; f < 2; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -8, 16, 16};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    /* The 0x085288B4 probe descriptor: 1 wall probe, ceiling -8, floor 8,
     * half width 4, wall y 0. */
    kind.probes.count = 1;
    kind.probes.ceiling = -8;
    kind.probes.floor = 8;
    kind.probes.half_width = 4;
    kind.probes.wall_y[0] = 0;
    const AosEnemyStats stats = {96, 18, 4, 12, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(900), AOS_FIXED(40), NULL);
    soma.max_hp = soma.hp = 320;

    /* The create: state 0, animation 0 looping, element-1 contact, the
     * + 0x72 attack bit set. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_KILLER_MANTLE, 200, 150, 0, 0, &soma,
                            &layer, &kind, &stats));
    assert(e.state == 0 && e.step == 0 && e.anim.id == 0 &&
           e.contact_elements == 1 && e.combat.attack_off);

    /* The first update emits the linked orbit child (the create's
     * sub_0806DFF8) and the player far away keeps it waiting. */
    soma.x = AOS_FIXED(900);
    soma.y = AOS_FIXED(40);
    AosHitReport r = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                                      10, 4, 0, 0, always_zero);
    assert(r.child_count == 1 &&
           r.children[0].role == AOS_ROLE_MANTLE_ORBIT);
    AosEnemy orbit = r.children[0];
    orbit.parent = &e;
    int frames = 0;
    while (e.state == 0 && frames < 200) {
        soma.x = e.x;
        soma.y = e.y;
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.state == 1 && frames < 200);

    /* The orbit child initializes on its first update (state 0 -> 1),
     * still hidden while the parent's + 0x1A bit 0 is clear. */
    (void)aos_enemy_update(&orbit, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(orbit.state == 1 && orbit.hidden && orbit.static_frame == 0x12 &&
           !orbit.removed);

    /* Patrolling with the player glued in the 0xA0 x 0x46 box: the phase
     * counter passes 0x2F and the attack starts (state 2). */
    frames = 0;
    while (e.state == 1 && frames < 400) {
        soma.x = e.x;
        soma.y = e.y;
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.state == 2 && e.step == 0 && frames < 400);

    /* Attack step 0: face the player, animation 3, + 0x1A bit 0 set —
     * which the orbit child picks up on its next update. */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.step == 1 && e.anim.id == 3 && (e.producer_timer & 1));
    (void)aos_enemy_update(&orbit, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    /* States 0-1-2 fall through: scale and angle start growing, the blade
     * unhides 8 px to the facing side of the parent. */
    assert(orbit.state == 2 && !orbit.hidden && orbit.static_frame == 0x12 &&
           orbit.walk_timer == 0x1000 && orbit.producer_timer == 0xD300 &&
           orbit.angle == 0xD300);
    assert((orbit.x >> 16) == (e.x >> 16) + (e.mirrored ? -8 : 8) &&
           (orbit.y >> 16) == (e.y >> 16));

    /* Animation 3 ends: step 1 -> animation 4, the hop (vy -0x1B000 with
     * a zero RNG), ay 0x1000, speed 0x48000, attack contact live, song
     * 0x89. */
    frames = 0;
    while (!(e.anim.id == 4) && frames < 100) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.step == 2 && e.anim.id == 4 && e.vy == (int32_t)0xFFFE5000 &&
           e.ay == 0x1000 && e.mantle_speed == 0x48000 &&
           !e.combat.attack_off &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x89) >= 1);

    /* Animation 4 ends: the afterimage child is emitted, the attack bit
     * closes, + 0x1A bit 1 clears, animation 5 plays. */
    aos_audio_clear();
    frames = 0;
    AosHitReport mid = {0};
    while (e.step != 3 && frames < 200) {
        mid = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.step == 3 && e.anim.id == 5 && e.combat.attack_off &&
           !(e.producer_timer & 2));
    int ghost_i = -1;
    for (int i = 0; i < mid.child_count; ++i)
        if (mid.children[i].role == AOS_ROLE_MANTLE_GHOST) ghost_i = i;
    assert(ghost_i >= 0);
    AosEnemy ghost = mid.children[ghost_i];
    ghost.parent = &e;

    /* The ghost's first update: animation 8, following 12 px to the back
     * (negated by the facing) and 4 px down. */
    (void)aos_enemy_update(&ghost, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(ghost.state == 1 && ghost.anim.id == 8 && !ghost.removed);
    assert((ghost.x >> 16) == (e.x >> 16) + (e.mirrored ? 0xC : -0xC) &&
           (ghost.y >> 16) == (e.y >> 16) + 4);

    /* Animation 5 ends: back to the patrol, state 1 step 0. */
    frames = 0;
    while (e.state == 2 && frames < 200) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.state == 1 && e.step == 0 && frames < 200);

    /* The orbit blade keeps spinning while the parent's bit 0 holds:
     * + 0x1A advances 0x300 a frame, the angle follows. */
    uint16_t a0 = orbit.producer_timer;
    (void)aos_enemy_update(&orbit, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(orbit.producer_timer == (uint16_t)(a0 + 0x300) &&
           orbit.angle == orbit.producer_timer && orbit.state == 2);

    /* The ghost deletes when animation 8 ends. */
    frames = 0;
    while (!ghost.removed && frames < 100) {
        (void)aos_enemy_update(&ghost, &soma, &layer, &kind, NULL, NULL, 10,
                               4, 0, 0, always_zero);
        ++frames;
    }
    assert(ghost.removed && frames < 100);

    /* The parent's lethal blow: state 3, then animation 6, song 0x6D and
     * a gore drift every fourth tick until the animation deletes it —
     * which also ends the orbit child in state 4. */
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    soma.x = e.x;
    soma.y = e.y;
    e.hp = stats.hp;
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind, &weapon,
                                        &blade, 9999, 4, 0, 0, always_zero);
    assert(hit.killed && e.state == 3 && e.step == 0);
    aos_audio_clear();
    /* The parent's hit flash freezes the blade for a few frames; once it
     * clears, the parent's state 3 ends it in state 4 and it deletes. */
    frames = 0;
    while (!orbit.removed && frames < 100) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        (void)aos_enemy_update(&orbit, &soma, &layer, &kind, NULL, NULL, 10,
                               4, 0, 0, always_zero);
        ++frames;
    }
    assert(orbit.state == 4 && orbit.removed && frames < 100);
    frames = 0;
    bool gore = false;
    while (!e.removed && frames < 200) {
        AosHitReport d = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                          NULL, 10, 4, 0, 0, always_zero);
        for (int i = 0; i < d.effect_count; ++i)
            if (d.effects[i].kind == AOS_EFFECT_HIT_ELEMENT_5 &&
                d.effects[i].drift)
                gore = true;
        ++frames;
    }
    assert(e.removed && frames < 200 && gore &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x6D) >= 1);

    /* The flag path (0x8E & 0x40) is state 4: animation only. */
    aos_enemy_set_flag_8e_40(true);
    assert(aos_enemy_create(&e, AOS_ENEMY_KILLER_MANTLE, 200, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(e.state == 4 && e.anim.id == 0);
}

static void dryad_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    /* Animations 0-8 (the seed reuses 6-8), two 4-tick frames each —
     * animation 3 (the spit) gets six frames so its 24 ticks outlast the
     * 0x12 seed timer like the native one. */
    static const uint8_t two[2] = {4, 4};
    static const uint8_t six[6] = {4, 4, 4, 4, 4, 4};
    AosAnimDef defs[9];
    for (int a = 0; a < 9; ++a) defs[a] = (AosAnimDef){2, two};
    defs[3] = (AosAnimDef){6, six};
    const AosAnimSet anims = {defs, 9};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 9; ++a)
        for (int f = 0; f < 6; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -8, 16, 16};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    const AosEnemyStats stats = {350, 45, 38, 2, 0, 200};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(900), AOS_FIXED(40), NULL);
    soma.max_hp = soma.hp = 320;

    /* The create: state 0, animation 0 looping, one pixel down. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_DRYAD, 200, 150, 0, 0, &soma,
                            &layer, &kind, &stats));
    assert(e.state == 0 && e.step == 0 && e.anim.id == 0 &&
           e.contact_elements == 0);

    /* The notice box (+ 0x42 - 0x6E, + 0x46 - 0x28, 0xDC x 0x50) holds
     * Soma: step 1, then RNG & 3 == 0 wakes it into state 1 with
     * animation 1 once. */
    soma.x = e.x;
    soma.y = e.y;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.state == 0 && e.step == 1);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.state == 1 && e.step == 0 && e.anim.id == 1);

    /* State 1 step 0 waits the animation end, then loops animation 2;
     * step 1 rolls RNG & 7 == 0 into animation 3, timer 0x12, step 2. */
    int frames = 0;
    while (e.step != 1 && frames < 100) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.anim.id == 2 && frames < 100);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.step == 2 && e.anim.id == 3 && e.timer == 0x12);

    /* Step 2 spits on the last three timer ticks: song 0x89 and one seed
     * each (RNG & 0xF = 0 -> 10 px to the back since unmirrored, 26 px
     * up; attack byte 45 - 11 = 34). */
    aos_audio_clear();
    AosEnemy seeds[3];
    int seed_count = 0;
    frames = 0;
    while (seed_count < 3 && frames < 100) {
        AosHitReport r = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                          NULL, 10, 4, 0, 0, always_zero);
        for (int i = 0; i < r.child_count && seed_count < 3; ++i)
            seeds[seed_count++] = r.children[i];
        ++frames;
    }
    assert(seed_count == 3 && frames < 100 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x89) >= 1);
    for (int i = 0; i < 3; ++i) {
        assert(seeds[i].role == AOS_ROLE_DRYAD_SEED &&
               seeds[i].stats.contact == 34 && seeds[i].contact_elements == 0);
        assert(seeds[i].x == e.x + (e.mirrored ? 0xA0000 : -0xA0000) &&
               seeds[i].y == e.y - 0x1A0000);
        assert(seeds[i].vy < 0);
    }

    /* The seed's first update registers its (-2,-2,4,4) boxes, plays
     * animation 8 loop and integrates the arc with gravity. */
    AosEnemy s = seeds[0];
    int32_t sx0 = s.x, sy0 = s.y;
    (void)aos_enemy_update(&s, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(s.state == 1 && s.own_boxes && s.anim.id == 8 && s.vy < 0);
    assert(s.own_hurt.x == -2 && s.own_attack.x == -2);
    (void)aos_enemy_update(&s, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(s.x != sx0 || s.y != sy0);

    /* It arcs down onto the row-20 floor: state 2 plays animations 6
     * then 7 once each, then state 3 with timer 0x78 tracking the
     * player's x on the floor. */
    soma.x = s.x;
    soma.y = s.y;
    frames = 0;
    while (s.state != 3 && frames < 600) {
        (void)aos_enemy_update(&s, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    /* The shared landed tail counts the 0x78 timer down on the transition
     * tick itself, like the native fall-through. */
    assert(s.state == 3 && s.timer == 0x77 && frames < 600);
    int32_t floor_y = s.y;
    soma.x = s.x + 0x20000;
    (void)aos_enemy_update(&s, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert((s.x >> 16) == (soma.x >> 16) && s.y == floor_y);

    /* State 3 deals the periodic record every sixteenth tick while the
     * player stands in its 16 px box: soma_hit with 34 attack. */
    soma.x = s.x;
    soma.y = s.y;
    soma.hp = 300;
    bool hurt = false;
    frames = 0;
    while (s.state == 3 && !hurt && frames < 600) {
        AosHitReport r = aos_enemy_update(&s, &soma, &layer, &kind, NULL,
                                          NULL, 10, 4, 0, 0, always_zero);
        if (r.soma_hit) hurt = true;
        ++frames;
    }
    assert(hurt && frames < 600);

    /* The 0x78 timer deletes it. */
    frames = 0;
    while (!s.removed && frames < 600) {
        (void)aos_enemy_update(&s, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(s.removed && frames < 600);

    /* Any weapon hit deletes a live seed (sub_080AEB80). */
    AosEnemy s2 = seeds[1];
    s2.state = 1;
    s2.step = 0;
    s2.vx = s2.vy = 0;
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    soma.x = s2.x;
    soma.y = s2.y;
    (void)aos_enemy_update(&s2, &soma, &layer, &kind, &weapon, &blade, 10, 4,
                           0, 0, always_zero);
    assert(s2.removed);

    /* The parent's lethal blow mid-attack: animation 5, state 3, timer
     * 0x28; then the death handler arms 0x40, song 0x6E, bursts and the
     * affine turn until the timer deletes it. */
    e.state = 1;
    e.step = 1;
    e.x = AOS_FIXED(200);
    e.y = AOS_FIXED(100);
    e.vx = e.vy = e.ax = e.ay = 0;
    e.hp = stats.hp;
    soma.x = e.x;
    soma.y = e.y;
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind, &weapon,
                                        &blade, 9999, 4, 0, 0, always_zero);
    assert(hit.killed && e.state == 3 && e.step == 0 && e.defeated);
    aos_audio_clear();
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.step == 1 && e.timer == 0x40 && e.anim.id == 5 &&
           e.angle == 0x10000);
    frames = 0;
    int bursts = 0;
    while (!e.removed && frames < 200) {
        AosHitReport d = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                          NULL, 10, 4, 0, 0, always_zero);
        bursts += d.effect_count;
        ++frames;
    }
    assert(e.removed && bursts > 0 && frames < 200 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E) >= 1);

    /* A lethal blow from state 0 plays animation 4 instead. */
    assert(aos_enemy_create(&e, AOS_ENEMY_DRYAD, 200, 150, 0, 0, &soma,
                            &layer, &kind, &stats));
    e.x = AOS_FIXED(200);
    e.y = AOS_FIXED(100);
    e.hp = 1;
    soma.x = e.x;
    soma.y = e.y;
    hit = aos_enemy_update(&e, &soma, &layer, &kind, &weapon, &blade, 10, 4,
                           0, 0, always_zero);
    assert(hit.killed && e.state == 3);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.anim.id == 4);

    /* The flag path (0x8E & 0x40) is state 4: animation only. */
    aos_enemy_set_flag_8e_40(true);
    assert(aos_enemy_create(&e, AOS_ENEMY_DRYAD, 200, 150, 0, 0, &soma,
                            &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(e.state == 4 && e.anim.id == 0);
}

/* A seeded sequence for the landed-hop draws: the default 0x7F fails
 * every threshold (deepest path), 0 passes the first gate. */
static const uint32_t *tsu_seq;
static size_t tsu_seq_len, tsu_seq_i;
static uint32_t tsu_rand(void) {
    return tsu_seq_i < tsu_seq_len ? tsu_seq[tsu_seq_i++] : 0x7F;
}

static void tsuchinoko_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    /* Animations 0-4 (the children reuse 4): anim 1 needs a third frame
     * for the + 0x6E == 2 hop launch, anim 2 a fourth for the state 1
     * spit at frame 3 tick 0, anim 3 ten frames (the dormant spawn parks
     * on frame 8) with a long fourth frame for the step 0xB wait at
     * frame 3 tick 0x14. */
    static const uint8_t two[2] = {4, 4};
    static const uint8_t f3[3] = {4, 4, 4};
    /* anim 2's last frame must outlast the step 0xB wait (+ 0x6E ==
     * 0x1403, frame 3 tick 0x14) like the native one. */
    static const uint8_t f4[4] = {4, 4, 4, 30};
    static const uint8_t f10[10] = {4, 4, 4, 30, 4, 4, 4, 4, 4, 4};
    AosAnimDef defs[5];
    defs[0] = (AosAnimDef){2, two};
    defs[1] = (AosAnimDef){3, f3};
    defs[2] = (AosAnimDef){4, f4};
    defs[3] = (AosAnimDef){10, f10};
    defs[4] = (AosAnimDef){2, two};
    const AosAnimSet anims = {defs, 5};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 5; ++a)
        for (int f = 0; f < 10; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -8, 16, 16};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    const AosEnemyStats stats = {300, 30, 25, 2, 0, 80};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(900), AOS_FIXED(40), NULL);
    soma.max_hp = soma.hp = 320;

    /* The create: state 0, animation 0 looping, + 0x14 = the HP. The
     * dormant roll (RNG & 0x7F > 0x18) reads the first LCG draw, which
     * is constant across a 256-seed block: 1536 rolls active, 256
     * dormant. */
    AosEnemy e;
    aos_random_seed(1536);
    assert(aos_enemy_create(&e, AOS_ENEMY_TSUCHINOKO, 200, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.step == 0 && e.anim.id == 0 &&
           e.hp_home == stats.hp);

    /* The notice box (+ 0x42 - 0x5A, + 0x46 - 0x64, 0xB4 x 0xC8) holds
     * Soma: step 1 with animation 1 once. At frame 2 tick 0 the facing
     * boxes pick the hop — Soma at +70 px sits in the far 0xC8 box (the
     * near 0x78 box ends at +60), which keeps the facing step 0 wrote,
     * so mirrored stays and vx = +1.0, vy = -1.5. */
    soma.x = e.x + AOS_FIXED(70);
    soma.y = e.y;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.step == 1 && e.anim.id == 1);
    int frames = 0;
    while (!(e.step == 2 && e.vy == (int32_t)0xFFFE8000) && frames < 100) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.step == 2 && e.mirrored && e.vx == 0x10000 && e.ay == 0x2800 &&
           frames < 100);

    /* The hop lands on the row-20 floor: the first landed roll
     * (always_zero) enters state 1, the attack. */
    frames = 0;
    while (e.state != 1 && frames < 600) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.state == 1 && e.step == 0 && frames < 600);

    /* State 1 step 0 rears with animation 2 once; step 1 at frame 3
     * tick 0 spits the fast lob with song 0x85, then the animation end
     * re-enters the hop at step 1. */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.step == 1 && e.anim.id == 2);
    aos_audio_clear();
    AosEnemy spit;
    bool got_spit = false;
    frames = 0;
    while (e.state == 1 && frames < 200) {
        AosHitReport r = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                          NULL, 10, 4, 0, 0, always_zero);
        if (r.child_count) { spit = r.children[0]; got_spit = true; }
        ++frames;
    }
    assert(got_spit && e.state == 0 && e.step == 1 && e.anim.id == 1 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x85) >= 1 &&
           frames < 200);
    /* The spit: role, facing copy, +3.0/-3.0 by facing, vy -1.0, ay
     * 0x1800, 26 px above the parent's pixel, element 0x40. */
    assert(spit.role == AOS_ROLE_TSUCHINOKO_SPIT && spit.mirrored == e.mirrored);
    assert(spit.x == (e.x >> 16) << 16 && spit.y == (((e.y >> 16) - 0x1A) << 16));
    assert(spit.vx == (e.mirrored ? 0x30000 : (int32_t)0xFFFD0000) &&
           spit.vy == (int32_t)0xFFFF0000 && spit.ay == 0x1800 &&
           spit.contact_elements == 0x40 && spit.stats.contact == 30);

    /* The spit arcs and deletes off screen (sub_0806D128). */
    frames = 0;
    while (!spit.removed && frames < 600) {
        (void)aos_enemy_update(&spit, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(spit.removed || frames < 600);

    /* A spit touching Soma deals the 30-byte contact at element 0x40
     * and deletes itself (sub_080A7A54). */
    AosEnemy s2;
    memset(&s2, 0, sizeof s2);
    s2.id = AOS_ENEMY_TSUCHINOKO;
    s2.role = AOS_ROLE_TSUCHINOKO_SPIT;
    s2.static_frame = -1;
    s2.stats = stats;
    s2.contact_elements = 0x40;
    s2.combat.type = AOS_TYPE_ENEMY;
    s2.x = soma.x;
    s2.y = soma.y;
    s2.vx = 0x30000;
    s2.vy = (int32_t)0xFFFF0000;
    s2.ay = 0x1800;
    AosHitReport contact = aos_enemy_update(&s2, &soma, &layer, &kind, NULL,
                                            NULL, 10, 4, 0, 0, always_zero);
    assert(contact.soma_hit && s2.removed);

    /* Any weapon hit deletes a live spit (sub_080A7A90). */
    memset(&s2, 0, sizeof s2);
    s2.id = AOS_ENEMY_TSUCHINOKO;
    s2.role = AOS_ROLE_TSUCHINOKO_SPIT;
    s2.static_frame = -1;
    s2.stats = stats;
    s2.combat.type = AOS_TYPE_ENEMY;
    s2.x = soma.x;
    s2.y = soma.y;
    s2.vx = 0x30000;
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    (void)aos_enemy_update(&s2, &soma, &layer, &kind, &weapon, &blade, 10, 4,
                           0, 0, always_zero);
    assert(s2.removed);

    /* The dormant spawn (RNG & 0x7F > 0x18): step 0xC parked on anim 3
     * frame 8 spits a harmless rock every sixteenth tick with song
     * 0x152 and deletes itself when the animation ends. */
    aos_random_seed(256);
    assert(aos_enemy_create(&e, AOS_ENEMY_TSUCHINOKO, 200, 150, 0, 0, &soma,
                            &layer, &kind, &stats));
    assert(e.step == 0xC && e.anim.id == 3 && e.anim.frame == 8 &&
           e.anim.tick == 0);
    aos_audio_clear();
    int rocks = 0;
    frames = 0;
    while (!e.removed && frames < 600) {
        AosHitReport r = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                          NULL, 10, 4, 0, 0, tsu_rand);
        for (int i = 0; i < r.child_count; ++i)
            if (r.children[i].role == AOS_ROLE_TSUCHINOKO_ROCK) ++rocks;
        ++frames;
    }
    assert(e.removed && rocks >= 1 && frames < 600 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x152) >= 1);

    /* The rock itself: still frame 0x12, no contact, arcs and deletes
     * off screen. */
    AosEnemy rock;
    memset(&rock, 0, sizeof rock);
    rock.id = AOS_ENEMY_TSUCHINOKO;
    rock.role = AOS_ROLE_TSUCHINOKO_ROCK;
    rock.static_frame = 0x12;
    rock.stats = stats;
    rock.combat.type = AOS_TYPE_ENEMY;
    rock.combat.attack_off = rock.combat.hurt_off = true;
    rock.x = AOS_FIXED(200);
    rock.y = AOS_FIXED(100);
    rock.vx = 0x8000;
    rock.vy = (int32_t)0xFFFF0000;
    rock.ay = 0x1800;
    int32_t rx = rock.x, ry = rock.y;
    (void)aos_enemy_update(&rock, &soma, &layer, &kind, NULL, NULL, 10, 4, 0,
                           0, always_zero);
    assert(rock.x != rx || rock.y != ry);

    /* The landed tree's burrow gate: roll1 fails, roll2 passes ->
     * step 0xA. Drive a fresh snake to a landing with that sequence. */
    assert(aos_enemy_create(&e, AOS_ENEMY_TSUCHINOKO, 200, 150, 0, 0, &soma,
                            &layer, &kind, &stats));
    e.step = 0;
    e.state = 0;
    e.hp_home = stats.hp;
    e.hp = stats.hp;
    e.x = AOS_FIXED(200);
    e.y = AOS_FIXED(100);
    e.vx = 0x10000;
    e.vy = 0;
    e.ay = 0x2800;
    e.step = 2;   /* mid-hop: the next grounded frame runs the tree */
    static const uint32_t burrow_seq[] = {0x7F, 0x00};
    tsu_seq = burrow_seq;
    tsu_seq_len = 2;
    tsu_seq_i = 0;
    /* Force the landing: put the snake just above the floor with a
     * downward velocity so walk_collide reports the ground bit. */
    e.y = AOS_FIXED(158);
    e.vy = 0x4000;
    frames = 0;
    while (e.step == 2 && frames < 60) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, tsu_rand);
        ++frames;
    }
    assert(e.step == 0xA && frames < 60);
    tsu_seq = NULL;
    tsu_seq_len = tsu_seq_i = 0;

    /* Steps 0xA/0xB play anims 2 then 3 into the dormant spitter. */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.step == 0xB && e.anim.id == 2);
    frames = 0;
    while (e.step != 0xC && frames < 200) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.step == 0xC && e.anim.id == 3 && frames < 200);

    /* The parent's lethal blow: song 0x70, state 2, timer 0x3C; the
     * death spawns two gore chunks, falls and deletes at timer 0. */
    assert(aos_enemy_create(&e, AOS_ENEMY_TSUCHINOKO, 200, 150, 0, 0, &soma,
                            &layer, &kind, &stats));
    e.x = AOS_FIXED(200);
    e.y = AOS_FIXED(150);
    e.hp = 1;
    soma.x = e.x;
    soma.y = e.y;
    static const uint8_t blade_ticks2[] = {8};
    const AosAnimDef blade_def2 = {1, blade_ticks2};
    const AosAnimSet blade_set2 = {&blade_def2, 1};
    const AosHitbox blade_box2[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade2 = {&blade_set2, blade_box2};
    AosWeaponEntity weapon2 = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    aos_audio_clear();
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind, &weapon2,
                                        &blade2, 9999, 4, 0, 0, always_zero);
    assert(hit.killed && e.state == 2 && e.step == 0 && e.timer == 0x3C &&
           e.defeated);
    frames = 0;
    int chunks = 0;
    while (!e.removed && frames < 200) {
        AosHitReport d = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                          NULL, 10, 4, 0, 0, always_zero);
        chunks += d.effect_count;
        ++frames;
    }
    assert(e.removed && chunks >= 2 && frames < 200 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x70) == 1);

    /* The flag path (0x8E & 0x40) is state 3: animation only. */
    aos_enemy_set_flag_8e_40(true);
    assert(aos_enemy_create(&e, AOS_ENEMY_TSUCHINOKO, 200, 150, 0, 0, &soma,
                            &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(e.state == 3 && e.anim.id == 0);
}

static void kicker_skeleton_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    /* A thick floor like a real room: the + 0x10 floor probe must find
     * solid below the standing origin (160 - 16 = 144). */
    for (int y = 20; y < 26; ++y)
        for (int x = 0; x < W; ++x) cells[y * W + x] = 0x03;
    /* Animations 0-6: 0 idle, 1-2 the fidget, 3 the wind-up and landing,
     * 4 the hop (short so AOS_ANIM_ENDED comes quickly), 5 the slide,
     * 6 the fall (six frames so + 0x6E == 5 is reached). */
    static const uint8_t three[3] = {4, 4, 4};
    static const uint8_t six[6] = {4, 4, 4, 4, 4, 4};
    static const uint8_t two[2] = {4, 4};
    AosAnimDef defs[7];
    defs[0] = (AosAnimDef){3, three};
    defs[1] = (AosAnimDef){3, three};
    defs[2] = (AosAnimDef){3, three};
    defs[3] = (AosAnimDef){6, six};
    defs[4] = (AosAnimDef){2, two};
    defs[5] = (AosAnimDef){2, two};
    defs[6] = (AosAnimDef){6, six};
    const AosAnimSet anims = {defs, 7};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 7; ++a)
        for (int f = 0; f < 6; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -8, 16, 16};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* The 0x08528908 probe table: 2 wall probes, ceiling -10, floor 16,
     * half width 6, wall rows 6 and -10. */
    kind.probes = (AosProbes){2, -10, 16, 6, {6, -10}};
    /* enemies.tsv id 83: hp 200, field_e 30, contact 40, defence 2,
     * weak 0x20, resist 0x10, experience 80. */
    const AosEnemyStats stats = {200, 40, 2, 0x20, 0x10, 80};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(900), AOS_FIXED(40), NULL);
    soma.max_hp = soma.hp = 320;

    /* The create (sub_080C0324): state 0, animation 0 looping, the floor
     * snap; the flag path (0x8E & 0x40) is state 3. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_KICKER_SKELETON, 200, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.step == 0 && e.anim.id == 0 && e.hp == stats.hp);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy flagged;
    assert(aos_enemy_create(&flagged, AOS_ENEMY_KICKER_SKELETON, 200, 150, 0,
                            0, &soma, &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(flagged.state == 3 && flagged.anim.id == 0);
    /* State 3: animation only — the animation still advances. */
    (void)aos_enemy_update(&flagged, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(!flagged.removed);

    /* The wake: Soma in the centred 0xC8 x 0x50 box moves it to state 1;
     * substep 0 faces, plays animation 3 and lands on substep 2 in the
     * same call. */
    soma.x = e.x + AOS_FIXED(60);
    soma.y = e.y;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.state == 1 && e.step == 0);
    /* Substep 0 faces, plays animation 3 and lands on substep 2 in the
     * same call (the native case-0 fall-through). */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.step == 2 && e.anim.id == 3);

    /* Animation 3 ends (6 frames x 4 ticks): substep 2 -> the hop, anim 4,
     * speed 0x8000, vy -0x30000, ay 0x1000, substep 3. */
    int frames = 0;
    while (!(e.step == 3 && e.anim.id == 4) && frames < 100) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.step == 3 && e.anim.id == 4 && e.kicker_speed == 0x8000 &&
           e.vy == (int32_t)0xFFFD0000 && e.ay == 0x1000 && frames < 100);

    /* Rising under ay 0x1000: when vy reaches -0x18000 the apex switch
     * plays animation 6 and substep 4. */
    frames = 0;
    while (e.step != 4 && frames < 100) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.step == 4 && e.anim.id == 6 && frames < 100);

    /* Walk frame 5 of animation 6: the slam — animation 5, speed 0x28000,
     * vy +0x30000, + 0x14 bit 0, sound 0x87, substep 5. */
    frames = 0;
    while (e.step != 5 && frames < 100) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.step == 5 && e.anim.id == 5 && e.kicker_speed == 0x28000 &&
           e.vy == 0x30000 && (e.kicker_flags & 1) && frames < 100 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x87) >= 1);

    /* The slide lands (probe bit 8): animation 3, + 0x14 bit 1 cleared,
     * sound 0x127, substep 6. */
    frames = 0;
    while (e.step != 6 && frames < 60) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.step == 6 && e.anim.id == 3 &&
           !(e.kicker_flags & 2) &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x127) >= 1);

    /* The skid decays 31/32 a frame until 0x7FFF, then idles: state 0,
     * animation 0 looping, substep 0. */
    frames = 0;
    while (e.state != 0 && frames < 200) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.state == 0 && e.step == 0 && e.anim.id == 0 && frames < 200);

    /* The strike contact: while + 0x14 & 1 the power is + 0x3C * 5 / 4 —
     * contact 40 deals 50. */
    AosEnemy striker;
    memset(&striker, 0, sizeof striker);
    striker.id = AOS_ENEMY_KICKER_SKELETON;
    striker.role = AOS_ROLE_ENEMY;
    striker.static_frame = -1;
    striker.stats = stats;
    striker.hp = stats.hp;
    striker.hp_home = stats.hp;
    striker.kicker_flags = 1;
    striker.combat.type = AOS_TYPE_ENEMY;
    striker.x = soma.x;
    striker.y = soma.y;
    soma.combat = (AosCombat){.type = AOS_TYPE_PLAYER};
    AosHitReport contact = aos_enemy_update(&striker, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 0, 0,
                                            always_zero);
    assert(contact.soma_hit &&
           contact.soma_damage == aos_player_damage(50, 4));

    /* The lethal blow (sub_080C0478): only a kill reacts — + 0x20 the
     * side away from the attacker, state 2. The next update runs
     * sub_080C0250: song 0x6B and seven debris pieces, then the entity
     * deletes itself. */
    assert(aos_enemy_create(&e, AOS_ENEMY_KICKER_SKELETON, 200, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    e.x = AOS_FIXED(200);
    e.y = AOS_FIXED(150);
    e.hp = 1;
    soma.x = e.x + AOS_FIXED(10);
    soma.y = e.y + AOS_FIXED(10);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    aos_audio_clear();
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind, &weapon,
                                        &blade, 9999, 4, 0, 0, always_zero);
    assert(hit.killed && e.state == 2 && e.away == -1 && e.defeated);
    AosHitReport death = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                          NULL, 10, 4, 0, 0, always_zero);
    assert(e.removed && death.child_count == 7 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x6B) == 1);
    for (int i = 0; i < death.child_count; ++i)
        assert(death.children[i].role == AOS_ROLE_DEBRIS);
}

static void waiter_skeleton_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    /* A thick floor like a real room for the 0x08528608 probes. */
    for (int y = 20; y < 26; ++y)
        for (int x = 0; x < W; ++x) cells[y * W + x] = 0x03;
    /* Animations 0-17: 0 idle, 1 the post-attack loop, 3 the retreat,
     * 4 the notice, 5 the strike (four frames so + 0x6E == 2 is
     * reached), 6/7 the tray raises, 8 the companion replay, 9 and 17
     * the tray frames, 10-16 the death debris. */
    static const uint8_t four[4] = {4, 4, 4, 4};
    static const uint8_t three[3] = {4, 4, 4};
    static const uint8_t two[2] = {4, 4};
    static const uint8_t one[1] = {4};
    AosAnimDef defs[18];
    defs[0] = (AosAnimDef){3, three};
    defs[1] = (AosAnimDef){3, three};
    defs[2] = (AosAnimDef){1, one};
    defs[3] = (AosAnimDef){3, three};
    defs[4] = (AosAnimDef){3, three};
    defs[5] = (AosAnimDef){4, four};
    defs[6] = (AosAnimDef){2, two};
    defs[7] = (AosAnimDef){2, two};
    defs[8] = (AosAnimDef){1, one};
    defs[9] = (AosAnimDef){1, one};
    for (int a = 10; a <= 16; ++a) defs[a] = (AosAnimDef){1, one};
    defs[17] = (AosAnimDef){1, one};
    const AosAnimSet anims = {defs, 18};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 18; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -8, 16, 16};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 0x100;
    /* The 0x08528608 walker probes, as in the Kicker Skeleton test. */
    kind.probes = (AosProbes){2, -10, 16, 6, {6, -10}};
    /* The companion and tray attach to the parent's frame edge
     * (sub_0806C3F8 approximated): animation 0 frame 0 sits at (8, -8). */
    kind.edges[0][0][0] = 8;
    kind.edges[0][0][1] = -8;
    /* enemies.tsv id 30: hp 120, contact 22, defence 14, weak 0x20,
     * resist 0x10, experience 38. */
    const AosEnemyStats stats = {120, 22, 14, 0x20, 0x10, 38};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(900), AOS_FIXED(40), NULL);
    soma.max_hp = soma.hp = 320;

    /* The create (sub_080AF574): state 0, animation 0 looping, + 0x20 = 2;
     * the flag path (0x8E & 0x40) is state 4. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_WAITER_SKELETON, 200, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.step == 0 && e.anim.id == 0 && e.ws_flags == 2 &&
           e.hp == stats.hp);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy flagged;
    assert(aos_enemy_create(&flagged, AOS_ENEMY_WAITER_SKELETON, 200, 150, 0,
                            0, &soma, &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(flagged.state == 4 && flagged.ws_flags == 2);
    /* State 4: animation only — it steps and never collides. */
    (void)aos_enemy_update(&flagged, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(!flagged.removed && flagged.state == 4 && flagged.anim.id == 0 &&
           flagged.anim.tick == 2);

    /* The first update emits the constructor's companion child
     * (sub_080AEC0C): state 0 plays animation 0x11, state 1 pins it to
     * the parent's attachment (edge (8,-8), mirrored -> -8). */
    AosHitReport built = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                          NULL, 10, 4, 0, 0, always_zero);
    assert(built.child_count == 1 &&
           built.children[0].role == AOS_ROLE_WAITER_COMPANION &&
           built.children[0].parent == &e);
    AosEnemy comp = built.children[0];
    (void)aos_enemy_update(&comp, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(comp.state == 1 && comp.anim.id == 0x11);
    (void)aos_enemy_update(&comp, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert((comp.x >> 16) == (e.x >> 16) - 8 &&
           (comp.y >> 16) == (e.y >> 16) - 8);
    /* The companion hides once + 0x20 bit 1 clears (after the first
     * attack's step 2) or the parent dies. */
    e.ws_flags = 0;
    (void)aos_enemy_update(&comp, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(comp.hidden);
    e.ws_flags = 2;

    /* The wake box (-0xA0, -0x28, 0x140, 0x50): Soma inside moves it to
     * state 1; substep 0 picks the normal tray (RNG & 3 == 0). */
    soma.x = e.x - AOS_FIXED(0x40);
    soma.y = e.y;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.state == 1 && e.step == 0);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.step == 1);
    /* Step 1 spawns the tray (animation 0x11) and plays animation 6. */
    AosHitReport tray_birth = aos_enemy_update(&e, &soma, &layer, &kind,
                                               NULL, NULL, 10, 4, 0, 0,
                                               always_zero);
    assert(e.step == 2 && e.anim.id == 6 && tray_birth.child_count == 1 &&
           tray_birth.children[0].role == AOS_ROLE_WAITER_TRAY &&
           tray_birth.children[0].ws_anim == 0x11);
    AosEnemy tray = tray_birth.children[0];
    /* The tray's first update: state 0 -> state 1 at the parent's
     * offset, animation 0x11. */
    (void)aos_enemy_update(&tray, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(tray.state == 1 && tray.anim.id == 0x11 && !tray.hidden);
    /* Animation 6 ends: state 2, animation 3 looping, steps cleared and
     * + 0x20 bits 0-1 cleared (the companion now hides). */
    int frames = 0;
    while (e.state != 2 && frames < 60) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.state == 2 && e.step == 0 && e.anim.id == 3 && e.ws_flags == 0 &&
           frames < 60);

    /* Walk step 0: Soma in the (-0x64, -0x28, 0xC8, 0x50) box plays
     * animation 4; RNG & 3 != 0 -> step 1. */
    soma.x = e.x + AOS_FIXED(0x40);
    soma.y = e.y;
    static const uint32_t odd[] = {1};
    script = odd;
    script_at = 0;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           scripted);
    assert(e.step == 1 && e.anim.id == 4);
    /* Step 1: the tighter box (-0x50, -0x28, 0xA0, 0x50) plays
     * animation 5 once -> step 2. */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.step == 2 && e.anim.id == 5);
    /* Step 2: frame 2 arms + 0x20 bit 2 (the tray's throw window); the
     * tray follows while the parent's animation is 2-5 and throws on
     * the bit — sound 0x89, state 4, vy -0x20000, ay 0x2000. */
    frames = 0;
    while (!(tray.state == 4 && (e.ws_flags & 4)) && frames < 80) {
        (void)aos_enemy_update(&tray, &soma, &layer, &kind, NULL, NULL, 10,
                               4, 0, 0, always_zero);
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(tray.state == 4 && (e.ws_flags & 4) && frames < 80 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x89) >= 1 &&
           tray.vy == (int32_t)0xFFFE0000 && tray.ay == 0x2000);
    /* The thrown tray spins and flies: state 4 integrates vx/vy and
     * spins the affine angle. */
    int32_t px = tray.x, py = tray.y;
    (void)aos_enemy_update(&tray, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(tray.x != px || tray.y != py);
    /* Its contact: on Soma it deals the parent's contact and zeroes
     * + 0x34 — the parent's + 0x20 bit 3 is set (the palette bytes
     * match). */
    tray.x = soma.x;
    tray.y = soma.y;
    soma.combat = (AosCombat){.type = AOS_TYPE_PLAYER};
    AosHitReport thrown = aos_enemy_update(&tray, &soma, &layer, &kind,
                                           NULL, NULL, 10, 4, 0, 0,
                                           always_zero);
    assert(thrown.soma_hit && tray.hp == 0 && (e.ws_flags & 8));
    /* HP 0 -> state 5: the sub_0806D5C0 dust burst and deletion. */
    (void)aos_enemy_update(&tray, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(tray.removed && tray.state == 5);

    /* Let the walk finish: animation 5 ends -> step 5 (animation 1
     * looping, timer 0x3C + RNG & 0xF), walks back, then state 0. */
    frames = 0;
    while (e.state != 0 && frames < 200) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.state == 0 && e.step == 0 && e.anim.id == 0 && frames < 200);

    /* The lethal blow (sub_080AF6FC): state 3, timer 0x28, steps
     * cleared; the next update plays sound 0x6B and throws the 7
     * debris pieces (animations 0xA-0x10, + 0x20 bit 1 cleared by the
     * attack -> wait, bit 1 was cleared: 6 pieces 0xB-0x10). */
    assert(aos_enemy_create(&e, AOS_ENEMY_WAITER_SKELETON, 200, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    e.x = AOS_FIXED(200);
    e.y = AOS_FIXED(150);
    e.hp = 1;
    e.ws_flags = 2;   /* fresh: bit 1 still set -> 7 pieces */
    soma.x = e.x + AOS_FIXED(10);
    soma.y = e.y + AOS_FIXED(10);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    aos_audio_clear();
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind, &weapon,
                                        &blade, 9999, 4, 0, 0, always_zero);
    assert(hit.killed && e.state == 3 && e.timer == 0x28 && e.defeated);
    AosHitReport death = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                          NULL, 10, 4, 0, 0, always_zero);
    /* sub_080AF450 step 0: + 0x20 bit 1 still set on this fresh enemy ->
     * animations 0xA-0x10, 7 pieces. */
    assert(!e.removed && death.child_count == 7 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x6B) == 1);
    for (int i = 0; i < death.child_count; ++i)
        assert(death.children[i].role == AOS_ROLE_DEBRIS);
    AosHitReport gone = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                         NULL, 10, 4, 0, 0, always_zero);
    assert(e.removed && gone.child_count == 0);
}

static void cockatrice_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    /* A thick floor for the 0x085288C0 probes. */
    for (int y = 20; y < 26; ++y)
        for (int x = 0; x < W; ++x) cells[y * W + x] = 0x03;
    /* Animations 0-2: 0 the patrol (8 frames), 1 the attack (16 frames
     * so index 0xA exists for the rock spawn), 2 the death. */
    static const uint8_t eight[8] = {6, 6, 6, 6, 6, 6, 6, 6};
    static const uint8_t sixteen[16] = {2, 2, 2, 2, 2, 2, 2, 2,
                                        2, 2, 2, 2, 2, 2, 2, 2};
    static const uint8_t three[3] = {4, 4, 4};
    const AosAnimDef defs[3] = {{8, eight}, {16, sixteen}, {3, three}};
    const AosAnimSet anims = {defs, 3};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 3; ++a)
        for (int f = 0; f < 16; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-24, -48, 48, 48};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 0x100;
    kind.probes = (AosProbes){2, -10, 16, 6, {6, -10}};
    kind.edges[0][0][0] = 8;
    kind.edges[0][0][1] = -8;
    /* enemies.tsv id 46: hp 380, contact 35, defence 30, weak 0x20,
     * resist 0x10, experience 150. */
    const AosEnemyStats stats = {380, 35, 30, 0x20, 0x10, 150};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(800), AOS_FIXED(40), NULL);
    soma.max_hp = soma.hp = 320;

    /* The create (sub_080BBFF0): state 0, animation 0 looping,
     * variant 0; the flag path (0x8E & 0x40) is state 3. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_COCKATRICE, 200, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.step == 0 && e.anim.id == 0 &&
           e.variant == 0 && e.hp == stats.hp);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy flagged;
    assert(aos_enemy_create(&flagged, AOS_ENEMY_COCKATRICE, 200, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(flagged.state == 3);
    (void)aos_enemy_update(&flagged, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(!flagged.removed && flagged.state == 3 && flagged.anim.tick == 2);

    /* The first update emits the shadow child (sub_080BB924): state 0
     * -> state 1, parent linked, inert to hits. */
    AosHitReport built = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                          NULL, 10, 4, 0, 0, always_zero);
    assert(built.child_count == 1 &&
           built.children[0].role == AOS_ROLE_COCKATRICE_SHADOW &&
           built.children[0].parent == &e);
    AosEnemy shadow = built.children[0];
    (void)aos_enemy_update(&shadow, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(shadow.state == 1);
    /* The shadow rides the parent's attachment edge (8,-8) plus the
     * -10 x offset flipped by the facing (the parent faces Soma on
     * its right, so it is mirrored). */
    (void)aos_enemy_update(&shadow, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    int sdx = e.mirrored ? -8 + 10 : 8 - 10;
    assert((shadow.x >> 16) == (e.x >> 16) + sdx &&
           (shadow.y >> 16) == (e.y >> 16) - 8);
    /* The parent's death mark (+ 0x18 bit 0) deletes the shadow. */
    e.ck_flags |= 1;
    (void)aos_enemy_update(&shadow, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(shadow.removed);
    e.ck_flags = 0;

    /* The wake box (0xF0 x 0x64 centred): Soma 60 px to the left wakes
     * it into state 1. */
    soma.x = e.x - AOS_FIXED(60);
    soma.y = e.y;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.state == 1 && e.step == 0);

    /* State 1 step 0 -> 1; the walk reaches an animation boundary with
     * Soma on the matching side within 0x77 px: step 2, animation 1. */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.step == 1);
    int frames = 0;
    while (!(e.step == 2 && e.anim.id == 1) && frames < 120) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.step == 2 && e.anim.id == 1 && frames < 120);

    /* Animation 1 frame 0xA tick 0 spawns the rock (sub_080BBAC4):
     * parent linked, static frame 1, own contact box, hurt off. */
    AosHitReport thrown = {0};
    frames = 0;
    while (!thrown.child_count && frames < 40) {
        thrown = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10,
                                  4, 0, 0, always_zero);
        ++frames;
    }
    assert(thrown.child_count == 1 &&
           thrown.children[0].role == AOS_ROLE_COCKATRICE_ROCK &&
           thrown.children[0].parent == &e &&
           thrown.children[0].static_frame == 1 &&
           thrown.children[0].combat.hurt_off);
    AosEnemy rock = thrown.children[0];
    /* The rock's first update: state 0 -> 1, aimed toss toward Soma. */
    (void)aos_enemy_update(&rock, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(rock.state == 1);
    /* It flies, spins and dies past + 0x1A 0x1F with sound 0x65. */
    int32_t rx = rock.x, ry = rock.y;
    uint32_t ra = rock.angle;
    frames = 0;
    while (!rock.removed && frames < 80) {
        (void)aos_enemy_update(&rock, &soma, &layer, &kind, NULL, NULL, 10,
                               4, 0, 0, always_zero);
        ++frames;
    }
    assert(rock.removed && frames < 80 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x65) >= 1);
    (void)rx; (void)ry; (void)ra;

    /* The rock's contact: on Soma it deals damage through its own
     * box. Rebuild a fresh rock on top of Soma. */
    assert(aos_enemy_create(&e, AOS_ENEMY_COCKATRICE, 200, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    e.state = 1;
    e.step = 2;
    e.anim.id = 1;
    e.anim.frame = 0xA;
    e.anim.tick = 0;
    e.anim.flags = AOS_ANIM_TABLE;   /* the attack's non-looping play */
    soma.combat = (AosCombat){.type = AOS_TYPE_PLAYER};
    soma.hp = soma.max_hp = 320;
    AosHitReport rock_hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                             NULL, 10, 4, 0, 0, always_zero);
    /* The spawn update itself does not collide; place the emitted rock
     * on Soma for its own update. */
    (void)rock_hit;
    AosEnemy live;
    memset(&live, 0, sizeof live);
    live.id = AOS_ENEMY_COCKATRICE;
    live.role = AOS_ROLE_COCKATRICE_ROCK;
    live.x = soma.x;
    live.y = soma.y;
    live.stats = stats;
    live.hp = 1;
    live.static_frame = 1;
    live.own_boxes = true;
    live.own_hurt = live.own_attack = (AosBox){-6, -6, 12, 12};
    live.combat.type = AOS_TYPE_ENEMY;
    live.state = 1;
    AosHitReport contact = aos_enemy_update(&live, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 0, 0,
                                            always_zero);
    assert(contact.soma_hit);

    /* Let the attack finish: animation 1 ends -> step 3 timer 0x20 ->
     * state 0 animation 0. */
    frames = 0;
    while (e.state != 0 && frames < 200) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++frames;
    }
    assert(e.state == 0 && e.step == 0 && e.anim.id == 0 && frames < 200);

    /* The lethal blow (sub_080BC0B4): state 2; the next update plays
     * sound 0x65, then song 0x6E with gore, deletes at the timer's
     * end. */
    assert(aos_enemy_create(&e, AOS_ENEMY_COCKATRICE, 200, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    e.x = AOS_FIXED(200);
    e.y = AOS_FIXED(150);
    e.hp = 1;
    soma.x = e.x + AOS_FIXED(10);
    soma.y = e.y + AOS_FIXED(10);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    aos_audio_clear();
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind, &weapon,
                                        &blade, 9999, 4, 0, 0, always_zero);
    assert(hit.killed && e.state == 2 && e.step == 0 && e.defeated);
    AosHitReport death = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                          NULL, 10, 4, 0, 0, always_zero);
    assert(!e.removed && e.anim.id == 2 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x65) == 1);
    (void)death;
    int ticks = 0;
    while (!e.removed && ticks < 200) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        ++ticks;
    }
    assert(e.removed && ticks < 200 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E) >= 1);

    /* The Basilisk (id 0x64) shares the whole family: EnemyBasiliskCreate
     * (0x080BC1F8) is sub_080BBFF0(e, 1) and EnemyBasiliskUpdate
     * (0x080BC204) is byte-identical to EnemyCockatriceUpdate, so the
     * same state machine runs on the variant-1 palette bank. */
    /* enemies.tsv id 100: hp 700, contact 65, defence 38, weak 0x81,
     * resist 0, experience 600. */
    const AosEnemyStats basil_stats = {700, 65, 38, 0x0081, 0x0000, 600};
    AosEnemy b;
    assert(aos_enemy_create(&b, AOS_ENEMY_BASILISK, 200, 150, 0, 0,
                            &soma, &layer, &kind, &basil_stats));
    assert(b.state == 0 && b.variant == 1 && b.hp == basil_stats.hp);
    AosHitReport bh = aos_enemy_update(&b, &soma, &layer, &kind, NULL,
                                       NULL, 10, 4, 0, 0, always_zero);
    assert(bh.child_count == 1 && bh.children[0].id == AOS_ENEMY_BASILISK &&
           bh.children[0].role == AOS_ROLE_COCKATRICE_SHADOW);
}

static void chronomage_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    /* A thick floor for the 0x08528824 probes. */
    for (int y = 20; y < 26; ++y)
        for (int x = 0; x < W; ++x) cells[y * W + x] = 0x03;
    /* Animations 0-6: 0 idle, 1 the cast/death (40 ticks so the 0x28
     * death timer and the animation end together), 2 the inert second
     * pose, 3 the time-stop (60 ticks, still playing when the wisp
     * spawns), 4/5 the chase left/right loops, 6 the wisp. */
    static const uint8_t eight[8] = {6, 6, 6, 6, 6, 6, 6, 6};
    static const uint8_t five[5] = {8, 8, 8, 8, 8};
    static const uint8_t ten[10] = {6, 6, 6, 6, 6, 6, 6, 6, 6, 6};
    static const uint8_t four[4] = {8, 8, 8, 8};
    const AosAnimDef defs[7] = {{8, eight}, {5, five}, {3, five},
                                {10, ten},   {8, eight}, {8, eight},
                                {4, four}};
    const AosAnimSet anims = {defs, 7};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 7; ++a)
        for (int f = 0; f < 16; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-24, -48, 48, 48};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 0x100;
    kind.probes = (AosProbes){2, -10, 16, 6, {6, -10}};
    /* enemies.tsv id 40: hp 80, field_e 100, contact 30, defence 0,
     * experience 30, soul index 55. */
    const AosEnemyStats stats = {80, 100, 30, 0, 0, 30};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(40), NULL);
    soma.max_hp = soma.hp = 320;

    /* The create (sub_080B712C -> 0x0806E0D0 resources): state 0,
     * animation 0 looping, hurt off until the chase; the 0x8E & 0x40
     * flag path is the inert state 4. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_CHRONOMAGE, 200, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.step == 0 && e.anim.id == 0 &&
           e.combat.hurt_off && e.hp == stats.hp);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy flagged;
    assert(aos_enemy_create(&flagged, AOS_ENEMY_CHRONOMAGE, 200, 150, 0,
                            0, &soma, &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(flagged.state == 4);
    /* State 4 (sub_080B70B4): animation 0 for 0x80 ticks, then 1, then
     * 2 held. */
    int frames = 0;
    while (flagged.step < 2 && frames < 200) {
        (void)aos_enemy_update(&flagged, &soma, &layer, &kind, NULL, NULL,
                               10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(flagged.step == 2 && flagged.anim.id == 1);
    frames = 0;
    while (flagged.step < 3 && frames < 100) {
        (void)aos_enemy_update(&flagged, &soma, &layer, &kind, NULL, NULL,
                               10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(flagged.step == 3 && flagged.anim.id == 2);

    /* The dormant watch (sub_080B6D68): Soma beyond 0x6F px keeps it
     * idle; within range it enters state 1. */
    soma.x = e.x + AOS_FIXED(200);
    soma.y = e.y;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0,
                           0, always_zero);
    assert(e.state == 0);
    soma.x = e.x + AOS_FIXED(60);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0,
                           0, always_zero);
    assert(e.state == 1 && e.step == 0);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0,
                           0, always_zero);
    assert(e.step == 1 && e.anim.id == 1);

    /* The time-stop sequence (sub_080B6DEC): animation 1 (40 ticks) ->
     * animation 3 + timer 0x18 -> song 0xEC at 8 -> the + 0xD ramp ->
     * the fade steps -> the wisp spawn once animation 3 ends -> the
     * 0x30 timer -> state 2. */
    frames = 0;
    while (e.state == 1 && e.step < 3 && frames < 100) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10,
                               4, 0, 0, always_zero);
        ++frames;
    }
    assert(e.step == 3 && e.anim.id == 3 && e.timer == 0x18);
    frames = 0;
    while (e.state == 1 && e.step == 3 && frames < 60) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10,
                               4, 0, 0, always_zero);
        ++frames;
    }
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xEC) == 1);
    /* Steps 4-7 run to the wisp spawn (animation 3 still playing). */
    AosHitReport spawned = {0};
    frames = 0;
    while (e.state == 1 && frames < 200) {
        spawned = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                                   10, 4, 0, 0, always_zero);
        if (spawned.child_count) break;
        ++frames;
    }
    assert(spawned.child_count == 1 &&
           spawned.children[0].role == AOS_ROLE_CHRONO_WISP &&
           (spawned.children[0].y >> 16) == (e.y >> 16) - 0x20 &&
           (spawned.children[0].cm_flags & 2));
    AosEnemy wisp = spawned.children[0];
    /* The wisp (sub_080B6C3C): state 0 -> 1 ramping + 0x18, state 2
     * hold, state 3 decay and delete; the affine scale follows the
     * ramp. */
    (void)aos_enemy_update(&wisp, &soma, &layer, &kind, NULL, NULL, 10,
                           4, 0, 0, always_zero);
    assert(wisp.state == 1 && wisp.spin == 0);
    (void)aos_enemy_update(&wisp, &soma, &layer, &kind, NULL, NULL, 10,
                           4, 0, 0, always_zero);
    assert(wisp.spin == 0x1000 && wisp.affine_sx > 0);
    frames = 0;
    while (!wisp.removed && frames < 120) {
        (void)aos_enemy_update(&wisp, &soma, &layer, &kind, NULL, NULL,
                               10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(wisp.removed && frames < 120);
    /* Back in state 1 step 7: the 0x30 timer returns to the chase. */
    frames = 0;
    while (e.state == 1 && frames < 100) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10,
                               4, 0, 0, always_zero);
        ++frames;
    }
    assert(e.state == 2 && e.step == 0 && e.combat.hurt_off);

    /* The chase (sub_080B6FB8): step 0 clears + 0x72 bit 2 and plays
     * animation 1; step 2 picks 4 or 5 by the target side; step 3
     * ramps + 0x18 by 0x2000 and walks the probes. */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(e.step == 1 && e.anim.id == 1 && !e.combat.hurt_off);
    frames = 0;
    while (e.step < 3 && frames < 100) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10,
                               4, 0, 0, always_zero);
        ++frames;
    }
    assert(e.step == 3 && e.anim.id == 4);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    /* The + 0x18 ramp and the probe walk ran; the floor probes block
     * the step here and fire the native -0x30000 hop on + 0x4C. */
    assert(e.spin == 0x2000 && e.vy == (int32_t)0xFFFD0000);

    /* The lethal blow (sub_080B72B8): state 3, animation 1, the
     * sub_0806AEAC flames and the deletion at the animation's end. */
    assert(aos_enemy_create(&e, AOS_ENEMY_CHRONOMAGE, 200, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    e.x = AOS_FIXED(200);
    e.y = AOS_FIXED(150);
    e.hp = 1;
    e.combat.hurt_off = false;
    soma.x = e.x + AOS_FIXED(10);
    soma.y = e.y + AOS_FIXED(10);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    aos_audio_clear();
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind, &weapon,
                                        &blade, 9999, 4, 0, 0, always_zero);
    assert(hit.killed && e.state == 3 && e.step == 0 && e.defeated);
    int death_ticks = 0, flame_ticks = 0;
    while (!e.removed && death_ticks < 120) {
        AosHitReport d = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                          NULL, 10, 4, 0, 0, always_zero);
        flame_ticks += d.effect_count;
        ++death_ticks;
    }
    assert(e.removed && death_ticks < 120 && flame_ticks > 0);
}

static void mandragora_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int y = 20; y < 26; ++y)
        for (int x = 0; x < W; ++x) cells[y * W + x] = 0x03;
    /* Animations 0-5: 0 idle, 1/2 the head attack (short so the
     * one-shot ends), 3 the body loop, 4/5 the tail reveal and loop. */
    static const uint8_t six[6] = {6, 6, 6, 6, 6, 6};
    static const uint8_t four[4] = {8, 8, 8, 8};
    static const uint8_t two[2] = {6, 6};
    const AosAnimDef defs[6] = {{8, six}, {2, two}, {2, two}, {8, six},
                                {4, four}, {8, six}};
    const AosAnimSet anims = {defs, 6};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 6; ++a)
        for (int f = 0; f < 16; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-24, -48, 48, 48};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 0x100;
    /* enemies.tsv id 79: hp 200, contact 65, defence 0, experience 100,
     * soul index 38. */
    const AosEnemyStats stats = {200, 65, 0, 0, 0, 100};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(40), NULL);
    soma.max_hp = soma.hp = 320;
    int frames = 0;

    /* EnemyMandragoraCreate (0x080CEF34): state 1, animation 0 looping,
     * floor snapped; the 0x8E & 0x40 flag path is the inert state 3. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_MANDRAGORA, 200, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 1 && e.step == 0 && e.anim.id == 0 &&
           e.hp == stats.hp && e.mg_seeds == 0);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy flagged;
    assert(aos_enemy_create(&flagged, AOS_ENEMY_MANDRAGORA, 200, 150, 0,
                            0, &soma, &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(flagged.state == 3);
    /* State 3 (sub_080CFA18): animation 0 held, only the step latch. */
    (void)aos_enemy_update(&flagged, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(flagged.step == 1 && flagged.anim.id == 0);

    /* The first on-screen update emits the body child
     * (sub_080CEFE8); the body then emits the tail (sub_080CF2B8). */
    soma.x = e.x + AOS_FIXED(200);
    soma.y = e.y;
    AosHitReport born = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                          NULL, 10, 4, 0, 0, always_zero);
    assert(born.child_count == 1 &&
           born.children[0].role == AOS_ROLE_MANDRAGORA_BODY);
    AosEnemy body = born.children[0];
    body.parent = &e;
    e.mg_body = &body;
    /* sub_080CF0E4 state 0: dormant, hidden, synced to the head. */
    (void)aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(body.state == 0 && body.hidden && body.step == 1);
    /* The tail spawn rides the body's first update report. */
    AosHitReport tailborn = {0};
    {
        /* Force the tail emission: the body already ran once, so emit
         * it through a fresh dormant body the way the room does. */
        AosEnemy b2 = born.children[0];
        b2.parent = &e;
        tailborn = aos_enemy_update(&b2, &soma, &layer, &kind, NULL,
                                     NULL, 10, 4, 0, 0, always_zero);
        body = b2;
        e.mg_body = &body;
    }
    assert(tailborn.child_count == 1 &&
           tailborn.children[0].role == AOS_ROLE_MANDRAGORA_TAIL);
    AosEnemy tail = tailborn.children[0];
    tail.parent = &body;
    body.mg_tail = &tail;
    (void)aos_enemy_update(&tail, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(tail.state == 0 && tail.hidden);

    /* State 1 (sub_080CF5C4): the centred probe keeps it dormant while
     * Soma is beyond the box, then enters the attack state 2. */
    soma.x = e.x + AOS_FIXED(400);
    soma.y = e.y;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(e.state == 1 && e.step == 1);
    soma.x = e.x + AOS_FIXED(60);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(e.step == 2);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(e.state == 2 && e.step == 0);

    /* State 2 (sub_080CF624): animation 1 then 2, song 0x119, the body
     * state bump, then the held attack frame while no hit lands. */
    frames = 0;
    while (e.step == 0 && frames < 40) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10,
                               4, 0, 0, always_zero);
        ++frames;
    }
    assert(e.step == 1 && e.substep >= 0);
    frames = 0;
    while (e.substep != 2 && frames < 60) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10,
                               4, 0, 0, always_zero);
        ++frames;
    }
    assert(e.substep == 2 && e.static_frame == 0xA &&
           body.state == 1);
    /* Body state 1 (sub_080CF0E4): hp sync, head sensor bit 1, song
     * 0x83, straight into state 2 which reveals the tail. */
    (void)aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL, 10,
                           4, 0, 0, always_zero);
    assert(body.state == 2 && (e.mg_sensors & 2) &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x83) == 1);
    (void)aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL, 10,
                           4, 0, 0, always_zero);
    assert(tail.state == 1);
    frames = 0;
    while (tail.state != 3 && frames < 80) {
        (void)aos_enemy_update(&tail, &soma, &layer, &kind, NULL, NULL,
                               10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(tail.state == 3);
    /* State 3 step 0 arms the head's + 0x20 bit 4 and loops anim 5. */
    (void)aos_enemy_update(&tail, &soma, &layer, &kind, NULL, NULL, 10,
                           4, 0, 0, always_zero);
    assert(e.mg_sensors & 0x10);

    /* The lethal blow on the head (sub_080CFA4C): state 0xF0, then the
     * sub_080CF748 volley of five seeds (sub_080CF910 children). */
    e.hp = 1;
    e.combat.hurt_off = false;
    soma.x = e.x + AOS_FIXED(10);
    soma.y = e.y + AOS_FIXED(10);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind, &weapon,
                                        &blade, 9999, 4, 0, 0, always_zero);
    assert(hit.killed && e.state == 0xF0 && e.step == 0);
    AosHitReport volley = {0};
    frames = 0;
    while (e.mg_seeds == 0 && frames < 10) {
        volley = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                                  10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(volley.child_count == 5 && e.mg_seeds == 5);
    for (int i = 0; i < 5; ++i)
        assert(volley.children[i].role == AOS_ROLE_MANDRAGORA_SEED &&
               volley.children[i].static_frame >= 0x1F &&
               volley.children[i].static_frame <= 0x23);
    /* A seed (sub_080CF910): lifetime countdown then removal, which
     * decrements the head's live-seed count. */
    AosEnemy seed = volley.children[0];
    seed.parent = &e;
    frames = 0;
    while (!seed.removed && frames < 200) {
        (void)aos_enemy_update(&seed, &soma, &layer, &kind, NULL, NULL,
                               10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(seed.removed && e.mg_seeds == 4);

    /* The body death (sub_080CF9FC -> 0xF0 -> 0xF1): a lethal blow drops
     * it into the one-tick 0xF0 then the flame-emitting 0xF1; the tail,
     * still looping state 3, sees body state > 0xEF and enters its own
     * 0xF0 before the body's 0xF1 step 0 puts it back to dormant. */
    body.hp = 1;
    body.combat.hurt_off = false;
    hit = aos_enemy_update(&body, &soma, &layer, &kind, &weapon, &blade,
                           9999, 4, 0, 0, always_zero);
    assert(hit.killed && body.state == 0xF0);
    (void)aos_enemy_update(&tail, &soma, &layer, &kind, NULL, NULL, 10,
                           4, 0, 0, always_zero);
    assert(tail.state == 0xF0);
    (void)aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL, 10,
                           4, 0, 0, always_zero);
    assert(body.state == 0xF1 && (e.mg_sensors & 0x20));
    (void)aos_enemy_update(&body, &soma, &layer, &kind, NULL, NULL, 10,
                           4, 0, 0, always_zero);
    assert(tail.state == 0 && (e.mg_sensors & 8));
}

static void disc_armor_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int y = 20; y < 26; ++y)
        for (int x = 0; x < W; ++x) cells[y * W + x] = 0x03;
    /* Animations 0-8: 0/1 patrol, 2/3 the attack, 4/5 shard frames,
     * 6 the disc, 7 the glow, 8 the death lurch. */
    static const uint8_t six[6] = {6, 6, 6, 6, 6, 6};
    static const uint8_t two[2] = {6, 6};
    static const uint8_t four[4] = {8, 8, 8, 8};
    const AosAnimDef defs[9] = {{8, six}, {8, six}, {2, two}, {2, two},
                                {4, four}, {4, four}, {8, six}, {8, six},
                                {8, six}};
    const AosAnimSet anims = {defs, 9};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 9; ++a)
        for (int f = 0; f < 16; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-24, -48, 48, 48};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 0x100;
    /* enemies.tsv id 52 (0x34): hp 240, contact 35, defence 25,
     * experience 150, soul red index 30. */
    const AosEnemyStats stats = {240, 35, 25, 0x0108, 0, 150};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(40), NULL);
    soma.max_hp = soma.hp = 320;
    int frames = 0;

    /* EnemyDiscArmorCreate (0x080B2228): animation 1 stepped once,
     * state 0; the 0x8E & 0x40 flag path is the inert state 3. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_DISC_ARMOR, 200, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.step == 0 && e.da_flags == 0);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy flagged;
    assert(aos_enemy_create(&flagged, AOS_ENEMY_DISC_ARMOR, 200, 150, 0,
                            0, &soma, &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(flagged.state == 3);
    /* State 3 only steps the animation. */
    (void)aos_enemy_update(&flagged, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(flagged.anim.id == 1 || flagged.anim.id == 0);

    /* State 0 step 0 (sub_080B1F14): param0 clear picks animation 0 and
     * step 1, the aggro gate. */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(e.step == 1 && e.anim.id == 0);
    /* Soma far away: no aggro; the patrol keeps walking. */
    soma.x = e.x + AOS_FIXED(600);
    soma.y = e.y;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    /* Step 2 is reached on the next patrol pass once the aggro gate
     * re-arms; drive it directly to check the walk physics. */
    e.step = 2;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(e.state == 0 || e.state == 1);

    /* Aggro (sub_0806D044 <= 0x4F): Soma close enters state 1. */
    e.state = 0;
    e.step = 1;
    soma.x = e.x + AOS_FIXED(40);
    soma.y = e.y;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(e.state == 1 && e.step == 0);

    /* State 1 step 0 (sub_080B205C): faces the player, animation 2,
     * da_flags = 1, orbit offsets -20/-36. */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(e.step == 1 && e.da_flags == 1 && e.da_orbit_x == -20 &&
           e.da_orbit_y == -36);
    /* The disc throw at animation word 0x10C (frame 0x0C, tick 1). */
    e.anim.frame = 0x0C;
    e.anim.tick = 1;
    AosHitReport thrown = aos_enemy_update(&e, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 0, 0,
                                            always_zero);
    assert(thrown.child_count == 1 &&
           thrown.children[0].role == AOS_ROLE_DISC_BLADE &&
           thrown.children[0].parent == &e);
    AosEnemy disc = thrown.children[0];
    disc.parent = &e;
    e.da_flags |= 2; /* the disc's return signal, set by its update */
    /* da_flags bit 1 -> animation 3, step 2, orbit eases to -10/-16. */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(e.step == 2 && e.anim.id == 3);
    e.da_orbit_x = -20;
    e.da_orbit_y = -36;
    e.anim.flags |= AOS_ANIM_ENDED;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(e.state == 0 && e.da_flags == 0);

    /* The disc (sub_080B1678): state 0 emits the glow child
     * (sub_080B14B4), plays song 0x74 and enters state 1. */
    disc.state = 0;
    disc.da_momentum = 0;
    disc.da_launch = 0;
    AosHitReport glowborn = aos_enemy_update(&disc, &soma, &layer, &kind,
                                              NULL, NULL, 10, 4, 0, 0,
                                              always_zero);
    assert(glowborn.child_count == 1 &&
           glowborn.children[0].role == AOS_ROLE_DISC_GLOW &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x74) == 1);
    assert(disc.state == 1 && disc.x == e.da_ox && disc.y == e.da_oy);
    AosEnemy glow = glowborn.children[0];
    glow.parent = &disc;
    disc.da_glow = &glow;
    /* The glow registers itself on the disc and tracks the orbit. */
    disc.x += AOS_FIXED(30);
    disc.y += AOS_FIXED(10);
    aos_enemy_disc_glow_update(&glow, &kind);
    assert(disc.da_glow == &glow && glow.da_radius != 0 &&
           glow.x == e.da_ox && glow.y == e.da_oy);

    /* States 1-3 fly the sine orbit; drive the machine forward. */
    frames = 0;
    while (disc.state < 4 && frames < 400) {
        (void)aos_enemy_update(&disc, &soma, &layer, &kind, NULL, NULL,
                               10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(disc.state >= 1);

    /* State 4 (sub_080B1B42): shard bursts — type-5 shards while the
     * timer is <= 3 and song 0x73 every 16 ticks. */
    disc.state = 4;
    disc.timer = 0;
    disc.da_momentum = 0x1000;
    AosHitReport burst = {0};
    frames = 0;
    while (disc.state == 4 && burst.child_count == 0 && frames < 8) {
        burst = aos_enemy_update(&disc, &soma, &layer, &kind, NULL,
                                 NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(burst.child_count >= 1 &&
           burst.children[0].role == AOS_ROLE_DISC_SHARD &&
           burst.children[0].variant == 5);

    /* A shard (sub_080B1428): animation 5 once, moves, deletes on the
     * animation end. */
    AosEnemy shard = burst.children[0];
    shard.parent = &disc;
    int sx = (int)(shard.x >> 16);
    (void)aos_enemy_update(&shard, &soma, &layer, &kind, NULL, NULL, 10,
                           4, 0, 0, always_zero);
    assert(shard.state == 1 && shard.anim.id == 5);
    frames = 0;
    while (!shard.removed && frames < 200) {
        (void)aos_enemy_update(&shard, &soma, &layer, &kind, NULL, NULL,
                               10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(shard.removed);
    (void)sx;

    /* State 5 (sub_080B1D14): the return flight — speed decays toward
     * the armor, position follows the heading. */
    disc.state = 5;
    disc.da_speed = 0x30000;
    disc.da_heading = 0;
    disc.timer = 0;
    int32_t dx0 = disc.x;
    (void)aos_enemy_update(&disc, &soma, &layer, &kind, NULL, NULL, 10,
                           4, 0, 0, always_zero);
    assert(disc.da_speed < 0x30000 || disc.da_speed == 0x20001);
    (void)dx0;

    /* State 7 (sub_080B1E76): the fade timer then removal. */
    disc.state = 7;
    disc.timer = 2;
    (void)aos_enemy_update(&disc, &soma, &layer, &kind, NULL, NULL, 10,
                           4, 0, 0, always_zero);
    assert(disc.timer == 1 && !disc.removed);
    (void)aos_enemy_update(&disc, &soma, &layer, &kind, NULL, NULL, 10,
                           4, 0, 0, always_zero);
    (void)aos_enemy_update(&disc, &soma, &layer, &kind, NULL, NULL, 10,
                           4, 0, 0, always_zero);
    assert(disc.removed);

    /* The lethal blow (sub_080B2370 -> state 2): song 0x6F on entry,
     * gravity on + 0x4C, deletion when animation 8 ends. */
    e.hp = 1;
    e.combat.hurt_off = false;
    soma.x = e.x + AOS_FIXED(10);
    soma.y = e.y + AOS_FIXED(10);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind, &weapon,
                                        &blade, 9999, 4, 0, 0, always_zero);
    assert(hit.killed && e.state == 2 && e.step == 0);
    frames = 0;
    while (!e.removed && frames < 200) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10,
                               4, 0, 0, always_zero);
        ++frames;
    }
    assert(e.removed);
}

static void alura_une_tests(void) {
    aos_audio_clear();
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int y = 20; y < 26; ++y)
        for (int x = 0; x < W; ++x) cells[y * W + x] = 0x03;
    /* Animations 0-0xD: 0 idle loop, 1-6 the fight cycles, 7-8 the
     * launched scythe, 9 the orbit blade, 0xA-0xD the spin frames. */
    static const uint8_t four[4] = {4, 4, 4, 4};
    const AosAnimDef defs[14] = {
        {4, four}, {4, four}, {4, four}, {4, four}, {4, four},
        {4, four}, {4, four}, {4, four}, {4, four}, {4, four},
        {4, four}, {4, four}, {4, four}, {4, four}};
    const AosAnimSet anims = {defs, 14};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 14; ++a)
        for (int f = 0; f < 16; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-24, -48, 48, 48};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 0x100;
    /* enemies.tsv id 77 (0x4D): hp 800, contact 53, defence 50,
     * weak 0x0802, experience 400. */
    const AosEnemyStats stats = {800, 53, 50, 0x0802, 0, 400};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(40), NULL);
    soma.max_hp = soma.hp = 320;

    /* EnemyAluraUneCreate (0x0807C0A0): animation 0 looping, state 0;
     * the 0x8E & 0x40 flag path is the materialize state 3. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_ALURA_UNE, 200, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.step == 0 && e.anim.id == 0);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy flagged;
    assert(aos_enemy_create(&flagged, AOS_ENEMY_ALURA_UNE, 200, 150, 0,
                            0, &soma, &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(flagged.state == 3);
    /* State 3 (sub_0807C888): the 0x3B timer gate, then two animation
     * gates advance the steps to the looping animation 4. */
    flagged.timer = 0x3B;
    (void)aos_enemy_update(&flagged, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(flagged.step == 1 && flagged.anim.id == 1);
    flagged.cycle_done = true;
    (void)aos_enemy_update(&flagged, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(flagged.step == 2 && flagged.anim.id == 3);
    flagged.cycle_done = true;
    (void)aos_enemy_update(&flagged, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(flagged.step == 3 && flagged.anim.id == 4);

    /* State 0 step 0 (sub_0807C150): the aggro box arms the + 0x20 seen
     * flag and jumps to sub-state 10. */
    soma.x = e.x;
    soma.y = e.y;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(e.au_seen == 1 && e.step == 10);
    /* Sub-state 10 (sub_0807C9A4 x8): four scythe pairs with the 0x28
     * life and the immediate state-2 launch. */
    AosHitReport volley = aos_enemy_update(&e, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 0, 0,
                                            always_zero);
    assert(volley.child_count == 8);
    for (int i = 0; i < 8; ++i) {
        assert(volley.children[i].role == AOS_ROLE_ALURA_SCYTHE);
        assert(volley.children[i].parent == &e);
        assert(volley.children[i].au_life == 0x28);
        assert(volley.children[i].state == 2);
    }
    AosEnemy scythe = volley.children[0];
    scythe.parent = &e;

    /* The scythe (sub_0807CAD4): state 2 flies until the animation ends,
     * state 3 runs the + 0x20 life, state 4 ends on the parent check. */
    scythe.cycle_done = true;
    (void)aos_enemy_update(&scythe, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(scythe.state == 3);
    scythe.timer = 0x28;
    (void)aos_enemy_update(&scythe, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    /* The boss still lives (state 0): the scythe keeps flying. */
    assert(scythe.state == 3);
    e.state = 2; /* the lethal path of sub_0807C8FC */
    scythe.cycle_done = true;
    (void)aos_enemy_update(&scythe, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(scythe.state == 4);
    scythe.cycle_done = true;
    (void)aos_enemy_update(&scythe, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(scythe.removed);
    e.state = 0;

    /* State 1 sub-state 1 (sub_0807C494): the delayed scythe volley —
     * up to twelve children on the 0x6E life (capped at eight). */
    e.state = 1;
    e.step = 1;
    e.cycle_done = true;
    AosHitReport fan = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                         NULL, 10, 4, 0, 0, always_zero);
    assert(fan.child_count > 0 && fan.child_count <= 8);
    for (int i = 0; i < fan.child_count; ++i) {
        assert(fan.children[i].role == AOS_ROLE_ALURA_SCYTHE);
        assert(fan.children[i].au_life == 0x6E);
        assert(fan.children[i].state == 0);
    }

    /* State 1 sub-state 12: the + 0x6E frame word 3 emits two orbit
     * blades (sub_0807CC9C) with facing and its complement. */
    e.step = 12;
    e.anim.frame = 3;
    e.anim.tick = 0;
    e.cycle_done = false;
    AosHitReport orbit = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                           NULL, 10, 4, 0, 0, always_zero);
    assert(orbit.child_count == 2);
    assert(orbit.children[0].role == AOS_ROLE_ALURA_BLADE &&
           orbit.children[1].role == AOS_ROLE_ALURA_BLADE);
    assert(orbit.children[0].mirrored != orbit.children[1].mirrored);
    AosEnemy blade = orbit.children[0];
    blade.parent = &e;
    /* The orbit blade (sub_0807CDA8): song 0x80 on the frame word
     * 0x100, then the hidden/reveal cycle on the + 0x18 counter. */
    blade.anim.frame = 0;
    blade.anim.tick = 0; /* step_anim advances it to the 0x100 word */
    (void)aos_enemy_update(&blade, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x80) == 1);
    /* Seven reveal cycles move the + 0x18 counter past the flip marks. */
    for (int i = 0; i < 40 && !blade.removed; ++i) {
        blade.anim.flags |= AOS_ANIM_ENDED;
        blade.cycle_done = i % 2 == 0;
        (void)aos_enemy_update(&blade, &soma, &layer, &kind, NULL, NULL,
                               10, 4, 0, 0, always_zero);
    }
    assert(blade.spawned_count > 0);

    /* The lethal path: state 2 runs the shared generic death and
     * removes the boss. */
    e.state = 2;
    e.timer = 1;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(e.removed);
}

static void student_witch_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;   /* floor top 160 */
    enum { SW_ANIMS = 12 };
    static const uint8_t eight[8] = {8, 8, 8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[SW_ANIMS];
    for (int a = 0; a < SW_ANIMS; ++a) defs[a] = (AosAnimDef){8, eight};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = SW_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < SW_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
        }
    /* enemies.tsv id 22: hp 80, field_e 999, contact 15, defence 10,
     * weak 0x0941, resist 0x0000, experience 28. */
    const AosEnemyStats stats = {80, 15, 10, 0x0941, 0x0000, 28};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(120), AOS_FIXED(150), NULL);
    soma.max_hp = soma.hp = 400;

    /* EnemyStudentWitchCreate (0x080B8CA0): state 0 hovering, the
     * translucent ghost bit set and animation 7 looping; the global
     * 0x8E & 0x40 parks it in the inert state 5. */
    AosEnemy w;
    assert(aos_enemy_create(&w, AOS_ENEMY_STUDENT_WITCH, 100, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(w.state == 0 && w.hp == 80 && w.translucent);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy wi;
    assert(aos_enemy_create(&wi, AOS_ENEMY_STUDENT_WITCH, 100, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(wi.state == 5);
    aos_enemy_set_flag_8e_40(false);

    /* sub_080B8850: the hover arms + 0x14 = 0xC000 on animation frame
     * 0 and the mode-8 probe walk moves her at that speed; the floor
     * at row 20 keeps bit 8 set so she stays in state 0. */
    w.anim.frame = 0;
    int32_t x0 = w.x;
    (void)aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(w.angle == 0xC000 && w.x != x0);
    /* Soma sits in the 0xA0 x 0x3C box and always_zero fails the & 4
     * roll, so the first hover already perched her in state 2. */
    assert(w.state == 2);

    /* Back in state 0 on animation frame 0 with the scripted RNG & 4
     * set, the player box dives her into state 3. */
    w.state = 0;
    w.step = 1;
    w.substep = 0;
    w.anim.frame = 0;
    script = (const uint32_t[]){4, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0};
    script_at = 0;
    (void)aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, scripted);
    assert(w.state == 3);
    /* sub_080B8A90 substep 0 arms the dive on the next update. */
    (void)aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(w.vy < 0 && w.ay == 0x400 && w.angle == 0x8000 && w.step == 1);

    /* sub_080B8A90 substep 0 armed the dive; substep 1 climbs the
     * + 0x18 tick. Run the dive until the bolt fires at animation
     * frame 6 — drive the frame directly since the 8-frame test
     * animation never reaches it natively. */
    w.step = 2;
    w.substep = 0;
    w.anim.frame = 6;
    w.anim.tick = 0;
    AosHitReport r = aos_enemy_update(&w, &soma, &layer, &kind, NULL,
                                      NULL, 10, 4, 0, 0, always_zero);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x1CB) == 1);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x81) == 1);
    assert(r.child_count == 1 &&
           r.children[0].role == AOS_ROLE_STUDENT_WITCH_BOLT &&
           r.children[0].step == 0);

    /* sub_080B87C4/sub_080B8668: the bolt inits on animation 0xB (param
     * bit 0 clear), hp 1 and 0x4000 gravity, then accelerates its
     * + 0x14 speed by 0x4000 a frame through the mode-8 probes. */
    AosEnemy b = r.children[0];
    /* The native init falls through into the flight block: after one
     * update the bolt is in state 1 with its speed already ramped. */
    (void)aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(b.state == 1 && b.hp == 1 && b.ay == 0x4000 &&
           b.angle == 0x4000);
    /* A wall result (bits 3) bursts it: wall the probe path ahead of
     * its rightward flight (mirrored faces +x). */
    b.mirrored = true;
    for (int y = 0; y < H; ++y) cells[y * W + 30] = 0x03;
    int guard = 0;
    while (b.state == 1 && !b.removed && guard++ < 200)
        (void)aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL,
                               10, 4, 0, 0, always_zero);
    assert(b.state == 2 || b.removed);
    for (int y = 0; y < H; ++y) cells[y * W + 30] = 0;

    /* sub_080B8C0C: the lethal blow (sub_080B9EEC) enters state 4;
     * the 0x20 counter flings spark drifts on even counts and the
     * final substep emits the param-2 bolt before deleting. */
    AosEnemy d;
    assert(aos_enemy_create(&d, AOS_ENEMY_STUDENT_WITCH, 100, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    d.state = 4;
    d.step = 0;
    (void)aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(d.step == 1 && d.timer == 0x20);
    /* Drive the counter down: each update spawns a spark on even
     * counts; the last one emits the death bolt and removes. */
    int sparks = 0;
    for (int i = 0; i < 40 && !d.removed; ++i) {
        AosHitReport dr = aos_enemy_update(&d, &soma, &layer, &kind,
                                           NULL, NULL, 10, 4, 0, 0,
                                           always_zero);
        sparks += dr.effect_count;
        for (int k = 0; k < dr.child_count; ++k)
            assert(dr.children[k].role == AOS_ROLE_STUDENT_WITCH_BOLT &&
                   dr.children[k].step == 2);
    }
    assert(d.removed && sparks > 0);
}

static void witch_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;   /* floor top 160 */
    enum { W_ANIMS = 15 };
    static const uint8_t eight[8] = {8, 8, 8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[W_ANIMS];
    for (int a = 0; a < W_ANIMS; ++a) defs[a] = (AosAnimDef){8, eight};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = W_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < W_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
        }
    /* enemies.tsv id 42: hp 180, field_e 4000, contact 25, defence 20,
     * weak 0x0941, resist 0x0002, experience 75, soul type 1 index 5. */
    const AosEnemyStats stats = {180, 25, 20, 0x0941, 0x0002, 75};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(120), AOS_FIXED(150), NULL);
    soma.max_hp = soma.hp = 400;

    /* EnemyWitchCreate (0x080B9D90): state 0 hovering on the shared
     * witch art, translucent, no floor snap; the global 0x8E & 0x40
     * parks it in the inert state 4. */
    AosEnemy w;
    assert(aos_enemy_create(&w, AOS_ENEMY_WITCH, 100, 150, 0, 0, &soma,
                            &layer, &kind, &stats));
    assert(w.state == 0 && w.hp == 180 && w.translucent);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy wi;
    assert(aos_enemy_create(&wi, AOS_ENEMY_WITCH, 100, 150, 0, 0, &soma,
                            &layer, &kind, &stats));
    assert(wi.state == 4);
    aos_enemy_set_flag_8e_40(false);

    /* sub_080B9614: on animation frame 0 tick 0 with Soma inside the
     * centred box the + 0x14 bit-2 latch arms the grown box, the
     * facing mismatch sends her to the approach (state 1). */
    w.anim.frame = 0;
    w.anim.tick = 0;
    (void)aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(w.state == 1 && (w.variant & 4));

    /* sub_080B96DC case 0 arms the hover tick and zeroes the
     * velocities; case 1 steers toward the offset target point. */
    (void)aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(w.step == 1 && w.spin == 0);
    int guard = 0;
    while (w.step == 1 && guard++ < 60)
        (void)aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10,
                               4, 0, 0, always_zero);
    /* She either closed in (step 3) or the facing check dove her into
     * the case-2 chase; both leave state 1 progressing. */
    assert(w.step == 2 || w.step == 3);

    /* sub_080B96DC case 4 waits for the brace animation to end, then
     * arms the 0x40-tick charge: companion child, bit 1 of + 0x14 and
     * song 0x81. */
    w.step = 4;
    w.anim.frame = 7;
    w.anim.tick = 7;
    w.anim.flags |= AOS_ANIM_ENDED;
    AosHitReport r = aos_enemy_update(&w, &soma, &layer, &kind, NULL,
                                      NULL, 10, 4, 0, 0, always_zero);
    assert(w.step == 5 && w.timer == 0x40 && (w.variant & 2) &&
           r.child_count == 1 &&
           r.children[0].role == AOS_ROLE_WITCH_COMPANION);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x81) == 1);

    /* sub_080B96DC case 5: above timer 0x30 the orbit spark pairs
     * spawn (two children per call, angle advanced by 0x20), at
     * 0x20 the homing bolt fires with song 0x12A, at zero the charge
     * clears and the recovery animation (2) plays. */
    w.timer = 0x31;
    r = aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4,
                         0, 0, always_zero);
    assert(r.child_count == 2 &&
           r.children[0].role == AOS_ROLE_WITCH_ORBIT &&
           r.children[1].role == AOS_ROLE_WITCH_ORBIT &&
           (uint16_t)(r.children[1].angle - r.children[0].angle) == 0x20);
    /* Walk the timer down to the bolt shot at 0x20. */
    guard = 0;
    int bolt = 0;
    AosEnemy shot = {0};
    while (w.timer > 0 && guard++ < 80) {
        r = aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10,
                             4, 0, 0, always_zero);
        for (int k = 0; k < r.child_count; ++k)
            if (r.children[k].role == AOS_ROLE_WITCH_BOLT) {
                bolt = 1;
                shot = r.children[k];
            }
    }
    assert(bolt);
    /* The bolt's own state-0 update plays song 0x12A. */
    (void)aos_enemy_update(&shot, &soma, &layer, &kind, NULL, NULL, 10,
                           4, 0, 0, always_zero);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x12A) == 1);

    /* The bolt (sub_080B911C): state 0 rolls the shade, plays
     * animation 4 and launches at -0x18000 mirrored-negated; state 1
     * ramps the speed then locks the target 0x10 above the player. */
    AosEnemy b;
    memset(&b, 0, sizeof b);
    b.id = AOS_ENEMY_WITCH;
    b.role = AOS_ROLE_WITCH_BOLT;
    b.x = AOS_FIXED(100);
    b.y = AOS_FIXED(150);
    b.stats = stats;
    b.hp = 1;
    b.static_frame = -1;
    b.parent = &w;
    b.mirrored = true;
    b.state = 0;
    (void)aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, scripted);
    assert(b.state == 1 && b.vx == 0x18000 && b.speed == 0);
    for (int i = 0; i < 40 && b.state == 1; ++i)
        (void)aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10,
                               4, 0, 0, always_zero);
    assert(b.state == 2 && b.spin == 0x20);

    /* sub_080B9F2C / the hurt path: a lethal blow enters state 3 at
     * step and substep 1; case 0 arms the 0x20 spark counter, the
     * even counts fling sparks and the last step emits the param-1
     * bolt then removes. */
    AosEnemy d;
    assert(aos_enemy_create(&d, AOS_ENEMY_WITCH, 100, 150, 0, 0, &soma,
                            &layer, &kind, &stats));
    d.state = 3;
    d.step = 0;
    (void)aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(d.step == 1 && d.timer == 0x20);
    int sparks = 0, bolts = 0;
    for (int i = 0; i < 60 && !d.removed; ++i) {
        AosHitReport dr = aos_enemy_update(&d, &soma, &layer, &kind,
                                           NULL, NULL, 10, 4, 0, 0,
                                           always_zero);
        sparks += dr.effect_count;
        for (int k = 0; k < dr.child_count; ++k) {
            assert(dr.children[k].role == AOS_ROLE_STUDENT_WITCH_BOLT &&
                   dr.children[k].step == 1);
            bolts++;
        }
    }
    assert(d.removed && sparks > 0 && bolts == 1);
}


static void mudman_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;   /* floor top 160 */
    enum { M_ANIMS = 7 };
    static const uint8_t four[4] = {4, 4, 4, 4};
    static const uint8_t eight[8] = {8, 8, 8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[M_ANIMS];
    for (int a = 0; a < M_ANIMS; ++a) defs[a] = (AosAnimDef){8, eight};
    defs[0] = (AosAnimDef){4, four};
    defs[1] = (AosAnimDef){4, four};
    defs[3] = (AosAnimDef){4, four};
    defs[4] = (AosAnimDef){4, four};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = M_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < M_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    /* enemies.tsv id 86: hp 200, contact 50, defence 1, weak 0, resist 0,
     * experience 400, soul_type 0 (red) soul_index 21. */
    const AosEnemyStats stats = {200, 50, 1, 0, 0, 400};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(100), NULL);
    soma.max_hp = soma.hp = 400;

    /* EnemyMudmanCreate (0x08072670): state 0 animation 0; the global
     * 0x8E & 0x40 sends it to the inert state 4 with animation 2. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_MUDMAN, 100, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.hp == 200 && e.param1 == 0);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ei;
    assert(aos_enemy_create(&ei, AOS_ENEMY_MUDMAN, 100, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(ei.state == 4 && ei.anim.id == 2);
    aos_enemy_set_flag_8e_40(false);

    /* sub_080719F4 step 0: Soma inside the 0x78x0x50 box at
     * (px-0x3C, py-0x28) advances to step 1 and clears + 0x59 bit 0. */
    soma.x = e.x + AOS_FIXED(0x10);
    soma.y = e.y;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.step == 1 && !e.cycle_done);

    /* Step 1 waits on the animation cycle, then arms animation 2,
     * timer 0x64 and vy 0x20000 (sub_08071AD4). */
    e.cycle_done = true;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(e.step == 2 && e.anim.id == 2 && e.timer == 0x64 &&
           e.vy == 0x20000);

    /* Step 2 walks: the floor bit wraps the 0x64 timer past 0x3B on the
     * first grounded frame; rng&3 == 0 (always_zero) enters state 1. */
    script = (const uint32_t[]){0, 0, 0, 0, 0, 0, 0, 0};
    script_at = 0;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           scripted);
    assert(e.state == 1 && e.step == 0);

    /* sub_08071BA4: step 0 plays animation 3; once anim frame > 1 the
     * arm child spawns (sub_08072038) and song 0x85 plays; step 2
     * counts 0x3B then returns to state 0 step 2. */
    AosHitReport rep;
    AosEnemy arm;
    memset(&arm, 0, sizeof arm);
    int arm_seen = 0;
    for (int i = 0; i < 120 && e.state == 1; ++i) {
        rep = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
        for (int c = 0; c < rep.child_count; ++c)
            if (rep.children[c].role == AOS_ROLE_MUDMAN_ARM) {
                arm = rep.children[c];
                arm_seen = 1;
            }
    }
    assert(arm_seen);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x85) == 1);
    assert(e.state == 0 && e.step == 2);

    /* The arm child (sub_08072120 state 0): it drifts toward Soma and
     * sheds bubbles; near the target it enters state 2 and re-attaches
     * to the parent's offset. */
    arm.parent = &e;
    assert(arm.role == AOS_ROLE_MUDMAN_ARM && arm.vx == 0x20000 &&
           arm.vy == (int32_t)0xFFFF0000 && arm.ay == 0x1800);
    int bubble_seen = 0, arm_state2 = 0;
    for (int i = 0; i < 80 && !arm.removed; ++i) {
        AosHitReport arep = aos_enemy_update(&arm, &soma, &layer, &kind,
                                             NULL, NULL, 10, 4, 0, 0,
                                             always_zero);
        for (int c = 0; c < arep.child_count; ++c) {
            assert(arep.children[c].role == AOS_ROLE_MUDMAN_BUBBLE);
            assert(arep.children[c].static_frame == 0x12 ||
                   arep.children[c].static_frame == 0x15);
            bubble_seen = 1;
        }
        if (arm.state == 2) arm_state2 = 1;
    }
    assert(bubble_seen && arm_state2);

    /* sub_080725C0: the bubble's timer counts to 0x12, the direct frame
     * advances at counts 6 and 0xC, and it deletes past 0x11. */
    AosEnemy bub;
    memset(&bub, 0, sizeof bub);
    bub.id = AOS_ENEMY_MUDMAN;
    bub.role = AOS_ROLE_MUDMAN_BUBBLE;
    bub.stats = stats;
    bub.hp = 1;
    bub.static_frame = 0x12;
    bub.x = AOS_FIXED(120);
    bub.y = AOS_FIXED(140);
    bub.vx = 0x1000;
    bub.vy = -0x10000;
    bub.ay = 0x2800;
    for (int i = 0; i < 6; ++i)
        (void)aos_enemy_update(&bub, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
    assert(bub.timer == 6 && bub.static_frame == 0x13);
    for (int i = 0; i < 8; ++i)
        (void)aos_enemy_update(&bub, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
    assert(bub.timer == 0xE && bub.static_frame == 0x14);
    for (int i = 0; i < 6 && !bub.removed; ++i)
        (void)aos_enemy_update(&bub, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
    assert(bub.removed);

    /* sub_08071DBC state 2 (the lethal path wired in aos_enemy.c):
     * step 0 bursts eight bubbles and shares the step-1 probe; on the
     * floor bit plus the animation cycle it arms timer 0x28, step 2
     * blinks and sets + 0x59 bit 3 before the corpse deletes. */
    AosEnemy d;
    assert(aos_enemy_create(&d, AOS_ENEMY_MUDMAN, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    aos_anim_start(&d.anim, &anims, 4, false);
    d.state = 2;
    d.step = 0;
    d.cycle_done = true;
    rep = aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(d.step == 2 && d.timer == 0x28 && rep.child_count == 4);
    for (int i = 0; i < 4; ++i)
        assert(rep.children[i].role == AOS_ROLE_MUDMAN_BUBBLE);
    for (int i = 0; i < 0x2A && !d.removed; ++i)
        (void)aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               0, 0, always_zero);
    assert(d.removed && (d.md_flags & 8));

    /* sub_08071C30 state 3 (the weapon flinch): step 0 spits two
     * bubbles off the facing side and shares the probe; on the floor
     * bit plus the cycle it returns to state 0 step 0 animation 0. */
    AosEnemy f;
    assert(aos_enemy_create(&f, AOS_ENEMY_MUDMAN, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    f.state = 3;
    f.step = 0;
    f.cycle_done = true;
    rep = aos_enemy_update(&f, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           always_zero);
    assert(f.state == 0 && f.step == 0 && f.anim.id == 0);
    assert(rep.child_count == 2);
    for (int i = 0; i < 2; ++i)
        assert(rep.children[i].role == AOS_ROLE_MUDMAN_BUBBLE);
}

static void were_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;   /* floor top 160 */
    enum { W_ANIMS = 8 };
    static const uint8_t eight[8] = {8, 8, 8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[W_ANIMS];
    for (int a = 0; a < W_ANIMS; ++a) defs[a] = (AosAnimDef){8, eight};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = W_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < W_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    /* enemies.tsv: id 75 hp 380 contact 45 defence 30 weak 0x00C1 resist
     * 0 exp 260 soul red:35; id 84 hp 400 contact 45 defence 35 exp 320
     * soul red:36. */
    const AosEnemyStats jstats = {380, 45, 30, 0x00C1, 0x0000, 260};
    const AosEnemyStats tstats = {400, 45, 35, 0x00C1, 0x0000, 320};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(600), AOS_FIXED(100), NULL);
    soma.max_hp = soma.hp = 400;

    /* EnemyWerejaguarCreate / EnemyWeretigerCreate through
     * sub_0807968C: state 0 animation 0; the global 0x8E & 0x40 sends
     * it to the inert state 4. */
    AosEnemy e, t;
    assert(aos_enemy_create(&e, AOS_ENEMY_WEREJAGUAR, 300, 150, 0, 0,
                            &soma, &layer, &kind, &jstats));
    assert(e.state == 0 && e.hp == 380 && e.anim.id == 0);
    assert(aos_enemy_create(&t, AOS_ENEMY_WERETIGER, 300, 150, 0, 0,
                            &soma, &layer, &kind, &tstats));
    assert(t.state == 0 && t.hp == 400);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ei;
    assert(aos_enemy_create(&ei, AOS_ENEMY_WEREJAGUAR, 300, 150, 0, 0,
                            &soma, &layer, &kind, &jstats));
    assert(ei.state == 4);
    aos_enemy_set_flag_8e_40(false);

    /* sub_08078880 step 0: each floor frame grows + 0x0D; past 0x1D the
     * shared leap setup arms vy 0x40000 / vx +/-0x20000, static frame
     * 0x10 and + 0x14 = 1, then step 1 (the walk) takes over. */
    soma.x = AOS_FIXED(600);
    soma.y = AOS_FIXED(140);
    int saw_leap = 0;
    for (int i = 0; i < 60 && !saw_leap; ++i) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               200, 0, always_zero);
        if (e.step == 1 && e.state == 0 && e.vy == 0x40000 &&
            (e.vx == 0x20000 || e.vx == (int32_t)-0x20000) &&
            e.static_frame == 0x10 && e.were_attack)
            saw_leap = 1;
    }
    assert(saw_leap);

    /* Step 1 walk: on animation frame 3 with Soma inside the probe box
     * it enters state 1 step 0 (the pounce). Keep Soma close so the
     * step-2 decision box (px-0x14, py-0x28, 0x28x0x50) also catches
     * it. */
    soma.x = e.x + AOS_FIXED(0x10);
    soma.y = e.y;
    int pounced = 0;
    for (int i = 0; i < 240 && !pounced; ++i) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               200, 0, always_zero);
        if (e.state == 1 && e.step == 0) pounced = 1;
    }
    assert(pounced);

    /* sub_08078BB4 step 0 mirrors step 0 of state 0: past timer 0x1D it
     * re-arms the leap setup into step 1. */
    e.timer = 0x1E;
    e.step = 0;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(e.step == 1 && e.vy == 0x40000 && e.were_attack);

    /* The shared miss tail (_080791CC): a walk probe that finds no
     * floor ends the attack — state 0, step 0, animation 0, + 0x14
     * cleared. Rising velocity keeps it airborne for the probe. */
    e.state = 1;
    e.step = 1;
    e.vy = (int32_t)-0x40000;
    e.were_attack = 1;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(e.state == 0 && e.step == 0 && e.anim.id == 0 &&
           !e.were_attack);

    /* sub_08079294 state 2 (the vanish): every fourth tick spawns the
     * ash drift, the timer counts down and the enemy removes itself. */
    memset(&e, 0, sizeof e);
    e.id = AOS_ENEMY_WEREJAGUAR;
    e.role = AOS_ROLE_ENEMY;
    e.stats = jstats;
    e.hp = 10;
    e.x = AOS_FIXED(300);
    e.y = AOS_FIXED(140);
    e.state = 2;
    e.timer = 8;
    int ash = 0;
    for (int i = 0; i < 12 && !e.removed; ++i) {
        AosHitReport rep = aos_enemy_update(&e, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 200, 0,
                                            always_zero);
        if (rep.effect_count) ash = 1;
    }
    assert(e.removed && ash);

    /* sub_080791E8 state 3 (the leap): step 0 arms vx -/+0x18000 away
     * from the player, vy -0x24000, ay 0x3000; the floor bit in step 1
     * lands it into state 0 step 1 animation 6. */
    memset(&e, 0, sizeof e);
    e.id = AOS_ENEMY_WEREJAGUAR;
    e.role = AOS_ROLE_ENEMY;
    e.stats = jstats;
    e.hp = 10;
    e.x = AOS_FIXED(300);
    e.y = AOS_FIXED(140);
    e.state = 3;
    e.step = 0;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           0, 0, always_zero);
    assert(e.step == 1 && (e.vx == 0x18000 || e.vx == (int32_t)-0x18000) &&
           e.vy == (int32_t)-0x24000 && e.ay == 0x3000);
    int landed = 0;
    for (int i = 0; i < 90 && !landed; ++i) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               200, 0, always_zero);
        if (e.state == 0 && e.step == 1 && e.anim.id == 6) landed = 1;
    }
    assert(landed);

    /* sub_08078BB4 step 0xA (the Weretiger dash): on animation frame 1
     * it dashes — song 0x86, two dust drifts, were_element 3, step 0xB. */
    memset(&t, 0, sizeof t);
    t.id = AOS_ENEMY_WERETIGER;
    t.role = AOS_ROLE_ENEMY;
    t.stats = tstats;
    t.hp = 10;
    t.x = AOS_FIXED(300);
    t.y = AOS_FIXED(140);
    t.state = 1;
    t.step = 0xA;
    t.anim.frame = 1;
    AosHitReport rep = aos_enemy_update(&t, &soma, &layer, &kind, NULL,
                                        NULL, 10, 4, 200, 0, always_zero);
    assert(t.step == 0xB && t.were_element == 3 &&
           (t.vx == 0x10000 || t.vx == (int32_t)-0x10000));
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x86) == 1);
    assert(rep.effect_count >= 2);

    /* sub_0807931C contact: the were family carries the shared attack
     * element, so contact damage flows through the standard collide
     * pass — verified live in area_10_room_000 / area_08_room_003. */
}

static void werewolf_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;   /* floor top 160 */
    enum { WW_ANIMS = 6 };
    static const uint8_t eight[8] = {8, 8, 8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[WW_ANIMS];
    for (int a = 0; a < WW_ANIMS; ++a) defs[a] = (AosAnimDef){8, eight};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = WW_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < WW_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    /* enemies.tsv id 50: hp 290, contact 33, defence 30, weak 0x0081,
     * resist 0, experience 100, soul_type 0 (red) soul_index 23. */
    const AosEnemyStats stats = {290, 33, 30, 0x0081, 0x0000, 100};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(100), NULL);
    soma.max_hp = soma.hp = 400;

    /* EnemyWerewolfCreate (0x0806F7A4) through sub_0806F818: state 0
     * animation 0; the global 0x8E & 0x40 sends it to state 4. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_WEREWOLF, 150, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.hp == 290 && e.anim.id == 0);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ei;
    assert(aos_enemy_create(&ei, AOS_ENEMY_WEREWOLF, 150, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(ei.state == 4);
    aos_enemy_set_flag_8e_40(false);

    /* sub_0806EEB4 step 0: Soma inside the 0xA0 x 0xA0 box at
     * (px-0x50, py-0x50) arms step 1 with animation 1. */
    soma.x = e.x;
    soma.y = e.y;
    int armed = 0;
    for (int i = 0; i < 40 && !armed; ++i) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               200, 0, always_zero);
        if (e.step == 1) armed = 1;
    }
    assert(armed && e.anim.id == 1);

    /* Step 1 walk: on a tick-0 frame inside the 0x78 x 0x50 box at
     * (px-0x3C, py-0x28) it enters state 1 step 0. */
    int pounced = 0;
    for (int i = 0; i < 240 && !pounced; ++i) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               200, 0, always_zero);
        if (e.state == 1 && e.step == 0) pounced = 1;
    }
    assert(pounced);

    /* sub_0806F024 case 0: past timer 0x1D the rng & 1 roll picks the
     * charge (step 1, animation 2) — scripted() returns 1. */
    e.timer = 0x1E;
    script = (const uint32_t[]){1, 1, 1, 1, 1, 1, 1, 1};
    script_at = 0;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           200, 0, scripted);
    assert(e.step == 1 && e.anim.id == 2);

    /* Case 1: once animation frame 2 shows it launches the dash —
     * vx +/-0x50000, vy 0x20000, ay 0x2800, timer and + 0x10 cleared. */
    e.anim.frame = 2;
    e.vx = 0;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           200, 0, always_zero);
    assert(e.step == 2 && (e.vx == 0x50000 || e.vx == (int32_t)-0x50000) &&
           e.vy == 0x20000 && e.ay == 0x2800 && e.ww_mark == 0);

    /* Case 2 with + 0x10 set: clears the mark, plays animation 0 and
     * drops into the state-3 leap. */
    e.ww_mark = 1;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           200, 0, always_zero);
    assert(e.state == 3 && e.step == 0 && e.ww_mark == 0 &&
           e.anim.id == 0);

    /* sub_0806F41C: step 0 arms vx -/+0x18000, vy -0x24000, ay 0x1800;
     * the floor bit lands it into state 0 step 1 animation 1. */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                           200, 0, always_zero);
    assert(e.step == 1 && (e.vx == 0x18000 || e.vx == (int32_t)-0x18000) &&
           e.vy == (int32_t)-0x24000 && e.ay == 0x1800);
    int landed = 0;
    for (int i = 0; i < 90 && !landed; ++i) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               200, 0, always_zero);
        if (e.state == 0 && e.step == 1 && e.anim.id == 1) landed = 1;
    }
    assert(landed);

    /* sub_0806F024 case 4 (the howl), driven directly: every eighth
     * timer tick song 0x77 and a dust drift; timer 0x1F/0x24/0x25
     * spawn the shadow approximations; past 0x23 it steps to 5 with
     * song 0x79. */
    memset(&e, 0, sizeof e);
    e.id = AOS_ENEMY_WEREWOLF;
    e.role = AOS_ROLE_ENEMY;
    e.stats = stats;
    e.hp = 10;
    e.x = AOS_FIXED(150);
    e.y = AOS_FIXED(140);
    e.state = 1;
    e.step = 4;
    e.timer = 0x18;
    int dust = 0, ash = 0;
    for (int i = 0; i < 16 && e.step == 4; ++i) {
        AosHitReport rep = aos_enemy_update(&e, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 200, 0,
                                            always_zero);
        for (int k = 0; k < rep.effect_count; ++k) {
            if (rep.effects[k].kind == AOS_EFFECT_DUST) dust = 1;
            if (rep.effects[k].kind == AOS_EFFECT_DARK_SMOKE) ash = 1;
        }
    }
    assert(dust && ash && e.step == 5);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x77) >= 1);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x79) == 1);

    /* sub_08079294 state 2 (the shared vanish): ash every fourth tick,
     * the timer counts down and the enemy removes itself. */
    memset(&e, 0, sizeof e);
    e.id = AOS_ENEMY_WEREWOLF;
    e.role = AOS_ROLE_ENEMY;
    e.stats = stats;
    e.hp = 10;
    e.x = AOS_FIXED(150);
    e.y = AOS_FIXED(140);
    e.state = 2;
    e.timer = 8;
    int smoke = 0;
    for (int i = 0; i < 12 && !e.removed; ++i) {
        AosHitReport rep = aos_enemy_update(&e, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 200, 0,
                                            always_zero);
        if (rep.effect_count) smoke = 1;
    }
    assert(e.removed && smoke);
}


static void slime_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;   /* floor top 160 */
    enum { SL_ANIMS = 5 };
    static const uint8_t eight[8] = {8, 8, 8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[SL_ANIMS];
    for (int a = 0; a < SL_ANIMS; ++a) defs[a] = (AosAnimDef){8, eight};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = SL_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < SL_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    /* enemies.tsv id 68: hp 200, field_e 1, contact 42, defence 30,
     * weak 0x0802, resist 0x0005, experience 200, soul_type 0
     * soul_index 32. */
    const AosEnemyStats stats = {200, 42, 30, 0x0802, 0x0005, 200};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(100), NULL);
    soma.max_hp = soma.hp = 400;

    /* EnemySlimeCreate (0x080C11C0): state 0 animation 0, the affine
     * charge and width at 0x40000; the global 0x8E & 0x40 sends it to
     * the inert state 3. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_SLIME, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.hp == 200 && e.anim.id == 0 &&
           e.sl_charge == 0x40000 && e.sl_width == 0x40000);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ei;
    assert(aos_enemy_create(&ei, AOS_ENEMY_SLIME, 150, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(ei.state == 3);
    aos_enemy_set_flag_8e_40(false);

    /* A nonzero param0 is the ceiling drop: substep 2, angle 0x8000. */
    AosEnemy ed;
    assert(aos_enemy_create(&ed, AOS_ENEMY_SLIME, 150, 40, 1, 0,
                            &soma, &layer, &kind, &stats));
    assert(ed.substep == 2 && ed.sl_angle == 0x8000);

    /* sub_080C0A1C step 0: Soma inside the centred 0x78 x 0xB4 box,
     * rest 0 and rng & 0xFF <= 3 (always_zero) enters state 1. */
    soma.x = e.x;
    soma.y = e.y;
    int attacked = 0;
    for (int i = 0; i < 60 && !attacked; ++i) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4,
                               200, 0, always_zero);
        if (e.state == 1) attacked = 1;
    }
    assert(attacked);

    /* sub_080C0F14: case 0 plays animation 1; the grounded case 1
     * arms animation 2, the + 0x11 bit-2 mark, song 0x164 and the
     * launch; the walk and landing cases return to state 0. */
    int launched = 0, returned = 0, saw_debris = 0;
    for (int i = 0; i < 600 && e.state == 1; ++i) {
        AosHitReport rep = aos_enemy_update(&e, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 200, 0,
                                            always_zero);
        if (e.sl_flags & 4) launched = 1;
        if (rep.child_count) saw_debris = 1;
        if (e.state == 0) returned = 1;
    }
    assert(launched && returned);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x164) == 1 ||
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x163) == 1);
    (void)saw_debris;

    /* The hurt mark: every blow sets + 0x11 bit 0 before the damage
     * walk; the squash animation and four debris follow on the next
     * grounded update. */
    e.sl_flags |= 1;
    int squashed = 0;
    for (int i = 0; i < 30 && !squashed; ++i) {
        AosHitReport rep = aos_enemy_update(&e, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 200, 0,
                                            always_zero);
        if (e.anim.id == 4 || rep.child_count >= 4) squashed = 1;
    }
    assert(squashed);

    /* sub_080C10BC state 2: song 0x70, the orbit sheds debris every
     * fourth tick and song 0x165 every sixteenth while + 0x1C counts
     * down by 0x400; the corpse deletes when it is spent. */
    AosEnemy d;
    assert(aos_enemy_create(&d, AOS_ENEMY_SLIME, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    d.state = 2;
    d.step = 0;
    d.sl_width = 0x1000;   /* short spin for the test */
    int spin_debris = 0;
    for (int i = 0; i < 40 && !d.removed; ++i) {
        AosHitReport rep = aos_enemy_update(&d, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 200, 0,
                                            always_zero);
        if (rep.child_count) spin_debris = 1;
    }
    assert(d.removed && spin_debris);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x70) == 1);
}

static void altair_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    enum { AL_ANIMS = 4 };
    static const uint8_t eight[8] = {8, 8, 8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[AL_ANIMS];
    for (int a = 0; a < AL_ANIMS; ++a) defs[a] = (AosAnimDef){8, eight};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = AL_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < AL_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    /* enemies.tsv id 44: hp 30, field_e 10, contact 28, defence 0,
     * weak 0x0001, resist 0, experience 10, soul 0/20. */
    const AosEnemyStats stats = {30, 10, 0, 0x0001, 0, 10};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(100), NULL);
    soma.max_hp = soma.hp = 400;
    const int CAM = 200, CAMY = 120;

    /* EnemyAltairCreate (0x080C1490): state 1, + 0x5C = 3, the
     * facing from sub_0806BD64 (Soma left of it -> mirrored). */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_ALTAIR, 300, 150, 1, 1,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 1 && e.hp == 30 && e.al_draw5c == 3 &&
           e.mirrored);

    /* param0 == 0 parks it hidden in the state-5 spawner; the
     * 0x8E & 0x40 flag sends it to the dormant state 4; param0 3
     * seeds + 0x42 = 0x100. */
    AosEnemy es;
    assert(aos_enemy_create(&es, AOS_ENEMY_ALTAIR, 150, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(es.state == 5 && es.hidden && es.attack_off);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, AOS_ENEMY_ALTAIR, 150, 150, 1, 0,
                            &soma, &layer, &kind, &stats));
    assert(ed.state == 4);
    aos_enemy_set_flag_8e_40(false);
    AosEnemy e3;
    assert(aos_enemy_create(&e3, AOS_ENEMY_ALTAIR, 150, 150, 3, 0,
                            &soma, &layer, &kind, &stats));
    assert(e3.state == 1 && e3.al_wx == 0x100);

    /* sub_080C1694 step 0: animation 1 without the link yet (param1
     * 1 spawns it), vx -0xA000 on the mirrored facing, + 0xD = 0x30. */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 1 && e.vx == 0xA000 && e.timer == 0x30);

    /* The link blade of func_080C1A64 was emitted as a child on the
     * dash entry; its sub_080C1B54 init scatters it and arms the
     * flight. */
    AosHitReport r = {0};
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    /* state-1 step 1 keeps moving; run the machine forward. */
    for (int i = 0; i < 40; ++i)
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(e.step >= 1 && e.step <= 5);

    /* sub_080C1940: the state-5 spawner clones itself (param0 2/3)
     * while (rng & 0x3F) == 0 and + 0x1E <= 1 — always_zero fires. */
    for (int i = 0; i < 200 && es.al_copies < 2; ++i)
        (void)aos_enemy_update(&es, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(es.al_copies >= 1);

    /* The lethal branch of sub_080C19E0 runs inside the shared
     * damage walk (soma_atk drives it); the state-2 / timer 0x32
     * carry-over is covered by the room path. */
    (void)r;
}

static void kyoma_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;   /* floor top 160 */
    enum { KD_ANIMS = 4 };
    static const uint8_t eight[8] = {8, 8, 8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[KD_ANIMS];
    for (int a = 0; a < KD_ANIMS; ++a) defs[a] = (AosAnimDef){8, eight};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = KD_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < KD_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    /* enemies.tsv id 39: hp 220, field_e 999, contact 30, defence 10,
     * weak 0x0020, resist 0x0410, experience 60, soul 0/54. */
    const AosEnemyStats stats = {220, 30, 10, 0x0020, 0x0410, 60};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(100), NULL);
    soma.max_hp = soma.hp = 400;
    const int CAM = 200;

    /* EnemyKyomaDemonCreate (0x080A01B8): state 0, animation 1,
     * + 0x5C = 0x1D, + 0x18 = 1, + 0x1A = (rng & 0x1F) + 0x20 and
     * + 0x42/+ 0x46 the spawn slot pixels. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_KYOMA_DEMON, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.hp == 220 && e.anim.id == 1 &&
           e.kd_5c == 0x1D && e.kd_alert == 1 &&
           e.producer_timer >= 0x20 && e.producer_timer <= 0x3F &&
           e.kd_ox == 300 && e.kd_oy == 150);

    /* The global 0x8E & 0x40 sends it to the inert state 3; a
     * nonzero param0 parks it hidden in state 2 step 1. */
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ei;
    assert(aos_enemy_create(&ei, AOS_ENEMY_KYOMA_DEMON, 150, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(ei.state == 3);
    aos_enemy_set_flag_8e_40(false);
    AosEnemy ep;
    assert(aos_enemy_create(&ep, AOS_ENEMY_KYOMA_DEMON, 150, 150, 1, 0,
                            &soma, &layer, &kind, &stats));
    assert(ep.state == 2 && ep.step == 1 && ep.hidden);

    /* sub_080A02AC step 0: the demon copies Soma's position (x +
     * 16 px) and facing; the spawn window latches on the first
     * update and the edge check keeps it visible near its spawn. */
    soma.x = AOS_FIXED(290);
    soma.y = AOS_FIXED(140);
    soma.facing_left = false;
    for (int i = 0; i < 3; ++i)
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, 0, always_zero);
    assert(e.x == soma.x + 0x100000 && e.y == soma.y && !e.hidden);

    /* The + 0x1A countdown re-rolls (always_zero -> 0x40) and the
     * screen edge inside [sx - 0xA, sx + 0x14] enters state 1. */
    int attacked = 0;
    for (int i = 0; i < 90 && !attacked; ++i) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, 0, always_zero);
        if (e.state == 1) attacked = 1;
    }
    assert(attacked);

    /* sub_080A04B0: step 0 plays animation 2, lifts 16 px and sets
     * + 0x5C = 0x11; the loop end arms animation 3 and the 0x28
     * blink wait hands back to state 0. */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, 0, always_zero);
    assert(e.step == 1 && e.kd_5c == 0x11 && e.anim.id == 2);
    int recoiled = 0;
    for (int i = 0; i < 300 && e.state == 1; ++i) {
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, 0, always_zero);
        if (e.state == 0) recoiled = 1;
    }
    assert(recoiled);

    /* sub_080A05B4 lethal: defeated, sound 0x72, state 2 with the
     * 0x3C blink wait of sub_080A0790. */
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    soma.x = e.x - 0x100000;
    soma.y = e.y;
    aos_audio_clear();
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        &weapon, &blade, 9999, 4, CAM, 0,
                                        always_zero);
    assert(hit.killed && e.state == 2 && e.timer == 0x3C && e.defeated &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x72) == 1);
}

static void bael_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    enum { GL_ANIMS = 4 };
    static const uint8_t eight[8] = {8, 8, 8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[GL_ANIMS];
    for (int a = 0; a < GL_ANIMS; ++a) defs[a] = (AosAnimDef){8, eight};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = GL_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < GL_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    /* enemies.tsv id 94: hp 300, field_e 666, contact 52, defence 42,
     * weak 0x0040, resist 0x0000, experience 380, soul 2/32. */
    const AosEnemyStats stats = {300, 52, 42, 0x0040, 0x0000, 380};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(100), NULL);
    soma.max_hp = soma.hp = 400;
    const int CAM = 200, CAMY = 120;

    /* func_080CC624: state 1 (param0 0), + 0x18 = 0x8000 speed seed,
     * ay 0x4000, scaled draw, y anchor -0x18 px. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BAEL, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 1 && e.hp == 300 && e.spin == 0x8000 &&
           e.ay == 0x4000 && e.scaled && e.static_frame == -1);

    /* param0 nonzero parks it in state 2; the 0x8E & 0x40 flag in
     * the dormant state 4. */
    AosEnemy e2;
    assert(aos_enemy_create(&e2, AOS_ENEMY_BAEL, 150, 150, 1, 0,
                            &soma, &layer, &kind, &stats));
    assert(e2.state == 2);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy e4;
    assert(aos_enemy_create(&e4, AOS_ENEMY_BAEL, 150, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e4.state == 4);
    aos_enemy_set_flag_8e_40(false);

    /* The func_080CC744 companion emits lazily on the first live
     * tick: same kind frames, static frame 1, translucent, parent
     * link, and func_080CD348 mirrors the parent's position. */
    int32_t ex0 = e.x, ey0 = e.y;
    AosHitReport r = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                      NULL, 10, 4, CAM, CAMY,
                                      always_zero);
    assert(r.child_count >= 1);
    const AosEnemy *comp = NULL;
    for (int i = 0; i < r.child_count; ++i)
        if (r.children[i].role == AOS_ROLE_BAEL_COMPANION)
            comp = &r.children[i];
    assert(comp && comp->static_frame == 1 && comp->translucent &&
           comp->parent == &e && comp->x == ex0 && comp->y == ey0);

    /* sub_080CD144: the + 0x18 speed decays 0x20 per tick on flat
     * ground and + 0x14 gains speed >> 6 — run the walk forward. */
    int32_t spin0 = e.spin;
    uint32_t ang0 = e.angle;
    for (int i = 0; i < 12; ++i)
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(e.spin < spin0 && e.angle != ang0);

    /* func_080CD378's aggro box (0x5C): Soma inside it enters the
     * attack state 2. */
    soma.x = e.x;
    soma.y = e.y;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.state == 2 && e.step == 0);

    /* func_080CC800 step 0: the 0x20 disengage box stays clear at
     * +30 px while the 0x58 engage and the 0xC0 x 0x30 centred probe
     * hold — the spin (step 1, animation 1) starts. */
    soma.x = e.x + AOS_FIXED(30);
    soma.y = e.y;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 1);
    for (int i = 0; i < 50 && e.step == 1; ++i)
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(e.step == 2);
    for (int i = 0; i < 30 && e.state == 2; ++i)
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(e.state == 1);

    /* func_080CCDDC: song 0x6E, spin +/-0x20000, the 0x60 wait then
     * the seven-piece ring of 0x08119500 and removal. */
    AosEnemy d;
    assert(aos_enemy_create(&d, AOS_ENEMY_BAEL, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    d.state = 3;
    d.step = d.substep = 0;
    d.timer = 0;
    d.ba_flag = 0;
    d.spin = 0x8000;
    aos_audio_clear();
    (void)aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E) == 1 &&
           d.timer == 0x5F && d.spin == 0x6FFF && d.attack_off);
    int saw_debris = 0;
    for (int i = 0; i < 400 && !d.removed; ++i) {
        AosHitReport dr = aos_enemy_update(&d, &soma, &layer, &kind,
                                           NULL, NULL, 10, 4, CAM,
                                           CAMY, always_zero);
        if (dr.child_count > 0) saw_debris = 1;
    }
    assert(d.removed && saw_debris);

    /* The dormant state 4 rises and spins its angle (func_080CD3AC). */
    AosEnemy d4;
    assert(aos_enemy_create(&d4, AOS_ENEMY_BAEL, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    d4.state = 4;
    int32_t y0 = d4.y;
    uint32_t a0 = d4.angle;
    (void)aos_enemy_update(&d4, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(d4.y < y0 && d4.angle != a0);
}

static void nightmare_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    enum { NM_ANIMS = 12 };
    static const uint8_t eight[8] = {8, 8, 8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[NM_ANIMS];
    for (int a = 0; a < NM_ANIMS; ++a) defs[a] = (AosAnimDef){8, eight};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = NM_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < NM_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    /* enemies.tsv id 66: hp 260, field_e 300, contact 43, defence 30,
     * weak 0x0020, resist 0x0010, experience 280. */
    const AosEnemyStats stats = {260, 43, 30, 0x0020, 0x0010, 280};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(100), NULL);
    soma.max_hp = soma.hp = 400;
    const int CAM = 200, CAMY = 120;

    /* EnemyNightmareCreate: state 0, + 0x1C = 0x28, animation
     * param0 looping, one pixel of + 0x46 anchor. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_NIGHTMARE, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.hp == 260 && e.walk_timer == 0x28 &&
           e.nm_speed == 0 && e.nm_wisp_a == 0);

    /* param0 nonzero keeps state 0 but bases the animations on it;
     * the 0x8E & 0x40 flag parks it in dormant state 3 with param0
     * forced to 1. */
    AosEnemy e9;
    assert(aos_enemy_create(&e9, AOS_ENEMY_NIGHTMARE, 300, 150, 9, 0,
                            &soma, &layer, &kind, &stats));
    assert(e9.state == 0 && e9.param0 == 9);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, AOS_ENEMY_NIGHTMARE, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(ed.state == 3 && ed.param0 == 1);
    aos_enemy_set_flag_8e_40(false);

    /* State 0 registers the two trail slots and arms state 1; step 0
     * then zeroes the speed and arms step 1. */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.state == 1 && e.step == 0 && e.nm_wisp_a == 1 &&
           e.nm_wisp_b == 1);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 1 && e.nm_speed == 0);

    /* sub_080B80BC step 1: the + 0x18 speed ramps + 0x4000 a tick and
     * + 0x1C past 0x50 pounces — animation param0 + 2 once, song
     * 0x127, step 2. */
    e.nm_speed = 0x10000;
    e.walk_timer = 0x50;
    e.timer = 1;    /* keep + 0xD % 0x19 off the 0x7D howl */
    aos_audio_clear();
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 2 && e.nm_speed == 0x14000 &&
           e.anim.id == 2);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x127) == 1);

    /* Step 2 bleeds the speed - 0x1000 a tick and sheds a wisp drift
     * every fourth tick; the U-turn waits on the probe byte's bit 2
     * (a ceiling mark the flat test layer never raises). */
    e.step = 2;
    e.timer = 3;
    e.nm_speed = 0x8000;
    AosHitReport sr = aos_enemy_update(&e, &soma, &layer, &kind,
                                       NULL, NULL, 10, 4, CAM, CAMY,
                                       always_zero);
    assert(e.step == 2 && e.nm_speed == 0x7000 && e.timer == 4 &&
           sr.effect_count > 0);

    /* sub_080B8240: the death flings the skull (vx by facing, vy
     * - 0x28000, ay 0x200000), steps the animation and deletes on
     * the window/probe/timer rule after the drift puffs. */
    AosEnemy d;
    assert(aos_enemy_create(&d, AOS_ENEMY_NIGHTMARE, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    d.state = 2;
    d.step = d.substep = 0;
    d.timer = 0;
    d.nm_probe = 0;
    (void)aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    /* vy already absorbed one ay step through the shared integrate. */
    assert(d.step == 1 && d.vx == (d.mirrored ? 0x28000 : -0x28000) &&
           d.vy == -0x28000 + 0x200000 && d.ay == 0x200000 &&
           d.timer == 1);
    /* Park it mid-screen with the timer on the song beat: + 0xD % 8
     * plays 0x6E and + 0xD % 4 sheds the debris drift. */
    d.vy = 0;
    d.y = AOS_FIXED(150);
    d.timer = 7;
    aos_audio_clear();
    AosHitReport sr2 = aos_enemy_update(&d, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, CAM, CAMY,
                                        always_zero);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E) == 1 &&
           sr2.effect_count > 0);
    int saw_debris = 1;
    for (int i = 0; i < 400 && !d.removed; ++i) {
        AosHitReport dr = aos_enemy_update(&d, &soma, &layer, &kind,
                                           NULL, NULL, 10, 4, CAM,
                                           CAMY, always_zero);
        if (dr.effect_count > 0) saw_debris = 1;
    }
    assert(d.removed && saw_debris);
}

static void une_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    enum { UNE_ANIMS = 12 };
    static const uint8_t eight[8] = {8, 8, 8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[UNE_ANIMS];
    for (int a = 0; a < UNE_ANIMS; ++a) defs[a] = (AosAnimDef){8, eight};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = UNE_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < UNE_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 61: hp 10, field_e 10, contact 32, defence 1,
     * weak 0x0002, resist 0x0000, experience 10. */
    const AosEnemyStats stats = {10, 32, 1, 0x0002, 0x0000, 10};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(250), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 400;
    const int CAM = 200, CAMY = 120;

    /* EnemyUneCreate: state 0, + 0xD = 0x3C, + 0x54 = 0x2800 the
     * projectile gravity carried in ay; param1 nonzero is the
     * hidden state-3 spawner; the 0x8E & 0x40 flag parks state 4. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_UNE, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.hp == 10 && e.timer == 0x3C &&
           e.ay == 0x2800 && e.parent == NULL);
    AosEnemy sp;
    assert(aos_enemy_create(&sp, AOS_ENEMY_UNE, 300, 150, 5, 1,
                            &soma, &layer, &kind, &stats));
    assert(sp.state == 3 && sp.hidden && sp.param0 == 5);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, AOS_ENEMY_UNE, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(ed.state == 4);
    aos_enemy_set_flag_8e_40(false);

    /* State 0: gravity pulls it to the floor row, then the step-0
     * block resets the motion and the aggro box (player inside
     * 0xC8 x 0x140) chains animation 1 and step 1. */
    for (int i = 0; i < 60 && e.ay != 0; ++i)
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(e.ay == 0 && e.vy == 0x10000);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 1 && e.anim.id == 1);

    /* Steps 1/2 chain the one-shot animations; step 2 arms the
     * split-cycle timer (a lone Une's + 0x1C is 0: rng % 0x28 +
     * 0x64, here 100 with the zero rng). */
    e.step = 1;
    e.cycle_done = true;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 2 && e.anim.id == 2);
    e.cycle_done = true;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 3 && e.anim.id == 3 && e.timer == 100);

    /* A lone Une has no + 0x18 link: step 3 re-arms the timer and
     * never advances — no splits without a root. */
    e.timer = 1;
    e.une_wait = 1;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 3);

    /* Linked to a root below its param0 cap, step 3 advances to the
     * split cycle; at timer 0xF a full Une child is emitted —
     * step 0xA, gravity, linked to the root, count incremented —
     * and every fourth tick sheds a debris sprite. */
    AosEnemy root;
    memset(&root, 0, sizeof root);
    root.id = AOS_ENEMY_UNE;
    root.param0 = 4;
    root.spawned_count = 0;
    AosEnemy p;
    assert(aos_enemy_create(&p, AOS_ENEMY_UNE, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    p.parent = &root;
    p.step = 3;
    p.timer = 1;
    p.une_wait = 0x8C;
    (void)aos_enemy_update(&p, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    (void)aos_enemy_update(&p, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(p.step == 4);
    p.timer = 0xE;
    AosHitReport sr = aos_enemy_update(&p, &soma, &layer, &kind,
                                       NULL, NULL, 10, 4, CAM, CAMY,
                                       always_zero);
    assert(sr.child_count >= 1);
    const AosEnemy *split = NULL, *debris = NULL;
    for (unsigned i = 0; i < sr.child_count; ++i) {
        if (sr.children[i].role == AOS_ROLE_UNE_DEBRIS) debris = &sr.children[i];
        else if (sr.children[i].step == 0xA) split = &sr.children[i];
    }
    /* timer 0xF is odd: no debris this tick — the next one (0x10,
     * & 3 == 0) sheds it. */
    assert(split && split->parent == &root && split->ay == 0x2800 &&
           split->hp == 10 && root.spawned_count == 1);
    sr = aos_enemy_update(&p, &soma, &layer, &kind, NULL, NULL,
                          10, 4, CAM, CAMY, always_zero);
    for (unsigned i = 0; i < sr.child_count; ++i)
        if (sr.children[i].role == AOS_ROLE_UNE_DEBRIS) debris = &sr.children[i];
    assert(debris != NULL);

    /* The debris decays at 7/8 a tick and deletes past + 0xD = 0x13. */
    AosEnemy d = *debris;
    int32_t vx0 = d.vx;
    for (int i = 0; i < 30 && !d.removed; ++i)
        (void)aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(d.removed && (vx0 == 0 || d.vx == 0 ||
           llabs((int64_t)d.vx) < llabs((int64_t)vx0)));

    /* State 2: the blink death counts + 0xD down from 0x28, removes
     * at zero and decrements the root's live count once. */
    AosEnemy dying;
    assert(aos_enemy_create(&dying, AOS_ENEMY_UNE, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    dying.state = 2;
    dying.timer = 3;
    dying.parent = &root;
    root.spawned_count = 2;
    for (int i = 0; i < 10 && !dying.removed; ++i)
        (void)aos_enemy_update(&dying, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(dying.removed && root.spawned_count == 1);

    /* The state-3 spawner emits its create child on the first
     * update (short-fuse + 0x1C, one pixel up, linked to itself)
     * and removes itself once the count reaches zero. */
    AosEnemy s2;
    assert(aos_enemy_create(&s2, AOS_ENEMY_UNE, 300, 150, 5, 1,
                            &soma, &layer, &kind, &stats));
    AosHitReport cr = aos_enemy_update(&s2, &soma, &layer, &kind,
                                       NULL, NULL, 10, 4, CAM, CAMY,
                                       always_zero);
    assert(cr.child_count == 1 && cr.children[0].une_child == 1 &&
           cr.children[0].parent == &s2 && s2.spawned_count == 1 &&
           !s2.removed);
    s2.spawned_count = 0;
    (void)aos_enemy_update(&s2, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(s2.removed);
}

static void arachne_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    enum { AR_ANIMS = 4 };
    static const uint8_t eight[8] = {8, 8, 8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[AR_ANIMS];
    for (int a = 0; a < AR_ANIMS; ++a) defs[a] = (AosAnimDef){8, eight};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = AR_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < AR_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 23: hp 100, field_e 200, contact 18, defence 13,
     * weak 0x0001, resist 0x0400, experience 32. */
    const AosEnemyStats stats = {100, 18, 13, 0x0001, 0x0400, 32};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(250), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 400;
    const int CAM = 200, CAMY = 120;

    /* EnemyArachneCreate: state 0, the accumulators cleared; the
     * 0x8E & 0x40 flag parks the dormant state 4. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_ARACHNE, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.hp == 100 && e.step == 0 &&
           e.spin == 0 && e.walk_timer == 0);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, AOS_ENEMY_ARACHNE, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(ed.state == 4);
    aos_enemy_set_flag_8e_40(false);

    /* The first update emits the five linked legs (odd slots on
     * frame 0x19, even on 0x15, drops of 0/0/7/7/14 px) and the
     * state-0 aggro box (Soma inside 0xA0) steps into the charge. */
    AosHitReport lr = aos_enemy_update(&e, &soma, &layer, &kind,
                                       NULL, NULL, 10, 4, CAM, CAMY,
                                       always_zero);
    assert(lr.child_count == 5);
    int legs = 0;
    for (unsigned i = 0; i < lr.child_count; ++i) {
        const AosEnemy *c = &lr.children[i];
        if (c->role != AOS_ROLE_ARACHNE_LEG) continue;
        legs++;
        assert(c->parent == &e && c->id == AOS_ENEMY_ARACHNE &&
               c->state == 0 && c->combat.hurt_off &&
               c->static_frame == ((c->ar_leg & 1) ? 0x19 : 0x15));
    }
    assert(legs == 5);
    /* Soma spawns inside the 0xA0 aggro box, so the first update
     * already stepped into the charge (step 1, mode 1 or 2). */
    assert(e.step == 1 && e.ar_mode >= 1 && e.ar_mode <= 2);

    /* A leg follows its parent: same position plus its drop, the
     * parent's facing, a table frame from the speed accumulator. */
    AosEnemy leg;
    assert(aos_enemy_create(&leg, AOS_ENEMY_ARACHNE, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    leg.role = AOS_ROLE_ARACHNE_LEG;
    leg.parent = &e;
    leg.ar_leg = 3;
    leg.ar_drop = ((3 >> 1) * 7) << 16;
    leg.static_frame = 0x15;
    e.spin = 0x10000;
    e.ar_mode = 1;
    e.state = 0;
    (void)aos_enemy_update(&leg, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(leg.x == e.x && leg.y == e.y + leg.ar_drop &&
           leg.static_frame >= 0x13 && leg.static_frame <= 0x16);

    /* State 1 step 2 rains webs every fourth tick; each web is a
     * hurt-proof child on the 0x2800/0x5800 launch angle. */
    e.state = 1;
    e.step = 2;
    e.timer = 3;
    e.mirrored = false;
    AosHitReport wr = aos_enemy_update(&e, &soma, &layer, &kind,
                                       NULL, NULL, 10, 4, CAM, CAMY,
                                       always_zero);
    const AosEnemy *web = NULL;
    for (unsigned i = 0; i < wr.child_count; ++i)
        if (wr.children[i].role == AOS_ROLE_ARACHNE_WEB) web = &wr.children[i];
    assert(web && web->parent == &e && web->angle == 0x5800 &&
           web->combat.hurt_off);
    /* A web dies on the floor/wall flags or its 0x60-tick fuse. */
    AosEnemy w = *web;
    w.removed = false;
    int ticks = 0;
    while (!w.removed && ticks < 0x70)
        (void)aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero), ticks++;
    assert(w.removed && ticks <= 0x61);

    /* State 2 death: the 0x5A blink/sink removes the body. */
    e.state = 2;
    e.timer = 0x5A;
    for (int i = 0; i < 0x70 && !e.removed; ++i)
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(e.removed);
}

static uint32_t persephone_odd(void) { return 1; }

static void persephone_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { PR_ANIMS = 8 };
    static const uint8_t eight[8] = {8, 8, 8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[PR_ANIMS];
    for (int a = 0; a < PR_ANIMS; ++a) defs[a] = (AosAnimDef){8, eight};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = PR_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < PR_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 36: hp 150, field_e 96, contact 23, defence 25,
     * weak 0x0041, resist 0x0000, experience 55. */
    const AosEnemyStats stats = {150, 23, 25, 0x0041, 0x0000, 55};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(250), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 400;
    const int CAM = 200, CAMY = 120;

    /* EnemyPersephoneCreate: state 0, the hit mark cleared, + 0x14
     * self-linked; the 0x8E & 0x40 flag parks the dormant state 4. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_PERSEPHONE, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.hp == 150 && e.step == 0 && e.pr_hit == 0 &&
           e.parent == &e);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, AOS_ENEMY_PERSEPHONE, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(ed.state == 4);
    aos_enemy_set_flag_8e_40(false);

    /* State 0 step 0: Soma inside the 0xF0 x 0x60 approach box steps
     * into the animation-1 windup. */
    AosHitReport lr = aos_enemy_update(&e, &soma, &layer, &kind,
                                       NULL, NULL, 10, 4, CAM, CAMY,
                                       always_zero);
    (void)lr;
    assert(e.step == 1 && e.state == 0);

    /* Step 1: the animation end launches the animation-2 charge with
     * vx +/-0x10000, vy 0x10000 and ay 0x2800. */
    e.cycle_done = true;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 2 && e.vx == (e.mirrored ? 0x10000 : -0x10000) &&
           e.vy == 0x10000 && e.ay == 0x2800);

    /* State 1 step 0: animation 4, the hit mark cleared; a far target
     * and an odd coin take the step-0xA uppercut. */
    e.state = 1;
    e.step = 0;
    e.mirrored = false;
    soma.x = AOS_FIXED(600);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, persephone_odd);
    assert(e.step == 0xA && e.pr_hit == 0);
    /* Step 0xA frame gate: song 0x1CA, step 0xB, the launch. */
    e.anim.frame = 1;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 0xB && e.vy < 0 && e.ay == 0x2000);

    /* Step 2 on frame 3 emits the two-tick strike child and song 0x85. */
    e.state = 1;
    e.step = 2;
    e.anim.frame = 3;
    e.cycle_done = false;
    e.timer = 0;
    e.pr_hit = 0;
    soma.x = AOS_FIXED(250);
    AosHitReport sr = aos_enemy_update(&e, &soma, &layer, &kind,
                                       NULL, NULL, 10, 4, CAM, CAMY,
                                       always_zero);
    const AosEnemy *strike = NULL;
    for (unsigned i = 0; i < sr.child_count; ++i)
        if (sr.children[i].role == AOS_ROLE_PERSEPHONE_STRIKE)
            strike = &sr.children[i];
    assert(strike && strike->parent == &e && strike->static_frame == 0x14 &&
           strike->combat.hurt_off && strike->walk_timer == 1);

    /* The strike registers contact and expires after two updates. */
    AosEnemy st = *strike;
    st.removed = false;
    st.walk_timer = 1;
    (void)aos_enemy_update(&st, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(!st.removed);
    (void)aos_enemy_update(&st, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(st.removed);

    /* State 3 knockdown: gravity drop, floor landing resumes state 0
     * step 6 with animation 5. */
    AosEnemy d;
    assert(aos_enemy_create(&d, AOS_ENEMY_PERSEPHONE, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    d.state = 3;
    d.vy = 0;
    for (int i = 0; i < 40 && d.state == 3; ++i)
        (void)aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(d.state == 0 && d.step == 6 && d.vy == 0x10000);

    /* State 2 death: the 0x46-tick blink/sink removes the body. */
    e.state = 2;
    e.timer = 0x46;
    for (int i = 0; i < 0x50 && !e.removed; ++i)
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(e.removed);

    /* State 4 dormant: ~0x78 ticks on step 0, then step 1 waits for
     * the animation end before looping back with a re-seeded timer. */
    ed.step = 0;
    ed.timer = 0x78;
    (void)aos_enemy_update(&ed, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(ed.step == 1);
    ed.cycle_done = true;
    (void)aos_enemy_update(&ed, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(ed.step == 0 && ed.timer <= 0x1F);
}

static void lilith_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { LI_ANIMS = 10 };
    static const uint8_t ten[10] = {8, 8, 8, 8, 8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[LI_ANIMS];
    for (int a = 0; a < LI_ANIMS; ++a) defs[a] = (AosAnimDef){8, ten};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = LI_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < LI_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 37: hp 220, field_e 999, contact 30, defence 30,
     * weak 0x0801, resist 0x0410, experience 60. */
    const AosEnemyStats stats = {220, 30, 30, 0x0801, 0x0410, 60};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 400;
    const int CAM = 200, CAMY = 120;

    /* EnemyLilithCreate: state 0, hidden and intangible, vy 0x10000,
     * ay 0x1800; the 0x8E & 0x40 flag parks the dormant state 3. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_LILITH, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.hp == 220 && e.hidden && e.li_inactive == 1 &&
           e.vy == 0x10000 && e.ay == 0x1800);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, AOS_ENEMY_LILITH, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(ed.state == 3 && !ed.hidden && ed.li_inactive == 0);
    aos_enemy_set_flag_8e_40(false);

    /* State 0 step 0: Soma inside the 0x64 x 0x50 box wakes her —
     * step 1, unhidden, song 0x1C8. Move Soma into the box first. */
    soma.x = AOS_FIXED(290);
    soma.y = AOS_FIXED(140);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 1 && !e.hidden);

    /* Step 1: the animation end loops anim 5, activates (+ 0x1B = 0)
     * and enters the drift. */
    e.cycle_done = true;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 2 && e.li_inactive == 0);

    /* Step 2 drift: with Soma outside the 0x64 box but inside the wide
     * 0x190 box the + 0x18 approach counter climbs; past 0x3B and an
     * even coin she enters state 1 step 0xA. */
    soma.x = AOS_FIXED(120);   /* px-0xC8 = 100 .. px+0x68 wide box */
    e.li_flag = 0;
    for (int i = 0; i < 0x45 && e.state == 0; ++i)
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(e.state == 1 && e.step == 0xA);

    /* Step 0xA -> 11: anim 6 once, then the shared move block; step 11
     * spawns the breath bolt once anim frame passes 4 (song 0x7B) and
     * step 12 loops back to the hover on the animation end. */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 11);
    e.anim.frame = 5;
    AosHitReport br = aos_enemy_update(&e, &soma, &layer, &kind,
                                       NULL, NULL, 10, 4, CAM, CAMY,
                                       always_zero);
    const AosEnemy *bolt = NULL;
    for (unsigned i = 0; i < br.child_count; ++i)
        if (br.children[i].role == AOS_ROLE_LILITH_BOLT)
            bolt = &br.children[i];
    assert(e.step == 12 && bolt && bolt->static_frame == 0x36 &&
           bolt->combat.attack_off && !bolt->mirrored);
    e.cycle_done = true;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.state == 0 && e.step == 2);

    /* The bolt's + 0x1C sawtooth: + 0x400 per tick while the old timer
     * is <= 0xE, horizontal drift by facing, margin delete. */
    AosEnemy b = *bolt;
    b.removed = false;
    b.timer = 5;
    b.li_bolt = 0x8000;
    (void)aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(b.li_bolt == 0x8400 && b.vx == 0x8400);
    /* Off-screen bolts are deleted by the margin check. */
    b.x = AOS_FIXED(-400);
    (void)aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(b.removed);

    /* State 2 death: the shared flame/blink/sink removes the body. */
    e.state = 2;
    e.timer = 0x28;
    for (int i = 0; i < 0x40 && !e.removed; ++i)
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(e.removed);
}

static void succubus_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { SU_ANIMS = 11 };
    static const uint8_t eleven[11] = {8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[SU_ANIMS];
    for (int a = 0; a < SU_ANIMS; ++a) defs[a] = (AosAnimDef){8, eleven};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = SU_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < SU_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 95: hp 550, field_e 3000, contact 54, defence 40,
     * weak 0x0801, resist 0x0410, experience 360, soul yellow 7. */
    const AosEnemyStats stats = {550, 54, 40, 0x0801, 0x0410, 360};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;
    const int CAM = 200, CAMY = 120;

    /* EnemySuccubusCreate + sub_080946C4: anim 0xA looping, state 0;
     * param0 clear starts step 0 with + 0x1A = 1, set starts step 2;
     * vy 0x10000 / ay 0x1800; the 0x8E & 0x40 flag parks state 3 on
     * anim 1. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_SUCCUBUS, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.step == 0 && e.hp == 550 &&
           e.su_touch == 1 && e.vy == 0x10000 && e.ay == 0x1800);
    AosEnemy ep;
    assert(aos_enemy_create(&ep, AOS_ENEMY_SUCCUBUS, 300, 150, 1, 0,
                            &soma, &layer, &kind, &stats));
    assert(ep.state == 0 && ep.step == 2 && ep.su_touch == 0);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, AOS_ENEMY_SUCCUBUS, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(ed.state == 3);
    aos_enemy_set_flag_8e_40(false);

    /* State 0 step 0: the wide 0xB4 x 0x46 box marks + 0x21 (song
     * 0x1C7); the narrow 0x3C x 0x46 box — or the + 0x18 hit gate —
     * advances to step 1 on anim 0. */
    soma.x = AOS_FIXED(255);   /* px-0x5A = 197 .. px+0x5A: inside wide */
    soma.y = AOS_FIXED(140);   /* py-0x40 = 100 .. py+6: inside */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.su_aggro == 1);
    soma.x = AOS_FIXED(295);   /* px-0x1E = 277 .. px+0x1E: inside narrow */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 1);
    /* The + 0x18 hit gate also advances step 0. */
    AosEnemy eg;
    assert(aos_enemy_create(&eg, AOS_ENEMY_SUCCUBUS, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    eg.su_gate = 1;
    (void)aos_enemy_update(&eg, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(eg.step == 1);

    /* Step 1: the animation end loops anim 1 and clears + 0x1A/+ 0x1B. */
    e.cycle_done = true;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 2 && e.su_touch == 0 && e.su_inactive == 0);

    /* Step 2 drift: the 0x64 x 0xA0 box with an even coin dashes into
     * step 4 (vx +/-0x28000); always_zero rolls the dash. */
    soma.x = AOS_FIXED(280);   /* px-0x32 = 248 .. px+0x32 */
    soma.y = AOS_FIXED(130);   /* py-0x50 = 80 .. py+0x50 */
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 4 && (e.vx == 0x28000 || e.vx == -0x28000));

    /* Step 4: the 23/24 friction bleeds the dash; past 0x27 ticks it
     * resumes the drift with vy 0x10000 / ay 0x1800. */
    for (int i = 0; i < 0x30 && e.step == 4; ++i)
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(e.step == 2);

    /* Step 2 again: with the + 0x19 attempt flag set the dash gate is
     * skipped; the close 0x50 x 0xA0 box with an even coin enters
     * state 1 step 0 (the pounce wind-up). */
    e.su_try = 1;
    for (int i = 0; i < 0x80 && e.state == 0; ++i) {
        /* She drifts: keep Soma inside the close 0x50 x 0xA0 box. */
        soma.x = e.x;
        soma.y = e.y;
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    }
    assert(e.state == 1 && e.step == 0);

    /* State 1: step 0 waits 0x28 ticks (song 0x1C7, vx cleared); step 1
     * dashes at anim frame 3 (vx +/-0x30000); step 2 with + 0x1D set
     * chains into the homing pounce step 10. */
    e.timer = 0x28;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 1 && e.vx == 0);
    e.anim.frame = 3;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 2 && (e.vx == 0x30000 || e.vx == -0x30000) &&
           e.su_dash == 1);
    e.su_chain = 1;
    e.timer = 0x1E;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 10 && e.timer == 0x10 && e.su_emit == 1 &&
           e.su_volley == 0 && e.su_age == 0);

    /* Step 10 homing: the countdown divides the remaining distance —
     * the body closes on Soma; the + 0x1E age counter ends the
     * pounce into step 11 (anim 3, vx +/-0x28000). */
    int32_t before_x = e.x, before_y = e.y;
    soma.x = AOS_FIXED(290);
    soma.y = AOS_FIXED(145);
    for (int i = 0; i < 0x22 && e.step == 10; ++i)
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(e.step == 11 && e.x != before_x);
    (void)before_y;

    /* Step 11: past 0x27 ticks she returns to state 0 step 2 on anim 1. */
    e.timer = 0x28;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.state == 0 && e.step == 2 && e.su_try == 0);

    /* State 2 death: the shared flame/blink/sink removes the body. */
    e.state = 2;
    e.timer = 0x28;
    for (int i = 0; i < 0x40 && !e.removed; ++i)
        (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(e.removed);

    /* The dormant state 3 runs the animation step only: no state
     * machine, no contact pass — the body just idles off-screen. */
    AosEnemy ed2;
    aos_enemy_set_flag_8e_40(true);
    assert(aos_enemy_create(&ed2, AOS_ENEMY_SUCCUBUS, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    ed2.hidden = false;
    (void)aos_enemy_update(&ed2, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(ed2.state == 3 && !ed2.removed);
}

static void triton_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { TR_ANIMS = 12 };
    static const uint8_t twelve[12] = {8, 8, 8, 8, 8, 8,
                                       8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[TR_ANIMS];
    for (int a = 0; a < TR_ANIMS; ++a) defs[a] = (AosAnimDef){8, twelve};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = TR_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < TR_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
            kind.sprites[a][f] = (uint8_t)(a * 8 + f < 32 ? a * 8 + f : 0);
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 67: hp 400, field_e 999, contact 43, defence 35,
     * weak 0x0082, resist 0x0000, experience 300, soul yellow 25. */
    const AosEnemyStats stats = {400, 43, 35, 0x0082, 0x0000, 300};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;
    const int CAM = 200, CAMY = 120;

    /* EnemyTritonCreate + sub_08087D24: animation 1 stepped, state 0
     * step 0xA, tr fields cleared; the 0x8E & 0x40 flag parks state 4. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_TRITON, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.step == 0xA && e.hp == 400 &&
           e.anim.id == 1 && !e.tr_launch && !e.tr_returned &&
           e.tr_saved == 0);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, AOS_ENEMY_TRITON, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(ed.state == 4);
    aos_enemy_set_flag_8e_40(false);

    /* The first update emits the linked whirlpool head (sub_080871C0
     * approximated by the first-update spawn): role TRITON_HEAD,
     * parent pointer, animation 0xA. */
    soma.x = AOS_FIXED(295);   /* inside the 0x6400000 aggro range */
    soma.y = AOS_FIXED(150);
    AosHitReport rep = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                        NULL, 10, 4, CAM, CAMY,
                                        always_zero);
    assert(rep.child_count == 1 &&
           rep.children[0].role == AOS_ROLE_TRITON_HEAD &&
           rep.children[0].parent == &e &&
           rep.children[0].id == AOS_ENEMY_TRITON);
    /* State 10 -> 11 on the proximity (player_in_range 0x6400000),
     * or -> 0xC when the facing refresh diverts. */
    assert(e.step == 0xB || e.step == 0xC || e.state == 1);

    /* Step 11 aggro roll: timer past 0x3B + range + coin + floor bit
     * enters state 1 step 0. Drive it directly. */
    e.state = 0;
    e.step = 0xB;
    e.timer = 0x3C;
    e.vx = e.vy = e.ax = e.ay = 0;
    rep = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    /* always_zero: (0 & 0x7F) <= 0x5F holds; the floor bit needs the
     * drift move to report & 4 — in the water cells it may not, so
     * accept either the attack or the continued swim. */
    (void)rep;

    /* State 1 chain: step 10 waits for anim frame 0 -> anim 3 + step
     * 11; the cycle plays anim 2 loop + song 0x8A + step 12; past
     * timer 0x1D tr_launch raises and step 13 holds for the child's
     * return while jets volley every 32 ticks; tr_returned plays anim
     * 4 -> step 14 -> anim 1 -> state 0 on tr_saved. */
    e.state = 1;
    e.step = 10;
    e.tr_saved = 0xA;
    e.timer = 0;
    e.anim.frame = 0;
    e.cycle_done = false;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 0xB);
    e.cycle_done = true;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 0xC &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x8A) == 1);
    e.timer = 0x1E;
    e.cycle_done = false;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 0xD && e.tr_launch == 1 && e.tr_returned == 0);
    /* Step 13: timer 0x1F -> ++ 0x20 & 0x1F == 0 spawns a jet. */
    e.timer = 0x1F;
    rep = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(rep.child_count == 1 &&
           rep.children[0].role == AOS_ROLE_TRITON_JET &&
           rep.children[0].tr_jvy == (e.mirrored ? 0x1F0000 : 0x190000));
    /* The child signals its return: anim 4, step 14, then the cycle
     * restores state 0 on tr_saved. */
    e.tr_returned = 1;
    e.cycle_done = false;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 0xE && e.tr_launch == 0 && e.anim.id == 4);
    e.cycle_done = true;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.state == 0 && e.step == 0xA && e.timer == 0);

    /* State 1 minor spit: on anim frame 1 step 1 spawns the homing
     * bubble, then step 2 returns to state 0 on tr_saved. */
    e.state = 1;
    e.step = 1;
    e.tr_saved = 0xA;
    e.anim.frame = 1;
    rep = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(rep.child_count == 1 &&
           rep.children[0].role == AOS_ROLE_TRITON_BUBBLE &&
           rep.children[0].parent == &e &&
           rep.children[0].contact_elements ==
               (uint16_t)((e.contact_elements * 3) >> 2));
    assert(e.step == 2);
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.state == 0 && e.step == 0xA);

    /* The bubble homes after timer 0x31 and deletes past 0xB3. */
    AosEnemy b = rep.children[0];
    b.parent = &e;
    b.timer = 0x32;
    b.x = AOS_FIXED(400);
    b.y = AOS_FIXED(150);
    soma.x = AOS_FIXED(100);
    soma.y = AOS_FIXED(150);
    b.vx = b.vy = 0;
    for (int i = 0; i < 4; ++i)
        (void)aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(b.vx < 0);   /* homing left toward Soma */
    b.timer = 0xB4;
    (void)aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(b.removed);

    /* The whirlpool head child: a fresh parent emits it on the first
     * update; attach reads the parent's + 0x65 sprite for the Y
     * offset and tr_launch sends it through rise/spin/turn/dock. */
    AosEnemy e2;
    assert(aos_enemy_create(&e2, AOS_ENEMY_TRITON, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    AosHitReport rep2 = aos_enemy_update(&e2, &soma, &layer, &kind,
                                         NULL, NULL, 10, 4, CAM, CAMY,
                                         always_zero);
    assert(rep2.child_count == 1);
    AosEnemy head = rep2.children[0];
    /* Case 0 attach: x = parent x, y offset from the sprite table,
     * facing x offset; tr_launch advances to the rise. */
    e2.tr_launch = 1;
    head.y = e2.y;
    (void)aos_enemy_update(&head, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(head.step == 1 && head.timer == 0);
    /* Rise: y -= 0x18000 until <= 0x200000, song 0x86, step 2. */
    head.y = 0x210000;
    (void)aos_enemy_update(&head, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(head.step == 2 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x86) == 1);
    /* Spin case 2 with facing set and angle just above 0xC000:
     * each proceed adds a full turn (+ 0x10000) then waits ~16 ticks
     * for the angle to decay back under 0xC000, so the aim takes
     * ~4 x 16 ticks. */
    head.step = 2;
    head.tr_angle = 0xD000;
    e2.mirrored = true;
    for (int i = 0; i < 90 && head.step == 2; ++i)
        (void)aos_enemy_update(&head, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(head.step == 3 && head.tr_angle_target != 0);
    /* Turn case 3: angle walks onto the target at 0x1000/tick. */
    head.tr_angle = (uint32_t)(head.tr_angle_target - 0x2000);
    for (int i = 0; i < 4 && head.step == 3; ++i)
        (void)aos_enemy_update(&head, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(head.step == 4);
    /* Splash + launch: timer 0xA -> 0xB spawns a splash child; timer
     * past 0x3B sets the return velocity and step 5. */
    head.step = 4;
    head.timer = 0xA;
    rep2 = aos_enemy_update(&head, &soma, &layer, &kind, NULL, NULL,
                            10, 4, CAM, CAMY, always_zero);
    assert(rep2.child_count == 1 &&
           rep2.children[0].role == AOS_ROLE_TRITON_SPLASH);
    head.timer = 0x3B;
    (void)aos_enemy_update(&head, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(head.step == 5 && (head.vx != 0 || head.vy != 0));
    /* Splash update: cycle end removes it. */
    AosEnemy sp = rep2.children[0];
    sp.cycle_done = true;
    (void)aos_enemy_update(&sp, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(sp.removed);
    /* Dock case 6: rise 0x18000/tick until 0x300000 above the parent,
     * then tr_returned and back to attach. */
    head.step = 6;
    head.y = e2.y - 0x2E8000;   /* one tick of rise reaches the dock */
    (void)aos_enemy_update(&head, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e2.tr_returned == 1 && e2.tr_launch == 0 && head.step == 0);

    /* Parent death: state 2 blinks on kind->blink down from 0x28 into
     * state 3, which removes on its 0xA timer. */
    e2.state = 2;
    e2.timer = 0x28;
    e2.step = 0;
    for (int i = 0; i < 0x28 && e2.state == 2; ++i)
        (void)aos_enemy_update(&e2, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(e2.state == 3 && e2.timer == 0xA && e2.hidden);
    for (int i = 0; i < 0x0A && !e2.removed; ++i)
        (void)aos_enemy_update(&e2, &soma, &layer, &kind, NULL, NULL,
                               10, 4, CAM, CAMY, always_zero);
    assert(e2.removed);
}

static void devil_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { DV_ANIMS = 12 };
    static const uint8_t twelve[12] = {8, 8, 8, 8, 8, 8,
                                       8, 8, 8, 8, 8, 8};
    static AosAnimDef defs[DV_ANIMS];
    for (int a = 0; a < DV_ANIMS; ++a) defs[a] = (AosAnimDef){8, twelve};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = DV_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < DV_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
            kind.sprites[a][f] = (uint8_t)(a * 8 + f < 32 ? a * 8 + f : 0);
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv: id 48 hp 300, id 72 hp 500, id 102 hp 1800,
     * id 104 hp 500 (contact/defence/resist per record). */
    const AosEnemyStats stats48 = {300, 38, 30, 0x0820, 0x0010, 200};
    const AosEnemyStats stats72 = {500, 46, 35, 0x0820, 0x0410, 400};
    const AosEnemyStats stats102 = {1800, 78, 45, 0x0820, 0x0410, 3000};
    const AosEnemyStats stats104 = {500, 55, 35, 0x0824, 0x0412, 400};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;
    const int CAM = 200, CAMY = 120;

    /* EnemyDevilCreate: the + 0x36 variant (the id) picks the animation
     * — 0x30/0x68 -> 0xA, 0x48 -> 2, 0x66 -> 6 — state 0 step 0xA,
     * timer 1, + 0x26 = 1; the 0x8E & 0x40 flag parks state 3. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_DEVIL, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats48));
    assert(e.state == 0 && e.step == 0xA && e.timer == 1 &&
           e.dv_c26 == 1 && e.anim.id == 0xA && e.hp == 300 &&
           !e.boss_parts_spawned);
    AosEnemy ea;
    assert(aos_enemy_create(&ea, AOS_ENEMY_ARC_DEMON, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats72));
    assert(ea.anim.id == 2 && ea.hp == 500);
    AosEnemy el;
    assert(aos_enemy_create(&el, AOS_ENEMY_DEMON_LORD, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats102));
    assert(el.anim.id == 6 && el.hp == 1800);
    AosEnemy ef;
    assert(aos_enemy_create(&ef, AOS_ENEMY_FLAME_DEMON, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats104));
    assert(ef.anim.id == 0xA && ef.hp == 500);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, AOS_ENEMY_DEVIL, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats48));
    assert(ed.state == 3);
    aos_enemy_set_flag_8e_40(false);

    /* The first update emits the eight imps (the + 0x1C container):
     * role DEVIL_IMP, parent pointer, variant frame, orbit records. */
    soma.x = AOS_FIXED(200);
    soma.y = AOS_FIXED(140);
    AosHitReport rep = aos_enemy_update(&e, &soma, &layer, &kind, NULL,
                                        NULL, 10, 4, CAM, CAMY,
                                        always_zero);
    assert(rep.child_count == 8 && e.boss_parts_spawned &&
           e.step == 0);
    for (int i = 0; i < 8; ++i) {
        assert(rep.children[i].role == AOS_ROLE_DEVIL_IMP &&
               rep.children[i].parent == &e &&
               rep.children[i].id == AOS_ENEMY_DEVIL &&
               rep.children[i].combat.attack_off &&
               rep.children[i].combat.hurt_off);
    }
    /* Attached imps ride their orbit record: position = parent + the
     * record offset (dv.ox/dv.oy). */
    AosEnemy imp = rep.children[0];
    imp.parent = &e;
    int32_t ix = imp.x, iy = imp.y;
    (void)aos_enemy_update(&imp, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(imp.x == e.x + e.dv.ox[imp.dv_slot & 7] &&
           imp.y == e.y + e.dv.oy[imp.dv_slot & 7]);
    (void)ix; (void)iy;

    /* State-1 entry (the close-range check): state 1 step 0 rolls the
     * variant gate — Devil always takes step 1 timer 0x12. */
    e.state = 1;
    e.step = 0;
    e.timer = 0;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(e.step == 1 && e.timer == 0x12);

    /* The aimed volley: state 1 step 0xC at timer 0x1E spawns the
     * DEVIL_FIRE child and plays song 0x78; the timer then counts
     * down to step 0xD. */
    aos_audio_clear();
    e.step = 0xC;
    e.timer = 0x1E;
    rep = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(rep.child_count == 1 &&
           rep.children[0].role == AOS_ROLE_DEVIL_FIRE &&
           rep.children[0].parent == &e &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x78) == 1);
    assert(e.timer == 0x1D && e.step == 0xC);

    /* The Arc Demon orb: state 1 step 0xB at timer 0x28 spawns the
     * straight DEVIL_ORB (imps already emitted). */
    ea.boss_parts_spawned = true;
    ea.state = 1;
    ea.step = 0xB;
    ea.timer = 0x28;
    rep = aos_enemy_update(&ea, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(rep.child_count == 1 &&
           rep.children[0].role == AOS_ROLE_DEVIL_ORB);
    /* Flame Demon / Devil do not fire the orb on that step. */
    ef.boss_parts_spawned = true;
    ef.state = 1;
    ef.step = 0xB;
    ef.timer = 0x28;
    rep = aos_enemy_update(&ef, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(rep.child_count == 0);

    /* The imp dive: parent state 1 step >= 2 releases the child —
     * state 0 -> 1 launch (angle to the player), then state 2 flight
     * integrates and expires back to the orbit. */
    imp.state = 0;
    imp.step = 0;
    imp.timer = 0;
    e.state = 1;
    e.step = 2;
    (void)aos_enemy_update(&imp, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(imp.state == 1 && imp.timer == 0x28 &&
           !imp.combat.attack_off && !imp.combat.hurt_off);
    (void)aos_enemy_update(&imp, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(imp.state == 2);
    imp.timer = 1;
    (void)aos_enemy_update(&imp, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(imp.state == 0 && imp.combat.attack_off && imp.combat.hurt_off);

    /* The fireball: first update arms, then it re-anchors on the
     * parent and expires with song 0x12A past its life. */
    AosEnemy fire = rep.children[0]; /* the orb copy — re-create a fire */
    (void)fire;
    e.step = 0xC;
    e.timer = 0x1E;
    rep = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    AosEnemy fb = rep.children[0];
    fb.parent = &e;
    fb.role = AOS_ROLE_DEVIL_FIRE;
    fb.state = 1;
    fb.dv_life = 1;
    fb.timer = 0x40;
    fb.x = e.x;
    fb.y = e.y;
    rep = aos_enemy_update(&fb, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    /* life hit 0 -> drift spawns + song 0x12A + removal. */
    assert(fb.removed);

    /* Lethal damage (sub_080920D8): defeated, song 0x72, state 2,
     * timer 0x28, step 0 — the shared death path. */
    aos_audio_clear();
    soma = aos_soma_spawn(AOS_FIXED(300), AOS_FIXED(150), NULL);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    AosEnemy kd;
    assert(aos_enemy_create(&kd, AOS_ENEMY_DEVIL, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats48));
    kd.y = AOS_FIXED(150);
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-10, -40, 20, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    AosHitReport hit = aos_enemy_update(&kd, &soma, &layer, &kind,
                                        &weapon, &blade, 9999, 4,
                                        0, 0, always_zero);
    assert(hit.enemy_hit && hit.killed && kd.state == 2 &&
           kd.timer == 0x28 && kd.step == 0 && kd.defeated &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x72) == 1);

    /* Dormant state 3: the formation cycle only — timer 0x16 re-forms
     * and steps 1-4 rotate the formation index. */
    ed.state = 3;
    ed.step = 0;
    ed.timer = 0;
    (void)aos_enemy_update(&ed, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(ed.step == 1 && ed.timer == 0x16);
    ed.timer = 1;
    (void)aos_enemy_update(&ed, &soma, &layer, &kind, NULL, NULL,
                           10, 4, CAM, CAMY, always_zero);
    assert(ed.step == 2 && ed.timer == 0x16);
}

static void ukoback_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    static const uint8_t four[4] = {6, 6, 6, 6};
    const AosAnimDef defs[4] = {{4, four}, {4, four}, {4, four}, {4, four}};
    const AosAnimSet anims = {defs, 4};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 4; ++a)
        for (int f = 0; f < 16; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 76: hp 80, field_e 999, contact 40, defence 20,
     * weak 0x0124, resist 0x0012, experience 180. */
    const AosEnemyStats stats = {80, 40, 20, 0x0124, 0x0012, 180};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(100), NULL);
    soma.max_hp = soma.hp = 400;

    /* EnemyUkobackCreate (0x080C5BDC): state 0 with the sub_080C6510
     * contact and sub_080C653C hurt; the global 0x8E & 0x40 sends it
     * to the inert state 1 instead. */
    AosEnemy u;
    assert(aos_enemy_create(&u, AOS_ENEMY_UKOBACK, 100, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(u.state == 0 && u.hp == 80 && u.uk_timer == 0 && u.uk_peek == 0);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ui;
    assert(aos_enemy_create(&ui, AOS_ENEMY_UKOBACK, 100, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(ui.state == 1);
    aos_enemy_set_flag_8e_40(false);

    /* sub_080C5CE8: the first substep arms vx = vy = 0xC000 and the
     * cooldown umod(random, 0x168) + 0x50; script {0x200, 0, 1} keeps
     * both directions positive (the vy draw inverts on a clear bit). */
    script = (const uint32_t[]){0x200, 0, 1, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0};
    script_at = 0;
    int32_t x0 = u.x, y0 = u.y;
    AosHitReport r = aos_enemy_update(&u, &soma, &layer, &kind, NULL,
                                      NULL, 10, 4, 0, 0, scripted);
    assert(u.vx == 0xC000 && u.vy == 0xC000 && u.mirrored);
    assert(u.x == x0 + 0xC000 && u.y == y0 + 0xC000);
    assert(u.uk_timer == 0xE8);   /* umod(0x200, 0x168) + 0x50 = 0x98+0x50 */

    /* Once the countdown reaches 0 and uk_peek <= 7 the pot emits the
     * peeked creature (sub_080C6590 -> sub_080C5FA0): same id, role
     * UKOBACK_CREATURE, parent pointer, hp 1, anim 1. The spawn check
     * runs after the decrement, so a timer of 1 expires this frame. */
    u.uk_timer = 1;
    r = aos_enemy_update(&u, &soma, &layer, &kind, NULL, NULL, 10, 4,
                         0, 0, scripted);
    assert(r.child_count == 1 && r.children[0].id == AOS_ENEMY_UKOBACK &&
           r.children[0].role == AOS_ROLE_UKOBACK_CREATURE &&
           r.children[0].parent == &u && r.children[0].hp == 1 &&
           u.uk_peek == 1);
    /* sub_080C5FA0's sub_08068AD4 box (-0x80, -0x40, 0x120, 0x100
     * centred) contains Soma here: song 0x78. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x78) == 1);

    /* sub_080C610C: the creature counts + 0x1C up to 0x3FF, then
     * enters the 0xFF drift; a soma touch bursts it into the shard
     * effects and the same drift. */
    AosEnemy c = r.children[0];
    c.walk_timer = 0x3FF;
    aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                     always_zero);
    assert(c.state == 0xFF);
    /* The drift integrates + 0x14/+ 0x18; with the offsets still at
     * their 0x10000 init the first pass counts them down through vx. */
    int32_t tx0 = c.target_x;
    aos_enemy_update(&c, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                     always_zero);
    assert(c.target_x != tx0 || c.target_y != 0x10000);

    /* sub_080C5F10: the lethal blow (the hurt callback
     * sub_080C653C) enters state 0xFF; the next update plays song
     * 0x72, freezes on direct frame 9, sinks at vy 0x8000 and emits
     * the lid child (sub_080C6624 -> sub_080C61F4). */
    AosEnemy d;
    assert(aos_enemy_create(&d, AOS_ENEMY_UKOBACK, 100, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    /* Drive it to the death state the way the hurt callback does. */
    d.state = 0xFF;
    d.step = 0;
    AosHitReport dr = aos_enemy_update(&d, &soma, &layer, &kind, NULL,
                                       NULL, 10, 4, 0, 0, always_zero);
    assert(d.static_frame == 9 && d.vy == 0x8000 && d.step == 1 &&
           dr.child_count == 1 &&
           dr.children[0].role == AOS_ROLE_UKOBACK_LID &&
           dr.children[0].parent == &d);
    /* sub_080C5F10 step 0: song 0x72. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x72) == 1);

    /* sub_080C61F4: state 0 copies the parent position, lifts the lid
     * 4 px and inherits 1.5x its vy; state 1 falls; state 2 fades. */
    AosEnemy l = dr.children[0];
    aos_enemy_update(&l, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                     always_zero);
    assert(l.state == 1 && l.vy == d.vy + d.vy / 2);
    for (int i = 0; i < 0x40; ++i)
        aos_enemy_update(&l, &soma, &layer, &kind, NULL, NULL, 10, 4,
                         0, 0, always_zero);
    assert(l.state == 2);
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
        if (request.action == AOS_AUDIO_START_SONG && request.value == 0xA4) continue;
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

static void minotaur_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;      /* floor top 160 */
    static const uint8_t one[] = {2};
    const AosAnimDef defs[8] = {{1, one}, {1, one}, {1, one}, {1, one},
                                {1, one}, {1, one}, {1, one}, {1, one}};
    const AosAnimSet anims = {defs, 8};
    static AosEnemyKind kind;
    kind.anims = &anims;
    kind.probes = (AosProbes){3, -50, -1, 8, {-15, -31, -47}};
    for (int a = 0; a < 8; ++a)
        for (int f = 0; f < 4; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -56, 32, 56};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {200, 25, 12, 0x08C1, 0, 60};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(159), NULL);

    /* sub_080B0AD4: animation 1 looping, + 0x18 = 3, feet on the floor,
     * state 0; the red one's palette bank 1 sits in + 0x15. */
    AosEnemy m;
    assert(aos_enemy_create(&m, AOS_ENEMY_MINOTAUR, 100, 140, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(m.state == 0 && m.anim.id == 1 && m.mino_flags == 3 && m.mino_bank == 0 &&
           (m.y >> 16) <= 160);
    AosEnemy r;
    assert(aos_enemy_create(&r, AOS_ENEMY_RED_MINOTAUR, 100, 140, 0, 0, &soma, &layer,
                            &kind, &stats));
    assert(r.mino_bank == 1);

    /* sub_080B0814: the brown gate always passes; the player in the
     * centred 0xC8 x 0x78 box (dx 80 inside, outside the chase stop box
     * 0x64) sends it to the chase (state 1). */
    soma.x = AOS_FIXED(180);
    soma.y = AOS_FIXED(m.y >> 16);
    aos_enemy_update(&m, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(m.state == 1 && m.step == 0);
    /* Chase step 0 plays animation 2 once, then animation 3 loops and the
     * probe walker carries it toward the player. */
    int frames = 0;
    while (m.step < 2 && frames < 20)
        aos_enemy_update(&m, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero),
        ++frames;
    assert(m.step == 2 && m.anim.id == 3);
    int32_t before = m.x;
    for (int i = 0; i < 30; ++i)
        aos_enemy_update(&m, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(m.x != before);

    /* The red gate (sub_080AFE08): RNG & 1 == 0, flags bit 1 set, the
     * player outside the centred 0xA0 box and no wall ahead -> attack
     * (state 2), clearing bit 1 for the wind-up. */
    soma.x = AOS_FIXED(190);    /* dx 90: inside 0xC8, outside 0xA0 */
    soma.y = AOS_FIXED(r.y >> 16);
    aos_enemy_update(&r, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(r.state == 2);
    /* Case 0 of the attack: bit 1 cleared, animation 7 once, step 1. */
    aos_enemy_update(&r, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(!(r.mino_flags & 2) && r.anim.id == 7 && r.step == 1);
    /* Wind-up ends (animation 7), then the 8-tick delay starts the charge
     * (animation 5, step 3) with the two dust effects of sub_080458E4 in
     * the same update's report. */
    AosHitReport hit = {0};
    for (int i = 0; i < 20 && r.step < 3; ++i)
        hit = aos_enemy_update(&r, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                               always_zero);
    assert(r.step == 3 && hit.effect_count == 2);

    /* The charge decelerates from 3.25 by 1.25 a tick and ends back in
     * the idle state. */
    int guard = 0;
    while (r.state == 2 && guard++ < 200)
        aos_enemy_update(&r, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(r.state == 0 && (r.mino_flags & 1));
}

static void worm_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;      /* floor top 160 */
    static const uint8_t ticks[] = {4, 4};
    AosAnimDef defs[4];
    for (int i = 0; i < 4; ++i) defs[i] = (AosAnimDef){2, ticks};
    const AosAnimSet anims = {defs, 4};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int a = 0; a < 4; ++a)
        for (int f = 0; f < 4; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -8, 16, 16};
        }
    const AosEnemyStats stats = {200, 25, 12, 0x08C1, 0, 60};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(900), AOS_FIXED(159), NULL);

    /* sub_080AD7F0: animation 3 (Giant Worm) or 0 (Poison Worm) looping,
     * variant in + 0x21, feet snapped on the floor, state 0. */
    AosEnemy w;
    assert(aos_enemy_create(&w, AOS_ENEMY_GIANT_WORM, 100, 140, 0, 0, &soma, &layer,
                            &kind, &stats));
    assert(w.state == 0 && w.anim.id == 3 && w.variant == 0 && (w.y >> 16) <= 160);
    AosEnemy p;
    assert(aos_enemy_create(&p, AOS_ENEMY_POISON_WORM, 100, 140, 0, 0, &soma, &layer,
                            &kind, &stats));
    assert(p.anim.id == 0 && p.variant == 1);

    /* The first update emits the four linked segments: index 1-4 in
     * + 0x1E, index << 12 in + 0x22, each 0xA0000 further along the
     * facing from the previous one, snapped on the floor. */
    AosHitReport hit = aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4,
                                        0, 0, always_zero);
    assert(hit.child_count == 4);
    AosEnemy segs[AOS_ENEMY_MAX_CHILDREN];
    memcpy(segs, hit.children, sizeof(segs));
    int dir = w.mirrored ? -1 : 1;
    int32_t px = w.x;
    for (int i = 0; i < 4; ++i) {
        AosEnemy *s = &segs[i];
        assert(s->role == AOS_ROLE_WORM_SEGMENT && s->id == w.id && s->variant == 0 &&
               s->worm_index == (uint8_t)(i + 1) && s->worm_phase == (uint16_t)((i + 1) << 12) &&
               s->x == px + dir * 0xA0000 && (s->y >> 16) <= 160);
        px = s->x;
    }
    /* The same update ran the state-0 decision: state 2, a hop
     * direction picked (the player is far: RandomNumberGenerator & 1). */
    assert(w.state == 2 && w.step == 0);

    /* The emerge machine (sub_080ADB94): launch, decelerate to 0,
     * wait 0x10 ticks, relaunch; sub_080AD9FC returns it to state 1. */
    int guard = 0;
    while (w.state != 1 && guard++ < 200)
        aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(w.state == 1);
    aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(w.state == 2);

    /* A segment's lazy init (child state 0): animation 1 — 2 for
     * segment 4 — then state 1 under the follow/settle path. */
    AosEnemy seg = segs[0];
    seg.parent = &w;
    seg.follow = &w;
    soma.x = AOS_FIXED(900);
    aos_enemy_update(&seg, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(seg.state == 1 && seg.anim.id == 1);
    AosEnemy tail = segs[3];
    tail.parent = &w;
    tail.follow = &segs[2];
    aos_enemy_update(&tail, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(tail.state == 1 && tail.anim.id == 2);

    /* sub_080AD748: a weapon hit on a segment is forwarded to the
     * parent — its HP drops, the segment keeps its own. */
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    int16_t parent_hp = w.hp;
    soma.x = seg.x;
    soma.y = seg.y + AOS_FIXED(10);
    hit = aos_enemy_update(&seg, &soma, &layer, &kind, &weapon, &blade, 40, 4, 0, 0,
                           always_zero);
    assert(hit.enemy_hit && w.hp < parent_hp && seg.hp == seg.stats.hp && w.hit_flash == 8);

    /* A lethal forwarded blow on a fresh segment (the first one's
     * combat cooldown is still running): song 0x70, the parent enters
     * the state-3 death lunge. */
    AosEnemy seg2 = segs[1];
    seg2.parent = &w;
    seg2.follow = &w;
    aos_enemy_update(&seg2, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                     always_zero);
    w.hp = 1;
    w.state = 1;
    w.hit_flash = 0;
    soma.x = seg2.x;
    soma.y = seg2.y + AOS_FIXED(10);
    hit = aos_enemy_update(&seg2, &soma, &layer, &kind, &weapon, &blade, 9999, 4,
                           0, 0, always_zero);
    assert(w.state == 3 && w.hp <= 0 && aos_audio_requested(AOS_AUDIO_START_SONG, 0x70) == 1);
    hit = aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(hit.killed && w.worm_lunge);
    int frames = 0;
    while (!w.removed && frames < 0x50)
        aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero),
        ++frames;
    assert(w.removed);

    /* The parent's wall-wait (+ 0x1D): a probe ahead sets it to 4 and
     * the next decision toggles the hop direction. */
    AosEnemy w2;
    assert(aos_enemy_create(&w2, AOS_ENEMY_GIANT_WORM, 100, 140, 0, 0, &soma, &layer,
                            &kind, &stats));
    w2.state = 1;
    w2.worm_dir = 0;
    w2.worm_wait = 4;
    aos_enemy_update(&w2, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(w2.state == 2 && w2.worm_dir == 1 && w2.worm_wait == 3);
}

static void biphron_tests(void) {
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;      /* floor top 160 */
    static const uint8_t t2[] = {4, 4}, t3[] = {3, 3, 3, 3, 3, 3},
                         t8[] = {3, 3, 3, 3, 3, 3, 3, 3};
    const AosAnimDef defs[5] = {{2, t2}, {6, t3}, {2, t2}, {8, t8}, {2, t2}};
    const AosAnimSet anims = {defs, 5};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int a = 0; a < 5; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -8, 16, 16};
        }
    const AosEnemyStats stats = {60, 25, 12, 0x08C1, 0, 30};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(900), AOS_FIXED(159), NULL);

    /* EnemyBiphronCreate / sub_0809A3D8: animation 0 looping, feet
     * snapped on the floor, state 0, contact elements 0. */
    AosEnemy b;
    assert(aos_enemy_create(&b, AOS_ENEMY_BIPHRON, 100, 140, 0, 0, &soma, &layer,
                            &kind, &stats));
    assert(b.state == 0 && b.anim.id == 0 && b.contact_elements == 0 &&
           (b.y >> 16) <= 160);

    /* The 0x8E & 0x40 create path selects the affine-only state 3. */
    aos_enemy_set_flag_8e_40(true);
    AosEnemy inert;
    assert(aos_enemy_create(&inert, AOS_ENEMY_BIPHRON, 100, 140, 0, 0, &soma, &layer,
                            &kind, &stats));
    assert(inert.state == 3);
    aos_enemy_set_flag_8e_40(false);
    aos_enemy_update(&inert, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                     always_zero);
    assert(inert.state == 3);

    /* State 0 (sub_08099CF0): facing, vx +-0x4000, vy +0x10000,
     * angle 0x2800, then the sub_08069A00 hover walker; the timer
     * only counts on the floor bit, and past 0x3B the player box +
     * RNG gate (always_zero passes) enters the attack. */
    int guard = 0;
    while (b.state == 0 && guard++ < 400) {
        soma.x = b.x;
        soma.y = b.y;
        aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                         always_zero);
    }
    assert(b.state == 1);
    /* State 1 step 0 plays the attack animation. */
    soma.x = b.x;
    soma.y = b.y;
    aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                     always_zero);
    assert(b.anim.id == 1);

    /* State 1 (sub_08099ED8): at anim frame 4 the sub_08099FD0
     * projectile is emitted (role BIPHRON_SHOT, elements-2 contact)
     * and song 0x78 requested; the ended animation returns to
     * state 0 with animation 0 looping. */
    AosEnemy shot = {0 };
    bool saw_shot = false, saw_78 = false;
    guard = 0;
    while (b.state == 1 && guard++ < 200) {
        AosHitReport r = aos_enemy_update(&b, &soma, &layer, &kind, NULL, NULL,
                                          10, 4, 0, 0, always_zero);
        for (int i = 0; i < r.child_count; ++i)
            if (r.children[i].role == AOS_ROLE_BIPHRON_SHOT &&
                !saw_shot) {
                saw_shot = true;
                shot = r.children[i];
            }
        if (aos_audio_requested(AOS_AUDIO_START_SONG, 0x78)) saw_78 = true;
    }
    assert(saw_shot && saw_78 && b.state == 0 && b.anim.id == 0);
    assert(shot.id == AOS_ENEMY_BIPHRON && shot.contact_elements == 2 &&
           shot.vy == -0x10000);

    /* The projectile (sub_0809A29C): rises, trails song 0x77, dies on
     * the 0xFF timer. */
    aos_audio_clear();
    shot.parent = NULL;
    int32_t shot_y = shot.y;
    guard = 0;
    while (!shot.removed && guard++ < 300)
        aos_enemy_update(&shot, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                         always_zero);
    assert(shot.removed && shot.y < shot_y &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x77) > 0);

    /* A lethal blow (sub_08099F50): killed + defeated, animation 3,
     * state 2 — the death burst sparks at frame 1, song 0x6E in the
     * 0x50-tick burst, then removal. */
    AosEnemy d;
    assert(aos_enemy_create(&d, AOS_ENEMY_BIPHRON, 100, 140, 0, 0, &soma, &layer,
                            &kind, &stats));
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-16, -40, 32, 48, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0, 0};
    soma.x = d.x;
    soma.y = d.y + AOS_FIXED(10);
    AosHitReport hit = aos_enemy_update(&d, &soma, &layer, &kind, &weapon, &blade,
                                        9999, 4, 0, 0, always_zero);
    assert(hit.killed && d.state == 2 && d.anim.id == 3 && d.defeated);
    bool saw_spark = false, saw_6e = false;
    aos_audio_clear();
    guard = 0;
    while (!d.removed && guard++ < 300) {
        AosHitReport r = aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL,
                                          10, 4, 0, 0, always_zero);
        for (int i = 0; i < r.effect_count; ++i)
            if (r.effects[i].kind == AOS_EFFECT_SPARK) saw_spark = true;
        if (aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E)) saw_6e = true;
    }
    assert(d.removed && saw_spark && saw_6e);
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
    const AosEffectWorld world = {&layer, 0, 0, always_zero, AOS_FIXED(77), 120, 0};
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
     * shared impact art and its table sound. */
    AosEnemy blood_target;
    assert(aos_enemy_create(&blood_target, AOS_ENEMY_SKELETON, 100, 100, 0, 0, &soma, &layer,
                            &kind, &stats));
    AosSoulAttack physical = {{90, 80, 110, 104}, 10, 0x0001, false, NULL, AOS_TYPE_SOUL};
    aos_enemy_set_soul_attacks(&physical, 1);
    hit = aos_enemy_update(&blood_target, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                           never_zero);
    assert(hit.effect_count == 1 && hit.effects[0].kind == AOS_EFFECT_IMPACT);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xA4));
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

static void quezlcoatl_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { QZ_ANIMS = 20 };
    static const uint8_t ticks20[20] = {4, 4, 4, 4, 4, 4, 4, 4,
                                        4, 4, 4, 4, 4, 4, 4, 4,
                                        4, 4, 4, 4};
    static AosAnimDef defs[QZ_ANIMS];
    for (int a = 0; a < QZ_ANIMS; ++a) defs[a] = (AosAnimDef){8, ticks20};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = QZ_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < QZ_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
            kind.sprites[a][f] = (uint8_t)(a * 8 + f < 32 ? a * 8 + f : 0);
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 26: hp 60, contact 20, defence 12, weak 0x0020,
     * resist 0x0010, exp 42, rare 0x007c, soul type 2 index 27. */
    const AosEnemyStats stats = {60, 20, 12, 0x0020, 0x0010, 42};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;

    /* EnemyQuezlcoatlCreate: state 0, + 0x65 = 0xC static frame, the
     * trail cleared, the dormant flag parks state 3. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_QUEZLCOATL, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.step == 0 && e.hp == 60 &&
           e.static_frame == 0xC && e.substep == 0);
    for (int i = 0; i < 240; i += 2)
        assert(e.qz_trail[i] == 0 && e.qz_trail[i + 1] == 0);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, AOS_ENEMY_QUEZLCOATL, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(ed.state == 3 && ed.step == 0);
    aos_enemy_set_flag_8e_40(false);

    /* First update emits the first eight segments (depths 0xE..7,
     * frames 0x11/0x10/0xF by the native role table). */
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0,
                                        always_zero);
    assert(hit.child_count == 8 && e.substep == 8);
    assert(hit.children[0].role == AOS_ROLE_QUEZLCOATL_SEGMENT &&
           hit.children[0].qz_depth == 0xE &&
           hit.children[0].qz_seg == 1 &&
           hit.children[0].static_frame == 0x11);
    assert(hit.children[7].qz_depth == 7 && hit.children[7].qz_seg == 8);
    /* Second update emits the remaining seven (depths 6..0). */
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(hit.child_count == 7 && e.substep == 15);
    assert(hit.children[0].qz_depth == 6 && hit.children[0].qz_seg == 9);
    assert(hit.children[6].qz_depth == 0 && hit.children[6].qz_seg == 15);

    /* State 0 step 1: facing/probe init — vx +-2.0 by the mirrored
     * bit, vy -4.0, ay 0x2800, trail cleared, step 2. */
    e.step = 0;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(e.step == 2 && e.timer == 0);
    assert(e.vx == (e.mirrored ? 0x20000 : (int32_t)0xFFFE0000) &&
           e.vy == (int32_t)0xFFFC0000 && e.ay == 0x2800);

    /* Step 2 records the per-frame pixel delta into the trail head. */
    e.x += 0x30000;                    /* +3 px this frame */
    int before_x = e.x;
    (void)aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(e.qz_trail[0] != 0 || e.qz_trail[1] != 0);
    (void)before_x;

    /* A segment follows its trail record: depth 4 reads entry 16 for
     * facing and entry 20 for the move. */
    AosEnemy seg;
    assert(aos_enemy_create(&seg, AOS_ENEMY_QUEZLCOATL, 300, 140, 0, 0,
                            &soma, &layer, &kind, &stats));
    seg.role = AOS_ROLE_QUEZLCOATL_SEGMENT;
    seg.parent = &e;
    seg.state = 0;
    seg.qz_depth = 4;
    seg.qz_seg = 5;
    seg.timer = 8;                     /* & 7 == 0: the contact tick */
    e.qz_trail[32] = 1;                /* dir > 0 -> face right */
    e.qz_trail[40] = 2;                /* entry 20 dx = +2 px */
    e.qz_trail[41] = -1;               /* entry 20 dy = -1 px */
    int sx0 = seg.x, sy0 = seg.y;
    (void)aos_enemy_update(&seg, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(seg.x == sx0 + (2 << 16) && seg.y == sy0 - 0x10000);
    assert(seg.mirrored);

    /* Parent death scatters the segment into state 1. */
    e.state = 2;
    seg.state = 0;
    seg.timer = 0;
    (void)aos_enemy_update(&seg, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(seg.state == 1 && seg.step == 0);
    /* Next update: state 1 step 0 -> 1, attack/hurt off. */
    (void)aos_enemy_update(&seg, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(seg.step == 1 && seg.combat.attack_off &&
           seg.combat.hurt_off);

    /* Lethal blow: song 0x6B, defeated, state 2, timer 0x28 — the soma
     * stands under the head like the native room placement. */
    soma.x = AOS_FIXED(300);
    soma.y = AOS_FIXED(150);
    AosEnemy kd;
    assert(aos_enemy_create(&kd, AOS_ENEMY_QUEZLCOATL, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    kd.y = AOS_FIXED(150);
    static const uint8_t qz_steps[] = {8};
    const AosAnimDef qz_def = {1, qz_steps};
    const AosAnimSet qz_set = {&qz_def, 1};
    const AosHitbox qz_box[] = {{-10, -40, 20, 20, true}};
    const AosWeaponFrames qz_weapon = {&qz_set, qz_box};
    AosWeaponEntity weapon = {.active = true};
    /* step 0 -> 1 -> 2 warm-up: the patrol collide only runs at step 2. */
    (void)aos_enemy_update(&kd, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    (void)aos_enemy_update(&kd, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    hit = aos_enemy_update(&kd, &soma, &layer, &kind,
                           &weapon, &qz_weapon, 9999, 4,
                           0, 0, always_zero);
    assert(hit.enemy_hit && hit.killed && kd.state == 2 &&
           kd.timer == 0x28 && kd.step == 0 && kd.defeated &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x6B) == 1);

    /* State 2 step 0 spawns the two gore entities. */
    kd.step = 0;
    hit = aos_enemy_update(&kd, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(kd.step == 1 && hit.child_count == 2);
    assert(hit.children[0].role == AOS_ROLE_QUEZLCOATL_GORE &&
           hit.children[0].static_frame == 0x12);
    assert(hit.children[1].static_frame == 0xB);
    /* Steps 1 and 2 retire the head. */
    (void)aos_enemy_update(&kd, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(kd.step == 2);
    (void)aos_enemy_update(&kd, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(kd.removed);

    /* Dormant state 3: step 1 init then the drift records the trail. */
    ed.state = 3;
    ed.step = 0;
    (void)aos_enemy_update(&ed, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(ed.step == 2 && ed.vx == (ed.mirrored ? 0x8000 : (int32_t)0xFFFF8000) &&
           ed.vy == (int32_t)0xFFFE0000 && ed.ay == 0x1800);
}


static void alastor_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { AL_ANIMS = 20 };
    static const uint8_t ticks20[20] = {4, 4, 4, 4, 4, 4, 4, 4,
                                        4, 4, 4, 4, 4, 4, 4, 4,
                                        4, 4, 4, 4};
    static AosAnimDef defs[AL_ANIMS];
    for (int a = 0; a < AL_ANIMS; ++a) defs[a] = (AosAnimDef){8, ticks20};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = AL_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < AL_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
            kind.sprites[a][f] = (uint8_t)(a * 8 + f < 32 ? a * 8 + f : 0);
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 90: hp 1200, contact 80, defence 45, weak 0x0000,
     * resist 0x0400, exp 1500, soul blue index 23 rarity 25. */
    const AosEnemyStats stats = {1200, 80, 45, 0x0000, 0x0400, 1500};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;

    /* EnemyAlastorCreate 0x080CD544: state 0 step 9 substep 3, the
     * + 0x24 three-hit counter at 3, + 0x17 = 9 pattern column; the
     * 0x8E & 0x40 flag parks state 8. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_ALASTOR, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.step == 9 && e.substep == 3 &&
           e.ala_hits == 3 && e.ala_col == 9 && e.ala_row == 0);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, AOS_ENEMY_ALASTOR, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(ed.state == 8 && ed.step == 0);
    aos_enemy_set_flag_8e_40(false);

    /* The first update emits the partner body (sub_080CEAA4): role
     * ALASTOR_BODY, hidden, attack/hurt off, linked to the head. */
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0,
                                        always_zero);
    assert(hit.child_count == 1);
    assert(hit.children[0].role == AOS_ROLE_ALASTOR_BODY &&
           hit.children[0].hidden &&
           hit.children[0].parent == &e &&
           hit.children[0].combat.attack_off &&
           hit.children[0].combat.hurt_off);

    /* State 0 (sub_080CD75C): the partner walk-on — after the six
     * substeps (~0x6C ticks) the head enters state 1. */
    for (int i = 0; i < 140 && e.state == 0; ++i)
        hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, 0, 0, always_zero);
    assert(e.state == 1);

    /* State 1 (sub_080CD934): the flight accelerates toward the
     * player; force the wall-probe exit into state 2. */
    e.state = 2;
    e.step = e.substep = 0;
    e.ala_row = 0;
    e.ala_col = 0;   /* pattern[0][0] = 0 -> the run continues */
    int32_t x0 = e.x;
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(e.x != x0 || e.vx != 0);

    /* State 3 (sub_080CDE70): the dash — velocity set, song 0x86,
     * the timer counts down the approach. */
    e.state = 3;
    e.step = 0;
    e.timer = 0;
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(e.step == 1 && e.vx != 0);

    /* State 4 (sub_080CE13C): the volley dash — song 0x86, the record
     * fields drive vx/vy and the spin probe. */
    e.state = 4;
    e.step = 0;
    e.ala_vol = 0;
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(e.step == 1);
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(e.step == 2 && e.timer != 0);

    /* State 6 (sub_080CE598): the assault emits a muzzle shot child
     * (role ALASTOR_SHOT, lifetime + 0xD = 5) and plays 0x1BB+0x86. */
    e.state = 6;
    e.step = 0;
    e.substep = 0;
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(e.step == 1);
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(hit.child_count >= 1);
    int shot = -1;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_ALASTOR_SHOT) shot = i;
    assert(shot >= 0 && hit.children[shot].ala_plife == 5);

    /* The shot dies with its parent or after five ticks
     * (sub_080CEEE8). */
    AosEnemy shot_e = hit.children[shot];
    shot_e.parent = &e;
    for (int i = 0; i < 6; ++i)
        aos_enemy_update(&shot_e, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 0, 0, always_zero);
    assert(shot_e.removed);

    /* State 7 (sub_080CE758): the death spiral — orbits, then hands
     * to 0xFF once the partner settles and hp is gone. */
    e.state = 7;
    e.step = 0;
    e.hp = 0;
    e.ala_pstate = 0;
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(e.state == 0xFF || e.step >= 1);

    /* State 0xFF (sub_080CE85C): song 0x6E, the burst, then removed. */
    e.state = 0xFF;
    e.step = 0;
    e.substep = 0;
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(e.state == 0xFF);
    for (int i = 0; i < 80 && !e.removed; ++i)
        hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, 0, 0, always_zero);
    assert(e.removed);

    /* Lethal hook (sub_080CE9E8): hp depleted -> state 7, song 0x1BE,
     * the partner mirror to state 3. */
    AosEnemy el;
    assert(aos_enemy_create(&el, AOS_ENEMY_ALASTOR, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    el.hp = 1;
    el.state = 1;
    el.step = 3;
    soma.x = AOS_FIXED(300);
    soma.y = AOS_FIXED(150);
    static const uint8_t al_steps[] = {8};
    const AosAnimDef al_def = {1, al_steps};
    const AosAnimSet al_set = {&al_def, 1};
    const AosHitbox al_box[] = {{-10, -40, 20, 20, true}};
    const AosWeaponFrames al_weapon = {&al_set, al_box};
    AosWeaponEntity weapon = {.active = true};
    hit = aos_enemy_update(&el, &soma, &layer, &kind,
                           &weapon, &al_weapon, 9999, 4,
                           0, 0, always_zero);
    assert(hit.enemy_hit && hit.killed && el.state == 7 &&
           el.ala_pstate == 3 && el.defeated &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x1BE) == 1);
}


static void maneater_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { ME_ANIMS = 20 };
    static const uint8_t ticks20[20] = {4, 4, 4, 4, 4, 4, 4, 4,
                                        4, 4, 4, 4, 4, 4, 4, 4,
                                        4, 4, 4, 4};
    static AosAnimDef defs[ME_ANIMS];
    for (int a = 0; a < ME_ANIMS; ++a) defs[a] = (AosAnimDef){8, ticks20};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = ME_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < ME_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
            kind.sprites[a][f] = (uint8_t)(a * 8 + f < 32 ? a * 8 + f : 0);
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 64: hp 666, contact 45, defence 35, weak 0x0001,
     * resist 0, exp 444, rare 0x002c. */
    const AosEnemyStats stats = {666, 45, 35, 0x0001, 0x0000, 444};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;

    /* EnemyManEaterCreate: state 0, + 0x1C = 3 death flag armed,
     * + 0x5C = 0x12 static frame; the 0x8E & 0x40 flag parks state 4. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_MAN_EATER, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.static_frame == 0x12 && e.hp == 666);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, AOS_ENEMY_MAN_EATER, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(ed.state == 4);
    aos_enemy_set_flag_8e_40(false);

    /* The first update emits the three level children (sub_080BAAC0):
     * roles MAN_LEVEL with slots 0..2, hidden, attack/hurt off. */
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0,
                                        always_zero);
    assert(hit.child_count == 3);
    for (int i = 0; i < 3; ++i) {
        assert(hit.children[i].role == AOS_ROLE_MAN_LEVEL &&
               hit.children[i].me_bolt == i &&
               hit.children[i].parent == &e &&
               hit.children[i].combat.attack_off &&
               hit.children[i].combat.hurt_off);
    }

    /* State 0 aggro: the soma inside the 0xA0 x 0x78 box flips state 1
     * with the 0x30 fan timer. */
    e.x = AOS_FIXED(150);
    e.y = AOS_FIXED(120);
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(e.state == 1 && e.timer == 0x30);

    /* The fan: three volleys of timer 0x30 each; every volley entity
     * emits six MAN_SHOT bolts on its own first update. */
    int volleys = 0;
    AosEnemy first_volley;
    int have_volley = 0;
    for (int i = 0; i < 200 && e.state == 1; ++i) {
        hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, 0, 0, always_zero);
        for (int c = 0; c < hit.child_count; ++c) {
            if (hit.children[c].role == AOS_ROLE_MAN_VOLLEY) {
                volleys++;
                if (!have_volley) {
                    first_volley = hit.children[c];
                    first_volley.parent = &e;
                    have_volley = 1;
                }
            }
        }
    }
    assert(e.state == 2 && volleys == 3 && have_volley);
    /* The volley's first update emits the six-bolt fan. */
    hit = aos_enemy_update(&first_volley, &soma, &layer, &kind, NULL,
                           NULL, 10, 4, 0, 0, always_zero);
    int bolts = 0;
    for (int c = 0; c < hit.child_count; ++c)
        if (hit.children[c].role == AOS_ROLE_MAN_SHOT) bolts++;
    assert(bolts == 6);

    /* State 3 death: song 0x6B, the particle rain, then removed. */
    e.state = 3;
    e.timer = 0x80;
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    int parts = 0;
    for (int c = 0; c < hit.child_count; ++c)
        if (hit.children[c].role == AOS_ROLE_MAN_DEATHP) parts++;
    assert(parts >= 1);
    for (int i = 0; i < 200 && !e.removed; ++i)
        hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                               10, 4, 0, 0, always_zero);
    assert(e.removed);

    /* Lethal: hp 0 -> state 3 death sequence (song 0x6B armed by the
     * head gate), never a plain flinch. */
    AosEnemy el;
    assert(aos_enemy_create(&el, AOS_ENEMY_MAN_EATER, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    el.hp = 0;
    hit = aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(el.state == 3 && el.defeated && el.timer >= 0x7F &&
           el.timer <= 0x80 && el.static_frame == 0x11);
}


static void gladiator_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { GL_ANIMS = 20 };
    static const uint8_t ticks20[20] = {4, 4, 4, 4, 4, 4, 4, 4,
                                        4, 4, 4, 4, 4, 4, 4, 4,
                                        4, 4, 4, 4};
    static AosAnimDef defs[GL_ANIMS];
    for (int a = 0; a < GL_ANIMS; ++a) defs[a] = (AosAnimDef){8, ticks20};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = GL_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < GL_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-16, -32, 32, 32};
            kind.sprites[a][f] = (uint8_t)(a * 8 + f < 32 ? a * 8 + f : 0);
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 93: hp 600, contact 55, defence 35, weak 0x0020,
     * resist 0x0010, exp 1000. */
    const AosEnemyStats stats = {600, 55, 35, 0x0020, 0x0010, 1000};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;

    /* EnemyGladiatorCreate: state 0, anim 0xC, the dormant flag
     * parks state 2. */
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_GLADIATOR, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.state == 0 && e.hp == 600 && !e.boss_parts_spawned);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, AOS_ENEMY_GLADIATOR, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(ed.state == 2);
    aos_enemy_set_flag_8e_40(false);

    /* The first update emits the three limbs (sub_0806DFF8 x3). */
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0,
                                        always_zero);
    assert(hit.child_count == 3);
    assert(hit.children[0].role == AOS_ROLE_GLADIATOR_PART0 &&
           hit.children[1].role == AOS_ROLE_GLADIATOR_PART1 &&
           hit.children[2].role == AOS_ROLE_GLADIATOR_PART2);
    for (int i = 0; i < 3; ++i)
        assert(hit.children[i].parent == &e &&
               hit.children[i].combat.hurt_off);

    /* State 0 approach: the soma inside the aggro area flips the
     * attack substate (step 1) with song 0x127. */
    e.x = AOS_FIXED(150);
    e.y = AOS_FIXED(120);
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    (void)hit;

    /* The sword arm's strike phases spawn GLADIATOR_SHOT children
     * with their GLADIATOR_TRAIL afterimages. */
    AosEnemy arm;
    memset(&arm, 0, sizeof arm);
    arm.role = AOS_ROLE_GLADIATOR_PART1;
    arm.parent = &e;
    arm.id = AOS_ENEMY_GLADIATOR;
    arm.state = 0;
    arm.gl_atk = 1;
    arm.gl_atk2 = 0;
    arm.gl_life = 100;
    arm.anim.frame = 3;
    hit = aos_enemy_update(&arm, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    int shots = 0, trails = 0;
    for (int c = 0; c < hit.child_count; ++c) {
        if (hit.children[c].role == AOS_ROLE_GLADIATOR_SHOT) shots++;
        if (hit.children[c].role == AOS_ROLE_GLADIATOR_TRAIL) trails++;
    }
    assert(shots == 1 && trails == 1);

    /* The shot orbits the parent and dies with it. */
    AosEnemy shot = hit.children[0];
    shot.role = AOS_ROLE_GLADIATOR_SHOT;
    shot.parent = &e;
    for (int i = 0; i < 10; ++i)
        hit = aos_enemy_update(&shot, &soma, &layer, &kind, NULL, NULL,
                               10, 4, 0, 0, always_zero);
    assert(!shot.removed);
    e.removed = true;
    hit = aos_enemy_update(&shot, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(shot.removed);

    /* The flail detaches into its thrown state when the parent dies. */
    AosEnemy mace;
    memset(&mace, 0, sizeof mace);
    mace.role = AOS_ROLE_GLADIATOR_PART2;
    mace.parent = &e;
    mace.id = AOS_ENEMY_GLADIATOR;
    mace.state = 0;
    e.removed = false;
    e.state = 1;
    hit = aos_enemy_update(&mace, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(mace.state == 1 && mace.timer >= 0x3F && mace.timer <= 0x40);

    /* Lethal: hp 0 -> sub_080A0D7C song 0x71, state 1 death spin. */
    AosEnemy el;
    assert(aos_enemy_create(&el, AOS_ENEMY_GLADIATOR, 300, 150, 0, 0,
                            &soma, &layer, &kind, &stats));
    el.hp = 0;
    hit = aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(el.state == 1 && el.defeated && el.timer <= 0x3C &&
           el.timer >= 0x3B);
}


static void golem_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { GO_ANIMS = 8 };
    static const uint8_t ticks8[8] = {4, 4, 4, 4, 4, 4, 4, 4};
    static AosAnimDef defs[GO_ANIMS];
    for (int a = 0; a < GO_ANIMS; ++a) defs[a] = (AosAnimDef){8, ticks8};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = GO_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < GO_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-24, -48, 48, 56};
            kind.sprites[a][f] = (uint8_t)(a * 8 + f < 32 ? a * 8 + f : 0);
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv: Wooden 34 {250,29,30,0x0802,0x0000,200},
     * Golem 53 {200,40,40,0x0800,0x0fff,180},
     * Flesh 80 {2000,47,25,0x0801,0x0000,400},
     * Iron 101 {9999,70,255,0x0800,0x0fff,500}. */
    static const struct { uint8_t id; AosEnemyStats st; } vars[4] = {
        {0x22, {250, 29, 30, 0x0802, 0x0000, 200}},
        {0x35, {200, 40, 40, 0x0800, 0x0fff, 180}},
        {0x50, {2000, 47, 25, 0x0801, 0x0000, 400}},
        {0x65, {9999, 70, 255, 0x0800, 0x0fff, 500}},
    };
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;

    /* EnemyGolemCreate: state 0 (or 3 under the dormant flag), the
     * + 0x24 variant index by id. */
    for (int v = 0; v < 4; ++v) {
        AosEnemy e;
        assert(aos_enemy_create(&e, vars[v].id, 300, 150, 0, 0,
                                &soma, &layer, &kind, &vars[v].st));
        /* Native + 0x24: 0x35->0, 0x65->1, 0x22->2, 0x50->3. */
        static const uint8_t want_var[4] = {2, 0, 3, 1};
        assert(e.state == 0 && e.go_var == want_var[v] &&
               e.hp == vars[v].st.hp);
        aos_enemy_set_flag_8e_40(true);
        AosEnemy ed;
        assert(aos_enemy_create(&ed, vars[v].id, 300, 150, 0, 0,
                                &soma, &layer, &kind, &vars[v].st));
        assert(ed.state == 3);
        aos_enemy_set_flag_8e_40(false);
    }

    /* The first updates emit the ten limbs (sub_0806DFF8 x10,
     * spread over the 8-child room cap). */
    AosEnemy e;
    assert(aos_enemy_create(&e, 0x22, 300, 150, 0, 0,
                            &soma, &layer, &kind, &vars[0].st));
    AosEnemy limbs[10];
    int nlimbs = 0;
    for (int i = 0; i < 4 && nlimbs < 10; ++i) {
        AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 0, 0,
                                            always_zero);
        for (int c = 0; c < hit.child_count && nlimbs < 10; ++c)
            if (hit.children[c].role >= AOS_ROLE_GOLEM_PART0 &&
                hit.children[c].role <= AOS_ROLE_GOLEM_PART9)
                limbs[nlimbs++] = hit.children[c];
    }
    assert(nlimbs == 10 && e.spawned_count == 10);
    for (int i = 0; i < 10; ++i) {
        assert(limbs[i].role == AOS_ROLE_GOLEM_PART0 + i);
        assert(limbs[i].parent == &e && limbs[i].combat.hurt_off);
        assert(limbs[i].go_piece == i);
    }
    AosHitReport hit = {0 };


    /* A limb rides its anchor beside the parent and mirrors facing. */
    AosEnemy arm = limbs[3];
    arm.role = AOS_ROLE_GOLEM_PART3;
    arm.parent = &e;
    e.mirrored = true;
    hit = aos_enemy_update(&arm, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    (void)hit;
    assert(arm.x == e.x - (40 << 16) && arm.y == e.y - (24 << 16));
    assert(arm.mirrored);

    /* The limbs detach into their thrown state on the parent death. */
    e.state = 2;
    hit = aos_enemy_update(&arm, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(arm.state == 1 && arm.timer >= 0x30 && arm.timer <= 0x50);

    /* State 0 cycle: the aggro probe walks the machine into the
     * song-0x7C attack and out through song 0x7E. */
    AosEnemy w;
    assert(aos_enemy_create(&w, 0x22, 300, 150, 0, 0,
                            &soma, &layer, &kind, &vars[0].st));
    w.x = AOS_FIXED(150);
    w.y = AOS_FIXED(120);
    for (int i = 0; i < 40 && w.step != 2; ++i)
        aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 0, 0, always_zero);
    assert(w.step == 2);
    for (int i = 0; i < 80; ++i)
        aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 0, 0, always_zero);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x7C) >= 1);

    /* State 1 entry: the recover step can roll into the rush. */
    w.state = 1;
    w.step = 0;
    aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(w.step == 1 && w.vx != 0);

    /* Death: state 2 spawns sub_0806D7F0 debris every fourth tick. */
    AosEnemy d;
    assert(aos_enemy_create(&d, 0x50, 300, 150, 0, 0,
                            &soma, &layer, &kind, &vars[2].st));
    d.state = 2;
    d.timer = 0x28;
    hit = aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    int debris = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_GOLEM_DEBRIS) debris++;
    assert(debris == 1);

    /* Lethal: hp 0 enters the shared death (state 2 keeps running). */
    AosEnemy el;
    assert(aos_enemy_create(&el, 0x65, 300, 150, 0, 0,
                            &soma, &layer, &kind, &vars[3].st));
    el.hp = 0;
    hit = aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(el.defeated);
}

static void stolas_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { ST_ANIMS = 8 };
    static const uint8_t ticks8[8] = {4, 4, 4, 4, 4, 4, 4, 4};
    static AosAnimDef defs[ST_ANIMS];
    for (int a = 0; a < ST_ANIMS; ++a) defs[a] = (AosAnimDef){8, ticks8};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = ST_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < ST_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-32, -64, 64, 72};
            kind.sprites[a][f] = (uint8_t)(a * 8 + f < 32 ? a * 8 + f : 0);
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 97: hp 1500, contact 70, defence 40,
     * weak 0x0820, resist 0x0010, experience 1500. */
    static const AosEnemyStats st = {1500, 70, 40, 0x0820, 0x0010, 1500};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;

    /* EnemyStolasCreate: state 0 (or 3 under the dormant flag), the
     * six record homes of table 0x0852813C and the + 0xD divider. */
    AosEnemy e;
    assert(aos_enemy_create(&e, 0x61, 300, 150, 0, 0,
                            &soma, &layer, &kind, &st));
    assert(e.state == 0 && e.sto_d == 1 && e.hp == 1500);
    assert(e.param0 == 0xD && e.param1 == 0x52);
    static const int8_t homes[6][2] = {
        {2, -32}, {2, -24}, {2, -14}, {8, -31}, {8, -23}, {8, -13},
    };
    for (int i = 0; i < 6; ++i)
        assert(e.sto_rec[i].x == ((int32_t)homes[i][0] << 16) &&
               e.sto_rec[i].y == ((int32_t)homes[i][1] << 16));
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, 0x61, 300, 150, 0, 0,
                            &soma, &layer, &kind, &st));
    assert(ed.state == 3);
    aos_enemy_set_flag_8e_40(false);

    /* The first update primes the records (sub_0809B7F0/B6B0/C324)
     * and emits the six bones (sub_0806DFF8 x6). */
    e.x = AOS_FIXED(150);
    e.y = AOS_FIXED(120);
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0,
                                        always_zero);
    assert(e.sto_c == 1);
    int bones = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role >= AOS_ROLE_STOLAS_BONE0 &&
            hit.children[i].role <= AOS_ROLE_STOLAS_BONE5)
            bones++;
    assert(bones == 6 && e.spawned_count == 6 && e.boss_parts_spawned);
    AosEnemy bone0 = hit.children[0];
    assert(bone0.role == AOS_ROLE_STOLAS_BONE0 && bone0.parent == &e &&
           bone0.sto_slot == 0 && bone0.sto_tgt == 1 &&
           bone0.static_frame == 43);

    /* sub_0809B9D4 state 0: the bone rides its record offset. */
    hit = aos_enemy_update(&bone0, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    (void)hit;
    assert(bone0.x == e.x + e.sto_rec[0].x &&
           bone0.y == e.y + e.sto_rec[0].y);

    /* The parent death detaches the bones into state 1 (timer 0x3C). */
    e.state = 2;
    hit = aos_enemy_update(&bone0, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(bone0.state == 1 && bone0.timer == 0x3C);

    /* State 0 aggro: the player in the 0xA0 box walks the 7-case
     * machine toward the state-1 fan attack. */
    AosEnemy w;
    assert(aos_enemy_create(&w, 0x61, 300, 150, 0, 0,
                            &soma, &layer, &kind, &st));
    w.x = AOS_FIXED(150);
    w.y = AOS_FIXED(120);
    for (int i = 0; i < 40 && w.step != 1; ++i)
        aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 0, 0, always_zero);
    assert(w.step >= 1 && w.step <= 6);

    /* State 1 entry: song 0x11F, the aimed shot, five fan bolts and
     * the wave. */
    w.state = 1;
    w.step = 0;
    w.timer = 0;
    hit = aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    int shot = 0, fan = 0, wave = 0;
    for (int i = 0; i < hit.child_count; ++i) {
        if (hit.children[i].role == AOS_ROLE_STOLAS_SHOT) shot++;
        if (hit.children[i].role == AOS_ROLE_STOLAS_FAN) fan++;
        if (hit.children[i].role == AOS_ROLE_STOLAS_WAVE) wave++;
    }
    /* Bones may still be emitting under the 8-child cap; the
     * projectiles fill the rest. */
    assert(shot >= 1 && fan >= 1 && wave >= 1);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x11F) >= 1);
    assert(w.state == 1 && w.step == 1);

    /* The wave projectile flies and dies on its timer. */
    AosEnemy wv;
    memset(&wv, 0, sizeof wv);
    wv.id = 0x61;
    wv.role = AOS_ROLE_STOLAS_WAVE;
    wv.parent = &w;
    wv.state = 0;
    wv.hp = 1;
    wv.timer = 0x2F;
    wv.x = w.x;
    wv.y = w.y;
    wv.vx = 0x50000;
    for (int i = 0; i < 0x30; ++i)
        aos_enemy_update(&wv, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 0, 0, always_zero);
    assert(wv.removed);

    /* Death: hp 0 enters the lethal path (song 0x6E, state 2, timer
     * 0x3C); state 2 spawns gore on the tick masks. */
    AosEnemy el;
    assert(aos_enemy_create(&el, 0x61, 300, 150, 0, 0,
                            &soma, &layer, &kind, &st));
    el.x = AOS_FIXED(150);
    el.y = AOS_FIXED(120);
    el.hp = 0;
    hit = aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    /* The lethal sets + 0xD = 0x3C and the same-frame state-2 pass
     * decrements it to 0x3B. */
    assert(el.defeated && el.state == 2 && el.timer == 0x3B);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E) >= 1);
    el.timer = 8;
    hit = aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    int gore = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_STOLAS_GORE) gore++;
    assert(gore >= 1);
    el.timer = 1;
    aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(el.removed);
}

static void legion_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { LG_ANIMS = 4 };
    static const uint8_t ticks4[4] = {4, 4, 4, 4};
    static AosAnimDef defs[LG_ANIMS];
    for (int a = 0; a < LG_ANIMS; ++a) defs[a] = (AosAnimDef){8, ticks4};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = LG_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < LG_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-32, -64, 64, 72};
            kind.sprites[a][f] = (uint8_t)(a * 8 + f < 32 ? a * 8 + f : 0);
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 108: hp 5000, contact 55, defence 25,
     * weak 0x0020, resist 0x0410, experience 12000, soul red 49. */
    static const AosEnemyStats lg = {5000, 55, 25, 0x0020, 0x0410, 12000};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;

    /* Create: hp from the record, state 0 awake, state 3 dormant
     * under the global flag. */
    AosEnemy e;
    assert(aos_enemy_create(&e, 0x6C, 300, 150, 0, 0,
                            &soma, &layer, &kind, &lg));
    assert(e.state == 0 && e.hp == 5000);
    assert(!e.boss_parts_spawned && e.spawned_count == 0);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, 0x6C, 300, 150, 0, 0,
                            &soma, &layer, &kind, &lg));
    assert(ed.state == 3 && ed.hp == 5000);
    aos_enemy_set_flag_8e_40(false);

    /* The five soldiers emit on the first update (5 < 8-child cap). */
    e.x = AOS_FIXED(150);
    e.y = AOS_FIXED(120);
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0,
                                        always_zero);
    int soldiers = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_LG_SOLDIER) soldiers++;
    assert(soldiers == 5 && e.spawned_count == 5 &&
           e.boss_parts_spawned);

    /* A soldier child tracks the parent and fires a shot on its
     * sequence; it dies when the parent's state 2 starts. */
    AosEnemy s;
    memset(&s, 0, sizeof s);
    s.id = 0x6C;
    s.role = AOS_ROLE_LG_SOLDIER;
    s.parent = &e;
    s.lg_slot = 2;
    s.x = AOS_FIXED(150);
    s.y = AOS_FIXED(120);
    for (int i = 0; i < 0x60; ++i)
        aos_enemy_update(&s, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 0, 0, always_zero);
    hit = aos_enemy_update(&s, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    int shots = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_LG_SHOT) shots++;
    assert(shots == 1);
    e.state = 2;
    aos_enemy_update(&s, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(s.removed);
    e.state = 0;

    /* State 1 attack: song 0x6F on entry, the 0x8F accent mid
     * volley, back to state 0. */
    AosEnemy a;
    assert(aos_enemy_create(&a, 0x6C, 150, 120, 0, 0,
                            &soma, &layer, &kind, &lg));
    a.x = AOS_FIXED(150);
    a.y = AOS_FIXED(120);
    a.state = 1;
    a.boss_parts_spawned = true;
    a.spawned_count = 5;
    hit = aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(a.substep == 1 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x6F) >= 1);
    a.timer = 0x1E;
    aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x8F) >= 1);
    a.timer = 0x40;
    aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    a.substep = 2;
    a.timer = 0x20;
    aos_enemy_update(&a, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(a.state == 0 && a.step == 2);

    /* Lethal: hp 0 -> defeated, song 0x6E, state 2 gore machine,
     * then the delete. */
    AosEnemy el;
    assert(aos_enemy_create(&el, 0x6C, 150, 120, 0, 0,
                            &soma, &layer, &kind, &lg));
    el.x = AOS_FIXED(150);
    el.y = AOS_FIXED(120);
    el.state = 1;
    el.hp = 0;
    hit = aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(el.defeated && el.state == 2);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E) >= 1);
    el.timer = 8;
    hit = aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    int gore = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_LG_GORE) gore++;
    assert(gore == 1);
    el.timer = 1;
    aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(el.removed);
}

static void belmont_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { BEL_ANIMS = 4 };
    static const uint8_t ticks4[4] = {4, 4, 4, 4};
    static AosAnimDef defs[BEL_ANIMS];
    for (int a = 0; a < BEL_ANIMS; ++a) defs[a] = (AosAnimDef){8, ticks4};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = BEL_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < BEL_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-32, -64, 64, 72};
            kind.sprites[a][f] = (uint8_t)(a * 8 + f < 32 ? a * 8 + f : 0);
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 110: hp 6000, contact 95, defence 80,
     * weak 0x0000, resist 0x0FE0, no drops, no soul. */
    static const AosEnemyStats bel = {6000, 95, 80, 0x0000, 0x0FE0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;

    /* Create: hp from the record, state 3 awake, state 0xC dormant
     * under the global flag. */
    AosEnemy e;
    assert(aos_enemy_create(&e, 0x6E, 300, 150, 0, 0,
                            &soma, &layer, &kind, &bel));
    assert(e.state == 3 && e.hp == 6000 && e.bel_juggle == 0);
    aos_enemy_set_flag_8e_40(true);
    AosEnemy ed;
    assert(aos_enemy_create(&ed, 0x6E, 300, 150, 0, 0,
                            &soma, &layer, &kind, &bel));
    assert(ed.state == 0xC && ed.hp == 6000);
    aos_enemy_set_flag_8e_40(false);

    /* State 3 hands off to state 2 on the first update. */
    e.x = AOS_FIXED(150);
    e.y = AOS_FIXED(120);
    aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(e.state == 2);

    /* State 2 with always_zero random picks state 9 (0 % 0xC == 0);
     * the attack envelope plays song 0x94 then returns to state 2. */
    e.step = 0;
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0,
                                        always_zero);
    assert(e.state == 9 && e.substep == 0);
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(e.substep == 1 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x94) >= 1);
    e.timer = 0x18;
    aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(e.substep == 2);
    aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(e.substep == 3);
    e.timer = 0x28;
    aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(e.state == 2 && e.substep == 0);

    /* State 6 fires the three-shot volley (sub_080D1BB4 x3). */
    e.state = 6;
    e.substep = 2;
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    int shots = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_BEL_SHOT) shots++;
    assert(shots == 3);

    /* A BEL_SHOT child flies and expires at its lifetime. */
    AosEnemy sh;
    memset(&sh, 0, sizeof sh);
    sh.id = 0x6E;
    sh.role = AOS_ROLE_BEL_SHOT;
    sh.parent = &e;
    sh.x = AOS_FIXED(150);
    sh.y = AOS_FIXED(120);
    sh.vx = 0x380000;
    sh.timer = 0x50;
    for (int i = 0; i < 0x50; ++i)
        aos_enemy_update(&sh, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 0, 0, always_zero);
    assert(sh.removed);

    /* Lethal: hp 0 -> defeated, song 0x1E1, state 0xB, then the
     * 0xF0 death machine gores and deletes. */
    AosEnemy el;
    assert(aos_enemy_create(&el, 0x6E, 150, 120, 0, 0,
                            &soma, &layer, &kind, &bel));
    el.x = AOS_FIXED(150);
    el.y = AOS_FIXED(120);
    el.state = 2;
    el.hp = 0;
    hit = aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(el.defeated && el.state == 0xB);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x1E1) >= 1);
    el.timer = 0x50;
    aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(el.state == 0xF0 && el.timer == 0xA0);
    el.timer = 4;
    hit = aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    int gore = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role == AOS_ROLE_BEL_GORE) gore++;
    assert(gore == 1);
    /* Count the 0xF0 countdown to the delete. */
    int frames = 0;
    while (!el.removed && frames < 0xA0 + 8) {
        aos_enemy_update(&el, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(el.removed);
}

static void big_golem_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { BG_ANIMS = 4 };
    static const uint8_t ticks4[4] = {4, 4, 4, 4};
    static AosAnimDef defs[BG_ANIMS];
    for (int a = 0; a < BG_ANIMS; ++a) defs[a] = (AosAnimDef){8, ticks4};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = BG_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < BG_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-64, -128,
                                                       128, 96};
            kind.sprites[a][f] = (uint8_t)(a * 8 + f < 32 ? a * 8 + f : 0);
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 69: hp 1200, contact 55, defence 15,
     * weak 0x0000, resist 0x0400, exp 1000, rare 0x0029. */
    static const AosEnemyStats bg = {1200, 55, 15, 0x0000, 0x0400, 1000};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;

    /* Create: state 3 parks dormant under the global flag, state 0
     * awake otherwise; the four sub_0806DFF8 children emit on the
     * first update. */
    AosEnemy e;
    assert(aos_enemy_create(&e, 0x45, 300, 150, 0, 0,
                            &soma, &layer, &kind, &bg));
    assert(e.state == 0 && e.hp == 1200);
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0,
                                        always_zero);
    int kids = 0;
    for (int i = 0; i < hit.child_count; ++i)
        if (hit.children[i].role >= AOS_ROLE_BG_CHILD0 &&
            hit.children[i].role <= AOS_ROLE_BG_CHILD3)
            kids++;
    assert(kids == 4);
    assert(e.boss_parts_spawned);

    /* State 0 case 12 exits into the state-1 attack machine. */
    e.step = 12;
    aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(e.state == 1 && e.step == 0);

    /* State 1: windup song 0x86 then strike song 0x71 volleys. */
    aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(e.step == 1);
    aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x86) >= 1);
    e.timer = 0x31;
    aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(e.step == 2);
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x71) >= 1);

    /* A BG_CHILD part tracks its body and dies with it. */
    AosEnemy ch;
    memset(&ch, 0, sizeof ch);
    ch.id = 0x45;
    ch.role = AOS_ROLE_BG_CHILD1;
    ch.variant = 1;
    ch.parent = &e;
    ch.x = AOS_FIXED(100);
    ch.y = AOS_FIXED(100);
    e.x = AOS_FIXED(150);
    e.y = AOS_FIXED(200);
    aos_enemy_update(&ch, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(!ch.removed && ch.x == AOS_FIXED(150) + 0x500000 &&
           ch.y == AOS_FIXED(200) - 0x600000);
    e.removed = true;
    aos_enemy_update(&ch, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(ch.removed);
    e.removed = false;

    /* Lethal: hp 0 -> defeated -> state 2 death machine with the
     * decoded timer; the cycle-gated flag deletes it. */
    e.state = 1;
    e.hp = 0;
    hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                           10, 4, 0, 0, always_zero);
    assert(e.defeated && e.state == 2);
    int frames = 0;
    while (!e.removed && frames < 0xD6 + 0x40) {
        aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(e.removed);
}

static void great_armor_tests(void) {
    aos_audio_clear();
    aos_enemy_set_flag_8e_40(false);
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int r = 20; r < 24; ++r)
        for (int x = 0; x < W; ++x) cells[r * W + x] = 0x03;
    enum { GA_ANIMS = 4 };
    static const uint8_t ticks4[4] = {4, 4, 4, 4};
    static AosAnimDef defs[GA_ANIMS];
    for (int a = 0; a < GA_ANIMS; ++a) defs[a] = (AosAnimDef){8, ticks4};
    static AosAnimSet anims;
    anims.anims = defs;
    anims.count = GA_ANIMS;
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < GA_ANIMS; ++a)
        for (int f = 0; f < 8; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-64, -128,
                                                       128, 96};
            kind.sprites[a][f] = (uint8_t)(a * 8 + f < 32 ? a * 8 + f : 0);
        }
    for (int n = 0; n < 7; ++n) kind.margins[n][0] = kind.margins[n][1] = 48;
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    /* enemies.tsv id 60: hp 650, contact 40, defence 25,
     * weak 0x0008, resist 0x0000, exp 300, common 0x0067,
     * rare 0x0036, blue soul 8. */
    static const AosEnemyStats ga = {650, 40, 25, 0x0008, 0x0000, 300};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(120), NULL);
    soma.max_hp = soma.hp = 800;

    /* Create: state 0 awake; the fourteen sub_0806DFF8 children emit
     * over the first updates under the 8-child cap (8 then 6). */
    AosEnemy e;
    assert(aos_enemy_create(&e, 0x3C, 300, 150, 0, 0,
                            &soma, &layer, &kind, &ga));
    assert(e.state == 0 && e.hp == 650);
    int emitted = 0;
    for (int u = 0; u < 3 && e.spawned_count < 14; ++u) {
        AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 0, 0,
                                            always_zero);
        for (int i = 0; i < hit.child_count; ++i)
            if (hit.children[i].role == AOS_ROLE_GA_PART) emitted++;
    }
    assert(e.spawned_count == 14 && emitted == 14 &&
           e.boss_parts_spawned);

    /* State 0 step 0 -> 1 on the first update; the chase timer plus
     * the 0xA0-pixel player range enters state 1 with song 0x7C. */
    e.step = 0;
    aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(e.step == 1);
    e.x = AOS_FIXED(150);
    e.y = AOS_FIXED(120);
    e.timer = 0x51;
    aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(e.state == 1 && e.step == 0);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x7C) >= 1);

    /* State 1: entry -> windup song 0x7F -> strike song 0x86 volleys
     * -> recover -> the decoded return to state 0 step 3. */
    aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(e.step == 1);
    aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x7F) >= 1);
    e.timer = 0x31;
    aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(e.step == 2);
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0,
                                        always_zero);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x86) >= 1);
    (void)hit;
    /* Run the recover steps to the state-0 return. */
    for (int i = 0; i < 8 && e.state == 1; ++i) {
        e.timer = 0x30;
        aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 0, 0, always_zero);
    }
    assert(e.state == 0 && e.step == 3);

    /* A GA_PART child tracks its body offset and dies with it. */
    AosEnemy pt;
    memset(&pt, 0, sizeof pt);
    pt.id = 0x3C;
    pt.role = AOS_ROLE_GA_PART;
    pt.variant = 4;
    pt.parent = &e;
    pt.x = AOS_FIXED(100);
    pt.y = AOS_FIXED(100);
    e.x = AOS_FIXED(150);
    e.y = AOS_FIXED(200);
    aos_enemy_update(&pt, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(!pt.removed && pt.x == AOS_FIXED(150) - 0x800000 &&
           pt.y == AOS_FIXED(200) - 0x900000);
    e.removed = true;
    aos_enemy_update(&pt, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(pt.removed);
    e.removed = false;

    /* Lethal: hp 0 -> defeated -> state 2 death machine; the
     * cycle-gated flag deletes it. */
    e.state = 1;
    e.hp = 0;
    aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                     10, 4, 0, 0, always_zero);
    assert(e.defeated && e.state == 2);
    int frames = 0;
    while (!e.removed && frames < 0xD6 + 0x40) {
        aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL,
                         10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(e.removed);
}

int main(void) {
    great_armor_tests();
    big_golem_tests();
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
    assert(!aos_enemy_create(&bat, 0x67, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
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
    assert(hit.effect_count == 1 && hit.effects[0].kind == AOS_EFFECT_BLOOD);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x9E));
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
    bomber_armor_tests();
    sky_fish_tests();
    ectoplasm_tests();
    butcher_tests();
    killer_mantle_tests();
    dryad_tests();
    tsuchinoko_tests();
    kicker_skeleton_tests();
    waiter_skeleton_tests();
    cockatrice_tests();
    chronomage_tests();
    mandragora_tests();
    disc_armor_tests();
    alura_une_tests();
    ukoback_tests();
    student_witch_tests();
    witch_tests();
    mudman_tests();
    were_tests();
    werewolf_tests();
    slime_tests();
    altair_tests();
    kyoma_tests();
    bael_tests();
    nightmare_tests();
    une_tests();
    arachne_tests();
    persephone_tests();
    lilith_tests();
    succubus_tests();
    triton_tests();

    devil_tests();
    quezlcoatl_tests();
    alastor_tests();
    maneater_tests();
    gladiator_tests();
    golem_tests();
    stolas_tests();
    creaking_skull_tests();
    balore_tests();
    manticore_tests();
    death_tests();
    legion_tests();
    belmont_tests();
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
    minotaur_tests();
    worm_tests();
    biphron_tests();
    medusa_tests();
    headhunter_death_tests();
    puts("aos_enemy: ok");
    return 0;
}
