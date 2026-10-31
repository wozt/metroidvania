/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Aria of Sorrow enemy framework and its ported enemy
 * state machines, including Nemesis. */
#include "aos_enemy.h"
#include "aos_audio.h"

#include <assert.h>
#include <stdio.h>
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

/* sub_080CBAEC: Buer state-2 has a 255-count entry trigger and a
 * 32..63-tick random attack window. Native linked sprite/probe behavior
 * remains separate; this test only checks reproducible state/RNG timing. */
/* sub_0806E29C / sub_080CBC64: the 96x48 inclusive trigger,
 * followed by the 80-frame attack/32-frame recovery clock and 0x84 cues. */
/* sub_080CBA30 / sub_080CBBDA / _080CBC4C: linked Buer fire owns
 * its facing, toggles it once per action window, and dictates the parent's
 * velocity sign when an even RNG draw ends that window. No ROM required. */
static void buer_linked_fire_facing_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2, 2};
    const AosAnimDef defs[] = {{2, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    /* Soma stays outside the native 96x48 attack trigger while Buer rolls. */
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy buer;
    assert(aos_enemy_create(&buer, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    assert(buer.state == 2 && buer.vx > 0 && buer.buer_fire_mirrored);
    AosHitReport spawned = aos_enemy_update(&buer, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(spawned.child_count == 1 &&
           spawned.children[0].role == AOS_ROLE_BUER_FIRE &&
           spawned.children[0].mirrored);
    AosEnemy fire = spawned.children[0];
    fire.parent = &buer;

    /* Window 1: positive angular speed, then the linked sprite
     * flips left; even expiry reverses the +0x18 spin, not +0x48 X speed. */
    const uint32_t left_draws[] = {0, 0};
    script = left_draws;
    script_at = 0;
    /* The preceding idle tick reversed angular speed and linked facing.
     * Establish a positive-spin/right-facing start for window 1. */
    buer.spin = 0x8000;
    buer.buer_fire_mirrored = true;
    buer.step = 2;
    buer.substep = buer.timer = 0;
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, scripted);
    assert(script_at == 1 && buer.substep == 2 && buer.timer == 31);
    assert(!buer.buer_fire_mirrored && buer.mirrored);
    aos_enemy_update(&fire, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(!fire.mirrored && fire.x == buer.x && fire.y == buer.y);
    for (int frame = 0; frame < 32; ++frame)
        aos_enemy_update(&buer, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, scripted);
    assert(script_at == 2 && buer.step == 0 && buer.substep == 0);
    assert(buer.spin < 0 && !buer.buer_fire_mirrored);
    assert(buer.vx > 0); /* Provisional translation is not the native spin. */

    /* Window 2: start spinning left, flip linked fire right, and restore
     * positive angular velocity on expiry without altering X velocity. */
    buer.x = AOS_FIXED(100);
    buer.spin = -0x8000;
    buer.step = 2;
    buer.substep = buer.timer = 0;
    const uint32_t right_draws[] = {0, 0};
    script = right_draws;
    script_at = 0;
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, scripted);
    assert(script_at == 1 && buer.buer_fire_mirrored);
    aos_enemy_update(&fire, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(fire.mirrored && fire.x == buer.x);
    for (int frame = 0; frame < 32; ++frame)
        aos_enemy_update(&buer, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, scripted);
    assert(script_at == 2 && buer.step == 0 && buer.spin > 0);
    assert(buer.vx > 0 && buer.buer_fire_mirrored);
    memset(cells, 0, sizeof cells);
}

/* sub_080CC514: a non-lethal weapon/soul hit during Buer's state 2
 * forces step 1, substep 0. State 1 is unaffected; a lethal hit still wins
 * and starts state-3 death. Verify via the full collision path, with a real
 * floor to avoid the off-screen snap-to-floor fixture failure of 0648. */
static void buer_surviving_hit_interrupt_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(220), AOS_FIXED(120), NULL);
    AosEnemy buer;

    assert(aos_enemy_create(&buer, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    assert(buer.state == 2 && (buer.y >> 16) <= 0xB0);
    buer.step = 2;
    buer.substep = 2;
    buer.timer = 47;
    buer.vx = 0;
    AosSoulAttack hitbox = {{0, 0, 255, 200}, 8, 0, false, NULL};
    aos_enemy_set_soul_attacks(&hitbox, 1);
    AosHitReport hit = aos_enemy_update(&buer, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0, never_zero);
    aos_enemy_set_soul_attacks(NULL, 0);
    assert(hitbox.struck && hitbox.target == &buer && hit.enemy_hit);
    assert(!hit.killed && !buer.defeated && buer.hp > 0);
    assert(buer.state == 2 && buer.step == 1 && buer.substep == 0);
    /* The callback never overwrites +0x0D: the earlier AI tick decremented
     * 47 to 46, and the next frame starts the existing 80-frame attack. */
    assert(buer.timer == 46);
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(buer.state == 2 && buer.step == 1 && buer.substep == 1 &&
           buer.timer == 0x4F);

    /* The same surviving strike must not force step 1 from state 1. Keep
     * this fixture within the GBA's Y+92 room bounds so the independent
     * room-exit transition does not legitimately switch it into state 2. */
    assert(aos_enemy_create(&buer, AOS_ENEMY_BUER, 100, 100,
                            0, 0, &soma, &layer, &kind, &stats));
    buer.y = AOS_FIXED(60);
    buer.home = 60;
    buer.vx = 0;
    buer.step = 7;
    buer.substep = 5;
    buer.timer = 19;
    AosSoulAttack standing = {{0, 0, 255, 200}, 8, 0, false, NULL};
    aos_enemy_set_soul_attacks(&standing, 1);
    hit = aos_enemy_update(&buer, &soma, &layer, &kind,
                           NULL, NULL, 10, 4, 0, 0, never_zero);
    aos_enemy_set_soul_attacks(NULL, 0);
    assert(standing.struck && hit.enemy_hit && !hit.killed && buer.hp > 0);
    assert(buer.state == 1 && buer.step == 7 && buer.substep == 5 &&
           buer.timer == 19);

    /* A lethal hit must dispatch state 3, not the nonlethal attack step. */
    assert(aos_enemy_create(&buer, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    buer.step = 2;
    buer.substep = 2;
    AosSoulAttack fatal = {{0, 0, 255, 200}, 255, 0, false, NULL};
    aos_enemy_set_soul_attacks(&fatal, 1);
    hit = aos_enemy_update(&buer, &soma, &layer, &kind,
                           NULL, NULL, 10, 4, 0, 0, never_zero);
    aos_enemy_set_soul_attacks(NULL, 0);
    assert(fatal.struck && hit.killed && buer.defeated);
    assert(buer.state == 3 && buer.step == 0 && buer.substep == 0);
    memset(cells, 0, sizeof cells);
}

/* sub_080CC3C8: the linked Buer fire copies its owner's coordinates only
 * while its own state is zero. It remains allocated during the owner's
 * staged death and disappears when the owner slot is removed, not on the
 * lethal frame. There is no independent collision pass during that death. */
static void buer_linked_fire_lifetime_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(220), AOS_FIXED(120), NULL);
    AosEnemy buer;
    assert(aos_enemy_create(&buer, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    AosHitReport made = aos_enemy_update(&buer, &soma, &layer, &kind,
                                         NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(made.child_count == 1 && made.children[0].role == AOS_ROLE_BUER_FIRE);
    AosEnemy fire = made.children[0];
    fire.parent = &buer;

    /* Native helper state 0 follows the owner, including any subpixel part. */
    buer.x = AOS_FIXED(105) + 0x1234;
    buer.y = AOS_FIXED(126) + 0x2345;
    AosHitReport hit = aos_enemy_update(&fire, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(fire.state == 0 && fire.x == buer.x && fire.y == buer.y);
    assert(!fire.removed && !hit.soma_hit && !hit.enemy_hit);

    /* Native helper state 1 freezes its coordinates, despite owner motion. */
    const int32_t frozen_x = fire.x, frozen_y = fire.y;
    fire.state = 1;
    buer.x = AOS_FIXED(140);
    buer.y = AOS_FIXED(130);
    aos_enemy_update(&fire, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(!fire.removed && fire.x == frozen_x && fire.y == frozen_y);

    /* Buer's staged death still uses the linked owner visual. A defeated
     * owner is not equivalent to a deleted owner, and contact is disabled. */
    buer.defeated = true;
    buer.state = 3;
    buer.step = 1;
    soma.x = frozen_x;
    soma.y = frozen_y;
    hit = aos_enemy_update(&fire, &soma, &layer, &kind,
                           NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(!fire.removed && !hit.soma_hit && !hit.enemy_hit);
    assert(fire.x == frozen_x && fire.y == frozen_y);
    fire.state = 0;
    aos_enemy_update(&fire, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(fire.x == buer.x && fire.y == buer.y && !fire.removed);

    /* A deleted owner finally retires its linked child. */
    buer.removed = true;
    aos_enemy_update(&fire, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(fire.removed);
    memset(cells, 0, sizeof cells);
}

/* sub_080CC458 -> sub_0806DF40: Buer enters state 2 when its
 * (x, y + 0x5C) probe leaves the room; one-screen bounds are 240 x 208.
 * The source resets step/substep but NOT the independent 8-bit timer. */
static void buer_room_exit_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    const AosCollision single = {1, 1, W, H, cells, NULL, false};
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(90), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100, 0, 0,
                            &soma, &single, &kind, &stats));
    assert(e.state == 1);

    /* Stable interior: motion must not falsely trigger a transition. */
    e.home = 70;
    e.x = AOS_FIXED(100);
    e.vx = 0;
    e.phase = 0;
    e.timer = 37;
    aos_enemy_update(&e, &soma, &single, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.state == 1 && e.timer == 37);

    /* The rightmost pixel of a one-screen room is 239, still inside. */
    e.x = AOS_FIXED(239);
    e.vx = 0;
    e.phase = 0;
    e.step = 4;
    e.substep = 5;
    aos_enemy_update(&e, &soma, &single, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.state == 1);

    /* First pixel after the right boundary: native state 2, both steps 0. */
    e.x = AOS_FIXED(240);
    e.vx = 0;
    aos_enemy_update(&e, &soma, &single, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.state == 2 && e.step == 0 && e.substep == 0 && e.timer == 37);

    /* Negative coordinates -7..-1 still round into tile zero. */
    e.state = 1;
    e.home = 70;
    e.x = -AOS_FIXED(7);
    e.vx = 0;
    e.phase = 0;
    aos_enemy_update(&e, &soma, &single, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.state == 1);
    e.x = -AOS_FIXED(8);
    e.vx = 0;
    aos_enemy_update(&e, &soma, &single, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.state == 2);

    /* 92-pixel Y probe reaches the 208-pixel room bottom. */
    e.state = 1;
    e.x = AOS_FIXED(100);
    e.vx = 0;
    e.home = 125;
    e.phase = 0;
    e.step = 2;
    e.substep = 3;
    aos_enemy_update(&e, &soma, &single, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.state == 2 && e.step == 0 && e.substep == 0);
    memset(cells, 0, sizeof cells);
}

static void buer_near_player_attack_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(220), AOS_FIXED(120), NULL);
    AosEnemy buer;
    assert(aos_enemy_create(&buer, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    assert(buer.state == 2);
    buer.step = 2;
    buer.substep = 2;
    buer.timer = 6;
    aos_audio_clear();
    /* Out of range: countdown continues without triggering step 1. */
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(buer.step == 2 && buer.substep == 2 && buer.timer == 5);
    /* Native inclusive RIGHT boundary. Buer-minus-Soma is
     * negative, selecting source side 1 and positive acceleration.
     * This is the native early-cap (31-tick) attack branch. */
    soma.x = buer.x + AOS_FIXED(48);
    soma.y = buer.y + AOS_FIXED(24);
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(buer.step == 1 && buer.substep == 0 && buer.timer == 4);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x84) == 0);
    /* The 80-frame timer is an upper bound: with Soma RIGHT and
     * positive starting spin, +0xCCC overshoots at attack tick 31.
     * Native saturation forces early recovery. */
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(buer.step == 1 && buer.substep == 1 && buer.timer == 0x4F);
    for (int frame = 0; frame < 29; ++frame)
        aos_enemy_update(&buer, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(buer.step == 1 && buer.substep == 1 && buer.timer == 0x32);
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(buer.step == 1 && buer.substep == 2 && buer.timer == 0x20);
    assert(buer.spin == 0x20000);
    /* Exactly 72, 63 and 54 still request native sound 0x84. */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x84) == 3);
    for (int frame = 0; frame < 33; ++frame)
        aos_enemy_update(&buer, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(buer.step == 1 && buer.substep == 0 && buer.timer == 0xFF);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x84) == 3);
    aos_audio_clear();

    /* Symmetric LEFT boundary selects source side 0 and negative
     * acceleration. Its cap arrives at attack tick 51, instead of 31,
     * because the source starts every Buer spinning positively. */
    AosEnemy opposite;
    assert(aos_enemy_create(&opposite, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    opposite.x = AOS_FIXED(100);
    opposite.y = AOS_FIXED(136);
    opposite.home = 136;
    opposite.state = 2;
    opposite.step = 2;
    opposite.substep = 2;
    opposite.timer = 6;
    opposite.spin = 0x8000;
    soma.x = opposite.x - AOS_FIXED(48);
    soma.y = opposite.y + AOS_FIXED(24);
    aos_enemy_update(&opposite, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(opposite.step == 1 && opposite.substep == 0 && opposite.timer == 5);
    for (int frame = 0; frame < 50; ++frame)
        aos_enemy_update(&opposite, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(opposite.away == -1 && opposite.step == 1 &&
           opposite.substep == 1 && opposite.timer == 0x1E);
    assert(opposite.spin == 0x8000 - 50 * 0xCCC);
    aos_enemy_update(&opposite, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(opposite.spin == -0x20000 && opposite.step == 1 &&
           opposite.substep == 2 && opposite.timer == 0x20);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x84) == 3);
    aos_audio_clear();
    memset(cells, 0, sizeof cells);
}

/* sub_080CBAEC/_080CBB2E: idle state-2 Buer wakes only on the side
 * faced by its linked fire, within the inclusive GBA 192x48 window. */
static void buer_idle_directional_wakeup_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy buer;
    assert(aos_enemy_create(&buer, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    assert(buer.state == 2);

    /* Exactly on either horizontal edge (+/-96) and vertical edge (+24)
     * is inside. The linked fire faces separately from the owner. */
    for (int side = 0; side < 2; ++side) {
        buer.x = AOS_FIXED(100);
        buer.y = AOS_FIXED(buer.home);
        buer.phase = 0;
        buer.vx = 0;
        /* Idle no-contact motion reverses +0x18 BEFORE the wake-up gate. */
        buer.spin = side ? -0x8000 : 0x8000;
        buer.state = 2;
        buer.step = buer.substep = buer.timer = 0;
        /* A deliberately stale bit proves the spin-based update. */
        buer.buer_fire_mirrored = side == 0;
        soma.x = buer.x + (side ? AOS_FIXED(96) - 0x8000 :
                                  -AOS_FIXED(96) + 0x8000);
        soma.y = buer.y + AOS_FIXED(24);
        aos_enemy_update(&buer, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, never_zero);
        assert(buer.state == 2 && buer.step == 1 && buer.substep == 0);
        assert(buer.timer == 1);
    }

    /* Wrong-facing side, aligned X (direction 0), outside X at 97 and
     * outside Y at 25 must not start the attack. */
    static const int dx[] = {-20, 0, 97, 30};
    static const int dy[] = {0, 0, 0, 25};
    for (int i = 0; i < 4; ++i) {
        buer.x = AOS_FIXED(100);
        buer.y = AOS_FIXED(buer.home);
        buer.phase = 0;
        buer.vx = 0;
        buer.spin = -0x8000; /* Idle reversal leaves facing right. */
        buer.state = 2;
        buer.step = buer.substep = buer.timer = 0;
        buer.buer_fire_mirrored = true;
        soma.x = buer.x + dx[i] * AOS_FIXED(1) -
                 (i == 1 ? 0x8000 : 0);
        soma.y = buer.y + AOS_FIXED(dy[i]);
        aos_enemy_update(&buer, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, never_zero);
        assert(buer.step == 0 && buer.substep == 0 && buer.timer == 1);
    }

    /* On the precise 0xFF byte boundary the original consumes one RNG
     * value after checking proximity. Odd retains step 1; even wins and
     * selects step 2. A previous frame never consumes that RNG draw. */
    for (int parity = 0; parity < 2; ++parity) {
        const uint32_t draws[] = {(uint32_t)parity};
        script = draws;
        script_at = 0;
        buer.x = AOS_FIXED(100);
        buer.y = AOS_FIXED(buer.home);
        buer.phase = 0;
        buer.vx = 0;
        buer.spin = -0x8000;
        buer.state = 2;
        buer.step = buer.substep = 0;
        buer.timer = 0xFD;
        buer.buer_fire_mirrored = true;
        soma.x = buer.x + AOS_FIXED(20);
        soma.y = buer.y;
        aos_enemy_update(&buer, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, scripted);
        assert(buer.step == 1 && buer.timer == 0xFE && script_at == 0);
        /* Restore the same rightward angular heading at the byte
         * threshold to exercise the independent RNG/attack ordering. */
        buer.step = 0;
        buer.spin = -0x8000;
        aos_enemy_update(&buer, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, scripted);
        assert(script_at == 1 && buer.timer == 0);
        assert(buer.step == (parity ? 1 : 2) && buer.substep == 0);
    }
    memset(cells, 0, sizeof cells);
}

/* sub_080CC20C: the linked fire's orientation follows the horizontal
 * velocity on rolling ticks, not the parent's player-facing mirror bit.
 * State-2 attack and recovery preserve the separately animated fire bit. */
static void buer_rolling_fire_sign_sync_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy buer;
    assert(aos_enemy_create(&buer, AOS_ENEMY_BUER, 100, 100,
                            0, 0, &soma, &layer, &kind, &stats));
    assert(buer.state == 1 && buer.spin == 0x8000);
    buer.home = 60;
    buer.y = AOS_FIXED(60); /* Keep the Y+92 room probe inside. */

    /* State-1 +0x48 now derives from pre-friction +0x18 spin.
     * Parent player facing remains separate from linked flame facing. */
    buer.vx = 0x18000;
    buer.spin = -0x8000;
    buer.buer_fire_mirrored = true;
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(buer.state == 1 && buer.vx == -0x8000 && buer.spin < 0);
    assert(buer.mirrored && !buer.buer_fire_mirrored);

    /* Zero initial spin means zero X speed for this tick; only the
     * later angular-friction branch raises spin to +0x4000. */
    buer.vx = -0x18000;
    buer.spin = 0;
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(buer.spin == 0x4000 && buer.vx == 0);
    assert(buer.buer_fire_mirrored);

    /* Old +/-64 provisional patrol boundaries no longer reverse X
     * motion in state 1; room-front and terrain checks control it. */
    buer.x = AOS_FIXED(buer.home_right);
    buer.vx = 0x18000;
    buer.spin = 0x8000;
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(buer.state == 1 && buer.vx == 0x8000 && buer.spin > 0);
    assert(buer.buer_fire_mirrored);

    /* Idle spin reverses/halves BEFORE choosing linked facing. */
    buer.x = AOS_FIXED(100);
    buer.state = 2;
    buer.step = 0;
    buer.timer = 4;
    buer.spin = 0x8000;
    buer.vx = 0x18000;
    buer.buer_fire_mirrored = true;
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(buer.step == 0 && buer.timer == 5);
    assert(buer.spin == -0x3FE0 && buer.vx == 0x8000);
    assert(!buer.buer_fire_mirrored);

    /* Attack and recovery must not overwrite independent fire facing,
     * even when angular speed has the opposite sign. */
    buer.spin = 0x8000;
    buer.buer_fire_mirrored = false;
    buer.step = 2;
    buer.substep = 2;
    buer.timer = 5;
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(buer.step == 2 && buer.timer == 4);
    assert(!buer.buer_fire_mirrored);
    buer.step = 1;
    buer.substep = 2;
    buer.timer = 12;
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(buer.step == 1 && buer.timer == 11);
    assert(!buer.buer_fire_mirrored);
    memset(cells, 0, sizeof cells);
}

/* Source-derived no-contact slice of sub_080CC20C. The collision flags,
 * terrain probes, affine emitter and horizontal controller remain partial. */
static void buer_no_contact_angular_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            0, 0, &soma, &layer, &kind, &stats));
    assert(e.state == 1 && e.spin == 0x8000 && e.angle == 0);
    /* Keep the native +92 room-exit probe in bounds and avoid the
     * provisional controller's horizontal edge reversal. */
    e.home = 60;
    e.y = AOS_FIXED(60);
    e.x = AOS_FIXED(100);
    e.vx = 0;
    e.phase = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.state == 1 && e.spin == 0x7FC0 && e.angle == 0x1FF);
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0x7F80 && e.angle == 0x1FF + 0x1FE);

    /* Negative spin decays toward -0x4000; signed angle steps truncate
     * toward zero rather than floor-dividing negative subpixels. */
    e.spin = -0x8000;
    e.angle = 0x10;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == -0x7FC0 && e.angle == (uint32_t)(0x10u - 0x1FFu));
    e.spin = 0;
    e.angle = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0x4000 && e.angle == 0x100);

    /* State-2 idle uses argument zero: AFTER each angle update the native
     * empty-contact branch reverses and halves spin. Entry into the
     * source idle gate must not erase the just-computed angle. */
    e.state = 2;
    e.step = e.substep = 0;
    e.timer = 10;
    e.spin = 0x8000;
    e.angle = 0;
    e.vx = 0;
    e.x = AOS_FIXED(100);
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.step == 0 && e.timer == 11 && e.angle == 0x1FF);
    assert(e.spin == -0x3FE0 && !e.buer_fire_mirrored);
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.timer == 12 && e.angle == 0xFF && e.spin == 0x2000);
    assert(e.buer_fire_mirrored);
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.timer == 13 && e.angle == 0x1FF && e.spin == -0x2000);
    assert(!e.buer_fire_mirrored);

    /* State-two step 1 recovery retains spin but advances affine angle
     * without idle friction; step 2 still has no rolling angular update. */
    for (int step = 1; step <= 2; ++step) {
        e.step = (uint8_t)step;
        e.substep = 2;
        e.timer = 12;
        e.spin = 0x6000;
        e.angle = 0x1200;
        aos_enemy_update(&e, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, never_zero);
        assert(e.spin == 0x6000);
        assert(e.angle == (uint32_t)(0x1200 + (step == 1 ? 0x180 : 0)));
    }
    memset(cells, 0, sizeof cells);
}

