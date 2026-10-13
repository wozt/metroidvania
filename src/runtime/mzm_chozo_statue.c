/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission Chozo statues holding an item or a hint
 * (src/sprites_ai/chozo_statue.c: ChozoStatue, ChozoStatuePart,
 * ChozoStatueRefill), their item ball (chozo_ball.c: ChozoBall) and the
 * sitting echo (chozo_statue_movement.c); comments name the pinned
 * decompilation's routines. A hint statue opens the hint's map screen
 * (PAUSE_SCREEN_CHOZO_HINT, mzm_pause_screen.c) before sitting down; the
 * background flashes (MakeBackgroundFlash) and the slow scrolling are not
 * ported. */
#include "mzm_items.h"
#include "mzm_sprite_ai.h"

#include <stddef.h>
#include <string.h>

enum {
    /* PrimarySprite */
    LONG_HINT = 34, LONG = 35, ICE_HINT = 36, ICE = 37, WAVE_HINT = 38, WAVE = 39,
    BOMB_HINT = 40, BOMB = 41, SPEED_HINT = 42, SPEED = 43, HIGH_JUMP_HINT = 44,
    HIGH_JUMP = 45, SCREW_HINT = 46, SCREW = 47, VARIA_HINT = 48, VARIA = 49,
    /* SecondarySprite */
    SSPRITE_CHOZO_BALL = 0, SSPRITE_CHOZO_STATUE_PART = 1, SSPRITE_CHOZO_STATUE_REFILL = 2,
    SSPRITE_CHOZO_STATUE_MOVEMENT = 4, SSPRITE_UNKNOWN_ITEM_CHOZO_STATUE_PART = 34,
    SSPRITE_UNKNOWN_ITEM_CHOZO_STATUE_REFILL = 35,
    /* The unknown item statues (UnknownItemChozoStatue) */
    GRAVITY = 88, SPACE_JUMP = 89, PLASMA_BEAM = 148,
    /* ChozoStatueBehavior */
    BEHAVIOR_ITEM = 0, BEHAVIOR_REFILL, BEHAVIOR_HINT, BEHAVIOR_HINT_TAKEN,
    /* ChozoStatuePartId */
    PART_ARM = 0, PART_GLOW, PART_EYE, PART_LEG, PART_HEAD,
    /* CHOZO_STATUE_POSE_* */
    STATUE_IDLE = 0x9, STATUE_DO_NOTHING = 0xF, STATUE_REGISTER_HINT = 0x22,
    STATUE_HINT_FLASHING = 0x23, STATUE_SITTING_INIT = 0x24, STATUE_SITTING = 0x25,
    STATUE_DELAY_AFTER_SITTING = 0x27, STATUE_WAIT_FOR_ITEM = 0x29,
    STATUE_TIMER_AFTER_ITEM = 0x2B, STATUE_REFILL_INIT = 0x2E, STATUE_REFILL = 0x2F,
    STATUE_SLEEPING_INIT = 0x30, STATUE_SLEEPING = 0x31,
    /* CHOZO_STATUE_PART_POSE_* */
    PART_ARM_CHECK_GRAB_HINT = 0x9, PART_LEG_IDLE = 0xF, PART_ARM_SITTING = 0x23,
    PART_ARM_SEATED = 0x24, PART_ARM_CHECK_GRAB_REFILL = 0x25, PART_ARM_REFILL = 0x27,
    PART_ARM_SLEEPING_INIT = 0x29, PART_ARM_SLEEPING = 0x2B, PART_EYE_OPENING_INIT = 0x42,
    PART_EYE_OPENING = 0x43, PART_EYE_CLOSING_INIT = 0x44, PART_EYE_CLOSING = 0x45,
    PART_GLOW_IDLE = 0x4F, PART_DO_NOTHING = 0x61,
    REFILL_IDLE = 0x9,
    /* CHOZO_BALL_POSE_* */
    BALL_DO_NOTHING = 0x8, BALL_IDLE = 0x9, BALL_GETTING = 0x23, BALL_REVEALING = 0x67,
    /* EventType */
    EVENT_STATUE_LONG_BEAM_GRABBED = 8, EVENT_STATUE_BOMBS_GRABBED = 9,
    EVENT_STATUE_ICE_BEAM_GRABBED = 10, EVENT_STATUE_SPEEDBOOSTER_GRABBED = 11,
    EVENT_STATUE_HIGH_JUMP_GRABBED = 12, EVENT_STATUE_VARIA_SUIT_GRABBED = 13,
    EVENT_STATUE_WAVE_BEAM_GRABBED = 14, EVENT_STATUE_SCREW_ATTACK_GRABBED = 15,
    EVENT_HIGH_JUMP_OBTAINED = 18, EVENT_VARIA_SUIT_OBTAINED = 19,
    EVENT_SCREW_ATTACK_OBTAINED = 21, EVENT_SPACE_JUMP_OBTAINED = 22,
    EVENT_GRAVITY_SUIT_OBTAINED = 23, EVENT_PLASMA_BEAM_OBTAINED = 24,
    /* MessageId */
    MESSAGE_LONG_BEAM = 8, MESSAGE_ICE_BEAM = 10, MESSAGE_WAVE_BEAM = 11, MESSAGE_BOMB = 13,
    MESSAGE_VARIA_SUIT = 14, MESSAGE_UNKNOWN_ITEM_PLASMA = 12, MESSAGE_UNKNOWN_ITEM_GRAVITY = 15,
    MESSAGE_UNKNOWN_ITEM_SPACE_JUMP = 20, MESSAGE_SPEED_BOOSTER = 17, MESSAGE_HIGH_JUMP = 18,
    MESSAGE_SCREW_ATTACK = 19, MESSAGE_ENERGY_TANK_RECHARGE_COMPLETE = 25,
    MESSAGE_WEAPONS_AND_ENERGY_RESTORED = 26,
    /* CHOZO_STATUE_HAND_X_OFFSET, CHOZO_STATUE_HAND_Y_OFFSET */
    HAND_X_OFFSET = BLOCK_SIZE - EIGHTH_BLOCK_SIZE,
    HAND_Y_OFFSET = QUARTER_BLOCK_SIZE + 1,
    DELAY_BEFORE_HINT = 120,        /* CONVERT_SECONDS(2.f) */
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
    "UnknownItemChozoStatuePartOam_LegStanding", "UnknownItemChozoStatuePartOam_LegSitting",
    "UnknownItemChozoStatuePartOam_LegSeated", "UnknownItemChozoStatueOam_Idle",
    "UnknownItemChozoStatuePartOam_EyeOpened", "UnknownItemChozoStatuePartOam_EyeClosing",
    "UnknownItemChozoStatuePartOam_EyeOpening", "UnknownItemChozoStatuePartOam_EyeClosed",
    "UnknownItemChozoStatuePartOam_ArmIdle", "UnknownItemChozoStatuePartOam_ArmGlow",
    "UnknownItemChozoStatuePartOam_ArmSamusGrabbed", "ChozoBallOam_UnknownClosed",
    "ChozoBallOam_UnknownRevealing", "ChozoBallOam_UnknownRevealed",
    "UnknownItemChozoStatueRefillOam", "UnknownItemChozoStatuePartOam_GlowIdle",
};

