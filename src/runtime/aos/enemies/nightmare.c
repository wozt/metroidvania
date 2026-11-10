/* Nightmare (id 0x42) — EnemyNightmareCreate 0x080B839C,
 * EnemyNightmareUpdate 0x080B84B4, drift sub_080B80BC, death
 * sub_080B8240.
 *
 * The floating skull drifts on the 0x08528878 probes with a speed
 * (+ 0x18) that ramps + 0x4000 a tick up to 0x27FFF, holds for the
 * attack window (+ 0x1C > 0x50 → animation param0 + 2, song 0x127)
 * and bleeds off - 0x1000 a tick while it sheds wisps, turning
 * around when the probe byte's bit 2 (+ 0x6C) reports the ceiling.
 * Its death flings it up and out with the blink strobe of
 * sub_0806D518, song 0x6E every eighth tick and a debris puff of
 * sub_08045CEC every fourth.
 *
 * sub_080458E4's two trail slots (+ 0x14/+ 0x15) are OAM-history
 * effect records the native allocates at the state 0 → 1 transition
 * and releases on death; they are kept as marks only (documented).
 * The + 0x4 pre-hit callback sub_080B8628 is hit bookkeeping the
 * shared hurt path already covers, and the sub_08021278 target
 * registration is implicit in the engine. */
#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../math/aos_math.h"
#include "../core/aos_collision.h"

#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define step_anim aos_enemy_step_anim
#define outside_margins aos_enemy_outside_margins
#define collide aos_enemy_collide
#define ground_walk aos_enemy_ground_walk
#define spawn_drift aos_enemy_spawn_drift
#define snap_to_floor aos_enemy_snap_to_floor
#define umod aos_enemy_umod
#define random_centred aos_enemy_random_centred

static int16_t nm_px(const AosEnemy *e) { return (int16_t)(e->x >> 16); }
static int16_t nm_py(const AosEnemy *e) { return (int16_t)(e->y >> 16); }

/* 0x08118CE0: the 40-entry blink table read by sub_0806D518 — the
 * death reads it backwards as 0x28 - + 0xD. */
static const int8_t nm_blink[40] = {
    0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
    0, 0, 0, 1, 0, 0, 0, 1
};
static int nm_pattern(int idx) { return idx >= 0 && idx < 40 ? nm_blink[idx] : 0; }

/* sub_080B80BC, state 1: the drift machine. Step 0 zeroes the speed
 * and arms step 1; step 1 ramps the speed, howls 0x7D every 0x19
 * ticks inside the window and pounces (animation param0 + 2, song
 * 0x127) once + 0x1C passes 0x50; step 2 bleeds the speed, sheds a
 * wisp drift every fourth tick and U-turns on the probe's bit 2. */
static void nm_state1(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, AosHitReport *spawned,
                      int sx, int sy, uint32_t (*random)(void)) {
    e->nm_probe = (uint8_t)ground_walk(e, soma, layer, &kind->probes,
                                       e->nm_speed, 4);
    if (e->step == 0) {
        e->nm_speed = 0;
        e->step = 1;
    } else if (e->step == 1) {
        if (umod(e->timer, 0x19) == 0 &&
            sx >= -0x40 && sx <= 0x2F0 - 0x40 && sy >= -0x40 && sy <= 0x120) {
            /* The native's tighter song window (sub_08068AD4 with
             * -0x40/0x170/0x120) approximated by the screen box. */
            aos_audio_play_song(0x7D);
        }
        e->timer++;
        if (e->nm_speed <= 0x27FFF) e->nm_speed += 0x4000;
        if (++e->walk_timer > 0x50) {
            play_loop(e, kind->anims, (unsigned)e->param0 + 2, false);
            /* param0 == 0's palette/affine variant (0x1810E0F8) is
             * cosmetic and folded into the shared draw. */
            aos_audio_play_song(0x127);
            e->step = 2;
            e->timer = 0;
        }
    } else {
        if (e->nm_speed > 0) e->nm_speed -= 0x1000;
        e->timer++;
        if ((e->timer & 3) == 0) {
            /* sub_08045CEC(x, y, (rng & 1) + 4, 6): the shed wisp,
             * vx ± 0x8000 by facing — approximated by the shared
             * drift (documented). */
            uint32_t r = random();
            spawn_drift((uint8_t)((r & 1) + 4), e->x, e->y, spawned);
        }
        if (e->nm_probe & 4) {
            e->walk_timer = 0;
            play_loop(e, kind->anims, (unsigned)e->param0, true);
            e->step = 0;
            e->mirrored = !e->mirrored;
        }
    }
}

