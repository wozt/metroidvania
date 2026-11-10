/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemy: Student Witch (enemy 0x16) — the floating
 * caster of the castle's lower halls.
 *
 * EnemyStudentWitchCreate (0x080B8CA0) / EnemyStudentWitchUpdate
 * (0x080B8D50), the state handlers sub_080B8850 (hover patrol),
 * sub_080B8954 (perch), sub_080B8A90 (dive) and sub_080B8C0C (death),
 * the hurt sub_080B9EEC and the bolt sub_080B8668 spawned through
 * sub_080B87C4. The witch shares the Witch's resources (tiles
 * 0x081E41C4, palette 0x0820B880, frames 0x08241790). */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define ground_walk aos_enemy_ground_walk
#define outside_margins aos_enemy_outside_margins
#define pixel_delta aos_enemy_pixel_delta
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define player_in aos_enemy_player_in
#define player_in_centred aos_enemy_player_in_centred
#define spawn_drift aos_enemy_spawn_drift
#define step_anim aos_enemy_step_anim

/* The sub_0806CAF8 probe tables: the witch flies the 0x08528884 set
 * (count 2, ceiling -32, floor -1, half-width 7, wall offsets
 * -10/-24/1/-12) and her bolt the 0x08528890 one (count 1, ceiling
 * -12, floor 0, half-width 5, wall offset -7). */
static const AosProbes sw_fly_probes = {2, -32, -1, 7, {-10, -24, 1, -12}};
static const AosProbes sw_bolt_probes = {1, -12, 0, 5, {-7}};

/* The + 0x14 horizontal drift speed, stepped by the animation frame
 * exactly like the three state handlers repeat it: frame 0 arms
 * 0xC000, frame 1 bleeds 0x1200 down to zero and later frames feed
 * 0x4000 up to the 0xBFFF ceiling. */
static void sw_hover_speed(AosEnemy *e) {
    switch (e->anim.frame) {
    case 0:
        e->angle = 0xC000;
        break;
    case 1:
        e->angle = (uint32_t)((int32_t)e->angle - 0x1200);
        if ((int32_t)e->angle < 0) e->angle = 0;
        break;
    default:
        if ((int32_t)e->angle <= 0xBFFF) e->angle += 0x4000;
        break;
    }
}

/* sub_080B87C4: the bolt spawn. The child takes the parent's + 0x3C
 * palette nibble and facing, parks at the parent's pixel position plus
 * the (x, y) offsets — x negated while the parent faces left — and
 * carries `param` in + 0xB. */
