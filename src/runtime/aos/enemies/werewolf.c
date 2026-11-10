/* Werewolf (0x32): the Aria werewolf of the castle. Its native
 * routines are EnemyWerewolfCreate (0x0806F7A4) through sub_0806F818
 * (tiles record 0x081CBF34, palette 0x0820A440 bank 0, frames
 * 0x08212C64, draw sub_0803B9D0) and EnemyWerewolfUpdate (0x0806EDB8)
 * dispatching sub_0806EEB4 (state 0 idle/walk), sub_0806F024 (state 1
 * the eight-substep attack), the shared sub_08079294 vanish (state 2),
 * sub_0806F41C (state 3 the hit leap) and sub_0803F17C (state 4, the
 * inert animation). The contact sub_0806F4B8 feeds sub_08021654 and
 * the hurt sub_0806F50C feeds the shared damage walk; both are wired
 * in aos_enemy.c. */
#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../core/aos_effect.h"

#define walk_collide aos_enemy_walk_collide
#define player_in aos_enemy_player_in
#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define step_anim aos_enemy_step_anim
#define face_player aos_enemy_face_player
#define snap_to_floor aos_enemy_snap_to_floor
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y
#define collide aos_enemy_collide
#define outside_margins aos_enemy_outside_margins
#define spawn_drift aos_enemy_spawn_drift
#define accel_move aos_enemy_accel_move

static int16_t ww_px(const AosEnemy *e) { return (int16_t)(e->x >> 16); }
static int16_t ww_py(const AosEnemy *e) { return (int16_t)(e->y >> 16); }
static int16_t ww_tpx(const AosSoma *s) { return (int16_t)(aos_enemy_target_x(s) >> 16); }

static int ww_target_right(const AosEnemy *e, const AosSoma *s) {
    return ww_tpx(s) > ww_px(e);
}

static int ww_dist(const AosEnemy *e, const AosSoma *s) {
    int32_t d = ww_tpx(s) - ww_px(e);
    return d < 0 ? -d : d;
}

/* sub_08069A00 probe wrapper: the native calls all push the probe
 * half-width 0xA on the stack; the shared collide helper takes it as
 * the last argument. */
static int ww_probe(AosEnemy *e, const AosCollision *layer, int32_t vy) {
    return walk_collide(e, layer, vy, (int32_t)0xFFE00000, 0x80000, 0xA);
}

/* sub_0806D490(e, n): vx = vx * (n - 1) / n — the state-1 friction. */
static void ww_friction(AosEnemy *e, int n) {
    e->vx = e->vx * (n - 1) / n;
}

/* sub_0806D54C: the dispatch record's + 0xC field is the maximum HP
 * (0x122 for id 0x32); the walk triggers once the wolf is hurt. */
static int ww_hurt(const AosEnemy *e) {
    return e->hp < e->stats.hp;
}

/* sub_0806EEB4: state 0. Step 0 faces the player, stands on the floor
 * probe and, inside the 0xA0 x 0xA0 box at (px-0x50, py-0x50) or once
 * hurt, arms step 1 with animation 1. Step 1 walks: while the activity
 * mark stays clear it re-faces, rests on animation-tick frames and on
 * tick 0 sets vx +/-0x10000 — x4 on animation frame 3 — then the floor
 * probe plus the 0x78 x 0x50 box at (px-0x3C, py-0x28) drops it into
 * state 1 step 0. A nonzero sub_0806D128(3) returns it to step 0. */
static void ww_idle(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                    const AosEnemyKind *kind) {
    switch (e->step) {
    case 0: {
        e->mirrored = ww_target_right(e, soma);
        e->vx = e->ax = 0;
        e->vy = 0x20000;
        e->ay = 0x2800;
        if (!(ww_probe(e, layer, 0x20000) & 4))
            break;
        if (player_in(soma, (int16_t)(ww_px(e) - 0x50),
                      (int16_t)(ww_py(e) - 0x50), 0xA0, 0xA0) ||
            ww_hurt(e)) {
            e->step = 1;
            play_loop(e, kind->anims, 1, true);
        }
        break;
    }
    case 1: {
        e->mirrored = ww_target_right(e, soma);
        /* sub_0806D128(3): the sleep/activity window — not ported, the
         * walk always runs (documented divergence). */
        if (e->anim.tick != 0) {
            e->vx = 0;
        } else {
            e->vx = e->mirrored ? 0x10000 : (int32_t)-0x10000;
            if (e->anim.frame == 3) e->vx *= 4;
        }
        if (!(ww_probe(e, layer, 0x20000) & 4))
            break;
        if (player_in(soma, (int16_t)(ww_px(e) - 0x3C),
                      (int16_t)(ww_py(e) - 0x28), 0x78, 0x50)) {
            e->state = 1;
            e->step = 0;
            e->vx = 0;
            e->timer = 0;
            play_loop(e, kind->anims, 1, true);
        }
        break;
    }
    default:
        break;
    }
}

