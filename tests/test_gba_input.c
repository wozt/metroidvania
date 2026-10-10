/* SPDX-License-Identifier: GPL-3.0-only */
/* Device-free tests for the shared gamepad mapping. */
#include "gba_input.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    GbaPadMap map;
    gba_pad_map_default(&map);
    bool pressed[SDL_GAMEPAD_BUTTON_COUNT] = {0};
    assert(gba_pad_map_buttons(&map, pressed, 0, 0) == 0);

    /* Defaults: south A, east and west B, shoulders, back, start, D-pad. */
    pressed[SDL_GAMEPAD_BUTTON_SOUTH] = pressed[SDL_GAMEPAD_BUTTON_WEST] = true;
    pressed[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] = pressed[SDL_GAMEPAD_BUTTON_START] = true;
    assert(gba_pad_map_buttons(&map, pressed, 0, 0) ==
           (GBA_KEY_A | GBA_KEY_B | GBA_KEY_L | GBA_KEY_START));
    memset(pressed, 0, sizeof pressed);

    /* The stick past half travel, and opposite directions cancelling. */
    assert(gba_pad_map_buttons(&map, pressed, 16383, -16383) == 0);
    assert(gba_pad_map_buttons(&map, pressed, 16384, -16384) == (GBA_KEY_RIGHT | GBA_KEY_UP));
    pressed[SDL_GAMEPAD_BUTTON_DPAD_LEFT] = true;
    assert(gba_pad_map_buttons(&map, pressed, 20000, 0) == 0);
    pressed[SDL_GAMEPAD_BUTTON_DPAD_LEFT] = false;

    /* Remapping: B on north only, A on south and east, a smaller dead zone. */
    char error[160];
    assert(gba_pad_map_parse_line(&map, "b north", error, sizeof error));
    assert(gba_pad_map_parse_line(&map, "  A south east  # comment", error, sizeof error));
    assert(gba_pad_map_parse_line(&map, "deadzone 8000", error, sizeof error));
    assert(gba_pad_map_parse_line(&map, "# only a comment", error, sizeof error));
    assert(gba_pad_map_parse_line(&map, "", error, sizeof error));
    pressed[SDL_GAMEPAD_BUTTON_EAST] = true;
    assert(gba_pad_map_buttons(&map, pressed, 9000, 0) == (GBA_KEY_A | GBA_KEY_RIGHT));
    pressed[SDL_GAMEPAD_BUTTON_EAST] = false;
    pressed[SDL_GAMEPAD_BUTTON_NORTH] = true;
    assert(gba_pad_map_buttons(&map, pressed, 0, 0) == GBA_KEY_B);
    /* SDL's positional names work too: "y" is the north button. */
    assert(gba_pad_map_parse_line(&map, "select y", error, sizeof error));
    assert(gba_pad_map_buttons(&map, pressed, 0, 0) == (GBA_KEY_B | GBA_KEY_SELECT));
    assert(gba_pad_map_parse_line(&map, "select back", error, sizeof error));

    /* Rejected lines leave the map unchanged. */
    GbaPadMap before = map;
    assert(!gba_pad_map_parse_line(&map, "x south", error, sizeof error));
    assert(strstr(error, "unknown GBA key"));
    assert(!gba_pad_map_parse_line(&map, "a jump", error, sizeof error));
    assert(strstr(error, "unknown gamepad button"));
    assert(!gba_pad_map_parse_line(&map, "a south east west north", error, sizeof error));
    assert(!gba_pad_map_parse_line(&map, "deadzone 40000", error, sizeof error));
    assert(!memcmp(&before, &map, sizeof map));

    /* A map file applies all its lines or none. */
    const char *path = "gba_input_test_map.txt";
    FILE *f = fopen(path, "w");
    fputs("r leftshoulder\nl rightshoulder\n", f);
    fclose(f);
    assert(gba_pad_map_load(&map, path, error, sizeof error));
    memset(pressed, 0, sizeof pressed);
    pressed[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] = true;
    assert(gba_pad_map_buttons(&map, pressed, 0, 0) == GBA_KEY_R);
    f = fopen(path, "w");
    fputs("r south\nbad line\n", f);
    fclose(f);
    before = map;
    assert(!gba_pad_map_load(&map, path, error, sizeof error));
    assert(strstr(error, ":2:"));
    assert(!memcmp(&before, &map, sizeof map));
    remove(path);

    assert(gba_key_from_name("Select") == GBA_KEY_SELECT && gba_key_from_name("x") == 0);
    puts("gba_input: ok");
    return 0;
}
