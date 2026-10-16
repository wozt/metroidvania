/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Deorem (src/sprites_ai/deorem.c): the two-location worm
 * encounter, its exposed missile-vulnerable eye, body columns, thorn
 * volleys, timed retreat, death and Charge Beam reward. The body geometry
 * follows the original room-relative layout while the long per-segment
 * burrowing choreography is intentionally compacted into the same visible
 * phases. */
#include "mzm_blocks.h"
#include "mzm_screen_shake.h"
#include "mzm_sprite_ai.h"

#include <string.h>

enum {
    DEOREM_WAITING = 0x8,
    DEOREM_SPAWN_DOWN = 0x9,
    DEOREM_SPAWN_DELAY = 0x22,
    DEOREM_SPAWN_UP = 0x23,
    DEOREM_HEAD_DELAY = 0x24,
    DEOREM_HEAD_EMERGES = 0x25,
    DEOREM_MAIN = 0x26,
    DEOREM_RETRACTING = 0x27,
    DEOREM_THORNS_DOWN = 0x28,
    DEOREM_THORNS_UP = 0x29,
    DEOREM_AFTER_THORNS = 0x2A,
    DEOREM_ATTACKING = 0x2B,
    DEOREM_WAIT_TO_LEAVE = 0x42,
    DEOREM_START_LEAVING = 0x43,
    DEOREM_LEAVING = 0x44,
    DEOREM_LEAVING_IN_GROUND = 0x45,
    DEOREM_CALL_CHARGE_BEAM = 0x60,
    DEOREM_DYING = 0x67,
    DEOREM_DEATH = 0x68,
    EYE_IDLE = 0x9,
    EYE_DYING = 0x67,
    EYE_REWARD = 0x68,
    THORN_SPAWNING = 0x9,
    THORN_MOVING = 0x23,
    WIDTH = BLOCK_SIZE * 13,
    HEIGHT = BLOCK_SIZE * 10,
    HEAD_Y_OFFSET = BLOCK_SIZE * 2 + HALF_BLOCK_SIZE,
    SEGMENT_SPACING = BLOCK_SIZE + HALF_BLOCK_SIZE + PIXEL_SIZE,
    SEGMENT_PADDING = SEGMENT_SPACING + BLOCK_SIZE + PIXEL_SIZE,
    EYE_Y_OFFSET = HALF_BLOCK_SIZE - PIXEL_SIZE,
    EYE_X_OFFSET = PIXEL_SIZE,
    MAX_HEALTH = MISSILE_DAMAGE * 3,
};

enum {
    SEGMENT_DOWN_JUNCTION, SEGMENT_DOWN_1, SEGMENT_DOWN_2, SEGMENT_DOWN_3,
    SEGMENT_DOWN_4, SEGMENT_DOWN_5, SEGMENT_UP_JUNCTION, SEGMENT_UP_1,
    SEGMENT_UP_2, SEGMENT_UP_3, SEGMENT_UP_4, SEGMENT_UP_5,
    SEGMENT_NECK_JUNCTION, SEGMENT_NECK_1, SEGMENT_NECK_2,
};

static MzmSprite *sprite_slot(const Ctx *c, uint8_t slot) {
    if (!c->f->sprites || slot >= c->f->sprite_count) return NULL;
    return &c->f->sprites[slot];
}

static MzmSprite *parent(const Ctx *c) { return sprite_slot(c, c->s->ram_slot); }

static void set_event(const Ctx *c, uint8_t event) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->set_event) h->set_event(h->context, event);
}

static void clear_event(const Ctx *c, uint8_t event) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->clear_event) h->clear_event(h->context, event);
}

static void door_timer(const Ctx *c, int8_t timer) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->door_unlock_timer) h->door_unlock_timer(h->context, timer);
}

static void arena_walls(const Ctx *c, uint8_t caa) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (!h || !h->apply_caa || !c->f->globals) return;
    uint16_t y = c->f->globals->boss_work1;
    uint16_t left = c->f->globals->boss_work2;
    uint16_t right = (uint16_t)(left + WIDTH);
    for (unsigned row = 0; row < 8; ++row) {
        h->apply_caa(h->context, (uint16_t)(y + row * BLOCK_SIZE), left, caa);
        h->apply_caa(h->context, (uint16_t)(y + row * BLOCK_SIZE), right, caa);
    }
}

