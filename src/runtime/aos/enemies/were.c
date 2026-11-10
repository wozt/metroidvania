/* Werejaguar (0x4B) and Weretiger (0x54): one native AI behind two
 * palette banks — sub_0807968C picks bank 0 of 0x0820A5C4 for id 0x4B
 * and bank 1 for id 0x54 over the shared tiles record of 0x081CBF7C
 * and frames 0x08216A40, and both creates (0x080796F0 / 0x08079874)
 * install the same contact sub_0807931C, hurt sub_08079458 and state
 * machine. EnemyWerejaguarUpdate (0x08078754) and
 * EnemyWeretigerUpdate (0x0807976C) dispatch the shared handlers:
 * state 0 sub_08078880 (idle and the attack trigger), state 1
 * sub_08078BB4 (the fourteen-substep pounce), state 2 sub_08079294
 * (the blinking vanish), state 3 sub_080791E8 (the leap) and state 4
 * animation only. */
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
#define spawn_drift aos_enemy_spawn_drift
#define collide aos_enemy_collide
#define outside_margins aos_enemy_outside_margins
#define accel_move aos_enemy_accel_move

static int16_t wr_px(const AosEnemy *e) { return (int16_t)(e->x >> 16); }
static int16_t wr_py(const AosEnemy *e) { return (int16_t)(e->y >> 16); }
static int16_t wr_tpx(const AosSoma *s) { return (int16_t)(aos_enemy_target_x(s) >> 16); }

/* sub_0806CF2C: nonzero when the player stands to the enemy's right. */
static int wr_target_right(const AosEnemy *e, const AosSoma *s) {
    return wr_tpx(s) > wr_px(e);
}

static int wr_dist(const AosEnemy *e, const AosSoma *s) {
    int32_t d = wr_tpx(s) - wr_px(e);
    return d < 0 ? -d : d;
}

/* The shared leap-setup block (native _08078C62 / _08078E20 /
 * _08078E94): timer and chain marks cleared, + 0x14 armed, direct
 * frame 0x10, facing refreshed, ax 0, vy 0x40000 (0x20000 for the
 * Weretiger step-0xD variant), ay 0x2800 and vx +/-0x20000 by facing.
 * `vy` and the vx sign follow each call site's literal constants. */
static void wr_leap_setup(AosEnemy *e, const AosEnemyKind *kind,
                          const AosSoma *soma, int vy, int vx_mag,
                          bool invert_vx) {
    (void)kind;
    e->timer = 0;
    e->were_flag = 0;
    e->were_attack = 1;
    e->were_chain = 0;
    e->static_frame = 0x10;
    face_player(e, soma);
    e->ax = 0;
    e->vy = vy;
    e->ay = 0x2800;
    e->vx = (e->mirrored != invert_vx) ? vx_mag : -vx_mag;
}

/* sub_08078880: state 0. Step 0 drops to the floor (vy 0x20000,
 * ay 0x2800) and stands; each floor frame the + 0x0D timer grows and
 * the attack fires once it passes 0x1D, or at once while the player's
 * + 0x10 flags carry any of 0x360. While the player is free the
 * Weretiger alone probes the 0x78x0x6E box at (px-0x3C, py-0x78) and
 * answers a hit with animation 4 step 0xA. Step 1 (the walk) re-arms
 * while sub_0806D128(4) stays clear, drops vy 0x20000 again and, on
 * animation frame 3 with the player inside the remembered-side box
 * (0x78 high for Werejaguar, 0x28 for Weretiger, offset -0x3C), loops
 * the animation and returns to step 0. */
