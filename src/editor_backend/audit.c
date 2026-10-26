/* SPDX-License-Identifier: GPL-3.0-only */
/* Every discovered native room through the real render decoders (formerly
 * scripts/room_audit.py). The audit never writes previews: it exercises
 * decompression, resource loading, layer decoding, composition, collision
 * rendering and bitmap encoding, and returns structured diagnostics. */
#define _DEFAULT_SOURCE
#include "audit.h"

#include "aria_render.h"
#include "catalog.h"
#include "generated.h"
#include "mzm_render.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *const statuses[] = {"success", "partial", "unsupported", "error"};

static double now_ms(void) {
    struct timespec value;
    clock_gettime(CLOCK_MONOTONIC, &value);
    return (double)value.tv_sec * 1000.0 + (double)value.tv_nsec / 1e6;
}

/* _diagnostic_status(). */
static const char *diagnostic_status(const char *message) {
    char lower[FEB_MESSAGE_CAPACITY];
    size_t length = strlen(message);
    if (length >= sizeof lower) length = sizeof lower - 1;
    for (size_t index = 0; index < length; ++index)
        lower[index] = (char)tolower((unsigned char)message[index]);
    lower[length] = '\0';
    static const char *const unsupported[] = {"unsupported", "not available", "no supported",
                                              "not rle", "requires a future", "not decoded"};
    for (size_t index = 0; index < sizeof unsupported / sizeof *unsupported; ++index)
        if (strstr(lower, unsupported[index])) return "unsupported";
    return "error";
}

static bool clone_into(FusionJson *object, const char *key, const FusionJson *value) {
    FusionJson copy;
    if (!value) return feb_put(object, key, fusion_json_null());
    return fusion_json_clone(value, &copy) && feb_put(object, key, copy);
}

static bool mzm_descriptor(const FusionJson *room, FusionJson *base) {
    const FusionJson *fields = fusion_json_member(room, "fields");
    *base = fusion_json_object();
    FusionJson layers = fusion_json_array(), palettes = fusion_json_array(),
               graphics = fusion_json_array();
    static const char *const names[] = {"Bg1", "Bg2", "Bg3"};
    bool ok = true;
    for (int layer = 0; ok && layer < 3; ++layer) {
        char data[16], prop[16];
        snprintf(data, sizeof data, "p%sData", names[layer]);
        snprintf(prop, sizeof prop, "bg%dProp", layer + 1);
        const FusionJson *source = fusion_json_member(fields, data);
        if (!source) continue;
        FusionJson entry = fusion_json_object();
        ok = feb_put_string(&entry, "layer", names[layer]) && clone_into(&entry, "source", source) &&
             clone_into(&entry, "property", fusion_json_member(fields, prop)) &&
             fusion_json_push(&layers, entry);
    }
    const char *tileset = fusion_json_member(fields, "tileset")->string;
    char palette[64];
    snprintf(palette, sizeof palette, "tilesets/%s.pal", tileset);
    static const char *const references[] = {"pBg1Data", "pBg2Data", "pBg3Data", "pClipData"};
    ok = ok && fusion_json_push(&palettes, fusion_json_string(palette));
    for (int index = 0; ok && index < 4; ++index) {
        FusionJson copy = fusion_json_null();
        const FusionJson *value = fusion_json_member(fields, references[index]);
        ok = (!value || fusion_json_clone(value, &copy)) && fusion_json_push(&graphics, copy);
    }
    return ok && feb_put_string(base, "world", "zero_mission") &&
           clone_into(base, "area", fusion_json_member(room, "area")) &&
           feb_put(base, "area_index", fusion_json_null()) &&
           clone_into(base, "room", fusion_json_member(room, "room")) &&
           clone_into(base, "native_id", fusion_json_member(room, "native_id")) &&
           clone_into(base, "native_reference", fusion_json_member(room, "native_reference")) &&
           feb_put(base, "layers", layers) && feb_put_string(base, "tileset", tileset) &&
           feb_put(base, "palette_references", palettes) &&
           feb_put(base, "graphics_references", graphics);
}

