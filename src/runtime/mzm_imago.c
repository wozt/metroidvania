/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Imago (src/sprites_ai/imago.c): the wasp that appears
 * once the last egg is broken, sweeps the room low and climbs back,
 * dives at Samus near either end, shoots needles once hurt (or when Samus
 * has no missiles), loses its stinger when killed and keeps flying while
 * burning until Samus reaches the Super Missile, then crashes through the
 * wall; the event is set once Samus has picked the Super Missiles up. */
#include "mzm_sprite_ai.h"
#include "mzm_blocks.h"
#include "mzm_screen_shake.h"

enum {
    POSE_WAIT_FOR_LAST_EGG = 0x1,
    POSE_SPAWN = 0x2,
    POSE_COMING_DOWN_INIT = 0x8,
    POSE_COMING_DOWN = 0x9,
    POSE_MOVE_HORIZONTALLY = 0x23,
    POSE_GOING_UP = 0x25,
    POSE_ATTACKING_INIT = 0x26,
    POSE_ATTACKING_GOING_DOWN = 0x27,
    POSE_ATTACKING_GOING_UP = 0x29,
    POSE_DYING_INIT = 0x62,
    POSE_CHECK_SAMUS_AT_SUPER_MISSILE = 0x67,
    POSE_CHARGING_THROUGH_WALL = 0x68,
    POSE_DESTROY_WALL = 0x69,
    POSE_DYING = 0x6A,
    POSE_SET_EVENT = 0x6C,

    PART_LEFT_WING_INTERNAL = 0,
    PART_LEFT_WING_EXTERNAL = 1,
    PART_BODY = 2,
    PART_RIGHT_WING_INTERNAL = 3,
    PART_RIGHT_WING_EXTERNAL = 4,
    PART_CORE = 5,
    PART_IMAGO = 6,

    PART_POSE_BODY_SPAWN = 0x1,
    PART_POSE_BODY_UPDATE_PALETTE = 0x8,
    PART_POSE_CORE_SYNC_PALETTE = 0xE,
    PART_POSE_IDLE = 0x42,

    NEEDLE_POSE_MOVING = 0x9,
    NEEDLE_POSE_EXPLODING = 0x42,

    STINGER_POSE_FALLING = 0x9,
    STINGER_POSE_DISAPPEARING = 0x23,

    EGG_POSE_IDLE = 0x9,
    EGG_POSE_BREAKING = 0x23,
    EGG_POSE_DISAPPEARING = 0x25,
    EGG_PART_NORMAL = 0x0,
    EGG_PART_LAST = 0x80,

    STAGE_MOVING_HORIZONTALLY = 0,
    STAGE_MOVING_VERTICALLY = 1,
    STAGE_GO_UP = 2,
};

#define IMAGO_SIZE (BLOCK_SIZE * 4)
#define HEAD_HITBOX BLOCK_SIZE
#define TAIL_HITBOX (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)
#define CLOSE_DISTANCE (BLOCK_SIZE * 8 - QUARTER_BLOCK_SIZE + PIXEL_SIZE)
#define NEAR_DISTANCE (BLOCK_SIZE * 4 - QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE + PIXEL_SIZE / 2)
#define BODY_HITBOX_LEFT (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)
#define BODY_HITBOX_RIGHT (BLOCK_SIZE * 3)
#define ATTACKING_X_ENTRIES 49                         /* sImagoAttackingXVelocity */
#define SHOOTING_NEEDLES_LAST_FRAME 9                  /* sImagoOam_ShootingNeedles */

static MzmSubSpriteState *state_of(const Ctx *c) { return &c->f->globals->sub1; }
static uint16_t samus_y(const Ctx *c) { return (uint16_t)(c->f->samus_y - 1); }
static bool flipped(const MzmSprite *s) { return s->status & MZM_SPRITE_STATUS_X_FLIP; }

static MzmSprite *owner_of(const Ctx *c) {
    const MzmSprite *s = c->s;
    return c->f->sprites && s->ram_slot < c->f->sprite_count ? &c->f->sprites[s->ram_slot] : NULL;
}

static uint8_t stun_row(const MzmSprite *s) {
    return (uint8_t)(14 - s->gfx_slot - s->frozen_row_offset);
}

/* Multi-sprite helpers (SpriteUtilUpdateSubSprite1Anim and
 * SpriteUtilHasSubSprite1AnimationEnded). */
static const MzmMultiSprite *multi(const Ctx *c) {
    uint8_t id = state_of(c)->multi;
    return &c->tables->imago_adult_multi[id < MZM_IMAGO_ADULT_MULTI_COUNT ? id : MZM_IMAGO_ADULT_MULTI_IDLE];
}

static void set_multi(Ctx *c, uint8_t id) {
    MzmSubSpriteState *st = state_of(c);
    st->multi = id;
    st->frame = 0;
    st->adc = 0;
}

static void update_multi(Ctx *c) {
    MzmSubSpriteState *st = state_of(c);
    const MzmMultiSprite *m = multi(c);
    if (st->frame >= MZM_MULTI_FRAMES) st->frame = 0;
    st->adc++;
    if (m->timers[st->frame] < st->adc) {
        st->adc = 1;
        st->frame++;
        if (st->frame >= MZM_MULTI_FRAMES || !m->timers[st->frame]) st->frame = 0;
    }
}

static bool multi_ended(const Ctx *c) {
    const MzmSubSpriteState *st = state_of(c);
    const MzmMultiSprite *m = multi(c);
    unsigned next = st->frame + 1u;
    return m->timers[st->frame] <= st->adc && (next >= MZM_MULTI_FRAMES || !m->timers[next]);
}

