/* Altair (0x2C): the spectral swordsman. Its native routines are
 * EnemyAltairCreate (0x080C1490), EnemyAltairUpdate (0x080C15C8)
 * dispatching sub_080C1694 (state 1, the six-substep dash),
 * sub_080C1898 (state 2, the death burst), sub_080C1940 (state 5,
 * the recursive copy spawner) and sub_080C1CDC (state 4, the dormant
 * animation); the contact sub_080C1D0C feeds sub_08021654 and the
 * hurt sub_080C19E0 the shared damage walk — both are wired in
 * aos_enemy.c.
 *
 * The create parks param0 == 0 forms hidden in state 5 where every
 * 0x40 ticks (rng & 0x3F == 0, while the + 0x1E s16 count stays
 * <= 1) it clones itself through func_080C1490 with param0 2 or 3
 * (the two dash forms) and the parent's param1; the copies are full
 * Altair entities here too (role AOS_ROLE_ENEMY children), the
 * native's + 0x20 parent link maps to `parent`.
 *
 * The blade children are the kind-0x1B/0x48 entities of
 * func_080C1A64 (the persistent partner, param1 1 = Fleaman form /
 * 2 = Ripper form — the native draws the form enemy's frames through
 * func_080DA7A4; Altair's own frames stand in, documented
 * approximation) and func_080C1D38 (the four free bolts of the death
 * burst, angle 0 or 0x8000 by func_080C1DF0). All run sub_080C1B54:
 * two init ticks scattering around the parent's pixel anchor, then a
 * flight where x += sine(angle) >> 2, y += 0x1000 and the + 0x16
 * angle advances by the + 0x14 speed (rng & 0x7F + 0x40) until it
 * crosses the 0x1FFF / 0x9FFF bounds.
 *
 * Approximations: the + 0x4A window field's native writer was not
 * traced (kept 0, so the state-1 tail despawns on screen x outside
 * [-0x80, 0x170]); the global 0x84F0B14 + 0x8E & 0x40 dormant check
 * has no counterpart (the combat form always runs); func_0806D4C0's
 * stun gate and func_08042B8C's knockback are not ported; the
 * partner's Fleaman/Ripper frames use Altair's anims. */
#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../math/aos_math.h"
#include "../core/aos_collision.h"

#define player_in aos_enemy_player_in
#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define step_anim aos_enemy_step_anim
#define face_player aos_enemy_face_player
#define outside_margins aos_enemy_outside_margins
#define collide aos_enemy_collide
#define spawn_drift aos_enemy_spawn_drift
#define sine aos_sine

static int16_t al_px(const AosEnemy *e) { return (int16_t)(e->x >> 16); }
static int16_t al_py(const AosEnemy *e) { return (int16_t)(e->y >> 16); }

/* func_0803F2C8(e, seq, 0, loop): set the animation only when it
 * differs — the native helper is idempotent across the per-tick
 * calls of the pose check. */
static void al_anim(AosEnemy *e, const AosAnimSet *anims, unsigned seq,
                    bool loop) {
    if (e->anim.id != seq) play_loop(e, anims, seq, loop);
}

/* The + 0x42/+ 0x46 pixel anchor synced from the fixed-point position
 * each tick (the shared position commit of the native helpers); the
 * state machines and the blade scatter read it. */
static void al_sync_px(AosEnemy *e, int cam_x) {
    e->al_wx = (int16_t)(al_px(e) - cam_x);
    e->al_wy = al_py(e);
}

/* func_080C1A64: the persistent partner blade. It hovers on the
 * parent through the state-1/2 sync (sub_080C1694 copies the parent's
 * + 0x40/+ 0x44 + the 0x130000 / 0x110000 lift and facing onto it),
 * which the blade performs itself here since children cannot be
 * reached through a pointer; `al_linked` marks it versus the free
 * bolts. param1 1 / 2 selects the Fleaman / Ripper form natively —
 * stored in `variant` for the record, the frames are approximated. */
