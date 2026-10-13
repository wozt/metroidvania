/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's gunship (src/sprites_ai/gunship.c): the landing of a new
 * game, Samus entering it, the refill, the save prompt and the save, Samus
 * leaving it, and the escape take-off; its parts (the entrance, the
 * platform, the flames). Comments name the pinned decompilation's
 * routines. */
#include "mzm_message_banner.h"
#include "mzm_samus.h"
#include "mzm_sprite_ai.h"

enum {
    SSPRITE_GUNSHIP_PART = 50,
    MESSAGE_SAVE_PROMPT = 22, MESSAGE_SAVE_COMPLETE = 23, MESSAGE_WEAPONS_AND_ENERGY_RESTORED = 26,
    EVENT_MOTHER_BRAIN_KILLED = 39, EVENT_ESCAPED_ZEBES = 65,
    /* EffectCutscene (constants/color_fading.h) */
    EFFECT_CUTSCENE_EXITING_ZEBES = 1, EFFECT_CUTSCENE_INTRO_TEXT = 3,
    SOUND_GUNSHIP_OPENING = 0x118, SOUND_GUNSHIP_CLOSING = 0x119, SOUND_GUNSHIP_PLATFORM_MOVING = 0x11A,
    SOUND_ESCAPE_BEEP = 0x120, SOUND_GUNSHIP_REFILL = 0x21E, SOUND_GUNSHIP_SAVING = 0x21F,
    SOUND_GUNSHIP_LANDING = 0x232, SOUND_GUNSHIP_FLYING_OFF = 0x234,
    GUNSHIP_POSE_IDLE = 0x9, GUNSHIP_POSE_CHECK_ESCAPE = 0xF, GUNSHIP_POSE_SAMUS_ENTERING_WHEN_ESCAPING = 0x11,
    GUNSHIP_POSE_START_ESCAPING = 0x13, GUNSHIP_POSE_TAKING_OFF = 0x15, GUNSHIP_POSE_FLYING = 0x17,
    GUNSHIP_POSE_DO_NOTHING_ESCAPE = 0x19, GUNSHIP_POSE_SAMUS_ENTERING = 0x22, GUNSHIP_POSE_REFILL = 0x23,
    GUNSHIP_POSE_AFTER_REFILL = 0x24, GUNSHIP_POSE_SAMUS_LEAVE = 0x25, GUNSHIP_POSE_SAMUS_LEAVING = 0x27,
    GUNSHIP_POSE_RELEASE_SAMUS = 0x28, GUNSHIP_POSE_SAMUS_RELEASED = 0x29,
    GUNSHIP_POSE_CHECK_SAMUS_ON_TOP_AFTER_RELEASED = 0x2A, GUNSHIP_POSE_SAVING = 0x42,
    GUNSHIP_POSE_AFTER_SAVE = 0x43, GUNSHIP_POSE_LANDING = 0x55, GUNSHIP_POSE_UNKNOWN = 0x57,
    GUNSHIP_POSE_DO_NOTHING = 0x59,
    GUNSHIP_PART_MAIN = 0, GUNSHIP_PART_ENTRANCE_FRONT, GUNSHIP_PART_ENTRANCE_BACK, GUNSHIP_PART_PLATFORM,
    GUNSHIP_PART_FLAMES_HORIZONTAL, GUNSHIP_PART_FLAMES_VERTICAL,
    GUNSHIP_PART_POSE_ENTRANCE_FRONT_IDLE = 0x8, GUNSHIP_PART_POSE_ENTRANCE_FRONT_OPENING_CLOSING = 0x9,
    GUNSHIP_PART_POSE_ENTRANCE_BACK_IDLE = 0xE, GUNSHIP_PART_POSE_ENTRANCE_BACK_OPENING_CLOSING = 0xF,
    GUNSHIP_PART_POSE_CHECK_SET_VERTICAL_FLAMES = 0x32, GUNSHIP_PART_POSE_VERTICAL_FLAME_IDLE = 0x33,
    GUNSHIP_PART_POSE_CHECK_SET_HORIZONTAL_FLAMES = 0x34,
    GUNSHIP_PART_POSE_CHECK_SET_VERTICAL_FLAMES_BEFORE_LANDING = 0x35,
    GUNSHIP_PART_POSE_CHECK_SET_HORIZONTAL_FLAMES_BEFORE_LANDING = 0x36,
    GUNSHIP_PART_POSE_FLICKER_FLAMES_UNUSED = 0x37, GUNSHIP_PART_POSE_PLATFORM_CHECK_GO_UP = 0x42,
    GUNSHIP_PART_POSE_PLATFORM_GO_UP = 0x43, GUNSHIP_PART_POSE_PLATFORM_CHECK_GO_DOWN = 0x44,
    GUNSHIP_PART_POSE_PLATFORM_GO_DOWN = 0x45,
    /* CONVERT_SECONDS */
    ONE_THIRD_SECOND = 20, TWO_THIRD_SECOND = 40,
};

static void kill(MzmSprite *s) { s->status = 0; }

