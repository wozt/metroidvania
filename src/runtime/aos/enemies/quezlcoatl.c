/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemy: Quezlcoatl (enemy 0x1A).
 *
 * EnemyQuezlcoatlCreate (0x08077DF4) loads tiles 0x081CBF74, palette
 * 0x0820A580 bank 1 and frames 0x0821631C through sub_080786B0, sets
 * + 0x65 = 0xC and allocates the + 0x20 trail container, then spawns
 * fifteen linked segments (sub_0806DFF8 with sub_08078318) numbered
 * + 0x5C = 1..15 at depths + 0x1C = 0xE down to 0 — the head roles are
 * + 0x65 = 3/4 (animated), the middle ones 0xF/0x10 and the tail 0x11.
 * Under the global 0x8E & 0x40 it parks in state 3; otherwise the
 * sub_0806AF98 floor snap and state 0.
 *
 * The head's state 0 (sub_08077FC0) is a two-step patrol: step 1 faces
 * the player side (sub_0806CF2C), probes the room edge with
 * sub_0806D188 (param0 or 0xF) into + 0x58 bit 6, clears the + 0x14
 * phase, sets vx = bit6 ? 2.0 : -2.0, vy = -4.0, ay = 0x2800, step 2
 * and empties the trail; step 2 applies the 31/32 vx friction inside
 * the + 0xD - 0x13 <= 0xE window, moves with the sub_08069A00 walk
 * (mapped to walk_collide), records the per-frame pixel delta into the
 * trail head-first and, on a floor result (& 4), re-arms step 1 with
 * static frame 0xC, the sub_0806D7F0 lunge (not ported) and song 0x7C
 * when beyond margin 6. State 3 (sub_08078178) is the dormant drift:
 * the same machine at vx = +-0.5, vy = -2.0, ay = 0x1800, re-arming on
 * pixel y > 0x46.
 *
 * Each segment (sub_08078318) while its parent slithers unshifts nothing
 * but reads its own trail record at depth * 8 bytes — the sign byte of
 * entry depth * 4 for facing and the (dx, dy) of entry depth * 4 + 4 as
 * its pixel move — so segment k replays the head's motion from 4(k + 1)
 * frames ago. The native camera-scroll compensation (+ 0x18/+ 0x1A) is
 * not ported: the trail already stores world-pixel deltas. On the
 * parent's death each segment scatters (state 1: accel_move 28.0, the
 * sub_0806D518 blink on + 0xD, delete at 0); a deleted parent deletes
 * the segment.
 *
 * The hurt callback sub_0807829C only flashes (+ 0x2D = 8) and applies
 * the damage; lethal runs sub_080683BC, song 0x6B, the + 0x5A palette
 * merge (kept in qz_tint), + 0xD = 0x28, state 2 — wired in
 * aos_enemy.c. State 2 spawns two gore entities (sub_08078588 types
 * 0x12 and 0xB at +-8.0, sub_0806D578, three sub_0806D5C0) then deletes
 * the trail container and itself over three + 0xB steps.
 */
#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../core/aos_collision.h"
#include "../core/aos_soma.h"
#include "../math/aos_math.h"

#define collide aos_enemy_collide
#define copy_result aos_enemy_copy_collision_result
#define outside_margins aos_enemy_outside_margins
#define step_anim aos_enemy_step_anim
#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define soldier_face aos_enemy_soldier_face
#define snap_to_floor aos_enemy_snap_to_floor
#define walk_collide aos_enemy_walk_collide
#define accel_move aos_enemy_accel_move
#define spawn_flame_at aos_enemy_spawn_flame_at
#define room_columns aos_enemy_room_columns
#define random_centred aos_enemy_random_centred

static bool qz_is_quezlcoatl(uint8_t id) { return id == AOS_ENEMY_QUEZLCOATL; }

static int qz_px(const AosEnemy *e) { return (int)(e->x >> 16); }
static int qz_py(const AosEnemy *e) { return (int)(e->y >> 16); }

