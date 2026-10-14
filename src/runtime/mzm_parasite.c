/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission Parasites (src/sprites_ai/parasite.c): the single Parasites
 * that leech Samus and grab the Gerons, and the "multiple" ones of the bug
 * room (a main Parasite spawning five more, the doors locked until they are
 * all killed); comments name the pinned decompilation's routines. The
 * splashes of SpriteUtilCheckInRoomEffect and the dying BG priority are not
 * ported. */
#include "mzm_sprite_ai.h"

#include <string.h>

enum {
    PARASITE_POSE_IDLE_INIT = 0x8,
    PARASITE_POSE_IDLE = 0x9,
    PARASITE_POSE_TURNING_AROUND_INIT = 0xA,
    PARASITE_POSE_TURNING_AROUND_FIRST_PART = 0xB,
    PARASITE_POSE_TURNING_AROUND_SECOND_PART = 0xC,
    PARASITE_POSE_LANDING_INIT = 0xE,
    PARASITE_POSE_LANDING = 0xF,
    PARASITE_POSE_FALLING_INIT = 0x1E,
    PARASITE_POSE_FALLING = 0x1F,
    PARASITE_POSE_SAMUS_GRABBED_INIT = 0x42,
    PARASITE_POSE_SAMUS_GRABBED = 0x43,
    PARASITE_POSE_EXPULSED_INIT = 0x44,
    PARASITE_POSE_EXPULSED_UP = 0x45,
    PARASITE_POSE_EXPULSED_DOWN = 0x47,
    PARASITE_POSE_JUMPING_UP = 0x49,
    PARASITE_POSE_JUMPING_DOWN = 0x4B,
    PARASITE_POSE_GERON_GRABBED_INIT = 0x4C,
    PARASITE_POSE_GERON_GRABBED = 0x4D,
    PARASITE_POSE_DYING_INIT = 0x62,
    PARASITE_POSE_DYING = 0x67,
    /* GERON_POSE_DELAY_BEFORE_DESTROYED */
    GERON_POSE_DELAY_BEFORE_DESTROYED = 0x24,
    /* SPRITE_RNG_PROB(p) = p * 16 */
    RNG_COUNT = 16,
    MZM_EVENT_BUGS_KILLED = 45,
    PARASITE_RELATED_TIME = 90,         /* CONVERT_SECONDS(1.5f) */
};

static bool multiple(const MzmSprite *s) { return s->sprite_id == MZM_PSPRITE_PARASITE_MULTIPLE; }

/* ClipdataProcess & CLIPDATA_TYPE_SOLID_FLAG (the multiple Parasites). */
static bool clip_solid(const Ctx *c, uint16_t y, uint16_t x) {
    return mzm_clipdata_process(c->world, y, x, MZM_ACTOR_SPRITE) & MZM_CLIPDATA_SOLID_FLAG;
}

/* The wall test ahead: SpriteUtilGetCollisionAtPosition & COLLISION_FLAGS_UNKNOWN_F0
 * for the single Parasites, ClipdataProcess's solid flag for the others. */
static bool wall_at(const Ctx *c, uint16_t y, uint16_t x) {
    if (multiple(c->s)) return clip_solid(c, y, x);
    return ai_collision_at(c, y, x) & MZM_COLLISION_FLAGS_F0;
}

static void set_pose_anim(MzmSprite *s, uint8_t pose, uint8_t anim) {
    s->pose = pose;
    ai_set_anim(s, anim);
}

static MzmSpriteGlobals *globals(const Ctx *c) { return c->f->globals; }

