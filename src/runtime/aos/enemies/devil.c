/* SPDX-License-Identifier: GPL-3.0-only */
/* Devil family (ids 0x30 Devil, 0x48 Arc Demon, 0x66 Demon Lord,
 * 0x68 Flame Demon): EnemyDevilCreate 0x0808FA30, EnemyDevilUpdate
 * 0x0808F964, state 0 sub_0808FE10, state 1 sub_08090914, state 3
 * sub_08091204; the Arc Demon/Demon Lord/Flame Demon creates and
 * updates are pure thunks onto the same routines.  One port covers
 * all four records; the + 0x36 variant byte is the enemy id itself.
 *
 *   state 0  sub_0808FE10  sixteen-step idle/approach machine: aggro
 *                         boxes off the + 0x42/+ 0x46 anchor, a + 0xC
 *                         sub-cycle re-forming the eight-imps swarm
 *                         every 0x16 ticks and a + 0x24 walk timer
 *   state 1  sub_08090914  sixteen-step attack machine: formation
 *                         changes, songs 0x86/0x78, the aimed volley
 *                         (sub_08091C50/sub_08091E20 fireballs) and
 *                         the return to state 0
 *   state 2  sub_0806AEAC  shared flame death (song 0x72, + 0x34 hp)
 *   state 3  sub_08091204  dormant formation cycle (0x8E & 0x40)
 *
 * The + 0x1C container entity's eight 0x2C-byte orbit records are
 * folded into the parent's dv arrays; its eight child actors (the
 * circling imps, sub_080917B4) and the two projectile kinds
 * (sub_08091D54 homing fireball, sub_08092018 straight orb) are real
 * spawned children here.
 *
 * Approximation notes (documented, not silent):
 * - The native container is a real entity whose + 0x40/+ 0x44 mirror
 *   the parent's position and whose + 0x34 takes child-death damage;
 *   here the records live on the parent and child hits damage the
 *   parent's hp directly through the shared hurt pass.
 * - sub_08068AD4/sub_08021924 collapse to outside_margins plus the
 *   (-0x10, 0, 0x110, 0xB0) targetable box; sub_0806D128(4) is the
 *   shared margin hide and sub_0806AD24 the + 0x2D hit-flash gate.
 * - sub_0806CF2C/sub_0806CEC0 facing probes collapse to face_player;
 *   sub_0806CFFC/sub_0806D54C to player_near/player_in_range;
 *   sub_080696B0's approach step is the shared move toward the target.
 * - The 0x0811900C/0x08119014 descriptor bytes pick each variant's
 *   eight child frame indices (dv_kind -> static_frame); the
 *   0x0811901C/0x08119024/0x0811902C tables give the per-slot
 *   sub-parameters, the 0x08527C1C/0x08527CD0 formation tables the
 *   follow-mode pixel offsets and per-slot scales.
 * - The imp dive (sub_080917B4 states 1-3) keeps the decoded launch
 *   geometry — ArcTan2 to the player minus 0x4000, sine/cosine offsets
 *   scaled by the slot table — with the native per-frame speed table
 *   approximated by a constant dive speed.
 * - sub_08042848/sub_080429D0 contact-record builders map to the
 *   shared collide with contact_elements; sub_08021654 contacts map
 *   to positional elemental hits like the dryad seed's.
 * - The sub_0806D260 callback-variant pick, the + 0x5A palette merge
 *   and the + 0x72 flag writes are not ported (the shared hurt pass
 *   covers damage); EntityCreateInRange's 0x49/0xCF bounds become the
 *   report child spawn.
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
#define player_in aos_enemy_player_in
#define player_near aos_enemy_player_near
#define player_in_range aos_enemy_player_in_range
#define face_player aos_enemy_face_player
#define generic_death aos_enemy_generic_death
#define spawn_drift aos_enemy_spawn_drift
#define clamp_to_room aos_enemy_clamp_to_room
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y
#define head_toward aos_enemy_head_toward
#define sine aos_sine
#define arctan2 aos_arctan2

static int32_t dv_px(const AosEnemy *e) { return (int16_t)(e->x >> 16); }
static int32_t dv_py(const AosEnemy *e) { return (int16_t)(e->y >> 16); }

/* sub_0806D490(e, n): vx * (n - 1) / n (signed division). */
static void dv_friction(AosEnemy *e, int n) {
    e->vx = (int32_t)(((int64_t)e->vx * (n - 1)) / n);
}

/* The facing refresh + move tail shared by every state-0/1 step:
 * sub_0806CF2C -> face_player, the + 0x58 bit 0x40 mirror write,
 * sub_0806D490(0x18) x-friction, sub_0806D4A8(0x10) y-friction and
 * sub_0806D3AC integrate. The devil flies — no room clamp. */