/* The trail shift of sub_08077FC0/sub_08078178: 119 entries of 2 bytes
 * move one slot up, the new (dx, dy) pixel delta lands at index 0. */
static void qz_trail_push(AosEnemy *e, int dx, int dy) {
    int8_t *t = e->qz_trail;
    for (int i = 238; i > 0; i -= 2) {
        t[i] = t[i - 2];
        t[i + 1] = t[i - 1];
    }
    t[0] = (int8_t)dx;
    t[1] = (int8_t)dy;
}

static void qz_trail_clear(AosEnemy *e) {
    for (int i = 0; i < 240; i += 2) {
        e->qz_trail[i] = 0;
        e->qz_trail[i + 1] = 0;
    }
}

/* sub_0806D188(e, probe) mapped onto the room column probe: 1 at the
 * left edge, 2 at the right edge, 0 inside (the native returns feed the
 * + 0x58 bit 6 facing). */
static int qz_edge_probe(const AosEnemy *e, const AosCollision *layer,
                         int probe) {
    (void)probe;
    int column = qz_px(e) >> 3;
    int width = room_columns(layer);
    if (column < 1) return 1;
    if (column > width - 1) return 2;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Head: state 0 patrol (sub_08077FC0) and dormant drift (sub_08078178) */
/* ------------------------------------------------------------------ */

static void qz_state0(AosEnemy *e, const AosCollision *layer,
                      const AosEnemyKind *kind, AosSoma *soma,
                      const AosWeaponEntity *weapon,
                      const AosWeaponFrames *weapon_frames,
                      int soma_atk, int soma_def,
                      AosHitReport *report, int cam_x, int cam_y,
                      uint32_t (*random)(void)) {
    (void)random;
    int sx = qz_px(e) - cam_x, sy = qz_py(e) - cam_y;
    switch (e->step) {
    case 0:
        e->step = 1;
        /* fall through */
    case 1: {
        /* Facing refresh then the edge probe into + 0x58 bit 6. */
        soldier_face(e, soma);
        int probe = e->param0 ? e->param0 : 0xF;
        int edge = qz_edge_probe(e, layer, probe);
        if (edge == 1) e->mirrored = true;
        else if (edge == 2) e->mirrored = false;
        e->phase = 0;
        e->vx = e->mirrored ? 0x20000 : (int32_t)0xFFFE0000;
        e->vy = (int32_t)0xFFFC0000;
        e->ay = 0x2800;
        e->step = 2;
        e->timer = 0;
        qz_trail_clear(e);
        return;
    }
    default:
        break;
    }
    /* step 2: the per-frame patrol tick. */
    e->timer++;
    if ((uint8_t)(e->timer - 0x13) <= 0xE)
        e->vx = (int32_t)(((int64_t)e->vx * 31) / 32);   /* sub_0806D490(e, 0x20) */
    int ox = qz_px(e), oy = qz_py(e);
    int ip = walk_collide(e, layer, 0x100000, (int32_t)0xFFF00000, 0x180000, 8);
    qz_trail_push(e, qz_px(e) - ox, qz_py(e) - oy);
    /* The shared hurt/contact pass of sub_0807829C runs each patrol
     * frame (the native update calls it through the enemy tick). */
    {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(report, &hit);
    }
    if (ip & 4) {
        e->phase++;
        e->step = 1;
        e->static_frame = 0xC;
        if (outside_margins(kind, 6, sx, sy))
            aos_audio_play_song(0x7C);
    } else {
        /* + 0x65: 0xD while vy stays negative, 0xC once it rises. */
        e->static_frame =
            (uint32_t)(e->vy + 0x1FFFF) > 0x3FFFE ? 0xD : 0xC;
    }
    (void)report;
}

static void qz_state3(AosEnemy *e, const AosCollision *layer,
                      const AosEnemyKind *kind, AosSoma *soma,
                      int cam_x, int cam_y) {
    (void)kind; (void)soma; (void)layer;
    int sx = qz_px(e) - cam_x, sy = qz_py(e) - cam_y;
    (void)sx; (void)sy;
    switch (e->step) {
    case 0:
        e->step = 1;
        /* fall through */
    case 1: {
        /* Dormant facing from the pixel position, then the slow drift. */
        if (qz_px(e) <= 0x18) e->mirrored = true;
        else if (qz_px(e) > 0x77) e->mirrored = false;
        e->vx = e->mirrored ? 0x8000 : (int32_t)0xFFFF8000;
        e->vy = (int32_t)0xFFFE0000;
        e->ay = 0x1800;
        e->step = 2;
        e->timer = 0;
        qz_trail_clear(e);
        return;
    }
    default:
        break;
    }
    int ox = qz_px(e), oy = qz_py(e);
    accel_move(e, 0x40000);                    /* sub_0806D3D8(e, 4.0) */
    qz_trail_push(e, qz_px(e) - ox, qz_py(e) - oy);
    if (qz_py(e) > 0x46) {
        e->step = 1;
        e->static_frame = 0xC;
    } else {
        e->static_frame =
            (uint32_t)(e->vy + 0xFFFF) > 0x1FFFE ? 0xD : 0xC;
    }
}

/* ------------------------------------------------------------------ */
/* Segments: sub_08078318                                               */
/* ------------------------------------------------------------------ */

static AosHitReport qz_segment_update(AosEnemy *e, AosSoma *soma,
                              const AosEnemyKind *kind,
                              const AosWeaponEntity *weapon,
                              const AosWeaponFrames *weapon_frames,
                              int soma_atk, int soma_def,
                              uint32_t (*random)(void),
                              AosHitReport *report) {
    AosEnemy *p = e->parent;
    if (!p || p->removed || !qz_is_quezlcoatl(p->id)) {
        e->removed = true;
        return *report;
    }
    if (e->state == 0) {
        /* Linked flags: the parent's + 0x58 bit 0x20 and + 0x5A bits. */
        e->hidden = p->hidden;
        e->qz_tint = p->qz_tint;
        e->timer++;
        if (!p->hit_flash) {
            /* Slither follow: the record at depth * 8 bytes — the sign
             * of entry depth * 4 steers the facing, entries depth * 4 +
             * 4 give the (dx, dy) pixel move. */
            int d = e->qz_depth;
            int dir = (int8_t)p->qz_trail[d * 8];
            int dx = (int8_t)p->qz_trail[d * 8 + 8];
            int dy = (int8_t)p->qz_trail[d * 8 + 9];
            e->qz_tx = (int16_t)dx;
            e->qz_ty = (int16_t)dy;
            e->x += (int32_t)e->qz_tx << 16;
            e->y += (int32_t)e->qz_ty << 16;
            if (dir > 0) e->mirrored = true;
            else if (dir < 0) e->mirrored = false;
            if (d <= 5) e->static_frame = (e->timer & 8) ? 4 : 3;
        }
        /* Every eighth frame the segment runs its contact pass. */
        if ((e->timer & 7) == 0) {
            AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                       soma_atk, soma_def);
            copy_result(report, &hit);
        }
        if (p->state == 2) {
            /* The parent dies: the segment scatters. */
            e->state = 1;
            e->step = 0;
            e->ax = 0;
            e->ay = 0x2000;
            e->timer = (uint8_t)(0x3C + (random() & 3));
            int d = e->qz_depth;
            e->qz_tx = (int16_t)(int8_t)p->qz_trail[d * 8 + 8];
            e->qz_ty = (int16_t)(int8_t)p->qz_trail[d * 8 + 9];
        }
        return *report;
    }
    /* state 1: the scatter — accelerate to 28.0, blink on + 0xD, gone
     * once it runs out. */
    if (e->step == 0) {
        e->step = 1;
        e->combat.attack_off = true;
        e->combat.hurt_off = true;
    }
    accel_move(e, 0x1C00000);
    e->hidden = e->timer <= 0x27
                    ? (kind->blink[e->timer] & 1) != 0
                    : false;
    e->timer--;
    if ((int8_t)e->timer <= 0) e->removed = true;
    return *report;
}

