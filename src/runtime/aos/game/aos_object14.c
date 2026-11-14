#include "aos_object14.h"

#include "../math/aos_math.h"

/* Faithful ports of the small engine helpers the handlers
 * share; the tile/wall queries stay injected (see the header). */

/* GetEntityRoomX/YPositionInteger approximation: the entity's
 * whole-pixel room position, + 0x40/+ 0x44 >> 16. */
static int32_t room_px_x(const AosObject14 *o) {
    return o->pos_x >> 16;
}

static int32_t room_px_y(const AosObject14 *o) {
    return o->pos_y >> 16;
}

static void register_entity(AosObject14 *o, uint8_t chan) {
    /* sub_08021278(ent, chan): slot the entity into the
     * + 0x13170 proximity table. */
    o->register_calls++;
    o->register_arg = chan;
}

static void hurt_install(AosObject14 *o) {
    /* sub_0804277C(ent, 8, 0, 0): the shared hurt slot. */
    o->hurt_calls++;
}

static void preset_428f0(AosObject14 *o, uint32_t arg) {
    /* sub_080428F0(ent, arg): attack/collision preset. */
    o->preset_arg = arg;
}

static void affine_3e058(AosObject14 *o, int32_t angle, int32_t scale) {
    /* sub_0803E058(angle, scale, scale, + 0x58 bit 6,
     * {0, + 0x58 & 0x3F}): the affine transform request into
     * gUnk_03001458; counted with the raw r0/r1 args. */
    o->affine_calls++;
    o->affine_a = angle;
    o->affine_b = scale;
}

static void child_install(AosObject14 *o) {
    /* + 4 = sub_080527E4 on every state-0 init. */
    o->cb_installs++;
}

static void home_snapshot(AosObject14 *o) {
    /* GetEntityRoomX/YPositionInteger into + 0x18/+ 0x1A and
     * + 0x1C/+ 0x1E (strh truncation like the ROM). */
    o->home_x = (int16_t)room_px_x(o);
    o->home_y = (int16_t)room_px_y(o);
    o->room_x = o->home_x;
    o->room_y = o->home_y;
}

/* sub_0806E3F8: 16.16 distance from + 0x40/+ 0x44 to the target
 * fine position, whole-pixel truncation toward zero per axis. */
static int32_t obj14_distance(const AosObject14 *o,
                              int32_t tx, int32_t ty) {
    int32_t dx = tx - o->pos_x;
    int32_t dy = ty - o->pos_y;
    int32_t px = dx >= 0 ? dx >> 16 : -((-dx) >> 16);
    int32_t py = dy >= 0 ? dy >> 16 : -((-dy) >> 16);
    return (int32_t)aos_isqrt((uint32_t)(px * px + py * py)) << 16;
}

/* (v + sign) style truncation of a 16.16 value to whole pixels,
 * toward zero — the ROM's cmp/rsbs/asrs pairs. */
static int32_t trunc16(int32_t v) {
    return v >= 0 ? v >> 16 : -((-v) >> 16);
}

/* sub_080686CC(s16 *field, s16 step, s16 target): approach the
 * target on the s16 circle; snaps and returns 1 on arrival. */
static int s16_approach_dir(uint16_t cur, int32_t target) {
    if (cur == (uint16_t)target) return 0;
    uint16_t d1 = (uint16_t)(cur - (uint16_t)target);
    uint16_t d2 = (uint16_t)((uint16_t)target - cur);
    return d1 <= d2 ? -1 : 1;
}

static int angle_approach(int16_t *field, int16_t step,
                          int16_t target) {
    int dir = s16_approach_dir((uint16_t)*field, target);
    if (dir == 0) return 1;
    if (dir > 0) {
        *field = (int16_t)(uint16_t)(*field + step);
        if (s16_approach_dir((uint16_t)*field, target) == 1)
            return 0;
    } else {
        *field = (int16_t)(uint16_t)(*field - step);
        if (s16_approach_dir((uint16_t)*field, target) == -1)
            return 0;
    }
    *field = target;
    return 1;
}

/* sub_0806D490(ent, n): + 0x48 = + 0x48 * (n - 1) / n through
 * the ROM's 32-bit multiply and __aeabi_idiv. */
static void drag_x(AosObject14 *o, int32_t n) {
    int32_t p = (int32_t)((uint32_t)o->vel_x * (uint32_t)(n - 1));
    o->vel_x = p / n;
}

/* sub_0806D3D8(ent, limit): twin-axis velocity clamp then
 * position integration. */
