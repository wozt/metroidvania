/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Ridley (src/sprites_ai/ridley.c): the body drives
 * gSubSpriteData1 (wings, head, claw and the tail's root), the tail tip
 * gSubSpriteData2 (the eight tail sprites); Ridley drops from the ceiling
 * when Samus faces left on the ground, spits fireballs, hovers and slides
 * after her, grabs her with its claw to lift her and spit at her, swipes
 * with its tail, and dies in explosions. Comments name the pinned
 * decompilation's routines. Not ported: the Ridley spawn cutscene effect
 * (StartEffectForCutscene) and the statue opening one of its death, the
 * minimap chunk update, and the wings' frozen palette offset read from
 * their OAM (RidleyPartWingSetPaletteOffset: offset 0). */
#include "mzm_screen_shake.h"
#include "mzm_sprite_ai.h"

enum {
    RIDLEY_GROUND = BLOCK_SIZE * 18 - ONE_SUB_PIXEL,
    POSE_CHECK_PLAY_CUTSCENE = 0x1,
    POSE_SPAWNING = 0x2,
    POSE_IDLE_INIT = 0x8,
    POSE_IDLE = 0x9,
    POSE_TURNING_AROUND_INIT = 0xA,
    POSE_TURNING_AROUND_FIRST_PART = 0xB,
    POSE_TURNING_AROUND_SECOND_PART = 0xC,
    POSE_SMALL_FIREBALLS_INIT = 0x22,
    POSE_SMALL_FIREBALLS = 0x23,
    POSE_HURT_BIG_FIREBALLS_INIT = 0x2A,
    POSE_HURT_BIG_FIREBALLS = 0x2B,
    POSE_BIG_FIREBALLS_INIT = 0x2C,
    POSE_BIG_FIREBALLS = 0x2D,
    POSE_TAIL_ATTACKS_INIT = 0x34,
    POSE_TAIL_ATTACKS = 0x35,
    POSE_DYING = 0x67,
    /* RidleySpawningAction / RidleySmallFireballsAttackAction */
    ACTION_GOING_DOWN = 0, ACTION_DELAY_BEFORE_OPENING_MOUTH, ACTION_OPENING_MOUTH,
    ACTION_SPITTING_FIREBALLS, ACTION_CLOSING_MOUTH, ACTION_TURNING_AROUND_INIT,
    ACTION_TURNING_AROUND_FIRST_PART, ACTION_TURNING_AROUND_SECOND_PART, ACTION_TAKING_OFF,
    ACTION_UNKNOWN,
    /* RidleySamusGrabbedAction */
    GRAB_NONE = 0, GRAB_CARRYING, GRAB_LIFTING, GRAB_LIFTED, GRAB_OPENING_MOUTH, GRAB_SPITTING,
    GRAB_RELEASING,
    /* RidleyPartId / RidleyTailPartId */
    PART_LEFT_WING = 0, PART_HEAD, PART_BODY, PART_CLAW, PART_TAIL, PART_RIGHT_WING,
    TAIL_BODY_ATTACHMENT = 0, TAIL_RIGHT_LEFT, TAIL_RIGHT_MIDDLE, TAIL_RIGHT_MOST, TAIL_LEFT_RIGHT,
    TAIL_LEFT_MIDDLE, TAIL_LEFT_MOST, TAIL_TIP,
    TAIL_POSE_IDLE = 0x9,
    TAIL_POSE_MOVE_TO_ATTACK = 0x43,
    TAIL_POSE_SETTING_UP_ATTACK = 0x45,
    TAIL_POSE_CHARGING_ATTACK = 0x47,
    TAIL_POSE_FIRST_VERTICAL_ATTACK = 0x49,
    TAIL_POSE_VERTICAL_ATTACK = 0x4B,
    TAIL_POSE_LAST_VERTICAL_ATTACK = 0x4D,
    TAIL_POSE_DIAGONAL_ATTACK = 0x4F,
    TAIL_POSE_BACK_TO_IDLE = 0x51,
    PART_POSE_IDLE = 0x9,
    PART_POSE_DYING = 0x62,
    /* RidleyFireballType */
    FIREBALL_MIDDLE_TOP = 1, FIREBALL_MIDDLE, FIREBALL_MIDDLE_BOTTOM, FIREBALL_BOTTOM,
    FIREBALL_FLOATING = 1 << 7,
    FIREBALL_SAMUS_GRABBED = FIREBALL_FLOATING | 8,
    FIREBALL_POSE_DESCENDING = 0x9,
    FIREBALL_POSE_ASCENDING = 0x23,
    FIREBALL_POSE_SLIDE_ON_WALL_INIT = 0x24,
    FIREBALL_POSE_SLIDE_ON_WALL = 0x25,
    FIREBALL_POSE_FLOATING = 0x27,
    FIREBALL_POSE_DIAGONAL = 0x29,
    FIREBALL_POSE_DESTROY = 0x42,
    FIREBALL_ASCENDING_Y_TARGET_OFFSET = THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE / 2,
    RIDLEY_LEFT_HITBOX = BLOCK_SIZE * 2,
    RIDLEY_RIGHT_HITBOX = BLOCK_SIZE + HALF_BLOCK_SIZE,
    CLAW_LEFT_HITBOX = BLOCK_SIZE * 3 + HALF_BLOCK_SIZE,
    CLAW_RIGHT_HITBOX = BLOCK_SIZE * 2 + HALF_BLOCK_SIZE,
};

static uint16_t clamp16(uint16_t v, uint16_t lo, uint16_t hi) { return v < lo ? lo : v > hi ? hi : v; }

static MzmSprite *slot(const Ctx *c, uint8_t index) {
    return c->f->sprites && index < c->f->sprite_count ? &c->f->sprites[index] : NULL;
}

static const MzmMultiSprite *table(const Ctx *c, uint8_t index) {
    return &c->tables->ridley_multi[index < MZM_RIDLEY_MULTI_COUNT ? index : 0];
}

static const int16_t *part_data(const Ctx *c, uint8_t multi, uint8_t frame, uint8_t part) {
    return table(c, multi)->parts[frame < MZM_MULTI_FRAMES ? frame : 0][part < MZM_MULTI_PARTS ? part : 0];
}

static void set_sub1(Ctx *c, uint8_t multi) {
    MzmSpriteGlobals *g = c->f->globals;
    g->sub1.multi = multi;
    g->sub1.adc = 0;
    g->sub1.frame = 0;
}

static void set_sub2(Ctx *c, uint8_t multi) {
    MzmSpriteGlobals *g = c->f->globals;
    g->sub2.multi = multi;
    g->sub2.adc = 0;
    g->sub2.frame = 0;
}

/* SpriteUtilUpdateSubSprite1Anim / SpriteUtilUpdateSubSprite2Anim */
static void update_anim(const MzmMultiSprite *m, uint8_t *frame, uint8_t *adc) {
    (*adc)++;
    if (m->timers[*frame] < *adc) {
        *adc = 1;
        (*frame)++;
        if (*frame >= MZM_MULTI_FRAMES || m->timers[*frame] == 0) *frame = 0;
    }
}

/* SpriteUtilHasSubSprite1AnimationEnded / ...Sprite2... */
static bool anim_ended(const MzmMultiSprite *m, uint8_t frame, uint8_t adc) {
    unsigned next = frame + 1u;
    return m->timers[frame] < (uint8_t)(adc + 1) && (next >= MZM_MULTI_FRAMES || m->timers[next] == 0);
}

static bool sub1_ended(const Ctx *c) {
    const MzmSpriteGlobals *g = c->f->globals;
    return anim_ended(table(c, g->sub1.multi), g->sub1.frame, g->sub1.adc);
}

static bool sub2_ended(const Ctx *c) {
    const MzmSpriteGlobals *g = c->f->globals;
    return anim_ended(table(c, g->sub2.multi), g->sub2.frame, g->sub2.adc);
}

/* The x of a part from its offset, mirrored by X_FLIP. */
static uint16_t part_x(const MzmSprite *s, uint16_t base, int16_t offset) {
    return (uint16_t)(s->status & MZM_SPRITE_STATUS_X_FLIP ? base - offset : base + offset);
}

/* RidleySyncSubSprites */
static void sync_sub_sprites(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    const int16_t *p = part_data(c, g->sub1.multi, g->sub1.frame, s->room_slot);
    if (p[0] >= 0 && p[0] < MZM_RIDLEY_ANIMS && s->anim != p[0]) ai_set_anim(s, (uint8_t)p[0]);
    s->y = (uint16_t)(g->sub1.y + p[1]);
    s->x = part_x(s, g->sub1.x, p[2]);
    if (s->room_slot == PART_TAIL) {
        g->sub2.y = s->y;
        g->sub2.x = s->x;
    }
}

/* SpriteUtilSyncCurrentSpritePositionWithSubSpriteData1PositionAndOam */
static void sync_position(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSpriteGlobals *g = c->f->globals;
    const int16_t *p = part_data(c, g->sub1.multi, g->sub1.frame, s->room_slot);
    s->y = (uint16_t)(g->sub1.y + p[1]);
    s->x = part_x(s, g->sub1.x, p[2]);
}

/* RidleyTailSyncSubSprites */
static void tail_sync(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSpriteGlobals *g = c->f->globals;
    const int16_t *p = part_data(c, g->sub2.multi, g->sub2.frame, s->room_slot);
    if (p[0] >= 0 && p[0] < MZM_RIDLEY_ANIMS && s->anim != p[0]) ai_set_anim(s, (uint8_t)p[0]);
    s->y = (uint16_t)(g->sub2.y + p[1]);
    s->x = part_x(s, g->sub2.x, p[2]);
}

/* The head (gSubSpriteData1.work4) and claw (work5) sprites' tables. */
static void head_anim(const Ctx *c, uint8_t anim) {
    MzmSprite *h = slot(c, c->f->globals->sub1.work4);
    if (h) ai_set_anim(h, anim);
}

static void claw_anim(const Ctx *c, uint8_t anim) {
    MzmSprite *h = slot(c, c->f->globals->sub1.work5);
    if (h) ai_set_anim(h, anim);
}

static void door_timer(const Ctx *c, int8_t timer) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->door_unlock_timer) h->door_unlock_timer(h->context, timer);
}

