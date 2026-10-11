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
    SPRITE_POSE_SCREW_ATTACK_DESTROYED = 0x65,
    /* SamusSpriteCollision */
    SSC_NONE = 0,
    SSC_HURTS_SAMUS = 4,
    SSC_SMALL_ENERGY_DROP = 31,
    SSC_LARGE_ENERGY_DROP = 32,
    SSC_MISSILE_DROP = 33,
    SSC_SUPER_MISSILE_DROP = 34,
    SSC_POWER_BOMB_DROP = 35,
    SPRITE_DROP_MAX_PROB = 1024,
    /* CONVERT_SECONDS of the combat timers. */
    ISFT_DAMAGED = 17,              /* .25f + 2 * DELTA_TIME */
    ISFT_SCREW_ATTACK = 17,         /* .29f */
    ISFT_SOLID_HIT = 3,             /* .05f */
    IGNORE_SAMUS_AFTER_HIT = 15,    /* .25f */
    NORMAL_BEAM_DAMAGE = 2,
    MISSILE_DAMAGE = 20,
    SUPER_MISSILE_DAMAGE = 100,
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
    ENEMY_DROP_POSE_IDLE = 0x9,
};

/* Ripper poses (ripper.c). */
enum {
    RIPPER_POSE_MOVING_INIT = 0x8,
    RIPPER_POSE_MOVING = 0x9,
    RIPPER_POSE_TURNING_AROUND_INIT = 0xA,
    RIPPER_POSE_TURNING_AROUND_FIRST_PART = 0xB,
    RIPPER_POSE_TURNING_AROUND_SECOND_PART = 0xC,
};

