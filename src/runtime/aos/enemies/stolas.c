/* Stolas (EnemyStolasCreate 0x0809A6C0 / EnemyStolasUpdate 0x0809A4AC,
 * id 0x61): the giant skeleton whose six bone children orbit it through
 * the + 0x1C container's six 0x30-byte records and launch on the
 * parent's attack mask.
 *
 * Native layout folded into AosEnemy:
 * - the container entity and its records become `sto_rec[6]` plus the
 *   + 0x68/+ 0xF8 extras (`sto_x68`/`sto_xf8`);
 * - the six limb entities of sub_0806DFF8 (callback sub_0809B9D4)
 *   become persistent children of roles AOS_ROLE_STOLAS_BONE0..5,
 *   emitted over the first updates under the 8-child cap;
 * - the state-1 projectiles — the shot of sub_0809BD5C (update
 *   sub_0809BE78), the five fan bolts of sub_0809BF7C (sub_0809C054),
 *   the wave of sub_0809C0C8 (sub_0809C1A0) and the death gore of
 *   sub_0809C224 (sub_0809C488) — become children of roles
 *   AOS_ROLE_STOLAS_SHOT/FAN/WAVE/GORE;
 * - the hurt boxes sub_0809C3D8/sub_0809C41C (children) and
 *   sub_0809C534/sub_0809C564 (body) fold into the shared collide.
 *
 * Documented approximations: the two 7/14-case state machines keep
 * their case order, record configurations (the sub_0809B7F0 speed
 * table), launch divider and timers, but the per-case wall probes,
 * the sub_0806D288/sub_0806D3C0 movement waits and the palette merge
 * tail collapse into the shared helpers; the variant record tables
 * 0x08528148/0x085281A8 are reproduced for variant 0 (the only kind
 * sub_08068264(0x0820AC28, 0) produces for id 0x61). */

#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../core/aos_collision.h"
#include "../core/aos_soma.h"
#include "../math/aos_math.h"
#include <string.h>

#define copy_result aos_enemy_copy_collision_result
#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define target_x(...) aos_enemy_target_x(__VA_ARGS__)
#define target_y(...) aos_enemy_target_y(__VA_ARGS__)
#define player_near aos_enemy_player_near
#define outside_margins aos_enemy_outside_margins
#define step_anim aos_enemy_step_anim
#define play_loop aos_enemy_play_loop
#define integrate aos_enemy_integrate

static bool sto_is_stolas(uint8_t id) { return id == 0x61; }

/* sub_080009E4: the native sine table, 16.16 results (alastor.c twin). */
static int32_t sto_sin(uint32_t angle) {
    return aos_sine(angle & 0xFFFF);
}
static int32_t sto_cos(uint32_t angle) {
    return aos_sine((angle + 0x4000) & 0xFFFF);
}
/* sub_0803E86C: the 16.16 fixed multiply. */
static int32_t sto_mul(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * b) >> 16);
}
/* __divsi3 on the + 0xD divider, saturating like the native path. */
static int32_t sto_div(int32_t v, int d) {
    return d ? v / d : 0;
}
static int32_t sto_px(int32_t v16) {
    return v16 >= 0 ? (int32_t)(v16 >> 16) : -(int32_t)((-v16) >> 16);
}

/* Home orbit offsets of the six records, bytes 0..11 of the
 * variant-0 table 0x0852813C (s8 pairs, << 16). */
static const int8_t sto_home[6][2] = {
    {2, -32}, {2, -24}, {2, -14}, {8, -31}, {8, -23}, {8, -13},
};
/* The six child slot indices (+ 0x20) and aim targets (+ 0x21) of the
 * create loop: tables 0x081190AC and 0x081190A6. */
static const uint8_t sto_tgt_tab[6] = {1, 2, 2, 4, 5, 5};
/* The + 0x44 frame bytes of the children, table 0x081190A0. */
static const uint8_t sto_frame_tab[6] = {43, 44, 45, 40, 41, 42};

