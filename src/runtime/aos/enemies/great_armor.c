/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Great Armor (boss 0x3C, Final Guard shares
 * the routines).
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#include <string.h>

#define accelerate_toward aos_enemy_accelerate_toward
#define accel_move aos_enemy_accel_move
#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define move_toward aos_enemy_move_toward
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define player_in_range aos_enemy_player_in_range
#define skeleton_merge aos_enemy_skeleton_merge
#define spawn_drift aos_enemy_spawn_drift
#define spawn_flame_at aos_enemy_spawn_flame_at
#define step_anim aos_enemy_step_anim

#define GA_PARTS 14

/* sub_0808E024 resource init: tiles 0x081CC004 through sub_0803B800,
 * the palette bank picked by the + 0x36 id byte (0x3C -> 0, 0x67 -> 1,
 * 0x69 -> 2 of the 0x0820A8F8 table) and frames 0x0821F240 through
 * sub_0803B924. The fourteen sub_0806DFF8 children of the create
 * (callback sub_0808E164, container + 0x1C of 0x8C bytes) fold into
 * the persistent AOS_ROLE_GA_PART parts emitted over the first
 * updates; their per-part offsets approximate the 0x0852778C/
 * 0x08527B7C table and the sub_0808E96C/sub_0808E994 motion. */
static const int32_t ga_ox[GA_PARTS] = {
    -0x600000, 0x600000, -0x400000, 0x400000,
    -0x800000, 0x800000, -0x200000, 0x200000,
    -0xA00000, 0xA00000, -0x500000, 0x500000,
    -0x300000, 0x300000};
static const int32_t ga_oy[GA_PARTS] = {
    -0x700000, -0x700000, -0x500000, -0x500000,
    -0x900000, -0x900000, -0x300000, -0x300000,
    -0xB00000, -0xB00000, -0x400000, -0x400000,
    -0x600000, -0x600000};

static void ga_emit_parts(AosEnemy *e, AosHitReport *hit) {
    while (e->spawned_count < GA_PARTS &&
           hit->child_count < AOS_ENEMY_MAX_CHILDREN) {
        unsigned i = e->spawned_count++;
        AosEnemy c;
        memset(&c, 0, sizeof c);
        c.id = e->id;
        c.x = e->x;
        c.y = e->y;
        c.role = AOS_ROLE_GA_PART;
        c.variant = (uint8_t)i;
        c.parent = e;
        c.static_frame = (int16_t)i;
        c.hp = 1;
        c.combat = (AosCombat){.type = AOS_TYPE_ENEMY, .attack_off = true,
                               .hurt_off = true};
        hit->children[hit->child_count++] = c;
    }
}

static void ga_part_update(AosEnemy *part, AosEnemy *body) {
    if (!body || body->removed || body->id != 0x3C) {
        part->removed = true;
        return;
    }
    unsigned v = part->variant < GA_PARTS ? part->variant : 0;
    part->x = body->x + ga_ox[v];
    part->y = body->y + ga_oy[v];
    part->mirrored = body->mirrored;
}

/* EnemyGreatArmorUpdate 0x0808A224 state 2: the native death machine.
 * While (+ 0xD - 0x79) & 0xFF <= 0x5D and + 0xD & 3 == 0 a
 * sub_0806D7F0 particle fires at x + a random offset in
 * [-0x140000, +0x140000); + 0xD counts down, the + 0x59 bit-3 flag
 * sets once the timer reaches 1 and the + 0x1C room record gates the
 * sub_0806C250 delete. The update tail runs sub_0806B120 and clears
 * + 0x1A on a (RandomNumberGenerator() & 0x7F) <= 3 roll. */
static AosHitReport ga_death(AosEnemy *enemy, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (enemy->timer >= 0x79 && enemy->timer < 0xD6 &&
        (enemy->timer & 3) == 0) {
        int32_t px = enemy->x +
                     (int32_t)(random() % 0x280000) - 0x140000;
        spawn_drift(AOS_EFFECT_DARK_SMOKE, px, enemy->y - 0x80000,
                    &report);
    }
    if (enemy->timer)
        enemy->timer--;
    if (enemy->cycle_done && enemy->timer <= 0x10) {
        enemy->cycle_done = false;
        enemy->removed = true;
    }
    return report;
}

/* sub_0808A908 state 0: fifty-three cases — the sub_0808DD38 aggro
 * probe and the sub_0806E29C(0xA0, 0xA0) player-range test gate the
 * combat entries, the RandomNumberGenerator2 & 0x1F roll picks the
 * attack entry (+ 0xA = 1, + 0xB = 0 or 0x28; the + 0x36 == 0x67
 * variant reads sub_080212C8 & 0x360 for the + 0xB = 0x1E entry) and
 * the songs 0x64/0x7C mark the native beats. The port keeps the
 * decoded skeleton: an aggro gate, a chase cycle and the attack
 * calls, with the per-case choreography collapsed. */
