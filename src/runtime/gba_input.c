/* SPDX-License-Identifier: GPL-3.0-only */
#include "gba_input.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const key_names[GBA_INPUT_KEYS] = {
    "a", "b", "select", "start", "right", "left", "up", "down", "r", "l",
};

uint16_t gba_key_from_name(const char *name) {
    for (int i = 0; i < GBA_INPUT_KEYS; ++i)
        if (!SDL_strcasecmp(name, key_names[i])) return (uint16_t)(1u << i);
    return 0;
}

/* Positional names first (unambiguous next to the GBA "a" / "b"), then
 * SDL's own names ("a" south, "b" east, "x" west, "y" north, "dpup"...). */
static SDL_GamepadButton button_from_name(const char *name) {
    static const struct { const char *name; SDL_GamepadButton button; } aliases[] = {
        {"south", SDL_GAMEPAD_BUTTON_SOUTH}, {"east", SDL_GAMEPAD_BUTTON_EAST},
        {"west", SDL_GAMEPAD_BUTTON_WEST}, {"north", SDL_GAMEPAD_BUTTON_NORTH},
    };
    for (size_t i = 0; i < sizeof aliases / sizeof aliases[0]; ++i)
        if (!SDL_strcasecmp(name, aliases[i].name)) return aliases[i].button;
    return SDL_GetGamepadButtonFromString(name);
}

static int key_index(uint16_t key) {
    for (int i = 0; i < GBA_INPUT_KEYS; ++i)
        if (key == (1u << i)) return i;
    return -1;
}

static void bind(GbaPadMap *map, uint16_t key, SDL_GamepadButton a, SDL_GamepadButton b) {
    int i = key_index(key);
    map->buttons[i][0] = (int8_t)a;
    map->buttons[i][1] = (int8_t)b;
}

void gba_pad_map_default(GbaPadMap *map) {
    for (int i = 0; i < GBA_INPUT_KEYS; ++i)
        for (int j = 0; j < GBA_PAD_BINDINGS; ++j) map->buttons[i][j] = SDL_GAMEPAD_BUTTON_INVALID;
    bind(map, GBA_KEY_A, SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_INVALID);
    bind(map, GBA_KEY_B, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST);
    bind(map, GBA_KEY_SELECT, SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_INVALID);
    bind(map, GBA_KEY_START, SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_INVALID);
    bind(map, GBA_KEY_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_INVALID);
    bind(map, GBA_KEY_LEFT, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_INVALID);
    bind(map, GBA_KEY_UP, SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_INVALID);
    bind(map, GBA_KEY_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_INVALID);
    bind(map, GBA_KEY_R, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_INVALID);
    bind(map, GBA_KEY_L, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_INVALID);
    map->dead_zone = 16383;     /* past half travel: |axis| >= 16384 */
}

static char *next_word(char **cursor) {
    char *p = *cursor;
    while (*p && isspace((unsigned char)*p)) ++p;
    if (!*p || *p == '#') return NULL;
    char *word = p;
    while (*p && !isspace((unsigned char)*p)) ++p;
    if (*p) *p++ = '\0';
    *cursor = p;
    return word;
}

bool gba_pad_map_parse_line(GbaPadMap *map, const char *line, char *error, size_t error_size) {
    char buffer[256];
    if (strlen(line) >= sizeof buffer) {
        snprintf(error, error_size, "line too long");
        return false;
    }
    strcpy(buffer, line);
    char *cursor = buffer;
    char *name = next_word(&cursor);
    if (!name) return true;
    if (!SDL_strcasecmp(name, "deadzone")) {
        char *value = next_word(&cursor), *end = NULL;
        long zone = value ? strtol(value, &end, 10) : -1;
        if (!value || *end || zone < 0 || zone > 32767 || next_word(&cursor)) {
            snprintf(error, error_size, "deadzone needs one value in 0..32767");
            return false;
        }
        map->dead_zone = (int16_t)zone;
        return true;
    }
    int index = key_index(gba_key_from_name(name));
    if (index < 0) {
        snprintf(error, error_size, "unknown GBA key: %s", name);
        return false;
    }
    int8_t buttons[GBA_PAD_BINDINGS];
    int count = 0;
    for (char *word; (word = next_word(&cursor));) {
        SDL_GamepadButton button = button_from_name(word);
        if (button == SDL_GAMEPAD_BUTTON_INVALID) {
            snprintf(error, error_size, "unknown gamepad button: %s", word);
            return false;
        }
        if (count == GBA_PAD_BINDINGS) {
            snprintf(error, error_size, "at most %d buttons per key", GBA_PAD_BINDINGS);
            return false;
        }
        buttons[count++] = (int8_t)button;
    }
    for (int j = 0; j < GBA_PAD_BINDINGS; ++j)
        map->buttons[index][j] = j < count ? buttons[j] : SDL_GAMEPAD_BUTTON_INVALID;
    return true;
}

