/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemy: Ukoback (enemy 0x4C) — the hopping pot.
 *
 * EnemyUkobackCreate (0x080C5BDC) / EnemyUkobackUpdate (0x080C6480),
 * the state-0 hop sub_080C5CE8, the state-0xFF shatter sub_080C5F10,
 * the peeked creature sub_080C5FA0/sub_080C610C and the lid
 * sub_080C61F4. The sub_080C6310/sub_080C639C shard entities are
 * emitted as AOS_EFFECT_BURST_15 effects (same special-15 row-7 art)
 * pinned around Soma — a documented approximation. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define outside_margins aos_enemy_outside_margins
#define pixel_delta aos_enemy_pixel_delta
#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define player_in_centred aos_enemy_player_in_centred
#define spawn_drift aos_enemy_spawn_drift
#define step_anim aos_enemy_step_anim
#define umod aos_enemy_umod

/* sub_080C5CE8 state 0: the hop. + 0x16 counts down a random
 * 0x50..0x1B7 cooldown; the first substep arms vx = vy = 0xC000 with
 * the sub_0806BD64 / sub_0806BDEC direction draws (approximated by
 * the RNG bits) and later substeps share the body. The facing bit
 * follows the sign of vx. The room-edge checks against the camera
 * scroll byte and the four wall probes of sub_0800210C / sub_080020D8
 * / sub_08001CCC / sub_08001E58 are approximated by a screen-edge
 * bounce that negates the velocity and pushes the position back two
 * steps, like the native correction. When the cooldown expires and
 * fewer than eight creatures are out (+ 0x22 <= 7) the pot peeks one
 * out through sub_080C6590. */
static void ukoback_hop(AosEnemy *e, AosSoma *soma,
                        const AosEnemyKind *kind, AosHitReport *spawned,
                        uint32_t (*random)(void), int cam_x, int cam_y) {
    (void)kind;
    if (e->uk_timer == 0)
        e->uk_timer = (int16_t)(umod(random(), 0x168) + 0x50);
    else
        e->uk_timer--;
    if (e->step == 0) {
        e->vx = e->vy = 0xC000;
        if (random() & 1) e->vx = -e->vx;
        if (!(random() & 1)) e->vy = -e->vy;
        e->step = 1;
    }
    e->mirrored = e->vx >= 0;
    /* Room-edge bounce: the native compares the room pixel position
     * plus its + 0x4A / + 0x4E offsets against the scroll-derived
     * bound; the screen margins stand in. */
    int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
    if (sx < -8 || sx > 0x108) {
        int32_t ov = e->vx;
        e->vx = -ov;
        e->x += -2 * ov;
        e->mirrored = e->vx >= 0;
    }
    if (sy < -8 || sy > 0xD8) {
        int32_t ov = e->vy;
        e->vy = -ov;
        e->y += -2 * ov;
    }
    /* The vertical probe pair adjusts + 0x46 and the horizontal one
     * + 0x42; both are collision nudges the bounce above covers. */
    if (e->uk_timer == 0 && e->uk_peek <= 7 &&
        spawned->child_count < AOS_ENEMY_MAX_CHILDREN) {
        AosEnemy c = {0};
        c.id = e->id;
        c.role = AOS_ROLE_UKOBACK_CREATURE;
        c.x = e->x;
        c.y = e->y;
        c.mirrored = (random() & 1) != 0;
        c.stats = e->stats;
        c.hp = 1;
        c.static_frame = -1;
        c.parent = e;
        /* The + 0x14 drift offset counts down through a negative
         * velocity (sub_080C610C's state-0xFF block); + 0x18 drifts
         * with vy. */
        c.target_x = 0x10000;
        c.target_y = 0x10000;
        c.vx = -0x400;
        c.vy = 0;
        c.ax = c.ay = 0;
        play_loop(&c, kind->anims, 1, true);
        spawned->children[spawned->child_count++] = c;
        e->uk_peek++;
        if (player_in_centred(e, soma, 0x120, 0x100))
            aos_audio_play_song(0x78);
    }
    e->x += e->vx;
    e->y += e->vy;
    e->vx += e->ax;
    e->vy += e->ay;
}

/* sub_080C5F10 state 0xFF: the shatter. Step 0 plays song 0x72,
 * freezes on direct frame 9, drops at vy 0x8000 and emits the lid
 * child (sub_080C6624 -> sub_080C61F4); step 1 sinks and, past the
 * + 0x46 > 0xAA mark (approximated by the distance fallen), fades.
 * The sub_0806BE74 flame emitter runs every frame of the death. */
static void ukoback_die(AosEnemy *e, const AosEnemyKind *kind,
                        AosHitReport *spawned, uint32_t (*random)(void)) {
    (void)random;
    if (e->step == 0) {
        aos_audio_play_song(0x72);
        e->static_frame = 9;
        e->timer = 0x32;
        e->vy = 0x8000;
        e->uk_death_y = (int16_t)(e->y >> 16);
        if (spawned->child_count < AOS_ENEMY_MAX_CHILDREN) {
            AosEnemy l = {0};
            l.id = e->id;
            l.role = AOS_ROLE_UKOBACK_LID;
            l.x = e->x;
            l.y = e->y;
            l.stats = e->stats;
            l.hp = 1;
            l.static_frame = 10;
            l.parent = e;
            l.timer = 0;
            spawned->children[spawned->child_count++] = l;
        }
        e->step = 1;
    } else {
        e->y += e->vy;
        if ((int16_t)(e->y >> 16) - e->uk_death_y > 0xAA) e->translucent = true;
        /* sub_0806BE74(e, 7, 0x100000, 0xC0000): the shared ember
         * emitter; the blood effect stands in. */
        if (!(e->timer & 7))
            spawn_drift(AOS_EFFECT_BLOOD, e->x + 0x8000, e->y, spawned);
    }
    (void)kind;
}