static bool aria_descriptor(const FusionJson *room, FusionJson *base) {
    *base = fusion_json_object();
    FusionJson layers = fusion_json_array(), palettes = fusion_json_array(),
               graphics = fusion_json_array();
    bool ok = true;
    const FusionJson *backgrounds = fusion_json_member(room, "backgrounds");
    static const char *const keys[] = {"layer", "metadata_pointer", "control", "width_screens",
                                       "height_screens"};
    for (size_t index = 0; ok && backgrounds && index < backgrounds->count; ++index) {
        FusionJson entry = fusion_json_object();
        for (size_t key = 0; ok && key < 5; ++key)
            ok = clone_into(&entry, keys[key], fusion_json_member(&backgrounds->items[index],
                                                                  keys[key]));
        ok = ok && fusion_json_push(&layers, entry);
    }
    const char *lists[2] = {"palette_loads", "graphics_loads"};
    FusionJson *targets[2] = {&palettes, &graphics};
    for (int list = 0; ok && list < 2; ++list) {
        const FusionJson *loads = fusion_json_member(room, lists[list]);
        for (size_t index = 0; ok && loads && index < loads->count; ++index) {
            FusionJson copy;
            ok = fusion_json_clone(fusion_json_member(&loads->items[index], "resource_pointer"),
                                   &copy) &&
                 fusion_json_push(targets[list], copy);
        }
    }
    char native_id[32];
    snprintf(native_id, sizeof native_id, "aria:%02lld:%03lld",
             (long long)fusion_json_member(room, "engine_area")->integer,
             (long long)fusion_json_member(room, "room")->integer);
    return ok && feb_put_string(base, "world", "aria") &&
           clone_into(base, "area", fusion_json_member(room, "area")) &&
           clone_into(base, "area_index", fusion_json_member(room, "engine_area")) &&
           clone_into(base, "room", fusion_json_member(room, "room")) &&
           feb_put_string(base, "native_id", native_id) &&
           clone_into(base, "native_reference", fusion_json_member(room, "source_pointer")) &&
           feb_put(base, "layers", layers) && feb_put(base, "tileset", fusion_json_null()) &&
           feb_put(base, "palette_references", palettes) &&
           feb_put(base, "graphics_references", graphics);
}

