/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission message banner; comments name the pinned decompilation's
 * routines. */
#include "mzm_message_banner.h"
#include "mzm_items.h"
#include "mzm_sfx.h"

#include <string.h>

const char *const mzm_banner_tables[MZM_BANNER_TABLES] = {
    "MessageBannerOam_TwoLinesSpawn", "MessageBannerOam_TwoLinesStatic",
    "MessageBannerOam_TwoLinesRemoving", "MessageBannerOam_OneLineSpawn",
    "MessageBannerOam_OneLineStatic", "MessageBannerOam_OneLineRemoving",
};

static bool is_item(uint8_t m) {
    return m >= MZM_MESSAGE_LONG_BEAM && m <= MZM_MESSAGE_POWER_GRIP;
}
static bool is_first_tank(uint8_t m) {
    return m == MZM_MESSAGE_FIRST_MISSILE_TANK || m == MZM_MESSAGE_FIRST_SUPER_MISSILE_TANK ||
           m == MZM_MESSAGE_FIRST_POWER_BOMB_TANK;
}
static bool is_unknown_item(uint8_t m) {
    return m == MZM_MESSAGE_UNKNOWN_ITEM_PLASMA || m == MZM_MESSAGE_UNKNOWN_ITEM_GRAVITY ||
           m == MZM_MESSAGE_UNKNOWN_ITEM_SPACE_JUMP;
}
static bool is_tank(uint8_t m) {
    return m == MZM_MESSAGE_ENERGY_TANK_ACQUIRED || m == MZM_MESSAGE_MISSILE_TANK_ACQUIRED ||
           m == MZM_MESSAGE_SUPER_MISSILE_TANK_ACQUIRED || m == MZM_MESSAGE_POWER_BOMB_TANK_ACQUIRED;
}
static bool is_map(uint8_t m) {
    return m >= MZM_MESSAGE_BRINSTAR_MAP_ACQUIRED && m <= MZM_MESSAGE_MOTHER_SHIP_MAP_ACQUIRED;
}

static uint8_t timer_of(const MzmBannerKind *kind, uint8_t table, uint8_t frame) {
    return frame < 32 ? kind->timers[table][frame] : 0;
}

/* SpriteUtilHasCurrentAnimationEnded */
static bool animation_ended(const MzmMessageBanner *b, const MzmBannerKind *kind) {
    uint8_t adc = (uint8_t)(b->adc + 1);
    return timer_of(kind, b->table, b->frame) < adc &&
           timer_of(kind, b->table, (uint8_t)(b->frame + 1)) == 0;
}

static void set_table(MzmMessageBanner *b, uint8_t table) {
    b->table = table;
    b->adc = 0;
    b->frame = 0;
}

void mzm_banner_spawn(MzmMessageBanner *banner, uint8_t message) {
    memset(banner, 0, sizeof *banner);
    banner->exists = true;
    banner->message = message;
    banner->pose = MZM_BANNER_POSE_INIT;
}

