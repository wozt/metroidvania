/* Slime (0x44): the hopping slime of the castle. Its native routines
 * are EnemySlimeCreate (0x080C11C0) and EnemySlimeUpdate (0x080C1348)
 * dispatching sub_080C0A1C (state 0 idle/hop decision), sub_080C0F14
 * (state 1 the five-substep attack hop), sub_080C10BC (state 2 the
 * spin death) and sub_0803F17C (state 3, inert). The contact
 * sub_080C13DC feeds sub_08021654 and the hurt sub_080C140C the
 * shared damage walk; both are wired in aos_enemy.c. The hop rides a
 * steering angle (+ 0x20) turned toward the launch table
 * 0x08528922 by sub_080686CC while the power (+ 0x14) carries it —
 * a curved bound that lands when the angle arrives. */
#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../math/aos_math.h"
#include "../core/aos_collision.h"

#define player_in aos_enemy_player_in
#define player_in_centred aos_enemy_player_in_centred
#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define step_anim aos_enemy_step_anim
#define face_player aos_enemy_face_player
#define snap_to_floor aos_enemy_snap_to_floor
#define target_x aos_enemy_target_x
#define collide aos_enemy_collide
#define outside_margins aos_enemy_outside_margins
#define spawn_drift aos_enemy_spawn_drift
#define ground_walk aos_enemy_ground_walk
#define make_debris aos_enemy_make_debris
#define sine aos_sine
#define arctan2 aos_arctan2

static int16_t sl_px(const AosEnemy *e) { return (int16_t)(e->x >> 16); }
static int16_t sl_py(const AosEnemy *e) { return (int16_t)(e->y >> 16); }
static int16_t sl_tpx(const AosSoma *s) { return (int16_t)(aos_enemy_target_x(s) >> 16); }

/* 0x08528922: the four hop launch angles, one per direction index. */
static const uint16_t sl_angles[4] = {0x0000, 0xC000, 0x8000, 0x4000};
/* 0x08528914: the state-1 wall probe set of sub_0806CAF8 mode 0x22. */
static const AosProbes sl_probes = {3, -4, 5, 5, {-1, 0, 1}};

static int sl_target_right(const AosEnemy *e, const AosSoma *s) {
    return sl_tpx(s) > sl_px(e);
}

/* sub_080009E4 scaled: the native reads sine(x) >> 8 * power >> 8,
 * i.e. the 16.16 product — (a >> 8) * (b >> 8). */
static int32_t sl_vel(uint32_t angle, int32_t power) {
    return (sine(angle) >> 8) * (power >> 8);
}

/* A sine pixel offset: sine(angle) * n in whole pixels (16.16). */
static int32_t sl_off(uint32_t angle, int n) {
    return (int32_t)(((int64_t)sine(angle) * n) >> 16);
}

/* sub_080686CC(&angle, step, target): turns the u16 angle toward the
 * target by up to `step`, clamps on arrival and returns 1 once the
 * angle stands on the target. */
static int sl_steer(uint16_t *angle, uint16_t step, uint16_t target) {
    int32_t diff = (int32_t)(int16_t)(*angle - target);
    int dir = diff == 0 ? 0 : (diff < 0 ? 1 : -1);
    /* The native compares the masked halves; the sign of the plain
     * difference picks the turn direction. */
    if (diff == 0) return 1;
    if (dir > 0) {
        *angle = (uint16_t)(*angle + step);
        if ((int32_t)(int16_t)(*angle - target) >= 0) { *angle = target; return 1; }
    } else {
        *angle = (uint16_t)(*angle - step);
        if ((int32_t)(int16_t)(*angle - target) <= 0) { *angle = target; return 1; }
    }
    return 0;
}

/* sub_080C04C8(e, count, vx_base, vy_base): `count` debris chunks of
 * the kind-0x49 set — our AOS_ROLE_DEBRIS pieces on the slime's own
 * art — flung with a random spread around the bases. The native
 * + 0x65 lifetime draw and the sub_08068AD4 wall mark are folded
 * into the piece timer and the + 0x59 bit-3 mark (approximated). */
