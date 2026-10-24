/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/aos.h"

#include "output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    AOS_MAP_TABLE_POINTER = 0x08116650,
    AOS_AREA_DIRECTORY_POINTER = 0x0850EF08
};

static const char *const area_names[FUSION_AOS_AREA_COUNT] = {
    "Castle Corridor", "Chapel", "Study", "Dance Hall", "Inner Quarters",
    "Floating Garden", "Clock Tower", "Underground region", "The Arena",
    "Top Floor", "Chaotic Realm entrance", "Chaotic Realm boss"
};

typedef struct {
    const FusionRom *rom;
    FusionAosAreaDirectory directory;
    FusionAosMap map;
    FusionAosWorldSummary summary;
    char *error;
    size_t error_size;
} CatalogContext;

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static bool json_string(FILE *file, const char *value) {
    if (fputc('"', file) == EOF) return false;
    for (const unsigned char *cursor = (const unsigned char *)value;
         *cursor; ++cursor) {
        switch (*cursor) {
        case '"': if (fputs("\\\"", file) == EOF) return false; break;
        case '\\': if (fputs("\\\\", file) == EOF) return false; break;
        case '\b': if (fputs("\\b", file) == EOF) return false; break;
        case '\f': if (fputs("\\f", file) == EOF) return false; break;
        case '\n': if (fputs("\\n", file) == EOF) return false; break;
        case '\r': if (fputs("\\r", file) == EOF) return false; break;
        case '\t': if (fputs("\\t", file) == EOF) return false; break;
        default:
            if (*cursor < 0x20) {
                if (fprintf(file, "\\u%04x", *cursor) < 0) return false;
            } else if (fputc(*cursor, file) == EOF) {
                return false;
            }
        }
    }
    return fputc('"', file) != EOF;
}

static bool json_pointer(FILE *file, uint32_t pointer) {
    return fprintf(file, "\"0x%08x\"", pointer) >= 0;
}

static bool json_nullable_pointer(FILE *file, uint32_t pointer) {
    return pointer ? json_pointer(file, pointer) : fputs("null", file) != EOF;
}

static const FusionAosBossSpec *boss_for_entity(size_t area, size_t room,
                                                const FusionAosEntity *entity) {
    for (size_t index = 0; index < FUSION_AOS_BOSS_ROOM_COUNT; ++index) {
        const FusionAosBossSpec *spec = fusion_aos_boss_spec(index);
        if (spec && spec->area == area && spec->room == room &&
            entity->kind == 1 && entity->entity_id == spec->enemy_id) {
            return spec;
        }
    }
    return NULL;
}

static bool write_backgrounds(FILE *file, CatalogContext *context,
                              const FusionAosRoomDescriptor *descriptor) {
    FusionAosBackgroundList backgrounds;
    if (!fusion_aos_read_room_backgrounds(context->rom, descriptor, &backgrounds,
                                          context->error, context->error_size)) {
        return false;
    }
    if (fputc('[', file) == EOF) return false;
    for (size_t index = 0; index < FUSION_AOS_BACKGROUND_COUNT; ++index) {
        const FusionAosBackground *background = &backgrounds.entries[index];
        if (index && fputc(',', file) == EOF) return false;
        if (fprintf(file, "{\"layer\":%u,\"descriptor_pointer\":",
                    background->layer) < 0 ||
            !json_pointer(file, background->descriptor_pointer) ||
            fprintf(file, ",\"field_0\":%u,\"field_1\":%u,\"control\":%u,"
                          "\"field_4\":%u,\"field_6\":%u,\"metadata_pointer\":",
                    background->field_0, background->field_1, background->control,
                    background->field_4, background->field_6) < 0 ||
            !json_nullable_pointer(file, background->metadata_pointer) ||
            fputs(",\"width_screens\":", file) == EOF) {
            return false;
        }
        if (background->has_dimensions) {
            if (fprintf(file, "%u,\"height_screens\":%u}",
                        background->width_screens,
                        background->height_screens) < 0) return false;
        } else if (fputs("null,\"height_screens\":null}", file) == EOF) {
            return false;
        }
    }
    return fputc(']', file) != EOF;
}

