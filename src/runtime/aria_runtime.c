/* SPDX-License-Identifier: GPL-3.0-only */
/* Experimental Aria of Sorrow room runtime: Soma in one native room.
 *
 * Loads a room exported by scripts/aos_runtime_room.py (AOSROOM-NATIVE
 * collision bytes and background.bmp) and Soma's sprite library from
 * scripts/aos_soma_pipeline.py, then runs the ported player frame
 * aos_soma_update at 60 Hz. The library's animation_NNN keys are the
 * native animation descriptor indices, and their durations drive the
 * animation timing. Rooms, entities, souls and transitions are not
 * modelled. */
#include "aos_soma.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VIEW_W 240
#define VIEW_H 160
#define MAX_FILE_BYTES (8L * 1024 * 1024)
#define MAX_ANIMS 256
#define MAX_FRAMES 64
#define INDEX_SCHEMA "schema\tmetroidvania-sprite-index-v1"
#define DEFAULT_ROOMS "assets/extracted/aria/rooms/runtime"
#define DEFAULT_LIBRARY "assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv"

typedef struct {
    int width_screens, height_screens, width_cells, height_cells;
    uint8_t *cells;
} AriaRoom;

typedef struct {
    SDL_Texture *texture;
    int offset_x, offset_y;
    float w, h;
} AriaFrame;

typedef struct {
    AosAnimDef defs[MAX_ANIMS];
    uint8_t durations[MAX_ANIMS][MAX_FRAMES];
    AriaFrame frames[MAX_ANIMS][MAX_FRAMES];
    AosAnimSet set;
} AriaLibrary;

static bool load_room(const char *path, AriaRoom *room) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return false; }
    char line[4096], extra;
    int version, area, number, row = 0;
    bool ended = false;
    if (!fgets(line, sizeof line, f) ||
        sscanf(line, "AOSROOM-NATIVE\t%d\t%d\t%d\t%d\t%d\t%d\t%d %c", &version, &area, &number,
               &room->width_screens, &room->height_screens, &room->width_cells,
               &room->height_cells, &extra) != 7 || version != 1 ||
        room->width_cells < 1 || room->height_cells < 1 ||
        room->width_cells > 1024 || room->height_cells > 1024)
        goto failure;
    room->cells = calloc((size_t)room->width_cells * room->height_cells, 1);
    if (!room->cells) goto failure;
    while (fgets(line, sizeof line, f)) {
        if (!strcmp(line, "END\n") || !strcmp(line, "END")) { ended = true; break; }
        if (strncmp(line, "R\t", 2) || row >= room->height_cells) goto failure;
        for (int x = 0; x < room->width_cells; ++x) {
            unsigned value;
            if (sscanf(line + 2 + 2 * x, "%2x", &value) != 1) goto failure;
            room->cells[row * room->width_cells + x] = (uint8_t)value;
        }
        ++row;
    }
    if (!ended || row != room->height_cells) goto failure;
    fclose(f);
    return true;
failure:
    fclose(f);
    free(room->cells);
    room->cells = NULL;
    fprintf(stderr, "Invalid Aria runtime room: %s\n", path);
    return false;
}

/* Reads the sprite index; textures are created only with a renderer. */
static bool load_library(const char *path, AriaLibrary *lib, SDL_Renderer *renderer) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return false; }
    char line[1024];
    if (!fgets(line, sizeof line, f) || strncmp(line, INDEX_SCHEMA, strlen(INDEX_SCHEMA))) {
        fclose(f);
        fprintf(stderr, "Not a sprite index: %s\n", path);
        return false;
    }
    int max_id = -1;
    while (fgets(line, sizeof line, f)) {
        char frame_path[512];
        int id, frame, ticks, ox, oy;
        if (sscanf(line, "Soma/animation_%d\t%d\t%d\t%d\t%d\t%511s", &id, &frame, &ticks,
                   &ox, &oy, frame_path) != 6)
            continue;   /* knife and other non-body keys */
        if (id < 0 || id >= MAX_ANIMS || frame < 0 || frame >= MAX_FRAMES || ticks < 1 ||
            ticks > 255 || frame != lib->defs[id].count) {
            fclose(f);
            fprintf(stderr, "Invalid Soma library row: %s", line);
            return false;
        }
        lib->durations[id][frame] = (uint8_t)ticks;
        lib->defs[id].count = (uint16_t)(frame + 1);
        lib->defs[id].durations = lib->durations[id];
        AriaFrame *out = &lib->frames[id][frame];
        out->offset_x = ox;
        out->offset_y = oy;
        if (renderer) {
            SDL_Surface *surface = SDL_LoadBMP(frame_path);
            if (!surface) {
                fclose(f);
                fprintf(stderr, "%s: %s\n", frame_path, SDL_GetError());
                return false;
            }
            out->texture = SDL_CreateTextureFromSurface(renderer, surface);
            out->w = (float)surface->w;
            out->h = (float)surface->h;
            SDL_DestroySurface(surface);
            if (!out->texture) { fclose(f); return false; }
            SDL_SetTextureScaleMode(out->texture, SDL_SCALEMODE_NEAREST);
        }
        if (id > max_id) max_id = id;
    }
    fclose(f);
    if (max_id < 0) { fprintf(stderr, "No Soma animation in %s\n", path); return false; }
    lib->set.anims = lib->defs;
    lib->set.count = (uint16_t)(max_id + 1);
    return true;
}

