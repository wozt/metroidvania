/* SPDX-License-Identifier: GPL-3.0-only */
/* 0124/0125: local BMP-assisted diagnostic viewer, NOT gameplay. */
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_MARKS 32768
#define MAX_PREVIEW_BYTES 4000000L

typedef struct {
    char kind;
    int x, y, width, height, code;
} Mark;

typedef struct {
    int width, height, resolution, room;
    char world[16], area[40];
    size_t count;
    Mark *marks;
} Preview;

static bool load_preview(const char *path, Preview *preview)
{
    FILE *file = fopen(path, "rb");
    char line[256], extra;
    int version;
    bool finished = false;
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0 || ftell(file) < 0 ||
        ftell(file) > MAX_PREVIEW_BYTES || fseek(file, 0, SEEK_SET) != 0)
        goto fail;
    if (!fgets(line, sizeof(line), file) || !strchr(line, '\n') ||
        sscanf(line, "MVROOM-PREVIEW\t%d\t%15[^\t]\t%39[^\t]\t%d\t%d\t%d\t%d %c",
               &version, preview->world, preview->area, &preview->room, &preview->width,
               &preview->height, &preview->resolution, &extra) != 7 ||
        version != 1 || preview->room < 0 || preview->room > 999 ||
        (strcmp(preview->world, "mzm") && strcmp(preview->world, "aria")) ||
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

/* PATCH_0125_LOCAL_NATIVE_BG: strict opt-in/automatic LOCAL MZM BMP input.
 * Images NEVER enter room.json/preview.tsv; opaque BG1/BG2 are shown separately.
 * The extracted previews are partial and may not match a project-only room. */
#define MAX_BACKGROUND_BYTES (64LL * 1024LL * 1024LL)

typedef struct {
    const char *preview_path, *bg1_path, *bg2_path;
    bool check_only, auto_background;
} Arguments;

static void usage(const char *program)
{
    fprintf(stderr, "Usage: %s [--check] [--no-auto-bg] "
            "[--bg1 path.bmp] [--bg2 path.bmp] path/to/preview.tsv\n"
            "Only local MZM BG1/BG2 partial ROM previews, not packaged assets or gameplay.\n",
            program);
}

static bool parse_arguments(int argc, char **argv, Arguments *a)
{
    a->auto_background = true;
    for (int i = 1; i < argc; ++i) {
        const char *value = argv[i];
        if (!strcmp(value, "--check")) {
            if (a->check_only) return false;
            a->check_only = true;
        } else if (!strcmp(value, "--no-auto-bg")) {
            a->auto_background = false;
        } else if (!strcmp(value, "--bg1") || !strcmp(value, "--bg2")) {
            const char **slot = !strcmp(value, "--bg1") ? &a->bg1_path : &a->bg2_path;
            if (*slot || i + 1 >= argc || argv[i + 1][0] == '-') return false;
            *slot = argv[++i];
        } else if (value[0] == '-' || a->preview_path) {
            return false;
        } else {
            a->preview_path = value;
        }
    }
    return a->preview_path != NULL;
}

/* Names from the verified native catalog; no source-controlled or user-provided
 * path segment from the TSV is inserted before membership is checked. */
static bool local_mzm_background(const Preview *preview, int layer,
                                 char *output, size_t capacity)
{
    static const char *const names[] = {
        "Brinstar", "Kraid", "Norfair", "Ridley", "Tourian", "Crateria", "Chozodia"
    };
    if (strcmp(preview->world, "mzm")) return false;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (!strcmp(preview->area, names[i])) {
            char lower[32];
            size_t len = strlen(names[i]);
            if (len >= sizeof(lower)) return false;
            for (size_t j = 0; j < len; ++j) {
                char ch = names[i][j];
                lower[j] = ch >= 'A' && ch <= 'Z' ? (char)(ch - 'A' + 'a') : ch;
            }
            lower[len] = '\0';
            int written = snprintf(output, capacity,
                "assets/extracted/rooms/metroid/previews/%s_%03d_bg%d.bmp",
                lower, preview->room, layer);
            return written > 0 && (size_t)written < capacity;
        }
    }
    return false;
}

