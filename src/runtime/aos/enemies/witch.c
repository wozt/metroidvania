/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemy: Witch (enemy 0x2A) — the adult caster of the
 * library, Student Witch's stronger sibling.
 *
 * EnemyWitchCreate (0x080B9D90) / EnemyWitchUpdate (0x080B9E2C), the
 * state handlers sub_080B9614 (hover/aggro), sub_080B96DC (approach,
 * seven substates), sub_080B9B20 (attack, four substates), sub_080B9CF8
 * (death), the hurt sub_080B9F2C, the companion sub_080B93D8, the
 * orbit sparks sub_080B8ECC spawned through sub_080B8FD0, the homing
 * bolt sub_080B911C, the sparkles sub_080B94E4 / sub_080B9074 and the
 * shared bolt spawn sub_080B87C4. The witch shares the Student Witch's
 * resources (tiles 0x081E41C4, palette 0x0820B880, frames 0x08241790). */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#define collide aos_enemy_collide
#define outside_margins aos_enemy_outside_margins
#define pixel_delta aos_enemy_pixel_delta
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define player_in_centred aos_enemy_player_in_centred
#define spawn_drift aos_enemy_spawn_drift
#define step_anim aos_enemy_step_anim
#define accel_move aos_enemy_accel_move

/* The whole-pixel positions the native + 0x42/+ 0x46 fields cache. */
static int16_t w_px(const AosEnemy *e) { return (int16_t)(e->x >> 16); }
static int16_t w_py(const AosEnemy *e) { return (int16_t)(e->y >> 16); }
static int16_t w_tpx(const AosSoma *s) { return (int16_t)(aos_enemy_target_x(s) >> 16); }
static int16_t w_tpy(const AosSoma *s) { return (int16_t)(aos_enemy_target_y(s) >> 16); }

/* sub_0806CF2C: 1 when the target stands right of the entity. */
static int w_target_right(const AosEnemy *e, const AosSoma *s) {
    return w_px(e) < w_tpx(s);
}

/* The facing block the three handlers share: when the facing bit (+
 * 0x58 bit 6, `mirrored`) disagrees with the target's side and the
 * target stands within 8 whole pixels horizontally, the witch turns
 * (bit flip plus animation 1). Returns true when she turned. */
static bool w_face_check(AosEnemy *e, const AosSoma *s,
                         const AosEnemyKind *kind) {
    if ((e->mirrored ? 1 : 0) == w_target_right(e, s)) return false;
    int32_t dx = (int32_t)w_tpx(s) - w_px(e);
    if (dx < 0) dx = -dx;
    if (dx > 8) return false;
    e->mirrored = !e->mirrored;
    play_loop(e, kind->anims, 1, false);
    return true;
}

/* The + 0x50/+ 0x54 acceleration pair of the approach handlers: a
 * 0x1000 push only while the target sits more than 0x40000 (4 px) away
 * on that axis, nothing inside the dead zone. */
static void w_seek_accel(AosEnemy *e, int32_t tx, int32_t ty) {
    int32_t dx = tx - e->x, dy = ty - e->y;
    e->ax = dx > 0x40000 ? 0x1000 : dx < -0x40000 ? -0x1000 : 0;
    e->ay = dy > 0x40000 ? 0x1000 : dy < -0x40000 ? -0x1000 : 0;
}

/* sub_080B9560: the drifting sparkle (id 0x49, callback sub_080B94E4)
 * the handlers emit on alternating ticks. It parks beside the witch —
 * 0x10 px toward her facing plus a 0..3 roll on both axes — and inherits
 * half her horizontal speed with a small vertical drift. */
