/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Brinstar hives (src/sprites_ai/hive.c): the hive and its
 * three shrinking phases, its roots, the Mellows that guard it and the
 * Mellow swarms that keep spawning until the three hives are destroyed. */
#include "mzm_sprite_ai.h"

enum {
    HIVE_POSE_PHASE_1 = 0x9,
    HIVE_POSE_PHASE_2 = 0x23,
    HIVE_POSE_PHASE_3 = 0x25,
    HIVE_POSE_DEAD = 0x67,

    HIVE_ROOTS_POSE_IDLE = 0x9,

    MELLOW_POSE_IDLE = 0x9,
    MELLOW_POSE_MOVE_INIT = 0x22,
    MELLOW_POSE_MOVE = 0x23,
    MELLOW_POSE_FLEEING = 0x25,

    MELLOW_SWARM_POSE_IDLE = 0x9,

    /* The room slot of a Mellow spawned by a swarm. */
    MELLOW_FROM_SWARM = 0x88,
};

/* The half lengths of sMellowIdleYVelocity / sMellowIdleXVelocity
 * (ARRAY_SIZE / 2: 65 and 81 entries). */
#define MELLOW_IDLE_Y_HALF 32
#define MELLOW_IDLE_X_HALF 40

static uint16_t base_health(const Ctx *c) {
    return c->tables->health[c->s->sprite_id];
}

/* HiveSpawnParticle */
static void spawn_particle(Ctx *c) {
    uint16_t y = c->s->y, x = c->s->x;
    ai_particle_set(c, (uint16_t)(y - HALF_BLOCK_SIZE), x, MZM_PE_MEDIUM_DUST);
    ai_particle_set(c, (uint16_t)(y + BLOCK_SIZE + HALF_BLOCK_SIZE), x, MZM_PE_TWO_MEDIUM_DUST);
}

/* HiveInit: the roots and four Mellows around the hive. */
static void hive_init(Ctx *c) {
    MzmSprite *s = c->s;
    if (ai_check_event(c, MZM_EVENT_THREE_HIVES_DESTROYED)) {
        s->status = 0;
        return;
    }
    s->draw_top = s->draw_bottom = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    s->draw_horizontal = (BLOCK_SIZE + QUARTER_BLOCK_SIZE) / PIXEL_SIZE;
    s->hitbox_top = -(BLOCK_SIZE * 2);
    s->hitbox_bottom = BLOCK_SIZE * 2;
    s->hitbox_left = -THREE_QUARTER_BLOCK_SIZE;
    s->hitbox_right = THREE_QUARTER_BLOCK_SIZE;
    s->samus_collision = SSC_HURTS_SAMUS_SOLID;
    s->frozen_row_offset = 1;
    ai_set_anim(s, MZM_HIVE_IDLE);
    s->draw_order = 5;
    s->health = base_health(c);
    s->pose = HIVE_POSE_PHASE_1;
    s->work0 = 0;

    uint16_t y = s->y, x = s->x;
    uint8_t gfx = s->gfx_slot, slot = s->room_slot;
    ai_spawn_secondary(c, MZM_SSPRITE_HIVE_ROOTS, slot, gfx, y, x, 0);
    ai_spawn_primary(c, MZM_PSPRITE_MELLOW, slot, gfx, (uint16_t)(y + BLOCK_SIZE),
                     (uint16_t)(x - (HALF_BLOCK_SIZE - ONE_SUB_PIXEL)), 0);
    ai_spawn_primary(c, MZM_PSPRITE_MELLOW, slot, gfx, (uint16_t)(y + HALF_BLOCK_SIZE),
                     (uint16_t)(x + QUARTER_BLOCK_SIZE), 0);
    ai_spawn_primary(c, MZM_PSPRITE_MELLOW, slot, gfx, (uint16_t)(y - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE)),
                     (uint16_t)(x - QUARTER_BLOCK_SIZE), 0);
    ai_spawn_primary(c, MZM_PSPRITE_MELLOW, slot, gfx, (uint16_t)(y - (BLOCK_SIZE + HALF_BLOCK_SIZE)),
                     (uint16_t)(x + (HALF_BLOCK_SIZE - ONE_SUB_PIXEL)), 0);
}

