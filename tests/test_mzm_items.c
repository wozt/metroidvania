/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free regression tests for the Zero Mission tank pickups and the
 * message banner. */
#include "mzm_items.h"
#include "mzm_message_banner.h"
#include "mzm_sfx.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* sTankBehaviors, sStartingHealthAmmo, the game total and
 * sTankIncreaseAmount[DIFF_NORMAL] as the export gives them. */
static MzmTankTables tables(void) {
    MzmTankTables t;
    memset(&t, 0, sizeof t);
    static const uint8_t types[MZM_TANK_BEHAVIORS] = {0, 0, 0, 0, 2, 1, 3, 4, 2, 1, 3, 4};
    static const int16_t messages[MZM_TANK_BEHAVIORS] = {-1, -1, -1, -1, 1, 2, 4, 6, 1, 2, 4, 6};
    for (int i = 0; i < MZM_TANK_BEHAVIORS; ++i) {
        t.item_type[i] = types[i];
        t.underwater[i] = i >= 8;
        t.message[i] = messages[i];
    }
    const int start[4] = {99, 0, 0, 0}, total[4] = {12, 50, 15, 9}, increase[4] = {100, 5, 2, 2};
    memcpy(t.start, start, sizeof start);
    memcpy(t.total, total, sizeof total);
    memcpy(t.increase, increase, sizeof increase);
    t.air_clip = 0;
    t.water_clip = 0x43C;
    return t;
}

static void test_tanks(void) {
    MzmTankTables t = tables();
    MzmTankCollection collection = {0};
    MzmEquipment equipment = {.energy = 40, .max_energy = 99};
    /* Nothing touched. */
    MzmTouchProbes probes = {{0, 0, 0, 0}, {1, 2, 3, 3}, {5, 5, 6, 4}};
    assert(mzm_tank_touch(&t, &collection, &probes, &equipment, NULL, NULL) == -1 && !collection.collecting);
    /* A hidden tank is not a tank yet. */
    probes.behaviors[2] = MZM_TANK_BEHAVIOR_START;
    assert(mzm_tank_touch(&t, &collection, &probes, &equipment, NULL, NULL) == -1);
    /* An energy tank at the right probe: 100 more energy, refilled. */
    probes.behaviors[0] = MZM_TANK_BEHAVIOR_START + 4;
    assert(mzm_tank_touch(&t, &collection, &probes, &equipment, NULL, NULL) ==
               MZM_MESSAGE_ENERGY_TANK_ACQUIRED &&
           collection.collecting && collection.x == 1 && collection.y == 5 &&
           equipment.max_energy == 199 && equipment.energy == 199);
    /* While collecting, nothing else is taken. */
    assert(mzm_tank_touch(&t, &collection, &probes, &equipment, NULL, NULL) == -1 && equipment.max_energy == 199);
    /* The last probe is checked first; the first missile tank has its own
     * message. */
    collection.collecting = false;
    probes.behaviors[3] = MZM_TANK_BEHAVIOR_START + 5;
    assert(mzm_tank_touch(&t, &collection, &probes, &equipment, NULL, NULL) == MZM_MESSAGE_FIRST_MISSILE_TANK &&
           collection.x == 3 && collection.y == 4 && equipment.max_missiles == 5 &&
           equipment.missiles == 5 && equipment.max_energy == 199);
    collection.collecting = false;
    assert(mzm_tank_touch(&t, &collection, &probes, &equipment, NULL, NULL) == MZM_MESSAGE_MISSILE_TANK_ACQUIRED &&
           equipment.max_missiles == 10);
    /* At the game total, the tank is still collected but adds nothing. */
    collection.collecting = false;
    equipment.max_missiles = equipment.missiles = 250;
    assert(mzm_tank_touch(&t, &collection, &probes, &equipment, NULL, NULL) == MZM_MESSAGE_MISSILE_TANK_ACQUIRED &&
           collection.collecting && equipment.max_missiles == 250);
    /* Underwater tanks leave water; others air. */
    assert(mzm_tank_cleared_clip(&t, MZM_TANK_BEHAVIOR_START + 9) == 0x43C);
    assert(mzm_tank_cleared_clip(&t, MZM_TANK_BEHAVIOR_START + 5) == 0);
    assert(mzm_tank_item_type(&t, MZM_TANK_BEHAVIOR_START + 7) == MZM_ITEM_TYPE_POWER_BOMB);
    assert(mzm_tank_item_type(&t, 3) == MZM_ITEM_TYPE_NONE);
    /* gItemsCollected */
    static MzmCollectedItems items;
    mzm_items_set_collected(&items, 2, 7, 12, 4, MZM_ITEM_TYPE_ENERGY);
    assert(items.count[2] == 1 && items.items[2][0].room == 7 && items.items[2][0].x == 12 &&
           items.items[2][0].y == 4 && items.items[2][0].type == MZM_ITEM_TYPE_ENERGY);
    mzm_items_set_collected(&items, 9, 7, 12, 4, MZM_ITEM_TYPE_ENERGY);
    assert(items.count[2] == 1);
}