/* sub_0809B7F0/sub_0809B6B0: stage a record launch — flag 0x80000000,
 * anchor slot, orbit radius (Sqrt of the slot delta when no explicit
 * distance is given) and the angle toward the anchor plus the speed.
 * The native speed cap/facing fold collapses to the literal value
 * (documented divergence). */
static void sto_config(AosEnemy *e, int idx, int sub, int32_t speed,
                       int32_t dist) {
    if (idx < 0 || idx > 5 || sub < 0 || sub > 5) return;
    AosStoRec *r = &e->sto_rec[idx];
    r->flag = 0x80000000u;
    r->sub = sub;
    if (!dist) {
        int32_t dx = sto_px(e->sto_rec[sub].x - r->x);
        int32_t dy = sto_px(e->sto_rec[sub].y - r->y);
        int64_t d2 = (int64_t)dx * dx + (int64_t)dy * dy;
        dist = (int32_t)(d2 > 0 ? (d2 >> 8) : 0);
        if (dist < 0) dist = 0;
    }
    r->dist = dist;
    int32_t dx = sto_px(e->sto_rec[sub].x - r->x);
    int32_t dy = sto_px(e->sto_rec[sub].y - r->y);
    uint32_t ang = (uint32_t)aos_arctan2(dy, dx) + 0x10000;
    r->ang = (int32_t)(ang & 0xFFFF);
    r->ang2 = r->ang;
    int32_t s = speed < 0 ? -speed : speed;
    r->spd = s > 0xFFFF ? (e->mirrored ? 0x28000 - s : 0x28000 + s) : s;
    if (speed < 0) r->ang2 = (r->ang2 + 0x8000) & 0xFFFF;
}

/* sub_0809B478: one record's per-frame step — unlaunched records lerp
 * toward their home offset (slots 2/5 toward the + 0x68/+ 0xF8 angle
 * extras), launched records ease their angle toward the speed and
 * orbit their anchor slot. */
static void sto_step_record(AosEnemy *e, int slot) {
    AosStoRec *r = &e->sto_rec[slot];
    int d = e->sto_d ? e->sto_d : 1;
    if (!r->spd && !r->ang2) {
        int32_t hx, hy;
        if (slot == 2) {
            hx = e->sto_x68;
            hy = r->y;
        } else if (slot == 5) {
            hx = e->mirrored ? 0x28000 - e->sto_xf8 : 0x28000 + e->sto_xf8;
            hx -= e->sto_xf8;
            hy = r->y;
        } else {
            hx = (int32_t)sto_home[slot][0] << 16;
            hy = (int32_t)sto_home[slot][1] << 16;
            if (e->mirrored) hx = -hx;
        }
        r->x += sto_div(hx - r->x, d);
        r->y += sto_div(hy - r->y, d);
        return;
    }
    r->ang2 += sto_div(r->spd - r->ang2, d);
    uint32_t a = (uint32_t)r->ang2 + 0x4000;
    int32_t dx = sto_mul(sto_cos(a), r->dist);
    int32_t dy = sto_mul(sto_sin(a), r->dist);
    if (r->sub >= 0 && r->sub < 6) {
        AosStoRec *anc = &e->sto_rec[r->sub];
        int32_t ax = anc->x, ay = anc->y;
        if (r->flag == 0x80000000u) {
            ax &= (int32_t)0xFFFF0000;
            ay &= (int32_t)0xFFFF0000;
        }
        r->x = ax + dx;
        r->y = ay + dy;
    } else {
        r->x = dx;
        r->y = dy;
    }
}

/* sub_0809C324: launch every record whose divider is armed. */
static void sto_launch_all(AosEnemy *e) {
    if (!e->sto_d) return;
    for (int i = 0; i < 6; ++i)
        sto_step_record(e, i);
}