/* HiveCountMellows: the Mellows of this hive (same room slot). */
static int count_mellows(const Ctx *c) {
    int count = 0;
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        const MzmSprite *m = &c->f->sprites[i];
        if (m->status & MZM_SPRITE_STATUS_EXISTS && m->samus_collision == SSC_MELLOW &&
            m->room_slot == c->s->room_slot)
            ++count;
    }
    return count;
}

/* HivePhase1/2/3: a Mellow replaces any lost one; under half then a
 * quarter of its health the hive shrinks. */
static void hive_phase(Ctx *c) {
    MzmSprite *s = c->s;
    if (count_mellows(c) < 4)
        ai_spawn_primary(c, MZM_PSPRITE_MELLOW, s->room_slot, s->gfx_slot, s->y, s->x, 0);
    uint8_t next_pose, offset, anim;
    if (s->pose == HIVE_POSE_PHASE_1 && s->health < base_health(c) / 2) {
        next_pose = HIVE_POSE_PHASE_2;
        offset = 2;
        anim = MZM_HIVE_PHASE2;
    } else if (s->pose == HIVE_POSE_PHASE_2 && s->health < base_health(c) / 4) {
        next_pose = HIVE_POSE_PHASE_3;
        offset = 3;
        anim = MZM_HIVE_PHASE3;
    } else {
        return;
    }
    s->frozen_row_offset = offset;
    ai_set_anim(s, anim);
    s->pose = next_pose;
    mzm_sfx_play_not_playing(MZM_SOUND_HIVE_SHRINKING);
    spawn_particle(c);
}

/* HiveDying: the event once no hive is alive, the idle Mellows of this
 * hive flee; a frozen kill (or a special death pose) explodes at once. */
static void hive_dying(Ctx *c) {
    MzmSprite *s = c->s;
    int alive = 0;
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        const MzmSprite *h = &c->f->sprites[i];
        if (h->status & MZM_SPRITE_STATUS_EXISTS && !h->secondary &&
            h->sprite_id == MZM_PSPRITE_HIVE && h->health != 0)
            ++alive;
    }
    const MzmSpriteHooks *hooks = c->f->hooks;
    if (alive == 0 && hooks && hooks->set_event)
        hooks->set_event(hooks->context, MZM_EVENT_THREE_HIVES_DESTROYED);
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        MzmSprite *m = &c->f->sprites[i];
        if (m->status & MZM_SPRITE_STATUS_EXISTS && m->samus_collision == SSC_MELLOW &&
            m->room_slot == s->room_slot && m->pose == MELLOW_POSE_IDLE) {
            m->pose = MELLOW_POSE_FLEEING;
            m->kill_off_screen = true;
        }
    }
    if (s->pose > SPRITE_POSE_DESTROYED || s->work0 != 0) {
        ai_sprite_death_full(c, DEATH_NORMAL, (uint16_t)(s->y + BLOCK_SIZE + EIGHTH_BLOCK_SIZE), s->x,
                             true, MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        return;
    }
    s->pose = HIVE_POSE_DEAD;
    ai_set_anim(s, MZM_HIVE_DYING);
    s->samus_collision = SSC_NONE;
    mzm_sfx_play_not_playing(MZM_SOUND_HIVE_DEAD);
    spawn_particle(c);
}

/* SpriteUtilUpdateSecondarySpriteFreezeTimerOfCurrent for the roots. */
static void freeze_roots(Ctx *c) {
    MzmSprite *s = c->s;
    if (!s->freeze_timer) return;
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        MzmSprite *r = &c->f->sprites[i];
        if (!(r->status & MZM_SPRITE_STATUS_EXISTS) || !r->secondary ||
            r->sprite_id != MZM_SSPRITE_HIVE_ROOTS || r->ram_slot != s->ram_slot ||
            r->freeze_timer >= s->freeze_timer)
            continue;
        r->freeze_timer = s->freeze_timer;
        r->palette_row = (uint8_t)(16 - (r->gfx_slot + r->frozen_row_offset) - 1);
    }
}

