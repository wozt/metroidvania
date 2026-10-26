/* SPDX-License-Identifier: GPL-3.0-only */
/* Regression digests of the native importer on the local ROMs (formerly
 * tests/test_fusion_assets_{aos,mzm}_golden.py). The fixtures record the
 * SHA-256 of every inspection command's standard output, of the console of
 * every writer and of every file it writes; they were recorded after those
 * outputs were proven identical to the former Python writers, and contain
 * only digests, paths and summaries, never ROM-derived bytes.
 *
 * Usage: fusion_native_golden_tests aos|mzm. Regenerate after an intended
 * format change with AOS_GOLDEN_UPDATE=1 or MZM_GOLDEN_UPDATE=1 and review
 * the fixture diff. Exit 77 when the local ROM or importer is absent. */
#define _XOPEN_SOURCE 700
#include "fusion/assets/rom.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef FUSION_SOURCE_DIR
#error "FUSION_SOURCE_DIR must identify the repository source directory"
#endif
#ifndef FUSION_BINARY_DIR
#error "FUSION_BINARY_DIR must identify the build directory"
#endif

#define IMPORTER FUSION_BINARY_DIR "/fusion_importer"
#define ARIA_ROM FUSION_SOURCE_DIR "/roms/Castlevania - Aria of Sorrow (USA).gba"
#define MZM_ROM FUSION_SOURCE_DIR "/roms/Metroid - Zero Mission (USA).gba"
#define MZM_ELF FUSION_SOURCE_DIR "/third_party/mzm/mzm_us.elf"

typedef struct {
    char **items;
    size_t count, capacity;
} Lines;

typedef struct {
    unsigned char *data;
    size_t size;
} Bytes;

static void die(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    fputs("FAIL: ", stderr);
    vfprintf(stderr, format, arguments);
    fputc('\n', stderr);
    va_end(arguments);
    exit(1);
}

static void add(Lines *lines, const char *format, ...) {
    char buffer[8192];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(buffer, sizeof buffer, format, arguments);
    va_end(arguments);
    if (lines->count == lines->capacity) {
        lines->capacity = lines->capacity ? lines->capacity * 2 : 1024;
        lines->items = realloc(lines->items, lines->capacity * sizeof *lines->items);
        if (!lines->items) die("out of memory");
    }
    lines->items[lines->count++] = strdup(buffer);
}

static void digest(const void *data, size_t size, char out[65]) {
    if (!fusion_sha256_hex(data, size, out)) die("SHA-256 failed");
}

static Bytes read_bytes(const char *path) {
    Bytes bytes = {0};
    FILE *file = fopen(path, "rb");
    if (!file) die("cannot read %s", path);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    bytes.data = malloc((size_t)size + 1);
    if (!bytes.data || fread(bytes.data, 1, (size_t)size, file) != (size_t)size) die("cannot read %s", path);
    bytes.size = (size_t)size;
    fclose(file);
    return bytes;
}

static void file_digest(const char *path, char out[65]) {
    Bytes bytes = read_bytes(path);
    digest(bytes.data, bytes.size, out);
    free(bytes.data);
}

/* _run(): the importer's standard output; a failure aborts the test. */
static Bytes run(const char *const *arguments) {
    int pipes[2];
    if (pipe(pipes)) die("pipe failed");
    pid_t child = fork();
    if (child < 0) die("fork failed");
    if (!child) {
        dup2(pipes[1], STDOUT_FILENO);
        close(pipes[0]);
        close(pipes[1]);
        if (chdir(FUSION_SOURCE_DIR)) _exit(126);
        execv(IMPORTER, (char *const *)arguments);
        _exit(127);
    }
    close(pipes[1]);
    Bytes output = {0};
    size_t capacity = 0;
    for (;;) {
        if (output.size + 65536 > capacity) {
            capacity = capacity ? capacity * 2 : 1 << 20;
            output.data = realloc(output.data, capacity);
            if (!output.data) die("out of memory");
        }
        ssize_t got = read(pipes[0], output.data + output.size, capacity - output.size);
        if (got <= 0) break;
        output.size += (size_t)got;
    }
    close(pipes[0]);
    int status = 0;
    waitpid(child, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) die("%s failed", arguments[1]);
    return output;
}