/* SpriteSpawnPrimary */
static void spawn_primary(Ctx *c, uint8_t sprite_id, uint8_t part, uint8_t gfx_slot, uint16_t y,
                          uint16_t x) {
    for (int slot = 0; c->f->sprites && slot < c->f->sprite_count; ++slot) {
        MzmSprite *n = &c->f->sprites[slot];
        if (n->status & MZM_SPRITE_STATUS_EXISTS) continue;
        memset(n, 0, sizeof *n);
        n->status = MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_ONSCREEN | MZM_SPRITE_STATUS_NOT_DRAWN;
        n->gfx_slot = gfx_slot;
        n->sprite_id = sprite_id;
        n->y = y;
        n->x = x;
        n->room_slot = part;
        n->draw_order = 4;
        n->pose = SPRITE_POSE_UNINITIALIZED;
        n->ignore_samus_timer = 1;
        n->ram_slot = (uint8_t)slot;
        return;
    }
}

/* ParasiteInit */
static void parasite_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->hitbox_top = -PIXEL_SIZE;
    s->hitbox_bottom = 0;
    s->hitbox_left = -PIXEL_SIZE;
    s->hitbox_right = PIXEL_SIZE;
    s->draw_top = HALF_BLOCK_SIZE / PIXEL_SIZE;
    s->draw_bottom = QUARTER_BLOCK_SIZE / PIXEL_SIZE;
    s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
    ai_set_anim(s, MZM_PARASITE_IDLE);
    s->samus_collision = SSC_PARASITE;
    s->health = 1;
    if (globals(c)) globals(c)->sub1.work2 = 0;
    /* SpriteUtilChooseRandomXFlip */
    if (c->rng & 1) s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
    else s->status |= MZM_SPRITE_STATUS_X_FLIP;
    s->pose = PARASITE_POSE_IDLE_INIT;
    if (!multiple(s)) return;
    if (c->f->equipment && c->f->equipment->items & MZM_ITEM_BOMBS &&
        !ai_check_event(c, MZM_EVENT_BUGS_KILLED)) {
        const MzmSpriteHooks *h = c->f->hooks;
        if (h && h->door_unlock_timer) h->door_unlock_timer(h->context, 1);    /* LOCK_DOORS */
    }
    if (s->status & MZM_SPRITE_STATUS_NOT_DRAWN) {
        /* A sub Parasite (SpriteSpawnPrimary adds the not drawn flag). */
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->x = (uint16_t)(s->x + c->rng * c->rng);
    } else {
        /* The main Parasite spawns five more. */
        uint8_t gfx = s->gfx_slot, part = s->room_slot;
        uint16_t y = s->y, x = s->x;
        spawn_primary(c, s->sprite_id, part, gfx, y, (uint16_t)(x - EIGHTH_BLOCK_SIZE));
        spawn_primary(c, s->sprite_id, part, gfx, y, (uint16_t)(x + EIGHTH_BLOCK_SIZE + PIXEL_SIZE));
        spawn_primary(c, s->sprite_id, part, gfx, y, (uint16_t)(x - (EIGHTH_BLOCK_SIZE + PIXEL_SIZE)));
        spawn_primary(c, s->sprite_id, part, gfx, y, (uint16_t)(x + EIGHTH_BLOCK_SIZE));
        spawn_primary(c, s->sprite_id, part, gfx, y, x);
    }
}

/* ParasiteGrabSamus / ParasiteGrabGeron: the offsets from the host's top
 * left corner. */
static void grab(Ctx *c, uint16_t host_top, uint16_t host_left, uint16_t flip_y) {
    MzmSprite *s = c->s;
    ai_set_anim(s, MZM_PARASITE_ATTACHED);
    s->work0 = 0;
    s->y_spawn = s->y < host_top ? 0 : (uint16_t)(s->y - host_top);
    s->x_spawn = s->x < host_left ? 0 : (uint16_t)(s->x - host_left);
    if (s->y > flip_y) s->status &= (uint16_t)~MZM_SPRITE_STATUS_Y_FLIP;
    else s->status |= MZM_SPRITE_STATUS_Y_FLIP;
}

/* The crawl over a host (ParasiteSamusGrabbed / ParasiteGeronGrabbed): the
 * direction turns at the host's edges, then the offsets move a subpixel. */
