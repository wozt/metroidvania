/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's elevator statues (src/sprites_ai/elevator_statue.c):
 * the Kraid and Ridley statues by the Brinstar and Norfair elevators that
 * crumble once their boss is dead, leaving rubble Samus can stand on. */
#include "mzm_sprite_ai.h"
#include "mzm_blocks.h"
#include "mzm_screen_shake.h"

enum {
    POSE_CHECK_FALL = 0x8,
    POSE_DELAY_BEFORE_FALLING = 0x9,
    POSE_FALLING = 0xB,
    POSE_IDLE = 0xF,

    DEBRIS_PART_KRAID = 0,
    DEBRIS_PART_RIDLEY = 1,
    DEBRIS_PART_DUST = 2,
    DEBRIS_POSE_IDLE = 0x9,
};

static void apply_caa(const Ctx *c, uint16_t y, uint16_t x, uint8_t caa) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->apply_caa) h->apply_caa(h->context, y, x, caa);
}

static bool is_kraid(const MzmSprite *s) { return s->sprite_id == MZM_PSPRITE_KRAID_ELEVATOR_STATUE; }

/* KraidElevatorStatueChangeCAA / RidleyElevatorStatueChangeCcaa: the
 * statue's blocks (Kraid's head sticks out a block to the left). */
static void statue_blocks(Ctx *c, uint8_t caa) {
    MzmSprite *s = c->s;
    uint16_t y = (uint16_t)(s->y - HALF_BLOCK_SIZE);
    if (is_kraid(s)) {
        uint16_t x = (uint16_t)(s->x - BLOCK_SIZE);
        apply_caa(c, (uint16_t)(y - BLOCK_SIZE * 2), (uint16_t)(x - BLOCK_SIZE), caa);
        for (int i = 3; i <= 5; ++i) apply_caa(c, (uint16_t)(y - BLOCK_SIZE * i), x, caa);
    } else {
        uint16_t x = (uint16_t)(s->x - BLOCK_SIZE * 2);
        for (int i = 2; i <= 5; ++i) apply_caa(c, (uint16_t)(y - BLOCK_SIZE * i), x, caa);
    }
}

/* ElevatorStatueChangeTwoGroundCcaa: the rubble's two blocks. */
static void rubble_blocks(Ctx *c, uint8_t caa) {
    MzmSprite *s = c->s;
    uint16_t y = (uint16_t)(s->y - HALF_BLOCK_SIZE), x = (uint16_t)(s->x - BLOCK_SIZE);
    apply_caa(c, y, (uint16_t)(x - BLOCK_SIZE), caa);
    apply_caa(c, y, (uint16_t)(x - BLOCK_SIZE * 2), caa);
}

/* Kraid/RidleyElevatorStatueFallenInit */
static void fallen_init(Ctx *c) {
    MzmSprite *s = c->s;
    if (is_kraid(s)) {
        s->draw_top = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->draw_bottom = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    } else {
        /* Natively written in subpixels, not converted to pixels. */
        s->draw_top = HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        s->draw_bottom = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    }
    s->hitbox_top = -(THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE * 3);
    s->hitbox_bottom = 0;
    s->hitbox_left = -(BLOCK_SIZE * 4);
    s->hitbox_right = BLOCK_SIZE + HALF_BLOCK_SIZE;
    ai_set_anim(s, is_kraid(s) ? MZM_ELEVATOR_KRAID_FALLEN : MZM_ELEVATOR_RIDLEY_FALLEN);
    s->pose = POSE_IDLE;
    rubble_blocks(c, MZM_CAA_MAKE_NON_POWER_GRIP);
}

/* Kraid/RidleyElevatorStatueInit: standing until its boss dies. */
static void init(Ctx *c) {
    MzmSprite *s = c->s;
    s->draw_horizontal = BLOCK_SIZE * 5 / PIXEL_SIZE;
    s->bg_priority_override = 1 + 1;                  /* bgPriority = 1 */
    s->draw_order = 2;
    s->always_active = true;
    s->solid_for_projectiles = true;
    s->samus_collision = SSC_NONE;
    if (ai_check_event(c, is_kraid(s) ? MZM_EVENT_KRAID_ELEVATOR_STATUE_DESTROYED
                                      : MZM_EVENT_RIDLEY_ELEVATOR_STATUE_DESTROYED)) {
        fallen_init(c);
        return;
    }
    s->draw_top = BLOCK_SIZE * 6 / PIXEL_SIZE;
    s->draw_bottom = 0;
    s->hitbox_top = -(BLOCK_SIZE * 6);
    s->hitbox_bottom = 0;
    s->hitbox_left = -BLOCK_SIZE;
    s->hitbox_right = BLOCK_SIZE;
    ai_set_anim(s, is_kraid(s) ? MZM_ELEVATOR_KRAID_IDLE : MZM_ELEVATOR_RIDLEY_IDLE);
    s->pose = POSE_CHECK_FALL;
    statue_blocks(c, MZM_CAA_MAKE_NON_POWER_GRIP);
}

