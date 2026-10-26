/* SPDX-License-Identifier: GPL-3.0-only */
/* Command-line contracts of the native tools: the GBA header extractor on a
 * ROM-free fixture, and the room package viewer's optional local BG1/BG2 and
 * standalone BG3 diagnostics, which never alter a project export. */
#define _XOPEN_SOURCE 700
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef FUSION_SOURCE_DIR
#error "FUSION_SOURCE_DIR must identify the repository source directory"
#endif
#ifndef FUSION_BINARY_DIR
#error "FUSION_BINARY_DIR must identify the build directory"
#endif

static int failures = 0;
static char directory[] = "/tmp/fusion-tool-contracts-XXXXXX";

static void fail(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    fputs("FAIL: ", stderr);
    vfprintf(stderr, format, arguments);
    fputc('\n', stderr);
    va_end(arguments);
    ++failures;
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return calloc(1, 1);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *text = calloc((size_t)size + 1, 1);
    if (text && fread(text, 1, (size_t)size, file) != (size_t)size) text[0] = '\0';
    fclose(file);
    return text;
}

/* Runs a shell command line; its stdout and stderr go to the captures. */
static int run(const char *command, char **out, char **err) {
    char line[8192], out_path[256], err_path[256];
    snprintf(out_path, sizeof out_path, "%s/stdout", directory);
    snprintf(err_path, sizeof err_path, "%s/stderr", directory);
    snprintf(line, sizeof line, "cd '%s' && %s >'%s' 2>'%s'", FUSION_SOURCE_DIR, command,
             out_path, err_path);
    int status = system(line);
    *out = read_file(out_path);
    *err = read_file(err_path);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static void write_bytes(const char *path, const unsigned char *data, size_t size) {
    FILE *file = fopen(path, "wb");
    if (!file || fwrite(data, 1, size, file) != size) fail("cannot write %s", path);
    if (file) fclose(file);
}

static void put32(unsigned char *data, unsigned value) {
    data[0] = (unsigned char)value;
    data[1] = (unsigned char)(value >> 8);
    data[2] = (unsigned char)(value >> 16);
    data[3] = (unsigned char)(value >> 24);
}

/* A 24-bit BMP filled with one color. */
static void write_bmp(const char *path, int width, int height, const unsigned char color[3]) {
    size_t pitch = ((size_t)width * 3 + 3) & ~(size_t)3, size = 54 + pitch * (size_t)height;
    unsigned char *data = calloc(1, size);
    if (!data) exit(2);
    data[0] = 'B';
    data[1] = 'M';
    put32(data + 2, (unsigned)size);
    put32(data + 10, 54);
    put32(data + 14, 40);
    put32(data + 18, (unsigned)width);
    put32(data + 22, (unsigned)height);
    data[26] = 1;
    data[28] = 24;
    put32(data + 34, (unsigned)(pitch * (size_t)height));
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) memcpy(data + 54 + (size_t)y * pitch + (size_t)x * 3, color, 3);
    write_bytes(path, data, size);
    free(data);
}

static void test_importer_header(void) {
    unsigned char rom[0xC0] = {0};
    memcpy(rom + 0xA0, "SYNTHETIC TE", 12);
    memcpy(rom + 0xAC, "ABCD", 4);
    memcpy(rom + 0xB0, "01", 2);
    rom[0xB2] = 0x96;
    rom[0xB3] = 0x01;
    rom[0xB4] = 0x02;
    rom[0xBC] = 0x03;
    unsigned sum = 0;
    for (int index = 0xA0; index < 0xBD; ++index) sum += rom[index];
    rom[0xBD] = (unsigned char)(-(int)(sum + 0x19));
    char path[256], command[512], *out, *err;
    snprintf(path, sizeof path, "%s/header.gba", directory);
    write_bytes(path, rom, sizeof rom);
    snprintf(command, sizeof command, "'%s/fusion_importer' --header '%s'", FUSION_BINARY_DIR, path);
    if (run(command, &out, &err) != 0) fail("fusion_importer --header: %s", err);
    static const char *const fields[] = {"title=SYNTHETIC TE\n", "game_code=ABCD\n",
                                         "maker_code=01\n", "main_unit_code=0x01\n",
                                         "device_type=0x02\n", "software_version=3\n",
                                         "checksum=valid\n"};
    for (size_t index = 0; index < sizeof fields / sizeof *fields; ++index)
        if (!strstr(out, fields[index])) fail("header: missing %s", fields[index]);
    free(out);
    free(err);
}

