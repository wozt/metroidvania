/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Ruins Test, the Chozo ghost (src/sprites_ai/ruins_test.c):
 * the reflection that freezes Samus facing the background, the four
 * merging symbols, the ghost's circle and atom patterns carrying the
 * symbol Samus must shoot with a charged pistol (an uncharged shot hurts
 * her), the return to the center and the despawn that awards the Fully
 * Powered Suit. The ghost, outline, lightning, shootable symbol and the
 * closing reflection actors follow in a later slice. The palette and
 * graphics DMA of the reflections and symbols are not ported. */
#include "mzm_sprite_ai.h"
#include "mzm_blocks.h"

enum {
    POSE_SPAWNING = 0x1,
    POSE_TURNING_INTO_REFLECTION = 0x2,
    POSE_FREE_SAMUS = 0x3,
    POSE_CHECK_SAMUS_NOT_FACING_BACKGROUND = 0x4,
    POSE_MOTIONLESS = 0x8,
    POSE_SPAWN_GHOST = 0x9,
    POSE_MOVING_INIT = 0x22,
    POSE_MOVING_CIRCLE_PATTERN = 0x23,
    POSE_MOVING_ATOM_PATTERN = 0x25,
    POSE_BACK_TO_CENTER = 0x67,
    POSE_CHECK_GHOST_DISAPPEARING = 0x68,
    POSE_DESPAWN = 0x69,
    POSE_DEAD = 0x6A,

    SYMBOL_POSE_DELAY_BEFORE_MUSIC = 0x8,
    SYMBOL_POSE_UPDATE_PALETTE = 0x9,
    SYMBOL_POSE_MERGING = 0x23,
    SYMBOL_POSE_DISAPPEARING = 0x25,

    REFLECTION_START_POSE_WAITING = 0x8,
    REFLECTION_START_POSE_LEAVING = 0x9,

    COVER_POSE_IDLE = 0x8,
    COVER_POSE_KILL = 0x9,

    GHOST_OUTLINE_PART_OUTLINE = 0,
    GHOST_OUTLINE_PART_SHOOTING_LIGHTNING = 1,
    GHOST_OUTLINE_PART_SHOOTING_GROUND_LIGHTNING = 2,
    GHOST_PART_GHOST = 0,
};

/* RuinsTestFightStage (gSubSpriteData1.work3). */
enum {
    STAGE_ON_GOING, STAGE_LAST_SYMBOL_HIT, STAGE_GHOST_AT_CENTER, STAGE_GHOST_FADING_BACK,
    STAGE_GHOST_STARTING_TO_DISAPPEAR, STAGE_STARTING_CUTSCENE_FADE, STAGE_STARTING_CUTSCENE,
    STAGE_STARTING_SUIT_ANIM, STAGE_SUIT_ANIM_ENDED,
};

#define PI_ 128
#define PI_2_ 64

/* SIN / COS on sSineTable (Q8.8). */
static int32_t sine(const Ctx *c, uint8_t angle) { return c->tables->sine[angle]; }
static int32_t cosine(const Ctx *c, uint8_t angle) { return c->tables->sine[angle + 64]; }

/* RuinsTestCalculateDelay: sRandomNumberTable[gFrameCounter8Bit] + delay. */
static void calculate_delay(Ctx *c, uint8_t delay) {
    c->f->globals->boss_work5 = (uint16_t)(c->tables->generic_random[c->frame8] + delay);
}

/* RuinsTestChangeCcaa: the thirteen ceiling blocks above the arena. */
static void change_ceiling(Ctx *c, uint8_t caa) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (!h || !h->apply_caa) return;
    uint16_t y = (uint16_t)(c->f->globals->boss_work1 - (BLOCK_SIZE * 6 + HALF_BLOCK_SIZE));
    uint16_t x = c->f->globals->boss_work2;
    for (int i = 0; i <= 6; ++i) h->apply_caa(h->context, y, (uint16_t)(x - BLOCK_SIZE * i), caa);
    for (int i = 1; i <= 6; ++i) h->apply_caa(h->context, y, (uint16_t)(x + BLOCK_SIZE * i), caa);
}

