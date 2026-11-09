/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Axe Armor (enemy 0x04) and its axe.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos_enemy_internal.h"
#include "../../aos_audio.h"

#include <string.h>

#define collide aos_enemy_collide
#define ground_walk aos_enemy_ground_walk
#define outside_margins aos_enemy_outside_margins
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define player_ahead_within aos_enemy_player_ahead_within
#define random_centred aos_enemy_random_centred
#define spawn_drift aos_enemy_spawn_drift
#define spawn_flame_at aos_enemy_spawn_flame_at
#define step_anim aos_enemy_step_anim
#define take_effects aos_enemy_take_effects

/* The walk speed of the axe armor for its walk frame (frame % 9 <= 6). */
static int32_t armor_speed(const AosEnemy *a) {
    unsigned r = a->anim.frame % 9;
    return r > 6 ? 0 : 0x1000 + (int32_t)r * 0x2800;
}

/* sub_080B0F1C: walking with the probe walker (mode 0x14: no slopes, no
 * ledges); without a patrol length (parameter 0) it turns around whenever
 * it is not simply on a floor, otherwise after that many steps (frame 17).
 * At the start of frame 17, the player ahead within 99 pixels starts an
 * attack. */
static void armor_walk(AosEnemy *a, const AosSoma *soma, const AosCollision *layer,
                       const AosEnemyKind *kind) {
    if (a->step != 0) return;
    bool frame17 = a->anim.frame == 0x11 && a->anim.tick == 0;
    if (frame17 && a->param0) {
        uint8_t steps = a->timer++;
        if (steps > a->param0) {
            a->timer = 0;
            a->mirrored = !a->mirrored;
        }
    }
    int result = ground_walk(a, soma, layer, &kind->probes, armor_speed(a), 0x14);
    if (!a->param0 && (result & 0x1B) != 8) a->mirrored = !a->mirrored;
    if (frame17 && player_ahead_within(a, soma, 0x63)) {
        a->state = 1;
        a->step = a->substep = 0;
    }
}

/* sub_080B0D5C setup: an axe 24 pixels ahead and 16 up, with the armor's
 * facing, contact power and throw height. */
static AosEnemy make_axe(const AosEnemy *armor) {
    AosEnemy axe = {0};
    axe.id = armor->id;
    axe.role = AOS_ROLE_AXE;
    axe.static_frame = -1;
    axe.x = armor->x + (armor->mirrored ? 0x180000 : (int32_t)0xFFE80000);
    axe.y = armor->y - 0x100000;
    axe.mirrored = armor->mirrored;
    axe.substep = armor->substep;
    axe.stats = armor->stats;
    axe.combat.type = 0x0A;
    return axe;
}

/* sub_080B1030: a high or low throw at random (animation 1 or 2); at frame
 * 12, tick 2, the axe leaves; when the animation ends, with the player still
 * ahead within 99 pixels it backs away playing the walk once, otherwise it
 * walks again. */
static void armor_attack(AosEnemy *a, const AosSoma *soma, const AosCollision *layer,
                         const AosEnemyKind *kind, uint32_t (*random)(void),
                         AosHitReport *report) {
    switch (a->step) {
    case 0:
        if (random() & 1) {
            play_loop(a, kind->anims, 1, false);
            a->substep = 0;
        } else {
            play_loop(a, kind->anims, 2, false);
            a->substep = 1;
        }
        a->step = 1;
        break;
    case 1:
        if (a->anim.frame == 12 && a->anim.tick == 2) {
            report->children[report->child_count++] = make_axe(a);
        }
        if (a->anim.flags & AOS_ANIM_ENDED) {
            if (player_ahead_within(a, soma, 0x63)) {
                play_loop(a, kind->anims, 0, false);
                a->step = 2;
            } else {
                a->step = 3;
            }
        }
        break;
    case 2:
        ground_walk(a, soma, layer, &kind->probes, -armor_speed(a), 0x14);
        if (a->anim.flags & AOS_ANIM_ENDED) a->step = 0;
        break;
    case 3:
        a->state = a->step = a->substep = 0;
        play(a, kind->anims, 0);
        break;
    default:
        break;
    }
}

/* sub_080B0D5C: the first update starts animation 3 (sound 0x86) and the
 * flight: vx 2.5 toward the throw, decelerated by 0x800 per frame (it comes
 * back), a spin of 0x800 per frame (a high throw starts 20 pixels higher
 * and spins the other way), one hit point and a 16 x 16 box (type 0xA).
 * Later updates run the collision pass first: struck, it vanishes with a
 * flame of sub_0806D5C0 within 8 x 8 pixels. It is deleted beyond screen
 * margin 4. The global pause of gEwramData + 0x4BE is not ported. */