/* The create's record priming (sub_0809B7F0 x4 + sub_0809C324, then
 * sub_0809B6B0 x4 + sub_0809C324): slots {1,2,4,5} anchor to
 * {0,1,3,4} with the decoded speed/distance pairs.  The second
 * sub_0809B6B0 pass recomputes each slot's angle — approximated by a
 * second config pass with the same distances (documented divergence:
 * the native aim target is not fully decoded). */
static void sto_init_records(AosEnemy *e) {
    static const int idxs[4] = {1, 2, 4, 5};
    static const int subs[4] = {0, 1, 3, 4};
    static const int32_t speeds[4] = {0x12800, 0x13000, 0x14000, 0x15000};
    static const int32_t dists[4] = {0x80000, 0xD00000, 0x80000, 0xD00000};
    for (int k = 0; k < 4; ++k)
        sto_config(e, idxs[k], subs[k], speeds[k], dists[k]);
    sto_launch_all(e);
    for (int k = 0; k < 4; ++k)
        sto_config(e, idxs[k], subs[k], 0, dists[k]);
    sto_launch_all(e);
}

/* Emit the six bone children progressively (the native create spawns
 * them all through sub_0806DFF8; the room's 8-child cap spreads the
 * emission over the first updates). */
static void sto_spawn_bones(AosHitReport *hit, AosEnemy *e,
                            uint32_t (*random)(void)) {
    if (e->boss_parts_spawned) return;
    while (e->spawned_count < 6 && hit->child_count < AOS_ENEMY_MAX_CHILDREN) {
        int i = e->spawned_count++;
        AosEnemy c;
        memset(&c, 0, sizeof c);
        c.id = e->id;
        c.role = (uint8_t)(AOS_ROLE_STOLAS_BONE0 + i);
        c.parent = e;
        c.state = 0;
        c.step = 0;
        c.timer = 0;
        c.hp = 1;
        c.combat.hurt_off = true;
        c.sto_slot = (uint8_t)i;
        c.sto_tgt = sto_tgt_tab[i];
        c.sto_anim = sto_frame_tab[i];
        c.static_frame = (int16_t)sto_frame_tab[i];
        c.x = e->x;
        c.y = e->y;
        /* The create's sub_0803E654 random draw sets the child's
         * facing bits and scatters its first velocity. */
        c.mirrored = random != NULL && (random() & 1);
        c.vx = ((int32_t)((int)(random ? random() : 0) & 0xFF) - 0x80) * 0x40;
        c.vy = 0;
        hit->children[hit->child_count++] = c;
    }
    if (e->spawned_count >= 6)
        e->boss_parts_spawned = true;
}

/* sub_0809B9D4 state 0: the bone rides its record — position is the
 * parent's plus the record offset; slots {1,3,4} aim at their target
 * record, slots {2,5} hold the record's stored angle; the parent's
 * + 0x22 attack mask launches each slot. */