static void remove_owned(const Ctx *c, bool keep_eye) {
    if (!c->f->sprites) return;
    for (int i = 0; i < c->f->sprite_count; ++i) {
        MzmSprite *n = &c->f->sprites[i];
        if (!n->secondary || n->ram_slot != c->s->ram_slot) continue;
        if (keep_eye && n->sprite_id == MZM_SSPRITE_DEOREM_EYE) continue;
        n->status = 0;
    }
}

static MzmSprite *spawn_primary(Ctx *c, uint8_t id, uint16_t y, uint16_t x) {
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        MzmSprite *n = &c->f->sprites[i];
        if (n->status & MZM_SPRITE_STATUS_EXISTS) continue;
        memset(n, 0, sizeof *n);
        n->status = MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_ONSCREEN |
                    MZM_SPRITE_STATUS_NOT_DRAWN;
        n->sprite_id = id;
        n->gfx_slot = c->s->gfx_slot;
        n->y = y;
        n->x = x;
        n->draw_order = 4;
        n->ignore_samus_timer = 1;
        n->ram_slot = (uint8_t)i;
        return n;
    }
    return NULL;
}

static void spawn_charge_beam(Ctx *c, uint16_t y, uint16_t x) {
    if (c->f->equipment && c->f->equipment->items & MZM_ITEM_CHARGE_BEAM) return;
    spawn_primary(c, MZM_PSPRITE_CHARGE_BEAM, y, x);
    mzm_sfx_play(MZM_SOUND_DEOREM_SPAWNING_CHARGE_BEAM);
}

static void spawn_segment(Ctx *c, uint8_t part, uint16_t y, uint16_t x) {
    ai_spawn_secondary(c, MZM_SSPRITE_DEOREM_SEGMENT, part, c->s->gfx_slot, y, x, 0);
}

static void spawn_body_columns(Ctx *c) {
    uint16_t y = c->s->y;
    uint16_t down_x = c->s->x_spawn;
    uint16_t up_x = (uint16_t)(down_x + (c->f->globals->boss_work3 ? -WIDTH : WIDTH));
    for (uint8_t i = 0; i < 6; ++i) {
        spawn_segment(c, i, (uint16_t)(y - SEGMENT_PADDING - i * SEGMENT_SPACING), down_x);
        spawn_segment(c, (uint8_t)(SEGMENT_UP_JUNCTION + i),
                      (uint16_t)(y + SEGMENT_PADDING + i * SEGMENT_SPACING), up_x);
    }
}

static int spawn_neck_and_eye(Ctx *c) {
    int previous = -1;
    for (uint8_t part = SEGMENT_NECK_JUNCTION; part <= SEGMENT_NECK_2; ++part) {
        int slot = ai_spawn_secondary(c, MZM_SSPRITE_DEOREM_SEGMENT, part, c->s->gfx_slot,
                                      c->s->y, c->s->x, 0);
        if (slot >= 0) {
            MzmSprite *n = sprite_slot(c, (uint8_t)slot);
            n->work0 = previous < 0 ? c->s->ram_slot : (uint8_t)previous;
            previous = slot;
        }
    }
    int eye = ai_spawn_secondary(c, MZM_SSPRITE_DEOREM_EYE, 0, c->s->gfx_slot,
                                 (uint16_t)(c->s->y - EYE_Y_OFFSET),
                                 (uint16_t)(c->s->x - EYE_X_OFFSET), 0);
    if (eye >= 0) c->s->work3 = (uint8_t)eye;
    return eye;
}

static uint8_t eye_open_time(const Ctx *c) {
    return c->f->difficulty == 0 ? 120 : c->f->difficulty == 2 ? 40 : 80;
}