/* sChozoStatueFlashingPaletteRows */
static const uint8_t flashing_rows[4] = {1, 2, 3, 2};

const char *const mzm_chozo_multi_names[MZM_CHOZO_MULTI_COUNT] = {
    "sChozoStatueMultiSpriteData_Standing", "sChozoStatueMultiSpriteData_Sitting",
    "sChozoStatueMultiSpriteData_Seated", "sUnknownItemChozoStatueMultiSpriteData_Standing",
    "sUnknownItemChozoStatueMultiSpriteData_Sitting",
    "sUnknownItemChozoStatueMultiSpriteData_Seated",
};

/* The item flag, the hint's "grabbed" event (0: no hint statue) and the
 * item's "obtained" event (0: none) and message of each item statue; the
 * unknown items are obtained without being activated. */
static const struct {
    uint8_t statue;
    uint32_t item;
    uint8_t grabbed, obtained, message;
    bool unknown;
} items[] = {
    {LONG, MZM_ITEM_LONG_BEAM, EVENT_STATUE_LONG_BEAM_GRABBED, 0, MESSAGE_LONG_BEAM, false},
    {ICE, MZM_ITEM_ICE_BEAM, EVENT_STATUE_ICE_BEAM_GRABBED, 0, MESSAGE_ICE_BEAM, false},
    {WAVE, MZM_ITEM_WAVE_BEAM, EVENT_STATUE_WAVE_BEAM_GRABBED, 0, MESSAGE_WAVE_BEAM, false},
    {BOMB, MZM_ITEM_BOMBS, EVENT_STATUE_BOMBS_GRABBED, 0, MESSAGE_BOMB, false},
    {SPEED, MZM_ITEM_SPEED_BOOSTER, EVENT_STATUE_SPEEDBOOSTER_GRABBED, 0, MESSAGE_SPEED_BOOSTER, false},
    {HIGH_JUMP, MZM_ITEM_HIGH_JUMP, EVENT_STATUE_HIGH_JUMP_GRABBED, EVENT_HIGH_JUMP_OBTAINED,
     MESSAGE_HIGH_JUMP, false},
    {SCREW, MZM_ITEM_SCREW_ATTACK, EVENT_STATUE_SCREW_ATTACK_GRABBED, EVENT_SCREW_ATTACK_OBTAINED,
     MESSAGE_SCREW_ATTACK, false},
    {VARIA, MZM_ITEM_VARIA_SUIT, EVENT_STATUE_VARIA_SUIT_GRABBED, EVENT_VARIA_SUIT_OBTAINED,
     MESSAGE_VARIA_SUIT, false},
    {SPACE_JUMP, MZM_ITEM_SPACE_JUMP, 0, EVENT_SPACE_JUMP_OBTAINED,
     MESSAGE_UNKNOWN_ITEM_SPACE_JUMP, true},
    {GRAVITY, MZM_ITEM_GRAVITY_SUIT, 0, EVENT_GRAVITY_SUIT_OBTAINED,
     MESSAGE_UNKNOWN_ITEM_GRAVITY, true},
    {PLASMA_BEAM, MZM_ITEM_PLASMA_BEAM, 0, EVENT_PLASMA_BEAM_OBTAINED,
     MESSAGE_UNKNOWN_ITEM_PLASMA, true},
};

