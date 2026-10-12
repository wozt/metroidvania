/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission Chozo statues holding an item or a hint
 * (src/sprites_ai/chozo_statue.c: ChozoStatue, ChozoStatuePart) and their
 * item ball (chozo_ball.c: ChozoBall); comments name the pinned
 * decompilation's routines. The hint and refill grabs (Samus's
 * SPOSE_GRABBED_BY_CHOZO_STATUE, the hint map screen, the refill and the
 * sitting animation they lead to) are not ported: a hint statue stays
 * standing and an item statue stays seated once its item is taken. */
#include "mzm_items.h"
#include "mzm_sprite_ai.h"

#include <stddef.h>

enum {
    /* PrimarySprite */
    LONG_HINT = 34, LONG = 35, ICE_HINT = 36, ICE = 37, WAVE_HINT = 38, WAVE = 39,
    BOMB_HINT = 40, BOMB = 41, SPEED_HINT = 42, SPEED = 43, HIGH_JUMP_HINT = 44,
    HIGH_JUMP = 45, SCREW_HINT = 46, SCREW = 47, VARIA_HINT = 48, VARIA = 49,
    /* SecondarySprite */
    SSPRITE_CHOZO_BALL = 0, SSPRITE_CHOZO_STATUE_PART = 1,
    /* ChozoStatueBehavior */
    BEHAVIOR_ITEM = 0, BEHAVIOR_REFILL, BEHAVIOR_HINT, BEHAVIOR_HINT_TAKEN,
    /* ChozoStatuePartId */
    PART_ARM = 0, PART_GLOW, PART_EYE, PART_LEG, PART_HEAD,
    /* CHOZO_STATUE_POSE_* */
    STATUE_IDLE = 0x9, STATUE_WAIT_FOR_ITEM = 0x29, STATUE_TIMER_AFTER_ITEM = 0x2B,
    /* CHOZO_STATUE_PART_POSE_* */
    PART_ARM_CHECK_GRAB_HINT = 0x9, PART_LEG_IDLE = 0xF, PART_ARM_CHECK_GRAB_REFILL = 0x25,
    PART_GLOW_IDLE = 0x4F, PART_DO_NOTHING = 0x61,
    /* CHOZO_BALL_POSE_* */
    BALL_DO_NOTHING = 0x8, BALL_IDLE = 0x9, BALL_GETTING = 0x23, BALL_REVEALING = 0x67,
    /* EventType */
    EVENT_STATUE_LONG_BEAM_GRABBED = 8, EVENT_STATUE_BOMBS_GRABBED = 9,
    EVENT_STATUE_ICE_BEAM_GRABBED = 10, EVENT_STATUE_SPEEDBOOSTER_GRABBED = 11,
    EVENT_STATUE_HIGH_JUMP_GRABBED = 12, EVENT_STATUE_VARIA_SUIT_GRABBED = 13,
    EVENT_STATUE_WAVE_BEAM_GRABBED = 14, EVENT_STATUE_SCREW_ATTACK_GRABBED = 15,
    EVENT_HIGH_JUMP_OBTAINED = 18, EVENT_VARIA_SUIT_OBTAINED = 19,
    EVENT_SCREW_ATTACK_OBTAINED = 21,
    /* MessageId */
    MESSAGE_LONG_BEAM = 8, MESSAGE_ICE_BEAM = 10, MESSAGE_WAVE_BEAM = 11, MESSAGE_BOMB = 13,
    MESSAGE_VARIA_SUIT = 14, MESSAGE_SPEED_BOOSTER = 17, MESSAGE_HIGH_JUMP = 18,
    MESSAGE_SCREW_ATTACK = 19,
    /* CHOZO_STATUE_HAND_*, CHOZO_BALL_OFFSET_* */
    BALL_OFFSET_Y = BLOCK_SIZE + HALF_BLOCK_SIZE,
    BALL_OFFSET_X = BLOCK_SIZE - EIGHTH_BLOCK_SIZE,
    TWO_THIRD_SECOND = 40,
};

