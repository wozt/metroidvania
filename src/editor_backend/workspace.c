/* SPDX-License-Identifier: GPL-3.0-only */
/* The private native workroom commands: layer-list, tile-get, tile-set and
 * tile-fill (formerly scripts/editor_backend.py through fusion_native_map_cli).
 * They use the native-map core of the GTK editor directly. */
#define _DEFAULT_SOURCE
#include "workspace.h"

#include "catalog.h"
#include "core/native_map.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *const workspace_commands[] = {"layer-list", "tile-get", "tile-set",
                                                  "tile-fill"};

bool feb_is_workspace_command(const char *command) {
    for (size_t index = 0; index < sizeof workspace_commands / sizeof *workspace_commands; ++index)
        if (!strcmp(workspace_commands[index], command)) return true;
    return false;
}

/* _require_repository_root(). */
bool feb_require_repository_root(const char *root, const char *repository, FebError *error) {
    char resolved[PATH_MAX];
    if (!realpath(repository, resolved) || strcmp(root, resolved))
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "native workspace operations require the repository root");
    return true;
}

/* _native_room(): the one original room of that identity. */
bool feb_native_room(const char *root, const char *repository, const FusionJson *options,
                     FusionJson *room, FebError *error) {
    const char *world = NULL;
    int64_t id = 0;
    FusionJson rooms;
    if (!feb_require_repository_root(root, repository, error) ||
        !feb_world(feb_option(options, "world"), &world, error) ||
        !feb_integer(feb_option(options, "room"), "room", 0, 999, &id, error) ||
        !feb_filter_native_rooms(repository, world, feb_option(options, "area"), &rooms, error))
        return false;
    size_t matches = 0, match = 0;
    for (size_t index = 0; index < rooms.count; ++index) {
        if (fusion_json_member(&rooms.items[index], "room")->integer != id) continue;
        ++matches;
        match = index;
    }
    bool ok = matches == 1 ||
              feb_fail(error, FEB_EXIT_OPERATION, "native room identity is missing or ambiguous");
    if (ok) {
        *room = rooms.items[match];
        rooms.items[match] = fusion_json_null();
    }
    fusion_json_free(&rooms);
    return ok;
}

/* _native_workspace_paths(): the extracted workroom and its override. */
static bool workspace_paths(const char *root, const FusionJson *room, char *base, char *override) {
    const char *world = fusion_json_member(room, "world")->string;
    int64_t number = fusion_json_member(room, "room")->integer;
    char name[96];
    if (!strcmp(world, "zero_mission")) {
        snprintf(name, sizeof name, "%s_%03lld.mvnative",
                 fusion_json_member(room, "area")->string, (long long)number);
        for (char *c = name; *c; ++c)
            if (*c >= 'A' && *c <= 'Z') *c = (char)(*c - 'A' + 'a');
    } else {
        snprintf(name, sizeof name, "area_%02lld_room_%03lld.mvnative",
                 (long long)fusion_json_member(room, "area_index")->integer, (long long)number);
    }
    const char *family = !strcmp(world, "zero_mission") ? "metroid" : "aria";
    return snprintf(base, FEB_PATH_CAPACITY, "%s/assets/extracted/rooms/%s/workrooms/%s", root,
                    family, name) < FEB_PATH_CAPACITY &&
           snprintf(override, FEB_PATH_CAPACITY, "%s/assets/extracted/overrides/%s/%s", root,
                    family, name) < FEB_PATH_CAPACITY;
}

/* _check_native_path(). */
static bool check_path(const char *root, const char *path, bool existing, FebError *error) {
    char current[FEB_PATH_CAPACITY];
    size_t length = strlen(root);
    for (const char *slash = path + length; slash && *slash;) {
        const char *next = strchr(slash + 1, '/');
        size_t end = next ? (size_t)(next - path) : strlen(path);
        snprintf(current, sizeof current, "%.*s", (int)end, path);
        if (feb_is_symlink(current))
            return feb_fail(error, FEB_EXIT_OPERATION,
                            "symlink native workspace component refused: %s", current);
        slash = next;
    }
    long long size = 0;
    struct stat status;
    if (existing && (!feb_is_regular(path, &size) || size > 1000000))
        return feb_fail(error, FEB_EXIT_OPERATION, "native workspace must be a bounded regular file");
    if (!existing && stat(path, &status) == 0 && !S_ISREG(status.st_mode))
        return feb_fail(error, FEB_EXIT_OPERATION, "native workspace target must be a regular file");
    return true;
}

static void make_parents(const char *path) {
    char current[FEB_PATH_CAPACITY];
    snprintf(current, sizeof current, "%s", path);
    char *last = strrchr(current, '/');
    if (!last) return;
    *last = '\0';
    for (char *slash = current + 1; (slash = strchr(slash, '/')) != NULL; ++slash) {
        *slash = '\0';
        mkdir(current, 0777);
        *slash = '/';
    }
    mkdir(current, 0777);
}