static void sto_bone_update(AosEnemy *c, const AosAnimSet *anims) {
    AosEnemy *p = c->parent;
    (void)anims;
    if (c->state == 0) {
        if (!p || p->removed || !sto_is_stolas(p->id) ||
            p->id != c->id) {
            c->removed = true;
            return;
        }
        c->mirrored = p->mirrored;
        int slot = c->sto_slot & 7;
        c->x = p->x + p->sto_rec[slot].x;
        c->y = p->y + p->sto_rec[slot].y;
        /* Aim: slots 1/3/4 track their target record through ArcTan2,
         * slots 2/5 hold the record's + 8 angle, slot 0 keeps its own. */
        if (slot == 1 || slot == 3 || slot == 4) {
            int32_t dx = sto_px(p->sto_rec[c->sto_tgt & 7].x -
                                p->sto_rec[slot].x);
            int32_t dy = sto_px(p->sto_rec[c->sto_tgt & 7].y -
                                p->sto_rec[slot].y);
            int32_t ang = aos_arctan2(dy, dx);
            if (p->mirrored) ang = (ang + 0x8000) & 0xFFFF;
            c->angle = (uint32_t)ang;
            p->sto_rec[slot].ang = ang;
        } else if (slot == 2 || slot == 5) {
            c->angle = (uint32_t)(p->sto_rec[slot].ang & 0xFFFF);
        }
        if (p->state == 2) {
            /* The native detach: thrown state 1, timer 0x3C. */
            c->state = 1;
            c->timer = 0x3C;
            c->vx = ((int32_t)((int)aos_random() & 0xFF) - 0x80) * 0x100;
            c->vy = 0;
            return;
        }
        /* Attack mask: the parent's + 0x22 bit per slot index. */
        if (p->sto_atk & (uint16_t)(1u << (slot & 15))) {
            if (slot == 0)
                c->hit_flash = 8;
            else if (slot == 2) {
                /* The thrown bone: velocity along the held angle. */
                uint32_t a = (uint32_t)c->angle + 0x4000;
                c->vx = sto_mul(sto_cos(a), 0x40000);
                c->vy = sto_mul(sto_sin(a), 0x40000);
            }
            c->attack_off = true;
        }
        /* sub_0803E058: move along + 0x48 at 0x40000 — folded to the
         * shared integrate; bones stay glued to their record anyway. */
        return;
    }
    /* state 1: ride the record once more while the timer runs out,
     * then delete (sub_0803E6BC + EntityDelete). */
    if (p && !p->removed && sto_is_stolas(p->id)) {
        int slot = c->sto_slot & 7;
        c->x = p->x + p->sto_rec[slot].x;
        c->y = p->y + p->sto_rec[slot].y;
        c->hidden = true;
    }
    if ((uint8_t)--c->timer == 0)
        c->removed = true;
}

/* The state-1 projectiles share the tiles/palette/frames of the
 * parent; each keeps its decoded anim-cycle timer. */
static void sto_projectile_update(AosEnemy *c) {
    AosEnemy *p = c->parent;
    bool gone = !p || p->removed || !sto_is_stolas(p->id);
    if (gone) {
        c->removed = true;
        return;
    }
    switch (c->role) {
    case AOS_ROLE_STOLAS_SHOT:
        /* sub_0809BE78: after 0xE ticks a +-0x800 drift on + 0x14,
         * life 0x3B, deleted with the parent. */
        c->x += c->vx >> 8;
        c->y += c->vy >> 8;
        if (c->timer > 0x0E)
            c->vx += p->mirrored ? -0x800 : 0x800;
        if ((uint8_t)--c->timer == 0 || c->timer > 0x3B)
            c->removed = true;
        break;
    case AOS_ROLE_STOLAS_FAN:
        /* sub_0809C054: anim 0x20->0x21->0x22 at 8/0x10/0x52, life 0x59. */
        c->x += c->vx >> 8;
        c->y += c->vy >> 8;
        if ((uint8_t)--c->timer == 0)
            c->removed = true;
        break;
    case AOS_ROLE_STOLAS_WAVE:
        /* sub_0809C1A0: anim 0x23->0x27 at 8/0x10/0x18/0x28, life 0x2F,
         * early delete while the parent is in state 2. */
        c->x += c->vx >> 8;
        c->y += c->vy >> 8;
        if ((uint8_t)--c->timer == 0 ||
            (p->state == 2 && c->timer < 0x20))
            c->removed = true;
        break;
    default:
        break;
    }
}

/* sub_0809C224/sub_0809C488: a gore chunk — gravity, delete at zero. */
static void sto_gore_update(AosEnemy *c) {
    c->vy += 0x2000;
    c->x += c->vx >> 8;
    c->y += c->vy >> 8;
    if ((uint8_t)--c->timer == 0)
        c->removed = true;
}

/* The state-0 attack cycle: the native 7-case machine of sub_0809AA48
 * configures records {1,2,4,5} with the decoded speed table, arms the
 * launch divider and waits through sub_0806D288. Collapsed to the
 * same case order with literal speeds. */