/* The BIOS Sqrt. */
static uint32_t isqrt(uint32_t value) {
    uint32_t root = 0, bit = 1u << 30;
    while (bit > value) bit >>= 2;
    while (bit) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

/* RuinsTestMoveToPosition: `speed * multiplier` subpixels from the start
 * along the straight line to the target. */
static void move_to_position(Ctx *c, uint16_t y0, uint16_t x0, uint16_t ty, uint16_t tx,
                             uint16_t multiplier, uint16_t speed) {
    MzmSprite *s = c->s;
    speed = (uint16_t)(speed * multiplier);
    uint16_t dy, dx;
    if (ty > y0) {
        dy = (uint16_t)(ty - y0);
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
    } else {
        dy = (uint16_t)(y0 - ty);
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
    }
    if (tx > x0) {
        dx = (uint16_t)(tx - x0);
        s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
    } else {
        dx = (uint16_t)(x0 - tx);
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
    }
    uint16_t total = (uint16_t)isqrt((uint32_t)dx * dx + (uint32_t)dy * dy);
    if (total == 0) return;
    int32_t my = (int32_t)(speed * (dy * 1024u / total)) >> 10;
    int32_t mx = (int32_t)(speed * (dx * 1024u / total)) >> 10;
    s->y = (uint16_t)(s->status & MZM_SPRITE_STATUS_FACING_DOWN ? y0 + my : y0 - my);
    s->x = (uint16_t)(s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? x0 + mx : x0 - mx);
}

static bool projectile_live(const MzmProjectile *p) {
    return p->active && p->stage != MZM_STAGE_TUMBLING && p->stage > MZM_STAGE_SPAWNING;
}

/* RuinsTestProjectileCollision: a pistol shot on the active symbol
 * reflects onto Samus (25, 50 or 100 energy by difficulty). */
static bool projectile_collision(Ctx *c) {
    MzmSprite *s = c->s;
    MzmWeapons *w = c->f->weapons;
    if (!w) return false;
    uint16_t damage = c->f->difficulty == 0 ? 25 : c->f->difficulty == 2 ? 100 : 50;
    uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
    uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
    for (int i = 0; i < MZM_MAX_PROJECTILES; ++i) {
        MzmProjectile *p = &w->list[i];
        if (!projectile_live(p) ||
            (p->type != MZM_PROJECTILE_PISTOL && p->type != MZM_PROJECTILE_CHARGED_PISTOL))
            continue;
        uint16_t py = (uint16_t)p->y, px = (uint16_t)p->x;
        if (!(px > left && px < right && py > top && py < bottom)) continue;
        /* PROJ_STATUS_CAN_AFFECT_ENVIRONMENT cleared: it flies on harmless. */
        p->stage = MZM_STAGE_TUMBLING;
        MzmSpriteSamusLink *link = c->f->samus_link;
        if (link && link->invincibility == 0) {
            MzmEquipment *eq = c->f->equipment;
            if (eq) eq->energy = eq->energy > damage ? eq->energy - damage : 0;
            link->set_forced = MZM_FORCED_DAMAGED_BY_RUINS_TEST;
            link->set_pose = MZM_POSE_HURT_REQUEST;
            return true;
        }
    }
    return false;
}

/* RuinsTestCheckHitByChargedPistol */
static bool charged_pistol_flying(const Ctx *c) {
    const MzmWeapons *w = c->f->weapons;
    for (int i = 0; w && i < MZM_MAX_PROJECTILES; ++i)
        if (projectile_live(&w->list[i]) && w->list[i].type == MZM_PROJECTILE_CHARGED_PISTOL) return true;
    return false;
}

/* RuinsTestCheckSymbolShot: the shootable symbol flashing the ghost; the
 * fourth one ends the fight. */
static bool check_symbol_shot(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *state = &c->f->globals->sub1;
    if (s->isft == 0 || s->health == 100) return false;
    mzm_sfx_play(MZM_SOUND_RUINS_TEST_SHOOTABLE_SYMBOL_SHOT);
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->health = 100;
    if (++state->health > 3) {
        s->pose = POSE_BACK_TO_CENTER;
        state->work3 = STAGE_LAST_SYMBOL_HIT;
        state->work1 = 1;
        s->scaling = 0;
        s->samus_collision = SSC_NONE;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->isft = (uint8_t)(s->isft + 60);
        return true;
    }
    state->work1 = 128;
    calculate_delay(c, 240);
    return false;
}

/* RuinsTestCheckSamusHurting: MOSAIC marks a reflected shot. */
static bool samus_hurting(Ctx *c) {
    MzmSprite *s = c->s;
    if (!(s->status & MZM_SPRITE_STATUS_MOSAIC)) return false;
    if (c->f->samus_link && c->f->samus_link->samus_pose == MZM_POSE_GETTING_HURT) return true;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_MOSAIC;
    return false;
}

/* RuinsTestUpdateSymbol: shows the shootable symbol for its time, then
 * waits a random delay; true when the pattern should change. */
static bool update_symbol(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    MzmSubSpriteState *state = &g->sub1;
    if (state->work1 & 127) {
        if (state->work1 == 1) {
            if (!charged_pistol_flying(c)) state->work1--;
        } else {
            state->work1--;
        }
        if (s->status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES) {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
            int slot = ai_spawn_secondary(c, MZM_SSPRITE_RUINS_TEST_SHOOTABLE_SYMBOL, 0, s->gfx_slot,
                                          state->y, state->x, 0);
            if (slot >= 0) c->f->sprites[slot].health = state->health;
        }
        return false;
    }
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    if (state->work1 & 0x80) return false;
    if (g->boss_work3 == 0) {
        calculate_delay(c, 240);
        return true;
    }
    if (g->boss_work5 == 0) {
        static const uint8_t active[4] = {120, 90, 60, 40};
        state->work1 = active[state->health < 4 ? state->health : 3];
        calculate_delay(c, 120);
    } else {
        g->boss_work5--;
        if (projectile_collision(c)) {
            s->status |= MZM_SPRITE_STATUS_MOSAIC;
            calculate_delay(c, 120);
        }
    }
    return false;
}

/* RuinsTestGhostMove: around (yPositionSpawn, xPositionSpawn) at the
 * radius in `scaling`, the angle in work1. */
static void ghost_move(Ctx *c, uint8_t d_angle) {
    MzmSprite *s = c->s;
    if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) s->work1 = (uint8_t)(s->work1 + d_angle);
    else s->work1 = (uint8_t)(s->work1 - d_angle);
    int32_t radius = (int16_t)s->scaling;
    int32_t sn = sine(c, s->work1), cs = cosine(c, s->work1);
    s->y = (uint16_t)(sn < 0 ? s->y_spawn - (int16_t)((-sn * radius) >> 8) : s->y_spawn + (int16_t)((sn * radius) >> 8));
    s->x = (uint16_t)(cs < 0 ? s->x_spawn - (int16_t)((-cs * radius) >> 8) : s->x_spawn + (int16_t)((cs * radius) >> 8));
}

