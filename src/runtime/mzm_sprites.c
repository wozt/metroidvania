/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission sprites; comments name the pinned decompilation's routines. */
#include "mzm_sprites.h"

#include <string.h>

/* Native units (include/types.h). */
enum {
    BLOCK_SIZE = 64,
    HALF_BLOCK_SIZE = 32,
    QUARTER_BLOCK_SIZE = 16,
    EIGHTH_BLOCK_SIZE = 8,
    THREE_QUARTER_BLOCK_SIZE = 48,
    PIXEL_SIZE = 4,
    SUB_PIXEL_POSITION_FLAG = 0x3F,
    BLOCK_POSITION_FLAG = 0xFFC0,
    SHORT_MAX = 32767,
    SCREEN_SIZE_X_SUB_PIXEL = 240 * 4,
    SCREEN_SIZE_Y_SUB_PIXEL = 160 * 4,
    HALF_SCREEN_BLOCKS_SUB_PIXEL = 8 * BLOCK_SIZE,  /* CEIL(SCREEN_SIZE_X_BLOCKS / 2) */
    /* SpriteUtilIsSpriteStunned on easy and normal: CONVERT_SECONDS(.15f). */
    STUN_THRESHOLD = 9,
    SPRITE_POSE_UNINITIALIZED = 0,
    SPRITE_POSE_DESTROYED = 0x62,
    SSC_HURTS_SAMUS = 2,
};

/* Zoomer poses (zoomer.c). */
enum {
    ZOOMER_POSE_IDLE_INIT = 0x8,
    ZOOMER_POSE_IDLE = 0x9,
    ZOOMER_POSE_TURNING_AROUND_INIT = 0xA,
    ZOOMER_POSE_TURNING_AROUND = 0xB,
    ZOOMER_POSE_LANDING_INIT = 0xE,
    ZOOMER_POSE_LANDING = 0xF,
    ZOOMER_POSE_FALLING_INIT = 0x1E,
    ZOOMER_POSE_FALLING = 0x1F,
};

/* ZoomerTurningDirection */
enum {
    TURN_BOTTOM_LEFT_CORNER, TURN_TOP_RIGHT_EDGE, TURN_BOTTOM_RIGHT_EDGE, TURN_TOP_RIGHT_CORNER,
    TURN_TOP_LEFT_EDGE, TURN_BOTTOM_RIGHT_CORNER, TURN_TOP_LEFT_CORNER, TURN_BOTTOM_LEFT_EDGE,
};

const char *const mzm_zoomer_anim_names[MZM_ZOOMER_ANIMS] = {
    "ZoomerOam_Falling", "ZoomerOam_OnGround", "ZoomerOam_OnWall",
    "ZoomerOam_WalkingOnLeftSlope", "ZoomerOam_WalkingOnRightSlope",
    "ZoomerOam_TurningEdgeLeft", "ZoomerOam_TurningEdgeRight",
    "ZoomerOam_TurningCornerLeft", "ZoomerOam_TurningCornerRight",
};

/* The state SpriteUpdate shares with an AI (gCurrentSprite, gSpriteRng,
 * gPreviousVerticalCollisionCheck, gFrameCounter8Bit). */
typedef struct {
    MzmSprite *s;
    const MzmSpriteWorld *world;
    const MzmSpriteTables *tables;
    const MzmSpriteKind *kind;
    uint8_t rng;
    uint8_t vertical;
    uint8_t frame8;
} Ctx;

