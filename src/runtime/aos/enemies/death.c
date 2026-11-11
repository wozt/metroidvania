/* Death (EnemyDeathCreate 0x080BF2E8 / EnemyDeathUpdate 0x080BF5FC,
 * id 0x6B): the reaper boss — a logic parent driving three persistent
 * children through the gEwramData + 0x4E4 slot table (stride 0x84):
 * the visual mirror of sub_080BC298, the mid entity of sub_080BD094
 * and the scythe arm of sub_080BD358 (its own nine-state machine).
 *
 * Native layout folded into AosEnemy:
 * - the + 0x0B/+ 0x0C/+ 0x0D machine bytes map to step/substep/timer,
 *   the + 0x16 attack timer to dth_atk, the + 0x48/+ 0x4C frame
 *   deltas to dth_dx/dth_dy, the + 0x50/+ 0x54 drift velocities to
 *   dth_vx/dth_vy, the + 0x14 flag bits to dth_flags, the + 0x19 pick
 *   to dth_idx and the + 0x6C/+ 0x6E gates to dth_g6c/dth_g6e; the
 *   + 0x1C/+ 0x1E charge target reuses target_x/target_y;
 * - the children become persistent roles AOS_ROLE_DEATH_MIRROR/MID/
 *   MID_TAIL/SCYTHE/SHOT/GORE emitted under the 8-child cap;
 * - the hurt callbacks sub_080BF4AC/sub_080BF504 fold into the shared
 *   collide; the lethal path (sub_080683BC, state 7) runs in the
 *   update.
 *
 * Documented approximations: the nine parent states keep their case
 * order, songs, timers and ArcTan2/Sqrt dives through the shared
 * helpers, but the per-case wall probes (sub_0806D490/sub_0806D4A8/
 * sub_0806D3D8), the sub_080009E4 sine offsets, the sub_080458E4
 * strike zones and the gEwramData + 0x13110/+ 0xA094/+ 0x42C global
 * reads collapse into player_in/outside_margins and documented
 * constants; the scythe's own nine-case machine keeps its order and
 * songs but its sub-children and projectile helpers collapse into
 * the shared SHOT/GORE roles; the sub_08021654 ring emits, the
 * sub_080427B8/sub_0804277C box registrations, the + 0x5A palette
 * merge and the sub_08013CF0/sub_08034498/sub_0800C5A8 event calls
 * are not ported; and there is no emulator comparison. */

#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../core/aos_collision.h"
#include "../core/aos_soma.h"
#include "../math/aos_math.h"
#include <string.h>

#define collide aos_enemy_collide
#define step_anim aos_enemy_step_anim
#define play_loop aos_enemy_play_loop
#define face_player aos_enemy_face_player
#define player_in aos_enemy_player_in
#define outside_margins aos_enemy_outside_margins
#define target_x(...) aos_enemy_target_x(__VA_ARGS__)
#define target_y(...) aos_enemy_target_y(__VA_ARGS__)

static bool dth_is_death(uint8_t id) { return id == 0x6B; }

/* sub_080009E4: the native sine table, 16.16 results (stolas twin). */
static int32_t dth_sin(uint32_t angle) {
    return aos_sine(angle & 0xFFFF);
}
static int32_t dth_cos(uint32_t angle) {
    return aos_sine((angle + 0x4000) & 0xFFFF);
}
/* The 16.16 fixed multiply (sub_0803E86C twin). */
static int32_t dth_mul(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * b) >> 16);
}

/* Emit the three persistent children progressively (native: the
 * create's three sub_0806DFF8 calls — mirror sub_080BC298 always,
 * mid sub_080BD094 only when param0 is set, scythe sub_080BD358). */