static MzmSpriteSamusLink *link_of(const Ctx *c) { return c->f->samus_link; }

/* RuinsTestInit: Samus frozen facing the background five blocks below. */
static void init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    MzmSubSpriteState *state = &g->sub1;
    s->draw_top = s->draw_bottom = s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    s->hitbox_top = s->hitbox_left = -(BLOCK_SIZE + PIXEL_SIZE);
    s->hitbox_bottom = s->hitbox_right = BLOCK_SIZE + PIXEL_SIZE;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->samus_collision = SSC_HURTS_SAMUS;
    s->draw_order = 12;
    s->health = 100;
    s->frozen_row_offset = 1;
    ai_set_anim(s, MZM_RUINS_TEST_SPAWNING);
    s->pose = POSE_SPAWNING;
    s->work0 = 0;
    s->y = (uint16_t)(s->y + QUARTER_BLOCK_SIZE);
    g->boss_work1 = s->y_spawn = state->y = s->y;
    g->boss_work2 = s->x_spawn = state->x = s->x;
    state->work3 = STAGE_ON_GOING;
    state->health = 0;
    state->work1 = 0;
    calculate_delay(c, 120);
    ai_spawn_secondary(c, MZM_SSPRITE_RUINS_TEST_SYMBOL, 0, s->gfx_slot, s->y, s->x, 0);
    MzmSpriteSamusLink *link = link_of(c);
    if (link) {
        link->set_pose = MZM_POSE_FACING_THE_BACKGROUND_SUITLESS;
        link->moved = true;
        link->y = (uint16_t)(s->y + BLOCK_SIZE * 5 - QUARTER_BLOCK_SIZE - ONE_SUB_PIXEL);
        link->x = s->x;
    }
    g->boss_work3 = 0;          /* pattern timer */
    g->boss_work4 = 0;          /* movement stopped */
    g->boss_work6 = 0;          /* lightning timer */
    g->boss_work7 = 0;          /* symbols placed */
}

