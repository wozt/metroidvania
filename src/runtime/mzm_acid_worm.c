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
    AW_SPIT_MOVING = 0x9, AW_SPIT_EXPLODING = 0x43, AW_SPIT_ACID = 0x45,
    AW_PARTS = 9, AW_WEAK_POINT = 2,
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

static void destroy_block_down(Ctx *c) {
    MzmSprite *s = c->s;
    change_pair(c, s->y_spawn, s->x_spawn);
    if (!ai_check_event(c, MZM_EVENT_ACID_WORM_KILLED))
        ai_particle_set(c, (uint16_t)(s->y_spawn - BLOCK_SIZE),
                        (uint16_t)(s->x_spawn - QUARTER_BLOCK_SIZE),
                        MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
}

static void destroy_block_middle(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t y = (uint16_t)(s->y_spawn - BLOCK_SIZE);
    change_pair(c, y, s->x_spawn);
    if (!ai_check_event(c, MZM_EVENT_ACID_WORM_KILLED))
        ai_particle_set(c, (uint16_t)(y - BLOCK_SIZE),
                        (uint16_t)(s->x_spawn + QUARTER_BLOCK_SIZE),
                        MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
}

static void destroy_block_top(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t y = (uint16_t)(s->y_spawn - BLOCK_SIZE * 2);
    uint16_t x = s->x_spawn;
    change_pair(c, y, x);
    if (ai_check_event(c, MZM_EVENT_ACID_WORM_KILLED)) return;
    ai_particle_set(c, (uint16_t)(y - BLOCK_SIZE), (uint16_t)(x - EIGHTH_BLOCK_SIZE),
                    MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
    y = (uint16_t)(y - BLOCK_SIZE * 2);
    ai_debris_init(c, 17, (uint16_t)(y - BLOCK_SIZE - PIXEL_SIZE / 2),
                   (uint16_t)(x - HALF_BLOCK_SIZE));
    ai_debris_init(c, 18, y, (uint16_t)(x - THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE));
    ai_debris_init(c, 19, (uint16_t)(y - BLOCK_SIZE - HALF_BLOCK_SIZE + PIXEL_SIZE),
                   (uint16_t)(x - BLOCK_SIZE + PIXEL_SIZE / 2));
    ai_debris_init(c, 4, y, (uint16_t)(x - HALF_BLOCK_SIZE));
    ai_debris_init(c, 17, (uint16_t)(y - HALF_BLOCK_SIZE - PIXEL_SIZE),
                   (uint16_t)(x + BLOCK_SIZE + QUARTER_BLOCK_SIZE + PIXEL_SIZE / 2));
    ai_debris_init(c, 18, (uint16_t)(y - BLOCK_SIZE - QUARTER_BLOCK_SIZE + PIXEL_SIZE),
                   (uint16_t)(x + HALF_BLOCK_SIZE));
    ai_debris_init(c, 19, y, (uint16_t)(x + BLOCK_SIZE + EIGHTH_BLOCK_SIZE));
    ai_debris_init(c, 4, (uint16_t)(y - BLOCK_SIZE - THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE),
                   (uint16_t)(x + THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE));
}

static bool on_zipline(const Ctx *c) {
    if (!c->f->samus_link) return false;
    uint8_t pose = c->f->samus_link->samus_pose;
    return pose == MZM_POSE_ON_ZIPLINE || pose == MZM_POSE_SHOOTING_ON_ZIPLINE ||
           pose == MZM_POSE_TURNING_ON_ZIPLINE || pose == MZM_POSE_MORPH_BALL_ON_ZIPLINE;
}

static void handle_rotation(Ctx *c, MzmSprite *s) {
    uint8_t angle = (uint8_t)(s->rotation +
        ((s->status & MZM_SPRITE_STATUS_FACING_RIGHT) ? 128 : 0));
    int32_t base_radius = c->f->globals->sub1.work3 ? BLOCK_SIZE * 6 : BLOCK_SIZE * 4;
    int32_t radius = base_radius;
    uint8_t offset = s->work3;
    int16_t swing = c->tables->acid_worm_swinging[offset];
    if (swing == SHORT_MAX) {
        offset = 0;
        swing = c->tables->acid_worm_swinging[0];
    }
    s->work3 = (uint8_t)(offset + 1);
    if (s->health)
        radius += (int32_t)swing * 16 * (s->room_slot / 4 + 1);
    else
        radius += (int32_t)swing * 16;
    int32_t sine = c->tables->sine[angle];
    int32_t cosine = c->tables->sine[angle + 64];
    int32_t x = c->f->globals->sub1.x;
    x += s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? base_radius : -base_radius;
    s->y = (uint16_t)((int32_t)c->f->globals->sub1.y + sine * radius / 256);
    s->x = (uint16_t)(x + cosine * radius / 256);
}

static void wiggle_horizontally(Ctx *c, MzmSprite *s) {
    uint8_t offset = s->work2;
    int16_t movement = c->tables->acid_worm_head_x[offset];
    if (movement == SHORT_MAX) {
        offset = 20;
        movement = c->tables->acid_worm_head_x[20];
    }
    s->work2 = (uint8_t)(offset + 1);
    s->x = (uint16_t)(s->x + movement);
}

static void splash_if_crossed(const Ctx *c, uint16_t old_y, uint16_t new_y) {
    if (!c->world->effect_y) return;
    if ((old_y < c->world->effect_y && new_y >= c->world->effect_y) ||
        (old_y >= c->world->effect_y && new_y < c->world->effect_y))
        mzm_sfx_play(MZM_SOUND_ACID_WORM_SPLASHING);
}

static void spawn_parts(Ctx *c) {
    for (uint8_t part = 1; part <= AW_PARTS; ++part)
        ai_spawn_secondary(c, MZM_SSPRITE_ACID_WORM_PART, part, c->s->gfx_slot,
                           (uint16_t)(c->s->y_spawn + BLOCK_SIZE * part),
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
    s->work2 = 20;
    s->work3 = 0;
    s->room_slot = 0;
    s->pose = AW_CHECK_ZIPLINE;
    door_timer(c, 1);
    spawn_parts(c);
}

static void start_intro(Ctx *c) {
    MzmSprite *s = c->s;
    s->pose = AW_SPAWN_EXTEND;
    s->work0 = 0;
    s->status &= (uint16_t)~(MZM_SPRITE_STATUS_FACING_DOWN |
                             MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
    c->f->globals->sub1.work2 = 1;
    destroy_block_down(c);
    mzm_screen_shake_start_vertical(60, 0x81);
    mzm_screen_shake_start_horizontal(60, 0x81);
    mzm_sfx_play(MZM_SOUND_ACID_WORM_DESTROYING_BLOCKS);
    mzm_sfx_play(MZM_SOUND_ACID_WORM_STRAIGHT_EXTEND);
    mzm_sfx_music_play(MZM_MUSIC_WORMS_BATTLE, 0);
}

static void intro(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == AW_SPAWN_EXTEND) {
        if (s->y >= s->y_spawn - BLOCK_SIZE * 7) {
            uint16_t old_y = s->y;
            s->y = (uint16_t)(s->y - QUARTER_BLOCK_SIZE);
            splash_if_crossed(c, old_y, s->y);
            move_acid(c, 1);
            if (++s->work0 == 4)
                destroy_block_middle(c);
            else if (s->work0 == 8)
                destroy_block_top(c);
            return;
        }
        s->pose = AW_SPAWN_TOP;
        ai_set_anim(s, MZM_ACID_WORM_SPAWN_ON_TOP);
        s->work0 = 70;
        mzm_sfx_play(MZM_SOUND_ACID_WORM_SCREAM);
        return;
    }
    if (s->pose == AW_SPAWN_TOP) {
        wiggle_horizontally(c, s);
        if (s->work0 && --s->work0) return;
        s->pose = AW_SPAWN_RETRACT;
        ai_set_anim(s, MZM_ACID_WORM_MOUTH_CLOSED);
        mzm_sfx_play(MZM_SOUND_ACID_WORM_STRAIGHT_GOING_DOWN);
        return;
    }
    if (s->y < s->y_spawn) {
        uint16_t old_y = s->y;
        wiggle_horizontally(c, s);
        s->y = (uint16_t)(s->y + QUARTER_BLOCK_SIZE);
        splash_if_crossed(c, old_y, s->y);
        if (c->world->effect_y > c->f->globals->sub1.health) move_acid(c, -1);
        return;
    }
    s->y = s->y_spawn;
    s->pose = AW_IDLE_INIT;
    s->work0 = 60;
    s->status |= MZM_SPRITE_STATUS_ROTATION_SCALING_WHOLE;
    s->scaling = 256;
    s->rotation = 0;
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
    if (c->world->effect_y > c->f->globals->sub1.health) move_acid(c, -1);
    if (c->f->samus_y > c->world->effect_y || c->f->samus_y > s->y) {
        s->work0 = 60;
        return;
    }
    if (s->work0 && --s->work0) return;
    uint32_t vertical = (uint16_t)(s->y - c->f->samus_y -
        (BLOCK_SIZE + QUARTER_BLOCK_SIZE + 1));
    c->f->globals->sub1.work3 = vertical >= BLOCK_SIZE * 4 - QUARTER_BLOCK_SIZE - 1 ||
        c->f->samus_x <= s->x_spawn - BLOCK_SIZE * 7 ||
        c->f->samus_x >= s->x_spawn + BLOCK_SIZE * 7;
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
    if (c->f->globals->sub1.health == s->y_spawn - BLOCK_SIZE * 3)
        move_acid(c, -BLOCK_SIZE);
}

static void spawn_spit(Ctx *c, uint8_t behavior) {
    MzmSprite *s = c->s;
    uint16_t status = s->status & MZM_SPRITE_STATUS_FACING_RIGHT ?
                      MZM_SPRITE_STATUS_X_FLIP : 0;
    int side = status ? THREE_QUARTER_BLOCK_SIZE : -THREE_QUARTER_BLOCK_SIZE;
    ai_spawn_secondary(c, MZM_SSPRITE_ACID_WORM_SPIT, behavior, s->gfx_slot, s->y,
                       (uint16_t)(s->x + side), status);
}

static void play_retracting_sound(const Ctx *c) {
    mzm_sfx_play(c->f->globals->sub1.work3 ? MZM_SOUND_ACID_WORM_RETRACT_FROM_SPIT :
                                            MZM_SOUND_ACID_WORM_RETRACT);
}

static bool abort_extension_on_samus(Ctx *c) {
    MzmSprite *s = c->s;
    if (!(s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING)) return false;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
    ai_set_anim(s, MZM_ACID_WORM_MOUTH_CLOSED);
    s->pose = AW_RETRACTING;
    play_retracting_sound(c);
    return true;
}

static void extension_crash(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t maximum = c->tables->health[s->sprite_id];
    uint16_t y = s->y;
    uint16_t x = (uint16_t)(s->x +
        ((s->status & MZM_SPRITE_STATUS_FACING_RIGHT) ? BLOCK_SIZE : -BLOCK_SIZE));
    if (s->health <= maximum / 4) {
        mzm_screen_shake_start_vertical(40, 0x81);
        mzm_screen_shake_start_horizontal(40, 0x81);
        ai_debris_init(c, 17, (uint16_t)(y - BLOCK_SIZE - PIXEL_SIZE / 2),
                       (uint16_t)(x - HALF_BLOCK_SIZE));
        ai_debris_init(c, 4, y, x);
        ai_debris_init(c, 18, y,
                       (uint16_t)(x + BLOCK_SIZE - EIGHTH_BLOCK_SIZE - PIXEL_SIZE));
        ai_debris_init(c, 19, (uint16_t)(y - BLOCK_SIZE - HALF_BLOCK_SIZE + PIXEL_SIZE),
                       (uint16_t)(x - BLOCK_SIZE + PIXEL_SIZE / 2));
        ai_debris_init(c, 4, (uint16_t)(y + HALF_BLOCK_SIZE),
                       (uint16_t)(x + HALF_BLOCK_SIZE));
        ai_debris_init(c, 18, (uint16_t)(y + BLOCK_SIZE), x);
        ai_particle_set(c, (uint16_t)(y + HALF_BLOCK_SIZE), x,
                        MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        s->work0 = 120;
        mzm_sfx_play(MZM_SOUND_ACID_WORM_CRASHING_FAST);
    } else if (s->health <= maximum / 2) {
        mzm_screen_shake_start_vertical(20, 0x81);
        mzm_screen_shake_start_horizontal(20, 0x81);
        ai_debris_init(c, 17, (uint16_t)(y - BLOCK_SIZE - PIXEL_SIZE / 2),
                       (uint16_t)(x - HALF_BLOCK_SIZE));
        ai_debris_init(c, 18, y,
                       (uint16_t)(x + BLOCK_SIZE - EIGHTH_BLOCK_SIZE - PIXEL_SIZE));
        ai_debris_init(c, 19, (uint16_t)(y - BLOCK_SIZE - HALF_BLOCK_SIZE + PIXEL_SIZE),
                       (uint16_t)(x - BLOCK_SIZE + PIXEL_SIZE / 2));
        ai_debris_init(c, 4, (uint16_t)(y + HALF_BLOCK_SIZE),
                       (uint16_t)(x + HALF_BLOCK_SIZE));
        ai_particle_set(c, (uint16_t)(y + HALF_BLOCK_SIZE), x,
                        MZM_PE_SPRITE_EXPLOSION_BIG);
        s->work0 = 140;
        mzm_sfx_play(MZM_SOUND_ACID_WORM_CRASHING_NORMAL);
    } else {
        mzm_screen_shake_start_vertical(10, 0x81);
        mzm_screen_shake_start_horizontal(10, 0x81);
        ai_debris_init(c, 18, (uint16_t)(y - BLOCK_SIZE - PIXEL_SIZE / 2),
                       (uint16_t)(x - HALF_BLOCK_SIZE));
        ai_debris_init(c, 4, (uint16_t)(y + HALF_BLOCK_SIZE),
                       (uint16_t)(x + HALF_BLOCK_SIZE));
        ai_particle_set(c, (uint16_t)(y + HALF_BLOCK_SIZE), x,
                        MZM_PE_SPRITE_EXPLOSION_MEDIUM);
        s->work0 = 160;
        mzm_sfx_play(MZM_SOUND_ACID_WORM_CRASHING_SLOW);
    }
    if (c->f->difficulty == 0) s->work0 = (uint8_t)(s->work0 + 60);
    else if (c->f->difficulty == 2) s->work0 = (uint8_t)(s->work0 - 20);
    s->work1 = 0;
    ai_set_anim(s, MZM_ACID_WORM_MOUTH_CLOSED);
}

static void attack(Ctx *c) {
    MzmSprite *s = c->s;
    bool right = (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) != 0;
    if (s->pose == AW_WARNING) {
        handle_rotation(c, s);
        if (!ai_animation_ending(c, 1)) return;
        start_extension(c);
        return;
    }
    if (s->pose == AW_EXTENDING) {
        if (s->work0) {
            if (--s->work0 == 0) {
                uint16_t maximum = c->tables->health[s->sprite_id];
                mzm_sfx_play(c->f->globals->sub1.work3 ? MZM_SOUND_ACID_WORM_EXTEND_TO_SPIT :
                    s->health <= maximum / 4 ? MZM_SOUND_ACID_WORM_EXTEND_FAST :
                    s->health <= maximum / 2 ? MZM_SOUND_ACID_WORM_EXTEND_NORMAL :
                                               MZM_SOUND_ACID_WORM_EXTEND_SLOW);
            }
            return;
        }
        move_acid(c, 1);
        uint16_t maximum = c->tables->health[s->sprite_id];
        uint8_t speed = c->f->globals->sub1.work3 ? 2 :
                        s->health <= maximum / 4 ? 4 :
                        s->health <= maximum / 2 ? 3 : 2;
        unsigned checks = 0;
        uint8_t rotation = (uint8_t)s->rotation;
        if (right) {
            if (rotation >= 63) checks++;
            else s->rotation = (uint8_t)(rotation + speed);
            if (c->f->globals->sub1.x < s->x_spawn + BLOCK_SIZE)
                c->f->globals->sub1.x = (uint16_t)(c->f->globals->sub1.x + speed);
            else checks++;
        } else {
            if ((uint8_t)(rotation - 1) <= 191) checks++;
            else s->rotation = (uint8_t)(rotation - speed);
            if (c->f->globals->sub1.x > s->x_spawn - BLOCK_SIZE)
                c->f->globals->sub1.x = (uint16_t)(c->f->globals->sub1.x - speed);
            else checks++;
        }
        if (c->f->globals->sub1.y > s->y_spawn - THREE_QUARTER_BLOCK_SIZE)
            c->f->globals->sub1.y = (uint16_t)(c->f->globals->sub1.y - speed);
        else checks++;
        uint16_t old_y = s->y;
        handle_rotation(c, s);
        splash_if_crossed(c, old_y, s->y);
        if (abort_extension_on_samus(c) || checks != 3) return;
        s->pose = AW_EXTENDED;
        if (c->f->globals->sub1.work3)
            ai_set_anim(s, MZM_ACID_WORM_SPITTING);
        else
            extension_crash(c);
        return;
    }
    if (s->pose == AW_EXTENDED) {
        handle_rotation(c, s);
        if (c->f->globals->sub1.work3) {
            bool finished = c->f->samus_x > s->x_spawn - BLOCK_SIZE * 7 &&
                            c->f->samus_x < s->x_spawn + BLOCK_SIZE * 7;
            if (s->frame == 1 && s->adc == 1)
                mzm_sfx_play(MZM_SOUND_ACID_WORM_OPENING_MOUTH_TO_SPIT);
            else if (s->frame == 2 && s->adc == 1)
                mzm_sfx_play(MZM_SOUND_ACID_WORM_SPITTING);
            else if (s->frame == 5 && s->adc == 1) spawn_spit(c, 0);
            else if (s->frame == 7 && s->adc == 1) spawn_spit(c, 1);
            else if (s->frame == 9 && s->adc == 1) spawn_spit(c, 2);
            if (!finished && !ai_animation_ending(c, 1)) return;
        } else {
            if (abort_extension_on_samus(c)) return;
            if ((s->work1++ & 31) == 0) mzm_sfx_play(MZM_SOUND_ACID_WORM_HOOKED_ON_BLOCK);
            if (s->work0 && --s->work0) return;
        }
        ai_set_anim(s, MZM_ACID_WORM_MOUTH_CLOSED);
        s->pose = AW_RETRACTING;
        play_retracting_sound(c);
        return;
    }
    unsigned checks = 0;
    uint8_t rotation = (uint8_t)s->rotation;
    if (right) {
        if (rotation < 3) { s->rotation = 0; checks++; }
        else s->rotation = (uint8_t)(rotation - 2);
        if (c->f->globals->sub1.x > s->x_spawn)
            c->f->globals->sub1.x -= 2;
        else { c->f->globals->sub1.x = s->x_spawn; checks++; }
    } else {
        if (rotation < 3) { s->rotation = 0; checks++; }
        else s->rotation = (uint8_t)(rotation + 2);
        if (c->f->globals->sub1.x < s->x_spawn)
            c->f->globals->sub1.x += 2;
        else { c->f->globals->sub1.x = s->x_spawn; checks++; }
    }
    if (c->f->globals->sub1.y < s->y_spawn)
        c->f->globals->sub1.y += 2;
    else { c->f->globals->sub1.y = s->y_spawn; checks++; }
    if (c->world->effect_y > c->f->globals->sub1.health) move_acid(c, -1);
    uint16_t old_y = s->y;
    handle_rotation(c, s);
    splash_if_crossed(c, old_y, s->y);
    if (checks != 3) return;
    if (s->health <= c->tables->health[s->sprite_id] / 2 && c->f->difficulty != 0) {
        s->status ^= MZM_SPRITE_STATUS_MOSAIC;
        uint32_t vertical = (uint32_t)((int32_t)s->y - c->f->samus_y -
                                      (BLOCK_SIZE + QUARTER_BLOCK_SIZE + 1));
        if ((s->status & MZM_SPRITE_STATUS_MOSAIC) &&
            vertical < BLOCK_SIZE * 4 - QUARTER_BLOCK_SIZE - 1 &&
            c->f->samus_x > s->x_spawn - BLOCK_SIZE * 7 &&
            c->f->samus_x < s->x_spawn + BLOCK_SIZE * 7) {
            s->pose = AW_RAISE_ACID;
            ai_set_anim(s, MZM_ACID_WORM_MOVING);
            s->work0 = 40;
            mzm_sfx_play(MZM_SOUND_ACID_WORM_RAISING_ACID);
            return;
        }
        s->work0 = (uint8_t)(60 + (c->rng & 7) * 8);
    } else {
        s->work0 = (uint8_t)(80 + (c->rng & 7) * 8);
    }
    s->pose = AW_IDLE_INIT;
}

static void acid_cycle(Ctx *c) {
    MzmSprite *s = c->s;
    if ((c->frame8 & 15) == 0) mzm_screen_shake_start_horizontal(10, 0x81);
    if (s->pose == AW_RAISE_ACID) {
        if (s->work0) { --s->work0; return; }
        move_acid(c, -1);
        int32_t projected = (int32_t)c->world->effect_y - 1;
        if (projected < (int32_t)s->y_spawn - BLOCK_SIZE * 5) {
            s->pose = AW_LOWER_ACID;
            mzm_sfx_play(MZM_SOUND_ACID_WORM_LOWERING_ACID);
        }
        return;
    }
    move_acid(c, 1);
    int32_t projected = (int32_t)c->world->effect_y + 1;
    if (projected > (int32_t)s->y_spawn - BLOCK_SIZE * 3) {
        s->pose = AW_IDLE_INIT;
        s->work0 = 1;
        move_acid(c, (int16_t)((int32_t)s->y_spawn - BLOCK_SIZE * 3 - projected));
        if (c->f->globals->sub1.health == s->y_spawn - BLOCK_SIZE * 2)
            c->f->globals->sub1.health = (uint16_t)(s->y_spawn - BLOCK_SIZE * 3);
    }
}

static void start_death(Ctx *c) {
    MzmSprite *s = c->s;
    s->pose = AW_DYING_FLASH;
    s->isft = 80;
    s->samus_collision = SSC_NONE;
    ai_set_anim(s, MZM_ACID_WORM_MOVING);
}

static void dying(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == AW_DYING_FLASH) {
        s->ignore_samus_timer = 1;
        handle_rotation(c, s);
        if (!(s->isft & 0x7F)) return;
        uint8_t isft = --s->isft;
        if (!(isft & 3)) {
            if (isft & 4) {
                s->palette_row = (uint8_t)(14 - s->gfx_slot - s->frozen_row_offset);
            } else {
                s->palette_row = s->absolute_palette_row;
                if (isft == 0) {
                    ai_particle_set(c, s->y, s->x,
                                    MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
                    s->pose = AW_DYING;
                    s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
                    s->work0 = 1;
                }
            }
        }
        return;
    }
    if (c->world->effect_y > s->y_spawn + BLOCK_SIZE * 7 + HALF_BLOCK_SIZE) {
        if (!c->f->globals->sub1.work2) {
            s->status = 0;
            mzm_sfx_music_play(MZM_MUSIC_BOSS_KILLED, 0);
        }
        return;
    }
    if (!c->f->globals->sub1.work2) {
        if (c->world->effect_y < s->y_spawn) {
            if ((c->frame8 & 31) == 0)
                ai_particle_set(c,
                    (uint16_t)(s->y_spawn + BLOCK_SIZE + EIGHTH_BLOCK_SIZE + c->rng * 8),
                    s->x_spawn, MZM_PE_SECOND_TWO_MEDIUM_DUST);
        } else {
            move_acid(c, 1);
        }
    }
    if (s->work0) {
        if (--s->work0 == 0) mzm_sfx_play(MZM_SOUND_ACID_WORM_DEAD);
    } else {
        move_acid(c, 1);
        if ((c->frame8 & 15) == 0) {
            mzm_screen_shake_start_vertical(10, 0x81);
            mzm_screen_shake_start_horizontal(10, 0x81);
        }
    }
}

static void main_ai(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) { init_main(c); return; }
    if (s->health == 0 && s->pose < AW_DYING_INIT) s->pose = AW_DYING_INIT;
    switch (s->pose) {
    case AW_CHECK_ZIPLINE:
        if (on_zipline(c)) {
            s->pose = AW_CHECK_SPAWN;
            mzm_sfx_music_fade(18);
        }
        break;
    case AW_CHECK_SPAWN:
        if (on_zipline(c) && c->f->samus_x > s->x_spawn - (BLOCK_SIZE * 2 + 12) &&
            c->f->samus_x < s->x_spawn + (BLOCK_SIZE * 2 + 12)) start_intro(c);
        break;
    case AW_SPAWN_EXTEND: case AW_SPAWN_TOP: case AW_SPAWN_RETRACT: intro(c); break;
    case AW_IDLE_INIT: case AW_IDLE: idle(c); break;
    case AW_WARNING: case AW_EXTENDING: case AW_EXTENDED: case AW_RETRACTING: attack(c); break;
    case AW_RAISE_ACID: case AW_LOWER_ACID: acid_cycle(c); break;
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
    if (s->room_slot < 1 || s->room_slot > AW_PARTS) { s->status = 0; return; }
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->pose = AW_PART_MOVING;
    s->draw_top = s->draw_bottom = s->draw_horizontal = 16;
    s->samus_collision = SSC_HURTS_SAMUS_IGNORE_INVINCIBILITY_NO_CONTACT_DAMAGE;
    s->health = 0x400;
    s->immune_to_projectiles = s->room_slot != AW_WEAK_POINT;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->work0 = (uint8_t)(s->room_slot * 8);
    s->work2 = (uint8_t)(20 - s->room_slot * 2);
    static const uint8_t swing_offsets[AW_PARTS] = {8, 16, 24, 32, 0, 8, 16, 24, 32};
    s->work3 = swing_offsets[s->room_slot - 1];
    if (s->room_slot == 1) {
        s->hitbox_top = s->hitbox_left = -QUARTER_BLOCK_SIZE;
        s->hitbox_bottom = s->hitbox_right = QUARTER_BLOCK_SIZE;
        s->draw_order = 3;
    } else if (s->room_slot == AW_WEAK_POINT) {
        s->hitbox_top = s->hitbox_left = -(BLOCK_SIZE - QUARTER_BLOCK_SIZE);
        s->hitbox_bottom = s->hitbox_right = BLOCK_SIZE - QUARTER_BLOCK_SIZE;
        s->draw_order = 2;
    } else if (s->room_slot == 3) {
        s->hitbox_top = -(BLOCK_SIZE - QUARTER_BLOCK_SIZE);
        s->hitbox_bottom = BLOCK_SIZE - QUARTER_BLOCK_SIZE;
        s->hitbox_left = -(HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        s->hitbox_right = HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        s->draw_order = 3;
    } else if (s->room_slot == 4) {
        s->hitbox_top = s->hitbox_left = -(HALF_BLOCK_SIZE + PIXEL_SIZE / 2);
        s->hitbox_bottom = s->hitbox_right = HALF_BLOCK_SIZE + PIXEL_SIZE / 2;
        s->draw_order = 4;
    } else {
        s->hitbox_top = s->hitbox_left = -HALF_BLOCK_SIZE;
        s->hitbox_bottom = s->hitbox_right = HALF_BLOCK_SIZE;
        s->draw_order = 5;
    }
    ai_set_anim(s, anim[s->room_slot - 1]);
}

static void transfer_part_damage(Ctx *c, MzmSprite *main) {
    MzmSprite *s = c->s;
    if (s->room_slot != AW_WEAK_POINT || s->health >= 0x400) return;
    uint16_t damage = (uint16_t)(0x400 - s->health);
    main->isft = s->isft;
    s->isft = 0;
    if (damage >= main->health) {
        main->health = 0;
        main->pose = AW_DYING_INIT;
        main->samus_collision = SSC_NONE;
        s->health = 0;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->ignore_samus_timer = 1;
        mzm_sfx_play(MZM_SOUND_ACID_WORM_DYING);
        return;
    }
    main->health = (uint16_t)(main->health - damage);
    s->health = 0x400;
    if (main->health <= c->tables->health[main->sprite_id] / 4) main->absolute_palette_row = 2;
    else if (main->health <= c->tables->health[main->sprite_id] / 2) main->absolute_palette_row = 1;
    mzm_sfx_play(MZM_SOUND_ACID_WORM_HURT);
}

static void part_ai(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) { init_part(c); return; }
    MzmSprite *main = slot(c, s->ram_slot);
    if (!main || !(main->status & MZM_SPRITE_STATUS_EXISTS)) { s->status = 0; return; }
    if (main->pose >= AW_DYING_INIT) {
        s->samus_collision = SSC_NONE;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        if (s->pose != AW_PART_DYING) {
            s->pose = AW_PART_DYING;
            s->health = 0;
            s->ignore_samus_timer = 1;
            return;
        }
        handle_rotation(c, s);
        if (main->isft & 0x7F) {
            s->palette_row = main->palette_row;
            return;
        }
        if (--s->work0) return;
        uint8_t effect = s->room_slot == AW_WEAK_POINT ?
                         MZM_PE_SPRITE_EXPLOSION_BIG : MZM_PE_SPRITE_EXPLOSION_MEDIUM;
        if (s->room_slot == AW_PARTS) {
            c->f->globals->sub1.work2 = 0;
            ai_particle_set(c, (uint16_t)(main->y_spawn + BLOCK_SIZE + HALF_BLOCK_SIZE),
                            main->x_spawn, MZM_PE_SPRITE_EXPLOSION_BIG);
            ai_particle_set(c, (uint16_t)(main->y_spawn + BLOCK_SIZE),
                            main->x_spawn, MZM_PE_SPRITE_EXPLOSION_BIG);
            change_pair(c, (uint16_t)(main->y_spawn + BLOCK_SIZE), main->x_spawn);
            set_event(c, MZM_EVENT_ACID_WORM_KILLED);
            door_timer(c, -20);
            mzm_sfx_music_fade(270);
        }
        ai_sprite_death_full(c, DEATH_NORMAL, s->y, s->x, false, effect);
        return;
    }
    if (s->pose == AW_PART_MOVING) {
        if (main->pose == AW_SPAWN_EXTEND) {
            uint16_t old_y = s->y;
            s->y = (uint16_t)(s->y - QUARTER_BLOCK_SIZE);
            if (!(s->room_slot & 1)) splash_if_crossed(c, old_y, s->y);
        } else if (main->pose == AW_SPAWN_RETRACT) {
            uint16_t old_y = s->y;
            wiggle_horizontally(c, s);
            s->y = (uint16_t)(s->y + QUARTER_BLOCK_SIZE);
            if (!(s->room_slot & 1)) splash_if_crossed(c, old_y, s->y);
        } else if (main->pose == AW_SPAWN_TOP) {
            wiggle_horizontally(c, s);
        } else if (main->pose == AW_IDLE_INIT) {
            s->pose = AW_PART_IDLE;
            s->status |= MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE;
            s->scaling = 256;
            s->rotation = 0;
        }
        if (main->pose > AW_CHECK_SPAWN)
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        transfer_part_damage(c, main);
        return;
    }
    if (main->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
        s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
        s->rotation = (uint8_t)(main->rotation - s->work0);
    } else {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
        s->rotation = (uint8_t)(main->rotation + s->work0);
    }
    if (!(main->status & MZM_SPRITE_STATUS_FACING_DOWN)) {
        uint16_t old_y = s->y;
        handle_rotation(c, s);
        if ((main->pose == AW_EXTENDING || main->pose == AW_RETRACTING) &&
            !(s->room_slot & 1))
            splash_if_crossed(c, old_y, s->y);
    }
    transfer_part_damage(c, main);
    if (s->room_slot == AW_WEAK_POINT && main->rotation != 0)
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    else
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
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
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->bg_priority_override = (uint8_t)(c->world->bg1_priority + 1);
        ai_set_anim(s, MZM_ACID_WORM_SPIT_EXPLODING);
        mzm_sfx_play(MZM_SOUND_ACID_WORM_SPIT_EXPLODING);
        return;
    }
    if (s->pose == AW_SPIT_EXPLODING || s->pose == AW_SPIT_ACID) {
        if (s->pose == AW_SPIT_ACID) s->y = c->world->effect_y;
        s->ignore_samus_timer = 1;
        if (ai_animation_ending(c, 1)) s->status = 0;
        return;
    }
    const int16_t *velocity;
    unsigned count;
    int speed;
    if (s->room_slot == 2) {
        velocity = c->tables->acid_worm_spit_close;
        count = 16;
        speed = 2;
    } else if (s->room_slot == 1) {
        velocity = c->tables->acid_worm_spit_far;
        count = 26;
        speed = 10;
    } else {
        velocity = c->tables->acid_worm_spit_middle;
        count = 20;
        speed = 6;
    }
    unsigned offset = s->work3;
    if (offset >= count) offset = count - 1;
    int16_t y_movement = velocity[offset];
    if (y_movement == SHORT_MAX) y_movement = velocity[offset ? offset - 1 : 0];
    else s->work3++;
    s->x = (uint16_t)(s->x + ((s->status & MZM_SPRITE_STATUS_X_FLIP) ? speed : -speed));
    s->y = (uint16_t)(s->y + y_movement);
    if (c->world->effect_y && s->y >= c->world->effect_y) {
        s->y = c->world->effect_y;
        s->pose = AW_SPIT_ACID;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        ai_set_anim(s, MZM_ACID_WORM_SPIT_EXPLODING);
    } else if (ai_collision_at(c, s->y, s->x) & MZM_CLIPDATA_SOLID_FLAG) {
        s->pose = SPRITE_POSE_STOPPED;
    }
}

void ai_acid_worm(Ctx *c) {
    if (!c->s->secondary) main_ai(c);
    else if (c->s->sprite_id == MZM_SSPRITE_ACID_WORM_PART) part_ai(c);
    else spit_ai(c);
}
