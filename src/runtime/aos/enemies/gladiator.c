/* Gladiator (Aria of Sorrow enemy id 0x5D), ported from
 * EnemyGladiatorCreate (0x080A07FC) / EnemyGladiatorUpdate
 * (0x080A1D64) and the cluster sub_080A0A8C..sub_080A1E00 of
 * code_080A0A8C.s.
 *
 * The native actor is a three-limb knight on the shared entity
 * table: the body plus three persistent children spawned by
 * sub_0806DFF8 — the shield arm (sub_080A0DEC, attach slot + 0x20),
 * the sword arm (sub_080A1060, attach slot + 0x20, its own attack
 * phase machine) and the flail ball on its chain
 * (sub_080A13D0, attach slot + 0x21).  The sword arm's shots are
 * sub_080A15CC projectiles (entity 0x1B/0x48, update sub_080A1740)
 * each trailed by a sub_080A18C0 afterimage (sub_080A19C4); the
 * body attack's knives come from sub_0806D9CC; and the death
 * throws seven sub_080A1A50 chunks (update sub_080A1B98).
 *
 * Head fields: + 0xA state, + 0xB attack substate, + 0xC death
 * one-shot, + 0xD attack timer, + 0x14 child-gore flag, + 0x18
 * variant, + 0x2C/+ 0x21 attach bytes, + 0x36 id, + 0x5C draw,
 * + 0x6D/+ 0x6E/+ 0x6F animation frame selectors (children mirror
 * the parent's through their + 0x14 link). */
#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../core/aos_collision.h"
#include "../core/aos_soma.h"
#include "../math/aos_math.h"
#include <stdlib.h>

#define copy_result aos_enemy_copy_collision_result
#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define target_x(...) aos_enemy_target_x(__VA_ARGS__)
#define target_y(...) aos_enemy_target_y(__VA_ARGS__)
#define player_in aos_enemy_player_in
#define player_near aos_enemy_player_near
#define outside_margins aos_enemy_outside_margins
#define step_anim aos_enemy_step_anim
#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define random_centred(r, n) aos_enemy_random_centred((r), (n))
#define move_toward aos_enemy_move_toward
#define walk_collide aos_enemy_walk_collide
#define integrate aos_enemy_integrate
#define at_room_edge aos_enemy_at_room_edge
#define blocked_ahead aos_enemy_blocked_ahead
#define generic_death aos_enemy_generic_death

static bool gl_is_gladiator(uint8_t id) { return id == AOS_ENEMY_GLADIATOR; }

static int32_t gl_sin(uint32_t a) { return aos_sine(a & 0xFFFF); }
static int32_t gl_cos(uint32_t a) { return aos_sine((a + 0x4000) & 0xFFFF); }
static int32_t gl_mul(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * b) >> 16);
}

/* ROM offset tables of sub_080A0A8C / sub_080A0DEC / sub_080A1060:
 * 0x08119144/5C pair the body's knife-volley x/y offsets (indexed by
 * the + 0x6E frame), 0x08119174 the y kick table, 0x0811917C/94 the
 * limbs' attach offsets. */
static const int16_t gl_tbl144[12] = {30, 29, 12, 11, 10, 9, 8, 7, 6, 5, 0, -1};
static const int16_t gl_tbl15C[12] = {3, 2, -10, -11, -12, -12, -13, -13, -14, -14, -23, -24};
static const int16_t gl_tbl174[4] = {2, 1, 0, 1};
static const int16_t gl_tbl17C[12] = {15, 14, 4, 3, 2, 1, 0, -3, -4, -5, -6, -6};
static const int16_t gl_tbl194[12] = {28, 27, 8, 7, 5, 2, 0, -1, -2, -3, -20, -21};

static void gl_shot_spawn(AosEnemy *c, AosHitReport *report,
                          const AosAnimSet *anims, uint32_t (*random)(void));

