/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/pipeline/inventory.h"

#include <stdio.h>
#include <string.h>

#ifndef FUSION_SOURCE_DIR
#define FUSION_SOURCE_DIR "."
#endif

static void usage(FILE *file, const char *program) {
    fprintf(file, "Usage: %s [--audit-routines] [--root PATH] [--inventory PATH]\n",
            program);
}

int main(int argc, char **argv) {
    const char *root = FUSION_SOURCE_DIR;
    const char *inventory = "data/native_parity/inventory.json";
    bool audit = false;
    for (int index = 1; index < argc; ++index) {
        if (!strcmp(argv[index], "--audit-routines")) audit = true;
        else if (!strcmp(argv[index], "--root") && index + 1 < argc)
            root = argv[++index];
        else if (!strcmp(argv[index], "--inventory") && index + 1 < argc)
            inventory = argv[++index];
        else if (!strcmp(argv[index], "--help")) {
            usage(stdout, argv[0]);
            return 0;
        } else {
            usage(stderr, argv[0]);
            return 2;
        }
    }
    if (!audit) {
        usage(stderr, argv[0]);
        return 2;
    }
    size_t counts[2] = {0};
    char error[512] = "";
    if (!fusion_inventory_audit_routines(root, inventory, counts,
                                         error, sizeof error)) {
        fprintf(stderr, "fusion_inventory: %s\n", error);
        return 1;
    }
    printf("mzm: %zu routine records match\n", counts[0]);
    printf("aos: %zu routine records match\n", counts[1]);
    return 0;
}