static void phys_step(AosObject14 *o, int32_t limit) {
    o->vel_x += o->acc_x;
    if (limit) {
        if (o->acc_x >= 0) {
            if (o->vel_x > limit) o->vel_x = limit;
        } else if (o->vel_x < -limit) {
            o->vel_x = -limit;
        }
    }
    o->vel_y += o->acc_y;
    if (limit) {
        if (o->acc_y >= 0) {
            if (o->vel_y > limit) o->vel_y = limit;
        } else if (o->vel_y < -limit) {
            o->vel_y = -limit;
        }
    }
    o->pos_x += o->vel_x;
    o->pos_y += o->vel_y;
}

/* sub_08051BF8(ent, mode): integrate, register channel 7, run
 * the primary sub_08001E58 probe below the entity, bump + 0x44
 * by the hit distance and steer + 0x20 through ArcTan2 +
 * sub_080686CC on the secondary probe. Returns the s16 first
 * hit distance (0 = clear). */
static int32_t probe_move_51bf8(AosObject14 *o, int32_t mode,
                                const AosObject14Env *env) {
    o->pos_x += o->vel_x;               /* + 0x40 += + 0x48 */
    o->pos_y += o->vel_y;               /* + 0x44 += + 0x4C */
    o->vel_y += o->acc_y;               /* + 0x4C += + 0x54 */
    register_entity(o, 7);

    int32_t rx = room_px_x(o);
    int32_t ry = room_px_y(o);
    /* yoff is the constant pair {0, 8}: 8 only while + 0x4C
     * (post-acceleration) is zero; + 0x4E is not read here. */
    int32_t yoff = o->vel_y == 0 ? 8 : 0;
    int32_t hit = env->probe
        ? (int16_t)env->probe(env->probe_ctx, AOS_OBJ14_PROBE_TILE,
                              (int16_t)rx,
                              (int16_t)ry + (int16_t)yoff, mode)
        : 0;
    if (hit != 0)
        o->pos_y += (int32_t)(int16_t)(hit + (int16_t)yoff) << 16;

    int16_t xstep = o->vel_x < 0 ? (int16_t)-8 : (int16_t)8;
    int32_t hit2 = env->probe
        ? (int16_t)env->probe(env->probe_ctx, AOS_OBJ14_PROBE_TILE,
                              (int16_t)(rx + xstep),
                              (int16_t)ry + (int16_t)yoff, mode)
        : 0;
    if (hit2 != 0) {
        int16_t steer = aos_arctan2(xstep,
                                    (int16_t)(yoff + hit2));
        /* sub_080686CC step is 0x80 here (0x400 in sub_08052208). */
        angle_approach(&o->angle, 0x80, steer);
    }
    return hit;
}

/* sub_08052208(ent, mode): the variant-1 probe — + 8 room-y
 * bias, single primary probe, + 0x44 bump (extra yoff when
 * + 0x4C == 0), then a two-step vertical rescan steering
 * + 0x20. Returns the s16 primary hit (0 = clear). */
static int32_t probe_move_52208(AosObject14 *o, int32_t mode,
                                const AosObject14Env *env) {
    int32_t yoff = (int16_t)o->probe_off;
    if (o->vel_y == 0) yoff = 8;
    o->pos_x += o->vel_x;
    o->pos_y += o->vel_y;
    o->vel_y += o->acc_y;
    register_entity(o, 7);

    int32_t rx = room_px_x(o);
    int32_t ry = room_px_y(o);
    int32_t hit = env->probe
        ? (int16_t)env->probe(env->probe_ctx, AOS_OBJ14_PROBE_TILE,
                              (int16_t)rx,
                              (int16_t)((uint16_t)(ry + 8) +
                                        (uint16_t)yoff), mode)
        : 0;
    if (hit != 0) {
        o->pos_y += (int32_t)(int16_t)hit << 16;
        if (o->vel_y == 0)
            o->pos_y += (int32_t)(int16_t)yoff << 16;
    }

    int16_t xstep = o->vel_x < 0 ? (int16_t)-8 : (int16_t)8;
    /* The ROM re-reads GetEntityRoomYPositionInteger here, after
     * the + 0x44 bump; the x base stays the first read. */
    int32_t ry2 = room_px_y(o);
    int32_t hit2 = 0;
    for (int iter = 0; iter < 2; ++iter) {
        int32_t dy = iter * 8;
        hit2 = env->probe
            ? (int16_t)env->probe(env->probe_ctx,
                                  AOS_OBJ14_PROBE_TILE,
                                  (int16_t)(rx + xstep),
                                  (int16_t)(ry2 + 8 + dy), mode)
            : 0;
        if (hit2 != 0) {
            hit2 = (int16_t)(uint16_t)(hit2 + dy);
            break;
        }
    }
    if (hit2 != 0) {
        int32_t dist = (int16_t)(uint16_t)hit2;
        if (o->vel_y == 0)
            dist = (int16_t)(uint16_t)(dist - 8);
        int16_t steer = aos_arctan2(xstep, (int16_t)dist);
        angle_approach(&o->angle, 0x400, steer);
    }
    return hit;
}