/* sub_080CC20C: axis-aligned solid forward wall at +/-24 gives the
 * native contact rebound (-spin/2), independent of the provisional X speed.
 * Air and floor/slope samples retain the already tested free-spin branch. */
static void buer_solid_wall_angular_bounce_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            0, 0, &soma, &layer, &kind, &stats));
    e.home = 60;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(60);
    e.vx = 0;
    e.phase = 0;

    /* A right-hand 0x03 wall receives a positive rolling probe. */
    cells[7 * W + 15] = 0x03; /* x + 24 = 124, y = 60 */
    e.spin = 0x8000;
    e.angle = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.state == 1 && e.angle == 0x200 && e.spin == -0x4000);
    assert(e.vx == 0x8000 && !e.buer_fire_mirrored);
    cells[7 * W + 15] = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == -0x4000 && e.angle == 0x100);

    /* The mirrored left-hand contact uses the rightward wall push. */
    cells[7 * W + 9] = 0x03; /* x - 24 = 76 */
    e.spin = -0x6000;
    e.angle = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0x3000 && e.angle == (uint32_t)(0u - 0x180u));
    assert(e.buer_fire_mirrored);
    cells[7 * W + 9] = 0;

    /* In state-2 idle, contact must not apply the empty-contact reversal
     * a second time; the byte timer still advances exactly once. */
    e.state = 2;
    e.step = e.substep = 0;
    e.timer = 4;
    e.spin = 0x8000;
    e.angle = 0;
    cells[7 * W + 15] = 0x03;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.step == 0 && e.timer == 5 && e.spin == -0x4000);
    assert(e.angle == 0x200 && !e.buer_fire_mirrored);

    /* Slopes are not promoted to solid-wall flags without a native probe. */
    cells[7 * W + 15] = 0x41;
    e.state = 1;
    e.spin = 0x8000;
    e.angle = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0x7FC0 && e.angle == 0x1FF);

    /* The attack window never executes the rolling contact branch. */
    cells[7 * W + 15] = 0x03;
    e.state = 2;
    e.step = 2;
    e.substep = 2;
    e.timer = 7;
    e.spin = 0x8000;
    e.angle = 0x1234;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0x8000 && e.angle == 0x1234);
    memset(cells, 0, sizeof cells);
}

/* A bounded slope probe exercises the original sub_080CC20C flag
 * 0x20/0x40 angular impulses without claiming complete native terrain
 * sampling or overwriting the solid-wall collision path from patch 0659. */
static void buer_floor_slope_angular_impulse_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            0, 0, &soma, &layer, &kind, &stats));
    e.vx = 0;
    e.home = 100;
    /* x=100, y=100: the bottom probe is (100,124), row 15/column 12.
     * Source-facing slope 0x41 and X-flipped slope 0x45 are both
     * penetrated here. Native 0x20 grows and 0x40 reduces angular speed. */
    static const struct {
        uint8_t tile;
        int32_t spin;
        int32_t expected;
    } cases[] = {
        {0x41, 0x6000, 0x6333},
        {0x45, 0x6000, 0x5CCD},
        {0x41, -0x6000, -0x5CCD},
        {0x45, -0x6000, -0x6333},
        {0x45, 0x0200, -0x0133},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        cells[15 * W + 12] = cases[i].tile;
        e.x = AOS_FIXED(100);
        e.y = AOS_FIXED(100);
        e.home = 100;
        e.phase = 0;
        e.state = 1;
        e.step = e.substep = 0;
        e.spin = cases[i].spin;
        e.angle = 0;
        aos_enemy_update(&e, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, never_zero);
        assert(e.state == 1 && e.spin == cases[i].expected);
        assert(e.angle == (uint32_t)(cases[i].expected / 64));
    }

    /* A slope tile outside the lower probe is not a contact. */
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(95);
    e.home = 95;
    e.phase = 0;
    e.spin = 0x6000;
    e.angle = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0x5FC0 && e.angle == 0x17F);

    /* With native flag 0x40 or 0x20, state-2 idle does not also
     * execute the no-contact reverse-and-half angular response. */
    cells[15 * W + 12] = 0x41;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(100);
    e.home = 100;
    e.phase = 0;
    e.state = 2;
    e.step = e.substep = 0;
    e.timer = 4;
    e.spin = -0x6000;
    e.angle = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.step == 0 && e.timer == 5 && e.spin == -0x5CCD);
    assert(e.angle == (uint32_t)(0u - 0x173u));

    /* The solid-wall collision still has priority over a lower slope. */
    cells[12 * W + 15] = 0x03; /* forward sample (124,100) */
    e.state = 1;
    e.spin = 0x6000;
    e.angle = 0;
    e.y = AOS_FIXED(100);
    e.home = 100;
    e.phase = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == -0x3000 && e.angle == 0x180);
    cells[12 * W + 15] = 0;

    /* State-2 attack windows do not execute the rolling slope probe. */
    e.state = 2;
    e.step = e.substep = 2;
    e.timer = 7;
    e.spin = 0x6000;
    e.angle = 0x1234;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0x6000 && e.angle == 0x1234 && e.timer == 6);
    memset(cells, 0, sizeof cells);
}

/* Patch 0661: source +0x18 spin is the mode-2 +0x48 X velocity
 * BEFORE the angular friction/bounce step, never the provisional speed. */
