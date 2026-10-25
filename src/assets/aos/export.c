/* SPDX-License-Identifier: GPL-3.0-only */
/* The Aria of Sorrow runtime-room export: each room with a decoded BG1
 * collision table gets
 * area_<AA>_room_<RRR>/room.tsv (AOSROOM-NATIVE v4) and background.bmp (the
 * static-origin composite), then export_index.tsv. */
#include "fusion/assets/aos.h"

#include "export.h"
#include "fusion/assets/rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { EXPORT_SKIP_CAPACITY = FUSION_AOS_EXPORT_SKIP_CAPACITY };

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

/* The global map's save and warp cells of the room (header version 4). */
static uint8_t room_flags(const FusionAosMap *map, size_t area, size_t room) {
    uint8_t flags = 0;
    for (size_t index = 0; index < map->count; ++index) {
        const FusionAosMapCell *cell = &map->cells[index];
        if (cell->area != area || cell->room != room) continue;
        if (cell->save) flags |= FUSION_AOS_ROOM_FLAG_SAVE;
        if (cell->warp) flags |= FUSION_AOS_ROOM_FLAG_WARP;
    }
    return flags;
}

/* Every decoded text background rendered and composited at its static
 * origin, then serialized as the export's 24-bit BMP. */
static bool composite_bitmap(const FusionRom *rom,
                             const FusionAosRoomDescriptor *descriptor,
                             const FusionAosBackgroundList *backgrounds,
                             FusionAosVideoMemory *memory, FusionAosBitmap24 *bitmap,
                             char *error, size_t error_size) {
    FusionAosRenderedBackground rendered[FUSION_AOS_BACKGROUND_COUNT] = {0};
    FusionAosCompositeLayer layers[FUSION_AOS_BACKGROUND_COUNT];
    size_t layer_count = 0;
    bool ok = fusion_aos_load_video_memory(rom, descriptor, memory, error, error_size);
    for (size_t index = 0; ok && index < FUSION_AOS_BACKGROUND_COUNT; ++index) {
        FusionAosBackgroundTilemap tilemap;
        if (!fusion_aos_read_background_tiles(rom, &backgrounds->entries[index],
                                              &tilemap, error, error_size)) {
            ok = false;
            break;
        }
        if (tilemap.status == FUSION_AOS_TILEMAP_DECODED) {
            ok = fusion_aos_render_background(&backgrounds->entries[index], &tilemap,
                                              memory, &rendered[layer_count], error,
                                              error_size);
            if (ok) {
                layers[layer_count] = (FusionAosCompositeLayer){
                    &backgrounds->entries[index], &rendered[layer_count]
                };
                ++layer_count;
            }
        }
        fusion_aos_free_background_tilemap(&tilemap);
    }
    FusionAosCompositeBackground composite = {0};
    if (ok && layer_count == 0) {
        set_error(error, error_size, "room has no decoded background to composite");
        ok = false;
    }
    ok = ok && fusion_aos_composite_backgrounds(layers, layer_count, &composite, error,
                                                error_size) &&
         fusion_aos_serialize_composite_bmp24(&composite, bitmap, error, error_size);
    fusion_aos_free_composite_background(&composite);
    for (size_t index = 0; index < layer_count; ++index)
        fusion_aos_free_rendered_background(&rendered[index]);
    return ok;
}