AosHitReport aos_enemy_axe_update(AosEnemy *axe, AosSoma *soma, const AosEnemyKind *kind,
                               const AosWeaponEntity *weapon,
                               const AosWeaponFrames *weapon_frames, int soma_atk,
                               int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (axe->state != 0) {
        report = collide(axe, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        if (axe->hp <= 0) {
            aos_audio_play_song(0x86);
            /* sub_0806D5C0(x, y, 8.0, 8.0): a flame where it was struck. */
            spawn_flame_at(axe->x, axe->y, 0x80000, 0x80000, random, &report);
            axe->removed = true;
            aos_combat_tick(&axe->combat);
            return report;
        }
    }
    if (axe->state == 0) {
        play_loop(axe, kind->anims, 3, false);
        step_anim(axe, kind->anims);
        axe->vx = (int32_t)0xFFFD8000;
        axe->ax = 0x800;
        axe->spin = 0x800;
        if (axe->substep == 1) {
            axe->y -= 0x140000;
            axe->spin = -axe->spin;
        }
        if (axe->mirrored) {
            axe->vx = -axe->vx;
            axe->ax = -axe->ax;
            axe->spin = -axe->spin;
        }
        axe->state = 1;
        axe->hp = 1;
        axe->own_boxes = true;
        axe->own_hurt = axe->own_attack = (AosBox){-8, -8, 16, 16};
    } else {
        /* sub_0806D430(e, 2.5) */
        axe->vx += axe->ax;
        if (axe->ax >= 0 ? axe->vx > 0x28000 : axe->vx < -0x28000)
            axe->vx = axe->ax >= 0 ? 0x28000 : -0x28000;
        axe->x += axe->vx;
    }
    axe->angle += (uint32_t)axe->spin;
    if (outside_margins(kind, 4, (axe->x >> 16) - cam_x, (axe->y >> 16) - cam_y))
        axe->removed = true;
    aos_combat_tick(&axe->combat);
    return report;
}

/* sub_080B11DC: death animation 4 (sound 0x6F) until it ends, then the
 * deletion. + 0x4C (from -40.0, + 0x12000 per frame) is the height of the
 * effects and + 0x48 (from 0, + 0x5000) their phase: during the first 32
 * frames, on even frames a fire (sub_08045CEC animation 1, row 8, vy 0.5,
 * ay -0x1000) at x + 16 * sin(phase + 0x4800) +/- 2.0, and every 8 frames
 * (sound 0x6E) a spark (animation 16, row 6) at x +/- 8 and the height
 * +/- 2 pixels. */
static void armor_die(AosEnemy *a, const AosEnemyKind *kind, uint32_t (*random)(void),
                      AosHitReport *report) {
    if (a->step == 0) {
        aos_audio_play_song(0x6F);
        play_loop(a, kind->anims, 4, false);
        a->step = 1;
        a->vy = (int32_t)0xFFD80000;
        a->vx = 0;
        return;
    }
    a->timer++;
    int32_t height = a->y + a->vy;
    a->vy += 0x12000;
    int32_t phase = a->vx;
    a->vx += 0x5000;
    if (a->anim.flags & AOS_ANIM_ENDED) {
        a->removed = true;
        return;
    }
    if (a->timer > 0x20) return;
    if ((a->timer & 1) == 0) {
        int32_t offset = aos_sine((uint32_t)(phase + 0x4800)) << 4;
        int32_t x = a->x + offset + random_centred(random, 0x40000);
        AosEffect *fire = spawn_drift(AOS_EFFECT_FIRE, x, height, report);
        if (fire) {
            fire->vy = 0x8000;
            fire->ay = -0x1000;
        }
    }
    if ((a->timer & 7) == 0) {
        aos_audio_play_song(0x6E);
        int32_t x = a->x + (int32_t)((uint32_t)random_centred(random, 16) << 16);
        int32_t y = height + (int32_t)((uint32_t)random_centred(random, 4) << 16);
        spawn_drift(AOS_EFFECT_SPARK, x, y, report);
    }
}

/* EnemyAxeArmorUpdate: inside the activity window, the state, then the
 * collision pass (sub_0806E314) before the animation step. */
AosHitReport aos_enemy_armor_update(AosEnemy *a, AosSoma *soma, const AosCollision *layer,
                                 const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
                                 int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (a->x >> 16) - cam_x, sy = (a->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    switch (a->state) {
    case 0: armor_walk(a, soma, layer, kind); break;
    case 1: armor_attack(a, soma, layer, kind, random, &spawned); break;
    case 2:
        armor_die(a, kind, random, &spawned);
        if (a->removed) {
            take_effects(&report, &spawned);
            return report;
        }
        break;
    default:
        step_anim(a, kind->anims);
        return report;
    }
    sx = (a->x >> 16) - cam_x;
    sy = (a->y >> 16) - cam_y;
    if (!a->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(a, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    step_anim(a, kind->anims);
    aos_combat_tick(&a->combat);
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    take_effects(&report, &spawned);
    return report;
}