static bool eye_timer_done(Ctx *c) {
    MzmSprite *eye = sprite_slot(c, c->s->work3);
    return !eye || !(eye->status & MZM_SPRITE_STATUS_EXISTS) || eye->y_spawn == 0;
}

static void init_head(Ctx *c) {
    MzmSprite *s = c->s;
    bool first = s->sprite_id == MZM_PSPRITE_DEOREM_FIRST_LOCATION;
    bool visited = ai_check_event(c, first ? MZM_EVENT_DEOREM_FIRST_LOCATION
                                           : MZM_EVENT_DEOREM_SECOND_LOCATION);
    if (visited) {
        bool reward = first ? ai_check_event(c, MZM_EVENT_DEOREM_SECOND_LOCATION) &&
                                  !ai_check_event(c, MZM_EVENT_DEOREM_KILLED_AT_SECOND_LOCATION)
                            : ai_check_event(c, MZM_EVENT_DEOREM_FIRST_LOCATION) &&
                                  ai_check_event(c, MZM_EVENT_DEOREM_KILLED_AT_SECOND_LOCATION);
        if (reward && (!c->f->equipment || !(c->f->equipment->items & MZM_ITEM_CHARGE_BEAM))) {
            s->pose = DEOREM_CALL_CHARGE_BEAM;
            s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
        } else s->status = 0;
        return;
    }
    if (!c->f->globals) { s->status = 0; return; }
    c->f->globals->boss_work1 = (uint16_t)(s->y - HALF_BLOCK_SIZE);
    c->f->globals->boss_work2 = s->x;
    s->y = (uint16_t)(s->y - HEAD_Y_OFFSET);
    s->hitbox_top = -(BLOCK_SIZE + HALF_BLOCK_SIZE);
    s->hitbox_bottom = BLOCK_SIZE;
    s->hitbox_left = -(BLOCK_SIZE + HALF_BLOCK_SIZE);
    s->hitbox_right = BLOCK_SIZE + HALF_BLOCK_SIZE;
    s->draw_top = s->draw_bottom = BLOCK_SIZE * 2 / PIXEL_SIZE;
    s->draw_horizontal = BLOCK_SIZE * 3 / PIXEL_SIZE;
    ai_set_anim(s, MZM_DEOREM_CLOSED_SLOW);
    s->samus_collision = SSC_HURTS_SAMUS;
    s->draw_order = 10;
    s->health = c->tables->health[s->sprite_id];
    s->pose = DEOREM_WAITING;
}

static void waiting(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t center = (uint16_t)(c->f->globals->boss_work2 + WIDTH / 2);
    s->work0++;
    bool has_missiles = c->f->equipment && c->f->equipment->max_missiles > 0;
    if (!has_missiles || c->f->samus_x < center - HALF_BLOCK_SIZE ||
        c->f->samus_x > center + HALF_BLOCK_SIZE) {
        if ((uint8_t)(s->work0 % 32) == 0) {
            mzm_screen_shake_start_vertical(10, 0x81);
            ai_debris_init(c, (uint8_t)(5 + (c->rng & 3)), c->f->globals->boss_work1,
                           (uint16_t)(center + (int8_t)c->rng * 2));
            mzm_sfx_play(MZM_SOUND_190);
        }
        return;
    }
    c->f->globals->lock_screen = true;
    c->f->globals->lock_y = c->f->samus_y;
    c->f->globals->lock_x = center;
    c->f->globals->boss_work3 = c->f->samus_h_moving == 2;
    if (c->f->globals->boss_work3) s->x = (uint16_t)(s->x + WIDTH);
    s->x_spawn = s->x;
    s->y_spawn = s->y;
    s->pose = DEOREM_SPAWN_DOWN;
    s->work0 = 46;
    spawn_body_columns(c);
    arena_walls(c, MZM_CAA_MAKE_SOLID_GRIPPABLE);
    door_timer(c, 1);
    mzm_screen_shake_start_vertical(40, 0x81);
    mzm_sfx_play(MZM_SOUND_DEOREM_MOVING);
}

