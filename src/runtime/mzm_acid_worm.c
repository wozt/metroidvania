/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Acid Worm (src/sprites_ai/acid_worm.c): zipline-triggered
 * entrance, segmented body and weak point, hooked and spit attacks, acid
 * movement, arena destruction and persistent death event. */
#include "mzm_blocks.h"
#include "mzm_samus.h"
#include "mzm_screen_shake.h"
#include "mzm_sprite_ai.h"

enum {
    AW_CHECK_ZIPLINE = 0x1, AW_IDLE_INIT = 0x8, AW_IDLE = 0x9, AW_EXTENDED = 0xF,
    AW_WARNING = 0x23, AW_EXTENDING = 0x25, AW_RETRACTING = 0x27,
    AW_RAISE_ACID = 0x29, AW_LOWER_ACID = 0x2B, AW_CHECK_SPAWN = 0x42,
    AW_SPAWN_EXTEND = 0x43, AW_SPAWN_TOP = 0x45, AW_SPAWN_RETRACT = 0x47,
    AW_DYING_INIT = 0x62, AW_DYING_FLASH = 0x67, AW_DYING = 0x68,
    AW_PART_MOVING = 0x43, AW_PART_IDLE = 0x9, AW_PART_DYING = 0x67,
    AW_SPIT_MOVING = 0x9, AW_SPIT_EXPLODING = 0x43,
    AW_PARTS = 9, AW_WEAK_POINT = 1, AW_MAX_EXTENSION = BLOCK_SIZE * 4,
};

static MzmSprite *slot(const Ctx *c, uint8_t index) {
    return c->f->sprites && index < c->f->sprite_count ? &c->f->sprites[index] : NULL;
}

static void set_event(const Ctx *c, uint8_t event) {
    if (c->f->hooks && c->f->hooks->set_event)
        c->f->hooks->set_event(c->f->hooks->context, event);
}

static void door_timer(const Ctx *c, int8_t timer) {
    if (c->f->hooks && c->f->hooks->door_unlock_timer)
        c->f->hooks->door_unlock_timer(c->f->hooks->context, timer);
}

static void move_acid(const Ctx *c, int16_t delta) {
    if (c->f->hooks && c->f->hooks->move_effect_y)
        c->f->hooks->move_effect_y(c->f->hooks->context, delta);
}

static void change_pair(const Ctx *c, uint16_t y, uint16_t x) {
    if (!c->f->hooks || !c->f->hooks->apply_caa) return;
    c->f->hooks->apply_caa(c->f->hooks->context, y, (uint16_t)(x - HALF_BLOCK_SIZE),
                           MZM_CAA_REMOVE_SOLID);
    c->f->hooks->apply_caa(c->f->hooks->context, y, (uint16_t)(x + HALF_BLOCK_SIZE),
                           MZM_CAA_REMOVE_SOLID);
}

static void open_column(const Ctx *c, bool ground) {
    MzmSprite *s = c->s;
    change_pair(c, s->y_spawn, s->x_spawn);
    change_pair(c, (uint16_t)(s->y_spawn - BLOCK_SIZE), s->x_spawn);
    change_pair(c, (uint16_t)(s->y_spawn - BLOCK_SIZE * 2), s->x_spawn);
    if (ground) change_pair(c, (uint16_t)(s->y_spawn + BLOCK_SIZE), s->x_spawn);
}

static bool on_zipline(const Ctx *c) {
    if (!c->f->samus_link) return false;
    uint8_t pose = c->f->samus_link->samus_pose;
    return pose == MZM_POSE_ON_ZIPLINE || pose == MZM_POSE_SHOOTING_ON_ZIPLINE ||
           pose == MZM_POSE_TURNING_ON_ZIPLINE || pose == MZM_POSE_MORPH_BALL_ON_ZIPLINE;
}

static void spawn_parts(Ctx *c) {
    for (uint8_t part = 0; part < AW_PARTS; ++part)
        ai_spawn_secondary(c, MZM_SSPRITE_ACID_WORM_PART, part, c->s->gfx_slot,
                           (uint16_t)(c->s->y_spawn + BLOCK_SIZE * (part + 1)),
                           c->s->x_spawn, 0);
}