/* ------------------------------------------------------------------ */
/* Gore: sub_08078588 create / sub_08078654 update                      */
/* ------------------------------------------------------------------ */

static AosHitReport qz_gore_update(AosEnemy *e, const AosEnemyKind *kind,
                                     uint32_t (*random)(void),
                                     AosHitReport *report) {
    (void)random;
    /* sub_08078654: move at 28.0, blink on + 0xD, a flame every eighth
     * frame, deleted once the countdown ends. */
    accel_move(e, 0x1C00000);
    e->hidden = e->timer <= 0x27
                    ? (kind->blink[e->timer] & 1) != 0
                    : false;
    if ((e->timer & 7) == 0)
        spawn_flame_at(e->x, e->y, 0x80000, 0x80000, random, report);
    e->timer--;
    if ((int8_t)e->timer <= 0) e->removed = true;
    return *report;
}

/* ------------------------------------------------------------------ */
/* Spawning and the public updates                                      */
/* ------------------------------------------------------------------ */

/* The fifteen segments of the native create loop (r7 = 0xE down to 0),
 * emitted 8 + 7 over the first two updates (the report cap). */
static void qz_spawn_segments(AosHitReport *report, AosEnemy *e,
                              const AosAnimSet *anims) {
    int emitted = e->substep;
    if (emitted >= 15) return;
    int want = emitted < 8 ? 8 : 15;
    int seg = emitted + 1;                    /* + 0x5C = 1..15 */
    int i = emitted;
    for (; i < want; ++i, ++seg) {
        if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) break;
        int depth = 14 - i;                   /* + 0x1C = 0xE .. 0 */
        AosEnemy *c = &report->children[report->child_count++];
        *c = *e;
        c->role = AOS_ROLE_QUEZLCOATL_SEGMENT;
        c->parent = e;
        c->state = 0;
        c->step = c->substep = 0;
        c->timer = 0;
        c->vx = c->vy = c->ax = c->ay = 0;
        c->hp = 1;
        c->hit_flash = 0;
        c->defeated = false;
        c->hidden = false;
        c->cycle_done = false;
        c->removed = false;
        c->param0 = c->param1 = 0;
        c->spawned_count = 0;
        c->boss_parts_spawned = true;
        c->static_frame = -1;
        c->qz_depth = (int8_t)depth;
        c->qz_seg = (uint8_t)seg;
        c->qz_tx = c->qz_ty = 0;
        c->qz_pal = e->qz_pal;
        c->qz_tint = 0;
        c->x = e->x;
        c->y = e->y - 0x40000;
        /* + 0x65 role: depth <= 5 animates 3/4 by its timer, <= 9 is
         * 0xF, <= 0xD is 0x10 and the tail (0xE) is 0x11. */
        int frame;
        if (depth <= 5) frame = 3;
        else if (depth <= 9) frame = 0xF;
        else if (depth <= 0xD) frame = 0x10;
        else frame = 0x11;
        c->static_frame = frame;
        c->contact_elements = e->contact_elements;
        /* sub_08078544: the segment hurts on contact but takes none. */
        c->combat.attack_off = false;
        c->combat.hurt_off = true;
        play_loop(c, anims, (unsigned)frame, false);
        step_anim(c, anims);
    }
    e->substep = (uint8_t)i;
}