uint32_t mzm_clipdata_process(const MzmSpriteWorld *world, uint16_t y, uint16_t x, int actor) {
    uint8_t type = world->block(world->context, y, x);
    unsigned sy = y % BLOCK_SIZE, sx = x % BLOCK_SIZE;
    switch (type) {
    case MZM_CLIPDATA_SOLID: return type | MZM_CLIPDATA_SOLID_FLAG;
    case MZM_CLIPDATA_LEFT_STEEP: return sy >= sx ? type | MZM_CLIPDATA_SOLID_FLAG : type;
    case MZM_CLIPDATA_RIGHT_STEEP:
        return sy >= SUB_PIXEL_POSITION_FLAG - sx ? type | MZM_CLIPDATA_SOLID_FLAG : type;
    case MZM_CLIPDATA_LEFT_UPPER_SLIGHT:
        return sy >= sx >> 1 ? type | MZM_CLIPDATA_SOLID_FLAG : type;
    case MZM_CLIPDATA_LEFT_LOWER_SLIGHT:
        return sy >= (sx + SUB_PIXEL_POSITION_FLAG) >> 1 ? type | MZM_CLIPDATA_SOLID_FLAG : type;
    case MZM_CLIPDATA_RIGHT_LOWER_SLIGHT:
        return sy >= SUB_PIXEL_POSITION_FLAG - (sx >> 1) ? type | MZM_CLIPDATA_SOLID_FLAG : type;
    case MZM_CLIPDATA_RIGHT_UPPER_SLIGHT:
        return sy >= (SUB_PIXEL_POSITION_FLAG - sx) >> 1 ? type | MZM_CLIPDATA_SOLID_FLAG : type;
    case MZM_CLIPDATA_ENEMY_ONLY:
        return actor > MZM_ACTOR_NON_SPRITE ? MZM_CLIPDATA_AIR
                                            : MZM_CLIPDATA_SOLID | MZM_CLIPDATA_SOLID_FLAG;
    case MZM_CLIPDATA_DOOR: return MZM_CLIPDATA_SOLID | MZM_CLIPDATA_SOLID_FLAG;
    case MZM_CLIPDATA_STOP_ENEMY:
        return actor < MZM_ACTOR_SPRITE ? MZM_CLIPDATA_AIR
                                        : MZM_CLIPDATA_SOLID | MZM_CLIPDATA_SOLID_FLAG;
    case MZM_CLIPDATA_TANK:
        return actor == MZM_ACTOR_SAMUS ? MZM_CLIPDATA_AIR
                                        : MZM_CLIPDATA_SOLID | MZM_CLIPDATA_SOLID_FLAG;
    default: return type;   /* air, pass-through bottom */
    }
}

/* SpriteUtilGetCollisionAtPosition */
static uint8_t collision_at(const Ctx *c, uint16_t y, uint16_t x) {
    uint32_t clip = mzm_clipdata_process(c->world, y, x, MZM_ACTOR_SPRITE);
    uint8_t collision = clip & MZM_CLIPDATA_SOLID_FLAG ? MZM_COLLISION_SOLID : MZM_COLLISION_AIR;
    switch (clip & 0xFF) {
    case MZM_CLIPDATA_RIGHT_STEEP: return MZM_COLLISION_RIGHT_STEEP_FLOOR_SLOPE;
    case MZM_CLIPDATA_RIGHT_UPPER_SLIGHT:
    case MZM_CLIPDATA_RIGHT_LOWER_SLIGHT: return MZM_COLLISION_RIGHT_SLIGHT_FLOOR_SLOPE;
    case MZM_CLIPDATA_LEFT_STEEP: return MZM_COLLISION_LEFT_STEEP_FLOOR_SLOPE;
    case MZM_CLIPDATA_LEFT_UPPER_SLIGHT:
    case MZM_CLIPDATA_LEFT_LOWER_SLIGHT: return MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE;
    case MZM_CLIPDATA_PASS_THROUGH_BOTTOM: return MZM_COLLISION_PASS_THROUGH_BOTTOM;
    default: return collision;
    }
}

/* The block top of SpriteUtilCheckVerticalCollisionAtPosition(Slopes) and
 * the collision it reports. */
static uint16_t vertical_block_top(const Ctx *c, uint16_t y, uint16_t x, uint8_t *collision) {
    uint32_t clip = mzm_clipdata_process(c->world, y, x, MZM_ACTOR_SPRITE);
    *collision = clip & MZM_CLIPDATA_SOLID_FLAG ? MZM_COLLISION_SOLID : MZM_COLLISION_AIR;
    unsigned sx = x & SUB_PIXEL_POSITION_FLAG;
    uint16_t top = y & BLOCK_POSITION_FLAG;
    switch (clip & 0xFF) {
    case MZM_CLIPDATA_RIGHT_STEEP:
        *collision = MZM_COLLISION_RIGHT_STEEP_FLOOR_SLOPE;
        return (uint16_t)(int16_t)(top - ((int)sx - SUB_PIXEL_POSITION_FLAG));
    case MZM_CLIPDATA_RIGHT_LOWER_SLIGHT:
        *collision = MZM_COLLISION_RIGHT_SLIGHT_FLOOR_SLOPE;
        return (uint16_t)(int16_t)(top - ((int)(sx >> 1) - SUB_PIXEL_POSITION_FLAG));
    case MZM_CLIPDATA_RIGHT_UPPER_SLIGHT:
        *collision = MZM_COLLISION_RIGHT_SLIGHT_FLOOR_SLOPE;
        return (uint16_t)(int16_t)(top - ((int)(sx >> 1) - SUB_PIXEL_POSITION_FLAG / 2));
    case MZM_CLIPDATA_LEFT_STEEP:
        *collision = MZM_COLLISION_LEFT_STEEP_FLOOR_SLOPE;
        return top | sx;
    case MZM_CLIPDATA_LEFT_LOWER_SLIGHT:
        *collision = MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE;
        return top | ((sx >> 1) + SUB_PIXEL_POSITION_FLAG / 2);
    case MZM_CLIPDATA_LEFT_UPPER_SLIGHT:
        *collision = MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE;
        return top | (sx >> 1);
    case MZM_CLIPDATA_PASS_THROUGH_BOTTOM:
        *collision = MZM_COLLISION_PASS_THROUGH_BOTTOM;
        return top;
    default:
        return top;
    }
}

