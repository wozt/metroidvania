/* SPDX-License-Identifier: GPL-3.0-only */
/* Experimental C11/SDL3 Zero Mission room runtime. Samus, weapons and
 * collision types follow the pinned decompilation; rooms, entities and the
 * room lifecycle are still incomplete. */
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug_menu.h"
#include "gba_input.h"
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

#define MAX_ROOM_DOORS 32
#define MAX_ROOM_HATCHES 16
#define HATCH_VERTICAL_SIZE 4
/* Hatch weakness bits written by scripts/mzm_runtime_room.py. */
enum { DAMAGE_BEAM = 1, DAMAGE_BOMB_PISTOL = 2, DAMAGE_MISSILE = 4,
       DAMAGE_SUPER_MISSILE = 8, DAMAGE_POWER_BOMB = 16 };
/* Clip behaviors that start a room transition. */
enum { BEHAVIOR_NONE, BEHAVIOR_DOOR, BEHAVIOR_UP, BEHAVIOR_DOWN };

/* One sAreaDoors entry of the room with its destination's placement. */
typedef struct {
    int index;
    char kind[12];
    int x0, x1, y0, y1;
    char destination[48];
    int dest_x, dest_y_end, dest_x_exit, dest_y_exit;
} RoomDoor;
/* One hatch built like ConnectionLoadDoors. */
typedef struct {
    int door, x, y;
    char type[24];
    int weakness, health, hits;
    bool open;
} RoomHatch;

/* One native collision type and transition behavior per 16-pixel block. */
typedef struct { int width, height, resolution, room; char world[16], area[40];
    unsigned char types[MAX_ROOM_CELLS]; unsigned char behaviors[MAX_ROOM_CELLS];
    RoomDoor doors[MAX_ROOM_DOORS]; int door_count;
    RoomHatch hatches[MAX_ROOM_HATCHES]; int hatch_count;
    size_t count; } Room;

