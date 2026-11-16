/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Tiny Devil (enemy 0x10) and Imp (0x31) with their orb and helper.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#define collide aos_enemy_collide
#define death_flames aos_enemy_death_flames
#define face_player aos_enemy_face_player
#define generic_death aos_enemy_generic_death
#define integrate aos_enemy_integrate
#define make_static_child aos_enemy_make_static_child
#define merge_enemy_hit aos_enemy_merge_hit
#define move_toward aos_enemy_move_toward
#define outside_margins aos_enemy_outside_margins
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define player_dx aos_enemy_player_dx
#define player_dy aos_enemy_player_dy
#define step_anim aos_enemy_step_anim
#define stop aos_enemy_stop
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y

/* sub_080C5898: the Tiny Devil holds an aimed animation-4 orb beside its
 * body. The orb cycles animation 5 a short random number of times, changes
 * to animation 6 and then travels for 60 frames. */
static void tiny_devil_orb(AosEnemy *owner, const AosSoma *soma,
                           const AosEnemyKind *kind, AosHitReport *report,
                           uint32_t (*random)(void)) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy orb;
    make_static_child(&orb, owner, AOS_ROLE_TINY_DEVIL_ORB, -1);
    orb.x += owner->x < target_x(soma) ? 0x100000 : -0x100000;
    orb.y += 0x80000;
    (void)move_toward(&orb, target_x(soma), target_y(soma) - 0x200000, 0x20000);
    orb.state = 0;
    orb.variant = (uint8_t)((random() % 5) + 1);
    orb.hp = 1;
    orb.own_boxes = true;
    orb.own_hurt = orb.own_attack = (AosBox){-5, -5, 10, 10};
    play_loop(&orb, kind->anims, 4, false);
    report->children[report->child_count++] = orb;
}