const char *const mzm_chozo_statue_anim_names[MZM_CHOZO_ANIMS] = {
    "ChozoStatuePartOam_LegStanding", "ChozoStatuePartOam_LegSitting",
    "ChozoStatuePartOam_LegSeated", "ChozoStatueOam_Idle", "ChozoStatuePartOam_EyeOpened",
    "ChozoStatuePartOam_EyeClosing", "ChozoStatuePartOam_EyeOpening",
    "ChozoStatuePartOam_EyeClosed", "ChozoStatuePartOam_ArmIdle", "ChozoStatuePartOam_ArmGlow",
    "ChozoStatuePartOam_ArmSamusGrabbed", "ChozoBallOam_NormalClosed",
    "ChozoBallOam_NormalRevealing", "ChozoBallOam_NormalRevealed", "ChozoStatueRefillOam",
    "ChozoStatuePartOam_GlowIdle",
};

const char *const mzm_chozo_multi_names[MZM_CHOZO_MULTI_COUNT] = {
    "sChozoStatueMultiSpriteData_Standing", "sChozoStatueMultiSpriteData_Sitting",
    "sChozoStatueMultiSpriteData_Seated",
};

/* The item flag, the hint's "grabbed" event and the item's "obtained" event
 * (0: none) and message of each item statue. */
static const struct {
    uint8_t statue;
    uint32_t item;
    uint8_t grabbed, obtained, message;
} items[] = {
    {LONG, MZM_ITEM_LONG_BEAM, EVENT_STATUE_LONG_BEAM_GRABBED, 0, MESSAGE_LONG_BEAM},
    {ICE, MZM_ITEM_ICE_BEAM, EVENT_STATUE_ICE_BEAM_GRABBED, 0, MESSAGE_ICE_BEAM},
    {WAVE, MZM_ITEM_WAVE_BEAM, EVENT_STATUE_WAVE_BEAM_GRABBED, 0, MESSAGE_WAVE_BEAM},
    {BOMB, MZM_ITEM_BOMBS, EVENT_STATUE_BOMBS_GRABBED, 0, MESSAGE_BOMB},
    {SPEED, MZM_ITEM_SPEED_BOOSTER, EVENT_STATUE_SPEEDBOOSTER_GRABBED, 0, MESSAGE_SPEED_BOOSTER},
    {HIGH_JUMP, MZM_ITEM_HIGH_JUMP, EVENT_STATUE_HIGH_JUMP_GRABBED, EVENT_HIGH_JUMP_OBTAINED,
     MESSAGE_HIGH_JUMP},
    {SCREW, MZM_ITEM_SCREW_ATTACK, EVENT_STATUE_SCREW_ATTACK_GRABBED, EVENT_SCREW_ATTACK_OBTAINED,
     MESSAGE_SCREW_ATTACK},
    {VARIA, MZM_ITEM_VARIA_SUIT, EVENT_STATUE_VARIA_SUIT_GRABBED, EVENT_VARIA_SUIT_OBTAINED,
     MESSAGE_VARIA_SUIT},
};

/* Item statues are the odd ids, each hint the id just before. */
static int item_index(uint8_t statue) {
    for (size_t i = 0; i < sizeof items / sizeof items[0]; ++i)
        if (items[i].statue == statue || items[i].statue == statue + 1) return (int)i;
    return -1;
}

static void set_event(const Ctx *c, uint8_t event) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->set_event) h->set_event(h->context, event);
}

/* ChozoStatueRegisterItem: the item (for an item statue) and the events. */
static void register_item(const Ctx *c, uint8_t statue) {
    int i = item_index(statue);
    if (i < 0) return;
    if (statue == items[i].statue) {
        if (c->f->equipment) c->f->equipment->items |= items[i].item;
        if (items[i].obtained) set_event(c, items[i].obtained);
    }
    set_event(c, items[i].grabbed);
}

