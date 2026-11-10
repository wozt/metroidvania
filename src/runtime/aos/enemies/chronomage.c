/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Chronomage (enemy 0x28) and its wisps.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#define collide aos_enemy_collide
#define ground_walk aos_enemy_ground_walk
#define outside_margins aos_enemy_outside_margins
#define play_loop aos_enemy_play_loop
#define spawn_flame aos_enemy_spawn_flame
#define step_anim aos_enemy_step_anim
#define target_x aos_enemy_target_x

/* sub_080B6C3C: state 0 loads the shared resources, keeps the
 * sub_0803E654 sign draw (~half delete, approximated by the RNG bit
 * 15), the + 0x58 low-5 palette nibble (not ported), + 0x59 |= 0x10,
 * + 0x5C = 0x12, plays animation 6 once and enters state 1. State 1
 * ramps + 0x18 by 0x1000 a frame to 0x10000 then state 2 with the
 * 0x20 timer; state 2 counts down to state 3; state 3 decays + 0x18
 * by 0x2000 a frame and deletes at 0 (the + 0x59 & 0x10 palette
 * restore of sub_0803E6BC is not ported). The shared tail draws it
 * through sub_0803E058 with + 0x18 — approximated by the spin field
 * and the affine renderer. */
void aos_enemy_chrono_wisp_update(AosEnemy *e, const AosEnemyKind *kind,
                               uint32_t (*random)(void)) {
    switch (e->state) {
    case 0:
        /* sub_0803E654's sign draw (~half delete, approximated by the
         * RNG bit 15); the + 0x58 palette nibble is not ported. */
        if (random() & 0x8000) {
            e->removed = true;
            return;
        }
        e->spin = 0;
        play_loop(e, kind->anims, 6, false);
        e->cm_flags |= 0x10;
        e->state = 1;
        break;
    case 1:
        e->spin += 0x1000;
        if (e->spin > 0xFFFF) {
            e->state = 2;
            e->timer = 0x20;
        }
        break;
    case 2:
        if (e->timer) e->timer--;
        else e->state = 3;
        break;
    case 3:
        if (e->spin > 0) e->spin -= 0x2000;
        else e->removed = true;
        break;
    default:
        e->removed = true;
        break;
    }
    /* sub_0803E058's affine slot takes + 0x18 as the scale argument;
     * the renderer's affine_sx/sy are divisors, so 0x10000 maps to
     * the natural size and smaller ramps shrink the wisp. */
    if (!e->removed && e->spin > 0) {
        int32_t s = (int32_t)(((int64_t)0x10000 * 0x10000) / e->spin);
        e->affine_sx = e->affine_sy = s > 0 ? s : 1;
    }
    if (!e->removed) step_anim(e, kind->anims);
}

/* sub_080B6D68: the state-0 dormant watch. The gEwramData + 0x4BE
 * nonzero sends it to the chase (approximated by the damaged flag —
 * the native global is not modelled); else the target distance over
 * the + 0x42 words within 0x6F, or the target's + 0x10 & 0x360 action
 * bits (approximated by AOS_FLAG_ATTACKING), starts state 1. */
static void chrono_idle(AosEnemy *e, AosSoma *soma) {
    int32_t dx = (int32_t)(int16_t)((target_x(soma) - e->x) >> 16);
    if (dx < 0) dx = -dx;
    if (e->hp < e->stats.hp) {
        e->state = 2;
        e->step = e->substep = 0;
    } else if (dx <= 0x6F ||
               (soma->flags & (AOS_FLAG_ATTACKING | AOS_FLAG_AIR_ATTACK))) {
        e->state = 1;
        e->step = e->substep = 0;
    }
}