/* Hive */
static void hive(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        freeze_roots(c);
        s->work0 = s->freeze_timer;
        return;
    }
    if ((s->isft & 0x7F) > ai_stun_threshold(c) && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: hive_init(c); break;
    case HIVE_POSE_PHASE_1:
    case HIVE_POSE_PHASE_2:
    case HIVE_POSE_PHASE_3: hive_phase(c); break;
    case HIVE_POSE_DEAD: s->ignore_samus_timer = 1; break;
    default: hive_dying(c); break;
    }
}

/* HiveRoots: follow the hive, gone once it shrinks (unless frozen). */
static void hive_roots(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    MzmSprite *h = c->f->sprites && s->ram_slot < c->f->sprite_count ? &c->f->sprites[s->ram_slot] : NULL;
    if (!h || h->secondary || h->sprite_id != MZM_PSPRITE_HIVE) {
        s->status = 0;
        return;
    }
    s->palette_row = h->palette_row;
    if (h->health < c->tables->health[h->sprite_id] / 2 && h->freeze_timer == 0) {
        s->status = 0;
        return;
    }
    if (h->status == 0) {
        s->status = 0;
        return;
    }
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        /* HiveRootsInit */
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->samus_collision = SSC_NONE;
        s->draw_top = 0;
        s->draw_bottom = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->draw_horizontal = (BLOCK_SIZE + QUARTER_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_bottom = s->hitbox_right = PIXEL_SIZE;
        s->pose = HIVE_ROOTS_POSE_IDLE;
        ai_set_anim(s, MZM_HIVE_ROOTS_IDLE);
        s->frozen_row_offset = 1;
    }
    s->y = h->y;
    s->x = h->x;
}

/* The Mellow's "chase" target state: SpriteUtilMakeSpriteFaceSamusDirection
 * and FACING_DOWN toward the top of Samus. */
static void face_samus_top(Ctx *c) {
    MzmSprite *s = c->s;
    ai_face_samus(c);
    if (s->y > (uint16_t)((c->f->samus_y - 1) + c->f->samus_hitbox_top))
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
    else
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
}

static void detected_priority(Ctx *c) {
    if (c->f->world) c->s->bg_priority_override = (uint8_t)(c->f->world->bg1_priority + 1);
}

/* MellowInit: idle around its hive, or (from a swarm) already chasing. */
static void mellow_init(Ctx *c) {
    MzmSprite *s = c->s;
    if (ai_check_event(c, MZM_EVENT_THREE_HIVES_DESTROYED)) {
        s->status = 0;
        return;
    }
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->draw_top = s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
    s->draw_horizontal = THREE_QUARTER_BLOCK_SIZE / PIXEL_SIZE;
    s->hitbox_top = -(QUARTER_BLOCK_SIZE - PIXEL_SIZE);
    s->hitbox_bottom = QUARTER_BLOCK_SIZE - PIXEL_SIZE;
    s->hitbox_left = -HALF_BLOCK_SIZE;
    s->hitbox_right = HALF_BLOCK_SIZE;
    s->adc = 0;
    s->frame = 0;
    s->samus_collision = SSC_MELLOW;
    s->health = base_health(c);
    if (s->room_slot != MELLOW_FROM_SWARM) {
        s->anim = MZM_HIVE_MELLOW_IDLE;
        s->pose = MELLOW_POSE_IDLE;
        s->work3 = (uint8_t)(c->rng * 4);              /* MUL_SHIFT(gSpriteRng, 4) */
        s->work2 = c->rng & 1 ? 0x14 : 0x3C;
        s->draw_order = s->x & 1 ? 3 : 6;
        return;
    }
    s->anim = MZM_HIVE_MELLOW_SAMUS_DETECTED;
    detected_priority(c);
    s->draw_order = 3;
    s->work1 = 0;
    s->work2 = 1;
    s->work0 = 0;
    s->work3 = 1;
    s->x_spawn = c->rng & 3;
    s->pose = MELLOW_POSE_MOVE;
    s->scaling = 0x20;
    face_samus_top(c);
}