static void room_set_cell(Room *room, int x, int y, ClipType type) {
    int columns = room->width / 16;
    unsigned char *cell = &room->types[y * columns + x];
    if (*cell == CLIP_AIR && type != CLIP_AIR) room->count++;
    else if (*cell != CLIP_AIR && type == CLIP_AIR) room->count--;
    *cell = (unsigned char)type;
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
        char kind[16];
        if (line[0] == 'B') {
            if (sscanf(line, "B\t%d\t%d\t%15s %c", &x, &y, kind, &extra) != 3 ||
                x < 0 || y < 0 || x >= room->width / 16 || y >= room->height / 16)
                goto failure;
            room->behaviors[y * (room->width / 16) + x] = (unsigned char)(
                !strcmp(kind, "door") ? BEHAVIOR_DOOR : !strcmp(kind, "up") ? BEHAVIOR_UP :
                !strcmp(kind, "down") ? BEHAVIOR_DOWN : BEHAVIOR_NONE);
            continue;
        }
        if (line[0] == 'D') {
            RoomDoor door = {0};
            if (room->door_count >= MAX_ROOM_DOORS ||
                sscanf(line, "D\t%d\t%11[^\t]\t%d\t%d\t%d\t%d\t%47[^\t]\t%d\t%d\t%d\t%d %c",
                       &door.index, door.kind, &door.x0, &door.x1, &door.y0, &door.y1,
                       door.destination, &door.dest_x, &door.dest_y_end,
                       &door.dest_x_exit, &door.dest_y_exit, &extra) != 11 ||
                door.x0 < 0 || door.x1 < door.x0 || door.y0 < 0 || door.y1 < door.y0)
                goto failure;
            room->doors[room->door_count++] = door;
            continue;
        }
        if (line[0] == 'H') {
            RoomHatch hatch = {0};
            if (room->hatch_count >= MAX_ROOM_HATCHES ||
                sscanf(line, "H\t%d\t%d\t%d\t%23[^\t]\t%d\t%d %c", &hatch.door, &hatch.x,
                       &hatch.y, hatch.type, &hatch.weakness, &hatch.health, &extra) != 6 ||
                hatch.x < 0 || hatch.x >= room->width / 16 || hatch.y < 0 ||
                hatch.y >= room->height / 16 || hatch.health < 0)
                goto failure;
            room->hatches[room->hatch_count++] = hatch;
            continue;
        }
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

static int runtime_behavior(const Room *room, int x, int y) {
    return room->behaviors[y * (room->width / 16) + x];
}

/* BgClipCheckTouchingTransitionOrTank + ConnectionCheckEnterDoor: the door
 * Samus is entering, and gSamusDoorPositionOffset for the destination. */
static const RoomDoor *runtime_door_touched(const Room *room, const MzmSamus *samus,
                                            int *offset) {
    const MzmBlockHitbox *box = mzm_block_hitbox(mzm_pose_hitbox(samus->pose));
    int32_t native_y = samus->y - 1;
    int32_t max_x = room->width / 16 * MZM_BLOCK_SIZE;
    int32_t max_y = room->height / 16 * MZM_BLOCK_SIZE;
    int32_t raw_x[3] = {(box->right >> 2) + samus->x, (box->left >> 2) + samus->x, samus->x};
    int32_t raw_y[3] = {(box->top >> 2) + native_y, (box->top >> 4) + native_y,
                        (box->top >> 4) + (box->top >> 2) + native_y};
    int xs[3], ys[3];
    for (int i = 0; i < 3; ++i) {
        int32_t x = raw_x[i] < 0 ? 0 : raw_x[i] > max_x ? max_x : raw_x[i];
        int32_t y = raw_y[i] < 0 ? 0 : raw_y[i] > max_y ? max_y : raw_y[i];
        xs[i] = x / MZM_BLOCK_SIZE;
        ys[i] = y / MZM_BLOCK_SIZE;
        if (xs[i] >= room->width / 16) xs[i] = room->width / 16 - 1;
        if (ys[i] >= room->height / 16) ys[i] = room->height / 16 - 1;
    }
    /* sBlockTouchOffsets: (center, right), (center, left), (bottom, center),
     * (top, center). */
    static const int touch[4][2] = {{0, 0}, {0, 1}, {1, 2}, {2, 2}};
    int j = -1;
    if (runtime_behavior(room, xs[0], ys[0]) == BEHAVIOR_DOOR) j = 0;
    else if (runtime_behavior(room, xs[1], ys[0]) == BEHAVIOR_DOOR) j = 1;
    else if (runtime_behavior(room, xs[2], ys[1]) == BEHAVIOR_UP) j = 2;
    else if (runtime_behavior(room, xs[2], ys[2]) == BEHAVIOR_DOWN) j = 3;
    if (j < 0) return NULL;
    int px = xs[touch[j][1]], py = ys[touch[j][0]];
    for (int i = 0; i < room->door_count; ++i) {
        const RoomDoor *door = &room->doors[i];
        if (!strcmp(door->kind, "none") || !strcmp(door->kind, "area")) continue;
        if (door->x0 <= px && px <= door->x1 && door->y0 <= py && py <= door->y1) {
            *offset = (door->y1 + 1) * MZM_BLOCK_SIZE - native_y - 1;
            return door;
        }
    }
    return NULL;
}

/* RoomLoad placement: Samus at the destination door's exit, keeping her
 * vertical offset inside the door. */
static void runtime_place_after_door(MzmSamus *samus, const RoomDoor *door, int offset) {
    int32_t native_y = (door->dest_y_end + 1) * MZM_BLOCK_SIZE + door->dest_y_exit * 4 - 1;
    samus->x = door->dest_x * MZM_BLOCK_SIZE + (door->dest_x_exit + 8) * 4;
    if (offset < 0) {
        offset = 0;
    } else {
        int top = -mzm_block_hitbox(mzm_pose_hitbox(samus->pose))->top;
        if (top + offset > 255) offset = 255 - top;
    }
    samus->y = native_y - offset + 1;
}

/* BgClipCheckOpeningHatch for one projectile impact (subpixels). */
static bool runtime_hit_hatch(Room *room, int32_t x, int32_t y, int damage) {
    int bx = x / MZM_BLOCK_SIZE, by = y / MZM_BLOCK_SIZE;
    for (int i = 0; i < room->hatch_count; ++i) {
        RoomHatch *hatch = &room->hatches[i];
        if (hatch->open || hatch->x != bx || by < hatch->y ||
            by > hatch->y + HATCH_VERTICAL_SIZE - 1 || !(hatch->weakness & damage))
            continue;
        if (!strcmp(hatch->type, "locked") || !strcmp(hatch->type, "locked_navigation")) {
            hatch->hits = 0;  /* Locked until an event unlocks it. */
            return false;
        }
        hatch->hits++;
        if (!strcmp(hatch->type, "missile") && (damage & DAMAGE_SUPER_MISSILE))
            hatch->hits = hatch->health;
        if (hatch->hits >= hatch->health) {
            hatch->open = true;
            for (int row = 0; row < HATCH_VERTICAL_SIZE; ++row)
                if (hatch->y + row < room->height / 16)
                    room_set_cell(room, hatch->x, hatch->y + row, CLIP_AIR);
        }
        return true;
    }
    return false;
}

static void runtime_collision_affect(void *context,int32_t x,int32_t y,int damage) {
    runtime_hit_hatch((Room *)context,x,y,damage);
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

/* Samus palette arrays exported by the Samus pipeline (palettes.tsv). */
#define RUNTIME_PALETTE_ROWS_MAX 256
typedef struct {
    char suit[16], kind[24];
    int row;
    SDL_Color colors[16];
} RuntimePaletteRow;
typedef struct {
    RuntimePaletteRow rows[RUNTIME_PALETTE_ROWS_MAX];
    int count;
} RuntimePalettes;

static bool runtime_palettes_open(RuntimePalettes *palettes,const char *path) {
    FILE *f=fopen(path,"rb");
    if(!f)return false;
    char line[512];
    bool ok=fgets(line,sizeof line,f) &&
        !strcmp(line,"schema\tmetroidvania-samus-palettes-v1\n");
    palettes->count=0;
    while(ok && fgets(line,sizeof line,f)) {
        RuntimePaletteRow row={0};
        int consumed=0;
        if(palettes->count>=RUNTIME_PALETTE_ROWS_MAX ||
           sscanf(line,"%15[^\t]\t%23[^\t]\t%d%n",row.suit,row.kind,&row.row,
                  &consumed)!=3 || row.row<0 || row.row>15){ok=false;break;}
        const char *cursor=line+consumed;
        for(int i=0;i<16 && ok;i++) {
            unsigned int rgb;
            int used=0;
            if(sscanf(cursor,"\t%6x%n",&rgb,&used)!=1 || used!=7)ok=false;
            else {
                row.colors[i]=(SDL_Color){(Uint8)(rgb>>16),(Uint8)(rgb>>8),(Uint8)rgb,255};
                cursor+=used;
            }
        }
        if(ok && *cursor!='\n' && *cursor!='\0')ok=false;
        if(ok)palettes->rows[palettes->count++]=row;
    }
    if(ferror(f))ok=false;
    fclose(f);
    return ok && palettes->count>0;
}

static const RuntimePaletteRow *runtime_palette_row(const RuntimePalettes *palettes,
        const char *suit,const char *kind,int row) {
    for(int i=0;i<palettes->count;i++) {
        const RuntimePaletteRow *entry=&palettes->rows[i];
        if(entry->row==row && !strcmp(entry->suit,suit) && !strcmp(entry->kind,kind))
            return entry;
    }
    return NULL;
}

/* The two OBJ palette rows SamusUpdatePalette loads for Samus. */
typedef struct {
    const char *suit0,*kind0; int row0;
    const char *suit1,*kind1; int row1;
} RuntimePaletteChoice;

static RuntimePaletteChoice runtime_samus_palette(const MzmSamus *samus,bool beam_release,
        const char *suit,unsigned int frame_counter) {
    /* Suitless Samus borrows the Power Suit speed boost and unmorph rows. */
    const char *borrowed=strcmp(suit,"Suitless")?suit:"PowerSuit";
    RuntimePaletteChoice choice={suit,"Default",0,suit,"Default",1};
    if(samus->pose==MZM_POSE_DYING) {
        /* Row 0 is always the Power Suit dying row; the source then indexes
         * past it into sSamusPal_Generic_Dying, which directly follows. */
        int frame=samus->anim_frame;
        choice=(RuntimePaletteChoice){"PowerSuit","Dying",0,"Generic","Dying",0};
        if(frame==11 || frame==15) choice.row1=1;
        else if(frame==12 || frame==14) choice.row1=3;
        else if(frame==13) choice.row1=5;
        else if(frame<=10) {
            choice.suit1=strcmp(suit,"Suitless")?suit:"Generic";
            choice.row1=0;
        }
        return choice;
    }
    if(samus->invincibility) {
        choice.kind0="Flashing";
        choice.row0=(frame_counter&3u)<=1u?0:1;
    } else if(samus->pose==MZM_POSE_SCREW_ATTACKING) {
        if(samus->anim_frame&1u){choice.kind0="Flashing";choice.row0=1;}
    } else if(beam_release) {
        choice.kind0="BeamRelease";
    } else if(samus->unmorph_palette_timer) {
        choice.suit0=borrowed;
        choice.kind0="Unmorph";
        choice.row0=(uint8_t)(samus->unmorph_palette_timer-5)>4?0:1;
    }
    return choice;
}

/* 32 OBJ colors for a choice; ``echo`` draws every part with bank 1. */
static bool runtime_palette_colors(const RuntimePalettes *palettes,
        const RuntimePaletteChoice *choice,bool echo,SDL_Color colors[32]) {
    const RuntimePaletteRow *row0=runtime_palette_row(palettes,choice->suit0,
                                                      choice->kind0,choice->row0);
    const RuntimePaletteRow *row1=runtime_palette_row(palettes,choice->suit1,
                                                      choice->kind1,choice->row1);
    if(!row0 || !row1)return false;
    memcpy(colors,echo?row1->colors:row0->colors,sizeof row0->colors);
    memcpy(colors+16,row1->colors,sizeof row1->colors);
    return true;
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
#define RUNTIME_PALETTE_SLOTS 16
typedef struct {
    unsigned int ticks;
    int offset_x,offset_y;
    bool has_cannon;
    int cannon_x,cannon_y;
    char path[256];
    SDL_Texture *texture;            /* 32-bit frames */
    SDL_Surface *indexed;            /* palette-indexed frames */
    SDL_Texture *variants[RUNTIME_PALETTE_SLOTS];
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
        for(int j=0;j<entry->count;j++) {
            RuntimeLibraryFrame *frame=&entry->frames[j];
            if(frame->texture) SDL_DestroyTexture(frame->texture);
            for(int k=0;k<RUNTIME_PALETTE_SLOTS;k++)
                if(frame->variants[k]) SDL_DestroyTexture(frame->variants[k]);
            if(frame->indexed) SDL_DestroySurface(frame->indexed);
        }
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
/* Texture of one frame; indexed frames are colorized with ``colors`` and
 * cached in ``slot`` (pass slot -1 and NULL colors for 32-bit libraries). */
static SDL_Texture *runtime_library_texture(SDL_Renderer *r,RuntimeLibraryEntry *entry,
                                            int index,int slot,const SDL_Color *colors) {
    RuntimeLibraryFrame *frame=&entry->frames[index];
    if(!frame->texture && !frame->indexed) {
        SDL_Surface *s=SDL_LoadBMP(frame->path);
        if(!s)return NULL;
        if(s->w<1||s->h<1||s->w>512||s->h>512){SDL_DestroySurface(s);return NULL;}
        frame->w=s->w;frame->h=s->h;
        if(s->format==SDL_PIXELFORMAT_INDEX8) {
            SDL_SetSurfaceColorKey(s,true,0);
            frame->indexed=s;
        } else {
            frame->texture=SDL_CreateTextureFromSurface(r,s);
            SDL_DestroySurface(s);
            if(!frame->texture)return NULL;
            SDL_SetTextureScaleMode(frame->texture,SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(frame->texture,SDL_BLENDMODE_BLEND);
        }
    }
    if(frame->texture)return frame->texture;
    if(slot<0 || slot>=RUNTIME_PALETTE_SLOTS || !colors)return NULL;
    if(!frame->variants[slot]) {
        SDL_Palette *palette=SDL_GetSurfacePalette(frame->indexed);
        if(!palette || !SDL_SetPaletteColors(palette,colors,0,32))return NULL;
        SDL_Texture *t=SDL_CreateTextureFromSurface(r,frame->indexed);
        if(!t)return NULL;
        SDL_SetTextureScaleMode(t,SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);
        frame->variants[slot]=t;
    }
    return frame->variants[slot];
}

/* Palette variants in use: each slot caches one 32-color combination. */
typedef struct {
    SDL_Color colors[RUNTIME_PALETTE_SLOTS][32];
    int count;
} RuntimePaletteSlots;
static void runtime_library_flush_variants(RuntimeLibrary *lib) {
    for(int i=0;i<lib->count;i++)
        for(int j=0;j<lib->entries[i].count;j++)
            for(int k=0;k<RUNTIME_PALETTE_SLOTS;k++)
                if(lib->entries[i].frames[j].variants[k]) {
                    SDL_DestroyTexture(lib->entries[i].frames[j].variants[k]);
                    lib->entries[i].frames[j].variants[k]=NULL;
                }
}
static int runtime_palette_slot(RuntimePaletteSlots *slots,RuntimeLibrary *lib,
                                const SDL_Color colors[32]) {
    for(int i=0;i<slots->count;i++)
        if(!memcmp(slots->colors[i],colors,sizeof slots->colors[i]))return i;
    if(slots->count==RUNTIME_PALETTE_SLOTS) {
        runtime_library_flush_variants(lib);
        slots->count=0;
    }
    memcpy(slots->colors[slots->count],colors,sizeof slots->colors[0]);
    return slots->count++;
}
/* Debug menu (F1, or the gamepad chord): every item edits the running
 * engine's state; nothing here exists in the original game. */
enum {
    DEBUG_PAUSE=1,DEBUG_STEP,DEBUG_HITBOX,DEBUG_SUIT,DEBUG_ITEMS,DEBUG_ENERGY,
    DEBUG_MAX_ENERGY,DEBUG_AMMO,DEBUG_REFILL,DEBUG_DAMAGE,DEBUG_AREA,DEBUG_ROOM,
    DEBUG_TELEPORT,
};
static const char *const debug_area_names[]={
    "brinstar","kraid","norfair","ridley","tourian","crateria","chozodia",
};
#define DEBUG_AREAS (int)(sizeof debug_area_names/sizeof debug_area_names[0])
/* Items whose native effect the engine implements (Speed Booster is not). */
static const struct { const char *label; uint32_t item; } debug_items[]={
    {"High Jump",MZM_ITEM_HIGH_JUMP},{"Space Jump",MZM_ITEM_SPACE_JUMP},
    {"Screw Attack",MZM_ITEM_SCREW_ATTACK},
};
#define DEBUG_ITEMS_COUNT (int)(sizeof debug_items/sizeof debug_items[0])

static void debug_draw(SDL_Renderer *renderer,const DebugMenu *menu,bool paused,
                       SDL_FRect viewport,float scale,const char *room_name,
                       const MzmSamus *samus,const MzmEquipment *equipment,long frame) {
    if(!menu->open && !paused)return;
    float text_scale=scale>=1.f?(float)(int)scale:1.f;
    float x=viewport.x/text_scale,y=viewport.y/text_scale;
    SDL_SetRenderScale(renderer,text_scale,text_scale);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,0,0,0,190);
    SDL_FRect panel={x,y,viewport.w/text_scale,menu->open?viewport.h/text_scale:12.f};
    SDL_RenderFillRect(renderer,&panel);
    SDL_SetRenderDrawColor(renderer,255,255,255,255);
    char line[64];
    if(!menu->open) {
        SDL_RenderDebugText(renderer,x+2.f,y+2.f,"PAUSED  F2 step  F1 menu");
    } else {
        snprintf(line,sizeof line,"DEBUG %s frame %ld",room_name,frame);
        SDL_RenderDebugText(renderer,x+2.f,y+2.f,line);
        snprintf(line,sizeof line,"x%d y%d %s",samus->x/MZM_SUBPIXELS_PER_PIXEL,
                 samus->y/MZM_SUBPIXELS_PER_PIXEL,mzm_pose_name(samus->pose));
        SDL_RenderDebugText(renderer,x+2.f,y+10.f,line);
        snprintf(line,sizeof line,"Energy %d/%d missiles %d supers %d",equipment->energy,
                 equipment->max_energy,equipment->missiles,equipment->super_missiles);
        SDL_RenderDebugText(renderer,x+2.f,y+18.f,line);
        enum { ROWS=22 };
        int top=menu->cursor-ROWS/2;
        if(top>menu->count-ROWS)top=menu->count-ROWS;
        if(top<0)top=0;
        for(int i=0;i<ROWS && top+i<menu->count;i++) {
            debug_menu_line(menu,top+i,line,sizeof line);
            SDL_RenderDebugText(renderer,x+2.f,y+34.f+8.f*(float)i,line);
        }
    }
    SDL_SetRenderScale(renderer,1.f,1.f);
}
/* Load an exported native room and its background texture. */
static bool runtime_load_room(const char *alias,Room *room,SDL_Renderer *renderer,
                              SDL_Texture **texture) {
    char path[256],background[256];
    if(!alias[0] || !strcmp(alias,"-"))return false;
    snprintf(path,sizeof path,"assets/extracted/metroid/rooms/runtime/%s/room.tsv",alias);
    snprintf(background,sizeof background,
             "assets/extracted/metroid/rooms/runtime/%s/background.bmp",alias);
    if(!parse_native_room(path,room))return false;
    SDL_Surface *surface=SDL_LoadBMP(background);
    if(!surface)return false;
    bool ok=surface->w==room->width && surface->h==room->height;
    *texture=ok?SDL_CreateTextureFromSurface(renderer,surface):NULL;
    SDL_DestroySurface(surface);
    if(!*texture)return false;
    SDL_SetTextureScaleMode(*texture,SDL_SCALEMODE_NEAREST);
    return true;
}
/* Gamepads come from the shared gba_input module (hot-plug, remappable);
 * its GBA KEYINPUT mask is converted to the runtime's key bits. */
static uint16_t runtime_gba_buttons(uint16_t gba) {
    static const struct { uint16_t gba, mzm; } keys[] = {
        {GBA_KEY_RIGHT, MZM_KEY_RIGHT}, {GBA_KEY_LEFT, MZM_KEY_LEFT},
        {GBA_KEY_UP, MZM_KEY_UP}, {GBA_KEY_DOWN, MZM_KEY_DOWN},
        {GBA_KEY_A, MZM_KEY_A}, {GBA_KEY_B, MZM_KEY_B}, {GBA_KEY_L, MZM_KEY_L},
        {GBA_KEY_R, MZM_KEY_R}, {GBA_KEY_SELECT, MZM_KEY_SELECT},
    };
    uint16_t buttons=0;
    for(size_t i=0;i<sizeof keys/sizeof keys[0];i++)
        if(gba&keys[i].gba)buttons|=keys[i].mzm;
    return buttons;
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
static uint16_t runtime_held_buttons(const bool *keys,const GbaInput *pad) {
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
    buttons|=runtime_gba_buttons(gba_input_buttons(pad));
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
    const char *projectile_index=NULL, *cannon_index=NULL, *palette_index=NULL;
    const char *room_alias=NULL, *samus_assets=NULL;
    const char *library_check=NULL,*capture_path=NULL;
    long capture_frames=0,capture_repeat=0;
    unsigned long capture_buttons=0;
    bool check=false,animation_check=false,debug_at_start=false;
    const char *debug_input=NULL;
    GbaPadMap pad_map;
    gba_pad_map_default(&pad_map);
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
        else if (!strcmp(argv[i],"--debug-menu")) debug_at_start=true;
        else if (!strcmp(argv[i],"--debug-input") && i+1<argc) debug_input=argv[++i];
        else if (!strcmp(argv[i],"--input-map") && i+1<argc) {
            char error[256];
            if(!gba_pad_map_load(&pad_map,argv[++i],error,sizeof error)) {
                fprintf(stderr,"Input map: %s\n",error);
                return 2;
            }
        }
        else if (argv[i][0]=='-' || room_path) {
            fprintf(stderr,"Usage: %s [--check] [--background image.bmp] [--native-source source.tsv] [--samus-assets directory | --samus-library index.tsv --samus-map map.tsv] [--input-map map.txt] [--debug-menu] [--debug-input MASK,...] preview.tsv\n",argv[0]);
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
    char bundle_index[4096],bundle_map[4096],bundle_cannon[4096],bundle_palettes[4096];
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
        n=snprintf(bundle_palettes,sizeof bundle_palettes,"%s/palettes.tsv",samus_assets);
        if(n<0 || (size_t)n>=sizeof bundle_palettes)return 2;
        palette_index=bundle_palettes;
    } else if (room_alias && !library_index) {
        library_index="assets/extracted/metroid/sprites/samus/runtime/runtime_index.tsv";
        animation_map_index="assets/extracted/metroid/sprites/samus/runtime/animation_map.tsv";
        cannon_index="assets/extracted/metroid/sprites/samus/runtime/cannon_offsets.tsv";
        palette_index="assets/extracted/metroid/sprites/samus/runtime/palettes.tsv";
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
        fprintf(stderr,"Usage: %s [--check] [--background image.bmp] [--native-source source.tsv] [--samus-assets directory | --samus-library index.tsv --samus-map map.tsv] [--input-map map.txt] [--debug-menu] [--debug-input MASK,...] preview.tsv\n",argv[0]);
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
    static RuntimePalettes palettes;
    RuntimePaletteSlots palette_slots={0};
    const char *palette_suit=NULL;
    unsigned int palette_frame=0;
    RuntimeAnimationMap animation_map = {0};
    GbaInput gamepad = {0};
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
    if(palette_index && !runtime_palettes_open(&palettes,palette_index)) {
        fprintf(stderr,"Samus palettes failed to load: %s\n",palette_index);
        goto cleanup;
    }
    if(projectile_index) {
        if(!runtime_library_open(&projectile_library,projectile_index)) {
            fprintf(stderr,"Projectile library failed to load: %s\n",projectile_index);
            goto cleanup;
        }
        printf("Projectile library: %d native sequences\n",projectile_library.count);
    }
    gba_input_init(&gamepad,&pad_map);
    MzmCollision collision={room,runtime_collision_blocked,runtime_collision_slope,
                            runtime_collision_point,runtime_collision_affect};
    bool door_lock=false;
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
    /* The debug menu's view of the engine state. */
    static DebugMenu debug;
    bool paused=false,step_once=false,chord_before=false;
    bool debug_item_flags[DEBUG_ITEMS_COUNT]={false};
    int debug_suit=(int)suit_preset,debug_area=0,debug_room=0;
    long debug_frame=0;
    const char *debug_suit_names[RUNTIME_SUIT_PRESETS];
    for(size_t i=0;i<RUNTIME_SUIT_PRESETS;i++)debug_suit_names[i]=runtime_suit_presets[i].name;
    char current_room[64];
    snprintf(current_room,sizeof current_room,"%s",room_alias?room_alias:"room");
    for(int i=0;i<DEBUG_AREAS;i++) {
        size_t length=strlen(debug_area_names[i]);
        if(!strncmp(current_room,debug_area_names[i],length) && current_room[length]=='_') {
            debug_area=i;
            debug_room=atoi(current_room+length+1);
        }
    }
    debug_menu_clear(&debug);
    debug_menu_toggle(&debug,"Paused",&paused,DEBUG_PAUSE);
    debug_menu_action(&debug,"Step one frame",DEBUG_STEP);
    debug_menu_toggle(&debug,"Hitbox",&show_hitbox,DEBUG_HITBOX);
    debug_menu_value(&debug,"Suit",&debug_suit,0,(int)RUNTIME_SUIT_PRESETS-1,1,
                     debug_suit_names,DEBUG_SUIT);
    for(int i=0;i<DEBUG_ITEMS_COUNT;i++)
        debug_menu_toggle(&debug,debug_items[i].label,&debug_item_flags[i],DEBUG_ITEMS);
    debug_menu_value(&debug,"Energy",&equipment.energy,0,2099,1,NULL,DEBUG_ENERGY);
    debug_menu_value(&debug,"Max energy",&equipment.max_energy,1,2099,1,NULL,DEBUG_MAX_ENERGY);
    debug_menu_value(&debug,"Missiles",&equipment.missiles,0,255,1,NULL,DEBUG_AMMO);
    debug_menu_value(&debug,"Max missiles",&equipment.max_missiles,0,255,1,NULL,DEBUG_AMMO);
    debug_menu_value(&debug,"Supers",&equipment.super_missiles,0,99,1,NULL,DEBUG_AMMO);
    debug_menu_value(&debug,"Max supers",&equipment.max_super_missiles,0,99,1,NULL,DEBUG_AMMO);
    debug_menu_action(&debug,"Refill energy, ammo",DEBUG_REFILL);
    debug_menu_action(&debug,"Take 20 damage",DEBUG_DAMAGE);
    debug_menu_value(&debug,"Area",&debug_area,0,DEBUG_AREAS-1,1,debug_area_names,DEBUG_AREA);
    debug_menu_value(&debug,"Room",&debug_room,0,255,1,NULL,DEBUG_ROOM);
    debug_menu_action(&debug,"Teleport",DEBUG_TELEPORT);
    debug.open=debug_at_start;
    uint16_t menu_previous=0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            gba_input_handle_event(&gamepad,&event);
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
            if (event.key.key == SDLK_F1) { debug.open=!debug.open; continue; }
            if (event.key.key == SDLK_F2) { if(paused)step_once=true; continue; }
            if (debug.open) continue;
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
        bool pad_armor=gba_input_button(&gamepad,SDL_GAMEPAD_BUTTON_GUIDE);
        if(pad_armor && !pad_armor_prev) {
            suit_preset=(suit_preset+1u)%RUNTIME_SUIT_PRESETS;
            runtime_apply_equipment(&equipment,suit_preset,toggled_items);
            pose_animation.suit=runtime_suit_presets[suit_preset].registry;
            title_dirty=true;
        }
        pad_armor_prev=pad_armor;
        bool pad_special=gba_input_button(&gamepad,SDL_GAMEPAD_BUTTON_NORTH);
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
        bool chord=gba_input_debug_chord(&gamepad);
        if(chord && !chord_before)debug.open=!debug.open;
        chord_before=chord;
        if(debug.open) {
            /* Mirror the engine before reading the menu's input. */
            debug_suit=(int)suit_preset;
            for(int i=0;i<DEBUG_ITEMS_COUNT;i++)
                debug_item_flags[i]=(toggled_items&debug_items[i].item)!=0;
            const bool *menu_keys=SDL_GetKeyboardState(NULL);
            uint16_t menu_held=chord?0:runtime_held_buttons(menu_keys,&gamepad),pressed;
            if(capture_path) {
                /* Scripted menu edges: the next mask of --debug-input (MZM
                 * key bits). */
                char *end=NULL;
                pressed=debug_input && *debug_input?(uint16_t)strtoul(debug_input,&end,0):0;
                if(end)debug_input=*end==','?end+1:end;
            } else {
                pressed=(uint16_t)(menu_held&~menu_previous);
            }
            /* The menu reads GBA KEYINPUT bits. */
            uint16_t gba=0;
            if(pressed&MZM_KEY_A)gba|=GBA_KEY_A;
            if(pressed&MZM_KEY_B)gba|=GBA_KEY_B;
            if(pressed&MZM_KEY_UP)gba|=GBA_KEY_UP;
            if(pressed&MZM_KEY_DOWN)gba|=GBA_KEY_DOWN;
            if(pressed&MZM_KEY_LEFT)gba|=GBA_KEY_LEFT;
            if(pressed&MZM_KEY_RIGHT)gba|=GBA_KEY_RIGHT;
            if(pressed&MZM_KEY_L)gba|=GBA_KEY_L;
            if(pressed&MZM_KEY_R)gba|=GBA_KEY_R;
            switch(debug_menu_input(&debug,gba)) {
            case DEBUG_STEP: step_once=true; break;
            case DEBUG_SUIT:
                suit_preset=(unsigned int)debug_suit;
                runtime_apply_equipment(&equipment,suit_preset,toggled_items);
                pose_animation.suit=runtime_suit_presets[suit_preset].registry;
                title_dirty=true;
                break;
            case DEBUG_ITEMS:
                for(int i=0;i<DEBUG_ITEMS_COUNT;i++) {
                    if(debug_item_flags[i])toggled_items|=debug_items[i].item;
                    else toggled_items&=~debug_items[i].item;
                }
                spin_items=((toggled_items&MZM_ITEM_SPACE_JUMP)?1u:0u)|
                           ((toggled_items&MZM_ITEM_SCREW_ATTACK)?2u:0u);
                runtime_apply_equipment(&equipment,suit_preset,toggled_items);
                title_dirty=true;
                break;
            case DEBUG_ENERGY: case DEBUG_MAX_ENERGY: case DEBUG_AMMO:
                if(equipment.energy>equipment.max_energy)equipment.energy=equipment.max_energy;
                if(equipment.missiles>equipment.max_missiles)
                    equipment.missiles=equipment.max_missiles;
                if(equipment.super_missiles>equipment.max_super_missiles)
                    equipment.super_missiles=equipment.max_super_missiles;
                title_dirty=true;
                break;
            case DEBUG_REFILL:
                equipment.energy=equipment.max_energy;
                equipment.missiles=equipment.max_missiles;
                equipment.super_missiles=equipment.max_super_missiles;
                title_dirty=true;
                break;
            case DEBUG_DAMAGE: damage_queued=true; step_once=true; break;
            case DEBUG_TELEPORT: {
                char alias[64];
                snprintf(alias,sizeof alias,"%s_%03d",debug_area_names[debug_area],debug_room);
                Room *next=calloc(1,sizeof *next);
                SDL_Texture *next_texture=NULL;
                MzmSamus placed=samus;
                if(next && runtime_load_room(alias,next,renderer,&next_texture) &&
                   runtime_spawn_samus(next,&placed)) {
                    free(room);
                    room=next;
                    collision.context=room;
                    if(texture)SDL_DestroyTexture(texture);
                    texture=next_texture;
                    samus=placed;
                    mzm_weapons_init(&weapons);
                    echo=(RuntimeEcho){0};
                    echo_visible=false;
                    door_lock=true;
                    title_dirty=true;
                    snprintf(current_room,sizeof current_room,"%s",alias);
                    printf("Debug teleport: %s\n",alias);
                } else {
                    if(next_texture)SDL_DestroyTexture(next_texture);
                    free(next);
                    fprintf(stderr,"Debug teleport: %s is not exported or has no spawn\n",alias);
                }
                break;
            }
            default: break;
            }
            menu_previous=menu_held;
        } else {
            menu_previous=0xFFFF;   /* no stray edge when the menu opens */
        }
        Uint64 current=SDL_GetTicks();
        float dt=clampf((float)(current-previous)/1000.f,0.f,0.05f);
        if(capture_path)dt=fixed_step;
        previous=current;
        const bool *keys=SDL_GetKeyboardState(NULL);
        accumulator += dt;
        /* The menu and the pause freeze the game (not in captures). */
        if((debug.open || paused) && !capture_path) {
            accumulator=step_once?fixed_step:0.f;
            step_once=false;
        }
        while (accumulator >= fixed_step) {
            debug_frame++;
            uint16_t held=runtime_held_buttons(keys,&gamepad);
            if(capture_path) {
                /* Scripted GBA buttons; A and B are released for one frame
                 * every REPEAT frames so they are pressed again. */
                held=(uint16_t)capture_buttons;
                if(capture_repeat && capture_step%capture_repeat==capture_repeat-1)
                    held&=(uint16_t)~(MZM_KEY_A|MZM_KEY_B);
                capture_step++;
            }
            if(debug.open && !capture_path)held=0;     /* a menu frame step */
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
            palette_frame++;
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
            /* Room lifecycle: a touched door transition loads the exported
             * destination room and places Samus at its exit. */
            int door_offset=0;
            const RoomDoor *door=samus.pose==MZM_POSE_DYING?NULL:
                runtime_door_touched(room,&samus,&door_offset);
            if(!door) {
                door_lock=false;
            } else if(!door_lock) {
                RoomDoor used=*door;
                Room *next=calloc(1,sizeof *next);
                SDL_Texture *next_texture=NULL;
                if(next && runtime_load_room(used.destination,next,renderer,&next_texture)) {
                    free(room);
                    room=next;
                    collision.context=room;
                    if(texture)SDL_DestroyTexture(texture);
                    texture=next_texture;
                    runtime_place_after_door(&samus,&used,door_offset);
                    mzm_weapons_init(&weapons);
                    echo=(RuntimeEcho){0};
                    echo_visible=false;
                    title_dirty=true;
                    snprintf(current_room,sizeof current_room,"%s",used.destination);
                    printf("Door %d -> %s\n",used.index,used.destination);
                } else {
                    free(next);
                    fprintf(stderr,"Door %d leads to %s, which is not exported or failed "
                            "to load.\n",used.index,used.destination);
                }
                door_lock=true;
            }
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
        if(palette_suit!=pose_animation.suit) {
            /* A suit change replaces every palette row. */
            runtime_library_flush_variants(&library);
            palette_slots.count=0;
            palette_suit=pose_animation.suit;
        }
        /* SamusUpdatePalette: the rows for this frame, and bank 1 for echoes. */
        RuntimePaletteChoice palette_choice=animation_browser?
            (RuntimePaletteChoice){pose_animation.suit,"Default",0,pose_animation.suit,"Default",1}:
            runtime_samus_palette(&samus,weapons.release_palette_timer>0,
                                  pose_animation.suit,palette_frame);
        SDL_Color body_colors[32],echo_colors[32];
        bool have_palette=palettes.count>0 &&
            runtime_palette_colors(&palettes,&palette_choice,false,body_colors) &&
            runtime_palette_colors(&palettes,&palette_choice,true,echo_colors);
        int body_slot=have_palette?runtime_palette_slot(&palette_slots,&library,body_colors):-1;
        SDL_Texture *sprite=lib_entry && lib_entry->count>0?
            runtime_library_texture(renderer,lib_entry,frame,body_slot,
                                    have_palette?body_colors:NULL):NULL;
        if(sprite) {
            const RuntimeLibraryFrame *art=&lib_entry->frames[frame];
            float sw=(float)art->w,sh=(float)art->h;
            if(animation_browser || !echo.active) {
                echo_visible=false;
            } else if(render_tick!=echo_render_tick) {
                echo_render_tick=render_tick;
                echo_visible=runtime_echo_sample(&echo,2u,&echo_x,&echo_y);
            }
            if(echo_visible && have_palette) {
                int echo_slot=runtime_palette_slot(&palette_slots,&library,echo_colors);
                /* The slot table may have been flushed; refetch both. */
                SDL_Texture *echo_sprite=runtime_library_texture(
                    renderer,lib_entry,frame,echo_slot,echo_colors);
                body_slot=runtime_palette_slot(&palette_slots,&library,body_colors);
                sprite=runtime_library_texture(renderer,lib_entry,frame,body_slot,body_colors);
                SDL_FRect echo_dest={viewport.x+(echo_x+(float)art->offset_x-cx)*scale,
                                     viewport.y+(echo_y+(float)art->offset_y-cy)*scale,
                                     sw*scale,sh*scale};
                if(echo_sprite)SDL_RenderTexture(renderer,echo_sprite,NULL,&echo_dest);
            }
            /* SamusDraw places OAM at the native pixel position; the
             * runtime stores feet on the block edge, one subpixel lower. */
            float anchor_x=(float)(samus.x>>2),anchor_y=(float)((samus.y-1)>>2);
            SDL_FRect dest={viewport.x+(anchor_x+(float)art->offset_x-cx)*scale,
                            viewport.y+(anchor_y+(float)art->offset_y-cy)*scale,
                            sw*scale,sh*scale};
            if(sprite)SDL_RenderTexture(renderer,sprite,NULL,&dest);
        }
        /* Hatch graphics use common tiles the partial room render cannot draw
         * yet; closed hatches are outlined with a diagnostic tint. */
        SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
        for(int i=0;i<room->hatch_count;i++) {
            const RoomHatch *hatch=&room->hatches[i];
            if(hatch->open)continue;
            Uint8 r=70,g=140,b=255;
            if(!strcmp(hatch->type,"missile")){r=230;g=60;b=60;}
            else if(!strcmp(hatch->type,"super_missile")){r=60;g=200;b=90;}
            else if(!strcmp(hatch->type,"power_bomb")){r=240;g=200;b=40;}
            else if(strncmp(hatch->type,"locked",6)==0){r=150;g=150;b=150;}
            SDL_SetRenderDrawColor(renderer,r,g,b,110);
            SDL_FRect shell={viewport.x+((float)(hatch->x*16)-cx)*scale,
                             viewport.y+((float)(hatch->y*16)-cy)*scale,
                             16.f*scale,(float)(16*HATCH_VERTICAL_SIZE)*scale};
            SDL_RenderFillRect(renderer,&shell);
        }
        SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
        for(int i=0;i<MZM_MAX_PROJECTILES && projectile_library.count>0;i++) {
            const MzmProjectile *projectile=&weapons.list[i];
            if(!projectile->active || projectile->stage==MZM_STAGE_INIT)continue;
            char key[160];
            runtime_projectile_key(projectile,key,sizeof key);
            RuntimeLibraryEntry *entry=runtime_library_find(&projectile_library,key);
            if(!entry || entry->count==0)continue;
            int index=projectile->anim_frame<entry->count?projectile->anim_frame:0;
            SDL_Texture *shot_texture=runtime_library_texture(renderer,entry,index,-1,NULL);
            if(!shot_texture)continue;
            const RuntimeLibraryFrame *art=&entry->frames[index];
            SDL_FRect shot={viewport.x+((float)(projectile->x>>2)+(float)art->offset_x-cx)*scale,
                            viewport.y+((float)(projectile->y>>2)+(float)art->offset_y-cy)*scale,
                            (float)art->w*scale,(float)art->h*scale};
            SDL_RenderTexture(renderer,shot_texture,NULL,&shot);
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
        debug_draw(renderer,&debug,paused,viewport,scale,current_room,&samus,&equipment,
                   debug_frame);
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
    gba_input_close(&gamepad);
    runtime_library_free(&library);
    runtime_library_free(&projectile_library);
    if (texture) SDL_DestroyTexture(texture);
    if (surface) SDL_DestroySurface(surface);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();free(room);return rc;
}
#endif /* FUSION_RUNTIME_TEST */
