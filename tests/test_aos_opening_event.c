/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_opening_event.h"
#include "aos_intro_resources.h"
#include "aos_soma.h"

#include <assert.h>
#include <string.h>

#include "aos_input.h"

static uint32_t zero_random(void *context) {
    (void)context;
    return 0;
}

static int32_t full_sine(uint32_t angle, void *context) {
    (void)angle;
    (void)context;
    return 0x10000;
}

static uint16_t zero_angle(int16_t x, int16_t y, void *context) {
    (void)x;
    (void)y;
    (void)context;
    return 0;
}

static void test_emitted_motion(void) {
    AosOpeningActors actors;
    aos_opening_actors_init(&actors);
    aos_opening_actors_set_motion(&actors, zero_random, full_sine,
                                  zero_angle, NULL);
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_ENEMY_APPEARS);
    assert(actors.emitted_count == 6);
    int32_t knight_y = actors.emitted[0].y;
    int32_t winged_y = actors.emitted[5].y;
    assert(actors.emitted[5].phase == 0);
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(actors.emitted[0].state == 1);
    assert(actors.emitted[5].state == 1 &&
           actors.emitted[5].y == winged_y + 0x4000 &&
           actors.emitted[5].phase == 0x200);
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(actors.emitted[0].state == 1 && actors.emitted[0].step == 1 &&
           actors.emitted[0].timer == 16);
    assert(actors.emitted[5].state == 2 &&
           actors.emitted[5].y == winged_y + 0x8000);
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(actors.emitted[0].timer == 15 &&
           actors.emitted[0].y == knight_y - 0x8000);
    assert(actors.emitted[2].state == 0);
}

