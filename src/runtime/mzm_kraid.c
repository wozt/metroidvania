/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Kraid (src/sprites_ai/kraid.c): the head sprite drives
 * gSubSpriteData1 (the multi-sprite animation of the parts and the body
 * drawn by BG2, moved with gBg2Movement), rises, steps left and right,
 * opens its mouth to missiles and charged beams, throws nails, swipes, and
 * shoots spikes from the holes of its belly that break the platforms;
 * its death sinks it, explodes the parts and removes BG2. Comments name
 * the pinned decompilation's routines. Not ported: the minimap chunk
 * update and the statue opening cutscene request of its death, and the
 * in-game timer at bosses. */
#include "mzm_projectiles.h"
#include "mzm_screen_shake.h"
#include "mzm_sprite_ai.h"

enum {
    KRAID_POSE_GO_UP = 0x1,
    KRAID_POSE_CHECK_FULLY_UP = 0x2,
    KRAID_POSE_FIRST_STEP_INIT = 0x8,
    KRAID_POSE_FIRST_STEP = 0x9,
    KRAID_POSE_STANDING_INIT = 0xE,
    KRAID_POSE_STANDING = 0xF,
    KRAID_POSE_STANDING_BETWEEN_STEPS_INIT = 0x10,
    KRAID_POSE_STANDING_BETWEEN_STEPS = 0x11,
    KRAID_POSE_SECOND_STEP_INIT = 0x22,
    KRAID_POSE_SECOND_STEP = 0x23,
    KRAID_POSE_DYING_INIT = 0x62,
    KRAID_POSE_DYING = 0x67,
    KRAID_POSE_DEAD_STATIONARY = 0x68,
    PART_POSE_IDLE = 0xE,
    PART_POSE_CHECK_ATTACK = 0x12,
    PART_POSE_CHECK_THROW_NAILS = 0x10,
    PART_POSE_CHECK_SPAWN_SPIKES = 0x42,
    PART_POSE_SPAWN_SPIKES = 0x43,
    PART_POSE_CHECK_PROJECTILES = 0x44,
    PART_POSE_DYING_INIT = 0x62,
    PART_POSE_ARM_DYING = 0x67,
    PART_POSE_DYING_STATIONARY = 0x68,
    SPIKE_POSE_DELAY_BEFORE_MOVING = 0x9,
    SPIKE_POSE_MOVING = 0x23,
    SPIKE_POSE_IN_WALL = 0x25,
    NAIL_POSE_MOVING = 0x9,
    NAIL_SLOW_ROTATION = 0,
    /* KraidPartId */
    PART_BELLY = 0, PART_LEFT_ARM, PART_TOP_HOLE_LEFT, PART_TOP_HOLE_RIGHT, PART_MIDDLE_HOLE_LEFT,
    PART_MIDDLE_HOLE_RIGHT, PART_KRAID, PART_BOTTOM_HOLE_LEFT, PART_BOTTOM_HOLE_RIGHT,
    PART_LEFT_FEET, PART_RIGHT_ARM, PART_RIGHT_FEET,
    DESTROYED_TOP = 1, DESTROYED_MIDDLE = 2, DESTROYED_BOTTOM = 4,
    KRAID_HEALTH = 300,             /* GET_PSPRITE_HEALTH(PSPRITE_KRAID) */
    ISFT_KRAID_HIT = 17,            /* CONVERT_SECONDS(.25f + 1.f / 30) */
};

/* SPRITE_CLEAR_AND_SET_ISFT */
static void set_isft(MzmSprite *s, uint8_t value) { s->isft = (uint8_t)((s->isft & 0x80) | value); }

static const MzmMultiSprite *multi(const Ctx *c) {
    return &c->tables->kraid_multi[c->f->globals->sub1.multi < MZM_KRAID_MULTI_COUNT ? c->f->globals->sub1.multi : 0];
}

static const int16_t *multi_part(const Ctx *c, uint8_t part) {
    const MzmSpriteGlobals *g = c->f->globals;
    return multi(c)->parts[g->sub1.frame < MZM_MULTI_FRAMES ? g->sub1.frame : 0][part < MZM_MULTI_PARTS ? part : 0];
}

static void set_multi(Ctx *c, uint8_t table) {
    MzmSpriteGlobals *g = c->f->globals;
    g->sub1.multi = table;
    g->sub1.adc = 0;
    g->sub1.frame = 0;
}

/* SpriteUtilUpdateSubSprite1Anim */
static void update_sub1_anim(Ctx *c) {
    MzmSpriteGlobals *g = c->f->globals;
    const MzmMultiSprite *m = multi(c);
    g->sub1.adc++;
    if (m->timers[g->sub1.frame] < g->sub1.adc) {
        g->sub1.adc = 1;
        g->sub1.frame++;
        if (g->sub1.frame >= MZM_MULTI_FRAMES || m->timers[g->sub1.frame] == 0) g->sub1.frame = 0;
    }
}

/* SpriteUtilHasSubSprite1AnimationNearlyEnded */
static bool sub1_nearly_ended(const Ctx *c) {
    const MzmSpriteGlobals *g = c->f->globals;
    const MzmMultiSprite *m = multi(c);
    uint8_t adc = (uint8_t)(g->sub1.adc + 2);
    unsigned next = g->sub1.frame + 1u;
    return m->timers[g->sub1.frame] < adc && (next >= MZM_MULTI_FRAMES || m->timers[next] == 0);
}

/* SpriteUtilSyncCurrentSpritePositionWithSubSprite1Position */
static void sync_position(Ctx *c) {
    const MzmSpriteGlobals *g = c->f->globals;
    const int16_t *p = multi_part(c, c->s->room_slot);
    c->s->y = (uint16_t)(g->sub1.y + p[1]);
    c->s->x = (uint16_t)(g->sub1.x + p[2]);
}

/* KraidSyncSubSprites: the part's OAM from the multi-sprite data. */
static void sync_sub_sprites(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t oam = (uint8_t)multi_part(c, s->room_slot)[0];
    if (oam < MZM_KRAID_ANIMS && s->anim != oam) ai_set_anim(s, oam);
    sync_position(c);
}

static void remove_solid(const Ctx *c, uint16_t y, uint16_t x) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->remove_solid) h->remove_solid(h->context, y, x);
}

static MzmSprite *slot(const Ctx *c, uint8_t index) {
    return c->f->sprites && index < c->f->sprite_count ? &c->f->sprites[index] : NULL;
}

/* PROJ_STATUS_CAN_AFFECT_ENVIRONMENT and a movement stage past spawning:
 * not tumbling, a bomb only while its explosion hits. */
static bool live_projectile(const MzmProjectile *p) {
    if (!p->active || p->stage == MZM_STAGE_TUMBLING) return false;
    if (p->type == MZM_PROJECTILE_POWER_BOMB) return false;
    if (p->type == MZM_PROJECTILE_BOMB) return p->hits_sprites;
    return p->stage > MZM_STAGE_SPAWNING;
}

/* KraidCheckProjectilesCollidingWithBelly (beams and missiles). */
static void belly_projectiles(Ctx *c) {
    MzmSprite *s = c->s;
    MzmWeapons *weapons = c->f->weapons;
    if (!weapons) return;
    uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
    uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
    for (int i = 0; i < MZM_MAX_PROJECTILES; ++i) {
        MzmProjectile *p = &weapons->list[i];
        if (!live_projectile(p) || p->type == MZM_PROJECTILE_BOMB) continue;
        uint16_t py = (uint16_t)p->y, px = (uint16_t)p->x;
        if (!(px > left && px < right && py > top && py < bottom)) continue;
        p->active = false;
        if (c->rng & 1) {
            ai_debris_init(c, 0x12, (uint16_t)(py + QUARTER_BLOCK_SIZE), (uint16_t)(px - EIGHTH_BLOCK_SIZE));
            ai_debris_init(c, 0x13, (uint16_t)(py - (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE + PIXEL_SIZE / 2)),
                           (uint16_t)(px + QUARTER_BLOCK_SIZE + PIXEL_SIZE));
        } else {
            ai_debris_init(c, 0x12, py, (uint16_t)(px + QUARTER_BLOCK_SIZE - PIXEL_SIZE));
            ai_debris_init(c, 0x13, (uint16_t)(py - (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)),
                           (uint16_t)(px - (HALF_BLOCK_SIZE - PIXEL_SIZE / 2)));
        }
        mzm_screen_shake_start_horizontal(10, 0x81);    /* CONVERT_SECONDS(1.f / 6) */
        ai_particle_set(c, py, px, MZM_PE_SPRITE_EXPLOSION_SMALL);
        uint8_t effect = 0;
        switch (p->type) {
        case MZM_PROJECTILE_BEAM: case MZM_PROJECTILE_CHARGED_BEAM:
            effect = MZM_PE_HITTING_SOMETHING_WITH_NORMAL_BEAM; break;
        case MZM_PROJECTILE_LONG_BEAM: case MZM_PROJECTILE_CHARGED_LONG_BEAM:
            effect = MZM_PE_HITTING_SOMETHING_WITH_LONG_BEAM; break;
        case MZM_PROJECTILE_ICE_BEAM: case MZM_PROJECTILE_CHARGED_ICE_BEAM:
            effect = MZM_PE_HITTING_SOMETHING_WITH_ICE_BEAM; break;
        case MZM_PROJECTILE_WAVE_BEAM: case MZM_PROJECTILE_CHARGED_WAVE_BEAM:
            effect = MZM_PE_HITTING_SOMETHING_WITH_WAVE_BEAM; break;
        case MZM_PROJECTILE_MISSILE: effect = MZM_PE_HITTING_SOMETHING_WITH_MISSILE; break;
        case MZM_PROJECTILE_SUPER_MISSILE: effect = MZM_PE_HITTING_SOMETHING_WITH_SUPER_MISSILE; break;
        default: break;
        }
        if (effect) ai_particle_set(c, py, px, effect);
    }
}