static void remove_solid(const Ctx *c, uint16_t y, uint16_t x) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->remove_solid) h->remove_solid(h->context, y, x);
}

/* The ground blocks Ridley's landing breaks (and a killed Ridley's room). */
static void remove_ground(const Ctx *c) {
    const MzmSpriteGlobals *g = c->f->globals;
    uint16_t y = (uint16_t)(g->boss_work1 + BLOCK_SIZE * 6 + HALF_BLOCK_SIZE), x = g->boss_work2;
    remove_solid(c, y, (uint16_t)(x - BLOCK_SIZE * 6));
    remove_solid(c, y, (uint16_t)(x - BLOCK_SIZE * 5));
    remove_solid(c, y, (uint16_t)(x + BLOCK_SIZE * 6));
    remove_solid(c, y, (uint16_t)(x + BLOCK_SIZE * 7));
}

/* Samus behind Ridley (it faces away from her). */
static bool samus_behind(const Ctx *c) {
    const MzmSpriteGlobals *g = c->f->globals;
    return c->s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? g->sub1.x > c->f->samus_x : g->sub1.x < c->f->samus_x;
}

static bool samus_in_front(const Ctx *c) {
    const MzmSpriteGlobals *g = c->f->globals;
    return c->s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? g->sub1.x < c->f->samus_x : g->sub1.x > c->f->samus_x;
}

/* RidleyIdleYFloatingMovement / RidleySpawnSpittingFireballsYFloatingMovement */
static void float_y(Ctx *c, const int16_t *offsets, unsigned length) {
    MzmSprite *s = c->s;
    uint8_t offset = s->work3;
    int16_t movement = offset < length ? offsets[offset] : SHORT_MAX;
    if (movement == SHORT_MAX) {
        movement = offsets[0];
        offset = 0;
    }
    s->work3 = (uint8_t)(offset + 1);
    c->f->globals->sub1.y = (uint16_t)(c->f->globals->sub1.y + movement);
}

/* RidleyUpdateHealth: its hurt sound, and the big fireballs once dead or
 * after 200 damage. */
static void update_health(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: case POSE_CHECK_PLAY_CUTSCENE: case POSE_SPAWNING:
    case POSE_HURT_BIG_FIREBALLS_INIT: case POSE_BIG_FIREBALLS: case POSE_BIG_FIREBALLS_INIT:
    case POSE_HURT_BIG_FIREBALLS: case POSE_TAIL_ATTACKS_INIT: case POSE_TAIL_ATTACKS:
        g->boss_work3 = 0;
        break;
    default:
        if ((s->isft & 0x7F) == 16) mzm_sfx_play_not_playing(MZM_SOUND_RIDLEY_DAMAGED);   /* .25f + 1.f / 60 */
        if (s->health == 0 && g->sub1.health != 0) {
            s->pose = POSE_HURT_BIG_FIREBALLS_INIT;
            g->boss_work3 = 0;
        } else {
            g->boss_work3 = (uint16_t)(g->boss_work3 + (g->sub1.health - s->health));
            if (g->boss_work3 > 200) {
                s->pose = POSE_HURT_BIG_FIREBALLS_INIT;
                g->boss_work3 = 0;
            }
        }
        g->sub1.health = s->health;
        break;
    }
}

/* RidleySpawnAscendingFireball */
static void spawn_ascending_fireball(Ctx *c, uint16_t timer) {
    MzmSprite *s = c->s;
    bool flip = s->status & MZM_SPRITE_STATUS_X_FLIP;
    uint16_t x = (uint16_t)(flip ? s->x + (BLOCK_SIZE * 2 - QUARTER_BLOCK_SIZE) : s->x - (BLOCK_SIZE * 2 - QUARTER_BLOCK_SIZE));
    ai_spawn_secondary(c, MZM_SSPRITE_RIDLEY_FIREBALL, (uint8_t)((timer / 16) & 15), s->gfx_slot,
                       (uint16_t)(s->y - (BLOCK_SIZE + HALF_BLOCK_SIZE)), x,
                       flip ? MZM_SPRITE_STATUS_FACING_RIGHT : 0);
}

/* RidleyInit */
static void ridley_init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    g->boss_work1 = s->y;
    g->boss_work2 = s->x;
    if (ai_check_event(c, MZM_EVENT_RIDLEY_KILLED)) {
        remove_ground(c);
        s->status = 0;
        return;
    }
    if (!ai_check_event(c, MZM_EVENT_GRAVITY_SUIT_OBTAINED)) {
        s->status = 0;
        return;
    }
    door_timer(c, 1);
    s->draw_order = 8;
    s->samus_collision = SSC_HURTS_SAMUS;
    s->draw_top = BLOCK_SIZE * 2 / PIXEL_SIZE;
    s->draw_bottom = BLOCK_SIZE * 4 / PIXEL_SIZE;
    s->draw_horizontal = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    s->hitbox_top = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    s->hitbox_bottom = BLOCK_SIZE * 2;
    s->y = (uint16_t)(s->y - BLOCK_SIZE * 8);
    s->x = (uint16_t)(s->x - BLOCK_SIZE);
    g->sub1.y = s->y;
    g->sub1.x = s->x;
    s->health = c->tables->health[s->sprite_id];
    g->sub1.health = s->health;
    g->boss_work3 = 0;
    set_sub1(c, MZM_RIDLEY_MULTI_IDLE);
    s->pose = POSE_CHECK_PLAY_CUTSCENE;
    s->status |= MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_FACING_RIGHT |
                 MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->room_slot = PART_BODY;
    static const struct { uint8_t id, part; } parts[] = {
        {MZM_SSPRITE_RIDLEY_PART, PART_LEFT_WING}, {MZM_SSPRITE_RIDLEY_PART, PART_HEAD},
        {MZM_SSPRITE_RIDLEY_PART, PART_TAIL}, {MZM_SSPRITE_RIDLEY_TAIL, TAIL_TIP},
        {MZM_SSPRITE_RIDLEY_TAIL, TAIL_LEFT_MOST}, {MZM_SSPRITE_RIDLEY_TAIL, TAIL_LEFT_MIDDLE},
        {MZM_SSPRITE_RIDLEY_TAIL, TAIL_LEFT_RIGHT}, {MZM_SSPRITE_RIDLEY_TAIL, TAIL_RIGHT_MOST},
        {MZM_SSPRITE_RIDLEY_TAIL, TAIL_RIGHT_MIDDLE}, {MZM_SSPRITE_RIDLEY_TAIL, TAIL_RIGHT_LEFT},
        {MZM_SSPRITE_RIDLEY_TAIL, TAIL_BODY_ATTACHMENT}, {MZM_SSPRITE_RIDLEY_PART, PART_CLAW},
        {MZM_SSPRITE_RIDLEY_PART, PART_RIGHT_WING},
    };
    for (unsigned i = 0; i < sizeof parts / sizeof parts[0]; ++i) {
        int n = ai_spawn_secondary(c, parts[i].id, parts[i].part, s->gfx_slot, g->sub1.y, g->sub1.x,
                                   MZM_SPRITE_STATUS_X_FLIP);
        uint8_t index = (uint8_t)(n < 0 ? 0xFF : n);
        if (parts[i].id == MZM_SSPRITE_RIDLEY_PART && parts[i].part == PART_HEAD) g->sub1.work4 = index;
        if (parts[i].id == MZM_SSPRITE_RIDLEY_PART && parts[i].part == PART_CLAW) g->sub1.work5 = index;
    }
}

/* RidleyCheckPlayCutscene: Samus on the ground facing left, near the
 * platform's left or the door. */