static MzmSpriteGlobals *globals(const Ctx *c) { return c->f->globals; }
static bool near_flag(const Ctx *c) { return c->f->globals && c->f->globals->sub1.work3; }

static void spawn_part(Ctx *c, uint8_t part, uint16_t y) {
    ai_spawn_secondary(c, SSPRITE_GUNSHIP_PART, part, c->s->gfx_slot, y, c->s->x, 0);
}

static void spawn_message(const Ctx *c, uint8_t message) {
    if (c->f->hooks && c->f->hooks->spawn_message) c->f->hooks->spawn_message(c->f->hooks->context, message);
}

/* SpriteUtilFindSecondaryWithRoomSlot */
static MzmSprite *find_part(const Ctx *c, uint8_t part) {
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        MzmSprite *s = &c->f->sprites[i];
        if (s->status & MZM_SPRITE_STATUS_EXISTS && s->secondary && s->sprite_id == SSPRITE_GUNSHIP_PART &&
            s->room_slot == part)
            return s;
    }
    return NULL;
}

/* SamusSetPose with gSamusData.timer, through the Samus link. */
static void set_samus_pose(const Ctx *c, uint8_t pose, int timer) {
    MzmSpriteSamusLink *link = c->f->samus_link;
    if (!link) return;
    if (pose != MZM_POSE_NONE) {
        link->set_pose = pose;
        link->samus_pose = pose;
    }
    if (timer >= 0) {
        link->set_timer = true;
        link->timer = (uint8_t)timer;
    }
}

static void move_samus(const Ctx *c, int dy) {
    MzmSpriteSamusLink *link = c->f->samus_link;
    if (!link) return;
    link->y = (uint16_t)(link->y + dy);
    link->moved = true;
}

/* GunshipFlickerFlames: the flames' palette, every entrance flash. */
static void flicker_flames(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->status & MZM_SPRITE_STATUS_MOSAIC || s->work2 != 4) return;    /* CONVERT_SECONDS(1.f / 15) */
    uint8_t row = (uint8_t)s->scaling;
    if (row & 0x80) {
        if (row > 0x80) s->scaling--;
        if (s->scaling == 0x80) s->scaling = 0;
    } else {
        if (row < 2) s->scaling++;
        if (s->scaling == 2) s->scaling |= 0x80;
    }
    if (globals(c)) globals(c)->gunship_flames_palette = (int8_t)(s->scaling & 0x7F);
}

/* GunshipEntranceFlashingAnim: the entrance lights' palette. */
static void entrance_flashing(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->status & MZM_SPRITE_STATUS_MOSAIC || --s->work2 != 0) return;
    if (s->anim == MZM_GADGET_GUNSHIP_REFILLING) s->work2 = 2;          /* CONVERT_SECONDS(1.f / 30) */
    else if (s->anim == MZM_GADGET_GUNSHIP_FLYING) s->work2 = 4;        /* CONVERT_SECONDS(1.f / 15) */
    else s->work2 = 8;                                                  /* CONVERT_SECONDS(2.f / 15) */
    uint8_t row = s->work1;
    if (row & 0x80) {
        if (s->work1 > 0x80) s->work1--;
        if (s->work1 == 0x80) s->work1 = 0;
    } else {
        if (s->work1 < 4) s->work1++;
        if (s->work1 == 4) s->work1 |= 0x80;
    }
    if (globals(c)) globals(c)->gunship_entrance_palette = (int8_t)(s->work1 & 0x7F);
}

/* GunshipCheckSamusEnter */
static bool check_samus_enter(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteSamusLink *link = c->f->samus_link;
    if (c->f->samus_crouching_or_morphed || !link) return false;
    if (s->x - THREE_QUARTER_BLOCK_SIZE < link->x && s->x + THREE_QUARTER_BLOCK_SIZE > link->x) {
        set_samus_pose(c, MZM_POSE_TURNING_FROM_FACING_THE_FOREGROUND, 1);
        link->standing_enemy = true;
        link->x = s->x;
        link->moved = true;
        s->work0 = 56;                                  /* CONVERT_SECONDS(.9f + 1.f / 30) */
        s->samus_collision = SSC_NONE;
        s->standing_on = MZM_STANDING_ON_SPRITE_OFF;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_ON_TOP;
        return true;
    }
    return false;
}