/* ImagoSyncSubSprites: the table's part, the offset mirrored with X_FLIP. */
static void sync_sub_sprites(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSubSpriteState *st = state_of(c);
    uint8_t frame = st->frame < MZM_MULTI_FRAMES ? st->frame : 0;
    const int16_t *part = multi(c)->parts[frame][s->room_slot < MZM_MULTI_PARTS ? s->room_slot : 0];
    if (part[0] >= 0 && part[0] < MZM_IMAGO_ANIMS && s->anim != part[0]) ai_set_anim(s, (uint8_t)part[0]);
    s->y = (uint16_t)(st->y + part[1]);
    s->x = flipped(s) ? (uint16_t)(st->x - part[2]) : (uint16_t)(st->x + part[2]);
}

static void apply_caa(const Ctx *c, uint16_t y, uint16_t x) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->apply_caa) h->apply_caa(h->context, y, x, MZM_CAA_REMOVE_SOLID);
}

static void door_timer(const Ctx *c, int8_t timer) {
    if (c->f->hooks && c->f->hooks->door_unlock_timer)
        c->f->hooks->door_unlock_timer(c->f->hooks->context, timer);
}

static int missiles_left(const Ctx *c) {
    const MzmEquipment *eq = c->f->equipment;
    return eq ? eq->missiles + eq->super_missiles : 0;
}

/* ImagoShootNeedles: once under two thirds (or Samus out of missiles) with
 * her ahead, a needle per shooting cycle, faster under a third. */
static void shoot_needles(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *st = state_of(c);
    uint16_t health = s->health;
    if (health == 0) return;
    if (st->multi == MZM_IMAGO_ADULT_MULTI_IDLE) {
        bool in_range = flipped(s) ? st->x < c->f->samus_x : st->x > c->f->samus_x;
        if ((s->health < st->health * 2 / 3 || missiles_left(c) == 0) && in_range)
            set_multi(c, MZM_IMAGO_ADULT_MULTI_SHOOTING_NEEDLES);
    } else if (st->multi == MZM_IMAGO_ADULT_MULTI_SHOOTING_NEEDLES) {
        if (s->frame == SHOOTING_NEEDLES_LAST_FRAME && s->adc == 1) {
            uint16_t y = (uint16_t)(s->y + (BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE));
            if (flipped(s))
                ai_spawn_secondary(c, MZM_SSPRITE_IMAGO_NEEDLE, s->room_slot, s->gfx_slot, y,
                                   (uint16_t)(s->x + (HALF_BLOCK_SIZE - PIXEL_SIZE)), MZM_SPRITE_STATUS_X_FLIP);
            else
                ai_spawn_secondary(c, MZM_SSPRITE_IMAGO_NEEDLE, s->room_slot, s->gfx_slot, y,
                                   (uint16_t)(s->x - HALF_BLOCK_SIZE), 0);
        }
        if (s->health < st->health / 3) s->adc = (uint8_t)(s->adc + 2);
        if (multi_ended(c)) set_multi(c, MZM_IMAGO_ADULT_MULTI_RECHARGING_NEEDLES);
    } else if (st->multi == MZM_IMAGO_ADULT_MULTI_RECHARGING_NEEDLES) {
        if (health < st->health / 3) s->adc = (uint8_t)(s->adc + 2);
        if (multi_ended(c)) set_multi(c, MZM_IMAGO_ADULT_MULTI_IDLE);
    }
}

/* ImagoCoreFlashingAnim: sImagoDynamicPaletteData's rows and delays. */
static void core_flashing(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->isft & 0x7F) return;
    if (s->scaling) {
        s->scaling--;
        return;
    }
    uint8_t offset = (uint8_t)s->rotation++;
    uint16_t entry = (uint16_t)c->tables->imago_core_palette[offset < 9 ? offset : 8];
    uint8_t row = entry & 0xFF;
    if (row == 0x80) {
        s->rotation = 1;
        entry = (uint16_t)c->tables->imago_core_palette[0];
        row = entry & 0xFF;
    }
    s->absolute_palette_row = s->palette_row = row;
    s->scaling = entry >> 8;
}

/* ImagoSetSidesHitbox: the head ahead, the tail behind. */
static void set_sides_hitbox(MzmSprite *s) {
    s->hitbox_left = (int16_t)(flipped(s) ? -HEAD_HITBOX : -TAIL_HITBOX);
    s->hitbox_right = (int16_t)(flipped(s) ? TAIL_HITBOX : HEAD_HITBOX);
}

/* ImagoInit: in the ceiling six blocks up, its parts and five eggs. */
static void init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *st = state_of(c);
    if (ai_check_event(c, MZM_EVENT_IMAGO_KILLED)) {
        s->status = 0;
        return;
    }
    door_timer(c, 1);
    st->work4 = c->f->equipment ? (uint8_t)c->f->equipment->max_super_missiles : 0;
    s->y = (uint16_t)(s->y - BLOCK_SIZE * 6);
    st->y = s->y_spawn = s->y;
    st->x = s->x_spawn = s->x;
    s->status |= MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->draw_top = BLOCK_SIZE * 2 / PIXEL_SIZE;
    s->draw_bottom = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE) / PIXEL_SIZE;
    s->hitbox_top = -PIXEL_SIZE;
    s->hitbox_bottom = BLOCK_SIZE * 2;
    set_sides_hitbox(s);
    s->frozen_row_offset = 1;
    s->scaling = 0;
    s->rotation = 0;
    s->samus_collision = SSC_HURTS_BIG_KNOCKBACK;
    s->work0 = 80;                                     /* CONVERT_SECONDS(1.f) + ONE_THIRD_SECOND */
    s->health = st->health = c->tables->health[s->sprite_id];
    set_multi(c, MZM_IMAGO_ADULT_MULTI_IDLE);
    st->work3 = 0;                                     /* last egg destroyed */
    st->work2 = 0;
    s->pose = POSE_WAIT_FOR_LAST_EGG;
    s->draw_order = 5;
    s->room_slot = PART_IMAGO;
    uint16_t y = st->y, x = st->x, flip = s->status & MZM_SPRITE_STATUS_X_FLIP;
    static const uint8_t parts[6] = {PART_LEFT_WING_INTERNAL, PART_LEFT_WING_EXTERNAL, PART_BODY,
                                     PART_RIGHT_WING_INTERNAL, PART_RIGHT_WING_EXTERNAL, PART_CORE};
    for (int i = 0; i < 6; ++i) ai_spawn_secondary(c, MZM_SSPRITE_IMAGO_PART, parts[i], s->gfx_slot, y, x, flip);
    static const uint8_t eggs[5][3] = {{EGG_PART_LAST, 14, 3}, {EGG_PART_NORMAL, 15, 7},
                                       {EGG_PART_NORMAL, 16, 12}, {EGG_PART_NORMAL, 18, 22},
                                       {EGG_PART_NORMAL, 22, 42}};
    for (int i = 0; i < 5; ++i)
        ai_spawn_secondary(c, MZM_SSPRITE_IMAGO_EGG, eggs[i][0], s->gfx_slot,
                           (uint16_t)(y + BLOCK_SIZE * eggs[i][1]), (uint16_t)(x + BLOCK_SIZE * eggs[i][2]), 0);
}