static bool write_loads(FILE *file, CatalogContext *context, uint32_t pointer) {
    FusionAosLoadList loads;
    if (!fusion_aos_read_load_list(context->rom, pointer, &loads,
                                   context->error, context->error_size)) {
        return false;
    }
    if (fputc('[', file) == EOF) return false;
    for (size_t index = 0; index < loads.count; ++index) {
        const FusionAosLoadEntry *entry = &loads.entries[index];
        if (index && fputc(',', file) == EOF) return false;
        if (fputs("{\"entry_pointer\":", file) == EOF ||
            !json_pointer(file, entry->entry_pointer) ||
            fputs(",\"resource_pointer\":", file) == EOF ||
            !json_pointer(file, entry->resource_pointer) ||
            fprintf(file, ",\"parameters\":[%u,%u,%u]}", entry->parameters[0],
                    entry->parameters[1], entry->parameters[2]) < 0) {
            return false;
        }
    }
    return fputc(']', file) != EOF;
}

static bool write_entities(FILE *file, CatalogContext *context,
                           size_t area, size_t room, uint32_t pointer) {
    FusionAosEntityList entities;
    if (!fusion_aos_read_entity_list(context->rom, pointer, &entities,
                                     context->error, context->error_size)) {
        return false;
    }
    if (fputc('[', file) == EOF) return false;
    for (size_t index = 0; index < entities.count; ++index) {
        const FusionAosEntity *entity = &entities.entries[index];
        const FusionAosBossSpec *boss = boss_for_entity(area, room, entity);
        if (index && fputc(',', file) == EOF) return false;
        if (fputs("{\"entry_pointer\":", file) == EOF ||
            !json_pointer(file, entity->entry_pointer) ||
            fprintf(file, ",\"x\":%d,\"y\":%d,\"persistent_index\":%u,"
                          "\"kind\":%u,\"entity_id\":%u,\"flags\":%u,"
                          "\"parameters\":[%u,%u]",
                    entity->x, entity->y, entity->persistent_index, entity->kind,
                    entity->entity_id, entity->flags, entity->parameters[0],
                    entity->parameters[1]) < 0) {
            return false;
        }
        if (boss && (fputs(",\"boss_id\":", file) == EOF ||
                     !json_string(file, boss->id) ||
                     fputs(",\"boss_phase\":", file) == EOF ||
                     !json_string(file, boss->phase))) {
            return false;
        }
        if (fputc('}', file) == EOF) return false;
    }
    return fputc(']', file) != EOF;
}

static bool write_transitions(FILE *file, CatalogContext *context,
                              const FusionAosRoomDescriptor *descriptor) {
    FusionAosTransitionList transitions;
    if (!fusion_aos_read_room_transitions(context->rom, &context->directory,
                                          descriptor, &transitions,
                                          context->error, context->error_size)) {
        return false;
    }
    if (fputc('[', file) == EOF) return false;
    for (size_t index = 0; index < transitions.count; ++index) {
        const FusionAosTransition *transition = &transitions.entries[index];
        if (index && fputc(',', file) == EOF) return false;
        if (fputs("{\"entry_pointer\":", file) == EOF ||
            !json_pointer(file, transition->entry_pointer) ||
            fputs(",\"target_pointer\":", file) == EOF ||
            !json_pointer(file, transition->target_pointer) ||
            fprintf(file, ",\"source_screen_x\":%d,\"source_screen_y\":%d,"
                          "\"field_6\":%u,\"field_8\":%u,\"load_x\":%u,"
                          "\"load_y\":%u,\"field_e\":%u,"
                          "\"target_engine_area\":%zu,\"target_area\":",
                    transition->source_screen_x, transition->source_screen_y,
                    transition->field_6, transition->field_8, transition->load_x,
                    transition->load_y, transition->field_e,
                    transition->target_area) < 0 ||
            !json_string(file, area_names[transition->target_area]) ||
            fprintf(file, ",\"target_room\":%zu}", transition->target_room) < 0) {
            return false;
        }
    }
    return fputc(']', file) != EOF;
}

static bool write_room_map_cells(FILE *file, const CatalogContext *context,
                                 size_t area, size_t room) {
    if (fputc('[', file) == EOF) return false;
    size_t written = 0;
    for (size_t index = 0; index < context->map.count; ++index) {
        const FusionAosMapCell *cell = &context->map.cells[index];
        if (cell->area != area || cell->room != room) continue;
        if (written++ && fputc(',', file) == EOF) return false;
        if (fprintf(file, "{\"map_x\":%u,\"map_y\":%u,\"save\":%s,"
                          "\"warp\":%s}",
                    cell->map_x, cell->map_y, cell->save ? "true" : "false",
                    cell->warp ? "true" : "false") < 0) return false;
    }
    return fputc(']', file) != EOF;
}