static void check_play_cutscene(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSpriteGlobals *g = c->f->globals;
    s->ignore_samus_timer = 1;
    /* gSamusData.yPosition (the runtime's feet are a subpixel lower). */
    if ((uint16_t)(c->f->samus_y - 1) != RIDLEY_GROUND) return;
    uint16_t sprite_x = (uint16_t)(g->boss_work2 + HALF_BLOCK_SIZE), samus_x = c->f->samus_x;
    bool in_range = (sprite_x + BLOCK_SIZE * 5 > samus_x && sprite_x + BLOCK_SIZE * 4 < samus_x) ||
                    sprite_x + BLOCK_SIZE * 7 < samus_x;
    const MzmSpriteSamusLink *link = c->f->samus_link;
    if (!in_range || !link || link->facing_right) return;
    s->status &= (uint16_t)~(MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
    s->pose = POSE_SPAWNING;
    set_sub1(c, MZM_RIDLEY_MULTI_IDLE);
    s->work1 = 0;
    s->work3 = 0;
    s->scaling = 300;               /* CONVERT_SECONDS(5.f), the timer */
}

/* The landing of RidleySpawning and of the small fireballs attack. */
static void going_down(Ctx *c, bool spawning) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    uint16_t bottom = (uint16_t)(g->sub1.y + BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    if (bottom < RIDLEY_GROUND) {
        uint16_t movement = (uint16_t)(RIDLEY_GROUND - bottom);
        if (movement > HALF_BLOCK_SIZE) movement = HALF_BLOCK_SIZE;
        g->sub1.y = (uint16_t)(g->sub1.y + movement);
        return;
    }
    if (spawning) {
        uint16_t y = (uint16_t)(g->boss_work1 + BLOCK_SIZE * 6 + HALF_BLOCK_SIZE), x = g->boss_work2;
        if (ai_collision_at(c, y, (uint16_t)(x - BLOCK_SIZE * 6)) != MZM_COLLISION_AIR) {
            remove_ground(c);
            ai_particle_set(c, (uint16_t)(y + HALF_BLOCK_SIZE), (uint16_t)(x - (BLOCK_SIZE * 5 + HALF_BLOCK_SIZE)),
                            MZM_PE_SPRITE_EXPLOSION_HUGE);
            ai_particle_set(c, (uint16_t)(y + HALF_BLOCK_SIZE), (uint16_t)(x + (BLOCK_SIZE * 6 + HALF_BLOCK_SIZE)),
                            MZM_PE_SPRITE_EXPLOSION_HUGE);
            mzm_sfx_play(MZM_SOUND_RIDLEY_EXPLODING_GROUND);
        }
    }
    g->sub1.y = RIDLEY_GROUND - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    mzm_screen_shake_start_vertical(30, 0x81);         /* CONVERT_SECONDS(.5f) */
    s->work1++;
    s->work0 = 30;
    s->work3 = 0;
    mzm_sfx_play(MZM_SOUND_RIDLEY_HITTING_GROUND);
}

/* The take off to eight blocks above the ground. */
static bool take_off(Ctx *c) {
    MzmSpriteGlobals *g = c->f->globals;
    uint16_t target = RIDLEY_GROUND - BLOCK_SIZE * 8;
    if (target < g->sub1.y) {
        uint16_t movement = clamp16((uint16_t)((g->sub1.y - target) / 4), PIXEL_SIZE, HALF_BLOCK_SIZE);
        g->sub1.y = (uint16_t)(g->sub1.y - movement);
        return false;
    }
    return true;
}

/* RidleySpawning */
static void spawning(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    switch (s->work1) {
    case ACTION_GOING_DOWN: going_down(c, true); break;
    case ACTION_DELAY_BEFORE_OPENING_MOUTH:
        if (--s->work0 == 0) {
            head_anim(c, MZM_RIDLEY_OPENING_MOUTH);
            mzm_sfx_play(MZM_SOUND_RIDLEY_OPENING_MOUTH);
            s->work0 = 5;           /* CONVERT_SECONDS(1.f / 12) */
            s->work1++;
        }
        break;
    case ACTION_OPENING_MOUTH:
        if (--s->work0 == 0) {
            s->status |= MZM_SPRITE_STATUS_MOSAIC;
            set_sub1(c, MZM_RIDLEY_MULTI_SPITTING_FIREBALLS);
            head_anim(c, MZM_RIDLEY_MOUTH_OPENED);
            claw_anim(c, MZM_RIDLEY_CLAW_SPITTING_FIREBALLS);
            s->work1++;
            mzm_sfx_play(MZM_SOUND_RIDLEY_SPITTING_FIREBALLS);
        }
        break;
    case ACTION_SPITTING_FIREBALLS:
        if (s->scaling % 16 == 0) spawn_ascending_fireball(c, s->scaling);  /* CONVERT_SECONDS(.25f + 1.f / 60) */
        if (s->scaling != 0) {
            s->scaling--;
            if (!samus_behind(c)) break;
        }
        head_anim(c, MZM_RIDLEY_OPENING_MOUTH);
        s->work0 = 5;
        s->work1++;
        mzm_sfx_fade(MZM_SOUND_RIDLEY_SPITTING_FIREBALLS, 30);
        break;
    case ACTION_CLOSING_MOUTH:
        if (--s->work0 == 0) {
            set_sub1(c, MZM_RIDLEY_MULTI_IDLE);
            head_anim(c, MZM_RIDLEY_HEAD_IDLE);
            claw_anim(c, MZM_RIDLEY_CLAW_IDLE);
            if (samus_behind(c)) {
                s->work0 = 10;      /* CONVERT_SECONDS(1.f / 6) */
                s->work1++;
            } else {
                s->work1 = ACTION_TAKING_OFF;
                s->work0 = 30;
            }
        }
        break;
    case ACTION_TURNING_AROUND_INIT:
        if (--s->work0 == 0) {
            set_sub1(c, MZM_RIDLEY_MULTI_TURNING_AROUND_FIRST_PART);
            head_anim(c, MZM_RIDLEY_HEAD_TURNING_AROUND);
            claw_anim(c, MZM_RIDLEY_CLAW_TURNING_AROUND_FIRST_PART);
            s->work1++;
        }
        break;
    case ACTION_TURNING_AROUND_FIRST_PART:
        if (sub1_ended(c)) {
            set_sub1(c, MZM_RIDLEY_MULTI_TURNING_AROUND_SECOND_PART);
            claw_anim(c, MZM_RIDLEY_CLAW_TURNING_AROUND_SECOND_PART);
            s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
            s->work1++;
            if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) s->status |= MZM_SPRITE_STATUS_X_FLIP;
            else s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        }
        break;
    case ACTION_TURNING_AROUND_SECOND_PART:
        if (sub1_ended(c)) {
            set_sub1(c, MZM_RIDLEY_MULTI_IDLE);
            head_anim(c, MZM_RIDLEY_HEAD_IDLE);
            claw_anim(c, MZM_RIDLEY_CLAW_IDLE);
            s->work0 = 10;
            s->work1 = ACTION_DELAY_BEFORE_OPENING_MOUTH;
        }
        break;
    case ACTION_TAKING_OFF:
        if (s->work0 == 0) {
            if (take_off(c)) {
                s->pose = POSE_IDLE_INIT;
                g->sub1.health = s->health;
            }
        } else {
            s->work0--;
        }
        return;
    case ACTION_UNKNOWN: {
        if (s->work0 != 0) {
            s->work0--;
            return;
        }
        uint16_t reach = BLOCK_SIZE * 5 + THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE * 3;
        if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
            if (ai_collision_at(c, g->sub1.y, (uint16_t)(g->sub1.x + reach)) != MZM_COLLISION_AIR)
                s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
            else
                g->sub1.x = (uint16_t)(g->sub1.x + QUARTER_BLOCK_SIZE);
        } else {
            if (ai_collision_at(c, g->sub1.y, (uint16_t)(g->sub1.x - reach)) != MZM_COLLISION_AIR)
                s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
            else
                g->sub1.x = (uint16_t)(g->sub1.x - QUARTER_BLOCK_SIZE);
        }
        if (--s->work2 == 0) {
            if (g->sub1.x < c->f->samus_x) s->status |= MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_FACING_RIGHT;
            else s->status &= (uint16_t)~(MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_FACING_RIGHT);
            s->work1 = ACTION_GOING_DOWN;
        }
        return;
    }
    default:
        return;
    }
    if (s->status & MZM_SPRITE_STATUS_MOSAIC) float_y(c, c->tables->ridley_spawning_y, 81);
}

/* RidleyIdleInit */
static void idle_init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    set_sub1(c, MZM_RIDLEY_MULTI_IDLE);
    head_anim(c, MZM_RIDLEY_HEAD_IDLE);
    MzmSprite *claw = slot(c, g->sub1.work5);
    if (claw) {
        ai_set_anim(claw, MZM_RIDLEY_CLAW_IDLE);
        claw->samus_collision = SSC_RIDLEY_CLAW;
    }
    s->work1 = 0;
    s->work3 = 0;
    s->pose = POSE_IDLE;
    if (BLOCK_SIZE * 10 - ONE_SUB_PIXEL < g->sub1.y) {
        s->work0 = 0;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_MOSAIC;
    } else {
        s->work0 = 60;
        s->status |= MZM_SPRITE_STATUS_MOSAIC;
    }
}

/* The claw's grab sequence of RidleyIdle; true while Samus is held. */
static bool grab_sequence(Ctx *c, MzmSprite *claw) {
    MzmSprite *s = c->s;
    uint8_t action = claw->work1;
    if (!(claw->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING)) {
        if (action == GRAB_RELEASING && --claw->work0 == 0) {
            s->pose = POSE_IDLE_INIT;
            claw->work1 = GRAB_NONE;
        }
        return false;
    }
    switch (action) {
    case GRAB_NONE:
        ai_set_anim(claw, MZM_RIDLEY_CLAW_CARRYING_SAMUS);
        claw->draw_order = 8;
        claw->work1 = GRAB_CARRYING;
        claw->work0 = 20;           /* ONE_THIRD_SECOND */
        break;
    case GRAB_CARRYING:
        if (--claw->work0 == 0) {
            ai_set_anim(claw, MZM_RIDLEY_CLAW_LIFTING_SAMUS);
            claw->work1 = GRAB_LIFTING;
            claw->work0 = 28;       /* CONVERT_SECONDS(.5f - 1.f / 30) */
            mzm_sfx_play(MZM_SOUND_RIDLEY_LIFTING_SAMUS);
        }
        break;
    case GRAB_LIFTING:
        if (--claw->work0 == 0) {
            ai_set_anim(claw, MZM_RIDLEY_CLAW_SAMUS_LIFTED);
            claw->work1 = GRAB_LIFTED;
            claw->work0 = 20;
        }
        break;
    case GRAB_LIFTED:
        if (--claw->work0 == 0) {
            head_anim(c, MZM_RIDLEY_OPENING_MOUTH);
            mzm_sfx_play(MZM_SOUND_RIDLEY_OPENING_MOUTH);
            claw->work1 = GRAB_OPENING_MOUTH;
            claw->work0 = 5;
        }
        break;
    case GRAB_OPENING_MOUTH:
        if (--claw->work0 == 0) {
            set_sub1(c, MZM_RIDLEY_MULTI_SPITTING_FIREBALLS);
            head_anim(c, MZM_RIDLEY_MOUTH_OPENED);
            claw->work1 = GRAB_SPITTING;
            claw->work0 = 0;
            claw->work2 = 0;
        }
        break;
    case GRAB_SPITTING: {
        if ((claw->work0++ & 15) == 0) {
            bool flip = s->status & MZM_SPRITE_STATUS_X_FLIP;
            ai_spawn_secondary(c, MZM_SSPRITE_RIDLEY_FIREBALL, FIREBALL_SAMUS_GRABBED, s->gfx_slot,
                               (uint16_t)(s->y - (BLOCK_SIZE + HALF_BLOCK_SIZE)),
                               (uint16_t)(flip ? s->x + (BLOCK_SIZE * 2 - QUARTER_BLOCK_SIZE)
                                               : s->x - (BLOCK_SIZE * 2 - QUARTER_BLOCK_SIZE)),
                               flip ? MZM_SPRITE_STATUS_FACING_RIGHT : 0);
            mzm_sfx_play(MZM_SOUND_RIDLEY_SPITTING_FIREBALLS_ON_SAMUS);
        }
        /* Mashing A, B or the pad frees Samus sooner. */
        if (c->f->changed_input & (MZM_KEY_A | MZM_KEY_B | MZM_KEY_RIGHT | MZM_KEY_LEFT | MZM_KEY_UP | MZM_KEY_DOWN))
            claw->work2 = (uint8_t)(claw->work2 + 4);
        else if (claw->work2 != 0)
            claw->work2--;
        if (claw->work0 == 0 || claw->work2 > 16) {
            ai_set_anim(claw, MZM_RIDLEY_CLAW_RELEASING_SAMUS);
            head_anim(c, MZM_RIDLEY_OPENING_MOUTH);
            claw->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            claw->work1 = GRAB_RELEASING;
            claw->work0 = 5;
            claw->draw_order = 9;
            claw->ignore_samus_timer = 31;  /* CONVERT_SECONDS(.5f + 1.f / 60) */
        }
        break;
    }
    default:
        break;
    }
    return true;
}

