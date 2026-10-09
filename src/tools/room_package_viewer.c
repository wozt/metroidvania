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

/* PATCH_0126B_NATIVE_SOURCE: real Clipdata/annotation source overlay is kept
 * separate from the stable project export, with its own strict room identity. */
static bool load_native_source(const char *path, Preview *preview, size_t *native_count)
{
    FILE *file = fopen(path, "rb");
    char line[256], world[16], area[40], extra;
    int version, room, width, height;
    bool finished = false;
    size_t start = preview->count;
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0 || ftell(file) < 0 ||
        ftell(file) > MAX_PREVIEW_BYTES || fseek(file, 0, SEEK_SET) != 0)
        goto fail;
    if (!fgets(line, sizeof(line), file) || !strchr(line, '\n') ||
        sscanf(line, "MVROOM-SOURCE\t%d\t%15[^\t]\t%39[^\t]\t%d\t%d\t%d %c",
               &version, world, area, &room, &width, &height, &extra) != 6 ||
        version != 1 || strcmp(world, preview->world) ||
        strcmp(area, preview->area) || room != preview->room ||
        width != preview->width || height != preview->height)
        goto fail;
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
            !(mark.kind == 'N' || mark.kind == 'A') ||
            mark.x < 0 || mark.y < 0 || mark.width <= 0 || mark.height <= 0 ||
            mark.x > preview->width - mark.width ||
            mark.y > preview->height - mark.height ||
            (mark.kind == 'N' &&
             (mark.code < 1 || mark.code > 65535 || mark.width != 16 ||
              mark.height != 16 || mark.x % 16 || mark.y % 16)) ||
            (mark.kind == 'A' && (mark.code < 1 || mark.code > 7)))
            goto fail;
        preview->marks[preview->count++] = mark;
    }
    if (!finished || fgetc(file) != EOF) goto fail;
    fclose(file);
    *native_count = preview->count - start;
    return true;
fail:
    fclose(file);
    preview->count = start;
    return false;
}

/* PATCH_0125_LOCAL_NATIVE_BG: strict opt-in/automatic LOCAL MZM BMP input.
 * Images NEVER enter room.json/preview.tsv; opaque BG1/BG2 are shown separately.
 * The extracted previews are partial and may not match a project-only room. */
#define MAX_BACKGROUND_BYTES (64LL * 1024LL * 1024LL)

typedef struct {
    const char *preview_path, *bg1_path, *bg2_path, *composite_path, *bg3_path;
    const char *native_source_path;
    bool check_only, auto_background;
} Arguments;

static void usage(const char *program)
{
    fprintf(stderr, "Usage: %s [--check] [--no-auto-bg] "
            "[--bg1 path.bmp] [--bg2 path.bmp] [--composite path.bmp] "
            "[--bg3 path.bmp] "
            "[--native-source path.tsv] "
            "path/to/preview.tsv\n"
            "Local MZM diagnostic layers only. BG3 is an independent tilemap, not a room composite.\n",
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
        } else if (!strcmp(value, "--native-source")) {
            if (a->native_source_path || i + 1 >= argc || argv[i + 1][0] == '-')
                return false;
            a->native_source_path = argv[++i];
        } else if (!strcmp(value, "--bg1") || !strcmp(value, "--bg2") ||
                   !strcmp(value, "--composite") || !strcmp(value, "--bg3")) {
            const char **slot = !strcmp(value, "--bg1") ? &a->bg1_path :
                                !strcmp(value, "--bg2") ? &a->bg2_path :
                                !strcmp(value, "--composite") ? &a->composite_path :
                                &a->bg3_path;
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
            int written = layer == 3 ?
                snprintf(output, capacity,
                    "assets/extracted/rooms/metroid/previews/%s_%03d_bg12_composite.bmp",
                    lower, preview->room) :
                snprintf(output, capacity,
                    "assets/extracted/rooms/metroid/previews/%s_%03d_bg%d.bmp",
                    lower, preview->room, layer == 4 ? 3 : layer);
            return written > 0 && (size_t)written < capacity;
        }
    }
    return false;
}

/* BG3 is a standalone 256x256 or 256x512 native TEXT tilemap. Its scroll
 * offsets relative to BG1/BG2 are unverified, so do not stretch it over a room. */
