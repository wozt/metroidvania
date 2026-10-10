/* SPDX-License-Identifier: GPL-3.0-only */
/* Early C11/SDL3 runtime experiment, NOT faithful MZM player physics. */
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MARKS 32768
#define MAX_FILE_BYTES 4000000L

typedef struct { int x, y, w, h, code; } Collision;
typedef struct { int width, height, resolution, room; char world[16], area[40];
    Collision collisions[MAX_MARKS]; size_t count; } Room;

static bool parse_room(const char *path, Room *room) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return false; }
    char line[256], extra; int version; bool ended = false;
    if (fseek(f, 0, SEEK_END) || ftell(f) < 0 || ftell(f) > MAX_FILE_BYTES ||
        fseek(f, 0, SEEK_SET)) goto failure;
    if (!fgets(line, sizeof line, f) || !strchr(line, '\n') ||
        sscanf(line, "MVROOM-PREVIEW\t%d\t%15[^\t]\t%39[^\t]\t%d\t%d\t%d\t%d %c",
            &version, room->world, room->area, &room->room, &room->width,
            &room->height, &room->resolution, &extra) != 7 || version != 1 ||
        (strcmp(room->world, "mzm") && strcmp(room->world, "aria")) ||
        room->room < 0 || room->room > 999 ||
        room->width < 16 || room->height < 16 ||
        room->width > 16384 || room->height > 16384 ||
        room->resolution != 16 || room->width % 16 || room->height % 16 ||
        (long long)room->width * room->height > 6144LL * 256) goto failure;
    while (fgets(line, sizeof line, f)) {
        char kind; int x, y, w, h, code;
        if (!strchr(line, '\n') && !feof(f)) goto failure;
        if (!strcmp(line, "END\n") || !strcmp(line, "END")) {
            ended = true; break;
        }
        if (sscanf(line, "%c\t%d\t%d\t%d\t%d\t%d %c",
                   &kind, &x, &y, &w, &h, &code, &extra) != 6 ||
            (kind != 'C' && kind != 'D' && kind != 'E' && kind != 'V') ||
            x < 0 || y < 0 || w <= 0 || h <= 0 ||
            x > room->width - w || y > room->height - h ||
            (kind == 'C' && (code < 1 || code > 7 || w != 16 || h != 16 ||
                 x % 16 || y % 16)) ||
            (kind != 'C' && code != 0)) goto failure;
        if (kind == 'C') {
            if (room->count >= MAX_MARKS) goto failure;
            room->collisions[room->count++] = (Collision){x,y,w,h,code};
        }
    }
    if (!ended || fgetc(f) != EOF) goto failure;
    fclose(f); return true;
failure:
    fclose(f); fprintf(stderr, "Invalid room preview: %s\n", path); return false;
}

/* PATCH_0141_STEEP_SLOPE_COLLISION
 * The pinned MZM clipdata converter marks the lower triangle solid:
 * 17 RIGHT_STEEP: local_y >= 15 - local_x;
 * 18 LEFT_STEEP:  local_y >= local_x.
 * Sample pixel centres at integer precision in the intersected 16px cell.
 * Exact GBA subpixel physics and Samus hitbox remain future work.
 */
static bool steep_slope_overlap(const Collision *c, float x, float y,
                                float w, float h) {
    float left = x > (float)c->x ? x : (float)c->x;
    float top = y > (float)c->y ? y : (float)c->y;
    float right = x+w < (float)(c->x+c->w) ? x+w : (float)(c->x+c->w);
    float bottom = y+h < (float)(c->y+c->h) ? y+h : (float)(c->y+c->h);
    if (left >= right || top >= bottom) return false;
    for (int ty = 0; ty < 16; ++ty) {
        float sy = (float)c->y + (float)ty + 0.5f;
        if (sy < top || sy >= bottom) continue;
        for (int tx = 0; tx < 16; ++tx) {
            float sx = (float)c->x + (float)tx + 0.5f;
            if (sx < left || sx >= right) continue;
            if (c->code == 17 ? ty >= 15-tx : ty >= tx) return true;
        }
    }
    return false;
}

