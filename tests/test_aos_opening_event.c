/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_opening_event.h"
#include "aos_intro_resources.h"
#include "aos_soma.h"

#include <assert.h>
#include <string.h>

#include "aos_input.h"

static void test_trigger(void) {
    assert(AOS_OPENING_SOMA_X == 168 && AOS_OPENING_SOMA_Y == 704);
    assert(AOS_OPENING_ACTOR_COUNT == 3);
    assert(AOS_OPENING_MINA_X == AOS_OPENING_SOMA_X &&
           AOS_OPENING_MINA_Y == AOS_OPENING_SOMA_Y);
    assert(AOS_OPENING_ARIKADO_X == 0x119 && AOS_OPENING_ARIKADO_Y == 0x2C0);
    assert(AOS_OPENING_SKELETON_X == 0x16F && AOS_OPENING_SKELETON_Y == 0x268);
    AosOpeningActors actors;
    aos_opening_actors_init(&actors);
    assert(actors.active && actors.state == 0 && actors.timer == 0x40);
    assert(actors.x[0] == (AOS_OPENING_MINA_X << 16));
    assert(actors.x[1] == (AOS_OPENING_ARIKADO_X << 16));
    assert(actors.animation[0] == 4 && actors.animation[1] == 1 && actors.animation[2] == 4);
    for (unsigned frame = 0; frame < 64; ++frame)
        aos_opening_actors_update(&actors, NULL);
    assert(actors.state == 1 && actors.timer == 0 && actors.x[1] == (0x119 << 16));
    aos_opening_actors_update(&actors, NULL);
    assert(actors.x[1] < (0x119 << 16));
    for (unsigned frame = 0; frame < 128; ++frame)
        aos_opening_actors_update(&actors, NULL);
    assert(actors.state == 2 && actors.x[1] == (0xD0 << 16));
    assert(actors.visible[0] && actors.visible[1] && actors.visible[2]);

    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_CONFRONTATION);
    assert(aos_opening_actors_update(&actors, NULL) ==
           AOS_OPENING_CUE_CONFRONTATION);
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_ENEMY_APPEARS);
    for (unsigned frame = 0; frame < 31; ++frame)
        assert(aos_opening_actors_update(&actors, NULL) == AOS_OPENING_CUE_NONE);
    assert(aos_opening_actors_update(&actors, NULL) ==
           AOS_OPENING_CUE_ENEMY_APPEARS);
    assert(actors.animation[0] == 5 && actors.animation_age[0] == 0);

    const uint16_t durations[AOS_OPENING_ACTOR_COUNT] = {1, 3, 1};
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_ENEMY_ATTACKS);
    assert(actors.animation[1] == 2);
    assert(aos_opening_actors_update(&actors, durations) ==
           AOS_OPENING_CUE_ENEMY_ATTACKS);
    assert(aos_opening_actors_update(&actors, durations) == AOS_OPENING_CUE_NONE);
    assert(aos_opening_actors_update(&actors, durations) == AOS_OPENING_CUE_NONE);
    for (unsigned frame = 0; frame < 15; ++frame)
        assert(aos_opening_actors_update(&actors, durations) == AOS_OPENING_CUE_NONE);
    assert(aos_opening_actors_update(&actors, durations) ==
           AOS_OPENING_CUE_ENEMY_ATTACKS_AGAIN);
    assert(actors.animation[1] == 3 && actors.animation_age[1] == 0);

    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_SOUL_ABSORBED);
    for (unsigned frame = 0; frame < 18; ++frame) {
        AosOpeningCue cue = aos_opening_actors_update(&actors, durations);
        if (frame == 17) assert(cue == AOS_OPENING_CUE_ENEMY_DEFEATED);
        else assert(cue == AOS_OPENING_CUE_NONE);
    }
    assert(!actors.visible[2]);
    for (unsigned frame = 0; frame < 64; ++frame)
        assert(aos_opening_actors_update(&actors, durations) == AOS_OPENING_CUE_NONE);
    assert(aos_opening_actors_update(&actors, durations) ==
           AOS_OPENING_CUE_SOUL_ABSORBED);
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_AFTER_BATTLE);
    assert(aos_opening_actors_update(&actors, durations) ==
           AOS_OPENING_CUE_AFTER_BATTLE);
    assert(AOS_SOMA_ANIM_OPENING_LYING == 0x51);
    assert(!strcmp(aos_opening_event_speaker_name(1), "Soma Cruz"));
    assert(!strcmp(aos_opening_event_speaker_name(2), "Mina Hakuba"));
    assert(!strcmp(aos_opening_event_speaker_name(7), "Genya Arikado"));
    assert(!strcmp(aos_opening_event_speaker_name(0), ""));
    uint32_t events[8] = {0};
    assert(aos_opening_event_should_start(0, 0, 0, events, true));
    assert(!aos_opening_event_should_start(1, 0, 0, events, true));
    assert(!aos_opening_event_should_start(0, 1, 0, events, true));
    assert(!aos_opening_event_should_start(0, 0, 1, events, true));
    assert(!aos_opening_event_should_start(0, 0, 0, events, false));
    events[1] = 1;
    assert(!aos_opening_event_should_start(0, 0, 0, events, true));
}