/* ------------------------------------------------------------------ */
/* Children                                                             */
/* ------------------------------------------------------------------ */

/* The shared limb prologue of sub_080A0DEC/sub_080A1060/sub_080A13D0:
 * the child must answer a live Gladiator parent, mirrors the parent's
 * facing/palette bits and takes the attach offsets. */
static bool gl_limb_guard(AosEnemy *c) {
    AosEnemy *p = c->parent;
    if (!p || p->removed || !gl_is_gladiator(p->id)) {
        c->removed = true;
        return false;
    }
    c->hidden = false;
    /* + 0x5A attach-byte merge and the + 0x58 mirror bit 6. */
    c->me_ang = 0; /* unused here */
    c->gl_ox = p->gl_ox;
    c->gl_oy = p->gl_oy;
    return true;
}

/* sub_080A0DEC: the shield arm (part 0).  During the parent's death
 * (state 1) it throws the seven death chunks through sub_080A1A50. */
static void gl_part0_update(AosEnemy *c, AosHitReport *report,
                            const AosAnimSet *anims,
                            uint32_t (*random)(void)) {
    if (!gl_limb_guard(c)) return;
    AosEnemy *p = c->parent;
    if (p->state == 1) {
        /* sub_080A1A50(c, x, c->y, p->+0x48): seven chunks. */
        for (int i = 0; i < 7 && report->child_count < AOS_ENEMY_MAX_CHILDREN;
             ++i) {
            AosEnemy *g = &report->children[report->child_count++];
            *g = *c;
            g->role = AOS_ROLE_GLADIATOR_GORE;
            g->parent = p;
            g->state = 0;
            g->step = g->substep = 0;
            g->hp = 1;
            g->hit_flash = 0;
            g->defeated = false;
            g->hidden = false;
            g->removed = false;
            g->gl_part = (uint8_t)i;
            g->gl_atk = g->gl_atk2 = 0;
            g->gl_ang = g->gl_ang2 = 0;
            g->timer = (uint8_t)(0x28 + (random_centred(random, 4) & 3));
            g->x = p->x + (i & 1 ? AOS_FIXED(8) : -AOS_FIXED(8));
            g->y = p->y - AOS_FIXED(8);
            g->vx = (int32_t)(random_centred(random, 0x20000));
            g->vy = (int32_t)(random_centred(random, 0x20000)) - 0x10000;
            g->ax = g->ay = 0;
            g->combat.attack_off = true;
            g->combat.hurt_off = true;
        }
        return;
    }
    /* Attached idle: ride the parent's position and mirror; the
     * +0x6D == 0xC attack anim adds the 0x08119174 y kick indexed by
     * the +0x6E frame pair (sub_080A0DEC's sp + 0x10 table). */
    c->x = p->x + AOS_FIXED(c->gl_ox);
    c->y = p->y + AOS_FIXED(c->gl_oy);
    if (p->anim.id == 0xC)
        c->y += AOS_FIXED(gl_tbl174[p->anim.frame & 3]);
    step_anim(c, anims);
}

/* sub_080A1060: the sword arm (part 1) — idle timer +0x1C refilled
 * Random & 0x3F, attack phases +0x6D: 1 windup, 3 strike spawning
 * sub_080A15CC shots with song 0x89. */