static void w_spawn_spark(AosEnemy *e, AosHitReport *spawned,
                          uint32_t (*random)(void)) {
    if (spawned->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy c = {0};
    c.id = e->id;
    c.role = AOS_ROLE_WITCH_SPARK;
    c.x = e->x + (int32_t)(e->mirrored ? -0x10 : 0x10) * 0x10000 +
              (int32_t)(random() & 3) * 0x10000;
    c.y = e->y + (int32_t)((int32_t)(random() & 3) - 4) * 0x10000;
    c.vx = -e->vx / 2;
    c.vy = ((-e->vy + (e->vy > 0 ? 3 : 0)) >> 2) +
           (int32_t)(random() & 7) * 0x4000;
    c.state = 0;
    c.stats = e->stats;
    c.hp = 1;
    c.static_frame = -1;
    c.parent = e;
    spawned->children[spawned->child_count++] = c;
}

/* sub_080B8FD0: the pair of orbit sparks (id 0x49, callback
 * sub_080B8ECC) emitted while the attack charge runs. `angle` is the
 * native i16 argument; the two sparks take angle and angle + 0x20. */
static void w_spawn_orbits(AosEnemy *e, int angle, AosHitReport *spawned) {
    for (int i = 0; i < 2; ++i) {
        if (spawned->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
        AosEnemy c = {0};
        c.id = e->id;
        c.role = AOS_ROLE_WITCH_ORBIT;
        c.angle = (uint16_t)angle;
        c.w_spin = 0xFC00;
        c.x = e->x + (int32_t)(e->mirrored ? 0x20 : -0x20) * 0x10000;
        c.y = e->y;
        /* The native + 0x50/+ 0x54 orbit centre is screen space; the
         * world position at spawn stands in for it. */
        c.ax = e->x;
        c.ay = e->y;
        c.state = 0;
        c.stats = e->stats;
        c.hp = 1;
        c.static_frame = -1;
        c.mirrored = e->mirrored;
        c.parent = e;
        angle += 0x20;
        spawned->children[spawned->child_count++] = c;
    }
}

/* sub_0806DFF8(sub_080B93D8): the companion orb — a second witch sprite
 * that waits invisible through the approach, hovers active through the
 * attack charge and dies with her. */
static void w_spawn_companion(AosEnemy *e, AosHitReport *spawned) {
    if (spawned->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy c = {0};
    c.id = e->id;
    c.role = AOS_ROLE_WITCH_COMPANION;
    c.x = e->x;
    c.y = e->y;
    c.state = 0;
    c.stats = e->stats;
    c.hp = 1;
    c.static_frame = -1;
    c.mirrored = e->mirrored;
    c.parent = e;
    spawned->children[spawned->child_count++] = c;
}

/* sub_080B911C through sub_0806DFF8: the homing bolt. It spawns 0x20 px
 * toward her facing and 0x10 px above her pivot with the song 0x12A. */
static void w_spawn_bolt(AosEnemy *e, AosHitReport *spawned) {
    if (spawned->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy c = {0};
    c.id = e->id;
    c.role = AOS_ROLE_WITCH_BOLT;
    c.x = e->x + (int32_t)(e->mirrored ? -0x20 : 0x20) * 0x10000;
    c.y = e->y - 0x10 * 0x10000;
    c.state = 0;
    c.stats = e->stats;
    c.hp = 1;
    c.static_frame = -1;
    c.mirrored = e->mirrored;
    c.parent = e;
    spawned->children[spawned->child_count++] = c;
}

/* sub_080B9614: state 0, the hover/aggro. While the animation sits on
 * frame 0 tick 0 and the player stands in the centred box (0xDC x 0xA0,
 * grown to 0x190 x 0xF0 once bit 2 of + 0x14 latches) she latches bit 2,
 * then — facing check, a one-in-two roll and the player within 0x1F of
 * pivot + 0x30 — dives into state 2, else state 1. */
static void w_hover(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                    uint32_t (*random)(void)) {
    (void)kind;
    if (e->anim.frame != 0 || e->anim.tick != 0) return;
    if (!player_in_centred(e, soma, (e->variant & 4) ? 0x190 : 0xDC,
                           (e->variant & 4) ? 0xF0 : 0xA0))
        return;
    e->variant |= 4;
    int pright = w_target_right(e, soma);
    /* RandomNumberGenerator & 2 gates the dive. */
    if ((e->mirrored ? 1 : 0) != pright || !(random() & 2)) {
        e->state = 1;
        e->step = e->substep = 0;
        return;
    }
    int32_t dy = (int32_t)w_tpy(soma) - (w_py(e) + 0x30);
    if (dy < 0) dy = -dy;
    e->state = dy <= 0x1F ? 2 : 1;
    e->step = e->substep = 0;
}

/* sub_080B96DC case 1 tail shared by cases 1-3: the friction +
 * acceleration + integrate trio (sub_0806D490 / sub_0806D4A8 /
 * sub_0806D3D8). */
static void w_fly(AosEnemy *e, int32_t limit, int frac_bits) {
    e->vx = e->vx * ((1 << frac_bits) - 1) / (1 << frac_bits);
    e->vy = e->vy * ((1 << frac_bits) - 1) / (1 << frac_bits);
    accel_move(e, limit);
}

/* sub_080B96DC: state 1, the approach — seven substates. Case 0 arms
 * the hover tick and zeroes the velocities; case 1 steers toward a
 * point beside the player (x offset by the target side, y by a sine
 * bob on the tick) until the player closes in (step 3) or the facing
 * check sends her down (step 2); case 2 dives at a point 0x38 px below
 * the player until the animation ends; case 3 hovers out the 0x20-tick
 * pause; case 4 waits for the landing animation; case 5 runs the
 * 0x40-tick charge — orbit sparks above 0x30, the bolt at 0x20; case 6
 * waits for the recovery animation and returns to state 0. */
static void w_approach(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                       AosHitReport *spawned, uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        e->spin = 0;
        e->vx = e->vy = 0;
        e->step = 1;
        break;
    case 1: {
        e->spin = (e->spin + 1) & 0xFFFF;
        int32_t xoff = (w_target_right(e, soma) ? -0x20 : 0x20) * 0x10000;
        int32_t yoff = (int32_t)aos_sine((uint32_t)(e->spin & 0xFFFF) << 9) * 8 -
                       0x300000;
        int32_t tx = aos_enemy_target_x(soma) + xoff;
        int32_t ty = aos_enemy_target_y(soma) + yoff;
        e->target_x = tx;
        e->target_y = ty;
        int32_t dx = tx - e->x, dy = ty - e->y;
        w_seek_accel(e, tx, ty);
        w_fly(e, 0x28000, 5);
        int32_t adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
        if ((adx <= 0x3FFFF && ady <= 0x3FFFF) ||
            player_in_centred(e, soma, 0x50, 0x36)) {
            e->timer = 0x20;
            e->step = 3;
            e->substep = 0;
            break;
        }
        if (w_face_check(e, soma, kind)) {
            e->step = 2;
            break;
        }
        if (e->spin & 1) w_spawn_spark(e, spawned, random);
        break;
    }
    case 2: {
        int32_t tx = aos_enemy_target_x(soma) +
                     (w_target_right(e, soma) ? -0x20 : 0x20) * 0x10000;
        int32_t ty = aos_enemy_target_y(soma) - 0x380000;
        w_seek_accel(e, tx, ty);
        w_fly(e, 0x28000, 5);
        if (e->anim.flags & AOS_ANIM_ENDED) {
            play_loop(e, kind->anims, 0, true);
            e->step = 1;
        }
        break;
    }
    case 3: {
        e->vx = e->vx * 63 / 64;
        e->vy = e->vy * 63 / 64;
        e->x += e->vx;
        e->y += e->vy;
        if (!e->substep && e->timer > 8 && w_face_check(e, soma, kind))
            e->substep = 1;
        if (!e->timer) {
            play_loop(e, kind->anims, 2, false);
            e->step = 4;
        } else {
            e->timer--;
        }
        break;
    }
    case 4:
        if (e->anim.flags & AOS_ANIM_ENDED) {
            e->timer = 0x40;
            e->step = 5;
            play_loop(e, kind->anims, 3, true);
            e->variant |= 2;
            w_spawn_companion(e, spawned);
            aos_audio_play_song(0x81);
        }
        break;
    case 5:
        if (!e->timer) {
            e->variant &= 0xFD;
            play_loop(e, kind->anims, 2, false);
            e->step = 6;
            break;
        }
        if (e->timer > 0x30) {
            /* (timer << 10 | rand & 0xF) << 20 >> 16, as i16. */
            uint32_t a = ((uint32_t)e->timer << 10) + (random() & 0xF);
            w_spawn_orbits(e, (int16_t)((a << 20) >> 16), spawned);
        }
        if (e->timer == 0x20) w_spawn_bolt(e, spawned);
        e->timer--;
        break;
    case 6:
        if (e->anim.flags & AOS_ANIM_ENDED) {
            play_loop(e, kind->anims, 0, true);
            e->state = 0;
            e->step = e->substep = 0;
        }
        break;
    default:
        break;
    }
}

/* sub_080B9B20: state 2, the attack — four substates. Case 0 arms the
 * 0x40-tick charge timer, the 0x18000 lunge (negated while mirrored)
 * with -0x600 decel, flags bits 0-1 and the companion; case 1 runs the
 * x-only accel move until the timer dies, then slams down (vy 0x1C0000,
 * ay -0xA00) with the 0x50/0xC smoke; case 2 integrates with the
 * 0x20000 limit, sparkling on odd ticks, into the 0x20 recovery; case 3
 * drifts home and returns to state 0. */
static void w_attack(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                     AosHitReport *spawned, uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        e->timer = 0x40;
        e->vx = 0x18000;
        e->ax = -0x600;
        if (e->mirrored) {
            e->vx = -e->vx;
            e->ax = -e->ax;
        }
        e->variant |= 3;
        w_spawn_companion(e, spawned);
        e->step = 1;
        break;
    case 1:
        e->vx += e->ax;
        if (e->ax >= 0 ? e->vx > 0x20000 : e->vx < -0x20000)
            e->vx = e->ax >= 0 ? 0x20000 : -0x20000;
        e->x += e->vx;
        if (!e->timer) {
            e->vx = -0x38000;
            e->ax = 0x600;
            e->vy = 0x1C0000;
            e->ay = -0xA00;
            if (e->mirrored) {
                e->vx = -e->vx;
                e->ax = -e->ax;
            }
            e->timer = 0xE0;
            e->step = 2;
            play_loop(e, kind->anims, 0xC, true);
            spawn_drift(AOS_EFFECT_SMOKE, e->x, e->y, spawned);
            spawn_drift(AOS_EFFECT_SMOKE, e->x, e->y, spawned);
        } else {
            e->timer--;
        }
        break;
    case 2:
        accel_move(e, 0x20000);
        if (e->timer & 1) w_spawn_spark(e, spawned, random);
        if (!e->timer) {
            e->variant &= 0xFC;
            e->timer = 0x20;
            e->step = 3;
        } else {
            e->timer--;
        }
        break;
    case 3:
        if (!e->substep && w_face_check(e, soma, kind)) e->substep = 1;
        e->vx = e->vx * 31 / 32;
        e->vy = e->vy * 31 / 32;
        e->x += e->vx;
        e->y += e->vy;
        if (!--e->timer) {
            play_loop(e, kind->anims, 0, true);
            e->state = 0;
            e->step = e->substep = 0;
        }
        break;
    default:
        break;
    }
}

/* sub_080B9CF8: state 3, the death — entered by the hurt at step 1.
 * Even ticks of the 0x20 counter fling the animation-5/row-6 spark on
 * the sine offset; at zero the shared bolt spawn (param 1) and the
 * handler reports the delete. */
static void w_death(AosEnemy *e, AosHitReport *spawned) {
    switch (e->step) {
    case 0:
        e->step = 1;
        e->w_active = true;
        e->timer = 0x20;
        break;
    case 1:
        if (!(e->timer & 1)) {
            int32_t ox = (int32_t)aos_sine((uint32_t)e->timer << 13) * 8;
            int32_t oy = (int32_t)(4 - (e->timer >> 1)) << 16;
            AosEffect *fx = spawn_drift(AOS_EFFECT_SPARK, e->x + ox,
                                        e->y + oy, spawned);
            if (fx) {
                fx->vy = -0x18000;
                fx->ay = 0x800;
            }
        }
        if (e->timer) {
            e->timer--;
        } else {
            e->step = 2;
        }
        break;
    case 2: {
        /* sub_080B87C4(e, 0, -0x10, 1): the shared witch bolt. */
        if (spawned->child_count < AOS_ENEMY_MAX_CHILDREN) {
            AosEnemy c = {0};
            c.id = e->id;
            c.role = AOS_ROLE_STUDENT_WITCH_BOLT;
            c.x = e->x;
            c.y = e->y - 0x10 * 0x10000;
            c.step = 1;
            c.state = 0;
            c.mirrored = e->mirrored;
            c.stats = e->stats;
            c.hp = 1;
            c.static_frame = -1;
            c.parent = e;
            c.vy = 0;
            c.ay = 0x4000;
            spawned->children[spawned->child_count++] = c;
        }
        e->removed = true;
        break;
    }
    default:
        break;
    }
}

/* EnemyWitchUpdate (0x080B9E2C): the standard preamble — the + 0x58
 * bit-5 visibility follows sub_0806D128(4), the sub_08021924 activity
 * mark and the sub_0806AD24 stun gate (the sub_080421AC room query is
 * not ported) — then the state jump: 0 hover, 1 approach, 2 attack,
 * 3 death, 4 inert; the sub_0806DF20 tail is the shared collide pass
 * (state 4 skips it entirely, state 3 skips only the room query). */
AosHitReport aos_enemy_witch_update(AosEnemy *e, AosSoma *soma,
                                    const AosCollision *layer,
                                    const AosEnemyKind *kind,
                                    const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames,
                                    int soma_atk, int soma_def, int cam_x,
                                    int cam_y, uint32_t (*random)(void)) {
    (void)cam_x; (void)cam_y; (void)layer;
    AosHitReport report = {0}, spawned = {0};
    bool run = !(e->hp > 0 && e->hit_flash);
    if (run) {
        switch (e->state) {
        case 0:
            w_hover(e, soma, kind, random);
            break;
        case 1:
            w_approach(e, soma, kind, &spawned, random);
            break;
        case 2:
            w_attack(e, soma, kind, &spawned, random);
            break;
        case 3:
            w_death(e, &spawned);
            break;
        default:
            break;
        }
        if (!e->removed) step_anim(e, kind->anims);
    }
    if (e->state != 4 && !e->removed) {
        int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
        if (!outside_margins(kind, 4, sx, sy)) {
            report = collide(e, soma, kind, weapon, weapon_frames,
                             soma_atk, soma_def);
        }
    }
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    for (int i = 0; i < spawned.effect_count &&
                    report.effect_count < AOS_ENEMY_MAX_EFFECTS; ++i)
        report.effects[report.effect_count++] = spawned.effects[i];
    return report;
}

/* sub_080B93D8: the companion orb. Case 0 loads the shared witch art,
 * animation 5 and the + 0xF active mark with a 0x10 timer; case 1 waits
 * it out (mark cleared on the way to case 2); case 2 holds while the
 * parent's + 0x14 bit 1 stands, then arms a 0x10 fade; case 3 counts it
 * down to the delete. While its state stays at or under 2 it mirrors
 * the parent's facing and position and adopts state 3 when the parent
 * dies. */
AosHitReport aos_enemy_witch_companion_update(AosEnemy *e,
                                              const AosEnemyKind *kind) {
    AosHitReport report = {0};
    switch (e->state) {
    case 0:
        play_loop(e, kind->anims, 5, true);
        e->w_active = true;
        e->timer = 0x10;
        e->state = 1;
        break;
    case 1:
        if (!e->timer) {
            e->w_active = false;
            e->state = 2;
        } else {
            e->timer--;
        }
        break;
    case 2:
        if (!(e->parent && (e->parent->variant & 2))) {
            e->state = 3;
            e->timer = 0x10;
        }
        break;
    case 3:
        e->w_active = true;
        if (!e->timer) {
            e->removed = true;
        } else {
            e->timer--;
        }
        break;
    default:
        break;
    }
    step_anim(e, kind->anims);
    if (e->state <= 2 && e->parent) {
        if (e->parent->state == 3) {
            e->state = 3;
            e->timer = 0x10;
        } else {
            e->mirrored = e->parent->mirrored;
            e->x = e->parent->x;
            e->y = e->parent->y;
        }
    }
    return report;
}

/* sub_080B8ECC: the orbit spark. Case 0 arms animation 0xD, the 0x20
 * timer and the 0x500000 orbit energy; case 1 shrinks the energy by the
 * accelerating -0x2800 step, places the spark on the sine/cosine of its
 * angle around the stored centre (the native screen-space base is
 * approximated by the world position captured at spawn) and spins it
 * by + 0x16; the timer's 0x10 tick raises the + 0xF mark and zero
 * deletes. */
AosHitReport aos_enemy_witch_orbit_update(AosEnemy *e,
                                          const AosEnemyKind *kind) {
    AosHitReport report = {0};
    switch (e->state) {
    case 0:
        play_loop(e, kind->anims, 0xD, false);
        e->timer = 0x20;
        e->speed = 0x500000;
        e->state = 1;
        break;
    case 1: {
        int16_t radius = (int16_t)(e->speed >> 16);
        int16_t angle = (int16_t)e->angle;
        e->x = e->ax + aos_sine((uint32_t)(uint16_t)(angle + 0x4000)) *
               radius;
        e->y = e->ay + aos_sine((uint32_t)(uint16_t)angle) * radius;
        e->vx -= 0x2800;
        e->speed += e->vx;
        e->angle = (uint16_t)(angle + (int16_t)e->w_spin);
        step_anim(e, kind->anims);
        if (!e->timer) {
            e->removed = true;
        } else {
            if (e->timer == 0x10) e->w_active = true;
            e->timer--;
        }
        break;
    }
    default:
        break;
    }
    return report;
}

/* sub_080B94E4: the drifting sparkle — animation 0xD, a 0x20-frame
 * life through sub_0806D3D8(8.0), deleted when the animation ends. */
AosHitReport aos_enemy_witch_spark_update(AosEnemy *e,
                                          const AosCollision *layer,
                                          const AosEnemyKind *kind) {
    (void)layer;
    AosHitReport report = {0};
    switch (e->state) {
    case 0:
        play_loop(e, kind->anims, 0xD, false);
        e->timer = 8;
        e->state = 1;
        break;
    case 1:
        accel_move(e, 0x80000);
        step_anim(e, kind->anims);
        if (e->anim.id == 0xD && e->cycle_done) e->removed = true;
        break;
    default:
        break;
    }
    return report;
}

/* sub_080B9074: the bolt's trail sparkle — animation 0xE, the 6-frame
 * launch, a 4-frame fade (+ 0xF) and the delete. */
AosHitReport aos_enemy_witch_trail_update(AosEnemy *e,
                                          const AosCollision *layer,
                                          const AosEnemyKind *kind) {
    (void)layer;
    AosHitReport report = {0};
    switch (e->state) {
    case 0:
        play_loop(e, kind->anims, 0xE, false);
        e->timer = 6;
        e->state = 1;
        break;
    case 1:
        accel_move(e, 0x80000);
        if (!e->timer) {
            e->w_active = true;
            e->timer = 4;
            e->state = 2;
        } else {
            e->timer--;
        }
        break;
    case 2:
        accel_move(e, 0x80000);
        if (!e->timer) e->removed = true;
        else e->timer--;
        break;
    default:
        break;
    }
    return report;
}

/* sub_080B8E14: the bolt's steering — the + 0x50/+ 0x54 pushes toward
 * the stored target, the 31/32 friction and the 0x28000-limited move,
 * reporting arrival inside 0x7FFFF on both axes. The native camera
 * offsets cancel in world space. */
static bool w_bolt_steer(AosEnemy *e) {
    int32_t dx = e->target_x - e->x, dy = e->target_y - e->y;
    e->ax = dx > 0x40000 ? 0x1000 : dx < -0x40000 ? -0x1000 : 0;
    e->ay = dy > 0x40000 ? 0x1000 : dy < -0x40000 ? -0x1000 : 0;
    e->vx = e->vx * 31 / 32;
    e->vy = e->vy * 31 / 32;
    accel_move(e, 0x28000);
    int32_t adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    return adx <= 0x7FFFF && ady <= 0x7FFFF;
}

/* sub_080B911C: the homing bolt. Case 0 picks a random low-nibble
 * shade, animation 4 and the 0x12A song, arms the -0x18000 launch
 * (negated while mirrored) with the 0xFD00 spin; case 1 ramps the
 * + 0x10 speed by 0x2000 to 0xFFFF then locks the target 16 px above
 * the player's feet with a 0x20-tick timer; case 2 steers until
 * arrival or timeout; case 3 drifts on velocity alone; case 4 fades
 * out of frame. The tail counts + 0xD, emits a trail sparkle every
 * other tick, spins + 0x14 by + 0x16 and steps the animation. */
AosHitReport aos_enemy_witch_bolt_update(AosEnemy *e, AosSoma *soma,
                                         const AosCollision *layer,
                                         const AosEnemyKind *kind,
                                         const AosWeaponEntity *weapon,
                                         const AosWeaponFrames *weapon_frames,
                                         int soma_atk, int soma_def, int cam_x,
                                         int cam_y, uint32_t (*random)(void)) {
    (void)weapon_frames; (void)cam_y; (void)layer;
    AosHitReport report = {0}, spawned = {0};
    switch (e->state) {
    case 0:
        e->variant = (uint8_t)(random() & 0x1F);
        play_loop(e, kind->anims, 4, true);
        e->combat.hurt_off = true;
        e->vx = -0x18000;
        e->w_spin = 0xFD00;
        if (e->mirrored) {
            e->vx = -e->vx;
            e->w_spin = (uint16_t)-e->w_spin;
        }
        e->target_x = aos_enemy_target_x(soma);
        e->target_y = aos_enemy_target_y(soma);
        aos_audio_play_song(0x12A);
        e->state = 1;
        break;
    case 1:
        if (e->speed <= 0xFFFF) {
            e->speed += 0x2000;
            break;
        }
        e->target_x = aos_enemy_target_x(soma);
        e->target_y = aos_enemy_target_y(soma) - 0x100000;
        e->spin = 0x20;
        e->state = 2;
        break;
    case 2:
        if (w_bolt_steer(e)) {
            e->state = 3;
        } else if (e->spin == 0) {
            e->state = 3;
        } else {
            e->spin--;
        }
        break;
    case 3:
        e->x += e->vx;
        e->y += e->vy;
        break;
    case 4:
        e->removed = true;
        break;
    default:
        break;
    }
    if (e->state <= 3) {
        step_anim(e, kind->anims);
        if (outside_margins(kind, 3, (int)(e->x >> 16) - cam_x,
                            (int)(e->y >> 16) - cam_y)) {
            e->state = 4;
        }
        if ((++e->timer & 1) && spawned.child_count < AOS_ENEMY_MAX_CHILDREN) {
            /* sub_080B9074: the trail sparkle at a jittered offset with
             * a quarter of the bolt's velocity. */
            AosEnemy c = {0};
            c.id = e->id;
            c.role = AOS_ROLE_WITCH_TRAIL;
            c.x = e->x + (int32_t)(random() & 0x1F) * 0x10000;
            c.y = e->y + (int32_t)(random() & 0x1F) * 0x10000;
            c.vx = e->vx / 4 + (int32_t)(random() & 0x1FFF);
            c.vy = e->vy / 4 - 0x4000 + (int32_t)(random() & 0x3FF);
            c.state = 0;
            c.stats = e->stats;
            c.hp = 1;
            c.static_frame = -1;
            c.parent = e;
            spawned.children[spawned.child_count++] = c;
        }
        e->angle = (uint16_t)((int16_t)e->angle + (int16_t)e->w_spin);
    }
    if (e->state <= 3 && !e->removed) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        aos_enemy_merge_hit(&report, hit);
        for (int i = 0; i < spawned.child_count &&
                        report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
            report.children[report.child_count++] = spawned.children[i];
        for (int i = 0; i < spawned.effect_count &&
                        report.effect_count < AOS_ENEMY_MAX_EFFECTS; ++i)
            report.effects[report.effect_count++] = spawned.effects[i];
        return report;
    }
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    for (int i = 0; i < spawned.effect_count &&
                    report.effect_count < AOS_ENEMY_MAX_EFFECTS; ++i)
        report.effects[report.effect_count++] = spawned.effects[i];
    return report;
}