static void dv_move(AosEnemy *e, AosSoma *soma,
                    const AosCollision *layer) {
    (void)layer;
    face_player(e, soma);
    dv_friction(e, 0x18);
    e->vy = (int32_t)(((int64_t)e->vy * 0xF) / 0x10);
    e->x += e->vx;
    e->y += e->vy;
}

/* sub_08091624(self, dst, src, offset, [dist]): configure orbit slot
 * dst to circle record src — angle = ArcTan2 between the two record
 * positions + 0x10000, radius = |offset| capped at 0xFFFF added to the
 * angle, anchor flag 0x80000000 with anchor index src, radius = the
 * stack distance or the slot-to-slot distance. */
static void dv_slot_orbit(AosEnemy *e, int dst, int src, int32_t offset,
                          int32_t dist) {
    e->dv.oan[dst] = (int32_t)0x80000000;
    e->dv.oai[dst] = src;
    if (dist)
        e->dv.orad[dst] = dist;
    else {
        int32_t dx = e->dv.ox[src] - e->dv.ox[dst];
        int32_t dy = e->dv.oy[src] - e->dv.oy[dst];
        int64_t d = (int64_t)dx * dx + (int64_t)dy * dy;
        e->dv.orad[dst] = (int32_t)(d > 0 ? ((d + 0x8000) >> 16) : 0);
        /* native: Sqrt(dx^2+dy^2) << 16 >> 8 — keep subpixels */
        {
            double dd = dx * dx + dy * dy;
            e->dv.orad[dst] = (int32_t)(dd > 0 ? __builtin_sqrt(dd) : 0);
        }
    }
    int32_t dx = e->dv.ox[dst] - e->dv.ox[src];
    int32_t dy = e->dv.oy[dst] - e->dv.oy[src];
    int32_t ang = (int32_t)arctan2((int16_t)(dx >> 16), (int16_t)(dy >> 16));
    e->dv.oac[dst] = ang + 0x10000;
    int32_t off = offset < 0 ? -offset : offset;
    if (off > 0xFFFF) off = offset;
    e->dv.oat[dst] = e->dv.oac[dst] + off;
}

/* The formation reposition block shared by the state handlers: five
 * sub_08091624 calls (slots 1,2,3,5,6 with params 0,0,2,4,4 and the
 * native offsets) — the caller passes the variant's offset set. */
static void dv_reform(AosEnemy *e, int32_t o1, int32_t o2, int32_t o3,
                      int32_t o5, int32_t o6, int32_t s1, int32_t s2,
                      int32_t s3) {
    dv_slot_orbit(e, 1, 0, o1, s1);
    dv_slot_orbit(e, 2, 0, o2, s2);
    dv_slot_orbit(e, 3, 0, o3, s3);
    dv_slot_orbit(e, 5, 4, o5, s1);
    dv_slot_orbit(e, 6, 4, o6, s2);
}

/* sub_08091F80 / sub_08091F9C: save/restore the record positions. */
static void dv_save_pos(AosEnemy *e) {
    for (int i = 0; i < 8; ++i) {
        e->dv.osx[i] = e->dv.ox[i];
        e->dv.osy[i] = e->dv.oy[i];
    }
}

/* 0x08527C1C: the follow-mode (form + slot) * 2 byte pairs, read as
 * signed pixel offsets << 16. The ROM bytes cycle (12,124),(82,8). */
static const int8_t dv_follow_base[8] = {12, 124, 82, 8, 12, 124, 82, 8};
/* 0x08527CD0: five formation tables of eight int32 scales targeting
 * record + 8 (1.0 nominal, 0.75..1.25 per slot). */
static const int32_t dv_form_scale[5][8] = {
    {0x11000, 0x10000, 0x10000, 0x15000, 0xF000, 0x10000, 0x10000, 0xD000},
    {0xF000, 0x10000, 0x10000, 0x10000, 0x11000, 0x10000, 0x10000, 0x10000},
    {0x11000, 0x10000, 0x10000, 0x10000, 0xF000, 0x10000, 0x10000, 0x10000},
    {0xE000, 0x10000, 0x10000, 0xC000, 0x12000, 0x10000, 0x10000, 0x14000},
    {0xE000, 0x10000, 0x10000, 0x13000, 0x12000, 0x10000, 0x10000, 0xD000},
};

/* sub_080914C4(self, slot): the per-record orbit/follow update.  With
 * no orbit angles the record chases its formation offset; otherwise
 * the angle lerps toward the target by 1/+ 0xD, spins 0x4000 and the
 * position is anchor + sine/cosine * radius. */
