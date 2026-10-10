/* SPDX-License-Identifier: GPL-3.0-only */
/* Experimental C11/SDL3 Zero Mission room runtime. Samus, weapons and
 * collision types follow the pinned decompilation; rooms, entities and the
 * room lifecycle are still incomplete. */
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mzm_projectiles.h"
#include "mzm_samus.h"

#define MAX_MARKS 32768
#define MAX_FILE_BYTES 4000000L
#define MAX_ROOM_CELLS 6144

/* ClipdataType order of the pinned decompilation. */
typedef enum {
    CLIP_AIR, CLIP_SOLID, CLIP_LEFT_STEEP, CLIP_RIGHT_STEEP,
    CLIP_LEFT_UPPER_SLIGHT, CLIP_LEFT_LOWER_SLIGHT, CLIP_RIGHT_LOWER_SLIGHT,
    CLIP_RIGHT_UPPER_SLIGHT, CLIP_ENEMY_ONLY, CLIP_STOP_ENEMY, CLIP_TANK,
    CLIP_DOOR, CLIP_PASS_THROUGH_BOTTOM, CLIP_TYPE_COUNT
} ClipType;
/* ClipdataActor */
typedef enum { ACTOR_SAMUS, ACTOR_NON_SPRITE, ACTOR_SPRITE } ClipActor;

/* One native collision type per 16-pixel block. */
typedef struct { int width, height, resolution, room; char world[16], area[40];
    unsigned char types[MAX_ROOM_CELLS]; size_t count; } Room;

static void room_set_cell(Room *room, int x, int y, ClipType type) {
    int columns = room->width / 16;
    if (room->types[y * columns + x] == CLIP_AIR && type != CLIP_AIR) room->count++;
    room->types[y * columns + x] = (unsigned char)type;
}

static ClipType room_cell(const Room *room, int x, int y) {
    return (ClipType)room->types[y * (room->width / 16) + x];
}

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
        /* Project geometry: only code 1 is solid; no Clipdata guesses. */
        if (kind == 'C' && code == 1) room_set_cell(room, x / 16, y / 16, CLIP_SOLID);
    }
    if (!ended || fgetc(f) != EOF) goto failure;
    fclose(f); return true;
failure:
    fclose(f); fprintf(stderr, "Invalid room preview: %s\n", path); return false;
}

/* ClipdataConvertToCollision: solidity of one subpixel (0..63 on each axis)
 * inside a block for the given actor. Slopes keep their lower part solid. */
static bool clip_solid(ClipType type, int sub_x, int sub_y, ClipActor actor) {
    switch (type) {
        case CLIP_SOLID:
        case CLIP_DOOR: return true;
        case CLIP_LEFT_STEEP: return sub_y >= sub_x;
        case CLIP_RIGHT_STEEP: return sub_y >= 63 - sub_x;
        case CLIP_LEFT_UPPER_SLIGHT: return sub_y >= sub_x >> 1;
        case CLIP_LEFT_LOWER_SLIGHT: return sub_y >= (sub_x + 63) >> 1;
        case CLIP_RIGHT_LOWER_SLIGHT: return sub_y >= 63 - (sub_x >> 1);
        case CLIP_RIGHT_UPPER_SLIGHT: return sub_y >= (63 - sub_x) >> 1;
        case CLIP_ENEMY_ONLY: return actor <= ACTOR_NON_SPRITE;
        case CLIP_STOP_ENEMY: return actor >= ACTOR_SPRITE;
        case CLIP_TANK: return actor != ACTOR_SAMUS;
        case CLIP_AIR:
        case CLIP_PASS_THROUGH_BOTTOM:
        case CLIP_TYPE_COUNT: break;
    }
    return false;
}

static bool clip_is_floor_slope(ClipType type) {
    return type >= CLIP_LEFT_STEEP && type <= CLIP_RIGHT_UPPER_SLIGHT;
}

/* Box overlap against the native type grid for Samus. Partially solid
 * blocks are sampled at each covered pixel's central subpixel. */
static bool blocked(const Room *r, float x, float y, float w, float h) {
    if (x < 0 || y < 0 || x + w > r->width || y + h > r->height) return true;
    int first_x = (int)x / 16, last_x = (int)(x + w - 0.001f) / 16;
    int first_y = (int)y / 16, last_y = (int)(y + h - 0.001f) / 16;
    for (int cy = first_y; cy <= last_y; ++cy) {
        for (int cx = first_x; cx <= last_x; ++cx) {
            ClipType type = room_cell(r, cx, cy);
            if (type == CLIP_AIR) continue;
            if (!clip_is_floor_slope(type)) {
                if (clip_solid(type, 0, 0, ACTOR_SAMUS)) return true;
                continue;
            }
            for (int py = 0; py < 16; ++py) {
                float sy = (float)(cy * 16 + py) + .5f;
                if (sy < y || sy >= y + h) continue;
                for (int px = 0; px < 16; ++px) {
                    float sx = (float)(cx * 16 + px) + .5f;
                    if (sx < x || sx >= x + w) continue;
                    if (clip_solid(type, px * 4 + 2, py * 4 + 2, ACTOR_SAMUS)) return true;
                }
            }
        }
    }
    return false;
}

/* ClipdataProcessForSamus (Samus probes) and ClipdataProcess (projectiles):
 * one subpixel point. Out of the room, Samus sees solid columns and air rows;
 * other actors see air. */
static bool solid_point(const Room *r, int x, int y, ClipActor actor) {
    if (x < 0 || x >= r->width * 4) return actor == ACTOR_SAMUS;
    if (y < 0 || y >= r->height * 4) return false;
    return clip_solid(room_cell(r, x / 64, y / 64), x % 64, y % 64, actor);
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
            /* Legacy sidecar: only the verified solid and steep IDs. */
            room_set_cell(room, x / 16, y / 16, code == 16 ? CLIP_SOLID :
                          code == 17 ? CLIP_RIGHT_STEEP : CLIP_LEFT_STEEP);
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

/* MVROOM-NATIVE room exported by scripts/mzm_runtime_room.py: one line per
 * nonzero Clipdata cell with its raw value and resolved native type. */
static bool parse_native_room(const char *path, Room *room) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return false; }
    char line[256], extra;
    int version;
    bool ended = false;
    if (fseek(f, 0, SEEK_END) || ftell(f) < 0 || ftell(f) > MAX_FILE_BYTES ||
        fseek(f, 0, SEEK_SET)) goto failure;
    if (!fgets(line, sizeof line, f) ||
        sscanf(line, "MVROOM-NATIVE\t%d\t%15[^\t]\t%39[^\t]\t%d\t%d\t%d %c",
               &version, room->world, room->area, &room->room, &room->width,
               &room->height, &extra) != 6 || version != 1 ||
        strcmp(room->world, "mzm") || room->room < 0 || room->room > 999 ||
        room->width < 16 || room->height < 16 || room->width % 16 ||
        room->height % 16 || room->width / 16 * (room->height / 16) > MAX_ROOM_CELLS)
        goto failure;
    room->resolution = 16;
    while (fgets(line, sizeof line, f)) {
        int x, y, raw, type;
        if (!strchr(line, '\n') && !feof(f)) goto failure;
        if (!strcmp(line, "END\n") || !strcmp(line, "END")) { ended = true; break; }
        if (sscanf(line, "C\t%d\t%d\t%d\t%d %c", &x, &y, &raw, &type, &extra) != 4 ||
            x < 0 || y < 0 || x >= room->width / 16 || y >= room->height / 16 ||
            raw < 1 || raw > 65535 || type < 0 || type >= CLIP_TYPE_COUNT)
            goto failure;
        room_set_cell(room, x, y, (ClipType)type);
    }
    if (!ended || fgetc(f) != EOF) goto failure;
    fclose(f);
    return true;