static bool write_room(FILE *file, CatalogContext *context,
                       size_t area, size_t room) {
    FusionAosRoomDescriptor descriptor;
    if (!fusion_aos_read_room_descriptor(context->rom, &context->directory,
                                         area, room, &descriptor,
                                         context->error, context->error_size)) {
        return false;
    }
    if (fprintf(file, "{\"engine_area\":%zu,\"area\":", area) < 0 ||
        !json_string(file, area_names[area]) ||
        fprintf(file, ",\"room\":%zu,\"source_pointer\":", room) < 0 ||
        !json_pointer(file, descriptor.source_pointer) ||
        fputs(",\"resolved_pointer\":", file) == EOF ||
        !json_pointer(file, descriptor.resolved_pointer) ||
        fputs(",\"variant_chain\":[", file) == EOF) {
        return false;
    }
    for (size_t index = 0; index < descriptor.variant_count; ++index) {
        if (index && fputc(',', file) == EOF) return false;
        if (fputs("{\"pointer\":", file) == EOF ||
            !json_pointer(file, descriptor.variants[index].pointer) ||
            fprintf(file, ",\"selector\":%u}",
                    descriptor.variants[index].selector) < 0) return false;
    }
    if (fprintf(file, "],\"display_control\":%u,\"blend_control\":%u,"
                      "\"map_x\":%u,\"map_y\":%u,\"flags\":[%u,%u],"
                      "\"backgrounds\":",
                descriptor.display_control, descriptor.blend_control,
                descriptor.map_x, descriptor.map_y,
                descriptor.coordinate_flag_0 ? 1u : 0u,
                descriptor.coordinate_flag_1 ? 1u : 0u) < 0 ||
        !write_backgrounds(file, context, &descriptor) ||
        fputs(",\"graphics_loads\":", file) == EOF ||
        !write_loads(file, context, descriptor.graphics_pointer) ||
        fputs(",\"palette_loads\":", file) == EOF ||
        !write_loads(file, context, descriptor.palette_pointer) ||
        fputs(",\"entity_list_pointer\":", file) == EOF ||
        !json_nullable_pointer(file, descriptor.entities_pointer) ||
        fputs(",\"entities\":", file) == EOF ||
        !write_entities(file, context, area, room, descriptor.entities_pointer) ||
        fputs(",\"transition_list_pointer\":", file) == EOF ||
        !json_pointer(file, descriptor.transitions_pointer) ||
        fputs(",\"transitions\":", file) == EOF ||
        !write_transitions(file, context, &descriptor) ||
        fputs(",\"next_pointer\":", file) == EOF ||
        !json_nullable_pointer(file, descriptor.next_pointer) ||
        fputs(",\"map_cells\":", file) == EOF ||
        !write_room_map_cells(file, context, area, room) ||
        fputc('}', file) == EOF) {
        return false;
    }
    return true;
}

static bool write_map_record(FILE *file, const FusionAosMapCell *cell,
                             bool include_id) {
    if (fputc('{', file) == EOF) return false;
    if (include_id) {
        if (fprintf(file, "\"id\":\"aria.save.%u.%u\",", cell->area,
                    cell->room) < 0) return false;
    }
    if (fprintf(file, "\"map_x\":%u,\"map_y\":%u,\"engine_area\":%u,"
                      "\"area\":",
                cell->map_x, cell->map_y, cell->area) < 0 ||
        !json_string(file, area_names[cell->area]) ||
        fprintf(file, ",\"room\":%u,\"room_pointer\":", cell->room) < 0 ||
        !json_pointer(file, cell->room_pointer) ||
        fprintf(file, ",\"save\":%s,\"warp\":%s}",
                cell->save ? "true" : "false",
                cell->warp ? "true" : "false") < 0) {
        return false;
    }
    return true;
}

