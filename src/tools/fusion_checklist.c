/* SPDX-License-Identifier: GPL-3.0-only */
/* Native parity-checklist generator. */
#include "internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifndef FUSION_SOURCE_DIR
#define FUSION_SOURCE_DIR "."
#endif

static void usage(FILE *file, const char *program) {
    fprintf(file,
            "Usage: %s [--check] [--root PATH] [--inventory PATH]\n"
            "          [--annotations PATH] [--output PATH]\n"
            "          [--aos-unassigned-output PATH]\n",
            program);
}

int main(int argc, char **argv) {
    const char *root = FUSION_SOURCE_DIR;
    const char *inventory = "data/native_parity/inventory.json";
    const char *annotations = "data/native_parity/annotations.tsv";
    const char *output = "docs/NATIVE_PARITY_CHECKLIST.md";
    const char *unassigned = "data/native_parity/aos_unassigned_routines.tsv";
    bool check = false;
    for (int index = 1; index < argc; ++index) {
        if (!strcmp(argv[index], "--help")) {
            usage(stdout, argv[0]);
            return 0;
        } else if (!strcmp(argv[index], "--check")) check = true;
        else if (!strcmp(argv[index], "--root") && index + 1 < argc)
            root = argv[++index];
        else if (!strcmp(argv[index], "--inventory") && index + 1 < argc)
            inventory = argv[++index];
        else if (!strcmp(argv[index], "--annotations") && index + 1 < argc)
            annotations = argv[++index];
        else if (!strcmp(argv[index], "--output") && index + 1 < argc)
            output = argv[++index];
        else if (!strcmp(argv[index], "--aos-unassigned-output") && index + 1 < argc)
            unassigned = argv[++index];
        else {
            usage(stderr, argv[0]);
            fprintf(stderr, "fusion_checklist: invalid argument: %s\n", argv[index]);
            return 2;
        }
    }
    char error[512] = "";
    if (!fusion_native_checklist(root, inventory, annotations, output, unassigned,
                                 check, error, sizeof error)) {
        fprintf(stderr, "fusion_checklist: %s\n", error);
        return 2;
    }
    printf("Checklist %s: %s\n", check ? "checked" : "written", output);
    printf("Aria unassigned routines %s: %s\n",
           check ? "checked" : "written", unassigned);
    return 0;
}