static bool load_map(NativeMap *map, const char *path, FebError *error) {
    char detail[192] = "";
    if (!native_map_load(map, path, detail, sizeof detail))
        return feb_fail(error, FEB_EXIT_OPERATION, "%s",
                        *detail ? detail : "invalid native map command/input");
    return true;
}

bool feb_workspace_command(const char *command, const char *root, const char *repository,
                           const FusionJson *options, bool dry_run, FusionJson *data,
                           FebError *error) {
    FusionJson room;
    char base[FEB_PATH_CAPACITY], override[FEB_PATH_CAPACITY];
    if (!feb_native_room(root, repository, options, &room, error)) return false;
    bool ok = workspace_paths(root, &room, base, override) ||
              feb_fail(error, FEB_EXIT_OPERATION, "native workspace path is too long");
    ok = ok && check_path(root, override, false, error);
    struct stat status;
    const char *source = ok && stat(override, &status) == 0 ? override : base;
    ok = ok && check_path(root, source, true, error);
    NativeMap *map = ok ? calloc(1, sizeof *map) : NULL;
    if (ok && !map) ok = feb_oom(error);
    const char *native_id = fusion_json_member(&room, "native_id")->string;
    if (ok && !strcmp(command, "layer-list")) {
        ok = load_map(map, source, error);
        FusionJson layers = fusion_json_array();
        for (unsigned layer = 0; ok && layer < 2; ++layer) {
            FusionJson entry = fusion_json_object();
            ok = (feb_put_string(&entry, "id", layer ? "bg2" : "bg1") &&
                  feb_put_int(&entry, "width", map->width[layer]) &&
                  feb_put_int(&entry, "height", map->height[layer]) &&
                  fusion_json_push(&layers, entry)) || feb_oom(error);
        }
        ok = ok && ((feb_put_string(data, "room", map->room_id) &&
                     feb_put_int(data, "tile_count", map->tile_count) &&
                     feb_put_int(data, "tileset", map->tileset) && feb_put(data, "layers", layers) &&
                     feb_put_string(data, "source", source)) || feb_oom(error));
        if (!ok) fusion_json_free(&layers);
    } else if (ok) {
        const FusionJson *layer_value = feb_option(options, "layer");
        const char *layer_name = layer_value && layer_value->kind == FUSION_JSON_STRING
            ? layer_value->string : "";
        int64_t x = 0, y = 0, tile = 0;
        if (strcmp(layer_name, "bg1") && strcmp(layer_name, "bg2"))
            ok = feb_fail(error, FEB_EXIT_OPERATION, "layer must be bg1 or bg2");
        ok = ok && feb_integer(feb_option(options, "x"), "x", 0, 254, &x, error) &&
             feb_integer(feb_option(options, "y"), "y", 0, 254, &y, error);
        bool get = !strcmp(command, "tile-get");
        if (ok && !get) {
            ok = feb_integer(feb_option(options, "tile_id"), "tile_id", 0, 1023, &tile, error);
            if (ok && !dry_run) {
                make_parents(override);
                ok = check_path(root, override, false, error);
            }
        }
        unsigned layer = !strcmp(layer_name, "bg2") ? 1u : 0u;
        ok = ok && load_map(map, source, error);
        if (ok && ((unsigned)x >= map->width[layer] || (unsigned)y >= map->height[layer]))
            ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid native map layer or coordinates");
        if (ok && get) {
            unsigned picked = 0;
            (void)native_map_edit(map, layer, (int)x, (int)y, 3, 0, &picked);
            ok = (feb_put_string(data, "room", native_id) &&
                  feb_put_string(data, "layer", layer_name) && feb_put_int(data, "x", x) &&
                  feb_put_int(data, "y", y) && feb_put_int(data, "tile_id", picked) &&
                  feb_put_string(data, "source", source)) || feb_oom(error);
        } else if (ok) {
            if ((uint64_t)tile >= map->tile_count)
                ok = feb_fail(error, FEB_EXIT_OPERATION, "unknown or invalid native map operation");
            bool changed = ok && native_map_edit(map, layer, (int)x, (int)y,
                                                 !strcmp(command, "tile-fill") ? 2 : 0,
                                                 (unsigned)tile, NULL);
            char detail[192] = "";
            if (ok && !dry_run && changed && !native_map_save(map, override, detail, sizeof detail))
                ok = feb_fail(error, FEB_EXIT_OPERATION, "%s",
                              *detail ? detail : "native map output required");
            ok = ok && ((feb_put_string(data, "room", native_id) &&
                         feb_put_string(data, "layer", layer_name) && feb_put_int(data, "x", x) &&
                         feb_put_int(data, "y", y) && feb_put_int(data, "tile_id", tile) &&
                         feb_put_bool(data, "changed", changed) &&
                         feb_put_bool(data, "persisted", changed && !dry_run) &&
                         (!changed || dry_run || feb_put_string(data, "path", override))) ||
                        feb_oom(error));
        }
    }
    free(map);
    fusion_json_free(&room);
    return ok;
}