/* ImagoComingDownInit: the base speed by health; dead and heading left,
 * it waits for Samus at the Super Missile instead. */
static void coming_down_init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *st = state_of(c);
    if (s->health == 0) {
        if (!flipped(s)) {
            s->pose = POSE_CHECK_SAMUS_AT_SUPER_MISSILE;
            return;
        }
    } else {
        set_multi(c, MZM_IMAGO_ADULT_MULTI_IDLE);
    }
    s->work0 = s->work1 = 0;
    s->work2 = !flipped(s) && s->health < st->health * 2 / 3 ? EIGHTH_BLOCK_SIZE : 0;
    s->work3 = 0;
    s->pose = POSE_COMING_DOWN;
    set_sides_hitbox(s);
}

/* ImagoComingDown: down to the floor, drifting right when flipped. */
static void coming_down(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *st = state_of(c);
    if ((s->work0++ & 15) == 0 && s->work2 < QUARTER_BLOCK_SIZE - PIXEL_SIZE) s->work2++;
    if (flipped(s)) st->x = (uint16_t)(st->x + s->work2);
    if (st->y + IMAGO_SIZE > s->y_spawn + BLOCK_SIZE * 14) {
        uint16_t top = ai_check_vertical_slopes(c, (uint16_t)(st->y + IMAGO_SIZE), st->x);
        if (c->vertical != MZM_COLLISION_AIR) {
            st->y = (uint16_t)(top - IMAGO_SIZE);
            s->pose = POSE_MOVE_HORIZONTALLY;
            return;
        }
    }
    st->y = (uint16_t)(st->y + (flipped(s) ? QUARTER_BLOCK_SIZE - PIXEL_SIZE : QUARTER_BLOCK_SIZE + PIXEL_SIZE));
}

/* ImagoMoveHorizontally: the low sweep, faster when hurt, rising near the
 * far end, hovering over the floor and its slopes. */
static void move_horizontally(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *st = state_of(c);
    shoot_needles(c);
    uint8_t stage = STAGE_MOVING_HORIZONTALLY, y_mask;
    if (flipped(s)) {
        if ((s->work0++ & 15) == 0 && s->work2 < QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE) s->work2++;
        y_mask = 1;
        if (st->x > s->x_spawn + BLOCK_SIZE * 46) {
            stage = STAGE_GO_UP;
        } else {
            if (st->x > s->x_spawn + BLOCK_SIZE * 36) stage = STAGE_MOVING_VERTICALLY;
            st->x = (uint16_t)(st->x + s->work2);
        }
    } else {
        uint8_t every, cap;
        uint16_t rise;
        if (s->health < st->health / 3) {
            every = 7; cap = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE; rise = BLOCK_SIZE * 22;
        } else if (s->health < st->health * 2 / 3) {
            every = 15; cap = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE; rise = BLOCK_SIZE * 16;
        } else {
            every = 15; cap = QUARTER_BLOCK_SIZE; rise = BLOCK_SIZE * 11;
        }
        if ((s->work0++ & every) == 0 && s->work2 < cap) s->work2++;
        y_mask = 3;
        if (st->x < s->x_spawn) {
            stage = STAGE_GO_UP;
        } else {
            if (st->x < s->x_spawn + rise) stage = STAGE_MOVING_VERTICALLY;
            st->x = (uint16_t)(st->x - s->work2);
        }
    }
    if (stage != STAGE_MOVING_HORIZONTALLY) {
        if (!(y_mask & s->work1++) && s->work3 < QUARTER_BLOCK_SIZE - PIXEL_SIZE) s->work3++;
        st->y = (uint16_t)(st->y - s->work3);
        if (stage == STAGE_GO_UP) {
            s->pose = POSE_GOING_UP;
            return;
        }
    }
    uint16_t y = (uint16_t)(st->y + IMAGO_SIZE), x = st->x;
    uint16_t top = ai_check_vertical(c, (uint16_t)(y - PIXEL_SIZE), x);
    if ((c->vertical & MZM_COLLISION_FLAGS_F) >= MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE) {
        st->y = (uint16_t)(top - IMAGO_SIZE);
        return;
    }
    top = ai_check_vertical(c, y, x);
    if ((c->vertical & MZM_COLLISION_FLAGS_F) >= MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE) {
        st->y = (uint16_t)(top - IMAGO_SIZE);
        return;
    }
    top = ai_check_vertical(c, (uint16_t)(y + PIXEL_SIZE), x);
    if (c->vertical != MZM_COLLISION_AIR) {
        st->y = (uint16_t)(top - IMAGO_SIZE);
        return;
    }
    if (flipped(s)) {
        ai_check_vertical(c, y, (uint16_t)(x - QUARTER_BLOCK_SIZE));
        if (c->vertical == MZM_COLLISION_LEFT_STEEP_FLOOR_SLOPE || c->vertical == MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE)
            st->y = (uint16_t)(ai_check_vertical(c, (uint16_t)(y + BLOCK_SIZE), x) - IMAGO_SIZE);
    } else {
        ai_check_vertical(c, y, (uint16_t)(x + QUARTER_BLOCK_SIZE));
        if (c->vertical == MZM_COLLISION_RIGHT_STEEP_FLOOR_SLOPE || c->vertical == MZM_COLLISION_RIGHT_SLIGHT_FLOOR_SLOPE)
            st->y = (uint16_t)(ai_check_vertical(c, (uint16_t)(y + BLOCK_SIZE), x) - IMAGO_SIZE);
    }
}

