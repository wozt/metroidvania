/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow Soma motion rules; comments name the cvaos routine and
 * the code path each function reproduces. */
#include "aos_soma.h"

const int8_t aos_soma_stand_probes[] = {3, -12, -20, -28};
const int8_t aos_soma_low_probes[] = {2, -6, -9};

/* sub_0801B0D8 prologue. */
void aos_soma_integrate(AosSoma *soma) {
    soma->x += soma->vx + soma->extra_vx;
    if (soma->vy > AOS_FALL_CAP) soma->vy = AOS_FALL_CAP;
    soma->y += soma->vy;
    soma->extra_vx = 0;
}

/* sub_0801B0D8 states 0 and 1: left is tested first, then right; without a
 * direction the friction moves vx toward zero and stops on a sign change. */
void aos_soma_steer(AosSoma *soma, uint16_t held, int32_t speed) {
    if (held & AOS_KEY_LEFT) {
        soma->facing_left = true;
        soma->vx = -speed;
        soma->friction = 0;
    } else if (held & AOS_KEY_RIGHT) {
        soma->facing_left = false;
        soma->vx = speed;
        soma->friction = 0;
    } else {
        soma->friction = soma->vx >= 0 ? -AOS_FRICTION : AOS_FRICTION;
    }
    int32_t old = soma->vx;
    int32_t updated = old + soma->friction;
    soma->vx = updated;
    if ((old < 0 && updated > 0) || (old >= 0 && updated < 0) ||
        (old == 0 && updated > 0)) {
        soma->vx = 0;
        soma->friction = 0;
    }
}

/* sub_08019180, normal jump branch: grounded, or airborne for at most three
 * frames. The flag 0x800000 branch and the extra jumps are not ported. */
bool aos_soma_jump(AosSoma *soma, uint16_t pressed) {
    if (!(pressed & AOS_KEY_JUMP)) return false;
    if ((soma->flags & AOS_FLAG_AIRBORNE) && soma->air_frames > 3) return false;
    soma->flags = (soma->flags | AOS_FLAG_AIRBORNE) & 0xFFEFFBFFu;
    soma->air_frames = 16;
    soma->gravity_mod = 0;
    soma->vy = (soma->flags & AOS_FLAG_HEAVY) ? AOS_SLOWED_JUMP_VELOCITY
                                              : AOS_JUMP_VELOCITY;
    return true;
}

/* sub_08018020 entry. */
void aos_soma_leave_ground(AosSoma *soma) {
    if (soma->flags & 0x00100002u) return;
    soma->flags |= AOS_FLAG_AIRBORNE;
    soma->vy = 0;
    soma->gravity_mod = (int32_t)0xFFFFF000;   /* -0.0625 */
}

/* sub_08018B98 (a label inside sub_08018020). The ceiling special case
 * sub_08017CC8 needs entity flag 0x10 and is not ported. */
void aos_soma_gravity(AosSoma *soma, const AosCollision *layer) {
    if (!(soma->flags & 0x1Eu)) return;
    soma->flags &= 0xEFEFFBFFu;
    if (soma->flags & AOS_FLAG_HEAVY) {
        soma->vy += 0x6000;                    /* 0.375 */
        soma->gravity_mod = 0;
    }
    if (soma->air_frames <= 15) soma->air_frames++;
    if (layer && (soma->vy <= 0 || soma->vx != 0)) {
        int32_t x = soma->x >> 16, y = (soma->y >> 16) - 32;
        int depth = aos_ceiling_depth(layer, x - 5, y, 0, false);
        if (!depth) depth = aos_ceiling_depth(layer, x + 5, y, 0, false);
        if (depth) {
            soma->y += (int32_t)((uint32_t)depth << 16);
            if (soma->vy < 0) {
                soma->vy = 0x1000;                      /* 0.0625 */
                soma->gravity_mod = (int32_t)0xFFFFE000; /* -0.125 */
            }
        }
    }
    if (soma->vy <= 0x1FFF) soma->vy += 0x2000; /* 0.125 */
    soma->vy += 0x1A00;                         /* 0.1015625 */
    if (soma->vy > 0) {
        soma->vy += soma->gravity_mod;
        soma->gravity_mod += 0x400;             /* 0.015625 */
        if (soma->gravity_mod > 0x1000) soma->gravity_mod = 0x1000;
    }
}