/* The sKraidPal row (BG palette row 10 and the sprite's palette row) a
 * health shows. */
static void health_palette(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t row;
    if (s->health < KRAID_HEALTH / 4) row = 3;
    else if (s->health < KRAID_HEALTH / 3) row = 2;
    else if (s->health < KRAID_HEALTH / 4 * 3) row = 1;
    else return;
    s->absolute_palette_row = row;
    c->f->globals->bg_palette_row10 = (int8_t)(4 + row);
}

/* KraidOpenCloseRoutineAndProjectileCollision */
static void head_and_projectiles(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if (g->sub1.work2) g->sub1.work2--;
    bool closed;
    if (s->anim == MZM_KRAID_MOUTH_CLOSED || s->anim == MZM_KRAID_MOUTH_CLOSED_BLINK) {
        closed = true;
        if (g->sub1.work2) {
            ai_set_anim(s, MZM_KRAID_OPENING_MOUTH);
            mzm_sfx_play(MZM_SOUND_KRAID_OPENING_MOUTH);
        } else if (s->anim == MZM_KRAID_MOUTH_CLOSED) {
            if (ai_animation_ending(c, 1) && c->rng < 5) ai_set_anim(s, MZM_KRAID_MOUTH_CLOSED_BLINK);
        } else if (ai_animation_ending(c, 1)) {
            ai_set_anim(s, MZM_KRAID_MOUTH_CLOSED);
        }
    } else if (s->anim == MZM_KRAID_OPENING_MOUTH) {
        /* Open from three quarters of the table. */
        unsigned frames = c->kind ? c->kind->counts[MZM_KRAID_OPENING_MOUTH] : 0;
        if (s->frame >= (unsigned)(frames * 3 / 4)) {
            closed = false;
            if (ai_animation_ending(c, 1)) ai_set_anim(s, MZM_KRAID_MOUTH_OPENED);
        } else {
            closed = true;
        }
    } else if (s->anim == MZM_KRAID_CLOSING_MOUTH) {
        if (s->frame <= 1) {
            closed = false;
        } else {
            closed = true;
            if (g->sub1.work2) {
                ai_set_anim(s, MZM_KRAID_OPENING_MOUTH);
                mzm_sfx_play(MZM_SOUND_KRAID_OPENING_MOUTH);
            } else if (ai_animation_ending(c, 1)) {
                ai_set_anim(s, c->rng & 1 ? MZM_KRAID_MOUTH_CLOSED : MZM_KRAID_MOUTH_CLOSED_BLINK);
            }
        }
    } else {
        closed = false;
        if (s->anim != MZM_KRAID_RISING && g->sub1.work2 == 0) ai_set_anim(s, MZM_KRAID_CLOSING_MOUTH);
    }
    uint16_t top_offset = closed ? BLOCK_SIZE + PIXEL_SIZE : BLOCK_SIZE + QUARTER_BLOCK_SIZE - PIXEL_SIZE;
    uint16_t bottom_offset = closed ? QUARTER_BLOCK_SIZE + PIXEL_SIZE : QUARTER_BLOCK_SIZE;
    MzmWeapons *weapons = c->f->weapons;
    if (!weapons) return;
    uint16_t y = s->y, x = s->x;
    uint16_t top = (uint16_t)(y + s->hitbox_top), bottom = (uint16_t)(y + s->hitbox_bottom);
    uint16_t left = (uint16_t)(x + s->hitbox_left), right = (uint16_t)(x + s->hitbox_right);
    uint32_t items = weapons->items;
    for (int i = 0; i < MZM_MAX_PROJECTILES; ++i) {
        MzmProjectile *p = &weapons->list[i];
        if (!live_projectile(p)) continue;
        uint16_t py = (uint16_t)p->y, px = (uint16_t)p->x;
        if (!(px > left && px < right && py > top && py < bottom)) continue;
        if (py > (uint16_t)(y - top_offset) && py < (uint16_t)(y + bottom_offset) &&
            p->direction == MZM_AIM_FORWARD && !p->x_flip) {
            /* In the mouth: missiles and charged beams hurt it (the
             * charged plasma beam does as the uncharged beams). */
            uint16_t damage;
            uint8_t effect;
            bool longb = items & MZM_ITEM_LONG_BEAM, ice = items & MZM_ITEM_ICE_BEAM;
            switch (p->type) {
            case MZM_PROJECTILE_MISSILE: damage = MISSILE_DAMAGE; effect = MZM_PE_HITTING_SOMETHING_WITH_MISSILE; break;
            case MZM_PROJECTILE_SUPER_MISSILE:
                damage = SUPER_MISSILE_DAMAGE;
                effect = MZM_PE_HITTING_SOMETHING_WITH_SUPER_MISSILE;
                break;
            case MZM_PROJECTILE_CHARGED_BEAM:
                damage = CHARGED_NORMAL_BEAM_DAMAGE;
                effect = MZM_PE_HITTING_SOMETHING_WITH_NORMAL_BEAM;
                break;
            case MZM_PROJECTILE_CHARGED_LONG_BEAM:
                damage = CHARGED_LONG_BEAM_DAMAGE;
                effect = MZM_PE_HITTING_SOMETHING_WITH_LONG_BEAM;
                break;
            case MZM_PROJECTILE_CHARGED_ICE_BEAM:
                damage = longb ? CHARGED_ICE_LONG_BEAM_DAMAGE : CHARGED_ICE_BEAM_DAMAGE;
                effect = MZM_PE_HITTING_SOMETHING_WITH_ICE_BEAM;
                break;
            case MZM_PROJECTILE_CHARGED_WAVE_BEAM:
                /* CHARGED_WAVE_(ICE_)(LONG_)BEAM_DAMAGE */
                damage = (uint16_t)(longb ? (ice ? 20 : 16) : (ice ? 16 : 12));
                effect = ice ? MZM_PE_HITTING_SOMETHING_WITH_FULL_BEAM_NO_PLASMA
                             : MZM_PE_HITTING_SOMETHING_WITH_WAVE_BEAM;
                break;
            default:
                p->active = false;
                if ((s->isft & 0x7F) < ISFT_SOLID_HIT) set_isft(s, ISFT_SOLID_HIT);
                ai_particle_set(c, py, px, MZM_PE_HITTING_SOMETHING_INVINCIBLE);
                return;
            }
            if (!closed) {
                set_isft(s, ISFT_KRAID_HIT);
                mzm_sfx_play(MZM_SOUND_KRAID_DAMAGED);
            } else {
                /* Shut: it opens for three seconds instead. */
                set_isft(s, 2);
                g->sub1.work2 = 180;
                damage = 0;
            }
            if (s->health > damage) {
                s->health -= damage;
                health_palette(c);
            } else {
                s->health = 0;
                s->freeze_timer = 0;
                s->pose = KRAID_POSE_DYING_INIT;
                s->ignore_samus_timer = 1;
            }
            s->damaged = true;
            g->sub1.health = s->health;
            p->active = false;
            ai_particle_set(c, py, px, effect);
            return;
        }
        /* Elsewhere on its head: missiles bounce off, the rest vanishes. */
        if (p->type == MZM_PROJECTILE_MISSILE || p->type == MZM_PROJECTILE_SUPER_MISSILE)
            mzm_projectile_start_tumbling(p, s->x);
        else
            p->active = false;
        if ((s->isft & 0x7F) < ISFT_SOLID_HIT) set_isft(s, ISFT_SOLID_HIT);
        ai_particle_set(c, py, px, MZM_PE_HITTING_SOMETHING_INVINCIBLE);
        return;
    }
}

/* KraidRandomSpriteDebrisOnCeiling */
static void ceiling_debris(Ctx *c, uint8_t timer) {
    const MzmSpriteGlobals *g = c->f->globals;
    uint16_t y, x;
    if (g->sub1.health != 0) {
        y = (uint16_t)(c->f->samus_y - BLOCK_SIZE * 7);
        x = c->f->samus_x;
    } else {
        y = (uint16_t)((c->world ? c->world->camera_y : 0) - (BLOCK_SIZE - PIXEL_SIZE));
        x = (uint16_t)(g->sub1.x + BLOCK_SIZE * 3);
    }
    uint8_t rng = c->rng, rng2 = c->frame8 & 7;
#define D(type, dy, dx) ai_debris_init(c, type, (uint16_t)(dy), (uint16_t)(dx))
    if (c->frame8 & 1) {
        D(5, y + rng, x - (BLOCK_SIZE * 4 + THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE) + rng * QUARTER_BLOCK_SIZE);
        D(5, y, x - (BLOCK_SIZE * 11 - PIXEL_SIZE) + rng2 * PIXEL_SIZE);
    } else {
        D(7, y - rng, x + (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) - rng * HALF_BLOCK_SIZE);
        D(7, y, x - (BLOCK_SIZE * 6 + QUARTER_BLOCK_SIZE) - rng2 * QUARTER_BLOCK_SIZE);
    }
    if (rng >= 8) {
        D(8, y, x - (BLOCK_SIZE * 3 + THREE_QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE + PIXEL_SIZE / 2) +
                    rng2 * QUARTER_BLOCK_SIZE);
        D(6, y, x + (BLOCK_SIZE * 4 + THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE) - rng2 * QUARTER_BLOCK_SIZE);
    } else {
        D(5, y, x + BLOCK_SIZE * 3 + EIGHTH_BLOCK_SIZE - rng2 * QUARTER_BLOCK_SIZE);
        D(5, y, x - (BLOCK_SIZE * 5 + HALF_BLOCK_SIZE - PIXEL_SIZE / 2) + rng2 * QUARTER_BLOCK_SIZE);
    }
    if ((timer & 3) == 0) {
        if (rng & 1) D(6, y, x - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) + rng2 * QUARTER_BLOCK_SIZE);
        else D(8, y, x + (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) - rng2 * QUARTER_BLOCK_SIZE);
        if (rng >= 8) {
            D(7, y, x - rng2 * HALF_BLOCK_SIZE);
            D(8, y, x + BLOCK_SIZE - rng2 * QUARTER_BLOCK_SIZE);
        } else {
            D(5, y, x - (BLOCK_SIZE * 4 + QUARTER_BLOCK_SIZE - PIXEL_SIZE / 2) + rng2 * QUARTER_BLOCK_SIZE);
            D(6, y, x + rng2 * PIXEL_SIZE);
        }
    }