static void spawn_sequence(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case DEOREM_SPAWN_DOWN:
        s->y += QUARTER_BLOCK_SIZE;
        if (--s->work0 == 0) {
            s->pose = DEOREM_SPAWN_DELAY;
            s->work0 = 10;
            s->status |= MZM_SPRITE_STATUS_Y_FLIP;
            s->x = (uint16_t)(s->x + (c->f->globals->boss_work3 ? -WIDTH : WIDTH));
        }
        break;
    case DEOREM_SPAWN_DELAY:
        if (--s->work0 == 0) {
            s->pose = DEOREM_SPAWN_UP;
            s->work0 = 50;
            s->y = (uint16_t)(s->y_spawn + HEIGHT + HEAD_Y_OFFSET - PIXEL_SIZE);
        }
        break;
    case DEOREM_SPAWN_UP:
        s->y -= QUARTER_BLOCK_SIZE;
        if (--s->work0 == 0) {
            s->pose = DEOREM_HEAD_DELAY;
            s->work0 = 60;
            s->status = (uint16_t)((s->status & ~MZM_SPRITE_STATUS_Y_FLIP) |
                                   MZM_SPRITE_STATUS_NOT_DRAWN);
            s->x = (uint16_t)(s->x + (c->f->globals->boss_work3 ? WIDTH / 2 : -WIDTH / 2));
        }
        break;
    case DEOREM_HEAD_DELAY:
        if (--s->work0 == 0) {
            s->pose = DEOREM_HEAD_EMERGES;
            s->y = s->y_spawn;
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
            s->work0 = 11;
            if (spawn_neck_and_eye(c) < 0) s->status = 0;
        }
        break;
    default:
        if (s->y < s->y_spawn + HEAD_Y_OFFSET) { s->y += EIGHTH_BLOCK_SIZE; break; }
        if (s->work0 && --s->work0) break;
        s->pose = DEOREM_MAIN;
        s->work0 = eye_open_time(c);
        s->hitbox_bottom = 0;
        ai_set_anim(s, MZM_DEOREM_OPENING);
        MzmSprite *eye = sprite_slot(c, s->work3);
        if (eye) eye->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        mzm_sfx_play(MZM_SOUND_DEOREM_OPENING_JAW);
        break;
    }
}

static void begin_retreat(Ctx *c) {
    MzmSprite *s = c->s;
    s->pose = DEOREM_WAIT_TO_LEAVE;
    s->work0 = 30;
    ai_set_anim(s, MZM_DEOREM_CLOSING);
    s->hitbox_bottom = BLOCK_SIZE;
    MzmSprite *eye = sprite_slot(c, s->work3);
    if (eye) eye->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    mzm_sfx_play(MZM_SOUND_DEOREM_CLOSING_JAW);
}

static void main_attack(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSprite *eye = sprite_slot(c, s->work3);
    if (eye_timer_done(c)) { begin_retreat(c); return; }
    if (s->anim == MZM_DEOREM_OPENING && ai_animation_ending(c, 1))
        ai_set_anim(s, MZM_DEOREM_OPENED_FAST);
    if (s->work0 > 0 && --s->work0) {
        if (s->work0 == 22) {
            ai_set_anim(s, MZM_DEOREM_CLOSING);
            s->hitbox_bottom = BLOCK_SIZE;
            if (eye) eye->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        }
        return;
    }
    s->pose = DEOREM_ATTACKING;
    s->work0 = 30;
    ai_set_anim(s, MZM_DEOREM_WARNING);
    ai_face_samus(c);
    mzm_sfx_play(MZM_SOUND_DEOREM_WARNING);
}

static void attack(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->work0 && --s->work0) return;
    ai_set_anim(s, MZM_DEOREM_GOING_DOWN);
    uint16_t bottom = (uint16_t)(s->y_spawn + HEIGHT - BLOCK_SIZE * 2);
    if (s->y < bottom) {
        s->y += EIGHTH_BLOCK_SIZE;
        int16_t dx = (int16_t)(c->f->samus_x - s->x);
        if (dx > PIXEL_SIZE) s->x += PIXEL_SIZE / 2;
        else if (dx < -PIXEL_SIZE) s->x -= PIXEL_SIZE / 2;
        return;
    }
    s->pose = DEOREM_RETRACTING;
    s->work0 = 30;
    ai_set_anim(s, MZM_DEOREM_OPENING);
    s->hitbox_bottom = 0;
    MzmSprite *eye = sprite_slot(c, s->work3);
    if (eye) eye->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
}

