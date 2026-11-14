/* SPDX-License-Identifier: GPL-3.0-only */
/* --simulate-script FILE: a per-frame held-input script for --simulate.
 * One directive per line ('#' starts a comment, blank lines are ignored):
 *   hold MASK [FRAMES]  base held mask MASK from this frame until the next
 *                       hold/release; with FRAMES, a timed span that holds
 *                       MASK for FRAMES frames without changing the base
 *   tap MASK FRAMES     timed span holding MASK for FRAMES frames
 *   release             base held mask 0 from this frame on
 *   wait FRAMES         timed span holding 0 for FRAMES frames
 * MASK is a strtoul base-0 AOS_KEY_* mask like --simulate-hold's. Timed
 * spans override the base mask and the base resumes when a span ends, so
 * "hold MASK" followed by "wait N" idles for N frames then walks again.
 * Positions are implicit: each timed directive consumes its frames and the
 * next directive starts where the previous one ended. */
#ifndef ARIA_SIM_H
#define ARIA_SIM_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    long pos;       /* first frame the directive applies to */
    long end;       /* timed span end (pos + frames); unused for state */
    unsigned held;  /* AOS_KEY_* mask: timed span value or new base mask */
    bool timed;     /* true: held applies inside [pos, end); false: sets base */
} AriaSimDirective;

/* Parse the script once; returns 0 on success, 2 on a malformed file. */
int aria_load_sim_script(const char *path, AriaSimDirective **out,
                         size_t *count_out);

#endif