/* ChozoStatueGetBehavior */
static int get_behavior(const Ctx *c, uint8_t statue) {
    int i = item_index(statue);
    if (i < 0) return BEHAVIOR_ITEM;
    if (statue != items[i].statue)
        return ai_check_event(c, items[i].grabbed) ? BEHAVIOR_HINT_TAKEN : BEHAVIOR_HINT;
    bool has = c->f->equipment && c->f->equipment->items & items[i].item;
    return has ? BEHAVIOR_REFILL : BEHAVIOR_ITEM;
}

/* ChozoStatueSetDirection */
static bool faces_right(uint8_t statue) {
    switch (statue) {
    case LONG: case ICE_HINT: case ICE: case WAVE_HINT: case WAVE: case BOMB: case SPEED:
    case HIGH_JUMP: case SCREW: case VARIA_HINT: case VARIA: return true;
    default: return false;
    }
}

static void apply_caa(const Ctx *c, uint16_t y, uint16_t x, uint8_t caa) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->apply_caa) h->apply_caa(h->context, y, x, caa);
}

/* ChozoStatueStandingChangeClipdata */
static void standing_clipdata(const Ctx *c, uint8_t body, uint8_t hand) {
    const MzmSpriteGlobals *g = c->f->globals;
    uint16_t y = g->sub1.y, x = g->sub1.x;
    if (c->s->status & MZM_SPRITE_STATUS_X_FLIP) {
        x += HALF_BLOCK_SIZE;
        apply_caa(c, (uint16_t)(y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE)), (uint16_t)(x + BLOCK_SIZE * 2), hand);
        apply_caa(c, (uint16_t)(y - (BLOCK_SIZE * 4 + HALF_BLOCK_SIZE)), (uint16_t)(x + BLOCK_SIZE), body);
        apply_caa(c, (uint16_t)(y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE)), (uint16_t)(x + BLOCK_SIZE), body);
    } else {
        x -= HALF_BLOCK_SIZE;
        apply_caa(c, (uint16_t)(y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE)), (uint16_t)(x - BLOCK_SIZE * 2), hand);
        apply_caa(c, (uint16_t)(y - (BLOCK_SIZE * 4 + HALF_BLOCK_SIZE)), (uint16_t)(x - BLOCK_SIZE), body);
        apply_caa(c, (uint16_t)(y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE)), (uint16_t)(x - BLOCK_SIZE), body);
    }
    for (int row = 0; row < 5; ++row)
        apply_caa(c, (uint16_t)(y - (BLOCK_SIZE * row + HALF_BLOCK_SIZE)), x, body);
}

/* ChozoStatueSeatedChangeClipdata */
static void seated_clipdata(const Ctx *c, uint8_t caa) {
    const MzmSpriteGlobals *g = c->f->globals;
    uint16_t y = g->sub1.y, x = g->sub1.x;
    if (c->s->status & MZM_SPRITE_STATUS_X_FLIP) {
        apply_caa(c, (uint16_t)(y - HALF_BLOCK_SIZE), (uint16_t)(x + HALF_BLOCK_SIZE), caa);
        x -= HALF_BLOCK_SIZE;
    } else {
        apply_caa(c, (uint16_t)(y - HALF_BLOCK_SIZE), (uint16_t)(x - HALF_BLOCK_SIZE), caa);
        x += HALF_BLOCK_SIZE;
    }
    for (int row = 0; row < 3; ++row)
        apply_caa(c, (uint16_t)(y - (BLOCK_SIZE * row + HALF_BLOCK_SIZE)), x, caa);
}

static const int16_t *multi_part(const Ctx *c, uint8_t part) {
    const MzmSpriteGlobals *g = c->f->globals;
    const MzmMultiSprite *m = &c->tables->chozo_multi[g->sub1.multi];
    return m->parts[g->sub1.frame < MZM_MULTI_FRAMES ? g->sub1.frame : 0][part];
}

/* ChozoStatueSyncSubSprites: the part's OAM and position from the
 * multi-sprite frame. */