static void sw_spawn_bolt(AosEnemy *e, int dx, int dy, int param,
                          AosHitReport *spawned) {
    if (spawned->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy c = {0};
    c.id = e->id;
    c.role = AOS_ROLE_STUDENT_WITCH_BOLT;
    c.x = e->x + (int32_t)(e->mirrored ? -dx : dx) * 0x10000;
    c.y = e->y + (int32_t)dy * 0x10000;
    c.step = (uint8_t)param;
    c.state = 0;
    c.mirrored = e->mirrored;
    c.stats = e->stats;
    c.hp = 1;
    c.static_frame = -1;
    c.parent = e;
    c.vy = 0;
    c.ay = 0x4000;
    c.attack_off = (param & 2) != 0;
    spawned->children[spawned->child_count++] = c;
}

/* sub_080B8850: state 0, the hover patrol. The + 0x14 speed feeds the
 * mode-8 probe walk; a wall or slope result bit (3) flips the facing
 * and a missing floor (bit 8 clear) drops her into state 2. While
 * animation frame 0 holds and the player stands in the 0xA0 x 0x3C
 * box a one-in-four roll dives (state 3), a failed roll perches
 * (state 2); five frames without the player turn her around. */
static void sw_hover(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                     AosHitReport *spawned, uint32_t (*random)(void)) {
    (void)spawned;
    if (e->step == 0) e->step = 1;
    sw_hover_speed(e);
    int result = ground_walk(e, soma, layer, &sw_fly_probes,
                             (int32_t)e->angle, 8);
    if (result & 3) e->mirrored = !e->mirrored;
    if (!(result & 8)) {
        e->state = 2;
        e->step = e->substep = 0;
        return;
    }
    if (e->anim.frame != 0) return;
    if (player_in_centred(e, soma, 0xA0, 0x3C)) {
        if (random() & 4) {
            e->state = 3;
        } else {
            e->state = 2;
        }
        e->step = e->substep = 0;
    } else if (++e->substep > 4) {
        e->substep = 0;
        e->mirrored = !e->mirrored;
    }
}

/* sub_080B8954: state 2, the perch. Substep 0 faces the player and
 * arms the 0x78-tick + 0x18 timer; substep 1 keeps hovering at mode
 * 0xD until the timer dies (back to state 0) or the player enters the
 * 0x64 x 0x3C box, which switches to animation 0xA and substep 2.
 * Substep 2 walks at mode 0xC: past animation frame 8 the frame-9 tick
 * plays song 0x1CA and arms the 0x20000 lunge speed that then decays
 * 0x2000 a frame; a ceiling probe (+ 0x6C bit 2) advances to substep
 * 3, which restores animation 7 and state 0. */
static void sw_perch(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                     const AosEnemyKind *kind) {
    switch (e->step) {
    case 0:
        face_player(e, soma);
        e->step = 1;
        e->spin = 0x78;
        break;
    case 1: {
        sw_hover_speed(e);
        ground_walk(e, soma, layer, &sw_fly_probes,
                    (int32_t)e->angle, 0xD);
        if (--e->spin <= 0) {
            e->state = 0;
            e->step = e->substep = 0;
            break;
        }
        if (player_in_centred(e, soma, 0x64, 0x3C)) {
            play(e, kind->anims, 0xA);
            e->step = 2;
            e->angle = 0;
        }
        break;
    }
    case 2: {
        int result = ground_walk(e, soma, layer, &sw_fly_probes,
                                 (int32_t)e->angle, 0xC);
        e->sw_probe = (uint8_t)result;
        if (e->anim.frame > 8) {
            if (e->anim.frame == 9) {
                if (e->anim.tick == 0) aos_audio_play_song(0x1CA);
                e->angle = 0x20000;
            } else {
                int32_t v = (int32_t)e->angle - 0x2000;
                e->angle = v < 0 ? 0 : (uint32_t)v;
            }
        }
        if (e->sw_probe & 4) e->step = 3;
        break;
    }
    case 3:
        play_loop(e, kind->anims, 7, true);
        e->state = 0;
        e->step = e->substep = 0;
        break;
    default:
        break;
    }
}

/* sub_080B8A90: state 3, the dive. Substep 0 faces the player,
 * switches to animation 8, arms + 0x14 = 0x8000 and a random upward
 * velocity of -(0xC000 + n * 0x800) with 0x400 gravity. Substep 1
 * climbs the + 0x18 tick: between ticks 0x14 and 0x1D a 1/64 roll
 * puffs two smoke effects (sub_080458E4, approximated by the shared
 * smoke drifts) and adds another -0xC000; at tick 0x1E animation 9
 * takes over. The mode-0 probe reports its ceiling bit into + 0x6C
 * and its floor bit (8) drops her to substep 3. Substep 2 fires the
 * bolt at animation frame 6 (songs 0x1CB and 0x81 first, bolt at
 * (-0xC, -0x14)) and pulls back to substep 1 on a ceiling; the floor
 * probe again ends the dive. Substep 3 restores animation 7 and
 * state 0. The + 0x4C fall speed clamps at 1.0. */
static void sw_dive(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                    const AosEnemyKind *kind, AosHitReport *spawned,
                    uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        face_player(e, soma);
        play_loop(e, kind->anims, 8, true);
        e->angle = 0x8000;
        e->vy = -(0xC000 + (int32_t)(random() & 7) * 0x800);
        e->ay = 0x400;
        e->spin = 0;
        e->step = 1;
        break;
    case 1:
        if (++e->spin > 0xFFFF) e->spin = 0xFFFF;
        if (!e->substep) {
            int t = (int16_t)e->spin;
            if (t >= 0x14 && t <= 0x1D && (random() & 0x3F) == 0) {
                /* sub_080458E4(x, 0, 0x17, 0x11) and (x, 0, 0x10,
                 * 0x10): the dive's smoke, approximated by drifts. */
                spawn_drift(AOS_EFFECT_SMOKE, e->x, e->y - 0x170000,
                            spawned);
                spawn_drift(AOS_EFFECT_SMOKE, e->x, e->y - 0x100000,
                            spawned);
                e->vy -= 0xC000;
                e->substep = 1;
            }
            if (!e->substep && t == 0x1E) {
                play(e, kind->anims, 9);
                e->step = 2;
            }
        }
        {
            int result = ground_walk(e, soma, layer, &sw_fly_probes,
                                     (int32_t)e->angle, 0);
            e->sw_probe = (uint8_t)result;
            if (result & 8) e->step = 3;
        }
        break;
    case 2:
        ++e->spin;
        if (!e->substep && e->anim.frame == 6) {
            aos_audio_play_song(0x1CB);
            aos_audio_play_song(0x81);
            sw_spawn_bolt(e, -0xC, -0x14, 0, spawned);
            e->substep = 1;
        }
        if (e->sw_probe & 4) {
            play_loop(e, kind->anims, 8, true);
            e->step = 1;
        }
        {
            int result = ground_walk(e, soma, layer, &sw_fly_probes,
                                     (int32_t)e->angle, 0);
            e->sw_probe = (uint8_t)result;
            if (result & 8) e->step = 3;
        }
        break;
    case 3:
        play_loop(e, kind->anims, 7, true);
        e->state = 0;
        e->substep = 0;
        e->step = 0;
        break;
    default:
        break;
    }
    if (e->vy > 0x100000) e->vy = 0x100000;
}