/* GunshipInit */
static void gunship_init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = globals(c);
    MzmSpriteSamusLink *link = c->f->samus_link;
    bool intro = false;
    if (g && g->landing) {
        intro = true;
        s->y = (uint16_t)(s->y - BLOCK_SIZE * 6);
        mzm_sfx_play(SOUND_GUNSHIP_LANDING);
    }
    s->always_active = true;
    s->hitbox_top = -(BLOCK_SIZE * 3 + PIXEL_SIZE);
    s->hitbox_bottom = 0;
    s->hitbox_left = -BLOCK_SIZE;
    s->hitbox_right = BLOCK_SIZE;
    s->draw_top = BLOCK_SIZE * 3 / PIXEL_SIZE;
    s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
    s->draw_horizontal = (BLOCK_SIZE * 7 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    ai_set_anim(s, MZM_GADGET_GUNSHIP_IDLE);
    s->work2 = 8;                                       /* CONVERT_SECONDS(2.f / 15) */
    s->work1 = s->work0 = s->work3 = 0;
    if (g) {
        g->sub1.work3 = false;
        g->gunship_entrance_palette = g->gunship_flames_palette = -1;
    }
    spawn_part(c, GUNSHIP_PART_ENTRANCE_FRONT, s->y);
    spawn_part(c, GUNSHIP_PART_ENTRANCE_BACK, s->y);
    if (link && link->samus_pose == MZM_POSE_SAVING_LOADING_GAME) {
        /* Loading a game saved in the gunship */
        s->samus_collision = SSC_NONE;
        s->pose = GUNSHIP_POSE_SAMUS_LEAVE;
        s->work0 = 30;                                  /* CONVERT_SECONDS(.5f) */
        set_samus_pose(c, MZM_POSE_NONE, 0);
        link->set_last_wall = true;
        link->last_wall = 1;
        s->y_spawn = 5 * BLOCK_SIZE + QUARTER_BLOCK_SIZE + PIXEL_SIZE;
        if (g) g->sub1.work3 = true;
        spawn_part(c, GUNSHIP_PART_PLATFORM, s->y);
    } else if (intro) {
        /* Landing */
        s->samus_collision = SSC_NONE;
        s->pose = GUNSHIP_POSE_LANDING;
        s->work0 = 192;                                 /* CONVERT_SECONDS(3.2f) */
        set_samus_pose(c, MZM_POSE_SAVING_LOADING_GAME, 1);
        if (link) {
            link->set_last_wall = true;
            link->last_wall = 1;
        }
        spawn_part(c, GUNSHIP_PART_FLAMES_VERTICAL, s->y);
        mzm_sfx_music_priority(0);
        if (g) g->hide_hud = true;
    } else {
        /* Idle */
        spawn_part(c, GUNSHIP_PART_PLATFORM, (uint16_t)(s->y - BLOCK_SIZE * 3 + HALF_BLOCK_SIZE));
        s->y_spawn = 0;
        s->samus_collision = SSC_CAN_STAND_ON_TOP;
        s->pose = ai_check_event(c, EVENT_MOTHER_BRAIN_KILLED) ? GUNSHIP_POSE_CHECK_ESCAPE : GUNSHIP_POSE_IDLE;
        s->draw_order = 12;
    }
}

/* GunshipLanding */
static void gunship_landing(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->work0 == 0) return;
    s->work0--;
    s->y = (uint16_t)(s->y + PIXEL_SIZE / 2);
    MzmSpriteSamusLink *link = c->f->samus_link;
    if (link) {
        link->y = (uint16_t)(s->y - PIXEL_SIZE);
        link->x = s->x;
        link->moved = true;
    }
    if (s->work0 == 30) {                               /* CONVERT_SECONDS(.5f) */
        /* Start intro */
        if (globals(c)) globals(c)->cutscene_effect = EFFECT_CUTSCENE_INTRO_TEXT;
        s->status |= MZM_SPRITE_STATUS_MOSAIC;
    }
}

/* GunshipIdle */
static void gunship_idle(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->status & MZM_SPRITE_STATUS_SAMUS_ON_TOP) {
        s->draw_order = 4;
        if (check_samus_enter(c)) s->pose = GUNSHIP_POSE_SAMUS_ENTERING;
    } else {
        s->draw_order = 12;
    }
}

/* The entrance closing (GunshipSamusEntering and its escape twin). */
static bool close_entrance(Ctx *c) {
    MzmSprite *front = find_part(c, GUNSHIP_PART_ENTRANCE_FRONT);
    if (!front) return false;
    ai_set_anim(front, MZM_GADGET_GUNSHIP_ENTRANCE_FRONT_CLOSING);
    front->pose = GUNSHIP_PART_POSE_ENTRANCE_FRONT_OPENING_CLOSING;
    MzmSprite *back = find_part(c, GUNSHIP_PART_ENTRANCE_BACK);
    if (!back) return false;
    ai_set_anim(back, MZM_GADGET_GUNSHIP_ENTRANCE_BACK_CLOSING);
    back->pose = GUNSHIP_PART_POSE_ENTRANCE_BACK_OPENING_CLOSING;
    return true;
}

/* GunshipSamusEntering */
static void gunship_samus_entering(Ctx *c) {
    MzmSprite *s = c->s;
    if (--s->work0 == 0) {
        s->pose = GUNSHIP_POSE_REFILL;
        s->work0 = 65;                                  /* CONVERT_SECONDS(1.f + 1.f / 12) */
        if (close_entrance(c)) {
            mzm_sfx_play(SOUND_GUNSHIP_CLOSING);
            mzm_sfx_fade(SOUND_GUNSHIP_PLATFORM_MOVING, 10);
        }
    } else if (s->work0 < TWO_THIRD_SECOND + 4) {
        move_samus(c, PIXEL_SIZE);
    } else if (s->work0 == TWO_THIRD_SECOND + 4) {
        mzm_sfx_play(SOUND_GUNSHIP_PLATFORM_MOVING);
    }
}