static const int32_t sto_state0_speed[5][4] = {
    {0x14800, 0x16800, 0x13000, 0x13800},
    {0x14000, 0x15000, 0x12800, 0x13000},
    {0x13000, 0x13800, 0x14800, 0x16800},
    {0x12800, 0x13000, 0x14000, 0x15000},
    {0x14800, 0x16800, 0x13000, 0x13800},
};
static const int8_t sto_state0_sub[4] = {0, 1, 3, 4};
static const uint8_t sto_state0_next[7] = {1, 2, 3, 4, 5, 6, 1};
static const uint8_t sto_state0_timer[7] = {0x24, 0x10, 0x0A, 0x10, 0x10, 0x10, 0x14};

static void sto_state0(AosEnemy *e, AosSoma *soma,
                       const AosAnimSet *anims) {
    int cs = e->step > 6 ? 0 : e->step;
    if (cs == 0) {
        /* Case 0: aggro — the player in the 0xA0 box or damaged. */
        face_player(e, soma);
        if (player_near(soma, e->x >> 16, e->y >> 16, 0xA0, 0xA0)) {
            e->step = 1;
            e->substep = 0;
            e->timer = 0x24;
            e->sto_d = 1;
            e->sto_b = 1;
            play_loop(e, anims, 1, true);
        }
        return;
    }
    /* Cases 1..6: configure the four attack records, launch them and
     * count the + 0xD divider down to the next case. */
    if (e->substep == 0) {
        int row = (cs - 1) & 3;
        static const int idxs[4] = {1, 2, 4, 5};
        for (int k = 0; k < 4; ++k)
            sto_config(e, idxs[k], sto_state0_sub[k],
                       sto_state0_speed[row][k], 0);
        e->substep = 1;
    }
    if (e->substep == 1) {
        sto_launch_all(e);
        if ((uint8_t)--e->timer == 0) {
            e->substep = 0;
            if (cs == 6) {
                /* The native tail rolls into the state-1 fan attack. */
                e->state = 1;
                e->step = 0;
                e->timer = 0;
                return;
            }
            int nxt = sto_state0_next[cs];
            e->step = (uint8_t)nxt;
            e->timer = sto_state0_timer[nxt];
        }
    }
}

/* The state-1 fan attack of sub_0809AF14: song 0x11F, the aimed shot
 * (sub_0809BD5C), five fan bolts (sub_0809BF7C at x offsets
 * 1/0x1F/0x3F/0x5F/0x7F) and the wave (sub_0809C0C8), then back to
 * state 0. The 14-case bone choreography collapses to the decoded
 * spawn sequence (documented divergence). */