static void sl_debris(AosEnemy *e, const AosAnimSet *anims, int count,
                      int32_t vx_base, int32_t vy_base,
                      AosHitReport *spawned, uint32_t (*random)(void)) {
    for (int i = 0; i < count && spawned->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy d = make_debris(e, anims, 0);
        d.x = e->x + (int32_t)((random() & 0xF) << 13);
        d.y = e->y + (int32_t)((random() & 0xF) << 13);
        d.vx = vx_base + (int32_t)((random() & 0x800) << 10);
        d.vy = vy_base + (int32_t)((random() & 0x400) << 9);
        d.ay = 0x4000;
        d.timer = (uint8_t)((random() & 3) + 7);
        spawned->children[spawned->child_count++] = d;
    }
}

/* sub_080C0630(e, idx, speed, astep): picks hop direction `idx` of
 * 0x08528922, sets the power to a quarter of the masked angle
 * distance from + 0x20 plus `speed`, the approach step to `astep`
 * (or power >> 4), flips the sign with the old power and lifts off
 * (+ 0xB = 1). */
static void sl_launch(AosEnemy *e, int idx, int32_t speed, uint16_t astep) {
    e->sl_dir = (uint8_t)idx;
    int32_t a = (int32_t)(int16_t)e->sl_angle & 0x7FFF;
    int32_t b = (int32_t)sl_angles[idx & 3] & 0x7FFF;
    int32_t diff = a > b ? a - b : b - a;
    int32_t old = e->sl_power;
    e->sl_power = diff / 4;
    e->sl_astep = astep ? astep : (uint16_t)(e->sl_power >> 4);
    e->sl_power += speed;
    if (old < 0) e->sl_power = -e->sl_power;
    e->step = 1;
}

/* sub_080C05D8(x, y): the special-cell probe — the native reads the
 * room cell byte (gUnk_03002CB0 + 0x100D) and answers the floor
 * offset or 0. Our cell byte's bit 1 marks the same special floor;
 * the 0xC0/0x8 slope cases are approximated by the plain test. */
static int sl_special(const AosCollision *layer, int32_t x, int32_t y) {
    return (aos_collision_cell(layer, x, y) & 2) ? 1 : 0;
}

/* The + 0x6C bit-2 floor mark of the native probes: the floor depth
 * answers nonzero (negative) once the body stands on or over a floor
 * cell — the wait gate of state-1 cases 1 and 4. */
static int sl_grounded(const AosEnemy *e, const AosCollision *layer) {
    return aos_floor_depth(layer, sl_px(e), sl_py(e) + 8, 0, false) != 0;
}

/* sub_080C06A8(e, step): the predictive landing probe. It builds the
 * + 0x24..+ 0x2A box from the hop angle's sine offsets around the
 * room position, nudges the box and the entity out of special cells,
 * ceiling depths and wall pushes quadrant by quadrant (+ 0xC),
 * steers + 0x20 toward the box heading by `step` through
 * sub_080686CC and answers the blocked mark. The native's four
 * quadrant probe orders collapse into the same edge tests here;
 * the exact per-quadrant probe choice is approximated (documented). */
static int sl_predict(AosEnemy *e, const AosCollision *layer, uint16_t step) {
    int16_t angle = (int16_t)e->sl_angle;
    int32_t rx = sl_px(e), ry = sl_py(e);
    int blocked = 0;
    /* sine(angle + 0x4000) * 6 is the horizontal lead, sine(angle) * 6
     * the vertical one; the far corner reuses them negated. */
    int32_t lx = sl_off((uint16_t)(angle + 0x4000), 6);
    int32_t ly = sl_off((uint16_t)angle, 6);
    if (e->sl_power <= 0) { lx = -lx; ly = -ly; }
    int16_t x0 = (int16_t)(rx + lx), y0 = (int16_t)(ry + ly);
    int16_t x1 = (int16_t)(rx - lx), y1 = (int16_t)(ry - ly);
    e->sl_box[0] = x0; e->sl_box[1] = y0;
    e->sl_box[2] = x1; e->sl_box[3] = y1;
    /* Quadrant 0 of the native switch: special cells push + 0x46 up,
     * floor depths settle + 0x26/+ 0x2A and wall pushes widen the
     * box — each miss marks the probe blocked. */
    int sp = sl_special(layer, x0, y0 + 6);
    if (sp) {
        e->y += (int32_t)((uint16_t)(sl_py(e) + 6 + 0) << 16) - e->y;
        blocked = 1;
    } else blocked = 1;
    int fd = aos_floor_depth(layer, x0, y0 + 6, 0, false);
    if (fd) e->sl_box[1] = (int16_t)(e->sl_box[1] + fd);
    fd = aos_floor_depth(layer, x1, y1 - 6, 0, false);
    if (fd) e->sl_box[3] = (int16_t)(e->sl_box[3] + fd);
    int wp = aos_wall_push_right(layer, x0 + 6, y0);
    if (wp) { e->x += (int32_t)((uint16_t)(sl_px(e) + 6 + wp) << 16) - e->x; blocked = 1; }
    else blocked = 1;
    wp = aos_wall_push_left(layer, x1 - 6, y1);
    if (wp) { e->sl_box[2] = (int16_t)(e->sl_box[2] + wp); }
    /* The heading of the predicted box steers the hop angle. */
    int32_t dx = (int32_t)e->sl_box[2] - e->sl_box[0];
    int32_t dy = (int32_t)e->sl_box[1] - e->sl_box[3];
    if (e->sl_power <= 0) { dx = -dx; dy = -dy; }
    uint16_t heading = arctan2((int16_t)dx, (int16_t)dy);
    sl_steer(&e->sl_angle, step, heading);
    return blocked;
}