/* bytes.replace(root, b"<OUTPUT>"). */
static Bytes normalized(Bytes output, const char *root) {
    size_t length = strlen(root);
    Bytes result = {malloc(output.size * 2 + 16), 0};
    if (!result.data) die("out of memory");
    for (size_t index = 0; index < output.size;) {
        if (index + length <= output.size && !memcmp(output.data + index, root, length)) {
            memcpy(result.data + result.size, "<OUTPUT>", 8);
            result.size += 8;
            index += length;
        } else {
            result.data[result.size++] = output.data[index++];
        }
    }
    free(output.data);
    return result;
}

/* The last line of a command's output, tabs as spaces. */
static void summary_of(const Bytes *output, char *summary, size_t size) {
    size_t end = output->size;
    while (end && output->data[end - 1] == '\n') --end;
    size_t start = end;
    while (start && output->data[start - 1] != '\n') --start;
    size_t length = end - start < size - 1 ? end - start : size - 1;
    memcpy(summary, output->data + start, length);
    summary[length] = '\0';
    for (char *c = summary; *c; ++c)
        if (*c == '\t') *c = ' ';
}

/* Path order compares components, as sorted(pathlib.Path). */
static int compare_paths(const void *left, const void *right) {
    const char *a = *(const char *const *)left, *b = *(const char *const *)right;
    for (;;) {
        size_t la = strcspn(a, "/"), lb = strcspn(b, "/");
        size_t common = la < lb ? la : lb;
        int order = memcmp(a, b, common);
        if (order) return order;
        if (la != lb) return la < lb ? -1 : 1;
        if (!a[la] || !b[lb]) return a[la] ? 1 : b[lb] ? -1 : 0;
        a += la + 1;
        b += lb + 1;
    }
}

static void collect(const char *root, const char *relative, Lines *files, bool recursive) {
    char path[8192];
    snprintf(path, sizeof path, "%s%s%s", root, *relative ? "/" : "", relative);
    DIR *directory = opendir(path);
    if (!directory) return;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child[8192], full[16384];
        snprintf(child, sizeof child, "%s%s%s", relative, *relative ? "/" : "", entry->d_name);
        snprintf(full, sizeof full, "%s/%s", root, child);
        struct stat status;
        if (lstat(full, &status)) continue;
        if (S_ISDIR(status.st_mode)) {
            if (recursive) collect(root, child, files, true);
        } else if (S_ISREG(status.st_mode)) {
            add(files, "%s", child);
        }
    }
    closedir(directory);
}

/* sorted(root.rglob("*")) of the regular files, relative to root. */
static Lines files_of(const char *root, bool recursive) {
    Lines files = {0};
    collect(root, "", &files, recursive);
    if (files.count) qsort(files.items, files.count, sizeof *files.items, compare_paths);
    return files;
}

static void free_lines(Lines *lines) {
    for (size_t index = 0; index < lines->count; ++index) free(lines->items[index]);
    free(lines->items);
    *lines = (Lines){0};
}

static void file_lines(Lines *lines, const char *kind, const char *prefix, const char *root) {
    Lines files = files_of(root, true);
    for (size_t index = 0; index < files.count; ++index) {
        char path[16384], hash[65];
        snprintf(path, sizeof path, "%s/%s", root, files.items[index]);
        file_digest(path, hash);
        add(lines, "%s\t%s%s\t%s", kind, prefix, files.items[index], hash);
    }
    free_lines(&files);
}

static void console_line(Lines *lines, const char *kind, Bytes console) {
    char hash[65];
    digest(console.data, console.size, hash);
    add(lines, "%s\tstdout\t%s", kind, hash);
    free(console.data);
}

static char work[] = "/tmp/fusion-native-golden-XXXXXX";

static void subdirectory(char *path, size_t size, const char *name, bool create) {
    snprintf(path, size, "%s/%s", work, name);
    if (create && mkdir(path, 0777)) die("cannot create %s", path);
}

