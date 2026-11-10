/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Giant Worm (enemy 0x3E) and Poison Worm (0x47) with their segments.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#define collide aos_enemy_collide
#define flag_8e_40 aos_enemy_flag_8e_40
#define play aos_enemy_play
#define player_near aos_enemy_player_near
#define skeleton_merge aos_enemy_skeleton_merge
#define snap_to_floor aos_enemy_snap_to_floor
#define spawn_drift aos_enemy_spawn_drift
#define step_anim aos_enemy_step_anim
#define target_x aos_enemy_target_x
#define worm_segment_hurt aos_enemy_worm_segment_hurt

/* ---- Giant Worm / Poison Worm (sub_080AD7F0 family) ---- */

/* sub_0806D5C0(x, y, rx, ry, param): a drifting dust effect
 * (EntityCreateInRange(0x49, 0xCF)) at x + RNG % rx - rx/2,
 * y + RNG % ry - ry/2. The + 0x5C - 2 = 0xF parameter selects a
 * sub-animation the shared effect does not model: AOS_EFFECT_DUST
 * stands for it (the Minotaur approximation). */
static void worm_dust(int32_t x, int32_t y, uint32_t rx, uint32_t ry,
                      AosHitReport *report, uint32_t (*random)(void)) {
    if (report->effect_count >= AOS_ENEMY_MAX_EFFECTS) return;
    int32_t ox = (int32_t)(random() % rx) - (int32_t)(rx >> 1);
    int32_t oy = (int32_t)(random() % ry) - (int32_t)(ry >> 1);
    spawn_drift(AOS_EFFECT_DUST, x + ox, y + oy, report);
}

/* sub_080ADB94(e, dir, r2, r3): the emerge hop step machine on + 0xB.
 * Step 0 launches horizontally (vx +/-0x8000, ax -/+0x300 by dir)
 * when r3 == 0 or r2 > 7; step 1 adds ax to vx until it crosses 0;
 * step 2 waits 0x10 ticks; step 0 relaunches. Position integrates
 * at the end of every call. */
static void worm_emerge(AosEnemy *e, int dir, int r2, int r3) {
    if (e->step == 0) {
        if (r3 == 0 || r2 > 7) {
            if (dir) { e->vx = 0x8000; e->ax = -0x300; }
            else     { e->vx = -0x8000; e->ax = 0x300; }
            e->step = 1;
            e->timer = 0;
        }
    } else if (e->step == 1) {
        e->timer++;
        e->vx += e->ax;
        if ((e->ax > 0 && e->vx >= 0) || (e->ax <= 0 && e->vx <= 0))
            e->step = 2;
    } else if (e->step == 2) {
        if (++e->timer > 0x10) {
            e->step = 0;
            e->timer = 0;
        }
    }
    e->x += e->vx;
    e->y += e->vy;
}

/* sub_080AD9FC(e): settle on the floor and probe the hop side.
 * The floor query sub_08001D94 at (x, y + 0xA) adjusts the y high
 * word by depth + 2; without a floor the owner (+ 0x14: the parent,
 * or the entity itself for the parent whose + 0x14 is itself) waits
 * 4 ticks in + 0x1D. A wall probe 0x18 px ahead also sets it: the
 * parent probes its hop side (dir 0 unmirrored left, dir 1 mirrored
 * right), segment 4 the opposite side by the parent's dir
 * (sub_0800207C / sub_08002058 = aos_wall_push_right /
 * aos_wall_push_left). */
static void worm_settle(AosEnemy *e, const AosCollision *layer) {
    AosEnemy *owner = e->role == AOS_ROLE_WORM_SEGMENT && e->parent ? e->parent : e;
    int x = (int)(e->x >> 16), y = (int)(e->y >> 16);
    int depth = aos_floor_depth(layer, x, y + 0xA, 0, false);
    if (depth) {
        e->y += (int32_t)(depth + 2) << 16;
    } else if (owner->worm_wait == 0) {
        owner->worm_wait = 4;
    }
    int push = 0;
    if (e->worm_index == 0) {
        if (e->worm_dir == 0) {
            if (!e->mirrored) push = aos_wall_push_right(layer, x - 0x18, y - 8);
        } else if (e->mirrored) {
            push = aos_wall_push_left(layer, x + 0x18, y - 8);
        }
    } else if (e->worm_index == 4 && owner != e) {
        if (owner->worm_dir != 0) {
            if (!e->mirrored) push = aos_wall_push_left(layer, x + 0x18, y - 8);
        } else if (e->mirrored) {
            push = aos_wall_push_right(layer, x - 0x18, y - 8);
        }
    }
    if (push && owner->worm_wait == 0) owner->worm_wait = 4;
}