static bool blocked(const Room *r, float x, float y, float w, float h) {
    if (x < 0 || y < 0 || x + w > r->width || y + h > r->height) return true;
    for (size_t i=0; i<r->count; ++i) {
        const Collision *c=&r->collisions[i];
        /* Only code=1 is treated as a solid rectangle. No native Clipdata guesses. */
        if (c->code == 1 && x < c->x+c->w && x+w > c->x &&
            y < c->y+c->h && y+h > c->y) return true;
        if ((c->code == 17 || c->code == 18) &&
            steep_slope_overlap(c, x, y, w, h)) return true;
    }
    return false;
}


/* PATCH_0138_NATIVE_CLIPDATA: opt-in original MZM source sidecar.
 * Native type 16 (CLIPDATA_SOLID) is a full solid tile according to the
 * pinned sClipdataCollisionTypes table. Other native codes are retained but
 * deliberately not converted into guessed full-block geometry. */
static bool parse_native_source(const char *path, Room *room,
                                size_t *native_count, size_t *solid_count) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return false; }
    char line[256], world[16] = {0}, area[40] = {0}, extra;
    int version, room_id, width, height;
    bool ended = false;
    size_t added = 0, solid = 0;
    if (fseek(f, 0, SEEK_END) || ftell(f) < 0 || ftell(f) > MAX_FILE_BYTES ||
        fseek(f, 0, SEEK_SET)) goto failure;
    if (!fgets(line, sizeof line, f) || !strchr(line, '\n') ||
        sscanf(line, "MVROOM-SOURCE\t%d\t%15[^\t]\t%39[^\t]\t%d\t%d\t%d %c",
               &version, world, area, &room_id, &width, &height, &extra) != 6 ||
        version != 1 || strcmp(world, room->world) || strcmp(area, room->area) ||
        room_id != room->room || width != room->width || height != room->height ||
        strcmp(world, "mzm")) goto failure;
    while (fgets(line, sizeof line, f)) {
        char kind;
        int x,y,w,h,code;
        if (!strchr(line,'\n') && !feof(f)) goto failure;
        if (!strcmp(line,"END\n") || !strcmp(line,"END")) { ended=true; break; }
        if (sscanf(line,"%c\t%d\t%d\t%d\t%d\t%d %c",
                   &kind,&x,&y,&w,&h,&code,&extra) != 6 ||
            (kind != 'N' && kind != 'A') ||
            x < 0 || y < 0 || w <= 0 || h <= 0 ||
            x > room->width-w || y > room->height-h ||
            (kind == 'N' && (code < 1 || code > 65535 || w != 16 || h != 16 ||
                              x % 16 || y % 16)) ||
            (kind == 'A' && (code < 1 || code > 7)) ||
            added >= MAX_MARKS) goto failure;
        ++added;
        if (kind == 'N' && (code == 16 || code == 17 || code == 18)) {
            if (room->count >= MAX_MARKS) goto failure;
            /* Preserve the native slope ID; 16 stays a full-solid rectangle. */
            room->collisions[room->count++] =
                (Collision){x,y,w,h,code == 16 ? 1 : code};
            if (code == 16) ++solid;
        }
    }
    if (!ended || fgetc(f) != EOF) goto failure;
    fclose(f);
    *native_count=added;
    *solid_count=solid;
    return true;
failure:
    fclose(f);
    fprintf(stderr,"Invalid or mismatched native source overlay: %s\n",path);
    return false;
}

/* PATCH_0139_CENTRAL_SPAWN
 * Iterate native/project geometry at four-pixel precision.
 * First choose the nearest standing position with free headroom; if the
 * current verified collision subset yields none, use a free central air
 * position. Never claim this is a native door/transition spawn point.
 */