/* sub_080C0A1C state 0: the grounded hop decision. Step 0 faces the
 * player (sub_0806CF2C -> + 0x58 bit 6), arms the hop power toward
 * the facing, charges + 0x18, counts + 0x12 down and, rested with
 * substep 0, rolls RandomNumberGenerator & 0xFF <= 3 inside the
 * centred 0x78 x 0xB4 box (sub_0806E29C) into state 1. The shared
 * tail then drifts, predicts the landing (sub_080C06A8) and picks
 * the next bound through sub_080C0630; step 1 is the flight itself. */
static void sl_state0(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, AosHitReport *spawned,
                      uint32_t (*random)(void)) {
    if (e->step == 1) {
        /* _080C0AF0: the flight — velocity = power * dir(angle),
         * integrate, bleed the charge and steer + 0x20 toward the
         * launch table entry of + 0x13 until sub_080686CC lands it. */
        uint32_t a = (uint16_t)e->sl_angle;
        e->vx = sl_vel((uint16_t)(a + 0x4000), e->sl_power);
        e->vy = sl_vel(a, e->sl_power);
        e->x += e->vx;
        e->y += e->vy;
        /* The integrate's + 0x6C bits: bit 2 marks the floor contact
         * the hurt squash waits on. A floor cell under the body ends
         * the bound — the native landing of the curved hop. */
        int fd = aos_floor_depth(layer, sl_px(e), sl_py(e), 0, false);
        e->sl_coll = fd ? 4 : 0;
        if (e->sl_charge > 0x8000) e->sl_charge -= 0x200;
        if (fd || sl_steer(&e->sl_angle, e->sl_astep,
                           sl_angles[e->sl_dir & 3])) {
            if (fd) e->y += (int32_t)((uint32_t)fd << 16);
            e->step = 0;
            e->substep = e->sl_dir;
            e->sl_rest = 0x18;
        }
        return;
    }
    int16_t sb = (int16_t)e->sl_angle;
    (void)sb;
    /* The hurt squash marks of sub_080C140C: bit 0 flares the squash
     * animation and four debris once, bit 1 waits for the floor. */
    if (e->sl_flags & 1) {
        e->sl_flags = (uint8_t)((e->sl_flags & 0xFE) | 2);
        play(e, kind->anims, 4);
        sl_debris(e, kind->anims, 4, 0, (int32_t)-0x18000, spawned, random);
    }
    if ((e->sl_flags & 2) && (e->sl_coll & 4)) {
        e->sl_flags = (uint8_t)(e->sl_flags & ~2);
        play(e, kind->anims, 0);
    }
    if (e->step == 0) {
        e->mirrored = sl_target_right(e, soma);
        e->sl_power = e->mirrored ? -0x4000 : 0x4000;
        if (e->sl_charge <= 0xFFFF) e->sl_charge += 0x200;
        if (e->sl_rest == 0 &&
            (random() & 0xFF) <= 3 &&
            player_in_centred(e, soma, 0x78, 0xB4)) {
            e->state = 1;
            e->step = 0;
            return;
        }
    }
    /* _080C0B6C: the grounded hop decision — rest countdown, drift,
     * the predictive landing probe and the direction pick. */
    if (e->sl_rest) e->sl_rest = (uint8_t)(e->sl_rest - 1);
    e->x += e->vx;
    e->y += e->vy;
    int blocked = sl_predict(e, layer, 0x4000);
    (void)blocked;
    /* The native's quadrant switch (sub_080C0D10/0D98/0DBE/0E76)
     * probes the predicted box edges against the wall pushes
     * (sub_08002058 / sub_0800207C), the floor depth (sub_08001C1C)
     * and the special cell (sub_080C05D8), then relaunches through
     * sub_080C0630 with the matching direction index, speed bonus
     * and angle override. Collapsed here: a blocked prediction
     * steers to the facing side, an open one hops forward. */
    int dir = e->mirrored ? 1 : 0;
    if (e->substep == 2 || e->substep == 3)
        dir = e->substep;
    sl_launch(e, dir, 0, 0);
}