static void sto_state1(AosHitReport *hit, AosEnemy *e, AosSoma *soma,
                       const AosAnimSet *anims) {
    (void)anims;
    if (e->step == 0) {
        /* Entry: the native BD5C + BF7C x5 + C0C8 spawn burst. */
        aos_audio_play_song(0x11F);
        int32_t px = e->x >> 16, py = e->y >> 16;
        int32_t dx = target_x(soma) - px, dy = target_y(soma) - py;
        uint32_t aim = (uint32_t)aos_arctan2(
            dy >= 0 ? (int32_t)(dy >> 16) : -(int32_t)((-dy) >> 16),
            dx >= 0 ? (int32_t)(dx >> 16) : -(int32_t)((-dx) >> 16));
        /* The aimed shot. */
        if (hit->child_count < AOS_ENEMY_MAX_CHILDREN) {
            AosEnemy c;
            memset(&c, 0, sizeof c);
            c.id = e->id;
            c.role = AOS_ROLE_STOLAS_SHOT;
            c.parent = e;
            c.state = 0;
            c.hp = 1;
            c.combat.hurt_off = true;
            c.x = e->x;
            c.y = e->y;
            c.timer = 0x3B;
            c.angle = aim;
            c.vx = sto_mul(sto_cos(aim + 0x4000), 0x40000);
            c.vy = sto_mul(sto_sin(aim + 0x4000), 0x40000);
            hit->children[hit->child_count++] = c;
        }
        /* The five fan bolts at the decoded x offsets. */
        static const int16_t fan_x[5] = {1, 0x1F, 0x3F, 0x5F, 0x7F};
        for (int i = 0; i < 5 && hit->child_count < AOS_ENEMY_MAX_CHILDREN;
             ++i) {
            AosEnemy c;
            memset(&c, 0, sizeof c);
            c.id = e->id;
            c.role = AOS_ROLE_STOLAS_FAN;
            c.parent = e;
            c.state = 0;
            c.hp = 1;
            c.combat.hurt_off = true;
            c.sto_slot = (uint8_t)i;
            c.x = e->x + ((int32_t)fan_x[i] << 16);
            c.y = e->y - (0x10 << 16);
            c.timer = 0x59;
            uint32_t a = (uint32_t)(0x8000 + i * 0x1000) & 0xFFFF;
            c.angle = a;
            c.vx = sto_mul(sto_cos(a + 0x4000), 0x30000);
            c.vy = sto_mul(sto_sin(a + 0x4000), 0x30000);
            hit->children[hit->child_count++] = c;
        }
        /* The wave. */
        if (hit->child_count < AOS_ENEMY_MAX_CHILDREN) {
            AosEnemy c;
            memset(&c, 0, sizeof c);
            c.id = e->id;
            c.role = AOS_ROLE_STOLAS_WAVE;
            c.parent = e;
            c.state = 0;
            c.hp = 1;
            c.combat.hurt_off = true;
            c.x = e->x;
            c.y = e->y;
            c.timer = 0x2F;
            c.angle = dx >= 0 ? 0 : 0x8000;
            c.vx = sto_mul(sto_cos(c.angle + 0x4000), 0x50000);
            c.vy = 0;
            hit->children[hit->child_count++] = c;
        }
        e->step = 1;
        e->timer = 0x20;
        face_player(e, soma);
        return;
    }
    /* Hold the attack while the native 14 cases run; the bones keep
     * orbiting through the record pass. */
    face_player(e, soma);
    if ((uint8_t)--e->timer == 0) {
        e->state = 0;
        e->step = 1;
        e->substep = 0;
        e->timer = 0x14;
    }
}

/* State 2, decoded inline in the update: debris every fourth tick,
 * random gore every eighth, container delete at timer end. */
static void sto_state2(AosHitReport *hit, AosEnemy *e) {
    if ((e->timer & 3) == 0 && hit->child_count < AOS_ENEMY_MAX_CHILDREN) {
        AosEnemy c;
        memset(&c, 0, sizeof c);
        c.id = e->id;
        c.role = AOS_ROLE_STOLAS_GORE;
        c.parent = e;
        c.state = 0;
        c.hp = 1;
        c.x = e->x - (0x26000);
        c.y = e->y + 0x100000;
        c.timer = 0x18;
        c.vx = (int32_t)((int)aos_random() & 0xFFFF) - 0x8000;
        c.vy = -(0x1D0000);
        hit->children[hit->child_count++] = c;
    }
    if ((e->timer & 7) == 0) {
        /* Two random gore chunks (sub_0809C224 x2). */
        for (int k = 0; k < 2 && hit->child_count < AOS_ENEMY_MAX_CHILDREN;
             ++k) {
            AosEnemy c;
            memset(&c, 0, sizeof c);
            c.id = e->id;
            c.role = AOS_ROLE_STOLAS_GORE;
            c.parent = e;
            c.state = 0;
            c.hp = 1;
            c.x = e->x + (int32_t)((int)aos_random() % 0x180000) - 0x30000;
            c.y = e->y - 0x1D0000;
            c.timer = 0x18;
            c.vx = (int32_t)((int)aos_random() % 0x180000) - 0x30000;
            c.vy = -0x1D0000;
            hit->children[hit->child_count++] = c;
        }
    }
    if ((uint8_t)--e->timer == 0)
        e->removed = true;
}