/* RidleyIdle: hovering and chasing Samus, the grab, and the next attack. */
static void idle(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    bool turning = false, start_slide = false;
    MzmSprite *claw = slot(c, g->sub1.work5);
    bool grabbed = claw && grab_sequence(c, claw);
    if (s->work0 != 0) {
        if (!grabbed) {
            s->work0--;
            if (c->f->samus_crouching_or_morphed && c->rng > 7 && s->work0 == 0 && samus_in_front(c)) {
                s->pose = s->status & MZM_SPRITE_STATUS_FACING_RIGHT
                              ? (c->rng > 13 ? POSE_TAIL_ATTACKS_INIT : POSE_SMALL_FIREBALLS_INIT)
                              : (c->rng >= 14 ? POSE_TAIL_ATTACKS_INIT : POSE_SMALL_FIREBALLS_INIT);
                return;
            }
            if ((c->frame8 & 15) == 0 && c->rng > 8 && s->work0 <= 9 && samus_in_front(c)) {
                s->pose = POSE_BIG_FIREBALLS_INIT;
                return;
            }
        }
        float_y(c, c->tables->ridley_idle_y, 65);
    } else if (s->status & MZM_SPRITE_STATUS_MOSAIC) {
        /* Going down toward the ground, then sliding at Samus. */
        uint16_t bottom = (uint16_t)(g->sub1.y + BLOCK_SIZE * 4 + PIXEL_SIZE);
        if (bottom < RIDLEY_GROUND) {
            uint16_t movement = (uint16_t)((RIDLEY_GROUND - bottom) / 4);
            if (movement > QUARTER_BLOCK_SIZE) movement = QUARTER_BLOCK_SIZE;
            g->sub1.y = (uint16_t)(g->sub1.y + movement);
            if (movement == 0) {
                if (s->work1 <= 16) s->work1++;
                if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
                    if ((uint16_t)(g->sub1.x + BLOCK_SIZE * 3) < c->f->samus_x) start_slide = true;
                } else if ((uint16_t)(g->sub1.x - BLOCK_SIZE * 3) > c->f->samus_x) {
                    start_slide = true;
                }
            }
        }
        if ((s->work1 > 16 && !start_slide) || grabbed) {
            s->work1 = 0;
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_MOSAIC;
        }
    } else {
        /* Going up (higher with Samus held). */
        uint16_t target = grabbed ? RIDLEY_GROUND - BLOCK_SIZE * 11 : RIDLEY_GROUND - BLOCK_SIZE * 8;
        if (target < g->sub1.y) {
            uint16_t movement = clamp16((uint16_t)((g->sub1.y - target) / 4), PIXEL_SIZE, QUARTER_BLOCK_SIZE);
            g->sub1.y = (uint16_t)(g->sub1.y - movement);
        } else {
            s->status |= MZM_SPRITE_STATUS_MOSAIC;
            s->work0 = 10;
        }
    }
    if (!grabbed) {
        uint16_t reach = BLOCK_SIZE * 4 + PIXEL_SIZE;
        uint16_t movement = s->status & MZM_SPRITE_STATUS_MOSAIC ? (start_slide ? EIGHTH_BLOCK_SIZE : PIXEL_SIZE / 2)
                                                                 : EIGHTH_BLOCK_SIZE;
        if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
            if (ai_collision_at(c, g->sub1.y, (uint16_t)(g->sub1.x + reach)) != MZM_COLLISION_AIR) turning = true;
            else g->sub1.x = (uint16_t)(g->sub1.x + movement);
        } else {
            if (ai_collision_at(c, g->sub1.y, (uint16_t)(g->sub1.x - reach)) != MZM_COLLISION_AIR) turning = true;
            else g->sub1.x = (uint16_t)(g->sub1.x - movement);
        }
        if (turning && (s->work0 != 0 || (s->work1 > 16 && start_slide))) s->pose = POSE_TURNING_AROUND_INIT;
    } else if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
        if (ai_collision_at(c, g->sub1.y, (uint16_t)(g->sub1.x + BLOCK_SIZE * 7)) != MZM_COLLISION_AIR)
            g->sub1.x = (uint16_t)(g->sub1.x - PIXEL_SIZE / 2);
    } else if (ai_collision_at(c, g->sub1.y, (uint16_t)(g->sub1.x - BLOCK_SIZE * 7)) != MZM_COLLISION_AIR) {
        g->sub1.x = (uint16_t)(g->sub1.x + PIXEL_SIZE / 2);
    }
}

/* RidleySmallFireballsAttackInit / RidleyBigFireballsAttackInit: the claw
 * lets go. */
static void claw_spitting(Ctx *c) {
    MzmSprite *claw = slot(c, c->f->globals->sub1.work5);
    if (!claw) return;
    ai_set_anim(claw, MZM_RIDLEY_CLAW_SPITTING_FIREBALLS);
    claw->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
    claw->samus_collision = SSC_NONE;
    claw->work1 = 0;
    claw->draw_order = 9;
}

/* RidleySmallFireballsAttack */
static void small_fireballs(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->work1) {
    case ACTION_GOING_DOWN: going_down(c, false); break;
    case ACTION_DELAY_BEFORE_OPENING_MOUTH:
        if (--s->work0 == 0) {
            head_anim(c, MZM_RIDLEY_OPENING_MOUTH);
            mzm_sfx_play(MZM_SOUND_RIDLEY_OPENING_MOUTH);
            s->work0 = 5;
            s->work1++;
        }
        break;
    case ACTION_OPENING_MOUTH:
        if (--s->work0 == 0) {
            set_sub1(c, MZM_RIDLEY_MULTI_SPITTING_FIREBALLS);
            head_anim(c, MZM_RIDLEY_MOUTH_OPENED);
            s->work1++;
            mzm_sfx_play(MZM_SOUND_RIDLEY_SPITTING_FIREBALLS);
        }
        break;
    case ACTION_SPITTING_FIREBALLS:
        if (s->scaling % 16 == 0) spawn_ascending_fireball(c, s->scaling);
        if (s->scaling != 0) {
            s->scaling--;
            if (!samus_behind(c)) break;
        }
        head_anim(c, MZM_RIDLEY_OPENING_MOUTH);
        s->work0 = 5;
        s->work1++;
        mzm_sfx_fade(MZM_SOUND_RIDLEY_SPITTING_FIREBALLS, 30);
        break;
    case ACTION_CLOSING_MOUTH:
        if (--s->work0 == 0) {
            set_sub1(c, MZM_RIDLEY_MULTI_IDLE);
            head_anim(c, MZM_RIDLEY_HEAD_IDLE);
            claw_anim(c, MZM_RIDLEY_CLAW_IDLE);
            s->work1 = ACTION_TAKING_OFF;
            s->work0 = 30;
        }
        break;
    case ACTION_TAKING_OFF:
        if (s->work0 != 0) {
            s->work0--;
            break;
        }
        if (take_off(c)) s->pose = POSE_IDLE_INIT;
        break;
    default:
        break;
    }
}

/* RidleyBigFireballsAttackInit */
static void big_fireballs_init(Ctx *c) {
    MzmSprite *s = c->s;
    set_sub1(c, MZM_RIDLEY_MULTI_SPITTING_FIREBALLS);
    uint8_t pose;
    if (s->pose == POSE_BIG_FIREBALLS_INIT) {
        head_anim(c, MZM_RIDLEY_MOUTH_OPENED);
        pose = POSE_BIG_FIREBALLS;
    } else {
        head_anim(c, MZM_RIDLEY_HEAD_DYING);
        pose = POSE_HURT_BIG_FIREBALLS;
    }
    claw_spitting(c);
    if (s->health == 0) {
        s->pose = POSE_DYING;
        s->work0 = 0;
        s->work1 = 150;             /* CONVERT_SECONDS(2.5f) */
        s->work2 = 180;
        s->work3 = 1;
        mzm_sfx_play(MZM_SOUND_RIDLEY_DYING_1);
    } else {
        s->pose = pose;
        s->work0 = 50;              /* CONVERT_SECONDS(5.f / 6) */
    }
}

/* RidleyBigFireballsAttack: five big fireballs in a fan. */
static void big_fireballs(Ctx *c) {
    MzmSprite *s = c->s;
    if (--s->work0 == 0) {
        s->pose = s->pose == POSE_BIG_FIREBALLS ? POSE_IDLE_INIT : POSE_TAIL_ATTACKS_INIT;
    } else if (s->work0 == 4) {
        head_anim(c, MZM_RIDLEY_OPENING_MOUTH);
    } else if (s->work0 == 20) {
        bool flip = s->status & MZM_SPRITE_STATUS_X_FLIP;
        uint16_t x = (uint16_t)(flip ? s->x + (BLOCK_SIZE * 2 - QUARTER_BLOCK_SIZE) : s->x - (BLOCK_SIZE * 2 - QUARTER_BLOCK_SIZE));
        uint16_t y = (uint16_t)(s->y - (BLOCK_SIZE + HALF_BLOCK_SIZE));
        for (uint8_t type = 0; type <= FIREBALL_BOTTOM; ++type)
            ai_spawn_secondary(c, MZM_SSPRITE_RIDLEY_BIG_FIREBALL, (uint8_t)(FIREBALL_FLOATING | type), s->gfx_slot,
                               y, x, flip ? MZM_SPRITE_STATUS_FACING_RIGHT : 0);
        mzm_sfx_play(MZM_SOUND_RIDLEY_SPITTING_BIG_FIREBALLS);
    }
}