static void gl_part1_update(AosEnemy *c, AosEnemy *p, AosHitReport *report,
                            const AosAnimSet *anims,
                            uint32_t (*random)(void)) {
    if (!p || p->removed || !gl_is_gladiator(p->id)) {
        c->removed = true;
        return;
    }
    c->hidden = false;
    c->x = p->x + AOS_FIXED(c->gl_ox);
    c->y = p->y + AOS_FIXED(c->gl_oy);
    if (p->state == 1) {
        /* Parent dying: the arm powers down. */
        c->hidden = false;
        step_anim(c, anims);
        return;
    }
    /* sub_080A1060's attack-phase machine: +0x6D phases 0 idle /
     * 1 windup / 3 strike; +0x1C refills Random & 0x3F; the strike
     * fires sub_080A15CC shots at the +0x6E frames 3/9/15 with the
     * 0x89 song on the first strike. */
    if (c->gl_atk == 0) {
        if (--c->gl_life <= 0) {
            c->gl_life = random_centred(random, 0x40) & 0x3F;
            c->gl_atk = (random_centred(random, 0x80) > 0x1F) ? 3 : 1;
            play(c, anims, c->gl_atk);
        }
    } else {
        int f = c->anim.frame;
        bool strike = (c->gl_atk == 1 && f == 3) ||
                      (c->gl_atk == 3 && (f == 3 || f == 9 || f == 0xF));
        if (strike && !c->gl_atk2) {
            c->gl_atk2 = 1;
            gl_shot_spawn(c, report, anims, random);
            if (c->gl_atk == 1) aos_audio_play_song(0x89);
        }
        if (c->anim.tick == 0 || f == 0) {
            c->gl_atk = 0;
            c->gl_atk2 = 0;
        }
    }
    step_anim(c, anims);
}

/* sub_080A13D0: the flail ball (part 2) — the chain spin +0x1C/+ 0x1E
 * and the sub_0803E058 rotation; on the parent's death it detaches
 * into the thrown state 1. */
static void gl_part2_update(AosEnemy *c, AosEnemy *p,
                            const AosAnimSet *anims) {
    if (!gl_limb_guard(c)) return;
    if (p->state == 1 && c->state == 0) {
        /* _080A1500: detach — state 1, timer 0x40, thrown. */
        c->state = 1;
        c->step = 0;
        c->timer = 0x40;
        c->gl_life = p->gl_life;
        c->vx = 0;
        c->vy = AOS_FIXED(-0xC0);
        c->ax = 0;
        c->ay = AOS_FIXED(0x10);
        c->hidden = false;
    }
    if (c->state == 0) {
        c->x = p->x + AOS_FIXED(c->gl_ox);
        c->y = p->y + AOS_FIXED(c->gl_oy);
        /* The chain spin: +0x1C accumulates ±+0x1E each tick. */
        if (p->hidden) c->gl_ang -= c->gl_ang2;
        else c->gl_ang += c->gl_ang2;
        if (c->gl_ang2 > 0x7FF) c->gl_ang2 += 0x18;
        step_anim(c, anims);
        return;
    }
    /* Thrown flight: sub_0806D490(0x20) decay + move 0x50000, the
     * continued spin, blink while +0xD <= 0x28, delete at 0. */
    c->vx = gl_mul(c->vx, 0x1F);
    c->vy = gl_mul(c->vy, 0x1F);
    c->x += c->vx + AOS_FIXED(5);
    c->y += c->vy + AOS_FIXED(5);
    c->gl_ang += c->gl_ang2;
    if (c->timer <= 0x28) {
        /* sub_0806D518 blink merge — approximated by hit_flash. */
        c->hit_flash = (c->timer & 1) ? 4 : 0;
    }
    step_anim(c, anims);
    if ((uint8_t)--c->timer == 0) c->removed = true;
}

/* sub_080A1740: the spinning shot — state 0 orbits the parent by the
 * +0x1E angle at radius 8.0 px, state 0xA fades out on a wall hit,
 * +0xD past 0xF0 fades too. */