/* SpriteUtilRefill*: a fraction of what is missing, between 1 and `cap`. */
static bool refill(int *current, int max, int divisor, int cap) {
    if (*current >= max) return false;
    int step = (max - *current) / divisor;
    if (step == 0) step = 1;
    else if (step >= cap) step = cap;
    *current += step;
    if (*current > max) *current = max;
    return true;
}

/* GunshipRefill */
static void gunship_refill(Ctx *c) {
    MzmSprite *s = c->s;
    MzmEquipment *e = c->f->equipment;
    uint8_t none[4] = {0}, *anim = globals(c) ? globals(c)->refill_animation : none;
    if (s->work0 > 6) {                                 /* CONVERT_SECONDS(.1f) */
        if (--s->work0 == 6) {
            ai_set_anim(s, MZM_GADGET_GUNSHIP_REFILLING);
            mzm_sfx_play(SOUND_GUNSHIP_REFILL);
        }
    } else if (s->work0 == 5) {
        if (!e || !refill(&e->energy, e->max_energy, 32, 11)) {
            s->work0--;
            anim[0] = 13;
        }
    } else if (s->work0 == 4) {
        if (anim[0]) anim[0]--;
        else if (!e || !refill(&e->missiles, e->max_missiles, 32, 6)) {
            s->work0--;
            if (e && e->max_missiles) anim[1] = 13;
        }
    } else if (s->work0 == 3) {
        if (anim[1]) anim[1]--;
        else if (!e || !refill(&e->super_missiles, e->max_super_missiles, 16, 4)) {
            s->work0--;
            if (e && e->max_super_missiles) anim[2] = 13;
        }
    } else if (s->work0 == 2) {
        if (anim[2]) anim[2]--;
        else if (!e || !refill(&e->power_bombs, e->max_power_bombs, 16, 4)) {
            s->work0--;
            if (e && e->max_power_bombs) anim[3] = 13;
        }
    } else if (anim[3]) {
        anim[3]--;
    } else if (s->work0 != 0) {
        s->work0--;
    } else {
        s->pose = GUNSHIP_POSE_AFTER_REFILL;
        s->work0 = 30;
        spawn_message(c, MESSAGE_WEAPONS_AND_ENERGY_RESTORED);
        mzm_sfx_fade(SOUND_GUNSHIP_REFILL, 15);       /* CONVERT_SECONDS(.25f) */
    }
}

/* GunshipAfterRefill: the save prompt once the message is out. */
static void gunship_after_refill(Ctx *c) {
    MzmSprite *s = c->s;
    if (c->f->banner_pose != MZM_BANNER_POSE_REMOVAL_ANIMATION) return;
    if (s->work0 != 0) {
        if (--s->work0 == 0) spawn_message(c, MESSAGE_SAVE_PROMPT);
    } else if (c->f->banner_yes) {
        s->pose = GUNSHIP_POSE_SAVING;
        s->work0 = 30;
        mzm_sfx_play(SOUND_GUNSHIP_SAVING);
    } else {
        s->pose = GUNSHIP_POSE_SAMUS_LEAVE;
        s->work0 = 30;
    }
}

/* GunshipSamusLeave: the entrance opens, then the platform rises. */
static void gunship_samus_leave(Ctx *c) {
    MzmSprite *s = c->s;
    if (--s->work0 == 0) {
        s->pose = GUNSHIP_POSE_SAMUS_LEAVING;
        s->work0 = TWO_THIRD_SECOND + 4;
        mzm_sfx_play(SOUND_GUNSHIP_PLATFORM_MOVING);
    } else if (s->work0 == 10) {                        /* CONVERT_SECONDS(1.f / 6) */
        MzmSprite *front = find_part(c, GUNSHIP_PART_ENTRANCE_FRONT);
        if (!front) return;
        ai_set_anim(front, MZM_GADGET_GUNSHIP_ENTRANCE_FRONT_OPENING);
        MzmSprite *back = find_part(c, GUNSHIP_PART_ENTRANCE_BACK);
        if (!back) return;
        ai_set_anim(back, MZM_GADGET_GUNSHIP_ENTRANCE_BACK_OPENING);
        ai_set_anim(s, MZM_GADGET_GUNSHIP_IDLE);
        mzm_sfx_play(SOUND_GUNSHIP_OPENING);
    }
}

/* GunshipSamusLeaving */
static void gunship_samus_leaving(Ctx *c) {
    MzmSprite *s = c->s;
    if (--s->work0 == 0) {
        if (globals(c) && globals(c)->hide_hud) {
            s->pose = GUNSHIP_POSE_DO_NOTHING;
            s->work0 = 50;                              /* CONVERT_SECONDS(5.f / 6) */
        } else {
            s->pose = GUNSHIP_POSE_RELEASE_SAMUS;
        }
        mzm_sfx_fade(SOUND_GUNSHIP_PLATFORM_MOVING, 10);
    } else {
        move_samus(c, -PIXEL_SIZE);
    }
}