/* SpriteUtilCheckVerticalCollisionAtPosition */
static uint16_t check_vertical(Ctx *c, uint16_t y, uint16_t x) {
    return vertical_block_top(c, y, x, &c->vertical);
}

/* SpriteUtilCheckVerticalCollisionAtPositionSlopes: the collision only
 * counts when the point is at or below the block top. */
static uint16_t check_vertical_slopes(Ctx *c, uint16_t y, uint16_t x) {
    uint8_t collision;
    uint16_t top = vertical_block_top(c, y, x, &collision);
    c->vertical = y >= top ? collision : MZM_COLLISION_AIR;
    return top;
}

/* SpriteUtilAlignYPositionOnSlopeAtOrigin */
static void align_on_slope(Ctx *c) {
    uint16_t y = c->s->y, x = c->s->x;
    uint16_t top = check_vertical(c, (uint16_t)(y - PIXEL_SIZE), x);
    if ((c->vertical & MZM_COLLISION_FLAGS_F) >= MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE) {
        c->s->y = top;
        return;
    }
    top = check_vertical(c, y, x);
    if ((c->vertical & MZM_COLLISION_FLAGS_F) >= MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE) {
        c->s->y = top;
        return;
    }
    top = check_vertical(c, (uint16_t)(y + PIXEL_SIZE), x);
    if (c->vertical != MZM_COLLISION_AIR) c->s->y = top;
}

static uint8_t timer(const Ctx *c, unsigned anim, unsigned frame) {
    if (anim >= MZM_SPRITE_MAX_ANIMS || frame >= MZM_SPRITE_MAX_FRAMES ||
        frame >= c->kind->counts[anim])
        return 0;
    return c->kind->timers[anim][frame];
}

/* SpriteUtilHasCurrentAnimationEnded (extra = 1) and ...NearlyEnded (2). */
static bool animation_ending(const Ctx *c, unsigned extra) {
    unsigned adc = (uint8_t)(c->s->adc + extra);
    return timer(c, c->s->anim, c->s->frame) < adc && timer(c, c->s->anim, c->s->frame + 1u) == 0;
}

static void set_anim(MzmSprite *s, uint8_t anim) {
    s->anim = anim;
    s->adc = 0;
    s->frame = 0;
}

/* SpriteUtilChooseRandomXDirection */
static void random_x_direction(Ctx *c) {
    if (c->rng & 1) c->s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
    else c->s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
}

/* ZoomerCheckCollidingWithAir */
static bool zoomer_in_air(const Ctx *c) {
    const MzmSprite *s = c->s;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        uint16_t x = s->status & MZM_SPRITE_STATUS_X_FLIP ? s->x : (uint16_t)(s->x - PIXEL_SIZE);
        return collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE), x) == MZM_COLLISION_AIR &&
               collision_at(c, (uint16_t)(s->y + HALF_BLOCK_SIZE), x) == MZM_COLLISION_AIR;
    }
    uint16_t y = s->status & MZM_SPRITE_STATUS_Y_FLIP ? (uint16_t)(s->y - PIXEL_SIZE) : s->y;
    return collision_at(c, y, (uint16_t)(s->x - HALF_BLOCK_SIZE)) == MZM_COLLISION_AIR &&
           collision_at(c, y, (uint16_t)(s->x + HALF_BLOCK_SIZE)) == MZM_COLLISION_AIR;
}

