/* Kyoma Demon (0x27): the doppelganger that haunts the castle. Its
 * native routines are EnemyKyomaDemonCreate (0x080A01B8) and
 * EnemyKyomaDemonUpdate (0x080A00DC) dispatching sub_080A02AC
 * (state 0, the position-copying watch), sub_080A04B0 (state 1, the
 * three-substep attack) and sub_080A0790 (state 2, the blinking
 * wait); state 3 only steps the animation. The contact sub_080A07D0
 * feeds sub_08021654 and the hurt sub_080A05B4 the shared damage
 * walk; both are wired in aos_enemy.c.
 *
 * States 0 and 1 copy the player entity's + 0x40/+ 0x44 position and
 * + 0x58 bit 6 facing onto the demon every tick, then gate on a
 * spawn-window check: the demon hides (+ 0x58 bit 5) while its
 * screen position sits outside the window latched at create. The
 * native latches that window against the room scroll at create; the
 * camera is only known on the first update here, so kd_sx/kd_sy and
 * kd_tx/kd_ty are latched lazily (documented approximation). */
#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../math/aos_math.h"
#include "../core/aos_collision.h"

#define player_in aos_enemy_player_in
#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define step_anim aos_enemy_step_anim
#define face_player aos_enemy_face_player
#define snap_to_floor aos_enemy_snap_to_floor
#define collide aos_enemy_collide
#define outside_margins aos_enemy_outside_margins
#define spawn_drift aos_enemy_spawn_drift

static int16_t kd_px(const AosEnemy *e) { return (int16_t)(e->x >> 16); }
static int16_t kd_py(const AosEnemy *e) { return (int16_t)(e->y >> 16); }

/* 0x08118CE0: the 40-entry blink table read by sub_0806D518 — the
 * callers mask the signed byte with 1 into + 0x58 bit 5, a strobe
 * while the demon waits out its attack or its death. */
static const int8_t kd_blink[40] = {
    0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
    0, 0, 0, 1, 0, 0, 0, 1
};
static int kd_pattern(uint8_t t) { return t < 40 ? kd_blink[t] : 0; }

/* The spawn window of sub_080A02AC: latched on the first update
 * against the camera (the native reads the room scroll at create). */
static void kd_origin(AosEnemy *e, int cam_x, int cam_y) {
    if (e->kd_origin) return;
    e->kd_origin = true;
    e->kd_sx = (int16_t)(e->kd_ox - cam_x);
    e->kd_sy = (int16_t)(e->kd_oy - cam_y);
    e->kd_tx = (int32_t)(e->kd_ox - cam_x) << 16;
    e->kd_ty = (int32_t)(e->kd_oy - cam_y) << 16;
}

/* The + 0x58 bit-5 window check shared by state 0 and the pose
 * checks: `edge` is the demon's screen x against the latched spawn
 * window [sx - 0x20, sx + 0x20] (state 0 step 1 uses -0x20..+0x20;
 * the attack entry re-check uses -0xA..+0x14). */
static bool kd_outside(const AosEnemy *e, int edge) {
    return edge < e->kd_sx - 0x20 || edge > e->kd_sx + 0x20;
}

/* func_0803F2C8(e, seq, 0, loop): set the animation only when it
 * differs — the native helper is idempotent across the per-tick
 * calls of the pose check. */
static void kd_anim(AosEnemy *e, const AosAnimSet *anims, unsigned seq,
                    bool loop) {
    if (e->anim.id != seq) play_loop(e, anims, seq, loop);
}

/* The position copy and facing mirror both watch states run before
 * their window check (sub_080A02AC step 0 and step 1 share it). */
static void kd_copy(AosEnemy *e, const AosSoma *soma, int cam_x) {
    e->mirrored = soma->facing_left;
    e->x = soma->x + 0x100000;
    e->y = soma->y;
    int edge = kd_px(e) - cam_x;
    e->hidden = kd_outside(e, edge);
}

/* func_0803F0E4(e, 0x08528364 / 0x0852836C, 0, 0): the reveal and
 * death child entities. Their records are not exported; a spark
 * drift stands in (documented approximation). */
static void kd_effect(AosEnemy *e, AosHitReport *report) {
    spawn_drift(AOS_EFFECT_SPARK, e->x, e->y, report);
}

/* sub_080A02AC, state 0. Step 0 watches: copy, window, the player
 * flag arming the reveal, then the attack-entry timer and the pose
 * check; step 1 is the one-tick reveal follow-through. */