static SDL_Surface *load_matching_bmp(const char *path, const Preview *preview,
                                      bool explicit_path, bool *invalid)
{
    struct stat st;
    *invalid = false;
    if (stat(path, &st) != 0) {
        if (explicit_path) {
            fprintf(stderr, "Cannot access explicitly selected BMP: %s\n", path);
            *invalid = true;
        }
        return NULL;
    }
    if (!S_ISREG(st.st_mode) || st.st_size < 54 ||
        (long long)st.st_size > MAX_BACKGROUND_BYTES) {
        fprintf(stderr, "Rejected non-regular or oversized local BMP: %s\n", path);
        *invalid = explicit_path;
        return NULL;
    }
    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) {
        fprintf(stderr, "Cannot decode local BMP %s: %s\n", path, SDL_GetError());
        *invalid = explicit_path;
        return NULL;
    }
    if (surface->w != preview->width || surface->h != preview->height) {
        fprintf(stderr, "BMP dimensions do not match project room: %s (%dx%d, expected %dx%d). "
                "Use a project room with matching native dimensions.\n",
                path, surface->w, surface->h, preview->width, preview->height);
        SDL_DestroySurface(surface);
        *invalid = explicit_path;
        return NULL;
    }
    fprintf(stdout, "Local partial native BMP: %s (%dx%d)\n", path,
            surface->w, surface->h);
    return surface;
}

static void update_title(SDL_Window *window, int active, bool collisions, bool markers)
{
    char title[240];
    const char *layer = active == 1 ? "authentic partial BG1" :
                        active == 2 ? "authentic partial BG2" : "project geometry only";
    snprintf(title, sizeof(title),
        "Metroid Vania / %s / collision %s / markers %s (1/2/0, C, M, Esc)",
        layer, collisions ? "ON" : "OFF", markers ? "ON" : "OFF");
    SDL_SetWindowTitle(window, title);
}

static void set_mark_color(SDL_Renderer *renderer, const Mark *mark, bool background)
{
    unsigned char opacity = background && mark->kind == 'C' ? 130 : 255;
    switch (mark->kind == 'C' ? mark->code : mark->kind == 'D' ? 8 :
            mark->kind == 'E' ? 9 : 10) {
    case 1: SDL_SetRenderDrawColor(renderer, 207, 75, 75, opacity); break;
    case 2: SDL_SetRenderDrawColor(renderer, 81, 209, 126, opacity); break;
    case 3: SDL_SetRenderDrawColor(renderer, 233, 110, 32, opacity); break;
    case 4: case 5: SDL_SetRenderDrawColor(renderer, 221, 164, 80, opacity); break;
    case 6: SDL_SetRenderDrawColor(renderer, 71, 125, 220, opacity); break;
    case 7: SDL_SetRenderDrawColor(renderer, 118, 182, 217, opacity); break;
    case 8: SDL_SetRenderDrawColor(renderer, 209, 144, 233, opacity); break;
    case 9: SDL_SetRenderDrawColor(renderer, 238, 225, 95, opacity); break;
    default: SDL_SetRenderDrawColor(renderer, 181, 186, 206, opacity); break;
    }
}