static void free_library(AriaLibrary *lib) {
    for (int i = 0; i < MAX_ANIMS; ++i)
        for (int j = 0; j < MAX_FRAMES; ++j)
            if (lib->frames[i][j].texture) SDL_DestroyTexture(lib->frames[i][j].texture);
}

/* A floor pixel with 40 free pixels above it, searched from the room
 * centre outward; this is a test placement, not native spawn data. */
static bool find_spawn(const AosCollision *layer, int *out_x, int *out_y) {
    int width = layer->width_cells * 8, height = layer->height_cells * 8;
    for (int d = 0; d < width / 2; d += 4) {
        for (int side = -1; side <= 1; side += 2) {
            int x = width / 2 + side * d;
            int free_run = 0;
            for (int y = 0; y < height - 1; ++y) {
                if (aos_collision_cell(layer, x, y)) { free_run = 0; continue; }
                if (++free_run >= 40 && (aos_collision_cell(layer, x, y + 1) & 1)) {
                    *out_x = x;
                    *out_y = y;
                    return true;
                }
            }
        }
    }
    return false;
}

static uint16_t keyboard_buttons(void) {
    const bool *keys = SDL_GetKeyboardState(NULL);
    uint16_t held = 0;
    if (keys[SDL_SCANCODE_RIGHT]) held |= AOS_KEY_RIGHT;
    if (keys[SDL_SCANCODE_LEFT]) held |= AOS_KEY_LEFT;
    if (keys[SDL_SCANCODE_UP]) held |= AOS_KEY_UP;
    if (keys[SDL_SCANCODE_DOWN]) held |= AOS_KEY_DOWN;
    if (keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_SPACE]) held |= AOS_KEY_JUMP;
    return held;
}

static void usage(const char *name) {
    fprintf(stderr,
            "Usage: %s [--check] [--library index.tsv] [--spawn X Y]\n"
            "       [--capture out.bmp FRAMES BUTTONS] (--area A --room R | room-folder)\n",
            name);
}