/* MellowIdle: hovers on the idle tables, swapping in front of and behind
 * the hive, until Samus comes within 5 blocks vertically and about 4
 * across. */
static void mellow_idle(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t offset = s->work3;
    int16_t movement = offset < 65 ? c->tables->mellow_idle_y[offset] : SHORT_MAX;
    if (movement == SHORT_MAX) {
        movement = c->tables->mellow_idle_y[0];
        offset = 0;
    }
    s->work3 = (uint8_t)(offset + 1);
    s->y = (uint16_t)(s->y + movement);

    offset = s->work2;
    movement = offset < 81 ? c->tables->mellow_idle_x[offset] : SHORT_MAX;
    if (movement == SHORT_MAX) {
        movement = c->tables->mellow_idle_x[0];
        offset = 0;
    }
    s->work2 = (uint8_t)(offset + 1);
    s->x = (uint16_t)(s->x + movement);
    if (s->work2 == 1 || s->work2 == MELLOW_IDLE_X_HALF + 1)
        s->draw_order = s->draw_order == 3 ? 6 : 3;

    if (ai_samus_nslr(c, BLOCK_SIZE * 5, BLOCK_SIZE * 4 + QUARTER_BLOCK_SIZE) != NSLR_OUT_OF_RANGE) {
        s->pose = MELLOW_POSE_MOVE_INIT;
        ai_set_anim(s, MZM_HIVE_MELLOW_SAMUS_DETECTED);
        detected_priority(c);
        s->draw_order = 3;
    }
}

/* MellowFleeing: away along the direction its idle tables last took. */
static void mellow_fleeing(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t rng = c->rng / 4;
    int16_t movement = (int16_t)(rng + EIGHTH_BLOCK_SIZE);
    if (s->work2 < MELLOW_IDLE_X_HALF) s->x = (uint16_t)(s->x + movement);
    else s->x = (uint16_t)(s->x - movement);
    movement = (int16_t)(rng + PIXEL_SIZE);
    if (s->work3 < MELLOW_IDLE_Y_HALF) s->y = (uint16_t)(s->y + movement);
    else s->y = (uint16_t)(s->y - movement);
}

/* MellowMoveInit */
static void mellow_move_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->work1 = 0;
    s->work2 = 1;
    s->work0 = 0;
    s->work3 = 1;
    s->x_spawn = 0;
    s->pose = MELLOW_POSE_MOVE;
    s->scaling = 32;                                   /* CONVERT_SECONDS(.5f + 1.f / 30) */
    face_samus_top(c);
}

/* One axis of MellowMove: accelerate toward the target up to `limit`,
 * brake past it, then turn around (a negative position clamps to 0). */
static bool mellow_axis(uint16_t *position, bool positive, uint16_t target, uint8_t *brake,
                        uint8_t *speed, uint8_t limit) {
    if (positive) {
        if (*brake == 0) {
            if (*position <= target - 4) {
                if (*speed < limit) ++*speed;
                *position = (uint16_t)(*position + (*speed >> 2));
            } else {
                *brake = *speed;
            }
        } else if (--*brake != 0) {
            *position = (uint16_t)(*position + (*brake >> 2));
        } else {
            return true;
        }
        return false;
    }
    uint8_t step;
    if (*brake == 0) {
        if (*position < target + 4) {
            *brake = *speed;
            return false;
        }
        if (*speed < limit) ++*speed;
        step = (uint8_t)(*speed >> 2);
    } else if (--*brake != 0) {
        step = (uint8_t)(*brake >> 2);
    } else {
        return true;
    }
    uint32_t next = (uint32_t)(uint16_t)(*position - step);
    if (next & 0x8000) {
        *brake = 0;
        *position = 0;
        return true;
    }
    *position = (uint16_t)next;
    return false;
}

