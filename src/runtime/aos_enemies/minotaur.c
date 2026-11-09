/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Minotaur (enemy 0x15) and Red Minotaur (0x58).
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../aos_enemy_internal.h"
#include "../aos_audio.h"

#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define generic_death aos_enemy_generic_death
#define ground_walk aos_enemy_ground_walk
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define player_in_centred aos_enemy_player_in_centred
#define skeleton_merge aos_enemy_skeleton_merge
#define spawn_drift aos_enemy_spawn_drift
#define step_anim aos_enemy_step_anim

/* sub_080AFE08(e, mask): the aggro gate of the Minotaur family. The brown
 * Minotaur (+ 0x15 = 0) always passes; the red one rolls
 * RandomNumberGenerator2 & mask (our aos_random: RNG2 falls back to RNG1
 * while gEwramData + 0x4CC bit 6 is clear), then needs + 0x18 bit 1, the
 * player in the centred 0xA0 x 0x78 box or a wall probe 0x50 px ahead on
 * the facing side (sub_08002058 / sub_0800207C = aos_wall_push_left /
 * aos_wall_push_right; a nonzero push means the charge path is blocked,
 * so the gate passes and it only walks). */
static bool minotaur_aggro(const AosEnemy *e, const AosSoma *soma,
                           const AosCollision *layer, int mask,
                           uint32_t (*random)(void)) {
    if (!e->mino_bank) return true;
    if (random() & (uint32_t)mask) return true;
    if (!(e->mino_flags & 2)) return false;
    if (player_in_centred(e, soma, 0xA0, 0x78)) return true;
    int32_t x = (e->x >> 16) + (e->mirrored ? 0x50 : -0x50);
    int push = e->mirrored ? aos_wall_push_left(layer, x, e->y >> 16)
                           : aos_wall_push_right(layer, x, e->y >> 16);
    return push != 0;
}

/* sub_080B0814, state 0: face the target (+ 0x6F, the animation tick,
 * zero faces it twice), then the aggro gate. The close centred
 * 0xC8 x 0x78 box or any damage (sub_0806D54C: hp < record + 0xC) rolls
 * mask 1; the far 0x118 x 0x78 box undamaged rolls mask 0x3F. A failed
 * roll attacks (state 2); a passed close roll walks (state 1) only while
 * + 0x18 bit 1 is set. */
static void minotaur_idle(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                          const AosEnemyKind *kind, uint32_t (*random)(void)) {
    (void)kind;
    face_player(e, soma);
    if (e->anim.tick == 0) face_player(e, soma);
    bool damaged = e->hp < e->stats.hp;
    if (player_in_centred(e, soma, 0xC8, 0x78) || damaged) {
        if (!minotaur_aggro(e, soma, layer, 1, random)) {
            e->state = 2;
            e->step = e->substep = 0;
        } else if (e->mino_flags & 2) {
            e->state = 1;
            e->step = e->substep = 0;
        }
        return;
    }
    if ((player_in_centred(e, soma, 0x118, 0x78) || damaged) &&
        !minotaur_aggro(e, soma, layer, 0x3F, random)) {
        e->state = 2;
        e->step = e->substep = 0;
    }
}

/* sub_080B0898, state 1 chase: animation 2 once, walk animation 3 while
 * the player stays outside the centred 0x64 x 0x50 box (the probe walker
 * sub_0806CAF8 at 0x8000, table 0x085286F8, mode 0xD), then animation 4
 * (sound 0x86 at its frame 3, tick 0) and back to idle. */
static void minotaur_chase(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                           const AosEnemyKind *kind, uint32_t (*random)(void)) {
    (void)random;
    switch (e->step) {
    case 0:
        play_loop(e, kind->anims, 2, false);
        e->step = 1;
        break;
    case 1:
        if (e->anim.flags & AOS_ANIM_ENDED) {
            play(e, kind->anims, 3);
            e->step = 2;
        }
        break;
    case 2:
        ground_walk(e, soma, layer, &kind->probes, 0x8000, 0xD);
        if (player_in_centred(e, soma, 0x64, 0x50)) {
            play_loop(e, kind->anims, 4, false);
            e->step = 3;
        }
        break;
    case 3:
        if (e->anim.frame == 3 && e->anim.tick == 0) aos_audio_play_song(0x86);
        if (e->anim.flags & AOS_ANIM_ENDED) {
            play(e, kind->anims, 1);
            e->state = 0;
            e->step = e->substep = 0;
        }
        break;
    default:
        break;
    }
}