/* Frames until the banner reaches `pose`. */
static int run_until(MzmMessageBanner *b, const MzmBannerKind *kind, uint8_t pose, bool buttons,
                     uint16_t *prevent, unsigned *events) {
    int frames = 0;
    while (b->exists && b->pose != pose && frames < 2000) {
        *events |= mzm_banner_update(b, kind, buttons, 0, true, prevent);
        frames++;
    }
    return frames;
}

static void test_banner(void) {
    static MzmBannerKind kind;
    memset(&kind, 0, sizeof kind);
    const uint8_t spawn[] = {1, 1, 2, 3};
    memcpy(kind.timers[MZM_BANNER_TWO_LINES_SPAWN], spawn, sizeof spawn);
    memcpy(kind.timers[MZM_BANNER_ONE_LINE_SPAWN], spawn, sizeof spawn);
    kind.timers[MZM_BANNER_TWO_LINES_STATIC][0] = 255;
    kind.timers[MZM_BANNER_ONE_LINE_STATIC][0] = 255;
    const uint8_t removing[] = {3, 1};
    memcpy(kind.timers[MZM_BANNER_TWO_LINES_REMOVING], removing, sizeof removing);
    memcpy(kind.timers[MZM_BANNER_ONE_LINE_REMOVING], removing, sizeof removing);
    kind.text_calls[MZM_MESSAGE_ENERGY_TANK_ACQUIRED] = 10;
    kind.text_calls[MZM_MESSAGE_FIRST_MISSILE_TANK] = 6;

    MzmMessageBanner b;
    uint16_t prevent = 0;
    unsigned events = 0;
    mzm_banner_spawn(&b, MZM_MESSAGE_ENERGY_TANK_ACQUIRED);
    assert(b.exists && !b.drawn);
    /* MessageBannerInit, then nine frames of MessageBannerGfxInit. */
    mzm_banner_update(&b, &kind, false, 0, true, &prevent);
    assert(b.pose == MZM_BANNER_POSE_GFX_INIT && prevent == MZM_SAMUS_ITEM_PMT && b.timer == 9);
    assert(run_until(&b, &kind, MZM_BANNER_POSE_POP_UP, false, &prevent, &events) == 9);
    /* Ten TextProcessMessageBanner calls with the animation held, then the
     * banner shows with the tank jingle. */
    mzm_sfx_clear();
    for (int i = 0; i < 9; ++i) mzm_banner_update(&b, &kind, false, 0, true, &prevent);
    assert(b.processing && !b.drawn && b.frame == 0 && b.adc == 1);
    mzm_banner_update(&b, &kind, false, 0, true, &prevent);
    assert(!b.processing && b.drawn && b.table == MZM_BANNER_TWO_LINES_SPAWN && !b.new_item &&
           mzm_sfx_count() == 2 && mzm_sfx_get(0).action == MZM_SFX_BACKUP_TRACK2 &&
           mzm_sfx_requested(MZM_SFX_PLAY, MZM_MUSIC_GETTING_TANK_JINGLE));
    /* The spawn animation (1 + 1 + 2 + 3 frames), then 100 static frames
     * before an input can close it. */
    int frames = run_until(&b, &kind, MZM_BANNER_POSE_STATIC, true, &prevent, &events);
    assert(frames == 7 && b.table == MZM_BANNER_TWO_LINES_STATIC && b.timer == 100);
    for (int i = 0; i < 100; ++i) mzm_banner_update(&b, &kind, true, 0, true, &prevent);
    assert(b.pose == MZM_BANNER_POSE_STATIC && b.timer == 0 && prevent == MZM_SAMUS_ITEM_PMT);
    mzm_banner_update(&b, &kind, false, 0, true, &prevent);
    assert(b.pose == MZM_BANNER_POSE_STATIC);
    mzm_banner_update(&b, &kind, true, 0, true, &prevent);
    assert(b.pose == MZM_BANNER_POSE_REMOVAL_INIT);
    /* MessageBannerRemovalInit finishes the tank collection. */
    events = mzm_banner_update(&b, &kind, false, 0, true, &prevent);
    assert(events & MZM_BANNER_FINISH_TANK && b.table == MZM_BANNER_TWO_LINES_REMOVING);
    events = 0;
    mzm_sfx_clear();
    frames = run_until(&b, &kind, 0xFF, false, &prevent, &events);
    assert(!b.exists && events & MZM_BANNER_REMOVED && prevent == 0 && frames == 4 &&
           mzm_sfx_count() == 1 && mzm_sfx_get(0).action == MZM_SFX_RETRIEVE_TRACK2);

    /* A first tank: one-line banner, the item jingle inserted in front of
     * the music, a new item; its removal retrieves nothing. */
    mzm_banner_spawn(&b, MZM_MESSAGE_FIRST_MISSILE_TANK);
    mzm_sfx_clear();
    events = 0;
    run_until(&b, &kind, MZM_BANNER_POSE_STATIC, false, &prevent, &events);
    assert(b.new_item && b.table == MZM_BANNER_ONE_LINE_STATIC && mzm_sfx_count() == 2 &&
           mzm_sfx_get(0).action == MZM_SFX_BACKUP_TRACK2 &&
           mzm_sfx_get(1).action == MZM_SFX_MUSIC_INSERT &&
           mzm_sfx_get(1).sound == MZM_MUSIC_GETTING_ITEM_JINGLE && mzm_sfx_get(1).timer == 0);
    mzm_sfx_clear();
    run_until(&b, &kind, 0xFF, true, &prevent, &events);
    assert(!b.exists && !mzm_sfx_requested(MZM_SFX_RETRIEVE_TRACK2, 0));

    /* Items: the unknown ones have their own jingle. */
    const uint8_t items[] = {MZM_MESSAGE_LONG_BEAM, MZM_MESSAGE_UNKNOWN_ITEM_PLASMA,
                             MZM_MESSAGE_UNKNOWN_ITEM_GRAVITY, MZM_MESSAGE_UNKNOWN_ITEM_SPACE_JUMP,
                             MZM_MESSAGE_POWER_GRIP};
    for (size_t i = 0; i < sizeof items; ++i) {
        mzm_banner_spawn(&b, items[i]);
        mzm_sfx_clear();
        run_until(&b, &kind, MZM_BANNER_POSE_STATIC, false, &prevent, &events);
        bool unknown = i >= 1 && i <= 3;
        assert(b.new_item && mzm_sfx_count() == 2 &&
               mzm_sfx_get(0).action == MZM_SFX_BACKUP_TRACK2 &&
               mzm_sfx_requested(MZM_SFX_MUSIC_INSERT, unknown ? MZM_MUSIC_GETTING_UNKNOWN_ITEM_JINGLE
                                                              : MZM_MUSIC_GETTING_ITEM_JINGLE));
    }

    /* The fully powered suit: the Brinstar remix behind its own jingle. */
    mzm_banner_spawn(&b, MZM_MESSAGE_FULLY_POWERED_SUIT);
    mzm_sfx_clear();
    run_until(&b, &kind, MZM_BANNER_POSE_STATIC, false, &prevent, &events);
    assert(!b.new_item && mzm_sfx_count() == 2 &&
           mzm_sfx_get(0).action == MZM_SFX_MUSIC_PLAY &&
           mzm_sfx_get(0).sound == MZM_MUSIC_BRINSTAR_REMIX &&
           mzm_sfx_requested(MZM_SFX_MUSIC_INSERT, MZM_MUSIC_GETTING_FULLY_POWERED_SUIT_JINGLE));

    /* A map: the tank jingle without a backup; the save prompt: silent. */
    mzm_banner_spawn(&b, MZM_MESSAGE_BRINSTAR_MAP_ACQUIRED);
    mzm_sfx_clear();
    run_until(&b, &kind, MZM_BANNER_POSE_STATIC, false, &prevent, &events);
    assert(mzm_sfx_count() == 1 && mzm_sfx_requested(MZM_SFX_PLAY, MZM_MUSIC_GETTING_TANK_JINGLE));
    mzm_banner_spawn(&b, MZM_MESSAGE_SAVE_PROMPT);
    mzm_sfx_clear();
    run_until(&b, &kind, MZM_BANNER_POSE_STATIC, false, &prevent, &events);
    assert(mzm_sfx_count() == 0);
    /* Its cursor starts on YES; RIGHT moves it to NO, A refuses: the
     * banner goes, without a save. */
    assert(b.yes && b.cursor.exists);
    kind.timers[MZM_BANNER_SAVE_CURSOR][0] = 4;
    mzm_banner_update(&b, &kind, false, 0, true, &prevent);
    assert(b.cursor.pose == 0x9 && b.cursor.x == 54 && b.cursor.y == 63);
    mzm_banner_update(&b, &kind, false, MZM_KEY_LEFT, true, &prevent);
    assert(mzm_sfx_count() == 0);
    mzm_banner_update(&b, &kind, false, MZM_KEY_RIGHT, true, &prevent);
    assert(b.cursor.x == 150 && mzm_sfx_requested(MZM_SFX_PLAY, 0x207));
    events = mzm_banner_update(&b, &kind, false, MZM_KEY_A, true, &prevent);
    assert(!b.yes && !b.cursor.exists && b.pose == MZM_BANNER_POSE_REMOVAL_INIT && !(events & MZM_BANNER_SAVE) &&
           mzm_sfx_requested(MZM_SFX_PLAY, 0x209));
    /* YES: the cursor hides, the next update saves. */
    mzm_banner_spawn(&b, MZM_MESSAGE_SAVE_PROMPT);
    mzm_sfx_clear();
    run_until(&b, &kind, MZM_BANNER_POSE_STATIC, false, &prevent, &events);
    mzm_banner_update(&b, &kind, false, 0, true, &prevent);
    events = mzm_banner_update(&b, &kind, false, MZM_KEY_A, true, &prevent);
    assert(b.yes && b.cursor.exists && b.cursor.pose == 0x43 && !(events & MZM_BANNER_SAVE) &&
           mzm_sfx_requested(MZM_SFX_PLAY, 0x208));
    events = mzm_banner_update(&b, &kind, false, 0, true, &prevent);
    assert(events & MZM_BANNER_SAVE && !b.cursor.exists);
    /* The save complete message re-enables the pause when it goes. */
    mzm_banner_spawn(&b, MZM_MESSAGE_SAVE_COMPLETE);
    events = 0;
    run_until(&b, &kind, MZM_BANNER_POSE_STATIC, false, &prevent, &events);
    run_until(&b, &kind, 0xFF, true, &prevent, &events);
    assert(!b.exists && events & MZM_BANNER_ENABLE_PAUSE && b.pose == MZM_BANNER_POSE_REMOVAL_ANIMATION);
}

int main(void) {
    test_tanks();
    test_banner();
    puts("mzm_items: ok");
    return 0;
}