static bool write_boss_rooms(FILE *file, CatalogContext *context) {
    if (fputc('[', file) == EOF) return false;
    for (size_t index = 0; index < FUSION_AOS_BOSS_ROOM_COUNT; ++index) {
        const FusionAosBossSpec *spec = fusion_aos_boss_spec(index);
        FusionAosBossRoom boss;
        if (!spec || !fusion_aos_read_boss_room(context->rom, &context->directory,
                                                spec, &boss, context->error,
                                                context->error_size)) {
            return false;
        }
        if (index && fputc(',', file) == EOF) return false;
        if (fputs("{\"id\":", file) == EOF || !json_string(file, spec->id) ||
            fputs(",\"name\":", file) == EOF || !json_string(file, spec->name) ||
            fputs(",\"phase\":", file) == EOF || !json_string(file, spec->phase) ||
            fprintf(file, ",\"engine_area\":%u,\"area\":", spec->area) < 0 ||
            !json_string(file, area_names[spec->area]) ||
            fprintf(file, ",\"room\":%u,\"map_x\":%u,\"map_y\":%u,"
                          "\"room_pointer\":",
                    spec->room, boss.map_x, boss.map_y) < 0 ||
            !json_pointer(file, boss.room_pointer) ||
            fputs(",\"entity_pointer\":", file) == EOF ||
            !json_pointer(file, boss.entity_pointer) ||
            fprintf(file, ",\"entity_id\":%u,\"enemy_table_pointer\":",
                    spec->enemy_id) < 0 ||
            !json_pointer(file, boss.enemy_table_pointer) ||
            fprintf(file, ",\"health\":%u}", boss.health) < 0) {
            return false;
        }
    }
    return fputc(']', file) != EOF;
}

static bool write_filtered_map(FILE *file, const CatalogContext *context,
                               bool saves) {
    if (fputc('[', file) == EOF) return false;
    size_t written = 0;
    for (size_t index = 0; index < context->map.count; ++index) {
        const FusionAosMapCell *cell = &context->map.cells[index];
        if (saves ? !cell->save : !cell->warp) continue;
        if (written++ && fputc(',', file) == EOF) return false;
        if (!write_map_record(file, cell, saves)) return false;
    }
    return fputc(']', file) != EOF;
}

static bool write_catalog_body(FILE *file, const void *raw_context) {
    CatalogContext *context = (CatalogContext *)raw_context;
    if (fputs("{\"format\":\"MV_AOS_WORLD_2\","
              "\"source_revision\":\"Aria of Sorrow USA SHA-1 "
              "abd71fe01ebb201bcc133074db1dd8c5253776c7\","
              "\"map_table\":", file) == EOF ||
        !json_pointer(file, AOS_MAP_TABLE_POINTER) ||
        fputs(",\"area_directory\":", file) == EOF ||
        !json_pointer(file, AOS_AREA_DIRECTORY_POINTER) ||
        fputs(",\"room_counts\":[", file) == EOF) {
        return false;
    }
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        if (area && fputc(',', file) == EOF) return false;
        if (fprintf(file, "%zu", context->directory.areas[area].room_count) < 0)
            return false;
    }
    if (fprintf(file, "],\"native_room_count\":%zu,\"mapped_cells\":%zu,"
                      "\"transition_count\":%zu,\"entity_count\":%zu,"
                      "\"boss_room_count\":%zu,\"savepoint_count\":%zu,"
                      "\"warp_count\":%zu,\"rooms\":[",
                context->summary.native_room_count, context->summary.mapped_cells,
                context->summary.transition_count, context->summary.entity_count,
                context->summary.boss_room_count, context->summary.savepoint_count,
                context->summary.warp_count) < 0) {
        return false;
    }
    size_t room_index = 0;
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        for (size_t room = 0; room < context->directory.areas[area].room_count;
             ++room) {
            if (room_index++ && fputc(',', file) == EOF) return false;
            if (!write_room(file, context, area, room)) return false;
        }
    }
    if (fputs("],\"boss_rooms\":", file) == EOF ||
        !write_boss_rooms(file, context) ||
        fputs(",\"savepoints\":", file) == EOF ||
        !write_filtered_map(file, context, true) ||
        fputs(",\"warps\":", file) == EOF ||
        !write_filtered_map(file, context, false) ||
        fputs("}\n", file) == EOF) {
        return false;
    }
    return !ferror(file);
}