static void aos_current(Lines *lines) {
    static const char *const commands[] = {
        "--aria-directory", "--aria-map", "--aria-rooms", "--aria-transitions",
        "--aria-backgrounds", "--aria-background-tiles", "--aria-background-renders",
        "--aria-background-composites", "--aria-collisions", "--aria-runtime-rooms",
        "--aria-loads", "--aria-video-memory", "--aria-entities", "--aria-bosses",
        "--aria-summary"};
    add(lines, "schema\tmetroidvania-aos-native-golden-v1");
    for (size_t index = 0; index < sizeof commands / sizeof *commands; ++index) {
        const char *arguments[] = {IMPORTER, commands[index], ARIA_ROM, NULL};
        Bytes output = run(arguments);
        char hash[65], summary[4096];
        digest(output.data, output.size, hash);
        summary_of(&output, summary, sizeof summary);
        add(lines, "command\t%s\t%s\t%s", commands[index], hash, summary);
        free(output.data);
    }
    char world[4096], rooms[4096], output[4096], soma[4096], metadata[4096], runtime[4096];
    char hash[65];
    subdirectory(world, sizeof world, "world.json", false);
    subdirectory(rooms, sizeof rooms, "rooms.tsv", false);
    const char *catalog[] = {IMPORTER, "--aria-catalog", ARIA_ROM, world, rooms, NULL};
    console_line(lines, "catalog-console", normalized(run(catalog), work));
    file_digest(world, hash);
    add(lines, "catalog-file\tworld.json\t%s", hash);
    file_digest(rooms, hash);
    add(lines, "catalog-file\trooms.tsv\t%s", hash);
    subdirectory(output, sizeof output, "runtime", false);
    const char *export_rooms[] = {IMPORTER, "--aria-export-rooms", ARIA_ROM, output, NULL};
    console_line(lines, "export-console", run(export_rooms));
    file_lines(lines, "export-file", "", output);
    subdirectory(soma, sizeof soma, "soma", true);
    const char *export_soma[] = {IMPORTER, "--aria-export-soma", ARIA_ROM, soma, NULL};
    console_line(lines, "soma-console", normalized(run(export_soma), soma));
    file_lines(lines, "soma-file", "", soma);
    subdirectory(metadata, sizeof metadata, "weapon_metadata", true);
    subdirectory(runtime, sizeof runtime, "weapons", true);
    const char *weapons[] = {IMPORTER, "--aria-export-weapons", ARIA_ROM, metadata, runtime, NULL};
    console_line(lines, "weapon-console", run(weapons));
    file_lines(lines, "weapon-file", "metadata/", metadata);
    file_lines(lines, "weapon-file", "runtime/", runtime);
    subdirectory(metadata, sizeof metadata, "object_metadata", true);
    subdirectory(runtime, sizeof runtime, "objects", true);
    const char *objects[] = {IMPORTER, "--aria-export-objects", ARIA_ROM, metadata, runtime, NULL};
    console_line(lines, "object-console", run(objects));
    file_lines(lines, "object-file", "metadata/", metadata);
    file_lines(lines, "object-file", "runtime/", runtime);
}

