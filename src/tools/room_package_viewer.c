/* SPDX-License-Identifier: GPL-3.0-only */
/* 0124: read-only geometry smoke viewer. NOT a native gameplay engine. */
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MARKS 32768
#define MAX_PREVIEW_BYTES 4000000L

typedef struct {
    char kind;
    int x, y, width, height, code;
} Mark;

typedef struct {
    int width, height, resolution;
    size_t count;
    Mark *marks;
} Preview;

static bool load_preview(const char *path, Preview *preview)
{
    FILE *file = fopen(path, "rb");
    char line[256], world[16], area[40], extra;
    int version, room;
    bool finished = false;
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0 || ftell(file) < 0 ||
        ftell(file) > MAX_PREVIEW_BYTES || fseek(file, 0, SEEK_SET) != 0)
        goto fail;
    if (!fgets(line, sizeof(line), file) || !strchr(line, '\n') ||
        sscanf(line, "MVROOM-PREVIEW\t%d\t%15[^\t]\t%39[^\t]\t%d\t%d\t%d\t%d %c",
               &version, world, area, &room, &preview->width,
               &preview->height, &preview->resolution, &extra) != 7 ||
        version != 1 || room < 0 || room > 999 ||
        (strcmp(world, "mzm") && strcmp(world, "aria")) ||
        preview->width < 16 || preview->width > 16384 ||
        preview->height < 16 || preview->height > 16384 ||
        (preview->resolution != 8 && preview->resolution != 16) ||
        preview->width % 16 || preview->height % 16 ||
        (long long)preview->width * preview->height > 6144LL * 256)
        goto fail;
    preview->marks = calloc(MAX_MARKS, sizeof(*preview->marks));
    if (!preview->marks) goto fail;
    while (fgets(line, sizeof(line), file)) {
        Mark mark = {0};
        if (!strchr(line, '\n') && !feof(file)) goto fail;
        if (!strcmp(line, "END\n") || !strcmp(line, "END")) {
            finished = true;
            break;
        }
        if (preview->count >= MAX_MARKS ||
            sscanf(line, "%c\t%d\t%d\t%d\t%d\t%d %c",
                   &mark.kind, &mark.x, &mark.y, &mark.width,
                   &mark.height, &mark.code, &extra) != 6 ||
            !(mark.kind == 'C' || mark.kind == 'D' ||
              mark.kind == 'E' || mark.kind == 'V') ||
            mark.x < 0 || mark.y < 0 || mark.width <= 0 || mark.height <= 0 ||
            mark.x + mark.width > preview->width ||
            mark.y + mark.height > preview->height ||
            (mark.kind == 'C' && (mark.code < 1 || mark.code > 7 ||
             mark.width != preview->resolution ||
             mark.height != preview->resolution ||
             mark.x % preview->resolution || mark.y % preview->resolution)) ||
            (mark.kind != 'C' && mark.code != 0))
            goto fail;
        preview->marks[preview->count++] = mark;
    }
    if (!finished || fgetc(file) != EOF) goto fail;
    fclose(file);
    return true;
fail:
    fclose(file);
    free(preview->marks);
    preview->marks = NULL;
    preview->count = 0;
    return false;
}

static void set_mark_color(SDL_Renderer *renderer, const Mark *mark)
{
    switch (mark->kind == 'C' ? mark->code : mark->kind == 'D' ? 8 :
            mark->kind == 'E' ? 9 : 10) {
    case 1: SDL_SetRenderDrawColor(renderer, 207, 75, 75, 255); break;
    case 2: SDL_SetRenderDrawColor(renderer, 81, 209, 126, 255); break;
    case 3: SDL_SetRenderDrawColor(renderer, 233, 110, 32, 255); break;
    case 4: case 5: SDL_SetRenderDrawColor(renderer, 221, 164, 80, 255); break;
    case 6: SDL_SetRenderDrawColor(renderer, 71, 125, 220, 255); break;
    case 7: SDL_SetRenderDrawColor(renderer, 118, 182, 217, 255); break;
    case 8: SDL_SetRenderDrawColor(renderer, 209, 144, 233, 255); break;
    case 9: SDL_SetRenderDrawColor(renderer, 238, 225, 95, 255); break;
    default: SDL_SetRenderDrawColor(renderer, 181, 186, 206, 255); break;
    }
}

int main(int argc, char **argv)
{
    Preview preview = {0};
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    bool check_only = argc == 3 && strcmp(argv[1], "--check") == 0;
    int result = 1;
    if ((!check_only && argc != 2) ||
        !load_preview(argv[check_only ? 2 : 1], &preview)) {
        fprintf(stderr, "Usage: %s [--check] path/to/preview.tsv\n"
                "A saved project-room preview, not ROM data.\n", argv[0]);
        return 2;
    }
    if (check_only) {
        printf("MVROOM 1: %dx%d, %zu markers, valid\n",
               preview.width, preview.height, preview.count);
        result = 0;
        goto done;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL3 init: %s\n", SDL_GetError());
        goto done;
    }
    window = SDL_CreateWindow("Metroid Vania / project data preview (not gameplay)",
                              960, 720, SDL_WINDOW_RESIZABLE);
    renderer = window ? SDL_CreateRenderer(window, NULL) : NULL;
    if (!window || !renderer) {
        fprintf(stderr, "SDL3 renderer: %s\n", SDL_GetError());
        goto done;
    }
    bool running = true;
    while (running) {
        SDL_Event event;
        int out_width, out_height;
        float scale, offset_x, offset_y;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE))
                running = false;
        }
        if (!running || !SDL_GetRenderOutputSize(renderer, &out_width, &out_height))
            break;
        scale = SDL_min((float)out_width / (float)preview.width,
                        (float)out_height / (float)preview.height);
        offset_x = (out_width - preview.width * scale) * 0.5f;
        offset_y = (out_height - preview.height * scale) * 0.5f;
        SDL_SetRenderDrawColor(renderer, 17, 23, 35, 255);
        SDL_RenderClear(renderer);
        for (size_t i = 0; i < preview.count; ++i) {
            const Mark *mark = &preview.marks[i];
            SDL_FRect rect = {offset_x + mark->x * scale,
                              offset_y + mark->y * scale,
                              mark->width * scale, mark->height * scale};
            set_mark_color(renderer, mark);
            if (mark->kind == 'C' && mark->code != 7)
                SDL_RenderFillRect(renderer, &rect);
            else
                SDL_RenderRect(renderer, &rect);
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    result = 0;
done:
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(preview.marks);
    return result;
}
