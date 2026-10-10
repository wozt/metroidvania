/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow Soma motion rules; comments name the cvaos routine and
 * the code path each function reproduces. */
#include "aos_soma.h"

const AosBox aos_soma_stand_box = {-6, -32, 12, 28};
const AosBox aos_soma_low_box = {-5, -16, 12, 14};
const AosBox aos_soma_slide_box = {-8, -12, 16, 12};
#define STAND (&aos_soma_stand_box)
#define LOW (&aos_soma_low_box)
#define SLIDE (&aos_soma_slide_box)

const int8_t aos_soma_stand_probes[] = {3, -12, -20, -28};
const int8_t aos_soma_low_probes[] = {2, -6, -9};
const int8_t aos_soma_ceiling_probes[] = {1, -12};       /* 0x080E12E8 */
const int8_t aos_soma_special_probes[] = {3, -6, -16, -28}; /* 0x080E12E4 */

/* sub_0801B0D8 prologue. */
void aos_soma_integrate(AosSoma *soma) {
    soma->x += soma->vx + soma->extra_vx;
    if (soma->vy > AOS_FALL_CAP) soma->vy = AOS_FALL_CAP;
    soma->y += soma->vy;
    soma->extra_vx = 0;
}

/* The pattern used at every animation change of the player code: start
 * the animation unless it is current, drop the pending one and clear the
 * animation-end flag, with the hurtbox of sub_080428B4. Per-animation
 * palettes (0x080E126C) and the 0x131B8 & 0x800 gate are not modelled. */
static void play(AosSoma *soma, unsigned id, bool loop, const AosBox *hurtbox) {
    if (soma->anim.id == id) return;
    soma->hurtbox = *hurtbox;
    aos_anim_start(&soma->anim, soma->anims, id, loop);
    soma->anim.id = (uint8_t)id;
    soma->anim_request = AOS_ANIM_NONE;
    soma->flags &= ~(uint32_t)AOS_FLAG_ANIM_DONE;
}

/* Velocity plus friction, stopping on a sign change (shared by the
 * states of sub_0801B0D8). */
static void apply_friction(AosSoma *soma) {
    int32_t old = soma->vx;
    int32_t updated = old + soma->friction;
    soma->vx = updated;
    if ((old < 0 && updated > 0) || (old >= 0 && updated < 0) ||
        (old == 0 && updated > 0)) {
        soma->vx = 0;
        soma->friction = 0;
    }
}

/* Held direction: left is tested first, then right; otherwise friction
 * toward zero. */
static void steer_direction(AosSoma *soma, uint16_t held, int32_t speed) {
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
}

void aos_soma_steer(AosSoma *soma, uint16_t held, int32_t speed) {
    steer_direction(soma, held, speed);
    apply_friction(soma);
}

/* sub_08017D90: Down + jump in the air (ability 3). It needs flag 4,
 * which only the mid-air jump sets in the code read so far. */
static void dive_kick(AosSoma *soma, uint16_t held, uint16_t pressed) {
    if (!(soma->moves & AOS_MOVE_DIVE_KICK) || !(soma->flags & AOS_FLAG_AIRBORNE) ||
        (soma->flags & 0x00800004u) != 4u || !(held & AOS_KEY_DOWN) ||
        !(pressed & AOS_KEY_JUMP))
        return;
    soma->state = 7;
    soma->vy = 0x4E000;                                 /* 4.875 */
    soma->gravity_mod = 0x2000;
    if (held & AOS_KEY_LEFT) {
        soma->facing_left = true;
        soma->vx = (int32_t)0xFFFC0000;                 /* -4.0 */
        soma->friction = 0;
    } else if (held & AOS_KEY_RIGHT) {
        soma->facing_left = false;
        soma->vx = 0x40000;
        soma->friction = 0;
    } else {
        soma->friction = soma->vx >= 0 ? -AOS_FRICTION : AOS_FRICTION;
    }
    int32_t speed = soma->vx < 0 ? -soma->vx : soma->vx;
    if (speed > 0x10000) play(soma, AOS_SOMA_ANIM_DIVE_KICK, true, LOW);
    else play(soma, AOS_SOMA_ANIM_DIVE_DROP, true, LOW);
    soma->air_anim_locked = true;
}

