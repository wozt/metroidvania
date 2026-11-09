/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Rock Armor (enemy 0x12) and its rocks.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos_enemy_internal.h"
#include "../../aos_audio.h"

#include <string.h>

#define collide aos_enemy_collide
#define ground_walk aos_enemy_ground_walk
#define outside_margins aos_enemy_outside_margins
#define pixel_delta aos_enemy_pixel_delta
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define player_ahead_within aos_enemy_player_ahead_within
#define random_centred aos_enemy_random_centred
#define spawn_drift aos_enemy_spawn_drift
#define step_anim aos_enemy_step_anim
#define take_effects aos_enemy_take_effects
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y

#define rock_armor_probes aos_enemy_rock_armor_probes
#define rock_armor_speed aos_enemy_rock_armor_speed
#define rock_armor_walk aos_enemy_rock_armor_walk
#define rock_armor_die aos_enemy_rock_armor_die
#define rock_probes aos_enemy_rock_probes

/* The walker probes of the Rock Armor (0x0852872C) and of its rock
 * (0x08528738), read from the ROM: wall count, ceiling, floor, half width,
 * wall y offsets. */
const AosProbes aos_enemy_rock_armor_probes = {2, -40, -1, 6, {-7, -24}};
const AosProbes aos_enemy_rock_probes = {1, -8, 8, 6, {0}};

/* The walk speed of the Rock Armor for its walk frame (frame % 5 <= 3). */
int32_t aos_enemy_rock_armor_speed(const AosEnemy *a) {
    unsigned r = a->anim.frame % 5;
    return r > 3 ? 0 : 0x2000 + (int32_t)r * 0x4000;
}

/* sub_080B2978: walking with the probe walker (mode 0x14); with a patrol
 * length (parameter 0) it turns around once frame 9 has been shown more
 * than that many updates, otherwise whenever it is not simply on a floor.
 * At the start of frame 9, the player ahead within 99 pixels starts the
 * throw. */
void aos_enemy_rock_armor_walk(AosEnemy *a, const AosSoma *soma, const AosCollision *layer) {
    if (a->step != 0) return;
    if (a->anim.frame == 9 && a->param0) {
        uint8_t shown = a->timer++;
        if (shown > a->param0) {
            a->timer = 0;
            a->mirrored = !a->mirrored;
        }
    }
    int result = ground_walk(a, soma, layer, &rock_armor_probes, rock_armor_speed(a), 0x14);
    if (!a->param0 && (result & 0x1B) != 8) a->mirrored = !a->mirrored;
    if (a->anim.tick == 0 && a->anim.frame == 9 && player_ahead_within(a, soma, 0x63)) {
        a->state = 1;
        a->step = a->substep = 0;
    }
}

/* sub_080B2BD8: animation 1 once; on each update of its frame 7 a rock
 * (sub_080B2798, sound 0x88) with the armor's position, facing, hit points
 * and contact power; when it ends, the player still ahead within 99 pixels
 * throws again, otherwise it walks (animation 0). */
static void rock_armor_throw(AosEnemy *a, const AosSoma *soma, const AosEnemyKind *kind,
                             AosHitReport *report) {
    switch (a->step) {
    case 0:
        play_loop(a, kind->anims, 1, false);
        a->step = 1;
        break;
    case 1:
        if (a->anim.frame == 7) {
            aos_audio_play_song(0x88);
            if (report->child_count < AOS_ENEMY_MAX_CHILDREN) {
                AosEnemy rock = {0};
                rock.id = a->id;
                rock.role = AOS_ROLE_ROCK;
                rock.static_frame = -1;
                rock.x = a->x;
                rock.y = a->y;
                rock.mirrored = a->mirrored;
                rock.hp = a->hp;
                rock.stats = a->stats;
                rock.combat.type = 0x0A;
                rock.combat.attack_off = rock.combat.hurt_off = true;
                report->children[report->child_count++] = rock;
            }
        }
        if (a->anim.flags & AOS_ANIM_ENDED)
            a->step = player_ahead_within(a, soma, 0x63) ? 0 : 2;
        break;
    case 2:
        a->state = a->step = a->substep = 0;
        play(a, kind->anims, 0);
        break;
    default: break;
    }
}