/* MellowMove: pushes one overlapping Mellow away, then circles Samus
 * around four waypoints (a swarm Mellow circles higher and wider). */
static void mellow_move(Ctx *c) {
    MzmSprite *s = c->s;
    enum { OFFSET = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE };
    uint16_t top = (uint16_t)(s->y - OFFSET), bottom = (uint16_t)(s->y + OFFSET);
    uint16_t left = (uint16_t)(s->x - OFFSET), right = (uint16_t)(s->x + OFFSET);
    for (int i = s->ram_slot + 1; c->f->sprites && i < c->f->sprite_count; ++i) {
        MzmSprite *m = &c->f->sprites[i];
        if (!(m->status & MZM_SPRITE_STATUS_EXISTS) || m->samus_collision != SSC_MELLOW) continue;
        uint16_t my = m->y, mx = m->x;
        if (bottom > (uint16_t)(my - OFFSET) && top < (uint16_t)(my + OFFSET) &&
            right > (uint16_t)(mx - OFFSET) && left < (uint16_t)(mx + OFFSET)) {
            if (m->freeze_timer == 0) {
                m->y = (uint16_t)(s->y > my ? my - PIXEL_SIZE : my + PIXEL_SIZE);
                m->x = (uint16_t)(s->x > mx ? mx - PIXEL_SIZE : mx + PIXEL_SIZE);
            }
            break;
        }
    }

    bool right_facing = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    uint8_t vertical, horizontal;
    uint16_t ty, tx = c->f->samus_x;
    if (s->room_slot == MELLOW_FROM_SWARM) {
        vertical = 0x14;
        horizontal = (uint8_t)(c->rng + 0x1E);
        ty = (uint16_t)((c->f->samus_y - 1) - (c->rng * 4 + 0xDC));
        if (s->x_spawn == 1) {
            ty = (uint16_t)(ty - BLOCK_SIZE * 2);
            tx = (uint16_t)(right_facing ? tx + BLOCK_SIZE * 4 : tx - BLOCK_SIZE * 4);
        } else if (s->x_spawn == 3) {
            ty = (uint16_t)(ty + BLOCK_SIZE / 2);
            tx = (uint16_t)(right_facing ? tx - BLOCK_SIZE * 4 : tx + BLOCK_SIZE * 4);
        }
    } else {
        ty = (uint16_t)((c->f->samus_y - 1) + c->f->samus_hitbox_top);
        vertical = 0x1E;
        horizontal = 0x28;
        if (s->x_spawn == 1) {
            ty = (uint16_t)(ty - BLOCK_SIZE / 2);
            tx = (uint16_t)(right_facing ? tx + (BLOCK_SIZE - BLOCK_SIZE / 4)
                                         : tx - (BLOCK_SIZE - BLOCK_SIZE / 4));
        } else if (s->x_spawn == 3) {
            ty = (uint16_t)(ty + BLOCK_SIZE / 2);
            tx = (uint16_t)(right_facing ? tx - (BLOCK_SIZE - BLOCK_SIZE / 4)
                                         : tx + (BLOCK_SIZE - BLOCK_SIZE / 4));
        }
    }

    if (mellow_axis(&s->x, right_facing, tx, &s->work1, &s->work2, horizontal)) {
        s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
        s->work2 = 1;
        if (++s->x_spawn > 3) s->x_spawn = 0;
    }
    if (mellow_axis(&s->y, s->status & MZM_SPRITE_STATUS_FACING_DOWN, ty, &s->work0, &s->work3,
                    vertical)) {
        s->status ^= MZM_SPRITE_STATUS_FACING_DOWN;
        s->work3 = 1;
    }
    if (--s->scaling == 0) {
        s->scaling = 32;
        ai_sound_onscreen_once(s, MZM_SOUND_MELLOW_MOVING);
    }
}

