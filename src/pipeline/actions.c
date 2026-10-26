/* SPDX-License-Identifier: GPL-3.0-only */
/* Concrete actions used by the native reconstruction orchestrator. */
#define _POSIX_C_SOURCE 200809L

#include "internal.h"

#include "fusion/pipeline/rebuild.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static bool set_error(char *error, size_t size, const char *format, ...) {
    if (error && size) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(error, size, format, arguments);
        va_end(arguments);
        error[size - 1] = '\0';
    }
    return false;
}

static bool safe_relative(const char *path) {
    if (!path || !path[0] || path[0] == '/' || path[0] == '\\' || strchr(path, ':'))
        return false;
    const char *part = path;
    for (const char *cursor = path;; ++cursor) {
        if (*cursor != '/' && *cursor != '\\' && *cursor != '\0') continue;
        size_t length = (size_t)(cursor - part);
        if (!length || (length == 1 && part[0] == '.') ||
            (length == 2 && part[0] == '.' && part[1] == '.')) return false;
        if (!*cursor) return true;
        part = cursor + 1;
    }
}

static bool root_path(char *output, size_t size, const char *root,
                      const char *relative, char *error, size_t error_size) {
    if (!safe_relative(relative))
        return set_error(error, error_size, "unsafe rebuild path: %s", relative);
    int length = snprintf(output, size, "%s%s%s", root,
                          root[0] && root[strlen(root) - 1] == '/' ? "" : "/",
                          relative);
    return length >= 0 && (size_t)length < size
               ? true
               : set_error(error, error_size, "rebuild path is too long");
}

static bool ensure_directory(const char *root, const char *relative,
                             char *error, size_t error_size) {
    if (!safe_relative(relative))
        return set_error(error, error_size, "unsafe rebuild directory: %s", relative);
    char path[4096];
    int length = snprintf(path, sizeof path, "%s", root);
    if (length < 0 || (size_t)length >= sizeof path)
        return set_error(error, error_size, "project root is too long");
    size_t used = (size_t)length;
    const char *cursor = relative;
    while (*cursor) {
        const char *slash = strchr(cursor, '/');
        size_t part = slash ? (size_t)(slash - cursor) : strlen(cursor);
        if (used + part + 2 > sizeof path)
            return set_error(error, error_size, "rebuild directory is too long");
        path[used++] = '/';
        memcpy(path + used, cursor, part);
        used += part;
        path[used] = '\0';
        struct stat status;
        if (!lstat(path, &status)) {
            if (S_ISLNK(status.st_mode) || !S_ISDIR(status.st_mode))
                return set_error(error, error_size,
                                 "rebuild directory is not a safe directory: %s", path);
        } else if (errno != ENOENT || mkdir(path, 0755)) {
            return set_error(error, error_size, "cannot create rebuild directory: %s", path);
        }
        if (!slash) break;
        cursor = slash + 1;
    }
    return true;
}

static bool prepare_outputs(const char *root, const FusionRebuildTask *task,
                            char *error, size_t error_size) {
    for (size_t index = 0; index < task->output_count; ++index) {
        char parent[4096];
        int length = snprintf(parent, sizeof parent, "%s", task->outputs[index]);
        if (length < 0 || (size_t)length >= sizeof parent)
            return set_error(error, error_size, "rebuild output path is too long");
        char *slash = strrchr(parent, '/');
        if (slash) {
            *slash = '\0';
            if (!ensure_directory(root, parent, error, error_size)) return false;
        }
        char absolute[4096];
        if (!root_path(absolute, sizeof absolute, root, task->outputs[index],
                       error, error_size)) return false;
        struct stat status;
        if (!lstat(absolute, &status)) {
            if (S_ISLNK(status.st_mode))
                return set_error(error, error_size,
                                 "rebuild output must not be a symlink: %s",
                                 task->outputs[index]);
        } else if (errno != ENOENT)
            return set_error(error, error_size,
                             "cannot inspect rebuild output: %s",
                             task->outputs[index]);
        char temporary[4100];
        length = snprintf(temporary, sizeof temporary, "%s.tmp", absolute);
        if (length < 0 || (size_t)length >= sizeof temporary)
            return set_error(error, error_size, "rebuild output path is too long");
        if (!lstat(temporary, &status)) {
            if (S_ISLNK(status.st_mode))
                return set_error(error, error_size,
                                 "temporary rebuild output must not be a symlink: %s",
                                 task->outputs[index]);
        } else if (errno != ENOENT)
            return set_error(error, error_size,
                             "cannot inspect temporary rebuild output: %s",
                             task->outputs[index]);
    }
    return true;
}