unsigned mzm_banner_update(MzmMessageBanner *b, const MzmBannerKind *kind, bool buttons,
                           bool collecting_tank, uint16_t *prevent_movement) {
    unsigned events = 0;
    if (!b->exists) return 0;
    uint8_t msg = b->message;
    switch (b->pose) {
    case MZM_BANNER_POSE_INIT:
        /* MessageBannerInit (TextStartMessage) */
        *prevent_movement = MZM_SAMUS_ITEM_PMT;
        set_table(b, MZM_BANNER_TWO_LINES_SPAWN);
        b->timer = 9;
        b->processing = true;
        b->new_item = false;
        b->text_calls = 0;
        b->pose = MZM_BANNER_POSE_GFX_INIT;
        break;
    case MZM_BANNER_POSE_GFX_INIT:
        /* MessageBannerGfxInit: SpriteLoadGfx at 7, SpriteLoadPal at 8. */
        *prevent_movement = MZM_SAMUS_ITEM_PMT;
        b->timer--;
        if (b->timer == 0) {
            b->pose = MZM_BANNER_POSE_POP_UP;
            b->adc = 0;
            b->frame = 0;
        }
        break;
    case MZM_BANNER_POSE_POP_UP:
        /* MessageBannerPopUp */
        *prevent_movement = MZM_SAMUS_ITEM_PMT;
        if (b->processing) {
            b->adc--;
            uint16_t needed = msg < MZM_MESSAGE_COUNT ? kind->text_calls[msg] : 1;
            if (++b->text_calls >= needed) {
                b->processing = false;
                b->drawn = true;
                if (is_item(msg)) {
                    b->new_item = true;
                    mzm_sfx_backup_track2();
                    mzm_sfx_music_insert(is_unknown_item(msg) ? MZM_MUSIC_GETTING_UNKNOWN_ITEM_JINGLE
                                                              : MZM_MUSIC_GETTING_ITEM_JINGLE,
                                         0);
                } else if (is_first_tank(msg)) {
                    b->new_item = true;
                    mzm_sfx_backup_track2();
                    mzm_sfx_music_insert(MZM_MUSIC_GETTING_ITEM_JINGLE, 0);
                } else if (msg == MZM_MESSAGE_FULLY_POWERED_SUIT) {
                    mzm_sfx_music_play(MZM_MUSIC_BRINSTAR_REMIX, 0);
                    mzm_sfx_music_insert(MZM_MUSIC_GETTING_FULLY_POWERED_SUIT_JINGLE, 0);
                } else if (msg != MZM_MESSAGE_SAVE_PROMPT) {
                    if (is_tank(msg)) mzm_sfx_backup_track2();
                    mzm_sfx_play(MZM_MUSIC_GETTING_TANK_JINGLE);
                }
                if (b->new_item || msg == MZM_MESSAGE_SAVE_COMPLETE || is_map(msg) ||
                    msg == MZM_MESSAGE_FULLY_POWERED_SUIT)
                    set_table(b, MZM_BANNER_ONE_LINE_SPAWN);
            }
            break;
        }
        if (animation_ended(b, kind)) {
            b->adc = 0;
            b->frame = 0;
            b->pose = MZM_BANNER_POSE_STATIC;
            if (b->table == MZM_BANNER_ONE_LINE_SPAWN) {
                b->table = MZM_BANNER_ONE_LINE_STATIC;
                /* CONVERT_SECONDS(5) + TWO_THIRD_SECOND, or 1 second + 2/3 */
                b->timer = msg == MZM_MESSAGE_FULLY_POWERED_SUIT ? 340 : 100;
            } else {
                b->table = MZM_BANNER_TWO_LINES_STATIC;
                b->timer = 100;
                /* The save prompt's cursor and the escape start are not
                 * ported. */
            }
        }
        break;
    case MZM_BANNER_POSE_STATIC:
        /* MessageBannerStatic */
        *prevent_movement = msg == MZM_MESSAGE_FULLY_POWERED_SUIT ? 0 : MZM_SAMUS_ITEM_PMT;
        if (b->timer != 0) {
            b->timer--;
            break;
        }
        if (msg != MZM_MESSAGE_SAVE_PROMPT && buttons)
            b->pose = MZM_BANNER_POSE_REMOVAL_INIT;
        break;
    case MZM_BANNER_POSE_REMOVAL_INIT:
        /* MessageBannerRemovalInit */
        if (collecting_tank) events |= MZM_BANNER_FINISH_TANK;
        set_table(b, b->table == MZM_BANNER_ONE_LINE_STATIC ? MZM_BANNER_ONE_LINE_REMOVING
                                                           : MZM_BANNER_TWO_LINES_REMOVING);
        b->pose = MZM_BANNER_POSE_REMOVAL_ANIMATION;
        break;
    case MZM_BANNER_POSE_REMOVAL_ANIMATION:
        /* MessageBannerRemovalAnimation: the item acquisition pause screen
         * is not ported. */
        if (animation_ended(b, kind)) {
            if (is_tank(msg)) mzm_sfx_retrieve_track2();
            b->exists = false;
            *prevent_movement = 0;
            events |= MZM_BANNER_REMOVED;
            return events;
        }
        break;
    }
    /* SpriteUpdateAnimation */
    b->adc++;
    if (timer_of(kind, b->table, b->frame) < b->adc) {
        b->adc = 1;
        b->frame++;
        if (timer_of(kind, b->table, b->frame) == 0) b->frame = 0;
    }
    return events;
}