static void wr_idle(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                    const AosEnemyKind *kind, uint32_t (*random)(void),
                    int cam_x, int cam_y) {
    switch (e->step) {
    case 0: {
        e->mirrored = wr_target_right(e, soma);
        e->vx = e->ax = 0;
        e->vy = 0x20000;
        e->ay = 0x2800;
        if (!(walk_collide(e, layer, 0x20000, (int32_t)0xFFE00000,
                           0x80000, 8) & 4))
            break;
        uint8_t old = e->timer;
        e->timer = (uint8_t)(old + 1);
        if (old > 0x1D || (soma->flags & 0x360)) {
            wr_leap_setup(e, kind, soma, 0x40000, 0x20000, false);
            e->step = 1;
            break;
        }
        if (e->id == AOS_ENEMY_WERETIGER &&
            player_in(soma, (int16_t)(wr_px(e) - 0x3C),
                      (int16_t)(wr_py(e) - 0x78), 0x78, 0x6E)) {
            play_loop(e, kind->anims, 4, false);
            e->step = 0xA;
        }
        break;
    }
    case 1: {
        /* _0807892C: sub_0806D128(4) clear keeps the approach; on
         * animation frame 3 with the player inside the box the attack
         * hop arms — the + 0x10 flags pick the dash, a free player
         * rolls the walk / long walk / feint. */
        if (aos_enemy_outside_margins(kind, 4, wr_px(e) - cam_x,
                                      wr_py(e) - cam_y)) {
            play_loop(e, kind->anims, 0, true);
            e->step = 0;
            break;
        }
        e->mirrored = wr_target_right(e, soma);
        e->vx = e->ax = 0;
        e->vy = 0x20000;
        e->ay = 0x2800;
        if (!(walk_collide(e, layer, 0x20000, (int32_t)0xFFE00000,
                           0x80000, 8) & 4))
            break;
        if (e->anim.frame != 3) break;
        int tall = (e->id == AOS_ENEMY_WEREJAGUAR) ? 0x78 : 0x28;
        if (player_in(soma, (int16_t)(wr_px(e) - 0x3C),
                      (int16_t)(wr_py(e) - tall), 0x78, tall * 2)) {
            /* _080789A8 probe hit: animation 0, state 1 step 0 with
             * the walk velocities re-armed. */
            play_loop(e, kind->anims, 0, true);
            e->state = 1;
            e->step = 0;
            e->timer = 0;
            e->vx = e->ax = 0;
            e->vy = 0x20000;
            e->ay = 0x2800;
            break;
        }
        /* _08078A00: the approach hop. Player flags & 0x360 close in
         * with +/-0x10000; a free player rolls rng & 3 — nonzero the
         * mirrored +/-0x10000 walk, zero the close feint (animation 3,
         * step 0) or the +/-0x8000 walk; past 0x4F/0x63 the speed
         * grows by half. */
        int d = wr_dist(e, soma);
        if ((soma->flags & 0x360) && d <= 0x4F) {
            e->vx = e->mirrored ? -0x10000 : 0x10000;
        } else if ((random() & 3) != 0 || d > 0x4F) {
            e->vx = e->mirrored ? 0x10000 : -0x10000;
        } else if (random() % 3 == 0) {
            play_loop(e, kind->anims, 3, false);
            e->step = 0;
            break;
        } else {
            e->vx = e->mirrored ? -0x8000 : 0x8000;
        }
        if (d > 0x63) e->vx += e->vx / 2;
        e->ax = 0;
        e->vy = (int32_t)0xFFFE8000;
        e->ay = 0x3000;
        e->step++;
        e->anim.flags |= 2;
        break;
    }
    case 2:
        e->anim.flags |= 2;
        if (!(walk_collide(e, layer, 0x20000, (int32_t)0xFFE00000,
                           0x80000, 8) & 4))
            break;
        e->anim.flags &= (uint8_t)~2;
        e->anim.tick = 0xFA;
        e->vx = e->ax = 0;
        e->vy = 0x20000;
        e->ay = 0x2800;
        e->state = 1;
        play_loop(e, kind->anims, 6, true);
        break;
    case 3:
        if (e->anim.tick != 0) break;
        if (e->anim.frame == 1 || e->anim.frame == 3) {
            if (!(e->cycle_done))
                aos_audio_play_song(0x85);
        }
        if (e->cycle_done) {
            play_loop(e, kind->anims, 6, true);
            e->step = 1;
        }
        break;
    default:
        break;
    }
}

/* sub_08078BB4: state 1, the pounce sequence. + 0x0B substeps:
 * 0 run-up leap, 1 approach, 2 probe, 3 attack anim, 4/5 the
 * follow-up choice, 6 second leap, 10 the Weretiger dash, 11 its
 * landing, 12/13 the recoveries. */