/* RidleyDying */
static void dying(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    s->ignore_samus_timer = 1;
    if (s->work2 != 0) {
        if (s->work2 > 2) {
            s->work2--;
        } else {
            if (g->sub1.y > RIDLEY_GROUND - BLOCK_SIZE * 5) s->work2--;
            /* (the statue opening cutscene at 1 is not ported) */
            if (s->work2 == 0) mzm_sfx_music_fade(150);    /* CONVERT_SECONDS(2.5f) */
        }
    }
    if (g->sub1.y < RIDLEY_GROUND - BLOCK_SIZE * 3) g->sub1.y++;
    uint8_t rng1 = c->rng, rng2 = c->frame8;
    if ((s->work0 & 31) == 0) {     /* CONVERT_SECONDS(.5f + 1.f / 30) */
        if (s->work0 & 32)
            ai_particle_set(c, (uint16_t)(s->y - (BLOCK_SIZE * 2 + QUARTER_BLOCK_SIZE + PIXEL_SIZE + PIXEL_SIZE / 2) + rng1 * PIXEL_SIZE),
                            (uint16_t)(s->x + (BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE / 2) - rng1 * PIXEL_SIZE),
                            MZM_PE_SPRITE_EXPLOSION_HUGE);
        else
            ai_particle_set(c, (uint16_t)(s->y + (BLOCK_SIZE * 2 + QUARTER_BLOCK_SIZE + PIXEL_SIZE + PIXEL_SIZE / 2) - rng1 * PIXEL_SIZE),
                            (uint16_t)(s->x - (BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE / 2) + rng1 * PIXEL_SIZE),
                            MZM_PE_SPRITE_EXPLOSION_BIG);
    }
    if ((s->work3 & 31) == 0) {
        if (s->work3 & 32)
            ai_particle_set(c, (uint16_t)(s->y - (BLOCK_SIZE + QUARTER_BLOCK_SIZE) + rng2 / PIXEL_SIZE),
                            (uint16_t)(s->x + (BLOCK_SIZE - PIXEL_SIZE) - rng2 / PIXEL_SIZE), MZM_PE_MAIN_BOSS_DEATH);
        else
            ai_particle_set(c, (uint16_t)(s->y + (BLOCK_SIZE + QUARTER_BLOCK_SIZE) - rng2 / PIXEL_SIZE),
                            (uint16_t)(s->x - (BLOCK_SIZE - PIXEL_SIZE) + rng2 / PIXEL_SIZE), MZM_PE_MAIN_BOSS_DEATH);
    }
    s->work0++;
    s->work3++;
    if (s->work2 != 0) return;
    if (s->work0 % 16 == 0) {       /* CONVERT_SECONDS(.25f + 1.f / 60) */
        if ((s->work0 / 8) & 1)     /* MOD_BLOCK_AND(work0, CONVERT_SECONDS(2.f / 15)) */
            ai_particle_set(c, (uint16_t)(s->y - rng1 * EIGHTH_BLOCK_SIZE), (uint16_t)(s->x - rng1 * EIGHTH_BLOCK_SIZE),
                            MZM_PE_SPRITE_EXPLOSION_BIG);
        else
            ai_particle_set(c, (uint16_t)(s->y - rng1 * PIXEL_SIZE), (uint16_t)(s->x - rng1 * PIXEL_SIZE),
                            MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
    }
    if (s->work3 % 16 == 0) {
        if ((s->work3 / 8) & 1)
            ai_particle_set(c, (uint16_t)(s->y - BLOCK_SIZE - rng1 * EIGHTH_BLOCK_SIZE),
                            (uint16_t)(s->x + HALF_BLOCK_SIZE + rng1 * EIGHTH_BLOCK_SIZE), MZM_PE_SPRITE_EXPLOSION_BIG);
        else
            ai_particle_set(c, (uint16_t)(s->y + (BLOCK_SIZE + QUARTER_BLOCK_SIZE) + rng1 * EIGHTH_BLOCK_SIZE),
                            (uint16_t)(s->x + HALF_BLOCK_SIZE + rng1 * (PIXEL_SIZE / 2)), MZM_PE_SPRITE_EXPLOSION_BIG);
    }
    s->status ^= MZM_SPRITE_STATUS_NOT_DRAWN;
    if (--s->work1 == 0) {
        s->status = 0;
        door_timer(c, -60);         /* -CONVERT_SECONDS(1.f) */
        const MzmSpriteHooks *h = c->f->hooks;
        if (h && h->set_event) h->set_event(h->context, MZM_EVENT_RIDLEY_KILLED);
        mzm_sfx_music_play(MZM_MUSIC_BOSS_KILLED, 0);
    } else if (s->work1 == 149) {
        mzm_sfx_play(MZM_SOUND_RIDLEY_DYING_2);
    }
}

/* NBR_OF_PALETTE_ROWS - SPRITE_STUN_PALETTE_OFFSET (15, the stun row of
 * graphics slot 0) in the runtime's numbering. */
static uint8_t stun_row(const MzmSprite *s) {
    (void)s;
    return 14;
}

/* Ridley */
static void ridley(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    update_health(c);
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: ridley_init(c); break;
    case POSE_CHECK_PLAY_CUTSCENE: check_play_cutscene(c); break;
    case POSE_SPAWNING: spawning(c); break;
    case POSE_IDLE_INIT:
        idle_init(c);
        /* fallthrough */
    case POSE_IDLE: idle(c); break;
    case POSE_TURNING_AROUND_INIT:
        set_sub1(c, MZM_RIDLEY_MULTI_TURNING_AROUND_FIRST_PART);
        head_anim(c, MZM_RIDLEY_HEAD_TURNING_AROUND);
        claw_anim(c, MZM_RIDLEY_CLAW_TURNING_AROUND_FIRST_PART);
        s->pose = POSE_TURNING_AROUND_FIRST_PART;
        /* fallthrough */
    case POSE_TURNING_AROUND_FIRST_PART:
        float_y(c, c->tables->ridley_idle_y, 65);
        if (sub1_ended(c)) {
            s->pose = POSE_TURNING_AROUND_SECOND_PART;
            set_sub1(c, MZM_RIDLEY_MULTI_TURNING_AROUND_SECOND_PART);
            claw_anim(c, MZM_RIDLEY_CLAW_TURNING_AROUND_SECOND_PART);
            s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
            if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) s->status |= MZM_SPRITE_STATUS_X_FLIP;
            else s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        }
        break;
    case POSE_TURNING_AROUND_SECOND_PART:
        float_y(c, c->tables->ridley_idle_y, 65);
        if (sub1_ended(c)) {
            s->pose = POSE_IDLE;
            set_sub1(c, MZM_RIDLEY_MULTI_IDLE);
            head_anim(c, MZM_RIDLEY_HEAD_IDLE);
            claw_anim(c, MZM_RIDLEY_CLAW_IDLE);
        }
        break;
    case POSE_SMALL_FIREBALLS_INIT:
        claw_spitting(c);
        s->pose = POSE_SMALL_FIREBALLS;
        s->work1 = ACTION_GOING_DOWN;
        s->work3 = 0;
        s->scaling = 180;           /* CONVERT_SECONDS(3.f) */
        /* fallthrough */
    case POSE_SMALL_FIREBALLS: small_fireballs(c); break;
    case POSE_TAIL_ATTACKS_INIT:
        /* RidleyTailAttacksInit: the tail tip takes over. */
        set_sub1(c, MZM_RIDLEY_MULTI_IDLE);
        head_anim(c, MZM_RIDLEY_HEAD_IDLE);
        claw_anim(c, MZM_RIDLEY_CLAW_IDLE);
        break;
    case POSE_TAIL_ATTACKS: break;
    case POSE_HURT_BIG_FIREBALLS_INIT:
    case POSE_BIG_FIREBALLS_INIT: big_fireballs_init(c); break;
    case POSE_BIG_FIREBALLS:
    case POSE_HURT_BIG_FIREBALLS: big_fireballs(c); break;
    case POSE_DYING: dying(c); break;
    default: break;
    }
    if (s->status & MZM_SPRITE_STATUS_EXISTS) {
        /* Half and a quarter of its health: palette rows 3 and 5. */
        if (s->palette_row != stun_row(s)) {
            uint16_t max = c->tables->health[s->sprite_id];
            if (s->health < max / 4) s->palette_row = s->absolute_palette_row = 5;
            else if (s->health < max / 2) s->palette_row = s->absolute_palette_row = 3;
        }
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
            s->hitbox_left = -RIDLEY_RIGHT_HITBOX;
            s->hitbox_right = RIDLEY_LEFT_HITBOX;
        } else {
            s->hitbox_left = -RIDLEY_LEFT_HITBOX;
            s->hitbox_right = RIDLEY_RIGHT_HITBOX;
        }
        const MzmSprite *claw = slot(c, g->sub1.work5);
        bool holding = claw && claw->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        if ((uint16_t)(c->f->samus_y - 1) < BLOCK_SIZE * 13 - ONE_SUB_PIXEL && !holding) {
            g->lock_screen = false;
        } else {
            g->lock_screen = true;
            g->lock_y = (uint16_t)(c->f->samus_y - BLOCK_SIZE * 2);
            g->lock_x = c->f->samus_x;
        }
    } else {
        g->lock_screen = false;
    }
    /* (without BUGFIX, also once Ridley is gone) */
    update_anim(table(c, g->sub1.multi), &g->sub1.frame, &g->sub1.adc);
    sync_sub_sprites(c);
}

/* RidleyPartClawIdle: Samus held loses energy, stays in mid-air and goes
 * where the claw carries her; a wall there drops her. */