#undef D
}

/* KraidMoveBg2ToRight / KraidMoveBg2ToLeft */
static void move_bg2(Ctx *c, int movement) {
    MzmSpriteGlobals *g = c->f->globals;
    g->sub1.x = (uint16_t)(g->sub1.x + movement);
    g->bg2_offset_x = (int16_t)(g->bg2_offset_x - movement);
}

/* KraidMoveUp */
static bool move_up(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    ceiling_debris(c, s->work1);
    if ((g->sub1.y & 15) == 0) mzm_screen_shake_start_vertical(10, 0x81);
    if (g->sub1.y > s->y_spawn) {
        g->sub1.y--;
        g->bg2_offset_y++;
        return false;
    }
    return true;
}

/* A block a foot breaks, with its explosion and debris. */
static void break_block(Ctx *c, uint16_t y, uint16_t x, uint8_t effect) {
    remove_solid(c, y, x);
    ai_particle_set(c, y, x, effect);
    ai_debris_init(c, 0x11, (uint16_t)(y - QUARTER_BLOCK_SIZE), x);
    ai_debris_init(c, 0x12, y, (uint16_t)(x + EIGHTH_BLOCK_SIZE + PIXEL_SIZE));
    ai_debris_init(c, 0x13, (uint16_t)(y - (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE + PIXEL_SIZE / 2)),
                   (uint16_t)(x + QUARTER_BLOCK_SIZE + PIXEL_SIZE));
    ai_debris_init(c, 0x4, (uint16_t)(y - (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)),
                   (uint16_t)(x - (HALF_BLOCK_SIZE - PIXEL_SIZE / 2)));
}

/* KraidMoveFeet: 1 past its left limit, 2 past its right one. */
static uint8_t move_feet(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
        uint16_t offset = g->sub1.health >= KRAID_HEALTH / 3 || c->f->difficulty == 0 ? BLOCK_SIZE : BLOCK_SIZE * 3;
        if ((uint16_t)(s->x_spawn + offset) < g->sub1.x) return 2;
        if (g->sub1.frame < 5) {
            move_bg2(c, 1);
            uint16_t y = BLOCK_SIZE * 39 + HALF_BLOCK_SIZE, x = (uint16_t)(g->sub1.x + BLOCK_SIZE * 5);
            if (ai_collision_at(c, y, x) != MZM_COLLISION_AIR) {
                break_block(c, y, x, MZM_PE_SPRITE_EXPLOSION_HUGE);
                mzm_sfx_play(MZM_SOUND_138);
                y = (uint16_t)(y - BLOCK_SIZE * 8);
                x = (uint16_t)(x - BLOCK_SIZE);
                if (ai_collision_at(c, y, x) != MZM_COLLISION_AIR) break_block(c, y, x, MZM_PE_SPRITE_EXPLOSION_MEDIUM);
            }
        }
    } else {
        uint16_t offset = g->sub1.health < KRAID_HEALTH / 3 ? BLOCK_SIZE : 0;
        if ((uint16_t)(s->x_spawn + offset) > g->sub1.x) return 1;
        if (g->sub1.frame < 5) move_bg2(c, -1);
    }
    return 0;
}

/* KraidFirstStep / KraidSecondStep: the next pose when the step ends. */
static void step(Ctx *c, uint8_t other_step, uint8_t standing) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    uint8_t feet = move_feet(c);
    if (!sub1_nearly_ended(c)) return;
    if (g->sub1.work1 != 0) {
        if (g->sub1.work1 <= 3) {               /* CONVERT_SECONDS(.05f) */
            g->sub1.work1++;
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
            s->pose = other_step;
        } else {
            g->sub1.work1 = 0;
            s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
            s->pose = standing;
        }
        return;
    }
    s->pose = g->sub1.health < KRAID_HEALTH / 4 ? other_step : standing;
    if (feet) {
        if (g->sub1.health < KRAID_HEALTH / 3 && feet == 2) s->pose = standing;
        else s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
    }
}

/* KraidPreventSamusGoingThrough */
static void keep_samus_right(Ctx *c) {
    MzmSpriteSamusLink *link = c->f->samus_link;
    uint16_t x = (uint16_t)(c->f->globals->sub1.x + BLOCK_SIZE * 3);
    if (link && link->x < x) {
        link->x = x;
        link->moved = true;
    }
}

/* KraidInit */
static void kraid_init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->door_unlock_timer) h->door_unlock_timer(h->context, 1);    /* LOCK_DOORS */
    s->y = (uint16_t)(s->y - (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE));
    s->x = (uint16_t)(s->x - HALF_BLOCK_SIZE);
    g->sub1.y = s->y;
    g->sub1.x = s->x;
    s->y_spawn = s->y;
    s->x_spawn = s->x;
    s->draw_top = BLOCK_SIZE * 3 / PIXEL_SIZE;
    s->draw_bottom = (BLOCK_SIZE + HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE) / PIXEL_SIZE;
    s->draw_horizontal = (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    s->hitbox_top = -(BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE);
    s->hitbox_bottom = BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE;
    s->hitbox_left = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    s->hitbox_right = BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE;
    s->work0 = 120;                 /* CONVERT_SECONDS(2.f) */
    s->work1 = 0;
    s->samus_collision = SSC_HURTS_KNOCKBACK_IF_INVINCIBLE;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES | MZM_SPRITE_STATUS_FACING_RIGHT | MZM_SPRITE_STATUS_MOSAIC;
    s->health = c->tables->health[MZM_PSPRITE_KRAID];
    g->sub1.health = s->health;
    g->sub1.work3 = g->sub1.work2 = g->sub1.work1 = 0;
    set_multi(c, MZM_KRAID_MULTI_RISING);
    ai_set_anim(s, MZM_KRAID_RISING);
    s->pose = KRAID_POSE_GO_UP;
    s->room_slot = PART_KRAID;
    static const uint8_t parts[] = {PART_BELLY, PART_LEFT_ARM, PART_TOP_HOLE_LEFT, PART_TOP_HOLE_RIGHT,
                                    PART_MIDDLE_HOLE_LEFT, PART_MIDDLE_HOLE_RIGHT, PART_BOTTOM_HOLE_LEFT,
                                    PART_BOTTOM_HOLE_RIGHT, PART_LEFT_FEET, PART_RIGHT_ARM, PART_RIGHT_FEET};
    for (unsigned i = 0; i < sizeof parts; ++i) {
        int n = ai_spawn_secondary(c, MZM_SSPRITE_KRAID_PART, parts[i], s->gfx_slot, s->y, s->x, 0);
        /* The left holes keep their own slot for their spikes. */
        if (n >= 0 && (parts[i] == PART_TOP_HOLE_LEFT || parts[i] == PART_MIDDLE_HOLE_LEFT ||
                       parts[i] == PART_BOTTOM_HOLE_LEFT))
            c->f->sprites[n].work1 = (uint8_t)n;
    }
    g->sub1.y = (uint16_t)(g->sub1.y + BLOCK_SIZE * 2);
    g->bg2_offset_y = (int16_t)(g->bg2_offset_y - BLOCK_SIZE * 2);
}

/* KraidDyingInit */
static void dying_init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    uint8_t m = g->sub1.multi;
    set_multi(c, m == MZM_KRAID_MULTI_STANDING || m == MZM_KRAID_MULTI_MOVING_RIGHT_FEET_TO_RIGHT ||
                         m == MZM_KRAID_MULTI_MOVING_LEFT_FEET_TO_LEFT
                     ? MZM_KRAID_MULTI_DYING1 : MZM_KRAID_MULTI_DYING2);
    if (s->anim != MZM_KRAID_OPENING_MOUTH && s->anim != MZM_KRAID_MOUTH_OPENED)
        ai_set_anim(s, MZM_KRAID_OPENING_MOUTH);
    s->pose = KRAID_POSE_DYING;
    s->work0 = 100;                 /* CONVERT_SECONDS(1.f) + TWO_THIRD_SECOND */
    s->work1 = 0;
    s->work2 = 200;                 /* CONVERT_SECONDS(3.f) + ONE_THIRD_SECOND */
    s->health = 1;
    s->isft = EIGHTH_BLOCK_SIZE;
    s->draw_order = 12;
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->set_event) h->set_event(h->context, MZM_EVENT_KRAID_KILLED);
    mzm_sfx_play(MZM_SOUND_KRAID_DYING_1);
}