/* Each hint statue is the id just before its item statue. */
static int item_index(uint8_t statue) {
    for (size_t i = 0; i < sizeof items / sizeof items[0]; ++i)
        if (items[i].statue == statue || (items[i].grabbed && items[i].statue == statue + 1))
            return (int)i;
    return -1;
}

static bool is_unknown(uint8_t statue) {
    return statue == GRAVITY || statue == SPACE_JUMP || statue == PLASMA_BEAM;
}

/* The statue a sprite of the family belongs to. */
static uint8_t statue_of(const MzmSprite *s) {
    return s->secondary ? s->owner : s->sprite_id;
}

/* UnknownItemChozoStatue uses its own OAM and multi-sprite tables. */
static uint8_t anim_base(const MzmSprite *s) {
    return is_unknown(statue_of(s)) ? MZM_CHOZO_UNKNOWN_BASE : 0;
}

static uint8_t multi_base(const MzmSprite *s) {
    return is_unknown(statue_of(s)) ? MZM_CHOZO_MULTI_UNKNOWN : 0;
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
        if (c->f->equipment) {
            if (items[i].unknown) c->f->equipment->unknown_items |= items[i].item;
            else c->f->equipment->items |= items[i].item;
        }
        if (items[i].obtained) set_event(c, items[i].obtained);
    }
    if (items[i].grabbed) set_event(c, items[i].grabbed);
}

/* ChozoStatueGetBehavior */
static int get_behavior(const Ctx *c, uint8_t statue) {
    int i = item_index(statue);
    if (i < 0) return BEHAVIOR_ITEM;
    if (statue != items[i].statue)
        return ai_check_event(c, items[i].grabbed) ? BEHAVIOR_HINT_TAKEN : BEHAVIOR_HINT;
    bool has = c->f->equipment &&
               (c->f->equipment->items | c->f->equipment->unknown_items) & items[i].item;
    return has ? BEHAVIOR_REFILL : BEHAVIOR_ITEM;
}