/* RuinsTestSpawning: held on its first frame until the symbols merge. */
static void spawning(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->work0 == 0) {
        s->adc = 0;
        return;
    }
    if (s->work0 == 2) {
        ai_set_anim(s, MZM_RUINS_TEST_TURNING_INTO_REFLECTION);
        s->pose = POSE_TURNING_INTO_REFLECTION;
        mzm_sfx_play(MZM_SOUND_RUINS_TEST_TURNING_INTO_FIRST_REFLECTION);
    }
}

/* RuinsTestTurningIntoReflection */
static void turning_into_reflection(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->frame == 7) {
        if (s->adc == 1)
            ai_spawn_secondary(c, MZM_SSPRITE_RUINS_TEST_REFLECTION_COVER, 0, s->gfx_slot, s->y, s->x, 0);
        else if (s->adc == 4)
            ai_spawn_secondary(c, MZM_SSPRITE_RUINS_TEST_SAMUS_REFLECTION_START, 0, s->gfx_slot,
                               (uint16_t)(s->y + (BLOCK_SIZE + HALF_BLOCK_SIZE - PIXEL_SIZE)), s->x, 0);
    }
    if (ai_animation_ending(c, 1)) {
        ai_set_anim(s, MZM_RUINS_TEST_SYMBOL);
        s->pose = POSE_FREE_SAMUS;
        s->work0 = 32;                                 /* CONVERT_SECONDS(.5f + 1.f / 30) */
    }
}

/* RuinsTestSpawnGhost: the outline, then the ghost. */
static void spawn_ghost(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if (g->sub1.health != 0 && g->boss_work7 == 0) return;
    s->work0--;
    if (s->work0 == 84)                                /* CONVERT_SECONDS(1.4f) */
        ai_spawn_secondary(c, MZM_SSPRITE_RUINS_TEST_GHOST_OUTLINE, GHOST_OUTLINE_PART_OUTLINE,
                           s->gfx_slot, s->y, s->x, 0);
    else if (s->work0 == 64)                           /* CONVERT_SECONDS(1.f + 1.f / 15) */
        ai_spawn_secondary(c, MZM_SSPRITE_RUINS_TEST_GHOST, GHOST_PART_GHOST, s->gfx_slot, s->y, s->x, 0);
    else if (s->work0 == 0) {
        s->pose = POSE_MOVING_INIT;
        g->boss_work3 = 1200;
    }
}

/* RuinsTestMoveCirclePattern: a widening circle, faster in its second
 * half; after two symbols it waits for a corner to start the atom. */
static void move_circle(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if (samus_hurting(c)) return;
    check_symbol_shot(c);
    uint8_t speed = g->boss_work3 < 600 && (c->frame8 & 3) == 0 ? 2 : 1;
    if (s->scaling < PI_ + PI_2_) {
        s->scaling = (uint16_t)(s->scaling + 3);       /* PI * .0234375f */
        g->boss_work6 = 0;
    } else {
        s->scaling = PI_ + PI_2_;
    }
    if (!g->boss_work4) ghost_move(c, speed);
    g->sub1.y = s->y;
    g->sub1.x = s->x;
    if (!update_symbol(c) && g->sub1.health <= 1) return;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
    uint8_t corner = (uint8_t)(s->work1 + PI_2_ + 1);
    if (corner > 2 || s->scaling != PI_ + PI_2_) return;  /* PI * .015625f */
    s->pose = POSE_MOVING_ATOM_PATTERN;
    s->work2 = 0;
    g->boss_work3 = 1200;
    s->y_spawn = (uint16_t)(s->y_spawn - BLOCK_SIZE * 3);
    if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
        s->work1 = 0;
        s->x_spawn = (uint16_t)(s->x_spawn - BLOCK_SIZE * 3);
    } else {
        s->work1 = PI_;
        s->x_spawn = (uint16_t)(s->x_spawn + BLOCK_SIZE * 3);
    }
}

/* One RuinsTestMoveAtomPattern bounce: at `angle`, the next angle and the
 * center's move (in blocks). */
static bool bounce(MzmSprite *s, uint8_t angle, uint8_t at, uint8_t next, int dy, int dx) {
    if (angle != at) return false;
    s->work1 = next;
    s->y_spawn = (uint16_t)(s->y_spawn + dy * BLOCK_SIZE);
    s->x_spawn = (uint16_t)(s->x_spawn + dx * BLOCK_SIZE);
    return true;
}