/* KraidDying: it sinks, shaking, exploding and throwing dust. */
static void dying(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if (s->anim == MZM_KRAID_OPENING_MOUTH && ai_animation_ending(c, 1)) ai_set_anim(s, MZM_KRAID_MOUTH_OPENED);
    ceiling_debris(c, s->work1);
    keep_samus_right(c);
    s->ignore_samus_timer = 1;
    s->work1++;
    if ((s->work1 & 31) == 0) {
        ai_particle_set(c, BLOCK_SIZE * 40, (uint16_t)(g->sub1.x - BLOCK_SIZE * 3 + s->work1 * PIXEL_SIZE / 2),
                        MZM_PE_SECOND_MEDIUM_DUST);
        ai_particle_set(c, BLOCK_SIZE * 40, (uint16_t)(g->sub1.x + BLOCK_SIZE * 3 - s->work1 * PIXEL_SIZE / 2),
                        MZM_PE_SECOND_TWO_MEDIUM_DUST);
    }
    if (s->work0 != 0) {
        s->work0--;
        return;
    }
    if (s->work2 != 0) {
        /* (the statue opening cutscene request at 1 is not ported) */
        if (--s->work2 == 0) mzm_sfx_play(MZM_SOUND_KRAID_DYING_3);
    }
    if ((s->work1 & 15) == 0) {
        mzm_screen_shake_start_vertical(20, 0x81);     /* ONE_THIRD_SECOND */
        ai_particle_set(c, (uint16_t)(s->y - (BLOCK_SIZE + HALF_BLOCK_SIZE + PIXEL_SIZE) + c->rng * QUARTER_BLOCK_SIZE),
                        (uint16_t)(s->x - (BLOCK_SIZE * 3 + EIGHTH_BLOCK_SIZE) + s->work1), MZM_PE_SPRITE_EXPLOSION_HUGE);
    }
    if (((s->work1 - 8) & 15) == 0)
        ai_particle_set(c, (uint16_t)(s->y + BLOCK_SIZE * 2 + QUARTER_BLOCK_SIZE - PIXEL_SIZE - c->rng * QUARTER_BLOCK_SIZE),
                        (uint16_t)(s->x + BLOCK_SIZE * 3 + THREE_QUARTER_BLOCK_SIZE - s->work1),
                        MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
    if (ai_collision_at(c, g->sub1.y, g->sub1.x) != MZM_COLLISION_AIR) {
        s->pose = KRAID_POSE_DEAD_STATIONARY;
        s->work0 = 120;             /* CONVERT_SECONDS(2.f) */
        mzm_sfx_play(MZM_SOUND_KRAID_DYING_2);
    } else {
        g->sub1.y++;
        g->bg2_offset_y--;
    }
}

/* KraidBeforeDeath */
static void before_death(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    ceiling_debris(c, s->work1);
    keep_samus_right(c);
    s->ignore_samus_timer = 1;
    uint8_t rng = c->rng;
    uint8_t timer = ++s->work1;
    if ((timer & 7) == 0) {
        if (timer & 8) {
            mzm_screen_shake_start_vertical(20, 0x81);
            ai_particle_set(c, (uint16_t)(s->y - (BLOCK_SIZE + HALF_BLOCK_SIZE + PIXEL_SIZE) + rng * QUARTER_BLOCK_SIZE),
                            (uint16_t)(s->x - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) + timer * PIXEL_SIZE / 2),
                            MZM_PE_SPRITE_EXPLOSION_HUGE);
            ai_particle_set(c, (uint16_t)(s->y + (BLOCK_SIZE * 2 + QUARTER_BLOCK_SIZE - PIXEL_SIZE) - rng * QUARTER_BLOCK_SIZE),
                            (uint16_t)(s->x + BLOCK_SIZE * 3 - timer * PIXEL_SIZE / 2),
                            MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        } else {
            ai_particle_set(c, (uint16_t)(s->y + rng * QUARTER_BLOCK_SIZE),
                            (uint16_t)(s->x + (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) - timer * PIXEL_SIZE / 2),
                            MZM_PE_SPRITE_EXPLOSION_HUGE);
            ai_particle_set(c, (uint16_t)(s->y - rng * EIGHTH_BLOCK_SIZE),
                            (uint16_t)(s->x - (BLOCK_SIZE + HALF_BLOCK_SIZE + PIXEL_SIZE) + timer * PIXEL_SIZE / 2),
                            MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        }
    }
    if ((timer & 15) == 0) {
        uint16_t w = (uint16_t)(s->work0 * PIXEL_SIZE / 2);
        if (timer & 16) {
            ai_particle_set(c, BLOCK_SIZE * 40,
                            (uint16_t)(g->sub1.x - (BLOCK_SIZE * 4 + QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE) + w),
                            MZM_PE_SPRITE_EXPLOSION_MEDIUM);
            ai_particle_set(c, BLOCK_SIZE * 40,
                            (uint16_t)(g->sub1.x + (BLOCK_SIZE * 4 + QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE) - w),
                            MZM_PE_SPRITE_EXPLOSION_BIG);
        } else {
            ai_particle_set(c, BLOCK_SIZE * 40,
                            (uint16_t)(g->sub1.x - (BLOCK_SIZE * 3 + QUARTER_BLOCK_SIZE + PIXEL_SIZE / 2) + w),
                            MZM_PE_SPRITE_EXPLOSION_BIG);
            ai_particle_set(c, BLOCK_SIZE * 40,
                            (uint16_t)(g->sub1.x + (BLOCK_SIZE * 3 + QUARTER_BLOCK_SIZE + PIXEL_SIZE / 2) - w),
                            MZM_PE_SPRITE_EXPLOSION_MEDIUM);
        }
    }
    if (--s->work0 == 0) {
        ai_sprite_death_full(c, DEATH_NORMAL, (uint16_t)(g->sub1.y - BLOCK_SIZE * 3), g->sub1.x, false,
                             MZM_PE_MAIN_BOSS_DEATH);
        mzm_sfx_music_fade_and_queue(20, MZM_MUSIC_BOSS_KILLED);   /* ONE_THIRD_SECOND */
        g->bg2_hidden = true;
        g->igt_at_bosses[0] = g->igt;
    }
}

/* The BG2 body's shifts of each multi-sprite frame, and the stomps. */
static void body_movement(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if (g->sub1.adc != 1) return;
    uint8_t m = g->sub1.multi, f = g->sub1.frame;
    if (m == MZM_KRAID_MULTI_RISING || m == MZM_KRAID_MULTI_STANDING ||
        m == MZM_KRAID_MULTI_STANDING_BETWEEN_STEPS) {
        if (f == 1) g->bg2_offset_y -= 4;
        else if (f == 2) g->bg2_offset_y += 4;
        return;
    }
    int dx;
    if (m == MZM_KRAID_MULTI_MOVING_LEFT_FEET_TO_RIGHT || m == MZM_KRAID_MULTI_MOVING_RIGHT_FEET_TO_LEFT) dx = -4;
    else if (m == MZM_KRAID_MULTI_MOVING_RIGHT_FEET_TO_RIGHT || m == MZM_KRAID_MULTI_MOVING_LEFT_FEET_TO_LEFT) dx = 4;
    else return;
    if (f >= 1 && f <= 3) {
        g->bg2_offset_x = (int16_t)(g->bg2_offset_x + dx);
    } else if (f == 4) {
        g->bg2_offset_y -= 4;
    } else if (f == 5) {
        g->bg2_offset_y += 4;
        mzm_screen_shake_start_vertical(10, 0x81);
        mzm_sfx_play(MZM_SOUND_KRAID_STOMPING_FEET);
        uint16_t y = (uint16_t)(s->y_spawn + BLOCK_SIZE * 7 + HALF_BLOCK_SIZE), x = g->sub1.x;
        uint16_t a, b;
        switch (m) {
        case MZM_KRAID_MULTI_MOVING_LEFT_FEET_TO_RIGHT:
            a = (uint16_t)(x - (HALF_BLOCK_SIZE + 12));
            b = (uint16_t)(x - (BLOCK_SIZE * 2 + QUARTER_BLOCK_SIZE));
            break;
        case MZM_KRAID_MULTI_MOVING_RIGHT_FEET_TO_LEFT:
            a = (uint16_t)(x + (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE + 12));
            b = (uint16_t)(x + (BLOCK_SIZE * 5 + QUARTER_BLOCK_SIZE));
            break;
        case MZM_KRAID_MULTI_MOVING_LEFT_FEET_TO_LEFT:
            a = (uint16_t)(x - (BLOCK_SIZE + HALF_BLOCK_SIZE + 4));
            b = (uint16_t)(x - (BLOCK_SIZE * 3 + 8));
            break;
        default:
            a = (uint16_t)(x + (BLOCK_SIZE * 4 + 10));
            b = (uint16_t)(x + (BLOCK_SIZE * 5 + HALF_BLOCK_SIZE + 14));
            break;
        }
        ai_particle_set(c, y, a, MZM_PE_SECOND_MEDIUM_DUST);
        ai_particle_set(c, y, b, MZM_PE_SECOND_MEDIUM_DUST);
    }
}

/* Kraid */
static void kraid(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if (s->pose != SPRITE_POSE_UNINITIALIZED && g->sub1.health != 0) head_and_projectiles(c);
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: kraid_init(c); break;
    case KRAID_POSE_GO_UP:
        /* KraidGoUp */
        if (s->work1 == 0) mzm_sfx_play(MZM_SOUND_KRAID_RISING);
        s->work1++;
        move_up(c);
        if (--s->work0 == 0) {
            s->pose = KRAID_POSE_CHECK_FULLY_UP;
            ai_set_anim(s, MZM_KRAID_CLOSING_MOUTH);
        }
        break;
    case KRAID_POSE_CHECK_FULLY_UP:
        s->work1++;
        if (move_up(c)) s->pose = KRAID_POSE_STANDING_INIT;
        break;
    case KRAID_POSE_FIRST_STEP_INIT:
        set_multi(c, s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? MZM_KRAID_MULTI_MOVING_LEFT_FEET_TO_RIGHT
                                                                 : MZM_KRAID_MULTI_MOVING_RIGHT_FEET_TO_LEFT);
        s->pose = KRAID_POSE_FIRST_STEP;
        /* fallthrough */
    case KRAID_POSE_FIRST_STEP:
        step(c, KRAID_POSE_SECOND_STEP_INIT, KRAID_POSE_STANDING_BETWEEN_STEPS_INIT);
        break;
    case KRAID_POSE_SECOND_STEP_INIT:
        set_multi(c, s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? MZM_KRAID_MULTI_MOVING_RIGHT_FEET_TO_RIGHT
                                                                 : MZM_KRAID_MULTI_MOVING_LEFT_FEET_TO_LEFT);
        s->pose = KRAID_POSE_SECOND_STEP;
        /* fallthrough */
    case KRAID_POSE_SECOND_STEP:
        step(c, KRAID_POSE_FIRST_STEP_INIT, KRAID_POSE_STANDING_INIT);
        break;
    case KRAID_POSE_STANDING_INIT:
        set_multi(c, MZM_KRAID_MULTI_STANDING);
        s->pose = KRAID_POSE_STANDING;
        /* fallthrough */
    case KRAID_POSE_STANDING:
        if (sub1_nearly_ended(c)) {
            s->pose = KRAID_POSE_FIRST_STEP_INIT;
            if (g->sub1.work1 == 1) {
                g->sub1.work1++;
                s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
            }
        }
        break;
    case KRAID_POSE_STANDING_BETWEEN_STEPS_INIT:
        set_multi(c, MZM_KRAID_MULTI_STANDING_BETWEEN_STEPS);
        s->pose = KRAID_POSE_STANDING_BETWEEN_STEPS;
        /* fallthrough */
    case KRAID_POSE_STANDING_BETWEEN_STEPS:
        if (sub1_nearly_ended(c)) {
            s->pose = KRAID_POSE_SECOND_STEP_INIT;
            if (g->sub1.work1 == 1) {
                g->sub1.work1++;
                s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
            }
        }
        break;
    case KRAID_POSE_DYING_INIT:
        dying_init(c);
        /* fallthrough */
    case KRAID_POSE_DYING: dying(c); break;
    case KRAID_POSE_DEAD_STATIONARY: before_death(c); break;
    default: break;
    }
    if ((s->status & MZM_SPRITE_STATUS_EXISTS) && !s->secondary && s->sprite_id == MZM_PSPRITE_KRAID) {
        update_sub1_anim(c);
        sync_position(c);
        body_movement(c);
        if (g->sub1.work1 != 0) g->sub1.adc = (uint8_t)(g->sub1.adc + 4);    /* CONVERT_SECONDS(1.f / 15) */
        /* gLockScreen: the camera five blocks left of Samus. */
        g->lock_screen = true;
        g->lock_y = c->f->samus_y;
        g->lock_x = (uint16_t)(c->f->samus_x - BLOCK_SIZE * 5);
        if (s->palette_row == 14 - s->frozen_row_offset) s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        else s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
    } else {
        g->lock_screen = false;
    }
}

/* The arms' hitboxes by animation frame (top, bottom, left, right), set
 * when a frame starts. */
static const int16_t right_arm_idle[10][4] = {
    {-96, 32, 224, 264}, {-72, 32, 224, 348}, {-48, 32, 224, 348}, {-24, 64, 224, 348},
    {-24, 64, 224, 348}, {-24, 64, 256, 348}, {-40, 32, 224, 348}, {-64, 32, 224, 348},
    {-96, 32, 224, 272}, {-96, 32, 224, 288},
};
static const int16_t right_arm_attacking[12][4] = {
    {-96, 0, 208, 288}, {-128, -64, 192, 288}, {-192, -64, 152, 200}, {-256, -96, 128, 184},
    {-320, -160, 112, 160}, {-320, -192, 256, 296}, {-320, 0, 288, 440}, {8, 64, 256, 416},
    {96, 128, 224, 384}, {104, 160, 224, 320}, {32, 160, 224, 288}, {0, 128, 232, 280},
};
static const int16_t left_arm_idle[10][4] = {
    {-160, -32, 8, 136}, {-160, -64, 8, 144}, {-164, -32, 8, 144}, {-168, -32, 16, 144},
    {-168, -32, 16, 160}, {-168, -32, 16, 144}, {-168, -40, 16, 144}, {-152, -64, 4, 144},
    {-160, -32, 4, 144}, {-160, -32, 0, 144},
};
static const int16_t left_arm_dying[10][4] = {
    {-160, -64, 8, 128}, {-192, -96, -32, 96}, {-224, -96, -64, 80}, {-352, -224, -64, 48},
    {-480, -384, -64, 64}, {-448, -320, 96, 224}, {-288, -192, 192, 288}, {-224, -96, 64, 192},
    {-160, -64, 4, 128}, {-160, -32, -16, 112},
};
static const int16_t left_arm_attacking[20][4] = {
    {-160, -32, 0, 128}, {-152, -56, -32, 96}, {-176, -48, -96, 32}, {-224, -96, -128, -32},
    {-256, -128, -128, -32}, {-256, -160, -128, -32}, {-384, -256, -128, -8}, {-352, -264, 8, 128},
    {-256, -192, 64, 160}, {-160, -48, 96, 160}, {-96, -4, 16, 96}, {-160, -64, 4, 96},
    {-128, -64, 4, 96}, {-160, -64, 16, 112}, {-160, -64, 16, 128}, {-160, -64, 16, 144},
    {-160, -64, 16, 160}, {-160, -64, 16, 152}, {-152, -64, 4, 128}, {-160, -32, 4, 144},
};

/* KraidPartUpdate*Hitbox, with their sounds. */
static void arm_hitbox(Ctx *c, const int16_t (*table)[4], unsigned count) {
    MzmSprite *s = c->s;
    if (s->adc != 1 || s->frame >= count) return;
    const int16_t *h = table[s->frame];
    s->hitbox_top = h[0];
    s->hitbox_bottom = h[1];
    s->hitbox_left = h[2];
    s->hitbox_right = h[3];
    if (table == right_arm_attacking) {
        if (s->frame == 0) mzm_sfx_play(MZM_SOUND_KRAID_RIGHT_ARM_DYING_1);
        else if (s->frame == 4) mzm_sfx_play(MZM_SOUND_KRAID_RIGHT_ARM_DYING_2);
    } else if (table == left_arm_dying) {
        if (s->frame == 0) mzm_sfx_play(MZM_SOUND_KRAID_LEFT_ARM_DYING_1);
        else if (s->frame == 4) mzm_sfx_play(MZM_SOUND_KRAID_LEFT_ARM_DYING_2);
    } else if (table == left_arm_attacking) {
        if (s->frame == 0) mzm_sfx_play(MZM_SOUND_KRAID_LEFT_ARM_ATTACKING_1);
        else if (s->frame == 6) mzm_sfx_play(MZM_SOUND_KRAID_LEFT_ARM_ATTACKING_2);
        else if (s->frame == 15) mzm_sfx_play(MZM_SOUND_KRAID_LEFT_ARM_ATTACKING_3);
    }
}

/* drawDistanceTop / Bottom / Horizontal (pixels, as KraidPartInit sets
 * them: some are written in subpixel units). */
static void draw(MzmSprite *s, int top, int bottom, int horizontal) {
    s->draw_top = (uint8_t)top;
    s->draw_bottom = (uint8_t)bottom;
    s->draw_horizontal = (uint8_t)horizontal;
}

static void set_box(MzmSprite *s, int16_t top, int16_t bottom, int16_t left, int16_t right) {
    s->hitbox_top = top;
    s->hitbox_bottom = bottom;
    s->hitbox_left = left;
    s->hitbox_right = right;
}

/* KraidPartInit */
static void part_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->health = 1000;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->pose = PART_POSE_IDLE;
    switch (s->room_slot) {
    case PART_BELLY:
        s->status |= MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->samus_collision = SSC_HURTS_KNOCKBACK_IF_INVINCIBLE;
        draw(s, BLOCK_SIZE * 3 / PIXEL_SIZE, BLOCK_SIZE * 8 / PIXEL_SIZE, BLOCK_SIZE * 5 / PIXEL_SIZE);
        set_box(s, -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE), BLOCK_SIZE * 7 + HALF_BLOCK_SIZE,
                -(BLOCK_SIZE * 4 + HALF_BLOCK_SIZE), BLOCK_SIZE * 3 + QUARTER_BLOCK_SIZE);
        s->draw_order = 1;
        s->pose = PART_POSE_CHECK_PROJECTILES;
        break;
    case PART_TOP_HOLE_LEFT:
        s->samus_collision = SSC_HURTS_SAMUS;
        s->frozen_row_offset = 4;
        draw(s, BLOCK_SIZE / PIXEL_SIZE, BLOCK_SIZE / PIXEL_SIZE, HALF_BLOCK_SIZE / PIXEL_SIZE);
        set_box(s, -HALF_BLOCK_SIZE, BLOCK_SIZE, -HALF_BLOCK_SIZE, HALF_BLOCK_SIZE);
        s->draw_order = 11;
        s->pose = PART_POSE_CHECK_SPAWN_SPIKES;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        break;
    case PART_MIDDLE_HOLE_LEFT:
        s->samus_collision = SSC_HURTS_SAMUS;
        s->frozen_row_offset = 4;
        draw(s, (BLOCK_SIZE + QUARTER_BLOCK_SIZE) / PIXEL_SIZE, BLOCK_SIZE / PIXEL_SIZE, HALF_BLOCK_SIZE / PIXEL_SIZE);
        set_box(s, -(BLOCK_SIZE + QUARTER_BLOCK_SIZE), BLOCK_SIZE + QUARTER_BLOCK_SIZE, -HALF_BLOCK_SIZE,
                THREE_QUARTER_BLOCK_SIZE);
        s->draw_order = 11;
        s->pose = PART_POSE_CHECK_SPAWN_SPIKES;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        break;
    case PART_BOTTOM_HOLE_LEFT:
        s->samus_collision = SSC_HURTS_SAMUS;
        s->frozen_row_offset = 4;
        draw(s, (BLOCK_SIZE + QUARTER_BLOCK_SIZE) / PIXEL_SIZE, BLOCK_SIZE / PIXEL_SIZE, BLOCK_SIZE / PIXEL_SIZE);
        set_box(s, -(BLOCK_SIZE + QUARTER_BLOCK_SIZE), BLOCK_SIZE * 3 + QUARTER_BLOCK_SIZE, -HALF_BLOCK_SIZE,
                HALF_BLOCK_SIZE);
        s->draw_order = 11;
        s->pose = PART_POSE_CHECK_SPAWN_SPIKES;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        break;
    case PART_TOP_HOLE_RIGHT:
    case PART_MIDDLE_HOLE_RIGHT:
    case PART_BOTTOM_HOLE_RIGHT:
        draw(s, (s->room_slot == PART_TOP_HOLE_RIGHT ? BLOCK_SIZE
                 : s->room_slot == PART_MIDDLE_HOLE_RIGHT ? BLOCK_SIZE + QUARTER_BLOCK_SIZE : HALF_BLOCK_SIZE) / PIXEL_SIZE,
             BLOCK_SIZE / PIXEL_SIZE, BLOCK_SIZE / PIXEL_SIZE);
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->samus_collision = SSC_NONE;
        s->frozen_row_offset = 4;
        set_box(s, -PIXEL_SIZE, PIXEL_SIZE, -PIXEL_SIZE, PIXEL_SIZE);
        s->draw_order = 13;
        break;
    case PART_LEFT_ARM:
        s->immune_to_projectiles = true;
        s->samus_collision = SSC_HURTS_SAMUS;
        draw(s, BLOCK_SIZE * 2, HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE, BLOCK_SIZE + HALF_BLOCK_SIZE);
        s->draw_order = 2;
        ai_set_anim(s, MZM_KRAID_LEFT_ARM_IDLE);
        s->pose = PART_POSE_CHECK_THROW_NAILS;
        break;
    case PART_RIGHT_ARM:
        s->immune_to_projectiles = true;
        s->samus_collision = SSC_HURTS_SAMUS;
        draw(s, BLOCK_SIZE * 6 / PIXEL_SIZE, (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE) / PIXEL_SIZE,
             (BLOCK_SIZE * 7 + HALF_BLOCK_SIZE) / PIXEL_SIZE);
        s->draw_order = 15;
        s->low_bg_priority = true;
        ai_set_anim(s, MZM_KRAID_RIGHT_ARM_IDLE);
        s->pose = PART_POSE_CHECK_ATTACK;
        s->work0 = 0;
        break;
    case PART_LEFT_FEET:
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->samus_collision = SSC_NONE;
        draw(s, (BLOCK_SIZE * 3 + QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE) / PIXEL_SIZE,
             (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE, (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE) / PIXEL_SIZE);
        set_box(s, -PIXEL_SIZE, PIXEL_SIZE, -PIXEL_SIZE, PIXEL_SIZE);
        s->draw_order = 3;
        break;
    case PART_RIGHT_FEET:
        s->samus_collision = SSC_HURTS_SAMUS;
        draw(s, (BLOCK_SIZE * 3 + QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE) / PIXEL_SIZE,
             HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE, BLOCK_SIZE * 7 / PIXEL_SIZE);
        set_box(s, BLOCK_SIZE * 2, BLOCK_SIZE * 2 + HALF_BLOCK_SIZE, BLOCK_SIZE * 3,
                BLOCK_SIZE * 5 + THREE_QUARTER_BLOCK_SIZE);
        s->draw_order = 14;
        s->low_bg_priority = true;
        s->pose = PART_POSE_CHECK_PROJECTILES;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        break;
    default:
        s->status = 0;
        break;
    }
}