static void claw_hold(Ctx *c, MzmSprite *main) {
    MzmSprite *s = c->s;
    if (!(s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING)) return;
    MzmSpriteSamusLink *link = c->f->samus_link;
    MzmEquipment *eq = c->f->equipment;
    bool varia = eq && eq->items & MZM_ITEM_VARIA_SUIT;
    uint8_t interval;
    if (c->f->difficulty == 0) interval = varia ? 7 : 3;
    else if (c->f->difficulty == 2) interval = varia ? 1 : 0;
    else interval = varia ? 3 : 1;
    bool killed = false;
    if (!(c->frame8 & interval) && eq) {
        if (eq->energy > 1) {
            eq->energy--;
            if (link) link->set_invincibility = 8;  /* CONVERT_SECONDS(.15f - 1.f / 60) */
        } else {
            eq->energy = 0;
            killed = true;
            if (link) link->set_pose = MZM_POSE_HURT_REQUEST;
        }
    }
    /* (SamusSetPose(SPOSE_MIDAIR) follows the kill natively; the runtime
     * keeps the hurt request so that Samus dies) */
    if (link && !killed && link->samus_pose != MZM_POSE_MIDAIR) link->set_pose = MZM_POSE_MIDAIR;
    bool flip = s->status & MZM_SPRITE_STATUS_X_FLIP;
    uint16_t y = 0, x = 0;
    int16_t dx = 0;
    bool update = true;
    switch (s->work1) {
    case GRAB_NONE:
    case GRAB_CARRYING:
        y = (uint16_t)(s->y + BLOCK_SIZE * 4);
        dx = BLOCK_SIZE * 3 - QUARTER_BLOCK_SIZE;
        break;
    case GRAB_LIFTING: {
        static const int16_t lifting[5][2] = {
            {BLOCK_SIZE * 3 + THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE, BLOCK_SIZE * 3 - EIGHTH_BLOCK_SIZE},
            {BLOCK_SIZE * 3 + HALF_BLOCK_SIZE - PIXEL_SIZE, BLOCK_SIZE * 3 + QUARTER_BLOCK_SIZE},
            {BLOCK_SIZE * 3, BLOCK_SIZE * 3 + THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE},
            {BLOCK_SIZE * 2 + HALF_BLOCK_SIZE - PIXEL_SIZE, BLOCK_SIZE * 4 - QUARTER_BLOCK_SIZE},
            {BLOCK_SIZE * 2 - PIXEL_SIZE, BLOCK_SIZE * 4 - QUARTER_BLOCK_SIZE},
        };
        if (s->frame >= 5) {
            /* (the native leaves y and x unset past frame 4) */
            update = false;
            break;
        }
        y = (uint16_t)(s->y + lifting[s->frame][0]);
        dx = lifting[s->frame][1];
        break;
    }
    case GRAB_LIFTED:
    case GRAB_OPENING_MOUTH:
    case GRAB_SPITTING:
        y = (uint16_t)(s->y + BLOCK_SIZE + HALF_BLOCK_SIZE);
        dx = BLOCK_SIZE * 4;
        break;
    default:
        update = false;
        break;
    }
    if (!update) return;
    x = (uint16_t)(flip ? s->x + dx : s->x - dx);
    if (ai_collision_at(c, y, (uint16_t)(x + c->f->samus_hitbox_left)) == MZM_COLLISION_AIR &&
        ai_collision_at(c, y, (uint16_t)(x + c->f->samus_hitbox_right)) == MZM_COLLISION_AIR) {
        if (link) {
            link->y = y;
            link->x = x;
            link->moved = true;
        }
    } else {
        main->pose = POSE_BIG_FIREBALLS_INIT;
        c->f->globals->boss_work3 = 0;
    }
}

/* RidleyPartWingPlaySound */
static void wing_sound(Ctx *c, const MzmSprite *main) {
    const MzmSprite *s = c->s;
    if (main->pose == SPRITE_POSE_UNINITIALIZED || main->pose == POSE_CHECK_PLAY_CUTSCENE) return;
    if (main->pose == POSE_DYING && main->work2 == 0) return;
    if (s->frame != 0 || s->adc != 1) return;
    if (s->anim == MZM_RIDLEY_LEFT_WING_IDLE) mzm_sfx_play(MZM_SOUND_RIDLEY_WINGS_FLAPPING);
    else if (s->anim == MZM_RIDLEY_LEFT_WING_SPITTING_FIREBALLS) mzm_sfx_play(MZM_SOUND_RIDLEY_WINGS_FLAPPING_FAST);
}

/* RidleyPartSyncPalette */
static void sync_palette(MzmSprite *s, const MzmSprite *main) {
    if (main->palette_row == stun_row(main)) s->palette_row = (uint8_t)(14 - s->frozen_row_offset);
    else s->palette_row = main->absolute_palette_row;
}

static void set_box(MzmSprite *s, int16_t top, int16_t bottom, int16_t left, int16_t right) {
    s->hitbox_top = top;
    s->hitbox_bottom = bottom;
    s->hitbox_left = left;
    s->hitbox_right = right;
}

/* RidleyPartInit */
static void part_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->pose = PART_POSE_IDLE;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    switch (s->room_slot) {
    case PART_LEFT_WING:
    case PART_RIGHT_WING:
        s->draw_order = s->room_slot == PART_LEFT_WING ? 6 : 10;
        s->draw_top = (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_bottom = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_horizontal = (s->room_slot == PART_LEFT_WING ? BLOCK_SIZE * 3 : BLOCK_SIZE * 2) / PIXEL_SIZE;
        set_box(s, 0, 0, 0, 0);
        s->samus_collision = SSC_NONE;
        break;
    case PART_HEAD:
        s->draw_order = 7;
        s->draw_top = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->draw_bottom = s->draw_horizontal = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        set_box(s, -BLOCK_SIZE, BLOCK_SIZE, -HALF_BLOCK_SIZE, HALF_BLOCK_SIZE);
        s->samus_collision = SSC_HURTS_SAMUS;
        break;
    case PART_CLAW:
        s->draw_order = 9;
        s->draw_top = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_bottom = BLOCK_SIZE * 3 / PIXEL_SIZE;
        s->draw_horizontal = (BLOCK_SIZE * 5 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        set_box(s, BLOCK_SIZE + HALF_BLOCK_SIZE, BLOCK_SIZE * 2 + HALF_BLOCK_SIZE, -CLAW_LEFT_HITBOX, -CLAW_RIGHT_HITBOX);
        s->samus_collision = SSC_HURTS_SAMUS;
        s->work1 = 0;
        break;
    case PART_TAIL:
        s->draw_order = 8;
        s->draw_top = s->draw_bottom = s->draw_horizontal = 1;
        set_box(s, 0, 0, 0, 0);
        s->samus_collision = SSC_NONE;
        break;
    default:
        s->status = 0;
        return;
    }
    sync_sub_sprites(c);
}

/* RidleyPart */
static void part(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSprite *main = slot(c, s->ram_slot);
    if (!main) return;
    uint8_t which = s->room_slot;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        part_init(c);
        return;
    }
    if (s->pose == PART_POSE_DYING) {
        if (which != PART_TAIL) {
            if (which == PART_LEFT_WING) wing_sound(c, main);
            if (main->status & MZM_SPRITE_STATUS_NOT_DRAWN) s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
            else s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        }
        if (main->status == 0) {
            s->status = 0;
            return;
        }
    } else if (main->health == 0) {
        s->pose = PART_POSE_DYING;
        s->samus_collision = SSC_NONE;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    }
    if (main->status & MZM_SPRITE_STATUS_X_FLIP) s->status |= MZM_SPRITE_STATUS_X_FLIP;
    else s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
    if (which != PART_TAIL && s->status & MZM_SPRITE_STATUS_NOT_DRAWN) {
        s->ignore_samus_timer = 1;
        if (!(main->status & MZM_SPRITE_STATUS_NOT_DRAWN))
            s->status &= (uint16_t)~(MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
    }
    switch (which) {
    case PART_LEFT_WING:
        wing_sound(c, main);
        /* fallthrough */
    case PART_RIGHT_WING:
        sync_palette(s, main);
        sync_sub_sprites(c);
        break;
    case PART_CLAW:
        claw_hold(c, main);
        /* RidleyPartClawUpdateSidesHitbox */
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
            s->hitbox_left = CLAW_RIGHT_HITBOX;
            s->hitbox_right = CLAW_LEFT_HITBOX;
        } else {
            s->hitbox_left = -CLAW_LEFT_HITBOX;
            s->hitbox_right = -CLAW_RIGHT_HITBOX;
        }
        /* fallthrough */
    case PART_HEAD:
        sync_palette(s, main);
        sync_position(c);
        break;
    default:
        sync_palette(s, main);
        sync_sub_sprites(c);
        break;
    }
}

/* RidleyTailAttacksXMove: a wall turns Ridley around. */
static bool tail_x_move(Ctx *c, MzmSprite *main, uint16_t movement) {
    MzmSpriteGlobals *g = c->f->globals;
    bool right = main->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    uint16_t y = (uint16_t)(g->sub1.y + BLOCK_SIZE);
    uint16_t x = (uint16_t)(right ? g->sub1.x + (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) : g->sub1.x - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE));
    if (ai_collision_at(c, y, x) != MZM_COLLISION_AIR) {
        main->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
        return true;
    }
    g->sub1.x = (uint16_t)(right ? g->sub1.x + movement : g->sub1.x - movement);
    return false;
}

/* RidleyTailAttacksYMove */
static void tail_y_move(Ctx *c, uint16_t movement) {
    MzmSpriteGlobals *g = c->f->globals;
    if ((uint16_t)(c->f->samus_y - 1) >= g->sub1.y && g->sub1.y < BLOCK_SIZE * 11 + HALF_BLOCK_SIZE &&
        c->s->y + QUARTER_BLOCK_SIZE + PIXEL_SIZE < RIDLEY_GROUND)
        g->sub1.y = (uint16_t)(g->sub1.y + movement);
}

/* RidleyTailCheckStartScreenShakeVerticalTailAttack */
static void tail_shake(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->y_spawn + QUARTER_BLOCK_SIZE + PIXEL_SIZE < RIDLEY_GROUND &&
        s->y + QUARTER_BLOCK_SIZE + PIXEL_SIZE > RIDLEY_GROUND - ONE_SUB_PIXEL)
        mzm_screen_shake_start_vertical(20, 0x81);
    s->y_spawn = s->y;
}

static void rise(Ctx *c, uint16_t movement) {
    MzmSpriteGlobals *g = c->f->globals;
    if (g->sub1.y > RIDLEY_GROUND - BLOCK_SIZE * 8) g->sub1.y = (uint16_t)(g->sub1.y - movement);
}