static void retract(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->work0 && --s->work0) return;
    uint16_t top = (uint16_t)(s->y_spawn + HEAD_Y_OFFSET);
    if (s->y > top) { s->y -= EIGHTH_BLOCK_SIZE; return; }
    if (eye_timer_done(c)) { begin_retreat(c); return; }
    s->pose = (c->rng & 1) ? DEOREM_THORNS_DOWN : DEOREM_THORNS_UP;
    s->work0 = 255;
    s->rotation = 0;
    ai_set_anim(s, MZM_DEOREM_OPENED_FAST);
}

static void thorn_phase(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->work0) --s->work0;
    if (s->work0 == 0) {
        s->pose = DEOREM_AFTER_THORNS;
        s->work0 = 60;
        ai_set_anim(s, MZM_DEOREM_CLOSED_FAST);
    }
}

static void after_thorns(Ctx *c) {
    if (c->s->work0 && --c->s->work0) return;
    c->s->pose = DEOREM_MAIN;
    c->s->work0 = eye_open_time(c);
    ai_set_anim(c->s, MZM_DEOREM_OPENING);
    MzmSprite *eye = sprite_slot(c, c->s->work3);
    if (eye) eye->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
}

static void retreat(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == DEOREM_WAIT_TO_LEAVE) {
        if (s->work0 && --s->work0) return;
        s->pose = DEOREM_START_LEAVING;
        s->work0 = 60;
        ai_set_anim(s, MZM_DEOREM_CLOSED_SLOW);
        return;
    }
    if (s->pose == DEOREM_START_LEAVING) {
        if (s->y > s->y_spawn - BLOCK_SIZE) { s->y -= PIXEL_SIZE; return; }
        if (s->work0 && --s->work0) return;
        arena_walls(c, MZM_CAA_REMOVE_SOLID);
        door_timer(c, -1);
        c->f->globals->lock_screen = false;
        if (s->sprite_id == MZM_PSPRITE_DEOREM_FIRST_LOCATION) {
            set_event(c, MZM_EVENT_DEOREM_FIRST_LOCATION);
            clear_event(c, MZM_EVENT_DEOREM_SECOND_LOCATION);
        } else {
            clear_event(c, MZM_EVENT_DEOREM_FIRST_LOCATION);
            set_event(c, MZM_EVENT_DEOREM_SECOND_LOCATION);
        }
        remove_owned(c, false);
        s->pose = DEOREM_LEAVING;
        mzm_sfx_play(MZM_SOUND_DEOREM_LEAVING);
        return;
    }
    if (s->pose == DEOREM_LEAVING) {
        s->y += QUARTER_BLOCK_SIZE;
        if (s->y >= s->y_spawn + HEIGHT + BLOCK_SIZE) {
            s->pose = DEOREM_LEAVING_IN_GROUND;
            s->work0 = 240;
            s->status |= MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        }
        return;
    }
    if (s->work0) --s->work0;
    if (s->work0 == 0) s->status = 0;
}