/* SpriteUtilCountDrops */
static int count_drops(const Ctx *c) {
    int count = 0;
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i)
        if (c->f->sprites[i].status & MZM_SPRITE_STATUS_EXISTS &&
            c->f->sprites[i].samus_collision >= SSC_SMALL_ENERGY_DROP)
            count++;
    return count;
}

/* KraidPartThrowNails */
static void throw_nails(Ctx *c, const MzmSprite *main) {
    MzmSprite *s = c->s;
    if (main->pose < KRAID_POSE_FIRST_STEP_INIT) return;
    if (s->anim == MZM_KRAID_LEFT_ARM_IDLE) {
        if ((c->frame8 & 127) == 0 && count_drops(c) < (c->f->difficulty == 0 ? 1 : 3))
            ai_set_anim(s, MZM_KRAID_LEFT_ARM_THROWING_NAILS);
        return;
    }
    if (s->frame == 7 && s->adc == 4)
        ai_spawn_secondary(c, MZM_SSPRITE_KRAID_NAIL, NAIL_SLOW_ROTATION, s->gfx_slot,
                           (uint16_t)(s->y - (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE)), (uint16_t)(s->x + BLOCK_SIZE * 2), 0);
    else if (s->frame == 8 && s->adc == 1)
        ai_spawn_secondary(c, MZM_SSPRITE_KRAID_NAIL, 1, s->gfx_slot, (uint16_t)(s->y - BLOCK_SIZE * 3),
                           (uint16_t)(s->x + BLOCK_SIZE * 3 + HALF_BLOCK_SIZE), 0);
    if (ai_animation_ending(c, 1)) ai_set_anim(s, MZM_KRAID_LEFT_ARM_IDLE);
}