static void init_main(Ctx *c) {
    MzmSprite *s = c->s;
    s->y = (uint16_t)(s->y - HALF_BLOCK_SIZE);
    s->x = (uint16_t)(s->x + HALF_BLOCK_SIZE);
    s->y_spawn = s->y;
    s->x_spawn = s->x;
    s->hitbox_top = -THREE_QUARTER_BLOCK_SIZE;
    s->hitbox_bottom = THREE_QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    s->hitbox_left = s->hitbox_top - EIGHTH_BLOCK_SIZE;
    s->hitbox_right = -s->hitbox_left;
    s->draw_top = 26;
    s->draw_bottom = 16;
    s->draw_horizontal = 26;
    s->health = c->tables->health[s->sprite_id];
    s->immune_to_projectiles = true;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES | MZM_SPRITE_STATUS_FACING_DOWN;
    s->samus_collision = SSC_HURTS_SAMUS;
    s->draw_order = 2;
    ai_set_anim(s, MZM_ACID_WORM_IDLE);
    if (ai_check_event(c, MZM_EVENT_ACID_WORM_KILLED)) {
        open_column(c, true);
        move_acid(c, BLOCK_SIZE * 9 + HALF_BLOCK_SIZE);
        s->status = 0;
        return;
    }
    if (!ai_check_event(c, MZM_EVENT_ZIPLINES_ACTIVATED)) {
        s->status = 0;
        return;
    }
    if (!c->f->globals) { s->status = 0; return; }
    c->f->globals->sub1.y = s->y_spawn;
    c->f->globals->sub1.x = s->x_spawn;
    c->f->globals->sub1.health = (uint16_t)(s->y_spawn - BLOCK_SIZE * 2);
    c->f->globals->sub1.work2 = 0;
    c->f->globals->sub1.work3 = 0;
    s->room_slot = 0;
    s->pose = AW_CHECK_ZIPLINE;
    door_timer(c, 1);
    spawn_parts(c);
}

static void start_intro(Ctx *c) {
    MzmSprite *s = c->s;
    s->pose = AW_SPAWN_EXTEND;
    s->work0 = 0;
    s->status &= ~MZM_SPRITE_STATUS_FACING_DOWN;
    open_column(c, false);
    mzm_screen_shake_start_vertical(60, 0x81);
    mzm_screen_shake_start_horizontal(60, 0x81);
    mzm_sfx_play(MZM_SOUND_ACID_WORM_DESTROYING_BLOCKS);
    mzm_sfx_play(MZM_SOUND_ACID_WORM_STRAIGHT_EXTEND);
}

static void intro(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == AW_SPAWN_EXTEND) {
        if (s->y > s->y_spawn - BLOCK_SIZE * 7) {
            s->y = (uint16_t)(s->y - QUARTER_BLOCK_SIZE);
            move_acid(c, 1);
            return;
        }
        s->pose = AW_SPAWN_TOP;
        ai_set_anim(s, MZM_ACID_WORM_SPAWN_ON_TOP);
        s->work0 = 70;
        mzm_sfx_play(MZM_SOUND_ACID_WORM_SCREAM);
        return;
    }
    if (s->pose == AW_SPAWN_TOP) {
        s->x = (uint16_t)(s->x_spawn + ((c->f->frame8 >> 2) & 3) - 1);
        if (s->work0 && --s->work0) return;
        s->pose = AW_SPAWN_RETRACT;
        ai_set_anim(s, MZM_ACID_WORM_MOUTH_CLOSED);
        mzm_sfx_play(MZM_SOUND_ACID_WORM_STRAIGHT_GOING_DOWN);
        return;
    }
    if (s->y < s->y_spawn) {
        s->y = (uint16_t)(s->y + QUARTER_BLOCK_SIZE);
        move_acid(c, -1);
        return;
    }
    s->y = s->y_spawn;
    s->pose = AW_IDLE_INIT;
    s->work0 = 60;
}

static void idle(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == AW_IDLE_INIT) {
        ai_set_anim(s, MZM_ACID_WORM_IDLE);
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        s->pose = AW_IDLE;
        if (!s->work0) s->work0 = 60;
        return;
    }
    if (s->work0 && --s->work0) return;
    int dx = (int)c->f->samus_x - (int)s->x_spawn;
    int dy = (int)s->y_spawn - (int)c->f->samus_y;
    c->f->globals->sub1.work3 = dx < -BLOCK_SIZE * 7 || dx > BLOCK_SIZE * 7 ||
                                      dy > BLOCK_SIZE * 4;
    ai_face_samus(c);
    s->status &= ~MZM_SPRITE_STATUS_FACING_DOWN;
    ai_set_anim(s, MZM_ACID_WORM_WARNING);
    s->pose = AW_WARNING;
    s->work0 = 42;
    mzm_sfx_play(MZM_SOUND_ACID_WORM_OPENING_MOUTH);
}