static bool find_spawn(const Room *room, float w, float h,
                       float *out_x, float *out_y) {
    bool found = false;
    float best = 0.f, sx = 0.f, sy = 0.f;
    float cx = ((float)room->width - w) * 0.5f;
    float cy = ((float)room->height - h) * 0.5f;
    for (int pass = 0; pass < 2; ++pass) {
        found = false;
        for (int y = 0; y <= room->height - (int)h; y += 4) {
            for (int x = 0; x <= room->width - (int)w; x += 4) {
                float fx = (float)x, fy = (float)y;
                if (blocked(room, fx, fy, w, h)) continue;
                if (pass == 0 && !blocked(room, fx, fy + 1.f, w, h))
                    continue;
                /* Squared distance; prefer a point near the centre. */
                float dx = fx - cx, dy = fy - cy;
                float distance = dx * dx + dy * dy;
                if (!found || distance < best) {
                    found = true; best = distance; sx = fx; sy = fy;
                }
            }
        }
        if (found) {
            *out_x = sx; *out_y = sy;
            return true;
        }
    }
    return false;
}

static float clampf(float x, float low, float high) {
    return x < low ? low : x > high ? high : x;
}

/* PATCH_0137_PLATFORM_PHYSICS */
/* First platformer physics approximation, not original MZM constants.
 * Collision code 1 is explicitly project-authored solid geometry only. */
/* PATCH_0142_GROUNDED_SLOPE_TRAVERSAL
 * The original 0137 X/Y separated solver cannot climb an incline.
 * Allow a grounded avatar near a native slope to step up at most two
 * pixels and descend along its collision surface, without stepping up
 * arbitrary rectangular walls. Not yet GBA-exact Samus physics.
 */
static bool near_steep_slope(const Room *room, float x, float y,
                             float w, float h) {
    for (size_t i = 0; i < room->count; ++i) {
        const Collision *c = &room->collisions[i];
        if (c->code != 17 && c->code != 18) continue;
        if (x < (float)(c->x+c->w) && x+w > (float)c->x &&
            y+h >= (float)c->y - 2.f &&
            y+h <= (float)(c->y+c->h) + 2.f) return true;
    }
    return false;
}

static void move_grounded_x(const Room *room, float *x, float *y,
                            float amount, float w, float h) {
    while (amount > 0.0001f || amount < -0.0001f) {
        float step = clampf(amount, -1.f, 1.f);
        float nx = *x + step;
        bool has_slope = near_steep_slope(room, *x, *y, w, h) ||
                         near_steep_slope(room, nx, *y, w, h);
        float ny = *y;
        if (blocked(room, nx, ny, w, h)) {
            bool climbed = false;
            if (has_slope) {
                for (int rise = 1; rise <= 2; ++rise) {
                    float up = *y - (float)rise;
                    if (!blocked(room, *x, up, w, h) &&
                        !blocked(room, nx, up, w, h)) {
                        ny = up;
                        climbed = true;
                        break;
                    }
                }
            }
            if (!climbed) break;
        }
        /* A descending slope must remain the source of the supporting floor.
         * Avoid following drops off ordinary project geometry. */
        if (has_slope && !blocked(room, nx, ny+1.f, w, h)) {
            for (int fall = 1; fall <= 2; ++fall) {
                float down = ny + (float)fall;
                if (blocked(room, nx, down, w, h)) break;
                ny = down;
                if (blocked(room, nx, ny+1.f, w, h)) break;
            }
        }
        *x = nx;
        *y = ny;
        amount -= step;
    }
}

/* PATCH_0144_HORIZONTAL_VELOCITY
 * Provisional acceleration/braking model. The exact GBA parameters are
 * not verified yet. Kept independent from native slope/collision handling.
 */
static float update_horizontal_velocity(float vx, float input,
                                        float dt, float max_speed,
                                        float acceleration, float braking) {
    float target = input * max_speed;
    float change = (input == 0.f ? braking : acceleration) * dt;
    if (vx < target) {
        vx += change;
        if (vx > target) vx = target;
    } else if (vx > target) {
        vx -= change;
        if (vx < target) vx = target;
    }
    return vx;
}