/* Mellow */
static void mellow(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->damaged) {
        s->damaged = false;
        ai_sound_onscreen_once(s, MZM_SOUND_MELLOW_DAMAGED);
    }
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
    if ((s->isft & 0x7F) > ai_stun_threshold(c) && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: mellow_init(c); break;
    case MELLOW_POSE_IDLE: mellow_idle(c); break;
    case MELLOW_POSE_MOVE_INIT:
        mellow_move_init(c);
        /* fallthrough */
    case MELLOW_POSE_MOVE: mellow_move(c); break;
    case MELLOW_POSE_FLEEING: mellow_fleeing(c); break;
    default:
        ai_sprite_death_full(c, DEATH_NORMAL, s->y, s->x, true, MZM_PE_SPRITE_EXPLOSION_SMALL);
        break;
    }
}

/* MellowSwarm: an invisible spawner keeping a number of Mellows (5, or
 * 3 to 15 by Samus's energy) diving from above the screen; once enough
 * exist it waits four seconds after the count drops to 19 or fewer. */
static void mellow_swarm(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        if (ai_check_event(c, MZM_EVENT_THREE_HIVES_DESTROYED)) {
            s->status = 0;
            return;
        }
        s->status |= MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->hitbox_top = s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_bottom = s->hitbox_right = PIXEL_SIZE;
        s->draw_top = s->draw_bottom = s->draw_horizontal = 1;
        s->samus_collision = SSC_NONE;
        ai_set_anim(s, MZM_HIVE_MELLOW_IDLE);
        s->pose = MELLOW_SWARM_POSE_IDLE;
        uint16_t camera_x = c->f->world ? c->f->world->camera_x : 0;
        if (c->f->samus_x / PIXEL_SIZE - camera_x / PIXEL_SIZE > SCREEN_SIZE_X_SUB_PIXEL / PIXEL_SIZE / 2)
            s->status |= MZM_SPRITE_STATUS_X_FLIP;
        s->y_spawn = 0xF0;
        if (s->sprite_id == MZM_PSPRITE_MELLOW_SWARM_HEALTH_BASED) {
            int energy = c->f->equipment ? c->f->equipment->energy : 0;
            s->work2 = energy >= 400 ? 15 : energy >= 300 ? 12 : energy >= 200 ? 9 :
                       energy >= 100 ? 6 : 3;
        } else {
            s->work2 = 5;
        }
        return;
    }
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN && s->y_spawn != 0) {
        s->y_spawn--;
        return;
    }
    int count = 0;
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i)
        if (c->f->sprites[i].status & MZM_SPRITE_STATUS_EXISTS &&
            c->f->sprites[i].samus_collision == SSC_MELLOW)
            ++count;
    if (!(s->status & MZM_SPRITE_STATUS_FACING_DOWN)) {
        if (count >= s->work2) {
            s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
            return;
        }
    } else if (count <= 0x13) {
        s->y_spawn = 0xF0;
    } else {
        return;
    }
    uint16_t x = s->status & MZM_SPRITE_STATUS_X_FLIP ? (uint16_t)(c->f->samus_x + c->rng * 0x20)
                                                      : (uint16_t)(c->f->samus_x - c->rng * 0x20);
    if (x & 0x8000) x = 0;
    uint16_t camera_y = c->f->world ? c->f->world->camera_y : 0;
    uint16_t y = (uint16_t)(camera_y - (c->rng * 2 + 0x10));
    ai_spawn_primary(c, MZM_PSPRITE_MELLOW, MELLOW_FROM_SWARM, s->gfx_slot, y, x, 0);
}

void ai_hive(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->secondary) {
        if (s->sprite_id == MZM_SSPRITE_HIVE_ROOTS) hive_roots(c);
        else s->status = 0;
        return;
    }
    switch (s->sprite_id) {
    case MZM_PSPRITE_HIVE: hive(c); break;
    case MZM_PSPRITE_MELLOW: mellow(c); break;
    default: mellow_swarm(c); break;
    }
}