static void gl_shot_update(AosEnemy *s, AosEnemy *p,
                           const AosAnimSet *anims,
                           const AosCollision *layer) {
    if (!p || p->removed || !gl_is_gladiator(p->id)) {
        s->removed = true;
        return;
    }
    if (s->state == 0xA) {
        if ((uint8_t)--s->timer == 0) s->removed = true;
        step_anim(s, anims);
        return;
    }
    /* +0x1E angle spin (wrap at 0x4000/0x10000), then the orbit:
     * x = parent + cos(a)*8 + fixed offset, y = parent + sin(a)*8. */
    s->gl_ang += 0x100;
    if (s->gl_ang > 0x4000) s->gl_ang -= 0x10000;
    s->x = p->x + gl_cos(s->gl_ang) * 8 + AOS_FIXED(4);
    s->y = p->y + gl_sin(s->gl_ang) * 8 + AOS_FIXED(-8);
    /* sub_0806D0B8 wall probe -> state 0xA with the 0x78 timer. */
    int sx = (int)(s->x >> 16), sy = (int)(s->y >> 16);
    if (blocked_ahead(layer, sx, sy) ||
        sx < -64 || sx > 64 * 30 || sy < -64 || sy > 26 * 32) {
        s->state = 0xA;
        s->timer = 0x78;
        return;
    }
    if (++s->substep > 0xF0) {
        s->state = 0xA;
        s->timer = 0x78;
        return;
    }
    step_anim(s, anims);
}

/* sub_080A19C4: the trail afterimage — mirrors the shot's orbit
 * position while the parent lives. */
static void gl_trail_update(AosEnemy *t, AosEnemy *s,
                            const AosAnimSet *anims) {
    if (!s || s->removed || s->role != AOS_ROLE_GLADIATOR_SHOT ||
        s->state != 0) {
        t->removed = true;
        return;
    }
    t->hidden = false;
    t->x = s->x;
    t->y = s->y;
    step_anim(t, anims);
}

/* sub_080A1B98: a death chunk — drift, spin, blink while +0xD <=
 * 0x28, delete at 0. */
static void gl_gore_update(AosEnemy *g, const AosAnimSet *anims) {
    g->x += g->vx;
    g->y += g->vy;
    g->gl_ang += g->gl_ang2;
    if (g->timer <= 0x28) g->hit_flash = (g->timer & 1) ? 4 : 0;
    step_anim(g, anims);
    if ((uint8_t)--g->timer == 0) g->removed = true;
}

/* sub_080A15CC: spawn the spinning shot (entity 0x1B/0x48) at the
 * limb's position plus its +0x48 memory, then its sub_080A18C0
 * trail.  Called by the sword arm's strike phases and by the body's
 * knife volley (approximated through the same role). */