/* sub_080B8C0C: state 4, the death. Substep 0 arms the 0x20 counter
 * and the + 0xF flag; the shared case runs while the counter's low
 * bit is clear, flinging an animation-5/row-6 spark (sub_08045CEC,
 * approximated by the spark drift) on a sine offset that climbs and
 * falls, then decrements. At zero substep 2 emits the (-0x10, param 2)
 * bolt and the handler reports the delete. */
static void sw_death(AosEnemy *e, const AosEnemyKind *kind,
                     AosHitReport *spawned, uint32_t (*random)(void)) {
    (void)kind; (void)random;
    switch (e->step) {
    case 0:
        e->step = 1;
        e->timer = 0x20;
        break;
    case 1:
        if (!(e->timer & 1)) {
            int32_t ox = (int32_t)aos_sine((uint32_t)e->timer << 13) * 8;
            int32_t oy = (int32_t)(4 - (e->timer >> 1)) << 16;
            AosEffect *fx = spawn_drift(AOS_EFFECT_SPARK, e->x + ox,
                                        e->y + oy, spawned);
            if (fx) {
                fx->vy = -0x18000;
                fx->ay = 0x8000;
            }
        }
        if (e->timer) {
            e->timer--;
        } else {
            e->step = 2;
        }
        break;
    case 2:
        sw_spawn_bolt(e, 0, -0x10, 2, spawned);
        e->removed = true;
        break;
    default:
        break;
    }
}

/* EnemyStudentWitchUpdate (0x080B8D50): the standard preamble — the
 * + 0x58 bit-5 visibility follows sub_0806D128(4), the sub_08021924
 * activity mark and the sub_0806AD24 stun gate (the sub_080421AC room
 * query is not ported) — then the state jump: 0 hover, 2 perch, 3
 * dive, 4 death, 5 inert; the sub_0806DF20 tail is the shared
 * collide pass. */