/* Skree poses (skree.c). */
enum {
    SKREE_POSE_IDLE_INIT = 0x8,
    SKREE_POSE_IDLE = 0x9,
    SKREE_POSE_SPINNING_INIT = 0x22,
    SKREE_POSE_SPINNING = 0x23,
    SKREE_POSE_GOING_DOWN_INIT = 0x34,
    SKREE_POSE_GOING_DOWN = 0x35,
    SKREE_POSE_CRASHING = 0x37,
    SKREE_EXPLOSION_PART_GOING_UP = 0,
    SKREE_EXPLOSION_POSE_MOVING = 0x9,
    SKREE_Y_RANGE = BLOCK_SIZE * 10 + PIXEL_SIZE,
    SKREE_X_RANGE = BLOCK_SIZE * 2 + QUARTER_BLOCK_SIZE + PIXEL_SIZE + PIXEL_SIZE / 2,
    PI_4 = 32,                      /* PI = Q_8_8(.5f) */
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

const char *const mzm_drop_anim_names[MZM_DROP_ANIMS] = {
    "EnemyDropOam_SmallEnergy", "EnemyDropOam_LargeEnergy", "EnemyDropOam_Missile",
    "EnemyDropOam_SuperMissile", "EnemyDropOam_PowerBomb",
};

const char *const mzm_ripper_anim_names[MZM_RIPPER_ANIMS] = {
    "RipperOam_Moving", "RipperOam_TurningAround", "RipperOam_TurningAroundPart2",
};

static const char *const zoomer_sprites[MZM_ZOOMER_ANIMS] = {
    "ZOOMER_YELLOW", "ZOOMER_YELLOW", "ZOOMER_YELLOW", "ZOOMER_YELLOW", "ZOOMER_YELLOW",
    "ZOOMER_YELLOW", "ZOOMER_YELLOW", "ZOOMER_YELLOW", "ZOOMER_YELLOW",
};
static const char *const ripper_sprites[MZM_RIPPER_ANIMS] = {
    "RIPPER_BROWN", "RIPPER_BROWN", "RIPPER_BROWN",
};
const char *const mzm_skree_anim_names[MZM_SKREE_ANIMS] = {
    "SkreeOam_Idle", "SkreeOam_Spinning", "SkreeOam_GoingDown", "SkreeOam_Crashing",
    "SkreeExplosionOam_GoingDown", "SkreeExplosionOam_GoingUp",
};
static const char *const skree_sprites[MZM_SKREE_ANIMS] = {
    "SKREE_GREEN", "SKREE_GREEN", "SKREE_GREEN", "SKREE_GREEN", "SKREE_GREEN", "SKREE_GREEN",
};
static const char *const drop_sprites[MZM_DROP_ANIMS] = {
    "SMALL_ENERGY_DROP", "LARGE_ENERGY_DROP", "MISSILE_DROP", "SUPER_MISSILE_DROP",
    "POWER_BOMB_DROP",
};

const MzmSpriteFamily mzm_sprite_families[MZM_SPRITE_FAMILIES] = {
    [MZM_FAMILY_ZOOMER] = {MZM_ZOOMER_ANIMS, mzm_zoomer_anim_names, zoomer_sprites},
    [MZM_FAMILY_RIPPER] = {MZM_RIPPER_ANIMS, mzm_ripper_anim_names, ripper_sprites},
    [MZM_FAMILY_SKREE] = {MZM_SKREE_ANIMS, mzm_skree_anim_names, skree_sprites},
    [MZM_FAMILY_DROP] = {MZM_DROP_ANIMS, mzm_drop_anim_names, drop_sprites},
};

/* The state SpriteUpdate shares with an AI (gCurrentSprite, gSpriteRng,
 * gPreviousVerticalCollisionCheck, gFrameCounter8Bit). */
typedef struct {
    MzmSprite *s;
    const MzmSpriteFrame *f;
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

/* SpriteUtilDetermineEnemyDrop: the sprite's drop probabilities stacked
 * down from SPRITE_DROP_MAX_PROB (power bomb, super missile, missile, large
 * energy, small energy), with the substitutions for full ammunition and
 * energy. */
static uint8_t determine_drop(const Ctx *c) {
    const MzmEquipment *e = c->f->equipment;
    const uint16_t *probs = c->tables->drops[c->s->sprite_id];
    bool full_life = e->energy == e->max_energy;
    uint16_t rng = (uint16_t)(c->rng * 256);
    rng = (uint16_t)(c->frame8 + c->f->frame16 + rng);
    rng %= SPRITE_DROP_MAX_PROB;
    if (rng == 0) rng = 1;          /* not REGION_US_BETA */
    /* Ammunition drops, power bombs first (none ported: 0 of 0). */
    static const uint8_t ammo_drops[3] = {
        MZM_PSPRITE_POWER_BOMB_DROP, MZM_PSPRITE_SUPER_MISSILE_DROP, MZM_PSPRITE_MISSILE_DROP,
    };
    int current[3] = {0, e->super_missiles, e->missiles};
    int maximum[3] = {0, e->max_super_missiles, e->max_missiles};
    uint16_t upper = SPRITE_DROP_MAX_PROB;
    for (int i = 0; i < 3; ++i) {
        uint16_t prob = probs[MZM_DROP_PROB_POWER_BOMB - i];
        if (prob == 0) continue;
        uint16_t lower = (uint16_t)(upper - prob);
        if (upper >= rng && rng > lower) {
            if (maximum[i] > current[i]) return ammo_drops[i];
            if (!full_life) return MZM_PSPRITE_LARGE_ENERGY_DROP;
            return maximum[i] != 0 ? ammo_drops[i] : MZM_PSPRITE_SMALL_ENERGY_DROP;
        }
        upper = lower;
    }
    static const uint8_t energy_drops[2] = {
        MZM_PSPRITE_LARGE_ENERGY_DROP, MZM_PSPRITE_SMALL_ENERGY_DROP,
    };
    for (int i = 0; i < 2; ++i) {
        uint16_t prob = probs[MZM_DROP_PROB_LARGE_ENERGY - i];
        if (prob == 0) continue;
        uint16_t lower = (uint16_t)(upper - prob);
        if (upper >= rng && rng > lower) {
            if (full_life && e->max_missiles > e->missiles) return MZM_PSPRITE_MISSILE_DROP;
            return energy_drops[i];
        }
        upper = lower;
    }
    return MZM_PSPRITE_UNUSED0;
}

/* SpriteUtilSpriteDeath (DEATH_NORMAL): the sprite becomes its drop at the
 * given position, or is removed. The explosion particle, debris and sound
 * are not ported. */
static void sprite_death(Ctx *c, uint16_t y, uint16_t x) {
    MzmSprite *s = c->s;
    uint8_t drop = determine_drop(c);
    if (drop == MZM_PSPRITE_UNUSED0) {
        s->status = 0;
        return;
    }
    s->status = MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_ONSCREEN | MZM_SPRITE_STATUS_NOT_DRAWN;
    s->secondary = false;           /* properties = 0 */
    s->kill_off_screen = false;
    s->gfx_slot = 0;
    s->sprite_id = drop;
    s->y = y;
    s->x = x;
    s->pose = SPRITE_POSE_UNINITIALIZED;
    s->health = 0;
    s->isft = 0;
    s->palette_row = 0;
    s->absolute_palette_row = 0;
    s->ignore_samus_timer = 1;
    s->freeze_timer = 0;
}

/* ZoomerDeath: the explosion point is the middle of the body. */
static void zoomer_death(Ctx *c) {
    const MzmSprite *s = c->s;
    uint16_t y = s->y, x = s->x;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) x -= HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        else x += HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    } else {
        if (s->status & MZM_SPRITE_STATUS_Y_FLIP) y += HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        else y -= HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    }
    sprite_death(c, y, x);
}