static void buer_state_one_horizontal_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            0, 0, &soma, &layer, &kind, &stats));
    e.home = 60;
    e.y = AOS_FIXED(60);
    e.phase = 0;

    /* The constructor's 0x8000 spin moves one half-pixel. Friction only
     * affects the NEXT tick's signed native X translation. */
    e.x = AOS_FIXED(100) + 0x2000;
    e.vx = -0x30000;
    e.spin = 0x8000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.state == 1 && e.x == AOS_FIXED(100) + 0xA000);
    assert(e.vx == 0x8000 && e.spin == 0x7FC0);
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(101) + 0x1FC0);
    assert(e.vx == 0x7FC0 && e.spin == 0x7F80);

    /* Signed negative subpixel motion, independent of player facing. */
    e.x = AOS_FIXED(100) + 0x2000;
    e.spin = -0x6000;
    e.vx = 0x18000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(99) + 0xC000);
    assert(e.vx == -0x6000 && e.spin == -0x5FC0 && e.mirrored);

    /* The native horizontal translation is bounded to eight pixels. */
    e.x = AOS_FIXED(100);
    e.spin = 0x90000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(108) && e.vx == 0x80000);

    /* A front solid wall causes wrapper rollback of the attempted X. */
    e.x = AOS_FIXED(100);
    e.spin = 0x8000;
    e.y = AOS_FIXED(60);
    e.phase = 0;
    cells[7 * W + 15] = 0x03; /* (100+24,60) lies in this tile */
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) && e.vx == 0x8000);
    assert(e.spin == -0x4000);
    cells[7 * W + 15] = 0;

    /* The shared test layer spans TWO screens (512px): x=217 with a
     * +24 forward probe remains inside, so the half-pixel motion survives. */
    e.x = AOS_FIXED(217);
    e.spin = 0x8000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(217) + 0x8000 && e.vx == 0x8000);

    /* One-screen rooms use 30 tiles = 240px. At x=217, the forward
     * +24 probe crosses x=240 after attempted movement: X rolls back. */
    const AosCollision single_screen = {1, 1, W, H, cells, NULL, false};
    e.x = AOS_FIXED(217);
    e.spin = 0x8000;
    aos_enemy_update(&e, &soma, &single_screen, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(217) && e.vx == 0x8000);
    e.x = AOS_FIXED(e.home_right);
    e.spin = 0x8000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(e.home_right) + 0x8000 && e.vx == 0x8000);

    /* State-2 active attack is unchanged: +0x48 is still provisional
     * until the separate native mode-6 probe and attack motion are ported. */
    e.x = AOS_FIXED(100);
    e.state = 2;
    e.step = 1;
    e.substep = 2;
    e.timer = 12;
    e.vx = 0x18000;
    e.spin = 0x8000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(101) + 0x8000);
    assert(e.vx == 0x18000 && e.spin == 0x8000);
    memset(cells, 0, sizeof cells);
}

/* Patch 0662: in state-2 idle, mode 6 copies pre-contact spin to
 * horizontal speed but rolls back X if no grounded flag 8 was observed.
 * No unsupported slope/contact geometry is treated as solid floor. */
static void buer_state_two_idle_horizontal_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    assert(e.state == 2 && e.step == 0);

    /* Flat floor at y=160: x moves by exactly 0x8000 before damping,
     * without using the old provisional 1.5-pixel horizontal step. */
    e.x = AOS_FIXED(100) + 0x2000;
    e.y = AOS_FIXED(136);
    e.home = 136;
    e.phase = 0;
    e.vx = -0x18000;
    e.spin = 0x8000;
    e.timer = 4;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.step == 0 && e.timer == 5);
    assert(e.x == AOS_FIXED(100) + 0xA000);
    assert(e.vx == 0x8000 && e.spin == -0x3FE0);

    /* Negative pre-contact spin moves left, again ahead of angular
     * reversal. The entire fractional portion must be preserved. */
    e.x = AOS_FIXED(100) + 0x2000;
    e.y = AOS_FIXED(136);
    e.phase = 0;
    e.spin = -0x6000;
    e.timer = 6;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(99) + 0xC000);
    assert(e.vx == -0x6000 && e.spin == 0x2FE0);

    /* No floor: mode-six's missing flag 8 restores old X, rather than
     * running the old provisional patrol. The velocity is still written. */
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(60);
    e.home = 60;
    e.phase = 0;
    e.spin = 0x8000;
    e.timer = 8;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) && e.vx == 0x8000);
    assert(e.timer == 9 && e.spin == -0x3FE0);

    /* Rolling over an exposed ledge is not falsely declared grounded:
     * the destination foot cell is checked after X motion as well. */
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(136);
    e.home = 136;
    e.phase = 0;
    e.spin = 0x90000;
    e.timer = 9;
    cells[20 * W + 13] = 0;
    cells[20 * W + 10] = 0; /* rear of destination x=108 */
    cells[20 * W + 16] = 0; /* front of destination x=108 */
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) && e.vx == 0x80000);
    cells[20 * W + 13] = 0x03;
    cells[20 * W + 10] = 0x03;
    cells[20 * W + 16] = 0x03;

    /* A flat floor and a solid right wall trigger the conservative
     * native X rollback, while angular impact still reverses spin. */
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(136);
    e.home = 136;
    e.phase = 0;
    e.spin = 0x8000;
    e.timer = 10;
    cells[17 * W + 15] = 0x03; /* forward centre: x=124, y=136 */
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) && e.vx == 0x8000);
    assert(e.spin == -0x4000 && e.timer == 11);
    cells[17 * W + 15] = 0;

    /* The 240-pixel single-screen forward boundary restores old X,
     * while the same x in a 512-pixel double-screen room remains valid. */
    const AosCollision single = {1, 1, W, H, cells, NULL, false};
    e.y = AOS_FIXED(136);
    e.phase = 0;
    e.spin = 0x8000;
    e.x = AOS_FIXED(217);
    e.timer = 12;
    aos_enemy_update(&e, &soma, &single, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(217));
    e.y = AOS_FIXED(136);
    e.phase = 0;
    e.spin = 0x8000;
    e.x = AOS_FIXED(217);
    e.timer = 12;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(217) + 0x8000);

    /* Attack windows must continue using their provisional controller;
     * neither +0x18 nor affine angle is touched by rolling logic. */
    e.state = 2;
    e.step = 1;
    e.substep = 2;
    e.timer = 12;
    e.x = AOS_FIXED(100);
    e.spin = 0x8000;
    e.angle = 0x1234;
    e.vx = 0x18000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(101) + 0x8000);
    assert(e.spin == 0x8000 && e.angle == 0x1434);
    memset(cells, 0, sizeof cells);
}

/* Patch 0663: bounded vertical rollback in state-two idle mode 6.
 * Only shallow penetration in a confirmed flat floor is corrected. */
static void buer_state_two_idle_vertical_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    assert(e.state == 2 && e.step == 0);
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(137) + 0x2345;
    e.home = 40; /* Prove idle does not inherit the provisional sine bob. */
    e.spin = 0;
    e.vx = 0;
    e.timer = 5;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.step == 0 && e.timer == 6);
    assert(e.y == AOS_FIXED(136)); /* Floor top 160 minus 24. */
    assert(e.x == AOS_FIXED(100));

    /* A pre-aligned grounded Buer remains at the exact floor contact. */
    e.y = AOS_FIXED(136);
    e.spin = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.y == AOS_FIXED(136));

    /* With no contact, the mode-six rollback holds Y; it does not bob
     * toward the independently stored provisional home position. */
    e.y = AOS_FIXED(60) + 0x4567;
    e.home = 140;
    e.spin = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.y == AOS_FIXED(60) + 0x4567);

    /* Neither slope geometry nor upward motion may be classified as a
     * supported flat-floor correction in this bounded implementation. */
    cells[20 * W + 12] = 0x41;
    cells[20 * W + 9] = 0;  /* no rear flat support */
    cells[20 * W + 15] = 0; /* no leading flat support */
    e.y = AOS_FIXED(137) + 0x1234;
    e.spin = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.y == AOS_FIXED(137) + 0x1234);
    cells[20 * W + 12] = 0x03;
    cells[20 * W + 9] = 0x03;
    cells[20 * W + 15] = 0x03;
    e.vy = -0x4000;
    e.spin = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.y == AOS_FIXED(137) + 0x1234);
    e.vy = 0;

    /* Active attacks retain their own provisional controller until
     * the original native attack choreography is reconstructed. */
    e.step = 1;
    e.substep = 2;
    e.timer = 12;
    e.y = AOS_FIXED(70);
    e.home = 136;
    e.spin = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert((e.y >> 16) > 120 && e.spin == 0);
    memset(cells, 0, sizeof cells);
}

static void buer_attack_timing_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2, 2};
    const AosAnimDef defs[] = {{2, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(220), AOS_FIXED(120), NULL);
    AosEnemy buer;
    assert(aos_enemy_create(&buer, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    assert(buer.state == 2 && buer.step == 0 && buer.substep == 0);
    /* The initial phase consumes RNG only at the 0xFF boundary. */
    const uint32_t draws[] = {0, 15, 0};
    script = draws;
    script_at = 0;
    buer.timer = 0xFD;
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, scripted);
    assert(buer.timer == 0xFE && buer.step == 0 && script_at == 0);
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, scripted);
    assert(buer.timer == 0 && buer.step == 2 && buer.substep == 0 && script_at == 1);
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, scripted);
    assert(buer.step == 2 && buer.substep == 2 && buer.timer == 46 && script_at == 2);
    for (int frame = 0; frame < 47; ++frame)
        aos_enemy_update(&buer, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, scripted);
    assert(buer.step == 0 && buer.substep == 0 && buer.timer == 0);
    assert(script_at == 3);

    /* An odd random draw at expiry starts another window, without
     * returning to the initial long idle phase. */
    const uint32_t retry[] = {31, 1, 31};
    script = retry;
    script_at = 0;
    buer.state = 2;
    buer.step = 2;
    buer.substep = 0;
    buer.timer = 0;
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, scripted);
    assert(buer.step == 2 && buer.substep == 2 && buer.timer == 62 && script_at == 1);
    for (int frame = 0; frame < 63; ++frame)
        aos_enemy_update(&buer, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, scripted);
    assert(buer.step == 2 && buer.substep == 0 && buer.timer == 0xFF && script_at == 2);
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, scripted);
    assert(buer.step == 2 && buer.substep == 2 && buer.timer == 62 && script_at == 3);
    memset(cells, 0, sizeof cells);
}

/* Buer's GBA sub_080CC514 / sub_080CBEA4: lethal hit enters
 * state 3, step 0; its updater starts sound 0x6E and a 96-frame stage,
 * followed by a 128-frame stage with 20-frame interval sound cues. */
static void buer_death_stages_tests(void) {
    memset(cells, 0, sizeof cells);
    /* Buer's create routine snaps to a floor. Without a solid tile,
     * snap_to_floor scans off-screen and skips the entire collision pass.
     * Use the same at-y=160 test floor as the early-enemy batch tests. */
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2, 2, 2};
    const AosAnimDef defs[] = {{3, ticks}, {3, ticks}, {3, ticks}};
    const AosAnimSet anims = {defs, 3};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    for (int a = 0; a < 3; ++a)
        for (int f = 0; f < 3; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -8, 16, 16};
        }
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(120), NULL);
    AosEnemy buer;
    assert(aos_enemy_create(&buer, AOS_ENEMY_BUER, 100, 100,
                            0, 0, &soma, &layer, &kind, &stats));
    assert((buer.y >> 16) > 0 && (buer.y >> 16) <= 0xB0);
    AosSoulAttack soul = {{0, 0, 255, 200}, 255, 0, false, NULL};
    aos_enemy_set_soul_attacks(&soul, 1);
    AosHitReport hit = aos_enemy_update(&buer, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0, always_zero);
    aos_enemy_set_soul_attacks(NULL, 0);
    assert(soul.struck && soul.target == &buer);
    assert(hit.killed && buer.defeated && buer.state == 3 && buer.step == 0);
    assert(!buer.removed);
    aos_audio_clear();
    int32_t death_x = buer.x, death_y = buer.y;

    hit = aos_enemy_update(&buer, &soma, &layer, &kind,
                           NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(buer.step == 1 && buer.timer == 0x5F && buer.state == 3);
    assert(buer.combat.attack_off && buer.combat.hurt_off);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E) == 1);
    assert(!hit.child_count && !hit.effect_count && !hit.soma_hit);
    for (int frame = 0; frame < 96; ++frame)
        aos_enemy_update(&buer, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(buer.step == 2 && buer.timer == 0x80 && !buer.removed);
    assert(buer.x == death_x && buer.y == death_y);
    for (int frame = 0; frame < 129; ++frame)
        aos_enemy_update(&buer, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(buer.step == 3 && buer.timer == 0xFF && !buer.removed);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x6E) > 1);
    aos_enemy_update(&buer, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(buer.removed && buer.hidden);
    aos_audio_clear();
    memset(cells, 0, sizeof cells);
}

/* EnemyBuerUpdate state 4 dispatches sub_080CC48C instead of the normal
 * Buer walking/contact path. The native visual effect is still missing. */
/* Patch 0664: cross-phase Buer attack/death mechanics. These tests
 * deliberately use ROM-free tiles and fixed callback RNG. */
static void buer_attack_death_animation_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(20), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(136);
    e.home = 136;
    e.state = 2;
    e.step = 1;
    e.substep = 0;
    e.spin = 0x8000;
    e.angle = 0;
    AosHitReport result = aos_enemy_update(&e, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 0, 0, never_zero);
    /* Soma LEFT: Buer-minus-Soma is positive, giving source
     * +0x13=0 and NEGATIVE angular acceleration. */
    assert(e.substep == 1 && e.timer == 0x4F && e.away == -1);
    assert(e.spin == 0x7334 && e.angle == (uint32_t)-0x400 &&
           result.effect_count == 0);
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    result = aos_enemy_update(&e, &soma, &layer, &kind,
                              NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.timer == 0x4D && result.child_count == 2 &&
           result.effect_count == 0);
    assert(result.children[0].role == AOS_ROLE_BUER_PROJECTILE &&
           result.children[1].role == AOS_ROLE_BUER_PROJECTILE);
    assert(result.children[0].variant == 0 && result.children[1].variant == 1);
    assert(result.children[0].x == result.children[1].x &&
           result.children[0].y == result.children[1].y);

    /* Soma RIGHT selects source side 1: positive acceleration. */
    soma.x = AOS_FIXED(400);
    e.x = AOS_FIXED(100);
    e.step = 1;
    e.substep = 0;
    e.spin = -0x8000;
    e.angle = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.away == 1 && e.spin == -0x7334 && e.angle == 0x400);

    /* Death begins at +/-0x20000, decelerates by 0x380, and rotates by
     * signed spin / 64; do not mistake this for the idle 0x40 friction. */
    e.defeated = true;
    e.state = 3;
    e.step = 0;
    e.timer = 0;
    e.spin = -0x8000;
    e.angle = 0;
    result = aos_enemy_update(&e, &soma, &layer, &kind,
                              NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.step == 1 && e.timer == 0x5F && e.spin == -0x1FC80);
    assert(e.angle == (uint32_t)(-0x1FC80 / 64) && result.effect_count == 0);
    e.timer = 91; /* first-stage multiple of 7; one visual flame */
    result = aos_enemy_update(&e, &soma, &layer, &kind,
                              NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(result.effect_count == 1 && result.effects[0].kind == AOS_EFFECT_FLAME);
    e.step = 2;
    e.timer = 128;
    e.spin = 0x8000;
    e.angle = 0;
    e.y = AOS_FIXED(60); /* No floor-slope or forward-wall fixture. */
    const int32_t before_death_x = e.x;
    result = aos_enemy_update(&e, &soma, &layer, &kind,
                              NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == before_death_x + 0x8000 && e.vx == 0x8000);
    assert(e.spin == 0x7FC0 && e.angle == 0x1FF && e.timer == 127);
    assert(result.effect_count == 1 && result.effects[0].kind == AOS_EFFECT_FLAME);
    memset(cells, 0, sizeof cells);
}