/* GunshipCheckEscapeZebes */
static void gunship_check_escape(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->status & MZM_SPRITE_STATUS_SAMUS_ON_TOP && check_samus_enter(c)) {
        s->draw_order = 4;
        s->pose = GUNSHIP_POSE_SAMUS_ENTERING_WHEN_ESCAPING;
        const MzmSpriteHooks *h = c->f->hooks;
        if (h && h->set_event) h->set_event(h->context, EVENT_ESCAPED_ZEBES);
        mzm_sfx_fade(SOUND_ESCAPE_BEEP, 60);
        mzm_sfx_music_priority(0);
    }
}

/* GunshipSamusEnteringWhenEscaping */
static void gunship_samus_entering_escaping(Ctx *c) {
    MzmSprite *s = c->s;
    if (--s->work0 == 0) {
        s->pose = GUNSHIP_POSE_START_ESCAPING;
        s->work0 = 120;                                 /* CONVERT_SECONDS(2.f) */
        if (close_entrance(c)) {
            MzmSprite *platform = find_part(c, GUNSHIP_PART_PLATFORM);
            if (platform) {
                kill(platform);
                mzm_sfx_play(SOUND_GUNSHIP_CLOSING);
                mzm_sfx_fade(SOUND_GUNSHIP_PLATFORM_MOVING, 10);
            }
        }
    } else if (s->work0 < TWO_THIRD_SECOND + 4) {
        move_samus(c, PIXEL_SIZE);
    } else if (s->work0 == TWO_THIRD_SECOND + 4) {
        mzm_sfx_play(SOUND_GUNSHIP_PLATFORM_MOVING);
    }
}

/* GunshipStartEscaping */
static void gunship_start_escaping(Ctx *c) {
    MzmSprite *s = c->s;
    if (--s->work0 == 0) {
        s->pose = GUNSHIP_POSE_TAKING_OFF;
        s->work0 = 152;                                 /* CONVERT_SECONDS(2.5f) + 2 * DELTA_TIME */
        s->work3 = 0;
        s->scaling = 0;
        spawn_part(c, GUNSHIP_PART_FLAMES_HORIZONTAL, s->y);
    } else if (s->work0 == 4) {
        ai_set_anim(s, MZM_GADGET_GUNSHIP_FLYING);
        mzm_sfx_play(SOUND_GUNSHIP_FLYING_OFF);
    }
}

/* One step of sGunshipTakingOffYVelocity / sGunshipFlyingYVelocity, which
 * end with SHORT_MAX (the previous velocity then repeats). */
static void velocity_step(Ctx *c, const int16_t *table, int length) {
    MzmSprite *s = c->s;
    int offset = s->work3 < length ? s->work3 : length - 1;
    int movement = table[offset];
    if (movement == SHORT_MAX) movement = offset > 0 ? table[offset - 1] : 0;
    else s->work3++;
    s->y = (uint16_t)(s->y + movement);
    move_samus(c, movement);
}

/* GunshipTakingOff */
static void gunship_taking_off(Ctx *c) {
    MzmSprite *s = c->s;
    flicker_flames(c);
    if (--s->work0 == 0) {
        s->pose = GUNSHIP_POSE_FLYING;
        s->work0 = 160;                                 /* CONVERT_SECONDS(2) + TWO_THIRD_SECOND */
        s->work3 = 0;
    } else {
        velocity_step(c, c->tables->gunship_taking_off, 153);
    }
}

/* GunshipFlying */
static void gunship_flying(Ctx *c) {
    MzmSprite *s = c->s;
    if (--s->work0 == 0) {
        s->pose = GUNSHIP_POSE_DO_NOTHING_ESCAPE;
        return;
    }
    if (s->work0 == 50) {                               /* CONVERT_SECONDS(5.f / 6) */
        if (globals(c)) globals(c)->cutscene_effect = EFFECT_CUTSCENE_EXITING_ZEBES;
        s->status |= MZM_SPRITE_STATUS_MOSAIC;
    }
    flicker_flames(c);
    velocity_step(c, c->tables->gunship_flying, 22);
}

