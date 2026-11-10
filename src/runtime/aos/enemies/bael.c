/* Bael (0x5E): the sword-sorcerer that duels Soma. Its native
 * routines are func_080CC624 (create — the resource loader is the
 * shared sub_0806E0D0 with tiles 0x081F428C, palette 0x0820BF9C,
 * frames 0x0824FDB0) and func_080CD2D0 (update) dispatching
 * func_080CD378 (state 1 idle), func_080CC800 (state 2 attack),
 * func_080CCDDC (state 3 death) and func_080CD3AC (state 4 dormant);
 * the contact func_080CD404 feeds sub_08021654 and the hurt
 * func_080CD434 the shared damage walk — both wired in aos_enemy.c.
 *
 * func_080CC744 spawns the persistent companion of kind 0x1B/0x48
 * (role AOS_ROLE_BAEL_COMPANION): it shares the Bael frames,
 * sits at + 0x65 = 1, and its update func_080CD348 copies the parent's
 * + 0x40/+ 0x44 every tick — the spinning blade visual. func_080CD144
 * is the walk driver: sub_0806CAF8 at the + 0x18 speed accumulator
 * (slope bits 0x20 grow / 0x40 brake it by 0x666, otherwise it decays
 * 0x20/frame clamped to [-0x7000, 0x6FFF]), + 0x14 += speed >> 6 as
 * the affine rotation angle, walls halve or reverse the speed and an
 * airborne walk with dir 0 halves it reversed; the + 0x58 bit 6 of
 * the shared instance record follows the speed sign (mirrored here).
 *
 * Approximations: the per-instance record at 0x084F0B14 + 0x4E4 +
 * index * 0x84 (probe origins, anim tables 0x08528B70 / 0x08528922-
 * family, the 6 x 5 sword-sweep grid of func_08001D94 in the attack)
 * collapses onto the kind probes and a centred player box; the
 * sub_0806DF40 range reads become square player_in boxes; the attack
 * substeps 1 / 2 / 0xA bodies were only partially traced and run as
 * approach / spin / recover; the death's split between body and
 * companion (the native rotates the companion's affine while the
 * body drifts) applies the spin to the body; func_0806B0A8's
 * tile-byte-1 kill floor is approximated by the floor cell byte;
 * func_0806AF98's large probe-table init is the framework's. */
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
#define ground_walk aos_enemy_ground_walk
#define player_in_centred aos_enemy_player_in_centred
#define cell_kind aos_enemy_cell_kind
#define spawn_drift aos_enemy_spawn_drift
#define make_debris aos_enemy_make_debris

static int16_t ba_px(const AosEnemy *e) { return (int16_t)(e->x >> 16); }
static int16_t ba_py(const AosEnemy *e) { return (int16_t)(e->y >> 16); }

/* func_0806DF40(e, xoff, size): the player inside the size box whose
 * centre sits xoff pixels ahead of the enemy — approximated as a
 * square centred on the enemy height. size 0 reads as the native's
 * smallest probe. */
static bool ba_player_box(const AosEnemy *e, const AosSoma *soma,
                          int xoff, int size) {
    if (size <= 0) size = 0x10;
    int16_t px = (int16_t)(ba_px(e) + xoff);
    int16_t py = ba_py(e);
    return player_in(soma, (int16_t)(px - size / 2), (int16_t)(py - size / 2),
                     size, size);
}

/* func_080CD144(e, dir): one walk step. dir 0 walks with mode 6
 * (raw sign + no-floor undo), dir 1 with mode 2; the + 0x18 speed
 * (`spin`) drives sub_0806CAF8, then the slope-bit dynamics, the
 * + 0x14 += speed >> 6 rotation and the wall / airborne corrections. */