/* ZoomerUpdateHitbox */
static void zoomer_hitbox(MzmSprite *s) {
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        s->hitbox_top = -(HALF_BLOCK_SIZE - PIXEL_SIZE);
        s->hitbox_bottom = HALF_BLOCK_SIZE - PIXEL_SIZE;
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
            s->hitbox_left = -(THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE);
            s->hitbox_right = PIXEL_SIZE;
        } else {
            s->hitbox_left = -PIXEL_SIZE;
            s->hitbox_right = THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE;
        }
    } else {
        s->hitbox_left = -(HALF_BLOCK_SIZE - PIXEL_SIZE);
        s->hitbox_right = HALF_BLOCK_SIZE - PIXEL_SIZE;
        if (s->status & MZM_SPRITE_STATUS_Y_FLIP) {
            s->hitbox_top = -PIXEL_SIZE;
            s->hitbox_bottom = THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE;
        } else {
            s->hitbox_top = -(THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE);
            s->hitbox_bottom = PIXEL_SIZE;
        }
    }
}

/* ZoomerSetCrawlingOam / ZoomerSetFallingOam */
static void zoomer_crawling_oam(MzmSprite *s) {
    set_anim(s, s->status & MZM_SPRITE_STATUS_FACING_DOWN ? MZM_ZOOMER_ON_WALL : MZM_ZOOMER_ON_GROUND);
}

static void zoomer_falling_oam(MzmSprite *s) {
    set_anim(s, s->status & MZM_SPRITE_STATUS_FACING_DOWN ? MZM_ZOOMER_ON_WALL : MZM_ZOOMER_FALLING);
}

/* ZoomerInit: the surface it starts on is searched below, above, then on
 * the left and right walls; without one the sprite is removed. */
static void zoomer_init(Ctx *c) {
    MzmSprite *s = c->s;
    random_x_direction(c);
    s->pose = ZOOMER_POSE_IDLE;
    if (collision_at(c, s->y, s->x) & MZM_COLLISION_FLAGS_F0) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
    } else if (collision_at(c, (uint16_t)(s->y - (BLOCK_SIZE + PIXEL_SIZE)), s->x) &
               MZM_COLLISION_FLAGS_F0) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
        s->status |= MZM_SPRITE_STATUS_Y_FLIP;
        s->y -= BLOCK_SIZE;
    } else if (collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE),
                            (uint16_t)(s->x - (HALF_BLOCK_SIZE + PIXEL_SIZE))) &
               MZM_COLLISION_FLAGS_F0) {
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        s->y -= HALF_BLOCK_SIZE;
        s->x -= HALF_BLOCK_SIZE;
    } else if (collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE),
                            (uint16_t)(s->x + HALF_BLOCK_SIZE)) & MZM_COLLISION_FLAGS_F0) {
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN | MZM_SPRITE_STATUS_X_FLIP;
        s->y -= HALF_BLOCK_SIZE;
        s->x += HALF_BLOCK_SIZE;
    } else {
        s->status = 0;
        return;
    }
    s->samus_collision = SSC_HURTS_SAMUS;
    zoomer_crawling_oam(s);
    zoomer_hitbox(s);
    s->health = c->tables->health[s->sprite_id];
    s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
    if (s->sprite_id == MZM_PSPRITE_ZOOMER_RED) s->absolute_palette_row = s->palette_row = 1;
}

/* A turn chosen by ZoomerCrawling: the new facing and direction. */
static bool zoomer_turn(MzmSprite *s, bool set_right, uint8_t direction) {
    if (set_right) s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
    else s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
    s->work1 = direction;
    return true;
}

