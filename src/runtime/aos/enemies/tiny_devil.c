/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Tiny Devil (enemy 0x10) and Imp (0x31) with their orb and helper.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define generic_death aos_enemy_generic_death
#define integrate aos_enemy_integrate
#define make_static_child aos_enemy_make_static_child
#define merge_enemy_hit aos_enemy_merge_hit
#define move_toward aos_enemy_move_toward
#define outside_margins aos_enemy_outside_margins
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
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

static void tiny_devil_attack(AosEnemy *e, const AosSoma *soma,
                              const AosEnemyKind *kind, int cam_x, int cam_y,
                              AosHitReport *report, uint32_t (*random)(void)) {
    if (e->step == 0) {
        bool player_right = target_x(soma) > e->x;
        e->vx = player_right ? 0x20000 : -0x20000;
        e->ax = player_right ? -0x800 : 0x800;
        e->vy = -0x10000;
        e->ay = 0x800;
        e->timer = 0;
        e->step = 1;
    } else if (e->step == 1) {
        integrate(e);
        int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
        if (sx < 8 || sx > 0xE8 || sy < 8 || sy > 0x98) {
            e->state = 1;
            e->step = 0;
            e->rest = 0;
        } else if (++e->timer > 30) {
            play(e, kind->anims, 2);
            tiny_devil_orb(e, soma, kind, report, random);
            stop(e);
            e->timer = 0;
            e->step = 2;
        }
    } else if (++e->timer > 80) {
        e->state = 1;
        e->step = 0;
        e->rest = 0;
    } else if (e->timer == 40) {
        play(e, kind->anims, 1);
    }
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
        if (e->step == 0) {
            /* sub_080C580C / sub_080C4ED4 step 0: sound 0x72. */
            aos_audio_play_song(0x72);
            if (imp) imp_death_child(e, &report);
            e->step = 1;
        }
        e->y += e->vy;
        generic_death(e, kind, random, &report);
        aos_combat_tick(&e->combat);
        return report;
    }
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x10) > 0x110 || (uint16_t)sy > 0xB0) return report;
    if (e->state == 1)
        devil_idle(e, soma, kind, cam_x, cam_y, random);
    else if (!imp && e->state == 2)
        tiny_devil_attack(e, soma, kind, cam_x, cam_y, &report, random);
    else if (imp && e->state == 2)
        imp_approach(e, soma);
    else if (imp && e->state == 3)
        imp_control(e, soma, &report);
    step_anim(e, kind->anims);
    AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    merge_enemy_hit(&report, hit);
    aos_combat_tick(&e->combat);
    return report;
}
