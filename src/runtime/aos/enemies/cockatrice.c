/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Cockatrice (enemy 0x2E), its shadow and rock.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos_enemy_internal.h"
#include "../../aos_audio.h"

#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define ground_walk aos_enemy_ground_walk
#define outside_margins aos_enemy_outside_margins
#define pixel_delta aos_enemy_pixel_delta
#define player_in_centred aos_enemy_player_in_centred
#define play_loop aos_enemy_play_loop
#define skeleton_corner aos_enemy_skeleton_corner
#define skeleton_place aos_enemy_skeleton_place
#define spawn_drift aos_enemy_spawn_drift
#define step_anim aos_enemy_step_anim
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y

/* sub_080BB8B0: the shadow sits at the parent's attachment component 4
 * plus the ROM constant -10 (0x08119490), the x offset negated by the
 * facing; approximated by the exported frame edge like the Waiter
 * companion. */
static void cockatrice_shadow_place(AosEnemy *e, const AosEnemy *parent,
                                    const AosEnemyKind *kind) {
    int dx, dy;
    skeleton_corner(parent, kind, &dx, &dy);
    e->mirrored = parent->mirrored;
    skeleton_place(e, parent, dx + (parent->mirrored ? 10 : -10), dy);
}

/* sub_080BB924: the shadow child. State 0 loads the shared resources,
 * draws a random palette nibble (not ported), sets + 0x59 = 2, + 0x1C =
 * 0x10000, + 0x20 = 8 and enters state 1. State 1 rides the parent and
 * drifts + 0x1A down by 0x2B0 a frame; state 2 (the parent dying) drifts
 * by 0x17B0 with + 0x20 = 0x10; state 3 deletes. The shared tail turns
 * + 0x1A into the bob (+ 0x18) and spin (+ 0x1C) of the affine draw —
 * approximated by aos_sine — and deletes it once the parent's + 0x18
 * bit 0 corpse mark is set. */
void aos_enemy_cockatrice_shadow_update(AosEnemy *e, const AosEnemyKind *kind) {
    const AosEnemy *parent = e->parent;
    if (!parent) {
        e->removed = true;
        return;
    }
    switch (e->state) {
    case 0:
        e->ck_phase = 0;
        e->ck_orbit = 8;
        e->state = 1;
        break;
    case 1:
        cockatrice_shadow_place(e, parent, kind);
        if (parent->state == 2) e->state = 2;
        e->ck_phase = (int16_t)(e->ck_phase - 0x2B0);
        break;
    case 2:
        cockatrice_shadow_place(e, parent, kind);
        e->ck_phase = (int16_t)(e->ck_phase - 0x17B0);
        e->ck_orbit = 0x10;
        break;
    default:
        break;
    }
    /* The affine draw: + 0x1C = cos-like spin around -0x2000 plus a small
     * + 0x18 bob, both approximated through aos_sine. */
    int32_t w = e->mirrored ? -(int32_t)e->ck_orbit : (int32_t)e->ck_orbit;
    e->angle = (uint32_t)(0xE000 +
        ((int32_t)((int64_t)aos_sine((uint32_t)e->ck_phase + 0x2000) >> 4) * w >> 8));
    if (parent->ck_flags & 1) e->state = 3;
    if (e->state == 3 || parent->removed) e->removed = true;
    step_anim(e, kind->anims);
}

/* sub_080BBAC4: the rock the attack throws at animation frame 0xA. The
 * native spawn keeps a sub_0803E654 sign draw (~half delete,
 * approximated by the RNG), aims + 0x18 by the facing, derives + 0x28 /
 * + 0x2C from the sine table and a speed factor read off + 0x58 / + 0x59
 * (approximated by a fixed 1.5 px/frame toss toward Soma), spins + 0x18
 * by 8 a frame, rises for the first 0x10 ticks of + 0x1A and dies past
 * 0x1F with sound 0x65. Its + 0x14 is the parent, + 0x5C the parent's
 * contact + 1 and + 0x65 the direct frame 1; the + 0x4 trail callback,
 * the + 0x24 scale and the palette nibble are not ported. */