static void sync_sub_sprites(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSpriteGlobals *g = c->f->globals;
    const int16_t *p = multi_part(c, s->room_slot < MZM_MULTI_PARTS ? s->room_slot : 0);
    if (s->anim != (uint8_t)p[0]) {
        s->anim = (uint8_t)p[0];
        s->adc = 0;
        s->frame = 0;
    }
    s->y = (uint16_t)(g->sub1.y + p[1]);
    s->x = (uint16_t)(s->status & MZM_SPRITE_STATUS_X_FLIP ? g->sub1.x - p[2] : g->sub1.x + p[2]);
}

/* SpriteUtilSyncCurrentSpritePositionWithSubSpriteData1PositionAndOam */
static void sync_position(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSpriteGlobals *g = c->f->globals;
    const int16_t *p = multi_part(c, s->room_slot < MZM_MULTI_PARTS ? s->room_slot : 0);
    s->y = (uint16_t)(g->sub1.y + p[1]);
    s->x = (uint16_t)(s->status & MZM_SPRITE_STATUS_X_FLIP ? g->sub1.x - p[2] : g->sub1.x + p[2]);
}

/* SpriteUtilUpdateSubSprite1Anim */
static void update_sub_sprite(Ctx *c) {
    MzmSpriteGlobals *g = c->f->globals;
    const MzmMultiSprite *m = &c->tables->chozo_multi[g->sub1.multi];
    g->sub1.adc++;
    if (m->timers[g->sub1.frame] < g->sub1.adc) {
        g->sub1.adc = 1;
        g->sub1.frame++;
        if (g->sub1.frame >= MZM_MULTI_FRAMES || m->timers[g->sub1.frame] == 0) g->sub1.frame = 0;
    }
}

static int spawn_part(Ctx *c, uint8_t sprite_id, uint8_t part, uint16_t y, uint16_t x,
                      uint16_t status) {
    return ai_spawn_secondary(c, sprite_id, part, c->s->gfx_slot, y, x, status);
}

/* ChozoStatueInit */
static void statue_init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    s->always_active = true;
    if (faces_right(s->sprite_id)) s->status |= MZM_SPRITE_STATUS_X_FLIP;
    g->sub1.x = (uint16_t)(s->status & MZM_SPRITE_STATUS_X_FLIP ? s->x + HALF_BLOCK_SIZE
                                                                : s->x - HALF_BLOCK_SIZE);
    g->sub1.y = s->y;
    s->draw_top = BLOCK_SIZE * 2 / PIXEL_SIZE;
    s->draw_bottom = BLOCK_SIZE * 2 / PIXEL_SIZE;
    s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = PIXEL_SIZE;
    s->draw_order = 3;
    s->samus_collision = SSC_NONE;
    s->health = 1;
    g->sub1.adc = 0;
    g->sub1.frame = 0;
    g->sub1.work2 = 0;
    g->sub1.work3 = false;
    int behavior = get_behavior(c, s->sprite_id);
    if (behavior > BEHAVIOR_REFILL) {
        s->pose = STATUE_IDLE;
        if (behavior == BEHAVIOR_HINT_TAKEN) {
            g->sub1.multi = MZM_CHOZO_MULTI_SEATED;
            seated_clipdata(c, MZM_CAA_MAKE_NON_POWER_GRIP);
        } else {
            g->sub1.work3 = true;
            g->sub1.multi = MZM_CHOZO_MULTI_STANDING;
            standing_clipdata(c, MZM_CAA_MAKE_NON_POWER_GRIP, MZM_CAA_MAKE_SOLID_GRIPPABLE);
        }
    } else {
        g->sub1.multi = MZM_CHOZO_MULTI_SEATED;
        seated_clipdata(c, MZM_CAA_MAKE_NON_POWER_GRIP);
        if (behavior == BEHAVIOR_ITEM) {
            s->pose = STATUE_WAIT_FOR_ITEM;
            spawn_part(c, SSPRITE_CHOZO_BALL, 0, (uint16_t)(g->sub1.y - BALL_OFFSET_Y),
                       (uint16_t)(s->status & MZM_SPRITE_STATUS_X_FLIP ? g->sub1.x + BALL_OFFSET_X
                                                                       : g->sub1.x - BALL_OFFSET_X),
                       0);
        } else {
            s->pose = STATUE_IDLE;
        }
    }
    s->room_slot = PART_HEAD;
    uint16_t y = g->sub1.y, x = g->sub1.x, flip = s->status & MZM_SPRITE_STATUS_X_FLIP;
    s->work1 = (uint8_t)spawn_part(c, SSPRITE_CHOZO_STATUE_PART, PART_EYE, y, x, flip);
    int arm = spawn_part(c, SSPRITE_CHOZO_STATUE_PART, PART_ARM, y, x, flip);
    spawn_part(c, SSPRITE_CHOZO_STATUE_PART, PART_LEG, y, x, flip);
    int glow = spawn_part(c, SSPRITE_CHOZO_STATUE_PART, PART_GLOW, y, x, flip);
    if (glow >= 0 && c->f->sprites) c->f->sprites[glow].work1 = (uint8_t)arm;
}