static void crawl(Ctx *c, uint16_t bottom, uint16_t top, uint16_t right, uint16_t left) {
    MzmSprite *s = c->s;
    if (globals(c) && globals(c)->parasite_related == 0)
        globals(c)->parasite_related = PARASITE_RELATED_TIME;
    if (s->status & MZM_SPRITE_STATUS_Y_FLIP) {
        if (bottom < s->y + EIGHTH_BLOCK_SIZE) {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_Y_FLIP;
            s->work0 = c->rng;
        }
    } else if (top > s->y - EIGHTH_BLOCK_SIZE) {
        s->status |= MZM_SPRITE_STATUS_Y_FLIP;
        s->work0 = c->rng;
    }
    if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
        if (right < s->x + EIGHTH_BLOCK_SIZE) {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
            s->work0 = c->rng;
        }
    } else if (left > s->x - EIGHTH_BLOCK_SIZE) {
        s->status |= MZM_SPRITE_STATUS_X_FLIP;
        s->work0 = c->rng;
    }
    if (s->work0 == 0) {
        if (c->rng != 0) s->y_spawn = (uint16_t)(s->status & MZM_SPRITE_STATUS_Y_FLIP ? s->y_spawn + 1 : s->y_spawn - 1);
        if (c->rng != s->ram_slot)
            s->x_spawn = (uint16_t)(s->status & MZM_SPRITE_STATUS_X_FLIP ? s->x_spawn + 1 : s->x_spawn - 1);
    } else {
        s->work0--;
    }
}

/* The expulsion's horizontal speed: the RNG, at least 6 subpixels. */
static void expel(Ctx *c) {
    MzmSprite *s = c->s;
    s->pose = PARASITE_POSE_EXPULSED_INIT;
    s->work2 = c->rng < 6 ? PIXEL_SIZE + PIXEL_SIZE / 2 : c->rng;
}

/* ParasiteSamusGrabbed */
static void samus_grabbed(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSpriteFrame *f = c->f;
    if (f->samus_link && f->samus_link->samus_pose == MZM_POSE_SCREW_ATTACKING) {
        expel(c);
        return;
    }
    uint16_t top = (uint16_t)(f->samus_y + f->samus_hitbox_top);
    uint16_t left = (uint16_t)(f->samus_x + f->samus_hitbox_left);
    crawl(c, (uint16_t)(f->samus_y + f->samus_hitbox_bottom), top,
          (uint16_t)(f->samus_x + f->samus_hitbox_right), left);
    s->y = (uint16_t)(top + s->y_spawn);
    s->x = (uint16_t)(left + s->x_spawn);
}

/* ParasiteGeronGrabbed: the Geron's destruction throws it off. */
static void geron_grabbed(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSprite *g = &c->f->sprites[s->work1];
    if (g->pose == GERON_POSE_DELAY_BEFORE_DESTROYED) {
        expel(c);
        s->samus_collision = SSC_PARASITE;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        return;
    }
    uint16_t top = (uint16_t)(g->y + g->hitbox_top), left = (uint16_t)(g->x + g->hitbox_left);
    crawl(c, g->y, top, (uint16_t)(g->x + g->hitbox_right), left);
    s->y = (uint16_t)(top + s->y_spawn);
    s->x = (uint16_t)(left + s->x_spawn);
}

/* The horizontal move of a jump or an expulsion (work2 a frame), stopped by
 * a wall a quarter block ahead. */
static void move_ahead(Ctx *c, uint16_t y, uint16_t x) {
    MzmSprite *s = c->s;
    if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
        if (!wall_at(c, y, (uint16_t)(x + QUARTER_BLOCK_SIZE))) s->x = (uint16_t)(s->x + s->work2);
    } else if (!wall_at(c, y, (uint16_t)(x - QUARTER_BLOCK_SIZE))) {
        s->x = (uint16_t)(s->x - s->work2);
    }
}

/* ParasiteExpelledUp / ParasiteJumpingUp (and their multiple versions):
 * rising by work3, one less each frame, held under a ceiling. */