/* sub_080190E0: mid-air jump (ability 2); true when it starts. */
static bool air_jump(AosSoma *soma, uint16_t pressed) {
    if (!(soma->flags & AOS_FLAG_HEAD_SPECIAL) && (soma->flags & 0x04000004u)) return false;
    if (!(pressed & AOS_KEY_JUMP) || soma->drop_timer) return false;
    soma->anim_request = AOS_SOMA_ANIM_AIR_JUMP;
    soma->hurtbox = aos_soma_low_box;
    soma->flags = (soma->flags | 4u) & ~0x10u;
    if (!(soma->flags & AOS_FLAG_SLOW_FALL)) {
        soma->vy = (int32_t)0xFFFBE000;                 /* -4.125 */
        soma->gravity_mod = 0x1400;                     /* 0.078125 */
    } else if (soma->vy >= 0) {
        soma->vy = (int32_t)0xFFFB8000;                 /* -4.5 */
        soma->gravity_mod = 0;
    }
    return true;
}

/* sub_08019180 */
bool aos_soma_jump(AosSoma *soma, uint16_t held, uint16_t pressed) {
    uint32_t flags = soma->flags;
    if (flags & AOS_FLAG_LOW_CEILING) return false;
    if (flags & 0x160u) return false;
    if (!(flags & AOS_FLAG_AIRBORNE) && (flags & AOS_FLAG_PLATFORM_ONLY) &&
        (held & AOS_KEY_DOWN) && (pressed & AOS_KEY_JUMP)) {
        /* Drop through a one-way platform. */
        soma->drop_timer = (flags & AOS_FLAG_BODY_SPECIAL) ? 32 : 16;
        soma->flags = (flags | AOS_FLAG_AIRBORNE) & 0xFFFEEFFFu;
        soma->vx = soma->vy = 0;
        soma->gravity_mod = (int32_t)0xFFFFF000;
    }
    flags = soma->flags;
    if ((soma->moves & AOS_MOVE_HIGH_JUMP) && (pressed & AOS_KEY_ABILITY) &&
        ((flags & AOS_FLAG_AIRBORNE) || (held & AOS_KEY_UP)) && !(flags & 0x10u)) {
        /* High jump (ability 4). */
        soma->anim_request = AOS_ANIM_NONE;
        soma->state = 5;
        soma->flags = (flags | 0x12u) & 0xFFEFFBFBu;
        soma->gravity_mod = 0;
        soma->vy = (soma->flags & AOS_FLAG_HEAVY) ? (int32_t)0xFFFD8000   /* -2.5 */
                                                  : (int32_t)0xFFF60000;  /* -10.0 */
        return true;
    }
    if (soma->drop_timer) return false;
    if (flags & AOS_FLAG_HEAD_SPECIAL) {
        if (!(flags & 0x02000000u) && (pressed & AOS_KEY_JUMP)) {
            soma->gravity_mod = 0;
            soma->flags = (flags | 0x02000002u) & 0xFFEFFBF7u;
            soma->vy = soma->vy > 0x10000 ? soma->vy + (int32_t)0xFFFB2000
                                          : (int32_t)0xFFFB2000;    /* -4.875 */
            return true;
        }
        dive_kick(soma, held, pressed);
        return false;
    }
    if (!(flags & AOS_FLAG_AIRBORNE) || soma->air_frames <= 3) {
        if (pressed & AOS_KEY_JUMP) {
            soma->flags = (flags | AOS_FLAG_AIRBORNE) & 0xFFEFFBFFu;
            soma->air_frames = 16;
            soma->gravity_mod = 0;
            soma->vy = (soma->flags & AOS_FLAG_HEAVY) ? AOS_SLOWED_JUMP_VELOCITY
                                                      : AOS_JUMP_VELOCITY;
            dive_kick(soma, held, pressed);
            return true;
        }
    } else if ((soma->moves & AOS_MOVE_AIR_JUMP) && air_jump(soma, pressed)) {
        return true;
    }
    dive_kick(soma, held, pressed);
    return false;
}

