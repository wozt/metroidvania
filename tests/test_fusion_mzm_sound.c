/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests of the native sound export on a synthetic ROM and map. */
#define _POSIX_C_SOURCE 200809L
#include "fusion/assets/mzm.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static size_t read_all(const char *path, char *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    assert(file);
    size_t read = fread(buffer, 1, size - 1, file);
    fclose(file);
    buffer[read] = '\0';
    return read;
}

int main(void) {
    static uint8_t data[0x4000];
    for (size_t index = 0; index < sizeof data; ++index) data[index] = (uint8_t)index;
    FusionRom rom = {data, sizeof data, FUSION_ASSET_GAME_METROID};
    /* The tables sit after the sound range; sUnk_808cc4d keeps 64 bytes of
     * LFO window on each side. */
    static const char *const names[] = {
        "sUnk_808cad0", "sClockTable", "sNoiseTable", "sCgb3Vol", "sUnk_808cc4d",
        "sNativeSampleRate", "sMusicPitchData", "sPcmSamplesPerVBlankTable",
        "sArray_808cce2", "sArray_808cee2"};
    char map[4096];
    size_t used = (size_t)snprintf(map, sizeof map,
        " .rodata        0x08000100       0x20 sound/a.o\n"
        " .rodata        0x08000120       0x40 sound/b.o\n"
        " .rodata        0x08000300       0x10 src/c.o\n"
        "                0x08000120                sSoundDataEntries\n");
    for (size_t index = 0; index < sizeof names / sizeof names[0]; ++index)
        used += (size_t)snprintf(map + used, sizeof map - used, "                0x%08x                %s\n",
                                 0x08000400u + (unsigned)index * 0x400u, names[index]);
    char directory[] = "fusion-mzm-sound-XXXXXX", error[256] = "";
    assert(mkdtemp(directory));
    FusionMzmSoundExportSummary summary;
    assert(fusion_mzm_export_sound(&rom, map, used, directory, &summary, error, sizeof error));
    assert(summary.start == 0x08000100 && summary.size == 0x60 && summary.entries == 8);
    char path[256];
    static char text[1 << 16];
    snprintf(path, sizeof path, "%s/sound_bank.bin", directory);
    assert(read_all(path, text, sizeof text) == 16 + 0x60);
    assert(!memcmp(text, "MZMSND01", 8) && (uint8_t)text[16] == 0x00 && (uint8_t)text[17] == 0x01);
    snprintf(path, sizeof path, "%s/sound_tables.tsv", directory);
    read_all(path, text, sizeof text);
    assert(strstr(text, "schema\tmetroidvania-mzm-sound-v1\nentries\t134218016\t8\n") == text);
    /* u16 entries, one past the table for key 127. */
    assert(strstr(text, "table\tsUnk_808cad0\t256,770,"));
    assert(strstr(text, "window\tsUnk_808cc4d\t192,193,"));
    assert(strstr(text, "alias\t90\t14\n") && strstr(text, "default\t169\n"));
    unlink(path);
    snprintf(path, sizeof path, "%s/sound_bank.bin", directory);
    unlink(path);
    rmdir(directory);
    /* A map without sound objects is refused. */
    assert(!fusion_mzm_export_sound(&rom, " .rodata 0x08000000 0x10 src/a.o\n", 34, "unused",
                                    &summary, error, sizeof error));
    assert(strstr(error, "sSoundDataEntries") || strstr(error, "no sound/"));
    puts("fusion_mzm_sound tests passed");
    return 0;
}