int main(int argc, char **argv)
{
    Preview preview = {0};
    Arguments options = {0};
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Surface *surfaces[2] = {NULL, NULL};
    SDL_Texture *textures[2] = {NULL, NULL};
    bool enabled_collisions = true, enabled_markers = true;
    int active = 0, result = 1;
    bool invalid = false, hard_failure = false;
    char automatic_paths[2][256] = {{0}};
    const char *background_paths[2] = {NULL, NULL};
    bool explicit_background[2] = {false, false};

    if (!parse_arguments(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }
    if (!load_preview(options.preview_path, &preview)) {
        fprintf(stderr, "Invalid project preview: %s\n", options.preview_path);
        usage(argv[0]);
        return 2;
    }
    for (int i = 0; i < 2; ++i) {
        const char *chosen = i == 0 ? options.bg1_path : options.bg2_path;
        if (chosen) {
            background_paths[i] = chosen;
            explicit_background[i] = true;
        } else if (options.auto_background &&
                   local_mzm_background(&preview, i + 1,
                                        automatic_paths[i], sizeof(automatic_paths[i]))) {
            background_paths[i] = automatic_paths[i];
        }
        if (!background_paths[i]) continue;
        surfaces[i] = load_matching_bmp(background_paths[i], &preview,
                                        explicit_background[i], &invalid);
        hard_failure |= invalid;
    }
    if (hard_failure) {
        fprintf(stderr, "Explicit local BMP rejected; project geometry was not changed.\n");
        result = 2;
        goto done;
    }
    if (options.check_only) {
        printf("MVROOM 1: %dx%d, %zu markers, valid; local BG1=%s BG2=%s\n",
               preview.width, preview.height, preview.count,
               surfaces[0] ? "matching" : "absent",
               surfaces[1] ? "matching" : "absent");
        result = 0;
        goto done;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL3 init: %s\n", SDL_GetError());
        goto done;
    }
    window = SDL_CreateWindow("Metroid Vania / local project room preview (not gameplay)",
                              960, 720, SDL_WINDOW_RESIZABLE);
    renderer = window ? SDL_CreateRenderer(window, NULL) : NULL;
    if (!window || !renderer) {
        fprintf(stderr, "SDL3 renderer: %s\n", SDL_GetError());
        goto done;
    }
    for (int i = 0; i < 2; ++i) {
        if (!surfaces[i]) continue;
        textures[i] = SDL_CreateTextureFromSurface(renderer, surfaces[i]);
        if (!textures[i]) {
            fprintf(stderr, "SDL3 texture error for %s: %s\n",
                    background_paths[i], SDL_GetError());
            goto done;
        }
        SDL_SetTextureScaleMode(textures[i], SDL_SCALEMODE_NEAREST);
    }
    active = textures[0] ? 1 : textures[1] ? 2 : 0;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    update_title(window, active, enabled_collisions, enabled_markers);
    printf("Controls: 1=BG1, 2=BG2, 0=no background, "
           "C=collision, M=door/entity/event markers, Esc=close.\n");
    bool running = true;
    while (running) {
        SDL_Event event;
        int out_width, out_height;
        float scale, offset_x, offset_y;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                switch (event.key.key) {
                case SDLK_ESCAPE: running = false; break;
                case SDLK_0: active = 0; break;
                case SDLK_1: if (textures[0]) active = 1; break;
                case SDLK_2: if (textures[1]) active = 2; break;
                case SDLK_c: enabled_collisions = !enabled_collisions; break;
                case SDLK_m: enabled_markers = !enabled_markers; break;
                default: break;
                }
                update_title(window, active, enabled_collisions, enabled_markers);
            }
        }
        if (!running || !SDL_GetRenderOutputSize(renderer, &out_width, &out_height))
            break;
        scale = SDL_min((float)out_width / (float)preview.width,
                        (float)out_height / (float)preview.height);
        offset_x = (out_width - preview.width * scale) * 0.5f;
        offset_y = (out_height - preview.height * scale) * 0.5f;
        SDL_FRect image = {offset_x, offset_y,
                           preview.width * scale, preview.height * scale};
        SDL_SetRenderDrawColor(renderer, 17, 23, 35, 255);
        SDL_RenderClear(renderer);
        if (active && textures[active - 1])
            SDL_RenderTexture(renderer, textures[active - 1], NULL, &image);
        for (size_t i = 0; i < preview.count; ++i) {
            const Mark *mark = &preview.marks[i];
            if ((mark->kind == 'C' && !enabled_collisions) ||
                (mark->kind != 'C' && !enabled_markers)) continue;
            SDL_FRect rect = {offset_x + mark->x * scale,
                              offset_y + mark->y * scale,
                              mark->width * scale, mark->height * scale};
            set_mark_color(renderer, mark, active != 0);
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
    SDL_DestroyTexture(textures[0]);
    SDL_DestroyTexture(textures[1]);
    SDL_DestroySurface(surfaces[0]);
    SDL_DestroySurface(surfaces[1]);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(preview.marks);
    return result;
}