/* sub_080AE008(e): the terrain slope angle under the entity, the
 * + 0x54 affine angle. Two floor scans in 8 px steps down from
 * y + 0xA at x - 8 and x + 8 give the ground heights; ArcTan2 of
 * the (-16, left - right) span plus 0x8000. */
static uint16_t worm_ground_angle(const AosEnemy *e, const AosCollision *layer) {
    int x = (int)(e->x >> 16), y = (int)(e->y >> 16);
    int left_y = y, right_y = y;
    for (int r6 = 0; r6 <= 0x1F; r6 += 8) {
        int d = aos_floor_depth(layer, x - 8, y + 0xA + r6, 0, false);
        if (d) { left_y = y + d + r6; break; }
    }
    int probe_y = y + 0xA;
    for (int r6 = 0; r6 <= 0x1F; r6 += 8, probe_y += 8) {
        int d = aos_floor_depth(layer, x + 8, probe_y, 0, false);
        if (d) { right_y = y + d + r6; break; }
    }
    return (uint16_t)(aos_arctan2(-16, (int16_t)(left_y - right_y)) + 0x8000);
}

/* sub_080AD748(c, hit, hurt): a segment's hurt callback forwards the
 * damage to the parent (+ 0x14): hit flash 8, HP + 0x34 reduced; at
 * HP <= 0 the parent dies in state 3 — PlaySong(0x70), + 0x0D = 0x40,
 * vx toward the player (+/-0x8000 by its x) and vy = -0x30000. The
 * sub_080683BC reward is the parent's own killed report. */
void aos_enemy_worm_segment_hurt(AosEnemy *c, AosEnemy *parent, AosSoma *soma,
                              AosHitReport *report) {
    if (!report->enemy_hit || report->enemy_damage <= 0) return;
    /* The native callback replaces the child's own HP loss. */
    c->hp = (int16_t)(c->hp + report->enemy_damage);
    parent->hit_flash = 8;
    parent->hp = (int16_t)(parent->hp - report->enemy_damage);
    if (parent->hp <= 0 && parent->state != 3) {
        aos_audio_play_song(0x70);
        parent->state = 3;
        parent->timer = 0x40;
        parent->vx = soma->x > parent->x ? 0x8000 : -0x8000;
        parent->vy = -0x30000;
    }
}

/* sub_080AE390 is the cleanup walk over the + 0x20-indexed chain:
 * the room removes dead entities, nothing to port. */