/* sub_080B6DEC: the state-1 time-stop sequence, an eight-step machine
 * on + 0xB. Step 0 plays animation 1 once unless the target entity's
 * + 0x10 & 0x360 action bits are set (approximated by the attack
 * flags); step 1 waits out the animation; step 2 plays animation 3
 * once with the 0x18 timer; step 3 counts it down, playing song 0xEC
 * when the post-decrement value is 8; step 4 ramps + 0xD to 0x10
 * while the native writes the gEwramData + 0x12FE4 time scale and
 * gates on + 0x4BE / sub_080212F4(6) — none of those globals are
 * modelled, so it collapses to the fade path; step 5 is the
 * sub_08011F44 / sub_08011AD0 palette fade (not ported, one tick);
 * step 6 restores the time scale and waits out animation 3, then
 * spawns the sub_080B6C3C wisp at (x, y - 0x20) with + 0x59 |= 2;
 * step 7's 0x30 timer returns to the chase. */
static void chrono_cutscene(AosEnemy *e, AosSoma *soma,
                            AosHitReport *spawned, const AosEnemyKind *kind) {
    switch (e->step) {
    case 0:
        if (soma->flags & (AOS_FLAG_ATTACKING | AOS_FLAG_AIR_ATTACK)) {
            e->step = 2;
            break;
        }
        play_loop(e, kind->anims, 1, false);
        e->step = 1;
        /* fallthrough */
    case 1:
        if (!(e->anim.flags & AOS_ANIM_ENDED)) break;
        e->step = 2;
        break;
    case 2:
        play_loop(e, kind->anims, 3, false);
        e->step = 3;
        e->timer = 0x18;
        break;
    case 3:
        if (e->timer) {
            e->timer--;
            if (e->timer == 8) aos_audio_play_song(0xEC);
        } else {
            e->step = 4;
        }
        break;
    case 4:
        if (e->timer <= 0xF) e->timer++;
        else e->step = 5;
        break;
    case 5:
        e->step = 6;
        break;
    case 6:
        if (!(e->anim.flags & AOS_ANIM_ENDED)) break;
        if (spawned->child_count < AOS_ENEMY_MAX_CHILDREN) {
            AosEnemy w = {0};
            w.id = e->id;
            w.role = AOS_ROLE_CHRONO_WISP;
            w.x = e->x;
            w.y = e->y - 0x200000;
            w.stats = e->stats;
            w.hp = 1;
            w.static_frame = -1;
            w.combat.type = AOS_TYPE_ENEMY;
            w.combat.attack_off = w.combat.hurt_off = true;
            w.cm_flags = 2;
            spawned->children[spawned->child_count++] = w;
        }
        e->step = 7;
        e->timer = 0x30;
        break;
    case 7:
        if (e->timer) e->timer--;
        else {
            e->state = 2;
            e->step = e->substep = 0;
        }
        break;
    default:
        break;
    }
}

/* sub_080B6FB8: the state-2 chase. Step 0 clears + 0x72 bit 2 (the
 * hurt-off of the cutscene — it becomes vulnerable), plays animation
 * 1 once; step 1 waits it out; step 2 picks animation 4 or 5 looping
 * by sub_0806CD38 — the target's + 0x40 x word against the entity's
 * own (4 = target right, 5 = target left) — and arms the + 0x54 =
 * 0x2000 field (not modelled); step 3 ramps + 0x18 by 0x2000 a frame
 * to 0x2FFFF, negated while animation 4 plays, and walks the
 * 0x08528824 probes (mode 2) at that speed; a floor+wall probe
 * result bumps + 0x4C by -0x30000 (mapped to vy). The sub_0806D128(3)
 * margin check runs every tick, setting + 0x59 bit 3. */