/* ChozoStatueSetDirection */
static bool faces_right(uint8_t statue) {
    switch (statue) {
    case LONG: case ICE_HINT: case ICE: case WAVE_HINT: case WAVE: case BOMB: case SPEED:
    case HIGH_JUMP: case SCREW: case VARIA_HINT: case VARIA: case GRAVITY: return true;
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
    uint8_t anim = (uint8_t)(anim_base(s) + p[0]);
    if (s->anim != anim) {
        s->anim = anim;
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
            g->sub1.multi = (uint8_t)(multi_base(s) + MZM_CHOZO_MULTI_SEATED);
            seated_clipdata(c, MZM_CAA_MAKE_NON_POWER_GRIP);
        } else {
            g->sub1.work3 = true;
            g->sub1.multi = (uint8_t)(multi_base(s) + MZM_CHOZO_MULTI_STANDING);
            standing_clipdata(c, MZM_CAA_MAKE_NON_POWER_GRIP, MZM_CAA_MAKE_SOLID_GRIPPABLE);
        }
    } else {
        g->sub1.multi = (uint8_t)(multi_base(s) + MZM_CHOZO_MULTI_SEATED);
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
    uint8_t part_id = is_unknown(s->sprite_id) ? SSPRITE_UNKNOWN_ITEM_CHOZO_STATUE_PART
                                               : SSPRITE_CHOZO_STATUE_PART;
    s->work1 = (uint8_t)spawn_part(c, part_id, PART_EYE, y, x, flip);
    int arm = spawn_part(c, part_id, PART_ARM, y, x, flip);
    spawn_part(c, part_id, PART_LEG, y, x, flip);
    int glow = spawn_part(c, part_id, PART_GLOW, y, x, flip);
    if (glow >= 0 && c->f->sprites) c->f->sprites[glow].work1 = (uint8_t)arm;
}

/* SpriteUtilUpdateSubSprite1Timer: work2 is the next frame + 1 when the
 * multi-sprite frame is about to change. */
static void update_sub_sprite_timer(Ctx *c) {
    MzmSpriteGlobals *g = c->f->globals;
    const MzmMultiSprite *m = &c->tables->chozo_multi[g->sub1.multi];
    g->sub1.work2 = 0;
    if (m->timers[g->sub1.frame] < (uint8_t)(g->sub1.adc + 1)) g->sub1.work2 = (uint8_t)(g->sub1.frame + 1);
}

/* SpriteUtilHasSubSprite1AnimationEnded */
static bool sub_sprite_ended(const Ctx *c) {
    const MzmSpriteGlobals *g = c->f->globals;
    const MzmMultiSprite *m = &c->tables->chozo_multi[g->sub1.multi];
    uint8_t adc = (uint8_t)(g->sub1.adc + 1);
    uint8_t next = (uint8_t)(g->sub1.frame + 1);
    return m->timers[g->sub1.frame] < adc && (next >= MZM_MULTI_FRAMES || m->timers[next] == 0);
}

static void set_multi(MzmSpriteGlobals *g, uint8_t multi) {
    g->sub1.multi = multi;
    g->sub1.adc = 0;
    g->sub1.frame = 0;
}

/* SpawnChozoStatueMovement: a flickering copy of the current sprite. */
static void spawn_movement(Ctx *c, uint8_t stage) {
    MzmSprite *s = c->s;
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        MzmSprite *n = &c->f->sprites[i];
        if (n->status & MZM_SPRITE_STATUS_EXISTS) continue;
        memset(n, 0, sizeof *n);
        n->status = (uint16_t)((s->status & MZM_SPRITE_STATUS_X_FLIP) | MZM_SPRITE_STATUS_EXISTS |
                               MZM_SPRITE_STATUS_ONSCREEN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
        if (!(stage & 1)) n->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
        uint8_t order = s->draw_order ? s->draw_order : 4;
        n->draw_order = (uint8_t)(order < 8 ? order + 9 : order + 1);
        n->always_active = true;
        n->secondary = true;
        n->gfx_slot = s->gfx_slot;
        n->sprite_id = SSPRITE_CHOZO_STATUE_MOVEMENT;
        n->owner = s->secondary ? s->owner : s->sprite_id;
        n->y = s->y;
        n->x = s->x;
        n->pose = SPRITE_POSE_UNINITIALIZED;
        n->palette_row = s->palette_row;
        n->absolute_palette_row = s->absolute_palette_row;
        n->ignore_samus_timer = 1;
        n->ram_slot = (uint8_t)i;
        n->work0 = 32;              /* CONVERT_SECONDS(.5f) + 2 * DELTA_TIME */
        n->samus_collision = SSC_NONE;
        n->anim = s->anim;
        n->frame = s->frame;
        n->draw_top = s->draw_top;
        n->draw_bottom = s->draw_bottom;
        n->draw_horizontal = s->draw_horizontal;
        return;
    }
}

/* The palette flash of the hint and the refill: the next row of
 * sChozoStatueFlashingPaletteRows every `delay` frames. */
static void flash_palette(MzmSprite *s, uint8_t delay) {
    if (--s->work2 != 0) return;
    s->work2 = delay;
    s->palette_row = flashing_rows[s->work3];
    s->work3 = s->work3 >= 3 ? 0 : (uint8_t)(s->work3 + 1);
}

static MzmSprite *slot(const Ctx *c, uint8_t index) {
    return c->f->sprites && index < c->f->sprite_count ? &c->f->sprites[index] : NULL;
}

/* ChozoStatue */
static void statue(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    s->ignore_samus_timer = 1;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: statue_init(c); break;
    case STATUE_REGISTER_HINT: {
        /* ChozoStatueRegisterHint (MakeBackgroundFlash is not ported; the
         * Long Beam hint keeps its longer delay). */
        MzmSprite *eye = slot(c, s->work1);
        if (eye) eye->pose = PART_EYE_OPENING_INIT;
        s->pose = STATUE_HINT_FLASHING;
        s->work0 = DELAY_BEFORE_HINT;
        s->work2 = 12;              /* CONVERT_SECONDS(.2f) */
        s->work3 = 0;
        if (s->sprite_id == LONG_HINT) s->work0 += TWO_THIRD_SECOND;
        register_item(c, s->sprite_id);
        mzm_sfx_music_fade(60);     /* CONVERT_SECONDS(1.f) */
        break;
    }
    case STATUE_HINT_FLASHING: {
        /* ChozoStatueHintFlashing: the hint screen (gPauseScreenFlag
         * PAUSE_SCREEN_CHOZO_HINT) and its music start and the statue sits
         * down when the delay ends. */
        const MzmSprite *eye = slot(c, s->work1);
        if (eye && eye->pose != PART_DO_NOTHING) break;
        if (--s->work0 == 0) {
            s->pose = STATUE_SITTING_INIT;
            s->palette_row = 0;
            if (g) g->pause_screen_flag = 4;
            mzm_sfx_music_play(MZM_MUSIC_CHOZO_STATUE_HINT, 0);
            break;
        }
        if (s->work0 == DELAY_BEFORE_HINT - 1) mzm_sfx_play(MZM_SOUND_CHOZO_STATUE_HINT);
        else if (s->work0 >= DELAY_BEFORE_HINT) break;
        flash_palette(s, 12);
        break;
    }
    case STATUE_SITTING_INIT:
        /* ChozoStatueSittingInit (gSlowScrollingTimer is not ported) */
        s->pose = STATUE_SITTING;
        set_multi(g, (uint8_t)(multi_base(s) + MZM_CHOZO_MULTI_SITTING));
        standing_clipdata(c, MZM_CAA_REMOVE_SOLID, MZM_CAA_REMOVE_SOLID);
        mzm_sfx_play(MZM_SOUND_CHOZO_STATUE_SITTING_DOWN);
        break;
    case STATUE_SITTING:
        /* ChozoStatueSitting */
        update_sub_sprite_timer(c);
        if (g->sub1.work2) spawn_movement(c, g->sub1.work2);
        if (sub_sprite_ended(c)) {
            set_multi(g, (uint8_t)(multi_base(s) + MZM_CHOZO_MULTI_SEATED));
            s->pose = STATUE_DELAY_AFTER_SITTING;
            s->work0 = 30;          /* CONVERT_SECONDS(.5f) */
            seated_clipdata(c, MZM_CAA_MAKE_NON_POWER_GRIP);
        }
        break;
    case STATUE_DELAY_AFTER_SITTING:
        /* ChozoStatueDelayBeforeRefillAfterHint */
        if (--s->work0 == 0) s->pose = STATUE_IDLE;
        break;
    case STATUE_REFILL_INIT: {
        /* ChozoStatueRefillInit */
        s->pose = STATUE_REFILL;
        s->work2 = 4;               /* CONVERT_SECONDS(1.f / 15) */
        s->work3 = 0;
        const MzmSpriteSamusLink *link = c->f->samus_link;
        if (link)
            spawn_part(c, is_unknown(s->sprite_id) ? SSPRITE_UNKNOWN_ITEM_CHOZO_STATUE_REFILL
                                                   : SSPRITE_CHOZO_STATUE_REFILL, 0,
                       (uint16_t)(link->y - (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)), link->x, 0);
        break;
    }
    case STATUE_REFILL:
        /* ChozoStatueRefillSamus */
        flash_palette(s, 4);
        break;
    case STATUE_SLEEPING_INIT:
        /* ChozoStatueSleepingInit */
        s->pose = STATUE_SLEEPING;
        s->palette_row = 0;
        s->work0 = 70;              /* CONVERT_SECONDS(1.f + 1.f / 6) */
        break;
    case STATUE_SLEEPING:
        /* ChozoStatueSleeping: a hint statue replays the room music. */
        if (--s->work0 == 0) {
            MzmSprite *eye = slot(c, s->work1);
            if (eye) eye->pose = PART_EYE_CLOSING_INIT;
            s->pose = STATUE_DO_NOTHING;
            if (g->sub1.work3 && c->f->world && c->f->world->room_track)
                mzm_sfx_music_play(c->f->world->room_track, 0);
        }
        break;
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
    default: break;
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
        ai_set_anim(s, (uint8_t)(anim_base(s) + (behavior == BEHAVIOR_ITEM ? MZM_CHOZO_ARM_IDLE : MZM_CHOZO_ARM_GLOW)));
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
        ai_set_anim(s, (uint8_t)(anim_base(s) + (behavior == BEHAVIOR_HINT ? MZM_CHOZO_EYE_CLOSED : MZM_CHOZO_EYE_OPENED)));
        break;
    case PART_GLOW:
        s->draw_top = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_bottom = 1;
        s->draw_horizontal = THREE_QUARTER_BLOCK_SIZE / PIXEL_SIZE;
        ai_set_anim(s, (uint8_t)(anim_base(s) + MZM_CHOZO_REFILL_GLOW_IDLE));
        s->pose = PART_GLOW_IDLE;
        s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
        break;
    default:
        s->status = 0;
    }
}