AosHitReport aos_enemy_worm_update(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                                const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                const AosWeaponFrames *weapon_frames, int soma_atk,
                                int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0 };
    /* The four linked segments of sub_080AD7F0, emitted on the first
     * update (the native create emits them): each + 0x18 links to the
     * previous entity, + 0x1E its index, + 0x22 index << 12, x =
     * previous x +/- 0xA0000 by the facing bit, y = previous y, then
     * the sub_080ADB0C floor snap. */
    if (!e->boss_parts_spawned) {
        e->boss_parts_spawned = true;
        int32_t offset = e->mirrored ? -0xA0000 : 0xA0000;
        int32_t cx = e->x, cy = e->y;
        for (int i = 1; i <= 4 && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
            AosEnemy s = {0};
            s.id = e->id;
            s.role = AOS_ROLE_WORM_SEGMENT;
            s.static_frame = -1;
            s.x = cx + offset;
            s.y = cy;
            s.mirrored = e->mirrored;
            s.stats = e->stats;
            s.hp = e->stats.hp;
            s.combat.type = AOS_TYPE_ENEMY;
            s.own_boxes = true;
            s.own_hurt = s.own_attack = (AosBox){-8, -8, 16, 16};
            s.worm_index = (uint8_t)i;
            s.worm_phase = (uint16_t)(i << 12);
            s.variant = e->variant;
            /* The segment's own contact hit keeps the parent's element
             * word (sub_080AE334 / sub_080AE3CC are installed on all
             * five entities by sub_080AD7F0). */
            s.contact_elements = e->contact_elements;
            cx = s.x;
            cy = s.y;
            snap_to_floor(&s, layer);
            report.children[report.child_count++] = s;
        }
    }
    /* sub_0806AD24: alive and flashing (+ 0x2D) — the hit-stop gate
     * sub_0806D4C0 is always false here. A stunned parent only
     * collides (states other than 3), skipping the state machine,
     * the affine setup and the animation commit. */
    if (e->state != 0 && e->hp > 0 && e->hit_flash) {
        e->hit_flash--;
        if (e->state != 3) {
            AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                       soma_atk, soma_def);
            skeleton_merge(&report, &hit);
        }
        return report;
    }
    /* sub_080AD748 kills the parent through a segment's callback: its
     * state-3 lunge returns before the epilogue, so the death reward
     * report is raised here, once, on the first state-3 update. */
    if (e->state == 3 && e->hp <= 0 && !e->defeated) {
        e->defeated = true;
        report.killed = true;
    }
    switch (e->state) {
    case 0:
        e->state = 0;
        /* fall through */
    case 1:
        /* + 0x1D wall-wait: while set, re-emerge each tick and count
         * down; at 4 the hop direction toggles. Otherwise decide the
         * direction: the sub_0806CFFC box (x - 0xA0, y - 0x3C,
         * 0x140 x 0x78) picks the aim target's side, a miss rolls
         * RandomNumberGenerator & 1. */
        if (e->worm_wait) {
            e->state = 2;
            if (e->worm_wait == 4) e->worm_dir ^= 1;
            e->worm_wait--;
        } else {
            if (!player_near(soma, (e->x >> 16) - 0xA0, (e->y >> 16) - 0x3C,
                             0x140, 0x78)) {
                e->worm_dir = (uint8_t)(random() & 1);
            } else {
                e->worm_dir = target_x(soma) >= e->x ? 1 : 0;
            }
            e->state = 2;
        }
        break;
    case 2:
        worm_emerge(e, e->worm_dir, 0, 0);
        worm_settle(e, layer);
        if (e->step == 0) e->state = 1;
        break;
    case 3:
        /* The death lunge (+ 0x1C tells the segments to attack too):
         * position += velocity, vy += 0x2000, the affine angle turns
         * 0x100 against the facing, dust every 8th tick; the + 0x0D
         * countdown ends in EntityDelete (sub_0803E6BC is cosmetic). */
        e->worm_lunge = 1;
        e->x += e->vx;
        e->y += e->vy;
        e->vy += 0x2000;
        e->angle = (uint32_t)((int32_t)e->angle + (e->mirrored ? -0x100 : 0x100));
        if ((e->timer & 7) == 0)
            worm_dust(e->x, e->y, 0x100000, 0x10000, &report, random);
        if (--e->timer == 0) {
            e->removed = true;
            return report;
        }
        step_anim(e, kind->anims);
        aos_combat_tick(&e->combat);
        return report;
    case 4:
        e->timer = 2;
        e->state = 5;
        /* fall through */
    case 5:
        /* A pure countdown to EntityDelete: no collision, no affine,
         * no animation commit. */
        if (--e->timer == 0) e->removed = true;
        return report;
    case 6:
        /* Inert under the global 0x8E & 0x40: the affine identity and
         * the animation commit only. */
        step_anim(e, kind->anims);
        aos_combat_tick(&e->combat);
        return report;
    default:
        break;
    }
    /* _080AE2E8 epilogue: the collision pass and the terrain angle
     * (state 3 keeps its own), then the affine commit sub_0803F17C
     * (step_anim) — the renderer reads e->angle. */
    if (e->state != 3) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        skeleton_merge(&report, &hit);
        e->angle = worm_ground_angle(e, layer);
    }
    step_anim(e, kind->anims);
    aos_combat_tick(&e->combat);
    return report;
}

/* sub_0803F2C8 init of a segment (child state 0): the parent's
 * palette byte and facing bit copy over (the + 0x2C / + 0x5A merges
 * are cosmetic), animation 1 — 2 for segment 4 — looping, + 0x5C =
 * 0x11, the hit callbacks and the 0x1010F8F8 box pair are set at
 * emit, state 1 (4 under the global flag). */