static void test_emitted_attack_tails(void) {
    AosOpeningActors actors;
    aos_opening_actors_init(&actors);
    aos_opening_actors_set_motion(&actors, zero_random, full_sine,
                                  zero_angle, NULL);
    actors.camera_x = 0xDF << 16;
    actors.emitted_count = 1;
    actors.emitted[0] = (AosOpeningEntity){
        .x = 0x158 << 16,
        .y = 0x288 << 16,
        .entity_type = 8,
        .variant = 0,
        .state = 4,
        .active = true,
        .disappearing = true,
    };
    AosOpeningEntity *entity = &actors.emitted[0];
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(entity->step == 1 && entity->timer == 0x20);
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(!entity->hidden && entity->timer == 0x1F);
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(entity->hidden && entity->timer == 0x1E);
    for (unsigned frame = 0; frame < 31; ++frame)
        aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(entity->step == 2 && entity->timer == 0x10 && !entity->hidden);
    for (unsigned frame = 0; frame < 17; ++frame)
        aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(entity->step == 3 && entity->active);
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(entity->state == 5 && entity->active &&
           entity->events == (AOS_OPENING_ENTITY_EVENT_SOUND_0X6B |
                              AOS_OPENING_ENTITY_EVENT_DEBRIS));
    assert(actors.debris_count == 7);
    assert(actors.debris[0].active && actors.debris[0].source_type == 8 &&
           actors.debris[0].frame == 0x24 &&
           actors.debris[0].x == entity->x &&
           actors.debris[0].y == entity->y - 30 * 0x10000 &&
           actors.debris[0].vx == 16 * 0x1000 &&
           actors.debris[0].vy == -64 * 0x1000 &&
           actors.debris[0].timer == 30);
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(!entity->active && entity->events == AOS_OPENING_ENTITY_EVENT_NONE);

    actors.emitted[0] = (AosOpeningEntity){
        .x = 0x400 << 16,
        .y = 0x288 << 16,
        .entity_type = 10,
        .variant = 0,
        .state = 3,
        .active = true,
        .disappearing = true,
    };
    entity = &actors.emitted[0];
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    for (unsigned frame = 0; frame < 33; ++frame)
        aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(entity->step == 2 && entity->active);
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(entity->step == 3 && entity->events == AOS_OPENING_ENTITY_EVENT_NONE);
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(entity->state == 4 && entity->active &&
           entity->events == AOS_OPENING_ENTITY_EVENT_PROJECTILE);
    assert(actors.projectile_count == 1 &&
           actors.projectiles[0].active &&
           actors.projectiles[0].x == entity->x &&
           actors.projectiles[0].y == entity->y - 0x200000 &&
           actors.projectiles[0].heading == 0x8000 &&
           actors.projectiles[0].speed == 0x30000 &&
           actors.projectiles[0].timer == 0xC8);
    int32_t projectile_x = actors.projectiles[0].x;
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(!entity->active && actors.projectiles[0].active &&
           actors.projectiles[0].speed == 0x31800 &&
           actors.projectiles[0].turn == 0x2A &&
           actors.projectiles[0].x == projectile_x + 0x31800 &&
           actors.projectiles[0].timer == 0xC7);

    actors.emitted[0] = (AosOpeningEntity){
        .x = 0x158 << 16,
        .y = 0x288 << 16,
        .entity_type = 10,
        .variant = 0,
        .state = 3,
        .active = true,
        .disappearing = true,
    };
    for (unsigned frame = 0; frame < 36; ++frame)
        aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(actors.emitted[0].state == 4 && actors.debris_count == 14 &&
           actors.debris[7].source_type == 10 &&
           actors.debris[7].frame == 0x1B &&
           actors.debris[7].vx == 32 * 0x1000 &&
           actors.debris[7].vy == -80 * 0x1000 &&
           actors.projectile_count == 2);

    actors.projectile_count = 1;
    actors.projectiles[0] = (AosOpeningProjectile){
        .x = actors.x[2], .y = actors.y[2], .state = 2, .active = true};
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(!actors.projectiles[0].active &&
           actors.projectiles[0].events ==
               AOS_OPENING_ENTITY_EVENT_SOUND_0XBC);
    aos_opening_actors_update(&actors, NULL, NULL, 0x230);
    assert(actors.projectiles[0].events == AOS_OPENING_ENTITY_EVENT_NONE);
}

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
        aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(actors.state == 1 && actors.timer == 0 && actors.x[1] == (0x119 << 16));
    aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(actors.x[1] < (0x119 << 16));
    for (unsigned frame = 0; frame < 128; ++frame)
        aos_opening_actors_update(&actors, NULL, NULL, 0);
    assert(actors.state == 2 && actors.x[1] == (0xD0 << 16));
    assert(actors.visible[0] && actors.visible[1] && actors.visible[2]);

    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_CONFRONTATION);
    assert(aos_opening_actors_update(&actors, NULL, NULL, 0) ==
           AOS_OPENING_CUE_CONFRONTATION);
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_ENEMY_APPEARS);
    assert(actors.emitted_batches == AOS_OPENING_EMISSION_DISTANT);
    assert(actors.emitted_count == 6);
    assert(actors.emitted[0].active && actors.emitted[0].entity_type == 10 &&
           actors.emitted[0].animation == 0 &&
           actors.emitted[0].y == (0x2A7 << 16));
    assert(actors.emitted[5].active && actors.emitted[5].entity_type == 8 &&
           actors.emitted[5].animation == 4);
    int32_t camera_x = 48;
    for (unsigned frame = 0; frame < 49; ++frame)
        assert(aos_opening_actors_update(&actors, NULL, &camera_x, 0) ==
               AOS_OPENING_CUE_NONE);
    assert(camera_x == 219 && actors.camera_active);
    assert(aos_opening_actors_update(&actors, NULL, &camera_x, 0) ==
           AOS_OPENING_CUE_NONE);
    assert(camera_x == 223 && !actors.camera_active);
    for (unsigned frame = 0; frame < 31; ++frame)
        assert(aos_opening_actors_update(&actors, NULL, &camera_x, 0) == AOS_OPENING_CUE_NONE);
    assert(aos_opening_actors_update(&actors, NULL, &camera_x, 0) ==
           AOS_OPENING_CUE_ENEMY_APPEARS);
    assert(actors.animation[0] == 5 && actors.animation_age[0] == 0);
    assert(actors.emitted_batches ==
           (AOS_OPENING_EMISSION_DISTANT | AOS_OPENING_EMISSION_NEAR_SOMA));
    assert(actors.emitted_count == AOS_OPENING_EMITTED_CAPACITY);
    assert(actors.emitted[6].animation == 0 && !actors.emitted[6].mirrored);
    assert(actors.emitted[7].animation == 0 && actors.emitted[7].mirrored);
    assert(actors.emitted[8].entity_type == 8 && actors.emitted[8].mirrored);
    assert(actors.emitted[9].entity_type == 8 && !actors.emitted[9].mirrored);

    AosOpeningSpawn spawns[AOS_OPENING_EMISSION_MAX_SPAWNS];
    assert(aos_opening_emission_spawns(AOS_OPENING_EMISSION_DISTANT,
                                       AOS_OPENING_SOMA_X, spawns,
                                       AOS_OPENING_EMISSION_MAX_SPAWNS) == 6);
    assert(spawns[0].x == 0x158 && spawns[0].y == 0x29F &&
           spawns[0].entity_type == 10 && spawns[0].parameter == 4 &&
           spawns[0].variant == 0);
    assert(spawns[5].x == 0x14F && spawns[5].y == 0x288 &&
           spawns[5].entity_type == 8);
    assert(aos_opening_emission_spawns(AOS_OPENING_EMISSION_NEAR_SOMA,
                                       AOS_OPENING_SOMA_X, spawns,
                                       AOS_OPENING_EMISSION_MAX_SPAWNS) == 4);
    assert(spawns[0].x == 0xC0 && spawns[0].parameter == 0x12 &&
           spawns[0].variant == 5);
    assert(spawns[1].x == 0x78 && spawns[1].parameter == 2 &&
           spawns[1].variant == 6);
    assert(spawns[2].x == 0x7C && spawns[3].x == 0xC4);

    const uint16_t durations[AOS_OPENING_ACTOR_COUNT] = {1, 3, 1};
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_ENEMY_ATTACKS);
    assert(actors.animation[1] == 2);
    assert(aos_opening_actors_update(&actors, durations, NULL, 0) ==
           AOS_OPENING_CUE_ENEMY_ATTACKS);
    assert(aos_opening_actors_update(&actors, durations, NULL, 0) == AOS_OPENING_CUE_NONE);
    assert(aos_opening_actors_update(&actors, durations, NULL, 0) == AOS_OPENING_CUE_NONE);
    assert(actors.emitted[0].disappearing && actors.emitted[0].state == 3 &&
           actors.emitted[0].step == 0);
    for (unsigned frame = 0; frame < 15; ++frame)
        assert(aos_opening_actors_update(&actors, durations, NULL, 0) == AOS_OPENING_CUE_NONE);
    assert(aos_opening_actors_update(&actors, durations, NULL, 0) ==
           AOS_OPENING_CUE_ENEMY_ATTACKS_AGAIN);
    assert(actors.animation[1] == 3 && actors.animation_age[1] == 0);

    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_SOUL_ABSORBED);
    for (unsigned frame = 0; frame < 18; ++frame) {
        AosOpeningCue cue = aos_opening_actors_update(&actors, durations, NULL, 0);
        if (frame == 17) assert(cue == AOS_OPENING_CUE_ENEMY_DEFEATED);
        else assert(cue == AOS_OPENING_CUE_NONE);
    }
    assert(!actors.visible[2]);
    for (unsigned frame = 0; frame < 64; ++frame)
        assert(aos_opening_actors_update(&actors, durations, NULL, 0) == AOS_OPENING_CUE_NONE);
    assert(aos_opening_actors_update(&actors, durations, NULL, 0) ==
           AOS_OPENING_CUE_SOUL_ABSORBED);
    aos_opening_actors_trigger(&actors, AOS_OPENING_CUE_AFTER_BATTLE);
    assert(aos_opening_actors_update(&actors, durations, NULL, 0) ==
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
    test_emitted_motion();
    test_emitted_attack_tails();
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
