/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Dryad (enemy 0x46) and its seeds.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos_enemy_internal.h"
#include "../../aos_audio.h"

#define append_effects aos_enemy_append_effects
#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define outside_margins aos_enemy_outside_margins
#define play_loop aos_enemy_play_loop
#define player_in aos_enemy_player_in
#define random_direction aos_enemy_random_direction
#define snap_to_floor aos_enemy_snap_to_floor
#define spawn_drift aos_enemy_spawn_drift
#define step_anim aos_enemy_step_anim
#define target_x aos_enemy_target_x

/* sub_080AE96C: the seed it spits — entity type 0x1B, role 0x48, update
 * sub_080AE7E4. Spawned 10..-5 px to the facing side and 11..26 px up
 * (RNG & 0xF offsets, the x flipped by the facing bit), its + 0x3C
 * attack byte is three quarters of the parent's. The native velocity is
 * distance-scaled (sub_0806CD38 / 64 corrections); approximated here as
 * a lob toward the player. */
static void dryad_spit(AosEnemy *e, AosSoma *soma, AosHitReport *report,
                       uint32_t (*random)(void)) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy seed = {0};
    seed.id = e->id;
    seed.role = AOS_ROLE_DRYAD_SEED;
    seed.static_frame = -1;
    seed.stats = e->stats;
    seed.stats.contact = (uint8_t)(e->stats.contact - (e->stats.contact >> 2));
    seed.contact_elements = 0;
    seed.mirrored = e->mirrored;
    int dx = (int)(random() & 0xF) - 10;
    if (e->mirrored) dx = -dx;
    seed.x = e->x + ((int32_t)dx << 16);
    seed.y = e->y + (((int32_t)(random() & 0xF) - 26) << 16);
    int32_t dist = target_x(soma) - seed.x;
    if (dist < 0) dist = -dist;
    int32_t dir = target_x(soma) > seed.x ? 1 : -1;
    seed.vx = dir * (0x8000 + (dist >> 6));
    seed.vy = -0x4000 - (int32_t)(random() & 0xF) * 0x200;
    report->children[report->child_count++] = seed;
}