/* EnemyUkobackUpdate: state 1 is the inert pose (animation 0 once,
 * then only stepping); outside it the sub_08021924 activity mark and
 * the sub_0806AD24 stun gate run (the sub_080421AC room query is not
 * ported); state 0 hops, state 0xFF shatters; the sub_0806DF20 /
 * sub_0806E314 tail is the shared housekeeping. */
AosHitReport aos_enemy_ukoback_update(AosEnemy *e, AosSoma *soma,
                                    const AosCollision *layer,
                                    const AosEnemyKind *kind,
                                    const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames,
                                    int soma_atk, int soma_def, int cam_x,
                                    int cam_y, uint32_t (*random)(void)) {
    (void)layer;
    AosHitReport report = {0}, spawned = {0};
    if (e->state == 1) {
        if (e->step == 0) {
            play_loop(e, kind->anims, 0, false);
            e->step = 1;
        }
        step_anim(e, kind->anims);
        return report;
    }
    bool run = e->state == 0xFF || !(e->hp > 0 && e->hit_flash);
    if (run) {
        if (e->state == 0)
            ukoback_hop(e, soma, kind, &spawned, random, cam_x, cam_y);
        else if (e->state == 0xFF)
            ukoback_die(e, kind, &spawned, random);
        if (!e->removed) step_anim(e, kind->anims);
    }
    if (!e->removed)
        report = collide(e, soma, kind, weapon, weapon_frames, soma_atk,
                         soma_def);
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    return report;
}

/* sub_080C610C: the peeked creature. States below 0xFF count + 0x1C
 * up to 0x3FF frames of life, then enter the 0xFF drift-out; the
 * drift integrates the + 0x14 / + 0x18 offsets by their velocities
 * and, once + 0x14 reaches zero, flips facing, fades and decrements
 * the pot's + 0x22 peek count. Its contact sub_080C65C8 bursts it
 * into the shard effects and its hurt sub_080C6614 sends it into the
 * same drift on any blow. */
AosHitReport aos_enemy_ukoback_creature_update(AosEnemy *e, AosSoma *soma,
                                            const AosCollision *layer,
                                            const AosEnemyKind *kind,
                                            const AosWeaponEntity *weapon,
                                            const AosWeaponFrames *weapon_frames,
                                            int soma_atk, int soma_def,
                                            int cam_x, int cam_y,
                                            uint32_t (*random)(void)) {
    (void)layer; (void)cam_x; (void)cam_y; (void)random;
    AosHitReport report = {0};
    if (!e->parent || e->parent->removed) {
        e->removed = true;
        return report;
    }
    if (e->state == 0xFF) {
        /* The drift-out: + 0x14 while positive, then the arrive. */
        if (e->target_x > 0) {
            e->target_x += e->vx;
            e->vx += e->ax;
        } else {
            e->target_x = 0;
        }
        e->target_y += e->vy;
        e->vy += e->ay;
        if (e->target_x == 0) {
            e->mirrored = !e->mirrored;
            e->translucent = true;
            if (e->parent->uk_peek) e->parent->uk_peek--;
            e->removed = true;
        }
        return report;
    }
    if (++e->walk_timer > 0x3FF) e->state = 0xFF;
    else {
        step_anim(e, kind->anims);
        report = collide(e, soma, kind, weapon, weapon_frames, soma_atk,
                         soma_def);
        /* sub_080C65C8: Soma touching the creature bursts it into the
         * ten shard entities (approximated by the burst-15 effects)
         * around him and sends it into the drift. */
        if (report.soma_hit && e->state != 0xFF) {
            for (int i = 0; i < 10; ++i) {
                int32_t ox = aos_enemy_random_centred(random, 0x10) << 16;
                int32_t oy = aos_enemy_random_centred(random, 0x10) << 16;
                AosEffect *fx = spawn_drift(AOS_EFFECT_BURST_15,
                                            aos_enemy_target_x(soma) + ox,
                                            aos_enemy_target_y(soma) + oy - 0x100000,
                                            &report);
                if (fx) fx->vy = -0x4000;
            }
            e->state = 0xFF;
        }
    }
    return report;
}

/* sub_080C61F4: the lid the shatter emits (sub_080C6624 copies the
 * parent's + 0x10 slot, here the parent pointer). State 0 loads the
 * shared resources, parks on direct frame 0xA four pixels above the
 * pot and inherits 1.5x its vy; state 1 falls until the 0x32-timer
 * or the sunk mark; state 2 fades. The sub_0806D518 blink drives the
 * + 0x58 bit-5 visibility. */
AosHitReport aos_enemy_ukoback_lid_update(AosEnemy *e, const AosSoma *soma,
                                        const AosEnemyKind *kind,
                                        int cam_y, uint32_t (*random)(void)) {
    (void)soma; (void)cam_y; (void)random;
    AosHitReport report = {0};
    const AosEnemy *p = e->parent;
    if (!p || p->removed) {
        e->removed = true;
        return report;
    }
    switch (e->state) {
    case 0:
        e->x = p->x;
        e->y = p->y + 0x40000;
        e->vy = p->vy + p->vy / 2;
        e->vx = 0;
        e->state = 1;
        e->timer = 0;
        break;
    case 1:
        e->y += e->vy;
        if (++e->timer > 0x32) e->state = 2;
        break;
    case 2:
        e->translucent = true;
        break;
    default:
        break;
    }
    e->timer++;
    e->hidden = (kind->blink[e->timer & 0xFF] & 1) != 0;
    return report;
}