/* _finish_record(); ``failure`` is NULL on success. */
static bool finish_record(FusionJson *record, double started, const FusionJson *result,
                          const FebError *failure) {
    FusionJson process = fusion_json_object();
    double elapsed = round((now_ms() - started) * 1000.0) / 1000.0;
    bool ok = feb_put(record, "duration_ms", fusion_json_double(elapsed)) &&
              feb_put_string(&process, "status", "completed") &&
              feb_put_int(&process, "exit_code", failure ? 2 : 0) &&
              feb_put(record, "process", process) &&
              feb_put(record, "expected_render", fusion_json_null()) &&
              feb_put(record, "invalid_graphics_references", fusion_json_array()) &&
              feb_put(record, "decoder_status", fusion_json_null()) &&
              feb_put(record, "error", fusion_json_null());
    if (!ok) return false;
    if (failure) {
        FusionJson error = fusion_json_object();
        return feb_put_string(record, "status", diagnostic_status(failure->message)) &&
               feb_put_string(&error, "type", "ValueError") &&
               feb_put_string(&error, "message", failure->message) && feb_put(record, "error", error);
    }
    const FusionJson *limitations = fusion_json_member(result, "limitations");
    const FusionJson *width = fusion_json_member(result, "width");
    const FusionJson *height = fusion_json_member(result, "height");
    ok = clone_into(record, "decoder_status", fusion_json_member(result, "status")) &&
         feb_put_string(record, "status",
                        limitations && limitations->count ? "partial" : "success");
    if (ok && width && height && width->integer && height->integer) {
        FusionJson expected = fusion_json_object();
        ok = feb_put_int(&expected, "width_px", width->integer) &&
             feb_put_int(&expected, "height_px", height->integer) &&
             feb_put(record, "expected_render", expected);
    }
    const FusionJson *layers = fusion_json_member(result, "layers");
    FusionJson rendered = fusion_json_array();
    FusionJson *invalid = fusion_json_member_mut(record, "invalid_graphics_references");
    bool named = layers && layers->kind == FUSION_JSON_OBJECT;
    for (size_t index = 0; ok && layers && index < layers->count; ++index) {
        const FusionJson *layer = &layers->items[index];
        FusionJson entry = fusion_json_object();
        for (size_t key = 0; ok && key < layer->count; ++key) {
            if (!strcmp(layer->keys[key], "path") || !strcmp(layer->keys[key], "painted_pixels"))
                continue;
            ok = clone_into(&entry, layer->keys[key], &layer->items[key]);
        }
        if (ok && !fusion_json_member(&entry, "layer"))
            ok = named ? feb_put_string(&entry, "layer", layers->keys[index])
                       : feb_put_int(&entry, "layer", (int64_t)index + 1);
        const FusionJson *unresolved = fusion_json_member(&entry, "unresolved_pixels_or_cells");
        if (!unresolved) unresolved = fusion_json_member(&entry, "unresolved_tiles_or_pixels");
        if (ok && unresolved && unresolved->integer) {
            FusionJson reference = fusion_json_object();
            ok = clone_into(&reference, "layer", fusion_json_member(&entry, "layer")) &&
                 feb_put_int(&reference, "unresolved_count", unresolved->integer) &&
                 fusion_json_push(invalid, reference);
        }
        ok = ok && fusion_json_push(&rendered, entry);
    }
    ok = ok && feb_put(record, "layers", rendered);
    if (ok && fusion_json_member(record, "expected_render")->kind == FUSION_JSON_NULL) {
        int64_t best_width = 0, best_height = 0;
        bool found = false;
        const FusionJson *list = fusion_json_member(record, "layers");
        for (size_t index = 0; index < list->count; ++index) {
            const FusionJson *blocks = fusion_json_member(&list->items[index], "width_blocks");
            if (!blocks || !blocks->integer) continue;
            const FusionJson *rows = fusion_json_member(&list->items[index], "height_blocks");
            int64_t w = blocks->integer * 16, h = rows ? rows->integer * 16 : 0;
            if (!found || w * h > best_width * best_height) {
                best_width = w;
                best_height = h;
                found = true;
            }
        }
        if (found) {
            FusionJson expected = fusion_json_object();
            ok = feb_put_int(&expected, "width_px", best_width) &&
                 feb_put_int(&expected, "height_px", best_height) &&
                 feb_put(record, "expected_render", expected);
        }
    }
    return ok;
}

static bool is_decimal(const FusionJson *value, int64_t *number) {
    if (!value) return false;
    if (value->kind == FUSION_JSON_NUMBER) {
        if (!value->integral || value->integer < 0) return false;
        *number = value->integer;
        return true;
    }
    if (value->kind != FUSION_JSON_STRING || !value->string[0]) return false;
    for (const char *c = value->string; *c; ++c)
        if (!isdigit((unsigned char)*c)) return false;
    *number = strtoll(value->string, NULL, 10);
    return true;
}

static int lower_compare(const char *left, const char *right) {
    for (;; ++left, ++right) {
        int a = tolower((unsigned char)*left), b = tolower((unsigned char)*right);
        if (a != b || !a) return a - b;
    }
}