/* sub_08018020 entry. */
void aos_soma_leave_ground(AosSoma *soma) {
    if (soma->flags & 0x00100002u) return;
    soma->flags |= AOS_FLAG_AIRBORNE;
    soma->vy = 0;
    soma->gravity_mod = (int32_t)0xFFFFF000;   /* -0.0625 */
}

/* sub_08017CC8: a fast rise with flag 0x10 (and not 0x80) crashes into
 * the ceiling (state 6; the screen shake is not modelled). */
static bool ceiling_crash(AosSoma *soma) {
    if (soma->vy > (int32_t)0xFFFB1000 || (soma->flags & 0x90u) != 0x10u) return false;
    play(soma, AOS_SOMA_ANIM_CEILING_CRASH, false, STAND);
    soma->state = 6;
    soma->vx = soma->vy = soma->gravity_mod = 0;
    return true;
}

/* Ceiling walks at (x - 5, y + dy) then (x + 5, y + dy); pushes the head
 * down and stops a rise. Returns true after a ceiling crash. */
static bool bump_ceiling(AosSoma *soma, const AosCollision *layer, int dy) {
    int32_t x = soma->x >> 16, y = (soma->y >> 16) + dy;
    int depth = aos_ceiling_depth(layer, x - 5, y, 0, false);
    if (!depth) depth = aos_ceiling_depth(layer, x + 5, y, 0, false);
    if (!depth) return false;
    soma->y += (int32_t)((uint32_t)depth << 16);
    if (soma->vy < 0) {
        if (ceiling_crash(soma)) return true;
        soma->vy = 0x1000;                      /* 0.0625 */
        soma->gravity_mod = (int32_t)0xFFFFE000; /* -0.125 */
    }
    return false;
}

/* sub_08018B98 (a label inside sub_08018020). */
void aos_soma_gravity(AosSoma *soma, const AosCollision *layer) {
    if (!(soma->flags & 0x1Eu)) return;
    soma->flags &= 0xEFEFFBFFu;
    if (soma->flags & AOS_FLAG_HEAVY) {
        soma->vy += 0x6000;                    /* 0.375 */
        soma->gravity_mod = 0;
    }
    if (soma->air_frames <= 15) soma->air_frames++;
    if (layer && (soma->vy <= 0 || soma->vx != 0) && bump_ceiling(soma, layer, -32)) return;
    if (soma->vy <= 0x1FFF) soma->vy += 0x2000; /* 0.125 */
    soma->vy += 0x1A00;                         /* 0.1015625 */
    if (soma->vy > 0) {
        soma->vy += soma->gravity_mod;
        soma->gravity_mod += 0x400;             /* 0.015625 */
        if (soma->gravity_mod > 0x1000) soma->gravity_mod = 0x1000;
    }
    /* _08018D0E: during a high jump the head is also checked at y - 24,
     * and flag 0x10 ends once falling faster than 1.125 (0.5 slowed). */
    if (soma->flags & 0x10u) {
        if (layer && bump_ceiling(soma, layer, -24)) return;
        int32_t limit = (soma->flags & AOS_FLAG_SLOW_FALL) ? 0x8000 : 0x12000;
        if (soma->vy > limit) soma->flags &= ~0x10u;
    }
    /* _08018E4A: airborne animations. */
    if ((soma->flags & 0x1E0u) || soma->air_anim_locked) return;
    if ((soma->abilities & AOS_ABILITY_FAST_WALK) && soma->vx) {
        play(soma, AOS_SOMA_ANIM_FAST, true, STAND);
    } else if (soma->vy > 0x3FFF) {
        play(soma, AOS_SOMA_ANIM_FALL, false, LOW);
    } else if ((soma->flags & 4u) && soma->anim_request != AOS_ANIM_NONE) {
        return;
    } else if (soma->flags & 0x10u) {
        play(soma, AOS_SOMA_ANIM_HIGH_JUMP, true, LOW);
    } else if (soma->vx) {
        play(soma, AOS_SOMA_ANIM_JUMP_FORWARD, false, LOW);
    } else {
        play(soma, AOS_SOMA_ANIM_JUMP, true, LOW);
    }
}