/* sub_0806F024: state 1, the attack in eight substates. Case 0 walks
 * and counts; past 0x1D a RandomNumberGenerator & 1 roll picks the
 * charge (step 1, animation 2) or the sub_0806D0B8 terrain probe —
 * approximated by the player distance — choosing the pounce wind-up
 * (step 3, animation 3) or the charge. Case 1 launches the dash once
 * animation frame 2 shows. Case 2 is the airborne follow-through:
 * ww_mark ends it into the state-3 leap, the animation cycle lands it
 * back into the walk, otherwise friction and the floor probe feed the
 * howl spawn (sub_0806DA6C -> sub_0806D7F0, approximated by a dark
 * smoke drift) every fourth grounded tick. Case 3 waits two frames.
 * Case 4 is the howl: song 0x77 with a dust puff each eighth timer
 * tick, the sub_0806F5C8 shadow spawns at timer 0x1F/0x24/0x25
 * (approximated by dark smoke drifts) and song 0x79 closes it.
 * Cases 5-7 are the landing, the cycle wait and the 0x2C recovery. */
static void ww_attack(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, AosHitReport *spawned,
                      uint32_t (*random)(void)) {
    switch (e->step) {
    case 0: {
        if (!(ww_probe(e, layer, 0x20000) & 4))
            break;
        uint8_t old = e->timer;
        e->timer = (uint8_t)(old + 1);
        if (old <= 0x1D) break;
        if (random() & 1) {
            e->step = 1;
            play_loop(e, kind->anims, 2, false);
            break;
        }
        /* sub_0806D0B8(x, y): the terrain probe at the player's side —
         * approximated by the horizontal distance (documented). */
        if (ww_dist(e, soma) <= 0x60) {
            e->step = 3;
            play_loop(e, kind->anims, 3, false);
        } else {
            e->step = 1;
            play_loop(e, kind->anims, 2, false);
        }
        break;
    }
    case 1:
        if (e->anim.frame <= 1) break;
        e->step = 2;
        e->vx = e->mirrored ? 0x50000 : (int32_t)-0x50000;
        e->ax = 0;
        e->vy = 0x20000;
        e->ay = 0x2800;
        e->timer = 0;
        e->ww_mark = 0;
        break;
    case 2: {
        if (e->ww_mark) {
            e->ww_mark = 0;
            play_loop(e, kind->anims, 0, false);
            e->state = 3;
            e->step = 0;
            break;
        }
        if (e->cycle_done) {
            e->vx = e->ax = 0;
            e->vy = 0x20000;
            e->ay = 0x2800;
            if (ww_probe(e, layer, 0x20000) & 4) {
                e->state = 0;
                e->step = 1;
                play_loop(e, kind->anims, 1, true);
            }
            break;
        }
        if (e->anim.frame > 2) {
            ww_friction(e, 4);
        } else {
            ww_friction(e, 0x10);
        }
        int hit = ww_probe(e, layer, 0x20000);
        uint8_t old = e->timer;
        e->timer = (uint8_t)(old + 1);
        if ((old & 3) == 0 && (hit & 4)) {
            /* sub_0806DA6C -> sub_0806D7F0: the rising spectral wolf —
             * approximated by a dark smoke drift (documented). */
            int32_t hx = e->x + e->vx * 6 - 0x20000;
            int32_t hy = e->y - 0x20000;
            spawn_drift(AOS_EFFECT_DARK_SMOKE, hx, hy, spawned);
        }
        break;
    }
    case 3:
        if (e->anim.frame <= 1) break;
        e->step = 4;
        e->anim.tick = 0;
        e->timer = 0;
        break;
    case 4: {
        if ((e->timer & 7) == 0) {
            aos_audio_play_song(0x77);
            int32_t dx = e->mirrored ? -0xE0000 : 0xE0000;
            spawn_drift(AOS_EFFECT_DUST, e->x + dx, e->y - 0x140000,
                        spawned);
        }
        e->timer = (uint8_t)(e->timer + 1);
        e->anim.tick = 0;
        if (e->timer == 0x1E) e->anim.tick = 0xC8;
        if (e->timer == 0x1F) {
            /* sub_0806F5C8(e, 0x40): the first shadow spawn —
             * approximated by a dark smoke drift (documented). */
            spawn_drift(AOS_EFFECT_DARK_SMOKE,
                        e->x + (e->mirrored ? -0x40000 : 0x40000), e->y,
                        spawned);
        }
        if (e->timer > 0x23) {
            e->step = 5;
            spawn_drift(AOS_EFFECT_DARK_SMOKE,
                        e->x + (e->mirrored ? -0x30000 : 0x30000), e->y,
                        spawned);
            spawn_drift(AOS_EFFECT_DARK_SMOKE,
                        e->x + (e->mirrored ? -0x50000 : 0x50000), e->y,
                        spawned);
            aos_audio_play_song(0x79);
        }
        break;
    }
    case 5:
        if (e->anim.frame <= 3) break;
        e->vx = e->mirrored ? 0x580000 : (int32_t)-0x160000;
        e->ax = 0;
        e->vy = 0x20000;
        e->ay = 0x2800;
        (void)ww_probe(e, layer, 0x20000);
        e->step = 6;
        e->vx = 0;
        break;
    case 6:
        if (!e->cycle_done) break;
        e->ax = 0;
        e->vy = 0x20000;
        e->ay = 0x2800;
        (void)ww_probe(e, layer, 0x20000);
        play_loop(e, kind->anims, 0, true);
        face_player(e, soma);
        e->timer = 0;
        e->step = 7;
        break;
    case 7: {
        uint8_t old = e->timer;
        e->timer = (uint8_t)(old + 1);
        if (old <= 0x2C) break;
        play_loop(e, kind->anims, 1, true);
        e->state = 0;
        e->step = 1;
        e->timer = 0;
        break;
    }
    default:
        break;
    }
    /* _0806F284: the attack transitions clear the + 0x59 bit-1 mark. */
    e->ww_bit = 0;
}

