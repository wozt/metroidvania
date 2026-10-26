/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "fusion/pipeline/inventory.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(text, 1, strlen(text), file) == strlen(text));
    assert(!fclose(file));
}

int main(void) {
    char root_template[] = "/tmp/fusion-inventory-XXXXXX";
    char *root = mkdtemp(root_template);
    assert(root != NULL);
    char src[512], c_path[1024], s_path[1024];
    assert(snprintf(src, sizeof src, "%s/src", root) > 0);
    assert(snprintf(c_path, sizeof c_path, "%s/example.c", src) > 0);
    assert(snprintf(s_path, sizeof s_path, "%s/example.s", src) > 0);
    assert(!mkdir(src, 0700));
    write_text(c_path,
        "void declaration(void);\n"
        "/** @brief 123 | Example controller */\n"
        "static void Example(void) {\n"
        "  Called(); Called(); /* Hidden(); */\n"
        "  const char *text = \"AlsoHidden()\";\n"
        "  if (text) Other();\n"
        "}\n"
        "void sub_08001234(void) {}\n");
    write_text(s_path,
        "thumb_func_start Example\n"
        "Example:\n"
        "  bl IgnoredDuplicateBody\n"
        ".type AssemblyOnly, %function\n"
        "AssemblyOnly:\n"
        "  bl Called\n"
        "  b Tail\n");

    FusionInventoryRoutines routines = {0};
    char error[256] = "";
    assert(fusion_inventory_scan_routines("mzm", root, &routines,
                                          error, sizeof error));
    assert(routines.count == 3);
    const FusionInventoryRoutine *assembly = &routines.items[0];
    const FusionInventoryRoutine *example = &routines.items[1];
    const FusionInventoryRoutine *addressed = &routines.items[2];
    assert(!strcmp(assembly->id, "mzm:asm:src/example.s:AssemblyOnly"));
    assert(assembly->raw_call_count == 2);
    assert(!strcmp(assembly->raw_calls[0], "Called"));
    assert(!strcmp(assembly->raw_calls[1], "Tail"));
    assert(!strcmp(example->id, "mzm:c:src/example.c:Example"));
    assert(!strcmp(example->linkage, "internal"));
    assert(!strcmp(example->description, "Example controller"));
    assert(!strcmp(example->address, "0x08000123"));
    assert(example->raw_call_count == 2);
    assert(!strcmp(example->raw_calls[0], "Called"));
    assert(!strcmp(example->raw_calls[1], "Other"));
    assert(example->source_variant_count == 1);
    assert(!strcmp(example->source_variants[0], "src/example.s"));
    assert(!strcmp(addressed->address, "0x08001234"));
    fusion_inventory_routines_free(&routines);
    assert(!unlink(c_path));
    assert(!unlink(s_path));
    assert(!rmdir(src));
    assert(!rmdir(root));
    return 0;
}