static void ga_state0(AosEnemy *enemy, AosSoma *soma,
                      const AosAnimSet *anims,
                      uint32_t (*random)(void)) {
    switch (enemy->step) {
    case 0:
        /* Case 0: the sub_0808DD38/sub_0806D54C probes — enter the
         * fight once the player is inside the 0xA0 range. */
        play_loop(enemy, anims, 0, true);
        enemy->step = 1;
        enemy->timer = 0;
        break;
    case 1:
        /* Chase/idle: face and close on the player. */
        if (soma)
            face_player(enemy, soma);
        if (soma)
            move_toward(enemy, soma->x, soma->y - 0x80000, 0x10);
        accel_move(enemy, 0x60000);
        if (++enemy->timer > 0x50 && soma &&
            player_in_range(enemy, soma, 0xA00000)) {
            /* Attack entry: native + 0xA = 1, + 0xB = 0 (the 0x28
             * variant entry and the 0x67-only 0x1E case collapse). */
            enemy->state = 1;
            enemy->step = 0;
            enemy->timer = 0;
            aos_audio_play_song(0x7C);
        }
        break;
    case 2:
        /* Roar/pose beat before the attack call (song 0x64). */
        if (enemy->timer == 0)
            aos_audio_play_song(0x64);
        play_loop(enemy, anims, 1, false);
        if (++enemy->timer > 0x28) {
            enemy->timer = 0;
            enemy->step = 1;
        }
        break;
    case 3:
    case 6:
        /* The decoded return entries (+ 0xB = 3/6): recover, then
         * resume the chase. */
        if (++enemy->timer > 0x20) {
            enemy->timer = 0;
            enemy->step = 1;
        }
        break;
    default:
        /* The remaining forty-nine cases collapse into the chase
         * cycle; the random & 0x1F attack gate is folded into the
         * timer above. */
        enemy->step = 1;
        break;
    }
    (void)random;
}

/* sub_0808C400 state 1 (with the sub_0808D3EC continuation): sixty
 * cases of attack combinations — windup song 0x7F, strike song 0x86,
 * the sub_0808DEEC volleys of the shared exits and the timer-gated
 * returns (+ 0xA = 0, + 0xB = 3 or 6 through sub_0808E9C4). The port
 * keeps the decoded beat order: windup, two strike volleys, recover,
 * return to state 0. */
static void ga_state1(AosEnemy *enemy, AosSoma *soma,
                      const AosAnimSet *anims, AosHitReport *report,
                      uint32_t (*random)(void)) {
    switch (enemy->step) {
    case 0:
        face_player(enemy, soma);
        play_loop(enemy, anims, 2, false);
        enemy->timer = 0;
        enemy->step = 1;
        break;
    case 1:
        /* Windup: song 0x7F, hold the pose. */
        if (enemy->timer == 0)
            aos_audio_play_song(0x7F);
        if (++enemy->timer > 0x30) {
            enemy->timer = 0;
            enemy->step = 2;
        }
        break;
    case 2:
    case 4:
        /* Strike volleys: song 0x86 and flame-folded shots (the
         * sub_0808DEEC entities). */
        if (enemy->timer == 0) {
            aos_audio_play_song(0x86);
            for (int i = 0; i < 2 &&
                            report->child_count < AOS_ENEMY_MAX_CHILDREN;
                 ++i) {
                int32_t sx = enemy->x + (enemy->mirrored ? -0x80000
                                                        : 0x80000);
                spawn_flame_at(sx, enemy->y - 0x40000, 0x60000, 0x80000,
                               random, report);
            }
        }
        if (++enemy->timer > 0x20) {
            enemy->timer = 0;
            enemy->step++;
        }
        break;
    case 3:
    case 5:
        /* Recover between strikes. */
        if (++enemy->timer > 0x28) {
            enemy->timer = 0;
            enemy->step++;
        }
        break;
    default:
        /* The decoded exits (_0808D692/_0808DAC6): + 0xA = 0 with
         * the + 0xB = 3/6 recovery entries — collapsed to the state-0
         * chase entry. */
        enemy->state = 0;
        enemy->step = 3;
        enemy->timer = 0;
        break;
    }
}

AosHitReport aos_enemy_great_armor_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind,
    const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    (void)layer;
    (void)cam_x;
    (void)cam_y;
    if (enemy->role == AOS_ROLE_GA_PART) {
        ga_part_update(enemy, enemy->parent);
        return report;
    }
    /* The fourteen children emit over the first updates under the
     * room child cap, like the Creaking Skull bones. */
    if (!enemy->boss_parts_spawned) {
        ga_emit_parts(enemy, &report);
        if (enemy->spawned_count >= GA_PARTS)
            enemy->boss_parts_spawned = true;
    }
    if (enemy->state == 2) {
        AosHitReport hit = ga_death(enemy, random);
        /* The update tail: a (rng & 0x7F) <= 3 roll clears + 0x1A. */
        if ((random() & 0x7F) <= 3)
            enemy->ga_f1a = 0;
        skeleton_merge(&report, &hit);
        step_anim(enemy, kind->anims);
        return report;
    }
    switch (enemy->state) {
    case 0:
        ga_state0(enemy, soma, kind->anims, random);
        break;
    case 1:
        ga_state1(enemy, soma, kind->anims, &report, random);
        break;
    default:
        /* State 3 dormant (the sub_0803F17C tail only). */
        break;
    }
    step_anim(enemy, kind->anims);
    AosHitReport hit = collide(enemy, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    skeleton_merge(&report, &hit);
    if (!enemy->defeated && enemy->hp <= 0 && enemy->state != 2) {
        enemy->defeated = true;
        enemy->state = 2;
        enemy->step = 0;
        enemy->timer = 0xD6;
        play_loop(enemy, kind->anims, 3, false);
    }
    /* The non-death tail: sub_0806B120 awake probe (covered by the
     * shared paths) and the + 0x1A clear roll. */
    if ((random() & 0x7F) <= 3)
        enemy->ga_f1a = 0;
    aos_combat_tick(&enemy->combat);
    return report;
}