static void dth_spawn_children(AosHitReport *hit, AosEnemy *e,
                               uint32_t (*random)(void)) {
    if (e->boss_parts_spawned) return;
    int want = e->param0 ? 3 : 2;
    while (e->spawned_count < want && hit->child_count < AOS_ENEMY_MAX_CHILDREN) {
        int i = e->spawned_count++;
        AosEnemy c;
        memset(&c, 0, sizeof c);
        c.id = e->id;
        c.parent = e;
        c.state = 0;
        c.step = 0;
        c.timer = 0;
        c.hp = 1;
        c.combat.hurt_off = true;
        c.x = e->x;
        c.y = e->y;
        c.mirrored = random != NULL && (random() & 1);
        if (i == 0) {
            c.role = AOS_ROLE_DEATH_MIRROR;
            c.static_frame = 0;
        } else if (e->param0 && i == 1) {
            c.role = AOS_ROLE_DEATH_MID;
            c.static_frame = 0x18;
        } else {
            c.role = AOS_ROLE_DEATH_SCYTHE;
            c.static_frame = 0x0C;
            c.hp = 0x7D00; /* the arm's own 0xFA << 3 */
        }
        hit->children[hit->child_count++] = c;
    }
    if (e->spawned_count >= want)
        e->boss_parts_spawned = true;
}

/* The mirror child sub_080BC298: state 0 loads the shared resources
 * and anim (0,2,1); state 1 copies the parent's position, mirrors the
 * + 0x5A bits and takes the parent's + 0x14 facing; the parent's
 * state 7 deletes it. */
static void dth_mirror_update(AosEnemy *c, const AosAnimSet *anims) {
    AosEnemy *p = c->parent;
    if (!p || p->state == 7 || p->defeated) { c->removed = true; return; }
    if (c->state == 0) {
        c->state = 1;
        play_loop(c, anims, 0, true);
    }
    c->x = p->x;
    c->y = p->y;
    c->mirrored = (p->angle & 0x8000) != 0;
    step_anim(c, anims);
}

/* The mid child sub_080BD094: state 0 loads resources and anim
 * 0x18 with the + 0x48/+ 0x4C entry velocity; state 1 parks beside
 * the parent until the parent's + 0x14 & 0x20 attack flag rises into
 * state 2; state 2 re-aims and state 3 counts + 0xD down while its
 * tail child (sub_080BCFC8) lives, then deletes. */
static void dth_mid_update(AosEnemy *c, const AosAnimSet *anims,
                           AosHitReport *hit) {
    AosEnemy *p = c->parent;
    if (!p) { c->removed = true; return; }
    switch (c->state) {
    case 0:
        c->state = 1;
        c->timer = 0x19;
        play_loop(c, anims, 0x18, true);
        break;
    case 1:
        c->x = p->x - 0x800000;
        c->y = p->y + 0x600000;
        if (p->dth_flags & 0x2000) {
            c->timer = 0x30;
            c->state = 2;
        }
        break;
    case 2:
        c->x = p->x - 0x800000;
        c->y = p->y + 0x600000;
        if (c->timer == 0) {
            c->state = 3;
            c->timer = 0x20;
        } else {
            (void)--c->timer;
        }
        break;
    case 3:
        /* sub_0806D460 dive then the sub_080BCFC8 tail child once. */
        if (!c->helper_spawned && hit->child_count < AOS_ENEMY_MAX_CHILDREN) {
            c->helper_spawned = true;
            AosEnemy t;
            memset(&t, 0, sizeof t);
            t.id = c->id;
            t.parent = p;
            t.role = AOS_ROLE_DEATH_MID_TAIL;
            t.state = 0;
            t.hp = 1;
            t.combat.hurt_off = true;
            t.x = c->x;
            t.y = c->y;
            t.timer = 0x18;
            hit->children[hit->child_count++] = t;
        }
        if (c->timer == 0) {
            c->removed = true;
        } else {
            (void)--c->timer;
        }
        break;
    default:
        c->removed = true;
        break;
    }
    step_anim(c, anims);
}

/* The mid's tail child sub_080BCFC8: a short-lived ember that fades
 * out over its + 0xB countdown. */
static void dth_tail_update(AosEnemy *c, const AosAnimSet *anims) {
    if (c->timer == 0 || (uint8_t)--c->timer == 0)
        c->removed = true;
    step_anim(c, anims);
}

/* The scythe arm sub_080BD358: its own nine-state machine riding the
 * parent slot — cases keep the native order, songs 0x92/0x1F/0x6E,
 * the ArcTan2/Sqrt aimed sweeps and the + 0x16 timer; its sub-children
 * and projectile helpers collapse into the shared SHOT/GORE roles. */