/* KraidPartCheckAttack: the swipe when Samus is near on the right once
 * Kraid lost a quarter of its health. */
static void check_attack(Ctx *c, MzmSprite *main) {
    MzmSprite *s = c->s;
    if (main->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
        if (ai_animation_ending(c, 1)) {
            main->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            ai_set_anim(s, MZM_KRAID_RIGHT_ARM_IDLE);
        }
        return;
    }
    uint8_t nslr = NSLR_OUT_OF_RANGE;
    if (s->work0 != 0) {
        s->work0--;
    } else {
        nslr = ai_samus_nslr(c, BLOCK_SIZE * 2, BLOCK_SIZE * 8 + HALF_BLOCK_SIZE);
        s->work0 = 180;             /* CONVERT_SECONDS(3.f) */
    }
    if (c->f->globals->sub1.health < KRAID_HEALTH / 4 * 3 && nslr == NSLR_RIGHT) {
        ai_set_anim(s, MZM_KRAID_RIGHT_ARM_ATTACKING);
        main->status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
    }
}

/* KraidPartCheckShouldSpawnSpikes: the bottom hole first, then the middle
 * one a second later, the top one two seconds later at low health; the
 * main sprite's MOSAIC flag is clear while the bottom hole shoots. */
static void check_spikes(Ctx *c, MzmSprite *main) {
    MzmSprite *s = c->s;
    const MzmSpriteGlobals *g = c->f->globals;
    if (main->pose < KRAID_POSE_FIRST_STEP_INIT) return;
    belly_projectiles(c);
    bool low = g->sub1.health < KRAID_HEALTH / 3;
    if (s->room_slot == PART_BOTTOM_HOLE_LEFT) {
        if (g->sub1.health >= KRAID_HEALTH / 4 * 3 && c->f->samus_y < (uint16_t)(s->y - BLOCK_SIZE * 2)) return;
        for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
            const MzmSprite *o = &c->f->sprites[i];
            if (o->status & MZM_SPRITE_STATUS_EXISTS && o->secondary && o->sprite_id == MZM_SSPRITE_KRAID_SPIKE)
                return;
        }
        main->status &= (uint16_t)~MZM_SPRITE_STATUS_MOSAIC;
        s->y_spawn = 1;
    } else if (s->room_slot == PART_MIDDLE_HOLE_LEFT) {
        if (main->status & MZM_SPRITE_STATUS_MOSAIC) return;
        s->y_spawn = 60;
    } else {
        if (main->status & MZM_SPRITE_STATUS_MOSAIC || !low) return;
        s->y_spawn = 120;
    }
    s->pose = PART_POSE_SPAWN_SPIKES;
}

/* KraidPartSpawnSpike */
static void spawn_spike(Ctx *c, MzmSprite *main) {
    MzmSprite *s = c->s;
    belly_projectiles(c);
    if (--s->y_spawn != 0) return;
    if (s->room_slot == PART_BOTTOM_HOLE_LEFT) main->status |= MZM_SPRITE_STATUS_MOSAIC;
    int n = ai_spawn_secondary(c, MZM_SSPRITE_KRAID_SPIKE, s->room_slot, s->gfx_slot, s->y, s->x, 0);
    if (n >= 0) {
        /* The spike follows its hole (its primarySpriteRamSlot). */
        c->f->sprites[n].ram_slot = s->work1;
        c->f->sprites[n].x_spawn = main->x_spawn;
    }
    s->pose = PART_POSE_CHECK_SPAWN_SPIKES;
}

/* KraidPartDyingInit */
static void part_dying_init(MzmSprite *s) {
    switch (s->room_slot) {
    case PART_LEFT_ARM:
        ai_set_anim(s, MZM_KRAID_LEFT_ARM_DYING);
        s->y_spawn = 400;           /* CONVERT_SECONDS(6.f) + TWO_THIRD_SECOND */
        s->pose = PART_POSE_ARM_DYING;
        s->draw_order = 10;
        break;
    case PART_RIGHT_ARM:
        ai_set_anim(s, MZM_KRAID_RIGHT_ARM_DYING);
        s->y_spawn = 400;
        s->pose = PART_POSE_ARM_DYING;
        break;
    case PART_LEFT_FEET:
        s->draw_order = 10;
        s->pose = PART_POSE_DYING_STATIONARY;
        break;
    case PART_TOP_HOLE_LEFT: case PART_TOP_HOLE_RIGHT: case PART_MIDDLE_HOLE_LEFT:
    case PART_MIDDLE_HOLE_RIGHT: case PART_BOTTOM_HOLE_LEFT: case PART_BOTTOM_HOLE_RIGHT:
        s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
        s->pose = PART_POSE_DYING_STATIONARY;
        break;
    default:
        s->pose = PART_POSE_DYING_STATIONARY;
        break;
    }
}