/* sub_0801938C: jump release, apex float and slowed falls, then the start
 * of sub_08018020 (ledge start and gravity). */
void aos_soma_air(AosSoma *soma, const AosCollision *layer, uint16_t held) {
    if (soma->flags & 0x1Eu) {
        if (!(soma->flags & 0x18u)) {
            if (soma->vy < (int32_t)0xFFFFC000 && !(held & AOS_KEY_JUMP)) {
                soma->vy = (int32_t)0xFFFFC000;            /* -0.25 */
                soma->gravity_mod = (int32_t)0xFFFFE000;   /* -0.125 */
            }
            if ((uint32_t)(soma->vy + 0x1BFFF) <= 0x1DFFEu && (held & AOS_KEY_JUMP)) {
                soma->gravity_mod += (int32_t)0xFFFFF800;  /* -0.03125 */
                if (soma->gravity_mod < (int32_t)0xFFFFE000)
                    soma->gravity_mod = (int32_t)0xFFFFE000;
            }
        }
        if (((soma->abilities & AOS_ABILITY_SLOW_FALL) ||
             (soma->flags & AOS_FLAG_SLOW_FALL)) && soma->vy > 0x2800)
            soma->vy += (int32_t)0xFFFFD800;               /* -0.15625 */
        if (soma->abilities & AOS_ABILITY_ZERO_FALL) soma->vy = soma->gravity_mod = 0;
    }
    aos_soma_leave_ground(soma);
    if ((soma->flags & 0x00800010u) != 0x00800000u) aos_soma_gravity(soma, layer);
}

/* Floor walk with the collision mode; *raw receives the first cell byte, as
 * the walks store it in gUnk_03002CB0.unk_100D. */
static int floor_probe(const AosCollision *layer, int32_t x, int32_t y, int mode,
                       uint8_t *raw) {
    *raw = aos_collision_cell(layer, x, y);
    return aos_floor_depth(layer, x, y, mode, true);
}

/* Solid or slope byte that is not a bit-3 cell without bit 0. */
static bool blocks_feet(uint8_t raw) {
    if (!(raw & 2) && !(raw & 0xC0)) return false;
    return !((raw & 8) && !(raw & 1));
}

/* Flags and slope step recorded for each ground probe. */
static void note_floor(AosSoma *soma, uint8_t raw, bool step) {
    if (blocks_feet(raw)) soma->flags &= ~(uint32_t)AOS_FLAG_PLATFORM_ONLY;
    if (raw & 0xC0)
        soma->flags |= (raw & 4) ? AOS_FLAG_SLOPE_RIGHT : AOS_FLAG_SLOPE_LEFT;
    if (step && soma->slope_step < (raw >> 6)) soma->slope_step = raw >> 6;
}

/* A one-way platform only catches feet that entered it this frame. */
static bool catches(const AosSoma *soma, int depth) {
    if ((soma->flags & AOS_FLAG_GROUNDED) || !(soma->flags & AOS_FLAG_PLATFORM_ONLY))
        return true;
    int16_t vy_pixels = (int16_t)(soma->vy >> 16);
    return vy_pixels >= 0 && -depth <= vy_pixels + 2;
}

static void push_y(AosSoma *soma, int pixels) {
    soma->y += (int32_t)((uint32_t)pixels << 16);
}