static void rising(Ctx *c, uint8_t next_pose) {
    MzmSprite *s = c->s;
    uint8_t velocity = s->work3--;
    if (velocity != 0) {
        s->y = (uint16_t)(s->y - velocity);
    } else {
        s->work3 = 0;
        s->pose = next_pose;
    }
    uint16_t y = s->y, x = s->x;
    uint16_t above = (uint16_t)(y - (QUARTER_BLOCK_SIZE + PIXEL_SIZE));
    bool ceiling = multiple(s) ? clip_solid(c, above, x) : ai_collision_at(c, above, x) == MZM_COLLISION_SOLID;
    if (ceiling) s->y = (uint16_t)((y & BLOCK_POSITION_FLAG) + BLOCK_SIZE + QUARTER_BLOCK_SIZE);
    move_ahead(c, y, x);
}

/* The ground test of a falling Parasite: the slope-aware vertical check
 * for the single ones, the solid block's top for the others. */
static bool landed(Ctx *c, uint16_t *top) {
    MzmSprite *s = c->s;
    if (multiple(s)) {
        if (clip_solid(c, s->y, s->x) && s->y >= (s->y & BLOCK_POSITION_FLAG)) {
            *top = s->y & BLOCK_POSITION_FLAG;
            return true;
        }
        return false;
    }
    *top = ai_check_vertical_slopes(c, s->y, s->x);
    return c->vertical != MZM_COLLISION_AIR;
}

/* ParasiteExpelledDown (and multiple): landing on its feet or tumbling. */
static void expelled_down(Ctx *c) {
    MzmSprite *s = c->s;
    int16_t velocity = (int16_t)s->work3;
    if (velocity < QUARTER_BLOCK_SIZE + PIXEL_SIZE) s->work3 = (uint8_t)(velocity + PIXEL_SIZE / 2);
    s->y = (uint16_t)(s->y + velocity);
    uint16_t top;
    if (landed(c, &top)) {
        if (c->rng > 8) {
            ai_set_anim(s, MZM_PARASITE_LANDING);
        } else {
            ai_set_anim(s, MZM_PARASITE_TUMBLING);
            s->work0 = (uint8_t)(c->rng * 2 + 32);     /* rng / 30 s + .5 s + 1 / 30 s */
        }
        s->pose = PARASITE_POSE_LANDING;
        s->y = top;
        return;
    }
    move_ahead(c, s->y, s->x);
}

/* ParasiteJumpingDown (and multiple) */
static void jumping_down(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
        s->pose = PARASITE_POSE_SAMUS_GRABBED_INIT;
        return;
    }
    int32_t velocity = s->work3;
    if (velocity < QUARTER_BLOCK_SIZE) s->work3 = (uint8_t)(velocity + PIXEL_SIZE / 2);
    s->y = (uint16_t)(s->y + velocity);
    uint16_t top;
    if (landed(c, &top)) {
        ai_set_anim(s, MZM_PARASITE_LANDING);
        s->pose = PARASITE_POSE_LANDING;
        s->y = top;
        return;
    }
    move_ahead(c, s->y, s->x);
}

/* ParasiteIdleInit */
static void idle_init(Ctx *c) {
    MzmSprite *s = c->s;
    set_pose_anim(s, PARASITE_POSE_IDLE, MZM_PARASITE_IDLE);
    uint8_t velocity = c->rng / 2;
    if (velocity < PIXEL_SIZE + PIXEL_SIZE / 2) velocity = PIXEL_SIZE + PIXEL_SIZE / 2;
    s->work2 = velocity;
    s->work0 = (uint8_t)(((s->x / HALF_BLOCK_SIZE) % RNG_COUNT + c->rng) % RNG_COUNT);
}

/* ParasiteIdle / ParasiteMultipleIdle: crawling, a short hop or a jump
 * when the frame counter meets its timer, faster once Samus came near. */