int main(int argc, char **argv) {
    const char *folder = NULL, *library_path = DEFAULT_LIBRARY, *capture_path = NULL;
    long area = -1, room_number = -1, capture_frames = 0;
    unsigned long capture_buttons = 0;
    int spawn_x = -1, spawn_y = -1;
    bool check = false;
    for (int i = 1; i < argc; ++i) {
        char *end = NULL;
        if (!strcmp(argv[i], "--check")) check = true;
        else if (!strcmp(argv[i], "--library") && i + 1 < argc) library_path = argv[++i];
        else if (!strcmp(argv[i], "--area") && i + 1 < argc) area = strtol(argv[++i], &end, 10);
        else if (!strcmp(argv[i], "--room") && i + 1 < argc) room_number = strtol(argv[++i], &end, 10);
        else if (!strcmp(argv[i], "--spawn") && i + 2 < argc) {
            spawn_x = atoi(argv[++i]);
            spawn_y = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--capture") && i + 3 < argc) {
            capture_path = argv[++i];
            capture_frames = strtol(argv[++i], &end, 10);
            if (*end || capture_frames < 1 || capture_frames > 36000) { usage(argv[0]); return 2; }
            capture_buttons = strtoul(argv[++i], &end, 0);
            if (*end || capture_buttons > 0xFFFFu) { usage(argv[0]); return 2; }
            end = NULL;
        } else if (argv[i][0] != '-' && !folder) folder = argv[i];
        else { usage(argv[0]); return 2; }
        if (end && *end) { usage(argv[0]); return 2; }
    }
    char folder_buffer[512], room_path[600], background_path[600];
    if (!folder) {
        if (area < 0 || area > 99 || room_number < 0 || room_number > 999) { usage(argv[0]); return 2; }
        snprintf(folder_buffer, sizeof folder_buffer, "%s/area_%02ld_room_%03ld", DEFAULT_ROOMS,
                 area, room_number);
        folder = folder_buffer;
    }
    snprintf(room_path, sizeof room_path, "%s/room.tsv", folder);
    snprintf(background_path, sizeof background_path, "%s/background.bmp", folder);

    AriaRoom room = {0};
    if (!load_room(room_path, &room)) {
        fprintf(stderr, "Export the room first: python3 -m scripts.aos_runtime_room "
                        "--area <A> --room <R>\n");
        return 1;
    }
    AosCollision layer = {room.width_screens, room.height_screens, room.width_cells,
                          room.height_cells, room.cells};
    if (spawn_x < 0 && !find_spawn(&layer, &spawn_x, &spawn_y)) {
        fprintf(stderr, "No floor with headroom found in %s\n", room_path);
        free(room.cells);
        return 1;
    }
    static AriaLibrary library;
    if (check) {
        bool ok = load_library(library_path, &library, NULL);
        if (ok) printf("Aria room %s: %d x %d cells, Soma library %d animations, spawn %d,%d\n",
                       folder, room.width_cells, room.height_cells, library.set.count,
                       spawn_x, spawn_y);
        free(room.cells);
        return ok ? 0 : 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    int rc = 1;
    SDL_Window *window = SDL_CreateWindow("Metroid Vania - experimental Aria runtime",
                                          VIEW_W * 4, VIEW_H * 4, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, NULL) : NULL;
    SDL_Texture *background = NULL;
    if (!renderer) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); goto cleanup; }
    SDL_SetRenderLogicalPresentation(renderer, VIEW_W, VIEW_H,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_Surface *surface = SDL_LoadBMP(background_path);
    if (!surface) { fprintf(stderr, "%s: %s\n", background_path, SDL_GetError()); goto cleanup; }
    background = SDL_CreateTextureFromSurface(renderer, surface);
    float room_w = (float)surface->w, room_h = (float)surface->h;
    SDL_DestroySurface(surface);
    if (!background || !load_library(library_path, &library, renderer)) goto cleanup;
    SDL_SetTextureScaleMode(background, SDL_SCALEMODE_NEAREST);

    AosSoma soma = aos_soma_spawn(spawn_x << 16, spawn_y << 16, &library.set);
    uint16_t previous = 0;
    long step = 0;
    Uint64 last = SDL_GetTicksNS(), accumulator = 0;
    const Uint64 frame_ns = 1000000000ull / 60;
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) running = false;
        }
        Uint64 now = SDL_GetTicksNS();
        accumulator += capture_path ? frame_ns : now - last;
        last = now;
        if (accumulator > frame_ns * 5) accumulator = frame_ns * 5;
        while (accumulator >= frame_ns) {
            accumulator -= frame_ns;
            uint16_t held = capture_path ? (uint16_t)capture_buttons : keyboard_buttons();
            aos_soma_update(&soma, &layer, held, (uint16_t)(held & ~previous));
            previous = held;
            ++step;
        }

        int sx = soma.x >> 16, sy = soma.y >> 16;
        float cam_x = (float)sx - VIEW_W / 2, cam_y = (float)sy - VIEW_H / 2 - 16;
        if (cam_x > room_w - VIEW_W) cam_x = room_w - VIEW_W;
        if (cam_y > room_h - VIEW_H) cam_y = room_h - VIEW_H;
        if (cam_x < 0) cam_x = 0;
        if (cam_y < 0) cam_y = 0;
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_FRect src = {cam_x, cam_y, VIEW_W, VIEW_H}, dst = {0, 0, VIEW_W, VIEW_H};
        SDL_RenderTexture(renderer, background, &src, &dst);
        const AriaFrame *frame = NULL;
        if (soma.anim.frame < MAX_FRAMES)
            frame = &library.frames[soma.anim.id][soma.anim.frame];
        if (frame && frame->texture) {
            /* Library offsets face right; facing left mirrors around x. */
            float x = soma.facing_left ? (float)(sx - frame->offset_x) - frame->w
                                       : (float)(sx + frame->offset_x);
            SDL_FRect rect = {x - cam_x, (float)(sy + frame->offset_y) - cam_y, frame->w, frame->h};
            SDL_RenderTextureRotated(renderer, frame->texture, NULL, &rect, 0, NULL,
                                     soma.facing_left ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        }
        if (capture_path && step >= capture_frames) {
            SDL_Surface *shot = SDL_RenderReadPixels(renderer, NULL);
            bool saved = shot && SDL_SaveBMP(shot, capture_path);
            if (shot) SDL_DestroySurface(shot);
            if (!saved) { fprintf(stderr, "Capture failed: %s\n", SDL_GetError()); goto cleanup; }
            printf("Captured frame %ld: %s (x=%d y=%d state=%u anim=%u frame=%u flags=%08x)\n",
                   step, capture_path, sx, sy, soma.state, soma.anim.id, soma.anim.frame,
                   (unsigned)soma.flags);
            running = false;
        }
        SDL_RenderPresent(renderer);
        if (!capture_path) SDL_Delay(1);
    }
    rc = 0;
cleanup:
    free_library(&library);
    if (background) SDL_DestroyTexture(background);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    free(room.cells);
    return rc;
}