static void start_extension(Ctx *c) {
    MzmSprite *s = c->s;
    ai_set_anim(s, MZM_ACID_WORM_MOVING);
    s->pose = AW_EXTENDING;
    s->work0 = 8;
    s->work1 = 0;
    mzm_sfx_play(c->f->globals->sub1.work3 ? MZM_SOUND_ACID_WORM_EXTEND_TO_SPIT :
                                            MZM_SOUND_ACID_WORM_EXTEND_SLOW);
}

static void spawn_spit(Ctx *c, uint8_t behavior) {
    MzmSprite *s = c->s;
    uint16_t status = s->status & MZM_SPRITE_STATUS_FACING_RIGHT ?
                      MZM_SPRITE_STATUS_X_FLIP : 0;
    int side = status ? THREE_QUARTER_BLOCK_SIZE : -THREE_QUARTER_BLOCK_SIZE;
    ai_spawn_secondary(c, MZM_SSPRITE_ACID_WORM_SPIT, behavior, s->gfx_slot, s->y,
                       (uint16_t)(s->x + side), status);
}

static void attack(Ctx *c) {
    MzmSprite *s = c->s;
    bool right = (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) != 0;
    if (s->pose == AW_WARNING) {
        if (s->work0 && --s->work0) return;
        start_extension(c);
        return;
    }
    if (s->pose == AW_EXTENDING) {
        if (s->work0 && --s->work0) return;
        unsigned speed = s->health <= c->tables->health[s->sprite_id] / 4 ? 8 :
                         s->health <= c->tables->health[s->sprite_id] / 2 ? 6 : 4;
        unsigned extension = s->work1 * 4u + speed;
        if (extension > AW_MAX_EXTENSION) extension = AW_MAX_EXTENSION;
        s->work1 = (uint8_t)(extension / 4);
        s->y = (uint16_t)(s->y_spawn - extension);
        s->x = (uint16_t)(s->x_spawn + (right ? (int)extension * 3 / 4 :
                                                -(int)extension * 3 / 4));
        move_acid(c, 1);
        if (extension < AW_MAX_EXTENSION) return;
        s->pose = AW_EXTENDED;
        s->work0 = c->f->globals->sub1.work3 ? 90 :
                   (uint8_t)(c->f->difficulty == 0 ? 180 : c->f->difficulty == 2 ? 100 : 140);
        s->work2 = 0;
        ai_set_anim(s, c->f->globals->sub1.work3 ? MZM_ACID_WORM_SPITTING :
                                                  MZM_ACID_WORM_MOUTH_CLOSED);
        mzm_screen_shake_start_vertical(20, 0x81);
        mzm_screen_shake_start_horizontal(20, 0x81);
        mzm_sfx_play(MZM_SOUND_ACID_WORM_CRASHING_SLOW);
        return;
    }
    if (s->pose == AW_EXTENDED) {
        if (c->f->globals->sub1.work3) {
            if (s->work2 == 18) { mzm_sfx_play(MZM_SOUND_ACID_WORM_SPITTING); spawn_spit(c, 0); }
            if (s->work2 == 36) spawn_spit(c, 1);
            if (s->work2 == 54) spawn_spit(c, 2);
            s->work2++;
        } else if ((s->work2++ & 31) == 0) {
            mzm_sfx_play(MZM_SOUND_ACID_WORM_HOOKED_ON_BLOCK);
        }
        if (s->work0 && --s->work0) return;
        s->pose = AW_RETRACTING;
        ai_set_anim(s, MZM_ACID_WORM_MOUTH_CLOSED);
        mzm_sfx_play(c->f->globals->sub1.work3 ? MZM_SOUND_ACID_WORM_RETRACT_FROM_SPIT :
                                                MZM_SOUND_ACID_WORM_RETRACT);
        return;
    }
    if (s->work1) {
        unsigned extension = s->work1 * 4u;
        extension = extension > 2 ? extension - 2 : 0;
        s->work1 = (uint8_t)(extension / 4);
        s->y = (uint16_t)(s->y_spawn - extension);
        s->x = (uint16_t)(s->x_spawn + (right ? (int)extension * 3 / 4 :
                                                -(int)extension * 3 / 4));
        move_acid(c, -1);
        return;
    }
    s->x = s->x_spawn;
    s->y = s->y_spawn;
    s->pose = AW_IDLE_INIT;
    s->work0 = (uint8_t)(80 + (c->rng & 31));
}