static void dth_scythe_update(AosEnemy *c, AosSoma *soma,
                              const AosAnimSet *anims,
                              AosHitReport *hit,
                              uint32_t (*random)(void)) {
    AosEnemy *p = c->parent;
    if (!p || p->state == 7 || p->defeated) {
        if (c->state != 8) {
            c->state = 7;
            c->timer = 0x3C;
            aos_audio_play_song(0x6E);
        }
    }
    switch (c->state) {
    case 0:
        /* Create tail: shared resources, hurt box mode 8, + 0x5C =
         * 0x12, anim (0xC,2,0), + 0x18 = 0x1000, + 0x72 |= 4. */
        c->state = 1;
        c->timer = 0x60;
        play_loop(c, anims, 0x0C, true);
        step_anim(c, anims);
        break;
    case 1:
        /* Hover sweep: sine bob on + 0x44, the + 0x16 timer. */
        c->y += dth_sin((uint32_t)c->timer << 7) >> 8;
        if (c->timer == 0) {
            c->timer = 0x40;
            c->state = 2;
        } else {
            (void)--c->timer;
        }
        step_anim(c, anims);
        break;
    case 2:
        /* Aimed sweep toward the player inside the aggro box: the
         * ArcTan2 heading at speed 0x4000, song 0x92 on entry, then
         * back to the hover with the + 0x16 = 0x80 timer. */
        if (player_in(soma, c->x >> 16, c->y >> 16, 0xA0, 0x78)) {
            uint16_t ang = aos_enemy_angle_to_target(c, soma);
            c->vx = dth_mul(dth_cos(ang), 0x40000);
            c->vy = dth_mul(dth_sin(ang), 0x40000);
            c->x += c->vx;
            c->y += c->vy;
            aos_audio_play_song(0x92);
            c->state = 1;
            c->timer = 0x80;
        } else {
            c->state = 1;
            c->timer = 0x40;
        }
        step_anim(c, anims);
        break;
    case 3:
    case 4:
    case 5:
    case 6:
        /* The sweep recovery and re-aim cases — the aimed motion
         * with shorter timers (documented collapse). */
        if (player_in(soma, c->x >> 16, c->y >> 16, 0xA0, 0x78)) {
            uint16_t ang = aos_enemy_angle_to_target(c, soma);
            c->vx = dth_mul(dth_cos(ang), 0x30000);
            c->vy = dth_mul(dth_sin(ang), 0x30000);
            c->x += c->vx;
            c->y += c->vy;
            if ((c->timer & 0x1F) == 0)
                aos_audio_play_song(0x1F);
        }
        if (c->timer == 0) {
            c->state = 1;
            c->timer = 0x40;
        } else {
            (void)--c->timer;
        }
        step_anim(c, anims);
        break;
    case 7:
        /* Death: song 0x6E already requested, scatter into state 8. */
        c->state = 8;
        c->timer = 0x3C;
        step_anim(c, anims);
        break;
    default:
        if (c->timer && (uint8_t)--c->timer == 0)
            c->removed = true;
        step_anim(c, anims);
        break;
    }
    (void)hit;
    (void)random;
}

/* The parent's nine states (sub_080BE414..sub_080BF178) plus the
 * dormant state 8.  The + 0xB/+ 0xC/+ 0xD machine bytes map to
 * step/substep/timer; + 0x16 to dth_atk; + 0x48/+ 0x4C to dth_dx/
 * dth_dy; + 0x50/+ 0x54 to dth_vx/dth_vy; + 0x1C/+ 0x1E to
 * target_x/target_y. */