static void dv_slot_update(AosEnemy *e, int slot) {
    int form = e->dv_form - 1;
    if (form < 0) form = 0;
    if (form > 4) form = 4;
    if (e->dv.oat[slot] == 0 && e->dv.oac[slot] == 0) {
        /* Follow mode: byte-pair pixel offset at (form + slot) parity,
         * divided by the + 0xD swarm timer like the native __divsi3. */
        int idx = (form + slot) & 7;
        int32_t tx = (int32_t)dv_follow_base[idx] << 16;
        int32_t ty = (int32_t)dv_follow_base[(idx + 1) & 7] << 16;
        int32_t div = e->timer ? e->timer : 1;
        e->dv.ox[slot] += (tx - e->dv.ox[slot]) / div;
        e->dv.oy[slot] += (ty - e->dv.oy[slot]) / div;
        e->dv.osc[slot] += (dv_form_scale[form][slot] - e->dv.osc[slot]) / div;
        return;
    }
    int32_t div = e->timer ? e->timer : 1;
    e->dv.oac[slot] += (e->dv.oat[slot] - e->dv.oac[slot]) / div;
    int32_t ang = e->dv.oac[slot] + 0x4000;
    int32_t sx = (int32_t)((int64_t)sine((uint32_t)ang) );
    int32_t cx = (int32_t)((int64_t)sine((uint32_t)(ang + 0x4000)) );
    int32_t rad = e->dv.orad[slot];
    if (rad < 0) rad = -rad;
    int32_t ox = (int32_t)((int64_t)sx * rad >> 16);
    int32_t oy = (int32_t)((int64_t)cx * rad >> 16);
    if (e->dv.oan[slot] == (int32_t)0x80000000) {
        int ai = e->dv.oai[slot] & 7;
        e->dv.ox[slot] = e->dv.ox[ai] + ox;
        e->dv.oy[slot] = e->dv.oy[ai] + oy;
    } else {
        e->dv.ox[slot] = e->dv.oan[slot] + ox;
        e->dv.oy[slot] = e->dv.oac[slot] + oy;
    }
    e->dv.osc[slot] += (dv_form_scale[form][slot] - e->dv.osc[slot]) / div;
}

/* sub_08091ED8: refresh all eight records while + 0xD stands. */
static void dv_swarm_update(AosEnemy *e) {
    if (!e->timer) return;
    for (int i = 0; i < 8; ++i) dv_slot_update(e, i);
}

/* The native retarget shared by the state machines: player position
 * (gEwramData + 0xA094 -> +4/+8, room pixels) plus the facing offset
 * (0xFFD80000 or 0x280000) and a +/- 0x7FFFF random jitter on y. */
static void dv_retarget(AosEnemy *e, AosSoma *soma, int mirrored,
                        uint32_t (*random)(void)) {
    int32_t sx = target_x(soma), sy = target_y(soma);
    e->dv_tx = sx;
    e->dv_ty = sy + (mirrored ? (int32_t)0xFFD80000 : 0x280000) +
               (int32_t)(random() & 0x7FFFF) - 0x3FFFF;
    e->dv_dist = 0;
}

/* Children                                                             */
/* ------------------------------------------------------------------ */

/* The per-variant child frame table: 0x0811900C for Devil/Flame Demon,
 * 0x08119014 for Arc Demon/Demon Lord — eight sprite frames, one per
 * imp slot. */
static const uint8_t dv_child_frames_a[8] = {1, 0x23, 2, 4, 1, 0x23, 2, 4};
static const uint8_t dv_child_frames_b[8] = {1, 0x23, 2, 3, 1, 0x23, 2, 3};
/* 0x08119024 enable order -> child slot index; 0x08119034 dive
 * parameter bytes (39,36,33,30). */
static const uint8_t dv_slot_order[8] = {4, 5, 6, 7, 0, 1, 2, 3};
static const uint8_t dv_dive_param[4] = {0x27, 0x24, 0x21, 0x1E};

static bool dv_is_devil(uint8_t id) {
    return id == AOS_ENEMY_DEVIL || id == AOS_ENEMY_ARC_DEMON ||
           id == AOS_ENEMY_DEMON_LORD || id == AOS_ENEMY_FLAME_DEMON;
}

/* Emit the eight imps on the parent's first update
 * (boss_parts_spawned): sub_0806DFF8 + sub_080917B4 copies the parent
 * record, links parent, picks the variant frame table and the slot
 * parameters. */
