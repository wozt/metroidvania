/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's scripted Chozodia sprites: the pillar that falls after
 * the Fully Powered Suit (src/sprites_ai/falling_chozo_pillar.c) and the
 * Power Bomb theft seen during the suitless sequence
 * (src/sprites_ai/space_pirate_carrying_power_bomb.c). */
#include "mzm_sprite_ai.h"
#include "mzm_screen_shake.h"

enum {
    PILLAR_POSE_CHECK_SUIT_ANIM_ENDED = 0x9,
    PILLAR_POSE_CHECK_ON_SCREEN = 0x23,
    PILLAR_POSE_FALLING = 0x25,
    PILLAR_POSE_FALLEN = 0x27,
    PILLAR_POSE_IDLE = 0x29,

    PIRATE_POSE_SPAWNING = 0x9,
    PIRATE_POSE_MOVING = 0x23,

    FAKE_POWER_BOMB_POSE_IDLE = 0x9,
    TRIGGER_POSE_IDLE = 0x8,

    /* RUINS_TEST_FIGHT_STAGE_SUIT_ANIM_ENDED (gSubSpriteData1.work3). */
    RUINS_TEST_SUIT_ANIM_ENDED = 8,

    /* The room in the ruins during the suitless sequence, and the room
     * near the first Power Bomb tank. */
    RUINS_ROOM = 32,
    POWER_BOMB_ROOM = 46,
};

/* FallingChozoPillar: once the suit animation ends and it is on screen,
 * it sinks 256 subpixels with a shake and dust every 16 frames. */
static void falling_pillar(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
        s->x = (uint16_t)(s->x + HALF_BLOCK_SIZE);
        s->hitbox_top = -(BLOCK_SIZE * 4);
        s->hitbox_bottom = 0;
        s->hitbox_left = -(BLOCK_SIZE * 2);
        s->hitbox_right = BLOCK_SIZE * 2;
        s->draw_top = BLOCK_SIZE * 4 / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->samus_collision = SSC_NONE;
        if (c->f->world) s->bg_priority_override = (uint8_t)(c->f->world->bg1_priority + 1);
        ai_set_anim(s, MZM_CHOZODIA_PILLAR_FALLING);
        s->pose = PILLAR_POSE_CHECK_SUIT_ANIM_ENDED;
        s->y_spawn = 256;                              /* CONVERT_SECONDS(4.25f) + 1 */
        s->work0 = 0;
        break;
    case PILLAR_POSE_CHECK_SUIT_ANIM_ENDED:
        if (c->f->globals && c->f->globals->sub1.work3 == RUINS_TEST_SUIT_ANIM_ENDED) {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
            s->pose = PILLAR_POSE_CHECK_ON_SCREEN;
        }
        break;
    case PILLAR_POSE_CHECK_ON_SCREEN:
        if (s->status & MZM_SPRITE_STATUS_ONSCREEN) {
            s->pose = PILLAR_POSE_FALLING;
            mzm_sfx_play(MZM_SOUND_CHOZO_PILLAR_FALLING);
        }
        break;
    case PILLAR_POSE_FALLING:
        if (s->y_spawn) {
            if ((s->work0++ & 15) == 0) {
                mzm_screen_shake_start_vertical(20, 0x81);
                uint8_t effect = c->frame8 & 1 ? MZM_PE_SECOND_MEDIUM_DUST : MZM_PE_SECOND_TWO_MEDIUM_DUST;
                ai_particle_set(c, (uint16_t)(s->y - BLOCK_SIZE * 4),
                                (uint16_t)(s->x - (BLOCK_SIZE - EIGHTH_BLOCK_SIZE) + c->rng * EIGHTH_BLOCK_SIZE),
                                effect);
            }
            s->y_spawn--;
            s->y++;
        } else {
            s->pose = PILLAR_POSE_FALLEN;
            mzm_screen_shake_start_vertical(60, 0x81);
            s->work0 = 40;                             /* TWO_THIRD_SECOND */
        }
        break;
    case PILLAR_POSE_FALLEN:
        if (--s->work0 == 0) {
            s->pose = PILLAR_POSE_IDLE;
            mzm_sfx_play(MZM_SOUND_CHOZO_PILLAR_FELL);
        }
        break;
    default: break;
    }
}

static void set_event(const Ctx *c, uint8_t event) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->set_event) h->set_event(h->context, event);
}

/* SpacePirateCarryingPowerBomb: seen once in each of its two rooms, it
 * walks right to the next pirate zoneline and disappears. */