failure:
    fclose(f);
    fprintf(stderr, "Invalid native runtime room: %s\n", path);
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
                if (pass == 0) {
                    /* Settle within the 4-pixel scan step so hitbox
                     * heights that are not multiples of four can stand. */
                    int settle = 0;
                    while (settle < 3 && !blocked(room, fx, fy + 1.f, w, h)) {
                        fy += 1.f;
                        ++settle;
                    }
                    if (!blocked(room, fx, fy + 1.f, w, h)) continue;
                }
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

/* Grounded movement may step at most two pixels while a native floor
 * slope is beneath or beside the hitbox. */
static bool near_steep_slope(const Room *room, float x, float y,
                             float w, float h) {
    int columns = room->width / 16, rows = room->height / 16;
    int first_x = (int)(x < 0 ? 0 : x) / 16, last_x = (int)(x + w - 0.001f) / 16;
    int first_y = (int)(y + h - 2.f < 0 ? 0 : y + h - 2.f) / 16;
    int last_y = (int)(y + h + 2.f) / 16;
    for (int cy = first_y; cy <= last_y && cy < rows; ++cy)
        for (int cx = first_x; cx <= last_x && cx < columns; ++cx)
            if (clip_is_floor_slope(room_cell(room, cx, cy))) return true;
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
    }
    /* As in SamusUpdateGraphicsOam, the refresh frame also counts down. */
    if (echo->timer>0) echo->timer--;
    else echo->active=false;
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

/* Adapters exposing the verified room collision to the pose controller. */
static bool runtime_collision_blocked(void *context,float x,float y,
                                      float w,float h) {
    return blocked((const Room *)context,x,y,w,h);
}
static bool runtime_collision_slope(void *context,float x,float y,
                                    float w,float h) {
    return near_steep_slope((const Room *)context,x,y,w,h);
}
static bool runtime_collision_point(void *context,int32_t x,int32_t y,int actor) {
    return solid_point((const Room *)context,x,y,
                       actor==0?ACTOR_SAMUS:ACTOR_NON_SPRITE);
}

/* Semantic registry action shown by each native pose. Every spin pose maps
 * to its own spin action so that generic MidAir can never replace it. */
static const char *runtime_pose_action(MzmPose pose,uint32_t items) {
    switch (pose) {
        case MZM_POSE_STANDING: return "idle";
        case MZM_POSE_RUNNING: return "run";
        case MZM_POSE_TURNING_AROUND: return "turn";
        case MZM_POSE_SHOOTING: return "fire";
        case MZM_POSE_CROUCHING: return "crouch";
        case MZM_POSE_TURNING_AROUND_AND_CROUCHING: return "turn_crouch";
        case MZM_POSE_SHOOTING_AND_CROUCHING: return "crouch_fire";
        case MZM_POSE_MIDAIR: return "midair";
        case MZM_POSE_TURNING_AROUND_MIDAIR: return "turn_midair";
        case MZM_POSE_LANDING: return "landing";
        case MZM_POSE_STARTING_SPIN_JUMP: return "spin_start";
        case MZM_POSE_SPINNING: return "spin";
        case MZM_POSE_STARTING_WALL_JUMP: return "wall_jump";
        case MZM_POSE_SPACE_JUMPING: return "space_jump";
        case MZM_POSE_SCREW_ATTACKING:
            /* SamusUpdateGraphicsOam: ScrewAttacking[space jump equipped]. */
            return (items&MZM_ITEM_SPACE_JUMP)?"screw_attack_space":"screw_attack";
        case MZM_POSE_MORPHING: return "morph_start";
        case MZM_POSE_MORPH_BALL: return "morph_ball";
        case MZM_POSE_ROLLING: return "rolling";
        case MZM_POSE_MORPH_BALL_MIDAIR: return "morph_midair";
        case MZM_POSE_GETTING_HURT_IN_MORPH_BALL: return "hurt_morph";
        case MZM_POSE_UNMORPHING: return "unmorph";
        case MZM_POSE_HANGING_ON_LEDGE: return "ledge_hang";
        case MZM_POSE_PULLING_UP: return "ledge_pull_up";
        case MZM_POSE_PULLING_FORWARD: return "ledge_pull_forward";
        case MZM_POSE_GETTING_HURT: return "hurt";
        case MZM_POSE_DYING: return "death";
        case MZM_POSE_COUNT: break;
    }
    return "idle";
}

/* Registry fallback used only when a suit lacks the exact native sequence. */
static const char *runtime_pose_fallback_action(MzmPose pose) {
    switch (pose) {
        case MZM_POSE_STARTING_SPIN_JUMP:
        case MZM_POSE_SPINNING:
        case MZM_POSE_SPACE_JUMPING:
        case MZM_POSE_SCREW_ATTACKING:
        case MZM_POSE_STARTING_WALL_JUMP: return "spin";
        case MZM_POSE_TURNING_AROUND_MIDAIR: return "midair";
        case MZM_POSE_TURNING_AROUND_AND_CROUCHING:
        case MZM_POSE_SHOOTING_AND_CROUCHING: return "crouch";
        case MZM_POSE_ROLLING:
        case MZM_POSE_MORPH_BALL_MIDAIR:
        case MZM_POSE_GETTING_HURT_IN_MORPH_BALL: return "morph_ball";
        default: break;
    }
    return mzm_pose_standing(pose) == MZM_STANDING_MIDAIR ? "midair" : "idle";
}

static const char *runtime_aim_name(MzmAim aim) {
    switch (aim) {
        case MZM_AIM_DIAGONAL_UP: return "diagonalup";
        case MZM_AIM_DIAGONAL_DOWN: return "diagonaldown";
        case MZM_AIM_UP: return "up";
        case MZM_AIM_DOWN: return "down";
        case MZM_AIM_FORWARD: break;
    }
    return "forward";
}

/* Diagnostic equipment presets. The registry suit names select graphics;
 * suit type and item flags drive the native rules. */
typedef struct {
    const char *name,*registry;
    MzmSuitType suit;
    uint32_t items;
} RuntimeSuitPreset;
static const RuntimeSuitPreset runtime_suit_presets[]={
    {"Power Suit","PowerSuit",MZM_SUIT_NORMAL,0},
    {"Varia Suit","VariaSuit",MZM_SUIT_NORMAL,MZM_ITEM_VARIA_SUIT},
    {"Gravity Suit","GravitySuit",MZM_SUIT_FULLY_POWERED,
     MZM_ITEM_VARIA_SUIT|MZM_ITEM_GRAVITY_SUIT},
    {"Full Suit","FullSuit",MZM_SUIT_FULLY_POWERED,MZM_ITEM_VARIA_SUIT},
    {"Suitless","Suitless",MZM_SUIT_SUITLESS,0},
};
#define RUNTIME_SUIT_PRESETS \
    (sizeof runtime_suit_presets/sizeof runtime_suit_presets[0])

/* Morph Ball and Power Grip stay available in suited presets so the
 * implemented abilities are reachable before item pickups exist. */
static void runtime_apply_equipment(MzmEquipment *equipment,unsigned int preset,
                                    uint32_t toggled_items) {
    const RuntimeSuitPreset *p=&runtime_suit_presets[preset%RUNTIME_SUIT_PRESETS];
    equipment->suit=p->suit;
    equipment->items=p->items;
    if(p->suit!=MZM_SUIT_SUITLESS)
        equipment->items|=MZM_ITEM_MORPH_BALL|MZM_ITEM_POWER_GRIP|toggled_items;
}