/* sub_080C0F14 state 1: the attack hop in five substates. Case 0
 * sets animation 1; case 1 — once + 0x6C bit 2 (on the floor) —
 * arms the launch by + 0xC (0 held, 1 back-up, 2 drop, 3 forward),
 * requests song 0x164 and enters step 2; case 2 walks the
 * 0x08528914 probes at the + 0x14 speed (sub_0806CAF8 mode 0x22)
 * and bursts debris on a wall bit; case 3 raises song 0x163, aims
 * + 0x20 at 0x08528922[+ 0xC] and probes the landing; case 4 — on
 * the floor — returns to state 0. */
static void sl_state1(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, AosHitReport *spawned,
                      uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        play(e, kind->anims, 1);
        e->step = 1;
        break;
    case 1:
        /* + 0x6C bit 2: the on-floor mark of the last probe — the
         * launch waits for the ground (approximated by a live floor
         * probe, documented). */
        if (!sl_grounded(e, layer))
            break;
        play(e, kind->anims, 2);
        e->sl_flags |= 4;
        e->sl_mark1b = 0;
        switch (e->substep) {
        case 1:
            e->sl_power = -((int32_t)0x8000 + (int32_t)(random() & 0x1FFF));
            e->vy = (int32_t)(random() % 0x8000) - 0x10000;
            break;
        case 2:
            e->sl_power = 0;
            e->vy = 0;
            break;
        case 3:
            e->sl_power = (int32_t)0x8000 + (int32_t)(random() & 0x1FFF);
            e->vy = (int32_t)(random() % 0x8000) - 0x10000;
            break;
        default:
            break;
        }
        aos_audio_play_song(0x164);
        e->sl_width = 0x4000;
        e->step = 2;
        break;
    case 2: {
        /* sub_0806CAF8 mode 0x22 over 0x08528914: wall bits 0/1 burst
         * debris and pick the return direction (+ 0xC = 3 left, 1
         * right), the floor bit 3 is the plain landing. */
        int result = ground_walk(e, soma, layer, &sl_probes, e->sl_power, 0x22);
        if (result & 1) {
            sl_debris(e, kind->anims, 3, 0x40000, (int32_t)-0x18000, spawned, random);
            e->substep = 3;
            e->step = 3;
            break;
        }
        if (result & 2) {
            sl_debris(e, kind->anims, 3, (int32_t)-0x10000, (int32_t)-0x18000, spawned, random);
            e->substep = 1;
            e->step = 3;
            break;
        }
        if (result & 8) {
            e->step = 4;
            break;
        }
        break;
    }
    case 3:
        aos_audio_play_song(0x163);
        e->sl_angle = sl_angles[e->substep & 3];
        e->sl_flags &= (uint8_t)~4;
        sl_predict(e, layer, 0x4000);
        play(e, kind->anims, 3);
        e->step = 4;
        break;
    case 4:
        if (!sl_grounded(e, layer))
            break;
        play(e, kind->anims, 0);
        e->state = 0;
        e->step = 0;
        e->sl_rest = 0;
        break;
    default:
        break;
    }
}