/* The hand's x (CHOZO_STATUE_HAND_X_OFFSET from the arm). */
static uint16_t hand_x(const MzmSprite *s) {
    return (uint16_t)(s->status & MZM_SPRITE_STATUS_X_FLIP ? s->x + HAND_X_OFFSET
                                                           : s->x - HAND_X_OFFSET);
}

/* Samus resting in the hand: at its height, within a quarter block. */
static bool samus_in_hand(const MzmSprite *s, const MzmSpriteSamusLink *link) {
    uint16_t x = hand_x(s);
    return link->y == (uint16_t)(s->y - HAND_Y_OFFSET) && link->x > x - QUARTER_BLOCK_SIZE &&
           link->x < x + QUARTER_BLOCK_SIZE;
}

/* ChozoStatuePartSyncSamusPosition */
static void sync_samus(const MzmSprite *s, MzmSpriteSamusLink *link) {
    link->y = (uint16_t)(s->y - HAND_Y_OFFSET);
    link->x = hand_x(s);
    link->moved = true;
}

static void grab(MzmSpriteSamusLink *link) {
    link->grab = true;
    link->release = false;
    link->pose = MZM_LINK_POSE_GRABBED;
}

/* SpriteUtilRefillEnergy / Missiles / SuperMissiles / PowerBombs: a
 * fraction of what is missing (`divisor`), between 1 and `cap`. */