/* KraidPartDyingSinking */
static void part_dying_sinking(Ctx *c) {
    MzmSprite *s = c->s;
    bool arm = s->room_slot == PART_RIGHT_ARM || s->room_slot == PART_LEFT_ARM;
    if (s->y_spawn != 0) {
        s->y_spawn--;
        if (arm) s->adc++;
        return;
    }
    if (arm && ai_animation_ending(c, 1)) {
        ai_set_anim(s, s->room_slot == PART_RIGHT_ARM ? MZM_KRAID_RIGHT_ARM_IDLE : MZM_KRAID_LEFT_ARM_IDLE);
        s->draw_top = s->room_slot == PART_RIGHT_ARM ? HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE : BLOCK_SIZE;
        s->draw_bottom = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        s->pose = PART_POSE_DYING_STATIONARY;
    }
}

/* KraidPartDyingStationary: once Kraid is gone, each part explodes. */
static void part_dying_stationary(Ctx *c, const MzmSprite *main) {
    MzmSprite *s = c->s;
    const MzmSpriteGlobals *g = c->f->globals;
    if (s->room_slot == PART_RIGHT_ARM || s->room_slot == PART_LEFT_ARM) s->adc = (uint8_t)(s->adc + 2);
    if ((main->status & MZM_SPRITE_STATUS_EXISTS) && !main->secondary && main->sprite_id == MZM_PSPRITE_KRAID) return;
    static const struct { int8_t dy, dx; uint8_t effect; } deaths[12] = {
        [PART_BELLY] = {-1, -2, MZM_PE_SPRITE_EXPLOSION_MEDIUM},
        [PART_TOP_HOLE_LEFT] = {-2, 4, MZM_PE_SPRITE_EXPLOSION_BIG},
        [PART_TOP_HOLE_RIGHT] = {-3, -6, MZM_PE_SPRITE_EXPLOSION_MEDIUM},
        [PART_MIDDLE_HOLE_LEFT] = {-4, 4, MZM_PE_SPRITE_EXPLOSION_MEDIUM},
        [PART_MIDDLE_HOLE_RIGHT] = {-5, -1, MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG},
        [PART_BOTTOM_HOLE_LEFT] = {-1, 3, MZM_PE_SPRITE_EXPLOSION_MEDIUM},
        [PART_BOTTOM_HOLE_RIGHT] = {-2, -5, MZM_PE_SPRITE_EXPLOSION_MEDIUM},
        [PART_LEFT_ARM] = {-3, 3, MZM_PE_SPRITE_EXPLOSION_BIG},
        [PART_RIGHT_ARM] = {-4, -1, MZM_PE_SPRITE_EXPLOSION_MEDIUM},
        [PART_LEFT_FEET] = {-5, 2, MZM_PE_SPRITE_EXPLOSION_MEDIUM},
        [PART_RIGHT_FEET] = {-1, -3, MZM_PE_SPRITE_EXPLOSION_BIG},
    };
    if (s->room_slot >= 12 || s->room_slot == PART_KRAID) {
        s->status = 0;
        return;
    }
    ai_sprite_death_full(c, DEATH_NORMAL, (uint16_t)(g->sub1.y + deaths[s->room_slot].dy * BLOCK_SIZE),
                         (uint16_t)(g->sub1.x + deaths[s->room_slot].dx * BLOCK_SIZE), false,
                         deaths[s->room_slot].effect);
}

/* KraidPart */
static void part(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    MzmSprite *main = slot(c, s->ram_slot);
    if (!main) return;
    s->absolute_palette_row = main->absolute_palette_row;
    s->palette_row = s->absolute_palette_row;
    if (g->sub1.health == 0) {
        s->ignore_samus_timer = 1;
        if (s->pose < PART_POSE_DYING_INIT) {
            s->pose = PART_POSE_DYING_INIT;
            s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        }
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: part_init(c); break;
    case PART_POSE_CHECK_THROW_NAILS: throw_nails(c, main); break;
    case PART_POSE_CHECK_ATTACK: check_attack(c, main); break;
    case PART_POSE_CHECK_SPAWN_SPIKES: check_spikes(c, main); break;
    case PART_POSE_SPAWN_SPIKES: spawn_spike(c, main); break;
    case PART_POSE_CHECK_PROJECTILES: belly_projectiles(c); break;
    case PART_POSE_DYING_INIT: part_dying_init(s); break;
    case PART_POSE_ARM_DYING: part_dying_sinking(c); break;
    case PART_POSE_DYING_STATIONARY: part_dying_stationary(c, main); break;
    default: break;
    }
    if (!(s->status & MZM_SPRITE_STATUS_EXISTS) || !s->secondary || s->sprite_id != MZM_SSPRITE_KRAID_PART) return;
    if (s->room_slot == PART_RIGHT_ARM) {
        if (s->anim == MZM_KRAID_RIGHT_ARM_IDLE) arm_hitbox(c, right_arm_idle, 10);
        else if (s->anim == MZM_KRAID_RIGHT_ARM_ATTACKING) arm_hitbox(c, right_arm_attacking, 12);
        else if (s->anim == MZM_KRAID_PART_2CADC4) {
            if (s->adc == 1) set_box(s, -(BLOCK_SIZE + HALF_BLOCK_SIZE), HALF_BLOCK_SIZE,
                                     BLOCK_SIZE * 3 + HALF_BLOCK_SIZE, BLOCK_SIZE * 4 + EIGHTH_BLOCK_SIZE);
        } else {
            s->ignore_samus_timer = 1;
        }
        sync_position(c);
    } else if (s->room_slot == PART_LEFT_ARM) {
        if (s->anim == MZM_KRAID_LEFT_ARM_IDLE) arm_hitbox(c, left_arm_idle, 10);
        else if (s->anim == MZM_KRAID_LEFT_ARM_DYING) arm_hitbox(c, left_arm_dying, 10);
        else if (s->anim == MZM_KRAID_LEFT_ARM_THROWING_NAILS) arm_hitbox(c, left_arm_attacking, 20);
        else if (s->anim == MZM_KRAID_PART_2CAC5C) {
            if (s->adc == 1) set_box(s, -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE), -HALF_BLOCK_SIZE, EIGHTH_BLOCK_SIZE,
                                     BLOCK_SIZE * 2 + EIGHTH_BLOCK_SIZE);
        } else {
            s->ignore_samus_timer = 1;
        }
        sync_position(c);
    } else {
        sync_sub_sprites(c);
        if (g->sub1.work1 != 0 && (s->room_slot == PART_LEFT_FEET || s->room_slot == PART_RIGHT_FEET) &&
            s->adc < 12)            /* CONVERT_SECONDS(.2f) */
            s->adc = (uint8_t)(s->adc + 4);
    }
}

/* KraidSpike: grows out of its hole, then flies right a pixel and a half a
 * frame, breaking the platforms on its way until the wall; Samus can ride
 * it. */