static void dth_state_machine(AosHitReport *hit, AosEnemy *e,
                              AosSoma *soma, const AosAnimSet *anims,
                              uint32_t (*random)(void)) {
    switch (e->state) {
    case 0:
        /* sub_080BE414 intro: substeps wait the + 0x42C flags, check
         * the player inside 0xC0, raise + 0x16 = 0x60 and the two
         * sub_080458E4 strike zones, song 0x121, then the ArcTan2
         * dive storing + 0x48/+ 0x4C; state 1 at the timer's end. */
        switch (e->substep) {
        case 0:
            e->substep = 1;
            break;
        case 1:
            if (player_in(soma, e->x >> 16, e->y >> 16, 0xC0, 0xC0)) {
                e->dth_flags |= 0x2000;
                e->dth_atk = 0x60;
                e->substep = 2;
            }
            break;
        case 2:
            if (e->dth_atk == 0) {
                e->dth_atk = 0x30;
                e->substep = 3;
                aos_audio_play_song(0x121);
            } else {
                (void)--e->dth_atk;
            }
            break;
        default: {
            uint16_t ang = aos_enemy_angle_to_target(e, soma);
            int32_t spd = 0x40000;
            e->vx = dth_mul(dth_cos(ang), spd);
            e->vy = dth_mul(dth_sin(ang), spd);
            e->x += e->vx;
            e->y += e->vy;
            e->dth_dx = e->vx;
            e->dth_dy = e->vy;
            if (e->dth_atk == 0) {
                e->state = 1;
                e->substep = 0;
            } else {
                (void)--e->dth_atk;
            }
            break;
        }
        }
        break;
    case 1:
        /* sub_080BE608 eight-case hover/attack machine (+ 0xB). */
        switch (e->step) {
        case 0:
            e->step = 1;
            break;
        case 1:
            /* Hover: sine bob, the wall-probe drift velocities, RNG
             * & 7 under the + 0x14 & 0x40 flag enters case 2. */
            e->y += dth_sin((uint32_t)e->timer << 7) >> 8;
            e->dth_vx = player_in(soma, (e->x >> 16) - 0x80,
                                  e->y >> 16, 0x28, 0x28) ? -0x40000 : 0;
            e->dth_vy = player_in(soma, (e->x >> 16) + 0x80,
                                  e->y >> 16, 0x28, 0x28) ? 0x40000 : 0;
            e->x += e->dth_vx;
            e->y += e->dth_vy;
            if (random && (random() & 7) == 0 && (e->dth_flags & 0x40)) {
                e->step = 2;
                e->substep = 0;
            }
            break;
        case 2:
            /* Wind-up: song 0xD1, + 0x16 = 0x40, then the volley. */
            if (e->dth_atk == 0) {
                e->dth_atk = 0x40;
                e->step = 3;
                aos_audio_play_song(0xD1);
            } else {
                (void)--e->dth_atk;
            }
            break;
        case 3:
            /* Volley: a sub_080BDD94 projectile every 16 ticks; the
             * timer's end plays anim 0x13 into case 6. */
            if ((e->dth_atk & 0xF) == 0 &&
                hit->child_count < AOS_ENEMY_MAX_CHILDREN) {
                AosEnemy c;
                memset(&c, 0, sizeof c);
                c.id = e->id;
                c.parent = e;
                c.role = AOS_ROLE_DEATH_SHOT;
                c.state = 0;
                c.hp = 1;
                c.combat.hurt_off = true;
                c.x = e->x;
                c.y = e->y;
                c.timer = 0x1F;
                uint16_t ang = aos_enemy_angle_to_target(&c, soma);
                c.angle = ang;
                c.vx = dth_mul(dth_cos(ang), 0x80000);
                c.vy = dth_mul(dth_sin(ang), 0x80000);
                hit->children[hit->child_count++] = c;
            }
            if (e->dth_atk == 0) {
                e->step = 6;
                play_loop(e, anims, 0x13, false);
            } else {
                (void)--e->dth_atk;
            }
            break;
        case 4:
        case 5:
            e->dth_vx = e->dth_vy = 0;
            e->step = 1;
            break;
        case 6:
            e->step = 7;
            break;
        default:
            /* sub_080BE8AE: drift, the anim sequence end returns. */
            if (e->cycle_done) {
                e->step = 1;
                play_loop(e, anims, 1, true);
            }
            break;
        }
        e->timer = (uint8_t)(e->timer + 1);
        break;
    case 2:
        /* sub_080BE984 aimed dive: substeps pick the point above the
         * player, ArcTan2/Sqrt, speed dist >> 6, + 0x48/+ 0x4C deltas,
         * song 0x121; the + 0x6E gate walks back to state 1. */
        switch (e->substep) {
        case 0:
            e->timer = 0x40;
            e->substep = 1;
            break;
        case 1: {
            int32_t tx = target_x(soma);
            int32_t ty = target_y(soma) - 0xA00000;
            int32_t dx = tx - e->x, dy = ty - e->y;
            int32_t dxi = dx >= 0 ? dx >> 16 : -((-dx) >> 16);
            int32_t dyi = dy >= 0 ? dy >> 16 : -((-dy) >> 16);
            uint16_t ang = (uint16_t)aos_arctan2((int16_t)dyi, (int16_t)dxi);
            uint32_t dist = aos_isqrt((uint32_t)(dxi * dxi + dyi * dyi));
            int32_t spd = (int32_t)(dist >> 6);
            e->vx = dth_mul(dth_cos(ang), spd << 16);
            e->vy = dth_mul(dth_sin(ang), spd << 16);
            e->x += e->vx;
            e->y += e->vy;
            e->dth_dx = e->vx;
            e->dth_dy = e->vy;
            if (e->timer == 0) {
                e->substep = 2;
                aos_audio_play_song(0x121);
            } else {
                (void)--e->timer;
            }
            break;
        }
        default:
            if (e->dth_atk == 0) {
                e->state = 1;
                e->substep = 0;
            } else {
                (void)--e->dth_atk;
            }
            break;
        }
        break;
    case 3:
        /* sub_080BEB50 wander: sine-offset drift, wall probes, RNG
         * pick every 0x20 ticks into state 4/5/6. */
        e->y += dth_sin((uint32_t)e->timer << 8) >> 9;
        if ((e->timer & 0x1F) == 0 && random) {
            uint32_t r = random() & 0x3F;
            if (r < 0x30) {
                e->state = (r <= 0x14 || e->dth_g6e > 0x60) ? 4 : 5;
            } else {
                e->state = 6;
            }
            e->substep = 0;
        }
        e->timer = (uint8_t)(e->timer + 1);
        break;
    case 4:
        /* sub_080BEC84 charge: facing from the player, the + 0x1C/+ 0x1E
         * target, move_toward at dist >> 4, songs by substep. */
        switch (e->substep) {
        case 0: {
            int32_t dx = target_x(soma) - (e->x >> 16);
            e->mirrored = dx < 0;
            e->target_x = target_x(soma) + (dx < 0 ? -0x78 : 0x78) * 0x10000;
            e->target_y = target_y(soma) + 0xA00000;
            e->substep = 1;
            play_loop(e, anims, 9, true);
            break;
        }
        case 1: {
            int32_t dx = e->target_x - e->x, dy = e->target_y - e->y;
            int32_t dxi = dx >= 0 ? dx >> 16 : -((-dx) >> 16);
            int32_t dyi = dy >= 0 ? dy >> 16 : -((-dy) >> 16);
            uint16_t ang = (uint16_t)aos_arctan2((int16_t)dyi, (int16_t)dxi);
            uint32_t dist = aos_isqrt((uint32_t)(dxi * dxi + dyi * dyi));
            int32_t spd = (int32_t)(dist >> 4);
            e->vx = dth_mul(dth_cos(ang), spd << 16);
            e->vy = dth_mul(dth_sin(ang), spd << 16);
            e->x += e->vx;
            e->y += e->vy;
            if (spd < 0x100) {
                e->substep = 2;
                play_loop(e, anims, 0x0A, false);
                e->timer = 0xF0;
            }
            break;
        }
        case 2:
            if (e->timer == 0) {
                e->state = 3;
                e->substep = 0;
            } else {
                (void)--e->timer;
            }
            break;
        default:
            e->state = 3;
            e->substep = 0;
            break;
        }
        break;
    case 5:
        /* sub_080BEF04 aimed sweep: ArcTan2/Sqrt/sub_08068CDC. */
        {
            int32_t dx = target_x(soma) - (e->x >> 16);
            int32_t dy = target_y(soma) - (e->y >> 16);
            uint16_t ang = (uint16_t)aos_arctan2((int16_t)dy, (int16_t)dx);
            e->vx = dth_mul(dth_cos(ang), 0x40000);
            e->vy = dth_mul(dth_sin(ang), 0x40000);
            e->x += e->vx;
            e->y += e->vy;
        }
        if (e->timer == 0) {
            e->state = 3;
            e->substep = 0;
        } else {
            (void)--e->timer;
        }
        break;
    case 6:
        /* sub_080BF108: the anim-only beat. */
        step_anim(e, anims);
        break;
    case 7:
        /* sub_080BF178 death: the sub_0803CDF0/3CED4/3D18C/3D270
         * sequence spawns the gore children; the timer end deletes. */
        if ((e->timer & 3) == 0 &&
            hit->child_count < AOS_ENEMY_MAX_CHILDREN) {
            AosEnemy c;
            memset(&c, 0, sizeof c);
            c.id = e->id;
            c.parent = e;
            c.role = AOS_ROLE_DEATH_GORE;
            c.state = 0;
            c.hp = 1;
            c.combat.hurt_off = true;
            c.x = e->x + (int32_t)((int)(random ? random() : 0) & 0x1F) * 0x1000 - 0x10000;
            c.y = e->y - 0x800000;
            c.vx = ((int32_t)((int)(random ? random() : 0) & 0xFF) - 0x80) * 0x1000;
            c.vy = ((int32_t)((int)(random ? random() : 0) & 0xFF) - 0x100) * 0x1000;
            c.timer = 0x40;
            hit->children[hit->child_count++] = c;
        }
        if (e->timer == 0) {
            e->removed = true;
        } else {
            (void)--e->timer;
        }
        step_anim(e, anims);
        break;
    default:
        /* State 8: dormant, anim only. */
        step_anim(e, anims);
        break;
    }
}