static void mzm_current(Lines *lines) {
    static const char *const commands[] = {
        "--mzm-symbols", "--mzm-rooms", "--mzm-clipdata", "--mzm-runtime-room-cores",
        "--mzm-runtime-room-doors", "--mzm-runtime-room-hatches", "--mzm-runtime-room-metadata",
        "--mzm-runtime-room-sprites", "--mzm-backgrounds", "--mzm-tilesets", "--mzm-metatiles",
        "--mzm-background-renders", "--mzm-background-composites", "--mzm-bitmaps",
        "--mzm-runtime-tables", "--mzm-export-backgrounds"};
    add(lines, "schema\tmetroidvania-mzm-native-golden-v1");
    for (size_t index = 0; index < sizeof commands / sizeof *commands; ++index) {
        const char *arguments[] = {IMPORTER, commands[index], MZM_ROM, MZM_ELF, NULL};
        Bytes output = run(arguments);
        char hash[65], summary[4096];
        digest(output.data, output.size, hash);
        summary_of(&output, summary, sizeof summary);
        add(lines, "command\t%s\t%s\t%s", commands[index], hash, summary);
        free(output.data);
    }
    char bank[4096], samus[4096], projectiles[4096], sprites[4096], metadata[4096], output[4096];
    char hash[65], path[16384];
    subdirectory(bank, sizeof bank, "rom", true);
    const char *bank_command[] = {IMPORTER, "--mzm-rom-bank", MZM_ROM, MZM_ELF, bank, NULL};
    console_line(lines, "bank-console", normalized(run(bank_command), work));
    Lines files = files_of(bank, false);
    for (size_t index = 0; index < files.count; ++index) {
        snprintf(path, sizeof path, "%s/%s", bank, files.items[index]);
        file_digest(path, hash);
        add(lines, "bank-file\t%s\t%s", files.items[index], hash);
    }
    free_lines(&files);
    subdirectory(samus, sizeof samus, "samus", false);
    subdirectory(projectiles, sizeof projectiles, "projectiles", false);
    const char *samus_command[] = {IMPORTER, "--mzm-export-samus", MZM_ROM, MZM_ELF, samus,
                                   projectiles, NULL};
    console_line(lines, "samus-console", run(samus_command));
    file_lines(lines, "samus-file", "", samus);
    file_lines(lines, "projectiles-file", "", projectiles);
    subdirectory(sprites, sizeof sprites, "sprites", false);
    subdirectory(metadata, sizeof metadata, "metadata", false);
    const char *sprite_command[] = {IMPORTER, "--mzm-export-sprites", MZM_ROM, MZM_ELF, sprites,
                                    metadata, NULL};
    console_line(lines, "sprites-console", run(sprite_command));
    const char *tables[][2] = {{metadata, "sprites.tsv"}, {sprites, "runtime_index.tsv"},
                               {sprites, "oam_bounds.tsv"}};
    for (size_t index = 0; index < 3; ++index) {
        snprintf(path, sizeof path, "%s/%s", tables[index][0], tables[index][1]);
        file_digest(path, hash);
        add(lines, "sprites-file\t%s\t%s", tables[index][1], hash);
    }
    /* The objects are content-addressed: each name is its digest. */
    char objects[8192];
    snprintf(objects, sizeof objects, "%s/objects", sprites);
    files = files_of(objects, false);
    size_t names_size = 0;
    char *names = calloc(1, files.count * 80 + 1);
    if (!names) die("out of memory");
    for (size_t index = 0; index < files.count; ++index) {
        snprintf(path, sizeof path, "%s/%s", objects, files.items[index]);
        file_digest(path, hash);
        char expected[80];
        snprintf(expected, sizeof expected, "%s.bmp", hash);
        if (strcmp(expected, files.items[index])) die("object digest mismatch: %s", files.items[index]);
        names_size += (size_t)sprintf(names + names_size, "%s%s", index ? "\n" : "",
                                      files.items[index]);
    }
    digest(names, names_size, hash);
    add(lines, "sprites-objects\t%zu\t%s", files.count, hash);
    free(names);
    free_lines(&files);
    subdirectory(output, sizeof output, "runtime", false);
    const char *rooms[] = {IMPORTER, "--mzm-export-rooms", MZM_ROM, MZM_ELF, output, NULL};
    Bytes console = run(rooms);
    digest(console.data, console.size, hash);
    free(console.data);
    add(lines, "console\t%s", hash);
    file_lines(lines, "file", "", output);
}

static void mzm_raw_current(Lines *lines) {
    char root[4096], hash[65], path[16384];
    subdirectory(root, sizeof root, "raw", true);
    const char *arguments[] = {IMPORTER, "--raw-import", MZM_ROM, ARIA_ROM, MZM_ELF,
                               FUSION_SOURCE_DIR "/third_party/mzm/database.json",
                               FUSION_SOURCE_DIR "/third_party/cvaos/database.json", root, NULL};
    Bytes console = run(arguments);
    digest(console.data, console.size, hash);
    free(console.data);
    add(lines, "raw-console\tstdout\t%s", hash);
    static const char *const relatives[] = {"shared/manifests/import_catalog.json",
                                            "rooms/metroid/rooms.tsv"};
    for (size_t index = 0; index < 2; ++index) {
        snprintf(path, sizeof path, "%s/%s", root, relatives[index]);
        file_digest(path, hash);
        add(lines, "raw-file\t%s\t%s", relatives[index], hash);
    }
    /* One digest over every raw block's path and the digest of its bytes. */
    Lines files = {0};
    static const char *const worlds[] = {"metroid", "aria"};
    for (size_t world = 0; world < 2; ++world) {
        char base[4096];
        snprintf(base, sizeof base, "%s/raw", worlds[world]);
        snprintf(path, sizeof path, "%s/%s", root, base);
        Lines found = files_of(path, true);
        for (size_t index = 0; index < found.count; ++index)
            add(&files, "%s/%s", base, found.items[index]);
        free_lines(&found);
    }
    if (files.count) qsort(files.items, files.count, sizeof *files.items, compare_paths);
    size_t size = 0, capacity = 1 << 20;
    unsigned char *joined = malloc(capacity);
    for (size_t index = 0; joined && index < files.count; ++index) {
        size_t length = strlen(files.items[index]);
        if (size + length + 33 > capacity) {
            capacity *= 2;
            joined = realloc(joined, capacity);
            if (!joined) break;
        }
        memcpy(joined + size, files.items[index], length + 1);
        size += length + 1;
        snprintf(path, sizeof path, "%s/%s", root, files.items[index]);
        file_digest(path, hash);
        for (int byte = 0; byte < 32; ++byte) {
            unsigned value = 0;
            sscanf(hash + byte * 2, "%2x", &value);
            joined[size++] = (unsigned char)value;
        }
    }
    if (!joined) die("out of memory");
    digest(joined, size, hash);
    free(joined);
    add(lines, "raw-blocks\t%zu\t%s", files.count, hash);
    free_lines(&files);
}