/* Walls: the first probe of the list that hits pushes Soma out. */
static int collide_walls(AosSoma *soma, const AosCollision *layer, int32_t y) {
    int32_t motion = soma->vx + soma->extra_vx;
    if (!motion) return 0;
    const int8_t *probes = soma->wall_probes ? soma->wall_probes : aos_soma_stand_probes;
    int direction = motion < 0 ? -1 : 1;
    for (int i = 0; i < probes[0]; ++i) {
        int32_t x = (soma->x >> 16) + 8 * direction;
        int push = direction < 0 ? aos_wall_push_right(layer, x, y + probes[1 + i])
                                 : aos_wall_push_left(layer, x, y + probes[1 + i]);
        if (!push) continue;
        soma->x += (int32_t)((uint32_t)push << 16);
        if (soma->flags & AOS_FLAG_STOP_AT_WALL) {
            soma->vx = soma->friction = 0;
        } else {
            soma->vx = -soma->vx / 4;
            soma->friction = -soma->friction;
        }
        soma->flags |= AOS_FLAG_WALL;
        break;
    }
    return direction;
}

/* Bit-3 cells: sets 0x1000000, 0xC00000 and 0x800 (sub_0803319C splash
 * calls are not modelled). */
static void collide_special(AosSoma *soma, const AosCollision *layer, int32_t x,
                            int32_t y) {
    bool head = aos_collision_special(layer, x - 5, y - 25) ||
                aos_collision_special(layer, x + 5, y - 25);
    if (aos_collision_special(layer, x - 5, y - 8) ||
        aos_collision_special(layer, x + 5, y - 8)) {
        if (!(soma->flags & AOS_FLAG_BODY_SPECIAL) && soma->vy > 0 &&
            (soma->abilities & AOS_ABILITY_SPECIAL_WALK)) {
            soma->vy >>= 2;
            soma->gravity_mod = 0;
        }
        soma->flags |= AOS_FLAG_BODY_SPECIAL;
    } else {
        soma->flags &= ~(uint32_t)AOS_FLAG_BODY_SPECIAL;
    }
    if (head) {
        soma->flags = (soma->flags & 0xFF7FFFFBu) | 0x00C00000u;
        if (!aos_collision_special(layer, x, y - 26)) soma->flags |= 0x800u;
    } else {
        if ((soma->flags & AOS_FLAG_HEAD_SPECIAL) && !(soma->flags & 0x10u)) {
            if (!(soma->flags & 0x02000000u)) soma->vy = 0;
            soma->flags &= ~0x02000000u;
        }
        soma->flags &= ~0x00C00000u;
    }
    if (soma->flags & AOS_FLAG_HEAD_SPECIAL) {
        if ((soma->abilities & AOS_ABILITY_SPECIAL_WALK) || (soma->flags & AOS_FLAG_HEAVY))
            soma->flags &= ~(uint32_t)AOS_FLAG_HEAD_SPECIAL;
        else
            soma->flags &= ~0x400u;
    }
}

static AosLanding land(AosSoma *soma) {
    soma->flags |= AOS_FLAG_GROUNDED;
    soma->air_frames = 0;
    soma->flags &= ~0x0020017Eu;   /* 0x20031E when the weapon has 0x2000 */
    soma->y &= ~0xFFFF;
    AosLanding landing = AOS_LANDING_NORMAL;
    if (soma->vy > 0x64000 || (soma->flags & AOS_FLAG_STOP_AT_WALL)) {
        soma->flags |= AOS_FLAG_HARD_LANDING;
        soma->state = 4;
        landing = AOS_LANDING_HARD;
    } else {
        soma->state = 0;
    }
    soma->vy = soma->gravity_mod = 0;
    return landing;
}