AosHitReport aos_enemy_student_witch_update(AosEnemy *e, AosSoma *soma,
                                          const AosCollision *layer,
                                          const AosEnemyKind *kind,
                                          const AosWeaponEntity *weapon,
                                          const AosWeaponFrames *weapon_frames,
                                          int soma_atk, int soma_def,
                                          int cam_x, int cam_y,
                                          uint32_t (*random)(void)) {
    (void)cam_x; (void)cam_y;
    AosHitReport report = {0}, spawned = {0};
    bool run = !(e->hp > 0 && e->hit_flash);
    if (run) {
        switch (e->state) {
        case 0:
            sw_hover(e, soma, layer, &spawned, random);
            break;
        case 2:
            sw_perch(e, soma, layer, kind);
            break;
        case 3:
            sw_dive(e, soma, layer, kind, &spawned, random);
            break;
        case 4:
            sw_death(e, kind, &spawned, random);
            break;
        default:
            break;
        }
        if (!e->removed) step_anim(e, kind->anims);
    }
    if (!e->removed) {
        int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
        if (!outside_margins(kind, 4, sx, sy)) {
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

/* sub_080B8668: the bolt. State 0 loads the shared witch resources
 * (carried by the kind), picks animation 6 when + 0xB bit 0 is set
 * and 0xB otherwise, arms hp 1, vy 0 with 0x4000 gravity and the +
 * 0x72 attack flag when + 0xB bit 1 is set; state 1 accelerates the
 * + 0x14 flight speed by 0x4000 up to the 0x1FFFF ramp end and walks
 * the mode-8 bolt probes at that speed — a wall (bits 3) bursts it,
 * the first floor touch (bit 8) plays song 0x68 once, and a zeroed
 * + 0x34 (the sub_080B9F88 hurt) sends it to state 2; state 2 throws
 * the animation-0/row-7 spark (sub_08045CEC, approximated) and
 * deletes. The sub_0806D128(3) activity check maps to the margins. */
AosHitReport aos_enemy_student_witch_bolt_update(AosEnemy *e,
                                               AosSoma *soma,
                                               const AosCollision *layer,
                                               const AosEnemyKind *kind,
                                               const AosWeaponEntity *weapon,
                                               const AosWeaponFrames *weapon_frames,
                                               int soma_atk, int soma_def,
                                               int cam_x, int cam_y,
                                               uint32_t (*random)(void)) {
    (void)random;
    AosHitReport report = {0};
    switch (e->state) {
    case 0:
        play_loop(e, kind->anims, (e->step & 1) ? 6 : 0xB, true);
        e->hp = 1;
        e->vy = 0;
        e->ay = 0x4000;
        e->attack_off = (e->step & 2) != 0;
        e->state = 1;
        e->substep = 0;
        e->angle = 0;
        /* The native init falls through into the flight block. */
        /* fall through */
    case 1: {
        if ((int32_t)e->angle <= 0x1FFFF) e->angle += 0x4000;
        int result = ground_walk(e, soma, layer, &sw_bolt_probes,
                                 (int32_t)e->angle, 8);
        if (result & 8 && !e->substep) {
            e->substep = 1;
            aos_audio_play_song(0x68);
        }
        if (result & 3) {
            e->state = 2;
            break;
        }
        step_anim(e, kind->anims);
        break;
    }
    case 2: {
        /* sub_08045CEC(x, y, 0, 7): the burst spark, approximated. */
        spawn_drift(AOS_EFFECT_SPARK, e->x, e->y, &report);
        e->removed = true;
        break;
    }
    default:
        step_anim(e, kind->anims);
        break;
    }
    if (e->state == 1) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        aos_enemy_merge_hit(&report, hit);
        /* sub_080B9F88 zeroes + 0x34 on any blow: with hp 1 every hit
         * is lethal and the bolt bursts on its next update. */
        if (hit.enemy_hit) e->state = 2;
    }
    if (!e->removed) {
        int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
        if (outside_margins(kind, 4, sx, sy)) e->removed = true;
    }
    return report;
}