/* Gunship */
static void gunship(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = globals(c);
    MzmSpriteSamusLink *link = c->f->samus_link;
    s->ignore_samus_timer = 0;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: gunship_init(c); break;
    case GUNSHIP_POSE_IDLE: gunship_idle(c); break;
    case GUNSHIP_POSE_SAMUS_ENTERING: gunship_samus_entering(c); break;
    case GUNSHIP_POSE_REFILL: gunship_refill(c); break;
    case GUNSHIP_POSE_AFTER_REFILL: gunship_after_refill(c); break;
    case GUNSHIP_POSE_SAMUS_LEAVE: gunship_samus_leave(c); break;
    case GUNSHIP_POSE_SAMUS_LEAVING: gunship_samus_leaving(c); break;
    case GUNSHIP_POSE_RELEASE_SAMUS:
        /* GunshipReleaseSamus */
        if (s->y_spawn == 0) {
            set_samus_pose(c, MZM_POSE_FACING_THE_FOREGROUND, -1);
            s->pose = GUNSHIP_POSE_SAMUS_RELEASED;
        }
        break;
    case GUNSHIP_POSE_SAMUS_RELEASED:
        /* GunshipSamusReleased */
        if (link && link->samus_pose != MZM_POSE_FACING_THE_FOREGROUND) {
            s->pose = GUNSHIP_POSE_CHECK_SAMUS_ON_TOP_AFTER_RELEASED;
            s->samus_collision = SSC_CAN_STAND_ON_TOP;
        }
        break;
    case GUNSHIP_POSE_CHECK_SAMUS_ON_TOP_AFTER_RELEASED:
        /* GunshipCheckSamusOnTopAfterLeaving */
        if (!(s->status & MZM_SPRITE_STATUS_SAMUS_ON_TOP)) {
            s->draw_order = 12;
            s->pose = GUNSHIP_POSE_IDLE;
        }
        break;
    case GUNSHIP_POSE_SAVING:
        /* GunshipSaving */
        if (--s->work0 == 0) {
            s->pose = GUNSHIP_POSE_AFTER_SAVE;
            spawn_message(c, MESSAGE_SAVE_COMPLETE);
        }
        break;
    case GUNSHIP_POSE_AFTER_SAVE:
        /* GunshipAfterSave */
        if (c->f->banner_pose == MZM_BANNER_POSE_REMOVAL_ANIMATION) {
            s->pose = GUNSHIP_POSE_SAMUS_LEAVE;
            s->work0 = 30;
        }
        break;
    case GUNSHIP_POSE_CHECK_ESCAPE: gunship_check_escape(c); break;
    case GUNSHIP_POSE_SAMUS_ENTERING_WHEN_ESCAPING: gunship_samus_entering_escaping(c); break;
    case GUNSHIP_POSE_START_ESCAPING: gunship_start_escaping(c); break;
    case GUNSHIP_POSE_TAKING_OFF: gunship_taking_off(c); break;
    case GUNSHIP_POSE_FLYING: gunship_flying(c); break;
    case GUNSHIP_POSE_LANDING: gunship_landing(c); break;
    default: break;
    }
    /* Samus near: the entrance opens and the platform goes up. */
    if (g) {
        if (!g->sub1.work3) {
            if (ai_samus_nslr(c, BLOCK_SIZE * 20, BLOCK_SIZE * 7) != NSLR_OUT_OF_RANGE) g->sub1.work3 = true;
        } else if (ai_samus_nslr(c, BLOCK_SIZE * 20, BLOCK_SIZE * 9) == NSLR_OUT_OF_RANGE) {
            g->sub1.work3 = false;
        }
    }
    entrance_flashing(c);
    if (!ai_check_event(c, EVENT_ESCAPED_ZEBES) && s->y_spawn != 0) {
        if (--s->y_spawn == 0 && g) g->disable_pause = false;
    }
}

/* GunshipPartInit */
static void part_init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteSamusLink *link = c->f->samus_link;
    bool loading = link && link->samus_pose == MZM_POSE_SAVING_LOADING_GAME;
    s->always_active = true;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->adc = s->frame = 0;
    s->samus_collision = SSC_NONE;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
    switch (s->room_slot) {
    case GUNSHIP_PART_ENTRANCE_FRONT:
        s->draw_top = (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->anim = MZM_GADGET_GUNSHIP_ENTRANCE_FRONT_CLOSED;
        s->pose = loading ? GUNSHIP_PART_POSE_ENTRANCE_FRONT_OPENING_CLOSING : GUNSHIP_PART_POSE_ENTRANCE_FRONT_IDLE;
        break;
    case GUNSHIP_PART_ENTRANCE_BACK:
        s->draw_top = BLOCK_SIZE * 4 / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->draw_order = 14;
        s->anim = MZM_GADGET_GUNSHIP_ENTRANCE_BACK_CLOSED;
        s->pose = loading ? GUNSHIP_PART_POSE_ENTRANCE_BACK_OPENING_CLOSING : GUNSHIP_PART_POSE_ENTRANCE_BACK_IDLE;
        break;
    case GUNSHIP_PART_PLATFORM:
        s->draw_top = 0;
        s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_order = 13;
        s->anim = MZM_GADGET_GUNSHIP_PLATFORM;
        if (loading) {
            s->pose = GUNSHIP_PART_POSE_PLATFORM_CHECK_GO_DOWN;
            s->y = (uint16_t)(s->y - (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE));
        } else {
            s->pose = GUNSHIP_PART_POSE_PLATFORM_CHECK_GO_UP;
        }
        break;
    case GUNSHIP_PART_FLAMES_HORIZONTAL:
        s->draw_top = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_bottom = BLOCK_SIZE * 3 / PIXEL_SIZE;
        s->draw_horizontal = BLOCK_SIZE * 6 / PIXEL_SIZE;
        s->draw_order = 15;
        s->anim = MZM_GADGET_GUNSHIP_FLAMES_HORIZONTAL;
        s->pose = GUNSHIP_PART_POSE_CHECK_SET_VERTICAL_FLAMES;
        break;
    case GUNSHIP_PART_FLAMES_VERTICAL:
        s->draw_top = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_bottom = BLOCK_SIZE * 3 / PIXEL_SIZE;
        s->draw_horizontal = BLOCK_SIZE * 6 / PIXEL_SIZE;
        s->draw_order = 15;
        s->anim = MZM_GADGET_GUNSHIP_FLAMES_VERTICAL;
        s->pose = GUNSHIP_PART_POSE_CHECK_SET_HORIZONTAL_FLAMES;
        s->work0 = 60 + ONE_THIRD_SECOND;               /* CONVERT_SECONDS(1.f) + ONE_THIRD_SECOND */
        break;
    default:
        kill(s);
        break;
    }
}