static void spike(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if (g->sub1.health == 0) s->pose = SPRITE_POSE_DESTROYED;
    MzmSprite *hole = slot(c, s->ram_slot);
    if (!hole) return;
    /* gSpriteData[primarySpriteRamSlot]: its hole, which has Kraid's row. */
    uint8_t main_row = hole->absolute_palette_row;
    if (s->absolute_palette_row != main_row) {
        s->absolute_palette_row = main_row;
        if (s->palette_row == 0) s->palette_row = main_row;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->health = c->tables->secondary_health[s->sprite_id];
        s->pose = SPIKE_POSE_DELAY_BEFORE_MOVING;
        s->samus_collision = SSC_HURTS_SAMUS;
        draw(s, QUARTER_BLOCK_SIZE, QUARTER_BLOCK_SIZE, THREE_QUARTER_BLOCK_SIZE);
        set_box(s, -HALF_BLOCK_SIZE, HALF_BLOCK_SIZE, 0, EIGHTH_BLOCK_SIZE);
        s->draw_order = 12;
        ai_set_anim(s, MZM_KRAID_SPIKE);
        s->work0 = 20;              /* ONE_THIRD_SECOND */
        mzm_sfx_play(MZM_SOUND_KRAID_SPIKE_SPAWNING);
        /* fallthrough */
    case SPIKE_POSE_DELAY_BEFORE_MOVING:
        if (s->frame <= 4) {
            static const int16_t growth[5] = {0, HALF_BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE + HALF_BLOCK_SIZE,
                                              BLOCK_SIZE * 2};
            if (s->frame >= 1) s->hitbox_right = growth[s->frame];
        } else {
            s->samus_collision = SSC_HURTS_RIGHT_CAN_STAND_ON_TOP;
            s->hitbox_left = QUARTER_BLOCK_SIZE;
            s->hitbox_right = BLOCK_SIZE * 2 + HALF_BLOCK_SIZE;
            if (s->work0 != 0) {
                s->work0--;
            } else {
                s->pose = SPIKE_POSE_MOVING;
                s->work0 = 4;       /* CONVERT_SECONDS(1.f / 15) */
                mzm_sfx_play(MZM_SOUND_KRAID_SPIKE_EMERGING);
            }
        }
        s->y = hole->y;
        s->x = hole->x;
        break;
    case SPIKE_POSE_MOVING: {
        if (s->work0 != 0 && --s->work0 == 0) s->draw_order = 4;
        MzmSpriteSamusLink *link = c->f->samus_link;
        if (s->status & MZM_SPRITE_STATUS_SAMUS_ON_TOP && link &&
            ai_collision_at(c, (uint16_t)(c->f->samus_y - HALF_BLOCK_SIZE), (uint16_t)(c->f->samus_x + HALF_BLOCK_SIZE)) ==
                MZM_COLLISION_AIR) {
            link->x = (uint16_t)(link->x + EIGHTH_BLOCK_SIZE);
            link->moved = true;
        }
        s->x = (uint16_t)(s->x + EIGHTH_BLOCK_SIZE);
        uint16_t y = s->y, x = (uint16_t)(s->x + BLOCK_SIZE * 2);
        if (s->room_slot == PART_TOP_HOLE_LEFT) y = (uint16_t)(y - HALF_BLOCK_SIZE);
        if (ai_collision_at(c, y, x) == MZM_COLLISION_AIR) break;
        uint8_t rng = c->rng;
        if (x < (uint16_t)(s->x_spawn + BLOCK_SIZE * 13)) {
            /* A platform: broken. */
            remove_solid(c, y, x);
            ai_particle_set(c, y, x, MZM_PE_SPRITE_EXPLOSION_MEDIUM);
            ai_debris_init(c, 0x11, (uint16_t)(y - (HALF_BLOCK_SIZE - PIXEL_SIZE - PIXEL_SIZE / 2) - rng), x);
            ai_debris_init(c, 0x12, (uint16_t)(y - QUARTER_BLOCK_SIZE), (uint16_t)(x + EIGHTH_BLOCK_SIZE + rng));
            ai_debris_init(c, 0x13, (uint16_t)(y - BLOCK_SIZE - rng), (uint16_t)(x + QUARTER_BLOCK_SIZE));
            ai_debris_init(c, 0x4, (uint16_t)(y - (HALF_BLOCK_SIZE + PIXEL_SIZE + PIXEL_SIZE / 2)),
                           (uint16_t)(x - (HALF_BLOCK_SIZE - PIXEL_SIZE) - rng));
            if (s->room_slot == PART_TOP_HOLE_LEFT) {
                if (!(g->sub1.work3 & DESTROYED_TOP)) {
                    g->sub1.work3 |= DESTROYED_TOP;
                    mzm_sfx_play(MZM_SOUND_KRAID_SPIKE_DESTROYING_PLATFORM);
                }
            } else if (s->room_slot == PART_MIDDLE_HOLE_LEFT) {
                if (!(g->sub1.work3 & DESTROYED_MIDDLE)) {
                    g->sub1.work3 |= DESTROYED_MIDDLE;
                    mzm_sfx_play(MZM_SOUND_KRAID_SPIKE_DESTROYING_BLOCKS);
                }
                remove_solid(c, (uint16_t)(y - BLOCK_SIZE), x);
                remove_solid(c, (uint16_t)(y - BLOCK_SIZE * 2), x);
                ai_particle_set(c, (uint16_t)(y - BLOCK_SIZE * 2), x, MZM_PE_SPRITE_EXPLOSION_MEDIUM);
            } else if (s->room_slot == PART_BOTTOM_HOLE_LEFT) {
                if (!(g->sub1.work3 & DESTROYED_BOTTOM)) {
                    g->sub1.work3 |= DESTROYED_BOTTOM;
                    mzm_sfx_play(MZM_SOUND_KRAID_SPIKE_DESTROYING_BLOCKS);
                }
                remove_solid(c, (uint16_t)(y - BLOCK_SIZE), x);
                remove_solid(c, (uint16_t)(y + BLOCK_SIZE), x);
                ai_particle_set(c, (uint16_t)(y + BLOCK_SIZE), x, MZM_PE_SPRITE_EXPLOSION_MEDIUM);
            }
            break;
        }
        /* The wall: it sticks there for six seconds. */
        if (s->room_slot == PART_TOP_HOLE_LEFT) {
            y = (uint16_t)(y + HALF_BLOCK_SIZE);
        } else if (s->room_slot == PART_MIDDLE_HOLE_LEFT) {
            static const int16_t dx[3] = {-HALF_BLOCK_SIZE, -(BLOCK_SIZE * 2 - HALF_BLOCK_SIZE),
                                          -(BLOCK_SIZE * 3 - HALF_BLOCK_SIZE)};
            for (int i = 0; i < 3; ++i) {
                uint16_t by = (uint16_t)(y - BLOCK_SIZE * 3), bx = (uint16_t)(x + dx[i]);
                if (ai_collision_at(c, by, bx) != MZM_COLLISION_AIR) {
                    remove_solid(c, by, bx);
                    ai_particle_set(c, by, bx, MZM_PE_SPRITE_EXPLOSION_MEDIUM);
                }
            }
        }
        ai_debris_init(c, 0x11, (uint16_t)(y - rng), (uint16_t)(x + QUARTER_BLOCK_SIZE));
        ai_debris_init(c, 0x12, (uint16_t)(y + QUARTER_BLOCK_SIZE), (uint16_t)(x + rng * PIXEL_SIZE / 2));
        ai_debris_init(c, 0x13, (uint16_t)(y + rng), (uint16_t)(x - QUARTER_BLOCK_SIZE));
        ai_debris_init(c, 0x4, (uint16_t)(y - QUARTER_BLOCK_SIZE), (uint16_t)(x - rng * PIXEL_SIZE / 2));
        s->y_spawn = 360;           /* CONVERT_SECONDS(6.f) */
        s->pose = SPIKE_POSE_IN_WALL;
        s->draw_order = 4;
        mzm_sfx_play(MZM_SOUND_KRAID_SPIKE_HITTING_WALL);
        break;
    }
    case SPIKE_POSE_IN_WALL: {
        /* Flashing its stun palette for its last second. */
        uint16_t timer = --s->y_spawn;
        if (timer <= 60 && (timer & 3) == 0) {
            if (timer & 4) {
                s->palette_row = (uint8_t)(14 - s->gfx_slot - s->frozen_row_offset);
            } else {
                s->palette_row = s->absolute_palette_row;
                if (timer == 0) s->pose = SPRITE_POSE_DESTROYED;
            }
        }
        break;
    }
    default:
        if (s->standing_on != MZM_STANDING_ON_SPRITE_OFF) s->standing_on = MZM_STANDING_ON_SPRITE_DROPPED;
        s->status = 0;
        ai_particle_set(c, s->y, (uint16_t)(s->x + BLOCK_SIZE + QUARTER_BLOCK_SIZE), MZM_PE_SPRITE_EXPLOSION_HUGE);
        mzm_sfx_play(MZM_SOUND_138);
        break;
    }
}

/* KraidNailMovement: along the unit vector from its block toward its
 * destination, accelerating (4 subpixels a frame each frame, 8 on hard). */
static void nail_move(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t velocity = c->f->difficulty == 2 ? EIGHTH_BLOCK_SIZE : PIXEL_SIZE;
    uint8_t acceleration = ++s->work1;
    if (acceleration == 0) {
        s->status = 0;
        return;
    }
    velocity = (uint16_t)(velocity * acceleration);
    uint32_t spawn_y = (uint32_t)s->work3 * BLOCK_SIZE, spawn_x = (uint32_t)s->work2 * BLOCK_SIZE;
    bool down = s->status & MZM_SPRITE_STATUS_FACING_DOWN, right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    uint16_t dy = down ? (uint16_t)(s->y_spawn - spawn_y) : (uint16_t)(spawn_y - s->y_spawn);
    uint16_t dx = right ? (uint16_t)(s->x_spawn - spawn_x) : (uint16_t)(spawn_x - s->x_spawn);
    int32_t total = mzm_bios_sqrt((uint32_t)dx * dx + (uint32_t)dy * dy);
    if (total == 0) return;
    int32_t oy = (velocity * ((int32_t)(dy << 10) / total)) >> 10;
    int32_t ox = (velocity * ((int32_t)(dx << 10) / total)) >> 10;
    s->y = (uint16_t)(down ? spawn_y + oy : spawn_y - oy);
    s->x = (uint16_t)(right ? spawn_x + ox : spawn_x - ox);
}

/* KraidNail */
static void nail(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->health = c->tables->secondary_health[s->sprite_id];
        s->pose = NAIL_POSE_MOVING;
        s->samus_collision = SSC_HURTS_SAMUS;
        s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        set_box(s, -(HALF_BLOCK_SIZE - PIXEL_SIZE), HALF_BLOCK_SIZE - PIXEL_SIZE, -(HALF_BLOCK_SIZE - PIXEL_SIZE),
                HALF_BLOCK_SIZE - PIXEL_SIZE);
        ai_set_anim(s, MZM_KRAID_NAIL);
        s->draw_order = 3;
        s->status |= MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE;
        s->scaling = 0x100;
        s->work1 = 0;
        s->work3 = (uint8_t)(s->y / BLOCK_SIZE);
        s->work2 = (uint8_t)(s->x / BLOCK_SIZE);
        uint16_t dst_y;
        if (s->room_slot != NAIL_SLOW_ROTATION) {
            dst_y = c->f->samus_y;
            s->rotation = BLOCK_SIZE;
        } else {
            dst_y = (uint16_t)(c->f->samus_y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE));
            s->rotation = 0;
        }
        uint16_t dst_x = c->f->samus_x;
        if (dst_y < (uint16_t)(s->work3 * BLOCK_SIZE)) s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
        else s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        if (dst_x < s->x) dst_x = (uint16_t)(c->f->samus_x + BLOCK_SIZE);
        s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
        s->y_spawn = dst_y;
        s->x_spawn = dst_x;
    }
        /* fallthrough */
    case NAIL_POSE_MOVING:
        if (ai_collision_at(c, s->y, s->x) != MZM_COLLISION_AIR) {
            ai_particle_set(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_SMALL);
            ai_sound_onscreen(s, MZM_SOUND_SPRITE_EXPLOSION_SMALL);
            s->status = 0;
            break;
        }
        /* (u8) PI times .15625 / .171875 on hard, .09375 / .109375 else. */
        if (c->f->difficulty == 2) s->rotation = (uint16_t)(s->rotation + (s->room_slot != NAIL_SLOW_ROTATION ? 20 : 22));
        else s->rotation = (uint16_t)(s->rotation + (s->room_slot != NAIL_SLOW_ROTATION ? 12 : 14));
        if (s->health == 0) s->pose = SPRITE_POSE_STOPPED;
        else nail_move(c);
        break;
    default:
        ai_sprite_death(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_SMALL);
        break;
    }
}

void ai_kraid(Ctx *c) {
    if (!c->f->globals) return;
    if (!c->s->secondary) kraid(c);
    else if (c->s->sprite_id == MZM_SSPRITE_KRAID_PART) part(c);
    else if (c->s->sprite_id == MZM_SSPRITE_KRAID_SPIKE) spike(c);
    else nail(c);
}