/* The tail tip's attack poses. */
static void tail_tip(Ctx *c, MzmSprite *main) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    uint8_t frame = g->sub2.frame, adc = g->sub2.adc;
    switch (s->pose) {
    case TAIL_POSE_IDLE:
        if (g->sub1.multi != MZM_RIDLEY_MULTI_SPITTING_FIREBALLS && g->sub2.multi == MZM_RIDLEY_MULTI_TAIL_DYING)
            set_sub2(c, MZM_RIDLEY_MULTI_TAIL_IDLE);
        if (main->pose == POSE_TAIL_ATTACKS_INIT) {
            s->pose = TAIL_POSE_MOVE_TO_ATTACK;
            set_sub2(c, MZM_RIDLEY_MULTI_TAIL_MOVE_TO_ATTACK);
        }
        break;
    case TAIL_POSE_MOVE_TO_ATTACK:
        if (sub2_ended(c)) {
            set_sub2(c, MZM_RIDLEY_MULTI_TAIL_SETTING_UP_DIAGONAL);
            s->pose = TAIL_POSE_SETTING_UP_ATTACK;
        }
        break;
    case TAIL_POSE_SETTING_UP_ATTACK:
        if (frame == 3 && adc == 1) mzm_sfx_play(MZM_SOUND_RIDLEY_TAIL_SPINNING);
        if (sub2_ended(c)) {
            set_sub2(c, MZM_RIDLEY_MULTI_TAIL_CHARGING_DIAGONAL);
            s->pose = TAIL_POSE_CHARGING_ATTACK;
            s->work0 = c->f->difficulty == 0 ? 2 : (uint8_t)(c->rng / 2 + 2);  /* swipes */
        }
        rise(c, PIXEL_SIZE / 2);
        break;
    case TAIL_POSE_CHARGING_ATTACK: {
        if (frame == 3 && adc == 1) mzm_sfx_play(MZM_SOUND_RIDLEY_TAIL_SPINNING);
        if (!sub2_ended(c)) break;
        main->pose = POSE_TAIL_ATTACKS;
        bool diagonal;
        if (main->status & MZM_SPRITE_STATUS_X_FLIP) {
            diagonal = !((uint16_t)(g->sub1.x - BLOCK_SIZE * 2) > c->f->samus_x);
            if (diagonal) main->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
            else main->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
        } else {
            diagonal = (uint16_t)(g->sub1.x + BLOCK_SIZE * 2) > c->f->samus_x;
            if (diagonal) main->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
            else main->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
        }
        if (diagonal) {
            set_sub2(c, MZM_RIDLEY_MULTI_TAIL_DIAGONAL);
            s->pose = TAIL_POSE_DIAGONAL_ATTACK;
        } else if (c->f->difficulty == 0) {
            set_sub2(c, MZM_RIDLEY_MULTI_TAIL_LAST_VERTICAL);
            s->pose = TAIL_POSE_LAST_VERTICAL_ATTACK;
            s->work0 = 0;
        } else {
            set_sub2(c, MZM_RIDLEY_MULTI_TAIL_CHARGING_VERTICAL);
            s->pose = TAIL_POSE_FIRST_VERTICAL_ATTACK;
        }
        break;
    }
    case TAIL_POSE_FIRST_VERTICAL_ATTACK:
        if (frame == 3 && adc == 1) mzm_sfx_play(MZM_SOUND_RIDLEY_TAIL_VERTICAL_SWIPE);
        if (sub2_ended(c)) {
            set_sub2(c, MZM_RIDLEY_MULTI_TAIL_VERTICAL);
            s->pose = TAIL_POSE_VERTICAL_ATTACK;
        } else {
            if (frame < 8) tail_y_move(c, QUARTER_BLOCK_SIZE - PIXEL_SIZE);
            else rise(c, EIGHTH_BLOCK_SIZE);
            tail_x_move(c, main, PIXEL_SIZE / 2);
            tail_shake(c);
        }
        break;
    case TAIL_POSE_VERTICAL_ATTACK:
        if (frame == 0 && adc == 1) mzm_sfx_play(MZM_SOUND_RIDLEY_TAIL_VERTICAL_SWIPE);
        if (sub2_ended(c)) {
            bool stop = --s->work0 == 0;
            if (!stop)
                stop = main->status & MZM_SPRITE_STATUS_FACING_RIGHT ? g->sub1.x > c->f->samus_x
                                                                      : g->sub1.x < c->f->samus_x;
            if (stop) {
                set_sub2(c, MZM_RIDLEY_MULTI_TAIL_LAST_VERTICAL);
                s->pose = TAIL_POSE_LAST_VERTICAL_ATTACK;
                break;
            }
        }
        if (g->sub2.frame < 5) tail_y_move(c, QUARTER_BLOCK_SIZE - PIXEL_SIZE);
        else rise(c, EIGHTH_BLOCK_SIZE);
        tail_x_move(c, main, PIXEL_SIZE);
        tail_shake(c);
        break;
    case TAIL_POSE_LAST_VERTICAL_ATTACK:
        if (frame == 0 && adc == 1) mzm_sfx_play(MZM_SOUND_RIDLEY_TAIL_VERTICAL_SWIPE);
        if (sub2_ended(c)) {
            if (s->work0 != 0) {
                set_sub2(c, MZM_RIDLEY_MULTI_TAIL_CHARGING_DIAGONAL);
                s->pose = TAIL_POSE_CHARGING_ATTACK;
            } else {
                set_sub2(c, MZM_RIDLEY_MULTI_TAIL_BACK_TO_IDLE);
                s->pose = TAIL_POSE_BACK_TO_IDLE;
            }
        } else {
            if (frame < 5) tail_y_move(c, QUARTER_BLOCK_SIZE - PIXEL_SIZE);
            else rise(c, EIGHTH_BLOCK_SIZE);
            tail_x_move(c, main, PIXEL_SIZE / 2);
            tail_shake(c);
        }
        break;
    case TAIL_POSE_DIAGONAL_ATTACK:
        if (frame == 3 && adc == 1) mzm_sfx_play(MZM_SOUND_RIDLEY_TAIL_DIAGONAL_SWIPE);
        if (sub2_ended(c)) {
            if (--s->work0 == 0) {
                set_sub2(c, MZM_RIDLEY_MULTI_TAIL_BACK_TO_IDLE);
                s->pose = TAIL_POSE_BACK_TO_IDLE;
            } else {
                set_sub2(c, MZM_RIDLEY_MULTI_TAIL_CHARGING_DIAGONAL);
                s->pose = TAIL_POSE_CHARGING_ATTACK;
            }
        } else {
            tail_y_move(c, PIXEL_SIZE);
            tail_x_move(c, main, PIXEL_SIZE);
        }
        break;
    case TAIL_POSE_BACK_TO_IDLE:
        rise(c, EIGHTH_BLOCK_SIZE);
        if (sub2_ended(c)) {
            set_sub2(c, MZM_RIDLEY_MULTI_TAIL_IDLE);
            s->pose = TAIL_POSE_IDLE;
            main->pose = POSE_IDLE_INIT;
            if (main->status & MZM_SPRITE_STATUS_X_FLIP) main->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
            else main->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
        }
        break;
    default:
        break;
    }
}

/* RidleyTailDead: each tail sprite explodes at its own place. */
static void tail_dead(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSpriteGlobals *g = c->f->globals;
    int rng = c->rng;
    static const struct { int16_t y, ry, x, rx; uint8_t effect; } deaths[8] = {
        [TAIL_BODY_ATTACHMENT] = {-(BLOCK_SIZE + PIXEL_SIZE), 1, BLOCK_SIZE + HALF_BLOCK_SIZE + PIXEL_SIZE, -2,
                                  MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG},
        [TAIL_RIGHT_LEFT] = {-(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE), -1, 0, -2, MZM_PE_SPRITE_EXPLOSION_MEDIUM},
        [TAIL_RIGHT_MIDDLE] = {THREE_QUARTER_BLOCK_SIZE, 1, -(HALF_BLOCK_SIZE - PIXEL_SIZE), -2,
                               MZM_PE_SPRITE_EXPLOSION_BIG},
        [TAIL_RIGHT_MOST] = {-THREE_QUARTER_BLOCK_SIZE, 1, -(BLOCK_SIZE + QUARTER_BLOCK_SIZE), 2,
                             MZM_PE_SPRITE_EXPLOSION_MEDIUM},
        [TAIL_LEFT_RIGHT] = {HALF_BLOCK_SIZE, -1, BLOCK_SIZE + QUARTER_BLOCK_SIZE, 2,
                             MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG},
        [TAIL_LEFT_MIDDLE] = {-(BLOCK_SIZE + HALF_BLOCK_SIZE + PIXEL_SIZE), -1,
                              -(BLOCK_SIZE * 2 + QUARTER_BLOCK_SIZE - PIXEL_SIZE), -2, MZM_PE_SPRITE_EXPLOSION_MEDIUM},
        [TAIL_LEFT_MOST] = {BLOCK_SIZE + QUARTER_BLOCK_SIZE, -1, HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE, 2,
                            MZM_PE_SPRITE_EXPLOSION_BIG},
        [TAIL_TIP] = {BLOCK_SIZE * 2 - EIGHTH_BLOCK_SIZE, 1, -(BLOCK_SIZE * 2 - EIGHTH_BLOCK_SIZE), 2,
                      MZM_PE_SPRITE_EXPLOSION_MEDIUM},
    };
    if (s->room_slot >= 8) {
        s->status = 0;
        return;
    }
    /* y + ry * rng (ONE_SUB_PIXEL), x + rx * rng * PIXEL_SIZE / 2 / 2 */
    ai_sprite_death_full(c, DEATH_NORMAL, (uint16_t)(g->sub1.y + deaths[s->room_slot].y + deaths[s->room_slot].ry * rng),
                         (uint16_t)(g->sub1.x + deaths[s->room_slot].x + deaths[s->room_slot].rx / 2 * rng * PIXEL_SIZE / 2),
                         false, deaths[s->room_slot].effect);
    if (s->room_slot == TAIL_TIP) mzm_sfx_play(MZM_SOUND_RIDLEY_TAIL_DYING);
}

/* RidleyTailInit */
static void tail_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->pose = TAIL_POSE_IDLE;
    s->immune_to_projectiles = true;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->health = 1;
    s->draw_order = 11;
    s->samus_collision = SSC_HURTS_SAMUS;
    if (s->room_slot == TAIL_TIP) {
        set_sub2(c, MZM_RIDLEY_MULTI_TAIL_IDLE);
        s->y_spawn = 0;
        s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        set_box(s, -THREE_QUARTER_BLOCK_SIZE, THREE_QUARTER_BLOCK_SIZE, -THREE_QUARTER_BLOCK_SIZE,
                THREE_QUARTER_BLOCK_SIZE);
    } else {
        s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        set_box(s, -HALF_BLOCK_SIZE, HALF_BLOCK_SIZE, -HALF_BLOCK_SIZE, HALF_BLOCK_SIZE);
    }
}