static void worm_segment_init(AosEnemy *c, const AosEnemyKind *kind) {
    play(c, kind->anims, c->worm_index == 4 ? 2 : 1);
    step_anim(c, kind->anims);
    c->state = flag_8e_40 ? 4 : 1;
}

/* The affine tail of sub_080ADC2C (states 1, 2 and 4+): sl = 0x8000
 * unmirrored else 0; the Poison Worm's segments (parent + 0x21) add a
 * sine wobble on the + 0x22 phase (3 * sine << 13, phase += 0x200 —
 * the secondary affine scale is not modelled); the rotation is the
 * direction to the + 0x18 follow target plus sl (sub_08068A70). */
static void worm_segment_affine(AosEnemy *c, AosEnemy *parent) {
    if (parent->variant != 0) c->worm_phase = (uint16_t)(c->worm_phase + 0x200);
    if (!c->follow) return;
    int32_t dx = (int16_t)((c->follow->x >> 16) - (c->x >> 16));
    int32_t dy = (int16_t)((c->follow->y >> 16) - (c->y >> 16));
    uint32_t sl = c->mirrored ? 0 : 0x8000;
    c->angle = (uint32_t)((uint16_t)aos_arctan2((int16_t)dx, (int16_t)dy) + sl);
}

AosHitReport aos_enemy_worm_segment_update(AosEnemy *c, AosSoma *soma,
                                        const AosCollision *layer, const AosEnemyKind *kind,
                                        const AosWeaponEntity *weapon,
                                        const AosWeaponFrames *weapon_frames, int soma_atk,
                                        int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0 };
    AosEnemy *parent = c->parent;
    if (!parent || parent->removed) {
        c->removed = true;
        return report;
    }
    /* A live segment under a stunned parent (hit_flash; the hit-stop
     * gate is always false) only collides — the sub_08021278 tracking
     * flag and the sub_080429D0 record are approximated by the regular
     * pass. */
    if (c->state != 0 && parent->state != 3 && parent->hp > 0 && parent->hit_flash) {
        AosHitReport hit = collide(c, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        skeleton_merge(&report, &hit);
        return report;
    }
    switch (c->state) {
    case 0:
        /* The lazy init is skipped when the parent already lunges:
         * the segment deletes at once (the case-3 path). */
        if (parent->worm_lunge) {
            c->removed = true;
            return report;
        }
        worm_segment_init(c, kind);
        /* fall through */
    case 1:
        /* sub_080ADB94(c, parent + 0x1F, follow + 0x0D, 1): the hop is
         * gated by the follow target's timer (> 7), then
         * sub_080AD9FC(c) settles it. */
        worm_emerge(c, parent->worm_dir, c->follow ? c->follow->timer : 0, 1);
        worm_settle(c, layer);
        if (parent->worm_lunge) {
            /* The parent's death lunge: state 2, timer 0x38, the affine
             * angle sl, vx = index << 15 - 0x6000 flipped toward Soma,
             * vy = -0x30000. */
            c->state = 2;
            c->timer = 0x38;
            c->angle = c->mirrored ? 0 : 0x8000;
            c->vx = ((int32_t)c->worm_index << 15) - 0x6000;
            if (soma->x > c->x) c->vx = -c->vx;
            c->vy = -0x30000;
        }
        break;
    case 2:
        c->x += c->vx;
        c->y += c->vy;
        c->vy += 0x2000;
        c->angle = (uint32_t)((int32_t)c->angle + (c->mirrored ? -0x100 : 0x100));
        if ((c->timer & 7) == 0)
            worm_dust(c->x, c->y, 0x100000, 0x100000, &report, random);
        if (c->timer == 0) {
            c->removed = true;
            return report;
        }
        c->timer--;
        break;
    case 3:
        c->removed = true;
        return report;
    default:
        break;
    }
    worm_segment_affine(c, parent);
    if (parent->state != 3) {
        AosHitReport hit = collide(c, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        skeleton_merge(&report, &hit);
    }
    step_anim(c, kind->anims);
    aos_combat_tick(&c->combat);
    return report;
}