static void dv_spawn_imps(AosHitReport *report, AosEnemy *e,
                          const AosAnimSet *anims) {
    if (report->child_count + 8 > AOS_ENEMY_MAX_CHILDREN) return;
    const uint8_t *frames =
        (e->id == AOS_ENEMY_DEVIL || e->id == AOS_ENEMY_FLAME_DEMON)
            ? dv_child_frames_a : dv_child_frames_b;
    for (int i = 0; i < 8; ++i) {
        AosEnemy *c = &report->children[report->child_count++];
        *c = *e;
        c->role = AOS_ROLE_DEVIL_IMP;
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
        c->dv_slot = dv_slot_order[i];
        c->dv_dvx = c->dv_dvy = c->dv_ang = 0;
        c->dv_life = 0;
        c->dv_kind = e->id;
        c->contact_elements = e->contact_elements;
        c->combat.attack_off = true;
        c->combat.hurt_off = true;
        /* native + 0x65 = the descriptor byte for this slot, + 0x20 the
         * slot index, + 0x21 the 0x0811901C parameter. */
        c->static_frame = frames[i];
        c->dv_form = (uint8_t)i;
        play_loop(c, anims, c->static_frame, false);
        step_anim(c, anims);
    }
    e->boss_parts_spawned = true;
}

/* sub_080917B4: the imp update. State 0 rides its orbit record at the
 * parent's position; the parent's attack steps flip the imp into the
 * dive (state 1 launch, state 2 flight) and back. */
static AosHitReport dv_imp_update(AosEnemy *c, AosSoma *soma,
                                  const AosEnemyKind *kind,
                                  const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames,
                                  int soma_atk, int soma_def,
                                  uint32_t (*random)(void)) {
    (void)random;
    AosHitReport report = {0 };
    AosEnemy *p = c->parent;
    if (!p || p->removed || p->state == 2) {
        /* _08091B28: the container record is gone — the imp dies. */
        c->removed = true;
        return report;
    }
    int slot = (int)(c->dv_slot & 7);
    if (c->state == 0) {
        /* Attached: position = parent pos + record offset, scale from
         * the record's + 8, flags mirror the container. */
        c->x = p->x + p->dv.ox[slot];
        c->y = p->y + p->dv.oy[slot];
        /* The parent dive signal: state 1 attack step >= 2 releases
         * the imps (native flips child + 0xA through its own table). */
        if (p->state == 1 && p->step >= 2 && c->step == 0) {
            c->state = 1;
            c->step = 0;
            c->timer = 0;
            /* Launch: angle to the player - 0x4000, dive param from the
             * 0x08119034 byte pair for this slot parity. */
            int32_t dx = (int16_t)((target_x(soma) - c->x) >> 16);
            int32_t dy = (int16_t)((target_y(soma) - c->y) >> 16);
            c->dv_ang = (int32_t)arctan2((int16_t)dx, (int16_t)dy) - 0x4000;
            c->dv_dvx = (int32_t)((int64_t)sine((uint32_t)(c->dv_ang + 0x4000)));
            c->dv_dvy = (int32_t)((int64_t)sine((uint32_t)c->dv_ang));
            int32_t spd = 0x18000 + (dv_dive_param[slot & 3] << 12);
            c->vx = (int32_t)((int64_t)c->dv_dvx * spd >> 16);
            c->vy = (int32_t)((int64_t)c->dv_dvy * spd >> 16);
            c->timer = 0x28;
            c->combat.attack_off = false;
            c->combat.hurt_off = false;
        }
    } else if (c->state == 1) {
        /* Launch frame: hand the velocity to the flight state. */
        c->state = 2;
        c->step = 0;
    } else {
        /* Flight: integrate, contact on the shared collide, return to
         * the parent orbit when the timer ends or it leaves margins. */
        c->x += c->vx;
        c->y += c->vy;
        if (--c->timer <= 0 ||
            outside_margins(kind, 4, dv_px(c) - 0, dv_py(c) - 0)) {
            c->state = 0;
            c->step = 0;
            c->timer = 0;
            c->vx = c->vy = 0;
            c->combat.attack_off = true;
            c->combat.hurt_off = true;
        }
    }
    step_anim(c, kind->anims);
    if (c->state == 2) {
        AosHitReport hit = collide(c, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(&report, &hit);
    }
    return report;
}

/* sub_08091C50: the aimed fireball — entity 0x49 in range, homing
 * update sub_08091D54, contact sub_08091FE8, inherits the parent's
 * variant byte and tiles. */
static void dv_spawn_fire(AosHitReport *report, AosEnemy *e,
                          const AosAnimSet *anims, int32_t ox, int32_t oy,
                          int32_t vx, int32_t vy, uint8_t life) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy *f = &report->children[report->child_count++];
    *f = *e;
    f->role = AOS_ROLE_DEVIL_FIRE;
    f->parent = e;
    f->state = 0;
    f->step = f->substep = 0;
    f->timer = life;
    f->x = e->x + ox;
    f->y = e->y + oy;
    f->vx = vx;
    f->vy = vy;
    f->ax = f->ay = 0;
    f->hp = 1;
    f->hit_flash = 0;
    f->defeated = false;
    f->hidden = false;
    f->cycle_done = false;
    f->removed = false;
    f->param0 = f->param1 = 0;
    f->spawned_count = 0;
    f->boss_parts_spawned = true;
    f->static_frame = -1;
    f->dv_kind = e->id;
    f->dv_life = life;
    f->contact_elements = e->contact_elements;
    f->combat.attack_off = false;
    f->combat.hurt_off = false;
    play_loop(f, anims, 1, false);
    step_anim(f, anims);
}

