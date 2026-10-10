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

/* PATCH_0185_MORPH_AND_WALL_JUMP
 * These helpers make the provisional gameplay rules independently testable.
 * Resizing preserves the avatar's feet and refuses to expand into collision.
 * Wall contact is sampled one pixel to either side of the current hitbox.
 * Neither the hitbox sizes nor the movement constants claim native fidelity. */
static bool runtime_resize_height(const Room *room, float x, float *y,
                                  float width, float *height,
                                  float requested_height) {
    if (requested_height <= 0.f) return false;
    float candidate_y = *y + *height - requested_height;
    if (blocked(room, x, candidate_y, width, requested_height)) return false;
    *y = candidate_y;
    *height = requested_height;
    return true;
}

static int runtime_wall_side(const Room *room, float x, float y,
                             float width, float height) {
    bool left = blocked(room, x - 1.f, y, width, height);
    bool right = blocked(room, x + 1.f, y, width, height);
    if (left == right) return 0;
    return left ? -1 : 1;
}

typedef struct {
    float hang_x, hang_y;
    float stand_x, stand_y;
    int side;
} RuntimeLedge;

/* PATCH_0186_LEDGE_GRAB
 * Find a solid-to-air corner beside the upper body and require both the
 * hanging volume and the final standing volume to be clear. This deliberately
 * uses the verified blocking geometry instead of assuming a 16-pixel grid. */
static bool runtime_find_ledge(const Room *room, float x, float y,
                               float width, float height, int side,
                               RuntimeLedge *ledge) {
    if ((side != -1 && side != 1) || !ledge) return false;
    float body_edge = x + (side > 0 ? width : 0.f);
    float edge = (float)(int)(body_edge + .5f);
    float probe_x = side > 0 ? edge : edge - 1.f;
    int first_y = (int)y - 4;
    int last_y = (int)y + 8;
    for (int top = first_y; top <= last_y; ++top) {
        if (top < 1 || top >= room->height) continue;
        if (blocked(room,probe_x,(float)top-1.f,1.f,1.f) ||
            !blocked(room,probe_x,(float)top,1.f,1.f)) continue;
        float hang_x = side > 0 ? edge-width : edge;
        float stand_x = side > 0 ? edge : edge-width;
        float stand_y = (float)top-height;
        if (stand_y < 0.f ||
            blocked(room,hang_x,(float)top,width,height) ||
            blocked(room,stand_x,stand_y,width,height) ||
            !blocked(room,stand_x,stand_y+1.f,width,height)) continue;
        *ledge=(RuntimeLedge){hang_x,(float)top,stand_x,stand_y,side};
        return true;
    }
    return false;
}

#define RUNTIME_ECHO_HISTORY 64
typedef struct {
    float x[RUNTIME_ECHO_HISTORY],y[RUNTIME_ECHO_HISTORY];
    unsigned int counter,position;
    int timer;
    bool wrapped,active;
} RuntimeEcho;

/* PATCH_0187_NATIVE_JUMP_ECHO
 * The pinned MZM source records 64 positions at 60 Hz. During a sufficiently
 * fast ascent it refreshes a six-tick timer, selects distance two, and draws
 * one previous body pose while cycling four lag positions. */
static void runtime_echo_step(RuntimeEcho *echo,float x,float y,
                              bool fast_ascent) {
    if (fast_ascent) {
        echo->active=true;
        echo->timer=6;
    } else if (echo->timer>0) {
        echo->timer--;
    } else {
        echo->active=false;
    }
    unsigned int index=echo->counter&(RUNTIME_ECHO_HISTORY-1u);
    echo->x[index]=x;
    echo->y[index]=y;
    echo->counter++;
    if(echo->counter>=RUNTIME_ECHO_HISTORY)echo->wrapped=true;
}

static bool runtime_echo_sample(RuntimeEcho *echo,unsigned int distance,
                                float *x,float *y) {
    int history=(int)echo->counter-(int)(distance*echo->position)-3;
    if(!echo->active || (!echo->wrapped && history<0))return false;
    unsigned int index=(unsigned int)history&(RUNTIME_ECHO_HISTORY-1u);
    *x=echo->x[index];
    *y=echo->y[index];
    echo->position=(echo->position+1u)&3u;
    return true;
}

typedef struct {
    int energy,max_energy;
    unsigned int invincibility_ticks;
    bool dead;
} RuntimeHealth;

/* PATCH_0187_DAMAGE_STATE
 * Damage amounts are project-side diagnostics until native entities exist.
 * The 48-tick invincibility window is taken from SamusChangeToHurtPose. */
static void runtime_health_reset(RuntimeHealth *health,int max_energy) {
    health->max_energy=max_energy;
    health->energy=max_energy;
    health->invincibility_ticks=0;
    health->dead=false;
}

static bool runtime_health_damage(RuntimeHealth *health,int amount) {
    if(amount<=0 || health->dead || health->invincibility_ticks>0)return false;
    health->energy-=amount;
    if(health->energy<0)health->energy=0;
    health->dead=health->energy==0;
    health->invincibility_ticks=48;
    return true;
}