/* sub_080CC5D8 + sub_080CC178: two independent animation-selected
 * fire entities, no guessed weapon boxes, no independent loot, and correct
 * velocity/acceleration ordering with an 8.0 maximum speed. */
/* Patch 0674: Buer's owner-linked flame has no independent collision
 * callback in native sub_080CC3C8, whereas the body remains targetable.
 * Synthetic animation boxes intentionally cover a very wide area to
 * detect accidental calls to collide() on this non-interactive child. */
static void buer_visual_only_linked_fire_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    kind.modes[0][0] = 1;
    kind.attack[0][0] = kind.hurt[0][0] = (AosBox){-64, -64, 128, 128};
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(136), NULL);
    AosEnemy owner;
    assert(aos_enemy_create(&owner, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    AosHitReport created = aos_enemy_update(&owner, &soma, &layer, &kind,
                                             NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(created.child_count == 1 &&
           created.children[0].role == AOS_ROLE_BUER_FIRE);
    AosEnemy fire = created.children[0];
    assert(fire.static_frame == 1 && !fire.own_boxes);
    assert(fire.combat.attack_off && fire.combat.hurt_off);
    fire.parent = &owner;
    fire.state = 0;
    fire.x = soma.x;
    fire.y = soma.y;
    /* Do not let a collision on the owner mask a false child contact. */
    soma.combat = (AosCombat){0};
    const int16_t hp = soma.hp;
    AosHitReport contact = aos_enemy_update(&fire, &soma, &layer, &kind,
                                             NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(!contact.soma_hit && !contact.enemy_hit && !contact.killed);
    assert(soma.hp == hp && !fire.removed);
    assert(fire.x == owner.x && fire.y == owner.y);
    fire.state = 1;
    const int32_t frozen_x = fire.x, frozen_y = fire.y;
    owner.x += AOS_FIXED(10);
    owner.y += AOS_FIXED(2);
    contact = aos_enemy_update(&fire, &soma, &layer, &kind,
                                NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(fire.x == frozen_x && fire.y == frozen_y && !contact.soma_hit);
    owner.defeated = true;
    fire.state = 0;
    contact = aos_enemy_update(&fire, &soma, &layer, &kind,
                                NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(!fire.removed && fire.x == owner.x && !contact.soma_hit);
    owner.removed = true;
    aos_enemy_update(&fire, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(fire.removed);
    memset(cells, 0, sizeof cells);
}

/* Patch 0678: source sub_0806BD64 subtracts integer positions,
 * sign-extending to 16 bits. Fractional same-pixel offsets must not
 * change the native attack-side choice. */
static void buer_native_attack_side_sign_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    const int targets[] = {90, 100, 110};
    const int signs[] = {-1, 1, 1};
    for (int i = 0; i < 3; ++i) {
        e.state = 2;
        e.step = 1;
        e.substep = 0;
        e.timer = 0;
        e.x = AOS_FIXED(100) + 0xC000;
        e.y = AOS_FIXED(136);
        e.home = 136;
        e.vx = 0;
        e.spin = 0x8000;
        e.angle = 0;
        soma.x = AOS_FIXED(targets[i]);
        soma.y = AOS_FIXED(120);
        aos_enemy_update(&e, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, never_zero);
        assert(e.away == signs[i]);
        assert(e.substep == 1 && e.timer == 0x4F);
        assert(e.angle == (signs[i] > 0 ? 0x400u : (uint32_t)-0x400));
        assert(e.spin == (signs[i] > 0 ? 0x8CCC : 0x7334));
    }
    memset(cells, 0, sizeof cells);
}

/* Patch 0683: sub_080CBC64 adds 24 to the high 16-bit Y word,
 * not to an unbounded signed integer. The projectile allocation keeps
 * the resulting halfword, including its wrap, before making 16.16 Y.
 * Verify source integer X/Y truncation, positive normal Y and the
 * signed-halfword boundary without a ROM or extracted assets. */
/* Patch 0684: original sub_080CBC64 branches on +0x13, not on
 * signed Buer-minus-Soma directly. Side 0 accelerates -0xCCC and
 * creates unmirrored flame 0/1; side 1 accelerates +0xCCC and mirrors
 * both. Equal integer X (despite different fractions) is side 1. */
static void buer_native_side_spin_and_projectile_mirror_tests(void) {
    memset(cells, 0, sizeof cells);
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(90), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    const int targets[3] = {90, 110, 100};
    const int expected_signs[3] = {-1, 1, 1};
    for (int pass = 0; pass < 3; ++pass) {
        e.helper_spawned = true;
        e.state = 2;
        e.step = 1;
        e.substep = 0;
        e.timer = 0;
        e.x = AOS_FIXED(100) + 0xC000;
        e.y = AOS_FIXED(100);
        e.home = 100;
        e.vx = 0;
        e.spin = 0;
        e.angle = 0;
        e.buer_fire_mirrored = pass == 0;
        soma.x = AOS_FIXED(targets[pass]);
        for (int frame = 0; frame < 3; ++frame) {
            AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                                 NULL, NULL, 10, 4,
                                                 0, 0, never_zero);
            if (frame < 2) assert(hit.child_count == 0);
            else {
                assert(e.away == expected_signs[pass]);
                assert(e.spin == expected_signs[pass] * (3 * 0xCCC));
                assert(e.timer == 77);
                assert(hit.child_count == 2);
                assert(e.buer_fire_mirrored == (pass != 0));
                for (int i = 0; i < 2; ++i) {
                    assert(hit.children[i].role == AOS_ROLE_BUER_PROJECTILE);
                    assert(hit.children[i].variant == i);
                    assert(hit.children[i].mirrored == (pass != 0));
                    assert(hit.children[i].combat.attack_off);
                    assert(hit.children[i].combat.hurt_off);
                }
            }
        }
    }
    memset(cells, 0, sizeof cells);
}

static void buer_projectile_signed_y_wrap_tests(void) {
    memset(cells, 0, sizeof cells);
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(20), AOS_FIXED(100), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    e.helper_spawned = true;
    for (int pass = 0; pass < 2; ++pass) {
        const int origin_y = pass ? 32760 : 100;
        e.state = 2;
        e.step = 1;
        e.substep = 1;
        e.timer = 54; /* >50 and divisible by three: emit both flames. */
        e.away = 1;
        e.spin = 0x8000;
        e.vx = 0;
        e.x = AOS_FIXED(100) + 0x2468;
        e.y = AOS_FIXED(origin_y) + 0x5678;
        e.home = (int16_t)origin_y;
        soma.y = AOS_FIXED(origin_y);
        const int camera_y = pass ? 32700 : 0;
        AosHitReport result = aos_enemy_update(&e, &soma, &layer, &kind,
                                                NULL, NULL, 10, 4, 0, camera_y,
                                                never_zero);
        assert(result.child_count == 2);
        for (int i = 0; i < 2; ++i) {
            const AosEnemy *flame = &result.children[i];
            assert(flame->role == AOS_ROLE_BUER_PROJECTILE);
            assert(flame->variant == i);
            assert(flame->x == AOS_FIXED(100));
            assert(flame->y == (pass ? -2146435072 : AOS_FIXED(124)));
            assert(flame->combat.attack_off && flame->combat.hurt_off);
        }
    }
    memset(cells, 0, sizeof cells);
}

static void buer_projectile_lifecycle_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t frames[] = {1, 1, 1};
    const AosAnimDef defs[] = {{3, frames}, {3, frames}};
    const AosAnimSet anims = {defs, 2};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    e.x = AOS_FIXED(100) + 0x1234;
    e.y = AOS_FIXED(136) + 0x5678;
    e.home = 136;
    e.state = 2;
    e.step = 1;
    e.substep = 1;
    e.timer = 78;
    e.spin = 0x8000;
    /* Native side 1: positive acceleration, mirrored fire. */
    e.away = 1;
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0, never_zero);
    /* The first owner update also emits its persistent linked flame.
     * Both fire projectiles occupy the next two child slots. */
    assert(hit.child_count == 3 && hit.effect_count == 0);
    assert(hit.children[0].role == AOS_ROLE_BUER_FIRE);
    for (int i = 1; i < 3; ++i) {
        const AosEnemy *p = &hit.children[i];
        assert(p->role == AOS_ROLE_BUER_PROJECTILE && p->variant == i - 1);
        assert(p->x == AOS_FIXED(100) && p->y == AOS_FIXED(160));
        assert(p->mirrored && p->state == 0 && !p->removed);
        assert(p->combat.attack_off && p->combat.hurt_off && !p->own_boxes);
    }
    AosEnemy shot = hit.children[1];
    shot.vx = 0x70000;
    shot.vy = -0x10000;
    shot.ax = 0x20000;
    shot.ay = 0;
    const int32_t x0 = shot.x, y0 = shot.y;
    AosHitReport update = aos_enemy_update(&shot, &soma, &layer, &kind,
                                           NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(shot.state == 1 && shot.vx == 0x80000);
    assert(shot.x == x0 + 0x80000 && shot.y == y0 - 0x10000);
    assert(!update.killed && !update.soma_hit && !update.targetable);
    for (int i = 0; i < 30 && !shot.removed; ++i)
        aos_enemy_update(&shot, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(shot.removed);

    /* Native sub_080CC178 deletes immediately if it cannot load
     * variant 1, rather than borrowing a Buer static sprite for 24 ticks. */
    const AosAnimSet single = {defs, 1};
    AosEnemy fallback = hit.children[2];
    kind.anims = &single;
    AosHitReport missing = aos_enemy_update(&fallback, &soma, &layer, &kind,
                                             NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(fallback.removed && fallback.state == 0 && fallback.static_frame == -1);
    assert(!missing.enemy_hit && !missing.soma_hit && !missing.killed);

    /* Later attack ticks emit exactly the two new projectiles: the
     * persistent owner-linked flame was emitted on the first tick. */
    /* Opposite attack side mirrors both native projectile variants. */
    kind.anims = &anims;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(136);
    e.step = 1;
    e.substep = 1;
    e.timer = 78;
    /* Native side 0: negative acceleration, unmirrored fire. */
    e.away = -1;
    hit = aos_enemy_update(&e, &soma, &layer, &kind,
                           NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(hit.child_count == 2 && !hit.children[0].mirrored &&
           !hit.children[1].mirrored);
    memset(cells, 0, sizeof cells);
}

/* Patch 0666: the native attack saturates ONLY on strict spin overflow;
 * recovery advances affine angle without idle damping and applies signed
 * floor-slope contact impulses in the same order as sub_080CBC64. */
/* Patch 0680: sub_080CBC64 branches on the native attack-side byte
 * BEFORE considering the sign of the updated spin. A counter-spinning
 * Buer must not overwrite the linked-fire mirror until the new spin has
 * crossed zero in the branch's direction. No ROM is required. */
static void buer_attack_fire_mirror_sign_gate_tests(void) {
    memset(cells, 0, sizeof cells);
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    e.helper_spawned = true; /* no unrelated constructor-linked emission */
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(100);
    e.home = 100;
    e.home_left = 0;
    e.home_right = 200;
    e.vx = 0;
    e.state = 2;
    e.step = 1;
    e.substep = 1;

    /* Positive-acceleration attack with a still-negative incoming spin:
     * the source leaves a previously mirrored linked flame mirrored. */
    e.away = 1;
    e.spin = -0x3000;
    e.buer_fire_mirrored = true;
    e.timer = 40; /* below projectile/sound window */
    e.angle = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == -0x2334 && e.buer_fire_mirrored);
    assert(e.angle == 0x800 && e.timer == 39 && e.substep == 1);

    /* Zero does not set the mirror. The next strictly-positive spin does. */
    e.spin = -0xCCC;
    e.buer_fire_mirrored = false;
    e.timer = 40;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0 && !e.buer_fire_mirrored);
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0xCCC && e.buer_fire_mirrored);

    /* Negative-acceleration attack with still-positive incoming spin:
     * the original must not set a previously unmirrored flame. */
    e.away = -1;
    e.spin = 0x3000;
    e.buer_fire_mirrored = false;
    e.timer = 40;
    e.angle = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0x2334 && !e.buer_fire_mirrored);
    assert(e.angle == (uint32_t)-0x800 && e.timer == 39);

    /* Zero does not clear the mirror. The next strictly-negative spin does. */
    e.spin = 0xCCC;
    e.buer_fire_mirrored = true;
    e.timer = 40;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0 && e.buer_fire_mirrored);
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == -0xCCC && !e.buer_fire_mirrored);
    memset(cells, 0, sizeof cells);
}

static void buer_attack_cap_and_recovery_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(100);
    e.home = 100;
    e.state = 2;
    e.step = 1;
    e.substep = 1;
    e.away = 1;
    e.timer = 78;
    e.spin = 0x20000 - 0xCCC;
    e.angle = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    /* Exactly at the limit: no timer reset, regular fast turn. */
    assert(e.spin == 0x20000 && e.substep == 1 && e.timer == 77);
    assert(e.angle == 0x400);

    /* One extra angular unit STRICTLY crosses the limit and forces
     * underflow and recovery on this tick; angle uses the slow turn. */
    e.spin = 0x20000 - 0xCCC + 1;
    e.angle = 0;
    e.timer = 78;
    e.substep = 1;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0x20000 && e.substep == 2 && e.timer == 0x20);
    assert(e.angle == 0x800);

    /* Same strict comparison in the negative direction. */
    e.away = -1;
    e.spin = -0x20000 + 0xCCC - 1;
    e.angle = 0;
    e.substep = 1;
    e.timer = 78;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == -0x20000 && e.substep == 2 && e.timer == 0x20);
    assert(e.angle == (uint32_t)-0x800);

    /* A no-contact recovery frame does not apply 0x40 friction. */
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(60);
    e.home = 60;
    e.step = 1;
    e.substep = 2;
    e.timer = 12;
    e.spin = 0x6000;
    e.angle = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0x6000 && e.angle == 0x180 && e.timer == 11);

    /* Probe at (100,124), row 15 col 12: floor-slope flag 0x20 grows
     * positive spin and flag 0x40 opposes it. */
    static const struct { uint8_t slope; int32_t expected; } slopes[] = {
        {0x41, 0x6333}, {0x45, 0x5CCD}
    };
    for (size_t i = 0; i < sizeof slopes / sizeof slopes[0]; ++i) {
        cells[15 * W + 12] = slopes[i].slope;
        e.x = AOS_FIXED(100);
        e.y = AOS_FIXED(100);
        e.home = 100;
        e.step = 1;
        e.substep = 2;
        e.timer = 12;
        e.spin = 0x6000;
        e.angle = 0;
        aos_enemy_update(&e, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, never_zero);
        assert(e.spin == slopes[i].expected);
        assert(e.angle == (uint32_t)(slopes[i].expected / 64));
    }
    memset(cells, 0, sizeof cells);
}