static void gl_shot_spawn(AosEnemy *c, AosHitReport *report,
                          const AosAnimSet *anims, uint32_t (*random)(void)) {
    if (report->child_count + 2 > AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy *p = c->parent;
    AosEnemy *s = &report->children[report->child_count++];
    *s = *c;
    s->role = AOS_ROLE_GLADIATOR_SHOT;
    s->parent = p;
    s->state = 0;
    s->step = s->substep = 0;
    s->hp = 1;
    s->hit_flash = 0;
    s->defeated = false;
    s->hidden = false;
    s->removed = false;
    s->gl_part = 0;
    s->gl_atk = s->gl_atk2 = 0;
    s->gl_ang = (int16_t)((c->gl_ang + 0x8000) & 0xFFFF);
    s->gl_ang2 = 0;
    s->gl_life = c->gl_life;
    s->x = c->x;
    s->y = c->y;
    s->vx = s->vy = s->ax = s->ay = 0;
    s->combat.attack_off = false;
    s->combat.hurt_off = true;
    /* sub_080A18C0's trail child. */
    AosEnemy *t = &report->children[report->child_count++];
    *t = *s;
    t->role = AOS_ROLE_GLADIATOR_TRAIL;
    t->parent = s;
    t->combat.attack_off = true;
    t->combat.hurt_off = true;
    (void)anims;
    (void)random;
}

/* ------------------------------------------------------------------ */
/* Head                                                                 */
/* ------------------------------------------------------------------ */

AosHitReport aos_enemy_gladiator_update(
    AosEnemy *e, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0 };
    const AosAnimSet *anims = kind->anims;

    /* Descendant roles first. */
    if (e->role == AOS_ROLE_GLADIATOR_PART0) {
        gl_part0_update(e, &report, anims, random);
        return report;
    }
    if (e->role == AOS_ROLE_GLADIATOR_PART1) {
        gl_part1_update(e, e->parent, &report, anims, random);
        return report;
    }
    if (e->role == AOS_ROLE_GLADIATOR_PART2) {
        gl_part2_update(e, e->parent, anims);
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(&report, &hit);
        return report;
    }
    if (e->role == AOS_ROLE_GLADIATOR_SHOT) {
        gl_shot_update(e, e->parent, anims, layer);
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(&report, &hit);
        return report;
    }
    if (e->role == AOS_ROLE_GLADIATOR_TRAIL) {
        gl_trail_update(e, e->parent, anims);
        return report;
    }
    if (e->role == AOS_ROLE_GLADIATOR_GORE) {
        gl_gore_update(e, anims);
        return report;
    }
    if (e->role != AOS_ROLE_ENEMY || !gl_is_gladiator(e->id))
        return report;

    int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
    if (e->state != 0xFF && e->state != 2)
        e->hidden = outside_margins(kind, 4, sx, sy);
    if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 &&
        (uint16_t)sy <= 0xB0)
        report.targetable = true;

    /* Emit the three limbs on the first live update (sub_0806DFF8 x3
     * of EnemyGladiatorCreate). */
    if (!e->boss_parts_spawned && e->state < 2 &&
        report.child_count + 3 <= AOS_ENEMY_MAX_CHILDREN) {
        static const uint8_t roles[3] = {AOS_ROLE_GLADIATOR_PART0,
                                         AOS_ROLE_GLADIATOR_PART1,
                                         AOS_ROLE_GLADIATOR_PART2};
        for (int i = 0; i < 3; ++i) {
            AosEnemy *c = &report.children[report.child_count++];
            *c = *e;
            c->role = roles[i];
            c->parent = e;
            c->state = 0;
            c->step = c->substep = 0;
            c->timer = 0;
            c->hp = 1;
            c->hit_flash = 0;
            c->defeated = false;
            c->hidden = false;
            c->removed = false;
            c->gl_part = (uint8_t)i;
            c->gl_atk = c->gl_atk2 = 0;
            c->gl_ang = 0;
            c->gl_ang2 = 0x100;
            /* Attach offsets: the limbs ride just off the body. */
            c->gl_ox = (int16_t)(i == 2 ? -10 : (i == 0 ? -14 : 14));
            c->gl_oy = (int16_t)(i == 2 ? -20 : -8);
            c->gl_life = (i == 1) ? (random_centred(random, 0x40) & 0x3F) : 0;
            c->vx = c->vy = c->ax = c->ay = 0;
            c->combat.attack_off = true;
            c->combat.hurt_off = true;
            c->x = e->x + AOS_FIXED(c->gl_ox);
            c->y = e->y + AOS_FIXED(c->gl_oy);
            play_loop(c, anims, i == 2 ? 0x10 : (i == 0 ? 4 : 0), true);
            step_anim(c, anims);
        }
        e->boss_parts_spawned = true;
    }

    /* The lethal hook sub_080A0D7C: flash, hp loss handled by the
     * shared pass; on death song 0x71, sub_080683BC, state 1. */
    if (!e->defeated && e->hp <= 0 && e->state == 0) {
        e->hit_flash = 8;
        e->defeated = true;
        aos_audio_play_song(0x71);
        e->state = 1;
        e->step = 0;
        e->timer = 0x3C;
        e->gl_life = 0;
        play_loop(e, anims, 0xC, true);
    }

    switch (e->state) {
    case 0: {
        /* sub_080A0A8C: approach then the knife attack. */
        if (e->step == 0) {
            /* Approach: face the player and walk at ~3 px/frame;
             * the +0xD & 0x1F idle roar gate plays song 0x7D. */
            face_player(e, soma);
            move_toward(e, target_x(soma), target_y(soma) - AOS_FIXED(8),
                        AOS_FIXED(3));
            integrate(e);
            if ((e->timer & 0x1F) == 0 && !player_near(soma,
                    (int)(e->x >> 16) - cam_x, (int)(e->y >> 16) - cam_y,
                    0xB0, 0x110))
                aos_audio_play_song(0x7D);
            e->timer++;
            /* sub_0806D188 edge probe -> turn; reaching the target
             * (or the wall) starts the attack substate. */
            if (at_room_edge(e, layer) ||
                player_in(soma, (int16_t)((e->x >> 16) - 0x18),
                          (int16_t)((e->y >> 16) - 0x18), 0x30, 0x30)) {
                e->step = 1;
                e->timer = 0;
                play(e, anims, 0xD);
                if (!player_near(soma, sx, sy, 0xB0, 0x110))
                    aos_audio_play_song(0x127);
            }
        } else {
            /* Attack: the charge keeps moving; every 4th tick two
             * knife volleys spawn at the +0x6E-indexed table offsets
             * (the +0xD & 3 == 0 / == 2 pair of sub_0806D9CC calls). */
            move_toward(e, target_x(soma), target_y(soma) - AOS_FIXED(8),
                        AOS_FIXED(3));
            integrate(e);
            int ph = e->timer & 3;
            if (ph == 0 || ph == 2) {
                int fi = e->anim.frame < 12 ? e->anim.frame : 0;
                int ox = gl_tbl144[fi], oy = gl_tbl15C[fi];
                if (ph == 2) { ox = gl_tbl17C[fi]; oy = gl_tbl194[fi]; }
                if (report.child_count < AOS_ENEMY_MAX_CHILDREN) {
                    AosEnemy *s = &report.children[report.child_count++];
                    *s = *e;
                    s->role = AOS_ROLE_GLADIATOR_SHOT;
                    s->parent = e;
                    s->state = 0;
                    s->step = s->substep = 0;
                    s->timer = 0;
                    s->hp = 1;
                    s->hit_flash = 0;
                    s->defeated = false;
                    s->hidden = false;
                    s->removed = false;
                    s->gl_part = 0;
                    s->gl_atk = s->gl_atk2 = 0;
                    s->gl_ang = (int16_t)((e->gl_ang + ox * 0x100) & 0xFFFF);
                    s->gl_ang2 = 0;
                    s->x = e->x + AOS_FIXED(ox);
                    s->y = e->y + AOS_FIXED(oy);
                    s->vx = s->vy = s->ax = s->ay = 0;
                    s->combat.attack_off = false;
                    s->combat.hurt_off = true;
                }
            }
            e->timer++;
            /* The mirror-turn branch resets to the approach once the
             * charge crosses the target (the +0x59 & 0x40 path). */
            if (e->timer > 0x60) {
                e->step = 0;
                e->timer = 0;
                play(e, anims, 0xC);
            }
        }
        step_anim(e, anims);
        break;
    }
    case 1:
        /* sub_080A1E00: the death spin — once song 0x6B, the shared
         * drift, then generic death once the timer ends. */
        if (!e->gl_atk) {
            e->gl_atk = 1;
            aos_audio_play_song(0x6B);
        }
        e->x += e->vx;
        e->y += e->vy;
        e->timer--;
        if (e->timer == 0 && !e->removed) {
            /* Seven chunks already flew from part 0; the body itself
             * finishes through the generic death. */
            e->removed = true;
        }
        break;
    case 2:
        /* Dormant under gEwramData + 0x8E & 0x40: animation only. */
        step_anim(e, anims);
        break;
    default:
        break;
    }

    /* The hurt/contact pass of the native update tail. */
    if (e->state != 1 && e->state != 2) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(&report, &hit);
    }
    return report;
}