/* RuinsTestMoveAtomPattern: six arcs around moving centers, drawing an
 * atom, faster during its first frames. */
static void move_atom(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if (g->boss_work3 == 0) g->boss_work3++;
    if (samus_hurting(c) || check_symbol_shot(c)) return;
    uint8_t speed, flag;
    if (g->boss_work3 < 2) {
        speed = (c->frame8 & 3) == 0 ? 3 : 2;
        flag = (uint8_t)~3;
    } else {
        speed = g->boss_work3 < 600 || (c->frame8 & 1) ? 2 : 1;
        flag = (uint8_t)~1;
    }
    if (!g->boss_work4) ghost_move(c, speed);
    uint8_t angle = flag & s->work1;
    bool right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT, bounced = false;
    switch (s->work2) {
    case 0:
        bounced = right ? bounce(s, angle, PI_, PI_ + PI_2_, 3, -3) : bounce(s, angle, 0, PI_ + PI_2_, 3, 3);
        break;
    case 1:
        bounced = right ? bounce(s, angle, PI_2_, PI_, 3, 3) : bounce(s, angle, PI_2_, 0, 3, -3);
        break;
    case 2:
        bounced = right ? bounce(s, angle, 0, PI_, 0, 6) : bounce(s, angle, PI_, 0, 0, -6);
        break;
    case 3:
        bounced = right ? bounce(s, angle, 0, PI_2_, -3, 3) : bounce(s, angle, PI_, PI_2_, -3, -3);
        break;
    case 4:
        bounced = right ? bounce(s, angle, PI_ + PI_2_, 0, -3, -3) : bounce(s, angle, PI_ + PI_2_, PI_, -3, 3);
        break;
    case 5:
        if (right ? bounce(s, angle, PI_, 0, 0, -6) : bounce(s, angle, 0, PI_, 0, 6)) s->work2 = 0;
        break;
    default: break;
    }
    if (bounced) s->work2++;
    g->sub1.y = s->y;
    g->sub1.x = s->x;
    update_symbol(c);
}

/* RuinsTestMoveToCenter: back to the spawn point, accelerating, once the
 * last hit stops flashing. */
static void move_to_center(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if (s->isft & 0x7F) return;
    uint16_t ty = g->boss_work1, tx = g->boss_work2;
    uint16_t dx = tx > s->x ? (uint16_t)(tx - s->x) : (uint16_t)(s->x - tx);
    uint16_t dy = ty > s->y ? (uint16_t)(ty - s->y) : (uint16_t)(s->y - ty);
    if (dx < PIXEL_SIZE && dy < PIXEL_SIZE) {
        s->y = ty;
        s->x = tx;
        s->pose = POSE_CHECK_GHOST_DISAPPEARING;
        g->sub1.work3 = STAGE_GHOST_AT_CENTER;
    } else {
        move_to_position(c, g->sub1.y, g->sub1.x, ty, tx, s->scaling++, PIXEL_SIZE);
    }
}

/* RuinsTestDespawn: the despawn animation, the closing reflection, then
 * the ceiling removed, the suit event and the cutscene request. */
static void despawn(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if (s->work0) {
        if (--s->work0 == 0) ai_set_anim(s, MZM_RUINS_TEST_DESPAWN);
    } else if (s->work1) {
        if (--s->work1 == 0)
            ai_spawn_secondary(c, MZM_SSPRITE_RUINS_TEST_SAMUS_REFLECTION_END, 0, s->gfx_slot,
                               (uint16_t)(s->y + (BLOCK_SIZE + HALF_BLOCK_SIZE - PIXEL_SIZE)), s->x, 0);
    } else if (g->sub1.work3 == STAGE_STARTING_CUTSCENE_FADE) {
        s->pose = POSE_DEAD;
        change_ceiling(c, MZM_CAA_REMOVE_SOLID);
        const MzmSpriteHooks *h = c->f->hooks;
        if (h && h->set_event) h->set_event(h->context, MZM_EVENT_FULLY_POWERED_SUIT_OBTAINED);
    }
}