/* Zoomer: the AI. The damaged sound and freezing are not ported. */
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
    default: zoomer_death(c); break;
    }
}

/* SpriteUtilChooseRandomXFlip */
static void random_x_flip(Ctx *c) {
    if (c->rng & 1) c->s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
    else c->s->status |= MZM_SPRITE_STATUS_X_FLIP;
}

/* RipperInit */
static void ripper_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->hitbox_top = -(HALF_BLOCK_SIZE + PIXEL_SIZE);
    s->hitbox_bottom = 0;
    s->hitbox_left = -HALF_BLOCK_SIZE;
    s->hitbox_right = HALF_BLOCK_SIZE;
    s->draw_top = BLOCK_SIZE / PIXEL_SIZE;
    s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
    s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
    set_anim(s, MZM_RIPPER_MOVING);
    s->samus_collision = SSC_HURTS_SAMUS;
    s->health = c->tables->health[s->sprite_id];
    s->y -= EIGHTH_BLOCK_SIZE;
    random_x_flip(c);
    s->pose = RIPPER_POSE_MOVING_INIT;
}

/* RipperMove: half a pixel per frame (purple: one), turning at a wall a
 * quarter block above its feet. */
static void ripper_move(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t speed = s->sprite_id == MZM_PSPRITE_RIPPER_PURPLE ? PIXEL_SIZE : PIXEL_SIZE / 2;
    uint16_t y = (uint16_t)(s->y - QUARTER_BLOCK_SIZE);
    if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
        if (collision_at(c, y, (uint16_t)(s->x + HALF_BLOCK_SIZE + PIXEL_SIZE)) != MZM_COLLISION_SOLID)
            s->x += speed;
        else
            s->pose = RIPPER_POSE_TURNING_AROUND_INIT;
    } else {
        if (collision_at(c, y, (uint16_t)(s->x - (HALF_BLOCK_SIZE + PIXEL_SIZE))) == MZM_COLLISION_SOLID)
            s->pose = RIPPER_POSE_TURNING_AROUND_INIT;
        else
            s->x -= speed;
    }
}

/* Ripper: the AI. The damaged sound and freezing are not ported. */
static void ripper(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) return;
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: ripper_init(c); break;
    case RIPPER_POSE_MOVING_INIT:
        s->pose = RIPPER_POSE_MOVING;
        set_anim(s, MZM_RIPPER_MOVING);
        /* fallthrough */
    case RIPPER_POSE_MOVING: ripper_move(c); break;
    case RIPPER_POSE_TURNING_AROUND_INIT:
        s->pose = RIPPER_POSE_TURNING_AROUND_FIRST_PART;
        set_anim(s, MZM_RIPPER_TURNING_AROUND);
        /* fallthrough */
    case RIPPER_POSE_TURNING_AROUND_FIRST_PART:
        if (animation_ending(c, 1)) {
            s->status ^= MZM_SPRITE_STATUS_X_FLIP;
            s->pose = RIPPER_POSE_TURNING_AROUND_SECOND_PART;
            set_anim(s, MZM_RIPPER_TURNING_AROUND_PART2);
        }
        break;
    case RIPPER_POSE_TURNING_AROUND_SECOND_PART:
        if (animation_ending(c, 2)) s->pose = RIPPER_POSE_MOVING_INIT;
        break;
    default:
        sprite_death(c, (uint16_t)(s->y - (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)), s->x);
        break;
    }
}

/* SpriteSpawnSecondary: the first free slot, or nothing. */
static void spawn_secondary(Ctx *c, uint8_t sprite_id, uint8_t part, uint8_t gfx_slot,
                            uint16_t y, uint16_t x, uint16_t status) {
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        MzmSprite *n = &c->f->sprites[i];
        if (n->status & MZM_SPRITE_STATUS_EXISTS) continue;
        memset(n, 0, sizeof *n);
        n->status = status | MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_ONSCREEN |
                    MZM_SPRITE_STATUS_NOT_DRAWN;
        n->secondary = true;
        n->gfx_slot = gfx_slot;
        n->sprite_id = sprite_id;
        n->y = y;
        n->x = x;
        n->room_slot = part;
        n->pose = SPRITE_POSE_UNINITIALIZED;
        n->ignore_samus_timer = 1;
        return;
    }
}

/* SkreeInit, SkreeIdleInit */
static void skree_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->samus_collision = SSC_HURTS_SAMUS;
    s->draw_top = 0;
    s->draw_bottom = BLOCK_SIZE * 2 / PIXEL_SIZE;
    s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
    s->hitbox_top = 0;
    s->hitbox_bottom = BLOCK_SIZE + HALF_BLOCK_SIZE;
    s->hitbox_left = -(QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
    s->hitbox_right = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    s->health = c->tables->health[s->sprite_id];
    s->y -= BLOCK_SIZE;
}