AosHitReport aos_enemy_tiny_devil_orb_update(AosEnemy *orb, AosSoma *soma,
                                           const AosEnemyKind *kind,
                                           const AosWeaponEntity *weapon,
                                           const AosWeaponFrames *weapon_frames, int soma_atk,
                                           int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    if (orb->state == 0) {
        step_anim(orb, kind->anims);
        if (orb->cycle_done) {
            play(orb, kind->anims, 5);
            orb->state = 1;
            orb->phase = 0;
        }
    } else if (orb->state == 1) {
        step_anim(orb, kind->anims);
        if (orb->cycle_done) {
            orb->cycle_done = false;
            if (++orb->phase > orb->variant) {
                /* sub_080C5A44: animation 6, then sound 0x89. */
                play_loop(orb, kind->anims, 6, false);
                aos_audio_play_song(0x89);
                orb->state = 2;
                orb->timer = 0;
                orb->scaled = true;
            }
        }
    } else {
        integrate(orb);
        step_anim(orb, kind->anims);
        int sx = (orb->x >> 16) - cam_x, sy = (orb->y >> 16) - cam_y;
        if (++orb->timer > 60 || sx < -32 || sx > 272 || sy < -32 || sy > 192) {
            orb->removed = true;
            return report;
        }
    }
    report = collide(orb, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (report.enemy_hit) orb->removed = true;
    aos_combat_tick(&orb->combat);
    return report;
}

/* sub_080C5010: Imp owns a hidden technical child for its entire lifetime.
 * State 3 makes it visible at Soma's position, 40 pixels above his feet. */
static void imp_helper(AosEnemy *owner, const AosEnemyKind *kind, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy helper;
    make_static_child(&helper, owner, AOS_ROLE_IMP_HELPER, -1);
    helper.parent = owner;
    helper.hidden = true;
    helper.combat.attack_off = helper.combat.hurt_off = true;
    play(&helper, kind->anims, 3);
    report->children[report->child_count++] = helper;
    owner->helper_spawned = true;
}

void aos_enemy_imp_helper_update(AosEnemy *helper, const AosSoma *soma,
                              const AosEnemyKind *kind) {
    if (!helper->parent || helper->parent->removed || helper->parent->defeated) {
        helper->removed = true;
        return;
    }
    bool active = helper->parent->state == 3 && helper->parent->step <= 2;
    if (active && helper->hidden) play(helper, kind->anims, 3);
    helper->hidden = !active;
    if (active) {
        helper->x = target_x(soma);
        helper->y = target_y(soma) - 0x280000;
        step_anim(helper, kind->anims);
    }
}

/* sub_080C50EC: Imp's death creates direct sprite frame 0x15, falling at
 * one and a half times the body's initial death velocity. */
static void imp_death_child(AosEnemy *owner, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy child;
    make_static_child(&child, owner, AOS_ROLE_IMP_DEATH, 0x15);
    child.y += 0x100000;
    child.vy = owner->vy + owner->vy / 2;
    child.timer = 0x32;
    child.combat.attack_off = child.combat.hurt_off = true;
    report->children[report->child_count++] = child;
}

void aos_enemy_imp_death_child_update(AosEnemy *child, const AosEnemyKind *kind,
                                   int cam_y) {
    child->y += child->vy;
    child->hidden = kind->blink[child->timer] & 1;
    if (--child->timer == 0 || (child->y >> 16) - cam_y > 0xAA) child->removed = true;
}

/* Imp's state-1 approximation (its native routines sub_080C47F8 and
 * friends are a different machine; documented in the parity row). The
 * shared struct fields keep the old meanings here: `rest` the idle
 * countdown, `walk_timer`/`speed` the hover target, `timer` the
 * arrive timeout. */
static void devil_idle(AosEnemy *e, const AosSoma *soma, const AosEnemyKind *kind,
                       int cam_x, int cam_y, uint32_t (*random)(void)) {
    bool imp = e->id == AOS_ENEMY_IMP;
    face_player(e, soma);
    if (e->step == 0) {
        if (!e->rest) e->rest = (uint16_t)(100 + random() % (imp ? 360 : 500));
        int32_t side = imp ? 0x460000 : 0x190000;
        e->walk_timer = target_x(soma) + (e->mirrored ? -side : side);
        e->speed = target_y(soma) + (imp ? -0x2D0000 : -0x460000);
        e->timer = 0;
        e->step = 1;
    }
    bool arrived = move_toward(e, e->walk_timer, e->speed, imp ? 0xA000 : 0xC000);
    integrate(e);
    if (arrived || ++e->timer >= 60) e->step = 0;
    if (e->rest) --e->rest;
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if (!e->rest && !outside_margins(kind, 4, sx, sy)) {
        stop(e);
        e->state = 2;
        e->step = 0;
        e->timer = 0;
    }
}

/* sub_080C542C state 1, Tiny Devil only: the hover. The native record
 * fields map onto shared struct members:
 *   +0x14 `leg`      signed rest countdown (100 + rand % 500 per roll)
 *   +0x16 `phase`    axis flags: bits 2/3 arm X/Y, bits 0/1 latch arrival
 *   +0x1C `walk_timer` signed hover X offset in pixels
 *   +0x20 `speed`    signed hover Y offset in pixels
 * The +0x0C idle substep of the native routine only runs when the aim
 * probe (sub_0806CD38/CD78) returns the body's own position — no target
 * at all — so it stays 0 on every real gameplay path and is not ported.
 * +0x6D is never written by EnemyTinyDevilCreate, so the animation
 * guards against variants 1 and 2 never trigger. */
static void tiny_devil_hover(AosEnemy *e, const AosSoma *soma,
                             const AosEnemyKind *kind, int cam_x, int cam_y,
                             uint32_t (*random)(void)) {
    int16_t dx = player_dx(e, soma);   /* sub_0806BD64 */
    int16_t dy = player_dy(e, soma);   /* sub_0806BDEC */
    if (e->leg) --e->leg;              /* +0x14 countdown */
    face_player(e, soma);              /* sub_0806BC40 */
    if (e->step == 0) {
        /* _080C54C0: roll the rest and both hover offsets. */
        if (!e->leg) e->leg = (int16_t)(0x64 + random() % 0x1F4);
        e->phase = 0;                  /* +0x16 = step (0) */
        play(e, kind->anims, 1);
        e->ax = 0x200;
        e->ay = 0x400;
        if (dx > 0) {
            e->walk_timer = (int32_t)(int16_t)(random() % 0x19) - 0x19;
            e->ax = -e->ax;
            e->phase = (uint16_t)(e->phase & ~5);
        } else {
            e->walk_timer = 0x19 - (int32_t)(random() % 0x19);
            e->phase = (uint16_t)(e->phase | 4);
        }
        if (dy > 0x19) {
            e->speed = (int32_t)(int16_t)(random() % 0x19) - 0x14;
            e->ay = -e->ay;
            e->phase = (uint16_t)(e->phase & ~9);
        } else {
            e->speed = -0x46 - (int32_t)(random() % 0x19);
            e->phase = (uint16_t)(e->phase | 8);
        }
        ++e->step;
    } else {
        /* _080C557A: drift on both axes. When the position delta crosses
         * the rolled offset the axis latches (bit 0 X / bit 1 Y) and the
         * acceleration flips — tripled when the speed already exceeds
         * 1.0 — to hold the hover point; velocity carrying past the
         * latch clears it and returns to step 0 for a re-roll. */
        uint16_t fl = e->phase;
        if (fl & 4) {
            if (fl & 1) {
                if (e->vx <= 0) { e->phase = (uint16_t)(fl & ~1); e->step = 0; }
            } else {
                if (e->vx > 0x18000) e->vx = 0x18000;
                if (dx >= e->walk_timer) {
                    e->phase = (uint16_t)(fl | 1);
                    e->ax = e->vx > 0x10000 ? -(e->ax * 3) : -e->ax;
                }
            }
        } else {
            if (fl & 1) {
                if (e->vx >= 0) { e->phase = (uint16_t)(fl & ~1); e->step = 0; }
            } else {
                if (e->vx < -0x18000) e->vx = -0x18000;
                if (dx <= e->walk_timer) {
                    e->phase = (uint16_t)(fl | 1);
                    e->ax = e->vx < -0x10000 ? -(e->ax * 3) : -e->ax;
                }
            }
        }
        fl = e->phase;                  /* _080C560E reloads +0x16 */
        if (fl & 8) {
            if (fl & 2) {
                if (e->vy <= 0) { e->phase = (uint16_t)(fl & ~2); e->step = 0; }
            } else {
                if (e->vy > 0x8000) e->vy = 0x8000;
                if (dy >= e->speed) {
                    e->phase = (uint16_t)(fl | 2);
                    e->ay = e->vy > 0x10000 ? -(e->ay * 3) : -e->ay;
                }
            }
        } else {
            if (fl & 2) {
                if (e->vy >= 0) { e->phase = (uint16_t)(fl & ~2); e->step = 0; }
            } else {
                if (e->vy < -0x8000) e->vy = -0x8000;
                if (dy <= e->speed) {
                    e->phase = (uint16_t)(fl | 2);
                    e->ay = e->vy < -0x10000 ? -(e->ay * 3) : -e->ay;
                }
            }
        }
    }
    /* _080C56A6: rest expired and inside the margin-4 view -> the
     * attack; velocities are zeroed and both substeps reset. */
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if (e->leg == 0 && !outside_margins(kind, 4, sx, sy)) {
        stop(e);
        e->state = 2;
        e->substep = 0;
        e->step = 0;
    }
    integrate(e);                      /* _080C56CC tail */
}

/* sub_080C56F0 state 2: the attack. Step 0 dashes toward the player
 * (vx +/-1.0 px/frame, ax +/-0x800 turning it back, vy -1.0 with
 * ay 0x800 arcing the leap — the native Y probes feed nothing here).
 * Step 1
 * faces the player and aborts to the hover when the body's integer
 * position leaves the 8-pixel screen box of sub_080C5BA4; every 31st
 * frame it plays animation 2 and fires the aimed orb (sub_080C5898),
 * then hangs motionless for 80 frames (animation 1 at frame 40) before
 * returning to the hover. */
static void tiny_devil_attack(AosEnemy *e, const AosSoma *soma,
                              const AosEnemyKind *kind,
                              AosHitReport *report, uint32_t (*random)(void)) {
    int16_t dx = player_dx(e, soma);
    if (e->step == 0) {
        /* _080C5724 */
        e->vy = 0;
        e->ay = 0;
        e->vx = 0x10000;
        e->ax = 0x800;
        if (dx <= 0) {
            e->vx = -0x10000;        /* ax stays +0x800 */
        } else {
            e->ax = -0x800;
        }
        e->vy = -0x10000;
        e->ay = 0x800;
        e->timer = 0;
        e->step = 1;
    } else if (e->step == 1) {
        /* _080C5754 */
        face_player(e, soma);
        int xi = (int16_t)(e->x >> 16), yi = (int16_t)(e->y >> 16);
        if (xi < 8 || xi > 0xE8 || yi < 8 || yi > 0x98) {
            e->state = 1;
            e->substep = 0;
            e->step = 0;
        } else {
            ++e->timer;              /* +0x0D, a u8 like the native byte */
            if (e->timer > 0x1E) {
                play(e, kind->anims, 2);
                tiny_devil_orb(e, soma, kind, report, random);
                stop(e);
                e->timer = 0;
                e->step = 2;
            }
        }
    } else {
        /* _080C57B2 */
        ++e->timer;
        if (e->timer > 0x50) {
            e->timer = 0;
            e->state = 1;
            e->substep = 0;
            e->step = 0;
        } else if (e->timer == 0x28) {
            play(e, kind->anims, 1);
        }
    }
    integrate(e);                      /* _080C57E8: every step */
}

static void imp_approach(AosEnemy *e, const AosSoma *soma) {
    if (e->step == 0) {
        bool player_right = target_x(soma) > e->x;
        e->vx = player_right ? 0x20000 : -0x20000;
        e->ax = player_right ? -0x800 : 0x800;
        e->vy = 0x20000;
        e->ay = -0x800;
        e->timer = 0;
        e->step = 1;
    } else if (e->step == 1) {
        integrate(e);
        if (++e->timer > 30) {
            stop(e);
            e->walk_timer = target_x(soma);
            e->speed = target_y(soma) - 0x200000;
            (void)move_toward(e, e->walk_timer, e->speed, 0x4000);
            e->timer = 0;
            e->step = 2;
        }
    } else if (e->step == 2) {
        integrate(e);
        if (++e->timer > 80) {
            e->ax = e->vx > 0 ? -0x800 : 0x800;
            e->ay = e->vy > 0 ? -0x800 : 0x800;
            e->step = 3;
        }
    } else {
        int32_t old_vx = e->vx, old_vy = e->vy;
        integrate(e);
        if ((old_vx <= 0 && e->vx >= 0) || (old_vx >= 0 && e->vx <= 0))
            e->vx = e->ax = 0;
        if ((old_vy <= 0 && e->vy >= 0) || (old_vy >= 0 && e->vy <= 0))
            e->vy = e->ay = 0;
        if (!e->vx && !e->vy) {
            e->state = 3;
            e->step = 0;
            e->timer = 0;
        }
    }
}

static void imp_control(AosEnemy *e, AosSoma *soma, AosHitReport *report) {
    if (e->step == 0) {
        stop(e);
        e->timer = 0;
        e->patience = 0;
        e->step = 1;
    } else if (e->step == 1) {
        int32_t tx = target_x(soma) + (soma->facing_left ? -0x100000 : 0x100000);
        bool arrived = move_toward(e, tx, target_y(soma) - 0x200000, 0x20000);
        integrate(e);
        if (arrived) {
            report->force_input = true;
            report->forced_held = (e->timer & 1) ? AOS_KEY_ATTACK : 0;
            ++e->timer;
            /* sub_080C4D34 waits until the forced B presses have put Soma
             * outside his neutral states 0, 4 and 8 for 20 frames. */
            if (soma->state != 0 && soma->state != 4 && soma->state != 8 &&
                ++e->patience > 20) {
                e->vx = e->x < target_x(soma) ? -0x10000 : 0x10000;
                e->vy = -0x10000;
                e->timer = 0;
                e->step = 2;
            }
        }
    } else if (e->step == 2) {
        integrate(e);
        if (++e->timer > 40) e->step = 3;
    } else {
        stop(e);
        e->state = 1;
        e->step = 0;
        e->rest = 0;
    }
}

AosHitReport aos_enemy_devil_update(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                                 const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk,
                                 int soma_def, int cam_x, int cam_y,
                                 uint32_t (*random)(void)) {
    AosHitReport report = {0};
    bool imp = e->id == AOS_ENEMY_IMP;
    if (imp && !e->helper_spawned) imp_helper(e, kind, &report);
    if ((imp && e->state == 4) || (!imp && e->state == 3)) {
        step_anim(e, kind->anims);
        return report;
    }
    if (e->state == 0xFF) {
        if (imp) {
            /* sub_080C4ED4 step 0: sound 0x72 and the direct-frame
             * death child; the shared generic death follows. */
            if (e->step == 0) {
                aos_audio_play_song(0x72);
                imp_death_child(e, &report);
                e->step = 1;
            }
            e->y += e->vy;
            generic_death(e, kind, random, &report);
        } else {
            /* sub_080C580C: step 0 plays song 0x72, runs the native
             * defeat bookkeeping (sub_080683BC, done by the shared hurt
             * pass) and sets +0x0D = 0x32, +0x65 = 0x14, vy = 0x8000 and
             * the +0x5A palette byte — the two draw-side bytes are not
             * modeled. Every step then does y += vy, removes the body
             * (+0x59 bit 3) once its integer y passes 0xAA and runs
             * sub_0806BE74(e, 7, 0x100000, 0xC0000). */
            if (e->step == 0) {
                aos_audio_play_song(0x72);
                e->timer = 0x32;
                e->vy = 0x8000;
                e->step = 1;
            }
            e->y += e->vy;
            if ((int16_t)(e->y >> 16) > 0xAA) e->removed = true;
            death_flames(e, kind, 7, 0x100000, 0xC0000, random, &report);
        }
        aos_combat_tick(&e->combat);
        return report;
    }
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x10) > 0x110 || (uint16_t)sy > 0xB0) return report;
    if (e->state == 1) {
        if (imp) devil_idle(e, soma, kind, cam_x, cam_y, random);
        else tiny_devil_hover(e, soma, kind, cam_x, cam_y, random);
    } else if (!imp && e->state == 2) {
        tiny_devil_attack(e, soma, kind, &report, random);
    } else if (imp && e->state == 2)
        imp_approach(e, soma);
    else if (imp && e->state == 3)
        imp_control(e, soma, &report);
    step_anim(e, kind->anims);
    AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    merge_enemy_hit(&report, hit);
    aos_combat_tick(&e->combat);
    return report;
}