static Lines read_lines(const char *path) {
    Lines lines = {0};
    FILE *file = fopen(path, "r");
    if (!file) return lines;
    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;
    while ((length = getline(&line, &capacity, file)) >= 0) {
        while (length && (line[length - 1] == '\n' || line[length - 1] == '\r')) line[--length] = '\0';
        add(&lines, "%s", line);
    }
    free(line);
    fclose(file);
    return lines;
}

/* The key of a fixture line: its first two tab-separated fields. */
static void key_of(const char *line, char *key, size_t size) {
    const char *first = strchr(line, '\t');
    const char *second = first ? strchr(first + 1, '\t') : NULL;
    size_t length = second ? (size_t)(second - line) : strlen(line);
    if (length >= size) length = size - 1;
    memcpy(key, line, length);
    key[length] = '\0';
}

static int compare_strings(const void *left, const void *right) {
    return strcmp(*(const char *const *)left, *(const char *const *)right);
}

/* _compare(): the same keys, and the same line for every key. */
static int compare(const Lines *current, size_t current_from, const Lines *expected,
                   size_t expected_from, bool (*keep)(const char *)) {
    int failures = 0;
    Lines produced = {0}, recorded = {0};
    for (size_t index = current_from; index < current->count; ++index)
        add(&produced, "%s", current->items[index]);
    for (size_t index = expected_from; index < expected->count; ++index)
        if (!keep || keep(expected->items[index])) add(&recorded, "%s", expected->items[index]);
    qsort(produced.items, produced.count, sizeof *produced.items, compare_strings);
    qsort(recorded.items, recorded.count, sizeof *recorded.items, compare_strings);
    for (size_t index = 0; index < produced.count; ++index) {
        char key[8192];
        key_of(produced.items[index], key, sizeof key);
        bool found = false;
        for (size_t other = 0; other < recorded.count && !found; ++other) {
            char recorded_key[8192];
            key_of(recorded.items[other], recorded_key, sizeof recorded_key);
            if (strcmp(key, recorded_key)) continue;
            found = true;
            if (strcmp(produced.items[index], recorded.items[other])) {
                fprintf(stderr, "FAIL: %s differs\n  recorded %s\n  produced %s\n", key,
                        recorded.items[other], produced.items[index]);
                ++failures;
            }
        }
        if (!found) {
            fprintf(stderr, "FAIL: unrecorded %s\n", key);
            ++failures;
        }
    }
    for (size_t index = 0; index < recorded.count; ++index) {
        char key[8192];
        key_of(recorded.items[index], key, sizeof key);
        bool found = false;
        for (size_t other = 0; other < produced.count && !found; ++other) {
            char produced_key[8192];
            key_of(produced.items[other], produced_key, sizeof produced_key);
            found = !strcmp(key, produced_key);
        }
        if (!found) {
            fprintf(stderr, "FAIL: missing %s\n", key);
            ++failures;
        }
    }
    free_lines(&produced);
    free_lines(&recorded);
    return failures;
}

static void write_lines(const char *path, const Lines *lines) {
    FILE *file = fopen(path, "w");
    if (!file) die("cannot write %s", path);
    for (size_t index = 0; index < lines->count; ++index) fprintf(file, "%s\n", lines->items[index]);
    fclose(file);
}

static bool is_raw(const char *line) { return !strncmp(line, "raw-", 4); }
static bool not_raw(const char *line) { return !is_raw(line); }

static bool present(const char *path) {
    struct stat status;
    return stat(path, &status) == 0;
}