/* The crumbling: the boss's debris, the draw height shrinking, the dust
 * and a shake on landing. */
static void falling(Ctx *c) {
    MzmSprite *s = c->s;
    bool kraid = is_kraid(s);
    switch (s->frame) {
    case 1:
        if (s->adc == 3)
            ai_spawn_secondary(c, MZM_SSPRITE_ELEVATOR_STATUE_DEBRIS, kraid ? DEBRIS_PART_KRAID : DEBRIS_PART_RIDLEY,
                               s->gfx_slot, s->y, s->x, 0);
        break;
    case 4:
        s->draw_top = (uint8_t)(kraid ? BLOCK_SIZE * 4 / PIXEL_SIZE : (BLOCK_SIZE * 5 + HALF_BLOCK_SIZE) / PIXEL_SIZE);
        s->draw_bottom = (uint8_t)(kraid ? BLOCK_SIZE / PIXEL_SIZE : HALF_BLOCK_SIZE / PIXEL_SIZE);
        break;
    case 5:
        if (!kraid) {
            s->draw_top = BLOCK_SIZE * 5 / PIXEL_SIZE;
            s->draw_bottom = BLOCK_SIZE / PIXEL_SIZE;
        }
        break;
    case 6:
        s->draw_top = (uint8_t)(kraid ? BLOCK_SIZE * 3 / PIXEL_SIZE : (BLOCK_SIZE * 4 + HALF_BLOCK_SIZE) / PIXEL_SIZE);
        s->draw_bottom = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        if (s->adc == 2) {
            mzm_screen_shake_start_vertical(20, 0x81);
            ai_spawn_secondary(c, MZM_SSPRITE_ELEVATOR_STATUE_DEBRIS, DEBRIS_PART_DUST, 0, s->y, s->x, 0);
        }
        break;
    default: break;
    }
    if (ai_animation_ending(c, 1)) fallen_init(c);
}

static void statue(Ctx *c) {
    MzmSprite *s = c->s;
    bool kraid = is_kraid(s);
    s->ignore_samus_timer = 1;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: init(c); break;
    case POSE_CHECK_FALL:
        /* Once its boss is dead, when Samus is off the elevator. */
        if (!c->f->samus_using_elevator &&
            ai_check_event(c, kraid ? MZM_EVENT_KRAID_KILLED : MZM_EVENT_RIDLEY_KILLED)) {
            s->pose = POSE_DELAY_BEFORE_FALLING;
            s->work0 = 16;                             /* CONVERT_SECONDS(.25f) + 1 */
        }
        break;
    case POSE_DELAY_BEFORE_FALLING:
        if (--s->work0 == 0) {
            ai_set_anim(s, kraid ? MZM_ELEVATOR_KRAID_FALLING : MZM_ELEVATOR_RIDLEY_FALLING);
            s->pose = POSE_FALLING;
            const MzmSpriteHooks *h = c->f->hooks;
            if (h && h->set_event)
                h->set_event(h->context, kraid ? MZM_EVENT_KRAID_ELEVATOR_STATUE_DESTROYED
                                               : MZM_EVENT_RIDLEY_ELEVATOR_STATUE_DESTROYED);
            statue_blocks(c, MZM_CAA_REMOVE_SOLID);
            mzm_screen_shake_start_horizontal(10, 0x81);
            mzm_sfx_play(kraid ? MZM_SOUND_KRAID_ELEVATOR_STATUE_CRUMBLING
                               : MZM_SOUND_RIDLEY_ELEVATOR_STATUE_CRUMBLING);
        }
        break;
    case POSE_FALLING: falling(c); break;
    default: break;
    }
}

/* ElevatorStatueDebris: the falling head or the dust, once. */
static void debris(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->always_active = true;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        s->pose = DEBRIS_POSE_IDLE;
        s->samus_collision = SSC_NONE;
        s->bg_priority_override = 1 + 1;
        if (s->room_slot == DEBRIS_PART_DUST) {
            s->draw_order = 1;
            s->draw_top = (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
            s->draw_bottom = BLOCK_SIZE / PIXEL_SIZE;
            s->draw_horizontal = BLOCK_SIZE * 5 / PIXEL_SIZE;
            ai_set_anim(s, MZM_ELEVATOR_DEBRIS_DUST);
        } else {
            s->draw_order = 5;
            s->draw_top = BLOCK_SIZE * 6 / PIXEL_SIZE;
            s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
            s->draw_horizontal = BLOCK_SIZE * 2 / PIXEL_SIZE;
            ai_set_anim(s, s->room_slot == DEBRIS_PART_KRAID ? MZM_ELEVATOR_DEBRIS_KRAID : MZM_ELEVATOR_DEBRIS_RIDLEY);
        }
        return;
    }
    if (ai_animation_ending(c, 1)) s->status = 0;
}

void ai_elevator_statue(Ctx *c) {
    if (c->s->secondary) {
        if (c->s->sprite_id == MZM_SSPRITE_ELEVATOR_STATUE_DEBRIS) debris(c);
        else c->s->status = 0;
        return;
    }
    statue(c);
}