bool fusion_aos_export_runtime_rooms(const FusionRom *rom, const char *output,
                                     FusionAosExportSummary *summary,
                                     char *error, size_t error_size) {
    if (summary) *summary = (FusionAosExportSummary){0};
    if (!rom || !output || !summary) {
        set_error(error, error_size, "invalid Aria export request");
        return false;
    }
    bool ok = false;
    FusionAosAreaDirectory directory;
    FusionAosMap *map = malloc(sizeof *map);
    FusionAosVideoMemory *memory = malloc(sizeof *memory);
    FusionExportRoom *rooms = NULL;
    FusionExportSkip skipped[EXPORT_SKIP_CAPACITY];
    size_t room_count = 0, skipped_count = 0;
    if (!map || !memory) {
        set_error(error, error_size, "cannot allocate the Aria export");
        goto done;
    }
    if (!fusion_export_ensure_directory(output, error, error_size) ||
        !fusion_aos_read_area_directory(rom, &directory, error, error_size) ||
        !fusion_aos_read_map(rom, &directory, map, error, error_size)) {
        goto done;
    }
    size_t total_rooms = 0;
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area)
        total_rooms += directory.areas[area].room_count;
    rooms = calloc(total_rooms ? total_rooms : 1, sizeof *rooms);
    if (!rooms) {
        set_error(error, error_size, "cannot allocate the Aria export");
        goto done;
    }
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionAosRoomDescriptor descriptor;
            FusionAosBackgroundList backgrounds;
            FusionAosCollisionMap collision = {0};
            FusionAosRuntimeRoomPayload payload = {0};
            FusionAosBitmap24 bitmap = {0};
            bool room_ok = false;
            if (!fusion_aos_read_room_descriptor(rom, &directory, area, room,
                                                 &descriptor, error, error_size) ||
                !fusion_aos_read_room_backgrounds(rom, &descriptor, &backgrounds,
                                                  error, error_size) ||
                !fusion_aos_read_background_collision(rom, &backgrounds.entries[0],
                                                      &collision, error, error_size)) {
                goto room_done;
            }
            if (collision.status != FUSION_AOS_COLLISION_DECODED) {
                if (skipped_count == EXPORT_SKIP_CAPACITY) {
                    set_error(error, error_size, "too many skipped Aria rooms");
                    goto room_done;
                }
                snprintf(skipped[skipped_count].item, sizeof skipped[0].item,
                         "%zu/%zu", area, room);
                snprintf(skipped[skipped_count].reason, sizeof skipped[0].reason,
                         "room has no decodable BG1 collision table");
                snprintf(summary->skipped[skipped_count],
                         sizeof summary->skipped[0], "%s: %s",
                         skipped[skipped_count].item, skipped[skipped_count].reason);
                ++skipped_count;
                room_ok = true;
                goto room_done;
            }
            FusionAosTransitionList transitions;
            FusionAosEntityList entities;
            if (!composite_bitmap(rom, &descriptor, &backgrounds, memory, &bitmap,
                                  error, error_size) ||
                !fusion_aos_read_room_transitions(rom, &directory, &descriptor,
                                                  &transitions, error, error_size) ||
                !fusion_aos_read_entity_list(rom, descriptor.entities_pointer,
                                             &entities, error, error_size) ||
                !fusion_aos_serialize_runtime_room(area, room, &collision,
                                                   &transitions, &entities,
                                                   room_flags(map, area, room),
                                                   &payload, error, error_size)) {
                goto room_done;
            }
            FusionExportRoom *exported = &rooms[room_count];
            char folder[FUSION_EXPORT_PATH_CAPACITY], path[FUSION_EXPORT_PATH_CAPACITY];
            snprintf(exported->folder, sizeof exported->folder,
                     "area_%02zu_room_%03zu", area, room);
            if (!fusion_export_join(folder, output, exported->folder) ||
                !fusion_export_ensure_directory(folder, error, error_size) ||
                !fusion_export_join(path, folder, "room.tsv") ||
                !fusion_export_write_file(path, payload.data, payload.size, error,
                                          error_size) ||
                !fusion_export_join(path, folder, "background.bmp") ||
                !fusion_export_write_file(path, bitmap.data, bitmap.size, error,
                                          error_size) ||
                !fusion_sha256_hex(payload.data, payload.size, exported->sha_room) ||
                !fusion_sha256_hex(bitmap.data, bitmap.size,
                                   exported->sha_background)) {
                goto room_done;
            }
            ++room_count;
            summary->room_bytes += payload.size;
            summary->background_bytes += bitmap.size;
            room_ok = true;
        room_done:
            fusion_aos_free_bitmap24(&bitmap);
            fusion_aos_free_runtime_room_payload(&payload);
            fusion_aos_free_collision_map(&collision);
            if (!room_ok) {
                if (error && error_size && !error[0])
                    set_error(error, error_size, "cannot export an Aria room");
                goto done;
            }
        }
    }
    if (!fusion_export_write_room_index(output, rooms, room_count, skipped,
                                        skipped_count, error, error_size)) {
        goto done;
    }
    summary->room_count = room_count;
    summary->skipped_count = skipped_count;
    ok = true;
    if (error && error_size) error[0] = '\0';

done:
    free(map);
    free(memory);
    free(rooms);
    return ok;
}