/* GunshipPartEntranceFrontIdle / GunshipPartEntranceBackIdle: opens while
 * Samus is near, else closes (the back part plays the sounds). */
static void entrance_idle(Ctx *c, bool back) {
    MzmSprite *s = c->s;
    uint8_t closed = back ? MZM_GADGET_GUNSHIP_ENTRANCE_BACK_CLOSED : MZM_GADGET_GUNSHIP_ENTRANCE_FRONT_CLOSED;
    uint8_t opening = back ? MZM_GADGET_GUNSHIP_ENTRANCE_BACK_OPENING : MZM_GADGET_GUNSHIP_ENTRANCE_FRONT_OPENING;
    uint8_t opened = back ? MZM_GADGET_GUNSHIP_ENTRANCE_BACK_OPENED : MZM_GADGET_GUNSHIP_ENTRANCE_FRONT_OPENED;
    uint8_t closing = back ? MZM_GADGET_GUNSHIP_ENTRANCE_BACK_CLOSING : MZM_GADGET_GUNSHIP_ENTRANCE_FRONT_CLOSING;
    if (near_flag(c)) {
        if (s->anim == closed) {
            ai_set_anim(s, opening);
            if (back) ai_sound_onscreen(s, SOUND_GUNSHIP_OPENING);
        } else if (s->anim == opening) {
            if (ai_animation_ending(c, 1)) ai_set_anim(s, opened);
        } else if (s->anim == closing) {
            if (ai_animation_ending(c, 1)) {
                ai_set_anim(s, opening);
                if (back) mzm_sfx_play(SOUND_GUNSHIP_OPENING);
            }
        }
    } else {
        if (s->anim == opened) {
            ai_set_anim(s, closing);
            if (back) mzm_sfx_play(SOUND_GUNSHIP_CLOSING);
        } else if (s->anim == closing) {
            if (ai_animation_ending(c, 1)) ai_set_anim(s, closed);
        } else if (s->anim == opening) {
            if (ai_animation_ending(c, 1)) {
                ai_set_anim(s, closing);
                if (back) mzm_sfx_play(SOUND_GUNSHIP_CLOSING);
            }
        }
    }
}

/* GunshipPartEntranceFrontOpenClose / GunshipPartEntranceBackOpenClose */
static void entrance_open_close(Ctx *c, bool back) {
    MzmSprite *s = c->s;
    if (s->anim == (back ? MZM_GADGET_GUNSHIP_ENTRANCE_BACK_CLOSING : MZM_GADGET_GUNSHIP_ENTRANCE_FRONT_CLOSING)) {
        if (ai_animation_ending(c, 1))
            ai_set_anim(s, back ? MZM_GADGET_GUNSHIP_ENTRANCE_BACK_CLOSED : MZM_GADGET_GUNSHIP_ENTRANCE_FRONT_CLOSED);
    } else if (s->anim ==
               (back ? MZM_GADGET_GUNSHIP_ENTRANCE_BACK_OPENING : MZM_GADGET_GUNSHIP_ENTRANCE_FRONT_OPENING)) {
        if (ai_animation_ending(c, 1)) {
            ai_set_anim(s, back ? MZM_GADGET_GUNSHIP_ENTRANCE_BACK_OPENED : MZM_GADGET_GUNSHIP_ENTRANCE_FRONT_OPENED);
            s->pose = back ? GUNSHIP_PART_POSE_ENTRANCE_BACK_IDLE : GUNSHIP_PART_POSE_ENTRANCE_FRONT_IDLE;
        }
    }
}