static void kd_state0(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                      AosHitReport *report, int cam_x, int cam_y,
                      uint32_t (*random)(void)) {
    kd_origin(e, cam_x, cam_y);
    if (e->step == 0) {
        kd_copy(e, soma, cam_x);
        if (soma->flags & 2) {
            /* func_0803F0E4(e, 0x08528364, 0, 0) then step 1. */
            kd_effect(e, report);
            e->step = 1;
            return;
        }
        if (e->producer_timer > 0) {
            e->producer_timer--;
        } else {
            e->producer_timer = (int16_t)((random() & 0x3F) + 0x40);
            int edge = kd_px(e) - cam_x;
            if (edge >= e->kd_sx - 0xA && edge <= e->kd_sx + 0x14) {
                e->state = 1;
                e->step = 0;
                return;
            }
        }
        /* The pose check of 0x080A03B6: the screen position against
         * the latched spawn target picks animation 0 when it stands
         * (and the current + 0x6D id != 0), animation 1 otherwise
         * (id != 1). */
        int32_t sx = e->x - ((int32_t)cam_x << 16);
        int32_t sy = e->y - ((int32_t)cam_y << 16);
        if (sx == e->kd_tx && sy == e->kd_ty && e->anim.id != 0) {
            kd_anim(e, kind->anims, 0, true);
        } else if (e->anim.id != 1) {
            kd_anim(e, kind->anims, 1, true);
            step_anim(e, kind->anims);
        }
        return;
    }
    /* Step 1 (0x080A0412): the same copy and window, then the player
     * flag decides — clear plays animation 1 and returns to step 0,
     * set parks the demon at step 2 until the hurt wakes it. */
    kd_copy(e, soma, cam_x);
    uint32_t fl = soma->flags & 2;
    if (!fl) kd_anim(e, kind->anims, 1, true);
    e->step = fl ? 2 : 0;
}

/* sub_080A04B0, state 1: the attack. Step 0 lifts the demon 16 px
 * and locks animation 2; step 1 waits out the loop for animation 3
 * (sound 0x85 on element 4); step 2 strobes hidden on the blink
 * table, then hands back to the watch. */
static void kd_state1(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                      uint32_t (*random)(void)) {
    if (e->step == 0) {
        kd_anim(e, kind->anims, 2, false);
        face_player(e, soma);
        e->kd_alert = 0;
        e->kd_f59 |= 2;
        e->step = 1;
        e->kd_5c = 0x11;
        e->y += (int32_t)0xFFF00000;
        return;
    }
    if (e->step == 1) {
        /* + 0x6E == 4: the animation frame 4 plays sound 0x85. */
        if (e->anim.frame == 4) aos_audio_play_song(0x85);
        if (e->cycle_done) {
            kd_anim(e, kind->anims, 3, false);
            e->step = 2;
            e->timer = 0x28;
        }
        return;
    }
    if (e->step == 2) {
        e->hidden = (kd_pattern(e->timer) & 1) != 0;
        e->timer = (uint8_t)(e->timer - 1);
        if ((int8_t)e->timer <= 0) {
            e->state = 0;
            e->step = 0;
            e->cycle_done = false;
            e->kd_f59 &= ~2;
            e->hidden = false;
            e->producer_timer = (int16_t)((random() & 0x3F) + 0x40);
            e->kd_alert = 1;
            e->kd_5c = 0x1D;
        }
    }
}

/* sub_080A0790, state 2: the post-hurt blink wait — the same strobe
 * on the + 0xD timer, then step 1 parks until the framework retires
 * the defeated demon. */
static void kd_state2(AosEnemy *e) {
    if (e->step != 0) return;
    e->hidden = (kd_pattern(e->timer) & 1) != 0;
    e->timer = (uint8_t)(e->timer - 1);
    if ((int8_t)e->timer <= 0) e->step = 1;
}

AosHitReport aos_enemy_kyoma_update(AosEnemy *e, AosSoma *soma,
                                    const AosCollision *layer,
                                    const AosEnemyKind *kind,
                                    const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames,
                                    int soma_atk, int soma_def, int cam_x,
                                    int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    (void)layer;
    int sx = kd_px(e) - cam_x, sy = kd_py(e) - cam_y;
    if (e->state != 2) e->hidden = outside_margins(kind, 4, sx, sy);
    /* sub_08068AD4/sub_0806AD24: the dormant and stun gates run in
     * the shared dispatch; the hit flash still lets state 2 tick. */
    bool run = !e->hit_flash || e->state == 2;
    if (run && !outside_margins(kind, 4, sx, sy)) {
        switch (e->state) {
        case 0:
            kd_state0(e, soma, kind, &spawned, cam_x, cam_y, random);
            break;
        case 1:
            kd_state1(e, soma, kind, random);
            break;
        case 2:
            kd_state2(e);
            break;
        case 3:
            step_anim(e, kind->anims);
            break;
        default:
            break;
        }
        if (!e->removed && e->state != 3) step_anim(e, kind->anims);
        if (!e->removed && e->state != 3 && !e->defeated && !e->hidden &&
            !outside_margins(kind, 4, sx, sy)) {
            report = collide(e, soma, kind, weapon, weapon_frames,
                             soma_atk, soma_def);
        }
    }
    if (e->state == 2 && e->step >= 1 && e->defeated) {
        /* The native parks the demon at state 2 step 1; the shared
         * defeated retirement removes it. */
    }
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    for (int i = 0; i < spawned.effect_count &&
                    report.effect_count < AOS_ENEMY_MAX_EFFECTS; ++i)
        report.effects[report.effect_count++] = spawned.effects[i];
    return report;
}