/* ZoomerCrawling */
static void zoomer_crawling(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t speed = s->sprite_id == MZM_PSPRITE_ZOOMER_RED ? PIXEL_SIZE * 3 / 4 : PIXEL_SIZE / 2;
    bool turning = false, right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    if (zoomer_in_air(c)) {
        s->pose = ZOOMER_POSE_FALLING_INIT;
        return;
    }
    /* SpriteUtilShouldFall: only during a screen shake, not ported. */
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
            if (right) {
                if (!(collision_at(c, s->y, s->x) & MZM_COLLISION_FLAGS_F0))
                    turning = zoomer_turn(s, true, TURN_BOTTOM_LEFT_EDGE);
                else if (collision_at(c, (uint16_t)(s->y + HALF_BLOCK_SIZE),
                                      (uint16_t)(s->x - PIXEL_SIZE)) == MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, false, TURN_BOTTOM_RIGHT_CORNER);
                else
                    s->y += speed;
            } else {
                if (!(collision_at(c, (uint16_t)(s->y - PIXEL_SIZE), s->x) & MZM_COLLISION_FLAGS_F0))
                    turning = zoomer_turn(s, true, TURN_BOTTOM_RIGHT_CORNER);
                else if (collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE),
                                      (uint16_t)(s->x - PIXEL_SIZE)) == MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, false, TURN_BOTTOM_LEFT_EDGE);
                else
                    s->y -= speed;
            }
        } else {
            if (right) {
                if (!(collision_at(c, s->y, (uint16_t)(s->x - PIXEL_SIZE)) & MZM_COLLISION_FLAGS_F0))
                    turning = zoomer_turn(s, false, TURN_TOP_LEFT_CORNER);
                else if (collision_at(c, (uint16_t)(s->y + HALF_BLOCK_SIZE), s->x) ==
                         MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, true, TURN_TOP_LEFT_EDGE);
                else
                    s->y += speed;
            } else {
                if (!(collision_at(c, (uint16_t)(s->y - PIXEL_SIZE), (uint16_t)(s->x - PIXEL_SIZE)) &
                      MZM_COLLISION_FLAGS_F0))
                    turning = zoomer_turn(s, false, TURN_TOP_LEFT_EDGE);
                else if (collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE), s->x) ==
                         MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, true, TURN_TOP_LEFT_CORNER);
                else
                    s->y -= speed;
            }
        }
    } else if (s->status & MZM_SPRITE_STATUS_Y_FLIP) {
        if (right) {
            if (!(collision_at(c, (uint16_t)(s->y - PIXEL_SIZE), s->x) & MZM_COLLISION_FLAGS_F))
                turning = zoomer_turn(s, false, TURN_BOTTOM_RIGHT_EDGE);
            else if (collision_at(c, s->y, (uint16_t)(s->x + HALF_BLOCK_SIZE)) == MZM_COLLISION_SOLID)
                turning = zoomer_turn(s, true, TURN_TOP_RIGHT_CORNER);
            else
                s->x += speed;
        } else {
            if (!(collision_at(c, (uint16_t)(s->y - PIXEL_SIZE), (uint16_t)(s->x - PIXEL_SIZE)) &
                  MZM_COLLISION_FLAGS_F))
                turning = zoomer_turn(s, false, TURN_TOP_RIGHT_CORNER);
            else if (collision_at(c, s->y, (uint16_t)(s->x - HALF_BLOCK_SIZE)) == MZM_COLLISION_SOLID)
                turning = zoomer_turn(s, true, TURN_BOTTOM_RIGHT_EDGE);
            else
                s->x -= speed;
        }
    } else {
        align_on_slope(c);
        if (c->vertical == MZM_COLLISION_AIR || (c->vertical & MZM_COLLISION_FLAGS_F0)) {
            if (s->anim != MZM_ZOOMER_ON_GROUND) set_anim(s, MZM_ZOOMER_ON_GROUND);
            if (right) {
                if (collision_at(c, s->y, s->x) == MZM_COLLISION_AIR)
                    turning = zoomer_turn(s, true, TURN_BOTTOM_LEFT_CORNER);
                else if (collision_at(c, (uint16_t)(s->y - PIXEL_SIZE),
                                      (uint16_t)(s->x + HALF_BLOCK_SIZE)) == MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, false, TURN_TOP_RIGHT_EDGE);
                else
                    s->x += speed;
            } else {
                if (collision_at(c, s->y, (uint16_t)(s->x - PIXEL_SIZE)) == MZM_COLLISION_AIR)
                    turning = zoomer_turn(s, true, TURN_TOP_RIGHT_EDGE);
                else if (collision_at(c, (uint16_t)(s->y - PIXEL_SIZE),
                                      (uint16_t)(s->x - HALF_BLOCK_SIZE)) == MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, false, TURN_BOTTOM_LEFT_CORNER);
                else
                    s->x -= speed;
            }
        } else {
            bool left_slope = c->vertical == MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE ||
                              c->vertical == MZM_COLLISION_LEFT_STEEP_FLOOR_SLOPE;
            bool right_slope = c->vertical == MZM_COLLISION_RIGHT_SLIGHT_FLOOR_SLOPE ||
                               c->vertical == MZM_COLLISION_RIGHT_STEEP_FLOOR_SLOPE;
            /* Down a slope at full speed, up it at two thirds with the
             * animation slowed every other frame. */
            bool down = right ? left_slope : right_slope;
            /* Only slopes reach here, so the table follows the slope. */
            uint8_t anim = left_slope ? MZM_ZOOMER_WALKING_ON_LEFT_SLOPE
                                      : MZM_ZOOMER_WALKING_ON_RIGHT_SLOPE;
            if (s->anim != anim) set_anim(s, anim);
            uint16_t step = down ? speed : (uint16_t)(speed * 2 / 3);
            if (right) s->x += step;
            else s->x -= step;
            if (!down && (c->frame8 & 1) && s->adc != 0) s->adc--;
        }
    }
    if (turning) s->pose = ZOOMER_POSE_TURNING_AROUND_INIT;
}