static bool audit_mzm(const char *repository, const FusionJson *area, FusionJson *scopes,
                      FusionJson *records, FebError *error) {
    FusionJson rooms;
    if (!feb_native_catalog(repository, "zero_mission", &rooms, error)) return false;
    FusionJson all = fusion_json_array();
    bool ok = true;
    for (size_t index = 0; ok && index < rooms.count; ++index) {
        const char *name = fusion_json_member(&rooms.items[index], "area")->string;
        bool seen = false;
        for (size_t other = 0; other < all.count; ++other)
            if (!strcmp(all.items[other].string, name)) seen = true;
        if (!seen) ok = fusion_json_push(&all, fusion_json_string(name)) || feb_oom(error);
    }
    int64_t index = 0;
    if (ok && area && area->kind != FUSION_JSON_NULL) {
        const char *match = NULL;
        if (is_decimal(area, &index)) {
            if (index >= (int64_t)all.count)
                ok = feb_fail(error, FEB_EXIT_OPERATION,
                              "Zero Mission audit area index is out of range");
            else match = all.items[index].string;
        } else {
            for (size_t other = 0; other < all.count && !match; ++other)
                if (area->kind == FUSION_JSON_STRING &&
                    !lower_compare(all.items[other].string, area->string))
                    match = all.items[other].string;
            if (!match) ok = feb_fail(error, FEB_EXIT_OPERATION, "unknown Zero Mission audit area");
        }
        if (ok) {
            *scopes = fusion_json_array();
            ok = fusion_json_push(scopes, fusion_json_string(match)) || feb_oom(error);
        }
        fusion_json_free(&all);
    } else {
        *scopes = all;
    }
    for (size_t scope = 0; ok && scope < scopes->count; ++scope) {
        for (size_t room = 0; ok && room < rooms.count; ++room) {
            const FusionJson *entry = &rooms.items[room];
            const char *name = fusion_json_member(entry, "area")->string;
            if (strcmp(name, scopes->items[scope].string)) continue;
            FusionJson record, result;
            ok = mzm_descriptor(entry, &record) || feb_oom(error);
            double started = now_ms();
            FebError failure = {0};
            bool rendered = ok && feb_mzm_decode_room(repository, name,
                                                      fusion_json_member(entry, "room")->integer,
                                                      false, &result, &failure);
            ok = ok && (finish_record(&record, started, rendered ? &result : NULL,
                                      rendered ? NULL : &failure) || feb_oom(error));
            if (rendered) fusion_json_free(&result);
            ok = ok && (fusion_json_push(records, record) || feb_oom(error));
        }
    }
    fusion_json_free(&rooms);
    return ok;
}

static bool audit_aria(const char *repository, const FusionJson *area, FusionJson *scopes,
                       FusionJson *records, FebError *error) {
    FusionRom rom = {0};
    FusionJson world;
    if (!feb_aria_open_world(repository, &rom, &world, error)) return false;
    const FusionJson *rooms = fusion_json_member(&world, "rooms");
    bool present[16] = {false};
    for (size_t index = 0; index < rooms->count; ++index) {
        int64_t engine = fusion_json_member(&rooms->items[index], "engine_area")->integer;
        if (engine >= 0 && engine < 16) present[engine] = true;
    }
    *scopes = fusion_json_array();
    bool ok = true;
    int64_t selected = -1;
    if (area && area->kind != FUSION_JSON_NULL) {
        int64_t number = 0;
        if (!is_decimal(area, &number) || number >= 16 || !present[number])
            ok = feb_fail(error, FEB_EXIT_OPERATION, "unknown Aria audit area");
        else selected = number;
    }
    for (int64_t engine = 0; ok && engine < 16; ++engine)
        if (present[engine] && (selected < 0 || engine == selected))
            ok = fusion_json_push(scopes, fusion_json_int(engine)) || feb_oom(error);
    for (size_t scope = 0; ok && scope < scopes->count; ++scope) {
        int64_t engine = scopes->items[scope].integer;
        for (size_t index = 0; ok && index < rooms->count; ++index) {
            const FusionJson *room = &rooms->items[index];
            if (fusion_json_member(room, "engine_area")->integer != engine) continue;
            FusionJson record, result;
            ok = aria_descriptor(room, &record) || feb_oom(error);
            double started = now_ms();
            FebError failure = {0};
            bool rendered = ok && feb_aria_render_loaded(repository, &rom, &world, engine,
                                                         fusion_json_member(room, "room")->integer,
                                                         false, &result, &failure);
            ok = ok && (finish_record(&record, started, rendered ? &result : NULL,
                                      rendered ? NULL : &failure) || feb_oom(error));
            if (rendered) fusion_json_free(&result);
            ok = ok && (fusion_json_push(records, record) || feb_oom(error));
        }
    }
    if (!ok) fusion_json_free(scopes);
    fusion_json_free(&world);
    fusion_rom_close(&rom);
    return ok;
}