/* sub_0809C530 state 3: the dormant machine holds the pose while the
 * + 0x8E & 0x40 flag stays set. */
static void sto_dormant(AosEnemy *e, const AosAnimSet *anims) {
    play_loop(e, anims, 0, true);
    step_anim(e, anims);
    if (!aos_enemy_flag_8e_40) {
        e->state = 0;
        e->step = 0;
    }
}

AosHitReport aos_enemy_stolas_update(AosEnemy *e, AosSoma *soma,
                                     const AosCollision *layer,
                                     const AosEnemyKind *kind,
                                     const AosWeaponEntity *weapon,
                                     const AosWeaponFrames *weapon_frames,
                                     int soma_atk, int soma_def,
                                     int cam_x, int cam_y,
                                     uint32_t (*random)(void)) {
    const AosAnimSet *anims = kind->anims;
    AosHitReport hit;
    memset(&hit, 0, sizeof hit);
    (void)layer; (void)soma_atk; (void)soma_def; (void)cam_x; (void)cam_y;

    /* Bone children, projectiles and gore dispatch on their roles. */
    if (e->role >= AOS_ROLE_STOLAS_BONE0 && e->role <= AOS_ROLE_STOLAS_BONE5) {
        sto_bone_update(e, anims);
        return hit;
    }
    if (e->role == AOS_ROLE_STOLAS_SHOT || e->role == AOS_ROLE_STOLAS_FAN ||
        e->role == AOS_ROLE_STOLAS_WAVE) {
        sto_projectile_update(e);
        return hit;
    }
    if (e->role == AOS_ROLE_STOLAS_GORE) {
        sto_gore_update(e);
        return hit;
    }
    if (!sto_is_stolas(e->id))
        return hit;

    /* The create's record priming runs once, before the dormant check
     * (the native B7F0/B6B0/C324 block precedes the + 0x8E flag test). */
    if (!e->sto_c) {
        e->sto_c = 1;
        sto_init_records(e);
    }

    /* EnemyStolasUpdate prologue: state 2 skips the activity gates;
     * the bounds/hurt/activity checks and sub_08021924 fold into the
     * shared collide below. */
    if (e->state != 2) {
        if (outside_margins(kind, 6, cam_x, cam_y)) {
            e->hidden = true;
            return hit;
        }
        hit = collide(e, soma, kind, weapon, weapon_frames,
                      soma_atk, soma_def);
    }
    /* sub_0809B928 lethal: sub_080683BC defeated, song 0x6E, state 2,
     * timer 0x3C — checked here like the other ports. */
    if (!e->defeated && e->hp <= 0 && e->state != 2) {
        e->defeated = true;
        aos_audio_play_song(0x6E);
        e->state = 2;
        e->step = e->substep = 0;
        e->timer = 0x3C;
    }

    switch (e->state) {
    case 0:
        sto_state0(e, soma, anims);
        break;
    case 1:
        sto_state1(&hit, e, soma, anims);
        break;
    case 2:
        sto_state2(&hit, e);
        break;
    case 3:
        sto_dormant(e, anims);
        break;
    default:
        break;
    }
    /* The container's record pass: every frame the six slots advance
     * (the native C304/C324 calls scattered through the state machines
     * fold into one pass here). */
    if (e->state != 3) {
        for (int i = 0; i < 6; ++i)
            sto_step_record(e, i);
    }
    /* The six bones emit on the first updates (sub_0806DFF8 x6 in
     * EnemyStolasCreate). */
    if (e->role == AOS_ROLE_ENEMY && !e->removed)
        sto_spawn_bones(&hit, e, random);
    integrate(e);
    return hit;
}