/* RidleyTail */
static void tail(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    MzmSprite *main = slot(c, s->ram_slot);
    if (!main) return;
    s->palette_row = main->palette_row;
    if (main->health == 0 && s->pose < SPRITE_POSE_DESTROYED) {
        s->pose = SPRITE_POSE_DESTROYED;
        s->samus_collision = SSC_NONE;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        set_sub2(c, MZM_RIDLEY_MULTI_TAIL_DYING);
    }
    if (s->status & MZM_SPRITE_STATUS_NOT_DRAWN) {
        s->ignore_samus_timer = 1;
        if (!(main->status & MZM_SPRITE_STATUS_NOT_DRAWN))
            s->status &= (uint16_t)~(MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
    }
    if (main->status & MZM_SPRITE_STATUS_X_FLIP) s->status |= MZM_SPRITE_STATUS_X_FLIP;
    else s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
    bool destroyed = s->pose == SPRITE_POSE_DESTROYED;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        tail_init(c);
    } else if (destroyed) {
        if (main->status & MZM_SPRITE_STATUS_NOT_DRAWN) s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
        else s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        if (main->status == 0) {
            tail_dead(c);
            return;
        }
    } else if (s->room_slot == TAIL_TIP) {
        tail_tip(c, main);
    }
    if (s->room_slot == TAIL_TIP)
        update_anim(table(c, g->sub2.multi), &g->sub2.frame, &g->sub2.adc);
    tail_sync(c);
}

/* RidleyFireballCheckSlideOnWall */
static void fireball_check_wall(Ctx *c) {
    MzmSprite *s = c->s;
    if (ai_collision_at(c, s->y, s->x) != MZM_COLLISION_AIR) {
        s->pose = FIREBALL_POSE_DESTROY;
        return;
    }
    int16_t side = s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? s->hitbox_right : s->hitbox_left;
    if (ai_collision_at(c, s->y, (uint16_t)(s->x + side)) != MZM_COLLISION_AIR) s->pose = FIREBALL_POSE_SLIDE_ON_WALL_INIT;
}

/* SpriteUtilRidleyFireballMove */
static void fireball_move(Ctx *c, uint16_t dst_y, uint8_t y_speed, uint8_t x_speed, uint8_t divisor) {
    MzmSprite *s = c->s;
    if (s->work2 < x_speed) s->work2++;
    s->x = (uint16_t)(s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? s->x + (s->work2 >> divisor)
                                                                  : s->x - (s->work2 >> divisor));
    bool flip = false;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        if (s->work0 == 0) {
            bool update = true;
            if (s->work3 < y_speed) {
                if (s->y <= dst_y - PIXEL_SIZE) s->work3++;
                else {
                    s->work0 = s->work3;
                    update = false;
                }
            }
            if (update) s->y = (uint16_t)(s->y + (s->work3 >> divisor));
        } else if (--s->work0 == 0) {
            flip = true;
        } else {
            s->y = (uint16_t)(s->y + (s->work0 >> divisor));
        }
    } else {
        if (s->work0 == 0) {
            bool update = true;
            if (s->work3 < y_speed) {
                if (s->y >= dst_y + PIXEL_SIZE) s->work3++;
                else {
                    s->work0 = s->work3;
                    update = false;
                }
            }
            if (update) s->y = (uint16_t)(s->y - (s->work3 >> divisor));
        } else if (--s->work0 != 0) {
            uint16_t speed = (uint16_t)(s->work0 >> divisor);
            if ((uint16_t)(s->y - speed) & 0x8000) {
                flip = true;
                s->work0 = 0;
            } else {
                s->y = (uint16_t)(s->y - speed);
            }
        } else {
            flip = true;
        }
    }
    if (flip) {
        s->status ^= MZM_SPRITE_STATUS_FACING_DOWN;
        s->work3 = 1;
    }
}

/* RidleyFireball */
static void fireball(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: {
        /* RidleyFireballInit */
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->status |= MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE;
        s->kill_off_screen = true;
        s->draw_order = 2;
        if (s->sprite_id == MZM_SSPRITE_RIDLEY_BIG_FIREBALL) {
            s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
            set_box(s, -(HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE), HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE,
                    -(HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE), HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
            ai_set_anim(s, MZM_RIDLEY_FIREBALL_BIG);
        } else {
            s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
            set_box(s, -(HALF_BLOCK_SIZE - PIXEL_SIZE), HALF_BLOCK_SIZE - PIXEL_SIZE, -(HALF_BLOCK_SIZE - PIXEL_SIZE),
                    HALF_BLOCK_SIZE - PIXEL_SIZE);
            ai_set_anim(s, MZM_RIDLEY_FIREBALL_SMALL);
        }
        s->health = c->tables->secondary_health[s->sprite_id];
        s->rotation = 0;
        if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) s->status |= MZM_SPRITE_STATUS_X_FLIP;
        if (s->room_slot == FIREBALL_SAMUS_GRABBED) {
            s->samus_collision = SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT_NO_KNOCKBACK;
            s->scaling = 0x80;      /* Q_8_8(.5f) */
            s->pose = FIREBALL_POSE_DIAGONAL;
        } else if (s->room_slot & FIREBALL_FLOATING) {
            s->samus_collision = SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT;
            s->scaling = 0x100;
            s->pose = FIREBALL_POSE_FLOATING;
        } else {
            s->samus_collision = SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT;
            s->scaling = 0x80;
            s->work0 = 16;          /* CONVERT_SECONDS(.25f + 1.f / 60) */
            s->work2 = 0;
            s->pose = FIREBALL_POSE_DESCENDING;
            if (s->y < c->f->samus_y) s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        }
        break;
    }
    case FIREBALL_POSE_DESCENDING: {
        uint16_t movement = s->work2++;
        if (s->scaling < 0xF0) s->scaling += 0x10;  /* Q_8_8(.94f), Q_8_8(.065f) */
        s->x = (uint16_t)(s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? s->x + movement : s->x - movement);
        s->y = (uint16_t)(s->y + PIXEL_SIZE * 3);
        if (--s->work0 == 0) {
            s->pose = FIREBALL_POSE_ASCENDING;
            s->work1 = 0;
            s->work2 = QUARTER_BLOCK_SIZE;
            s->work0 = 0;
            s->work3 = QUARTER_BLOCK_SIZE;
        }
        break;
    }
    case FIREBALL_POSE_ASCENDING: {
        uint16_t offset = s->room_slot == FIREBALL_MIDDLE_BOTTOM ? FIREBALL_ASCENDING_Y_TARGET_OFFSET
                          : s->room_slot == FIREBALL_MIDDLE ? FIREBALL_ASCENDING_Y_TARGET_OFFSET * 2
                          : s->room_slot == FIREBALL_MIDDLE_TOP ? FIREBALL_ASCENDING_Y_TARGET_OFFSET * 3
                                                                : 0;
        uint16_t target = (uint16_t)(c->f->globals->sub1.y + BLOCK_SIZE + EIGHTH_BLOCK_SIZE - offset);
        fireball_move(c, target, HALF_BLOCK_SIZE, HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE, 2);
        fireball_check_wall(c);
        break;
    }
    case FIREBALL_POSE_FLOATING:
        s->x = (uint16_t)(s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? s->x + PIXEL_SIZE : s->x - PIXEL_SIZE);
        switch (s->room_slot & ~FIREBALL_FLOATING) {
        case FIREBALL_MIDDLE_TOP: s->y = (uint16_t)(s->y - PIXEL_SIZE); break;
        case FIREBALL_MIDDLE: s->y = (uint16_t)(s->y + PIXEL_SIZE); break;
        case FIREBALL_MIDDLE_BOTTOM: s->y = (uint16_t)(s->y + EIGHTH_BLOCK_SIZE); break;
        case FIREBALL_BOTTOM: s->y = (uint16_t)(s->y + QUARTER_BLOCK_SIZE - PIXEL_SIZE); break;
        default: break;
        }
        fireball_check_wall(c);
        break;
    case FIREBALL_POSE_DIAGONAL:
        if (s->scaling < 0xF0) {
            const MzmSprite *main = slot(c, s->ram_slot);
            if (main) s->y = (uint16_t)(main->y - (BLOCK_SIZE + HALF_BLOCK_SIZE));
            s->scaling += 0x10;
        } else {
            s->x = (uint16_t)(s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? s->x + EIGHTH_BLOCK_SIZE : s->x - EIGHTH_BLOCK_SIZE);
            s->y = (uint16_t)(s->y + EIGHTH_BLOCK_SIZE + PIXEL_SIZE / 2);
            fireball_check_wall(c);
        }
        break;
    case FIREBALL_POSE_SLIDE_ON_WALL_INIT:
        s->pose = FIREBALL_POSE_SLIDE_ON_WALL;
        s->work3 = 4;
        /* fallthrough */
    case FIREBALL_POSE_SLIDE_ON_WALL: {
        uint16_t movement = s->work3 / 4;
        if (movement < QUARTER_BLOCK_SIZE + PIXEL_SIZE) s->work3++;
        s->y = (uint16_t)(s->y + movement);
        if (ai_collision_at(c, s->y, s->x) != MZM_COLLISION_AIR) s->pose = FIREBALL_POSE_DESTROY;
        break;
    }
    case FIREBALL_POSE_DESTROY:
        s->status = 0;
        ai_particle_set(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_MEDIUM);
        mzm_sfx_play(MZM_SOUND_RIDLEY_FIREBALL_EXPLODING);
        return;
    default:
        ai_sprite_death(c, s->y, s->x, s->sprite_id == MZM_SSPRITE_RIDLEY_BIG_FIREBALL ? MZM_PE_SPRITE_EXPLOSION_BIG
                                                                                     : MZM_PE_SPRITE_EXPLOSION_MEDIUM);
        return;
    }
    s->rotation = (uint16_t)(s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? s->rotation + HALF_BLOCK_SIZE
                                                                         : s->rotation - HALF_BLOCK_SIZE);
}

void ai_ridley(Ctx *c) {
    if (!c->f->globals) return;
    if (!c->s->secondary) ridley(c);
    else if (c->s->sprite_id == MZM_SSPRITE_RIDLEY_PART) part(c);
    else if (c->s->sprite_id == MZM_SSPRITE_RIDLEY_TAIL) tail(c);
    else fireball(c);
}