/* sub_08051CC4(ent): the shared child spawner — two flank
 * children then four free children through EntityCreateInRange
 * type 0x49 with the sub_08052834 callback. The port counts
 * the six spawn attempts; alloc failure and the per-child
 * RNG/velocity setup are not modelled. */
static void spawn_children(AosObject14 *o) {
    o->child_count += 6;
}

/* Variant 0 tail (_0805200C): player-distance preset + affine. */
static void variant0_tail(AosObject14 *o, const AosObject14Env *env) {
    int32_t dist = obj14_distance(o, env->player_x, env->player_y);
    if (dist > 0xC0000) dist = 0xC0000;
    if (env->player_x < o->pos_x) dist = -dist;
    int32_t range = o->angle == 0 ? 0xA : 0xC;
    int32_t dpx = trunc16(dist);
    int32_t sx = dpx * (int32_t)aos_sine((uint16_t)(o->angle + 0x4000));
    int32_t sy = dpx * (int32_t)aos_sine((uint16_t)o->angle);
    int32_t lo = -range + trunc16(sx);
    lo &= 0xFF;
    int32_t hi = (trunc16(sy) & 0xFF) << 8;
    preset_428f0(o, (uint32_t)(lo + hi + (range << 17) + 0x14000000));
    affine_3e058(o, o->angle, o->f24);
}

/* Variant 1 tail (_08052548): clear + 0x14, affine, then the
 * player-distance preset for states above 1. */
static void variant1_tail(AosObject14 *o,
                          const AosObject14Env *env) {
    o->flags14 = 0;
    affine_3e058(o, o->angle, 0x10000);
    if (o->state <= 1) return;
    int32_t dist = obj14_distance(o, env->player_x, env->player_y);
    if (dist > 0xC0000) dist = 0xC0000;
    if (env->player_x < o->pos_x) dist = -dist;
    int32_t dpx = trunc16(dist);
    int32_t sx = dpx * (int32_t)aos_sine((uint16_t)(o->angle + 0x4000));
    int32_t sy = dpx * (int32_t)aos_sine((uint16_t)o->angle);
    int32_t lo = (trunc16(sx) - 8) & 0xFF;
    int32_t hi = (trunc16(sy) & 0xFF) << 8;
    preset_428f0(o, (uint32_t)(lo + hi + 0x14140000));
}

/* sub_08051E60 — variant 0. */
static bool variant0(AosObject14 *o, const AosObject14Env *env) {
    if (env->gate_4be != 0) {
        if (o->state > 1) register_entity(o, 0);
        return true;                    /* _080520F4: full skip */
    }
    switch (o->state) {
    case 0: {
        /* sub_0803E654(1): palette slot or EntityDelete. */
        if (env->palette_slot < 0) {
            o->deleted = true;
            return false;
        }
        o->flags58 = (uint8_t)((o->flags58 & (uint8_t)~0x21) |
                               ((uint16_t)env->palette_slot & 0x1F));
        o->aux_5c = 0xC;
        o->flags59 |= 0x10;
        child_install(o);
        home_snapshot(o);
        o->vel_x = 0x8000;
        hurt_install(o);
        preset_428f0(o, 0x103000E8);
        o->flags72 |= 6;
        o->state = 1;
        /* fall through into state 1 like the ROM */
    }
    /* fall through */
    case 1:
        o->sel_65 = 0;
        o->f24 += 0x400;
        if (o->f24 > 0xFFFF) {
            o->sel_65 = 2;
            o->f24 = 0x10000;
            o->state = 2;
        }
        break;
    case 2:
        if (probe_move_51bf8(o, 2, env) == 0) {
            o->state = 3;
            o->acc_y = 0x2000;
        }
        break;
    case 3:
        if (probe_move_51bf8(o, 1, env) != 0) {
            if (o->vel_y > 0x40000) {
                o->state = 4;
            } else {
                o->state = 2;
                o->vel_y = 0;
                o->acc_y = 0;
            }
        }
        break;
    case 4:
        /* sub_08068AD4(-0x10, 0, 0x110, 0xB0) on + 0x42/+ 0x46. */
        if (aos_probe_box(o->scr_x, o->scr_y, -0x10, 0,
                          0x110, 0xB0)) {
            spawn_children(o);
            o->song = 0xAC;             /* PlaySong */
        }
        o->f24 = 0;
        o->angle = 0;
        o->vel_y = 0;
        o->acc_y = 0;
        o->scr_x = (int16_t)((uint16_t)o->home_x - env->room_ox);
        o->scr_y = (int16_t)((uint16_t)o->home_y - env->room_oy);
        o->state = 1;
        break;
    default:
        break;                         /* > 4: tail only */
    }
    variant0_tail(o, env);
    return true;
}