static void wr_pounce(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, AosHitReport *spawned,
                      uint32_t (*random)(void)) {
    (void)spawned;
    bool walked = true;
    switch (e->step) {
    case 0: {
        e->mirrored = wr_target_right(e, soma);
        if (!(walk_collide(e, layer, 0x20000, (int32_t)0xFFE00000,
                           0x80000, 8) & 4)) {
            walked = false;
            break;
        }
        uint8_t old = e->timer;
        e->timer = (uint8_t)(old + 1);
        if (old > 0x1D || (soma->flags & 0x360)) {
            wr_leap_setup(e, kind, soma, 0x40000, 0x20000, false);
            e->step = 1;
        } else if (e->id == AOS_ENEMY_WERETIGER &&
                   player_in(soma, (int16_t)(wr_px(e) - 0x3C),
                             (int16_t)(wr_py(e) - 0x78), 0x78, 0x6E)) {
            play_loop(e, kind->anims, 4, false);
            e->step = 0xA;
        }
        break;
    }
    case 1:
        e->were_attack = 1;
        if (!(walk_collide(e, layer, 0x40000, (int32_t)0xFFE00000,
                           0x80000, 8) & 4)) {
            walked = false;
            break;
        }
        {
            uint8_t old = e->timer;
            uint8_t now = (uint8_t)(old + 1);
            e->timer = now;
            if (old > 0x13 || (now > 3 && wr_dist(e, soma) <= 0x14)) {
                e->step = 2;
                e->vx = 0;
                e->static_frame = 0;
                e->timer = 0;
                face_player(e, soma);
            }
        }
        break;
    case 2:
        {
            uint8_t old = e->timer;
            e->timer = (uint8_t)(old + 1);
            if (old <= 5) break;
            if (player_in(soma, (int16_t)(wr_px(e) - 0x14),
                          (int16_t)(wr_py(e) - 0x28), 0x28, 0x50)) {
                if (wr_dist(e, soma) <= 0xC) {
                    /* _08078E20: the same leap setup, second run. */
                    wr_leap_setup(e, kind, soma, 0x40000, 0x20000, false);
                    e->step = 6;
                } else {
                    e->step = 3;
                    if (e->id == AOS_ENEMY_WEREJAGUAR) {
                        if (random() % 3 == 0)
                            play_loop(e, kind->anims, (random() & 1) ? 2 : 1,
                                      false);
                        else
                            play_loop(e, kind->anims, 3, false);
                    } else {
                        play_loop(e, kind->anims, 3, false);
                    }
                    e->were_attack = 0;
                }
            } else {
                e->were_attack = 0;
                play_loop(e, kind->anims, 6, true);
                e->state = 0;
                e->step = 1;
            }
        }
        break;
    case 3:
        if (wr_dist(e, soma) <= 0xC) {
            wr_leap_setup(e, kind, soma, 0x40000, 0x20000, false);
            e->step = 6;
            break;
        }
        if (!e->cycle_done) {
            e->were_element = 1;
            if (e->anim.tick == 0 &&
                (e->anim.frame == 1 || e->anim.frame == 3 ||
                 e->anim.frame == 5))
                aos_audio_play_song(0x85);
            break;
        }
        if (e->were_flag) {
            if (e->id == AOS_ENEMY_WEREJAGUAR) {
                e->step = 5;
                e->timer = 0;
                play_loop(e, kind->anims, 5, false);
            } else {
                e->timer = 0;
                e->step = 0xD;
                e->were_attack = 1;
                e->static_frame = 0x10;
                face_player(e, soma);
                e->ax = 0;
                e->vy = 0x20000;
                e->ay = 0x2800;
                e->vx = e->mirrored ? -0x20000 : 0x20000;
            }
        } else {
            e->were_element = 1;
            if (e->anim.tick != 0) break;
            if (e->anim.frame == 1 || e->anim.frame == 3 ||
                e->anim.frame == 5)
                aos_audio_play_song(0x85);
        }
        break;
    case 4:
        if (e->cycle_done) {
            play_loop(e, kind->anims, 6, true);
            e->state = 0;
            e->step = 1;
            break;
        }
        e->were_element = (e->anim.frame == 5) ? 2 : 1;
        if (e->anim.tick == 0 &&
            (e->anim.frame == 1 || e->anim.frame == 3 ||
             e->anim.frame == 5))
            aos_audio_play_song(0x85);
        break;
    case 5: {
        uint8_t old = e->timer;
        e->timer = (uint8_t)(old + 1);
        if (old <= 7) {
            e->were_element = 1;
            break;
        }
        if (e->id == AOS_ENEMY_WEREJAGUAR && !e->were_chain &&
            (random() & 3) == 0) {
            e->were_chain = 1;
            if (random() % 3 == 0)
                play_loop(e, kind->anims, (random() & 1) ? 2 : 1, false);
            else
                play_loop(e, kind->anims, 3, false);
            e->step = 3;
        } else {
            play_loop(e, kind->anims, (random() & 1) ? 2 : 1, true);
            e->step = 4;
        }
        break;
    }
    case 6:
        e->were_attack = 1;
        if (!(walk_collide(e, layer, 0x40000, (int32_t)0xFFE00000,
                           0x80000, 8) & 4)) {
            walked = false;
            break;
        }
        {
            uint8_t old = e->timer;
            e->timer = (uint8_t)(old + 1);
            if (old > 0xB) {
                e->step = 2;
                e->vx = 0;
                e->static_frame = 0;
                e->timer = 0;
                face_player(e, soma);
            }
        }
        break;
    case 0xA:
        /* The Weretiger dash: only from animation frame 1. */
        if (e->anim.frame != 1) break;
        e->step = 0xB;
        e->ax = e->ay = e->vy = 0;
        e->vx = e->mirrored ? 0x100000 : -0x100000;
        accel_move(e, 0x100000);
        e->were_element = 3;
        e->vx = e->mirrored ? 0x10000 : -0x10000;
        e->vy = (int32_t)0xFFFC0000;
        e->ay = 0x3000;
        aos_audio_play_song(0x86);
        spawn_drift(AOS_EFFECT_DUST, e->x, e->y, spawned);
        spawn_drift(AOS_EFFECT_DUST, e->x, e->y, spawned);
        break;
    case 0xB:
        e->were_element = 3;
        if (e->anim.frame == 3) {
            e->anim.flags |= 2;
            e->were_element = 0;
        }
        if (!(walk_collide(e, layer, 0x20000, (int32_t)0xFFE00000,
                           0x80000, 8) & 4))
            break;
        e->anim.flags &= (uint8_t)~2;
        e->anim.tick = 0xFA;
        e->vx = e->ax = 0;
        e->vy = 0x20000;
        e->ay = 0x2800;
        e->step++;
        e->timer = 0;
        break;
    case 0xC: {
        uint8_t old = e->timer;
        e->timer = (uint8_t)(old + 1);
        if (old > 0x1D) {
            play_loop(e, kind->anims, 6, true);
            e->state = 0;
            e->step = 1;
        }
        break;
    }
    case 0xD:
        e->were_attack = 1;
        if (!(walk_collide(e, layer, 0x40000, (int32_t)0xFFE00000,
                           0x80000, 8) & 4)) {
            walked = false;
            break;
        }
        {
            uint8_t old = e->timer;
            e->timer = (uint8_t)(old + 1);
            if (old > 3) {
                e->step = 5;
                e->timer = 0;
                play_loop(e, kind->anims, 0, false);
                e->were_attack = 0;
            }
        }
        break;
    default:
        break;
    }
    /* The shared walk_collide miss tail (_080791CC -> _080791DC): a
     * missed floor probe ends the attack — state 0, step 0,
     * animation 0, + 0x14 cleared. Each walking case breaks out on
     * the miss, so the flag is checked once here. */
    if (!walked && (e->step == 0 || e->step == 1 || e->step == 6 ||
                    e->step == 0xD)) {
        e->state = 0;
        e->step = 0;
        play_loop(e, kind->anims, 0, true);
        e->were_attack = 0;
    }
}