static bool run_process(const char *root, char *const arguments[],
                        const char *label, char *error, size_t error_size) {
    pid_t child = fork();
    if (child < 0)
        return set_error(error, error_size, "%s: cannot start process", label);
    if (child == 0) {
        if (chdir(root)) _exit(126);
        execvp(arguments[0], arguments);
        _exit(errno == ENOENT ? 127 : 126);
    }
    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        return set_error(error, error_size, "%s: cannot wait for process", label);
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return true;
    if (WIFEXITED(status))
        return set_error(error, error_size, "%s failed with exit code %d",
                         label, WEXITSTATUS(status));
    return set_error(error, error_size, "%s terminated by signal %d",
                     label, WTERMSIG(status));
}

static bool run_python(const char *root, const char *module, const char *label,
                       char *error, size_t error_size) {
    char *arguments[] = {"python3", "-m", (char *)module, NULL};
    return run_process(root, arguments, label, error, error_size);
}

bool fusion_rebuild_run_registered_action(const char *root,
                                          const FusionRebuildTask *task,
                                          void *user,
                                          char *error, size_t error_size) {
    (void)user;
    if (!root || !task)
        return set_error(error, error_size, "invalid rebuild action request");
    if (!prepare_outputs(root, task, error, error_size)) return false;

    if (!strcmp(task->name, "inventory"))
        return run_python(root, "scripts.native_inventory", "native inventory", error,
                          error_size);
    if (!strcmp(task->name, "checklist"))
        return run_python(root, "scripts.native_parity", "native checklist", error,
                          error_size);
    if (!strcmp(task->name, "metroid_sound"))
        return run_python(root, "scripts.mzm_audio", "Zero Mission audio export", error,
                          error_size);

    char importer[4096], aria_rom[4096], metroid_rom[4096], mzm_elf[4096];
    char mzm_database[4096], aria_database[4096];
    char output_a[4096], output_b[4096], output_c[4096];
    if (!root_path(importer, sizeof importer, root, "build/fusion_importer", error,
                   error_size) ||
        !root_path(aria_rom, sizeof aria_rom, root,
                   "roms/Castlevania - Aria of Sorrow (USA).gba", error, error_size) ||
        !root_path(metroid_rom, sizeof metroid_rom, root,
                   "roms/Metroid - Zero Mission (USA).gba", error, error_size) ||
        !root_path(mzm_elf, sizeof mzm_elf, root, "third_party/mzm/mzm_us.elf", error,
                   error_size) ||
        !root_path(mzm_database, sizeof mzm_database, root,
                   "third_party/mzm/database.json", error, error_size) ||
        !root_path(aria_database, sizeof aria_database, root,
                   "third_party/cvaos/database.json", error, error_size)) return false;

#define OUTPUT(buffer, relative) \
    root_path(buffer, sizeof buffer, root, relative, error, error_size)
#define RUN(arguments) run_process(root, arguments, "fusion_importer", error, error_size)
    if (!strcmp(task->name, "aria_rom")) {
        if (!OUTPUT(output_a, "assets/extracted/shared/manifests/rom_aria.json")) return false;
        char *arguments[] = {importer, "--rom-manifest", "aria", aria_rom, output_a, NULL};
        return RUN(arguments);
    }
    if (!strcmp(task->name, "metroid_rom")) {
        if (!OUTPUT(output_a, "assets/extracted/shared/manifests/rom_metroid.json")) return false;
        char *arguments[] = {importer, "--rom-manifest", "metroid", metroid_rom,
                             output_a, NULL};
        return RUN(arguments);
    }
    if (!strcmp(task->name, "raw_import")) {
        if (!OUTPUT(output_a, "assets/extracted")) return false;
        char *arguments[] = {importer, "--raw-import", metroid_rom, aria_rom, mzm_elf,
                             mzm_database, aria_database, output_a, NULL};
        return RUN(arguments);
    }
    if (!strcmp(task->name, "aria_world")) {
        if (!OUTPUT(output_a, "assets/extracted/rooms/aria/world.json") ||
            !OUTPUT(output_b, "assets/extracted/rooms/aria/rooms.tsv")) return false;
        char *arguments[] = {importer, "--aria-catalog", aria_rom, output_a, output_b, NULL};
        return RUN(arguments);
    }
    if (!strcmp(task->name, "aria_rooms")) {
        if (!OUTPUT(output_a, "assets/extracted/aria/rooms/runtime")) return false;
        char *arguments[] = {importer, "--aria-export-rooms", aria_rom, output_a, NULL};
        return RUN(arguments);
    }
    if (!strcmp(task->name, "aria_soma")) {
        if (!OUTPUT(output_a, "assets/extracted/aria/sprites/soma/runtime")) return false;
        char *arguments[] = {importer, "--aria-export-soma", aria_rom, output_a, NULL};
        return RUN(arguments);
    }
    if (!strcmp(task->name, "aria_objects") || !strcmp(task->name, "aria_weapons")) {
        if (!OUTPUT(output_a, "assets/extracted/aria/metadata") ||
            !OUTPUT(output_b, !strcmp(task->name, "aria_objects")
                                  ? "assets/extracted/aria/sprites/objects/runtime"
                                  : "assets/extracted/aria/sprites/weapons/runtime")) return false;
        char *arguments[] = {importer,
                             !strcmp(task->name, "aria_objects")
                                 ? "--aria-export-objects" : "--aria-export-weapons",
                             aria_rom, output_a, output_b, NULL};
        return RUN(arguments);
    }
    if (!strcmp(task->name, "metroid_rooms")) {
        if (!OUTPUT(output_a, "assets/extracted/metroid/rooms/runtime")) return false;
        char *arguments[] = {importer, "--mzm-export-rooms", metroid_rom, mzm_elf,
                             output_a, NULL};
        return RUN(arguments);
    }
    if (!strcmp(task->name, "metroid_samus")) {
        if (!OUTPUT(output_a, "assets/extracted/metroid/sprites/samus/runtime") ||
            !OUTPUT(output_b, "assets/extracted/metroid/sprites/projectiles/runtime"))
            return false;
        char *arguments[] = {importer, "--mzm-export-samus", metroid_rom, mzm_elf,
                             output_a, output_b, NULL};
        return RUN(arguments);
    }
    if (!strcmp(task->name, "metroid_sprites")) {
        if (!OUTPUT(output_a, "assets/extracted/metroid/sprites/enemies/runtime") ||
            !OUTPUT(output_b, "assets/extracted/metroid/metadata")) return false;
        char *arguments[] = {importer, "--mzm-export-sprites", metroid_rom, mzm_elf,
                             output_a, output_b, NULL};
        return RUN(arguments);
    }
    if (!strcmp(task->name, "metroid_rom_bank")) {
        if (!OUTPUT(output_c, "assets/extracted/metroid/rom")) return false;
        char *arguments[] = {importer, "--mzm-rom-bank", metroid_rom, mzm_elf,
                             output_c, NULL};
        return RUN(arguments);
    }
#undef RUN
#undef OUTPUT
    return set_error(error, error_size, "no action registered for rebuild task: %s",
                     task->name);
}