/* SkreeDetectSamus: Samus below it within 10 blocks, and within 150
 * subpixels horizontally, while it is on screen. */
static void skree_detect(Ctx *c) {
    MzmSprite *s = c->s;
    int samus_y = c->f->samus_y, samus_x = c->f->samus_x;
    if (samus_y <= s->y || samus_y - s->y >= SKREE_Y_RANGE) return;
    if (!(s->status & MZM_SPRITE_STATUS_ONSCREEN)) return;
    if (samus_x <= s->x - SKREE_X_RANGE || samus_x >= s->x + SKREE_X_RANGE) return;
    s->pose = SKREE_POSE_SPINNING_INIT;
}

/* SkreeGoingDownInit (the sound is not played) */
static void skree_going_down_init(Ctx *c) {
    MzmSprite *s = c->s;
    set_anim(s, MZM_SKREE_GOING_DOWN);
    s->work3 = 0;
    s->work2 = 0;
    s->pose = SKREE_POSE_GOING_DOWN;
    if (s->x > c->f->samus_x) s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
    else s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
}

/* SkreeGoDown: sSkreeFallingSpeed down, drifting toward Samus faster every
 * frame (a quarter of the frame count, below a quarter block), until its
 * hitbox bottom lands. */
static void skree_go_down(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t top = check_vertical_slopes(c, (uint16_t)(s->hitbox_bottom + s->y), s->x);
    if (c->vertical != MZM_COLLISION_AIR) {
        s->y = (uint16_t)(top - s->hitbox_bottom);
        s->pose = SKREE_POSE_CRASHING;
        s->work0 = 0;
        return;
    }
    uint16_t x_movement = s->work2 / 4;
    uint8_t offset = s->work3;
    int16_t y_movement = offset < 16 ? c->tables->skree_falling[offset] : SHORT_MAX;
    if (y_movement == SHORT_MAX) {
        s->y = (uint16_t)(s->y + c->tables->skree_falling[offset > 0 ? offset - 1 : 0]);
    } else {
        s->work3++;
        s->y = (uint16_t)(s->y + y_movement);
    }
    if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
        if (collision_at(c, (uint16_t)(s->y + BLOCK_SIZE), (uint16_t)(s->x + HALF_BLOCK_SIZE)) ==
            MZM_COLLISION_SOLID)
            return;
        s->x += x_movement;
    } else {
        if (collision_at(c, (uint16_t)(s->y + BLOCK_SIZE), (uint16_t)(s->x - HALF_BLOCK_SIZE)) ==
            MZM_COLLISION_SOLID)
            return;
        s->x -= x_movement;
    }
    if (x_movement < QUARTER_BLOCK_SIZE) s->work2++;
}

/* SkreeCrashGround: the crash table after 40 frames (pOam only), the four
 * explosion parts after 60. The debris, particle and sound are not ported. */
static void skree_crash(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t y = s->y, x = s->x;
    s->work0++;
    if (s->work0 == 40) {
        s->anim = MZM_SKREE_CRASHING;
    } else if (s->work0 == 60) {
        uint8_t id = s->sprite_id == MZM_PSPRITE_SKREE_BLUE ? MZM_SSPRITE_BLUE_SKREE_EXPLOSION
                                                            : MZM_SSPRITE_SKREE_EXPLOSION;
        spawn_secondary(c, id, SKREE_EXPLOSION_PART_GOING_UP, s->gfx_slot,
                        (uint16_t)(y - EIGHTH_BLOCK_SIZE), x, 0);
        spawn_secondary(c, id, SKREE_EXPLOSION_PART_GOING_UP, s->gfx_slot,
                        (uint16_t)(y - EIGHTH_BLOCK_SIZE), x, MZM_SPRITE_STATUS_X_FLIP);
        spawn_secondary(c, id, 1, s->gfx_slot, (uint16_t)(y + EIGHTH_BLOCK_SIZE),
                        (uint16_t)(x - (QUARTER_BLOCK_SIZE - PIXEL_SIZE)), 0);
        spawn_secondary(c, id, 1, s->gfx_slot, (uint16_t)(y + EIGHTH_BLOCK_SIZE),
                        (uint16_t)(x + (QUARTER_BLOCK_SIZE - PIXEL_SIZE)), MZM_SPRITE_STATUS_X_FLIP);
        s->status = 0;
    }
}