/* PATCH_0145_MOVEMENT_STATES
 * Runtime presentation states only; these are NOT original MZM pose IDs.
 * Deterministic classification allows later pose/animation mapping.
 */
typedef enum {
    RUNTIME_IDLE,
    RUNTIME_RUNNING,
    RUNTIME_TURNING,
    RUNTIME_JUMPING,
    RUNTIME_FALLING
} RuntimeMovementState;

static RuntimeMovementState runtime_movement_state(bool grounded, float vx,
                                                    float vy, float input) {
    if (!grounded) return vy < -0.1f ? RUNTIME_JUMPING : RUNTIME_FALLING;
    if (input > 0.f && vx < -0.1f) return RUNTIME_TURNING;
    if (input < 0.f && vx > 0.1f) return RUNTIME_TURNING;
    return vx > 0.1f || vx < -0.1f ? RUNTIME_RUNNING : RUNTIME_IDLE;
}

static const char *runtime_movement_state_name(RuntimeMovementState state) {
    switch (state) {
        case RUNTIME_IDLE: return "idle";
        case RUNTIME_RUNNING: return "running";
        case RUNTIME_TURNING: return "turning";
        case RUNTIME_JUMPING: return "jumping";
        case RUNTIME_FALLING: return "falling";
    }
    return "unknown";
}

/* PATCH_0146_SAMUS_BMP_ANIMATIONS
 * Private BMPs exported by scripts.mzm_samus_sprite, never bundled.
 * Image placement is provisional; game-accurate OAM axes come later.
 */
typedef struct {
    SDL_Texture *frames[3][10];
    int widths[3][10];
    int heights[3][10];
    unsigned int durations[3][10];
} SamusFrames;

static void samus_frames_free(SamusFrames *frames) {
    for (int a = 0; a < 3; ++a)
        for (int i = 0; i < 10; ++i)
            if (frames->frames[a][i]) SDL_DestroyTexture(frames->frames[a][i]);
}