bool fusion_aos_write_world_catalog(const FusionRom *rom, const char *path,
                                    char *error, size_t error_size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_ARIA || !path) {
        set_error(error, error_size, "invalid Aria world-catalog request");
        return false;
    }
    CatalogContext context = {.rom = rom, .error = error, .error_size = error_size};
    if (!fusion_aos_read_area_directory(rom, &context.directory,
                                        error, error_size) ||
        !fusion_aos_read_map(rom, &context.directory, &context.map,
                             error, error_size) ||
        !fusion_aos_read_world_summary(rom, &context.summary,
                                       error, error_size)) {
        return false;
    }
    uint32_t *room_pointers = malloc(context.directory.total_rooms *
                                     sizeof *room_pointers);
    if (!room_pointers) {
        set_error(error, error_size, "cannot allocate Aria room identity index");
        return false;
    }
    size_t count = 0;
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        for (size_t room = 0; room < context.directory.areas[area].room_count;
             ++room) {
            FusionAosRoomDescriptor descriptor;
            if (!fusion_aos_read_room_descriptor(rom, &context.directory, area, room,
                                                 &descriptor, error, error_size)) {
                free(room_pointers);
                return false;
            }
            for (size_t previous = 0; previous < count; ++previous) {
                if (room_pointers[previous] == descriptor.source_pointer) {
                    free(room_pointers);
                    set_error(error, error_size,
                              "Aria room pointer is shared by multiple identities");
                    return false;
                }
            }
            room_pointers[count++] = descriptor.source_pointer;
        }
    }
    free(room_pointers);
    if (error && error_size > 0) error[0] = '\0';
    return fusion_asset_write_atomic(path, write_catalog_body, &context,
                                     error, error_size);
}

typedef struct {
    const FusionRom *rom;
    FusionAosAreaDirectory directory;
    FusionAosMap map;
    char *error;
    size_t error_size;
} IndexContext;

static bool location_is_save(const IndexContext *context,
                             size_t area, size_t room) {
    for (size_t index = 0; index < context->map.count; ++index) {
        const FusionAosMapCell *cell = &context->map.cells[index];
        if (cell->area == area && cell->room == room && cell->save) return true;
    }
    return false;
}

static bool location_is_boss(size_t area, size_t room) {
    for (size_t index = 0; index < FUSION_AOS_BOSS_ROOM_COUNT; ++index) {
        const FusionAosBossSpec *spec = fusion_aos_boss_spec(index);
        if (spec && spec->area == area && spec->room == room) return true;
    }
    return false;
}

static bool write_index_body(FILE *file, const void *raw_context) {
    IndexContext *context = (IndexContext *)raw_context;
    if (fputs("# VERIFIED ARIA STRUCTURAL INDEX; NOT a decoded gameplay map\n"
              "# area|room|name|width_screens|height_screens|entities|"
              "transitions|save|boss\n", file) == EOF) {
        return false;
    }
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        for (size_t room = 0; room < context->directory.areas[area].room_count;
             ++room) {
            FusionAosRoomDescriptor descriptor;
            FusionAosBackgroundList backgrounds;
            FusionAosEntityList entities;
            if (!fusion_aos_read_room_descriptor(context->rom, &context->directory,
                                                 area, room, &descriptor,
                                                 context->error,
                                                 context->error_size) ||
                !fusion_aos_read_room_backgrounds(context->rom, &descriptor,
                                                  &backgrounds, context->error,
                                                  context->error_size) ||
                !fusion_aos_read_entity_list(context->rom,
                                             descriptor.entities_pointer,
                                             &entities, context->error,
                                             context->error_size)) {
                return false;
            }
            const FusionAosBackground *primary = &backgrounds.entries[0];
            unsigned width = primary->has_dimensions ? primary->width_screens : 0;
            unsigned height = primary->has_dimensions ? primary->height_screens : 0;
            if ((width && (width > 16 || height == 0 || height > 16)) ||
                (height && width == 0)) {
                set_error(context->error, context->error_size,
                          "invalid Aria primary background dimensions");
                return false;
            }
            if (fprintf(file, "%zu|%zu|%s|%u|%u|%zu|%zu|%u|%u\n",
                        area, room, area_names[area], width, height, entities.count,
                        descriptor.transition_count,
                        location_is_save(context, area, room) ? 1u : 0u,
                        location_is_boss(area, room) ? 1u : 0u) < 0) {
                return false;
            }
        }
    }
    return !ferror(file);
}

bool fusion_aos_write_room_index(const FusionRom *rom, const char *path,
                                 char *error, size_t error_size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_ARIA || !path) {
        set_error(error, error_size, "invalid Aria room-index request");
        return false;
    }
    IndexContext context = {.rom = rom, .error = error, .error_size = error_size};
    FusionAosWorldSummary summary;
    if (!fusion_aos_read_area_directory(rom, &context.directory,
                                        error, error_size) ||
        !fusion_aos_read_map(rom, &context.directory, &context.map,
                             error, error_size) ||
        !fusion_aos_read_world_summary(rom, &summary, error, error_size)) {
        return false;
    }
    if (error && error_size > 0) error[0] = '\0';
    return fusion_asset_write_atomic(path, write_index_body, &context,
                                     error, error_size);
}