/* Patch 0667: native Buer death finale emits eight independent flame
 * effects with ROM-locally imported signed offsets and one RNG bit each.
 * The fixture below is synthetic, not extracted from copyrighted data. */
static void buer_terminal_death_flames_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(136);
    e.defeated = true;
    e.state = 3;
    e.step = 3;
    e.timer = 0xFF;
    kind.buer_death_offsets_valid = true;
    for (int i = 0; i < 8; ++i) {
        kind.buer_death_offsets[i][0] = (int16_t)(i - 4);
        kind.buer_death_offsets[i][1] = (int16_t)(i * 2 - 7);
    }
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(hit.effect_count == 8 && !hit.child_count);
    assert(e.removed && e.hidden && !hit.targetable);
    for (int i = 0; i < 8; ++i) {
        assert(hit.effects[i].kind == AOS_EFFECT_FLAME);
        assert(hit.effects[i].x == AOS_FIXED(96 + i));
        assert(hit.effects[i].y == AOS_FIXED(133 + 2 * i));
        assert(!hit.effects[i].mirrored);
        assert(!hit.effects[i].particle && !hit.effects[i].drift);
    }
    /* GBA arithmetic adds the signed offsets to the full fixed-point
     * entity position, not just its integer pixels. Regression against
     * truncating the fractional coordinates in the final eight flames. */
    e.removed = e.hidden = false;
    e.x = AOS_FIXED(100) + 0x2345;
    e.y = AOS_FIXED(136) + 0x6789;
    hit = aos_enemy_update(&e, &soma, &layer, &kind,
                           NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(hit.effect_count == 8 && e.removed && e.hidden);
    for (int i = 0; i < 8; ++i) {
        assert(hit.effects[i].x == AOS_FIXED(96 + i) + 0x2345);
        assert(hit.effects[i].y == AOS_FIXED(133 + 2 * i) + 0x6789);
    }

    /* No local metadata (old export): preserve previous no-effect cleanup. */
    AosEnemy old = e;
    old.removed = old.hidden = false;
    kind.buer_death_offsets_valid = false;
    hit = aos_enemy_update(&old, &soma, &layer, &kind,
                           NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(old.removed && old.hidden && hit.effect_count == 0);
    memset(cells, 0, sizeof cells);
}

/* Patch 0668: two different native death scatter windows and the linked
 * fire's one-pixel 3-tick wobble. All coordinates here are synthetic. */
static void buer_death_scatter_and_linked_wobble_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    e.defeated = true;
    e.state = 3;
    e.step = 0;
    e.x = AOS_FIXED(100) + 0x1234;
    e.y = AOS_FIXED(80) + 0x5678;
    e.buer_death_linked_wobble = 1;
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(e.step == 1 && e.timer == 95 && e.buer_death_linked_wobble == 1);
    assert(hit.effect_count == 0);
    AosEnemy fire = {.role = AOS_ROLE_BUER_FIRE, .id = AOS_ENEMY_BUER,
                     .parent = &e, .state = 0, .static_frame = 1};
    hit = aos_enemy_update(&fire, &soma, &layer, &kind,
                           NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(fire.x == e.x + AOS_FIXED(1) && fire.y == e.y + AOS_FIXED(1));
    assert(!hit.soma_hit && !fire.removed);
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(e.timer == 94 && !e.buer_death_linked_wobble);
    aos_enemy_update(&fire, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(fire.x == e.x && fire.y == e.y);

    /* At timer 91, flame precedes wobble undo. Fixed RNG=0 samples the
     * first point of the source's 16x64-pixel rectangle. */
    e.timer = 91;
    e.buer_death_linked_wobble = 1;
    const int32_t before_x = e.x, before_y = e.y;
    hit = aos_enemy_update(&e, &soma, &layer, &kind,
                           NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(hit.effect_count == 1 && hit.effects[0].kind == AOS_EFFECT_FLAME);
    assert(hit.effects[0].x == before_x - AOS_FIXED(7));
    assert(hit.effects[0].y == before_y - AOS_FIXED(31));
    assert(!hit.effects[0].mirrored && !e.buer_death_linked_wobble);

    /* On death step two, sample the OLD X/Y, then advance the parent by
     * pre-contact spin. The linked fire moves by that exact parent delta. */
    e.step = 2;
    e.timer = 128;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(60);
    e.spin = 0x8000;
    e.buer_death_linked_wobble = 1;
    const int32_t old_x = e.x, old_y = e.y;
    hit = aos_enemy_update(&e, &soma, &layer, &kind,
                           NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(hit.effect_count == 1 && hit.effects[0].kind == AOS_EFFECT_FLAME);
    assert(hit.effects[0].x == old_x - AOS_FIXED(32));
    assert(hit.effects[0].y == old_y - AOS_FIXED(32));
    assert(e.x == old_x + 0x8000 && e.buer_death_linked_wobble == 1);
    aos_enemy_update(&fire, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(fire.x == e.x + AOS_FIXED(1) && fire.y == e.y + AOS_FIXED(1));
    /* Once the owner is removed, the visual child also retires. */
    e.removed = true;
    aos_enemy_update(&fire, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(fire.removed);
    memset(cells, 0, sizeof cells);
}

/* Patch 0669: synthetic per-entity probe geometry exercises all of the
 * existing ROM-local wall offsets and the source floor position. No table
 * from any ROM is embedded in the test. The legacy count-zero fallback is
 * covered by the earlier Buer 24-pixel probes. */
static void buer_native_probe_geometry_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    kind.probes.count = 2;
    kind.probes.half_width = 16;
    kind.probes.ceiling = -20;
    kind.probes.floor = 20;
    kind.probes.wall_y[0] = -12;
    kind.probes.wall_y[1] = 12;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));

    /* Only the UPPER wall contact at (116,88) intersects this tile;
     * a central y=100-only wall probe would miss it completely. */
    e.state = 1;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(100);
    e.spin = 0x8000;
    e.angle = 0;
    cells[11 * W + 14] = 0x03;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) && e.spin == -0x4000);
    cells[11 * W + 14] = 0;

    /* The previous tick may leave state 1 via the room-exit probe and
     * apply the provisional vertical controller. Restore the entire
     * test fixture so the lower wall sample is really (116, 112). */
    cells[14 * W + 14] = 0x03;
    e.state = 1;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(100);
    e.spin = 0x8000;
    e.angle = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) && e.spin == -0x4000);
    cells[14 * W + 14] = 0;

    /* Ground at y=160 is 20 pixels below a native y=140 entity;
     * old fixed y+24 logic would use a different landing height. */
    e.state = 2;
    e.step = 0;
    e.substep = 0;
    e.timer = 3;
    e.spin = 0;
    e.vy = 0;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(141) + 0x3456;
    e.home = 40;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.y == AOS_FIXED(140));

    /* Mode-six translation preserves the new native floor and front
     * offsets, including the subpixel portion of X. */
    e.x = AOS_FIXED(100) + 0x1000;
    e.y = AOS_FIXED(140);
    e.spin = 0x8000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) + 0x9000);

    /* Reject an out-of-range table: it must fall back to the previous
     * safe floor offset instead of reading past wall_y[8]. */
    kind.probes.count = 9;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(137) + 0x1234;
    e.spin = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.y == AOS_FIXED(136));
    memset(cells, 0, sizeof cells);
}

/* Patch 0670: the shared BG1 collision routines treat 0xFF as a
 * complete 16x16 solid block. These are artificial 0xFF cells; no ROM
 * tiles, probe offsets or extracted data are embedded in the fixture. */
static void buer_special_full_block_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    kind.probes.count = 2;
    kind.probes.half_width = 16;
    kind.probes.ceiling = -20;
    kind.probes.floor = 20;
    kind.probes.wall_y[0] = -12;
    kind.probes.wall_y[1] = 12;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));

    /* Right wall: only the upper native Y probe sees the special block. */
    e.state = 1;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(100);
    e.home = 100;
    e.phase = 0;
    e.spin = 0x8000;
    cells[11 * W + 14] = 0xFF;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) && e.spin == -0x4000);
    cells[11 * W + 14] = 0;

    /* Left wall: the lower native Y probe hits a 16-pixel block. */
    e.state = 1;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(100);
    e.home = 100;
    e.phase = 0;
    e.spin = -0x8000;
    cells[14 * W + 10] = 0xFF;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) && e.spin == 0x4000);
    cells[14 * W + 10] = 0;

    /* A curved collision value (0xC3) must NOT be mistaken for a
     * completely solid 0xFF tile. Its contact mask is still pending. */
    e.state = 1;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(100);
    e.home = 100;
    e.phase = 0;
    e.spin = 0x8000;
    cells[11 * W + 14] = 0xC3;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) + 0x8000 && e.spin == 0x7FC0);
    cells[11 * W + 14] = 0;

    /* Put a real (synthetic) 16x16 0xFF block under the central sample;
     * y=163 means the 20-pixel foot probe is three pixels inside it. */
    for (int col = 12; col <= 13; ++col)
        for (int row = 20; row <= 21; ++row)
            cells[row * W + col] = 0xFF;
    e.state = 2;
    e.step = e.substep = 0;
    e.timer = 3;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(143) + 0x1234;
    e.spin = e.vy = 0;
    e.home = 30;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.y == AOS_FIXED(140) && e.x == AOS_FIXED(100));

    /* Idle mode-six translation now also accepts confirmed 0xFF floor
     * contact and preserves the subpixel fraction of position. */
    e.x = AOS_FIXED(100) + 0x1000;
    e.y = AOS_FIXED(140);
    e.spin = 0x8000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) + 0x9000 && e.y == AOS_FIXED(140));

    /* A missing native metadata row retains the safe +24 foot offset,
     * and the full-block landing still aligns on the 16-pixel boundary. */
    kind.probes.count = 0;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(139) + 0x5678;
    e.spin = e.vy = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.y == AOS_FIXED(136));
    memset(cells, 0, sizeof cells);
}

/* Patch 0671: floor slope signals can be reached at the ROM-local
 * horizontal edges even when the centre stands over an ordinary flat tile.
 * All byte values and offsets below are synthetic collision fixtures. */
static void buer_floor_edge_slope_probe_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    kind.probes.count = 2;
    kind.probes.half_width = 16;
    kind.probes.ceiling = -20;
    kind.probes.floor = 20;
    kind.probes.wall_y[0] = -12;
    kind.probes.wall_y[1] = 12;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));

    /* State one: after +0.5 px motion the leading sample is x=116,
     * y=165 (tile column 14, row 20). The centre tile stays flat. */
    e.state = 1;
    e.step = e.substep = 0;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(145);
    e.home = 145;
    e.phase = 0;
    e.spin = 0x8000;
    e.angle = 0;
    cells[20 * W + 14] = 0x41;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0x8333 && e.angle == (uint32_t)(0x8333 / 64));
    cells[20 * W + 14] = 0x03;

    /* Mirrored edge: -0.5 px yields x=83, a flipped slope in column 10. */
    e.state = 1;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(145);
    e.home = 145;
    e.phase = 0;
    e.spin = -0x8000;
    e.angle = 0;
    cells[20 * W + 10] = 0x45;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == -0x8333 && e.angle == (uint32_t)(-0x8333 / 64));
    cells[20 * W + 10] = 0x03;

    /* Recovery shares the bounded contact helper but not rolling friction. */
    e.state = 2;
    e.step = 1;
    e.substep = 2;
    e.timer = 12;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(145);
    e.home = 145;
    e.vx = 0;
    e.spin = 0x6000;
    e.angle = 0;
    cells[20 * W + 14] = 0x41;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0x6333 && e.angle == (uint32_t)(0x6333 / 64));

    /* The earlier centre-only contact has priority over the edge. */
    cells[20 * W + 12] = 0x45;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(145);
    e.step = 1;
    e.substep = 2;
    e.timer = 12;
    e.spin = 0x6000;
    e.angle = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0x5CCD && e.angle == (uint32_t)(0x5CCD / 64));
    cells[20 * W + 12] = cells[20 * W + 14] = 0x03;

    /* Malformed metadata must use the previous bounded +/-24 offset. */
    kind.probes.count = 9;
    cells[21 * W + 15] = 0x41;  /* centre 100 + 24 = 124 */
    e.state = 1;
    e.step = e.substep = 0;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(149);  /* fallback foot y=173, row 21, local y=5 */
    e.home = 149;
    e.phase = 0;
    e.spin = 0x8000;
    e.angle = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.spin == 0x8333);
    memset(cells, 0, sizeof cells);
}

/* Patch 0672: mode-six flat support is sampled at both native edges,
 * without treating curved masks as flat contact or reading outside BG1. */
static void buer_mode_six_floor_edge_support_tests(void) {
    memset(cells, 0, sizeof cells);
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    kind.probes.count = 2;
    kind.probes.half_width = 16;
    kind.probes.ceiling = -20;
    kind.probes.floor = 20;
    kind.probes.wall_y[0] = -12;
    kind.probes.wall_y[1] = 12;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    e.state = 2;
    e.step = e.substep = 0;
    e.timer = 3;
    e.vy = 0;
    e.home = 30;

    /* Centre is empty: the leading imported edge supports both X motion
     * and the y=141 -> 140 correction on an ordinary 8px floor. */
    cells[20 * W + 14] = 0x03;
    e.x = AOS_FIXED(100) + 0x1000;
    e.y = AOS_FIXED(141) + 0x2345;
    e.spin = 0x8000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) + 0x9000 && e.y == AOS_FIXED(140));
    cells[20 * W + 14] = 0;

    /* The left edge supports the same grounded translation when rolling
     * left; the subpixel component must survive the negative X step. */
    cells[20 * W + 10] = 0x03;
    e.x = AOS_FIXED(100) + 0xC000;
    e.y = AOS_FIXED(141) + 0x4567;
    e.spin = -0x8000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) + 0x4000 && e.y == AOS_FIXED(140));

    /* Rear support is also valid when the leading edge is in air. */
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(141);
    e.spin = 0x8000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) + 0x8000 && e.y == AOS_FIXED(140));
    cells[20 * W + 10] = 0;

    /* Imported edges also recognize full 16px 0xFF blocks, retaining the
     * 16px top rather than snapping inside the block's second 8px row. */
    for (int row = 20; row <= 21; ++row)
        for (int col = 14; col <= 15; ++col)
            cells[row * W + col] = 0xFF;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(143) + 0x2468;
    e.spin = 0x8000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) + 0x8000 && e.y == AOS_FIXED(140));
    for (int row = 20; row <= 21; ++row)
        for (int col = 14; col <= 15; ++col)
            cells[row * W + col] = 0;

    /* A curved slope must not create flat support in mode six. */
    cells[20 * W + 14] = 0x41;
    e.x = AOS_FIXED(100) + 0x1000;
    e.y = AOS_FIXED(145) + 0x2468;
    e.spin = 0x8000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) + 0x1000);
    assert(e.y == AOS_FIXED(145) + 0x2468);
    cells[20 * W + 14] = 0;

    /* Unsupported air never permits movement or correction. */
    e.x = AOS_FIXED(100) + 0x1000;
    e.y = AOS_FIXED(141) + 0x2468;
    e.spin = 0x8000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) + 0x1000);
    assert(e.y == AOS_FIXED(141) + 0x2468);

    /* Malformed native metadata retains the 24px half-width and +24px
     * floor sample. The chosen floor tile is solely at x=124, y=161. */
    kind.probes.count = 9;
    cells[20 * W + 15] = 0x03;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(137) + 0x5678;
    e.spin = 0x8000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) + 0x8000 && e.y == AOS_FIXED(136));
    memset(cells, 0, sizeof cells);
}