static int run_aos(void) {
    if (!present(IMPORTER) || !present(ARIA_ROM)) {
        puts("native Aria golden skipped: importer or local ROM unavailable");
        return 77;
    }
    Lines current = {0};
    aos_current(&current);
    const char *fixture = FUSION_SOURCE_DIR "/tests/fixtures/aos_native_golden.tsv";
    if (getenv("AOS_GOLDEN_UPDATE")) write_lines(fixture, &current);
    Lines expected = read_lines(fixture);
    int failures = !expected.count || strcmp(current.items[0], expected.items[0]);
    if (failures) fputs("FAIL: schema differs\n", stderr);
    failures += compare(&current, 1, &expected, 1, NULL);
    free_lines(&current);
    free_lines(&expected);
    return failures ? 1 : 0;
}

/* The room export refuses a symlinked or regular-file output. */
static int export_refusals(void) {
    char target[4096], link[4096], blocked[4096];
    subdirectory(target, sizeof target, "target", true);
    subdirectory(link, sizeof link, "link", false);
    subdirectory(blocked, sizeof blocked, "file", false);
    if (symlink(target, link)) die("cannot create %s", link);
    FILE *file = fopen(blocked, "w");
    if (!file) die("cannot create %s", blocked);
    fputs("x", file);
    fclose(file);
    int failures = 0;
    const char *outputs[] = {link, blocked};
    for (int index = 0; index < 2; ++index) {
        pid_t child = fork();
        if (!child) {
            int null = open("/dev/null", O_WRONLY);
            dup2(null, STDOUT_FILENO);
            dup2(null, STDERR_FILENO);
            execl(IMPORTER, IMPORTER, "--mzm-export-rooms", MZM_ROM, MZM_ELF, outputs[index],
                  (char *)NULL);
            _exit(127);
        }
        int status = 0;
        waitpid(child, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            fprintf(stderr, "FAIL: the room export accepted %s\n", outputs[index]);
            ++failures;
        }
    }
    Lines written = files_of(target, true);
    if (written.count) {
        fputs("FAIL: the room export wrote through a symlink\n", stderr);
        ++failures;
    }
    free_lines(&written);
    return failures;
}

static int run_mzm(void) {
    if (!present(IMPORTER) || !present(MZM_ROM) || !present(MZM_ELF)) {
        puts("native Zero Mission golden skipped: importer, local ROM or ELF unavailable");
        return 77;
    }
    const char *fixture = FUSION_SOURCE_DIR "/tests/fixtures/mzm_native_golden.tsv";
    Lines current = {0};
    mzm_current(&current);
    bool raw = present(ARIA_ROM) &&
               present(FUSION_SOURCE_DIR "/third_party/mzm/database.json") &&
               present(FUSION_SOURCE_DIR "/third_party/cvaos/database.json");
    Lines raw_lines = {0};
    if (raw) mzm_raw_current(&raw_lines);
    if (getenv("MZM_GOLDEN_UPDATE")) {
        Lines old = read_lines(fixture), updated = {0};
        add(&updated, "%s", current.items[0]);
        for (size_t index = 1; index < current.count; ++index) add(&updated, "%s", current.items[index]);
        if (raw) {
            for (size_t index = 0; index < raw_lines.count; ++index)
                add(&updated, "%s", raw_lines.items[index]);
        } else {
            for (size_t index = 1; index < old.count; ++index)
                if (is_raw(old.items[index])) add(&updated, "%s", old.items[index]);
        }
        write_lines(fixture, &updated);
        free_lines(&updated);
        free_lines(&old);
    }
    Lines expected = read_lines(fixture);
    int failures = !expected.count || strcmp(current.items[0], expected.items[0]);
    if (failures) fputs("FAIL: schema differs\n", stderr);
    failures += compare(&current, 1, &expected, 1, not_raw);
    if (raw) failures += compare(&raw_lines, 0, &expected, 1, is_raw);
    else puts("raw import golden skipped: Aria ROM or databases unavailable");
    failures += export_refusals();
    free_lines(&current);
    free_lines(&raw_lines);
    free_lines(&expected);
    return failures ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc != 2 || (strcmp(argv[1], "aos") && strcmp(argv[1], "mzm"))) {
        fputs("usage: fusion_native_golden_tests aos|mzm\n", stderr);
        return 2;
    }
    if (!mkdtemp(work)) die("cannot create a temporary directory");
    int result = !strcmp(argv[1], "aos") ? run_aos() : run_mzm();
    char command[256];
    snprintf(command, sizeof command, "rm -rf '%s'", work);
    if (system(command) != 0) fprintf(stderr, "cannot remove %s\n", work);
    if (!result) printf("native %s golden: ok\n", argv[1]);
    return result;
}