static void pirate_carrying_power_bomb(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t room = c->f->room;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: {
        bool seen = room == RUINS_ROOM ? ai_check_event(c, MZM_EVENT_SPACE_PIRATE_WITH_POWER_BOMB_ONE)
                  : room == POWER_BOMB_ROOM ? ai_check_event(c, MZM_EVENT_SPACE_PIRATE_WITH_POWER_BOMB_TWO)
                                            : true;
        if (seen) {
            s->status = 0;
            break;
        }
        s->draw_top = BLOCK_SIZE * 3 / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        ai_set_anim(s, MZM_CHOZODIA_PIRATE_CARRYING_POWER_BOMB);
        s->samus_collision = SSC_NONE;
        s->pose = PIRATE_POSE_SPAWNING;
        s->status |= MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_FACING_RIGHT;
        break;
    }
    case PIRATE_POSE_SPAWNING:
        if (!(s->status & MZM_SPRITE_STATUS_ONSCREEN)) break;
        if (room == RUINS_ROOM) set_event(c, MZM_EVENT_SPACE_PIRATE_WITH_POWER_BOMB_ONE);
        else if (room == POWER_BOMB_ROOM) set_event(c, MZM_EVENT_SPACE_PIRATE_WITH_POWER_BOMB_TWO);
        s->pose = PIRATE_POSE_MOVING;
        if (s->adc > 5 && (s->frame & 3) == 0) mzm_sfx_play_not_playing(MZM_SOUND_SPACE_PIRATE_FOOTSTEPS);
        break;
    case PIRATE_POSE_MOVING: {
        if (s->adc > 5 && (s->frame & 3) == 0 && s->status & MZM_SPRITE_STATUS_ONSCREEN)
            mzm_sfx_play_not_playing(MZM_SOUND_SPACE_PIRATE_FOOTSTEPS);
        uint16_t front;
        if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
            front = (uint16_t)(s->x + BLOCK_SIZE);
            s->x = (uint16_t)(s->x + PIXEL_SIZE);
        } else {
            front = (uint16_t)(s->x - BLOCK_SIZE);
            s->x = (uint16_t)(s->x - PIXEL_SIZE);
        }
        uint16_t y = (uint16_t)(s->y - BLOCK_SIZE);
        if (ai_collision_at(c, y, front) == MZM_COLLISION_SOLID &&
            ai_movement_at(c, y, front) == MZM_CLIPDATA_MOVEMENT_SPACE_PIRATE_ZONELINE)
            s->status = 0;
        break;
    }
    default: break;
    }
}

/* FakePowerBomb: the Power Bomb on its pedestal until it is stolen. */
static void fake_power_bomb(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose != SPRITE_POSE_UNINITIALIZED) return;
    if (ai_check_event(c, MZM_EVENT_POWER_BOMB_STOLEN)) {
        s->status = 0;
        return;
    }
    s->draw_top = BLOCK_SIZE / PIXEL_SIZE;
    s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
    s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
    ai_set_anim(s, MZM_CHOZODIA_FAKE_POWER_BOMB);
    s->samus_collision = SSC_NONE;
    s->pose = FAKE_POWER_BOMB_POSE_IDLE;
    s->draw_order = 5;
    s->x = (uint16_t)(s->x + QUARTER_BLOCK_SIZE);
}

/* FakePowerBombEventTrigger: touching it records the theft. */
static void fake_power_bomb_trigger(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        if (ai_check_event(c, MZM_EVENT_POWER_BOMB_STOLEN)) {
            s->status = 0;
            return;
        }
        s->status |= MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->samus_collision = SSC_CHECK_COLLIDING;
        s->draw_top = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = -BLOCK_SIZE;
        s->hitbox_bottom = 0;
        s->hitbox_left = -(BLOCK_SIZE + HALF_BLOCK_SIZE);
        s->hitbox_right = BLOCK_SIZE + HALF_BLOCK_SIZE;
        s->pose = TRIGGER_POSE_IDLE;
        /* sEnemyDropOam_LargeEnergy, never drawn. */
        ai_set_anim(s, MZM_CHOZODIA_FAKE_POWER_BOMB);
    }
    if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
        s->status = 0;
        set_event(c, MZM_EVENT_POWER_BOMB_STOLEN);
    }
}

void ai_chozodia_scripted(Ctx *c) {
    switch (c->s->sprite_id) {
    case MZM_PSPRITE_FALLING_CHOZO_PILLAR: falling_pillar(c); break;
    case MZM_PSPRITE_SPACE_PIRATE_CARRYING_POWER_BOMB: pirate_carrying_power_bomb(c); break;
    case MZM_PSPRITE_FAKE_POWER_BOMB: fake_power_bomb(c); break;
    case MZM_PSPRITE_FAKE_POWER_BOMB_EVENT_TRIGGER: fake_power_bomb_trigger(c); break;
    default: c->s->status = 0; break;
    }
}