/* Skree: the AI. The damaged sound and freezing are not ported. */
static void skree(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) return;
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        skree_init(c);
        /* fallthrough */
    case SKREE_POSE_IDLE_INIT:
        set_anim(s, MZM_SKREE_IDLE);
        s->pose = SKREE_POSE_IDLE;
        /* fallthrough */
    case SKREE_POSE_IDLE: skree_detect(c); break;
    case SKREE_POSE_SPINNING_INIT:
        set_anim(s, MZM_SKREE_SPINNING);
        s->pose = SKREE_POSE_SPINNING;
        /* fallthrough */
    case SKREE_POSE_SPINNING:
        if (animation_ending(c, 2)) s->pose = SKREE_POSE_GOING_DOWN_INIT;
        break;
    case SKREE_POSE_GOING_DOWN_INIT:
        skree_going_down_init(c);
        /* fallthrough */
    case SKREE_POSE_GOING_DOWN: skree_go_down(c); break;
    case SKREE_POSE_CRASHING: skree_crash(c); break;
    default:
        sprite_death(c, (uint16_t)(s->y + THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE), s->x);
        break;
    }
}

/* SkreeExplosionInit and SkreeExplosionMove: a part flies off diagonally,
 * spinning an eighth of a turn per frame, harmless after its second frame,
 * until its table ends; removed off screen. */
static void skree_explosion(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->kill_off_screen = true;
        s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -(QUARTER_BLOCK_SIZE - PIXEL_SIZE);
        s->hitbox_bottom = s->hitbox_right = QUARTER_BLOCK_SIZE - PIXEL_SIZE;
        s->adc = 0;
        s->frame = 0;
        s->pose = SKREE_EXPLOSION_POSE_MOVING;
        s->samus_collision = SSC_HURTS_SAMUS;
        s->y += HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        s->status |= MZM_SPRITE_STATUS_DOUBLE_SIZE | MZM_SPRITE_STATUS_ROTATION_SCALING_WHOLE;
        s->rotation = 0;
        s->anim = s->room_slot != SKREE_EXPLOSION_PART_GOING_UP ? MZM_SKREE_EXPLOSION_GOING_DOWN
                                                                : MZM_SKREE_EXPLOSION_GOING_UP;
    }
    if (s->pose != SKREE_EXPLOSION_POSE_MOVING) return;
    if (s->frame > 1) s->ignore_samus_timer = 1;
    if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
        s->x += EIGHTH_BLOCK_SIZE;
        s->rotation += PI_4;
    } else {
        s->x -= EIGHTH_BLOCK_SIZE;
        s->rotation -= PI_4;
    }
    if (s->room_slot != SKREE_EXPLOSION_PART_GOING_UP) s->y += PIXEL_SIZE;
    else s->y -= PIXEL_SIZE;
    if (animation_ending(c, 1)) s->status = 0;
}

/* EnemyDropInit */
static void enemy_drop_init(MzmSprite *s) {
    s->ignore_samus_timer = 20;     /* ONE_THIRD_SECOND */
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->pose = ENEMY_DROP_POSE_IDLE;
    s->y_spawn = 200;               /* CONVERT_SECONDS(3.f) + ONE_THIRD_SECOND */
    s->x_spawn = 0;
    s->adc = 0;
    s->frame = 0;
    s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
    switch (s->sprite_id) {
    case MZM_PSPRITE_POWER_BOMB_DROP:
        s->anim = MZM_DROP_POWER_BOMB;
        s->samus_collision = SSC_POWER_BOMB_DROP;
        break;
    case MZM_PSPRITE_SUPER_MISSILE_DROP:
        s->anim = MZM_DROP_SUPER_MISSILE;
        s->samus_collision = SSC_SUPER_MISSILE_DROP;
        break;
    case MZM_PSPRITE_MISSILE_DROP:
        s->anim = MZM_DROP_MISSILE;
        s->samus_collision = SSC_MISSILE_DROP;
        break;
    case MZM_PSPRITE_LARGE_ENERGY_DROP:
        s->anim = MZM_DROP_LARGE_ENERGY;
        s->samus_collision = SSC_LARGE_ENERGY_DROP;
        break;
    default:                        /* PSPRITE_MULTIPLE_LARGE_ENERGY is not ported */
        s->anim = MZM_DROP_SMALL_ENERGY;
        s->samus_collision = SSC_SMALL_ENERGY_DROP;
        break;
    }
    s->hitbox_top = -(HALF_BLOCK_SIZE + PIXEL_SIZE);
    s->hitbox_bottom = HALF_BLOCK_SIZE + PIXEL_SIZE;
    s->hitbox_left = -HALF_BLOCK_SIZE;
    s->hitbox_right = HALF_BLOCK_SIZE;
}

static void refill(int *current, int maximum, int amount) {
    *current += amount;
    if (*current > maximum) *current = maximum;
}