/* sub_080B095C, state 2 attack: with + 0x18 bit 1 set it clears it, plays
 * animation 7 once and walks the wind-up; otherwise animation 5 loops
 * straight into the charge. The charge (case 3) decelerates from 3.25 by
 * 1.25 per tick (mode 0xC), switches to animation 6 at tick 0x18, then
 * either waits out animation 4 (bit 1 path, setting bit 0) or counts
 * 0x30 ticks of the current animation before animation 1 and idle. */
static void minotaur_attack(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                            const AosEnemyKind *kind, AosHitReport *report) {
    switch (e->step) {
    case 0:
        if (e->mino_flags & 2) {
            e->mino_flags &= ~2u;
            play_loop(e, kind->anims, 7, false);
            e->step = 1;
        } else {
            play(e, kind->anims, 5);
            e->step = 3;
            e->timer = 0;
        }
        break;
    case 1:
        if (e->anim.flags & AOS_ANIM_ENDED) {
            e->step = 2;
            e->timer = 8;
        }
        break;
    case 2:
        if (e->timer) {
            e->timer--;
            break;
        }
        play(e, kind->anims, 5);
        e->step = 3;
        /* sub_080458E4(e, 0x28, 0xC, 1) and (e, 0x28, 6, 0) spawn the
         * charge dust entities; approximated by drift dust on the enemy. */
        spawn_drift(AOS_EFFECT_DUST, e->x, e->y, report);
        spawn_drift(AOS_EFFECT_DUST, e->x, e->y, report);
        break;
    case 3: {
        int32_t speed = 0x34000 - (int32_t)e->timer * 0x1400;
        ground_walk(e, soma, layer, &kind->probes, speed, 0xC);
        e->timer++;
        if (e->timer == 0x18) play_loop(e, kind->anims, 6, false);
        if (e->timer > 0x28) {
            if (!(e->mino_flags & 2)) {
                e->step = 4;
                e->timer = 0x30;
            } else {
                e->step = 5;
                play_loop(e, kind->anims, 4, false);
                e->mino_flags |= 1;
            }
        }
        break;
    }
    case 4:
        if (e->timer) {
            e->timer--;
            break;
        }
        play(e, kind->anims, 1);
        e->state = e->step = e->substep = 0;
        break;
    case 5:
        if (e->anim.frame == 3 && e->anim.tick == 0) aos_audio_play_song(0x86);
        if (e->anim.flags & AOS_ANIM_ENDED) {
            play(e, kind->anims, 1);
            e->state = e->step = e->substep = 0;
        }
        break;
    default:
        break;
    }
}

/* sub_080B0BE8, EnemyMinotaurUpdate shared by ids 0x15 and 0x58: the
 * sleep flag, the tracking list, the hit flash, the state machine, the
 * collision pass and the animation step. State 3 is sub_08079294 (its
 * timed particles differ from generic_death's flames) and state 4 is
 * sub_0803F17C alone (no collision, no extra step). */
AosHitReport aos_enemy_minotaur_update(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                                    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames, int soma_atk,
                                    int soma_def, int cam_x, int cam_y,
                                    uint32_t (*random)(void)) {
    AosHitReport report = {0};
    (void)cam_x;
    int sy = (e->y >> 16) - cam_y;
    /* sub_0806E4F8(e, -16): hidden while above the screen top margin. */
    e->hidden = sy < -16;
    /* Inside the margins of sub_0806D128(e, 6), sub_08021924 tracks it;
     * aos_enemy_update's own targetable covers the list. */
    /* sub_0806AE54: the hit flash counts down, not while dying. */
    if (e->state != 3 && e->hit_flash) e->hit_flash--;
    /* sub_0806D4C0's hit-stop gate is not ported (always false). */
    switch (e->state) {
    case 0:
        /* param0 != 0 with gEwramData + 0x4CC bit 0x40 clear calls the
         * channel claim sub_08013CF0(0x10): not ported. */
        minotaur_idle(e, soma, layer, kind, random);
        break;
    case 1:
        minotaur_chase(e, soma, layer, kind, random);
        break;
    case 2:
        minotaur_attack(e, soma, layer, kind, &report);
        break;
    case 3:
        generic_death(e, kind, random, &report);
        break;
    case 4:
        /* sub_0803F17C alone: animation only, deleted at its end. */
        step_anim(e, kind->anims);
        if (e->cycle_done) e->removed = true;
        return report;
    default:
        break;
    }
    /* sub_080421AC: the collision pass, not while dying. */
    if (e->state != 3) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk,
                                   soma_def);
        skeleton_merge(&report, &hit);
    }
    /* + 0x14 = the frame byte, then sub_0806DF20. */
    step_anim(e, kind->anims);
    aos_combat_tick(&e->combat);
    return report;
}