/* ZoomerTurningAroundInit */
static void zoomer_turning_init(MzmSprite *s) {
    bool right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    s->pose = ZOOMER_POSE_TURNING_AROUND;
    s->adc = 0;
    s->frame = 0;
    static const struct { uint8_t if_right, otherwise; bool x_flip, y_flip; } turns[8] = {
        [TURN_BOTTOM_LEFT_CORNER] = {MZM_ZOOMER_TURNING_EDGE_LEFT, MZM_ZOOMER_TURNING_CORNER_RIGHT, false, false},
        [TURN_TOP_RIGHT_EDGE] = {MZM_ZOOMER_TURNING_EDGE_LEFT, MZM_ZOOMER_TURNING_CORNER_RIGHT, true, false},
        [TURN_BOTTOM_RIGHT_EDGE] = {MZM_ZOOMER_TURNING_CORNER_RIGHT, MZM_ZOOMER_TURNING_EDGE_LEFT, false, true},
        [TURN_TOP_RIGHT_CORNER] = {MZM_ZOOMER_TURNING_CORNER_RIGHT, MZM_ZOOMER_TURNING_EDGE_LEFT, true, true},
        [TURN_TOP_LEFT_EDGE] = {MZM_ZOOMER_TURNING_CORNER_LEFT, MZM_ZOOMER_TURNING_EDGE_RIGHT, false, false},
        [TURN_BOTTOM_RIGHT_CORNER] = {MZM_ZOOMER_TURNING_EDGE_RIGHT, MZM_ZOOMER_TURNING_CORNER_LEFT, true, false},
        [TURN_TOP_LEFT_CORNER] = {MZM_ZOOMER_TURNING_CORNER_LEFT, MZM_ZOOMER_TURNING_EDGE_RIGHT, false, true},
        [TURN_BOTTOM_LEFT_EDGE] = {MZM_ZOOMER_TURNING_EDGE_RIGHT, MZM_ZOOMER_TURNING_CORNER_LEFT, true, true},
    };
    if (s->work1 >= 8) {
        s->status = 0;
        return;
    }
    s->anim = right ? turns[s->work1].if_right : turns[s->work1].otherwise;
    s->status &= (uint16_t)~(MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_Y_FLIP);
    if (turns[s->work1].x_flip) s->status |= MZM_SPRITE_STATUS_X_FLIP;
    if (turns[s->work1].y_flip) s->status |= MZM_SPRITE_STATUS_Y_FLIP;
}

/* ZoomerTurningAround: once the turn animation ends, the zoomer moves onto
 * the new surface. */