/* EnemyDropIdle: shown once the ignore timer is nearly over, collected on
 * contact (sounds not ported), blinking during its last 80 frames and gone
 * after 200 frames counted every other frame. */
static void enemy_drop_idle(Ctx *c) {
    MzmSprite *s = c->s;
    MzmEquipment *e = c->f->equipment;
    if (s->ignore_samus_timer) {
        if (s->ignore_samus_timer < 2) s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        return;
    }
    if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
        switch (s->samus_collision) {
        case SSC_SMALL_ENERGY_DROP: refill(&e->energy, e->max_energy, 5); break;
        case SSC_LARGE_ENERGY_DROP: refill(&e->energy, e->max_energy, 20); break;
        case SSC_MISSILE_DROP: refill(&e->missiles, e->max_missiles, 2); break;
        case SSC_SUPER_MISSILE_DROP: refill(&e->super_missiles, e->max_super_missiles, 2); break;
        default: break;             /* power bombs: not ported */
        }
        s->status = 0;
        return;
    }
    if (s->x_spawn & 1) {
        uint8_t timer = (uint8_t)--s->y_spawn;
        if (timer == 0) s->status = 0;
        else if (timer < 80) s->status ^= MZM_SPRITE_STATUS_NOT_DRAWN;
    }
}

/* EnemyDrop */
static void enemy_drop(Ctx *c) {
    if (c->s->pose == SPRITE_POSE_UNINITIALIZED) enemy_drop_init(c->s);
    else if (c->s->pose == ENEMY_DROP_POSE_IDLE) enemy_drop_idle(c);
    c->s->x_spawn++;
}

static bool is_drop(uint8_t sprite_id) {
    return sprite_id >= MZM_PSPRITE_LARGE_ENERGY_DROP && sprite_id <= MZM_PSPRITE_POWER_BOMB_DROP;
}

int mzm_sprite_family(uint8_t sprite_id, bool secondary) {
    if (secondary)
        return sprite_id == MZM_SSPRITE_SKREE_EXPLOSION ||
               sprite_id == MZM_SSPRITE_BLUE_SKREE_EXPLOSION ? MZM_FAMILY_SKREE : -1;
    switch (sprite_id) {
    case MZM_PSPRITE_ZOOMER_YELLOW:
    case MZM_PSPRITE_ZOOMER_RED: return MZM_FAMILY_ZOOMER;
    case MZM_PSPRITE_RIPPER_BROWN:
    case MZM_PSPRITE_RIPPER_PURPLE: return MZM_FAMILY_RIPPER;
    case MZM_PSPRITE_SKREE_GREEN:
    case MZM_PSPRITE_SKREE_BLUE: return MZM_FAMILY_SKREE;
    default: return is_drop(sprite_id) ? MZM_FAMILY_DROP : -1;
    }
}

/* SPRITE_CLEAR_AND_SET_ISFT (bit 7 is the power bomb stun). */
static void set_isft(MzmSprite *s, uint8_t value) {
    s->isft = (uint8_t)((s->isft & 0x80) | value);
}

/* SpriteUtilCheckObjectsTouching */
static bool touching(uint16_t o1_top, uint16_t o1_bottom, uint16_t o1_left, uint16_t o1_right,
                     uint16_t o2_top, uint16_t o2_bottom, uint16_t o2_left, uint16_t o2_right) {
    return o2_bottom >= o1_top && o2_top < o1_bottom && o2_right >= o1_left && o2_left < o1_right;
}

/* SpriteUtilSpriteTakeDamageFromSamusContact without the speed booster,
 * shinespark and charged pseudo screw (not ported on Samus): true when the
 * screw attack destroyed the sprite (DCT_SCREW_ATTACK). */
/* GET_PSPRITE_ / GET_SSPRITE_ stats */
static uint8_t weakness_of(const MzmSprite *s, const MzmSpriteTables *tables) {
    return s->secondary ? tables->secondary_weakness[s->sprite_id] : tables->weakness[s->sprite_id];
}

static uint16_t damage_of(const MzmSprite *s, const MzmSpriteTables *tables) {
    return s->secondary ? tables->secondary_damage[s->sprite_id] : tables->damage[s->sprite_id];
}

static bool contact_kills(MzmSprite *s, const MzmSpriteSamus *samus,
                          const MzmSpriteTables *tables) {
    if (samus->suitless || !samus->screw_attacking) return false;
    /* Sprites ignoring projectiles take no contact damage. */
    if (s->status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES) return false;
    if (weakness_of(s, tables) & MZM_WEAKNESS_SPEEDBOOSTER_SCREW_ATTACK) {
        s->health = 0;
        s->freeze_timer = 0;
        s->palette_row = 0;
        s->pose = SPRITE_POSE_SCREW_ATTACK_DESTROYED;
        s->ignore_samus_timer = 1;
        set_isft(s, ISFT_SCREW_ATTACK);
        return true;
    }
    if ((s->isft & 0x7F) < ISFT_SOLID_HIT) set_isft(s, ISFT_SOLID_HIT);
    return false;
}