/* GunshipPart */
static void gunship_part(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSprite *main = c->f->sprites && s->ram_slot < c->f->sprite_count ? &c->f->sprites[s->ram_slot] : NULL;
    s->ignore_samus_timer = 1;
    if (s->room_slot == GUNSHIP_PART_ENTRANCE_FRONT && main) s->draw_order = main->draw_order == 4 ? 3 : 11;
    if (s->room_slot != GUNSHIP_PART_PLATFORM && main) {
        s->y = main->y;
        s->x = main->x;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: part_init(c); break;
    case GUNSHIP_PART_POSE_ENTRANCE_FRONT_IDLE: entrance_idle(c, false); break;
    case GUNSHIP_PART_POSE_ENTRANCE_FRONT_OPENING_CLOSING: entrance_open_close(c, false); break;
    case GUNSHIP_PART_POSE_ENTRANCE_BACK_IDLE: entrance_idle(c, true); break;
    case GUNSHIP_PART_POSE_ENTRANCE_BACK_OPENING_CLOSING: entrance_open_close(c, true); break;
    case GUNSHIP_PART_POSE_PLATFORM_CHECK_GO_UP: {
        /* GunshipPartCheckPlatformGoUp */
        MzmSprite *back = find_part(c, GUNSHIP_PART_ENTRANCE_BACK);
        if (back && near_flag(c) && back->anim == MZM_GADGET_GUNSHIP_ENTRANCE_BACK_OPENING) {
            s->pose = GUNSHIP_PART_POSE_PLATFORM_GO_UP;
            s->work0 = 8;                               /* CONVERT_SECONDS(2.f / 15) */
        }
        break;
    }
    case GUNSHIP_PART_POSE_PLATFORM_GO_UP:
        /* GunshipPartPlatformGoUp */
        if (s->work0 != 0) {
            s->work0--;
        } else if (main && main->y - (BLOCK_SIZE * 3 + PIXEL_SIZE) > s->y) {
            s->y = (uint16_t)(main->y - (BLOCK_SIZE * 3 + PIXEL_SIZE));
            s->pose = GUNSHIP_PART_POSE_PLATFORM_CHECK_GO_DOWN;
        } else {
            s->y = (uint16_t)(s->y - PIXEL_SIZE);
        }
        break;
    case GUNSHIP_PART_POSE_PLATFORM_CHECK_GO_DOWN:
        /* GunshipPartCheckPlatformGoDown */
        if (!near_flag(c)) {
            s->pose = GUNSHIP_PART_POSE_PLATFORM_GO_DOWN;
            s->work0 = 8;
        } else if (main && main->work0 < TWO_THIRD_SECOND + 4 &&
                   (main->pose == GUNSHIP_POSE_SAMUS_ENTERING ||
                    main->pose == GUNSHIP_POSE_SAMUS_ENTERING_WHEN_ESCAPING)) {
            s->y = (uint16_t)(s->y + PIXEL_SIZE);
        } else if (main && main->pose == GUNSHIP_POSE_SAMUS_LEAVING && main->work0 < TWO_THIRD_SECOND + 4) {
            s->y = (uint16_t)(s->y - PIXEL_SIZE);
        }
        break;
    case GUNSHIP_PART_POSE_PLATFORM_GO_DOWN:
        /* GunshipPartPlatformGoDown */
        if (s->work0 != 0) {
            s->work0--;
        } else if (main && main->y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) < s->y) {
            s->y = (uint16_t)(main->y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE));
            s->pose = GUNSHIP_PART_POSE_PLATFORM_CHECK_GO_UP;
        } else {
            s->y = (uint16_t)(s->y + PIXEL_SIZE);
        }
        break;
    case GUNSHIP_PART_POSE_CHECK_SET_VERTICAL_FLAMES:
        /* GunshipPartCheckSetVerticalFlames */
        if (main && main->pose == GUNSHIP_POSE_FLYING) {
            ai_set_anim(s, MZM_GADGET_GUNSHIP_FLAMES_VERTICAL);
            s->pose = GUNSHIP_PART_POSE_VERTICAL_FLAME_IDLE;
        }
        break;
    case GUNSHIP_PART_POSE_CHECK_SET_HORIZONTAL_FLAMES:
        /* GunshipPartCheckSetHorizontalFlames */
        if (s->work0 != 0 && --s->work0 == 0) {
            ai_set_anim(s, MZM_GADGET_GUNSHIP_FLAMES_HORIZONTAL);
            s->work0 = 15;                              /* CONVERT_SECONDS(.25f) */
            s->pose = GUNSHIP_PART_POSE_CHECK_SET_VERTICAL_FLAMES_BEFORE_LANDING;
        }
        break;
    case GUNSHIP_PART_POSE_CHECK_SET_VERTICAL_FLAMES_BEFORE_LANDING:
        if (s->work0 != 0 && --s->work0 == 0) {
            ai_set_anim(s, MZM_GADGET_GUNSHIP_FLAMES_VERTICAL);
            s->work0 = 10;                              /* CONVERT_SECONDS(1.f / 6) */
            s->pose = GUNSHIP_PART_POSE_CHECK_SET_HORIZONTAL_FLAMES_BEFORE_LANDING;
        }
        break;
    case GUNSHIP_PART_POSE_CHECK_SET_HORIZONTAL_FLAMES_BEFORE_LANDING:
        if (s->work0 != 0 && --s->work0 == 0) ai_set_anim(s, MZM_GADGET_GUNSHIP_FLAMES_HORIZONTAL);
        if (main && main->pose == GUNSHIP_POSE_UNKNOWN) {
            s->pose = GUNSHIP_PART_POSE_FLICKER_FLAMES_UNUSED;
            s->work0 = 60;
        }
        break;
    case GUNSHIP_PART_POSE_FLICKER_FLAMES_UNUSED:
        s->status ^= MZM_SPRITE_STATUS_NOT_DRAWN;
        if (--s->work0 == 0) kill(s);
        break;
    default:
        break;
    }
}

void ai_gunship(Ctx *c) {
    if (c->s->secondary) gunship_part(c);
    else gunship(c);
}
