/* SPDX-License-Identifier: GPL-3.0-only */
#include "mzm_escape.h"

#include "mzm_screen_shake.h"
#include "mzm_sfx.h"
#include "mzm_sprites.h"

#include <stdio.h>

enum { BLOCK_SIZE = 64, HALF_BLOCK_SIZE = 32, QUARTER_BLOCK_SIZE = 16 };

uint8_t mzm_escape_determine(bool mother_brain_killed, bool escaped_zebes,
                             bool mecha_ridley_killed, bool escaped_chozodia) {
    if (!escaped_zebes)
        return mother_brain_killed ? MZM_ESCAPE_MOTHER_BRAIN : MZM_ESCAPE_NONE;
    if (!escaped_chozodia && mecha_ridley_killed) return MZM_ESCAPE_MECHA_RIDLEY;
    return MZM_ESCAPE_NONE;
}

bool mzm_escape_has_escaped(bool mother_brain_killed, bool escaped_zebes,
                            bool mecha_ridley_killed, bool escaped_chozodia) {
    if (mecha_ridley_killed) return escaped_chozodia;
    return mother_brain_killed && escaped_zebes;
}

static void set_digits(MzmEscape *escape, unsigned minutes, unsigned seconds,
                       unsigned hundredths) {
    escape->digits.hundredths = (uint8_t)(hundredths % 10);
    escape->digits.tenths = (uint8_t)(hundredths / 10);
    escape->digits.seconds_ones = (uint8_t)(seconds % 10);
    escape->digits.seconds_tens = (uint8_t)(seconds / 10);
    escape->digits.minutes_ones = (uint8_t)(minutes % 10);
    escape->digits.minutes_tens = (uint8_t)(minutes / 10);
}

void mzm_escape_start(MzmEscape *escape, uint8_t kind, uint8_t difficulty) {
    if (!escape) return;
    escape->kind = kind;
    escape->status = MZM_ESCAPE_STATUS_HAPPENING;
    escape->counter = UINT8_MAX;
    if (kind == MZM_ESCAPE_MOTHER_BRAIN)
        set_digits(escape, difficulty == 0 ? 3 : difficulty == 2 ? 1 : 2, 0, 0);
    else if (kind == MZM_ESCAPE_MECHA_RIDLEY)
        set_digits(escape, difficulty == 2 ? 3 : 5, 0, 0);
    else
        set_digits(escape, 99, 99, 99);
}

bool mzm_escape_update(MzmEscape *escape, uint8_t current_kind, bool playing,
                       bool movement_prevented) {
    if (!escape) return false;
    if (current_kind == MZM_ESCAPE_NONE || escape->status != MZM_ESCAPE_STATUS_HAPPENING) {
        escape->status = MZM_ESCAPE_STATUS_NONE;
        return false;
    }
    if (!playing || movement_prevented) return false;

    uint8_t counter = ++escape->counter;
    if (counter % 2) {
        if (escape->digits.hundredths > 1) escape->digits.hundredths -= 2;
        else {
            escape->digits.hundredths += 8;
            if (escape->digits.tenths) escape->digits.tenths--;
            else escape->digits.tenths = 9;
        }
    } else if (escape->digits.hundredths) escape->digits.hundredths--;
    else {
        escape->digits.hundredths = 9;
        if (escape->digits.tenths) escape->digits.tenths--;
        else escape->digits.tenths = 9;
    }

    if (counter % 60) return false;
    escape->digits.hundredths = 9;
    escape->digits.tenths = 9;
    if (escape->digits.seconds_ones) {
        escape->digits.seconds_ones--;
        return false;
    }
    escape->digits.seconds_ones = 9;
    if (escape->digits.seconds_tens) {
        escape->digits.seconds_tens--;
        return false;
    }
    escape->digits.seconds_tens = 5;
    if (escape->digits.minutes_ones) {
        escape->digits.minutes_ones--;
        return false;
    }
    escape->digits.minutes_ones = 9;
    if (escape->digits.minutes_tens) {
        escape->digits.minutes_tens--;
        return false;
    }
    set_digits(escape, 0, 0, 0);
    escape->status = MZM_ESCAPE_STATUS_FAILED;
    return true;
}