static void idle(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
        s->pose = PARASITE_POSE_SAMUS_GRABBED_INIT;
        return;
    }
    if (multiple(s)) {
        /* Every other frame (by slot parity), falls off the ground. */
        if ((s->ram_slot & 1) == (c->frame8 & 1) && !clip_solid(c, s->y, s->x)) {
            s->pose = PARASITE_POSE_FALLING_INIT;
            return;
        }
    } else {
        ai_align_on_slope(c);
        if (c->vertical == MZM_COLLISION_AIR) {
            s->pose = PARASITE_POSE_FALLING_INIT;
            return;
        }
        if (ai_collision_at(c, (uint16_t)(s->y - QUARTER_BLOCK_SIZE), s->x) == MZM_COLLISION_SOLID) {
            s->pose = PARASITE_POSE_DYING_INIT;
            return;
        }
    }
    uint32_t timer = s->work0;
    uint32_t now = c->frame8 / RNG_COUNT;
    if (now == timer) {
        ai_set_anim(s, MZM_PARASITE_LANDING_AFTER_FALLING);
        s->work0 = (uint8_t)(c->rng * 3);
        s->pose = PARASITE_POSE_LANDING;
        return;
    }
    if (now == timer + 1 || now == timer - 1) {
        set_pose_anim(s, PARASITE_POSE_JUMPING_UP, MZM_PARASITE_JUMPING_UP);
        s->work3 = c->rng < 4 ? 4 : c->rng;
        if (s->status & MZM_SPRITE_STATUS_ONSCREEN) {
            if (s->work3 < 8) mzm_sfx_play_not_playing(MZM_SOUND_PARASITE_JUMPING_1);
            else if (s->work3 >= 12) mzm_sfx_play_not_playing(MZM_SOUND_PARASITE_JUMPING_3);
            else mzm_sfx_play_not_playing(MZM_SOUND_PARASITE_JUMPING_2);
        }
        return;
    }
    uint16_t velocity;
    MzmSpriteGlobals *g = globals(c);
    if (!g || g->parasite_related == 0) {
        /* The multiple Parasites' x range is BLOCK_SIZE + NSLR_RIGHT. */
        uint16_t x_range = multiple(s) ? BLOCK_SIZE + NSLR_RIGHT : BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        if (g && ai_samus_near_left_right(c, BLOCK_SIZE + HALF_BLOCK_SIZE, x_range))
            g->parasite_related = PARASITE_RELATED_TIME;
        velocity = (uint16_t)(timer % 2 + 1);
    } else {
        velocity = s->work2;
    }
    bool right = s->status & MZM_SPRITE_STATUS_X_FLIP;
    uint16_t ahead = (uint16_t)(right ? s->x + QUARTER_BLOCK_SIZE : s->x - QUARTER_BLOCK_SIZE);
    uint16_t y = (uint16_t)(s->y - QUARTER_BLOCK_SIZE);
    bool wall = multiple(s) ? clip_solid(c, y, ahead)
                            : c->vertical & MZM_COLLISION_FLAGS_F0 &&
                                  ai_collision_at(c, y, ahead) == MZM_COLLISION_SOLID;
    if (wall) s->pose = PARASITE_POSE_TURNING_AROUND_INIT;
    else s->x = (uint16_t)(right ? s->x + velocity : s->x - velocity);
}

/* ParasiteLanding: after a fall, a tumble (then back on its feet) or a
 * landing, idle or turning around. */
static void landing(Ctx *c) {
    MzmSprite *s = c->s;
    if (!clip_solid(c, s->y, s->x)) {
        s->pose = PARASITE_POSE_FALLING_INIT;
        return;
    }
    bool done = false;
    if (s->anim == MZM_PARASITE_LANDING_AFTER_FALLING) {
        if (--s->work0 == 0) done = true;
    } else if (s->anim == MZM_PARASITE_TUMBLING) {
        if (--s->work0 == 0) ai_set_anim(s, MZM_PARASITE_TURNING_BACK_ON_FEET);
    } else if (ai_animation_ending(c, 2)) {
        done = true;
    }
    if (done) s->pose = c->rng > 6 ? PARASITE_POSE_IDLE_INIT : PARASITE_POSE_TURNING_AROUND_INIT;
}