void mzm_sprites_samus_collision(MzmSprite *sprites, int count, MzmSpriteSamus *samus,
                                 const MzmSpriteTables *tables) {
    uint16_t samus_top = (uint16_t)(samus->y + samus->hitbox_top);
    uint16_t samus_bottom = (uint16_t)(samus->y + samus->hitbox_bottom);
    uint16_t samus_left = (uint16_t)(samus->x + samus->hitbox_left);
    uint16_t samus_right = (uint16_t)(samus->x + samus->hitbox_right);
    const uint16_t check = MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_ONSCREEN;
    bool ignore = false;            /* gIgnoreSamusAndSpriteCollision */
    for (int i = 0; i < count && !ignore; ++i) {
        MzmSprite *s = &sprites[i];
        if ((s->status & check) != check || s->ignore_samus_timer) continue;
        uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
        uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
        if (!touching(samus_top, samus_bottom, samus_left, samus_right, top, bottom, left, right))
            continue;
        uint16_t center_x = (uint16_t)(left + (uint16_t)(right - left) / 2);
        bool on_left = center_x >= samus->previous_x;
        /* Frozen sprites are solid: not ported (no ice beam). */
        if (s->freeze_timer) continue;
        switch (s->samus_collision) {
        case SSC_NONE: break;
        case SSC_HURTS_SAMUS:
            if (contact_kills(s, samus, tables)) break;
            if (!samus->invincible && samus->hurt) {
                samus->hurt(samus->context, damage_of(s, tables), on_left);
                samus->invincible = true;
            }
            s->ignore_samus_timer = IGNORE_SAMUS_AFTER_HIT;
            ignore = true;
            break;
        case SSC_SMALL_ENERGY_DROP:
        case SSC_LARGE_ENERGY_DROP:
        case SSC_MISSILE_DROP:
        case SSC_SUPER_MISSILE_DROP:
        case SSC_POWER_BOMB_DROP:
            s->status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            ignore = true;
            break;
        default:                    /* no other collision is ported */
            s->ignore_samus_timer = IGNORE_SAMUS_AFTER_HIT;
            ignore = true;
            break;
        }
    }
    for (int i = 0; i < count; ++i)
        if (sprites[i].status & MZM_SPRITE_STATUS_EXISTS && sprites[i].ignore_samus_timer)
            sprites[i].ignore_samus_timer--;
}

/* ProjectileDealDamage */
static void deal_damage(MzmSprite *s, uint16_t damage) {
    if (s->health > damage) {
        s->health -= damage;
    } else {
        s->health = 0;
        s->freeze_timer = 0;
        s->palette_row = 0;
        s->pose = SPRITE_POSE_DESTROYED;
        s->ignore_samus_timer = 1;
    }
    set_isft(s, ISFT_DAMAGED);
}

/* ProjectileHitSolidSprite */
static void hit_solid(MzmSprite *s) {
    if ((s->isft & 0x7F) < ISFT_SOLID_HIT) set_isft(s, ISFT_SOLID_HIT);
}

void mzm_sprites_projectile_hits(MzmSprite *sprites, int count, MzmWeapons *weapons,
                                 const MzmSpriteTables *tables) {
    /* The sprites are visited by draw order, then slot; every ported sprite
     * has the default draw order, so slot order. A sprite's hitbox is read
     * once, before the projectiles that hit it. */
    for (int i = 0; i < count; ++i) {
        MzmSprite *s = &sprites[i];
        if ((s->status & (MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_IGNORE_PROJECTILES)) !=
                MZM_SPRITE_STATUS_EXISTS || s->health == 0)
            continue;
        uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
        uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
        uint8_t weakness = weakness_of(s, tables);
        for (int p = 0; p < MZM_MAX_PROJECTILES; ++p) {
            MzmProjectile *proj = &weapons->list[p];
            if (!proj->active) continue;
            /* ProjectileProcessNormalBeam/Missile: +-EIGHTH_BLOCK_SIZE;
             * ProjectileProcessSuperMissile: +-(QUARTER_BLOCK_SIZE - PIXEL_SIZE). */
            int half = proj->type == MZM_PROJECTILE_SUPER_MISSILE ? QUARTER_BLOCK_SIZE - PIXEL_SIZE
                                                                  : EIGHTH_BLOCK_SIZE;
            uint16_t py = (uint16_t)proj->y, px = (uint16_t)proj->x;
            if (!touching(top, bottom, left, right, (uint16_t)(py - half), (uint16_t)(py + half),
                          (uint16_t)(px - half), (uint16_t)(px + half)))
                continue;
            switch (proj->type) {
            case MZM_PROJECTILE_BEAM:   /* ProjectileHitSprite */
                if (weakness & MZM_WEAKNESS_BEAM_BOMBS) deal_damage(s, NORMAL_BEAM_DAMAGE);
                else hit_solid(s);
                break;
            case MZM_PROJECTILE_MISSILE:    /* ProjectileMissileHitSprite */
                if (weakness & MZM_WEAKNESS_MISSILES) deal_damage(s, MISSILE_DAMAGE);
                else hit_solid(s);  /* ProjectileStartTumblingMissile: not ported */
                break;
            case MZM_PROJECTILE_SUPER_MISSILE:
                if (weakness & (MZM_WEAKNESS_MISSILES | MZM_WEAKNESS_SUPER_MISSILES))
                    deal_damage(s, SUPER_MISSILE_DAMAGE);
                else hit_solid(s);
                break;
            }
            proj->active = false;
        }
    }
}