void mzm_escape_format(const MzmEscape *escape, char *text, size_t size) {
    if (!text || !size) return;
    if (!escape) {
        text[0] = '\0';
        return;
    }
    snprintf(text, size, "%u%u:%u%u.%u%u", escape->digits.minutes_tens,
             escape->digits.minutes_ones, escape->digits.seconds_tens,
             escape->digits.seconds_ones, escape->digits.tenths,
             escape->digits.hundredths);
}

void mzm_escape_failure_start(MzmEscapeFailure *failure) {
    if (!failure) return;
    *failure = (MzmEscapeFailure){.active = true};
    /* ColorFadingProcess_EscapeFailed stage 0. */
    mzm_sfx_fade_all(60);
    mzm_sfx_music_fade(60);
}

bool mzm_escape_failure_update(MzmEscapeFailure *failure) {
    static const uint8_t levels[8] = {3, 7, 11, 15, 19, 23, 27, 32};
    if (!failure || !failure->active) return false;
    switch (failure->stage) {
    case 0:
        /* unk_5c220 and the two audio fades were started above. */
        failure->stage = 1;
        break;
    case 1:
        if (failure->fade_timer < sizeof levels) {
            failure->level = levels[failure->fade_timer++];
        } else {
            /* The finished ColorFadingUpdate call advances the process on
             * the frame after the last visible level. */
            failure->stage = 2;
        }
        break;
    case 2:
        /* unk_5d09c, then SET_BACKDROP_COLOR(COLOR_WHITE). */
        failure->level = 32;
        failure->hold_timer = 0;
        failure->stage = 3;
        break;
    case 3:
        if (++failure->hold_timer > 60) {
            failure->active = false;
            return true;
        }
        break;
    default:
        failure->active = false;
        break;
    }
    return false;
}

void mzm_escape_explosion_start(MzmEscapeExplosion *explosion, uint16_t y, uint16_t x) {
    if (!explosion) return;
    *explosion = (MzmEscapeExplosion){true, false, 0, 7, y, x};
}

static uint16_t pos(int value) { return (uint16_t)value; }