AosHitReport aos_enemy_cockatrice_rock_update(AosEnemy *e, AosSoma *soma,
                                           const AosCollision *layer,
                                           const AosEnemyKind *kind,
                                           const AosWeaponEntity *weapon,
                                           const AosWeaponFrames *weapon_frames,
                                           int soma_atk, int soma_def, int cam_x,
                                           int cam_y, uint32_t (*random)(void)) {
    (void)layer;
    AosHitReport report = collide(e, soma, kind, weapon, weapon_frames,
                                  soma_atk, soma_def);
    if (e->state == 0) {
        if (random() & 0x8000) {
            e->removed = true;
            return report;
        }
        e->mirrored = e->parent ? e->parent->mirrored : e->mirrored;
        face_player(e, soma);
        /* Aimed toss: 1.5 px/frame toward the player plus the native
         * early rise. */
        int32_t dx = pixel_delta(target_x(soma), e->x);
        int32_t dy = pixel_delta(target_y(soma), e->y);
        int32_t d = (int32_t)aos_isqrt((uint32_t)(dx * dx + dy * dy));
        if (d > 0) {
            e->vx = dx * 0x18000 / d;
            e->vy = dy * 0x18000 / d - 0x8000;
        } else {
            e->vx = e->vy = 0;
        }
        e->ck_phase = 0;
        e->angle = e->mirrored ? 0xFFFFEC00u : 0x6C00u;
        e->state = 1;
    } else if (e->state == 1) {
        e->ck_phase++;
        e->angle = (uint32_t)((int32_t)e->angle + (e->mirrored ? -8 : 8) * 0x100);
        if (e->ck_phase <= 0xF) e->vy -= 0x2000;
        e->x += e->vx;
        e->y += e->vy;
        if (e->ck_phase > 0x1F) {
            aos_audio_play_song(0x65);
            e->removed = true;
        }
    } else {
        e->removed = true;
    }
    if (outside_margins(kind, 1, (e->x >> 16) - cam_x, (e->y >> 16) - cam_y))
        e->removed = true;
    return report;
}

/* sub_080BBCC4: the state-0 patrol. Step 0 arms step 1; the walk uses
 * the probe walker 0x085288C0 (mode 0x14) at ((frame + 1) % 8 <= 4 ?
 * ((frame + 1) % 8 << 14) + 0x1000 : 0). While the walker reports a
 * clean floor ((flags & 0x1B) == 8) each animation loop boundary
 * (frame 0, tick 0) counts + 0xC; past 3 the facing flips and + 0xC
 * resets. A nonzero integer velocity (+ 0x4A) toggles the facing bit.
 * The 0xF0 x 0x64 centred box of sub_0806E29C or any damage
 * (sub_0806D54C: hp below the record's) wakes it into state 1. */
static void cockatrice_walk(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                            const AosEnemyKind *kind) {
    if (e->step == 0) {
        e->step = 1;
    }
    unsigned m = (e->anim.frame + 1) % 8;
    int32_t speed = m <= 4 ? (int32_t)(m << 14) + 0x1000 : 0;
    int result = ground_walk(e, soma, layer, &kind->probes, speed, 0x14);
    if ((result & 0x1B) == 8 && e->anim.frame == 0 && e->anim.tick == 0) {
        uint8_t old = e->substep;
        e->substep = old + 1;
        if (old > 2) {
            e->mirrored = !e->mirrored;
            e->substep = 0;
        }
    }
    if ((int16_t)(e->vx >> 16) != 0) e->mirrored = !e->mirrored;
    if (player_in_centred(e, soma, 0xF0, 0x64) || e->hp < e->stats.hp) {
        e->state = 1;
        e->substep = e->step = 0;
    }
}

/* sub_080BBD90: the state-1 attack. The same walk; at each animation
 * loop boundary the facing side check (mirrored wants the target to
 * its right, unmirrored to its left — sub_0806BBC4 approximated by the
 * target side), a 0x77 pixel reach (sub_0806D044) and the gEwramData
 * + 0x13270 bit 2 gate (not modelled) start step 2 with animation 1
 * once, then sub_0806BC40 faces the player. Step 2 spawns the rock at
 * frame 0xA tick 0 (its + 0x14 the parent, + 0x59 bit 1, + 0x3C the
 * parent's contact * 7 / 8) and, at the animation's end, arms the
 * 0x20 timer of step 3 which returns to the patrol. */
static void cockatrice_attack(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                              const AosEnemyKind *kind,
                              AosHitReport *spawned, uint32_t (*random)(void)) {
    (void)random;
    if (e->step == 0) e->step = 1;
    if (e->step <= 1) {
        unsigned m = (e->anim.frame + 1) % 8;
        int32_t speed = m <= 4 ? (int32_t)(m << 14) + 0x1000 : 0;
        ground_walk(e, soma, layer, &kind->probes, speed, 0x14);
        if (e->anim.frame == 0 && e->anim.tick == 0) {
            int dir = target_x(soma) > e->x ? 1 : -1;
            bool side = e->mirrored ? dir == 1 : dir == -1;
            int32_t dx = pixel_delta(target_x(soma), e->x);
            int32_t dy = pixel_delta(target_y(soma), e->y);
            int32_t dist = (int32_t)aos_isqrt((uint32_t)(dx * dx + dy * dy));
            if (side && dist <= 0x77) {
                e->step = 2;
                play_loop(e, kind->anims, 1, false);
            }
            face_player(e, soma);
        }
    } else if (e->step == 2) {
        if (e->anim.frame == 0xA && e->anim.tick == 0 &&
            spawned->child_count < AOS_ENEMY_MAX_CHILDREN) {
            AosEnemy r = {0};
            r.id = e->id;
            r.role = AOS_ROLE_COCKATRICE_ROCK;
            r.x = e->x;
            r.y = e->y;
            r.mirrored = e->mirrored;
            r.stats = e->stats;
            r.hp = 1;
            r.static_frame = 1;
            /* The still frame's attack record is empty; the native
             * contact is the sub_080BC0F4 / sub_08021654 callback, so a
             * small box stands in for it. */
            r.own_boxes = true;
            r.own_hurt = r.own_attack = (AosBox){-6, -6, 12, 12};
            r.parent = e;
            r.combat.type = AOS_TYPE_ENEMY;
            r.combat.hurt_off = true;
            spawned->children[spawned->child_count++] = r;
        }
        if (e->anim.flags & AOS_ANIM_ENDED) {
            e->timer = 0x20;
            e->step = 3;
        }
    } else {
        if (e->timer) e->timer--;
        else {
            e->state = 0;
            e->substep = e->step = 0;
            play_loop(e, kind->anims, 0, true);
        }
    }
}