/* Place Samus's native standing hitbox at the safe diagnostic spawn. */
static bool runtime_spawn_samus(const Room *room,MzmSamus *samus) {
    const MzmBlockHitbox *box=mzm_block_hitbox(MZM_HITBOX_STANDING);
    float w=(float)(box->right-box->left)/MZM_SUBPIXELS_PER_PIXEL;
    float h=(float)-box->top/MZM_SUBPIXELS_PER_PIXEL;
    float x=0.f,y=0.f;
    if(!find_spawn(room,w,h,&x,&y))return false;
    mzm_samus_init(samus,(int32_t)(x*MZM_SUBPIXELS_PER_PIXEL)-box->left,
                   (int32_t)(y*MZM_SUBPIXELS_PER_PIXEL)-box->top,1);
    return true;
}

/* SamusUpdateGraphicsOam echo trigger: listed poses rising faster than
 * SUB_PIXEL_TO_VELOCITY(EIGHTH_BLOCK_SIZE + PIXEL_SIZE / 2). */
static bool runtime_echo_fast_ascent(const MzmSamus *samus) {
    switch(samus->pose) {
        case MZM_POSE_MIDAIR:
        case MZM_POSE_SPINNING:
        case MZM_POSE_SPACE_JUMPING:
        case MZM_POSE_SCREW_ATTACKING:
        case MZM_POSE_MORPH_BALL_MIDAIR:
            return samus->y_velocity >
                8*(MZM_EIGHTH_BLOCK_SIZE+MZM_PIXEL_SIZE/2);
        default:
            return false;
    }
}

#ifndef FUSION_RUNTIME_TEST
/* Shared content-addressed sprite libraries (scripts/sprite_library.py). */
#define RUNTIME_LIBRARY_MAX 8192
#define RUNTIME_LIBRARY_FRAME_MAX 256
#define RUNTIME_ANIMATION_MAP_MAX 2048
#define RUNTIME_INDEX_SCHEMA "schema\tmetroidvania-sprite-index-v1\n"
#define RUNTIME_CANNON_SCHEMA "schema\tmetroidvania-samus-cannon-offsets-v1\n"
/* One native frame: duration, top-left offset from the character's draw
 * origin, an optional arm cannon offset and the private BMP. */