/* sub_080B8240, state 2: the death. Step 0 releases the trail slots,
 * plays animation param0 + 2 once and flings the skull up and out
 * (vx ± 0x28000 by facing, vy - 0x28000, ay 0x200000); step 1 runs
 * the blink strobe, song 0x6E every eighth tick, a debris puff of
 * sub_08045CEC(x + r, y + r - 0x20 px, 1, 7) every fourth, gravity
 * and deletes on the window/probe/timer rule. Returns true to
 * delete, like the native. */
static bool nm_state2(AosEnemy *e, const AosEnemyKind *kind,
                      AosHitReport *spawned, int sx, int sy,
                      uint32_t (*random)(void)) {
    if (e->step == 0) {
        e->nm_wisp_a = e->nm_wisp_b = 0;
        play_loop(e, kind->anims, (unsigned)e->param0 + 2, false);
        e->vx = e->mirrored ? 0x28000 : -0x28000;
        e->vy = -0x28000;
        e->ay = 0x200000;
        e->step = 1;
        e->timer = 0;
    }
    /* The blink strobe of sub_0806D518(0x28 - + 0xD) on + 0x5A —
     * approximated by the shared hidden flicker (documented). */
    if (e->timer <= 0x27 && (nm_pattern(0x28 - e->timer) & 1))
        e->hidden = true;
    e->timer++;
    if ((e->timer & 7) == 0) aos_audio_play_song(0x6E);
    if ((e->timer & 3) == 0) {
        int32_t rx = random_centred(random, 0x10);
        int32_t ry = random_centred(random, 0x10) - (0x20 << 16);
        spawn_drift(AOS_EFFECT_DUST, e->x + rx, e->y + ry, spawned);
    }
    /* sub_0806D3D8(e, -0x80000): the shared integrate stands in for
     * the native velocity pass (documented). */
    aos_enemy_integrate(e);
    if (outside_margins(kind, 4, sx, sy)) return true;
    if ((e->nm_probe & 4) && e->timer <= 0x80) return true;
    return false;
}

AosHitReport aos_enemy_nightmare_update(AosEnemy *e, AosSoma *soma,
                                        const AosCollision *layer,
                                        const AosEnemyKind *kind,
                                        const AosWeaponEntity *weapon,
                                        const AosWeaponFrames *weapon_frames,
                                        int soma_atk, int soma_def, int cam_x,
                                        int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = nm_px(e) - cam_x, sy = nm_py(e) - cam_y;
    /* sub_0806E4F8(e, -0x20): hidden while above the screen top. */
    e->hidden = sy < -0x20;
    /* sub_08068AD4: outside the window the native skips the whole
     * tick — animation step and collision included (the shared
     * margins check stands in, documented). The defeated mark
     * (+ 0x3E bit 1) bypasses that gate natively so the death can
     * run its own window check and delete; the + 0x2D hit flash
     * gates the same way sub_0806AD24 does for the other ports. */
    bool run = !e->hit_flash || e->state == 2;
    if (run && (e->state == 2 || !outside_margins(kind, 4, sx, sy))) {
        switch (e->state) {
        case 0:
            /* The state-0 entry registers the two trail slots of
             * sub_080458E4(e, 0, 0xC, 1) and (e, 0, 6, 0) into
             * + 0x14/+ 0x15, then arms state 1 with + 0xB/+ 0xC
             * cleared. */
            e->nm_wisp_a = e->nm_wisp_b = 1;
            e->state = 1;
            e->step = e->substep = 0;
            break;
        case 1:
            nm_state1(e, soma, layer, kind, &spawned, sx, sy, random);
            break;
        case 2:
            if (nm_state2(e, kind, &spawned, sx, sy, random))
                e->removed = true;
            break;
        case 3:
            step_anim(e, kind->anims);
            break;
        default:
            break;
        }
        /* Every surviving state ends on sub_0803F17C; only states
         * 0/1 fill the hit report through sub_080421AC. */
        if (!e->removed) step_anim(e, kind->anims);
        if (!e->removed && e->state < 2 && !e->defeated && !e->hidden &&
            !outside_margins(kind, 4, sx, sy)) {
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