/* sub_08052340 — variant 1. */
static bool variant1(AosObject14 *o, const AosObject14Env *env) {
    /* + 0x14 collision bits are written by the engine between
     * updates; the tail clears them, so each frame sees only
     * this frame's external writers. */
    o->flags14 = env->collide14;
    switch (o->state) {
    case 0: {
        if (env->palette_slot < 0) {
            o->deleted = true;
            return false;               /* _0805253C */
        }
        o->flags58 = (uint8_t)((o->flags58 & (uint8_t)~0x21) |
                               ((uint16_t)env->palette_slot & 0x1F));
        o->aux_5c = 0xC;
        o->flags59 |= 0x10;
        child_install(o);
        o->sel_65 = 2;
        home_snapshot(o);
        hurt_install(o);
        preset_428f0(o, 0x102EFEE8);
        o->flags72 |= 6;
        o->claim_calls++;               /* sub_08022088 claim */
        o->state = 1;
        /* fall through into state 1 */
    }
    /* fall through */
    case 1: {
        bool grounded = (o->flags14 & 1) != 0;
        if (grounded) {
            if (o->vel_x == 0)
                o->song = 0x136;        /* sub_08013D60(0x9B << 1) */
            o->cnt_c = (uint8_t)(o->cnt_c + 1);
            o->vel_x = 0x4000;
        } else {
            if (o->vel_x != 0)
                o->song_stop = 0x136;   /* sub_08013E18 */
            o->vel_x = 0;
        }
        probe_move_52208(o, 0, env);
        if (o->angle == 0) break;
        o->song = 0x136;
        o->angle = 0;
        o->state = 2;
        o->timer_d = 8;
        break;
    }
    case 2:
        if (o->timer_d != 0) {
            o->timer_d = (uint8_t)(o->timer_d - 1);
            if (o->timer_d == 0)
                o->release_calls++;     /* sub_080220B4 */
        }
        o->cnt_c = (uint8_t)(o->cnt_c + 1);
        if (o->vel_x <= 0x7FFFF)
            o->vel_x += o->angle / 2;
        o->vel_y = o->vel_x;
        probe_move_52208(o, 0, env);
        if ((int16_t)o->angle < 0) {
            o->song_stop = 0x136;
            o->state = 3;
            o->acc_y = 0x400;
            o->vel_y += (int32_t)0xFFF8C000;   /* -0x34000 */
        }
        break;
    case 3:
        if (o->vel_x <= 0x7FFFF)
            o->vel_x += 0x800;
        o->pos_x += o->vel_x;
        o->pos_y += o->vel_y;
        o->vel_y += o->acc_y;
        register_entity(o, 7);
        angle_approach(&o->angle, 0x80, (int16_t)0xE000);
        /* sub_080020D8(room_x + 0x1A, room_y, 0). */
        if (env->probe &&
            env->probe(env->probe_ctx, AOS_OBJ14_PROBE_POINT,
                       room_px_x(o) + 0x1A, room_px_y(o), 0)) {
            spawn_children(o);
            o->song = 0xAC;             /* PlaySong */
            o->palette_frees++;         /* sub_0803E6BC */
            o->palette_free_arg = o->flags58 & 7;
            o->deleted = true;
            return false;               /* _0805253C */
        }
        break;
    default:
        break;
    }
    variant1_tail(o, env);
    return true;
}

