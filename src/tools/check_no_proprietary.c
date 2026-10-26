/* SPDX-License-Identifier: GPL-3.0-only */
/* Fails when a tracked or unignored file looks like a ROM, a save or an
 * extracted asset: a console ROM/save extension, or more than 4 MiB outside
 * the pinned submodules. */
#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *const forbidden[] = {".gba", ".gb", ".gbc", ".nds", ".3ds", ".cia",
                                        ".sav", ".state"};

/* Path(relative).suffix, lowercased: the last dot of the final component. */
static void suffix_of(const char *relative, char *suffix, size_t size) {
    const char *name = strrchr(relative, '/');
    name = name ? name + 1 : relative;
    const char *dot = strrchr(name, '.');
    suffix[0] = '\0';
    if (!dot || dot == name || !dot[1]) return;
    size_t used = 0;
    for (const char *c = dot; *c && used + 1 < size; ++c) suffix[used++] = (char)tolower((unsigned char)*c);
    suffix[used] = '\0';
}

int main(int argc, char **argv) {
    const char *root = argc > 1 ? argv[1] : ".";
    char command[4096];
    snprintf(command, sizeof command,
             "git -C '%s' ls-files --cached --others --exclude-standard -z", root);
    FILE *list = popen(command, "r");
    if (!list) {
        perror("git ls-files");
        return 2;
    }
    char *entry = NULL;
    size_t capacity = 0;
    ssize_t length;
    int failures = 0;
    while ((length = getdelim(&entry, &capacity, '\0', list)) > 0) {
        if (!entry[0]) continue;
        char suffix[32], path[8192];
        suffix_of(entry, suffix, sizeof suffix);
        for (size_t index = 0; index < sizeof forbidden / sizeof *forbidden; ++index)
            if (!strcmp(suffix, forbidden[index])) {
                if (!failures++) fputs("Potential proprietary data leak:\n", stderr);
                fprintf(stderr, "- forbidden extension: %s\n", entry);
            }
        snprintf(path, sizeof path, "%s/%s", root, entry);
        struct stat status;
        if (!stat(path, &status) && S_ISREG(status.st_mode) && status.st_size > 4 * 1024 * 1024 &&
            strncmp(entry, "third_party/", 12)) {
            if (!failures++) fputs("Potential proprietary data leak:\n", stderr);
            fprintf(stderr, "- file larger than 4 MiB outside submodules: %s\n", entry);
        }
    }
    free(entry);
    if (pclose(list) != 0) {
        fputs("git ls-files failed\n", stderr);
        return 2;
    }
    if (failures) return 1;
    puts("Leak check passed: no tracked proprietary file detected.");
    return 0;
}
