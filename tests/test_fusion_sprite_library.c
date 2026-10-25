/* SPDX-License-Identifier: GPL-3.0-only */
#include "sprite_library.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#endif

int main(void) {
    const char *output = "fusion-sprite-library-test";
    assert(mkdir(output, 0777) == 0);

    uint8_t rgba[2 * 2 * 4] = {0};
    memcpy(rgba, (uint8_t[]){1, 2, 3, 255}, 4);
    memcpy(rgba + 12, (uint8_t[]){4, 5, 6, 255}, 4);
    uint8_t *bmp = NULL;
    size_t bmp_size = 0;
    int left = 0, top = 0;
    char error[256];
    assert(fusion_sprite_bmp32_crop(rgba, 2, 2, 1, 1, &bmp, &bmp_size,
                                    &left, &top, error, sizeof error));
    assert(bmp_size == 138 && left == -1 && top == -1);
    assert(!memcmp(bmp, "BM", 2));
    assert(!memcmp(bmp + 126, (uint8_t[]){6, 5, 4, 255}, 4));
    assert(!memcmp(bmp + 130, (uint8_t[]){3, 2, 1, 255}, 4));

    FusionSpriteLibrary library;
    assert(fusion_sprite_library_init(&library, output, "private/runtime",
                                      error, sizeof error));
    char stale[1024];
    snprintf(stale, sizeof stale,
             "%s/objects/ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff.bmp",
             output);
    FILE *file = fopen(stale, "wb");
    assert(file && fwrite("BMold", 1, 5, file) == 5 && fclose(file) == 0);
    assert(fusion_sprite_library_begin(&library, error, sizeof error));
    assert(fusion_sprite_library_add(&library, "Soma/animation_000", 0, 4,
                                     left, top, bmp, bmp_size,
                                     error, sizeof error));
    assert(fusion_sprite_library_add(&library, "Soma/animation_000", 1, 2,
                                     left, top, bmp, bmp_size,
                                     error, sizeof error));
    FusionSpriteLibrarySummary summary;
    assert(fusion_sprite_library_finish(&library, &summary,
                                        error, sizeof error));
    assert(summary.sequences == 1 && summary.frames == 2 &&
           summary.unique_bmps == 1 && summary.removed_stale_objects == 1);
    assert(fopen(stale, "rb") == NULL);

    char index_path[1024];
    snprintf(index_path, sizeof index_path, "%s/runtime_index.tsv", output);
    file = fopen(index_path, "rb");
    assert(file);
    char index[1024];
    size_t size = fread(index, 1, sizeof index - 1, file);
    assert(ferror(file) == 0 && fclose(file) == 0);
    index[size] = '\0';
    assert(strstr(index, "schema\tmetroidvania-sprite-index-v1\n"));
    assert(strstr(index, "Soma/animation_000\t1\t2\t-1\t-1\tprivate/runtime/objects/"));

    char object_path[1024];
    snprintf(object_path, sizeof object_path, "%s/objects/%s.bmp", output,
             library.packed[0].digest);
    assert(remove(object_path) == 0);
    assert(remove(index_path) == 0);
    char objects_path[1024];
    snprintf(objects_path, sizeof objects_path, "%s/objects", output);
    assert(remove(objects_path) == 0);
    assert(remove(output) == 0);
    fusion_sprite_library_free(&library);
    free(bmp);
    puts("Native sprite library tests passed");
    return 0;
}