/* At the ceiling: dive at Samus near this end, else turn and come down. */
static void at_ceiling(Ctx *c, bool attacking_again) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *st = state_of(c);
    st->y = s->y_spawn;
    if (flipped(s)) {
        if (!attacking_again) st->x = (uint16_t)(s->x_spawn + BLOCK_SIZE * 48);
        if (st->x < c->f->samus_x && s->health != 0) {
            if (!attacking_again) {
                st->x = (uint16_t)(s->x_spawn + BLOCK_SIZE * 46);
                s->work2 = 0;
                s->pose = POSE_ATTACKING_INIT;
            } else {
                s->pose = POSE_ATTACKING_GOING_DOWN;
            }
            return;
        }
        if (attacking_again) st->x = (uint16_t)(s->x_spawn + BLOCK_SIZE * 48);
    } else {
        if (!attacking_again) st->x = s->x_spawn;
        if (st->x + BLOCK_SIZE * 2 > c->f->samus_x && s->health != 0) {
            if (!attacking_again) {
                s->work2 = ATTACKING_X_ENTRIES / 2;
                s->pose = POSE_ATTACKING_INIT;
            } else {
                s->pose = POSE_ATTACKING_GOING_DOWN;
            }
            return;
        }
        if (attacking_again) st->x = s->x_spawn;
    }
    s->status ^= MZM_SPRITE_STATUS_X_FLIP;
    s->pose = POSE_COMING_DOWN_INIT;
}

/* ImagoGoingUp */
static void going_up(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *st = state_of(c);
    if ((s->work1++ & 3) == 0 && s->work3 < QUARTER_BLOCK_SIZE - PIXEL_SIZE) s->work3++;
    if (st->y < s->y_spawn) at_ceiling(c, false);
    else st->y = (uint16_t)(st->y - s->work3);
}

/* The dive's sway along sImagoAttackingXVelocity (doubled). */
static void attack_sway(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t offset = s->work2;
    int16_t movement = offset < ATTACKING_X_ENTRIES ? c->tables->imago_attacking_x[offset] : SHORT_MAX;
    if (movement == SHORT_MAX) {
        movement = c->tables->imago_attacking_x[0];
        offset = 0;
    }
    s->work2 = (uint8_t)(offset + 1);
    state_of(c)->x = (uint16_t)(state_of(c)->x + movement * 2);
}

/* ImagoAttackingGoingDown */
static void attacking_going_down(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *st = state_of(c);
    attack_sway(c);
    if (st->y + IMAGO_SIZE > s->y_spawn + BLOCK_SIZE * 14) {
        uint16_t reach = flipped(s) ? BLOCK_SIZE * 2 + HALF_BLOCK_SIZE : IMAGO_SIZE;
        uint16_t top = ai_check_vertical_slopes(c, (uint16_t)(st->y + reach), st->x);
        if (c->vertical != MZM_COLLISION_AIR) {
            st->y = (uint16_t)(top - reach);
            s->pose = POSE_ATTACKING_GOING_UP;
            s->work3 = QUARTER_BLOCK_SIZE - PIXEL_SIZE;
            return;
        }
    }
    st->y = (uint16_t)(st->y + QUARTER_BLOCK_SIZE + PIXEL_SIZE);
}

/* ImagoAttackingGoingUp */
static void attacking_going_up(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *st = state_of(c);
    attack_sway(c);
    if (st->y < s->y_spawn) at_ceiling(c, true);
    else st->y = (uint16_t)(st->y - (QUARTER_BLOCK_SIZE + PIXEL_SIZE));
}

/* ImagoDyingInit: the stinger drops, the dying wings, and back to the
 * pose it was in. */
static void dying_init(Ctx *c) {
    MzmSprite *s = c->s;
    if (flipped(s))
        ai_spawn_secondary(c, MZM_SSPRITE_IMAGO_DAMAGED_STINGER, s->room_slot, s->gfx_slot,
                           (uint16_t)(s->y + HALF_BLOCK_SIZE), (uint16_t)(s->x + (HALF_BLOCK_SIZE - PIXEL_SIZE)),
                           MZM_SPRITE_STATUS_X_FLIP);
    else
        ai_spawn_secondary(c, MZM_SSPRITE_IMAGO_DAMAGED_STINGER, s->room_slot, s->gfx_slot,
                           (uint16_t)(s->y + HALF_BLOCK_SIZE), (uint16_t)(s->x - HALF_BLOCK_SIZE), 0);
    set_multi(c, MZM_IMAGO_ADULT_MULTI_DYING);
    s->hitbox_bottom = 0;
    s->samus_collision = SSC_NONE;
    s->pose = state_of(c)->work2;
}

/* ImagoCheckSamusAtSuperMissile */
static void check_samus_at_super_missile(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *st = state_of(c);
    uint16_t sx = c->f->samus_x;
    uint16_t distance = sx > s->x_spawn ? (uint16_t)(sx - s->x_spawn) : (uint16_t)(s->x_spawn - sx);
    if (distance >= BLOCK_SIZE * 3) return;
    s->pose = POSE_CHARGING_THROUGH_WALL;
    s->draw_order = 12;
    st->y = (uint16_t)(s->y_spawn + BLOCK_SIZE * 11 + HALF_BLOCK_SIZE);
    st->x = (uint16_t)(s->x_spawn + BLOCK_SIZE * 16);
}

/* Remove a column of wall blocks (rows in blocks below `y`). */
static void wall_column(Ctx *c, uint16_t y, uint16_t x, int first, int last) {
    for (int row = first; row <= last; ++row) apply_caa(c, (uint16_t)(y + BLOCK_SIZE * row), x);
}