static void ba_walk(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                    const AosEnemyKind *kind, int dir) {
    int32_t x0 = e->x;
    int speed = (int)e->spin;
    int result = ground_walk(e, soma, layer, &kind->probes, speed,
                             dir ? 2 : 6);
    /* The native's ahead-probe snap-back (sub_0806DF40 with the speed
     * as pixel offset) keeps the body out of the player: approximated
     * by the centred box. */
    if (ba_player_box(e, soma, speed >= 0 ? 0x18 : -0x18, 0))
        e->x = x0;
    int32_t s = e->spin;
    if (result & 0x40) {
        s += s > 0 ? -0x666 : 0x666;
    } else if (result & 0x20) {
        s += s > 0 ? 0x666 : -0x666;
    } else {
        if (s < 0) {
            s += 0x20;
            if (s < -0x7000) s = -0x7000;
        } else {
            s -= 0x20;
            if (s > 0x6FFF) s = 0x6FFF;
        }
    }
    e->spin = s;
    /* + 0x14 += speed >> 6, truncated toward zero. */
    e->angle = (uint32_t)((int32_t)e->angle + (s / 64));
    if (result & 3) {
        /* Wall: a solid probe ahead reverses the speed, else halves it
         * toward zero (the instance-table probe origin approximated by
         * the body centre + 0x18 px). */
        int px = ba_px(e) + (s >= 0 ? 8 : -8);
        int py = ba_py(e) + 0x18;
        bool solid = cell_kind(layer, px, py) != 0 &&
                     cell_kind(layer, px, py) != 1;
        e->spin = solid ? -s : s / 2;
    } else if (dir == 0 && !(result & 8)) {
        e->spin = -(s / 2);
    }
    e->mirrored = e->spin < 0;
}

/* func_080CD47C: the out-of-world kill — the native reads tile byte 1
 * at the instance probe origin; approximated by the floor cell under
 * the feet. On a kill it also deletes the companion (the shared
 * instance record's sibling) and reports state 3. */
static bool ba_kill_check(AosEnemy *e, const AosCollision *layer) {
    uint8_t cell = aos_collision_cell(layer, ba_px(e), ba_py(e) + 8);
    if (cell != 1) return false;
    e->state = 3;
    e->step = e->substep = 0;
    e->timer = 0;
    return true;
}

/* func_080CC744's companion: spawned lazily on the first update
 * (children cannot be emitted inside create). It shares the kind's
 * frames, sits translucent at static frame 1 and copies the parent's
 * position every tick through its own update below. */