/* RuinsTest */
static void ruins_test(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    MzmSpriteSamusLink *link = link_of(c);
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: init(c); break;
    case POSE_SPAWNING: spawning(c); break;
    case POSE_TURNING_INTO_REFLECTION: turning_into_reflection(c); break;
    case POSE_FREE_SAMUS:
        if (s->work0 && --s->work0 == 0) {
            if (link) {
                link->set_last_wall = true;
                link->last_wall = 0;
            }
            s->pose = POSE_CHECK_SAMUS_NOT_FACING_BACKGROUND;
        }
        break;
    case POSE_CHECK_SAMUS_NOT_FACING_BACKGROUND:
        if (link && link->samus_pose != MZM_POSE_FACING_THE_BACKGROUND_SUITLESS) {
            s->pose = POSE_MOTIONLESS;
            g->boss_work3 = 1800;
        }
        break;
    case POSE_MOTIONLESS:
        if (samus_hurting(c)) break;
        check_symbol_shot(c);
        if (update_symbol(c) || g->sub1.health != 0) {
            s->pose = POSE_SPAWN_GHOST;
            s->work0 = 120;
            ai_random_x_direction(c);
        }
        break;
    case POSE_SPAWN_GHOST: spawn_ghost(c); break;
    case POSE_MOVING_INIT:
        s->work1 = PI_2_;
        s->scaling = 0;
        s->pose = POSE_MOVING_CIRCLE_PATTERN;
        s->status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        /* fallthrough */
    case POSE_MOVING_CIRCLE_PATTERN: move_circle(c); break;
    case POSE_MOVING_ATOM_PATTERN: move_atom(c); break;
    case POSE_BACK_TO_CENTER: move_to_center(c); break;
    case POSE_CHECK_GHOST_DISAPPEARING:
        if (g->sub1.work3 == STAGE_GHOST_STARTING_TO_DISAPPEAR) {
            s->pose = POSE_DESPAWN;
            s->work0 = 100;                            /* CONVERT_SECONDS(1.f) + TWO_THIRD_SECOND */
            s->work1 = 34;                             /* CONVERT_SECONDS(.5f + 1.f / 15) */
        }
        break;
    case POSE_DESPAWN: despawn(c); break;
    default: break;
    }

    if (g->sub1.work3 == STAGE_ON_GOING) {
        if (!g->boss_work4) {
            /* Lightning: along the ground every 256 frames from the third
             * symbol (not on easy), or at Samus when she stands still. */
            if (g->boss_work7 > 2 && c->frame8 == 0 && c->f->difficulty != 0) {
                g->boss_work6 = 0;
                g->boss_work4 = 1;
                ai_spawn_secondary(c, MZM_SSPRITE_RUINS_TEST_GHOST_OUTLINE,
                                   GHOST_OUTLINE_PART_SHOOTING_GROUND_LIGHTNING, s->gfx_slot,
                                   g->boss_work1, g->boss_work2, 0);
            }
            if (g->boss_work6 > 30 && c->frame8 != 0) {
                g->boss_work6 = 0;
                g->boss_work4 = 1;
                ai_spawn_secondary(c, MZM_SSPRITE_RUINS_TEST_GHOST_OUTLINE,
                                   GHOST_OUTLINE_PART_SHOOTING_LIGHTNING, s->gfx_slot,
                                   g->boss_work1, g->boss_work2, 0);
            }
        }
        if (g->boss_work3) g->boss_work3--;
        if (s->pose > POSE_MOVING_INIT && g->boss_work7 != 0) {
            uint16_t samus_y = (uint16_t)(c->f->samus_y - 1);
            if (c->f->samus_x == g->previous_samus_x && samus_y == g->previous_samus_y) g->boss_work6++;
            else g->boss_work6 = 0;
        }
    }
    g->previous_samus_y = (uint16_t)(c->f->samus_y - 1);
    g->previous_samus_x = c->f->samus_x;
}

/* RuinsTestSymbol: the four symbols light up with the battle music, merge
 * (releasing the reflection) and disappear. */