static void al_spawn_link(AosEnemy *e, AosHitReport *spawned,
                          const AosEnemyKind *kind,
                          uint32_t (*random)(void)) {
    if (spawned->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy c = {0};
    c.id = e->id;
    c.role = AOS_ROLE_ALTAIR_BLADE;
    c.stats = e->stats;
    c.hp = 1;
    c.static_frame = -1;
    c.translucent = true;           /* + 0x59 bit 1 */
    c.parent = e;
    c.variant = (uint8_t)e->param1; /* 1 Fleaman / 2 Ripper form */
    c.al_linked = 1;
    c.x = e->x;
    c.y = e->y + 0x130000;
    c.mirrored = e->mirrored;
    play_loop(&c, kind->anims, 0, true);
    (void)random;
    spawned->children[spawned->child_count++] = c;
}

/* func_080C1D38 + func_080C1DF0: one free bolt of the death burst —
 * the same kind-0x1B entity initialised by sub_080C1D88 (the shared
 * Altair tiles 0x081F4234 / palette 0x0820BD70 / frames 0x0824B660,
 * carried by the kind) with + 0x10 the parent and + 0x16 the start
 * angle (0 or 0x8000 by the caller's coin flip). */
static void al_spawn_bolt(AosEnemy *e, AosHitReport *spawned,
                          const AosEnemyKind *kind, uint32_t angle) {
    if (spawned->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy c = {0};
    c.id = e->id;
    c.role = AOS_ROLE_ALTAIR_BLADE;
    c.stats = e->stats;
    c.hp = 1;
    c.static_frame = -1;
    c.translucent = true;
    c.parent = e;
    c.x = e->x;
    c.y = e->y;
    c.mirrored = e->mirrored;
    c.al_angle = (uint16_t)angle;
    play_loop(&c, kind->anims, 0, true);
    spawned->children[spawned->child_count++] = c;
}

/* sub_080C1694, state 1: the six-substep dash. Step 0 picks
 * animation 0 with the link / 1 without, sets + 0x18 to match, arms
 * vx ±0xA000 against the facing bit and + 0xD = 0x30; step 1 moves
 * and re-anchors the partner 19 px below while the player-distance
 * probes arm + 0x1D; the armed window check on the pixel anchor
 * advances; steps 2 / 4 play animations 2 / 1; step 3 moves, lifts
 * the partner 17 px and waits for animation 2's frame before
 * advancing; step 5 coasts. The tail despawns outside the pixel
 * window and parks the partner in state 0xFF while step <= 2. */
static void al_state1(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                      AosHitReport *spawned, int cam_x,
                      uint32_t (*random)(void)) {
    (void)cam_x;
    switch (e->step) {
    case 0:
        /* The link spawn of func_080C1A64 happens at create natively;
         * children cannot be emitted there, so the first dash entry
         * emits it (documented one-tick shift). */
        if (!e->al_linked && (e->param1 == 1 || e->param1 == 2) &&
            e->al_copies >= 0) {
            /* + 0x14 link: only the root form (param0 0/1) keeps one;
             * the copies dash on their own. */
            if (e->param0 <= 1) {
                e->al_linked = 2;    /* link emitted mark */
                al_spawn_link(e, spawned, kind, random);
            }
        }
        al_anim(e, kind->anims, e->al_linked ? 0 : 1, true);
        e->vx = e->mirrored ? 0xA000 : -0xA000;
        e->vy = 0;
        e->timer = 0x30;
        e->step = 1;
        e->al_draw5c = 0x30;
        break;
    case 1: {
        e->x += e->vx;
        if (e->al_linked == 2) {
            /* The partner sync of 0x080C1720: the blade's own update
             * re-anchors it; here only the player-distance probes of
             * sub_0806BD64 arm + 0x1D. */
            int16_t d = (int16_t)((aos_enemy_target_x(soma) - e->x) >> 16);
            bool near = false;
            if (!e->mirrored) {
                if (d > 0x50 && (random() & 0x1F) == 0) near = true;
                else if (d < -4) near = true;
            } else {
                if (d < -0x50 && (random() & 0x1F) == 0) near = true;
                else if (d > 4) near = true;
            }
            if (near) e->al_mark1d = 1;
        }
        if (e->al_mark1d == 1 &&
            (uint16_t)(e->al_wx - 0x21) <= 0xAE) {
            e->step++;
            e->al_mark1d = 0;
            e->timer = 0;
        }
        break;
    }
    case 2:
        al_anim(e, kind->anims, 2, true);
        e->step++;
        e->timer = 0;
        break;
    case 3: {
        e->x += e->vx;
        if (e->al_linked == 2 && e->anim.id == 2 &&
            e->anim.frame == 7 && e->al_f6f == 0) {
            /* + 0x6F was 0 and + 0x65 read 7 natively; the animation
             * frame 7 of sequence 2 stands in (documented). */
            e->al_mark1c = 0;
            e->step++;
            e->timer = 0;
        }
        break;
    }
    case 4:
        al_anim(e, kind->anims, 1, true);
        e->step++;
        e->timer = 0;
        break;
    case 5:
        e->x += e->vx;
        break;
    }
    /* The tail of 0x080C1852: the pixel-window despawn and the
     * partner park while step <= 2 (the blade self-deletes on
     * parent->state == 2, covering the native + 0xB = 0xFF write). */
    if (e->al_wx < -e->al_ww - 0x80 || e->al_wx > e->al_ww + 0x170)
        e->removed = true;
}

/* sub_080C1898, state 2: the death burst — four free bolts at
 * angles 0 / 0x8000 by coin flip, song 0x72, animation 3, the
 * + 0x4C = 0xC0000 plunge and the + 0xD = 0x3F timer; the follow-up
 * integrates the plunge through func_0806BE74 (approximated by the
 * plain integrate — its collision detail is not ported). */
static void al_state2(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                      AosHitReport *spawned, int cam_x, int cam_y,
                      uint32_t (*random)(void)) {
    (void)soma; (void)cam_x; (void)cam_y;
    if (e->step == 0) {
        al_anim(e, kind->anims, 3, true);
        for (int i = 0; i < 4; ++i)
            al_spawn_bolt(e, spawned, kind, (random() & 1) ? 0x8000 : 0);
        aos_audio_play_song(0x72);
        e->vy = 0xC0000;
        e->timer = 0x3F;
        e->step = 1;
        return;
    }
    e->x += e->vx;
    e->y += e->vy;
}

/* sub_080C1940, state 5: the dormant spawner — every tick with
 * (rng & 0x3F) == 0 and the + 0x1E s16 count <= 1 it clones itself
 * through func_080C1490 with param0 2 + (count & 1), the parent's
 * param1, position and facing, then counts up. */
static void al_state5(AosEnemy *e, const AosEnemyKind *kind,
                      AosHitReport *spawned, uint32_t (*random)(void)) {
    if ((random() & 0x3F) != 0 || e->al_copies > 1) return;
    if (spawned->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy c = *e;
    c.step = c.substep = 0;
    c.state = 1;                    /* EnemyAltairCreate's default */
    c.param0 = (int16_t)(2 + (e->al_copies & 1));
    c.parent = e;                   /* + 0x20 */
    c.al_copies = 0;
    c.al_linked = 0;
    c.al_mark1c = c.al_mark1d = 0;
    c.al_draw5c = 3;
    c.removed = c.defeated = c.hidden = false;
    c.vx = c.vy = c.ax = c.ay = 0;
    play_loop(&c, kind->anims, 1, true);
    spawned->children[spawned->child_count++] = c;
    e->al_copies++;
}

/* sub_080C1CDC, state 4: the dormant form — animation 0 once, then
 * stepped; it never deletes natively, kept inert here. */
static void al_state4(AosEnemy *e, const AosEnemyKind *kind) {
    if (e->step == 0) {
        al_anim(e, kind->anims, 0, true);
        e->step = 1;
    }
    step_anim(e, kind->anims);
}

/* sub_080C1B54: the blade. States 0 and 1 scatter it around the
 * parent's pixel anchor (+ 0x42/+ 0x46 + rng - 0x10 / - 8), arm
 * vy 0x1000, the + 0x14 speed (rng & 0x7F + 0x40) and + 0x59 bit 1;
 * state 2 flies — x += sine(+ 0x16) >> 2, y += vy, angle += speed —
 * until the angle crosses the 0x1FFF / 0x9FFF bounds; state 3
 * deletes. The linked partner re-anchors on its parent every tick
 * (the sync sub_080C1694 performs natively through + 0x14). */
AosHitReport aos_enemy_altair_blade_update(AosEnemy *e, AosSoma *soma,
                                           const AosCollision *layer,
                                           const AosEnemyKind *kind,
                                           const AosWeaponEntity *weapon,
                                           const AosWeaponFrames *weapon_frames,
                                           int soma_atk, int soma_def,
                                           int cam_x, int cam_y,
                                           uint32_t (*random)(void)) {
    (void)weapon_frames; (void)layer;
    AosHitReport report = {0};
    /* The partner sync: while the parent dashes (states 1 / 2) the
     * blade stands 19 px (substep < 3) or 17 px below it, facing
     * copied — sub_080C1720 / sub_080C17DA. */
    if (e->al_linked && e->parent &&
        (e->parent->state == 1 || e->parent->state == 2)) {
        e->x = e->parent->x;
        e->y = e->parent->y + (e->parent->step >= 3 ? 0x110000 : 0x130000);
        e->mirrored = e->parent->mirrored;
        e->al_wx = (int16_t)(al_px(e) - cam_x);
        e->al_wy = al_py(e);
    }
    if (e->al_linked && e->parent && e->parent->state == 2) {
        /* The native parks the partner in state 0xFF on death. */
        e->removed = true;
        return report;
    }
    if (e->parent && (e->parent->removed || e->parent->defeated)) {
        e->removed = true;
        return report;
    }
    switch (e->state) {
    case 0:
    case 1: {
        int rx = e->parent ? (int16_t)(e->parent->al_wx) : 0;
        int ry = e->parent ? (int16_t)(e->parent->al_wy) : 0;
        e->al_wx = (int16_t)(rx + (int16_t)(random() & 0x1F) - 0x10);
        e->al_wy = (int16_t)(ry + (int16_t)(random() & 0xF) - 8);
        e->x = ((int32_t)e->al_wx + cam_x) << 16;
        e->y = (int32_t)e->al_wy << 16;
        e->vy = 0x1000;
        e->state++;
        e->timer = 0;
        e->al_avel = (uint16_t)((random() & 0x7F) + 0x40);
        break;
    }
    case 2:
        e->x += sine(e->al_angle) >> 2;
        e->y += e->vy;
        e->al_angle = (uint16_t)(e->al_angle + e->al_avel);
        {
            uint32_t shifted = (uint32_t)e->al_angle << 16;
            uint32_t limit = (e->al_angle & 0x8000) ? 0x9FFF0000u
                                                  : 0x1FFF0000u;
            if (shifted > limit) e->state = 3;
        }
        if (e->state == 2) {
            step_anim(e, kind->anims);
            AosHitReport hit = collide(e, soma, kind, weapon,
                                       weapon_frames, soma_atk, soma_def);
            aos_enemy_merge_hit(&report, hit);
        }
        break;
    case 3:
        e->removed = true;             /* + 0x59 bit 8 */
        break;
    default:
        step_anim(e, kind->anims);
        break;
    }
    if (!e->removed) {
        int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
        if (outside_margins(kind, 4, sx, sy)) e->removed = true;
    }
    (void)soma_atk; (void)soma_def; (void)cam_y;
    return report;
}

AosHitReport aos_enemy_altair_update(AosEnemy *e, AosSoma *soma,
                                     const AosCollision *layer,
                                     const AosEnemyKind *kind,
                                     const AosWeaponEntity *weapon,
                                     const AosWeaponFrames *weapon_frames,
                                     int soma_atk, int soma_def, int cam_x,
                                     int cam_y, uint32_t (*random)(void)) {
    (void)layer; (void)weapon; (void)weapon_frames;
    (void)soma_atk; (void)soma_def; (void)cam_y;
    AosHitReport report = {0};
    int sx = al_px(e) - cam_x, sy = al_py(e) - cam_y;
    if (e->state != 4 && e->state != 5)
        e->hidden = outside_margins(kind, 4, sx, sy);
    /* No hidden early-out: the state-5 spawner stays hidden (+ 0x58
     * bit 5) yet must keep updating, like the Ghost producer. */
    al_sync_px(e, cam_x);
    switch (e->state) {
    case 1:
        al_state1(e, soma, kind, &report, cam_x, random);
        break;
    case 2:
        al_state2(e, soma, kind, &report, cam_x, cam_y, random);
        break;
    case 4:
        al_state4(e, kind);
        break;
    case 5:
        al_state5(e, kind, &report, random);
        break;
    default:
        step_anim(e, kind->anims);
        break;
    }
    /* The + 0x59 bit-8 despawn tail of sub_080C1688: a removed copy
     * hands its + 0x1E count back to the Altair parent. */
    if (e->removed && e->parent && e->parent->id == AOS_ENEMY_ALTAIR &&
        e->parent->al_copies > 0)
        e->parent->al_copies--;
    /* sub_080421AC: the shared collide pass of every non-state-2
     * tick (state 2's burst replaces it natively). */
    if (e->state != 2) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        aos_enemy_merge_hit(&report, hit);
    }
    return report;
}