/* ParasiteFalling: sSpritesFallingSpeed. */
static void falling(Ctx *c) {
    MzmSprite *s = c->s;
    const int16_t *speeds = c->tables->falling;
    int16_t movement = speeds[s->work3];
    if (movement == SHORT_MAX) {
        s->y = (uint16_t)(s->y + speeds[s->work3 - 1]);
    } else {
        s->work3++;
        s->y = (uint16_t)(s->y + movement);
    }
    uint16_t top = ai_check_vertical_slopes(c, s->y, s->x);
    if (c->vertical != MZM_COLLISION_AIR) {
        s->y = top;
        s->pose = PARASITE_POSE_LANDING_INIT;
    }
}

/* ParasiteDying / ParasiteMultipleDying: the last multiple Parasite sets
 * EVENT_BUGS_KILLED and unlocks the doors. */
static void dying(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    if (!ai_animation_ending(c, 1)) return;
    s->status = 0;
    if (!multiple(s)) return;
    c->f->sprites[s->ram_slot].status = 0;
    if (ai_check_event(c, MZM_EVENT_BUGS_KILLED)) return;
    for (int i = 0; i < c->f->sprite_count; ++i) {
        const MzmSprite *n = &c->f->sprites[i];
        if (n->status & MZM_SPRITE_STATUS_EXISTS && !n->secondary &&
            n->sprite_id == MZM_PSPRITE_PARASITE_MULTIPLE)
            return;
    }
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->set_event) h->set_event(h->context, MZM_EVENT_BUGS_KILLED);
    if (h && h->door_unlock_timer) h->door_unlock_timer(h->context, -20);   /* -ONE_THIRD_SECOND */
}

/* ParasiteBombCollision: a hit only kills it under a power bomb or an
 * exploding bomb; any other hit throws it away from Samus. */
static void bomb_collision(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->isft & ISFT_POWER_BOMB_STUNNED) {
        s->pose = PARASITE_POSE_DYING_INIT;
        return;
    }
    uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
    uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
    const MzmWeapons *w = c->f->weapons;
    for (int i = 0; w && i < MZM_MAX_PROJECTILES; ++i) {
        const MzmProjectile *p = &w->list[i];
        if (!p->active || p->type != MZM_PROJECTILE_BOMB || p->bomb_stage != MZM_BOMB_EXPLODING) continue;
        /* The bomb's hitbox (ProjectileProcessBomb). */
        uint16_t py = (uint16_t)p->y, px = (uint16_t)p->x;
        uint16_t ptop = (uint16_t)(py - (BLOCK_SIZE - PIXEL_SIZE)), pbottom = (uint16_t)(py + THREE_QUARTER_BLOCK_SIZE);
        uint16_t pleft = (uint16_t)(px - THREE_QUARTER_BLOCK_SIZE), pright = (uint16_t)(px + THREE_QUARTER_BLOCK_SIZE);
        /* SpriteUtilCheckObjectsTouching */
        if (pbottom >= top && ptop < bottom && pright >= left && pleft < right) {
            s->pose = PARASITE_POSE_DYING_INIT;
            return;
        }
    }
    s->isft = 0;
    s->health = 1;
    s->pose = PARASITE_POSE_EXPULSED_INIT;
    uint8_t speed = c->rng / 2;
    if (speed < 9) speed = 9;
    s->work2 = speed;
    /* SpriteUtilMakeSpriteFaceAwayFromSamusDirection (the facing flag, not
     * the X flip the Parasite moves by). */
    if (s->x > c->f->samus_x) s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
    else s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
}