static void zoomer_turning(Ctx *c) {
    MzmSprite *s = c->s;
    if (!animation_ending(c, 1)) return;
    bool right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    s->pose = ZOOMER_POSE_IDLE;
    switch (s->work1) {
    case TURN_BOTTOM_LEFT_CORNER:
        if (!right) {
            s->y -= HALF_BLOCK_SIZE - PIXEL_SIZE;
            s->x &= BLOCK_POSITION_FLAG;
        }
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    case TURN_TOP_RIGHT_EDGE:
        if (!right) {
            s->y -= HALF_BLOCK_SIZE;
            s->x &= BLOCK_POSITION_FLAG;
            s->x += BLOCK_SIZE;
        }
        s->status |= MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    case TURN_BOTTOM_RIGHT_EDGE:
        if (right) {
            s->y += HALF_BLOCK_SIZE - PIXEL_SIZE;
            s->x &= BLOCK_POSITION_FLAG;
        }
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    case TURN_TOP_RIGHT_CORNER:
        if (right) {
            s->y += HALF_BLOCK_SIZE;
            s->x &= BLOCK_POSITION_FLAG;
            s->x += BLOCK_SIZE;
        }
        s->status |= MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    case TURN_TOP_LEFT_EDGE:
        if (right) {
            s->x += HALF_BLOCK_SIZE;
            s->y &= BLOCK_POSITION_FLAG;
            s->y += BLOCK_SIZE;
        } else {
            s->y &= BLOCK_POSITION_FLAG;
        }
        s->status &= (uint16_t)~(MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_Y_FLIP |
                                 MZM_SPRITE_STATUS_FACING_DOWN);
        break;
    case TURN_BOTTOM_RIGHT_CORNER:
        if (!right) {
            s->x -= HALF_BLOCK_SIZE;
            s->y &= BLOCK_POSITION_FLAG;
            s->y += BLOCK_SIZE;
        } else {
            s->y &= BLOCK_POSITION_FLAG;
        }
        s->status &= (uint16_t)~(MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_Y_FLIP |
                                 MZM_SPRITE_STATUS_FACING_DOWN);
        break;
    case TURN_TOP_LEFT_CORNER:
        if (right) {
            s->y &= BLOCK_POSITION_FLAG;
            s->x += HALF_BLOCK_SIZE - PIXEL_SIZE;
        }
        s->status |= MZM_SPRITE_STATUS_Y_FLIP;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    case TURN_BOTTOM_LEFT_EDGE:
        if (!right) {
            s->y &= BLOCK_POSITION_FLAG;
            s->x -= HALF_BLOCK_SIZE;
        }
        s->status |= MZM_SPRITE_STATUS_Y_FLIP;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    default:
        s->status = 0;
        return;
    }
    zoomer_crawling_oam(s);
    zoomer_hitbox(s);
}

/* ZoomerFallingInit */
static void zoomer_falling_init(MzmSprite *s) {
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) s->x -= HALF_BLOCK_SIZE;
        else s->x += HALF_BLOCK_SIZE;
    } else if (s->status & MZM_SPRITE_STATUS_Y_FLIP) {
        s->y += HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    }
    s->pose = ZOOMER_POSE_FALLING;
    s->work3 = 0;
    s->status &= (uint16_t)~(MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_Y_FLIP |
                             MZM_SPRITE_STATUS_FACING_DOWN);
    zoomer_hitbox(s);
    zoomer_falling_oam(s);
}

/* ZoomerFalling: sSpritesFallingSpeed until its SHORT_MAX end, then the last
 * speed; landing on any collision (the splash of SpriteUtilCheckInRoomEffect
 * is not ported). */
static void zoomer_falling(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t offset = s->work3;
    int16_t movement = offset < 16 ? c->tables->falling[offset] : SHORT_MAX;
    if (movement == SHORT_MAX) {
        s->y = (uint16_t)(s->y + c->tables->falling[offset > 0 ? offset - 1 : 0]);
    } else {
        s->work3++;
        s->y = (uint16_t)(s->y + movement);
    }
    uint16_t top = check_vertical_slopes(c, s->y, s->x);
    if (c->vertical != MZM_COLLISION_AIR) {
        s->y = top;
        s->pose = ZOOMER_POSE_LANDING;
        random_x_direction(c);
    }
}

/* ZoomerLanding */
static void zoomer_landing(Ctx *c) {
    if (zoomer_in_air(c)) c->s->pose = ZOOMER_POSE_FALLING_INIT;
    else if (animation_ending(c, 2)) c->s->pose = ZOOMER_POSE_IDLE_INIT;
}

/* Zoomer: the AI. The damaged sound, freezing and the death
 * (ZoomerDeath / SpriteUtilSpriteDeath) are not ported yet. */
static void zoomer(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) return;
    /* SpriteUtilIsSpriteStunned */
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: zoomer_init(c); break;
    case ZOOMER_POSE_IDLE_INIT:
        zoomer_crawling_oam(s);
        s->pose = ZOOMER_POSE_IDLE;
        /* fallthrough */
    case ZOOMER_POSE_IDLE: zoomer_crawling(c); break;
    case ZOOMER_POSE_TURNING_AROUND_INIT:
        zoomer_turning_init(s);
        if (!s->status) break;
        /* fallthrough */
    case ZOOMER_POSE_TURNING_AROUND: zoomer_turning(c); break;
    case ZOOMER_POSE_LANDING_INIT:
        s->pose = ZOOMER_POSE_LANDING;
        zoomer_falling_oam(s);
        /* fallthrough */
    case ZOOMER_POSE_LANDING: zoomer_landing(c); break;
    case ZOOMER_POSE_FALLING_INIT:
        zoomer_falling_init(s);
        /* fallthrough */
    case ZOOMER_POSE_FALLING: zoomer_falling(c); break;
    default: break;
    }
}

