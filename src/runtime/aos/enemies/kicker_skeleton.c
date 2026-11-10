/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Kicker Skeleton (enemy 0x53).
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#define append_effects aos_enemy_append_effects
#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define ground_walk aos_enemy_ground_walk
#define player_in_centred aos_enemy_player_in_centred
#define play_loop aos_enemy_play_loop
#define random_centred aos_enemy_random_centred
#define spawn_drift aos_enemy_spawn_drift
#define step_anim aos_enemy_step_anim

/* The Kicker Skeleton's attack machine (sub_080BFFF0): + 0x0B is its
 * substate, + 0x18 the walker speed (`kicker_speed`), + 0x14 the contact
 * flags (`kicker_flags`: bit 0 the strike's x5/4 contact, bit 1 cleared on
 * landing), + 0x20 the death direction (`away`), + 0x0D the gore counter
 * (`timer`). The two sub_080458E4 strike zones (0x30, 0xC, 0x11) and
 * (0x30, 6, 0x10) are approximated by the exported frame attack boxes;
 * the sub_08021248 script slot of a type-3 contact is not ported. */
static int kicker_walk(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                       const AosEnemyKind *kind) {
    return ground_walk(e, soma, layer, &kind->probes, e->kicker_speed, 0);
}

static void kicker_idle(AosEnemy *e, const AosEnemyKind *kind) {
    play_loop(e, kind->anims, 0, true);
    e->state = 0;
    e->step = e->substep = 0;
}

/* sub_080C0250: the death — song 0x6B and seven sub_0806C5AC pieces
 * (AOS_ROLE_DEBRIS) flung from its position, then the entity deletes
 * itself the same frame. */
static void kicker_shatter(AosEnemy *e, const AosEnemyKind *kind,
                           AosHitReport *report, uint32_t (*random)(void)) {
    aos_audio_play_song(0x6B);
    for (int i = 0; i < 7 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy d = {0};
        d.id = e->id;
        d.role = AOS_ROLE_DEBRIS;
        d.static_frame = -1;
        d.x = e->x;
        d.y = e->y;
        d.mirrored = e->mirrored;
        d.combat.attack_off = d.combat.hurt_off = true;
        d.vx = (((int32_t)(random() & 7) << 14) + 0x8000) * e->away;
        d.vy = ((int32_t)((random() & 0xF) - 8) << 12) - 0x28000;
        d.ay = 0x2000;
        if (e->anim.id < AOS_ENEMY_MAX_ANIMS &&
            e->anim.frame < AOS_ENEMY_MAX_FRAMES &&
            kind->sprites[e->anim.id][e->anim.frame])
            d.static_frame = kind->sprites[e->anim.id][e->anim.frame];
        report->children[report->child_count++] = d;
    }
    e->removed = true;
}

AosHitReport aos_enemy_kicker_skeleton_update(AosEnemy *e, AosSoma *soma,
                                           const AosCollision *layer,
                                           const AosEnemyKind *kind,
                                           const AosWeaponEntity *weapon,
                                           const AosWeaponFrames *weapon_frames,
                                           int soma_atk, int soma_def,
                                           int cam_x, int cam_y,
                                           uint32_t (*random)(void)) {
    (void)cam_x; (void)cam_y;
    AosHitReport report = {0}, spawned = {0};
    switch (e->state) {
    case 0:
        /* Idle: only while + 0x0B is 0, a 1/64 fidget animation, then the
         * attack on the player in the 0xC8 x 0x50 centred box or on any
         * damage. */
        if (e->step == 0) {
            if (!(random() & 0x3F))
                play_loop(e, kind->anims, random() & 1, true);
            if (player_in_centred(e, soma, 0xC8, 0x50) || e->hp < e->stats.hp) {
                e->state = 1;
                e->step = e->substep = 0;
            }
        }
        break;
    case 1:
        /* sub_080BFFF0's jump table on + 0x0B; case 0 falls through into
         * case 1, facing and playing animation 3 in the same call. */
        switch (e->step) {
        case 0:
            face_player(e, soma);
            play_loop(e, kind->anims, 3, false);
            e->step = 2;
            break;
        case 1:
            play_loop(e, kind->anims, 3, false);
            e->step = 2;
            break;
        case 2:
            if (!(e->anim.flags & AOS_ANIM_ENDED)) break;
            play_loop(e, kind->anims, 4, false);
            e->kicker_speed = 0x8000;
            e->vy = -0x30000;
            e->ay = 0x1000;
            e->step = 3;
            break;
        case 3: {
            int result = kicker_walk(e, soma, layer, kind);
            if (result & 4) {
                e->step = 7;
            } else if (e->vy >= -0x18000) {
                face_player(e, soma);
                play_loop(e, kind->anims, 6, false);
                e->step = 4;
            }
            break;
        }
        case 4:
            kicker_walk(e, soma, layer, kind);
            if (e->anim.frame != 5) break;
            /* The two sub_080458E4 strike zones (0x30, 0xC, 0x11) and
             * (0x30, 6, 0x10) are approximated by the frame attack boxes. */
            play_loop(e, kind->anims, 5, false);
            e->kicker_speed = 0x28000;
            e->vy = 0x30000;
            e->kicker_flags |= 1;
            aos_audio_play_song(0x87);
            e->step = 5;
            break;
        case 5: {
            int result = kicker_walk(e, soma, layer, kind);
            if (!(result & 8)) break;
            play_loop(e, kind->anims, 3, false);
            e->kicker_flags &= ~2u;
            aos_audio_play_song(0x127);
            e->step = 6;
            break;
        }
        case 6: {
            int result = kicker_walk(e, soma, layer, kind);
            e->kicker_speed = e->kicker_speed * 0x1F / 0x20;
            if (e->kicker_speed <= 0x7FFF) {
                kicker_idle(e, kind);
                break;
            }
            if (!(result & 8)) break;
            if (result & 3) e->kicker_speed = 0;
            if (e->kicker_speed <= 0xBFFF) break;
            e->timer++;
            if (e->timer & 3) break;
            /* The skid's gore: sub_08045CEC animation 6, row 4 or 5. */
            AosEffect *gore = spawn_drift(AOS_EFFECT_BLOOD,
                e->x + ((int32_t)random_centred(random, 0x10) << 16),
                e->y + 0xC0000 + ((int32_t)random_centred(random, 4) << 16),
                &spawned);
            if (gore) {
                gore->vx = e->mirrored ? -0x8000 : 0x8000;
                gore->ay = -(int32_t)(random() & 0x3FF);
            }
            break;
        }
        case 7: {
            int result = kicker_walk(e, soma, layer, kind);
            if (result & 8) kicker_idle(e, kind);
            break;
        }
        default:
            break;
        }
        break;
    case 2:
        kicker_shatter(e, kind, &spawned, random);
        break;
    case 3:
        /* Flag state: sub_0803F17C only — animation, no collision. */
        step_anim(e, kind->anims);
        aos_combat_tick(&e->combat);
        for (int i = 0; i < spawned.child_count &&
                        report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
            report.children[report.child_count++] = spawned.children[i];
        append_effects(&report, &spawned);
        return report;
    default:
        break;
    }
    if (!e->removed) {
        report = collide(e, soma, kind, weapon, weapon_frames, soma_atk,
                         soma_def);
        step_anim(e, kind->anims);
    }
    aos_combat_tick(&e->combat);
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    append_effects(&report, &spawned);
    return report;
}

/* The Waiter Skeleton's two linked children share its resources: the
 * companion (sub_080AEC0C) rides its animation attachment and the tray
 * (sub_080AED28) is thrown at Soma. */