/* sub_080B2CD8: death animation 2 (sound 0x6F) until it ends, then the
 * deletion. + 0x4C (from -40.0, + 0x8000 per frame) is the height of the
 * effects and + 0x48 (from 0, + 0x5000) their phase: every fourth frame a
 * burst (sub_08045CEC animation 15, row 7, vy 0.5, ay -0x2000) at x + 16 *
 * sin(phase + 0x9000) +/- 2.0, and every eighth (sound 0x72) a spark
 * (animation 16, row 6) at x +/- 8 and the height +/- 2 pixels. */
void aos_enemy_rock_armor_die(AosEnemy *a, const AosEnemyKind *kind, uint32_t (*random)(void),
                           AosHitReport *report) {
    if (a->step == 0) {
        play_loop(a, kind->anims, 2, false);
        a->step = 1;
        a->vy = (int32_t)0xFFD80000;
        a->vx = 0;
        aos_audio_play_song(0x6F);
        return;
    }
    a->timer++;
    int32_t height = a->y + a->vy;
    a->vy += 0x8000;
    int32_t phase = a->vx;
    a->vx += 0x5000;
    if ((a->timer & 3) == 0) {
        int32_t offset = aos_sine((uint32_t)(phase + 0x9000)) << 4;
        int32_t x = a->x + offset + random_centred(random, 0x40000);
        AosEffect *burst = spawn_drift(AOS_EFFECT_BURST_15, x, height, report);
        if (burst) {
            burst->vy = 0x8000;
            burst->ay = -0x2000;
        }
    }
    if ((a->timer & 7) == 0) {
        aos_audio_play_song(0x72);
        int32_t x = a->x + (int32_t)((uint32_t)random_centred(random, 16) << 16);
        int32_t y = height + (int32_t)((uint32_t)random_centred(random, 4) << 16);
        spawn_drift(AOS_EFFECT_SPARK, x, y, report);
    }
    if (a->anim.flags & AOS_ANIM_ENDED) a->removed = true;
}

/* EnemyRockArmorUpdate: inside the activity window of sub_0806CC20 (only
 * colliding while flashing from a blow), the state, the collision pass
 * (sub_080421AC, not while dying) and the animation step (sub_0806DF20);
 * state 3 (the global 0x8E & 0x40) only steps its animation. */