/* ImagoChargeThroughWall */
static void charge_through_wall(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *st = state_of(c);
    uint16_t y = (uint16_t)(s->y_spawn + BLOCK_SIZE * 5 + HALF_BLOCK_SIZE), x = s->x_spawn;
    if (ai_collision_at(c, st->y, (uint16_t)(st->x - BLOCK_SIZE * 3)) == MZM_COLLISION_AIR) {
        st->x = (uint16_t)(st->x - QUARTER_BLOCK_SIZE);
        return;
    }
    mzm_screen_shake_start_horizontal(30, 0x81);
    s->pose = POSE_DESTROY_WALL;
    s->work0 = 0;
    wall_column(c, y, (uint16_t)(x + BLOCK_SIZE * 2), 6, 8);
    ai_particle_set(c, (uint16_t)(y + BLOCK_SIZE * 8), (uint16_t)(x + BLOCK_SIZE * 2), MZM_PE_SPRITE_EXPLOSION_HUGE);
    wall_column(c, y, (uint16_t)(x + BLOCK_SIZE), 5, 8);
    ai_particle_set(c, (uint16_t)(y + BLOCK_SIZE * 7), (uint16_t)(x + BLOCK_SIZE), MZM_PE_SPRITE_EXPLOSION_HUGE);
    mzm_sfx_play(MZM_SOUND_IMAGO_DESTROYING_WALL);
}

/* ImagoDestroyWall: the wall broken column by column. */
static void destroy_wall(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *st = state_of(c);
    uint16_t y = (uint16_t)(s->y_spawn + BLOCK_SIZE * 5 + HALF_BLOCK_SIZE), x = s->x_spawn;
#define AT(dx) ((uint16_t)(x - BLOCK_SIZE * (dx)))
#define BELOW(rows) ((uint16_t)(y + BLOCK_SIZE * (rows)))
    switch (s->work0++) {
    case 0:
        wall_column(c, y, x, 5, 7);
        ai_particle_set(c, BELOW(7), x, MZM_PE_SPRITE_EXPLOSION_HUGE);
        wall_column(c, y, AT(1), 4, 7);
        ai_particle_set(c, BELOW(6), AT(1), MZM_PE_SPRITE_EXPLOSION_HUGE);
        ai_particle_set(c, (uint16_t)(y + BLOCK_SIZE * 4 + QUARTER_BLOCK_SIZE), (uint16_t)(x - (BLOCK_SIZE + 8)),
                        MZM_PE_SPRITE_EXPLOSION_HUGE);
        break;
    case 8:                                            /* CONVERT_SECONDS(.1f + 1.f / 30) */
        wall_column(c, y, AT(2), 4, 7);
        ai_particle_set(c, BELOW(7), AT(2), MZM_PE_SPRITE_EXPLOSION_HUGE);
        wall_column(c, y, AT(3), 4, 8);
        ai_particle_set(c, BELOW(6), AT(3), MZM_PE_SPRITE_EXPLOSION_HUGE);
        break;
    case 16:                                           /* CONVERT_SECONDS(.25f + 1.f / 60) */
        wall_column(c, y, AT(4), 5, 6);
        ai_particle_set(c, BELOW(8), AT(4), MZM_PE_SPRITE_EXPLOSION_HUGE);
        wall_column(c, y, AT(5), 5, 8);
        ai_particle_set(c, BELOW(6), AT(5), MZM_PE_SPRITE_EXPLOSION_HUGE);
        break;
    case 24:                                           /* CONVERT_SECONDS(.4f) */
        wall_column(c, y, AT(6), 5, 8);
        ai_particle_set(c, BELOW(6), AT(6), MZM_PE_SPRITE_EXPLOSION_HUGE);
        wall_column(c, y, AT(7), 6, 8);
        ai_particle_set(c, BELOW(8), AT(8), MZM_PE_SPRITE_EXPLOSION_HUGE);
        break;
    case 28:                                           /* CONVERT_SECONDS(.45f + 1.f / 60) */
        s->pose = POSE_DYING;
        s->work0 = 0;
        mzm_screen_shake_start_vertical(30, 0x81);
        mzm_screen_shake_start_horizontal(60, 0x81);
        mzm_sfx_music_fade(56);                        /* CONVERT_SECONDS(14.f / 15) */
        break;
    default: break;
    }
#undef AT
#undef BELOW
    st->x = (uint16_t)(st->x - QUARTER_BLOCK_SIZE);
    st->y = (uint16_t)(st->y + PIXEL_SIZE / 2);
}

/* ImagoDying: dust and explosions, then the boss-killed music. */
static void dying(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *st = state_of(c);
    if ((s->work0 & 15) == 0) {
        if (s->work0 & 16)
            ai_particle_set(c, (uint16_t)(st->y + BLOCK_SIZE + HALF_BLOCK_SIZE),
                            (uint16_t)(st->x - (BLOCK_SIZE + PIXEL_SIZE + PIXEL_SIZE / 2)), MZM_PE_TWO_MEDIUM_DUST);
        else
            ai_particle_set(c, (uint16_t)(st->y + BLOCK_SIZE + EIGHTH_BLOCK_SIZE),
                            (uint16_t)(st->x - (THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE / 2)), MZM_PE_MEDIUM_DUST);
    }
    switch (s->work0++) {
    case 0: ai_particle_set(c, st->y, st->x, MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG); break;
    case 9:
        ai_particle_set(c, (uint16_t)(st->y + HALF_BLOCK_SIZE), (uint16_t)(st->x - BLOCK_SIZE * 2),
                        MZM_PE_SPRITE_EXPLOSION_HUGE);
        break;
    case 18:
        ai_particle_set(c, (uint16_t)(st->y - HALF_BLOCK_SIZE), (uint16_t)(st->x + HALF_BLOCK_SIZE),
                        MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        break;
    case 27:
        ai_particle_set(c, (uint16_t)(st->y - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE)), (uint16_t)(st->x - BLOCK_SIZE),
                        MZM_PE_SPRITE_EXPLOSION_BIG);
        break;
    case 36:
        ai_particle_set(c, (uint16_t)(st->y - HALF_BLOCK_SIZE), (uint16_t)(st->x - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE)),
                        MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        break;
    case 45:
        ai_particle_set(c, (uint16_t)(st->y - THREE_QUARTER_BLOCK_SIZE),
                        (uint16_t)(st->x - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE)), MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        break;
    case 54:
        ai_particle_set(c, (uint16_t)(st->y + QUARTER_BLOCK_SIZE),
                        (uint16_t)(st->x - (BLOCK_SIZE * 3 - QUARTER_BLOCK_SIZE - PIXEL_SIZE)), MZM_PE_SPRITE_EXPLOSION_HUGE);
        s->pose = POSE_SET_EVENT;
        s->status |= MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        mzm_sfx_play(MZM_SOUND_IMAGO_DYING);
        mzm_sfx_music_play(MZM_MUSIC_BOSS_KILLED, 0);
        break;
    default: break;
    }
}