AosHitReport aos_enemy_dryad_update(AosEnemy *e, AosSoma *soma,
                                 const AosCollision *layer,
                                 const AosEnemyKind *kind,
                                 const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames,
                                 int soma_atk, int soma_def, int cam_x,
                                 int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    (void)layer;   /* the Dryad never moves after its create snap */
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if (outside_margins(kind, 4, sx, sy)) {
        aos_combat_tick(&e->combat);
        return report;
    }
    /* + 0x2D counts down in the shared update path. */
    if (e->hit_flash) e->hit_flash--;
    switch (e->state) {
    case 0:
        /* sub_080AE4BC: the facing write of sub_0806CF2C into + 0x58
         * bit 6 every tick; step 0 runs the notice box, step 1 the
         * RNG & 3 wake. */
        face_player(e, soma);
        if (e->step == 0) {
            if (player_in(soma, (int16_t)((e->x >> 16) - 0x6E),
                          (int16_t)((e->y >> 16) - 0x28), 0xDC, 0x50))
                e->step = 1;
        } else if ((random() & 3) == 0) {
            e->state = 1;
            e->step = e->substep = 0;
            play_loop(e, kind->anims, 1, false);
        }
        break;
    case 1:
        if (e->step == 0) {
            if (e->anim.flags & AOS_ANIM_ENDED) {
                play_loop(e, kind->anims, 2, true);
                e->step = 1;
            }
        } else if (e->step == 1) {
            if (!outside_margins(kind, 4, sx, sy)) {
                if ((random() & 7) == 0) {
                    play_loop(e, kind->anims, 3, false);
                    e->timer = 0x12;
                    e->step = 2;
                }
            } else {
                /* Off screen: + 0x59 |= 8 sleeps it and the gEwramData
                 * ledger bit clears; approximated by hidden. */
                e->hidden = true;
            }
        } else {
            /* step 2: the last three ticks of the 0x12 timer spit. */
            if (e->timer && --e->timer <= 2) {
                aos_audio_play_song(0x89);
                dryad_spit(e, soma, &spawned, random);
            }
            if (e->anim.flags & AOS_ANIM_ENDED) {
                play_loop(e, kind->anims, 2, true);
                e->step = 1;
            }
        }
        break;
    case 2:
        if (e->anim.flags & AOS_ANIM_ENDED) {
            e->state = 0;
            e->step = e->substep = 0;
        }
        break;
    case 3: {
        /* sub_080AE650: + 0xB step 0 arms the 0x40 timer and, when the
         * sub_0803E654 draw's sign bit is clear, owes the affine turn
         * (+ 0x59 bit 0x10) and rewrites the + 0x58 facing nibble with
         * rand & 0x1F (cosmetic, not ported). Step 1 plays song 0x6E
         * and the sub_0806D5C0 drift burst while the timer is a
         * multiple of eight, marks + 0xF at 0x10 (the fade ledger,
         * not ported), counts down and deletes at zero. The shared tail
         * turns the sprite by timer << 0xA while the turn flag lasts
         * and runs the sub_0806D518 blink palette merge (not ported). */
        if (e->step == 0) {
            e->timer = 0x40;
            e->step = 1;
            if ((int32_t)(random() << 16) >= 0) e->dryad_flags |= 0x10;
        } else if (e->step == 1) {
            if ((e->timer & 7) == 0) {
                aos_audio_play_song(0x6E);
                for (int i = 0; i < 4; ++i) {
                    int32_t vx, vy;
                    random_direction(random, &vx, &vy);
                    AosEffect *g = spawn_drift(AOS_EFFECT_HIT_ELEMENT_5,
                                               e->x, e->y - 0x100000, &spawned);
                    if (g) {
                        g->vx = vx;
                        g->vy = vy;
                        g->ax = vx / 0x18;
                        g->ay = vy / 0x18;
                    }
                }
            }
            if (e->timer && --e->timer == 0) {
                e->removed = true;
                e->hidden = true;
                e->dryad_flags &= ~0x10;
            }
        }
        if (e->dryad_flags & 0x10) e->angle = (uint32_t)e->timer << 10;
        break;
    }
    case 4:
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

/* The seed's landed tail shared by states 2 and 3 (sub_080AE8E8): x
 * tracks the player's, y the sub_0801CFB0 floor probe (snap_to_floor),
 * state 3 emits the sub_08021654 contact record (element 0, the + 0x3C
 * attack byte) every sixteenth tick and the timer deletes it. */
static void dryad_seed_landed(AosEnemy *s, AosSoma *soma,
                              const AosCollision *layer, int soma_def,
                              AosHitReport *report) {
    s->x = (s->x & 0xFFFF) | ((int32_t)(target_x(soma) >> 16) << 16);
    snap_to_floor(s, layer);
    /* The record is positional: it only reaches a player next to the
     * seed (the native hitbox extent is not traced — 16 px stands in). */
    if (s->state == 3 && (s->timer & 0xF) == 0 &&
        player_in(soma, (int16_t)((s->x >> 16) - 8),
                  (int16_t)((s->y >> 16) - 8), 16, 16)) {
        int dmg = aos_soma_take_elemental_hit(soma, s->stats.contact, soma_def,
                                              s->x, 0, s->contact_elements);
        if (dmg > 0) {
            report->soma_hit = true;
            report->soma_damage += dmg;
        }
    }
    if (s->timer && --s->timer == 0) s->removed = true;
}

/* sub_080AE7E4: state 0 (first update) registers the (-2, -2, 4, 4)
 * boxes of the 0x0404FEFE preset on both slots, animation 8 loop and
 * falls into state 1; state 1 integrates the velocity with 0x1000
 * gravity and the sub_080AE78C landing callback moves it to state 2
 * (timer 0xB4) — approximated by the floor probe; state 2 walks the
 * 0x08528604 table {6, 7, 0xFF} on the animation end then state 3
 * (timer 0x78); states 2 and 3 share the landed tail; sub_0806D128(1)
 * deletes it off screen. */
/* Tsuchinoko (EnemyTsuchinokoCreate 0x080A71CC, EnemyTsuchinokoUpdate
 * 0x080A7B54): a hopping snake. State 0 sub_080A7294 is a 13-step jump
 * table on + 0xB: step 0 faces the player and wakes into step 1 (anim 1
 * once) when the (+ 0x42 - 0x5A, + 0x46 - 0x64) 0xB4 x 0xC8 box of
 * sub_0806CFFC holds him or sub_0806D54C reports damage; step 1 at anim
 * frame 2 tick 0 picks the facing — the near 0x78 box inverts it, the
 * far 0xC8 box keeps it, outside both faces him — then hops (vy -1.5,
 * ay 0x2800, vx +/-1.0); step 2 sub_08069A00 walks it with bounce 1.0,
 * and on the ground bit rolls the landed tree: two RNG & 0x7F <= 0xF
 * gates to the state 1 attack or the step 0xA burrow sequence, then the
 * + 0x14 / + 0x34 HP thresholds pick a re-hop or the burrow. Steps 0xA
 * to 0xC play anims 2 and 3 into the dormant rock spitter, which dies
 * when its animation ends. State 1 sub_080A7534 rears (anim 2) and at
 * frame 3 tick 0 spits the fast lob of sub_080A776C with song 0x85,
 * then re-enters the hop at step 1. State 2 sub_080A75B0 is the death:
 * two sub_0806DD64 gore chunks, a 0x3C timer, the sub_0806D518 blink
 * and delete. State 3 only animates. The hurt sub_080A76E0 deals the
 * generic damage, + 0x2D = 8, and on a lethal blow plays the
 * sub_0803F0E4 death table 0x085283FC (a custom frame sequence not
 * ported — the last animation keeps running), song 0x70, timer 0x3C,
 * state 2. */

AosHitReport aos_enemy_dryad_seed_update(AosEnemy *s, AosSoma *soma,
                                      const AosCollision *layer,
                                      const AosEnemyKind *kind,
                                      const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames,
                                      int soma_atk, int soma_def, int cam_x,
                                      int cam_y, uint32_t (*random)(void)) {
    (void)random;
    AosHitReport report = {0}, landed = {0};
    int sx = (s->x >> 16) - cam_x, sy = (s->y >> 16) - cam_y;
    if (outside_margins(kind, 4, sx, sy)) {
        s->removed = true;
        return report;
    }
    if (s->state == 0) {
        s->own_boxes = true;
        s->own_hurt = s->own_attack = (AosBox){-2, -2, 4, 4};
        play_loop(s, kind->anims, 8, true);
        s->state = 1;
    }
    switch (s->state) {
    case 1:
        s->x += s->vx;
        s->y += s->vy;
        s->vy += 0x1000;
        if (s->vy > 0 &&
            aos_floor_depth(layer, s->x >> 16, (s->y >> 16) + 4, 0, false) < 0) {
            snap_to_floor(s, layer);
            s->vx = s->vy = 0;
            s->state = 2;
            s->timer = 0xB4;
            /* The landed mine damages through the periodic record, not
             * its box. */
            s->combat.attack_off = true;
        }
        break;
    case 2:
        /* Table 0x08528604 {6, 7, 0xFF}: animation 6 then 7 once each,
         * then state 3 with timer 0x78. */
        if (s->step == 0) {
            play_loop(s, kind->anims, 6, false);
            s->step = 1;
        } else if (s->step == 1) {
            if (s->anim.flags & AOS_ANIM_ENDED) {
                play_loop(s, kind->anims, 7, false);
                s->step = 2;
            }
        } else if (s->anim.flags & AOS_ANIM_ENDED) {
            s->state = 3;
            s->timer = 0x78;
        }
        dryad_seed_landed(s, soma, layer, soma_def, &landed);
        break;
    case 3:
        dryad_seed_landed(s, soma, layer, soma_def, &landed);
        break;
    default:
        break;
    }
    if (!s->removed) {
        report = collide(s, soma, kind, weapon, weapon_frames, soma_atk,
                         soma_def);
        step_anim(s, kind->anims);
    }
    report.soma_hit = report.soma_hit || landed.soma_hit;
    report.soma_damage += landed.soma_damage;
    aos_combat_tick(&s->combat);
    return report;
}