/* sub_08079294: state 2, the shared blinking vanish — identical to
 * the were family's port: ash every fourth timer tick, countdown to
 * the + 0x59 bit-8 removal. */
static void ww_vanish(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, AosHitReport *spawned,
                      uint32_t (*random)(void)) {
    (void)soma; (void)layer; (void)kind; (void)random;
    if ((e->timer & 3) == 0)
        spawn_drift(AOS_EFFECT_SMOKE, e->x, e->y - 0x40000, spawned);
    e->timer--;
    if ((int8_t)e->timer <= 0) e->removed = true;
}

/* sub_0806F41C: state 3, the hit leap. Step 0 faces away from the
 * player (vx -/+0x18000 by the + 0x58 bit 6), vy -0x24000, ay 0x1800;
 * step 1 lands on the floor bit into state 0 step 1 animation 1. */
static void ww_leap(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                    const AosEnemyKind *kind) {
    switch (e->step) {
    case 0:
        e->mirrored = ww_target_right(e, soma);
        e->vx = e->mirrored ? (int32_t)-0x18000 : 0x18000;
        e->ax = 0;
        e->vy = (int32_t)-0x24000;
        e->ay = 0x1800;
        e->step = 1;
        break;
    case 1:
        if (ww_probe(e, layer, 0x40000) & 4) {
            e->state = 0;
            e->step = 1;
            e->vx = 0;
            play_loop(e, kind->anims, 1, true);
        }
        break;
    default:
        break;
    }
}

AosHitReport aos_enemy_werewolf_update(AosEnemy *e, AosSoma *soma,
                                       const AosCollision *layer,
                                       const AosEnemyKind *kind,
                                       const AosWeaponEntity *weapon,
                                       const AosWeaponFrames *weapon_frames,
                                       int soma_atk, int soma_def, int cam_x,
                                       int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = ww_px(e) - cam_x, sy = ww_py(e) - cam_y;
    e->hidden = outside_margins(kind, 4, sx, sy);
    bool run = !e->hit_flash || e->state == 2 || e->state == 3;
    if (run) {
        switch (e->state) {
        case 0:
            ww_idle(e, soma, layer, kind);
            break;
        case 1:
            ww_attack(e, soma, layer, kind, &spawned, random);
            break;
        case 2:
            ww_vanish(e, soma, layer, kind, &spawned, random);
            break;
        case 3:
            ww_leap(e, soma, layer, kind);
            break;
        case 4:
            step_anim(e, kind->anims);
            break;
        default:
            break;
        }
        if (!e->removed && e->state != 4) step_anim(e, kind->anims);
    }
    if (run && !e->removed && e->state != 4 &&
        !outside_margins(kind, 4, sx, sy)) {
        report = collide(e, soma, kind, weapon, weapon_frames,
                         soma_atk, soma_def);
    }
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    for (int i = 0; i < spawned.effect_count &&
                    report.effect_count < AOS_ENEMY_MAX_EFFECTS; ++i)
        report.effects[report.effect_count++] = spawned.effects[i];
    return report;
}