/* ImagoSetEvent: once Samus has more Super Missiles than at the start. */
static void set_event(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    const MzmEquipment *eq = c->f->equipment;
    if (!eq || eq->max_super_missiles <= state_of(c)->work4) return;
    door_timer(c, -60);
    s->status = 0;
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->set_event) h->set_event(h->context, MZM_EVENT_IMAGO_KILLED);
}

/* The buzzing (and burning) sound by distance and side, every 16 frames. */
static void buzzing(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *st = state_of(c);
    if ((c->frame8 & 15) != 0) return;
    if ((uint8_t)(s->pose - 8) >= POSE_DESTROY_WALL - 8) return;
    uint16_t sy = samus_y(c), sx = c->f->samus_x;
    uint16_t dy = st->y > sy ? (uint16_t)(st->y - sy) : (uint16_t)(sy - st->y);
    bool right = st->x > sx;
    uint16_t dx = right ? (uint16_t)(st->x - sx) : (uint16_t)(sx - st->x);
    int distance = !(s->status & MZM_SPRITE_STATUS_ONSCREEN) || dy > CLOSE_DISTANCE ? 0 : dx < NEAR_DISTANCE ? 2 : 1;
    static const uint16_t buzz[2][3] = {
        {MZM_SOUND_IMAGO_BUZZING_FAR_LEFT, MZM_SOUND_IMAGO_BUZZING_NEAR_LEFT, MZM_SOUND_IMAGO_BUZZING_CLOSE_LEFT},
        {MZM_SOUND_IMAGO_BUZZING_FAR_RIGHT, MZM_SOUND_IMAGO_BUZZING_NEAR_RIGHT, MZM_SOUND_IMAGO_BUZZING_CLOSE_RIGHT},
    };
    static const uint16_t burn[2][3] = {
        {MZM_SOUND_IMAGO_BURNING_FAR_LEFT, MZM_SOUND_IMAGO_BURNING_NEAR_LEFT, MZM_SOUND_IMAGO_BURNING_CLOSE_LEFT},
        {MZM_SOUND_IMAGO_BURNING_FAR_RIGHT, MZM_SOUND_IMAGO_BURNING_NEAR_RIGHT, MZM_SOUND_IMAGO_BURNING_CLOSE_RIGHT},
    };
    mzm_sfx_play(buzz[right][distance]);
    if (s->health == 0) mzm_sfx_play(burn[right][distance]);
}

/* Imago */
static void imago(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *st = state_of(c);
    if (s->pose < POSE_DYING_INIT && s->damaged) {
        s->damaged = false;
        ai_sound_onscreen_once(s, MZM_SOUND_IMAGO_DAMAGED);
    }
    buzzing(c);
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: init(c); break;
    case POSE_WAIT_FOR_LAST_EGG:
        if (st->work3) {
            s->pose = POSE_SPAWN;
            mzm_sfx_music_fade(36);                    /* CONVERT_SECONDS(3.f / 5) */
        }
        break;
    case POSE_SPAWN:
        if (--s->work0 == 0) {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
            s->pose = POSE_COMING_DOWN_INIT;
            mzm_sfx_music_play(MZM_MUSIC_IMAGO_BATTLE, 0);
        }
        break;
    case POSE_COMING_DOWN_INIT: coming_down_init(c); break;
    case POSE_COMING_DOWN: coming_down(c); break;
    case POSE_MOVE_HORIZONTALLY: move_horizontally(c); break;
    case POSE_GOING_UP: going_up(c); break;
    case POSE_ATTACKING_INIT:
        set_multi(c, MZM_IMAGO_ADULT_MULTI_IDLE);
        s->pose = POSE_ATTACKING_GOING_DOWN;
        /* fallthrough */
    case POSE_ATTACKING_GOING_DOWN: attacking_going_down(c); break;
    case POSE_ATTACKING_GOING_UP: attacking_going_up(c); break;
    case POSE_DYING_INIT: dying_init(c); break;
    case POSE_CHECK_SAMUS_AT_SUPER_MISSILE: check_samus_at_super_missile(c); break;
    case POSE_CHARGING_THROUGH_WALL: charge_through_wall(c); break;
    case POSE_DESTROY_WALL: destroy_wall(c); break;
    case POSE_DYING: dying(c); break;
    case POSE_SET_EVENT: set_event(c); break;
    default: break;
    }
    if (s->pose > POSE_DYING || !s->status) return;
    st->work2 = s->pose;
    if (s->health == 0 && s->pose <= POSE_CHARGING_THROUGH_WALL) {
        /* Burning: explosions behind it and the stun flash. */
        if ((c->frame8 & 3) == 0) {
            ai_particle_set(c, st->y, (uint16_t)(flipped(s) ? st->x - BLOCK_SIZE : st->x + BLOCK_SIZE),
                            MZM_PE_SPRITE_EXPLOSION_MEDIUM);
            if (s->pose <= POSE_CHECK_SAMUS_AT_SUPER_MISSILE)
                s->palette_row = c->frame8 & 4 ? stun_row(s) : 0;
            else
                s->palette_row = 0;
        }
    } else {
        core_flashing(c);
    }
    update_multi(c);
    sync_sub_sprites(c);
}