static void dying(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_DESTROYED) {
        s->pose = DEOREM_DYING;
        s->work0 = 112;
        s->ignore_samus_timer = 1;
        ai_set_anim(s, MZM_DEOREM_DYING);
        arena_walls(c, MZM_CAA_REMOVE_SOLID);
        door_timer(c, -1);
        c->f->globals->lock_screen = false;
        set_event(c, MZM_EVENT_DEOREM_FIRST_LOCATION);
        set_event(c, MZM_EVENT_DEOREM_SECOND_LOCATION);
        if (s->sprite_id == MZM_PSPRITE_DEOREM_SECOND_LOCATION)
            set_event(c, MZM_EVENT_DEOREM_KILLED_AT_SECOND_LOCATION);
        remove_owned(c, true);
        mzm_sfx_play(MZM_SOUND_DEOREM_DYING);
    }
    if (s->pose == DEOREM_DYING) {
        s->ignore_samus_timer = 1;
        if (s->y < s->y_spawn + BLOCK_SIZE * 7 + QUARTER_BLOCK_SIZE) s->y += PIXEL_SIZE;
        if (s->work0 && --s->work0) return;
        s->pose = DEOREM_DEATH;
        s->work0 = 60;
        return;
    }
    if (s->work0 && --s->work0) return;
    ai_particle_set(c, s->y, (uint16_t)(s->x + BLOCK_SIZE),
                    MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
    s->status = 0;
}

static void head(Ctx *c) {
    switch (c->s->pose) {
    case SPRITE_POSE_UNINITIALIZED: init_head(c); break;
    case DEOREM_WAITING: waiting(c); break;
    case DEOREM_SPAWN_DOWN: case DEOREM_SPAWN_DELAY: case DEOREM_SPAWN_UP:
    case DEOREM_HEAD_DELAY: case DEOREM_HEAD_EMERGES: spawn_sequence(c); break;
    case DEOREM_MAIN: main_attack(c); break;
    case DEOREM_ATTACKING: attack(c); break;
    case DEOREM_RETRACTING: retract(c); break;
    case DEOREM_THORNS_DOWN: case DEOREM_THORNS_UP: thorn_phase(c); break;
    case DEOREM_AFTER_THORNS: after_thorns(c); break;
    case DEOREM_WAIT_TO_LEAVE: case DEOREM_START_LEAVING: case DEOREM_LEAVING:
    case DEOREM_LEAVING_IN_GROUND: retreat(c); break;
    case SPRITE_POSE_DESTROYED: case DEOREM_DYING: case DEOREM_DEATH: dying(c); break;
    case DEOREM_CALL_CHARGE_BEAM:
        spawn_charge_beam(c, (uint16_t)(c->s->y + BLOCK_SIZE * 3 - QUARTER_BLOCK_SIZE),
                          (uint16_t)(c->s->x + WIDTH / 2));
        c->s->status = 0;
        break;
    }
}

static void init_segment(Ctx *c) {
    MzmSprite *s = c->s;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->samus_collision = SSC_HURTS_SAMUS;
    s->health = 1;
    s->hitbox_top = -BLOCK_SIZE;
    s->hitbox_bottom = BLOCK_SIZE;
    s->hitbox_left = -(BLOCK_SIZE + HALF_BLOCK_SIZE);
    s->hitbox_right = BLOCK_SIZE + HALF_BLOCK_SIZE;
    s->draw_top = s->draw_bottom = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    s->draw_horizontal = BLOCK_SIZE * 2 / PIXEL_SIZE;
    s->anim = s->room_slot == SEGMENT_DOWN_JUNCTION || s->room_slot == SEGMENT_UP_JUNCTION ||
                      s->room_slot == SEGMENT_NECK_JUNCTION
                  ? MZM_DEOREM_SEGMENT_JUNCTION : MZM_DEOREM_SEGMENT_MIDDLE;
    s->frame = (uint8_t)((s->room_slot + 1) & 3);
    s->pose = EYE_IDLE;
    if (s->room_slot >= SEGMENT_UP_JUNCTION && s->room_slot <= SEGMENT_UP_5)
        s->status |= MZM_SPRITE_STATUS_Y_FLIP;
}