/* The start of sub_08018020: ledge start and gravity. */
static void air_routine(AosSoma *soma, const AosCollision *layer) {
    aos_soma_leave_ground(soma);
    if ((soma->flags & 0x00800010u) != 0x00800000u) aos_soma_gravity(soma, layer);
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
    air_routine(soma, layer);
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
    bool keeps_attack = soma->weapon.flags & AOS_WEAPON_LANDING_ATTACK;   /* sub_08023424 */
    soma->flags |= AOS_FLAG_GROUNDED;
    soma->air_frames = 0;
    soma->flags &= keeps_attack ? ~0x0020031Eu : ~0x0020017Eu;
    soma->y &= ~0xFFFF;
    AosLanding landing = AOS_LANDING_NORMAL;
    if (soma->vy > 0x64000 || (soma->flags & AOS_FLAG_STOP_AT_WALL)) {
        soma->flags |= AOS_FLAG_HARD_LANDING;
        if (keeps_attack) soma->flags &= ~0x160u;
        play(soma, AOS_SOMA_ANIM_HARD_LANDING, false, LOW);
        soma->state = 4;
        landing = AOS_LANDING_HARD;
    } else if (!(soma->flags & AOS_FLAG_ATTACKING) || !keeps_attack) {
        if (!(soma->held & 0xF0) &&
            !(soma->pressed & (AOS_KEY_JUMP | AOS_KEY_ATTACK | AOS_KEY_ABILITY | AOS_KEY_GUARDIAN)))
        {
            soma->anim_request = AOS_SOMA_ANIM_LAND;
            soma->hurtbox = aos_soma_low_box;
        }
        if (soma->held & (AOS_KEY_LEFT | AOS_KEY_RIGHT))
            play(soma, AOS_SOMA_ANIM_WALK, true, STAND);
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

/* Slope slowdown: vx / divisor * 16 by the steepest slope step. */
static void slow_on_slope(AosSoma *soma, const int32_t divisors[3]) {
    if (!((soma->vx > 0 && (soma->flags & AOS_FLAG_SLOPE_RIGHT)) ||
          (soma->vx < 0 && (soma->flags & AOS_FLAG_SLOPE_LEFT))))
        return;
    if (soma->slope_step >= 1 && soma->slope_step <= 3)
        soma->vx = soma->vx / divisors[soma->slope_step - 1] * 16;
}

/* sub_080197B4: attack start (the auto-attack flag 0x1325C & 2, the
 * 0x131B8 & 0x80 lock and the weapon entity of sub_080230A8 are not
 * modelled; the entity exists while weapon_active). */
static void attack_start(AosSoma *soma, uint16_t pressed) {
    if (!(pressed & AOS_KEY_ATTACK) || (soma->flags & 0x160u) || soma->weapon_active ||
        soma->weapon.weapon_class == 5)
        return;
    if (soma->flags & AOS_FLAG_AIRBORNE) {
        soma->flags |= AOS_FLAG_AIR_ATTACK;
        play(soma, soma->weapon.anims[2], false, LOW);
    } else if (soma->flags & AOS_FLAG_CROUCH) {
        play(soma, soma->weapon.anims[1], false, LOW);
    } else {
        play(soma, soma->weapon.anims[0], false, STAND);
    }
    soma->weapon_active = true;
    soma->flags |= AOS_FLAG_ATTACKING;
    soma->state = 1;
}

/* Grounded animations and crouch of case 0 (_0801BEAC .. _0801C298). */
static void ground_animation(AosSoma *soma, uint16_t held, int32_t start_speed) {
    if (held & AOS_KEY_DOWN || (soma->flags & AOS_FLAG_HEAD_CEILING)) {
        if (!(soma->flags & AOS_FLAG_CROUCH)) {
            soma->flags = (soma->flags | AOS_FLAG_CROUCH) & ~(uint32_t)AOS_FLAG_BACKDASH;
            soma->anim_request = AOS_SOMA_ANIM_CROUCH_DOWN;
            soma->hurtbox = aos_soma_low_box;
        }
        if (soma->anim_request == AOS_ANIM_NONE) play(soma, AOS_SOMA_ANIM_CROUCH, true, LOW);
    } else if (held & (AOS_KEY_LEFT | AOS_KEY_RIGHT)) {
        if ((soma->flags & AOS_FLAG_BACKDASH) || soma->anim_request == AOS_SOMA_ANIM_TURN) {
            /* no change */
        } else if (soma->abilities & AOS_ABILITY_FAST_WALK) {
            play(soma, AOS_SOMA_ANIM_FAST, true, STAND);
        } else {
            uint8_t current = soma->anim.id;
            if (current != AOS_SOMA_ANIM_WALK && current != AOS_SOMA_ANIM_WALK_START &&
                current != AOS_SOMA_ANIM_TURN)
                play(soma, AOS_SOMA_ANIM_WALK_START, false, STAND);
            if (soma->flags & AOS_FLAG_ANIM_DONE) play(soma, AOS_SOMA_ANIM_WALK, true, STAND);
        }
    } else {
        if (!(soma->flags & AOS_FLAG_CROUCH) && (held & AOS_KEY_UP)) {
            if (soma->up_frames > 3) {
                play(soma, AOS_SOMA_ANIM_LOOK_UP, false, STAND);
                return;             /* skips the crouch release */
            }
            soma->up_frames++;
        } else {
            soma->up_frames = 0;
        }
        uint8_t current = soma->anim.id;
        if (!(soma->flags & AOS_FLAG_BACKDASH) && current != AOS_SOMA_ANIM_WALK_START &&
            current != AOS_SOMA_ANIM_IDLE && current != AOS_SOMA_ANIM_TURN &&
            start_speed > 0xFFFF)
        {
            soma->anim_request = AOS_SOMA_ANIM_STOP;
            soma->hurtbox = aos_soma_stand_box;
        }
        if (soma->anim_request == AOS_ANIM_NONE || soma->anim.id == AOS_SOMA_ANIM_WALK_START) {
            soma->flags &= ~(uint32_t)AOS_FLAG_BACKDASH;
            play(soma, AOS_SOMA_ANIM_IDLE, true, STAND);
        }
    }
    /* _0801C254: stand up. */
    if ((soma->flags & (AOS_FLAG_HEAD_CEILING | AOS_FLAG_CROUCH)) == AOS_FLAG_CROUCH &&
        !(held & AOS_KEY_DOWN)) {
        soma->flags &= ~(uint32_t)AOS_FLAG_CROUCH;
        soma->anim_request = AOS_SOMA_ANIM_STAND_UP;
        soma->hurtbox = aos_soma_stand_box;
    }
}

/* sub_0801B0D8 case 0 (_0801BA98), movement and animations: the dust and
 * splash effects and the attacks (sub_080197B4, sub_08019478) are not
 * ported. */
static void normal_state(AosSoma *soma, const AosCollision *layer, uint16_t held,
                         uint16_t pressed) {
    static const int32_t uphill[3] = {24, 20, 18};
    static const int32_t crouched[3] = {23, 19, 17};
    int32_t start_speed = soma->vx < 0 ? -soma->vx : soma->vx;
    int32_t speed = (soma->abilities & AOS_ABILITY_FAST_WALK) ? AOS_FAST_WALK_SPEED
                                                              : AOS_WALK_SPEED;
    if ((pressed & AOS_KEY_ABILITY) && !(held & AOS_KEY_UP) &&
        (soma->moves & AOS_MOVE_BACKDASH) && !(soma->flags & 0x10008402u)) {
        /* Backdash (ability 0, sound 0xA9): ends with its animation. */
        soma->anim_request = AOS_SOMA_ANIM_BACKDASH;
        soma->hurtbox = aos_soma_stand_box;
        soma->flags = (soma->flags & ~(uint32_t)AOS_FLAG_ANIM_DONE) | AOS_FLAG_BACKDASH;
        soma->frame_counter = 0;
        soma->vx = soma->facing_left ? 0x3C000 : (int32_t)0xFFFC4000;   /* 3.75 */
    }
    if (!(soma->flags & (AOS_FLAG_BACKDASH | AOS_FLAG_HEAD_CEILING | AOS_FLAG_CROUCH))) {
        if (((held & AOS_KEY_RIGHT) && soma->facing_left) ||
            ((held & AOS_KEY_LEFT) && !soma->facing_left)) {
            if (!(soma->flags & AOS_FLAG_AIRBORNE)) {
                soma->anim_request = AOS_SOMA_ANIM_TURN;
                soma->hurtbox = aos_soma_stand_box;
            } else if ((soma->flags & 0x00800010u) == 0x00800000u) {
                soma->anim_request = AOS_SOMA_ANIM_SPECIAL_TURN;
                soma->hurtbox = aos_soma_stand_box;
            }
        }
        steer_direction(soma, held, speed);
        if (!(soma->flags & AOS_FLAG_AIRBORNE)) slow_on_slope(soma, uphill);
    } else if (soma->flags & AOS_FLAG_BACKDASH) {
        if (held & AOS_KEY_LEFT) soma->facing_left = true;
        else if (held & AOS_KEY_RIGHT) soma->facing_left = false;
        soma->friction = soma->vx < 0 ? AOS_FRICTION : -AOS_FRICTION;
    } else {
        if (!(soma->flags & AOS_FLAG_AIRBORNE)) slow_on_slope(soma, crouched);
        soma->friction = soma->vx < 0 ? 0x2800 : -0x2800;   /* 0.15625 */
    }
    apply_friction(soma);

    if (soma->flags & AOS_FLAG_GROUNDED) {
        soma->frame_counter++;
        if (!(soma->flags & 0x1000001Eu)) ground_animation(soma, held, start_speed);
    }
    attack_start(soma, pressed);
    if ((soma->moves & AOS_MOVE_SLIDE) && !(soma->flags & 0x1122u) &&
        (held & AOS_KEY_DOWN) && (pressed & AOS_KEY_JUMP)) {
        /* Slide (ability 1, state 3, sound 0xBD). */
        soma->state = 3;
        soma->anim_request = AOS_ANIM_NONE;
        play(soma, AOS_SOMA_ANIM_SLIDE, false, SLIDE);
        soma->vx = soma->facing_left ? (int32_t)0xFFFCE000 : 0x32000;   /* 3.125 */
        soma->friction = soma->vx >= 0 ? (int32_t)0xFFFFE800 : 0x1800; /* 0.09375 */
        soma->frame_counter = 0;
        soma->flags = (soma->flags | 0x20000420u) & ~(uint32_t)AOS_FLAG_BACKDASH;
    }
    aos_soma_jump(soma, held, pressed);
    aos_soma_air(soma, layer, held);
}

/* Starts an animation but keeps the frame and tick, as case 3 does. */
static void play_keeping_frame(AosSoma *soma, unsigned id, const AosBox *hurtbox) {
    if (soma->anim.id == id) return;
    uint8_t frame = soma->anim.frame, tick = soma->anim.tick;
    play(soma, id, false, hurtbox);
    soma->anim.frame = frame;
    soma->anim.tick = tick;
}

/* sub_0801B0D8 case 3 (_0801C7BC): slide for 32 frames, or until airborne. */
static void slide_state(AosSoma *soma, const AosCollision *layer) {
    if (((soma->flags & AOS_FLAG_SLOPE_LEFT) && soma->slope_step == 1 && !soma->facing_left) ||
        ((soma->flags & AOS_FLAG_SLOPE_RIGHT) && soma->slope_step == 1 && soma->facing_left))
        play_keeping_frame(soma, AOS_SOMA_ANIM_SLIDE_DOWNHILL, SLIDE);
    else
        play_keeping_frame(soma, AOS_SOMA_ANIM_SLIDE, SLIDE);
    apply_friction(soma);
    if (soma->flags & AOS_FLAG_AIRBORNE) soma->frame_counter = 0x20;
    if (++soma->frame_counter > 0x1F) {
        soma->flags &= ~0x20u;
        soma->state = 0;
    }
    air_routine(soma, layer);
}

/* Case 1 (_0801C410): attack. Steering only in the air; the attack ends
 * with its animation (recovery request by posture) or with a backdash. */
static void attack_state(AosSoma *soma, const AosCollision *layer, uint16_t held,
                         uint16_t pressed) {
    int32_t speed = (soma->abilities & AOS_ABILITY_FAST_WALK) ? AOS_FAST_WALK_SPEED
                                                              : AOS_WALK_SPEED;
    if ((soma->flags & AOS_FLAG_AIRBORNE) && (held & AOS_KEY_LEFT)) {
        soma->vx = -speed;
        soma->friction = 0;
    } else if ((soma->flags & AOS_FLAG_AIRBORNE) && (held & AOS_KEY_RIGHT)) {
        soma->vx = speed;
        soma->friction = 0;
    } else {
        soma->friction = soma->vx >= 0 ? -AOS_FRICTION : AOS_FRICTION;
    }
    apply_friction(soma);
    if ((soma->weapon.flags & AOS_WEAPON_LANDING_ATTACK) && (soma->flags & AOS_FLAG_AIR_ATTACK) &&
        (soma->flags & AOS_FLAG_GROUNDED)) {
        play_keeping_frame(soma, soma->weapon.anims[0], STAND);
        soma->flags &= ~0x41u;
    }
    if (soma->flags & AOS_FLAG_ANIM_DONE) {
        soma->flags &= 0xFFDFFF9Fu;
        if (!(soma->flags & AOS_FLAG_AIRBORNE) && soma->anim_request == AOS_ANIM_NONE)
        {
            bool low = soma->flags & AOS_FLAG_CROUCH;
            soma->anim_request = soma->weapon.anims[low ? 4 : 3];
            soma->hurtbox = low ? aos_soma_low_box : aos_soma_stand_box;
        }
        soma->state = 0;
    } else if ((pressed & AOS_KEY_ABILITY) && !(held & AOS_KEY_UP) &&
               (soma->moves & AOS_MOVE_BACKDASH) && !(soma->flags & 0x10008402u)) {
        soma->anim_request = AOS_SOMA_ANIM_BACKDASH;
        soma->hurtbox = aos_soma_stand_box;
        soma->flags = (soma->flags & 0xFFDFFF9Fu) | AOS_FLAG_BACKDASH;
        soma->frame_counter = 0;
        soma->vx = soma->facing_left ? 0x3C000 : (int32_t)0xFFFC4000;
        soma->state = 0;
    }
    aos_soma_air(soma, layer, held);
}

/* Case 5 (_0801CBB4): high jump until falling. */
static void high_jump_state(AosSoma *soma, const AosCollision *layer, uint16_t held) {
    if (soma->vy > 0) soma->state = 0;
    aos_soma_air(soma, layer, held);
}

/* Case 7 (_0801CBD4, sub_08017F94): dive kick; a hit (flag 0x80000)
 * bounces Soma back up. */
static void dive_kick_state(AosSoma *soma, const AosCollision *layer) {
    if (soma->flags & AOS_FLAG_SLOW_FALL) {
        soma->state = 0;
    } else if (soma->flags & AOS_FLAG_KICK_HIT) {
        soma->flags = (soma->flags | 8u) & ~4u;
        soma->anim_request = AOS_SOMA_ANIM_AIR_JUMP;
        soma->hurtbox = aos_soma_low_box;
        soma->vy = (int32_t)0xFFFC8000;                 /* -3.5 */
        soma->gravity_mod = (int32_t)0xFFFFE000;
        soma->state = 0;
    } else {
        soma->air_anim_locked = true;
    }
    air_routine(soma, layer);
}

/* sub_0801B0D8 case 4 (_0801C994): hard landing. The slide branch taken
 * with flag 0x80 is not ported. */
static void hard_landing_state(AosSoma *soma) {
    soma->friction = soma->vx >= 0 ? (int32_t)0xFFFFD000 : 0x3000; /* 0.1875 */
    apply_friction(soma);
    if (soma->frame_counter <= 7) soma->frame_counter++;
    if (!(soma->flags & AOS_FLAG_HARD_LANDING)) return;
    soma->flags |= AOS_FLAG_CROUCH;
    if ((soma->flags & (AOS_FLAG_GROUNDED | AOS_FLAG_ANIM_DONE)) != AOS_FLAG_GROUNDED) {
        soma->flags &= 0xFFFEFF7Fu;
        soma->state = 0;
    }
}

AosSoma aos_soma_spawn(int32_t x, int32_t y, const AosAnimSet *anims) {
    AosSoma soma = {
        .x = x, .y = y,
        .flags = AOS_FLAG_GROUNDED | AOS_FLAG_ANIM_DONE,
        .anim_request = AOS_ANIM_NONE,
        .air_probes = {4, -8, -12, -20, -28},
        .hurtbox = {-6, -32, 12, 28},
        .combat = {.type = AOS_TYPE_PLAYER, .attack_off = true},
        .anims = anims,
    };
    soma.wall_probes = aos_soma_stand_probes;
    aos_anim_start(&soma.anim, anims, AOS_SOMA_ANIM_IDLE, true);
    return soma;
}

/* _0801CD80: wall probes for the next frame. */
static void select_wall_probes(AosSoma *soma) {
    if (soma->state == 3 || (soma->flags & AOS_FLAG_HEAD_CEILING)) {
        soma->wall_probes = aos_soma_ceiling_probes;
    } else if (soma->flags & AOS_FLAG_HEAD_SPECIAL) {
        soma->wall_probes = aos_soma_special_probes;
    } else if (!(soma->flags & AOS_FLAG_AIRBORNE)) {
        soma->wall_probes = aos_soma_stand_probes;
    } else {
        int16_t vx_pixels = (int16_t)(soma->vx >> 16), vy_pixels = (int16_t)(soma->vy >> 16);
        int vx_abs = vx_pixels < 0 ? -vx_pixels : vx_pixels;
        int vy_abs = vy_pixels < 0 ? -vy_pixels : vy_pixels;
        int offset = -12;
        if (!soma->slope_contact && (vx_abs <= 2 || vy_pixels > 0))
            offset = -vy_abs / 2 - 4;
        soma->air_probes[1] = (int8_t)offset;
        soma->wall_probes = soma->air_probes;
    }
}

AosLanding aos_soma_update(AosSoma *soma, const AosCollision *layer, uint16_t held,
                           uint16_t pressed) {
    soma->held = held;
    soma->pressed = pressed;
    /* _0801B100 */
    soma->flags &= ~0x00240000u;
    if (soma->anim.flags & AOS_ANIM_ENDED)
        soma->flags = (soma->flags | AOS_FLAG_ANIM_DONE) & ~(uint32_t)AOS_FLAG_BACKDASH;
    aos_soma_integrate(soma);
    AosLanding landing = aos_soma_collide(soma, layer);
    switch (soma->state) {
    case 0: normal_state(soma, layer, held, pressed); break;
    case 1: attack_state(soma, layer, held, pressed); break;
    case 3: slide_state(soma, layer); break;
    case 4: hard_landing_state(soma); break;
    case 5: high_jump_state(soma, layer, held); break;
    case 6:     /* _0801CBC6: ceiling crash, until its animation ends */
        if (soma->flags & AOS_FLAG_ANIM_DONE) soma->state = 0;
        break;
    case 7: dive_kick_state(soma, layer); break;
    default: break;
    }
    select_wall_probes(soma);
    /* _0801CE5C: the pending animation plays once, then is dropped. */
    if (soma->anim_request != AOS_ANIM_NONE) {
        if (soma->anim.id != soma->anim_request) {
            aos_anim_start(&soma->anim, soma->anims, soma->anim_request, false);
            soma->anim.id = (uint8_t)soma->anim_request;
            soma->flags &= ~(uint32_t)AOS_FLAG_ANIM_DONE;
        } else if (soma->flags & AOS_FLAG_ANIM_DONE) {
            soma->anim_request = AOS_ANIM_NONE;
        }
    }
    aos_anim_step(&soma->anim, soma->anims);
    /* _0801CF2C: per-frame flags are dropped at the end of the update. */
    soma->flags &= 0xFBF7FFFFu;
    soma->air_anim_locked = false;
    /* The weapon entity deletes itself once the attack flag is clear. */
    if (!(soma->flags & AOS_FLAG_ATTACKING)) soma->weapon_active = false;
    return landing;
}