typedef struct {
    unsigned int ticks;
    int offset_x,offset_y;
    bool has_cannon;
    int cannon_x,cannon_y;
    char path[256];
    SDL_Texture *texture;
    int w,h;
} RuntimeLibraryFrame;
typedef struct {
    char name[160];
    int count,capacity;
    RuntimeLibraryFrame *frames;
    unsigned int *ticks;
} RuntimeLibraryEntry;
typedef struct {
    RuntimeLibraryEntry *entries;
    RuntimeLibraryEntry **sorted;
    int count,capacity;
} RuntimeLibrary;
static int runtime_library_compare(const void *a,const void *b) {
    const RuntimeLibraryEntry *const *left=a,*const *right=b;
    return strcmp((*left)->name,(*right)->name);
}
static void runtime_library_free(RuntimeLibrary *lib) {
    for(int i=0;i<lib->count;i++) {
        RuntimeLibraryEntry *entry=&lib->entries[i];
        for(int j=0;j<entry->count;j++)
            if(entry->frames[j].texture) SDL_DestroyTexture(entry->frames[j].texture);
        free(entry->frames);
        free(entry->ticks);
    }
    free(lib->entries);
    free(lib->sorted);
    *lib=(RuntimeLibrary){0};
}
static bool runtime_library_add_frame(RuntimeLibraryEntry *entry,
                                      const RuntimeLibraryFrame *frame) {
    if(entry->count>=RUNTIME_LIBRARY_FRAME_MAX)return false;
    if(entry->count==entry->capacity) {
        int capacity=entry->capacity?entry->capacity*2:8;
        RuntimeLibraryFrame *frames=realloc(entry->frames,(size_t)capacity*sizeof *frames);
        if(!frames)return false;
        entry->frames=frames;
        unsigned int *ticks=realloc(entry->ticks,(size_t)capacity*sizeof *ticks);
        if(!ticks)return false;
        entry->ticks=ticks;
        entry->capacity=capacity;
    }
    entry->frames[entry->count]=*frame;
    entry->ticks[entry->count]=frame->ticks;
    entry->count++;
    return true;
}
static bool runtime_library_open(RuntimeLibrary *lib,const char *index) {
    FILE *f=fopen(index,"rb");if(!f) return false;
    char line[1024],prev[160]="";
    bool ok=fgets(line,sizeof line,f) && !strcmp(line,RUNTIME_INDEX_SCHEMA);
    while(ok && fgets(line,sizeof line,f)) {
        char name[160];
        RuntimeLibraryFrame frame={0};
        unsigned int index_value;
        int consumed=0;
        if(sscanf(line,"%159[^\t]\t%u\t%u\t%d\t%d\t%255[^\t\r\n]%n",
                  name,&index_value,&frame.ticks,&frame.offset_x,&frame.offset_y,
                  frame.path,&consumed)!=6 ||
           consumed<=0 || (line[consumed]!='\n' && line[consumed]!='\r') ||
           (line[consumed]=='\n' && line[consumed+1]!='\0') ||
           (line[consumed]=='\r' &&
             !(line[consumed+1]=='\n' && line[consumed+2]=='\0')) ||
           frame.ticks<1 || frame.ticks>255 ||
           frame.offset_x<-256 || frame.offset_x>256 ||
           frame.offset_y<-256 || frame.offset_y>256 ||
           strchr(frame.path,'/')==NULL || frame.path[0]=='/' ||
           strstr(frame.path,"..")) {ok=false;break;}
        if(strcmp(name,prev)) {
            for(int i=0;i<lib->count;i++)
                if(!strcmp(lib->entries[i].name,name)){ok=false;break;}
            if(!ok || lib->count>=RUNTIME_LIBRARY_MAX){ok=false;break;}
            if(lib->count==lib->capacity) {
                int capacity=lib->capacity?lib->capacity*2:64;
                RuntimeLibraryEntry *entries=realloc(lib->entries,
                    (size_t)capacity*sizeof *entries);
                if(!entries){ok=false;break;}
                lib->entries=entries;
                lib->capacity=capacity;
            }
            snprintf(prev,sizeof prev,"%s",name);
            RuntimeLibraryEntry *entry=&lib->entries[lib->count++];
            *entry=(RuntimeLibraryEntry){0};
            snprintf(entry->name,sizeof entry->name,"%s",name);
        }
        RuntimeLibraryEntry *e=&lib->entries[lib->count-1];
        if(index_value!=(unsigned)e->count || !runtime_library_add_frame(e,&frame)) {
            ok=false;break;
        }
    }
    if(ferror(f))ok=false;
    fclose(f);
    if(ok && lib->count>0) {
        lib->sorted=malloc((size_t)lib->count*sizeof *lib->sorted);
        if(!lib->sorted)ok=false;
        else {
            for(int i=0;i<lib->count;i++)lib->sorted[i]=&lib->entries[i];
            qsort(lib->sorted,(size_t)lib->count,sizeof *lib->sorted,
                  runtime_library_compare);
        }
    }
    if(!ok || lib->count==0){runtime_library_free(lib);return false;}
    return true;
}
static RuntimeLibraryEntry *runtime_library_find(RuntimeLibrary *lib,const char *name) {
    int low=0,high=lib->count-1;
    while(lib->sorted && low<=high) {
        int middle=low+(high-low)/2;
        int order=strcmp(lib->sorted[middle]->name,name);
        if(!order)return lib->sorted[middle];
        if(order<0)low=middle+1; else high=middle-1;
    }
    return NULL;
}
/* Per-frame arm cannon offsets written by the Samus pipeline. */
static bool runtime_cannon_offsets_open(RuntimeLibrary *lib,const char *path) {
    FILE *f=fopen(path,"rb");
    if(!f)return false;
    char line[512];
    bool ok=fgets(line,sizeof line,f) && !strcmp(line,RUNTIME_CANNON_SCHEMA);
    int rows=0;
    while(ok && fgets(line,sizeof line,f)) {
        char key[160];unsigned int frame;int x,y,consumed=0;
        if(sscanf(line,"%159[^\t]\t%u\t%d\t%d%n",key,&frame,&x,&y,&consumed)!=4 ||
           (line[consumed]!='\n' && line[consumed]!='\0') ||
           x<-128 || x>128 || y<-128 || y>128){ok=false;break;}
        RuntimeLibraryEntry *entry=runtime_library_find(lib,key);
        if(!entry || frame>=(unsigned)entry->count){ok=false;break;}
        entry->frames[frame].has_cannon=true;
        entry->frames[frame].cannon_x=x;
        entry->frames[frame].cannon_y=y;
        rows++;
    }
    if(ferror(f))ok=false;
    fclose(f);
    return ok && rows>0;
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
typedef struct {
    RuntimeAnimationMap *map;
    const char *suit;
    const MzmEquipment *equipment;
} RuntimePoseAnimation;
static RuntimeAnimationMapRow *runtime_pose_row(RuntimeAnimationMap *map,
        const MzmSamus *samus,const char *suit,uint32_t items) {
    const char *side=samus->facing<0?"left":"right";
    const char *aim=runtime_aim_name(samus->aim);
    RuntimeAnimationMapRow *row=runtime_animation_map_find(
        map,runtime_pose_action(samus->pose,items),suit,side,aim);
    if(!row && samus->pose==MZM_POSE_SCREW_ATTACKING)
        row=runtime_animation_map_find(map,"screw_attack",suit,side,aim);
    if(!row)row=runtime_animation_map_find(
        map,runtime_pose_fallback_action(samus->pose),suit,side,aim);
    return row;
}
static int runtime_pose_durations(void *context,const MzmSamus *samus,
                                  uint8_t *durations,int max) {
    RuntimePoseAnimation *animation=context;
    if(animation->map->count==0)return 0;
    RuntimeAnimationMapRow *row=runtime_pose_row(animation->map,samus,
        animation->suit,animation->equipment->items);
    if(!row)return 0;
    int count=row->entry->count<max?row->entry->count:max;
    for(int i=0;i<count;i++)durations[i]=(uint8_t)row->entry->ticks[i];
    return count;
}
/* Projectile library key for ProjectileProcess* OAM selection and the
 * X/Y flips applied by ProjectileDraw. */
static void runtime_projectile_key(const MzmProjectile *projectile,char *key,size_t size) {
    static const char *names[]={"NormalBeam","Missile","SuperMissile"};
    const char *shape=projectile->direction==MZM_AIM_UP||projectile->direction==MZM_AIM_DOWN?
        "Vertical":projectile->direction==MZM_AIM_FORWARD?"Horizontal":"Diagonal";
    const char *flip=projectile->x_flip?(projectile->y_flip?"xy":"x"):
        (projectile->y_flip?"y":"none");
    snprintf(key,size,"Projectile/%sOam_%s/%s",names[projectile->type],shape,flip);
}
static int runtime_projectile_durations(void *context,const MzmProjectile *projectile,
                                        uint8_t *durations,int max) {
    char key[160];
    runtime_projectile_key(projectile,key,sizeof key);
    RuntimeLibraryEntry *entry=runtime_library_find(context,key);
    if(!entry)return 0;
    int count=entry->count<max?entry->count:max;
    for(int i=0;i<count;i++)durations[i]=(uint8_t)entry->ticks[i];
    return count;
}
static bool runtime_library_texture(SDL_Renderer *r,RuntimeLibraryEntry *entry,int index) {
    RuntimeLibraryFrame *frame=&entry->frames[index];
    if(frame->texture)return true;
    SDL_Surface *s=SDL_LoadBMP(frame->path);
    if(!s)return false;
    if(s->w<1||s->h<1||s->w>512||s->h>512){SDL_DestroySurface(s);return false;}
    int w=s->w,h=s->h;
    SDL_Texture *t=SDL_CreateTextureFromSurface(r,s);
    SDL_DestroySurface(s);
    if(!t)return false;
    frame->texture=t;frame->w=w;frame->h=h;
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
/* Keyboard layout for the GBA buttons; Q also starts a downward diagonal. */
static uint16_t runtime_key_buttons(SDL_Scancode key) {
    switch(key) {
        case SDL_SCANCODE_RIGHT: case SDL_SCANCODE_D: return MZM_KEY_RIGHT;
        case SDL_SCANCODE_LEFT: case SDL_SCANCODE_A: return MZM_KEY_LEFT;
        case SDL_SCANCODE_UP: case SDL_SCANCODE_W: return MZM_KEY_UP;
        case SDL_SCANCODE_DOWN: case SDL_SCANCODE_S:
        case SDL_SCANCODE_C: return MZM_KEY_DOWN;
        case SDL_SCANCODE_SPACE: case SDL_SCANCODE_Z: return MZM_KEY_A;
        case SDL_SCANCODE_F: case SDL_SCANCODE_X: return MZM_KEY_B;
        case SDL_SCANCODE_E: return MZM_KEY_L;
        case SDL_SCANCODE_Q: return MZM_KEY_L|MZM_KEY_DOWN;
        case SDL_SCANCODE_V: case SDL_SCANCODE_LSHIFT: return MZM_KEY_R;
        case SDL_SCANCODE_TAB: return MZM_KEY_SELECT;
        default: return 0;
    }
}
static uint16_t runtime_held_buttons(const bool *keys,SDL_Gamepad *pad) {
    static const SDL_Scancode held_keys[]={
        SDL_SCANCODE_RIGHT,SDL_SCANCODE_D,SDL_SCANCODE_LEFT,SDL_SCANCODE_A,
        SDL_SCANCODE_UP,SDL_SCANCODE_W,SDL_SCANCODE_DOWN,SDL_SCANCODE_S,
        SDL_SCANCODE_C,SDL_SCANCODE_SPACE,SDL_SCANCODE_Z,SDL_SCANCODE_F,
        SDL_SCANCODE_X,SDL_SCANCODE_E,SDL_SCANCODE_Q,SDL_SCANCODE_V,
        SDL_SCANCODE_LSHIFT,SDL_SCANCODE_TAB
    };
    uint16_t buttons=0;
    for(size_t i=0;i<sizeof held_keys/sizeof held_keys[0];i++)
        if(keys[held_keys[i]])buttons|=runtime_key_buttons(held_keys[i]);
    /* Q's downward start is an edge, not a held direction. */
    if(keys[SDL_SCANCODE_Q] && !keys[SDL_SCANCODE_DOWN] &&
       !keys[SDL_SCANCODE_S] && !keys[SDL_SCANCODE_C])
        buttons&=(uint16_t)~MZM_KEY_DOWN;
    if(pad) {
        float sx=(float)SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFTX)/32767.f;
        float sy=(float)SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFTY)/32767.f;
        if(runtime_pad_button(pad,SDL_GAMEPAD_BUTTON_DPAD_RIGHT)||sx>.5f)
            buttons|=MZM_KEY_RIGHT;
        if(runtime_pad_button(pad,SDL_GAMEPAD_BUTTON_DPAD_LEFT)||sx<-.5f)
            buttons|=MZM_KEY_LEFT;
        if(runtime_pad_button(pad,SDL_GAMEPAD_BUTTON_DPAD_UP)||sy<-.5f)
            buttons|=MZM_KEY_UP;
        if(runtime_pad_button(pad,SDL_GAMEPAD_BUTTON_DPAD_DOWN)||sy>.5f)
            buttons|=MZM_KEY_DOWN;
        if(runtime_pad_button(pad,SDL_GAMEPAD_BUTTON_SOUTH))buttons|=MZM_KEY_A;
        if(runtime_pad_button(pad,SDL_GAMEPAD_BUTTON_EAST)||
           runtime_pad_button(pad,SDL_GAMEPAD_BUTTON_WEST))buttons|=MZM_KEY_B;
        if(runtime_pad_button(pad,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))
            buttons|=MZM_KEY_L;
        if(runtime_pad_button(pad,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
            buttons|=MZM_KEY_R;
        if(runtime_pad_button(pad,SDL_GAMEPAD_BUTTON_BACK))
            buttons|=MZM_KEY_SELECT;
    }
    /* A GBA D-pad cannot report opposite directions together. */
    if((buttons&(MZM_KEY_LEFT|MZM_KEY_RIGHT))==(MZM_KEY_LEFT|MZM_KEY_RIGHT))
        buttons&=(uint16_t)~(MZM_KEY_LEFT|MZM_KEY_RIGHT);
    if((buttons&(MZM_KEY_UP|MZM_KEY_DOWN))==(MZM_KEY_UP|MZM_KEY_DOWN))
        buttons&=(uint16_t)~(MZM_KEY_UP|MZM_KEY_DOWN);
    return buttons;
}
int main(int argc, char **argv) {
    const char *room_path=NULL, *background=NULL, *native_source=NULL;
    const char *library_index=NULL, *animation_map_index=NULL;
    const char *projectile_index=NULL, *cannon_index=NULL;
    const char *room_alias=NULL, *samus_assets=NULL;
    const char *library_check=NULL,*capture_path=NULL;
    long capture_frames=0,capture_repeat=0;
    unsigned long capture_buttons=0;
    bool check=false,animation_check=false;
    for (int i=1;i<argc;i++) {
        if (!strcmp(argv[i],"--check")) { if(check) return 2; check=true; }
        else if (!strcmp(argv[i],"--check-animations")) {
            if(animation_check)return 2;
            animation_check=true;
        }
        else if (!strcmp(argv[i],"--capture") && !capture_path && i+4<argc) {
            /* --capture out.bmp FRAMES BUTTONS REPEAT: headless diagnostic. */
            char *end=NULL;
            capture_path=argv[++i];
            capture_frames=strtol(argv[++i],&end,10);
            if(*end || capture_frames<1 || capture_frames>36000)return 2;
            capture_buttons=strtoul(argv[++i],&end,0);
            if(*end || capture_buttons>0x1FFu)return 2;
            capture_repeat=strtol(argv[++i],&end,10);
            if(*end || capture_repeat<0 || capture_repeat>3600)return 2;
        }
        else if (!strcmp(argv[i],"--check-library") && !library_check && i+1<argc)
            library_check=argv[++i];
        else if (!strcmp(argv[i],"--background") && !background && i+1<argc) background=argv[++i];
        else if (!strcmp(argv[i],"--native-source") && !native_source && i+1<argc) native_source=argv[++i];
        else if (!strcmp(argv[i],"--room") && !room_alias && i+1<argc) room_alias=argv[++i];
        else if (!strcmp(argv[i],"--samus-assets") && !samus_assets && i+1<argc) samus_assets=argv[++i];
        else if (!strcmp(argv[i],"--samus-library") && !library_index && i+1<argc) library_index=argv[++i];
        else if (!strcmp(argv[i],"--samus-map") && !animation_map_index && i+1<argc) animation_map_index=argv[++i];
        else if (!strcmp(argv[i],"--projectile-library") && !projectile_index && i+1<argc)
            projectile_index=argv[++i];
        else if (argv[i][0]=='-' || room_path) {
            fprintf(stderr,"Usage: %s [--check] [--background image.bmp] [--native-source source.tsv] [--samus-assets directory | --samus-library index.tsv --samus-map map.tsv] preview.tsv\n",argv[0]);
            return 2;
        } else room_path=argv[i];
    }
    if (library_check) {
        /* Validate any shared sprite library (Samus or Soma) without SDL video. */
        if (argc != 3) {
            fprintf(stderr,"--check-library takes only an index path\n");
            return 2;
        }
        RuntimeLibrary checked={0};
        if (!runtime_library_open(&checked,library_check)) {
            fprintf(stderr,"Sprite library validation failed: %s\n",library_check);
            return 2;
        }
        int frames=0;
        for (int i=0;i<checked.count;i++) frames+=checked.entries[i].count;
        printf("Validated sprite library: %d sequences, %d frames\n",checked.count,frames);
        runtime_library_free(&checked);
        return 0;
    }
    /* --room <area>_<NNN>: a native room exported by scripts/mzm_runtime_room.py. */
    char bundle_index[4096],bundle_map[4096],bundle_cannon[4096];
    char native_room[4096],native_background[4096];
    bool native_format=false;
    if (room_alias) {
        size_t length=strlen(room_alias);
        bool valid=length>4 && length<48 && room_alias[length-4]=='_';
        for (size_t i=0;valid && i<length;i++)
            valid=(room_alias[i]>='a' && room_alias[i]<='z') || room_alias[i]=='_' ||
                  (i>=length-3 && room_alias[i]>='0' && room_alias[i]<='9');
        if (!valid || room_path || background || native_source) {
            fprintf(stderr,"Invalid room alias or conflicting room paths: %s\n",room_alias);
            return 2;
        }
        snprintf(native_room,sizeof native_room,
                 "assets/extracted/metroid/rooms/runtime/%s/room.tsv",room_alias);
        snprintf(native_background,sizeof native_background,
                 "assets/extracted/metroid/rooms/runtime/%s/background.bmp",room_alias);
        room_path=native_room;
        background=native_background;
        native_format=true;
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
        n=snprintf(bundle_cannon,sizeof bundle_cannon,"%s/cannon_offsets.tsv",samus_assets);
        if(n<0 || (size_t)n>=sizeof bundle_cannon)return 2;
        cannon_index=bundle_cannon;
    } else if (room_alias && !library_index) {
        library_index="assets/extracted/metroid/sprites/samus/runtime/runtime_index.tsv";
        animation_map_index="assets/extracted/metroid/sprites/samus/runtime/animation_map.tsv";
        cannon_index="assets/extracted/metroid/sprites/samus/runtime/cannon_offsets.tsv";
        if(!projectile_index)
            projectile_index="assets/extracted/metroid/sprites/projectiles/runtime/runtime_index.tsv";
    }
    if((animation_map_index!=NULL) != (library_index!=NULL)) {
        fprintf(stderr,"--samus-library and --samus-map must be used together\n");
        return 2;
    }
    if(animation_check) {
        if(check || room_path || background || native_source ||
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
    if (!room_path || (check && (background || library_index || animation_map_index ||
                                 projectile_index))) {
        fprintf(stderr,"Usage: %s [--check] [--background image.bmp] [--native-source source.tsv] [--samus-assets directory | --samus-library index.tsv --samus-map map.tsv] preview.tsv\n",argv[0]);
        return 2;
    }
    if (!native_format) {
        /* Detect an exported native room passed by path. */
        FILE *probe=fopen(room_path,"rb");
        char header[16]={0};
        if (probe) {
            size_t read=fread(header,1,sizeof header-1,probe);
            (void)read;
            fclose(probe);
        }
        native_format=!strncmp(header,"MVROOM-NATIVE\t",14);
    }
    Room *room=calloc(1,sizeof *room);
    if (!room) return 1;
    if (!(native_format ? parse_native_room(room_path,room) : parse_room(room_path,room))) {
        if (native_format)
            fprintf(stderr,"Export it first: python3 -m scripts.mzm_runtime_room "
                    "--area <Area> --room <number>\n");
        free(room); return 2;
    }
    printf("Runtime room %s/%s/%d %dx%d: %zu %s collision cells\n",
           room->world,room->area,room->room,room->width,room->height,room->count,
           native_format?"native Clipdata":"project");
    size_t native_records=0, native_solids=0;
    if (native_source && !parse_native_source(native_source,room,&native_records,&native_solids)) {
        free(room); return 2;
    }
    if (native_source)
        printf("Native source: %zu records, %zu verified full-solid cells (Clipdata 16)\n",
               native_records,native_solids);
    if (check) {
        MzmSamus spawn;
        if (!runtime_spawn_samus(room, &spawn)) {
            fprintf(stderr, "No free runtime test spawn found\n");
            free(room); return 2;
        }
        float bx, by, bw, bh;
        mzm_samus_box(&spawn, &bx, &by, &bw, &bh);
        printf("Validated test spawn: x=%.0f y=%.0f, ground=%s\n",
               bx, by, blocked(room, bx, by + 1.f, bw, bh) ? "yes" : "no");
        free(room); return 0;
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) { fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());free(room);return 1; }
    SDL_Window *window=SDL_CreateWindow("Metroid Vania - experimental C11 runtime",
                                       960,640,SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer=window ? SDL_CreateRenderer(window,NULL) : NULL;
    SDL_Surface *surface=NULL; SDL_Texture *texture=NULL;
    int rc=1;
    RuntimeLibrary library = {0};
    RuntimeLibrary projectile_library = {0};
    RuntimeAnimationMap animation_map = {0};
    SDL_Gamepad *gamepad = NULL;
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
    if(library_index && !runtime_library_open(&library,library_index)){fprintf(stderr,"Runtime animation index failed to load.\n");goto cleanup;}
    if(animation_map_index && !runtime_animation_map_open(
            &animation_map,animation_map_index,&library)) {
        fprintf(stderr,"Runtime animation map failed to load.\n");goto cleanup;
    }
    if(animation_map_index)
        printf("Samus animation registry: %d native sequences, %d semantic bindings\n",
               library.count,animation_map.count);
    if(cannon_index && !runtime_cannon_offsets_open(&library,cannon_index))
        fprintf(stderr,"Arm cannon offsets unavailable (%s); projectiles spawn at "
                "Samus's position.\n",cannon_index);
    if(projectile_index) {
        if(!runtime_library_open(&projectile_library,projectile_index)) {
            fprintf(stderr,"Projectile library failed to load: %s\n",projectile_index);
            goto cleanup;
        }
        printf("Projectile library: %d native sequences\n",projectile_library.count);
    }
    gamepad=runtime_pad_open();
    MzmCollision collision={room,runtime_collision_blocked,runtime_collision_slope,
                            runtime_collision_point};
    MzmEquipment equipment={0};
    unsigned int suit_preset=0,spin_items=0;
    uint32_t toggled_items=0;
    runtime_apply_equipment(&equipment,suit_preset,toggled_items);
    equipment.max_energy=99;
    equipment.energy=equipment.max_energy;
    /* Diagnostic ammunition until item pickups exist. */
    equipment.max_missiles=10;
    equipment.missiles=equipment.max_missiles;
    equipment.max_super_missiles=2;
    equipment.super_missiles=equipment.max_super_missiles;
    MzmWeapons weapons;
    mzm_weapons_init(&weapons);
    MzmProjectileAnimation projectile_animation={&projectile_library,
                                                 runtime_projectile_durations};
    RuntimePoseAnimation pose_animation={&animation_map,
                                         runtime_suit_presets[0].registry,
                                         &equipment};
    MzmAnimationSource animation_source={&pose_animation,runtime_pose_durations};
    MzmSamus samus;
    if(!runtime_spawn_samus(room,&samus)) {
        fprintf(stderr,"No free avatar spawn found\n");
        goto cleanup;
    }
    printf("Selected safe test spawn: feet x=%.2f y=%.2f\n",
           (double)samus.x/MZM_SUBPIXELS_PER_PIXEL,
           (double)samus.y/MZM_SUBPIXELS_PER_PIXEL);
    RuntimeEcho echo={0};
    unsigned int echo_render_tick=~0u;
    bool echo_visible=false,show_hitbox=false;
    float echo_x=0.f,echo_y=0.f;
    bool animation_browser=false;
    int browser_index=0;
    Uint64 animation_start=SDL_GetTicks();
    MzmPose titled_pose=MZM_POSE_COUNT;
    int titled_energy=-1,titled_ammo=-1;
    bool title_dirty=true;
    printf("Controls: arrows/WASD = D-pad; Space/Z = A (jump); F/X = B (fire); "
           "E/Q = L diagonal aim up/down; Escape = exit.\n");
    printf("Native poses: Down crouches, Down again morphs, Up unmorphs/stands; "
           "jump toward a wall then away+A to wall-jump; hold toward a ledge "
           "while falling, then A+toward to climb.\n");
    printf("Weapons: B fires; hold V/Left Shift (GBA R) to arm missiles, Tab "
           "(Select) toggles super missiles; M refills ammunition.\n");
    printf("Diagnostics: R suit, T Space Jump/Screw Attack, G High Jump, "
           "H 20 damage, Enter restart, F6 catalogue, F7 hitbox.\n");
    Uint64 previous=SDL_GetTicks(); bool running=true;
    float accumulator=0.f;
    const float fixed_step=1.f/60.f;
    long capture_step=0;
    uint16_t previous_held=0,latched=0;
    bool damage_queued=false,restart_queued=false;
    bool pad_armor_prev=false,pad_special_prev=false,pad_browser_prev=false;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            bool key_down=event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat;
            /* Diagnostic animation browser: gameplay state and collisions unchanged. */
            if (library_index && library.count > 0) {
                bool pad_browser=event.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                    event.gbutton.button==SDL_GAMEPAD_BUTTON_START;
                bool toggle=(key_down && event.key.key==SDLK_F6) ||
                            (pad_browser && !pad_browser_prev);
                if(event.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
                   event.type==SDL_EVENT_GAMEPAD_BUTTON_UP)
                    pad_browser_prev=pad_browser;
                bool forward=(key_down && event.key.key==SDLK_PAGEDOWN) ||
                             (event.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                              event.gbutton.button==SDL_GAMEPAD_BUTTON_RIGHT_STICK);
                bool backward=(key_down && event.key.key==SDLK_PAGEUP) ||
                              (event.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                               event.gbutton.button==SDL_GAMEPAD_BUTTON_LEFT_STICK);
                if(toggle) {
                    animation_browser=!animation_browser;
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
                        title_dirty=true;
                        fprintf(stderr,"Animation browser disabled\n");
                    }
                }
            }
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) running=false;
            if (!key_down) continue;
            latched|=runtime_key_buttons(event.key.scancode);
            if (event.key.key == SDLK_H) damage_queued=true;
            if (event.key.key == SDLK_RETURN) restart_queued=true;
            if (event.key.key == SDLK_F7) show_hitbox=!show_hitbox;
            if (event.key.key == SDLK_M) {
                equipment.missiles=equipment.max_missiles;
                equipment.super_missiles=equipment.max_super_missiles;
            }
            if (event.key.key == SDLK_T) {
                spin_items=(spin_items+1u)%4u;
                toggled_items=(toggled_items&~(uint32_t)(MZM_ITEM_SPACE_JUMP|
                                                         MZM_ITEM_SCREW_ATTACK))|
                    ((spin_items&1u)?MZM_ITEM_SPACE_JUMP:0u)|
                    ((spin_items&2u)?MZM_ITEM_SCREW_ATTACK:0u);
                runtime_apply_equipment(&equipment,suit_preset,toggled_items);
                title_dirty=true;
            }
            if (event.key.key == SDLK_G) {
                toggled_items^=MZM_ITEM_HIGH_JUMP;
                runtime_apply_equipment(&equipment,suit_preset,toggled_items);
                title_dirty=true;
            }
            if (event.key.key == SDLK_R) {
                suit_preset=(suit_preset+1u)%RUNTIME_SUIT_PRESETS;
                runtime_apply_equipment(&equipment,suit_preset,toggled_items);
                pose_animation.suit=runtime_suit_presets[suit_preset].registry;
                title_dirty=true;
            }
        }
        if(gamepad && !SDL_GamepadConnected(gamepad)){SDL_CloseGamepad(gamepad);gamepad=NULL;}
        if(!gamepad)gamepad=runtime_pad_open();
        bool pad_armor=runtime_pad_button(gamepad,SDL_GAMEPAD_BUTTON_GUIDE);
        if(pad_armor && !pad_armor_prev) {
            suit_preset=(suit_preset+1u)%RUNTIME_SUIT_PRESETS;
            runtime_apply_equipment(&equipment,suit_preset,toggled_items);
            pose_animation.suit=runtime_suit_presets[suit_preset].registry;
            title_dirty=true;
        }
        pad_armor_prev=pad_armor;
        bool pad_special=runtime_pad_button(gamepad,SDL_GAMEPAD_BUTTON_NORTH);
        if(pad_special && !pad_special_prev) {
            spin_items=(spin_items+1u)%4u;
            toggled_items=(toggled_items&~(uint32_t)(MZM_ITEM_SPACE_JUMP|
                                                     MZM_ITEM_SCREW_ATTACK))|
                ((spin_items&1u)?MZM_ITEM_SPACE_JUMP:0u)|
                ((spin_items&2u)?MZM_ITEM_SCREW_ATTACK:0u);
            runtime_apply_equipment(&equipment,suit_preset,toggled_items);
            title_dirty=true;
        }
        pad_special_prev=pad_special;
        Uint64 current=SDL_GetTicks();
        float dt=clampf((float)(current-previous)/1000.f,0.f,0.05f);
        if(capture_path)dt=fixed_step;
        previous=current;
        const bool *keys=SDL_GetKeyboardState(NULL);
        accumulator += dt;
        while (accumulator >= fixed_step) {
            uint16_t held=runtime_held_buttons(keys,gamepad);
            if(capture_path) {
                /* Scripted GBA buttons; A and B are released for one frame
                 * every REPEAT frames so they are pressed again. */
                held=(uint16_t)capture_buttons;
                if(capture_repeat && capture_step%capture_repeat==capture_repeat-1)
                    held&=(uint16_t)~(MZM_KEY_A|MZM_KEY_B);
                capture_step++;
            }
            MzmInput input={held,(uint16_t)((held&~previous_held)|latched),false};
            latched=0;
            previous_held=held;
            if(restart_queued) {
                if(samus.pose==MZM_POSE_DYING) {
                    equipment.energy=equipment.max_energy;
                    if(!runtime_spawn_samus(room,&samus)) {
                        fprintf(stderr,"Cannot find a safe restart position.\n");
                        running=false;
                    }
                    echo=(RuntimeEcho){0};echo_visible=false;
                    damage_queued=false;
                    mzm_weapons_init(&weapons);
                    equipment.missiles=equipment.max_missiles;
                    equipment.super_missiles=equipment.max_super_missiles;
                    fprintf(stderr,"Diagnostic restart: Energy %d.\n",equipment.energy);
                }
                restart_queued=false;
            }
            if(damage_queued) {
                if(samus.pose!=MZM_POSE_DYING && samus.invincibility==0) {
                    bool alive=mzm_equipment_damage(&equipment,20);
                    mzm_samus_hurt(&samus,&collision,!alive);
                    echo.active=false;echo.timer=0;echo_visible=false;
                    fprintf(stderr,alive?"Diagnostic damage: Energy %d/%d.\n":
                            "Samus diagnostic energy reached zero (%d/%d).\n",
                            equipment.energy,equipment.max_energy);
                }
                damage_queued=false;
            }
            if(samus.pose==MZM_POSE_DYING) {
                input.held=0;
                input.pressed=0;
            }
            input.new_projectile=mzm_weapons_begin_frame(
                &weapons,&samus,input.held,input.pressed,&equipment);
            mzm_samus_update(&samus,&input,&equipment,&collision,&animation_source);
            /* ProjectileUpdate reads the arm cannon offset of the pose and
             * frame Samus has after her update. */
            int cannon_x=0,cannon_y=0;
            RuntimeAnimationMapRow *cannon_row=animation_map.count?runtime_pose_row(
                &animation_map,&samus,pose_animation.suit,equipment.items):NULL;
            if(cannon_row && cannon_row->entry->count>0) {
                int index=samus.anim_frame<cannon_row->entry->count?
                    samus.anim_frame:cannon_row->entry->count-1;
                if(cannon_row->entry->frames[index].has_cannon) {
                    cannon_x=cannon_row->entry->frames[index].cannon_x;
                    cannon_y=cannon_row->entry->frames[index].cannon_y;
                }
            }
            mzm_weapons_update(&weapons,&samus,&equipment,cannon_x,cannon_y,
                               &collision,&projectile_animation);
            if(samus.grabbed_ledge) {
                echo.active=false;
                echo.timer=0;
            }
            runtime_echo_step(&echo,(float)(samus.x>>2),(float)((samus.y-1)>>2),
                              runtime_echo_fast_ascent(&samus));
            accumulator-=fixed_step;
        }
        int ammo=equipment.missiles*1000+equipment.super_missiles*10+(int)weapons.highlighted;
        if(!animation_browser && (title_dirty || samus.pose!=titled_pose ||
                                  equipment.energy!=titled_energy || ammo!=titled_ammo)) {
            char title[256];
            snprintf(title,sizeof title,
                     "Metroid Vania [%s] [%s] [Energy %d/%d] [Missiles %d/%d] "
                     "[Supers %d/%d]%s%s%s%s",
                     runtime_suit_presets[suit_preset].name,
                     mzm_pose_name(samus.pose),equipment.energy,equipment.max_energy,
                     equipment.missiles,equipment.max_missiles,
                     equipment.super_missiles,equipment.max_super_missiles,
                     weapons.highlighted==MZM_WEAPON_MISSILE?" [missile armed]":
                     weapons.highlighted==MZM_WEAPON_SUPER_MISSILE?" [super armed]":"",
                     (equipment.items&MZM_ITEM_HIGH_JUMP)?" [High Jump]":"",
                     (equipment.items&MZM_ITEM_SPACE_JUMP)?" [Space Jump]":"",
                     (equipment.items&MZM_ITEM_SCREW_ATTACK)?" [Screw Attack]":"");
            SDL_SetWindowTitle(window,title);
            titled_pose=samus.pose;
            titled_energy=equipment.energy;
            titled_ammo=ammo;
            title_dirty=false;
        }
        int ow=0,oh=0;
        if (!SDL_GetRenderOutputSize(renderer,&ow,&oh) || ow<=0 || oh<=0) continue;
        float scale=(float)ow/320.f;
        if ((float)oh/224.f < scale) scale=(float)oh/224.f;
        SDL_FRect viewport={(ow-320.f*scale)*0.5f,(oh-224.f*scale)*0.5f,
                            320.f*scale,224.f*scale};
        float feet_x=(float)samus.x/MZM_SUBPIXELS_PER_PIXEL;
        float feet_y=(float)samus.y/MZM_SUBPIXELS_PER_PIXEL;
        /* Whole-pixel camera keeps native sprite pixels aligned. */
        float cx=(float)(int)clampf(feet_x-160.f,0.f,(float)(room->width>320?room->width-320:0));
        float cy=(float)(int)clampf(feet_y-16.f-112.f,0.f,(float)(room->height>224?room->height-224:0));
        SDL_SetRenderDrawColor(renderer,12,14,24,255);SDL_RenderClear(renderer);
        if (texture) {
            SDL_FRect src={cx,cy,320.f,224.f};
            if (src.w>room->width) src.w=(float)room->width;
            if (src.h>room->height) src.h=(float)room->height;
            SDL_FRect dst={viewport.x,viewport.y,src.w*scale,src.h*scale};
            SDL_RenderTexture(renderer,texture,&src,&dst);
        }
        RuntimeLibraryEntry *lib_entry=NULL;
        int frame=0;
        if(animation_browser && library.count>0) {
            lib_entry=&library.entries[browser_index];
            Uint64 elapsed_ms=SDL_GetTicks()-animation_start;
            frame=samus_timeline_frame(lib_entry->ticks,lib_entry->count,
                                       (unsigned int)(elapsed_ms*60u/1000u));
        } else if(animation_map.count>0) {
            RuntimeAnimationMapRow *row=runtime_pose_row(
                &animation_map,&samus,pose_animation.suit,equipment.items);
            lib_entry=row?row->entry:NULL;
            if(lib_entry && weapons.highlighted!=MZM_WEAPON_NONE) {
                /* SamusUpdateGraphicsOam: armed cannon graphics. */
                char armed[176];
                snprintf(armed,sizeof armed,"%s/armed",lib_entry->name);
                RuntimeLibraryEntry *armed_entry=runtime_library_find(&library,armed);
                if(armed_entry && armed_entry->count==lib_entry->count)
                    lib_entry=armed_entry;
            }
            /* The controller owns the native frame index and timer. */
            if(lib_entry && lib_entry->count>0)
                frame=samus.anim_frame<lib_entry->count?samus.anim_frame:lib_entry->count-1;
        }
        unsigned int render_tick=(unsigned int)(SDL_GetTicks()*60u/1000u);
        if(lib_entry && lib_entry->count>0 &&
           runtime_library_texture(renderer,lib_entry,frame)) {
            const RuntimeLibraryFrame *art=&lib_entry->frames[frame];
            SDL_Texture *sprite=art->texture;
            float sw=(float)art->w,sh=(float)art->h;
            if(animation_browser || !echo.active) {
                echo_visible=false;
            } else if(render_tick!=echo_render_tick) {
                echo_render_tick=render_tick;
                echo_visible=runtime_echo_sample(&echo,2u,&echo_x,&echo_y);
            }
            if(echo_visible) {
                /* Palette bank 1 is not exported yet; use a translucent
                 * violet modulation while retaining the native timing. */
                SDL_SetTextureColorMod(sprite,110,100,255);
                SDL_SetTextureAlphaMod(sprite,145);
                SDL_FRect echo_dest={viewport.x+(echo_x+(float)art->offset_x-cx)*scale,
                                     viewport.y+(echo_y+(float)art->offset_y-cy)*scale,
                                     sw*scale,sh*scale};
                SDL_RenderTexture(renderer,sprite,NULL,&echo_dest);
                SDL_SetTextureColorMod(sprite,255,255,255);
                SDL_SetTextureAlphaMod(sprite,255);
            }
            /* SamusDraw places OAM at the native pixel position; the
             * runtime stores feet on the block edge, one subpixel lower. */
            float anchor_x=(float)(samus.x>>2),anchor_y=(float)((samus.y-1)>>2);
            SDL_FRect dest={viewport.x+(anchor_x+(float)art->offset_x-cx)*scale,
                            viewport.y+(anchor_y+(float)art->offset_y-cy)*scale,
                            sw*scale,sh*scale};
            bool damage_flash=samus.pose!=MZM_POSE_DYING &&
                samus.invincibility>0u && (render_tick&3u)<=1u;
            if(damage_flash) SDL_SetTextureAlphaMod(sprite,90);
            SDL_RenderTexture(renderer,sprite,NULL,&dest);
            if(damage_flash) SDL_SetTextureAlphaMod(sprite,255);
        }
        for(int i=0;i<MZM_MAX_PROJECTILES && projectile_library.count>0;i++) {
            const MzmProjectile *projectile=&weapons.list[i];
            if(!projectile->active || projectile->stage==MZM_STAGE_INIT)continue;
            char key[160];
            runtime_projectile_key(projectile,key,sizeof key);
            RuntimeLibraryEntry *entry=runtime_library_find(&projectile_library,key);
            if(!entry || entry->count==0)continue;
            int index=projectile->anim_frame<entry->count?projectile->anim_frame:0;
            if(!runtime_library_texture(renderer,entry,index))continue;
            const RuntimeLibraryFrame *art=&entry->frames[index];
            SDL_FRect shot={viewport.x+((float)(projectile->x>>2)+(float)art->offset_x-cx)*scale,
                            viewport.y+((float)(projectile->y>>2)+(float)art->offset_y-cy)*scale,
                            (float)art->w*scale,(float)art->h*scale};
            SDL_RenderTexture(renderer,art->texture,NULL,&shot);
        }
        if(show_hitbox || !lib_entry) {
            float bx,by,bw,bh;
            mzm_samus_box(&samus,&bx,&by,&bw,&bh);
            SDL_FRect avatar={viewport.x+(bx-cx)*scale,viewport.y+(by-cy)*scale,
                              bw*scale,bh*scale};
            SDL_SetRenderDrawColor(renderer,80,205,115,255);
            if(lib_entry) SDL_RenderRect(renderer,&avatar);
            else SDL_RenderFillRect(renderer,&avatar);
        }
        if(capture_path && capture_step>=capture_frames) {
            SDL_Surface *shot=SDL_RenderReadPixels(renderer,NULL);
            bool saved=shot && SDL_SaveBMP(shot,capture_path);
            if(shot)SDL_DestroySurface(shot);
            if(!saved) {
                fprintf(stderr,"Capture failed: %s\n",SDL_GetError());
                goto cleanup;
            }
            printf("Captured frame %ld: %s (pose %s)\n",capture_step,capture_path,
                   mzm_pose_name(samus.pose));
            running=false;
        }
        SDL_RenderPresent(renderer);
        if(!capture_path)SDL_Delay(8);
    }
    rc=0;
cleanup:
    if(gamepad)SDL_CloseGamepad(gamepad);
    runtime_library_free(&library);
    runtime_library_free(&projectile_library);
    if (texture) SDL_DestroyTexture(texture);
    if (surface) SDL_DestroySurface(surface);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();free(room);return rc;
}
#endif /* FUSION_RUNTIME_TEST */