/* ImagoPartInit */
static void part_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->pose = PART_POSE_IDLE;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
    s->samus_collision = SSC_NONE;
    switch (s->room_slot) {
    case PART_LEFT_WING_INTERNAL:
    case PART_LEFT_WING_EXTERNAL:
        s->draw_top = (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_order = 2;
        break;
    case PART_BODY:
        s->draw_top = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_bottom = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->draw_horizontal = (BLOCK_SIZE * 4 - QUARTER_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = -(BLOCK_SIZE * 2);
        s->hitbox_bottom = 0;
        s->hitbox_left = (int16_t)(flipped(s) ? -BODY_HITBOX_LEFT : -BODY_HITBOX_RIGHT);
        s->hitbox_right = (int16_t)(flipped(s) ? BODY_HITBOX_RIGHT : BODY_HITBOX_LEFT);
        s->draw_order = 3;
        s->immune_to_projectiles = true;
        s->samus_collision = SSC_HURTS_SAMUS;
        s->health = 1;
        s->pose = PART_POSE_BODY_SPAWN;
        break;
    case PART_RIGHT_WING_INTERNAL:
    case PART_RIGHT_WING_EXTERNAL:
        s->draw_top = (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = (BLOCK_SIZE * 4 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        break;
    case PART_CORE:
        s->draw_top = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->draw_bottom = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->frozen_row_offset = 1;
        s->pose = PART_POSE_CORE_SYNC_PALETTE;
        break;
    default: s->status = 0; break;
    }
}

/* ImagoPart: wings, body (health rows) and core (Imago's flashing). */
static void part(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSprite *owner = owner_of(c);
    if (!owner) {
        s->status = 0;
        return;
    }
    if (flipped(owner)) s->status |= MZM_SPRITE_STATUS_X_FLIP;
    else s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
    if (owner->health == 0) {
        s->ignore_samus_timer = 1;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        if (owner->pose >= POSE_CHARGING_THROUGH_WALL) {
            static const uint8_t orders[6] = {9, 9, 10, 11, 11, 11};
            if (s->room_slot < 6) s->draw_order = orders[s->room_slot];
        }
    }
    if (owner->pose > POSE_DYING) {
        MzmSubSpriteState *st = state_of(c);
        static const struct { int16_t dy, dx; uint8_t effect; } deaths[6] = {
            {-QUARTER_BLOCK_SIZE, BLOCK_SIZE + EIGHTH_BLOCK_SIZE, MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG},
            {-(BLOCK_SIZE + HALF_BLOCK_SIZE + PIXEL_SIZE), 0, MZM_PE_SPRITE_EXPLOSION_MEDIUM},
            {THREE_QUARTER_BLOCK_SIZE, -(HALF_BLOCK_SIZE - PIXEL_SIZE), MZM_PE_SPRITE_EXPLOSION_BIG},
            {-THREE_QUARTER_BLOCK_SIZE, -(BLOCK_SIZE + QUARTER_BLOCK_SIZE), MZM_PE_SPRITE_EXPLOSION_MEDIUM},
            {HALF_BLOCK_SIZE, -(BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE), MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG},
            {-BLOCK_SIZE, -(BLOCK_SIZE * 2 + QUARTER_BLOCK_SIZE + PIXEL_SIZE + PIXEL_SIZE / 2), MZM_PE_SPRITE_EXPLOSION_MEDIUM},
        };
        if (s->room_slot < 6)
            ai_sprite_death_full(c, DEATH_NORMAL, (uint16_t)(st->y + deaths[s->room_slot].dy),
                                 (uint16_t)(st->x + deaths[s->room_slot].dx), false, deaths[s->room_slot].effect);
        else
            s->status = 0;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: part_init(c); break;
    case PART_POSE_BODY_SPAWN:
        if (owner->pose >= POSE_COMING_DOWN_INIT) {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
            s->pose = PART_POSE_BODY_UPDATE_PALETTE;
        }
        break;
    case PART_POSE_BODY_UPDATE_PALETTE: {
        uint16_t max = state_of(c)->health;
        if (owner->health < max / 3) s->absolute_palette_row = s->palette_row = 5;
        else if (owner->health < max * 2 / 3) s->absolute_palette_row = s->palette_row = 4;
        s->hitbox_left = (int16_t)(flipped(s) ? -BODY_HITBOX_LEFT : -BODY_HITBOX_RIGHT);
        s->hitbox_right = (int16_t)(flipped(s) ? BODY_HITBOX_RIGHT : BODY_HITBOX_LEFT);
        break;
    }
    case PART_POSE_CORE_SYNC_PALETTE:
        s->palette_row = owner->palette_row;
        s->absolute_palette_row = owner->absolute_palette_row;
        break;
    default: break;
    }
    if (s->status & MZM_SPRITE_STATUS_EXISTS) sync_sub_sprites(c);
}

/* ImagoNeedle: a diagonal shot down that bursts on walls. */
static void needle(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -(QUARTER_BLOCK_SIZE - PIXEL_SIZE);
        s->hitbox_bottom = s->hitbox_right = QUARTER_BLOCK_SIZE - PIXEL_SIZE;
        s->health = c->tables->secondary_health[s->sprite_id];
        ai_set_anim(s, MZM_IMAGO_NEEDLE);
        s->samus_collision = SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT;
        s->pose = NEEDLE_POSE_MOVING;
        mzm_sfx_play(MZM_SOUND_IMAGO_NEEDLE_SHOT);
        break;
    case NEEDLE_POSE_MOVING:
        s->x = (uint16_t)(flipped(s) ? s->x + (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)
                                     : s->x - (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE));
        s->y = (uint16_t)(s->y + (QUARTER_BLOCK_SIZE - PIXEL_SIZE));
        if (ai_collision_at(c, s->y, s->x) != MZM_COLLISION_AIR) s->pose = NEEDLE_POSE_EXPLODING;
        break;
    case NEEDLE_POSE_EXPLODING:
        ai_particle_set(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_SMALL);
        mzm_sfx_play(MZM_SOUND_SPRITE_EXPLOSION_SMALL);
        s->status = 0;
        break;
    default:
        ai_sprite_death_full(c, DEATH_NORMAL, s->y, s->x, true, MZM_PE_SPRITE_EXPLOSION_SMALL);
        break;
    }
}

/* ImagoDamagedStinger: falls burning, lands, blinks and explodes. */
static void damaged_stinger(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->draw_top = s->draw_bottom = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = -(BLOCK_SIZE + HALF_BLOCK_SIZE);
        s->hitbox_bottom = BLOCK_SIZE + HALF_BLOCK_SIZE;
        s->hitbox_left = -HALF_BLOCK_SIZE;
        s->hitbox_right = HALF_BLOCK_SIZE;
        ai_set_anim(s, MZM_IMAGO_DAMAGED_STINGER);
        s->samus_collision = SSC_NONE;
        s->frozen_row_offset = 1;
        s->draw_order = 12;
        s->pose = STINGER_POSE_FALLING;
        s->work0 = 0;
        s->work2 = 10;                                 /* CONVERT_SECONDS(1.f / 6) */
        s->work3 = 0;
        mzm_sfx_play(MZM_SOUND_IMAGO_DAMAGED_STINGER_DETACHING);
        /* fallthrough */
    case STINGER_POSE_FALLING: {
        if ((s->work0 & 7) == 0)
            ai_particle_set(c, (uint16_t)(s->y - BLOCK_SIZE), s->x, MZM_PE_SPRITE_EXPLOSION_BIG);
        uint8_t offset = s->work3;
        int16_t movement = offset < 10 ? c->tables->imago_stinger_falling_y[offset] : SHORT_MAX;
        if (movement == SHORT_MAX) {
            movement = c->tables->imago_stinger_falling_y[offset ? (offset < 10 ? offset - 1 : 8) : 0];
        } else {
            s->work3++;
        }
        s->y = (uint16_t)(s->y + movement);
        s->work0++;
        if (s->work2) {
            s->work2--;
        } else if (ai_collision_at(c, (uint16_t)(s->y + BLOCK_SIZE), s->x) != MZM_COLLISION_AIR) {
            s->work0 = 60;
            s->pose = STINGER_POSE_DISAPPEARING;
            mzm_screen_shake_start_vertical(10, 0x81);
        }
        break;
    }
    case STINGER_POSE_DISAPPEARING:
        if (--s->work0 == 0) {
            ai_particle_set(c, (uint16_t)(s->y - HALF_BLOCK_SIZE), s->x, MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
            s->status = 0;
            mzm_sfx_play(MZM_SOUND_IMAGO_DAMAGED_STINGER_EXPLODING);
        } else if (s->work0 <= 40 && (s->work0 & 3) == 0) {
            s->palette_row = s->work0 & 4 ? stun_row(s) : s->absolute_palette_row;
        }
        break;
    default: break;
    }
}

/* ImagoEgg: solid until Samus touches it or shoots it; the last egg
 * wakes Imago. */
static void egg(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->draw_top = s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->hitbox_top = -(BLOCK_SIZE + PIXEL_SIZE);
        s->hitbox_bottom = 0;
        s->hitbox_left = -(HALF_BLOCK_SIZE - PIXEL_SIZE);
        s->hitbox_right = HALF_BLOCK_SIZE - PIXEL_SIZE;
        s->health = 1;
        ai_set_anim(s, MZM_IMAGO_EGG_STANDING);
        s->samus_collision = SSC_SOLID_CHECK_COLLIDING;
        s->pose = EGG_POSE_IDLE;
        break;
    case EGG_POSE_IDLE:
        if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
            ai_set_anim(s, MZM_IMAGO_EGG_BREAKING);
            s->pose = EGG_POSE_BREAKING;
            s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
            s->health = 0;
            mzm_sfx_play(MZM_SOUND_IMAGO_EGG_BREAKING);
            if (s->room_slot == EGG_PART_LAST) state_of(c)->work3 = 1;
        }
        break;
    case EGG_POSE_BREAKING:
        if (s->frame > 2) {
            s->ignore_samus_timer = 1;
            s->samus_collision = SSC_NONE;
            s->pose = EGG_POSE_DISAPPEARING;
            s->work0 = 180;                            /* CONVERT_SECONDS(3.f) */
        }
        break;
    case EGG_POSE_DISAPPEARING:
        if (s->work0 < 60) s->status ^= MZM_SPRITE_STATUS_NOT_DRAWN;
        if (--s->work0 == 0) s->status = 0;
        break;
    default:
        if (s->room_slot == EGG_PART_LAST) state_of(c)->work3 = 1;
        ai_sprite_death_full(c, DEATH_NO_DEATH_OR_RESPAWNING_ALREADY_HAS_DROP,
                             (uint16_t)(s->y - (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)), s->x, true,
                             MZM_PE_SPRITE_EXPLOSION_SMALL);
        s->status = 0;
        break;
    }
}

void ai_imago(Ctx *c) {
    MzmSprite *s = c->s;
    if (!c->f->globals) {
        s->status = 0;
        return;
    }
    if (!s->secondary) {
        imago(c);
        return;
    }
    switch (s->sprite_id) {
    case MZM_SSPRITE_IMAGO_PART: part(c); break;
    case MZM_SSPRITE_IMAGO_NEEDLE: needle(c); break;
    case MZM_SSPRITE_IMAGO_DAMAGED_STINGER: damaged_stinger(c); break;
    case MZM_SSPRITE_IMAGO_EGG: egg(c); break;
    default: s->status = 0; break;
    }
}
