/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission message banner (src/sprites_ai/message_banner.c,
 * PSPRITE_MESSAGE_BANNER) with the animation steps SpriteUpdate gives it
 * (SpriteUpdateAnimation, SpriteUtilHasCurrentAnimationEnded). The text
 * itself is pre-rendered by the exporter; the banner only needs how many
 * TextProcessMessageBanner calls its message takes. No SDL dependency. */
#ifndef MZM_MESSAGE_BANNER_H
#define MZM_MESSAGE_BANNER_H

#include <stdbool.h>
#include <stdint.h>

/* MessageId values the banner tests. */
enum {
    MZM_MESSAGE_ENERGY_TANK_ACQUIRED = 1,
    MZM_MESSAGE_MISSILE_TANK_ACQUIRED = 2,
    MZM_MESSAGE_FIRST_MISSILE_TANK = 3,
    MZM_MESSAGE_SUPER_MISSILE_TANK_ACQUIRED = 4,
    MZM_MESSAGE_FIRST_SUPER_MISSILE_TANK = 5,
    MZM_MESSAGE_POWER_BOMB_TANK_ACQUIRED = 6,
    MZM_MESSAGE_FIRST_POWER_BOMB_TANK = 7,
    MZM_MESSAGE_LONG_BEAM = 8,
    MZM_MESSAGE_UNKNOWN_ITEM_PLASMA = 12,
    MZM_MESSAGE_UNKNOWN_ITEM_GRAVITY = 15,
    MZM_MESSAGE_UNKNOWN_ITEM_SPACE_JUMP = 20,
    MZM_MESSAGE_POWER_GRIP = 21,
    MZM_MESSAGE_SAVE_PROMPT = 22,
    MZM_MESSAGE_SAVE_COMPLETE = 23,
    MZM_MESSAGE_BRINSTAR_MAP_ACQUIRED = 27,
    MZM_MESSAGE_MOTHER_SHIP_MAP_ACQUIRED = 31,
    MZM_MESSAGE_FULLY_POWERED_SUIT = 32,
    MZM_MESSAGE_COUNT = 39
};

/* The banner's OAM tables (sMessageBannerOam_*). */
enum {
    MZM_BANNER_TWO_LINES_SPAWN, MZM_BANNER_TWO_LINES_STATIC, MZM_BANNER_TWO_LINES_REMOVING,
    MZM_BANNER_ONE_LINE_SPAWN, MZM_BANNER_ONE_LINE_STATIC, MZM_BANNER_ONE_LINE_REMOVING,
    MZM_BANNER_TABLES
};
/* Their names without the leading `s`. */
extern const char *const mzm_banner_tables[MZM_BANNER_TABLES];

/* MESSAGE_BANNER_POSE_* */
enum {
    MZM_BANNER_POSE_INIT = 0, MZM_BANNER_POSE_GFX_INIT = 0x8, MZM_BANNER_POSE_POP_UP = 0x9,
    MZM_BANNER_POSE_STATIC = 0x23, MZM_BANNER_POSE_REMOVAL_INIT = 0x24,
    MZM_BANNER_POSE_REMOVAL_ANIMATION = 0x25
};

typedef struct {
    /* Frame durations of each table, 0-terminated. */
    uint8_t timers[MZM_BANNER_TABLES][32];
    /* TextProcessMessageBanner calls until it reports its line, by message. */
    uint16_t text_calls[MZM_MESSAGE_COUNT];
} MzmBannerKind;

typedef struct {
    bool exists;
    bool drawn;                 /* !SPRITE_STATUS_NOT_DRAWN */
    uint8_t message;            /* roomSlot */
    uint8_t pose;
    uint16_t timer;             /* MESSAGE_BANNER_TIMER (yPositionSpawn) */
    bool processing;            /* MESSAGE_BANNER_PROCESSING */
    bool new_item;              /* MESSAGE_BANNER_NEW_ITEM */
    uint8_t table, frame, adc;
    uint16_t text_calls;        /* TextProcessMessageBanner calls so far */
} MzmMessageBanner;

/* Events of one update. */
enum {
    MZM_BANNER_FINISH_TANK = 1 << 0,    /* BgClipFinishCollectingTank */
    MZM_BANNER_REMOVED = 1 << 1,        /* gPreventMovementTimer = 0 */
    /* BgClipFinishCollectingAbility, when the item message's text ends
     * (stage 3 of its text processing). */
    MZM_BANNER_FINISH_ABILITY = 1 << 2,
};

/* SpriteSpawnPrimary(PSPRITE_MESSAGE_BANNER, message, ...): spawned not
 * drawn; its AI runs from the next SpriteUpdate. */
void mzm_banner_spawn(MzmMessageBanner *banner, uint8_t message);
/* One SpriteUpdate of the banner: MessageBanner, then
 * SpriteUpdateAnimation. `buttons` is gButtonInput's A, B and direction
 * keys; `collecting_tank` is gCollectingTank; `prevent_movement` is
 * gPreventMovementTimer. The banner's sounds go through mzm_sfx. Returns
 * MZM_BANNER_* events. The banner is never part of a spriteset here, so it
 * always loads its graphics (MESSAGE_BANNER_POSE_GFX_INIT). */
unsigned mzm_banner_update(MzmMessageBanner *banner, const MzmBannerKind *kind, bool buttons,
                           bool collecting_tank, uint16_t *prevent_movement);

#endif /* MZM_MESSAGE_BANNER_H */