void ai_parasite(Ctx *c) {
    MzmSprite *s = c->s;
    bool multi = multiple(s);
    if ((s->isft & 0x7F) != 0 && s->pose < PARASITE_POSE_DYING) bomb_collision(c);
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        parasite_init(c);
        break;
    case PARASITE_POSE_IDLE_INIT:
        idle_init(c);
        /* The multiple Parasites wait a frame. */
        if (multi) break;
        /* fallthrough */
    case PARASITE_POSE_IDLE:
        idle(c);
        break;
    case PARASITE_POSE_TURNING_AROUND_INIT:
        set_pose_anim(s, PARASITE_POSE_TURNING_AROUND_FIRST_PART, MZM_PARASITE_TURNING_AROUND);
        /* fallthrough */
    case PARASITE_POSE_TURNING_AROUND_FIRST_PART:
        if (ai_animation_ending(c, 1)) {
            s->status ^= MZM_SPRITE_STATUS_X_FLIP;
            s->pose = PARASITE_POSE_TURNING_AROUND_SECOND_PART;
        }
        break;
    case PARASITE_POSE_TURNING_AROUND_SECOND_PART:
        if (ai_animation_ending(c, 2)) s->pose = PARASITE_POSE_IDLE_INIT;
        break;
    case PARASITE_POSE_LANDING_INIT:
        set_pose_anim(s, PARASITE_POSE_LANDING, MZM_PARASITE_LANDING_AFTER_FALLING);
        s->work0 = 30;                          /* CONVERT_SECONDS(.5f) */
        /* fallthrough */
    case PARASITE_POSE_LANDING:
        landing(c);
        break;
    case PARASITE_POSE_FALLING_INIT:
        s->pose = PARASITE_POSE_FALLING;
        s->work3 = 0;
        /* fallthrough */
    case PARASITE_POSE_FALLING:
        falling(c);
        break;
    case PARASITE_POSE_SAMUS_GRABBED_INIT: {
        const MzmSpriteFrame *f = c->f;
        s->pose = PARASITE_POSE_SAMUS_GRABBED;
        grab(c, (uint16_t)(f->samus_y + f->samus_hitbox_top), (uint16_t)(f->samus_x + f->samus_hitbox_left),
             (uint16_t)(f->samus_y - BLOCK_SIZE));
    }
        /* fallthrough */
    case PARASITE_POSE_SAMUS_GRABBED:
        samus_grabbed(c);
        break;
    case PARASITE_POSE_EXPULSED_INIT:
        set_pose_anim(s, PARASITE_POSE_EXPULSED_UP, MZM_PARASITE_EXPELLED);
        s->work3 = EIGHTH_BLOCK_SIZE;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_Y_FLIP;
        /* fallthrough */
    case PARASITE_POSE_EXPULSED_UP:
        rising(c, PARASITE_POSE_EXPULSED_DOWN);
        break;
    case PARASITE_POSE_EXPULSED_DOWN:
        expelled_down(c);
        break;
    case PARASITE_POSE_JUMPING_UP:
        if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) s->pose = PARASITE_POSE_SAMUS_GRABBED_INIT;
        else rising(c, PARASITE_POSE_JUMPING_DOWN);
        break;
    case PARASITE_POSE_JUMPING_DOWN:
        jumping_down(c);
        break;
    case PARASITE_POSE_GERON_GRABBED_INIT:
        if (multi) break;
        s->pose = PARASITE_POSE_GERON_GRABBED;
        {
            const MzmSprite *g = &c->f->sprites[s->work1];
            grab(c, (uint16_t)(g->y + g->hitbox_top), (uint16_t)(g->x + g->hitbox_left),
                 (uint16_t)(g->y - (BLOCK_SIZE + HALF_BLOCK_SIZE)));
        }
        break;
    case PARASITE_POSE_GERON_GRABBED:
        if (!multi) geron_grabbed(c);
        break;
    case PARASITE_POSE_DYING_INIT:
        s->status &= (uint16_t)~(MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE | MZM_SPRITE_STATUS_Y_FLIP);
        set_pose_anim(s, PARASITE_POSE_DYING, MZM_PARASITE_DYING);
        /* fallthrough */
    case PARASITE_POSE_DYING:
        dying(c);
        break;
    default:
        break;
    }
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
}
