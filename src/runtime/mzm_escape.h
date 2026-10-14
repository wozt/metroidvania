/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission escape countdown and the invisible Zebes explosion actor.
 * The timer follows src/escape.c; the actor follows
 * src/sprites_ai/explosion_zebes_escape.c. No SDL dependency. */
#ifndef MZM_ESCAPE_H
#define MZM_ESCAPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mzm_effects.h"

enum {
    MZM_ESCAPE_NONE,
    MZM_ESCAPE_MOTHER_BRAIN,
    MZM_ESCAPE_MECHA_RIDLEY,
};

enum {
    MZM_ESCAPE_STATUS_NONE,
    MZM_ESCAPE_STATUS_HAPPENING,
    MZM_ESCAPE_STATUS_FAILED,
};

typedef struct {
    uint8_t hundredths, tenths;
    uint8_t seconds_ones, seconds_tens;
    uint8_t minutes_ones, minutes_tens;
} MzmEscapeDigits;

typedef struct {
    bool active, initialized;
    uint8_t particle_timer, debris_timer;
    uint16_t spawn_y, spawn_x;
} MzmEscapeExplosion;

typedef struct {
    uint8_t kind, status, counter;
    MzmEscapeDigits digits;
    MzmEscapeExplosion explosion;
} MzmEscape;

/* EscapeDetermineTimer and EscapeCheckHasEscaped, expressed with the four
 * event values they read. */
uint8_t mzm_escape_determine(bool mother_brain_killed, bool escaped_zebes,
                             bool mecha_ridley_killed, bool escaped_chozodia);
bool mzm_escape_has_escaped(bool mother_brain_killed, bool escaped_zebes,
                            bool mecha_ridley_killed, bool escaped_chozodia);

/* EscapeSetTimer plus the status set by ParticleEscape's first update.
 * Difficulty is DIFF_EASY (0), DIFF_NORMAL (1), or DIFF_HARD (2). */
void mzm_escape_start(MzmEscape *escape, uint8_t kind, uint8_t difficulty);
/* One EscapeUpdateTimer. Returns true only on the frame time expires. */
bool mzm_escape_update(MzmEscape *escape, uint8_t current_kind, bool playing,
                       bool movement_prevented);
void mzm_escape_format(const MzmEscape *escape, char *text, size_t size);

void mzm_escape_explosion_start(MzmEscapeExplosion *explosion, uint16_t y, uint16_t x);
/* One ExplosionZebesEscape update. Positions and cameras use native
 * subpixels. `rng` is gSpriteRng. */
void mzm_escape_explosion_update(MzmEscapeExplosion *explosion, MzmEffects *effects,
                                 uint16_t camera_x, uint16_t camera_y, uint16_t samus_y,
                                 uint8_t rng);

#endif /* MZM_ESCAPE_H */