AosHitReport aos_enemy_rock_armor_update(AosEnemy *a, AosSoma *soma, const AosCollision *layer,
                                      const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames, int soma_atk,
                                      int soma_def, int cam_x, int cam_y,
                                      uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (a->x >> 16) - cam_x, sy = (a->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    if (a->hp > 0 && a->hit_flash) {
        a->hit_flash--;
        report = collide(a, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&a->combat);
        return report;
    }
    switch (a->state) {
    case 0: rock_armor_walk(a, soma, layer); break;
    case 1: rock_armor_throw(a, soma, kind, &spawned); break;
    case 2:
        rock_armor_die(a, kind, random, &spawned);
        if (a->removed) {
            take_effects(&report, &spawned);
            return report;
        }
        break;
    default:
        step_anim(a, kind->anims);
        return report;
    }
    if (a->state != 2)
        report = collide(a, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    step_anim(a, kind->anims);
    aos_combat_tick(&a->combat);
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    take_effects(&report, &spawned);
    return report;
}

/* sub_080B2798: the collision pass first (its hit callback sub_080B2F5C
 * zeroes its hit points: it bursts). State 0: animation 3, both boxes
 * (-16, -16, 32, 32) (0x2020F0F0), 58 pixels higher, thrown up at 2.0 with
 * gravity 0x2000 and forward at 0.125 + its distance to the player (from
 * the armor's feet) / 64 a frame, contact power + 1 / 4; its contact
 * sub_080B2F94 knocks back. State 1: the probe walker (0x08528738, mode 2)
 * until it meets anything. State 2: sound 0x80, eight pieces within 8
 * pixels, 8 higher, facing at random, and it is gone. Beyond screen margin
 * 3 it is deleted. The global pause of gEwramData + 0x4BE is not ported. */
AosHitReport aos_enemy_rock_update(AosEnemy *r, AosSoma *soma, const AosCollision *layer,
                                const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
                                int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = collide(r, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&r->combat);
    if (r->state != 0 && r->hp == 0) r->state = 2;
    switch (r->state) {
    case 0: {
        play(r, kind->anims, 3);
        step_anim(r, kind->anims);
        r->combat.attack_off = r->combat.hurt_off = false;
        r->own_boxes = true;
        r->own_hurt = r->own_attack = (AosBox){-16, -16, 32, 32};
        int32_t dx = pixel_delta(target_x(soma), r->x), dy = pixel_delta(target_y(soma), r->y);
        int32_t distance = (int16_t)aos_isqrt((uint32_t)(dx * dx + dy * dy));
        r->speed = -0x2000 - ((distance << 16) >> 6);
        r->y -= 0x3A0000;
        if (r->mirrored) r->speed = -r->speed;
        r->vy = -0x20000;
        r->ay = 0x2000;
        r->stats.contact = (uint8_t)(r->stats.contact + (r->stats.contact >> 2));
        r->state = 1;
    }
        /* fallthrough */
    case 1:
        if (ground_walk(r, soma, layer, &rock_probes, r->speed, 2)) r->state = 2;
        break;
    case 2:
        aos_audio_play_song(0x80);
        for (int i = 0; i < 8 && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
            AosEnemy piece = {0};
            piece.id = r->id;
            piece.role = AOS_ROLE_ROCK_PIECE;
            piece.static_frame = -1;
            piece.x = r->x + random_centred(random, 0x100000);
            piece.y = r->y + random_centred(random, 0x100000) - 0x80000;
            piece.mirrored = random() & 1;
            piece.combat.attack_off = piece.combat.hurt_off = true;
            report.children[report.child_count++] = piece;
        }
        r->removed = true;
        return report;
    default: break;
    }
    if (outside_margins(kind, 3, (r->x >> 16) - cam_x, (r->y >> 16) - cam_y)) r->removed = true;
    return report;
}

/* sub_080B264C: a piece shows animation 4, 5 or 6 (r & 3, at most 2),
 * flies at (+/- 0.5) - 0.25 away from its facing and up at 1.0 + (r &
 * 0x1FFF) / 0x1000 with gravity 0.25, bounces once at half speed off a
 * solid cell 4 pixels below it, and is gone outside the screen box (-16,
 * -16, 0x110, 0xC0). */
void aos_enemy_rock_piece_update(AosEnemy *p, const AosCollision *layer, const AosEnemyKind *kind,
                              int cam_x, int cam_y, uint32_t (*random)(void)) {
    switch (p->state) {
    case 0: {
        uint32_t size = random() & 3;
        play(p, kind->anims, 4 + (size > 2 ? 2 : size));
        step_anim(p, kind->anims);
        p->vx = random_centred(random, 0x10000) - 0x4000;
        if (p->mirrored) p->vx = -p->vx;
        p->vy = -0x10000 - (int32_t)((random() & 0x1FFF) << 4);
        p->state = 1;
        break;
    }
    case 1:
        if (aos_collision_cell(layer, p->x >> 16, (p->y >> 16) + 4) & 1) {
            p->vy = -(p->vy >> 1);
            p->state = 2;
            break;
        }
        /* fallthrough */
    case 2:
        p->x += p->vx;
        p->y += p->vy;
        p->vy += 0x4000;
        break;
    default: break;
    }
    int sx = (p->x >> 16) - cam_x, sy = (p->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x10) > 0x110 || (uint16_t)(sy + 0x10) > 0xC0) p->removed = true;
}