void mzm_escape_explosion_update(MzmEscapeExplosion *explosion, MzmEffects *effects,
                                 uint16_t camera_x, uint16_t camera_y, uint16_t samus_y,
                                 uint8_t rng) {
    if (!explosion || !explosion->active || !effects) return;
    /* The spawned actor spends its first AI call in initialization. */
    if (!explosion->initialized) {
        explosion->initialized = true;
        return;
    }
    int y = explosion->spawn_y;
    int x = camera_x + BLOCK_SIZE * 7 + HALF_BLOCK_SIZE;
    uint8_t shift = rng & 3;
    uint8_t particle_timer = explosion->particle_timer++;
    uint8_t debris_timer = explosion->debris_timer++;
    if (samus_y < explosion->spawn_y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE))
        y = samus_y + BLOCK_SIZE + HALF_BLOCK_SIZE + 4;

    if (particle_timer % 16 == 0 && rng > 7) {
        uint8_t effect;
        uint16_t sound;
        int py, px;
        if (particle_timer & 16) {
            if (rng > 11) {
                if (rng & 1) {
                    py = y - (BLOCK_SIZE * 3 + QUARTER_BLOCK_SIZE + 12) + rng * 8;
                    px = x + BLOCK_SIZE * 2 + HALF_BLOCK_SIZE + (rng << shift);
                    effect = MZM_PE_SPRITE_EXPLOSION_HUGE;
                    sound = MZM_SOUND_ZEBES_ESCAPE_EXPLOSION_1;
                } else {
                    py = y - (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE + 6) + rng * 8;
                    px = x + (rng << shift);
                    effect = MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG;
                    sound = MZM_SOUND_ZEBES_ESCAPE_EXPLOSION_3;
                }
            } else if (rng & 1) {
                py = y - (BLOCK_SIZE * 3 + QUARTER_BLOCK_SIZE + 12) + rng * 8;
                px = x - (rng << shift);
                effect = MZM_PE_SPRITE_EXPLOSION_HUGE;
                sound = MZM_SOUND_ZEBES_ESCAPE_EXPLOSION_1;
            } else {
                py = y - (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE + 6) + rng * 8;
                px = x - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) - (rng << shift);
                effect = MZM_PE_SPRITE_EXPLOSION_BIG;
                sound = MZM_SOUND_ZEBES_ESCAPE_EXPLOSION_3;
            }
        } else {
            mzm_screen_shake_start_vertical(10, 0x81);
            mzm_screen_shake_start_horizontal(10, 0x81);
            if (rng > 11) {
                if (rng & 1) {
                    py = y - BLOCK_SIZE * 5 + rng * 16;
                    px = x + BLOCK_SIZE * 3 + (rng << shift);
                    effect = MZM_PE_TWO_MEDIUM_DUST;
                    sound = MZM_SOUND_ZEBES_ESCAPE_EXPLOSION_2;
                } else {
                    py = y - (BLOCK_SIZE * 4 + QUARTER_BLOCK_SIZE + 8) + rng * 16;
                    px = x + (rng << shift);
                    effect = MZM_PE_MEDIUM_DUST;
                    sound = MZM_SOUND_ZEBES_ESCAPE_EXPLOSION_4;
                }
            } else if (rng & 1) {
                py = y - (BLOCK_SIZE * 5 + HALF_BLOCK_SIZE + 8) + rng * 16;
                px = x - BLOCK_SIZE * 3 - (rng << shift);
                effect = MZM_PE_MEDIUM_DUST;
                sound = MZM_SOUND_ZEBES_ESCAPE_EXPLOSION_4;
            } else {
                py = y - (BLOCK_SIZE * 4 + HALF_BLOCK_SIZE + 12) + rng * 16;
                px = x - (rng << shift);
                effect = MZM_PE_TWO_MEDIUM_DUST;
                sound = MZM_SOUND_ZEBES_ESCAPE_EXPLOSION_2;
            }
        }
        mzm_particle_set(effects, pos(py), pos(px), effect);
        mzm_sfx_play_not_playing(sound);
    }

    y = camera_y - BLOCK_SIZE;
    if (debris_timer % 32 == 0) {
        if (debris_timer & 32) {
            mzm_sprite_debris_init(effects, MZM_DEBRIS_5, pos(y),
                                   pos(x + BLOCK_SIZE * 2 - QUARTER_BLOCK_SIZE + 8 - rng * 16));
            mzm_sprite_debris_init(effects, MZM_DEBRIS_8, pos(y),
                                   pos(x - (BLOCK_SIZE * 6 + QUARTER_BLOCK_SIZE) + rng * 8));
        } else {
            mzm_sprite_debris_init(effects, MZM_DEBRIS_7, pos(y),
                                   pos(x - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) - rng * 16));
            mzm_sprite_debris_init(effects, MZM_DEBRIS_5, pos(y),
                                   pos(x - (BLOCK_SIZE * 4 + HALF_BLOCK_SIZE + 12) + rng * 8));
        }
    }
    if (particle_timer % 16 == 0) {
        if (rng > 7) {
            mzm_sprite_debris_init(effects, MZM_DEBRIS_8, pos(y),
                                   pos(x - (BLOCK_SIZE * 7 + 2) + rng * 32));
            mzm_sprite_debris_init(effects, MZM_DEBRIS_6, pos(y),
                                   pos(x + BLOCK_SIZE * 9 + QUARTER_BLOCK_SIZE - 2 - rng * 32));
        } else {
            mzm_sprite_debris_init(effects, MZM_DEBRIS_6, pos(y),
                                   pos(x + BLOCK_SIZE * 6 - 4 - rng * 32));
            mzm_sprite_debris_init(effects, MZM_DEBRIS_8, pos(y),
                                   pos(x - (BLOCK_SIZE * 10 - QUARTER_BLOCK_SIZE + 8) + rng * 8));
        }
    }
}