static void start_death(Ctx *c) {
    MzmSprite *s = c->s;
    s->pose = AW_DYING_FLASH;
    s->work0 = 80;
    s->samus_collision = SSC_NONE;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    ai_set_anim(s, MZM_ACID_WORM_MOVING);
    mzm_sfx_play(MZM_SOUND_ACID_WORM_DYING);
}

static void dying(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == AW_DYING_FLASH) {
        s->ignore_samus_timer = 1;
        if ((s->work0 & 7) < 4) s->status ^= MZM_SPRITE_STATUS_NOT_DRAWN;
        if (s->work0 && --s->work0) return;
        s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
        s->pose = AW_DYING;
        s->work0 = 90;
        ai_particle_set(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        return;
    }
    move_acid(c, 4);
    if ((s->work0 & 15) == 0) {
        mzm_screen_shake_start_vertical(10, 0x81);
        mzm_screen_shake_start_horizontal(10, 0x81);
    }
    if (s->work0 && --s->work0) return;
    open_column(c, true);
    set_event(c, MZM_EVENT_ACID_WORM_KILLED);
    door_timer(c, -20);
    mzm_sfx_play(MZM_SOUND_ACID_WORM_DEAD);
    s->status = 0;
}

static void main_ai(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) { init_main(c); return; }
    if (s->health == 0 && s->pose < AW_DYING_INIT) s->pose = AW_DYING_INIT;
    switch (s->pose) {
    case AW_CHECK_ZIPLINE:
        if (on_zipline(c)) s->pose = AW_CHECK_SPAWN;
        break;
    case AW_CHECK_SPAWN:
        if (on_zipline(c) && c->f->samus_x > s->x_spawn - (BLOCK_SIZE * 2 + 12) &&
            c->f->samus_x < s->x_spawn + (BLOCK_SIZE * 2 + 12)) start_intro(c);
        break;
    case AW_SPAWN_EXTEND: case AW_SPAWN_TOP: case AW_SPAWN_RETRACT: intro(c); break;
    case AW_IDLE_INIT: case AW_IDLE: idle(c); break;
    case AW_WARNING: case AW_EXTENDING: case AW_EXTENDED: case AW_RETRACTING: attack(c); break;
    case AW_DYING_INIT: start_death(c); break;
    case AW_DYING_FLASH: case AW_DYING: dying(c); break;
    default: s->pose = AW_IDLE_INIT; break;
    }
}

static void init_part(Ctx *c) {
    MzmSprite *s = c->s;
    static const uint8_t anim[AW_PARTS] = {
        MZM_ACID_WORM_PART_AROUND_MOUTH, MZM_ACID_WORM_PART_WEAK_POINT,
        MZM_ACID_WORM_PART_BELOW_WEAK_POINT, MZM_ACID_WORM_PART_ABOVE_SEGMENTS,
        MZM_ACID_WORM_PART_SEGMENT, MZM_ACID_WORM_PART_SEGMENT,
        MZM_ACID_WORM_PART_SEGMENT, MZM_ACID_WORM_PART_SEGMENT, MZM_ACID_WORM_PART_SEGMENT,
    };
    s->status &= ~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->pose = AW_PART_MOVING;
    s->draw_top = s->draw_bottom = s->draw_horizontal = 16;
    s->draw_order = s->room_slot < 4 ? 3 : 5;
    s->hitbox_top = s->hitbox_left = -(s->room_slot == AW_WEAK_POINT ? 48 : 32);
    s->hitbox_bottom = s->hitbox_right = -s->hitbox_top;
    s->samus_collision = SSC_HURTS_SAMUS;
    s->health = 0x400;
    s->immune_to_projectiles = s->room_slot != AW_WEAK_POINT;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    ai_set_anim(s, anim[s->room_slot < AW_PARTS ? s->room_slot : 0]);
}

