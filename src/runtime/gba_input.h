/* SPDX-License-Identifier: GPL-3.0-only */
/* SDL3 gamepads for both native runtimes.
 *
 * Buttons come out as the GBA KEYINPUT mask (A, B, Select, Start, Right,
 * Left, Up, Down, R, L as bits 0-9), the layout both games read; each
 * runtime adds its own keyboard layout and converts the mask if needed.
 * A gamepad is opened at start-up and on hot-plug, and released when it
 * disconnects (the next connected one is then used). Every GBA button maps
 * to up to GBA_PAD_BINDINGS gamepad buttons; the D-pad also follows the
 * left stick beyond a dead zone. A text map can replace the defaults:
 *
 *     # GBA key, then gamepad buttons: south, east, west, north, or SDL's
 *     # names (SDL_GetGamepadStringForButton: start, back, leftshoulder,
 *     # dpup...; SDL's "a"/"b"/"x"/"y" are the south/east/west/north buttons)
 *     a south
 *     b east west
 *     deadzone 16383
 *     debug back start     # buttons held together for the debug menu
 */
#ifndef GBA_INPUT_H
#define GBA_INPUT_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    GBA_KEY_A = 0x001,
    GBA_KEY_B = 0x002,
    GBA_KEY_SELECT = 0x004,
    GBA_KEY_START = 0x008,
    GBA_KEY_RIGHT = 0x010,
    GBA_KEY_LEFT = 0x020,
    GBA_KEY_UP = 0x040,
    GBA_KEY_DOWN = 0x080,
    GBA_KEY_R = 0x100,
    GBA_KEY_L = 0x200,
};

#define GBA_INPUT_KEYS 10
#define GBA_PAD_BINDINGS 3

typedef struct {
    /* SDL_GamepadButton values per GBA key bit, SDL_GAMEPAD_BUTTON_INVALID
     * when unused. */
    int8_t buttons[GBA_INPUT_KEYS][GBA_PAD_BINDINGS];
    int16_t dead_zone;      /* left stick, out of 32767 */
    /* Buttons held together to open the debug menu (F1's equivalent). */
    int8_t debug[GBA_PAD_BINDINGS];
} GbaPadMap;

typedef struct {
    SDL_Gamepad *pad;
    GbaPadMap map;
} GbaInput;

/* South A; east and west B; shoulders L and R; back Select; start Start;
 * the D-pad and the left stick past half travel; Back + Start for the debug
 * menu. */
void gba_pad_map_default(GbaPadMap *map);
/* One map line ("b east west", "deadzone 12000", blank or # comment). On
 * failure, `error` describes it and the map is unchanged. */
bool gba_pad_map_parse_line(GbaPadMap *map, const char *line, char *error, size_t error_size);
/* A map file over the defaults; false (with `error`) on the first bad line. */
bool gba_pad_map_load(GbaPadMap *map, const char *path, char *error, size_t error_size);
/* The KEYINPUT mask from button states and the left stick. Opposite
 * directions cancel out, as a GBA D-pad cannot report both. */
uint16_t gba_pad_map_buttons(const GbaPadMap *map, const bool pressed[SDL_GAMEPAD_BUTTON_COUNT],
                             int16_t stick_x, int16_t stick_y);
/* Whether every debug chord button is pressed (false for an empty chord). */
bool gba_pad_map_debug(const GbaPadMap *map, const bool pressed[SDL_GAMEPAD_BUTTON_COUNT]);
/* A GBA key name ("a", "select", "up"...) to its bit, or 0. */
uint16_t gba_key_from_name(const char *name);

/* Opens the first connected gamepad (SDL_INIT_GAMEPAD must be on). */
void gba_input_init(GbaInput *input, const GbaPadMap *map);
/* Hot-plug: SDL_EVENT_GAMEPAD_ADDED / REMOVED. */
void gba_input_handle_event(GbaInput *input, const SDL_Event *event);
uint16_t gba_input_buttons(const GbaInput *input);
bool gba_input_button(const GbaInput *input, SDL_GamepadButton button);
bool gba_input_debug_chord(const GbaInput *input);
void gba_input_close(GbaInput *input);

#endif /* GBA_INPUT_H */