static void chrono_chase(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                         const AosEnemyKind *kind, int cam_x, int cam_y) {
    switch (e->step) {
    case 0:
        e->combat.hurt_off = false;
        play_loop(e, kind->anims, 1, false);
        e->step = 1;
        /* fallthrough */
    case 1:
        if (e->anim.flags & AOS_ANIM_ENDED) e->step = 2;
        break;
    case 2:
        /* + 0x58 &= ~0x41 palette clear and + 0x54 = 0x2000 are not
         * ported. */
        play_loop(e, kind->anims, target_x(soma) > e->x ? 4 : 5, true);
        e->step = 3;
        break;
    case 3:
        if (e->spin <= 0x2FFFF) e->spin += 0x2000;
        {
            int32_t speed = e->spin;
            if (e->anim.id == 4) speed = -speed;
            int result = ground_walk(e, soma, layer, &kind->probes, speed, 2);
            if ((result & 8) && (result & 3)) e->vy += (int32_t)0xFFFD0000;
        }
        break;
    default:
        break;
    }
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if (outside_margins(kind, 3, sx, sy)) e->cm_flags |= 8;
}

/* sub_080B70B4: the state-4 inert sequence under the global flag —
 * animation 0 looping for 0x80 ticks, then animation 1 once, then
 * animation 2 once held. The handler steps the animation itself
 * (sub_0803F17C) because the shared tail skips it for state 4. */
static void chrono_inert(AosEnemy *e, const AosEnemyKind *kind) {
    switch (e->step) {
    case 0:
        play_loop(e, kind->anims, 0, true);
        e->step = 1;
        e->timer = 0x80;
        break;
    case 1:
        if (e->timer) e->timer--;
        else {
            play_loop(e, kind->anims, 1, false);
            e->step = 2;
        }
        break;
    case 2:
        if (e->anim.flags & AOS_ANIM_ENDED) {
            play_loop(e, kind->anims, 2, false);
            e->step = 3;
        }
        break;
    default:
        break;
    }
    step_anim(e, kind->anims);
}

/* EnemyChronomageUpdate (0x080B71F8): state 3 skips the sub_0806AE54
 * hit-flash bookkeeping here (the shared tail does it); states 0-2
 * dispatch, state 3 plays animation 1 once with the 0x28 timer and
 * the sub_0806AEAC generic death flames, deleting on the animation's
 * end; state 4 runs the inert sequence; every other state gets the
 * sub_080421AC room query (not ported) and the animation step. */
AosHitReport aos_enemy_chronomage_update(AosEnemy *e, AosSoma *soma,
                                      const AosCollision *layer,
                                      const AosEnemyKind *kind,
                                      const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames,
                                      int soma_atk, int soma_def, int cam_x,
                                      int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    switch (e->state) {
    case 0:
        chrono_idle(e, soma);
        break;
    case 1:
        chrono_cutscene(e, soma, &spawned, kind);
        break;
    case 2:
        chrono_chase(e, soma, layer, kind, cam_x, cam_y);
        break;
    case 3:
        if (e->step == 0) {
            play_loop(e, kind->anims, 1, false);
            e->step = 1;
            e->timer = 0x28;
        } else {
            /* sub_0806AEAC: a flame every 4 frames over 16 x 12
             * pixels, the + 0xD countdown and the + 0x59 |= 8 marker
             * (the + 0x58 palette bit of the 0x08118CE0 table is not
             * ported); the tail still steps the animation. */
            if ((e->timer & 3) == 0)
                spawn_flame(e, 0x100000, 0xC0000, random, &spawned);
            if ((int8_t)--e->timer <= 0) e->cm_flags |= 8;
            if (e->anim.flags & AOS_ANIM_ENDED) e->removed = true;
        }
        break;
    case 4:
        chrono_inert(e, kind);
        break;
    default:
        break;
    }
    /* The shared tail steps the animation for states 0-3 (state 4
     * already did inside sub_080B70B4); the sub_080421AC room query
     * is skipped for states 3 and 4. */
    if (e->state != 4 && !e->removed) step_anim(e, kind->anims);
    if (!e->removed)
        report = collide(e, soma, kind, weapon, weapon_frames, soma_atk,
                         soma_def);
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    for (int i = 0; i < spawned.effect_count &&
                    report.effect_count < AOS_ENEMY_MAX_EFFECTS; ++i)
        report.effects[report.effect_count++] = spawned.effects[i];
    return report;
}