static bool refill(int *current, int max, int divisor, int cap) {
    if (*current >= max) return false;
    int step = (max - *current) / divisor;
    if (step == 0) step = 1;
    else if (step >= cap) step = cap;
    *current += step;
    if (*current > max) *current = max;
    return true;
}

/* ChozoStatuePartArmRefill */
static void arm_refill(Ctx *c, MzmSprite *s, MzmSprite *p) {
    MzmEquipment *e = c->f->equipment;
    uint8_t *anim = c->f->globals->refill_animation;
    if (!p || p->pose != STATUE_REFILL || !e) return;
    switch (s->work0) {
    case 30:
        if (!refill(&e->energy, e->max_energy, 32, 11)) {
            s->work0--;
            anim[0] = 13;
        }
        break;
    case 29:
        if (anim[0]) anim[0]--;
        else if (!refill(&e->missiles, e->max_missiles, 32, 6)) {
            s->work0--;
            anim[1] = 13;
        }
        break;
    case 28:
        if (anim[1]) anim[1]--;
        else if (!refill(&e->super_missiles, e->max_super_missiles, 16, 4)) {
            s->work0--;
            anim[2] = 13;
        }
        break;
    case 27:
        if (anim[2]) anim[2]--;
        else if (!refill(&e->power_bombs, e->max_power_bombs, 16, 4)) {
            s->work0--;
            anim[3] = 13;
        }
        break;
    default:
        if (anim[3]) {
            anim[3]--;
        } else if (s->work0 != 0) {
            s->work0--;
        } else {
            p->pose = STATUE_SLEEPING_INIT;
            s->pose = PART_ARM_SLEEPING_INIT;
            const MzmSpriteHooks *h = c->f->hooks;
            if (h && h->spawn_message)
                h->spawn_message(h->context,
                                 e->max_missiles == 0 && e->max_super_missiles == 0 &&
                                         e->max_power_bombs == 0
                                     ? MESSAGE_ENERGY_TANK_RECHARGE_COMPLETE
                                     : MESSAGE_WEAPONS_AND_ENERGY_RESTORED);
        }
        break;
    }
}