static SDL_Surface *load_matching_bmp(const char *path, const Preview *preview,
                                      bool explicit_path, bool bg3, bool *invalid)
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
    if (bg3 && (surface->w != 256 ||
                  (surface->h != 256 && surface->h != 512))) {
        fprintf(stderr, "BG3 diagnostic dimensions invalid: %s (%dx%d; expected 256x256 or 256x512).\n",
                path, surface->w, surface->h);
        SDL_DestroySurface(surface);
        *invalid = explicit_path;
        return NULL;
    }
    if (!bg3 && (surface->w != preview->width || surface->h != preview->height)) {
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
                        active == 2 ? "authentic partial BG2" :
                        active == 3 ? "partial BG1-over-BG2 (order not verified)" :
                        active == 4 ? "experimental BG3 (independent native tilemap)" :
                                      "project geometry only";
    snprintf(title, sizeof(title),
        "Metroid Vania / %s / collision %s / markers %s (1/2/3/4/0, C, M, Esc)",
        layer, collisions ? "ON" : "OFF", markers ? "ON" : "OFF");
    SDL_SetWindowTitle(window, title);
}

/* GTK room_legend_draw and draw_project_collision color values, rounded to 8 bit. */
static void mark_color(const Mark *mark, unsigned char *r,
                       unsigned char *g, unsigned char *b)
{
    if (mark->kind == 'N') {
        /* The native collision preview classifies raw Clipdata for DIAGNOSTICS.
         * It does not pretend every red raw Clipdata type means solid. */
        if (mark->code >= 6 && mark->code <= 11) {
            *r = 245; *g = 178; *b = 52;
        } else if (mark->code >= 33 && mark->code <= 37) {
            *r = 185; *g = 88; *b = 245;
        } else { *r = 235; *g = 55; *b = 75; }
    } else if (mark->kind == 'C') {
        switch (mark->code) {
        case 2: *r = 51; *g = 255; *b = 115; break;  /* Platform */
        case 4: case 5: *r = 255; *g = 158; *b = 26; break; /* Slopes */
        case 6: *r = 26; *g = 143; *b = 255; break;  /* Water */
        default: *r = 255; *g = 46; *b = 46; break; /* Wall/hazard */
        }
    } else {
        int role = mark->kind == 'A' ? mark->code :
                   mark->kind == 'D' ? 4 :
                   mark->kind == 'E' ? 2 :
                   mark->kind == 'V' ? 5 : 7;
        switch (role) {
        case 1: *r = 255; *g = 77; *b = 77; break; /* Enemy */
        case 2: *r = 255; *g = 209; *b = 46; break; /* Item or legacy entity */
        case 3: *r = 51; *g = 230; *b = 115; break; /* Object */
        case 4: *r = 184; *g = 89; *b = 255; break; /* Door */
        case 5: *r = 255; *g = 166; *b = 31; break; /* Event */
        case 6: *r = 51; *g = 204; *b = 255; break; /* Trigger */
        default: *r = 166; *g = 166; *b = 166; break; /* Other */
        }
    }
}

static void render_overlay_mark(SDL_Renderer *renderer, const Mark *mark,
                                const SDL_FRect *rect)
{
    unsigned char r, g, b;
    mark_color(mark, &r, &g, &b);
    if (mark->kind == 'C' && mark->code == 7) return; /* Air erases project override. */
    SDL_SetRenderDrawColor(renderer, r, g, b,
                           mark->kind == 'N' ? 148 : mark->kind == 'C' ? 117 : 71);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, r, g, b, 245);
    SDL_RenderRect(renderer, rect);
    if (mark->kind == 'C' && mark->code == 2) {
        /* Platform: extra top edge just like GTK. */
        SDL_RenderLine(renderer, rect->x, rect->y + 1.0f,
                       rect->x + rect->w, rect->y + 1.0f);
    } else if (mark->kind == 'C' && (mark->code == 4 || mark->code == 5)) {
        /* Slope orientation, matching the two GTK slope brush directions. */
        SDL_RenderLine(renderer, rect->x,
                       rect->y + (mark->code == 4 ? rect->h : 0),
                       rect->x + rect->w,
                       rect->y + (mark->code == 4 ? 0 : rect->h));
    }
}