static void test_script_and_commit(void) {
    static const uint8_t script[] = {
        0, 0, 3, 4, 7, 2, 'A', 'B', 6, 'C', 5,
        4, 7, 8, 'D', 2, 9, 5, 9, 'E', 10,
    };
    AosOpeningEvent event;
    assert(aos_opening_event_init(&event, script, sizeof script));
    aos_opening_event_update(&event, 0);
    assert(event.portrait == 4 && event.speaker == 2);
    assert(event.glyph_counts[0] == 1 && event.glyphs[0][0] == 'A');
    for (unsigned frame = 0; frame < 16 && !event.waiting; ++frame)
        aos_opening_event_update(&event, 0);
    assert(event.waiting && event.line == 1);
    assert(event.glyph_counts[0] == 2 && event.glyph_counts[1] == 1);
    aos_opening_event_update(&event, AOS_KEY_A);
    assert(!event.waiting && event.glyph_counts[0] == 1 &&
           event.glyphs[0][0] == 'D');
    for (unsigned frame = 0; frame < 16 && !event.waiting; ++frame)
        aos_opening_event_update(&event, 0);
    assert(event.waiting && event.beat == 9 && event.speaker == 8);
    assert(event.beat_count == 1 && event.cue == AOS_OPENING_CUE_NONE);
    assert(event.glyph_counts[0] == 1 && event.glyphs[0][0] == 'D');
    aos_opening_event_update(&event, AOS_KEY_A);
    for (unsigned frame = 0; frame < 16 && event.active; ++frame)
        aos_opening_event_update(&event, 0);
    assert(event.complete && !event.active && !event.malformed);
    assert(event.cue == AOS_OPENING_CUE_COMPLETE);
    assert(event.glyph_counts[0] == 1 && event.glyphs[0][0] == 'E');

    uint32_t events[8] = {0};
    AosStats stats = {0};
    assert(aos_opening_event_commit(&event, events, &stats));
    assert(events[1] == 1);
    assert(aos_souls_total(&stats, AOS_SOUL_RED, 0) == 1);
    assert(stats.red_soul == 1);
    assert(!aos_opening_event_should_start(0, 0, 0, events, true));
    assert(!aos_opening_event_commit(&event, events, &stats));
    assert(aos_souls_total(&stats, AOS_SOUL_RED, 0) == 1);
}

static bool resolve_insert(void *context, uint8_t id, const uint8_t **script,
                           size_t *script_size) {
    static const uint8_t insert[] = {0, 0, 'X', 10};
    (void)context;
    if (id != 3) return false;
    *script = insert;
    *script_size = sizeof insert;
    return true;
}

static void test_insert_and_malformed(void) {
    static const uint8_t insert_command[] = {0, 0, 1, 3, 10};
    AosOpeningEvent event;
    assert(aos_opening_event_init(&event, insert_command,
                                  sizeof insert_command));
    aos_opening_event_update(&event, 0);
    assert(event.malformed && !event.active && !event.complete);
    assert(aos_opening_event_init(&event, insert_command,
                                  sizeof insert_command));
    aos_opening_event_set_resolver(&event, resolve_insert, NULL);
    aos_opening_event_update(&event, AOS_KEY_A);
    assert(event.active && event.glyph_counts[0] == 1 &&
           event.glyphs[0][0] == 'X');
    aos_opening_event_update(&event, 0);
    assert(event.complete && event.glyph_counts[0] == 1 &&
           event.glyphs[0][0] == 'X');
    assert(!aos_opening_event_init(&event, insert_command, 2));
}

static bool resolve_rom_text(void *context, uint8_t id,
                             const uint8_t **script, size_t *script_size) {
    return aos_intro_resources_entry(context, 1, id, script, script_size);
}

int main(int argc, char **argv) {
    test_trigger();
    test_script_and_commit();
    test_insert_and_malformed();
    if (argc == 2) {
        FusionRom rom = {0};
        AosIntroResources resources;
        const uint8_t *script = NULL;
        size_t script_size = 0;
        char error[128];
        assert(fusion_rom_open(FUSION_ASSET_GAME_ARIA, argv[1], &rom,
                               error, sizeof error));
        assert(aos_intro_resources_init(&resources, &rom, error, sizeof error));
        assert(aos_intro_resources_entry(&resources, 1, AOS_OPENING_TEXT_ID,
                                         &script, &script_size));
        AosOpeningEvent event;
        assert(aos_opening_event_init(&event, script, script_size));
        aos_opening_event_set_resolver(&event, resolve_rom_text, &resources);
        bool cues[AOS_OPENING_CUE_COMPLETE + 1] = {false};
        for (unsigned frame = 0; frame < 10000 && event.active; ++frame) {
            aos_opening_event_update(&event, AOS_KEY_A);
            cues[event.cue] = true;
        }
        assert(event.complete && !event.malformed);
        assert(event.beat_count == 7);
        assert(cues[AOS_OPENING_CUE_CONFRONTATION]);
        assert(cues[AOS_OPENING_CUE_ENEMY_APPEARS]);
        assert(cues[AOS_OPENING_CUE_ENEMY_ATTACKS]);
        assert(cues[AOS_OPENING_CUE_SOUL_ABSORBED]);
        assert(cues[AOS_OPENING_CUE_AFTER_BATTLE]);
        assert(cues[AOS_OPENING_CUE_COMPLETE]);
        fusion_rom_close(&rom);
    }
    return 0;
}