bool mzm_sprite_supported(uint8_t sprite_id) {
    return mzm_sprite_family(sprite_id, false) >= 0;
}

uint8_t mzm_sprite_graphics_owner(const MzmSprite *sprite) {
    if (!sprite->secondary) return sprite->sprite_id;
    return sprite->sprite_id == MZM_SSPRITE_BLUE_SKREE_EXPLOSION ? MZM_PSPRITE_SKREE_BLUE
                                                                  : MZM_PSPRITE_SKREE_GREEN;
}

const char *mzm_sprite_anim_name(const MzmSprite *sprite) {
    int family = mzm_sprite_family(sprite->sprite_id, sprite->secondary);
    if (family < 0 || sprite->anim >= mzm_sprite_families[family].count) return NULL;
    return mzm_sprite_families[family].anims[sprite->anim];
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
    if (left < sprite_x && sprite_x < right && bottom < sprite_y && sprite_y < top) {
        s->status |= MZM_SPRITE_STATUS_ONSCREEN;
        return;
    }
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_ONSCREEN;
    if (s->kill_off_screen) {
        /* Removed beyond 9 blocks above or left of the screen, 9 below and
         * 14 right of it (the native ranges). */
        bg_y = (uint16_t)(c->world->camera_y + BLOCK_SIZE * 10);
        sprite_y = (uint16_t)(s->y + BLOCK_SIZE * 10);
        bottom = (uint16_t)(bg_y - BLOCK_SIZE * 9);
        top = (uint16_t)(bg_y + BLOCK_SIZE * 19);
        bg_x = (uint16_t)(c->world->camera_x + BLOCK_SIZE * 10);
        sprite_x = (uint16_t)(s->x + BLOCK_SIZE * 10);
        left = (uint16_t)(bg_x - BLOCK_SIZE * 9);
        right = (uint16_t)(bg_x + BLOCK_SIZE * 24);
        if (left >= sprite_x || sprite_x >= right || bottom >= sprite_y || sprite_y >= top)
            s->status = 0;
    }
}

void mzm_sprite_update(MzmSprite *sprite, uint8_t slot, const MzmSpriteFrame *frame) {
    int family = mzm_sprite_family(sprite->sprite_id, sprite->secondary);
    if (!(sprite->status & MZM_SPRITE_STATUS_EXISTS) || family < 0) return;
    Ctx c = {sprite, frame, frame->world, frame->tables, frame->kinds[family],
             0, MZM_COLLISION_AIR, frame->frame8};
    /* gSpriteRng = sSpriteRandomNumberTable[(frame8 + slot + frame16 / 16 +
     * x + y) % 32] */
    c.rng = c.tables->random[((uint32_t)frame->frame8 + slot + frame->frame16 / 16u + sprite->x +
                              sprite->y) % 32u];
    /* SpriteUtilUpdateStunTimer: the stun palette flash is not drawn yet. */
    if (sprite->isft & 0x7F) sprite->isft--;
    switch (family) {
    case MZM_FAMILY_ZOOMER: zoomer(&c); break;
    case MZM_FAMILY_RIPPER: ripper(&c); break;
    case MZM_FAMILY_SKREE:
        if (sprite->secondary) skree_explosion(&c);
        else skree(&c);
        break;
    default: enemy_drop(&c); break;
    }
    if (sprite->status & MZM_SPRITE_STATUS_EXISTS) {
        /* A sprite that just became a drop finishes the frame on its
         * previous OAM (pOam is only set by EnemyDropInit), so `c.kind`
         * stays the one it started the frame with. */
        update_animation(&c);
        check_on_screen(&c);
    }
}