static void test_viewer_sources(void) {
    char path[4096];
    snprintf(path, sizeof path, "%s/src/tools/room_package_viewer.c", FUSION_SOURCE_DIR);
    char *viewer = read_file(path);
    static const char *const terms[] = {
        "PATCH_0125_LOCAL_NATIVE_BG", "local_mzm_background",
        "rooms/metroid/previews/%s_%03d_bg%d.bmp",
        "surface->w != preview->width || surface->h != preview->height",
        "SDL_CreateTextureFromSurface", "SDL_SCALEMODE_NEAREST",
        "SDL_RenderTexture(renderer, textures[active - 1], NULL, &image)", "SDLK_C", "SDLK_M",
        "SDLK_1", "SDLK_2", "--no-auto-bg", "--bg3 path.bmp", "SDLK_4",
        "experimental BG3 (independent native tilemap)", "active != 4 && i < preview.count",
        "active == 4 ? surfaces[3]->w : preview.width", "BG3 diagnostic dimensions invalid",
        "SDL_DestroyTexture(textures[3]);"};
    for (size_t index = 0; index < sizeof terms / sizeof *terms; ++index)
        if (!strstr(viewer, terms[index])) fail("viewer: missing %s", terms[index]);
    if (strstr(viewer, "SDL_RenderTexture(renderer, textures[0], NULL, &image);"))
        fail("viewer: BG1 drawn unconditionally");
    snprintf(path, sizeof path, "%s/src/editor_backend/commands_room.c", FUSION_SOURCE_DIR);
    char *exporter = read_file(path);
    if (!strstr(exporter, "feb_put_bool(&payload, \"native_assets_included\", false)"))
        fail("room-export: native assets flag");
    free(exporter);
    free(viewer);
}

static void test_viewer_checks(void) {
    char viewer[4096];
    snprintf(viewer, sizeof viewer, "%s/fusion_room_package_viewer", FUSION_BINARY_DIR);
    if (access(viewer, X_OK)) {
        puts("room package viewer checks skipped: not built");
        return;
    }
    const char *fixture = "tests/fixtures/project_room_0124.preview.tsv";
    const unsigned char blue[3] = {100, 70, 35}, black[3] = {0, 0, 0};
    char bg1[256], wrong[256], bg3[256], tall[256], bad[256], command[8192], *out, *err;
    snprintf(bg1, sizeof bg1, "%s/bg1.bmp", directory);
    snprintf(wrong, sizeof wrong, "%s/wrong.bmp", directory);
    snprintf(bg3, sizeof bg3, "%s/bg3.bmp", directory);
    snprintf(tall, sizeof tall, "%s/tall.bmp", directory);
    snprintf(bad, sizeof bad, "%s/bad.bmp", directory);
    write_bmp(bg1, 128, 64, blue);
    write_bmp(wrong, 160, 64, blue);
    write_bmp(bg3, 256, 256, black);
    write_bmp(tall, 256, 512, black);
    write_bmp(bad, 144, 64, black);
    snprintf(command, sizeof command, "'%s' --check --no-auto-bg --bg1 '%s' %s", viewer, bg1, fixture);
    if (run(command, &out, &err) != 0 || !strstr(out, "local BG1=matching"))
        fail("viewer: matching BG1 (%s)", err);
    free(out);
    free(err);
    snprintf(command, sizeof command, "'%s' --check --no-auto-bg --bg1 '%s' %s", viewer, wrong,
             fixture);
    if (run(command, &out, &err) != 2 || !strstr(err, "BMP dimensions do not match"))
        fail("viewer: mismatched BG1 accepted");
    free(out);
    free(err);
    snprintf(command, sizeof command, "'%s' --check %s", viewer, fixture);
    if (run(command, &out, &err) != 0) fail("viewer: plain check (%s)", err);
    free(out);
    free(err);
    const char *valid[] = {bg3, tall};
    for (int index = 0; index < 2; ++index) {
        snprintf(command, sizeof command, "'%s' --check --no-auto-bg --bg3 '%s' %s", viewer,
                 valid[index], fixture);
        if (run(command, &out, &err) != 0 || !strstr(out, "BG3=native-tilemap"))
            fail("viewer: BG3 %s refused (%s)", valid[index], err);
        free(out);
        free(err);
    }
    snprintf(command, sizeof command, "'%s' --check --no-auto-bg --bg3 '%s' %s", viewer, bad, fixture);
    if (run(command, &out, &err) != 2 || !strstr(err, "BG3 diagnostic dimensions invalid"))
        fail("viewer: invalid BG3 accepted");
    free(out);
    free(err);
    snprintf(command, sizeof command, "'%s' --check --no-auto-bg %s", viewer, fixture);
    if (run(command, &out, &err) != 0 || !strstr(out, "BG3=absent")) fail("viewer: BG3 absent");
    free(out);
    free(err);
}

int main(void) {
    if (!mkdtemp(directory)) {
        perror("mkdtemp");
        return 1;
    }
    test_importer_header();
    test_viewer_sources();
    test_viewer_checks();
    char command[512];
    snprintf(command, sizeof command, "rm -rf '%s'", directory);
    if (system(command) != 0) fail("cannot remove %s", directory);
    if (failures) {
        fprintf(stderr, "%d tool contract failure(s)\n", failures);
        return 1;
    }
    puts("tool contracts: ok");
    return 0;
}