/* sub_080C10BC state 2: the spin death. Step 0 requests song 0x70
 * and arms animation 0; step 1 — while + 0x11 bit 2 stands — hops
 * on the probes, and on a wall hit zeroes the power, kicks the
 * height by the remaining spin and clears the mark; the body then
 * orbits its angle at 0x12 px per sine unit while + 0x1C counts
 * down by 0x400 — debris every fourth tick, song 0x165 every
 * sixteenth — and answers 1 (delete) once it is spent. */
static void sl_state2(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, AosHitReport *spawned,
                      uint32_t (*random)(void)) {
    int16_t angle = (int16_t)e->sl_angle;
    (void)angle;
    switch (e->step) {
    case 0:
        aos_audio_play_song(0x70);
        play(e, kind->anims, 0);
        if (e->sl_flags & 4) {
            e->sl_width = 0x4000;
            e->sl_angle = (uint16_t)e->step;
        }
        e->step = 1;
        break;
    case 1:
        if (e->sl_flags & 4) {
            int result = ground_walk(e, soma, layer, &sl_probes, e->sl_power, 0x22);
            if (result & 3) {
                e->sl_flags &= (uint8_t)~4;
                e->sl_power = 0;
                int32_t kick = ((0x40000 - e->sl_width) >> 8) * 0x500;
                e->y += kick;
            }
        }
        if (e->sl_width > 0) {
            /* The orbit: position += 0x12 * sine(angle), 0x12 *
             * sine(angle + 0x4000) per axis. */
            e->x += sl_vel((uint16_t)(e->sl_angle + 0x4000), 0x12 << 16);
            e->y += sl_vel(e->sl_angle, 0x12 << 16);
            e->timer = (uint8_t)(e->timer + 1);
            if ((e->timer & 3) == 0)
                sl_debris(e, kind->anims, 1, 0, (int32_t)-0x20000, spawned, random);
            if ((e->timer & 0xF) == 0)
                aos_audio_play_song(0x165);
            e->sl_width -= 0x400;
        }
        break;
    default:
        break;
    }
    e->removed = e->sl_width <= 0;
}

AosHitReport aos_enemy_slime_update(AosEnemy *e, AosSoma *soma,
                                    const AosCollision *layer,
                                    const AosEnemyKind *kind,
                                    const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames,
                                    int soma_atk, int soma_def, int cam_x,
                                    int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = sl_px(e) - cam_x, sy = sl_py(e) - cam_y;
    e->hidden = outside_margins(kind, 4, sx, sy);
    /* sub_0806CC20: the dormant gate — offscreen enemies skip the
     * state, the animation step and the collision (approximated by
     * the shared margins check plus the hit flash, documented). */
    bool run = !e->hit_flash || e->state == 2;
    if (run && !outside_margins(kind, 4, sx, sy)) {
        switch (e->state) {
        case 0:
            sl_state0(e, soma, layer, kind, &spawned, random);
            break;
        case 1:
            sl_state1(e, soma, layer, kind, &spawned, random);
            break;
        case 2:
            sl_state2(e, soma, layer, kind, &spawned, random);
            break;
        case 3:
            step_anim(e, kind->anims);
            break;
        default:
            break;
        }
        if (!e->removed && e->state != 3) step_anim(e, kind->anims);
        /* The affine draw of sub_0803E058: + 0x20 the rotation angle
         * (the renderer reads `angle`), + 0x18/+ 0x1C the scale
         * arguments — the charge stretches the height, the width
         * holds the spin countdown. */
        e->angle = e->sl_angle;
        if (e->sl_charge > 0 && e->sl_charge < 0x40000) {
            e->affine_sy = e->affine_sx =
                (int32_t)(((int64_t)0x10000 * 0x10000) / e->sl_charge);
            if (e->affine_sy < 1) e->affine_sy = e->affine_sx = 1;
        }
        if (!e->removed && e->state != 3 && !e->defeated &&
            !outside_margins(kind, 4, sx, sy)) {
            report = collide(e, soma, kind, weapon, weapon_frames,
                             soma_atk, soma_def);
        }
    }
    if (e->state == 2 && e->sl_width <= 0) e->removed = true;
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    for (int i = 0; i < spawned.effect_count &&
                    report.effect_count < AOS_ENEMY_MAX_EFFECTS; ++i)
        report.effects[report.effect_count++] = spawned.effects[i];
    return report;
}