/* Patch 0673: exercise the actual shared native collision walker,
 * instead of Buer's historical bounded mode-2/mode-6 approximation.
 * These geometry values are synthetic, not copied from a ROM. */
static void buer_native_integrated_motion_tests(void) {
    memset(cells, 0, sizeof cells);
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    kind.buer_native_motion = true;
    kind.probes.count = 2;
    kind.probes.half_width = 16;
    kind.probes.ceiling = -20;
    kind.probes.floor = 20;
    kind.probes.wall_y[0] = -12;
    kind.probes.wall_y[1] = 12;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            0, 0, &soma, &layer, &kind, &stats));
    assert(e.ay == 0x4000);

    /* A state-one tick uses the native +0x54 gravity (0x4000) and
     * pre-friction 0x8000 horizontal spin. The legacy path has no Y fall. */
    e.state = 1;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(60);
    e.vx = e.vy = 0;
    e.spin = 0x8000;
    e.angle = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) + 0x8000);
    assert(e.y == AOS_FIXED(60) + 0x4000);
    assert(e.vy == 0x4000 && e.spin == 0x7FC0 && e.angle == 0x1FF);

    /* Mode two: an imported upper wall probe causes the original X to
     * roll back and the signed spin to reverse/halve. */
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(100);
    e.vy = 0;
    e.spin = 0x8000;
    e.angle = 0;
    cells[11 * W + 14] = 0x03;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) && e.spin == -0x3FE0);
    cells[11 * W + 14] = 0;

    /* Mode six requires ground flag 8. With air on all samples, motion
     * and the falling offset are restored; spin still reverses in idle. */
    e.state = 2;
    e.step = e.substep = 0;
    e.timer = 1;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(60);
    e.vy = 0;
    e.spin = 0x8000;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) && e.y == AOS_FIXED(60));
    assert(e.spin == -0x3FE0);

    /* Active attack/recovery also calls mode-six, not the previous
     * provisional +0x48 translation and sine positioning. */
    e.state = 2;
    e.step = 1;
    e.substep = 2;
    e.timer = 12;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(60);
    e.vy = 0;
    e.vx = 0x18000;
    e.spin = 0x6000;
    e.angle = 0;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) && e.y == AOS_FIXED(60));
    assert(e.angle == 0x180 && e.timer == 11);

    /* An old export still receives the unconditional native
     * constructor gravity word. The metadata gate controls which motion
     * solver executes, not the initial acceleration field. */
    kind.buer_native_motion = false;
    AosEnemy legacy;
    assert(aos_enemy_create(&legacy, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    assert(legacy.ay == 0x4000);
    legacy.state = 2;
    legacy.step = 1;
    legacy.substep = 2;
    legacy.timer = 12;
    /* Empty synthetic collision rooms can push the constructor's
     * snap_to_floor result well below the update activity window.
     * Place the legacy fixture explicitly on screen, as for the native
     * motion cases above, before testing the fallback X translation. */
    legacy.x = AOS_FIXED(100);
    legacy.y = AOS_FIXED(100);
    legacy.home = 100;
    legacy.phase = 0;
    legacy.vx = 0x18000;
    legacy.spin = 0x8000;
    aos_enemy_update(&legacy, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(legacy.x == AOS_FIXED(101) + 0x8000);
    memset(cells, 0, sizeof cells);
}

/* Patch 0675: Buer's body does not turn toward Soma each frame. The
 * original mode-2/mode-6 walker has the raw-speed flag and does not
 * call sub_0806BC40. This must hold with legacy and imported probes;
 * only the separate fire facing may switch during the attack state. */
/* Patch 0682: EnemyBuerCreate stores +0x18 rolling spin, NOT +0x48
 * horizontal velocity. sub_0806CAF8 writes +0x48 only on the movement
 * tick. Preserve the earlier bounded start speed when the ROM-local Buer
 * probe table is missing or malformed, even if its flag was set. */
static void buer_constructor_native_velocity_tests(void) {
    memset(cells, 0, sizeof cells);
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(210), AOS_FIXED(120), NULL);

    for (int metadata = 0; metadata < 3; ++metadata) {
        /* 0 = legacy; 1 = native flag but invalid zero-count probes;
         * 2 = full native flag plus validated geometry. */
        kind.buer_native_motion = metadata != 0;
        kind.probes.count = metadata == 2 ? 2 : 0;
        kind.probes.half_width = 16;
        kind.probes.ceiling = -20;
        kind.probes.floor = 20;
        kind.probes.wall_y[0] = -12;
        kind.probes.wall_y[1] = 12;
        for (int side = 0; side < 2; ++side) {
            soma.x = AOS_FIXED(side ? 20 : 210);
            AosEnemy e;
            assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                                    0, 0, &soma, &layer, &kind, &stats));
            assert(e.state == 1 && e.spin == 0x8000 && e.ay == 0x4000);
            assert(e.mirrored == (side == 0) && e.buer_fire_mirrored);
            if (metadata == 2) assert(e.vx == 0);
            else assert(e.vx == (side ? -0x18000 : 0x18000));

            /* Both paths take their first movement speed from spin, but
             * only the validated native path starts with a zero +0x48. */
            e.helper_spawned = true;
            e.x = AOS_FIXED(100);
            e.y = AOS_FIXED(60);
            e.home = 60;
            const int32_t x_before = e.x;
            aos_enemy_update(&e, &soma, &layer, &kind,
                             NULL, NULL, 10, 4, 0, 0, never_zero);
            assert(e.vx == 0x8000 && e.x == x_before + 0x8000);
        }
    }
    memset(cells, 0, sizeof cells);
}

static void buer_native_body_facing_tests(void) {
    memset(cells, 0, sizeof cells);
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(210), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            0, 0, &soma, &layer, &kind, &stats));
    e.helper_spawned = true;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(60);
    e.home = 60;
    e.state = 1;
    e.spin = 0x8000;
    e.mirrored = true;
    assert(e.mirrored);
    /* Legacy fallback used to re-face this body on every update. */
    soma.x = AOS_FIXED(20);
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.mirrored);
    soma.x = AOS_FIXED(210);
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.mirrored);

    /* Native mode six, when no ground is available, still preserves
     * the body's mirror while the independent linked fire can flip. */
    kind.buer_native_motion = true;
    kind.probes.count = 2;
    kind.probes.half_width = 16;
    kind.probes.ceiling = -20;
    kind.probes.floor = 20;
    kind.probes.wall_y[0] = -12;
    kind.probes.wall_y[1] = 12;
    e.state = 2;
    e.step = e.substep = 0;
    e.timer = 1;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(60);
    e.spin = 0x8000;
    e.buer_fire_mirrored = true;
    soma.x = AOS_FIXED(20);
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.mirrored && !e.buer_fire_mirrored);
    e.step = 1;
    e.substep = 2;
    e.timer = 12;
    e.spin = 0x6000;
    soma.x = AOS_FIXED(210);
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.mirrored && !e.buer_fire_mirrored && e.timer == 11);
    /* The follower uses its own mirror, not the parent body's bit. */
    AosEnemy fire = {.id = AOS_ENEMY_BUER, .role = AOS_ROLE_BUER_FIRE,
                     .parent = &e, .static_frame = 1};
    AosHitReport follow = aos_enemy_update(&fire, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(!fire.mirrored && e.mirrored && !follow.soma_hit);
    memset(cells, 0, sizeof cells);
}

/* Patch 0676: source sub_080CBEA4 death step 1 calls the same
 * sub_0806CAF8 collision walker as ordinary motion, BUT with speed zero
 * and mode two. Rotation must never become horizontal drift. Gravity and
 * floor contact must remain active through the entire 96-frame stage. */
static void buer_native_death_stage_one_walk_tests(void) {
    memset(cells, 0, sizeof cells);
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.buer_native_motion = true;
    kind.probes.count = 2;
    kind.probes.half_width = 16;
    kind.probes.ceiling = -20;
    kind.probes.floor = 20;
    kind.probes.wall_y[0] = -12;
    kind.probes.wall_y[1] = 12;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(120), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    assert(e.ay == 0x4000);
    e.defeated = true;
    e.state = 3;
    e.step = 0;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(60);
    e.vx = 0x30000; /* must be overwritten by zero-speed native mode 2 */
    e.vy = 0;
    e.spin = -0x8000;
    e.angle = 0;
    AosHitReport report = aos_enemy_update(&e, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.step == 1 && e.timer == 95 && e.spin == -0x1FC80);
    assert(e.angle == (uint32_t)(-0x1FC80 / 64));
    assert(e.vx == 0 && e.vy == 0x4000);
    assert(e.x == AOS_FIXED(100) && e.y == AOS_FIXED(60) + 0x4000);
    assert(!report.soma_hit && !report.enemy_hit);

    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.timer == 94 && e.vx == 0 && e.vy == 0x8000);
    assert(e.x == AOS_FIXED(100) && e.y == AOS_FIXED(60) + 0xC000);

    /* A real collision layer still stops the native fall with mode 2.
     * Unlike mode 6, the floor support is not required to keep X/Y. */
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(140);
    e.vx = 0x30000;
    e.vy = 0;
    e.step = 1;
    e.timer = 40;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) && e.vx == 0);
    assert(e.vy == 0 && e.ground == 0x03);
    assert(e.y < AOS_FIXED(141) && e.y >= AOS_FIXED(138));

    /* An older export without locally imported probes retains the
     * previous compatibility behavior, without synthesized gravity. */
    memset(cells, 0, sizeof cells);
    kind.buer_native_motion = false;
    e.x = AOS_FIXED(100);
    e.y = AOS_FIXED(60);
    e.vx = 0x30000;
    e.vy = 0;
    e.ay = 0;
    e.timer = 40;
    e.step = 1;
    const uint32_t saved_angle = e.angle;
    aos_enemy_update(&e, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.x == AOS_FIXED(100) && e.y == AOS_FIXED(60));
    assert(e.vx == 0x30000 && e.vy == 0 && e.angle != saved_angle);
    memset(cells, 0, sizeof cells);
}