/* ChozoStatuePart */
static void part(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    MzmSprite *p = parent(c);
    MzmSpriteGlobals *g = c->f->globals;
    MzmSpriteSamusLink *link = c->f->samus_link;
    uint16_t prevent = c->f->prevent_movement ? *c->f->prevent_movement : 0;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: part_init(c); break;
    case PART_ARM_CHECK_GRAB_HINT:
        /* ChozoStatuePartArmCheckGrabSamusHint (gDisablePause is not
         * ported) */
        if (!p || p->pose != STATUE_IDLE || !link || !samus_in_hand(s, link)) break;
        if (link->pose == MZM_LINK_POSE_MORPH_BALL || link->pose == MZM_LINK_POSE_ROLLING) {
            grab(link);
            p->pose = STATUE_REGISTER_HINT;
            s->pose = PART_ARM_SITTING;
            ai_set_anim(s, (uint8_t)(anim_base(s) + MZM_CHOZO_ARM_SAMUS_GRABBED));
        }
        break;
    case PART_ARM_SITTING:
        /* ChozoStatuePartArmSitting */
        if (p && p->pose == STATUE_DELAY_AFTER_SITTING) s->pose = PART_ARM_SEATED;
        if (link) sync_samus(s, link);
        if (g->sub1.work2) spawn_movement(c, g->sub1.work2);
        break;
    case PART_ARM_SEATED:
        /* ChozoStatuePartArmSeated */
        if (link) sync_samus(s, link);
        if (p && p->pose == STATUE_IDLE) s->pose = PART_ARM_CHECK_GRAB_REFILL;
        break;
    case PART_ARM_CHECK_GRAB_REFILL: {
        /* ChozoStatuePartArmCheckGrabSamusRefill (without BUGFIX: the grab
         * ignores gPreventMovementTimer) */
        if (s->anim == anim_base(s) + MZM_CHOZO_ARM_IDLE && prevent == 0 && p &&
            get_behavior(c, p->sprite_id) != BEHAVIOR_ITEM)
            s->anim = (uint8_t)(anim_base(s) + MZM_CHOZO_ARM_GLOW);
        if (!p || p->pose != STATUE_IDLE || !link || !samus_in_hand(s, link)) break;
        bool grabbed = false;
        if (link->pose == MZM_LINK_POSE_MORPH_BALL) {
            grab(link);
            grabbed = true;
        } else if (link->pose == MZM_LINK_POSE_GRABBED) {
            grabbed = true;
        }
        if (grabbed) {
            p->pose = STATUE_REFILL_INIT;
            s->pose = PART_ARM_REFILL;
            sync_samus(s, link);
            ai_set_anim(s, (uint8_t)(anim_base(s) + MZM_CHOZO_ARM_SAMUS_GRABBED));
            s->work0 = 30;          /* CONVERT_SECONDS(.5f) */
        }
        break;
    }
    case PART_ARM_REFILL: arm_refill(c, s, p); break;
    case PART_ARM_SLEEPING_INIT:
        /* ChozoStatuePartSleepingInit */
        if (p && p->pose == STATUE_SLEEPING) s->pose = PART_ARM_SLEEPING;
        break;
    case PART_ARM_SLEEPING:
        /* ChozoStatuePartArmSleeping: Samus is let go once free to move. */
        if (prevent == 0 && s->anim == anim_base(s) + MZM_CHOZO_ARM_SAMUS_GRABBED) {
            ai_set_anim(s, (uint8_t)(anim_base(s) + MZM_CHOZO_ARM_IDLE));
            if (link) {
                link->release = true;
                link->grab = false;
                link->pose = MZM_LINK_POSE_MORPH_BALL;
            }
        }
        break;
    case PART_EYE_OPENING_INIT:
        s->pose = PART_EYE_OPENING;
        ai_set_anim(s, (uint8_t)(anim_base(s) + MZM_CHOZO_EYE_OPENING));
        break;
    case PART_EYE_OPENING:
        if (ai_animation_ending(c, 1)) {
            s->pose = PART_DO_NOTHING;
            ai_set_anim(s, (uint8_t)(anim_base(s) + MZM_CHOZO_EYE_OPENED));
        }
        break;
    case PART_EYE_CLOSING_INIT:
        s->pose = PART_EYE_CLOSING;
        ai_set_anim(s, (uint8_t)(anim_base(s) + MZM_CHOZO_EYE_CLOSING));
        break;
    case PART_EYE_CLOSING:
        if (ai_animation_ending(c, 1)) {
            s->pose = PART_DO_NOTHING;
            ai_set_anim(s, (uint8_t)(anim_base(s) + MZM_CHOZO_EYE_CLOSED));
        }
        break;
    case PART_LEG_IDLE:
        /* ChozoStatuePartLegIdle */
        if (p && p->pose == STATUE_SITTING) {
            if (g->sub1.work2) spawn_movement(c, g->sub1.work2);
        } else if (p && p->pose == STATUE_DELAY_AFTER_SITTING) {
            s->pose = PART_DO_NOTHING;
        }
        break;
    case PART_GLOW_IDLE: {
        /* ChozoStatuePartGlowIdle */
        const MzmSprite *arm = c->f->sprites && s->work1 < c->f->sprite_count
                                   ? &c->f->sprites[s->work1] : NULL;
        if (arm && arm->anim == anim_base(s) + MZM_CHOZO_ARM_GLOW) {
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
    default: break;
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
        ai_set_anim(s, (uint8_t)(anim_base(s) + MZM_CHOZO_BALL_NORMAL_CLOSED));
        break;
    case BALL_DO_NOTHING: break;
    case BALL_REVEALING:
        /* ChozoBallCheckRevealingAnimEnded */
        if (ai_animation_ending(c, 1)) {
            s->pose = BALL_IDLE;
            ai_set_anim(s, (uint8_t)(anim_base(s) + MZM_CHOZO_BALL_NORMAL_REVEALED));
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
        ai_set_anim(s, (uint8_t)(anim_base(s) + MZM_CHOZO_BALL_NORMAL_REVEALING));
        mzm_sfx_play(MZM_SOUND_CHOZO_BALL_REVEALING);
        break;
    }
}

/* ChozoStatueRefill: the refill's glow around Samus until the statue
 * sleeps. */
static void refill_glow(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSprite *p = parent(c);
    s->ignore_samus_timer = 1;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->always_active = true;
        s->samus_collision = SSC_NONE;
        s->draw_order = 1;
        s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = PIXEL_SIZE;
        s->pose = REFILL_IDLE;
        ai_set_anim(s, (uint8_t)(anim_base(s) + MZM_CHOZO_REFILL));
        mzm_sfx_play(MZM_SOUND_CHOZO_STATUE_REFILL);
    } else if (p && p->pose == STATUE_SLEEPING) {
        s->status = 0;
        mzm_sfx_fade(MZM_SOUND_CHOZO_STATUE_REFILL, 30);
    }
}

/* ChozoStatueMovement: flickers with its frame held, for 32 frames. */
static void movement(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    s->status ^= MZM_SPRITE_STATUS_NOT_DRAWN;
    s->adc--;
    if (--s->work0 == 0) s->status = 0;
}

void ai_chozo_statue(Ctx *c) {
    if (!c->f->globals) return;
    if (!c->s->secondary) statue(c);
    else if (c->s->sprite_id == SSPRITE_CHOZO_BALL) ball(c);
    else if (c->s->sprite_id == SSPRITE_CHOZO_STATUE_REFILL ||
             c->s->sprite_id == SSPRITE_UNKNOWN_ITEM_CHOZO_STATUE_REFILL)
        refill_glow(c);
    else if (c->s->sprite_id == SSPRITE_CHOZO_STATUE_MOVEMENT) movement(c);
    else part(c);
}