static void spawn_thorn_from_segment(Ctx *c, MzmSprite *p) {
    MzmSprite *s = c->s;
    unsigned timer = p->work0;
    uint8_t index;
    bool down = s->room_slot >= SEGMENT_DOWN_1 && s->room_slot <= SEGMENT_DOWN_4;
    bool up = s->room_slot >= SEGMENT_UP_1 && s->room_slot <= SEGMENT_UP_4;
    if (p->pose == DEOREM_THORNS_DOWN && down) index = (uint8_t)(s->room_slot - SEGMENT_DOWN_1);
    else if (p->pose == DEOREM_THORNS_UP && up) index = (uint8_t)(SEGMENT_UP_4 - s->room_slot);
    else return;
    static const uint8_t times[] = {241, 181, 101, 21};
    if (timer != times[index]) return;
    bool right = c->f->globals->boss_work3 ? !down : down;
    uint16_t x = (uint16_t)(s->x + (right ? BLOCK_SIZE + HALF_BLOCK_SIZE
                                                : -(BLOCK_SIZE + HALF_BLOCK_SIZE)));
    uint16_t status = right ? MZM_SPRITE_STATUS_X_FLIP : 0;
    ai_spawn_secondary(c, MZM_SSPRITE_DEOREM_THORN, index, s->gfx_slot, s->y, x, status);
    mzm_sfx_play(MZM_SOUND_DEOREM_THROWING_THORNS);
}

static void segment(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSprite *p = parent(c);
    if (!p || !(p->status & MZM_SPRITE_STATUS_EXISTS)) { s->status = 0; return; }
    if (s->pose == SPRITE_POSE_UNINITIALIZED) init_segment(c);
    if (p->pose >= SPRITE_POSE_DESTROYED) { s->status = 0; return; }
    if (s->room_slot >= SEGMENT_NECK_JUNCTION) {
        unsigned n = s->room_slot - SEGMENT_NECK_JUNCTION + 1u;
        s->y = (uint16_t)(p->y - SEGMENT_PADDING - n * SEGMENT_SPACING);
        s->x = p->x;
        return;
    }
    bool down = s->room_slot <= SEGMENT_DOWN_5;
    unsigned n = down ? s->room_slot : s->room_slot - SEGMENT_UP_JUNCTION;
    uint16_t base_x = down ? p->x_spawn
                           : (uint16_t)(p->x_spawn + (c->f->globals->boss_work3 ? -WIDTH : WIDTH));
    int16_t wave = c->tables->deorem_segment_x[s->work2 < 32 ? s->work2 : 0];
    s->work2 = (uint8_t)((s->work2 + 1) % 32);
    s->x = (uint16_t)(base_x + wave * (int)(n + 1));
    s->y = down ? (uint16_t)(p->y_spawn - SEGMENT_PADDING - n * SEGMENT_SPACING)
                : (uint16_t)(p->y_spawn + HEIGHT + SEGMENT_PADDING + n * SEGMENT_SPACING);
    spawn_thorn_from_segment(c, p);
}