static void buer_inert_orbit_tests(void) {
    memset(cells, 0, sizeof cells);
    static const uint8_t ticks[] = {2};
    const AosAnimDef defs[] = {{1, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 7};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(210), AOS_FIXED(140), NULL);
    AosEnemy buer;

    aos_enemy_set_flag_8e_40(true);
    bool created = aos_enemy_create(&buer, AOS_ENEMY_BUER, 100, 100,
                                    0, 0, &soma, &layer, &kind, &stats);
    aos_enemy_set_flag_8e_40(false);
    assert(created && buer.state == 4 && buer.step == 0);
    /* Source EnemyBuerCreate sets up +0x18 and child facing before the
     * global 0x8E bit 0x40 can select the inert state. */
    assert(buer.spin == 0x8000 && buer.buer_fire_mirrored &&
           buer.ay == 0x4000);
    assert(!buer.helper_spawned);
    int32_t first_y = buer.y, first_x = buer.x;
    /* Native +0x14 is a full 32-bit affine angle. Cross its
     * 0xffffffff wrap to catch accidental 16-bit phase updates. */
    const uint16_t first_phase = buer.phase;
    buer.angle = 0xFFFFFF80u;
    const uint32_t first_angle = buer.angle;

    AosHitReport report = aos_enemy_update(&buer, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(buer.y == first_y - AOS_FIXED(32));
    assert(buer.x == first_x && buer.step == 1 && buer.static_frame == 0);
    assert(buer.angle == first_angle + 0x100u);
    assert(buer.phase == first_phase);
    assert(report.child_count == 1 && buer.helper_spawned);
    assert(!report.soma_hit && !report.enemy_hit);
    assert(report.children[0].role == AOS_ROLE_BUER_FIRE);
    assert(report.children[0].mirrored && report.children[0].static_frame == 1);
    assert(!report.children[0].own_boxes && report.children[0].combat.attack_off &&
           report.children[0].combat.hurt_off);
    /* The original linked visual follows the newly raised parent on its
     * own update, rather than being deleted or applying contact damage. */
    AosEnemy fire = report.children[0];
    fire.parent = &buer;
    AosHitReport visual = aos_enemy_update(&fire, &soma, &layer, &kind,
                                           NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(fire.x == buer.x && fire.y == buer.y && !fire.removed);
    assert(!visual.soma_hit && !visual.enemy_hit);

    int32_t raised_y = buer.y;
    for (int i = 0; i < 5; ++i) {
        report = aos_enemy_update(&buer, &soma, &layer, &kind,
                                  NULL, NULL, 10, 4, 0, 0, always_zero);
        assert(buer.y == raised_y && buer.x == first_x);
        assert(buer.state == 4 && buer.step == 1);
        assert(!report.child_count && !report.soma_hit && !report.enemy_hit);
    }
    assert(buer.angle == first_angle + 6u * 0x100u);
    assert(buer.phase == first_phase);

    /* Native and legacy metadata now agree on +0x54 gravity.
     * The dormant state never runs the walker, emits one linked visual
     * and raises the body exactly once. */
    kind.buer_native_motion = true;
    AosEnemy native;
    aos_enemy_set_flag_8e_40(true);
    assert(aos_enemy_create(&native, AOS_ENEMY_BUER, 100, 100,
                            1, 0, &soma, &layer, &kind, &stats));
    aos_enemy_set_flag_8e_40(false);
    assert(native.state == 4 && native.spin == 0x8000 && native.ay == 0x4000);
    assert(native.buer_fire_mirrored && native.step == 0);
    const int32_t native_y = native.y;
    report = aos_enemy_update(&native, &soma, &layer, &kind,
                              NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(native.y == native_y - AOS_FIXED(32) && native.vy == 0);
    assert(native.spin == 0x8000 && native.angle == 0x100u);
    assert(native.helper_spawned && report.child_count == 1 &&
           report.children[0].role == AOS_ROLE_BUER_FIRE);
    report = aos_enemy_update(&native, &soma, &layer, &kind,
                              NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(!report.child_count && native.y == native_y - AOS_FIXED(32));
    memset(cells, 0, sizeof cells);
}
/* Skeleton: the native attack's third animation frame launches a single
 * bone, using source-derived OAM IDs, sound, launch trajectory and boxes.
 * The regular kill path is deliberately still used for XP and soul drops. */
static void skeleton_bone_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    static const uint8_t ticks[] = {1, 1, 1, 1};
    const AosAnimDef defs[5] = {
        {4, ticks}, {4, ticks}, {4, ticks}, {4, ticks}, {4, ticks}
    };
    const AosAnimSet anims = {defs, 5};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    for (int a = 0; a < 5; ++a)
        for (int f = 0; f < 4; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -16, 16, 16};
        }
    const AosEnemyStats stats = {20, 10, 1, 0, 0, 15};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(110), AOS_FIXED(150), NULL);
    AosEnemy e;
    assert(aos_enemy_create(&e, AOS_ENEMY_SKELETON, 100, 100, 0, 0,
                            &soma, &layer, &kind, &stats));
    assert(e.attack_wait == 40);
    e.attack_wait = 0;
    AosHitReport report = aos_enemy_update(&e, &soma, &layer, &kind,
                                            NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(e.state == 4 && report.child_count == 0);
    aos_audio_clear();
    AosEnemy bone = {0};
    int launches = 0;
    for (int frame = 0; frame < 20; ++frame) {
        report = aos_enemy_update(&e, &soma, &layer, &kind,
                                  NULL, NULL, 10, 4, 0, 0, never_zero);
        launches += report.child_count;
        if (report.child_count) bone = report.children[0];
    }
    assert(launches == 1);
    assert(bone.id == AOS_ENEMY_SKELETON && bone.role == AOS_ROLE_SKELETON_BONE);
    assert(bone.static_frame == 0x12 && bone.y < e.y);
    assert(bone.vx == 0x8AAA && bone.vy == -0x30000 && bone.ay == 0x6000);
    assert(bone.own_hurt.w == 16 && bone.own_attack.w == 2);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x88) == 1);
    assert(e.state == 1 && e.attack_wait > 0);
    int32_t x0 = bone.x, y0 = bone.y;
    aos_enemy_update(&bone, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(bone.x > x0 && bone.y < y0 && bone.vy == -0x2A000);
    /* A struck projectile disappears but does not become a rewardable mob. */
    bone.defeated = true;
    aos_enemy_update(&bone, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(bone.removed);

    AosEnemy mirrored = e;
    mirrored.state = 4;
    mirrored.step = 0;
    mirrored.mirrored = false;
    mirrored.variant = 1;
    mirrored.anim = (AosAnimState){0};
    aos_audio_clear();
    for (int frame = 0; frame < 12; ++frame) {
        report = aos_enemy_update(&mirrored, &soma, &layer, &kind,
                                  NULL, NULL, 10, 4, 0, 0, never_zero);
        if (report.child_count) {
            assert(report.children[0].static_frame == 0x1B);
            assert(report.children[0].vx == -0x8AAA);
            break;
        }
    }
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x89) == 1);
    aos_audio_clear();
    AosSoulAttack soul = {{e.x >> 16, (e.y >> 16) - 16, e.x >> 16,
                           (e.y >> 16) - 16}, 255, 0, false, NULL};
    aos_enemy_set_soul_attacks(&soul, 1);
    report = aos_enemy_update(&e, &soma, &layer, &kind,
                              NULL, NULL, 10, 4, 0, 0, never_zero);
    aos_enemy_set_soul_attacks(NULL, 0);
    assert(report.killed && e.defeated && e.timer == 0x28);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x6B) == 1);
    memset(cells, 0, sizeof cells);
}

/* The GBA Giant Ghost's lethal-hit callback uses song 0x6B,
 * animation 5 and 0x5A death frames. This is ROM-free: no private assets. */
static void giant_ghost_death_cue_tests(void) {
    memset(cells, 0, sizeof cells);
    static const uint8_t ticks[] = {2, 2, 2};
    const AosAnimDef defs[6] = {
        {3, ticks}, {3, ticks}, {3, ticks},
        {3, ticks}, {3, ticks}, {3, ticks}
    };
    const AosAnimSet anims = {defs, 6};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    for (int a = 0; a < 6; ++a)
        for (int f = 0; f < 3; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-8, -8, 16, 16};
        }
    const AosEnemyStats stats = {30, 12, 2, 0, 0, 4};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(120), NULL);
    AosEnemy ghost;
    assert(aos_enemy_create(&ghost, AOS_ENEMY_GIANT_GHOST, 100, 100,
                            0, 0, &soma, &layer, &kind, &stats));
    AosSoulAttack hitbox = {{0, 0, 255, 200}, 255, 0, false, NULL};
    aos_enemy_set_soul_attacks(&hitbox, 1);
    aos_audio_clear();
    AosHitReport hit = aos_enemy_update(&ghost, &soma, &layer, &kind,
                                        NULL, NULL, 10, 4, 0, 0, always_zero);
    aos_enemy_set_soul_attacks(NULL, 0);
    assert(hit.killed && ghost.defeated);
    assert(ghost.state == 2 && ghost.timer == 0x5A && ghost.anim.id == 5);
    assert(ghost.combat.attack_off && ghost.combat.hurt_off);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x6B) == 1);
    aos_audio_clear();
    for (int frame = 0; frame < 90; ++frame) {
        assert(!ghost.removed);
        aos_enemy_update(&ghost, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, always_zero);
    }
    assert(ghost.removed && !aos_audio_requested(AOS_AUDIO_START_SONG, 0x6B));
    memset(cells, 0, sizeof cells);
}

/* Peeping Eye sub_080882D2/sub_08088478: exactly three target draws,
 * symmetric 0x800 axis acceleration and no return-to-home teleport. */
static void peeping_eye_native_steering_tests(void) {
    memset(cells, 0, sizeof cells);
    static const uint8_t ticks[] = {2, 2};
    const AosAnimDef defs[1] = {{2, ticks}};
    const AosAnimSet anims = {defs, 1};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 4};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(220), AOS_FIXED(120), NULL);
    AosEnemy eye;
    assert(aos_enemy_create(&eye, AOS_ENEMY_PEEPING_EYE, 100, 100,
                            0, 0, &soma, &layer, &kind, &stats));
    eye.helper_spawned = true;
    eye.x = AOS_FIXED(100);
    eye.y = AOS_FIXED(100);
    eye.home = 140;
    eye.step = 2;
    eye.timer = 73;
    const uint32_t target_draws[] = {0, 1, 0, 0xFF};
    script = target_draws;
    script_at = 0;
    aos_enemy_update(&eye, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, scripted);
    assert(script_at == 3 && eye.step == 3 && eye.timer == 0);
    assert(eye.target_x == AOS_FIXED(164) && eye.target_y == AOS_FIXED(68));
    assert(eye.y == AOS_FIXED(100));

    eye.target_x = eye.x + AOS_FIXED(20);
    eye.target_y = eye.y - AOS_FIXED(20);
    eye.vx = eye.vy = 0;
    aos_enemy_update(&eye, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.ax == 0x800 && eye.ay == -0x800);
    assert(eye.vx == 0x800 && eye.vy == -0x800);

    eye.target_x = eye.x - AOS_FIXED(20);
    eye.target_y = eye.y + AOS_FIXED(20);
    eye.vx = eye.vy = 0;
    aos_enemy_update(&eye, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.ax == -0x800 && eye.ay == 0x800);
    assert(eye.vx == -0x800 && eye.vy == 0x800);

    eye.step = 3;
    eye.timer = 0;
    eye.vx = eye.vy = 0;
    eye.target_x = eye.x + AOS_FIXED(3);
    eye.target_y = eye.y - AOS_FIXED(3);
    int32_t y_before = eye.y;
    aos_enemy_update(&eye, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.step == 2 && eye.y == y_before);

    eye.step = 0;
    eye.phase = 0x2000;
    y_before = eye.y;
    aos_enemy_update(&eye, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.step == 0 && eye.y == y_before);

    eye.step = 3;
    eye.timer = 120;
    eye.target_x = eye.x + AOS_FIXED(80);
    eye.target_y = eye.y - AOS_FIXED(80);
    eye.vx = eye.vy = 0;
    aos_enemy_update(&eye, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.step == 2); /* 120-frame native timeout */
    memset(cells, 0, sizeof cells);
}

/* White Dragon state 3 (sub_08076ED8) holds its position for
 * thirty updates and resets on update 31, retaining shared contact. */