/* ChozoStatue */
static void statue(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: statue_init(c); break;
    case STATUE_WAIT_FOR_ITEM:
        /* ChozoStatueWaitForItemToBeCollected */
        if (get_behavior(c, s->sprite_id) == BEHAVIOR_REFILL) {
            s->pose = STATUE_TIMER_AFTER_ITEM;
            s->work0 = TWO_THIRD_SECOND;
        }
        break;
    case STATUE_TIMER_AFTER_ITEM:
        /* ChozoStatueTimerAfterItemGrabbed */
        if (--s->work0 == 0) s->pose = STATUE_IDLE;
        break;
    default: break;             /* the hint and refill poses are not ported */
    }
    update_sub_sprite(c);
    sync_sub_sprites(c);
}

static MzmSprite *parent(const Ctx *c) {
    return c->f->sprites && c->s->ram_slot < c->f->sprite_count ? &c->f->sprites[c->s->ram_slot]
                                                                : NULL;
}

/* ChozoStatuePartInit */
static void part_init(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSprite *p = parent(c);
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->always_active = true;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->samus_collision = SSC_NONE;
    s->draw_order = 2;
    s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = PIXEL_SIZE;
    int behavior = p ? get_behavior(c, p->sprite_id) : BEHAVIOR_ITEM;
    switch (s->room_slot) {
    case PART_ARM:
        s->draw_top = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_bottom = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = (BLOCK_SIZE * 2 - EIGHTH_BLOCK_SIZE) / PIXEL_SIZE;
        s->pose = behavior == BEHAVIOR_HINT ? PART_ARM_CHECK_GRAB_HINT : PART_ARM_CHECK_GRAB_REFILL;
        ai_set_anim(s, behavior == BEHAVIOR_ITEM ? MZM_CHOZO_ARM_IDLE : MZM_CHOZO_ARM_GLOW);
        break;
    case PART_LEG:
        s->draw_top = BLOCK_SIZE * 3 / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->pose = behavior == BEHAVIOR_HINT ? PART_LEG_IDLE : PART_DO_NOTHING;
        break;
    case PART_EYE:
        s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->pose = PART_DO_NOTHING;
        ai_set_anim(s, behavior == BEHAVIOR_HINT ? MZM_CHOZO_EYE_CLOSED : MZM_CHOZO_EYE_OPENED);
        break;
    case PART_GLOW:
        s->draw_top = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_bottom = 1;
        s->draw_horizontal = THREE_QUARTER_BLOCK_SIZE / PIXEL_SIZE;
        ai_set_anim(s, MZM_CHOZO_REFILL_GLOW_IDLE);
        s->pose = PART_GLOW_IDLE;
        s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
        break;
    default:
        s->status = 0;
    }
}