/* sub_08079294: state 2, the vanish. Every fourth timer tick spawns
 * the sub_08079510 ash at (x, y-4) seeded by timer*0x666 (approximated
 * by a smoke drift), the blink palette merge of sub_0806D518 is not
 * ported, and the timer counts down to the + 0x59 bit-8 removal. */
static void wr_vanish(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, AosHitReport *spawned,
                      uint32_t (*random)(void)) {
    (void)soma; (void)layer; (void)kind; (void)random;
    if ((e->timer & 3) == 0)
        spawn_drift(AOS_EFFECT_SMOKE, e->x, e->y - 0x40000, spawned);
    e->timer--;
    if ((int8_t)e->timer <= 0) e->removed = true;
}

/* sub_080791E8: state 3, the leap. Step 0 faces the player, arms
 * vx -/+0x18000, vy -0x24000 and ay 0x3000; step 1 lands on the floor
 * bit into state 0 step 1 with animation 6 and the walk velocities. */
static void wr_leap(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                    const AosEnemyKind *kind) {
    switch (e->step) {
    case 0:
        e->mirrored = wr_target_right(e, soma);
        e->vx = e->mirrored ? (int32_t)0xFFFE8000 : 0x18000;
        e->ax = 0;
        e->vy = (int32_t)0xFFFDC000;
        e->ay = 0x3000;
        e->step = 1;
        break;
    case 1:
        if (walk_collide(e, layer, 0x40000, (int32_t)0xFFE00000,
                         0x80000, 8) & 4) {
            e->state = 0;
            e->step = 1;
            play_loop(e, kind->anims, 6, true);
            e->vx = e->ax = 0;
            e->vy = 0x20000;
            e->ay = 0x2800;
        }
        break;
    default:
        break;
    }
}

AosHitReport aos_enemy_were_update(AosEnemy *e, AosSoma *soma,
                                   const AosCollision *layer,
                                   const AosEnemyKind *kind,
                                   const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames,
                                   int soma_atk, int soma_def, int cam_x,
                                   int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = wr_px(e) - cam_x, sy = wr_py(e) - cam_y;
    e->hidden = outside_margins(kind, 4, sx, sy);
    bool run = !e->hit_flash || e->state == 2 || e->state == 3;
    if (run) {
        switch (e->state) {
        case 0:
            wr_idle(e, soma, layer, kind, random, cam_x, cam_y);
            break;
        case 1:
            wr_pounce(e, soma, layer, kind, &spawned, random);
            break;
        case 2:
            wr_vanish(e, soma, layer, kind, &spawned, random);
            break;
        case 3:
            wr_leap(e, soma, layer, kind);
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
