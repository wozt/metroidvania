/* SPDX-License-Identifier: GPL-3.0-only */
#include "aria_sim.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int aria_load_sim_script(const char *path, AriaSimDirective **out,
                         size_t *count_out) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Cannot open simulate script %s: %s\n",
                path, strerror(errno));
        return 2;
    }
    AriaSimDirective *dirs = NULL;
    size_t count = 0, capacity = 0;
    long cursor = 0, lineno = 0;
    char line[256];
    while (fgets(line, sizeof line, fp)) {
        ++lineno;
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';
        char *tok[3];
        int ntok = 0;
        bool bad = false;
        char *p = line;
        for (;;) {
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
            if (!*p) break;
            if (ntok == 3) { bad = true; break; }
            tok[ntok++] = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
                ++p;
            if (*p) *p++ = '\0';
        }
        if (bad) {
            fprintf(stderr, "%s:%ld: too many fields in simulate script\n",
                    path, lineno);
            goto fail;
        }
        if (!ntok) continue;
        bool timed = false, ok = true;
        unsigned held = 0;
        long frames = 0;
        if (!strcmp(tok[0], "hold")) {
            if (ntok < 2 || ntok > 3) ok = false;
            else {
                char *end = NULL;
                unsigned long mask = strtoul(tok[1], &end, 0);
                if (*end || mask > 0xFFFFul) ok = false;
                else held = (unsigned)mask;
                if (ok && ntok == 3) {
                    char *fend = NULL;
                    frames = strtol(tok[2], &fend, 10);
                    if (*fend || frames < 0) ok = false;
                    else timed = true;
                }
            }
        } else if (!strcmp(tok[0], "tap")) {
            if (ntok != 3) ok = false;
            else {
                char *end = NULL;
                unsigned long mask = strtoul(tok[1], &end, 0);
                char *fend = NULL;
                frames = strtol(tok[2], &fend, 10);
                if (*end || mask > 0xFFFFul || *fend || frames < 0) ok = false;
                else { held = (unsigned)mask; timed = true; }
            }
        } else if (!strcmp(tok[0], "release")) {
            if (ntok != 1) ok = false;
        } else if (!strcmp(tok[0], "wait")) {
            if (ntok != 2) ok = false;
            else {
                char *fend = NULL;
                frames = strtol(tok[1], &fend, 10);
                if (*fend || frames < 0) ok = false;
                else timed = true;   /* held stays 0 */
            }
        } else ok = false;
        if (!ok) {
            fprintf(stderr, "%s:%ld: bad simulate-script directive\n",
                    path, lineno);
            goto fail;
        }
        if (count == capacity) {
            capacity = capacity ? capacity * 2 : 16;
            AriaSimDirective *grown =
                realloc(dirs, capacity * sizeof *dirs);
            if (!grown) {
                fprintf(stderr, "Out of memory reading simulate script\n");
                goto fail;
            }
            dirs = grown;
        }
        AriaSimDirective *d = &dirs[count++];
        d->pos = cursor;
        d->timed = timed;
        d->held = held;
        d->end = timed ? cursor + frames : 0;
        if (timed) cursor += frames;
    }
    fclose(fp);
    *out = dirs;
    *count_out = count;
    return 0;
fail:
    free(dirs);
    fclose(fp);
    return 2;
}