/* sub_0805262C — variant 2. */
static bool variant2(AosObject14 *o, const AosObject14Env *env) {
    /* Head probe every frame: sub_08068AD4(-0x80, -0x40, 0x1F0,
     * 0x120) on + 0x42/+ 0x46. */
    if (aos_probe_box(o->scr_x, o->scr_y, -0x80, -0x40,
                      0x1F0, 0x120)) {
        register_entity(o, 3);
        if (env->linked_self) {
            /* sub_0802138C: player + 0x40 = s16(+ 0x40 low). */
            o->player_snap = true;
            o->player_snap_x = (int16_t)o->pos_x;
        }
    }
    switch (o->state) {
    case 0:
        o->flags58 = (uint8_t)(o->flags58 & (uint8_t)~0x21);
        o->aux_5c = 0xC;
        o->sel_65 = 2;
        child_install(o);
        home_snapshot(o);
        hurt_install(o);
        preset_428f0(o, 0x103000E8);
        o->flags72 |= 6;
        if (env->rng_a & 1)
            o->f24 = (int32_t)(uint16_t)0x8000;
        o->f26 = (int16_t)((env->rng_b & 0x3F) + 0x40);
        o->state = 1;
        /* fall through into state 1 */
    /* fall through */
    case 1:
        /* + 0x24/+ 0x26 are u16 angle/speed here. */
        o->f24 = (o->f24 & ~0xFFFF) |
                 (uint16_t)(o->f24 + (uint16_t)o->f26);
        o->vel_x = aos_sine((uint16_t)o->f24) / 3;
        o->pos_x += o->vel_x;
        break;
    case 2: {
        /* Lead point (_08052730): x = A094 room origin +
         * + 0x42 screen + u16(+ 0x48 >> 16) (the velocity's
         * whole-pixel part), then -0x18/+ 0x18 px by the +
         * 0x48 sign; y = A094 + + 0x46. */
        uint16_t lead = (uint16_t)((uint32_t)o->vel_x >> 16);
        uint16_t sum_x = (uint16_t)(env->room_ox +
                                    (uint16_t)o->scr_x + lead);
        uint16_t sum_y = (uint16_t)(env->room_oy +
                                    (uint16_t)o->scr_y);
        int32_t tx, ty;
        if (o->vel_x < 0) {
            tx = (int16_t)(uint16_t)(sum_x - 0x18);
        } else {
            tx = (int16_t)(uint16_t)(sum_x + 0x18);
        }
        ty = (int16_t)sum_y;
        bool wall = env->probe &&
            env->probe(env->probe_ctx, AOS_OBJ14_PROBE_WALL_A,
                       tx, ty, 0) &&
            !env->probe(env->probe_ctx, AOS_OBJ14_PROBE_WALL_B,
                        tx, ty, 0);
        if (wall)
            o->vel_x = -o->vel_x / 2;
        drag_x(o, 0x50);
        phys_step(o, 0x800000);
        int32_t av = o->vel_x < 0 ? -o->vel_x : o->vel_x;
        if (av <= 0x7FF)
            o->state = 1;
        break;
    }
    default:
        break;
    }
    return true;
}

void aos_object14_create(AosObject14 *o, int16_t param0,
                         int32_t pos_x, int32_t pos_y,
                         bool sprite_ok, bool *deleted) {
    *o = (AosObject14){0};
    *deleted = false;
    o->param0 = param0;
    o->pos_x = pos_x;
    o->pos_y = pos_y;
    o->song = -1;
    o->song_stop = -1;
    if (!sprite_ok) {
        o->deleted = true;
        *deleted = true;
        return;
    }
    /* + 0x58 = (+ 0x58 & ~0x41) | 0x20 ; + 0x59 |= 2. */
    o->flags58 = (uint8_t)((o->flags58 & (uint8_t)~0x41) | 0x20);
    o->flags59 |= 2;
}

bool aos_object14_update(AosObject14 *o, const AosObject14Env *env) {
    o->song = -1;
    o->song_stop = -1;
    o->player_snap = false;
    switch (o->param0) {
    case 0: return variant0(o, env);
    case 1: return variant1(o, env);
    case 2: return variant2(o, env);
    default:
        /* The 0x08526408 table has three entries; other param0
         * values read past it natively — treated as a no-op. */
        return true;
    }
}

void aos_object14_child_update(AosObject14 *o,
                               uint16_t room_ox, uint16_t room_oy) {
    /* sub_080527E4: + 0x42/+ 0x46 = + 0x1C/+ 0x1E - A094,
     * sub_0803B9D0 draw, + 0x40/+ 0x44 restored, then the room
     * position refresh. */
    o->scr_x = (int16_t)((uint16_t)o->room_x - room_ox);
    o->scr_y = (int16_t)((uint16_t)o->room_y - room_oy);
    o->draw_calls++;
    o->room_x = (int16_t)room_px_x(o);
    o->room_y = (int16_t)room_px_y(o);
}