static void symbol(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSprite *owner = c->f->sprites && s->ram_slot < c->f->sprite_count ? &c->f->sprites[s->ram_slot] : NULL;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->draw_top = s->draw_bottom = BLOCK_SIZE * 4 / PIXEL_SIZE;
        s->draw_horizontal = (BLOCK_SIZE * 6 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        ai_set_anim(s, MZM_RUINS_TEST_SYMBOL_FOUR_SYMBOLS);
        s->pose = SYMBOL_POSE_DELAY_BEFORE_MUSIC;
        s->samus_collision = SSC_NONE;
        s->work0 = 60;
        s->draw_order = 13;
        c->f->globals->disable_pause = true;
        break;
    case SYMBOL_POSE_DELAY_BEFORE_MUSIC:
        if (--s->work0 == 0) {
            s->pose = SYMBOL_POSE_UPDATE_PALETTE;
            s->frame = 1;
            s->adc = 0;
            mzm_sfx_music_play(MZM_MUSIC_RUINS_TEST_BATTLE_WITH_INTRO, 0);
            mzm_sfx_play(MZM_SOUND_RUINS_TEST_SPAWNING);
        }
        break;
    case SYMBOL_POSE_UPDATE_PALETTE:
        if (s->adc == 1 && s->frame == 10) mzm_sfx_play(MZM_SOUND_RUINS_TEST_SYMBOLS_MERGING);
        if (ai_animation_ending(c, 1)) {
            ai_set_anim(s, MZM_RUINS_TEST_SYMBOL_MERGING);
            s->pose = SYMBOL_POSE_MERGING;
            s->work0 = 120;
        }
        break;
    case SYMBOL_POSE_MERGING:
        if (--s->work0 == 0) {
            ai_set_anim(s, MZM_RUINS_TEST_SYMBOL_DISAPPEARING);
            s->pose = SYMBOL_POSE_DISAPPEARING;
            break;
        }
        if (owner && s->work0 == 60) owner->work0 = 2;
        else if (owner && s->work0 == 110) owner->work0 = 1;  /* CONVERT_SECONDS(2.f - 1.f / 6) */
        break;
    case SYMBOL_POSE_DISAPPEARING:
        if (ai_animation_ending(c, 1)) s->status = 0;
        break;
    default: break;
    }
}

/* RuinsTestSamusReflectionStart: Samus's reflection, gone two frames
 * after she turns from the background (re-enabling the pause). */
static void reflection_start(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->draw_top = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        ai_set_anim(s, MZM_RUINS_TEST_SAMUS_REFLECTION);
        s->pose = REFLECTION_START_POSE_WAITING;
        s->samus_collision = SSC_NONE;
        s->draw_order = 3;
        s->work0 = 1;
        break;
    case REFLECTION_START_POSE_WAITING:
        if (s->work0 < 16) s->work0++;
        if (c->f->samus_link && c->f->samus_link->samus_pose != MZM_POSE_FACING_THE_BACKGROUND_SUITLESS) {
            c->f->globals->disable_pause = false;
            s->work0 = 2;                              /* CONVERT_SECONDS(1.f / 30) */
            s->pose = REFLECTION_START_POSE_LEAVING;
        }
        break;
    case REFLECTION_START_POSE_LEAVING:
        /* The PE_SAMUS_REFLECTION particle is not ported. */
        if (--s->work0 == 0) s->status = 0;
        break;
    default: break;
    }
}

/* RuinsTestReflectionCover: over the ghost while no symbol is shootable. */
static void reflection_cover(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status |= MZM_SPRITE_STATUS_HIGH_PRIORITY;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->draw_top = s->draw_bottom = s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        ai_set_anim(s, MZM_RUINS_TEST_REFLECTION_COVER);
        s->pose = COVER_POSE_IDLE;
        s->samus_collision = SSC_NONE;
        s->draw_order = 2;
        break;
    case COVER_POSE_IDLE: {
        const MzmSprite *owner = c->f->sprites && s->ram_slot < c->f->sprite_count ? &c->f->sprites[s->ram_slot] : NULL;
        if (owner) {
            s->y = owner->y;
            s->x = owner->x;
        }
        if (g->sub1.work1 == 0) s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        else s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
        if (g->sub1.work3 == STAGE_GHOST_STARTING_TO_DISAPPEAR) s->pose = COVER_POSE_KILL;
        break;
    }
    case COVER_POSE_KILL: s->status = 0; break;
    default: break;
    }
}

void ai_ruins_test(Ctx *c) {
    MzmSprite *s = c->s;
    if (!c->f->globals) {
        s->status = 0;
        return;
    }
    if (!s->secondary) {
        ruins_test(c);
        return;
    }
    switch (s->sprite_id) {
    case MZM_SSPRITE_RUINS_TEST_SYMBOL: symbol(c); break;
    case MZM_SSPRITE_RUINS_TEST_SAMUS_REFLECTION_START: reflection_start(c); break;
    case MZM_SSPRITE_RUINS_TEST_REFLECTION_COVER: reflection_cover(c); break;
    default: s->status = 0; break;              /* not ported yet */
    }
}