bool gba_pad_map_load(GbaPadMap *map, const char *path, char *error, size_t error_size) {
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(error, error_size, "%s: cannot open", path);
        return false;
    }
    GbaPadMap updated = *map;
    char line[256], reason[160];
    int number = 0;
    bool ok = true;
    while (ok && fgets(line, sizeof line, f)) {
        ++number;
        line[strcspn(line, "\r\n")] = '\0';
        if (!gba_pad_map_parse_line(&updated, line, reason, sizeof reason)) {
            snprintf(error, error_size, "%s:%d: %s", path, number, reason);
            ok = false;
        }
    }
    fclose(f);
    if (ok) *map = updated;
    return ok;
}

uint16_t gba_pad_map_buttons(const GbaPadMap *map, const bool pressed[SDL_GAMEPAD_BUTTON_COUNT],
                             int16_t stick_x, int16_t stick_y) {
    uint16_t keys = 0;
    for (int i = 0; i < GBA_INPUT_KEYS; ++i)
        for (int j = 0; j < GBA_PAD_BINDINGS; ++j) {
            int button = map->buttons[i][j];
            if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT && pressed[button])
                keys |= (uint16_t)(1u << i);
        }
    if (stick_x > map->dead_zone) keys |= GBA_KEY_RIGHT;
    if (stick_x < -map->dead_zone) keys |= GBA_KEY_LEFT;
    if (stick_y > map->dead_zone) keys |= GBA_KEY_DOWN;
    if (stick_y < -map->dead_zone) keys |= GBA_KEY_UP;
    if ((keys & (GBA_KEY_LEFT | GBA_KEY_RIGHT)) == (GBA_KEY_LEFT | GBA_KEY_RIGHT))
        keys &= (uint16_t)~(GBA_KEY_LEFT | GBA_KEY_RIGHT);
    if ((keys & (GBA_KEY_UP | GBA_KEY_DOWN)) == (GBA_KEY_UP | GBA_KEY_DOWN))
        keys &= (uint16_t)~(GBA_KEY_UP | GBA_KEY_DOWN);
    return keys;
}

static void open_first(GbaInput *input) {
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (!ids) return;
    for (int i = 0; i < count && !input->pad; ++i) input->pad = SDL_OpenGamepad(ids[i]);
    SDL_free(ids);
}

void gba_input_init(GbaInput *input, const GbaPadMap *map) {
    input->pad = NULL;
    input->map = *map;
    open_first(input);
}

void gba_input_handle_event(GbaInput *input, const SDL_Event *event) {
    if (event->type == SDL_EVENT_GAMEPAD_ADDED && !input->pad) {
        input->pad = SDL_OpenGamepad(event->gdevice.which);
    } else if (event->type == SDL_EVENT_GAMEPAD_REMOVED && input->pad &&
               SDL_GetGamepadID(input->pad) == event->gdevice.which) {
        SDL_CloseGamepad(input->pad);
        input->pad = NULL;
        open_first(input);
    }
}

bool gba_input_button(const GbaInput *input, SDL_GamepadButton button) {
    return input->pad && SDL_GetGamepadButton(input->pad, button);
}

uint16_t gba_input_buttons(const GbaInput *input) {
    if (!input->pad) return 0;
    bool pressed[SDL_GAMEPAD_BUTTON_COUNT];
    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i)
        pressed[i] = SDL_GetGamepadButton(input->pad, (SDL_GamepadButton)i);
    return gba_pad_map_buttons(&input->map, pressed,
                               SDL_GetGamepadAxis(input->pad, SDL_GAMEPAD_AXIS_LEFTX),
                               SDL_GetGamepadAxis(input->pad, SDL_GAMEPAD_AXIS_LEFTY));
}

void gba_input_close(GbaInput *input) {
    if (input->pad) SDL_CloseGamepad(input->pad);
    input->pad = NULL;
}