AosHitReport aos_enemy_quezlcoatl_update(
    AosEnemy *e, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0 };
    const AosAnimSet *anims = kind->anims;
    if (e->role == AOS_ROLE_QUEZLCOATL_SEGMENT)
        return qz_segment_update(e, soma, kind, weapon, weapon_frames,
                                 soma_atk, soma_def, random, &report);
    if (e->role == AOS_ROLE_QUEZLCOATL_GORE)
        return qz_gore_update(e, kind, random, &report);
    if (e->role != AOS_ROLE_ENEMY || !qz_is_quezlcoatl(e->id))
        return report;

    int sx = qz_px(e) - cam_x, sy = qz_py(e) - cam_y;
    if (e->state != 2) {
        e->hidden = outside_margins(kind, 4, sx, sy);
        if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 &&
            (uint16_t)sy <= 0xB0)
            report.targetable = true;
    } else {
        e->hidden = outside_margins(kind, 4, sx, sy);
    }
    if (e->hit_flash && e->state != 2) {
        /* sub_0806AD24 gate: a flashing live body runs contact alone. */
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(&report, &hit);
        return report;
    }
    /* The first updates emit the fifteen segments (8 then 7). */
    if (e->state != 2 && e->substep < 15)
        qz_spawn_segments(&report, e, anims);

    switch (e->state) {
    case 0:
        qz_state0(e, layer, kind, soma, weapon, weapon_frames,
                  soma_atk, soma_def, &report, cam_x, cam_y, random);
        break;
    case 1:
        /* state 1 only runs the shared collision pass natively. */
        break;
    case 2:
        /* sub_08077D22: the three-step death — gore on step 0, the trail
         * container's deletion on step 1 (the segments notice the
         * parent's state themselves), self-delete on step 2. */
        if (e->step == 0) {
            e->step = 1;
            /* sub_08078588 twice at (x +- 8.0, y - 8.0), types 0x12
             * and 0xB — the +- follows the + 0x58 bit 6 facing. */
            int gox = e->x + (e->mirrored ? (int32_t)0xFFF80000 : 0x80000);
            int goy = e->y + (int32_t)0xFFF80000;
            for (int i = 0; i < 2 && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
                AosEnemy *g = &report.children[report.child_count++];
                *g = *e;
                g->role = AOS_ROLE_QUEZLCOATL_GORE;
                g->parent = e;
                g->state = 0;
                g->step = g->substep = 0;
                g->hp = 1;
                g->hit_flash = 0;
                g->defeated = false;
                g->hidden = false;
                g->cycle_done = false;
                g->removed = false;
                g->static_frame = i == 0 ? 0x12 : 0xB;
                g->vx = e->vx + (int32_t)(random() & 0x7FFF) - 0x4000;
                g->vy = (int32_t)(random() & 0x7FFF) - 0x24000;
                g->ax = 0;
                g->ay = 0x2000;
                g->timer = (uint8_t)(0x3C + (random() & 3));
                g->combat.attack_off = true;
                g->combat.hurt_off = true;
                g->x = gox;
                g->y = goy;
                play_loop(g, anims, (unsigned)g->static_frame, false);
                step_anim(g, anims);
            }
            /* sub_0806D578 + three sub_0806D5C0 flames along the body. */
            spawn_flame_at(e->x - 0x80000, e->y, 0x80000, 0x80000, random, &report);
            spawn_flame_at(e->x, e->y - 0x80000, 0x80000, 0x80000, random, &report);
            spawn_flame_at(e->x + 0x80000, e->y, 0x80000, 0x80000, random, &report);
            spawn_flame_at(e->x, e->y + 0x80000, 0x80000, 0x80000, random, &report);
            e->hidden = true;                      /* + 0x58 |= 0x20 */
        } else if (e->step == 1) {
            e->step = 2;
            /* The trail container's deletion: the segments scatter on
             * their own through the parent's state. */
        } else {
            e->removed = true;                     /* + 0x59 |= 8 */
        }
        break;
    case 3:
        qz_state3(e, layer, kind, soma, cam_x, cam_y);
        break;
    default:
        break;
    }
    return report;
}