static void white_dragon_recovery_tests(void) {
    memset(cells, 0, sizeof cells);
    static const uint8_t ticks[] = {2, 2};
    const AosAnimDef defs[2] = {{2, ticks}, {2, ticks}};
    const AosAnimSet anims = {defs, 2};
    AosEnemyKind kind = {.anims = &anims};
    kind.margins[0][0] = kind.margins[0][1] = 32;
    for (int animation = 0; animation < 2; ++animation)
        for (int frame = 0; frame < 2; ++frame) {
            kind.modes[animation][frame] = 1;
            kind.hurt[animation][frame] = kind.attack[animation][frame] =
                (AosBox){-8, -8, 16, 16};
        }
    const AosEnemyStats stats = {30, 10, 2, 0, 0, 4};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(100), NULL);
    AosEnemy dragon;
    assert(aos_enemy_create(&dragon, AOS_ENEMY_WHITE_DRAGON,
                            100, 100, 0, 0, &soma, &layer, &kind, &stats));
    dragon.spawned_count = 9; /* Body linkage is tested separately. */
    dragon.state = 3;
    dragon.step = 4;
    dragon.timer = 0;
    int32_t x = dragon.x, y = dragon.y;
    for (int frame = 0; frame < 30; ++frame) {
        aos_enemy_update(&dragon, &soma, &layer, &kind,
                         NULL, NULL, 10, 4, 0, 0, never_zero);
        assert(dragon.state == 3 && dragon.x == x && dragon.y == y);
    }
    assert(dragon.timer == 30 && dragon.step == 4);
    aos_enemy_update(&dragon, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(dragon.state == 0 && dragon.step == 0 && dragon.timer == 0);
    assert(dragon.static_frame == 0 && dragon.x == x && dragon.y == y);
    aos_enemy_update(&dragon, &soma, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(dragon.x != x); /* Normal controller resumed after recovery. */
    memset(cells, 0, sizeof cells);
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
    const uint8_t ids[] = {AOS_ENEMY_SKELETON, AOS_ENEMY_PEEPING_EYE,
                           AOS_ENEMY_KILLER_FISH, AOS_ENEMY_BONE_PILLAR,
                           AOS_ENEMY_BUER, AOS_ENEMY_WHITE_DRAGON, AOS_ENEMY_GHOST,
                           AOS_ENEMY_ROCK_ARMOR, AOS_ENEMY_GIANT_GHOST,
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

    AosEnemy eye;
    assert(aos_enemy_create(&eye, AOS_ENEMY_PEEPING_EYE, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    soma.x = AOS_FIXED(170);
    AosHitReport eye_parts = aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0,
                                              0, never_zero);
    assert(eye_parts.child_count == 8 && eye.helper_spawned && eye.angle != 0);
    assert(eye.step == 1 && eye.timer == 1);
    assert(eye_parts.children[0].role == AOS_ROLE_PEEPING_SEGMENT &&
           eye_parts.children[0].static_frame == 0xF &&
           eye_parts.children[7].static_frame == 0xE);
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.step == 1 && eye.substep == 1 && eye.timer == 0);
    for (int frame = 0; frame < 18; ++frame)
        eye_parts = aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                     never_zero);
    assert(eye.step == 1 && eye.substep == 9 && eye.timer == 0);
    for (int frame = 0; frame < 19; ++frame)
        eye_parts = aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                     never_zero);
    assert(eye.step == 1 && eye.substep == 9 && eye.timer == 19);
    eye_parts = aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                 never_zero);
    assert(eye.step == 2 && eye.timer == 0);
    assert(eye_parts.child_count == 0);
    int32_t eye_x = eye.x, eye_y = eye.y;
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.step == 3 && eye.target_x > eye.x && eye.target_y < eye.y);
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.x > eye_x && eye.y < eye_y);
    /* At exactly four pixels, the native <0x40000 threshold does not
     * complete the steering state (the old <= test returned too early). */
    eye.step = 3;
    eye.vx = eye.vy = 0;
    eye.timer = 0;
    eye.target_x = eye.x + AOS_FIXED(4);
    eye.target_y = eye.y;
    aos_enemy_update(&eye, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(eye.step == 3);

    AosEnemy dragon;
    assert(aos_enemy_create(&dragon, AOS_ENEMY_WHITE_DRAGON, 100, 100, 0, 0, &soma, &layer,
                            &kind, &stats));
    AosHitReport dragon_parts = aos_enemy_update(&dragon, &soma, &layer, &kind, NULL, NULL, 10,
                                                 4, 0, 0, never_zero);
    assert(dragon_parts.child_count == 8 && dragon.spawned_count == 8);
    assert(dragon.x != AOS_FIXED(100) && dragon.phase == 0x100);
    for (unsigned i = 0; i < dragon_parts.child_count; ++i)
        assert(dragon_parts.children[i].role == AOS_ROLE_WHITE_DRAGON_PART &&
               dragon_parts.children[i].anim.id == 1);
    assert(dragon_parts.children[5].scaled && dragon_parts.children[5].scale == 0xFFFF);
    dragon_parts = aos_enemy_update(&dragon, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                    never_zero);
    assert(dragon_parts.child_count == 1 && dragon.spawned_count == 9);
    AosEnemy part = dragon_parts.children[0];
    part.parent = &dragon;
    int32_t part_x = part.x;
    aos_enemy_update(&part, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(part.x != part_x || part.y != dragon.y);
    assert(part.angle != 0);

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

    AosEnemy giant_death;
    assert(aos_enemy_create(&giant_death, AOS_ENEMY_GIANT_GHOST,
                            100, 100, 0, 0, &near_left, &layer, &kind, &stats));
    giant_death.defeated = true;
    giant_death.state = 2;
    giant_death.timer = 2;
    aos_enemy_update(&giant_death, &near_left, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(!giant_death.removed);
    aos_enemy_update(&giant_death, &near_left, &layer, &kind,
                     NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(giant_death.removed);

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

    AosEnemy pillar;
    assert(aos_enemy_create(&pillar, AOS_ENEMY_BONE_PILLAR, 100, 100, 0, 0, &soma, &layer,
                            &kind, &stats));
    assert(pillar.param0 == 1);
    assert(aos_enemy_create(&pillar, AOS_ENEMY_BONE_PILLAR, 100, 100, 12, 0, &soma, &layer,
                            &kind, &stats));
    assert(pillar.param0 == 8);
    int32_t pillar_y = pillar.y;
    AosHitReport pillar_parts = aos_enemy_update(&pillar, &soma, &layer, &kind, NULL, NULL, 10,
                                                 4, 0, 0, never_zero);
    assert(pillar_parts.child_count == 8 && pillar.helper_spawned);
    assert(pillar_parts.children[0].role == AOS_ROLE_BONE_PILLAR_SEGMENT);
    assert(pillar_parts.children[0].y > pillar_y &&
           pillar_parts.children[7].y > pillar.y);
    AosEnemy pillar_part = pillar_parts.children[7];
    pillar_part.parent = &pillar;
    aos_enemy_update(&pillar_part, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(pillar_part.y == pillar.y + AOS_FIXED(22));

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
    AosSoulAttack blast = {{0, 0, 300, 300}, 999, 0, false, NULL};
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
    AosSoulAttack harpy_blast = {{0, 0, 300, 300}, 999, 0, false, NULL};
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
    int breaths = 0;
    AosEnemy cloud = {0};
    for (int frame = 0; frame < 180 && breaths < 21; ++frame) {
        AosHitReport hit = aos_enemy_update(&gorgon, &soma, &layer, &kind, NULL, NULL, 10, 4, 0,
                                            0, always_zero);
        for (int i = 0; i < hit.child_count; ++i) {
            assert(hit.children[i].role == AOS_ROLE_GORGON_BREATH);
            assert(hit.children[i].static_frame == 25);
            cloud = hit.children[i];
            ++breaths;
        }
    }
    assert(breaths == 21 && gorgon.state == 1);
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    hit = aos_enemy_update(&z, &soma, &layer, &kind, &weapon, &blade, 10, 4, 100, 48, always_zero);
    assert(hit.enemy_hit && hit.killed && z.state == 1 && z.anim.id == 3 && z.timer == 0x28);
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    assert(aos_audio_count() == 5);
    assert(aos_audio_get(0).action == AOS_AUDIO_START_SONG && aos_audio_get(0).value == 0x6F);
    for (int i = 1; i < 5; ++i) {
        assert(aos_audio_get(i).action == AOS_AUDIO_START_SONG);
        assert(aos_audio_get(i).value == 0x6E);
    }

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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    AosSoulAttack curly_blast = {{0, 0, 300, 300}, 999, 0, false, NULL};
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
                          100, 0, false, NULL};
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    while (w.step != 3 && frames < 200) {
        aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(w.step == 3 && frames == 51 && w.anim.id == 2 && w.walk_timer == 0x20000);
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    while (!w.removed && dying < 200) {
        hit = aos_enemy_update(&w, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        for (int i = 0; i < hit.effect_count; ++i)
            if (hit.effects[i].kind == AOS_EFFECT_EMBER) ++fires;
        ++dying;
    }
    /* Deleted on the update after its 60-tick death animation ends (or off
     * screen first). */
    assert(w.removed && fires >= 10);
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
    /* 45 + 31 + 21 frames of animations 2, 3 and 4, then walking again. */
    frames = 0;
    while (k.state == 1 && frames < 200) {
        aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(frames == 1 + 45 + 31 + 21 && k.step == 1 && k.anim.id == 1);
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
    while (k.state == 1 && frames < 200) {
        aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        if (k.step == 3) assert(k.anim.id == 4);
        ++frames;
    }
    assert(frames == 1 + 39 + 31 + 21 && k.step == 1 && k.anim.id == 1);
    /* Otherwise it slashes: 13 frames, then animation 6 four times 13. */
    k.state = 1;
    k.step = 0;
    frames = 0;
    bool slashed = false;
    while (k.state == 1 && frames < 200) {
        aos_enemy_update(&k, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_one_hundred);
        slashed |= k.anim.id == 6;
        ++frames;
    }
    assert(slashed && frames == 1 + 39 + 13 + 4 * 13 && k.step == 1);
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
    soma.x = k.x - AOS_FIXED(30);
    soma.y = AOS_FIXED(159);
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{20, -40, 20, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    AosHitReport hit = aos_enemy_update(&k, &soma, &layer, &kind, &weapon, &blade, 50, 4, 0, 0,
                                        always_one_hundred);
    assert(k.step == 5 && k.guarding && k.anim.id == 7 && k.vx == 0);
    assert(hit.enemy_hit && hit.enemy_damage == 0 && k.hp == 180);
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
    hit = aos_enemy_update(&k, &soma, &layer, &kind, &weapon, &blade, 999, 4, 0, 0,
                           always_one_hundred);
    assert(hit.killed && k.state == 2 && k.timer == 0x28);
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{20, -40, 20, 30, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    AosHitReport hit = aos_enemy_update(&dead, &soma, &layer, &kind, &weapon, &blade, 999, 4,
                                        0, 0, always_zero);
    assert(hit.killed && dead.state == 0xFF && dead.hidden && hit.child_count == 5 &&
           dead.spawned_count == 5);
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
    const AosAnimDef defs[4] = {{1, ticks}, {1, ticks}, {1, ticks}, {1, ticks}};
    const AosAnimSet anims = {defs, 4};
    static AosEnemyKind kind;
    memset(&kind, 0, sizeof kind);
    kind.anims = &anims;
    kind.sprites[0][2] = 2;
    for (int a = 1; a < 4; ++a) {
        kind.modes[a][0] = 1;
        kind.hurt[a][0] = kind.attack[a][0] = (AosBox){-8, -16, 16, 24};
    }
    const AosEnemyStats stats = {440, 40, 24, 0x0181, 0, 0};
    AosSoma soma = aos_soma_spawn(AOS_FIXED(220), AOS_FIXED(100), NULL);
    AosEnemy n;
    aos_enemy_set_flag_8e_40(false);
    assert(aos_enemy_create(&n, AOS_ENEMY_NEMESIS, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    assert(n.state == 0 && n.anim.id == 0 && n.attack_wait >= 0x40 && n.attack_wait <= 0xBF);
    n.attack_wait = 0;
    AosHitReport helper_report =
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(helper_report.child_count == 4);
    assert(helper_report.children[0].role == AOS_ROLE_NEMESIS_HELPER &&
           helper_report.children[0].state == 1 && helper_report.children[0].static_frame == 1);
    AosEnemy helper = helper_report.children[0];
    assert(helper_report.children[1].role == AOS_ROLE_NEMESIS_EFFECT &&
           helper_report.children[1].variant == 9);
    assert(helper_report.children[2].role == AOS_ROLE_NEMESIS_EFFECT &&
           helper_report.children[2].variant == 12);
    assert(helper_report.children[3].role == AOS_ROLE_NEMESIS_EFFECT &&
           helper_report.children[3].variant == 6);
    helper.parent = &n;
    aos_enemy_update(&helper, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(!helper.hidden && helper.static_frame == 1 && helper.x == n.x && helper.y == n.y);
    helper.state = 2;
    helper.step = 0;
    aos_enemy_update(&helper, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(helper.static_frame == 2 && helper.y == n.y + AOS_FIXED(37) &&
           (helper.x == n.x - AOS_FIXED(9) || helper.x == n.x + AOS_FIXED(9)));
    helper.state = 3;
    helper.step = 0;
    aos_enemy_update(&helper, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(helper.static_frame == 3 && helper.y == n.y - AOS_FIXED(4));
    AosEnemy effect = helper_report.children[1];
    effect.parent = &n;
    n.anim.frame = 2;
    aos_enemy_update(&effect, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(effect.state == 2 && effect.timer == 1 && effect.step == 9 && !effect.hidden &&
           effect.translucent && effect.static_frame == 0 && effect.x == 0 && effect.y == 0 &&
           effect.combat.attack_off && effect.combat.hurt_off);
    aos_enemy_update(&effect, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(effect.timer == 2 && effect.static_frame == 0);
    for (int i = 0; i < 8; ++i)
        aos_enemy_update(&effect, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(effect.timer == 10 && effect.static_frame == 2 && effect.x == n.x &&
           effect.y == (n.y >> 16) * 65536);
    n.defeated = true;
    aos_enemy_update(&helper, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(helper.removed);
    n.defeated = false;
    aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.state == 1);
    aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.anim.id == 1 && n.vx > 0);
    int frames = 0;
    while (n.state == 1 && frames++ < 80)
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.state == 2);
    aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.anim.id == 2);
    while (n.state == 2 && frames++ < 120)
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.state == 3);
    aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.anim.id == 3);
    while (n.state == 3 && frames++ < 180)
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.state == 0 && n.attack_wait == 0x40);

    /* The direct frame-1 box participates in the normal weapon collision. */
    n.state = 1;
    n.step = 1;
    n.timer = 0x20;
    n.x = soma.x;
    n.y = soma.y;
    n.hp = 1;
    static const uint8_t blade_ticks[] = {8};
    const AosAnimDef blade_def = {1, blade_ticks};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-8, -16, 16, 24, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
    AosHitReport hit = aos_enemy_update(&n, &soma, &layer, &kind, &weapon, &blade, 999, 4, 0, 0,
                                        always_zero);
    assert(hit.enemy_hit && hit.killed && n.defeated && n.timer == 0x28 && n.anim.id == 3);
    frames = 0;
    while (!n.removed && frames++ < 50)
        aos_enemy_update(&n, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(n.removed && frames == 40);

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
    aos_enemy_update(&d, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
    assert(d.anim.id == 3 && d.vx == 0 && d.vy == 0 && d.timer == 0x80);
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
    soma.x = d.x - AOS_FIXED(30);
    soma.y = d.y + AOS_FIXED(10);
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{20, -40, 20, 30, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    d.hit_flash = 0;
    AosHitReport hit =
        aos_enemy_update(&d, &soma, &layer, &kind, &weapon, &blade, 999, 4, 0, 0, always_zero);
    assert(hit.killed && d.state == 0xFF && d.hidden && hit.child_count == 5 &&
           d.spawned_count == 5);
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    while (g.step == 2 && frames < 100) {
        aos_enemy_update(&g, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, always_zero);
        ++frames;
    }
    assert(frames == 21 && g.step == 3 && g.anim.id == 1 && g.ay == 0x1800);
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    AosSoulAttack ring = {{90, 80, 110, 104}, 26, 0, false, NULL};
    aos_enemy_set_soul_attacks(&ring, 1);
    AosHitReport hit = aos_enemy_update(&e, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0,
                                        never_zero);
    int expected = aos_enemy_damage(26, stats.defence, 0, 0, 0);
    assert(hit.enemy_hit && hit.enemy_damage == expected && e.hp == 30 - expected);
    /* The skill learns it struck (its hit callback). */
    assert(ring.struck && ring.target == &e);
    /* The weapon's ATK (10) is not used for it. */
    assert(expected != aos_enemy_damage(10, stats.defence, 0, 0, 0));
    /* Elements meet the record's weakness. */
    AosEnemy weak;
    const AosEnemyStats weak_stats = {30, 16, 2, 0x0004, 0, 0};
    assert(aos_enemy_create(&weak, AOS_ENEMY_SKELETON, 100, 100, 0, 0, &soma, &layer, &kind,
                            &weak_stats));
    AosSoulAttack fire = {{90, 80, 110, 104}, 26, 0x0004, false, NULL};
    aos_enemy_set_soul_attacks(&fire, 1);
    hit = aos_enemy_update(&weak, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(hit.enemy_damage == aos_enemy_damage(26, 2, 0x0004, 0, 0x0004) &&
           hit.enemy_damage > expected);
    /* A box beside the enemy does nothing. */
    AosEnemy apart;
    assert(aos_enemy_create(&apart, AOS_ENEMY_SKELETON, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    AosSoulAttack beside = {{130, 80, 150, 104}, 26, 0, false, NULL};
    aos_enemy_set_soul_attacks(&beside, 1);
    hit = aos_enemy_update(&apart, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(!hit.enemy_hit && apart.hp == 30 && !beside.struck);
    /* Enough power kills it. */
    AosSoulAttack strong = {{90, 80, 110, 104}, 200, 0, false, NULL};
    aos_enemy_set_soul_attacks(&strong, 1);
    hit = aos_enemy_update(&apart, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(hit.killed && apart.defeated);
    aos_enemy_set_soul_attacks(NULL, 0);
    memset(cells, 0, sizeof cells);
}

int main(void) {
    buer_surviving_hit_interrupt_tests();
    buer_linked_fire_lifetime_tests();
    buer_room_exit_tests();
    buer_linked_fire_facing_tests();
    buer_near_player_attack_tests();
    buer_idle_directional_wakeup_tests();
    buer_rolling_fire_sign_sync_tests();
    buer_no_contact_angular_tests();
    buer_solid_wall_angular_bounce_tests();
    buer_floor_slope_angular_impulse_tests();
    buer_state_one_horizontal_tests();
    buer_state_two_idle_horizontal_tests();
    buer_state_two_idle_vertical_tests();
    buer_attack_timing_tests();
    buer_death_stages_tests();
    buer_attack_death_animation_tests();
    buer_visual_only_linked_fire_tests();
    buer_native_attack_side_sign_tests();
    buer_native_side_spin_and_projectile_mirror_tests();
    buer_projectile_signed_y_wrap_tests();
    buer_projectile_lifecycle_tests();
    buer_attack_fire_mirror_sign_gate_tests();
    buer_attack_cap_and_recovery_tests();
    buer_terminal_death_flames_tests();
    buer_death_scatter_and_linked_wobble_tests();
    buer_native_probe_geometry_tests();
    buer_special_full_block_tests();
    buer_floor_edge_slope_probe_tests();
    buer_mode_six_floor_edge_support_tests();
    buer_native_integrated_motion_tests();
    buer_constructor_native_velocity_tests();
    buer_native_body_facing_tests();
    buer_native_death_stage_one_walk_tests();
    buer_inert_orbit_tests();
    white_dragon_recovery_tests();
    peeping_eye_native_steering_tests();
    giant_ghost_death_cue_tests();
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
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15, 0};
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
    archer_tests();
    needles_tests();
    officer_tests();
    gargoyle_tests();
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
