/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_game_over.h"

#include "aos_game_mode.h"

int32_t aos_fixed_divide(int32_t numerator, int32_t denominator) {
    if (denominator == 0) return 0;
    if (denominator == 0x10000) return numerator;
    int32_t top = (numerator < 0 ? -((-numerator) >> 8) : numerator >> 8) << 8;
    int32_t bottom = denominator < 0 ? -((-denominator) >> 8) : denominator >> 8;
    return (top / bottom) << 8;
}

void aos_fade_start(AosPaletteFade *fade, uint8_t r, uint8_t g, uint8_t b, int32_t from,
                    int32_t to, int32_t frames) {
    *fade = (AosPaletteFade){r, g, b, from, to, aos_fixed_divide(to - from, frames << 16), true};
}

void aos_fade_tick(AosPaletteFade *fade) {
    if (!fade->active) return;
    fade->weight += fade->step;
    if ((fade->step >= 0 && fade->weight >= fade->to) ||
        (fade->step < 0 && fade->weight <= fade->to) || fade->step == 0) {
        fade->weight = fade->to;
        fade->active = false;
    }
}

void aos_death_start(AosGameOver *death) {
    *death = (AosGameOver){0};
    aos_fade_start(&death->bg, 0, 0, 0, 0x10000, 0, 0x40);
    aos_fade_start(&death->obj, 0, 0, 0, 0x10000, 0, 0x80);
    death->eva = 0;
    death->evb = 16;
}

bool aos_death_update(AosGameOver *death) {
    bool done = !death->obj.active;
    aos_fade_tick(&death->bg);
    aos_fade_tick(&death->obj);
    return done;
}

int32_t aos_game_over_update(AosGameOver *game_over, const AosInput *input) {
    int32_t mode = AOS_MODE_SAME;
    switch (game_over->stage) {
    case 0:
        /* The BG1 256-colour screen (0x080E5C08, 0x080E5C10, palette
         * 0x080E6A3C, map 0x080E9584) and MUS_15 (not played). */
        aos_fade_start(&game_over->bg, 0, 0, 0, 0, 0x10000, 0x20);
        game_over->obj = (AosPaletteFade){0, 0, 0, 0x10000, 0x10000, 0, false};
        game_over->eva = 0;
        game_over->evb = 16;
        game_over->alpha = 0x20;
        game_over->stage = 1;
        game_over->timer = 0;
        break;
    case 1:
        if (!game_over->bg.active) {
            game_over->stage = 2;
            game_over->timer = 0x80;
        }
        break;
    case 2:
        if (game_over->timer == 0) {
            if (game_over->alpha) {
                game_over->alpha--;
                game_over->evb = (uint8_t)(game_over->alpha >> 1);
                game_over->eva = (uint8_t)(16 - (game_over->alpha >> 1));
            } else {
                game_over->stage = 3;
                game_over->timer = 0;
            }
        } else {
            if (game_over->timer == 1) game_over->sprite_shown = true;
            game_over->timer--;
        }
        break;
    case 3:
        if (input->new_input) {
            aos_fade_start(&game_over->bg, 0x1F, 0x1F, 0x1F, 0x10000, 0, 0x20);
            aos_fade_start(&game_over->obj, 0x1F, 0x1F, 0x1F, 0x10000, 0, 0x20);
            game_over->stage = 4;
            game_over->timer = 0;
        }
        break;
    case 4:
        if (!game_over->bg.active) {
            /* The screen blanks and the GAME OVER sprite's graphics are
             * released. */
            game_over->sprite_shown = false;
            mode = AOS_MODE_KONAMI_LOGO;
        }
        break;
    default: break;
    }
    aos_fade_tick(&game_over->bg);
    aos_fade_tick(&game_over->obj);
    return mode;
}
