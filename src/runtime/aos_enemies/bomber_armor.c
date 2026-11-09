/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Bomber Armor (enemy 0x3A), its bomb and sparks.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../aos_enemy_internal.h"
#include "../aos_audio.h"

#include <string.h>

#define burst_source aos_enemy_burst_source
#define burst_source_count aos_enemy_burst_source_count
#define collide aos_enemy_collide
#define ground_walk aos_enemy_ground_walk
#define outside_margins aos_enemy_outside_margins
#define pixel_delta aos_enemy_pixel_delta
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define player_ahead_within aos_enemy_player_ahead_within
#define rock_armor_die aos_enemy_rock_armor_die
#define rock_armor_probes aos_enemy_rock_armor_probes
#define rock_armor_speed aos_enemy_rock_armor_speed
#define rock_armor_walk aos_enemy_rock_armor_walk
#define rock_probes aos_enemy_rock_probes
#define step_anim aos_enemy_step_anim
#define take_effects aos_enemy_take_effects
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y

/* sub_080B2A80: the throw. Substep 0 plays animation 1 once; at frame 7
 * sound 0x88 and the bomb (sub_0806DFF8 with sub_080B243C: the facing,
 * position, hit points and contact power copied). When the animation ends
 * the player still ahead within 99 pixels (sub_0806BBC4 / sub_0806D044)
 * backs it off one step — substep 2 walks backward once at the negated
 * walk speed, then substep 0 replays the throw — otherwise substep 3
 * resets to the walk. */