/* Child dispatch on the roles (the native slot-table callbacks). */
static void dth_child_update(AosEnemy *e, AosSoma *soma,
                             const AosAnimSet *anims, AosHitReport *hit,
                             uint32_t (*random)(void)) {
    switch (e->role) {
    case AOS_ROLE_DEATH_MIRROR:
        dth_mirror_update(e, anims);
        break;
    case AOS_ROLE_DEATH_MID:
        dth_mid_update(e, anims, hit);
        break;
    case AOS_ROLE_DEATH_MID_TAIL:
        dth_tail_update(e, anims);
        break;
    case AOS_ROLE_DEATH_SCYTHE:
        dth_scythe_update(e, soma, anims, hit, random);
        break;
    case AOS_ROLE_DEATH_SHOT:
        e->x += e->vx;
        e->y += e->vy;
        if (e->timer && (uint8_t)--e->timer == 0)
            e->removed = true;
        break;
    case AOS_ROLE_DEATH_GORE:
        e->vy += 0x2000;
        e->x += e->vx;
        e->y += e->vy;
        if (e->timer && (uint8_t)--e->timer == 0)
            e->removed = true;
        break;
    default:
        break;
    }
}

AosHitReport aos_enemy_death_update(AosEnemy *e, AosSoma *soma,
                                    const AosCollision *layer,
                                    const AosEnemyKind *kind,
                                    const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames,
                                    int soma_atk, int soma_def,
                                    int cam_x, int cam_y,
                                    uint32_t (*random)(void)) {
    const AosAnimSet *anims = kind->anims;
    AosHitReport hit;
    memset(&hit, 0, sizeof hit);
    (void)layer; (void)soma_atk; (void)soma_def; (void)cam_x; (void)cam_y;
    (void)weapon; (void)weapon_frames;

    /* Child roles dispatch first. */
    if (e->role >= AOS_ROLE_DEATH_MIRROR && e->role <= AOS_ROLE_DEATH_GORE) {
        dth_child_update(e, soma, anims, &hit, random);
        return hit;
    }
    if (!dth_is_death(e->id))
        return hit;

    /* The create's child emission runs on the first updates. */
    dth_spawn_children(&hit, e, random);

    /* The sub_080BF4AC/sub_080BF504 lethal path: a lethal blow marks
     * defeated, flashes, requests the death song and enters state 7
     * with the + 0xD = 0x3C timer. */
    if (e->hp <= 0 && !e->defeated && e->state != 7 && e->state != 8) {
        e->defeated = true;
        e->hit_flash = 0xFF;
        aos_audio_play_song(0x6E);
        e->state = 7;
        e->step = e->substep = 0;
        e->timer = 0x3C;
        e->dth_atk = 0;
    }

    dth_state_machine(&hit, e, soma, anims, random);

    /* The shared collide pass (the + 0x3E & 2 gate skips it in the
     * state-2 dive, matching the native early-out). */
    if (!(e->state == 2 && (e->dth_flags & 2))) {
        AosHitReport c = aos_enemy_collide(e, soma, kind, weapon,
                                           weapon_frames, soma_atk,
                                           soma_def);
        aos_enemy_take_effects(&hit, &c);
        for (int i = 0; i < c.child_count &&
             hit.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
            hit.children[hit.child_count++] = c.children[i];
    }
    return hit;
}