static void ba_spawn_companion(AosEnemy *e, AosHitReport *spawned,
                               const AosEnemyKind *kind) {
    if (spawned->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy c = {0};
    c.id = e->id;
    c.role = AOS_ROLE_BAEL_COMPANION;
    c.stats = e->stats;
    c.hp = 1;
    c.static_frame = 1;
    c.translucent = true;           /* + 0x59 bit 1 */
    c.parent = e;
    c.x = e->x;
    c.y = e->y;
    c.mirrored = e->mirrored;
    play_loop(&c, kind->anims, 0, true);
    spawned->children[spawned->child_count++] = c;
}

/* func_080CD378: state 1 — kill check, walk (dir 1), and the
 * sub_0806DF40(e, 0, 0x5C) aggro box entering the attack. */
static void ba_idle(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                    const AosEnemyKind *kind) {
    if (ba_kill_check(e, layer)) return;
    ba_walk(e, soma, layer, kind, 1);
    if (ba_player_box(e, soma, 0, 0x5C)) {
        e->state = 2;
        e->step = e->substep = 0;
    }
}

/* func_080CC800: state 2 — the spin attack. Substep 0 approaches or
 * retreats on the player boxes (0x58 engage / 0x20 special), waits
 * for the ground probe, then the 6 x 5 wall sweep selects the spin;
 * the traced tail runs approach -> spin (anim 1, contact live through
 * the framework collide) -> recover -> idle. */
static void ba_attack(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind) {
    if (ba_kill_check(e, layer)) return;
    switch (e->step) {
    case 0: {
        if (ba_player_box(e, soma, 0, 0x58)) {
            ba_walk(e, soma, layer, kind, 0);
        } else {
            ba_walk(e, soma, layer, kind, 1);
            e->ay = 0x4000;
        }
        if (ba_player_box(e, soma, 0, 0x20)) {
            e->step = 0xA;
            e->substep = 0;
            break;
        }
        /* The native's ground + 6 x 5 wall-sweep probes gate the spin:
         * approximated by a wall ahead during the approach. */
        int px = ba_px(e) + (e->mirrored ? -0x10 : 0x10);
        if (cell_kind(layer, px, ba_py(e)) >= 2) {
            e->step = 0xA;
            e->substep = 0;
            break;
        }
        if (player_in_centred(e, soma, 0xC0, 0x30)) {
            play(e, kind->anims, 1);
            e->step = 1;
            e->substep = 0;
            e->timer = 0;
        }
        if (++e->timer == 0xFF) { e->step = 1; e->timer = 0; }
        break;
    }
    case 1:
        /* The spin: keep drifting with the walk driver at reduced
         * speed while the anim plays; contact damage is the shared
         * collide pass. */
        ba_walk(e, soma, layer, kind, 0);
        if (e->cycle_done || e->timer++ > 0x28) {
            e->step = 2;
            e->timer = 0;
        }
        break;
    case 2:
        ba_walk(e, soma, layer, kind, 1);
        if (e->timer++ > 0x14) {
            play(e, kind->anims, 0);
            e->state = 1;
            e->step = e->substep = 0;
        }
        break;
    default: /* 0xA: disengage back to the watch. */
        play(e, kind->anims, 0);
        e->state = 1;
        e->step = e->substep = 0;
        break;
    }
}

/* func_080CCDDC: state 3 — the death. Song 0x6E once, the + 0x72 bit 1
 * attack mark, + 0xF = 1, the + 0x5A draw group from + 0x2C, spin
 * +/-0x20000 decaying (>> 6, clamped), the + 0x14/+ 0x15 pixel drift
 * and, at the end, the seven-piece debris ring of 0x08119500 kind
 * 0x49. The native splits the spin onto the companion's affine; the
 * body carries it here (documented). */
static void ba_death(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                     const AosEnemyKind *kind, AosHitReport *spawned,
                     uint32_t (*random)(void)) {
    (void)soma; (void)random;
    switch (e->step) {
    case 0:
        if (!e->ba_flag) {
            e->ba_flag = 1;
            aos_audio_play_song(0x6E);
            e->attack_off = true;      /* + 0x72 bit 1 */
            e->translucent = true;     /* + 0x5A draw group approx */
            e->timer = 0x60;
            e->spin = e->spin >= 0 ? 0x20000 : -0x20000;
            e->ba_drift_x = 1;
            e->ba_drift_y = 1;
            e->vx = e->vy = e->ax = e->ay = 0;
        }
        /* spin decay: +0x18 += (+0x18 >> 6) clamped [-0x7000, 0x6FFF]. */
        int32_t s = e->spin + (e->spin >> 6);
        if (s > 0x6FFF) s = 0x6FFF;
        if (s < -0x7000) s = -0x7000;
        e->spin = s;
        e->angle = (uint32_t)((int32_t)e->angle + (s / 64));
        e->x += (int32_t)e->ba_drift_x << 16;
        e->y += (int32_t)e->ba_drift_y << 16;
        if (--e->timer == 0xFF) {
            e->timer = 0x80;
            e->step = 1;
        }
        break;
    case 1:
        /* The fall continues while the burst waits; the native's
         * substep 2 spawns the drifting effect — spark drifts stand
         * in (documented). */
        if ((e->timer & 7) == 0)
            spawn_drift(AOS_EFFECT_SPARK, e->x, e->y, spawned);
        if (--e->timer == 0xFF) e->step = 2;
        break;
    default:
        /* Seven debris at the 0x08119500 ring offsets + 0x40000. */
        if (e->substep == 0) {
            static const int16_t ring[7][2] = {
                {0x14, 0}, {0xF, 0xF}, {0, 0x14}, {-0xF, 0xF},
                {-0x14, 0}, {-0xF, -0xF}, {0, -0x14}};
            for (int i = 0; i < 7; ++i) {
                AosEnemy d = make_debris(e, kind->anims, 0);
                d.x = e->x + ((int32_t)ring[i][0] << 16) + 0x40000;
                d.y = e->y + ((int32_t)ring[i][1] << 16) + 0x40000;
                if (spawned->child_count < AOS_ENEMY_MAX_CHILDREN)
                    spawned->children[spawned->child_count++] = d;
            }
            aos_audio_play_song(0x6E);
            e->substep = 1;
        }
        e->removed = true;
        break;
    }
    (void)layer;
}

/* func_080CD3AC: state 4 — the dormant spin (the cleared-area
 * form): rise 0x20 px per tick, static frame cleared, angle += 0x100. */
static void ba_dormant(AosEnemy *e, const AosEnemyKind *kind) {
    (void)kind;
    if (e->step == 0) {
        e->y -= 0x20 << 16;
        e->static_frame = 0;
        e->step = 1;
    }
    e->angle = (uint32_t)((int32_t)e->angle + 0x100);
}

/* The companion's update (func_080CD348): while its state byte is 0
 * it copies the parent's + 0x40/+ 0x44 — here x/y — every tick. */
static AosHitReport ba_companion_update(AosEnemy *e, AosSoma *soma,
                                        const AosCollision *layer,
                                        const AosEnemyKind *kind,
                                        const AosWeaponEntity *weapon,
                                        const AosWeaponFrames *weapon_frames,
                                        int soma_atk, int soma_def,
                                        int cam_x, int cam_y,
                                        uint32_t (*random)(void)) {
    (void)weapon; (void)weapon_frames; (void)soma_atk; (void)soma_def;
    (void)cam_x; (void)cam_y; (void)random;
    if (e->parent && !e->parent->removed && !e->parent->defeated) {
        e->x = e->parent->x;
        e->y = e->parent->y;
        e->mirrored = e->parent->mirrored;
    } else {
        e->removed = true;
    }
    AosHitReport r = {0};
    (void)soma; (void)layer; (void)kind;
    return r;
}

AosHitReport aos_enemy_bael_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    if (enemy->role == AOS_ROLE_BAEL_COMPANION)
        return ba_companion_update(enemy, soma, layer, kind, weapon,
                                   weapon_frames, soma_atk, soma_def,
                                   cam_x, cam_y, random);
    AosHitReport report = {0};
    /* func_080CD2D0: state 4 dormant and state 3 death bypass the
     * shared preamble; the rest run the anim step then dispatch. The
     * func_080CC744 companion is emitted lazily on the first live
     * tick (children cannot spawn inside create — documented). */
    if (enemy->role == AOS_ROLE_ENEMY && !enemy->ba_companion &&
        enemy->state != 3 && enemy->state != 4) {
        enemy->ba_companion = 1;
        ba_spawn_companion(enemy, &report, kind);
    }
    if (enemy->state == 4) {
        ba_dormant(enemy, kind);
        AosHitReport hit = collide(enemy, soma, kind, weapon,
                                   weapon_frames, soma_atk, soma_def);
        aos_enemy_merge_hit(&report, hit);
        return report;
    }
    if (enemy->state == 3) {
        ba_death(enemy, soma, layer, kind, &report, random);
        return report;
    }
    step_anim(enemy, kind->anims);
    if (enemy->state == 1)
        ba_idle(enemy, soma, layer, kind);
    else if (enemy->state == 2)
        ba_attack(enemy, soma, layer, kind);
    AosHitReport hit = collide(enemy, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    aos_enemy_merge_hit(&report, hit);
    return report;
}