static void init_eye(Ctx *c) {
    MzmSprite *s = c->s;
    s->status |= MZM_SPRITE_STATUS_ROTATION_SCALING_WHOLE;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->scaling = 256;
    s->draw_order = 11;
    s->health = c->tables->secondary_health[s->sprite_id];
    s->hitbox_top = -BLOCK_SIZE;
    s->hitbox_bottom = THREE_QUARTER_BLOCK_SIZE;
    s->hitbox_left = -(HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
    s->hitbox_right = HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
    s->frozen_row_offset = 4;
    s->samus_collision = SSC_NONE;
    ai_set_anim(s, MZM_DEOREM_EYE_PULSING);
    s->pose = EYE_IDLE;
    s->y_spawn = ai_check_event(c, MZM_EVENT_DEOREM_FIRST_LOCATION) ||
                         ai_check_event(c, MZM_EVENT_DEOREM_SECOND_LOCATION) ? 1800 : 900;
}

static void eye(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSprite *p = parent(c);
    s->ignore_samus_timer = 1;
    if (!p || !(p->status & MZM_SPRITE_STATUS_EXISTS)) {
        if (s->pose != EYE_REWARD) {
            s->status = 0;
            return;
        }
        s->rotation += 12;
        s->y -= ONE_SUB_PIXEL;
        if (s->work1 && --s->work1) return;
        spawn_charge_beam(c, s->y, s->x);
        s->status = 0;
        return;
    }
    if (s->pose == SPRITE_POSE_UNINITIALIZED) init_eye(c);
    if (s->pose == SPRITE_POSE_DESTROYED) {
        p->pose = SPRITE_POSE_DESTROYED;
        s->pose = EYE_DYING;
        s->work1 = 120;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        ai_set_anim(s, MZM_DEOREM_EYE_IDLE);
    }
    s->y = (uint16_t)(p->y - EYE_Y_OFFSET);
    s->x = (uint16_t)(p->x - EYE_X_OFFSET);
    if (s->pose == EYE_DYING) {
        s->rotation += 8;
        if (s->work1 && --s->work1) return;
        s->pose = EYE_REWARD;
        s->work1 = 60;
        return;
    }
    if (s->pose == EYE_REWARD) {
        s->rotation += 12;
        s->y -= ONE_SUB_PIXEL;
        if (s->work1 && --s->work1) return;
        spawn_charge_beam(c, s->y, s->x);
        s->status = 0;
        return;
    }
    int16_t dx = (int16_t)(c->f->samus_x - s->x);
    int16_t dy = (int16_t)(c->f->samus_y - s->y);
    uint16_t target = dx >= 0 ? (dy >= 0 ? 32 : 224) : (dy >= 0 ? 96 : 160);
    int16_t delta = (int16_t)(target - s->rotation);
    s->rotation = (uint16_t)(s->rotation + (delta > 0 ? 1 : delta < 0 ? -1 : 0));
    if ((s->health != MAX_HEALTH || ai_check_event(c, MZM_EVENT_DEOREM_FIRST_LOCATION) ||
         ai_check_event(c, MZM_EVENT_DEOREM_SECOND_LOCATION)) && s->y_spawn)
        s->y_spawn--;
    if (s->damaged && s->health <= MAX_HEALTH / 3) p->absolute_palette_row = 2;
}

static void thorn(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->status |= MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->scaling = 256;
        s->kill_off_screen = true;
        s->hitbox_top = -(HALF_BLOCK_SIZE - PIXEL_SIZE);
        s->hitbox_bottom = HALF_BLOCK_SIZE - PIXEL_SIZE;
        s->hitbox_left = -(QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        s->hitbox_right = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->samus_collision = SSC_HURTS_SAMUS;
        s->health = c->tables->secondary_health[s->sprite_id];
        ai_set_anim(s, MZM_DEOREM_THORN_IDLE);
        s->pose = THORN_SPAWNING;
        s->work3 = 20;
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
            s->status = (uint16_t)((s->status & ~MZM_SPRITE_STATUS_X_FLIP) |
                                   MZM_SPRITE_STATUS_FACING_RIGHT);
            s->rotation = 128;
        }
        s->y -= EIGHTH_BLOCK_SIZE + PIXEL_SIZE;
    }
    if (s->pose == THORN_SPAWNING) {
        s->rotation = (uint16_t)(s->rotation + (s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? 32 : -32));
        if (s->work3 && --s->work3) return;
        s->pose = THORN_MOVING;
        s->work3 = 0;
        mzm_sfx_play(MZM_SOUND_DEOREM_THORN_EJECTING);
    }
    if (s->pose == THORN_MOVING) {
        unsigned i = s->work3 < 40 ? s->work3 : 39;
        int16_t yv = c->tables->deorem_thorn_y[i];
        if (yv == SHORT_MAX) yv = c->tables->deorem_thorn_y[i - 1];
        else if (s->work3 < 39) s->work3++;
        s->y = (uint16_t)(s->y + yv);
        s->x = (uint16_t)(s->x + (s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? PIXEL_SIZE : -PIXEL_SIZE));
        s->rotation = (uint16_t)(s->rotation + (s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? 16 : -16));
        if (i > 32 && ai_collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE), s->x) != MZM_COLLISION_AIR)
            s->status = 0;
    }
}

void ai_deorem(Ctx *c) {
    if (!c->s->secondary) head(c);
    else if (c->s->sprite_id == MZM_SSPRITE_DEOREM_SEGMENT) segment(c);
    else if (c->s->sprite_id == MZM_SSPRITE_DEOREM_EYE) eye(c);
    else thorn(c);
}