static bool samus_frames_load(SDL_Renderer *renderer, const char *dir,
                              SamusFrames *frames) {
    const char *names[] = {"idle", "run", "jump"};
    const int counts[] = {4, 10, 8};
    for (int a = 0; a < 3; ++a) {
        char duration_path[4096];
        int npath = snprintf(duration_path, sizeof duration_path,
                             "%s/%s_durations.txt", dir, names[a]);
        if (npath < 0 || (size_t)npath >= sizeof duration_path) return false;
        FILE *timings = fopen(duration_path, "rb");
        if (!timings) {
            fprintf(stderr, "Missing Samus duration sidecar: %s\n", duration_path);
            return false;
        }
        bool valid = true;
        for (int i=0; i<counts[a]; ++i) {
            unsigned int duration = 0;
            if (fscanf(timings, "%u", &duration) != 1 ||
                duration < 1 || duration > 255) { valid = false; break; }
            frames->durations[a][i] = duration;
        }
        char trailing;
        if (valid && fscanf(timings, " %c", &trailing) != EOF) valid = false;
        if (fclose(timings) != 0) valid = false;
        if (!valid) {
            fprintf(stderr, "Invalid Samus animation timings: %s\n", duration_path);
            return false;
        }
        for (int i = 0; i < counts[a]; ++i) {
            char path[4096];
            int n = snprintf(path, sizeof path, "%s/%s_%d.bmp", dir, names[a], i);
            if (n < 0 || (size_t)n >= sizeof path) return false;
            SDL_Surface *surface = SDL_LoadBMP(path);
            if (!surface) {
                fprintf(stderr, "Cannot load Samus frame %s: %s\n", path, SDL_GetError());
                return false;
            }
            frames->widths[a][i] = surface->w;
            frames->heights[a][i] = surface->h;
            if (surface->w < 1 || surface->h < 1 ||
                surface->w > 512 || surface->h > 512) {
                SDL_DestroySurface(surface);
                return false;
            }
            frames->frames[a][i] = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_DestroySurface(surface);
            if (!frames->frames[a][i]) return false;
            SDL_SetTextureScaleMode(frames->frames[a][i], SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(frames->frames[a][i], SDL_BLENDMODE_BLEND);
        }
    }
    return true;
}

static int samus_animation_group(RuntimeMovementState state) {
    if (state == RUNTIME_RUNNING || state == RUNTIME_TURNING) return 1;
    if (state == RUNTIME_JUMPING || state == RUNTIME_FALLING) return 2;
    return 0;
}

/* PATCH_0147_NATIVE_FRAME_TIMING
 * Native animation record durations are in 60 Hz frames. Keep each
 * sprite series on a separate playback clock; reset on state changes.
 * Visual timeline only; collision and motion physics remain unchanged.
 */
static int samus_timeline_frame(const unsigned int *durations, int count,
                                unsigned int elapsed_ticks) {
    if (count <= 0) return 0;
    unsigned int total = 0;
    for (int i=0; i<count; ++i) total += durations[i];
    if (!total) return 0;
    unsigned int phase = elapsed_ticks % total;
    for (int i=0; i<count; ++i) {
        if (phase < durations[i]) return i;
        phase -= durations[i];
    }
    return count-1;
}

static float move_axis(const Room *room, float start, float other,
                       float amount, float w, float h, bool vertical,
                       bool *hit) {
    float position = start;
    *hit = false;
    /* At most one world pixel of travel per collision probe to prevent
     * tunneling through 16-pixel solids even on long frames. */
    while (amount != 0.f) {
        float step = clampf(amount, -1.f, 1.f);
        float candidate = position + step;
        bool collision = vertical ? blocked(room, other, candidate, w, h) :
                                    blocked(room, candidate, other, w, h);
        if (collision) { *hit = true; break; }
        position = candidate;
        amount -= step;
        if (amount < 0.0001f && amount > -0.0001f) amount = 0.f;
    }
    return position;
}

#ifndef FUSION_RUNTIME_TEST
int main(int argc, char **argv) {
    const char *room_path=NULL, *background=NULL, *native_source=NULL;
    const char *samus_dir=NULL; bool check=false;
    for (int i=1;i<argc;i++) {
        if (!strcmp(argv[i],"--check")) { if(check) return 2; check=true; }
        else if (!strcmp(argv[i],"--background") && !background && i+1<argc) background=argv[++i];
        else if (!strcmp(argv[i],"--native-source") && !native_source && i+1<argc) native_source=argv[++i];
        else if (!strcmp(argv[i],"--samus-sprites") && !samus_dir && i+1<argc) samus_dir=argv[++i];
        else if (argv[i][0]=='-' || room_path) {
            fprintf(stderr,"Usage: %s [--check] [--background image.bmp] [--native-source source.tsv] [--samus-sprites directory] preview.tsv\n",argv[0]);
            return 2;
        } else room_path=argv[i];
    }
    if (!room_path || (check && (background || samus_dir))) {
        fprintf(stderr,"Usage: %s [--check] [--background image.bmp] [--native-source source.tsv] [--samus-sprites directory] preview.tsv\n",argv[0]);
        return 2;
    }
    Room *room=calloc(1,sizeof *room);
    if (!room) return 1;
    if (!parse_room(room_path,room)) { free(room); return 2; }
    printf("Runtime room %s/%s/%d %dx%d: %zu project collision entries\n",
           room->world,room->area,room->room,room->width,room->height,room->count);
    size_t native_records=0, native_solids=0;
    if (native_source && !parse_native_source(native_source,room,&native_records,&native_solids)) {
        free(room); return 2;
    }
    if (native_source)
        printf("Native source: %zu records, %zu verified full-solid cells (Clipdata 16)\n",
               native_records,native_solids);
    if (check) {
        float spawn_x = 0.f, spawn_y = 0.f;
        if (!find_spawn(room, 12.f, 16.f, &spawn_x, &spawn_y)) {
            fprintf(stderr, "No free runtime test spawn found\n");
            free(room); return 2;
        }
        printf("Validated test spawn: x=%.0f y=%.0f, ground=%s\n",
               spawn_x, spawn_y,
               blocked(room, spawn_x, spawn_y + 1.f, 12.f, 16.f) ? "yes" : "no");
        free(room); return 0;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());free(room);return 1; }
    SDL_Window *window=SDL_CreateWindow("Metroid Vania - experimental C11 runtime",
                                       960,640,SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer=window ? SDL_CreateRenderer(window,NULL) : NULL;
    SDL_Surface *surface=NULL; SDL_Texture *texture=NULL;
    int rc=1;
    SamusFrames samus_frames = {0};
    if (!renderer) { fprintf(stderr,"SDL renderer: %s\n",SDL_GetError()); goto cleanup; }
    if (background) {
        surface=SDL_LoadBMP(background);
        if (!surface || surface->w != room->width || surface->h != room->height) {
            fprintf(stderr,"Background BMP must match room dimensions: %dx%d\n",
                    room->width,room->height); goto cleanup;
        }
        texture=SDL_CreateTextureFromSurface(renderer,surface);
        if (!texture) { fprintf(stderr,"Texture: %s\n",SDL_GetError());goto cleanup; }
        SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST);
    }
    if (samus_dir && !samus_frames_load(renderer,samus_dir,&samus_frames)) {
        fprintf(stderr,"Samus sprite loading failed; aborting instead of showing incomplete art.\n");
        goto cleanup;
    }
    float px=16,py=16, pw=12,ph=16;
    /* Initial gameplay tuning, NOT confirmed Zero Mission physics. */
    const float run_speed=115.f, gravity=650.f, jump_speed=265.f;
    const float terminal_speed=400.f;
    /* Provisional input response, NOT extracted MZM parameters. */
    const float run_accel=520.f, run_braking=850.f;
    float vx=0.f, vy=0.f;
    bool grounded=false;
    RuntimeMovementState movement_state=RUNTIME_IDLE;
    int facing=1;
    Uint64 animation_start=SDL_GetTicks();
    /* Prefer a grounded, collision-free test spawn near the room centre.
     * This is NOT a verified original Samus entry position. */
    bool spawn = find_spawn(room, pw, ph, &px, &py);
    if (!spawn) { fprintf(stderr,"No free avatar spawn found\n"); goto cleanup; }
    printf("Selected safe test spawn: x=%.0f y=%.0f, ground=%s\n",
           px, py, blocked(room, px, py+1.f, pw, ph) ? "yes" : "no");
    printf("Controls: Left/Right or A/D = move; Space/Up/W = jump; Escape = exit. ");
    printf("Experimental platformer physics; only project code-1 solids block.\n");
    Uint64 previous=SDL_GetTicks(); bool running=true;
    float accumulator=0.f;
    const float fixed_step=1.f/120.f;
    bool jump_queued=false;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) running=false;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                (event.key.key == SDLK_SPACE || event.key.key == SDLK_UP ||
                 event.key.key == SDLK_W)) jump_queued=true;
        }
        Uint64 current=SDL_GetTicks();
        float dt=clampf((float)(current-previous)/1000.f,0.f,0.05f);
        previous=current;
        const bool *keys=SDL_GetKeyboardState(NULL);
        float dx=((keys[SDL_SCANCODE_RIGHT]||keys[SDL_SCANCODE_D]) ? 1.f:0.f)-
                 ((keys[SDL_SCANCODE_LEFT]||keys[SDL_SCANCODE_A]) ? 1.f:0.f);
        if (dx > 0.f) facing=1;
        else if (dx < 0.f) facing=-1;
        accumulator += dt;
        while (accumulator >= fixed_step) {
            bool hit=false;
            grounded=blocked(room,px,py+1.f,pw,ph);
            if (jump_queued && grounded) {
                vy=-jump_speed;
                grounded=false;
            }
            jump_queued=false;
            vx=update_horizontal_velocity(vx,dx,fixed_step,run_speed,
                                          run_accel,run_braking);
            if (grounded)
                move_grounded_x(room,&px,&py,vx*fixed_step,pw,ph);
            else
                px=move_axis(room,px,py,vx*fixed_step,pw,ph,false,&hit);
            vy=clampf(vy+gravity*fixed_step,-jump_speed,terminal_speed);
            py=move_axis(room,py,px,vy*fixed_step,pw,ph,true,&hit);
            if (hit) {
                if (vy>0.f) grounded=true;
                vy=0.f;
            }
            RuntimeMovementState next_state =
                runtime_movement_state(grounded, vx, vy, dx);
            if (next_state != movement_state) {
                movement_state = next_state;
                animation_start=SDL_GetTicks();
                char title[128];
                snprintf(title, sizeof title,
                         "Metroid Vania - test avatar [%s]",
                         runtime_movement_state_name(movement_state));
                SDL_SetWindowTitle(window, title);
            }
            accumulator-=fixed_step;
        }
        int ow=0,oh=0;
        if (!SDL_GetRenderOutputSize(renderer,&ow,&oh) || ow<=0 || oh<=0) continue;
        float scale=(float)ow/320.f;
        if ((float)oh/224.f < scale) scale=(float)oh/224.f;
        SDL_FRect viewport={(ow-320.f*scale)*0.5f,(oh-224.f*scale)*0.5f,
                            320.f*scale,224.f*scale};
        float cx=clampf(px+pw*.5f-160.f,0.f,(float)(room->width>320?room->width-320:0));
        float cy=clampf(py+ph*.5f-112.f,0.f,(float)(room->height>224?room->height-224:0));
        SDL_SetRenderDrawColor(renderer,12,14,24,255);SDL_RenderClear(renderer);
        if (texture) {
            SDL_FRect src={cx,cy,320.f,224.f};
            if (src.w>room->width) src.w=(float)room->width;
            if (src.h>room->height) src.h=(float)room->height;
            SDL_FRect dst={viewport.x,viewport.y,src.w*scale,src.h*scale};
            SDL_RenderTexture(renderer,texture,&src,&dst);
        }
        if (samus_dir) {
            int group=samus_animation_group(movement_state);
            const int counts[]={4,10,8};
            Uint64 elapsed_ms = SDL_GetTicks() - animation_start;
            /* 60 fps is a GBA playback clock; SDL render rate is independent. */
            unsigned int elapsed_frames = (unsigned int)(elapsed_ms * 60u / 1000u);
            int frame=samus_timeline_frame(samus_frames.durations[group],
                                           counts[group], elapsed_frames);
            SDL_Texture *sprite=samus_frames.frames[group][frame];
            float sw=(float)samus_frames.widths[group][frame];
            float sh=(float)samus_frames.heights[group][frame];
            /* Provisional bottom-centre anchor, separate from physics hitbox. */
            SDL_FRect dest={viewport.x+(px+pw*0.5f-cx-sw*0.5f)*scale,
                            viewport.y+(py+ph-cy-sh)*scale,sw*scale,sh*scale};
            SDL_RenderTextureRotated(renderer,sprite,NULL,&dest,0.0,NULL,
                                     facing<0 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        } else {
            SDL_SetRenderDrawColor(renderer,80,205,115,255);
            SDL_FRect avatar={viewport.x+(px-cx)*scale, viewport.y+(py-cy)*scale,
                              pw*scale,ph*scale};
            SDL_RenderFillRect(renderer,&avatar);
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(8);
    }
    rc=0;
cleanup:
    samus_frames_free(&samus_frames);
    if (texture) SDL_DestroyTexture(texture);
    if (surface) SDL_DestroySurface(surface);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();free(room);return rc;
}
#endif /* FUSION_RUNTIME_TEST */