AosLanding aos_soma_collide(AosSoma *soma, const AosCollision *layer) {
    soma->flags &= ~0x28000800u;
    int32_t x = soma->x >> 16, y = soma->y >> 16;
    if (aos_ceiling_depth(layer, x, y - 33, 0, false))
        soma->flags |= AOS_FLAG_LOW_CEILING;
    int mode = 0;
    if ((soma->abilities & AOS_ABILITY_FLOOR_MODE) &&
        !aos_collision_special(layer, x - 5, y - 7) &&
        !aos_collision_special(layer, x + 5, y - 7))
        mode = 1;
    uint8_t raw;
    int under = floor_probe(layer, x, y + 1, 0, &raw);

    int direction = collide_walls(soma, layer, y);
    x = soma->x >> 16;
    if (aos_ceiling_depth(layer, x - 5, y - 20, 0, false) ||
        aos_ceiling_depth(layer, x + 5, y - 20, 0, false))
        soma->flags |= AOS_FLAG_LOW_CEILING | AOS_FLAG_HEAD_CEILING;
    else
        soma->flags &= ~(uint32_t)AOS_FLAG_HEAD_CEILING;
    if (soma->drop_timer) soma->drop_timer--;
    collide_special(soma, layer, x, y);

    soma->slope_contact = 0;
    for (int i = 0; direction && i < 4; ++i) {
        if (floor_probe(layer, x + i * direction, y + 1, mode, &raw) &&
            (raw & 0xC0) && !(raw & 2)) {
            soma->slope_contact = 1;
            break;
        }
    }

    if (!(soma->flags & AOS_FLAG_GROUNDED) && soma->vy <= 0) {
        /* Rising: only solid cells and slopes push the feet out. */
        int depth = floor_probe(layer, x, y, mode, &raw);
        if (blocks_feet(raw) && depth) push_y(soma, depth);
    }
    if (soma->vy < 0 || soma->drop_timer) return AOS_LANDING_NONE;

    bool snapped = false;
    soma->flags = (soma->flags & 0xFFFF9FFFu) | AOS_FLAG_PLATFORM_ONLY;
    soma->slope_step = 0;
    int center = floor_probe(layer, x, y + 1, mode, &raw);
    note_floor(soma, raw, true);
    if (!center) center = under;
    int left = floor_probe(layer, x - 5, y + 1, mode, &raw);
    note_floor(soma, raw, true);
    int right = floor_probe(layer, x + 5, y + 1, mode, &raw);
    note_floor(soma, raw, true);

    if (soma->flags & AOS_FLAG_HEAD_SPECIAL) {
        soma->flags |= AOS_FLAG_AIRBORNE;
        if (aos_collision_special(layer, x, y) &&
            ((soma->flags & 0x800u) || aos_collision_special(layer, x, y - 30)))
            return AOS_LANDING_NONE;
    }
    if (center && catches(soma, center)) {
        push_y(soma, center + 1);
        snapped = true;
        soma->flags |= AOS_FLAG_SNAPPED;
    }
    if (!(soma->flags & AOS_FLAG_AIRBORNE)) {
        center = floor_probe(layer, x, y + 7, mode, &raw);
        if (snapped) goto contact;
        if (center) {
            push_y(soma, center + 7);
            snapped = true;
            soma->flags |= AOS_FLAG_SNAPPED;
            goto contact;
        }
    }
    if (!snapped && left && catches(soma, left) &&
        !(soma->flags & (AOS_FLAG_SLOPE_LEFT | AOS_FLAG_SLOPE_RIGHT))) {
        push_y(soma, left + 1);
        snapped = true;
    }
    if (!snapped && right && catches(soma, right) &&
        !(soma->flags & (AOS_FLAG_SLOPE_LEFT | AOS_FLAG_SLOPE_RIGHT)))
        push_y(soma, right + 1);

contact:
    if (!center && !left && !right) {
        soma->flags &= ~(uint32_t)AOS_FLAG_GROUNDED;
        return AOS_LANDING_NONE;
    }
    if (!catches(soma, center) || soma->vy <= 0) return AOS_LANDING_NONE;
    if (!center && (soma->flags & (AOS_FLAG_SLOPE_LEFT | AOS_FLAG_SLOPE_RIGHT)))
        return AOS_LANDING_NONE;
    return land(soma);
}