/* sub_08091D54: the fireball update — + 0xD lifetime, position re-anchored
 * to the parent plus its own offset each frame (the offset integrates
 * through vx/vy here), song 0x12A at expiry, contact while alive. */
static AosHitReport dv_fire_update(AosEnemy *f, AosSoma *soma,
                                   const AosEnemyKind *kind,
                                   const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames,
                                   int soma_atk, int soma_def) {
    AosHitReport report = {0 };
    AosEnemy *p = f->parent;
    if (f->state == 0) {
        f->state = 1;
        f->ax = 0x40000;
    } else {
        f->dv_life--;
        if (p && !p->removed) {
            f->x = p->x + f->vx * f->dv_life / 8;
            f->y = p->y + f->vy * f->dv_life / 8;
        } else {
            f->x += f->vx;
            f->y += f->vy;
        }
        if (f->dv_life <= 0) {
            aos_audio_play_song(0x12A);
            spawn_drift(0, f->x, f->y, &report);
            spawn_drift(0, f->x - 0x40000, f->y, &report);
            f->removed = true;
            return report;
        }
    }
    step_anim(f, kind->anims);
    AosHitReport hit = collide(f, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    copy_result(&report, &hit);
    return report;
}

/* sub_08091E20: the straight orb — same spawn shape, update
 * sub_08092018 (position = parent + integrated offset, + 0xD life). */
static void dv_spawn_orb(AosHitReport *report, AosEnemy *e,
                         const AosAnimSet *anims, int32_t ox, int32_t oy,
                         int32_t vx, int32_t vy, uint8_t life) {
    dv_spawn_fire(report, e, anims, ox, oy, vx, vy, life);
    if (report->child_count)
        report->children[report->child_count - 1].role = AOS_ROLE_DEVIL_ORB;
}

static AosHitReport dv_orb_update(AosEnemy *f, AosSoma *soma,
                                  const AosEnemyKind *kind,
                                  const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames,
                                  int soma_atk, int soma_def) {
    AosHitReport report = {0 };
    AosEnemy *p = f->parent;
    f->dv_life--;
    if (p && !p->removed) {
        f->x = p->x + f->vx;
        f->y = p->y + f->vy;
        f->vx += f->ax;
        f->vy += f->ay;
    } else {
        f->x += f->vx;
        f->y += f->vy;
    }
    if (f->dv_life <= 0) {
        f->removed = true;
        return report;
    }
    step_anim(f, kind->anims);
    AosHitReport hit = collide(f, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    copy_result(&report, &hit);
    return report;
}

/* ------------------------------------------------------------------ */
/* Parent update                                                        */
/* ------------------------------------------------------------------ */

/* The aimed volley of state-1 step 11: ArcTan2 to the player, sine and
 * cosine scaled by 0x20000, sub_08091C50 with the facing-adjusted
 * angle and song 0x78. */
static void dv_volley(AosHitReport *report, AosEnemy *e, AosSoma *soma,
                      const AosAnimSet *anims, uint32_t (*random)(void)) {
    int32_t dx = (int16_t)((target_x(soma) - e->x) >> 16);
    int32_t dy = (int16_t)((target_y(soma) - e->y) >> 16);
    int32_t ang = (int32_t)arctan2((int16_t)dx, (int16_t)dy);
    int32_t sx = (int32_t)((int64_t)sine((uint32_t)(ang + 0x4000)) * 0x20000 >> 16);
    int32_t sy = (int32_t)((int64_t)sine((uint32_t)ang) * 0x20000 >> 16);
    if (e->mirrored) sx = -sx;
    dv_spawn_fire(report, e, anims, 0, 0, sx, sy, e->timer);
    aos_audio_play_song(0x78);
    (void)random;
}

/* sub_0808FE10 state 0: the idle/approach machine. Steps 0-3 aggro and
 * re-form the swarm, step 4 walks toward the target, steps 10-15 are
 * the formation/return variants; a close player enters state 1. */
static void dv_state0(AosHitReport *report, AosEnemy *e, AosSoma *soma,
                      const AosCollision *layer,
                      const AosEnemyKind *kind,
                      const AosAnimSet *anims,
                      uint32_t (*random)(void)) {
    int32_t px = dv_px(e) - 0x50, py = dv_py(e) - 0x50;
    switch (e->step) {
    case 0xA:                       /* create entry: wait one frame */
        e->step = 0;
        e->substep = 0;
        break;
    case 0: {
        face_player(e, soma);
        /* aggro box 0xA0 x 0xA0 off the + 0x42/+ 0x46 anchor, or the
         * sub_0806D54C proximity probe */
        if (player_near(soma, (int16_t)e->dv_ax16 - 0x50,
                        (int16_t)e->dv_ay16 - 0x50, 0xA0, 0xA0) ||
            player_in_range(e, soma, 0xA00000)) {
            /* _0808FEA4: ++step, sub-cycle 0, +0x10/+0x14 target,
             * animation +0x6D+1 looping (the wing-beat entry), +0xD =
             * 0x16, formation 1 and the five-slot re-form. */
            e->step = 1;
            e->substep = 0;
            e->timer = 0x16;
            e->dv_form = 1;
            e->dv_dist = 0;
            dv_retarget(e, soma, e->mirrored, random);
            play_loop(e, anims, (uint8_t)((e->anim.id + 1) % anims->count), true);
            dv_reform(e, -0xA00, -0x800, 0x800, 0xA000, 0xD0000, 0, 0, 0);
        } else {
            /* idle: the swarm drifts on its records */
            dv_swarm_update(e);
        }
        break;
    }
    case 1: case 2: case 3: {
        /* _0808FFD0/_080901B0/_080901F0: the approach legs — +0x18
         * ramps +0x10000 to 0x1FFFF, sub_080696B0 flies toward
         * +0x10/+0x14 and a close or arrived target enters state 1;
         * the +0xC sub-cycle re-forms every 0x16 ticks. */
        if (e->dv_dist < 0x1FFFF) {
            e->dv_dist += 0x10000;
            if (e->dv_dist > 0x1FFFF) e->dv_dist = 0x1FFFF;
        }
        bool arrived = head_toward(e, e->dv_tx, e->dv_ty, e->dv_dist);
        int32_t ddx = target_x(soma) - e->x;
        int32_t ddy = target_y(soma) - e->y;
        if (arrived ||
            ((ddx < 0 ? -ddx : ddx) < 0x2BFFFF &&
             (ddy < 0 ? -ddy : ddy) < 0xFFFFF)) {
            e->state = 1;
            e->step = 0;
            e->substep = 0;
            e->timer = 1;
            e->dv_form = 1;
            dv_save_pos(e);
            dv_reform(e, -0xA00, -0x800, 0x800, 0xA000, 0xD0000, 0, 0, 0);
            break;
        }
        face_player(e, soma);
        dv_swarm_update(e);
        if (!e->timer) e->timer = 0x16;
        if (--e->timer == 0) {
            e->timer = 0x16;
            e->substep = (uint8_t)((e->substep + 1) & 3);
            e->dv_form = (uint8_t)(1 + e->substep);
            if (++e->step > 3) e->step = 4;
            dv_reform(e, -0xA00, -0x800, 0x800, 0xA000, 0xD0000, 0, 0, 0);
        }
        break;
    }
    case 4: {
        /* _08090370: coast on the last velocity, 0x1D ticks, then
         * step 1 with a fresh target. */
        dv_move(e, soma, layer);
        dv_swarm_update(e);
        if (++e->walk_timer > 0x1D) {
            e->step = 1;
            e->walk_timer = 0;
            e->dv_dist = 0;
            dv_retarget(e, soma, e->mirrored, random);
        }
        break;
    }
    case 5: case 6: case 7: case 8: case 9:
    case 11: case 12: case 13: case 14: case 15: {
        /* approach/attack-entry variants: same aggro + sub-cycle */
        face_player(e, soma);
        dv_swarm_update(e);
        if (e->timer) {
            if (--e->timer == 0) {
                e->timer = 0x16;
                e->substep = (uint8_t)(e->substep + 1);
                if (e->substep > 3) e->substep = 0;
                e->dv_form = (uint8_t)(1 + e->substep);
                dv_reform(e, -0xA00, -0x800, 0x800, 0xA000, 0xD0000, 0, 0, 0);
            }
        } else {
            e->timer = 0x16;
        }
        /* close-range check enters the attack */
        if (player_near(soma, dv_px(e) - 0x2C, dv_py(e) - 0x10, 0x59, 0x20) ||
            player_near(soma, px, py, 0xA0, 0xA0)) {
            e->state = 1;
            e->step = 0;
            e->substep = 0;
            e->timer = 1;
            e->dv_form = 1;
            dv_save_pos(e);
            dv_reform(e, -0xA00, -0x800, 0x800, 0xA000, 0xD0000, 0, 0, 0);
        }
        break;
    }
    default:
        break;
    }
    (void)report;
    (void)kind;
}

/* sub_08090914 state 1: the attack machine — timed steps re-forming
 * the swarm, song cues and the aimed volley, ending back in state 0. */
static void dv_state1(AosHitReport *report, AosEnemy *e, AosSoma *soma,
                      const AosCollision *layer,
                      const AosEnemyKind *kind,
                      const AosAnimSet *anims,
                      uint32_t (*random)(void)) {
    (void)kind;
    dv_swarm_update(e);
    /* watchdog: a step entered with a drained timer re-arms the
     * standard 0x16 leg instead of stalling the machine. */
    if (!e->timer) e->timer = 0x16;
    switch (e->step) {
    case 0:
        /* variant-gated entry: non-Devil variants may skip to step 0xA
         * on a 0x40/0x7F roll; else animation + swarm re-form. */
        if (e->id != AOS_ENEMY_DEVIL && (random() & 0x7F) > 0x40) {
            e->dv_form = 4;
            e->timer = 0xE;
            play_loop(e, anims, (uint8_t)((e->anim.id + 1) % anims->count), false);
            e->step = 0xA;
            dv_reform(e, -0xA00, -0x800, 0x800, 0xA000, 0xD0000, 0, 0, 0);
            dv_move(e, soma, layer);
            break;
        }
        e->step = 1;
        e->timer = 0x12;
        break;
    case 1: case 3: case 5: case 6: case 9:
        /* timed steps: move + swarm, advance on timer */
        dv_move(e, soma, layer);
        if (!e->timer) e->timer = 0x16;
        if (--e->timer == 0) {
            e->step = (uint8_t)(e->step + 1);
            e->timer = (e->step == 2) ? 8 : 0x16;
            if (e->step == 2) {
                e->dv_form = 3;
                aos_audio_play_song(0x86);
            }
            if (e->step == 4) e->dv_form = 2;
            dv_reform(e, -0xA00, -0x800, 0x800, 0xA000, 0xD0000, 0, 0, 0);
        }
        break;
    case 2:
        dv_move(e, soma, layer);
        if (e->timer && --e->timer == 0) {
            e->step = 3;
            e->timer = 8;
            e->dv_form = 3;
            aos_audio_play_song(0x86);
            dv_reform(e, -0xA00, -0x800, 0x800, 0xA000, 0xD0000, 0, 0, 0);
        }
        break;
    case 4:
        dv_move(e, soma, layer);
        if (e->timer && --e->timer == 0) {
            e->step = 5;
            e->timer = 0x16;
            e->dv_form = 2;
            /* native plays the + 0x6D + 1 transition looping (r3 = 1)
             * — the wing-beat keeps animating through the attack. */
            play_loop(e, anims, (uint8_t)((e->anim.id + 1) % anims->count), true);
            dv_reform(e, -0xA00, -0x800, 0x800, 0xA000, 0xD0000, 0, 0, 0);
        }
        break;
    case 7: case 8:
        /* reserved native steps (shared epilogue) */
        dv_move(e, soma, layer);
        if (!e->timer) e->timer = 0x16;
        if (--e->timer == 0) {
            e->step = 9;
            e->timer = 0x16;
        }
        break;
    case 0xA:
        /* the dive release: imps launch (their state 0 -> 1) */
        dv_move(e, soma, layer);
        if (e->timer && --e->timer == 0) {
            e->step = 0xB;
            e->timer = 0x28;
            dv_reform(e, -0xA00, -0x800, 0x800, 0xA000, 0xD0000, 0, 0, 0);
        }
        break;
    case 0xB:
        /* variant fireball at + 0xD == 0x28 for Arc Demon/Demon Lord */
        dv_move(e, soma, layer);
        if (e->timer == 0x28 &&
            (e->id == AOS_ENEMY_ARC_DEMON || e->id == AOS_ENEMY_DEMON_LORD)) {
            dv_spawn_orb(report, e, anims, 0, 0, 0, -0x20000, 0x20);
        }
        if (e->timer && --e->timer == 0) {
            e->step = 0xC;
            e->timer = 0x30;
        }
        break;
    case 0xC:
        /* the aimed volley at + 0xD == 0x1E */
        dv_move(e, soma, layer);
        if (e->timer == 0x1E) dv_volley(report, e, soma, anims, random);
        if (e->timer && --e->timer == 0) {
            e->step = 0xD;
            e->timer = 0x16;
            e->dv_form = 2;
            dv_reform(e, -0xA00, -0x800, 0x800, 0xA000, 0xD0000, 0, 0, 0);
        }
        break;
    case 0xD: case 0xE: case 0xF:
        /* return legs: move + swarm, then back to state 0 */
        dv_move(e, soma, layer);
        if (e->timer && --e->timer == 0) {
            if (e->step == 0xF) {
                e->state = 0;
                e->step = 1;
                e->substep = 0;
                e->timer = 0x16;
                e->dv_form = 1;
                e->dv_dist = 0;
                dv_save_pos(e);
                dv_retarget(e, soma, e->mirrored, random);
                dv_reform(e, -0xA00, -0x800, 0x800, 0xA000, 0xD0000, 0, 0, 0);
            } else {
                e->step = (uint8_t)(e->step + 1);
                e->timer = 0x16;
            }
        }
        break;
    default:
        e->state = 0;
        e->step = 1;
        break;
    }
}

/* sub_08091204 state 3: the dormant formation cycle — animation only
 * plus the swarm re-form every 0x16 ticks. */
static void dv_state3(AosHitReport *report, AosEnemy *e, AosSoma *soma,
                      const AosEnemyKind *kind,
                      const AosAnimSet *anims) {
    (void)report;
    (void)soma;
    (void)kind;
    switch (e->step) {
    case 0:
        e->step = 1;
        play_loop(e, anims, (uint8_t)((e->anim.id + 1) % anims->count), true);
        e->timer = 0x16;
        e->dv_form = 1;
        dv_save_pos(e);
        dv_reform(e, -0xA00, -0x800, 0x800, 0xA000, 0xD0000, 0, 0, 0);
        break;
    default:
        if (e->timer && --e->timer == 0) {
            e->timer = 0x16;
            e->step = (uint8_t)(e->step + 1);
            if (e->step > 4) e->step = 0;
            e->dv_form = (uint8_t)(1 + (e->step & 3));
            dv_reform(e, -0xA00, -0x800, 0x800, 0xA000, 0xD0000, 0, 0, 0);
        }
        break;
    }
}

/* ------------------------------------------------------------------ */

AosHitReport aos_enemy_demon_update(AosEnemy *e, AosSoma *soma,
                                    const AosCollision *layer,
                                    const AosEnemyKind *kind,
                                    const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames,
                                    int soma_atk, int soma_def, int cam_x,
                                    int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0 };
    const AosAnimSet *anims = kind->anims;
    /* Child roles dispatch first. */
    if (e->role == AOS_ROLE_DEVIL_IMP)
        return dv_imp_update(e, soma, kind, weapon, weapon_frames,
                             soma_atk, soma_def, random);
    if (e->role == AOS_ROLE_DEVIL_FIRE)
        return dv_fire_update(e, soma, kind, weapon, weapon_frames,
                              soma_atk, soma_def);
    if (e->role == AOS_ROLE_DEVIL_ORB)
        return dv_orb_update(e, soma, kind, weapon, weapon_frames,
                             soma_atk, soma_def);
    if (e->role != AOS_ROLE_ENEMY || !dv_is_devil(e->id))
        return report;

    int sx = dv_px(e) - cam_x, sy = dv_py(e) - cam_y;
    if (e->state != 2) {
        e->hidden = outside_margins(kind, 4, sx, sy);
        if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 &&
            (uint16_t)sy <= 0xB0)
            report.targetable = true;
    } else {
        e->hidden = outside_margins(kind, 4, sx, sy);
    }
    if (e->hit_flash) {
        /* sub_0806AD24 gate: a flashing live body runs contact alone. */
        if (e->state == 2)
            return report;
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(&report, &hit);
        return report;
    }
    /* The first update emits the eight imps (the + 0x1C container). */
    if (!e->boss_parts_spawned)
        dv_spawn_imps(&report, e, anims);
    switch (e->state) {
    case 0:
        dv_state0(&report, e, soma, layer, kind, anims, random);
        break;
    case 1:
        dv_state1(&report, e, soma, layer, kind, anims, random);
        break;
    case 2:
        generic_death(e, kind, random, &report);
        break;
    case 3:
        dv_state3(&report, e, soma, kind, anims);
        break;
    default:
        break;
    }
    step_anim(e, anims);
    if (e->state != 2) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(&report, &hit);
    }
    return report;
}