/* ChozoStatuePart */
static void part(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    const MzmSprite *p = parent(c);
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: part_init(c); break;
    case PART_ARM_CHECK_GRAB_REFILL:
        /* ChozoStatuePartArmCheckGrabSamusRefill: the glow once the item is
         * taken (the grab itself is not ported). */
        if (s->anim == MZM_CHOZO_ARM_IDLE && c->f->prevent_movement &&
            *c->f->prevent_movement == 0 && p && get_behavior(c, p->sprite_id) != BEHAVIOR_ITEM)
            s->anim = MZM_CHOZO_ARM_GLOW;
        break;
    case PART_GLOW_IDLE: {
        /* ChozoStatuePartGlowIdle */
        const MzmSprite *arm = c->f->sprites && s->work1 < c->f->sprite_count
                                   ? &c->f->sprites[s->work1] : NULL;
        if (arm && arm->anim == MZM_CHOZO_ARM_GLOW) {
            if (s->status & MZM_SPRITE_STATUS_NOT_DRAWN) {
                s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
                s->adc = 0;
                s->frame = 0;
            }
        } else {
            s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
        }
        break;
    }
    default: break;             /* the hint grab, sitting and refill are not ported */
    }
    if (!(s->status & MZM_SPRITE_STATUS_EXISTS)) return;
    if (s->room_slot == PART_LEG) sync_sub_sprites(c);
    else sync_position(c);
    if (p) s->palette_row = p->palette_row;
}

/* ChozoBall */
static void ball(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSprite *p = parent(c);
    uint16_t *prevent = c->f->prevent_movement;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* ChozoBallInit (the unknown items' tables are not ported) */
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->hitbox_top = s->hitbox_left = -(QUARTER_BLOCK_SIZE + PIXEL_SIZE * 3);
        s->hitbox_bottom = s->hitbox_right = QUARTER_BLOCK_SIZE + PIXEL_SIZE * 3;
        s->draw_top = s->draw_bottom = s->draw_horizontal = THREE_QUARTER_BLOCK_SIZE / PIXEL_SIZE;
        s->samus_collision = SSC_SOLID;
        s->health = 1;
        s->pose = BALL_DO_NOTHING;
        ai_set_anim(s, MZM_CHOZO_BALL_NORMAL_CLOSED);
        break;
    case BALL_DO_NOTHING: break;
    case BALL_REVEALING:
        /* ChozoBallCheckRevealingAnimEnded */
        if (ai_animation_ending(c, 1)) {
            s->pose = BALL_IDLE;
            ai_set_anim(s, MZM_CHOZO_BALL_NORMAL_REVEALED);
        }
        break;
    case BALL_IDLE:
        /* ChozoBallRegisterItem */
        if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
            if (prevent) *prevent = MZM_SAMUS_ITEM_PMT;
            s->always_active = true;
            s->ignore_samus_timer = 1;
            s->pose = BALL_GETTING;
            s->work0 = 0;
            if (p) {
                register_item(c, p->sprite_id);
                int i = item_index(p->sprite_id);
                const MzmSpriteHooks *h = c->f->hooks;
                if (i >= 0 && h && h->spawn_message) h->spawn_message(h->context, items[i].message);
            }
        }
        break;
    case BALL_GETTING:
        /* ChozoBallFlashAnimation (its flicker timer never moves) */
        s->ignore_samus_timer = 1;
        if ((s->work0 & 1) == 0) s->status ^= MZM_SPRITE_STATUS_NOT_DRAWN;
        if (prevent && *prevent < MZM_SAMUS_ITEM_PMT - 1) s->status = 0;
        break;
    default:
        /* ChozoBallRevealingInit: shot (its "death" pose) */
        s->immune_to_projectiles = true;
        s->health = 1;
        s->samus_collision = SSC_CHECK_COLLIDING;
        s->pose = BALL_REVEALING;
        s->palette_row = s->absolute_palette_row;
        s->isft &= 0x80;
        ai_set_anim(s, MZM_CHOZO_BALL_NORMAL_REVEALING);
        mzm_sfx_play(MZM_SOUND_CHOZO_BALL_REVEALING);
        break;
    }
}

void ai_chozo_statue(Ctx *c) {
    if (!c->f->globals) return;
    if (!c->s->secondary) statue(c);
    else if (c->s->sprite_id == SSPRITE_CHOZO_BALL) ball(c);
    else part(c);
}