static void runtime_health_step(RuntimeHealth *health) {
    if(health->invincibility_ticks>0)health->invincibility_ticks--;
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

static int samus_timeline_frame_once(const unsigned int *durations, int count,
                                     unsigned int elapsed_ticks) {
    if(count<=0)return 0;
    unsigned int total=0;
    for(int i=0;i<count;i++)total+=durations[i];
    if(!total || elapsed_ticks>=total)return count-1;
    for(int i=0;i<count;i++) {
        if(elapsed_ticks<durations[i])return i;
        elapsed_ticks-=durations[i];
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
/* PATCH_0163_PRIVATE_COMPOSITIONS: opt-in, experimental source frames.
 * This is visual-only; original platform collision is unchanged.
 * C = crouch preview, F = fire while crouched, E = aim diagonally while running.
 */
/* PATCH_0165_EXTENDED_COMPOSITIONS */
#define COMPOSED_COUNT 14
#define COMPOSED_BASE_COUNT 3
#define COMPOSED_EXTRA_COUNT 8
#define COMPOSED_MAX_FRAMES 10
#define SPECIAL_COUNT 6
#define SPECIAL_FRAMES 8
/* PATCH_0176_LIBRARY: 0175-derived private runtime index, loaded on demand. */
#define RUNTIME_LIBRARY_MAX 1024
#define RUNTIME_LIBRARY_FRAME_MAX 256
#define RUNTIME_ANIMATION_MAP_MAX 512
typedef struct {
    char name[160];
    int count;
    unsigned int ticks[RUNTIME_LIBRARY_FRAME_MAX];
    char paths[RUNTIME_LIBRARY_FRAME_MAX][320];
    SDL_Texture *textures[RUNTIME_LIBRARY_FRAME_MAX];
    int w[RUNTIME_LIBRARY_FRAME_MAX],h[RUNTIME_LIBRARY_FRAME_MAX];
} RuntimeLibraryEntry;
typedef struct {
    RuntimeLibraryEntry *entries;
    int count;
    char root[2048];
} RuntimeLibrary;
static void runtime_library_free(RuntimeLibrary *lib) {
    if(!lib->entries) return;
    for(int i=0;i<lib->count;i++)
        for(int j=0;j<lib->entries[i].count;j++)
            if(lib->entries[i].textures[j]) SDL_DestroyTexture(lib->entries[i].textures[j]);
    free(lib->entries);lib->entries=NULL;lib->count=0;
}
static bool runtime_library_open(RuntimeLibrary *lib,const char *index) {
    FILE *f=fopen(index,"rb");if(!f) return false;
    lib->entries=calloc(RUNTIME_LIBRARY_MAX,sizeof *lib->entries);
    if(!lib->entries){fclose(f);return false;}
    char line[1024],prev[160]="";
    bool ok=true;
    while(fgets(line,sizeof line,f)) {
        char name[160],path[320];unsigned int frame,tick;
        int consumed=0;
        if(sscanf(line,"%159[^\t]\t%u\t%u\t%319[^\t\r\n]%n",
                  name,&frame,&tick,path,&consumed)!=4 ||
           consumed<=0 || (line[consumed]!='\n' && line[consumed]!='\r') ||
           (line[consumed]=='\n' && line[consumed+1]!='\0') ||
           (line[consumed]=='\r' &&
             !(line[consumed+1]=='\n' && line[consumed+2]=='\0')) ||
           tick<1 || tick>255 || strchr(path,'/')==NULL || path[0]=='/' || strstr(path,"..")) {ok=false;break;}
        if(strcmp(name,prev)) {
            if(lib->count>=RUNTIME_LIBRARY_MAX){ok=false;break;}
            strncpy(prev,name,sizeof prev-1);prev[sizeof prev-1]='\0';
            RuntimeLibraryEntry *entry=&lib->entries[lib->count++];
            snprintf(entry->name,sizeof entry->name,"%s",name);
        }
        RuntimeLibraryEntry *e=&lib->entries[lib->count-1];
        if(frame!=(unsigned)e->count || frame>=RUNTIME_LIBRARY_FRAME_MAX){ok=false;break;}
        e->ticks[frame]=tick;
        snprintf(e->paths[frame],sizeof e->paths[frame],"%s",path);
        e->count++;
    }
    if(ferror(f))ok=false;
    fclose(f);
    if(!ok || lib->count==0){runtime_library_free(lib);return false;}
    return true;
}
static RuntimeLibraryEntry *runtime_library_find(RuntimeLibrary *lib,const char *name) {
    for(int i=0;i<lib->count;i++) if(!strcmp(lib->entries[i].name,name)) return &lib->entries[i];
    return NULL;
}
typedef struct {
    char action[32],suit[20],facing[8],aim[20];
    bool once;
    RuntimeLibraryEntry *entry;
} RuntimeAnimationMapRow;
typedef struct {
    RuntimeAnimationMapRow rows[RUNTIME_ANIMATION_MAP_MAX];
    int count;
} RuntimeAnimationMap;
static bool runtime_animation_token(const char *value) {
    if(!value[0])return false;
    for(const unsigned char *p=(const unsigned char *)value;*p;p++)
        if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||
             (*p>='0'&&*p<='9')||*p=='_'))return false;
    return true;
}
static bool runtime_animation_map_open(RuntimeAnimationMap *map,const char *path,
                                       RuntimeLibrary *library) {
    FILE *file=fopen(path,"rb");
    if(!file)return false;
    char line[512];bool ok=true;
    if(!fgets(line,sizeof line,file) ||
       strcmp(line,"schema\tmetroidvania-samus-animation-map-v1\n"))ok=false;
    if(ok && (!fgets(line,sizeof line,file) ||
       strcmp(line,"action\tsuit\tfacing\taim\tmode\tkey\n")))ok=false;
    while(ok && fgets(line,sizeof line,file)) {
        if(map->count>=RUNTIME_ANIMATION_MAP_MAX){ok=false;break;}
        char action[32],suit[20],facing[8],aim[20],mode[8],key[160];int consumed=0;
        if(sscanf(line,"%31[^\t]\t%19[^\t]\t%7[^\t]\t%19[^\t]\t%7[^\t]\t%159[^\t\r\n]%n",
                  action,suit,facing,aim,mode,key,&consumed)!=6 || consumed<=0 ||
           (line[consumed]!='\n' && line[consumed]!='\r') ||
           !runtime_animation_token(action) || !runtime_animation_token(suit) ||
           !runtime_animation_token(facing) || !runtime_animation_token(aim) ||
           (strcmp(mode,"loop") && strcmp(mode,"once"))) {ok=false;break;}
        RuntimeLibraryEntry *entry=runtime_library_find(library,key);
        if(!entry){ok=false;break;}
        for(int i=0;i<map->count;i++) {
            RuntimeAnimationMapRow *existing=&map->rows[i];
            if(!strcmp(existing->action,action)&&!strcmp(existing->suit,suit)&&
               !strcmp(existing->facing,facing)&&!strcmp(existing->aim,aim)) {
                ok=false;break;
            }
        }
        if(!ok)break;
        RuntimeAnimationMapRow *row=&map->rows[map->count++];
        snprintf(row->action,sizeof row->action,"%s",action);
        snprintf(row->suit,sizeof row->suit,"%s",suit);
        snprintf(row->facing,sizeof row->facing,"%s",facing);
        snprintf(row->aim,sizeof row->aim,"%s",aim);
        row->once=!strcmp(mode,"once");row->entry=entry;
    }
    if(ferror(file))ok=false;
    fclose(file);
    if(!ok || map->count==0){map->count=0;return false;}
    return true;
}
static RuntimeAnimationMapRow *runtime_animation_map_find(
        RuntimeAnimationMap *map,const char *action,const char *suit,
        const char *facing,const char *aim) {
    for(int pass=0;pass<3;pass++) {
        const char *wanted=pass==0?aim:pass==1?"forward":"none";
        if(pass>0 && !strcmp(wanted,aim))continue;
        for(int i=0;i<map->count;i++) {
            RuntimeAnimationMapRow *row=&map->rows[i];
            if(!strcmp(row->action,action)&&!strcmp(row->suit,suit)&&
               !strcmp(row->facing,facing)&&!strcmp(row->aim,wanted))return row;
        }
    }
    return NULL;
}
static unsigned int runtime_animation_total(const RuntimeAnimationMapRow *row) {
    unsigned int total=0;
    if(row && row->entry)
        for(int i=0;i<row->entry->count;i++)total+=row->entry->ticks[i];
    return total;
}
static float runtime_animation_seconds(RuntimeAnimationMap *map,
        const char *action,const char *suit,const char *facing,float fallback) {
    RuntimeAnimationMapRow *row=runtime_animation_map_find(
        map,action,suit,facing,"none");
    unsigned int ticks=runtime_animation_total(row);
    return ticks ? (float)ticks/60.f : fallback;
}
static int runtime_animation_priority(const char *action) {
    if(!strcmp(action,"death"))return 100;
    if(!strcmp(action,"hurt"))return 90;
    if(!strcmp(action,"morph_start")||!strcmp(action,"unmorph")||
       !strcmp(action,"ledge_pull_forward")||
       !strcmp(action,"ledge_pull_up"))return 85;
    if(!strcmp(action,"ledge_hang"))return 82;
    if(!strcmp(action,"spin_start")||!strcmp(action,"wall_jump"))return 80;
    if(!strcmp(action,"landing"))return 60;
    if(!strcmp(action,"turn")||!strcmp(action,"skid"))return 40;
    return 10;
}
static const char *runtime_requested_action(RuntimeMovementState state,bool spin,
        bool crouch,bool fire,bool skid,int special_kind,bool morphed,
        bool dead,bool hurt,
        bool morph_started,bool unmorph_started,bool hanging,
        bool ledge_pull_forward,bool ledge_pull_up,bool wall_jump_started,
        bool spin_started,bool landed) {
    if(dead)return "death";
    if(hurt)return morphed?"morph_ball":"hurt";
    if(morph_started)return "morph_start";
    if(unmorph_started)return "unmorph";
    if(ledge_pull_forward)return "ledge_pull_forward";
    if(ledge_pull_up)return "ledge_pull_up";
    if(hanging)return "ledge_hang";
    if(wall_jump_started)return "wall_jump";
    if(morphed)return "morph_ball";
    if(spin_started)return "spin_start";
    if(landed)return "landing";
    if(spin&&(state==RUNTIME_JUMPING||state==RUNTIME_FALLING))
        return special_kind==2?"screw_attack":special_kind==1?"space_jump":"spin";
    if(state==RUNTIME_TURNING)return "turn";
    if(skid)return "skid";
    if(state==RUNTIME_RUNNING)return "run";
    if(state==RUNTIME_JUMPING||state==RUNTIME_FALLING)return "midair";
    if(crouch)return fire?"crouch_fire":"crouch";
    return fire?"fire":"idle";
}
static bool runtime_library_texture(SDL_Renderer *r,RuntimeLibraryEntry *entry,int frame) {
    if(entry->textures[frame])return true;
    SDL_Surface *s=SDL_LoadBMP(entry->paths[frame]);
    if(!s)return false;
    if(s->w<1||s->h<1||s->w>512||s->h>512){SDL_DestroySurface(s);return false;}
    int w=s->w,h=s->h;
    SDL_Texture *t=SDL_CreateTextureFromSurface(r,s);
    SDL_DestroySurface(s);
    if(!t)return false;
    entry->textures[frame]=t;entry->w[frame]=w;entry->h[frame]=h;
    SDL_SetTextureScaleMode(t,SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);
    return true;
}
/* PATCH_0176_GAMEPAD: SDL3 standard gamepad, keyboard remains available. */
static SDL_Gamepad *runtime_pad_open(void) {
    int count=0;
    SDL_JoystickID *ids=SDL_GetGamepads(&count);
    SDL_Gamepad *pad=NULL;
    if (ids) {
        for(int i=0;i<count && !pad;i++) pad=SDL_OpenGamepad(ids[i]);
        SDL_free(ids);
    }
    return pad;
}
static bool runtime_pad_button(SDL_Gamepad *pad, SDL_GamepadButton button) {
    return pad && SDL_GetGamepadButton(pad,button);
}
static float runtime_pad_horizontal(SDL_Gamepad *pad) {
    if(!pad) return 0.f;
    float stick=(float)SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFTX)/32767.f;
    if(stick>-.22f && stick<.22f) stick=0.f;
    if(runtime_pad_button(pad,SDL_GAMEPAD_BUTTON_DPAD_LEFT)) return -1.f;
    if(runtime_pad_button(pad,SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) return 1.f;
    return stick;
}
static RuntimeLibraryEntry *runtime_library_select(RuntimeLibrary *lib,
        unsigned int armor,int facing,RuntimeMovementState state,
        bool spin,bool diag_up,bool diag_down,bool crouch,bool fire,int special_kind) {
    static const char *suits[]={"PowerSuit","PowerSuit","PowerSuit","FullSuit","Suitless"};
    const char *suit=suits[armor%5];
    const char *side=facing<0?"left":"right";
    char key[160];
    if(spin && (state==RUNTIME_JUMPING||state==RUNTIME_FALLING)) {
        const char *special=special_kind==2?"screwattacking":special_kind==1?"spacejumping":"spinning";
        snprintf(key,sizeof key,"PowerSuit/%s_%s",special,side);
        RuntimeLibraryEntry *e=runtime_library_find(lib,key);
        if(e)return e;
    }
    const char *pose=state==RUNTIME_RUNNING?"running":
        state==RUNTIME_JUMPING||state==RUNTIME_FALLING?"midair":
        crouch?(fire?"shootingandcrouching":"crouching"):
        fire?"shooting":"standing";
    const char *aim=diag_up?"diagonalup":diag_down?"diagonaldown":"forward";
    snprintf(key,sizeof key,"%s/%s_%s_%s",suit,pose,aim,side);
    RuntimeLibraryEntry *entry=runtime_library_find(lib,key);
    if(entry)return entry;
    snprintf(key,sizeof key,"%s/%s_%s_%s",suit,pose,"forward",side);
    entry=runtime_library_find(lib,key);
    if(entry)return entry;
    /* The body library uses native symbol suffixes, not the diagnostic aliases. */
    const char *native=state==RUNTIME_RUNNING?"running":
        state==RUNTIME_JUMPING||state==RUNTIME_FALLING?"midair":
        crouch?(fire?"shootingandcrouching":"crouching"):
        fire?"shooting":"standing";
    snprintf(key,sizeof key,"%s/%s_%s",suit,side,native);
    return runtime_library_find(lib,key);
}

typedef struct {
    SDL_Texture *textures[SPECIAL_COUNT][SPECIAL_FRAMES];
    unsigned int durations[SPECIAL_COUNT][SPECIAL_FRAMES];
    int widths[SPECIAL_COUNT][SPECIAL_FRAMES], heights[SPECIAL_COUNT][SPECIAL_FRAMES];
} SpecialAnimations;
static const char *special_names[SPECIAL_COUNT] = {
    "spinning_right", "spinning_left",
    "spacejumping_right", "spacejumping_left",
    "screwattacking_right", "screwattacking_left"
};
static void special_free(SpecialAnimations *a) {
    for (int g=0;g<SPECIAL_COUNT;g++)
        for (int i=0;i<SPECIAL_FRAMES;i++)
            if (a->textures[g][i]) SDL_DestroyTexture(a->textures[g][i]);
}
static bool special_load(SDL_Renderer *renderer,const char *dir,SpecialAnimations *a) {
    for (int g=0;g<SPECIAL_COUNT;g++) {
        char path[4096];
        int n=snprintf(path,sizeof path,"%s/composed/%s/durations.txt",dir,special_names[g]);
        if (n<0 || (size_t)n>=sizeof path) return false;
        FILE *f=fopen(path,"rb");
        if (!f) {fprintf(stderr,"Missing special timings: %s\\n",path);return false;}
        bool valid=true;
        for(int i=0;i<SPECIAL_FRAMES;i++) {
            unsigned int t=0;
            if(fscanf(f,"%u",&t)!=1 || t<1 || t>255) {valid=false;break;}
            a->durations[g][i]=t;
        }
        char extra;
        if(valid && fscanf(f," %c",&extra)!=EOF) valid=false;
        if(fclose(f)!=0 || !valid) return false;
        for(int i=0;i<SPECIAL_FRAMES;i++) {
            n=snprintf(path,sizeof path,"%s/composed/%s/%03d.bmp",dir,special_names[g],i);
            if(n<0 || (size_t)n>=sizeof path) return false;
            SDL_Surface *surface=SDL_LoadBMP(path);
            if(!surface) {fprintf(stderr,"Missing special BMP: %s\\n",path);return false;}
            int w=surface->w,h=surface->h;
            if(w<1 || w>512 || h<1 || h>512) {SDL_DestroySurface(surface);return false;}
            a->textures[g][i]=SDL_CreateTextureFromSurface(renderer,surface);
            SDL_DestroySurface(surface);
            if(!a->textures[g][i]) return false;
            a->widths[g][i]=w;a->heights[g][i]=h;
            SDL_SetTextureScaleMode(a->textures[g][i],SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(a->textures[g][i],SDL_BLENDMODE_BLEND);
        }
    }
    return true;
}
typedef struct {
    SDL_Texture *textures[COMPOSED_COUNT][COMPOSED_MAX_FRAMES];
    unsigned int durations[COMPOSED_COUNT][COMPOSED_MAX_FRAMES];
    int widths[COMPOSED_COUNT][COMPOSED_MAX_FRAMES];
    int heights[COMPOSED_COUNT][COMPOSED_MAX_FRAMES];
} ComposedAnimations;

static const int composed_counts[COMPOSED_COUNT] = {10, 5, 3, 10, 10, 3, 5, 3, 5, 5, 3, 3, 3, 10};
static const char *composed_names[COMPOSED_COUNT] = {
    "run_diagonal_up_right", "midair_forward_right", "shoot_crouch_right",
    "run_diagonal_down_right", "run_diagonal_up_left", "shoot_standing_right",
    "midair_diagonal_up_right", "shoot_crouch_diagonal_up_right",
    "midair_forward_left", "midair_diagonal_up_left", "shoot_standing_left",
    "shoot_crouch_left", "shoot_crouch_diagonal_up_left", "run_diagonal_down_left"
};

static void composed_free(ComposedAnimations *a) {
    for (int group=0; group<COMPOSED_COUNT; ++group)
        for (int i=0; i<composed_counts[group]; ++i)
            if (a->textures[group][i]) SDL_DestroyTexture(a->textures[group][i]);
}

static bool composed_load(SDL_Renderer *renderer, const char *dir,
                          ComposedAnimations *a, int first, int last) {
    for (int group=first; group<last; ++group) {
        char path[4096];
        int n=snprintf(path,sizeof path,"%s/composed/%s/durations.txt",
                       dir,composed_names[group]);
        if (n<0 || (size_t)n>=sizeof path) return false;
        FILE *f=fopen(path,"rb");
        if (!f) { fprintf(stderr,"Missing composition timings: %s\n",path);return false; }
        bool valid=true;
        for (int i=0; i<composed_counts[group]; ++i) {
            unsigned int value=0;
            if (fscanf(f,"%u",&value)!=1 || value<1 || value>255) {
                valid=false; break;
            }
            a->durations[group][i]=value;
        }
        char trailing;
        if (valid && fscanf(f," %c",&trailing)!=EOF) valid=false;
        if (fclose(f)!=0 || !valid) {
            fprintf(stderr,"Invalid composition timings: %s\n",path);
            return false;
        }
        for (int i=0; i<composed_counts[group]; ++i) {
            n=snprintf(path,sizeof path,"%s/composed/%s/%03d.bmp",
                       dir,composed_names[group],i);
            if (n<0 || (size_t)n>=sizeof path) return false;
            SDL_Surface *surface=SDL_LoadBMP(path);
            if (!surface) { fprintf(stderr,"Missing composed frame %s\n",path);return false; }
            int w=surface->w,h=surface->h;
            if (w<1 || w>512 || h<1 || h>512) {
                SDL_DestroySurface(surface);return false;
            }
            a->textures[group][i]=SDL_CreateTextureFromSurface(renderer,surface);
            SDL_DestroySurface(surface);
            if (!a->textures[group][i]) return false;
            a->widths[group][i]=w;
            a->heights[group][i]=h;
            SDL_SetTextureScaleMode(a->textures[group][i],SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(a->textures[group][i],SDL_BLENDMODE_BLEND);
        }
    }
    return true;
}

static int composed_select(RuntimeMovementState state, int facing,
                           bool diagonal_up, bool diagonal_down,
                           bool crouch, bool fire, bool extended, bool left_set,
                           bool spin_jump) {
    /* Spin jumps must not be silently replaced with straight MidAir poses. */
    if (spin_jump && (state == RUNTIME_JUMPING || state == RUNTIME_FALLING))
        return -1;
    if (facing < 0) {
        if (crouch && fire && state == RUNTIME_IDLE && left_set)
            return diagonal_up ? 12 : 11;
        if (fire && state == RUNTIME_IDLE && !crouch && left_set) return 10;
        if (state == RUNTIME_JUMPING || state == RUNTIME_FALLING)
            return left_set ? (diagonal_up ? 9 : (!diagonal_down ? 8 : -1)) : -1;
        if (state == RUNTIME_RUNNING) {
            if (diagonal_down && left_set) return 13;
            if (diagonal_up && extended) return 4;
        }
        return -1;
    }
    if (crouch && fire && state == RUNTIME_IDLE)
        return extended && diagonal_up ? 7 : 2;
    if (extended && fire && state == RUNTIME_IDLE && !crouch) return 5;
    if (state == RUNTIME_JUMPING || state == RUNTIME_FALLING)
        return extended && diagonal_up ? 6 : (!diagonal_up && !diagonal_down ? 1 : -1);
    if (state == RUNTIME_RUNNING) {
        if (extended && diagonal_down) return 3;
        if (diagonal_up) return 0;
    }
    return -1;
}

int main(int argc, char **argv) {
    const char *room_path=NULL, *background=NULL, *native_source=NULL;
    const char *samus_dir=NULL, *composed_dir=NULL, *extended_dir=NULL, *left_dir=NULL;
    const char *special_dir=NULL, *library_index=NULL, *animation_map_index=NULL;
    const char *room_alias=NULL, *samus_assets=NULL;
    bool check=false,animation_check=false;
    for (int i=1;i<argc;i++) {
        if (!strcmp(argv[i],"--check")) { if(check) return 2; check=true; }
        else if (!strcmp(argv[i],"--check-animations")) {
            if(animation_check)return 2;
            animation_check=true;
        }
        else if (!strcmp(argv[i],"--background") && !background && i+1<argc) background=argv[++i];
        else if (!strcmp(argv[i],"--native-source") && !native_source && i+1<argc) native_source=argv[++i];
        else if (!strcmp(argv[i],"--samus-sprites") && !samus_dir && i+1<argc) samus_dir=argv[++i];
        else if (!strcmp(argv[i],"--samus-composed") && !composed_dir && i+1<argc) composed_dir=argv[++i];
        else if (!strcmp(argv[i],"--samus-composed-extra") && !extended_dir && i+1<argc) extended_dir=argv[++i];
        else if (!strcmp(argv[i],"--samus-composed-left") && !left_dir && i+1<argc) left_dir=argv[++i];
        else if (!strcmp(argv[i],"--samus-special") && !special_dir && i+1<argc) special_dir=argv[++i];
        else if (!strcmp(argv[i],"--room") && !room_alias && i+1<argc) room_alias=argv[++i];
        else if (!strcmp(argv[i],"--samus-assets") && !samus_assets && i+1<argc) samus_assets=argv[++i];
        else if (!strcmp(argv[i],"--samus-library") && !library_index && i+1<argc) library_index=argv[++i];
        else if (!strcmp(argv[i],"--samus-map") && !animation_map_index && i+1<argc) animation_map_index=argv[++i];
        else if (argv[i][0]=='-' || room_path) {
            fprintf(stderr,"Usage: %s [--check] [--background image.bmp] [--native-source source.tsv] [--samus-sprites directory] preview.tsv\n",argv[0]);
            return 2;
        } else room_path=argv[i];
    }
    /* PATCH_0181_ROOM_AND_ASSETS: room shorthand keeps native collision mandatory. */
    char bundle_index[4096],bundle_map[4096];
    if (room_alias) {
        if (strcmp(room_alias,"brinstar_033") || room_path || background || native_source) {
            fprintf(stderr,"Unknown room alias or conflicting room paths: %s\n",room_alias);
            return 2;
        }
        room_path="assets/extracted/native_demo_0125/assets/extracted/exports/mzm/brinstar_033_b10ffe9d3dfbc2a12bb545fe1a6cd0a70f3060fd3a1d11d537148a954a3890ad/preview.tsv";
        background="assets/extracted/rooms/metroid/previews/brinstar_033_bg12_composite.bmp";
        native_source="assets/extracted/native_source_overlays/mzm/brinstar_033.tsv";
    }
    if (samus_assets && library_index) {
        fprintf(stderr,"Use either --samus-assets or --samus-library\n");
        return 2;
    }
    if (samus_assets) {
        int n=snprintf(bundle_index,sizeof bundle_index,"%s/runtime_index.tsv",samus_assets);
        if (n<0 || (size_t)n>=sizeof bundle_index) return 2;
        library_index=bundle_index;
        if(!animation_map_index) {
            n=snprintf(bundle_map,sizeof bundle_map,"%s/animation_map.tsv",samus_assets);
            if(n<0 || (size_t)n>=sizeof bundle_map)return 2;
            animation_map_index=bundle_map;
        }
    } else if (room_alias && !library_index) {
        library_index="assets/extracted/metroid/sprites/samus/runtime/runtime_index.tsv";
        animation_map_index="assets/extracted/metroid/sprites/samus/runtime/animation_map.tsv";
    }
    if(animation_map_index && !library_index) {
        fprintf(stderr,"--samus-map requires --samus-library or --samus-assets\n");
        return 2;
    }
    if(animation_check) {
        if(check || room_path || background || native_source || samus_dir ||
           composed_dir || extended_dir || left_dir || special_dir ||
           !library_index || !animation_map_index) {
            fprintf(stderr,"Animation check requires only --samus-assets, or a library and map pair\n");
            return 2;
        }
        RuntimeLibrary checked_library={0};RuntimeAnimationMap checked_map={0};
        bool valid=runtime_library_open(&checked_library,library_index) &&
            runtime_animation_map_open(&checked_map,animation_map_index,&checked_library);
        if(valid)printf("Validated Samus animation registry: %d sequences, %d semantic bindings\n",
                        checked_library.count,checked_map.count);
        else fprintf(stderr,"Samus animation registry validation failed\n");
        runtime_library_free(&checked_library);
        return valid?0:2;
    }
    if (!room_path || (check && (background || samus_dir || composed_dir || extended_dir || left_dir || special_dir || library_index || animation_map_index))) {
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
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) { fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());free(room);return 1; }
    SDL_Window *window=SDL_CreateWindow("Metroid Vania - experimental C11 runtime",
                                       960,640,SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer=window ? SDL_CreateRenderer(window,NULL) : NULL;
    SDL_Surface *surface=NULL; SDL_Texture *texture=NULL;
    int rc=1;
    SamusFrames samus_frames = {0};
    ComposedAnimations composed_frames = {0};
    SpecialAnimations special_frames = {0};
    RuntimeLibrary library = {0};
    RuntimeAnimationMap animation_map = {0};
    SDL_Gamepad *gamepad = NULL;
    bool pad_jump_prev=false, pad_morph_prev=false;
    bool pad_armor_prev=false, pad_special_prev=false;
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
    if (composed_dir && !composed_load(renderer,composed_dir,&composed_frames,0,COMPOSED_BASE_COUNT)) {
        fprintf(stderr,"Composed Samus sprite loading failed.\n");
        goto cleanup;
    }
    if (extended_dir && !composed_load(renderer,extended_dir,&composed_frames,
                                       COMPOSED_BASE_COUNT,COMPOSED_EXTRA_COUNT)) {
        fprintf(stderr,"Extended composed Samus sprite loading failed.\n");
        goto cleanup;
    }
    if (left_dir && !composed_load(renderer,left_dir,&composed_frames,
                                   COMPOSED_EXTRA_COUNT,COMPOSED_COUNT)) {
        fprintf(stderr,"Left-facing composed Samus sprite loading failed.\n");
        goto cleanup;
    }
    if (special_dir && !special_load(renderer,special_dir,&special_frames)) {
        fprintf(stderr,"Special animation load failed.\\n");goto cleanup;
    }
    if(library_index && !runtime_library_open(&library,library_index)){fprintf(stderr,"Runtime animation index failed to load.\n");goto cleanup;}
    if(animation_map_index && !runtime_animation_map_open(
            &animation_map,animation_map_index,&library)) {
        fprintf(stderr,"Runtime animation map failed to load.\n");goto cleanup;
    }
    if(animation_map_index)
        printf("Samus animation registry: %d native sequences, %d semantic bindings\n",
               library.count,animation_map.count);
    gamepad=runtime_pad_open();
    const float standing_width=12.f,standing_height=16.f,morph_height=10.f;
    float px=16,py=16,pw=standing_width,ph=standing_height;
    /* Initial gameplay tuning, NOT confirmed Zero Mission physics. */
    const float run_speed=115.f, gravity=650.f, jump_speed=265.f;
    const float terminal_speed=400.f;
    /* Provisional input response, NOT extracted MZM parameters. */
    const float run_accel=520.f, run_braking=850.f;
    float vx=0.f, vy=0.f;
    bool grounded=false;
    RuntimeMovementState movement_state=RUNTIME_IDLE;
    int facing=1;
    bool spin_jump=false;
    bool morphed=false;
    float wall_jump_lock=0.f;
    bool hanging=false,ledge_input_armed=false;
    float ledge_regrab_lock=0.f,ledge_pull_lock=0.f;
    RuntimeLedge active_ledge={0};
    RuntimeEcho echo={0};
    unsigned int echo_substep=0,echo_render_tick=~0u;
    bool echo_visible=false;
    float echo_x=0.f,echo_y=0.f;
    RuntimeHealth health={0};
    runtime_health_reset(&health,99);
    float hurt_lock=0.f;
    int special_kind=0; /* 0=spin; 1=space, 2=screw (visual preview only). */
    unsigned int armor_index=0;
    bool animation_browser=false;
    int browser_index=0;
    RuntimeAnimationMapRow *active_animation=NULL;
    bool spin_started=false,landed=false,morph_started=false;
    bool unmorph_started=false,wall_jump_started=false;
    bool ledge_pull_forward_started=false,ledge_pull_up_started=false;
    static const char *armor_names[] = {
        "Power Suit", "Varia Suit", "Gravity Suit", "Full Suit", "Suitless"
    };
    static const char *suit_names[] = {
        "PowerSuit","VariaSuit","GravitySuit","FullSuit","Suitless"
    };
    Uint64 animation_start=SDL_GetTicks();
    /* Prefer a grounded, collision-free test spawn near the room centre.
     * This is NOT a verified original Samus entry position. */
    bool spawn = find_spawn(room, pw, ph, &px, &py);
    if (!spawn) { fprintf(stderr,"No free avatar spawn found\n"); goto cleanup; }
    printf("Selected safe test spawn: x=%.0f y=%.0f, ground=%s\n",
           px, py, blocked(room, px, py+1.f, pw, ph) ? "yes" : "no");
    SDL_SetWindowTitle(window,"Metroid Vania [Power Suit] [idle] [Energy 99/99]");
    printf("Controls: Left/Right or A/D = move; Space/Up/W = jump; X = morph; Escape = exit. ");
    printf("Experimental platformer physics; only project code-1 solids block.\n");
    printf("Ledges: hold toward while falling; release, then jump/toward to climb; C/away drops.\n");
    printf("Damage diagnostic: H = 20 damage; Enter = restart after death.\n");
    printf("Animation controls: E/Q aim, C crouch, F fire, R suit, T spin type, F6 catalogue.\n");
    if (composed_dir) printf("Compositions: E=diagonal run, C+F=crouch shooting, jump=straight midair (right facing).\n");
    Uint64 previous=SDL_GetTicks(); bool running=true;
    float accumulator=0.f;
    const float fixed_step=1.f/120.f;
    bool jump_queued=false,morph_queued=false;
    bool damage_queued=false,restart_queued=false;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            /* Diagnostic animation browser: gameplay state and collisions unchanged. */
            if (library_index && library.count > 0) {
                bool toggle=(event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                             event.key.key==SDLK_F6) ||
                            (event.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                             event.gbutton.button==SDL_GAMEPAD_BUTTON_DPAD_UP);
                bool forward=(event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                              event.key.key==SDLK_PAGEDOWN) ||
                             (event.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                              event.gbutton.button==SDL_GAMEPAD_BUTTON_RIGHT_STICK);
                bool backward=(event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                               event.key.key==SDLK_PAGEUP) ||
                              (event.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                               event.gbutton.button==SDL_GAMEPAD_BUTTON_LEFT_STICK);
                if(toggle) {
                    animation_browser=!animation_browser;
                    active_animation=NULL;
                    animation_start=SDL_GetTicks();
                }
                if(animation_browser && (forward || backward)) {
                    browser_index=(browser_index+(forward?1:library.count-1))%library.count;
                    animation_start=SDL_GetTicks();
                }
                if(toggle || (animation_browser && (forward || backward))) {
                    if(animation_browser) {
                        char title[256];
                        snprintf(title,sizeof title,"Samus animation [%d/%d] %s",
                                 browser_index+1,library.count,library.entries[browser_index].name);
                        SDL_SetWindowTitle(window,title);
                        fprintf(stderr,"Animation browser: %s (%d/%d)\n",
                                library.entries[browser_index].name,browser_index+1,library.count);
                    } else {
                        SDL_SetWindowTitle(window,"Metroid Vania - runtime");
                        fprintf(stderr,"Animation browser disabled\n");
                    }
                }
            }
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) running=false;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                (event.key.key == SDLK_SPACE || event.key.key == SDLK_UP ||
                 event.key.key == SDLK_W)) jump_queued=true;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                event.key.key == SDLK_X) morph_queued=true;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                event.key.key == SDLK_H) damage_queued=true;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                event.key.key == SDLK_RETURN) restart_queued=true;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                event.key.key == SDLK_T) {
                special_kind=(special_kind+1)%3;
                fprintf(stderr,"Spin preview mode: %s\\n",
                        special_kind==0 ? "Spin" : special_kind==1 ? "Space" : "Screw");
            }
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                event.key.key == SDLK_R) {
                armor_index=(armor_index+1u)%5u;
                static const char *armor_sources[]={
                    "Power Suit","Power Suit palette fallback",
                    "Power Suit palette fallback","Full Suit","Suitless"
                };
                fprintf(stderr,"Selected armor: %s (animation source: %s)\n",
                        armor_names[armor_index],armor_sources[armor_index]);
            }
        }
        if(gamepad && !SDL_GamepadConnected(gamepad)){SDL_CloseGamepad(gamepad);gamepad=NULL;}
        if(!gamepad)gamepad=runtime_pad_open();
        bool pad_jump=runtime_pad_button(gamepad,SDL_GAMEPAD_BUTTON_SOUTH);
        if(pad_jump && !pad_jump_prev)jump_queued=true;
        pad_jump_prev=pad_jump;
        bool pad_morph=runtime_pad_button(gamepad,SDL_GAMEPAD_BUTTON_WEST);
        if(pad_morph && !pad_morph_prev)morph_queued=true;
        pad_morph_prev=pad_morph;
        bool pad_armor=runtime_pad_button(gamepad,SDL_GAMEPAD_BUTTON_BACK);
        if(pad_armor && !pad_armor_prev) armor_index=(armor_index+1u)%5u;
        pad_armor_prev=pad_armor;
        bool pad_special=runtime_pad_button(gamepad,SDL_GAMEPAD_BUTTON_NORTH);
        if(pad_special && !pad_special_prev) special_kind=(special_kind+1)%3;
        pad_special_prev=pad_special;
        Uint64 current=SDL_GetTicks();
        float dt=clampf((float)(current-previous)/1000.f,0.f,0.05f);
        previous=current;
        const bool *keys=SDL_GetKeyboardState(NULL);
        float dx=((keys[SDL_SCANCODE_RIGHT]||keys[SDL_SCANCODE_D]) ? 1.f:0.f)-
                 ((keys[SDL_SCANCODE_LEFT]||keys[SDL_SCANCODE_A]) ? 1.f:0.f);
        if(dx==0.f)dx=runtime_pad_horizontal(gamepad);
        bool drop_held=keys[SDL_SCANCODE_C] ||
            runtime_pad_button(gamepad,SDL_GAMEPAD_BUTTON_DPAD_DOWN);
        if (!hanging && wall_jump_lock <= 0.f) {
            if (dx > 0.f) facing=1;
            else if (dx < 0.f) facing=-1;
        }
        accumulator += dt;
        while (accumulator >= fixed_step) {
            bool hit=false;
            bool was_grounded=grounded;
            bool suspend_physics=false;
            if(restart_queued) {
                if(health.dead) {
                    runtime_health_reset(&health,99);
                    pw=standing_width;
                    ph=standing_height;
                    if(!find_spawn(room,pw,ph,&px,&py)) {
                        fprintf(stderr,"Cannot find a safe restart position.\n");
                        running=false;
                    }
                    vx=0.f;vy=0.f;grounded=false;morphed=false;hanging=false;
                    spin_jump=false;hurt_lock=0.f;wall_jump_lock=0.f;
                    ledge_pull_lock=0.f;ledge_regrab_lock=0.f;
                    echo=(RuntimeEcho){0};echo_visible=false;
                    active_animation=NULL;animation_start=SDL_GetTicks();
                    damage_queued=false;
                    fprintf(stderr,"Diagnostic restart: Energy %d.\n",health.energy);
                }
                restart_queued=false;
            }
            if(damage_queued) {
                bool on_ground=blocked(room,px,py+1.f,pw,ph);
                if(runtime_health_damage(&health,20)) {
                    hanging=false;ledge_pull_lock=0.f;spin_jump=false;
                    echo.active=false;echo.timer=0;echo_visible=false;
                    if(health.dead) {
                        vx=0.f;vy=0.f;hurt_lock=0.f;
                        fprintf(stderr,"Samus diagnostic health reached zero.\n");
                    } else {
                        /* Source-relative launch ratios; horizontal recoil is
                         * provisional until enemy collision is implemented. */
                        vy=-jump_speed*(on_ground?112.f/192.f:56.f/192.f);
                        vx=(float)-facing*90.f;
                        grounded=false;
                        hurt_lock=13.f/60.f;
                        fprintf(stderr,"Diagnostic damage: Energy %d/%d.\n",
                                health.energy,health.max_energy);
                    }
                    char title[160];
                    snprintf(title,sizeof title,"Metroid Vania [%s] [Energy %d/%d]%s",
                             armor_names[armor_index],health.energy,
                             health.max_energy,health.dead?" [DEAD]":"");
                    if(!animation_browser)SDL_SetWindowTitle(window,title);
                }
                damage_queued=false;
            }
            if(hurt_lock>0.f) {
                hurt_lock-=fixed_step;
                if(hurt_lock<0.f)hurt_lock=0.f;
            }
            if(health.dead) {
                vx=0.f;vy=0.f;jump_queued=false;morph_queued=false;
                hanging=false;grounded=false;suspend_physics=true;
            }
            if (wall_jump_lock > 0.f) {
                wall_jump_lock-=fixed_step;
                if (wall_jump_lock < 0.f) wall_jump_lock=0.f;
            }
            if (ledge_regrab_lock > 0.f) {
                ledge_regrab_lock-=fixed_step;
                if (ledge_regrab_lock < 0.f) ledge_regrab_lock=0.f;
            }
            if (!suspend_physics && ledge_pull_lock > 0.f) {
                ledge_pull_lock-=fixed_step;
                if (ledge_pull_lock < 0.f) ledge_pull_lock=0.f;
                grounded=true;
                vx=0.f;
                vy=0.f;
                jump_queued=false;
                morph_queued=false;
                suspend_physics=true;
            }
            if (!suspend_physics && hanging) {
                grounded=false;
                vx=0.f;
                vy=0.f;
                if (dx==0.f) ledge_input_armed=true;
                if (jump_queued) {
                    px=active_ledge.stand_x;
                    py=active_ledge.stand_y;
                    hanging=false;
                    grounded=true;
                    ledge_pull_up_started=true;
                    ledge_pull_lock=runtime_animation_seconds(
                        &animation_map,"ledge_pull_up",
                        suit_names[armor_index%5],
                        active_ledge.side<0?"left":"right",.15f);
                } else if (drop_held ||
                           dx*(float)active_ledge.side<-.1f) {
                    hanging=false;
                    vx=(float)-active_ledge.side*35.f;
                    vy=20.f;
                    spin_jump=false;
                    ledge_regrab_lock=.18f;
                } else if (ledge_input_armed &&
                           dx*(float)active_ledge.side>.1f) {
                    px=active_ledge.stand_x;
                    py=active_ledge.stand_y;
                    hanging=false;
                    grounded=true;
                    ledge_pull_forward_started=true;
                    ledge_pull_lock=runtime_animation_seconds(
                        &animation_map,"ledge_pull_forward",
                        suit_names[armor_index%5],
                        active_ledge.side<0?"left":"right",.2f);
                }
                jump_queued=false;
                if (morph_queued) {
                    fprintf(stderr,"Cannot morph while hanging from a ledge.\n");
                    morph_queued=false;
                }
                suspend_physics=true;
            }
            if (!suspend_physics) {
                grounded=blocked(room,px,py+1.f,pw,ph);
                if(hurt_lock>0.f) {
                    jump_queued=false;
                    morph_queued=false;
                }
                if (morph_queued) {
                    if (!morphed) {
                        if (armor_index == 4u) {
                            fprintf(stderr,"Morph Ball is unavailable for Suitless Samus.\n");
                        } else if (runtime_resize_height(
                                       room,px,&py,pw,&ph,morph_height)) {
                            morphed=true;
                            spin_jump=false;
                            morph_started=true;
                        }
                    } else if (runtime_resize_height(
                                   room,px,&py,pw,&ph,standing_height)) {
                        morphed=false;
                        unmorph_started=true;
                    } else {
                        fprintf(stderr,"Cannot unmorph: standing hitbox is blocked.\n");
                    }
                    morph_queued=false;
                }
                int wall_side=runtime_wall_side(room,px,py,pw,ph);
                if (jump_queued && grounded && !morphed && hurt_lock<=0.f) {
                    /* Jump type is latched at takeoff, not reclassified by aim keys. */
                    spin_jump=(dx > 0.1f || dx < -0.1f);
                    spin_started=spin_jump;
                    vy=-jump_speed;
                    grounded=false;
                } else if (jump_queued && !grounded && !morphed &&
                           hurt_lock<=0.f &&
                           spin_jump && wall_side != 0) {
                    /* Provisional wall-jump impulse and short steering lock. */
                    const float wall_jump_speed=150.f;
                    vx=wall_side < 0 ? wall_jump_speed : -wall_jump_speed;
                    vy=-jump_speed*.9f;
                    facing=wall_side < 0 ? 1 : -1;
                    wall_jump_lock=.13f;
                    wall_jump_started=true;
                    spin_started=false;
                }
                jump_queued=false;
                if (wall_jump_lock <= 0.f && hurt_lock<=0.f)
                    vx=update_horizontal_velocity(vx,dx,fixed_step,run_speed,
                                                  run_accel,run_braking);
                if (grounded)
                    move_grounded_x(room,&px,&py,vx*fixed_step,pw,ph);
                else
                    px=move_axis(room,px,py,vx*fixed_step,pw,ph,false,&hit);
                vy=clampf(vy+gravity*fixed_step,-jump_speed,terminal_speed);
                py=move_axis(room,py,px,vy*fixed_step,pw,ph,true,&hit);
                if (hit) {
                    if (vy>0.f) {
                        grounded=true;
                        if(!was_grounded)landed=true;
                    }
                    vy=0.f;
                }
                if (!grounded && !morphed && hurt_lock<=0.f && vy>=0.f &&
                    ledge_regrab_lock<=0.f && dx!=0.f) {
                    RuntimeLedge candidate={0};
                    int ledge_side=dx>0.f?1:-1;
                    if (runtime_find_ledge(room,px,py,pw,ph,
                                           ledge_side,&candidate)) {
                        active_ledge=candidate;
                        px=candidate.hang_x;
                        py=candidate.hang_y;
                        vx=0.f;
                        vy=0.f;
                        grounded=false;
                        hanging=true;
                        ledge_input_armed=false;
                        spin_jump=false;
                        landed=false;
                        facing=ledge_side;
                        echo.active=false;
                        echo.timer=0;
                    }
                }
            }
            RuntimeMovementState next_state =
                runtime_movement_state(grounded, vx, vy, dx);
            if (grounded && next_state != RUNTIME_JUMPING &&
                next_state != RUNTIME_FALLING) spin_jump=false;
            if (next_state != movement_state) {
                movement_state = next_state;
                char title[180];
                snprintf(title, sizeof title,
                         "Metroid Vania [%s] [%s] [Energy %d/%d]%s",
                         armor_names[armor_index],
                         runtime_movement_state_name(movement_state),
                         health.energy,health.max_energy,
                         health.dead?" [DEAD]":"");
                if(!animation_browser) SDL_SetWindowTitle(window, title);
            }
            echo_substep++;
            if(echo_substep>=2u) {
                echo_substep=0;
                /* Native threshold is 80/192 of the low-jump launch speed. */
                bool fast_ascent=!health.dead && hurt_lock<=0.f &&
                    !grounded && !hanging &&
                    vy < -jump_speed*(80.f/192.f);
                runtime_echo_step(&echo,px,py,fast_ascent);
                runtime_health_step(&health);
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
        bool aim_up=keys[SDL_SCANCODE_E] ||
            runtime_pad_button(gamepad,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        bool aim_down=keys[SDL_SCANCODE_Q] ||
            runtime_pad_button(gamepad,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        bool crouch=!morphed && (keys[SDL_SCANCODE_C] ||
            runtime_pad_button(gamepad,SDL_GAMEPAD_BUTTON_DPAD_DOWN));
        bool fire=keys[SDL_SCANCODE_F] ||
            runtime_pad_button(gamepad,SDL_GAMEPAD_BUTTON_EAST);
        int selected=(composed_dir || extended_dir || left_dir) ? composed_select(
            movement_state,facing,aim_up,aim_down,crouch,fire,
            extended_dir != NULL,left_dir != NULL,spin_jump) : -1;
        if (selected >= 0 && ((selected < COMPOSED_BASE_COUNT && !composed_dir) ||
                              (selected >= COMPOSED_BASE_COUNT &&
                               selected < COMPOSED_EXTRA_COUNT && !extended_dir) ||
                              (selected >= COMPOSED_EXTRA_COUNT && !left_dir)))
            selected = -1;
        RuntimeLibraryEntry *lib_entry=NULL;
        if(library_index && animation_map.count>0 && !animation_browser) {
            const char *side=facing<0?"left":"right";
            const char *aim=aim_up?"diagonalup":aim_down?"diagonaldown":"forward";
            const char *action=runtime_requested_action(
                movement_state,spin_jump,crouch,fire,
                grounded && dx==0.f && (vx>.1f || vx<-.1f),
                special_kind,morphed,health.dead,hurt_lock>0.f,
                morph_started,unmorph_started,
                hanging,ledge_pull_forward_started,ledge_pull_up_started,
                wall_jump_started,spin_started,landed);
            RuntimeAnimationMapRow *requested=runtime_animation_map_find(
                &animation_map,action,suit_names[armor_index%5],side,aim);
            if(!requested && (!strcmp(action,"space_jump")||
                              !strcmp(action,"screw_attack")))
                requested=runtime_animation_map_find(
                    &animation_map,"spin",suit_names[armor_index%5],side,"none");
            if(!requested)
                requested=runtime_animation_map_find(
                    &animation_map,grounded?"idle":"midair",
                    suit_names[armor_index%5],side,aim);
            RuntimeAnimationMapRow *chosen=requested;
            Uint64 now=SDL_GetTicks();
            if(active_animation && active_animation->once) {
                unsigned int elapsed=(unsigned int)((now-animation_start)*60u/1000u);
                unsigned int total=runtime_animation_total(active_animation);
                if(elapsed<total && (!requested ||
                   runtime_animation_priority(requested->action)<
                   runtime_animation_priority(active_animation->action)))
                    chosen=active_animation;
            }
            if(chosen!=active_animation) {
                active_animation=chosen;
                animation_start=now;
            }
            lib_entry=active_animation?active_animation->entry:NULL;
        } else if(library_index && !animation_browser) {
            lib_entry=runtime_library_select(&library,armor_index,facing,
                movement_state,spin_jump,aim_up,aim_down,crouch,fire,special_kind);
        }
        if(animation_browser && library_index && library.count>0) {
            lib_entry=&library.entries[browser_index];
            active_animation=NULL;
        }
        spin_started=false;landed=false;morph_started=false;
        unmorph_started=false;wall_jump_started=false;
        ledge_pull_forward_started=false;ledge_pull_up_started=false;
        int special_selected = -1;
        if (special_dir && spin_jump &&
            (movement_state == RUNTIME_JUMPING || movement_state == RUNTIME_FALLING))
            special_selected = special_kind*2 + (facing<0 ? 1 : 0);
        if(lib_entry && lib_entry->count>0) {
            Uint64 elapsed_ms=SDL_GetTicks()-animation_start;
            unsigned int ticks=(unsigned int)(elapsed_ms*60u/1000u);
            int frame=(active_animation && active_animation->once && !animation_browser) ?
                samus_timeline_frame_once(lib_entry->ticks,lib_entry->count,ticks):
                samus_timeline_frame(lib_entry->ticks,lib_entry->count,ticks);
            if(runtime_library_texture(renderer,lib_entry,frame)) {
                float sw=(float)lib_entry->w[frame],sh=(float)lib_entry->h[frame];
                unsigned int render_tick=(unsigned int)(SDL_GetTicks()*60u/1000u);
                if(animation_browser || !echo.active) {
                    echo_visible=false;
                } else if(render_tick!=echo_render_tick) {
                    echo_render_tick=render_tick;
                    echo_visible=runtime_echo_sample(&echo,2u,&echo_x,&echo_y);
                }
                if(echo_visible) {
                    /* Palette bank 1 is not exported yet; use a translucent
                     * violet modulation while retaining the native timing. */
                    SDL_SetTextureColorMod(lib_entry->textures[frame],110,100,255);
                    SDL_SetTextureAlphaMod(lib_entry->textures[frame],145);
                    SDL_FRect echo_dest={
                        viewport.x+(echo_x+pw*.5f-cx-sw*.5f)*scale,
                        viewport.y+(echo_y+ph-cy-sh)*scale,
                        sw*scale,sh*scale};
                    SDL_RenderTexture(renderer,lib_entry->textures[frame],NULL,
                                      &echo_dest);
                    SDL_SetTextureColorMod(lib_entry->textures[frame],255,255,255);
                    SDL_SetTextureAlphaMod(lib_entry->textures[frame],255);
                }
                SDL_FRect dest={viewport.x+(px+pw*.5f-cx-sw*.5f)*scale,
                    viewport.y+(py+ph-cy-sh)*scale,sw*scale,sh*scale};
                bool damage_flash=!health.dead &&
                    health.invincibility_ticks>0u && (render_tick&3u)<=1u;
                if(damage_flash)
                    SDL_SetTextureAlphaMod(lib_entry->textures[frame],90);
                SDL_RenderTexture(renderer,lib_entry->textures[frame],NULL,&dest);
                if(damage_flash)
                    SDL_SetTextureAlphaMod(lib_entry->textures[frame],255);
            }
        } else if (special_selected >= 0) {
            Uint64 elapsed_ms=SDL_GetTicks()-animation_start;
            unsigned int elapsed_frames=(unsigned int)(elapsed_ms*60u/1000u);
            int frame=samus_timeline_frame(special_frames.durations[special_selected],
                                            SPECIAL_FRAMES,elapsed_frames);
            SDL_Texture *sprite=special_frames.textures[special_selected][frame];
            float sw=(float)special_frames.widths[special_selected][frame];
            float sh=(float)special_frames.heights[special_selected][frame];
            SDL_FRect dest={viewport.x+(px+pw*.5f-cx-sw*.5f)*scale,
                            viewport.y+(py+ph-cy-sh)*scale,sw*scale,sh*scale};
            SDL_RenderTexture(renderer,sprite,NULL,&dest);
        } else if (selected>=0) {
            Uint64 elapsed_ms=SDL_GetTicks()-animation_start;
            unsigned int elapsed_frames=(unsigned int)(elapsed_ms*60u/1000u);
            int frame=samus_timeline_frame(composed_frames.durations[selected],
                                           composed_counts[selected],elapsed_frames);
            SDL_Texture *sprite=composed_frames.textures[selected][frame];
            float sw=(float)composed_frames.widths[selected][frame];
            float sh=(float)composed_frames.heights[selected][frame];
            SDL_FRect dest={viewport.x+(px+pw*.5f-cx-sw*.5f)*scale,
                            viewport.y+(py+ph-cy-sh)*scale,sw*scale,sh*scale};
            SDL_RenderTexture(renderer,sprite,NULL,&dest);
        } else if (samus_dir) {
            /* Legacy jump BMP is not proven to be a native spin animation.
             * Keep it as a fallback without claiming spin fidelity. */
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
    if(gamepad)SDL_CloseGamepad(gamepad);
    runtime_library_free(&library);
    special_free(&special_frames);
    composed_free(&composed_frames);
    samus_frames_free(&samus_frames);
    if (texture) SDL_DestroyTexture(texture);
    if (surface) SDL_DestroySurface(surface);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();free(room);return rc;
}
#endif /* FUSION_RUNTIME_TEST */