static void bomber_throw(AosEnemy *a, const AosSoma *soma, const AosCollision *layer,
                         const AosEnemyKind *kind, AosHitReport *report) {
    switch (a->step) {
    case 0:
        play_loop(a, kind->anims, 1, false);
        a->step = 1;
        break;
    case 1:
        if (a->anim.frame == 7) {
            aos_audio_play_song(0x88);
            if (report->child_count < AOS_ENEMY_MAX_CHILDREN) {
                AosEnemy bomb = {0};
                bomb.id = a->id;
                bomb.role = AOS_ROLE_BOMBER_BOMB;
                bomb.static_frame = -1;
                bomb.x = a->x;
                bomb.y = a->y;
                bomb.mirrored = a->mirrored;
                bomb.hp = a->hp;
                bomb.stats = a->stats;
                report->children[report->child_count++] = bomb;
            }
        }
        if (a->anim.flags & AOS_ANIM_ENDED)
            a->step = player_ahead_within(a, soma, 0x63) ? 2 : 3;
        break;
    case 2:
        ground_walk(a, soma, layer, &rock_armor_probes, -rock_armor_speed(a), 0x14);
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

/* sub_080B243C: the bomb. The collision pass first (its hit callback
 * sub_080B2F5C zeroes its hit points). State 0, on the first update:
 * animation 3 looping, contact elements 2 (sub_0804277C with sub_080B2F64
 * / sub_080B2F5C), the boxes of sub_08042884 (0x3030E8E8) and
 * sub_080428F0 (0x1010F8F8), + 0x72 bit 1 (not ported), then the throw —
 * 58 pixels up, vy -2.5 with gravity 0.125 (+ 0x54 = 0x2000) through the
 * probe walker sub_0806CAF8 (0x08528738, mode 2) at the speed of + 0x18:
 * -2.0 - the distance to the player / 64 a frame, negated by the facing;
 * walls halve and reverse it, a ceiling starts the fuse. Every fourth
 * frame a fuse spark (sub_080B2FC4) 8 pixels above. State 2 counts 16
 * frames down, then the burst of sub_08045A98 list 0 and state 3, which
 * plays sound 0x76 and deletes it. Beyond screen margin 3 it is deleted.
 * The global pause of gEwramData + 0x4BE is not ported. */
static void bomber_burst(const AosEnemy *b, AosHitReport *report) {
    for (size_t i = 0; i < burst_source_count &&
                        report->effect_count < AOS_ENEMY_MAX_EFFECTS;
         ++i) {
        AosEffect piece = burst_source[i];
        piece.x = (int32_t)((uint32_t)(int16_t)((b->x >> 16) + (piece.x >> 16)) << 16);
        piece.y = (int32_t)((uint32_t)(int16_t)((b->y >> 16) + (piece.y >> 16)) << 16);
        report->effects[report->effect_count++] = piece;
    }
}

AosHitReport aos_enemy_bomber_bomb_update(AosEnemy *b, AosSoma *soma, const AosCollision *layer,
                                       const AosEnemyKind *kind,
                                       const AosWeaponEntity *weapon,
                                       const AosWeaponFrames *weapon_frames, int soma_atk,
                                       int soma_def, int cam_x, int cam_y,
                                       uint32_t (*random)(void)) {
    (void)random;
    AosHitReport report = collide(b, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&b->combat);
    switch (b->state) {
    case 0:
        play(b, kind->anims, 3);
        step_anim(b, kind->anims);
        b->contact_elements = 2;
        b->own_boxes = true;
        b->own_hurt = (AosBox){-8, -8, 16, 16};
        b->own_attack = (AosBox){-24, -24, 48, 48};
        {
            int32_t dx = pixel_delta(target_x(soma), b->x);
            int32_t dy = pixel_delta(target_y(soma), b->y);
            int32_t distance = (int16_t)aos_isqrt((uint32_t)(dx * dx + dy * dy));
            b->spin = -0x2000 - ((distance << 16) >> 6);
        }
        b->y -= 0x3A0000;
        if (b->mirrored) b->spin = -b->spin;
        b->vy = (int32_t)0xFFFD8000;
        b->ay = 0x2000;
        b->stats.contact = (uint8_t)(b->stats.contact + (b->stats.contact >> 2));
        b->state = 1;
        /* fallthrough */
    case 1:
        if ((b->timer & 3) == 0 && report.child_count < AOS_ENEMY_MAX_CHILDREN) {
            AosEnemy spark = {0};
            spark.id = b->id;
            spark.role = AOS_ROLE_BOMBER_SPARK;
            spark.static_frame = -1;
            spark.x = b->x;
            spark.y = b->y - 0x80000;
            spark.combat.attack_off = spark.combat.hurt_off = true;
            report.children[report.child_count++] = spark;
        }
        b->timer++;
        {
            int walk = ground_walk(b, soma, layer, &rock_probes, b->spin, 2);
            if (walk & 3) b->spin = -b->spin / 2;
            if (walk & 8) {
                b->state = 2;
                b->timer = 0x10;
            }
        }
        break;
    case 2:
        if (b->timer) {
            b->timer--;
        } else {
            bomber_burst(b, &report);
            b->state = 3;
        }
        break;
    case 3:
        aos_audio_play_song(0x76);
        b->removed = true;
        return report;
    default:
        break;
    }
    if (outside_margins(kind, 3, (b->x >> 16) - cam_x, (b->y >> 16) - cam_y))
        b->removed = true;
    else
        step_anim(b, kind->anims);
    return report;
}

/* sub_080B2FC4: the fuse spark — animation 4 once, then its affine step;
 * gone when the animation ends (the affine slot's return 3). */
void aos_enemy_bomber_spark_update(AosEnemy *s, const AosEnemyKind *kind) {
    if (s->state == 0) {
        play_loop(s, kind->anims, 4, false);
        s->state = 1;
    }
    step_anim(s, kind->anims);
    if (s->anim.flags & AOS_ANIM_ENDED) s->removed = true;
}

/* EnemyBomberArmorUpdate: inside the activity window of sub_0806CC20
 * (only colliding while flashing from a blow), the state, the collision
 * pass (sub_0806E314, skipped while defeated) and the animation step
 * (sub_0806DF20); state 3 (the global 0x8E & 0x40) only steps its
 * animation. */
AosHitReport aos_enemy_bomber_update(AosEnemy *a, AosSoma *soma, const AosCollision *layer,
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
    case 0:
        rock_armor_walk(a, soma, layer);
        break;
    case 1:
        bomber_throw(a, soma, layer, kind, &spawned);
        break;
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