bool mzm_sprite_supported(uint8_t sprite_id) {
    return sprite_id == MZM_PSPRITE_ZOOMER_YELLOW || sprite_id == MZM_PSPRITE_ZOOMER_RED;
}

void mzm_sprite_init_primary(MzmSprite *sprite, uint8_t sprite_id, uint8_t gfx_slot,
                             uint16_t y_block, uint16_t x_block, uint8_t room_slot) {
    memset(sprite, 0, sizeof *sprite);
    sprite->status = MZM_SPRITE_STATUS_EXISTS;
    sprite->sprite_id = sprite_id;
    sprite->gfx_slot = gfx_slot;
    sprite->room_slot = room_slot;
    /* Bottom middle of the block. */
    sprite->y = (uint16_t)(y_block * BLOCK_SIZE + BLOCK_SIZE);
    sprite->x = (uint16_t)(x_block * BLOCK_SIZE + HALF_BLOCK_SIZE);
    sprite->pose = SPRITE_POSE_UNINITIALIZED;
    sprite->ignore_samus_timer = 1;
}

/* SpriteUpdateAnimation */
static void update_animation(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) return;
    s->adc++;
    if (timer(c, s->anim, s->frame) < s->adc) {
        s->adc = 1;
        s->frame++;
        if (timer(c, s->anim, s->frame) == 0) s->frame = 0;
    }
}

/* SpriteCheckOnScreen (sprites without an absolute position). */
static void check_on_screen(const Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t bg_y = (uint16_t)(c->world->camera_y + HALF_SCREEN_BLOCKS_SUB_PIXEL);
    uint16_t sprite_y = (uint16_t)(s->y + HALF_SCREEN_BLOCKS_SUB_PIXEL);
    uint16_t bottom = (uint16_t)(bg_y - s->draw_bottom * PIXEL_SIZE);
    uint16_t top = (uint16_t)(bg_y + s->draw_top * PIXEL_SIZE + SCREEN_SIZE_Y_SUB_PIXEL);
    uint16_t bg_x = (uint16_t)(c->world->camera_x + HALF_SCREEN_BLOCKS_SUB_PIXEL);
    uint16_t sprite_x = (uint16_t)(s->x + HALF_SCREEN_BLOCKS_SUB_PIXEL);
    uint16_t left = (uint16_t)(bg_x - s->draw_horizontal * PIXEL_SIZE);
    uint16_t right = (uint16_t)(bg_x + s->draw_horizontal * PIXEL_SIZE + SCREEN_SIZE_X_SUB_PIXEL);
    if (left < sprite_x && sprite_x < right && bottom < sprite_y && sprite_y < top)
        s->status |= MZM_SPRITE_STATUS_ONSCREEN;
    else
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_ONSCREEN;
}

void mzm_sprite_update(MzmSprite *sprite, uint8_t slot, const MzmSpriteWorld *world,
                       const MzmSpriteTables *tables, const MzmSpriteKind *kind,
                       uint8_t frame8, uint16_t frame16) {
    if (!(sprite->status & MZM_SPRITE_STATUS_EXISTS) || !mzm_sprite_supported(sprite->sprite_id))
        return;
    Ctx c = {sprite, world, tables, kind, 0, MZM_COLLISION_AIR, frame8};
    /* gSpriteRng = sSpriteRandomNumberTable[(frame8 + slot + frame16 / 16 +
     * x + y) % 32] */
    c.rng = tables->random[((uint32_t)frame8 + slot + frame16 / 16u + sprite->x + sprite->y) % 32u];
    /* SpriteUtilUpdateStunTimer: no damage source yet, the timer only runs
     * down. */
    if (sprite->isft & 0x7F) sprite->isft--;
    zoomer(&c);
    if (sprite->status & MZM_SPRITE_STATUS_EXISTS) {
        update_animation(&c);
        check_on_screen(&c);
    }
    if (sprite->ignore_samus_timer) sprite->ignore_samus_timer--;
}