static void part_ai(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) init_part(c);
    MzmSprite *main = slot(c, s->ram_slot);
    if (!main || !(main->status & MZM_SPRITE_STATUS_EXISTS)) { s->status = 0; return; }
    unsigned part = s->room_slot + 1;
    int dx = (int)main->x - (int)main->x_spawn;
    int dy = (int)main->y - (int)main->y_spawn;
    s->x = (uint16_t)(main->x_spawn + dx * (10 - (int)part) / 10 +
                      (((c->f->frame8 + part * 5) >> 3) & 1 ? 2 : -2));
    s->y = (uint16_t)(main->y_spawn + dy * (10 - (int)part) / 10 + part * BLOCK_SIZE);
    if (main->pose >= AW_DYING_INIT) {
        s->samus_collision = SSC_NONE;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        if (s->pose != AW_PART_DYING) { s->pose = AW_PART_DYING; s->work0 = (uint8_t)(part * 7); }
        if (s->work0 && --s->work0) return;
        ai_sprite_death_full(c, DEATH_NO_DEATH_OR_RESPAWNING_ALREADY_HAS_DROP,
                             s->y, s->x, false, s->room_slot == AW_WEAK_POINT ?
                             MZM_PE_SPRITE_EXPLOSION_BIG : MZM_PE_SPRITE_EXPLOSION_MEDIUM);
        return;
    }
    bool exposed = main->pose == AW_WARNING || main->pose == AW_EXTENDING ||
                   main->pose == AW_EXTENDED || main->pose == AW_RETRACTING;
    if (s->room_slot == AW_WEAK_POINT && exposed)
        s->status &= ~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    else
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    if (s->room_slot == AW_WEAK_POINT && s->health < 0x400) {
        uint16_t damage = (uint16_t)(0x400 - s->health);
        if (damage >= main->health) main->health = 0;
        else main->health = (uint16_t)(main->health - damage);
        s->health = 0x400;
        mzm_sfx_play(MZM_SOUND_ACID_WORM_HURT);
    }
    s->pose = main->pose == AW_IDLE_INIT ? AW_PART_IDLE : AW_PART_MOVING;
}

static void spit_ai(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->status &= ~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->kill_off_screen = true;
        s->hitbox_top = s->hitbox_left = -QUARTER_BLOCK_SIZE;
        s->hitbox_bottom = s->hitbox_right = QUARTER_BLOCK_SIZE;
        s->draw_top = s->draw_bottom = s->draw_horizontal = 8;
        s->draw_order = 3;
        s->health = c->tables->secondary_health[s->sprite_id];
        s->samus_collision = SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT;
        s->pose = AW_SPIT_MOVING;
        s->work3 = 0;
        ai_set_anim(s, MZM_ACID_WORM_SPIT_MOVING);
        return;
    }
    if (s->pose == SPRITE_POSE_STOPPED) {
        s->pose = AW_SPIT_EXPLODING;
        s->work0 = 20;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        ai_set_anim(s, MZM_ACID_WORM_SPIT_EXPLODING);
        mzm_sfx_play(MZM_SOUND_ACID_WORM_SPIT_EXPLODING);
    }
    if (s->pose == AW_SPIT_EXPLODING) {
        s->ignore_samus_timer = 1;
        if (s->work0 && --s->work0) return;
        s->status = 0;
        return;
    }
    int speed = s->room_slot == 1 ? 10 : s->room_slot == 2 ? 2 : 6;
    s->x = (uint16_t)(s->x + ((s->status & MZM_SPRITE_STATUS_X_FLIP) ? speed : -speed));
    int age = s->work3++;
    s->y = (uint16_t)(s->y + (age < 12 ? -6 + age / 3 : 3 + (age - 12) / 3));
    if ((c->world->effect_y && s->y >= c->world->effect_y) ||
        (ai_collision_at(c, s->y, s->x) & MZM_CLIPDATA_SOLID_FLAG)) {
        s->pose = SPRITE_POSE_STOPPED;
    }
}

void ai_acid_worm(Ctx *c) {
    if (!c->s->secondary) main_ai(c);
    else if (c->s->sprite_id == MZM_SSPRITE_ACID_WORM_PART) part_ai(c);
    else spit_ai(c);
}
