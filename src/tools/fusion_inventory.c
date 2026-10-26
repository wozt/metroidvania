/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/pipeline/inventory.h"

#include <stdio.h>
#include <string.h>

#ifndef FUSION_SOURCE_DIR
#define FUSION_SOURCE_DIR "."
#endif

static void usage(FILE *file, const char *program) {
    fprintf(file, "Usage: %s [--write] [--audit-routines] [--root PATH] [--inventory PATH]\n",
            program);
}

int main(int argc, char **argv) {
    const char *root = FUSION_SOURCE_DIR;
    const char *inventory = "data/native_parity/inventory.json";
    bool audit = false, write = false;
    for (int index = 1; index < argc; ++index) {
        if (!strcmp(argv[index], "--audit-routines")) audit = true;
        else if (!strcmp(argv[index], "--write")) write = true;
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
        char output[4096], error[1024] = "";
        if (inventory[0] == '/') snprintf(output, sizeof output, "%s", inventory);
        else snprintf(output, sizeof output, "%s/%s", root, inventory);
        FusionInventorySummary summary[2];
        if (!fusion_inventory_build(root, output, write, summary, error, sizeof error)) {
            fprintf(stderr, "fusion_inventory: %s\n", error);
            return 2;
        }
        static const char *const games[] = {"mzm", "aos"};
        for (int game = 0; game < 2; ++game)
            printf("%s: %zu routines, %zu declarations, %zu data symbols, %zu resolved call edges\n",
                   games[game], summary[game].routines, summary[game].declarations,
                   summary[game].data_symbols, summary[game].call_edges);
        if (write) printf("Inventory: %s\n", output);
        return 0;
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