/* sub_080BBEE4: the state-2 death. Step 0 plays sound 0x65 and
 * animation 2 once with the 0x40 timer; step 1 drifts (vx += 0x3A00,
 * vy += -0x6000), plays song 0x6E every eighth tick, bursts gore every
 * other tick (the native sub_08045CEC spawn approximated by the blood
 * effect) and blinks (the + 0x5A palette merge is not ported); at 0 it
 * marks + 0x18 bit 0 and step 2 deletes. */
static bool cockatrice_die(AosEnemy *e, const AosEnemyKind *kind,
                           AosHitReport *report, uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        aos_audio_play_song(0x65);
        play_loop(e, kind->anims, 2, false);
        e->timer = 0x40;
        e->step = 1;
        break;
    case 1:
        e->vx += 0x3A00;
        e->vy -= 0x6000;
        if (!(e->timer & 7)) aos_audio_play_song(0x6E);
        if (!(e->timer & 1)) {
            int32_t ox = (int32_t)(random() % 0x60000) - 0x30000;
            int32_t oy = (int32_t)(random() % 0x60000) - 0x30000;
            spawn_drift(AOS_EFFECT_BLOOD, e->x + ox, e->y + oy, report);
        }
        if (e->timer) e->timer--;
        else {
            e->step = 2;
            e->ck_flags |= 1;
        }
        break;
    default:
        return true;
    }
    (void)random;
    return false;
}

/* EnemyCockatriceUpdate: sub_0806E4F8 hides it above y -0x20; outside
 * state 2 the sub_0806D128 margin 6 check and the sub_0806AD24 stun gate
 * skip the state machine (the room query of sub_080421AC is not ported);
 * state 3 only steps its animation. */
AosHitReport aos_enemy_cockatrice_update(AosEnemy *e, AosSoma *soma,
                                      const AosCollision *layer,
                                      const AosEnemyKind *kind,
                                      const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames,
                                      int soma_atk, int soma_def, int cam_x,
                                      int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    e->hidden = (int16_t)(e->y >> 16) < -0x20;
    if (e->state != 2 &&
        outside_margins(kind, 6, (e->x >> 16) - cam_x, (e->y >> 16) - cam_y))
        return report;
    bool run = e->state == 2 || !(e->hp > 0 && e->hit_flash);
    if (run) {
        switch (e->state) {
        case 0:
            if (!e->helper_spawned) {
                e->helper_spawned = true;
                if (spawned.child_count < AOS_ENEMY_MAX_CHILDREN) {
                    AosEnemy s = {0};
                    s.id = e->id;
                    s.role = AOS_ROLE_COCKATRICE_SHADOW;
                    s.x = e->x;
                    s.y = e->y;
                    s.mirrored = e->mirrored;
                    s.stats = e->stats;
                    s.hp = 1;
                    s.static_frame = -1;
                    s.parent = e;
                    s.combat.type = AOS_TYPE_ENEMY;
                    s.combat.attack_off = s.combat.hurt_off = true;
                    spawned.children[spawned.child_count++] = s;
                }
            }
            cockatrice_walk(e, soma, layer, kind);
            break;
        case 1:
            cockatrice_attack(e, soma, layer, kind, &spawned, random);
            break;
        case 2:
            if (cockatrice_die(e, kind, &report, random)) e->removed = true;
            break;
        case 3:
            step_anim(e, kind->anims);
            goto out;
        default:
            break;
        }
        if (!e->removed) step_anim(e, kind->anims);
    }
    if (!e->removed)
        report = collide(e, soma, kind, weapon, weapon_frames, soma_atk,
                         soma_def);
out:
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    return report;
}

/* The Chronomage's time-magic wisp (sub_080B6C3C, spawned by the
 * state-1 cutscene) shares its resources, pulses its affine scale and
 * deletes itself; it has no contact or hurt callbacks. */