int main(int argc, char **argv)
{
    Preview preview = {0};
    Arguments options = {0};
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Surface *surfaces[4] = {NULL, NULL, NULL, NULL};
    SDL_Texture *textures[4] = {NULL, NULL, NULL, NULL};
    bool enabled_collisions = true, enabled_markers = true;
    size_t native_count = 0;
    int active = 0, result = 1;
    bool invalid = false, hard_failure = false;
    char automatic_paths[4][256] = {{0}};
    const char *background_paths[4] = {NULL, NULL, NULL, NULL};
    bool explicit_background[4] = {false, false, false, false};

    if (!parse_arguments(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }
    if (!load_preview(options.preview_path, &preview)) {
        fprintf(stderr, "Invalid project preview: %s\n", options.preview_path);
        usage(argv[0]);
        return 2;
    }
    if (options.native_source_path &&
        !load_native_source(options.native_source_path, &preview, &native_count)) {
        fprintf(stderr, "Invalid native source overlay or mismatched room: %s\n",
                options.native_source_path);
        result = 2;
        goto done;
    }
    for (int i = 0; i < 4; ++i) {
        const char *chosen = i == 0 ? options.bg1_path :
                             i == 1 ? options.bg2_path :
                             i == 2 ? options.composite_path : options.bg3_path;
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
                                        explicit_background[i], i == 3, &invalid);
        hard_failure |= invalid;
    }
    if (hard_failure) {
        fprintf(stderr, "Explicit local BMP rejected; project geometry was not changed.\n");
        result = 2;
        goto done;
    }
    if (options.check_only) {
        printf("MVROOM 1: %dx%d, %zu markers, valid; local BG1=%s BG2=%s BG12=%s BG3=%s\n",
               preview.width, preview.height, preview.count,
               surfaces[0] ? "matching" : "absent",
               surfaces[1] ? "matching" : "absent",
               surfaces[2] ? "matching" : "absent",
               surfaces[3] ? "native-tilemap" : "absent");
        printf("Native source overlay records: %zu (separate private input)\n", native_count);
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
    for (int i = 0; i < 4; ++i) {
        if (!surfaces[i]) continue;
        textures[i] = SDL_CreateTextureFromSurface(renderer, surfaces[i]);
        if (!textures[i]) {
            fprintf(stderr, "SDL3 texture error for %s: %s\n",
                    background_paths[i], SDL_GetError());
            goto done;
        }
        SDL_SetTextureScaleMode(textures[i], SDL_SCALEMODE_NEAREST);
    }
    active = textures[2] ? 3 : textures[0] ? 1 : textures[1] ? 2 : 0;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    update_title(window, active, enabled_collisions, enabled_markers);
    printf("Controls: 1=BG1, 2=BG2, 3=partial composite, "
           "4=independent experimental BG3, 0=no background, "
           "C=collision, M=door/entity/event markers, Esc=close.\n");
    printf("BG3 mode is a separate tilemap: room overlays hidden (scroll/priority unverified).\n");
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
                case SDLK_3: if (textures[2]) active = 3; break;
                case SDLK_4: if (textures[3]) active = 4; break;
                case SDLK_C: enabled_collisions = !enabled_collisions; break;
                case SDLK_M: enabled_markers = !enabled_markers; break;
                default: break;
                }
                update_title(window, active, enabled_collisions, enabled_markers);
            }
        }
        if (!running || !SDL_GetRenderOutputSize(renderer, &out_width, &out_height))
            break;
        /* BG3 has its own 256x256/512 tilemap dimensions; it must not be
         * rescaled to the room extent or have room-space markers overlaid. */
        int display_width = active == 4 ? surfaces[3]->w : preview.width;
        int display_height = active == 4 ? surfaces[3]->h : preview.height;
        scale = SDL_min((float)out_width / (float)display_width,
                        (float)out_height / (float)display_height);
        offset_x = (out_width - display_width * scale) * 0.5f;
        offset_y = (out_height - display_height * scale) * 0.5f;
        SDL_FRect image = {offset_x, offset_y,
                           display_width * scale, display_height * scale};
        SDL_SetRenderDrawColor(renderer, 17, 23, 35, 255);
        SDL_RenderClear(renderer);
        if (active && textures[active - 1])
            SDL_RenderTexture(renderer, textures[active - 1], NULL, &image);
        for (size_t i = 0; active != 4 && i < preview.count; ++i) {
            const Mark *mark = &preview.marks[i];
            if (((mark->kind == 'C' || mark->kind == 'N') && !enabled_collisions) ||
                ((mark->kind != 'C' && mark->kind != 'N') && !enabled_markers))
                continue;
            SDL_FRect rect = {offset_x + mark->x * scale,
                              offset_y + mark->y * scale,
                              mark->width * scale, mark->height * scale};
            render_overlay_mark(renderer, mark, &rect);
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    result = 0;
done:
    SDL_DestroyTexture(textures[0]);
    SDL_DestroyTexture(textures[1]);
    SDL_DestroyTexture(textures[2]);
    SDL_DestroyTexture(textures[3]);
    SDL_DestroySurface(surfaces[0]);
    SDL_DestroySurface(surfaces[1]);
    SDL_DestroySurface(surfaces[2]);
    SDL_DestroySurface(surfaces[3]);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(preview.marks);
    return result;
}