static int64_t sort_area_index(const FusionJson *record) {
    const FusionJson *value = fusion_json_member(record, "area_index");
    return value && value->kind == FUSION_JSON_NUMBER ? value->integer : 0;
}

static int compare_records(const FusionJson *left, const FusionJson *right) {
    int64_t a = sort_area_index(left), b = sort_area_index(right);
    if (a != b) return a < b ? -1 : 1;
    int order = strcmp(fusion_json_member(left, "area")->string,
                       fusion_json_member(right, "area")->string);
    if (order) return order;
    a = fusion_json_member(left, "room")->integer;
    b = fusion_json_member(right, "room")->integer;
    return a < b ? -1 : a > b;
}

bool feb_audit_world(const char *repository, const char *world, const FusionJson *area,
                     int64_t workers, FusionJson *report, FebError *error) {
    (void)workers;  /* the native audit runs in one process; the result is the same */
    FusionJson scopes = fusion_json_null(), records = fusion_json_array();
    bool ok = !strcmp(world, "zero_mission")
        ? audit_mzm(repository, area, &scopes, &records, error)
        : audit_aria(repository, area, &scopes, &records, error);
    if (!ok) {
        fusion_json_free(&records);
        return false;
    }
    /* A stable insertion sort by (area index, area, room). */
    for (size_t index = 1; index < records.count; ++index) {
        FusionJson item = records.items[index];
        size_t position = index;
        while (position > 0 && compare_records(&records.items[position - 1], &item) > 0) {
            records.items[position] = records.items[position - 1];
            --position;
        }
        records.items[position] = item;
    }
    int64_t counts[4] = {0, 0, 0, 0};
    for (size_t index = 0; index < records.count; ++index) {
        const char *status = fusion_json_member(&records.items[index], "status")->string;
        for (int kind = 0; kind < 4; ++kind)
            if (!strcmp(status, statuses[kind])) ++counts[kind];
    }
    FusionJson scope = fusion_json_object(), summary = fusion_json_object();
    *report = fusion_json_object();
    int64_t room_count = (int64_t)records.count;
    ok = clone_into(&scope, "area", area && area->kind != FUSION_JSON_NULL ? area : NULL) &&
         feb_put(&scope, "discovered_areas", scopes);
    for (int kind = 0; ok && kind < 4; ++kind) ok = feb_put_int(&summary, statuses[kind], counts[kind]);
    ok = ok && feb_put_string(report, "format", "MV_ROOM_RENDER_AUDIT_1") &&
         feb_put_string(report, "world", world) && feb_put(report, "scope", scope) &&
         feb_put_int(report, "room_count", room_count) && feb_put(report, "summary", summary) &&
         feb_put(report, "rooms", records);
    if (!ok) {
        fusion_json_free(report);
        return feb_oom(error);
    }
    return true;
}

bool feb_audit_write_report(const char *repository, const FusionJson *report, const char *name,
                            char *path, FebError *error) {
    char relative[160], *text = NULL;
    size_t size = 0;
    snprintf(relative, sizeof relative, "audits/%s", name);
    if (!fusion_json_write(report, 2, &text, &size)) return feb_oom(error);
    char *line = realloc(text, size + 1);
    if (!line) {
        free(text);
        return feb_oom(error);
    }
    line[size] = '\n';
    bool ok = feb_write_generated(repository, relative, (uint8_t *)line, size + 1, NULL, error);
    free(line);
    if (ok) snprintf(path, FEB_PATH_CAPACITY, "%s/assets/extracted/%s", repository, relative);
    return ok;
}
