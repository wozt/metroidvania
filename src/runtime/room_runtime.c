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
#include "mzm_blocks.h"
#include "mzm_audio.h"
#include "mzm_sfx.h"
#include "mzm_power_bomb.h"
#include "mzm_hatches.h"
#include "mzm_items.h"
#include "mzm_message_banner.h"
#include "mzm_projectiles.h"
#include "mzm_samus.h"
#include "mzm_sprites.h"

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

/* One default room sprite (SpriteLoadRoomSprites), exported by
 * scripts/mzm_sprites.py: block position, sprite name, graphics slot. */
#define MAX_ROOM_SPRITES 24
#define MAX_SPRITESET_ENTRIES 15    /* MAX_AMOUNT_OF_SPRITE_TYPES */
typedef struct { int y, x, gfx_slot, room_slot; char name[64]; } RoomSprite;
/* One native collision type and transition behavior per 16-pixel block. */
typedef struct { int width, height, resolution, room; char world[16], area[40];
    unsigned char types[MAX_ROOM_CELLS]; unsigned char behaviors[MAX_ROOM_CELLS];
    /* gCurrentAffectingClipdata.movement of each block (ClipdataMovement). */
    unsigned char movements[MAX_ROOM_CELLS];
    int never_reform_recorded;      /* blocks.never_reform entries remembered */
    RoomDoor doors[MAX_ROOM_DOORS]; int door_count;
    RoomHatch hatches[MAX_ROOM_HATCHES]; int hatch_count;
    RoomSprite sprites[MAX_ROOM_SPRITES]; int sprite_count;
    /* Backgrounds (scripts/mzm_bg_layers.py): the tileset, the room's
     * transparency and BG properties, the block layers BG0 (when RLE), BG1
     * and BG2 as sheet indices, and the LZ77 text layers BG0 and BG3. */
    int effect_y;                   /* effectY in blocks (255: none) */
    bool has_backgrounds; int tileset, transparency, bg0_prop, bg2_prop, bg3_prop, bg3_scrolling;
    int bg_w[3], bg_h[3]; bool bg_present[3]; unsigned short bg[3][MAX_ROOM_CELLS];
    bool text_present[4]; int text_size[4], text_w[4], text_h[4];
    /* The native hatches (mzm_hatches) with their ROM tables (V, A, W
     * lines) and the collision of the common tilemap values (K). */
    bool has_hatch_tables; MzmHatchEnv hatch_env; unsigned char common_types[256];
    MzmHatch native_hatches[MZM_MAX_HATCHES]; int native_hatch_count;
    /* Raw Clipdata values (gBgPointersAndDimensions.pClipDecomp) and the
     * breakable block state (mzm_blocks). */
    unsigned short clip[MAX_ROOM_CELLS];
    MzmBlocks blocks;
    /* The default spriteset (gSpriteset, gSpritesetSpritesId names). */
    int spriteset; char spriteset_names[MAX_SPRITESET_ENTRIES][64]; int spriteset_count;
    /* The room entry's musicTrack (U line). */
    bool has_music; int music;
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
    static char line[8192]; char extra;
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
        if (line[0] == 'M') {
            int value;
            if (sscanf(line, "M\t%d\t%d\t%d %c", &x, &y, &value, &extra) != 3 ||
                x < 0 || y < 0 || x >= room->width / 16 || y >= room->height / 16 ||
                value < 1 || value > 255)
                goto failure;
            room->movements[y * (room->width / 16) + x] = (unsigned char)value;
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
        if (line[0] == 'P') {
            RoomSprite sprite = {0};
            if (room->sprite_count >= MAX_ROOM_SPRITES ||
                sscanf(line, "P\t%d\t%d\t%63[^\t]\t%d\t%d %c", &sprite.y, &sprite.x, sprite.name,
                       &sprite.gfx_slot, &sprite.room_slot, &extra) != 5 ||
                sprite.x < 0 || sprite.x >= room->width / 16 || sprite.y < 0 ||
                sprite.y >= room->height / 16 || sprite.gfx_slot < 0 || sprite.gfx_slot > 7)
                goto failure;
            room->sprites[room->sprite_count++] = sprite;
            continue;
        }
        if (line[0] == 'L') {
            char effect[48];
            if (sscanf(line, "L\t%d\t%d\t%d\t%d\t%d\t%d\t%47s %c", &room->tileset,
                       &room->transparency, &room->bg0_prop, &room->bg2_prop, &room->bg3_prop,
                       &room->bg3_scrolling, effect, &extra) != 7 ||
                room->tileset < 0 || room->tileset > 78)
                goto failure;
            room->has_backgrounds = true;
            continue;
        }
        if (line[0] == 'R') {
            int layer, row, consumed = 0;
            if (sscanf(line, "R\t%d\t%d\t%n", &layer, &row, &consumed) != 2 || !consumed ||
                layer < 0 || layer > 2 || !room->bg_present[layer] || row < 0 ||
                row >= room->bg_h[layer])
                goto failure;
            const char *cursor = line + consumed;
            for (int column = 0; column < room->bg_w[layer]; ++column) {
                char *end = NULL;
                long value = strtol(cursor, &end, 10);
                if (end == cursor || value < 0 || value > 0xFFFF ||
                    (*end != ',' && column + 1 < room->bg_w[layer]))
                    goto failure;
                room->bg[layer][row * room->bg_w[layer] + column] = (unsigned short)value;
                cursor = *end == ',' ? end + 1 : end;
            }
            continue;
        }
        if (line[0] == 'G') {
            int layer, width, height;
            if (sscanf(line, "G\t%d\t%d\t%d %c", &layer, &width, &height, &extra) != 3 ||
                layer < 0 || layer > 2 || width < 1 || height < 1 ||
                width * height > MAX_ROOM_CELLS)
                goto failure;
            room->bg_w[layer] = width;
            room->bg_h[layer] = height;
            room->bg_present[layer] = true;
            continue;
        }
        if (line[0] == 'T') {
            int layer, size, width, height;
            if (sscanf(line, "T\t%d\t%d\t%d\t%d %c", &layer, &size, &width, &height,
                       &extra) != 4 || (layer != 0 && layer != 3) || width < 8 || height < 8)
                goto failure;
            room->text_present[layer] = true;
            room->text_size[layer] = size;
            room->text_w[layer] = width;
            room->text_h[layer] = height;
            continue;
        }
        if (line[0] == 'V' || line[0] == 'A' || line[0] == 'W' || line[0] == 'K') {
            char kind = line[0];
            int limit = kind == 'V' ? MZM_HATCH_COUNT : kind == 'A' ? 6 :
                        kind == 'W' ? MZM_HATCH_COUNT : 256;
            const char *cursor = line + 2;
            int n = 0;
            while (*cursor && *cursor != '\n' && n < limit) {
                char *end = NULL;
                long value = strtol(cursor, &end, 10), second = 0;
                if (end == cursor || value < 0 || value > 0xFFFF) goto failure;
                if (kind == 'W') {
                    if (*end != ':') goto failure;
                    cursor = end + 1;
                    second = strtol(cursor, &end, 10);
                    if (end == cursor || second < 0 || second > 255) goto failure;
                }
                if (kind == 'V') room->hatch_env.tilemap_values[n] = (uint16_t)value;
                else if (kind == 'A') room->hatch_env.durations[n] = (uint8_t)value;
                else if (kind == 'W') {
                    room->hatch_env.weakness[n] = (uint8_t)value;
                    room->hatch_env.health[n] = (uint8_t)second;
                } else room->common_types[n] = (unsigned char)value;
                n++;
                cursor = *end == ',' ? end + 1 : end;
            }
            if (kind == 'K') room->has_hatch_tables = true;
            continue;
        }
        if (line[0] == 'S') {
            int index, gfx_slot;
            if (room->spriteset_count >= MAX_SPRITESET_ENTRIES ||
                sscanf(line, "S\t%d\t%63[^\t]\t%d %c", &index,
                       room->spriteset_names[room->spriteset_count], &gfx_slot, &extra) != 3 ||
                index < 0 || index > 255 || gfx_slot < 0 || gfx_slot > 7)
                goto failure;
            room->spriteset = index;
            room->spriteset_count++;
            continue;
        }
        if (line[0] == 'E') {
            if (sscanf(line, "E\t%d %c", &room->effect_y, &extra) != 1 || room->effect_y < 0 ||
                room->effect_y > 255)
                goto failure;
            continue;
        }
        if (line[0] == 'U') {
            if (sscanf(line, "U\t%d %c", &room->music, &extra) != 1 || room->music < 0 ||
                room->music > 0xFFFF)
                goto failure;
            room->has_music = true;
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
        room->clip[y * (room->width / 16) + x] = (unsigned short)raw;
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
                       actor==0?ACTOR_SAMUS:actor==1?ACTOR_NON_SPRITE:ACTOR_SPRITE);
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
        equipment->items|=MZM_ITEM_MORPH_BALL|MZM_ITEM_BOMBS|MZM_ITEM_POWER_GRIP|toggled_items;
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
    /* The room load drops STANDING_ENEMY to STANDING_MIDAIR. */
    samus->on_enemy = false;
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
/* gHatchesOpened: per area, the doors whose hatch was opened (their
 * destinations are remembered by position, as the D lines do not carry
 * the destination door's index). */
static struct {
    uint32_t opened[10][8];
    struct { char alias[48]; int x, y; } destinations[128];
    int destination_count;
} runtime_hatch_memory;

static uint8_t runtime_area_index(const char *area) {
    static const char *const names[] = {"Brinstar", "Kraid", "Norfair", "Ridley", "Tourian",
                                        "Crateria", "Chozodia", "Test1", "Test2", "Test3"};
    for (int i = 0; i < 10; ++i)
        if (!strcmp(area, names[i])) return (uint8_t)i;
    return 0;
}

/* The native block tables (rooms/runtime/blocks.tsv, scripts/mzm_c_tables.py)
 * and the collision type and behavior of every raw Clipdata value. */
#define RUNTIME_BLOCK_TABLES "assets/extracted/metroid/rooms/runtime/blocks.tsv"
/* sHaze_PowerBomb_WindowValuesPointers (scripts/mzm_c_tables.py). */
#define RUNTIME_POWER_BOMB_TABLE "assets/extracted/metroid/rooms/runtime/power_bomb.tsv"
static struct {
    bool ok;
    MzmBlockEnv env;
    int main_count, tilemap_count;
    unsigned char main_types[256], tilemap_types[256];
    unsigned short main_behaviors[256], tilemap_behaviors[256];
    /* The tank pickups (tankitem, tanklist and tankclip lines). */
    bool tanks_ok;
    MzmTankTables tanks;
} runtime_block_tables;

static bool runtime_parse_list(const char *cursor, long *values, int count) {
    for (int i = 0; i < count; ++i) {
        char *end = NULL;
        values[i] = strtol(cursor, &end, 10);
        if (end == cursor) return false;
        cursor = *end == ',' ? end + 1 : end;
    }
    return true;
}

static bool runtime_block_tables_open(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    static char line[8192];
    long values[32];
    MzmBlockEnv *env = &runtime_block_tables.env;
    bool ok = fgets(line, sizeof line, f) && !strcmp(line, "schema\tmetroidvania-mzm-blocks-v1\n");
    while (ok && fgets(line, sizeof line, f)) {
        int index, a, b, c, d, e;
        if (!strncmp(line, "damage\t", 7)) {
            ok = runtime_parse_list(line + 7, values, MZM_CAA_COUNT);
            for (int i = 0; ok && i < MZM_CAA_COUNT; ++i) env->caa_damage[i] = (uint16_t)values[i];
        } else if (sscanf(line, "behavior\t%d\t%d\t%d\t%d\t%d\t%d", &index, &a, &b, &c, &d, &e) == 6) {
            if (index < 0 || index >= MZM_BLOCK_BEHAVIORS) { ok = false; break; }
            env->behaviors[index] = (MzmBlockBehavior){(uint8_t)a, (uint8_t)b, (uint8_t)c,
                                                       (uint8_t)d, (uint8_t)e};
        } else if (!strncmp(line, "weakness\t", 9)) {
            ok = runtime_parse_list(line + 9, values, MZM_BLOCK_TYPES);
            for (int i = 0; ok && i < MZM_BLOCK_TYPES; ++i) env->weakness[i] = (uint16_t)values[i];
        } else if (!strncmp(line, "reforming\t", 10)) {
            ok = runtime_parse_list(line + 10, values, MZM_BLOCK_TYPES);
            for (int i = 0; ok && i < MZM_BLOCK_TYPES; ++i) env->reforming[i] = (uint16_t)values[i];
        } else if (sscanf(line, "timers\t%d\t", &index) == 1) {
            const char *cursor = strchr(line + 7, '\t');
            ok = cursor && index >= 0 && index < MZM_BLOCK_TYPES &&
                 runtime_parse_list(cursor + 1, values, 13);
            for (int i = 0; ok && i < 13; ++i) env->timers[index][i] = (uint8_t)values[i];
        } else if (!strncmp(line, "tank\t", 5)) {
            ok = runtime_parse_list(line + 5, values, MZM_TANK_BEHAVIORS);
            for (int i = 0; ok && i < MZM_TANK_BEHAVIORS; ++i)
                env->tank_revealed[i] = (uint16_t)values[i];
        } else if (sscanf(line, "tankitem\t%d\t%d\t%d\t%d", &index, &a, &b, &c) == 4) {
            if (index < 0 || index >= MZM_TANK_BEHAVIORS) { ok = false; break; }
            runtime_block_tables.tanks.item_type[index] = (uint8_t)a;
            runtime_block_tables.tanks.underwater[index] = b != 0;
            runtime_block_tables.tanks.message[index] = (int16_t)c;
            runtime_block_tables.tanks_ok = true;
        } else if (!strncmp(line, "tanklist\t", 9)) {
            const char *cursor = strchr(line + 9, '\t');
            int *row = !strncmp(line + 9, "start", 5) ? runtime_block_tables.tanks.start :
                       !strncmp(line + 9, "total", 5) ? runtime_block_tables.tanks.total :
                       !strncmp(line + 9, "increase", 8) ? runtime_block_tables.tanks.increase : NULL;
            ok = cursor && row && runtime_parse_list(cursor + 1, values, 4);
            for (int i = 0; ok && i < 4; ++i) row[i] = (int)values[i];
        } else if (sscanf(line, "makesolid\t%d\t%d\t%d", &a, &b, &c) == 3) {
            env->make_solid_clip[0] = (uint16_t)a;
            env->make_solid_clip[1] = (uint16_t)b;
            env->make_solid_clip[2] = (uint16_t)c;
        } else if (sscanf(line, "tankclip\t%d\t%d", &a, &b) == 2) {
            runtime_block_tables.tanks.air_clip = (uint16_t)a;
            runtime_block_tables.tanks.water_clip = (uint16_t)b;
        } else if (sscanf(line, "chain\t%d\t%d\t%d", &index, &a, &b) == 3) {
            if (index < 0 || index >= 8) { ok = false; break; }
            env->chain_flag[index] = (uint8_t)a;
            env->chain_behavior[index] = (uint16_t)b;
        } else if (sscanf(line, "subchain\t%d\t", &index) == 1) {
            const char *cursor = strchr(line + 9, '\t');
            ok = cursor && index >= 0 && index < 4 && runtime_parse_list(cursor + 1, values, 4);
            for (int i = 0; ok && i < 4; ++i) env->sub_chain[index][i] = (int8_t)values[i];
        } else if (!strncmp(line, "main\t", 5) || !strncmp(line, "tilemap\t", 8)) {
            bool main = line[0] == 'm';
            const char *cursor = strchr(line, '\t') + 1;
            int n = 0;
            while (*cursor && *cursor != '\n' && n < 256) {
                char *end = NULL;
                long type = strtol(cursor, &end, 10), behavior;
                if (end == cursor || *end != ':') { ok = false; break; }
                cursor = end + 1;
                behavior = strtol(cursor, &end, 10);
                if (end == cursor) { ok = false; break; }
                if (main) {
                    runtime_block_tables.main_types[n] = (unsigned char)type;
                    runtime_block_tables.main_behaviors[n] = (unsigned short)behavior;
                } else {
                    runtime_block_tables.tilemap_types[n] = (unsigned char)type;
                    runtime_block_tables.tilemap_behaviors[n] = (unsigned short)behavior;
                }
                n++;
                cursor = *end == ',' ? end + 1 : end;
            }
            if (main) runtime_block_tables.main_count = n;
            else runtime_block_tables.tilemap_count = n;
        }
    }
    fclose(f);
    runtime_block_tables.ok = ok && runtime_block_tables.main_count > 0 &&
                              runtime_block_tables.tilemap_count > 0;
    return runtime_block_tables.ok;
}

/* gTilemapAndClipPointers.pClipCollisions / pClipBehaviors of a raw value
 * (the common values fall back to the room's K line). */
static uint8_t runtime_clip_type(const Room *room, uint16_t raw) {
    if (raw & 0x400) {
        int common = raw - 0x400;
        if (runtime_block_tables.ok)
            return common < runtime_block_tables.tilemap_count ? runtime_block_tables.tilemap_types[common] : 0;
        return common < 256 ? room->common_types[common] : 0;
    }
    return raw < runtime_block_tables.main_count ? runtime_block_tables.main_types[raw] : 0;
}

static uint16_t runtime_clip_behavior(uint16_t raw) {
    if (raw & 0x400) {
        int common = raw - 0x400;
        return common < runtime_block_tables.tilemap_count ? runtime_block_tables.tilemap_behaviors[common] : 0;
    }
    return raw < runtime_block_tables.main_count ? runtime_block_tables.main_behaviors[raw] : 0;
}

/* BgClipSetClipdataBlockValue: the raw value and its collision. */
static void runtime_room_set_clip(Room *room, uint16_t raw, int y, int x) {
    if (x < 0 || y < 0 || x >= room->width / 16 || y >= room->height / 16) return;
    room->clip[y * (room->width / 16) + x] = raw;
    room_set_cell(room, x, y, (ClipType)runtime_clip_type(room, raw));
}

/* BgClipSetBg1BlockValue: a block value as a sheet index. */
static void runtime_room_set_bg1(Room *room, uint16_t value, int y, int x) {
    if (!room->bg_present[1] || x < 0 || y < 0 || x >= room->bg_w[1] || y >= room->bg_h[1]) return;
    int sheet = value & 0x400 ? 1024 + (value - 0x400) : value;
    if (sheet >= 1024 + 208 || (!(value & 0x400) && value >= 1024)) sheet = 0;
    room->bg[1][y * room->bg_w[1] + x] = (unsigned short)sheet;
}

/* Hatches change both the BG1 block and its Clipdata. */
static void runtime_hatch_set_block(void *context, uint16_t value, int y, int x) {
    Room *room = context;
    runtime_room_set_bg1(room, value, y, x);
    runtime_room_set_clip(room, value, y, x);
}

static int runtime_hatch_type(const char *name) {
    static const char *const names[] = {"none", "unused", "normal", "missile", "super_missile",
                                        "power_bomb", "locked", "locked_navigation"};
    for (int i = 0; i < MZM_HATCH_COUNT; ++i)
        if (!strcmp(name, names[i])) return i;
    return MZM_HATCH_NORMAL;
}

/* ConnectionLoadDoors for the exported hatches, the opened ones overridden
 * to normal hatches, and the hatch Samus came through closing behind her
 * (`arrival` is that door's index, -1 without a door). */
static void runtime_hatches_load(Room *room, const char *alias, int arrival) {
    room->native_hatch_count = 0;
    if (!room->has_hatch_tables) return;
    room->hatch_env.context = room;
    room->hatch_env.set_block = runtime_hatch_set_block;
    uint8_t area = runtime_area_index(room->area);
    for (int i = 0; i < room->hatch_count && room->native_hatch_count < MZM_MAX_HATCHES; ++i) {
        const RoomHatch *exported = &room->hatches[i];
        MzmHatch *h = &room->native_hatches[room->native_hatch_count++];
        *h = (MzmHatch){0};
        h->exists = true;
        h->type = (uint8_t)runtime_hatch_type(exported->type);
        h->x = exported->x;
        h->y = exported->y;
        h->source_door = exported->door;
        for (int d = 0; d < room->door_count; ++d)
            if (room->doors[d].index == exported->door) h->facing_right = h->x > room->doors[d].x0;
        bool opened = exported->door < 256 &&
                      runtime_hatch_memory.opened[area][exported->door / 32] &
                          (1u << (exported->door % 32));
        for (int d = 0; d < room->door_count && !opened; ++d) {
            if (room->doors[d].index != exported->door) continue;
            for (int k = 0; k < runtime_hatch_memory.destination_count; ++k)
                if (!strcmp(runtime_hatch_memory.destinations[k].alias, alias) &&
                    runtime_hatch_memory.destinations[k].x == room->doors[d].x0 &&
                    runtime_hatch_memory.destinations[k].y == room->doors[d].y1)
                    opened = true;
        }
        if (opened && h->type != MZM_HATCH_NONE && h->type != MZM_HATCH_UNUSED)
            mzm_hatch_override(h, &room->hatch_env, MZM_HATCH_NORMAL);
    }
    for (int i = 0; i < room->native_hatch_count && arrival >= 0; ++i) {
        MzmHatch *h = &room->native_hatches[i];
        if (h->source_door == arrival && h->type != MZM_HATCH_NONE)
            mzm_hatch_start_lock_animation(h, &room->hatch_env, MZM_HATCH_CLOSING);
    }
}

/* BlockApplyCcaa on a door block: BgClipCheckOpeningHatch, then
 * ConnectionSetHatchAsOpened (missile and stronger hatches open their
 * destination too). */
static bool runtime_hit_hatch(Room *room, int32_t x, int32_t y, int damage) {
    if (x < 0 || y < 0) return false;
    MzmHatch *opened = NULL;
    int action = mzm_hatch_check_opening(room->native_hatches, room->native_hatch_count,
                                         &room->hatch_env, x / MZM_BLOCK_SIZE,
                                         y / MZM_BLOCK_SIZE, (unsigned)damage, &opened);
    if (opened && opened->source_door < 256) {
        uint8_t area = runtime_area_index(room->area);
        runtime_hatch_memory.opened[area][opened->source_door / 32] |=
            1u << (opened->source_door % 32);
        for (int d = 0; d < room->door_count && opened->type >= MZM_HATCH_MISSILE; ++d) {
            const RoomDoor *door = &room->doors[d];
            if (door->index != opened->source_door || !strcmp(door->destination, "-") ||
                runtime_hatch_memory.destination_count >= 128)
                continue;
            int k = runtime_hatch_memory.destination_count++;
            snprintf(runtime_hatch_memory.destinations[k].alias,
                     sizeof runtime_hatch_memory.destinations[k].alias, "%s", door->destination);
            runtime_hatch_memory.destinations[k].x = door->dest_x;
            runtime_hatch_memory.destinations[k].y = door->dest_y_end;
        }
    }
    return action != MZM_HATCH_NOT_OPENING;
}

/* The Samus whose hitbox the reforming blocks check. */
static const MzmSamus *runtime_block_samus;

static uint16_t runtime_block_get_clip(void *context, int y, int x) {
    const Room *room = context;
    if (x < 0 || y < 0 || x >= room->width / 16 || y >= room->height / 16) return 0;
    return room->clip[y * (room->width / 16) + x];
}
static uint16_t runtime_block_behavior(void *context, uint16_t clip) {
    (void)context;
    return runtime_clip_behavior(clip);
}
static uint8_t runtime_block_collision(void *context, uint16_t clip) {
    return runtime_clip_type(context, clip);
}
static void runtime_block_set_clip(void *context, uint16_t clip, int y, int x) {
    runtime_room_set_clip(context, clip, y, x);
}
static void runtime_block_set_bg1(void *context, uint16_t value, int y, int x) {
    runtime_room_set_bg1(context, value, y, x);
}
static bool runtime_block_check_hatch(void *context, int x, int y, unsigned damage) {
    return runtime_hit_hatch(context, x * MZM_BLOCK_SIZE, y * MZM_BLOCK_SIZE, (int)damage);
}
/* BlockCheckSamusInReformingBlock */
static bool runtime_block_samus_in(void *context, int x, int y) {
    (void)context;
    const MzmSamus *samus = runtime_block_samus;
    if (!samus) return false;
    const MzmBlockHitbox *box = mzm_block_hitbox(mzm_pose_hitbox(samus->pose));
    return (samus->x + box->left) / MZM_BLOCK_SIZE <= x && x <= (samus->x + box->right) / MZM_BLOCK_SIZE &&
           (samus->y + box->top) / MZM_BLOCK_SIZE <= y && y <= samus->y / MZM_BLOCK_SIZE;
}

/* The room's MzmBlockEnv. */
static MzmBlockEnv runtime_block_env(Room *room) {
    MzmBlockEnv env = runtime_block_tables.env;
    env.context = room;
    env.width = room->width / 16;
    env.height = room->height / 16;
    env.get_clip = runtime_block_get_clip;
    env.behavior = runtime_block_behavior;
    env.collision = runtime_block_collision;
    env.set_clip = runtime_block_set_clip;
    env.set_bg1 = runtime_block_set_bg1;
    env.check_hatch = runtime_block_check_hatch;
    env.samus_in_block = runtime_block_samus_in;
    return env;
}

/* BlockApplyCcaa for a clipdata-affecting action of the given damage type
 * (MZM_DAMAGE_*) at a subpixel point; hatches only without block tables. */
static bool runtime_room_affect(Room *room, int32_t x, int32_t y, int damage) {
    if (!runtime_block_tables.ok) return runtime_hit_hatch(room, x, y, damage);
    uint8_t caa = damage & DAMAGE_SUPER_MISSILE ? MZM_CAA_SUPER_MISSILE :
                  damage & DAMAGE_MISSILE ? MZM_CAA_MISSILE :
                  damage & DAMAGE_POWER_BOMB ? MZM_CAA_POWER_BOMB :
                  damage & DAMAGE_BOMB_PISTOL ? MZM_CAA_BOMB_PISTOL : MZM_CAA_BEAM;
    MzmBlockEnv env = runtime_block_env(room);
    return mzm_blocks_affect(&room->blocks, &env, caa, x, y);
}

/* gNeverReformBlocks: the never-reform blocks destroyed per room. */
static struct {
    struct { char alias[48]; int x, y; } blocks[1024];
    int count;
} runtime_never_reform_memory;

/* The power bomb explosion's look over the backgrounds (not the sprites):
 * PowerBombYellowTint (+5 red and green per channel) while expanding, and
 * outside window 1 (the sHaze_PowerBomb rows around the bomb) the BLDY 12
 * brightness decrease. Approximation: the native effects act on the BG
 * palette and blending registers, here on the drawn layers. */
static void runtime_draw_power_bomb(SDL_Renderer *renderer,const MzmPowerBomb *pb,float cx,
                                    float cy,SDL_FRect viewport,float scale) {
    if(pb->state==MZM_PB_EXPLODING) {
        SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_ADD);
        SDL_SetRenderDrawColor(renderer,41,41,0,255);
        SDL_RenderFillRect(renderer,&viewport);
    }
    /* 16 - 12 = 4 sixteenths of each colour remain. */
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_MOD);
    SDL_SetRenderDrawColor(renderer,64,64,64,255);
    int rows=(int)(viewport.h/scale);
    int centre_x=(int)((float)(pb->x>>2)-cx), centre_y=(int)((float)(pb->y>>2)-cy);
    int axis=pb->window_axis;
    for(int row=0;row<rows;row++) {
        int left=0,right=0;
        bool inside=mzm_power_bomb_windows_ready() &&
            mzm_power_bomb_window(axis,row-(centre_y-axis),&left,&right);
        float y=viewport.y+(float)row*scale;
        if(!inside) {
            SDL_FRect line={viewport.x,y,viewport.w,scale};
            SDL_RenderFillRect(renderer,&line);
            continue;
        }
        float a=(float)(centre_x+left)*scale, b=(float)(centre_x+right)*scale;
        if(a<0.f)a=0.f;
        if(b>viewport.w)b=viewport.w;
        if(a>0.f){SDL_FRect l={viewport.x,y,a,scale};SDL_RenderFillRect(renderer,&l);}
        if(b<viewport.w){SDL_FRect r={viewport.x+b,y,viewport.w-b,scale};SDL_RenderFillRect(renderer,&r);}
    }
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
}

/* BlockApplyCcaa(CAA_POWER_BOMB) on a block the explosion's edge reaches. */
static void runtime_power_bomb_apply(void *context, int y, int x) {
    runtime_room_affect((Room *)context, x * 64, y * 64, DAMAGE_POWER_BOMB);
}

/* BlockRemoveNeverReformBlocks at a room load, with a fresh block state. */
/* gItemsCollected and gCollectingTank / gLastTankCollected of this session. */
static MzmCollectedItems runtime_collected_items;
static MzmTankCollection runtime_tank_collection;

static void runtime_blocks_load(Room *room, const char *alias) {
    memset(&room->blocks, 0, sizeof room->blocks);
    if (!runtime_block_tables.ok) return;
    MzmBlockEnv env = runtime_block_env(room);
    for (int i = 0; i < runtime_never_reform_memory.count; ++i)
        if (!strcmp(runtime_never_reform_memory.blocks[i].alias, alias))
            mzm_blocks_remove_never_reform(&env, runtime_never_reform_memory.blocks[i].x,
                                           runtime_never_reform_memory.blocks[i].y);
    /* BgClipRemoveCollectedTanks: the room's collected items become water
     * (underwater tank behaviors) or air, with an empty BG1 block. */
    uint8_t area = runtime_area_index(room->area);
    if (area >= MZM_MAX_AREAS || !runtime_block_tables.tanks_ok) return;
    for (int i = 0; i < runtime_collected_items.count[area]; ++i) {
        const int x = runtime_collected_items.items[area][i].x;
        const int y = runtime_collected_items.items[area][i].y;
        if (runtime_collected_items.items[area][i].room != room->room || x >= room->width / 16 ||
            y >= room->height / 16)
            continue;
        uint16_t behavior = runtime_clip_behavior(room->clip[y * (room->width / 16) + x]);
        runtime_room_set_clip(room, mzm_tank_cleared_clip(&runtime_block_tables.tanks, behavior),
                              y, x);
        runtime_room_set_bg1(room, 0, y, x);
    }
}

/* The probes of BgClipCheckTouchingTransitionOrTank: blocks at Samus's
 * right and left hitbox edges (center row), and at her bottom and top
 * rows (center column), with the behavior of their current Clipdata. */
static void runtime_touch_probes(const Room *room, const MzmSamus *samus, MzmTouchProbes *probes) {
    const MzmBlockHitbox *box = mzm_block_hitbox(mzm_pose_hitbox(samus->pose));
    int32_t native_y = samus->y - 1;
    int width = room->width / 16, height = room->height / 16;
    int32_t raw_x[3] = {(box->right >> 2) + samus->x, (box->left >> 2) + samus->x, samus->x};
    int32_t raw_y[3] = {(box->top >> 2) + native_y, (box->top >> 4) + native_y,
                        (box->top >> 4) + (box->top >> 2) + native_y};
    int xs[3], ys[3];
    for (int i = 0; i < 3; ++i) {
        int32_t x = raw_x[i] < 0 ? 0 : raw_x[i] > width * MZM_BLOCK_SIZE ? width * MZM_BLOCK_SIZE : raw_x[i];
        int32_t y = raw_y[i] < 0 ? 0 : raw_y[i] > height * MZM_BLOCK_SIZE ? height * MZM_BLOCK_SIZE : raw_y[i];
        xs[i] = x / MZM_BLOCK_SIZE < width ? x / MZM_BLOCK_SIZE : width - 1;
        ys[i] = y / MZM_BLOCK_SIZE < height ? y / MZM_BLOCK_SIZE : height - 1;
    }
    /* sBlockTouchOffsets: (center, right), (center, left), (bottom, center),
     * (top, center). */
    static const int touch[4][2] = {{0, 0}, {0, 1}, {1, 2}, {2, 2}};
    for (int j = 0; j < 4; ++j) {
        probes->x[j] = xs[touch[j][1]];
        probes->y[j] = ys[touch[j][0]];
        probes->behaviors[j] = runtime_clip_behavior(room->clip[probes->y[j] * width + probes->x[j]]);
    }
}

/* BgClipFinishCollectingTank: the tank block becomes air or water, empty in
 * BG1, and is registered as collected (no minimap is ported). */
static void runtime_finish_collecting_tank(Room *room) {
    MzmTankCollection *c = &runtime_tank_collection;
    c->collecting = false;
    uint8_t type = mzm_tank_item_type(&runtime_block_tables.tanks, c->behavior);
    if (type == MZM_ITEM_TYPE_NONE) return;
    runtime_room_set_bg1(room, 0, c->y, c->x);
    runtime_room_set_clip(room, mzm_tank_cleared_clip(&runtime_block_tables.tanks, c->behavior),
                          c->y, c->x);
    mzm_items_set_collected(&runtime_collected_items, runtime_area_index(room->area),
                            (uint8_t)room->room, c->x, c->y, type);
}

/* BlockSamusApplyScrewSpeedboosterDamageToEnvironment, called by the
 * controller's SamusCheckScrewSpeedboosterAffectingEnvironment. */
static void runtime_samus_destruct(void *context, uint16_t x, uint16_t y, int action) {
    Room *room = context;
    if (!runtime_block_tables.ok) return;
    MzmBlockEnv env = runtime_block_env(room);
    mzm_blocks_samus_destruct(&room->blocks, &env, x, y, action);
}

/* BgClipCheckWalkingOnCrumbleBlock */
static void runtime_crumble_blocks(Room *room, const MzmBlockEnv *env, const MzmSamus *samus) {
    enum { CRUMBLE = 35, SLOW_CRUMBLE = 46, TYPE_CRUMBLE = 4, TYPE_SLOW_CRUMBLE = 5 };
    if (samus->y_velocity < 0 || mzm_pose_standing(samus->pose) == MZM_STANDING_FORCED) return;
    const MzmBlockHitbox *box = mzm_block_hitbox(mzm_pose_hitbox(samus->pose));
    int last = (samus->x + box->right) / MZM_BLOCK_SIZE;
    if (last > env->width) last = env->width;
    int32_t left = samus->x + box->left;
    int first = (left < 0 ? 0 : left) / MZM_BLOCK_SIZE;
    int y = (samus->y + 2) / MZM_BLOCK_SIZE;
    if (y > env->height) y = env->height;
    for (int x = first; x <= last; ++x) {
        uint16_t behavior = runtime_clip_behavior(runtime_block_get_clip(room, y, x));
        if (behavior == CRUMBLE) {
            mzm_blocks_store_broken_reform(&room->blocks, env, TYPE_CRUMBLE, x, y, false);
        } else if (behavior == SLOW_CRUMBLE &&
                   mzm_blocks_store_broken_reform(&room->blocks, env, TYPE_SLOW_CRUMBLE, x, y, true)) {
            runtime_room_set_bg1(room, 0x400 | 1, y, x);
            runtime_room_set_clip(room, 0x400 | 1, y, x);
        }
    }
}

/* One frame of the blocks: Samus's screw attack and the crumble blocks
 * under her, BlockUpdateBrokenBlocks, BlockProcessBombChains, then the
 * never-reform blocks destroyed this frame are remembered. */
static void runtime_blocks_frame(Room *room, const char *alias, const MzmSamus *samus,
                                 const MzmEquipment *equipment, uint8_t frame8) {
    if (!runtime_block_tables.ok) return;
    runtime_block_samus = samus;
    MzmBlockEnv env = runtime_block_env(room);
    (void)equipment;
    runtime_crumble_blocks(room, &env, samus);
    mzm_blocks_update_broken(&room->blocks, &env);
    mzm_blocks_process_bomb_chains(&room->blocks, &env, frame8);
    /* The blocks Samus destroyed during her update are recorded here too. */
    int recorded = room->never_reform_recorded;
    room->never_reform_recorded = room->blocks.never_reform_count;
    for (int i = recorded; i < room->blocks.never_reform_count; ++i) {
        if (runtime_never_reform_memory.count >= 1024) break;
        int k = runtime_never_reform_memory.count++;
        snprintf(runtime_never_reform_memory.blocks[k].alias,
                 sizeof runtime_never_reform_memory.blocks[k].alias, "%s", alias);
        runtime_never_reform_memory.blocks[k].x = room->blocks.never_reform[i][0];
        runtime_never_reform_memory.blocks[k].y = room->blocks.never_reform[i][1];
    }
}

static void runtime_collision_affect(void *context,int32_t x,int32_t y,int damage) {
    runtime_room_affect((Room *)context,x,y,damage);
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
        const char *suit,unsigned int frame_counter,uint32_t items,int charge) {
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
    } else if(samus->speedboosting || samus->shinespark_timer) {
        /* gFrameCounter8Bit % 6 picks one row for both halves. */
        int row=(int)((frame_counter&0xFFu)%6u)/2;
        choice=(RuntimePaletteChoice){borrowed,"Speedboost",row,borrowed,"Speedboost",row};
    } else if(samus->pose==MZM_POSE_SCREW_ATTACKING) {
        if(samus->anim_frame&1u){choice.kind0="Flashing";choice.row0=1;}
    } else if(beam_release) {
        choice.kind0="BeamRelease";
        choice.row0=(items&MZM_ITEM_ICE_BEAM)?2:(items&MZM_ITEM_PLASMA_BEAM)?4:
            (items&MZM_ITEM_WAVE_BEAM)?3:(items&MZM_ITEM_LONG_BEAM)?1:0;
    } else if(samus->unmorph_palette_timer) {
        choice.suit0=borrowed;
        choice.kind0="Unmorph";
        choice.row0=(uint8_t)(samus->unmorph_palette_timer-5)>4?0:1;
    } else if(strcmp(suit,"Suitless") && charge>=MZM_CHARGE_FULL) {
        /* The charging rows: two consecutive rows, alternating every 16
         * frames of full charge, none on the fourth step. */
        int offset=(charge-MZM_CHARGE_FULL)>>4;
        if(offset!=3) {
            int row=((items&MZM_ITEM_ICE_BEAM)?4:(items&MZM_ITEM_PLASMA_BEAM)?8:
                     (items&MZM_ITEM_WAVE_BEAM)?6:(items&MZM_ITEM_LONG_BEAM)?2:0)+(offset&1);
            choice=(RuntimePaletteChoice){suit,"ChargingBeam",row,suit,"ChargingBeam",row+1};
        }
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
#define RUNTIME_LIBRARY_MAX 16384
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
/* SamusUpdateGraphicsOam's body animation in the library (with the armed
 * arm cannon graphics while missiles are highlighted). A table without the
 * direction's row falls back to its forward row, then to the pose's. */
static RuntimeLibraryEntry *runtime_samus_entry(RuntimeLibrary *library,const MzmSamus *samus,
                                                const MzmEquipment *equipment,
                                                uint8_t weapon_highlighted) {
    char key[160];
    mzm_samus_graphics_key(samus,equipment,weapon_highlighted,key,sizeof key);
    RuntimeLibraryEntry *entry=runtime_library_find(library,key);
    if(!entry) {
        char fallback[160];
        const char *acd=strstr(key,"/ACD_");
        if(acd) {
            const char *side=strchr(acd+1,'/');
            snprintf(fallback,sizeof fallback,"%.*s/ACD_FORWARD%s",(int)(acd-key),key,
                     side?side:"");
            entry=runtime_library_find(library,fallback);
        }
        if(!entry) {
            const char *family=equipment->suit==MZM_SUIT_SUITLESS?"Suitless":
                equipment->suit==MZM_SUIT_NORMAL?"PowerSuit":"FullSuit";
            snprintf(fallback,sizeof fallback,"%s/pose/%s/%s",family,mzm_pose_name(samus->pose),
                     samus->facing<0?"left":"right");
            entry=runtime_library_find(library,fallback);
        }
    }
    if(entry && mzm_samus_armed_graphics(weapon_highlighted)) {
        char armed[176];
        snprintf(armed,sizeof armed,"%s/armed",entry->name);
        RuntimeLibraryEntry *armed_entry=runtime_library_find(library,armed);
        if(armed_entry && armed_entry->count==entry->count) entry=armed_entry;
    }
    return entry;
}
/* The native frame durations of a Samus animation by its library key. */
static int runtime_pose_durations(void *context,const char *key,uint8_t *durations,int max) {
    RuntimeLibraryEntry *entry=runtime_library_find(context,key);
    if(!entry)return 0;
    int count=entry->count<max?entry->count:max;
    for(int i=0;i<count;i++)durations[i]=(uint8_t)entry->ticks[i];
    return count;
}
/* gSamusWeaponInfo.weaponHighlighted */
static uint8_t runtime_weapon_highlighted(const MzmWeapons *weapons) {
    switch(weapons->highlighted) {
        case MZM_WEAPON_MISSILE: return MZM_WH_MISSILE;
        case MZM_WEAPON_SUPER_MISSILE: return MZM_WH_SUPER_MISSILE;
        case MZM_WEAPON_POWER_BOMB: return MZM_WH_POWER_BOMB;
        case MZM_WEAPON_NONE: break;
    }
    return 0;
}
/* Projectile library key for ProjectileProcess* OAM selection and the
 * X/Y flips applied by ProjectileDraw. */
static void runtime_projectile_key(const MzmProjectile *projectile,uint32_t items,char *key,
                                   size_t size) {
    static const char *names[]={
        [MZM_PROJECTILE_BEAM]="NormalBeam",[MZM_PROJECTILE_MISSILE]="Missile",
        [MZM_PROJECTILE_SUPER_MISSILE]="SuperMissile",[MZM_PROJECTILE_BOMB]="Bomb",
        [MZM_PROJECTILE_POWER_BOMB]="PowerBomb",[MZM_PROJECTILE_LONG_BEAM]="LongBeam",
        [MZM_PROJECTILE_CHARGED_BEAM]="ChargedNormalBeam",
        [MZM_PROJECTILE_CHARGED_LONG_BEAM]="ChargedLongBeam",
        [MZM_PROJECTILE_ICE_BEAM]="IceBeam",[MZM_PROJECTILE_CHARGED_ICE_BEAM]="ChargedIceBeam",
        [MZM_PROJECTILE_WAVE_BEAM]="WaveBeam",[MZM_PROJECTILE_CHARGED_WAVE_BEAM]="ChargedWaveBeam",
        [MZM_PROJECTILE_PLASMA_BEAM]="PlasmaBeam",
        [MZM_PROJECTILE_CHARGED_PLASMA_BEAM]="ChargedPlasmaBeam"};
    if(projectile->type==MZM_PROJECTILE_BOMB || projectile->type==MZM_PROJECTILE_POWER_BOMB) {
        /* s(Power)BombOam_Slow / _Fast, never flipped. */
        snprintf(key,size,"Projectile/%sOam_%s/none",
                 projectile->type==MZM_PROJECTILE_BOMB?"Bomb":"PowerBomb",
                 projectile->fast?"Fast":"Slow");
        return;
    }
    const char *shape=projectile->direction==MZM_AIM_UP||projectile->direction==MZM_AIM_DOWN?
        "Vertical":projectile->direction==MZM_AIM_FORWARD?"Horizontal":"Diagonal";
    /* s(Super)MissileOam_Tumbling */
    if(projectile->stage==MZM_STAGE_TUMBLING)shape="Tumbling";
    const char *flip=projectile->x_flip?(projectile->y_flip?"xy":"x"):
        (projectile->y_flip?"y":"none");
    /* The Plasma Beam has its Wave and NoWave tables; the Wave and Plasma
     * sets are recoloured by the Ice Beam's sBeamPal row (the /ice keys). */
    bool plasma=projectile->type==MZM_PROJECTILE_PLASMA_BEAM ||
        projectile->type==MZM_PROJECTILE_CHARGED_PLASMA_BEAM;
    bool wave=projectile->type==MZM_PROJECTILE_WAVE_BEAM ||
        projectile->type==MZM_PROJECTILE_CHARGED_WAVE_BEAM;
    snprintf(key,size,"Projectile/%sOam_%s%s/%s%s",names[projectile->type],shape,
             plasma?(projectile->wave_oam?"_Wave":"_NoWave"):"",flip,
             (plasma||wave)&&(items&MZM_ITEM_ICE_BEAM)?"/ice":"");
}
static int runtime_projectile_durations(void *context,const MzmProjectile *projectile,
                                        uint8_t *durations,int max) {
    char key[160];
    /* The durations do not depend on the Ice Beam recolouring. */
    runtime_projectile_key(projectile,0,key,sizeof key);
    RuntimeLibraryEntry *entry=runtime_library_find(context,key);
    if(!entry)return 0;
    int count=entry->count<max?entry->count:max;
    for(int i=0;i<count;i++)durations[i]=(uint8_t)entry->ticks[i];
    return count;
}
/* The beam graphics ProjectileLoadGraphics loads (the ported beams). */
static const char *runtime_beam_variant(uint32_t items) {
    bool ice=items&MZM_ITEM_ICE_BEAM;
    if(items&MZM_ITEM_PLASMA_BEAM)return ice?"PlasmaIceBeam":"PlasmaBeam";
    if(items&MZM_ITEM_WAVE_BEAM)return ice?"WaveIceBeam":"WaveBeam";
    return ice?"IceBeam":(items&MZM_ITEM_LONG_BEAM)?"LongBeam":"NormalBeam";
}

/* One tick of a looping or one-shot particle animation from its library
 * durations; true when a one-shot animation ended. */
static bool runtime_anim_step(const RuntimeLibraryEntry *entry,uint16_t *frame,uint16_t *counter) {
    if(!entry || entry->count<=0) return true;
    if(*frame>=entry->count)*frame=0;
    if(++*counter>=entry->ticks[*frame]) {
        *counter=0;
        if(++*frame>=entry->count){*frame=0;return true;}
    }
    return false;
}

/* The charging beam particle and the beam shot flashes drawn from the
 * projectile library's beam variant. */
typedef struct {
    uint8_t charge_stage;
    uint16_t charge_frame,charge_counter;
    int cannon_x,cannon_y;
    struct { bool active; int x,y; const char *table; bool flip; uint16_t frame,counter; } flash;
} RuntimeBeamFx;

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
    {"Screw Attack",MZM_ITEM_SCREW_ATTACK},{"Speed Booster",MZM_ITEM_SPEED_BOOSTER},
    {"Charge Beam",MZM_ITEM_CHARGE_BEAM},{"Long Beam",MZM_ITEM_LONG_BEAM},
    {"Ice Beam",MZM_ITEM_ICE_BEAM},{"Wave Beam",MZM_ITEM_WAVE_BEAM},
    {"Plasma Beam",MZM_ITEM_PLASMA_BEAM},
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
/* Zero Mission sprites: the ROM tables and OAM frame durations of
 * scripts/mzm_sprites.py, the sprite library, and the room's live sprites. */
/* The Zero Mission sound engine's data (scripts/mzm_audio.py). */
#define RUNTIME_SOUND_BANK "assets/extracted/metroid/sound/runtime/sound_bank.bin"
#define RUNTIME_SOUND_TABLES "assets/extracted/metroid/sound/runtime/sound_tables.tsv"
#define RUNTIME_AUDIO_RATE 48000

/* The sound hardware's output of one game frame into the SDL stream; the
 * frame is dropped when the device lags (more than 0.2 s queued). */
static void runtime_audio_frame(SDL_AudioStream *stream) {
    static int16_t buffer[2 * 1024];
    mzm_audio_update();
    int frames = mzm_audio_render(buffer, 1024, RUNTIME_AUDIO_RATE);
    if (stream && frames > 0 &&
        SDL_GetAudioStreamQueued(stream) < RUNTIME_AUDIO_RATE * 4 / 5)
        SDL_PutAudioStreamData(stream, buffer, frames * 4);
}

/* The gameplay modules' sound requests (mzm_sfx) into the sound engine. */
static void runtime_sfx_sink(void *context, MzmSfxAction action, uint16_t sound,
                             uint16_t timer) {
    (void)context;
    switch (action) {
    case MZM_SFX_PLAY: mzm_sound_play(sound); break;
    case MZM_SFX_PLAY_NOT_PLAYING: mzm_sound_play_not_already_playing(sound); break;
    case MZM_SFX_STOP: mzm_sound_stop(sound); break;
    case MZM_SFX_FADE: mzm_sound_fade(sound, timer); break;
    case MZM_SFX_STOP_ALL: mzm_audio_stop_all(); break;
    case MZM_SFX_MUSIC_PLAY: mzm_music_play(sound, (uint8_t)timer); break;
    case MZM_SFX_MUSIC_INSERT: mzm_music_insert(sound, (uint8_t)timer); break;
    case MZM_SFX_BACKUP_TRACK2: mzm_audio_backup_track2(); break;
    case MZM_SFX_RETRIEVE_TRACK2: mzm_audio_retrieve_track2(); break;
    }
}

/* CheckPlayRoomMusicTrack when a room is entered: the room's track fades
 * the current one in 30 frames; CheckPlayTransitionMusicTrack starts it
 * when the (absent) door transition would end, returned in frames. */
static int runtime_room_music(const Room *room) {
    if (!mzm_audio_ready() || !room->has_music) return 0;
    mzm_music_check_set_new_track((uint16_t)room->music);
    return 32;
}

#define RUNTIME_SPRITE_TABLES "assets/extracted/metroid/metadata/sprites.tsv"
#define RUNTIME_SPRITE_LIBRARY "assets/extracted/metroid/sprites/enemies/runtime/runtime_index.tsv"
#define RUNTIME_MAX_SPRITES 24
typedef struct {
    MzmSpriteTables tables;
    MzmSpriteKind kinds[MZM_SPRITE_FAMILIES];
    MzmEffectKinds effect_kinds;
    MzmEffects effects;
    char names[256][64];
    bool tables_ok, kinds_ok;
    /* Families whose OAM tables are all in the library; the others are
     * reported once and their sprites are not spawned. */
    bool family_ok[MZM_SPRITE_FAMILIES];
    /* Native event flags of this session (EventFunction); nothing sets the
     * story events yet, so they start clear as in a new game. */
    uint8_t events[32];
    Room *room;                     /* the room the hooks change */
    /* gAlarmTimer and the other sprite globals; the alarm survives door
     * transitions (SpriteLoadAllData only clears it on a new game). */
    MzmSpriteGlobals globals;
    MzmSprite live[RUNTIME_MAX_SPRITES];
    uint8_t frame8;
    uint16_t frame16;
    /* The message banner: its durations and text calls (message lines),
     * the message names of its library keys, and the banner itself (kept
     * outside the sprite slots). */
    MzmBannerKind banner_kind;
    char message_names[MZM_MESSAGE_COUNT][40];
    bool banner_ok;
    MzmMessageBanner banner;
} RuntimeSprites;

static bool runtime_sprite_tables_open(RuntimeSprites *sprites, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char line[1024];
    bool ok = fgets(line, sizeof line, f) &&
              !strcmp(line, "schema\tmetroidvania-mzm-sprite-tables-v1\n");
    int random_count = 0, falling_count = 0;
    /* The `table` lines of the AIs (scripts/mzm_sprites.py AI_TABLES), each
     * with its exact length in the decompilation. */
    MzmSpriteTables *t = &sprites->tables;
    struct { const char *name; int16_t *destination; int capacity, length; } ai_tables[] = {
        {"sSkreeFallingSpeed", t->skree_falling, 16, 8},
        {"sZeelaEyesFallingFromUpSpeed", t->zeela_eyes_up, 16, 16},
        {"sZeelaEyesFallingFromBottomSpeed", t->zeela_eyes_bottom, 16, 12},
        {"sSpritesFallingCeilingSpeed", t->falling_ceiling, 16, 16},
        {"sSidehopperHighJumpVelocity", t->sidehopper_high, 10, 10},
        {"sSidehopperLowJumpVelocity", t->sidehopper_low, 10, 10},
        {"sDessgeegaHighJumpYVelocity", t->dessgeega_high, 10, 10},
        {"sDessgeegaLowJumpYVelocity", t->dessgeega_low, 10, 10},
        {"sReoIdleYMovement", t->reo_idle_y, 65, 65},
        {"sReoIdleXMovement", t->reo_idle_x, 81, 81},
        {"sMellaIdleYMovement", t->mella_idle_y, 65, 65},
        {"sMellaIdleXMovement", t->mella_idle_x, 49, 49},
        {"sMellaGoingDownYMovement", t->mella_down, 6, 6},
        {"sMellaGoingUpYMovement", t->mella_up, 7, 7},
        {"sMellaMovingXMovement", t->mella_x, 7, 7},
        {"sHoltzIdleYVelocity", t->holtz_idle_y, 65, 65},
        {"sSqueeptRisingMovement", t->squeept_rising, 65, 65},
        {"sSqueeptFallingMovement", t->squeept_falling, 16, 16},
        {"sSpacePirateWallJumpingVelocity", t->pirate_wall_jump, 8, 8},
        {"sSpacePirate_2e1030", t->pirate_jump_low, 11, 11},
        {"sSpacePirateJumpingVelocity", t->pirate_jump, 10, 10},
        {"sSpacePirate_2e105a", t->pirate_jump_high, 11, 11},
        {mzm_debris_table_names[0], t->debris[0], 9, 9},
        {mzm_debris_table_names[1], t->debris[1], 9, 9},
        {mzm_debris_table_names[2], t->debris[2], 9, 9},
        {mzm_debris_table_names[3], t->debris[3], 9, 9},
        {mzm_debris_table_names[4], t->debris[4], 9, 9},
        {mzm_debris_table_names[5], t->debris[5], 9, 9},
        {mzm_debris_table_names[6], t->debris[6], 9, 9},
        {mzm_debris_table_names[7], t->debris[7], 9, 9},
        {mzm_debris_table_names[8], t->debris[8], 9, 9},
    };
    unsigned long long found = 0;
    while (ok && fgets(line, sizeof line, f)) {
        char *cursor = strchr(line, '\t');
        if (!cursor) continue;
        *cursor++ = '\0';
        if (!strcmp(line, "random")) {
            for (random_count = 0; random_count < 32; ++random_count)
                sprites->tables.random[random_count] = (uint8_t)strtol(cursor, &cursor, 10);
        } else if (!strcmp(line, "falling")) {
            for (falling_count = 0; falling_count < 16; ++falling_count)
                sprites->tables.falling[falling_count] = (int16_t)strtol(cursor, &cursor, 10);
        } else if (!strcmp(line, "message")) {
            char name[40];
            int id, calls;
            if (sscanf(cursor, "%d %39s %d", &id, name, &calls) == 3 && id >= 0 &&
                id < MZM_MESSAGE_COUNT) {
                sprites->banner_kind.text_calls[id] = (uint16_t)calls;
                snprintf(sprites->message_names[id], sizeof sprites->message_names[id], "%s", name);
            }
        } else if (!strcmp(line, "sstats")) {
            char name[64];
            int id, health, damage, weakness;
            if (sscanf(cursor, "%63s %d %d %d %d", name, &id, &health, &damage, &weakness) != 5 ||
                id < 0 || id > 255) { ok = false; break; }
            sprites->tables.secondary_damage[id] = (uint16_t)damage;
            sprites->tables.secondary_weakness[id] = (uint8_t)(weakness & 0xFF);
            sprites->tables.secondary_freeze[id] = (uint8_t)(weakness >> 8);
        } else if (!strcmp(line, "multi")) {
            /* multi <name> <frame> <timer> <oam,y,x> per part */
            char name[64];
            int index, timer, used = 0;
            if (sscanf(cursor, "%63s %d %d%n", name, &index, &timer, &used) != 3) continue;
            for (int m = 0; m < MZM_CHOZO_MULTI_COUNT; ++m) {
                if (strcmp(name, mzm_chozo_multi_names[m]) || index < 0 || index >= MZM_MULTI_FRAMES)
                    continue;
                MzmMultiSprite *multi = &sprites->tables.chozo_multi[m];
                multi->timers[index] = (uint8_t)timer;
                const char *part = cursor + used;
                for (int p = 0; p < MZM_MULTI_PARTS; ++p) {
                    int oam, y, x, n = 0;
                    if (sscanf(part, " %d,%d,%d%n", &oam, &y, &x, &n) != 3) break;
                    multi->parts[index][p][0] = (int16_t)oam;
                    multi->parts[index][p][1] = (int16_t)y;
                    multi->parts[index][p][2] = (int16_t)x;
                    part += n;
                }
            }
        } else if (!strcmp(line, "table")) {
            char *values = strchr(cursor, '\t');
            if (!values) { ok = false; break; }
            *values++ = '\0';
            for (size_t t = 0; t < sizeof ai_tables / sizeof ai_tables[0]; ++t) {
                if (strcmp(cursor, ai_tables[t].name)) continue;
                char *end;
                int count;
                for (count = 0; count < ai_tables[t].capacity; ++count) {
                    long value = strtol(values, &end, 10);
                    if (end == values) break;
                    ai_tables[t].destination[count] = (int16_t)value;
                    values = end;
                }
                if (count != ai_tables[t].length) { ok = false; break; }
                found |= 1ull << t;
            }
        } else if (!strcmp(line, "stats")) {
            char name[64];
            int id, health, damage, weakness, no_drop, drops[5], used = 0;
            if (sscanf(cursor, "%63s %d %d %d %d %d %d %d %d %d %d%n", name, &id, &health,
                       &damage, &weakness, &no_drop, &drops[0], &drops[1], &drops[2], &drops[3],
                       &drops[4], &used) != 11 ||
                id < 0 || id > 255) { ok = false; break; }
            /* SPRITE_STATS_SMALL_ENERGY_PROB .. SPRITE_STATS_POWER_BOMB_PROB */
            for (int d = 0; d < 5; ++d) sprites->tables.drops[id][d] = (uint16_t)drops[d];
            snprintf(sprites->names[id], sizeof sprites->names[id], "%s", name);
            sprites->tables.health[id] = (uint16_t)health;
            sprites->tables.damage[id] = (uint16_t)damage;
            /* C_16_2_8(freeze, weakness): the weakness is the low byte. */
            sprites->tables.weakness[id] = (uint8_t)(weakness & 0xFF);
            sprites->tables.freeze[id] = (uint8_t)(weakness >> 8);
        }
    }
    fclose(f);
    for (size_t i = 0; i < sizeof ai_tables / sizeof ai_tables[0]; ++i)
        if (!(found & (1ull << i)))
            fprintf(stderr, "Zero Mission sprite tables: %s missing (python3 -m scripts.mzm_sprites)\n",
                    ai_tables[i].name);
    return ok && random_count == 32 && falling_count == 16 &&
           found == (1ull << (sizeof ai_tables / sizeof ai_tables[0])) - 1;
}

/* Frame durations of OAM tables from the sprite library. */
static bool runtime_sprite_kind_open(MzmSpriteKind *kind, RuntimeLibrary *library,
                                     const char *const *sprites, const char *const *anims,
                                     int count) {
    for (int a = 0; a < count; ++a) {
        char key[160];
        snprintf(key, sizeof key, "Sprite/%s/%s/none", sprites[a], anims[a]);
        RuntimeLibraryEntry *entry = runtime_library_find(library, key);
        if (!entry || entry->count > MZM_SPRITE_MAX_FRAMES) {
            fprintf(stderr, "Zero Mission sprite library: %s %s (%s): %s\n", sprites[a],
                    anims[a], key, entry ? "too many frames" : "missing");
            return false;
        }
        kind->counts[a] = (uint8_t)entry->count;
        for (int f = 0; f < entry->count; ++f) kind->timers[a][f] = (uint8_t)entry->ticks[f];
    }
    return true;
}

static bool runtime_effect_kind_open(uint8_t *timers, uint8_t *count, RuntimeLibrary *library,
                                     const char *table) {
    char key[160];
    snprintf(key, sizeof key, "Particle/%s/none", table);
    RuntimeLibraryEntry *entry = runtime_library_find(library, key);
    if (!entry || entry->count > MZM_EFFECT_MAX_FRAMES) return false;
    *count = (uint8_t)entry->count;
    for (int f = 0; f < entry->count; ++f) timers[f] = (uint8_t)entry->ticks[f];
    return true;
}

static bool runtime_sprite_kinds_open(RuntimeSprites *sprites, RuntimeLibrary *library) {
    MzmEffectKinds *effects = &sprites->effect_kinds;
    for (int e = 0; e < MZM_PARTICLE_EFFECTS; ++e) {
        const char *table = mzm_particle_table((uint8_t)e);
        if (table && !runtime_effect_kind_open(effects->timers[e], &effects->counts[e], library,
                                               table))
            return false;
    }
    if (!runtime_effect_kind_open(effects->debris_timers, &effects->debris_count, library,
                                  "SpriteDebrisOam"))
        return false;
    /* The banner's durations do not depend on its message. */
    sprites->banner_ok = sprites->message_names[MZM_MESSAGE_ENERGY_TANK_ACQUIRED][0] != '\0';
    for (int t = 0; t < MZM_BANNER_TABLES && sprites->banner_ok; ++t) {
        char key[160];
        snprintf(key, sizeof key, "Banner/%s/%s/none",
                 sprites->message_names[MZM_MESSAGE_ENERGY_TANK_ACQUIRED], mzm_banner_tables[t]);
        RuntimeLibraryEntry *entry = runtime_library_find(library, key);
        if (!entry || entry->count >= 32) { sprites->banner_ok = false; break; }
        for (int f = 0; f < entry->count; ++f)
            sprites->banner_kind.timers[t][f] = (uint8_t)entry->ticks[f];
    }
    if (!sprites->banner_ok)
        fprintf(stderr, "Zero Mission message banner disabled until re-exported "
                "(python3 -m scripts.mzm_sprites)\n");
    /* Drops are required (every death may leave one); an enemy family with
     * a missing table is disabled alone, with the diagnostic above. */
    for (int f = 0; f < MZM_SPRITE_FAMILIES; ++f) {
        const MzmSpriteFamily *family = &mzm_sprite_families[f];
        sprites->family_ok[f] = runtime_sprite_kind_open(&sprites->kinds[f], library,
                                                         family->library_sprites, family->anims,
                                                         family->count);
        if (!sprites->family_ok[f] && f == MZM_FAMILY_DROP) return false;
        if (!sprites->family_ok[f])
            fprintf(stderr, "Zero Mission %s sprites disabled until re-exported "
                    "(python3 -m scripts.mzm_sprites)\n", family->library_sprites[0]);
    }
    return true;
}

/* SpriteLoadRoomSprites: the ported sprites of the room. */
static void runtime_sprites_spawn(RuntimeSprites *sprites, const Room *room) {
    memset(sprites->live, 0, sizeof sprites->live);
    memset(&sprites->effects, 0, sizeof sprites->effects);
    if (!sprites->tables_ok) return;
    int slot = 0;
    for (int i = 0; i < room->sprite_count && slot < RUNTIME_MAX_SPRITES; ++i) {
        for (int id = 0; id < 256; ++id) {
            if (strcmp(sprites->names[id], room->sprites[i].name) ||
                !mzm_sprite_supported((uint8_t)id) ||
                !sprites->family_ok[mzm_sprite_family((uint8_t)id, false)])
                continue;
            /* SpriteInitPrimary: the sprite owns the slot it takes. */
            mzm_sprite_init_primary(&sprites->live[slot], (uint8_t)id,
                                    (uint8_t)room->sprites[i].gfx_slot,
                                    (uint16_t)room->sprites[i].y, (uint16_t)room->sprites[i].x,
                                    (uint8_t)room->sprites[i].room_slot, (uint8_t)slot);
            slot++;
            break;
        }
    }
}

/* ClipdataProcess's block lookup for sprites: the room's native types. */
static uint8_t runtime_sprite_block(void *context, uint16_t y, uint16_t x) {
    const Room *room = context;
    int column = x / 64, row = y / 64;
    if (column >= room->width / 16 || row >= room->height / 16) return MZM_CLIPDATA_AIR;
    return (uint8_t)room_cell(room, column, row);
}

/* ClipdataProcess's movement for sprites (CLIPDATA_MOVEMENT_NONE outside). */
static uint8_t runtime_sprite_movement(void *context, uint16_t y, uint16_t x) {
    const Room *room = context;
    int column = x / 64, row = y / 64;
    if (column >= room->width / 16 || row >= room->height / 16)
        return MZM_CLIPDATA_MOVEMENT_NONE;
    return room->movements[row * (room->width / 16) + column];
}

/* ClipdataProcessForSamus: past the room's right edge (or left of it, as
 * the u16 position wraps) a solid block, below it air. */
static uint32_t runtime_samus_clipdata(void *context, uint16_t y, uint16_t x) {
    const Room *room = context;
    if (x / 64 >= room->width / 16) return MZM_SAMUS_CLIP_SOLID_FLAG | CLIP_SOLID;
    if (y / 64 >= room->height / 16) return CLIP_AIR;
    MzmSpriteWorld world = {(void *)room, runtime_sprite_block, 0, 0, NULL, 0};
    return mzm_clipdata_process(&world, y, x, MZM_ACTOR_SAMUS);
}

/* ClipdataCheckCurrentAffectingAtPosition: the block's movement (no
 * hazard: the runtime has no liquids, heat or cold). The elevator blocks are
 * reported as none, as the elevator pads and their room changes are not
 * ported. */
static uint32_t runtime_samus_affecting(void *context, uint16_t y, uint16_t x) {
    uint8_t movement = runtime_sprite_movement(context, y, x);
    if (movement == MZM_SAMUS_MOVEMENT_ELEVATOR_DOWN_BLOCK ||
        movement == MZM_SAMUS_MOVEMENT_ELEVATOR_UP_BLOCK)
        movement = MZM_CLIPDATA_MOVEMENT_NONE;
    return (uint32_t)movement << 16;
}

/* ClipdataCheckGroundEffect: sGroundEffectsClipdataValues of the block's
 * behavior (CLIP_BEHAVIOR_WET_GROUND .. CLIP_BEHAVIOR_VERY_DUSTY_GROUND). */
static uint8_t runtime_samus_ground_effect(void *context, uint16_t y, uint16_t x) {
    static const uint8_t effects[5] = {
        MZM_GROUND_EFFECT_WET_GROUND, MZM_GROUND_EFFECT_DUSTY_GROUND, MZM_GROUND_EFFECT_UNKNOWN,
        MZM_GROUND_EFFECT_BUBBLY_GROUND, MZM_GROUND_EFFECT_VERY_DUSTY_GROUND,
    };
    const Room *room = context;
    int column = x / 64, row = y / 64;
    if (column >= room->width / 16 || row >= room->height / 16) return MZM_GROUND_EFFECT_NONE;
    uint16_t behavior = runtime_clip_behavior(room->clip[row * (room->width / 16) + column]);
    return (uint16_t)(behavior - 0x50) < 5 ? effects[behavior - 0x50] : MZM_GROUND_EFFECT_NONE;
}

/* SpriteUtilTakeDamageFromSprite (normal difficulty) and the knockback of
 * SpriteUtilSamusAndSpriteCollision. */
typedef struct {
    MzmSamus *samus;
    MzmEquipment *equipment;
    const MzmCollision *collision;
} RuntimeSpriteHurt;

static void runtime_sprite_hurt(void *context, uint16_t damage, bool sprite_on_left) {
    RuntimeSpriteHurt *hurt = context;
    bool alive = mzm_equipment_damage(hurt->equipment, damage);
    mzm_samus_hurt(hurt->samus, hurt->collision, !alive);
    /* SUB_PIXEL_TO_VELOCITY(EIGHTH_BLOCK_SIZE) away from the sprite. */
    if (alive) hurt->samus->x_velocity = (int16_t)(sprite_on_left ? -64 : 64);
}

/* MzmSpriteHooks of the runtime. */
static bool runtime_sprite_check_event(void *context, uint8_t event) {
    const RuntimeSprites *sprites = context;
    return sprites->events[event / 8] & (1u << (event % 8));
}

static void runtime_sprite_set_event(void *context, uint8_t event) {
    RuntimeSprites *sprites = context;
    sprites->events[event / 8] |= (uint8_t)(1u << (event % 8));
}

/* gDoorUnlockTimer: the native door lock lifecycle is not ported, so the
 * request is only reported. */
static void runtime_sprite_door_timer(void *context, int8_t timer) {
    (void)context;
    fprintf(stderr, "Zero Mission sprites: gDoorUnlockTimer = %d (door locks not ported)\n",
            timer);
}

/* CAA_REMOVE_SOLID: the block's collision becomes air; the static background
 * keeps its graphics (BgClipSetBg1BlockValue is not reproduced). */
static void runtime_sprite_remove_solid(void *context, uint16_t y, uint16_t x) {
    RuntimeSprites *sprites = context;
    Room *room = sprites->room;
    int column = x / 64, row = y / 64;
    if (room && column < room->width / 16 && row < room->height / 16)
        room_set_cell(room, column, row, CLIP_AIR);
}

/* SpawnWaitingPirates after a door transition: the arrival position of
 * RoomLoad (Samus's door exit, half a block back toward the door) and
 * whether the arrival door has a hatch. */
static void runtime_sprites_door_arrival(RuntimeSprites *sprites, const Room *room,
                                         const RoomDoor *used) {
    if (!sprites->tables_ok) return;
    uint8_t ids[MAX_SPRITESET_ENTRIES];
    int count = 0;
    for (int i = 0; i < room->spriteset_count; ++i)
        for (int id = 0; id < 256; ++id)
            if (!strcmp(sprites->names[id], room->spriteset_names[i])) {
                ids[count++] = (uint8_t)id;
                break;
            }
    int32_t x = used->dest_x * MZM_BLOCK_SIZE + (used->dest_x_exit + 8) * 4;
    int32_t y = (used->dest_y_end + 1) * MZM_BLOCK_SIZE + used->dest_y_exit * 4 - 1;
    if (used->dest_x_exit > 0) x -= MZM_BLOCK_SIZE / 2;
    else if (used->dest_x_exit < 0) x += MZM_BLOCK_SIZE / 2;
    bool no_hatch = false;
    for (int i = 0; i < room->door_count; ++i)
        if (room->doors[i].x0 == used->dest_x && room->doors[i].y1 == used->dest_y_end)
            no_hatch = !strcmp(room->doors[i].kind, "nohatch");
    static const MzmSpriteHooks hooks = {NULL, runtime_sprite_check_event, NULL, NULL, NULL,
                                         NULL, NULL, NULL};
    MzmSpriteHooks bound = hooks;
    bound.context = sprites;
    mzm_spawn_waiting_pirates(sprites->live, RUNTIME_MAX_SPRITES, &sprites->globals, &bound, ids,
                              count, (uint8_t)room->spriteset, (uint16_t)y, (uint16_t)x,
                              no_hatch);
}

/* BlockApplyCcaa from a sprite (the Space Pirate laser): hatches. */
static void runtime_sprite_affect(void *context, uint16_t y, uint16_t x, int damage) {
    RuntimeSprites *sprites = context;
    if (sprites->room) runtime_room_affect(sprites->room, x, y, damage);
}

/* ClipdataProcess with a sprite's clipdata-affecting action. */
static void runtime_sprite_apply_caa(void *context, uint16_t y, uint16_t x, uint8_t caa) {
    RuntimeSprites *sprites = context;
    if (!sprites->room || !runtime_block_tables.ok) return;
    MzmBlockEnv env = runtime_block_env(sprites->room);
    mzm_blocks_affect(&sprites->room->blocks, &env, caa, x, y);
}

/* SpriteSpawnPrimary(PSPRITE_MESSAGE_BANNER, message, ...) */
static void runtime_sprite_spawn_message(void *context, uint8_t message) {
    RuntimeSprites *sprites = context;
    if (message < MZM_MESSAGE_COUNT) mzm_banner_spawn(&sprites->banner, message);
}

/* SpriteUtilCheckCrouchingOrMorphed for the ported Samus poses. */
static bool runtime_samus_small(const MzmSamus *samus) {
    switch (samus->pose) {
    case MZM_POSE_CROUCHING:
    case MZM_POSE_TURNING_AROUND_AND_CROUCHING:
    case MZM_POSE_SHOOTING_AND_CROUCHING:
    case MZM_POSE_MORPHING:
    case MZM_POSE_MORPH_BALL:
    case MZM_POSE_ROLLING:
    case MZM_POSE_UNMORPHING:
    case MZM_POSE_MORPH_BALL_MIDAIR:
    case MZM_POSE_GETTING_HURT_IN_MORPH_BALL:
        return true;
    default:
        return false;
    }
}

/* gCurrentArea (AreaId) of an exported area name. */
static uint8_t runtime_area_id(const char *area) {
    static const char *const names[] = {"Brinstar", "Kraid", "Norfair", "Ridley", "Tourian",
                                        "Crateria", "Chozodia", "Test1", "Test2", "Test3"};
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i)
        if (!strcmp(area, names[i])) return (uint8_t)i;
    return 0xFF;
}

/* SpriteUpdate: Samus's contact with the sprites, then every sprite. */
/* SpriteUtilSpriteTakeDamageFromSamusContact's DCT_PSEUDO_SCREW: a full
 * charge while spinning (not screw attacking), 4 * (2 + one per beam). */
static uint16_t runtime_pseudo_screw_damage(const MzmSamus *samus,const MzmEquipment *equipment,
                                            const MzmWeapons *weapons) {
    if(!weapons || weapons->charge_counter<MZM_CHARGE_FULL)return 0;
    switch(samus->pose) {
        case MZM_POSE_STARTING_SPIN_JUMP: case MZM_POSE_SPINNING:
        case MZM_POSE_STARTING_WALL_JUMP: case MZM_POSE_SPACE_JUMPING: break;
        default: return 0;
    }
    uint32_t items=equipment->items;
    return (uint16_t)(4*(2+((items&MZM_ITEM_LONG_BEAM)?1:0)+((items&MZM_ITEM_ICE_BEAM)?1:0)+
                           ((items&MZM_ITEM_WAVE_BEAM)?1:0)+((items&MZM_ITEM_PLASMA_BEAM)?1:0)));
}

/* The kills of a sprite Samus stands on drop her to STANDING_MIDAIR. */
static void runtime_sprites_drop_samus(RuntimeSprites *sprites, MzmSamus *samus) {
    if (mzm_sprites_samus_dropped(sprites->live, RUNTIME_MAX_SPRITES)) samus->on_enemy = false;
}

static void runtime_sprites_update(RuntimeSprites *sprites, Room *room, float cx, float cy,
                                   MzmSamus *samus, int32_t previous_x, MzmEquipment *equipment,
                                   const MzmCollision *collision, MzmWeapons *weapons,
                                   const MzmInput *input, bool stopped,
                                   uint16_t *prevent_movement) {
    if (!sprites->kinds_ok) return;
    /* gEffectYPosition without gWaterMovement's wave (not ported). */
    uint16_t effect_y = room->effect_y == 255 ? 0 : (uint16_t)(room->effect_y * 64);
    MzmSpriteWorld world = {room, runtime_sprite_block, (uint16_t)((int)cx * 4),
                            (uint16_t)((int)cy * 4), runtime_sprite_movement, effect_y};
    mzm_sprite_debris_process(&sprites->effects, &world, &sprites->tables,
                              &sprites->effect_kinds);
    /* SpriteUtilCheckStopSpritesPose: no Samus collision while movement is
     * prevented. */
    if (samus->pose != MZM_POSE_DYING && !stopped) {
        RuntimeSpriteHurt hurt = {samus, equipment, collision};
        const MzmBlockHitbox *box = mzm_block_hitbox(mzm_pose_hitbox(samus->pose));
        /* The sprites see gSamusData.yPosition (the port's y - 1). */
        MzmSpriteSamus view = {&hurt, (uint16_t)samus->x, (uint16_t)(samus->y - 1),
                               (uint16_t)previous_x, box->top, 0, box->left, box->right,
                               samus->pose == MZM_POSE_SCREW_ATTACKING,
                               equipment->suit == MZM_SUIT_SUITLESS, samus->invincibility != 0,
                               runtime_sprite_hurt, samus->speedboosting,
                               samus->pose==MZM_POSE_SHINESPARKING ||
                                   samus->pose==MZM_POSE_BALLSPARKING,
                               runtime_pseudo_screw_damage(samus,equipment,weapons),
                               weapons?&weapons->charge_counter:NULL, &world, samus->y_velocity,
                               samus->pose == MZM_POSE_PULLING_YOURSELF_UP_FROM_HANGING ||
                                   samus->pose == MZM_POSE_PULLING_YOURSELF_FORWARD_FROM_HANGING,
                               mzm_pose_hitbox(samus->pose) == MZM_HITBOX_MORPHED,
                               (int8_t)(samus->facing > 0 ? 1 : -1), equipment->energy != 0,
                               false, false};
        mzm_sprites_samus_collision(sprites->live, RUNTIME_MAX_SPRITES, &view, &sprites->tables);
        /* The solid (frozen) sprites' pushes. */
        samus->x = view.x;
        samus->y = (int32_t)view.y + 1;
        if (view.stop_x) samus->x_velocity = 0;
        if (view.stop_y) samus->y_velocity = 0;
    }
    /* SpriteUtilSamusStandingOnSprite runs after each sprite's AI. */
    MzmSpriteStanding standing = {samus->on_enemy,
                                  !samus->on_enemy &&
                                      mzm_pose_standing(samus->pose) == MZM_STANDING_MIDAIR,
                                  (uint16_t)(samus->y - 1), (uint16_t)(samus->copy_y - 1),
                                  false, false};
    sprites->room = room;
    static const MzmSpriteHooks hook_table = {NULL, runtime_sprite_check_event,
                                              runtime_sprite_set_event, runtime_sprite_door_timer,
                                              runtime_sprite_remove_solid, runtime_sprite_affect,
                                              runtime_sprite_apply_caa,
                                              runtime_sprite_spawn_message};
    MzmSpriteHooks hooks = hook_table;
    hooks.context = sprites;
    /* Samus for the Chozo statues' hands (native y). */
    MzmSpriteSamusLink link = {
        samus->pose == MZM_POSE_MORPH_BALL ? MZM_LINK_POSE_MORPH_BALL :
        samus->pose == MZM_POSE_ROLLING ? MZM_LINK_POSE_ROLLING :
        samus->pose == MZM_POSE_GRABBED_BY_CHOZO_STATUE ? MZM_LINK_POSE_GRABBED :
                                                          MZM_LINK_POSE_OTHER,
        (uint16_t)(samus->y - 1), (uint16_t)samus->x, false, false, false};
    MzmSpriteFrame frame = {&world, &sprites->tables, {NULL}, equipment, sprites->frame8,
                            sprites->frame16, (uint16_t)samus->x, (uint16_t)samus->y,
                            mzm_block_hitbox(mzm_pose_hitbox(samus->pose))->top,
                            sprites->live, RUNTIME_MAX_SPRITES, &sprites->effects, &hooks,
                            &sprites->globals, runtime_samus_small(samus), samus->x_velocity,
                            runtime_area_id(room->area), (uint8_t)room->room,
                            samus->pose != MZM_POSE_DYING ? &standing : NULL, stopped,
                            prevent_movement, &link};
    for (int f = 0; f < MZM_SPRITE_FAMILIES; ++f)
        frame.kinds[f] = sprites->family_ok[f] ? &sprites->kinds[f] : NULL;
    for (int i = 0; i < RUNTIME_MAX_SPRITES; ++i)
        mzm_sprite_update(&sprites->live[i], (uint8_t)i, &frame);
    if (!stopped) mzm_sprites_end_frame(&sprites->globals, &hooks);
    /* The statues' SamusSetPose calls and hand positions. */
    if (link.grab) mzm_samus_chozo_grab(samus, true, equipment, collision);
    if (link.moved) {
        samus->x = link.x;
        samus->y = (int32_t)link.y + 1;
    }
    if (link.release) mzm_samus_chozo_grab(samus, false, equipment, collision);
    if (samus->pose != MZM_POSE_DYING && !stopped) {
        if (standing.landing_request)
            mzm_samus_land_on_sprite(samus, input, equipment, collision);
        if (standing.y_set) samus->y = (int32_t)standing.y + 1;
        samus->on_enemy = standing.on_enemy;
    }
    runtime_sprites_drop_samus(sprites, samus);
    sprites->frame8++;
    sprites->frame16++;
}
/* The room's background art (scripts/mzm_bg_layers.py): the tileset's
 * metatile sheet and the LZ77 text layers, reloaded when the room changes. */
typedef struct {
    const Room *room;
    char alias[64];
    int sheet_tileset;
    SDL_Texture *sheet;
    SDL_Texture *text[4];
} RuntimeBgArt;

static SDL_Texture *runtime_bg_texture(SDL_Renderer *renderer, const char *path) {
    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) return NULL;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture) {
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    return texture;
}

static void runtime_bg_art_sync(RuntimeBgArt *art, SDL_Renderer *renderer, const Room *room,
                                const char *alias) {
    if (art->room == room && !strcmp(art->alias, alias)) return;
    art->room = room;
    snprintf(art->alias, sizeof art->alias, "%s", alias);
    for (int i = 0; i < 4; ++i) {
        if (art->text[i]) SDL_DestroyTexture(art->text[i]);
        art->text[i] = NULL;
    }
    if (!room->has_backgrounds) return;
    char path[256];
    if (!art->sheet || art->sheet_tileset != room->tileset) {
        if (art->sheet) SDL_DestroyTexture(art->sheet);
        snprintf(path, sizeof path, "assets/extracted/metroid/rooms/runtime/tilesets/tileset_%02d.bmp",
                 room->tileset);
        art->sheet = runtime_bg_texture(renderer, path);
        art->sheet_tileset = room->tileset;
        if (!art->sheet) fprintf(stderr, "Missing tileset sheet %s (python3 -m scripts.mzm_runtime_room --all)\n", path);
    }
    for (int layer = 0; layer < 4; ++layer) {
        if (!room->text_present[layer]) continue;
        snprintf(path, sizeof path, "assets/extracted/metroid/rooms/runtime/%s/bg%d.bmp", alias,
                 layer);
        art->text[layer] = runtime_bg_texture(renderer, path);
    }
}

static void runtime_bg_art_free(RuntimeBgArt *art) {
    for (int i = 0; i < 4; ++i)
        if (art->text[i]) SDL_DestroyTexture(art->text[i]);
    if (art->sheet) SDL_DestroyTexture(art->sheet);
    *art = (RuntimeBgArt){0};
}

/* TransparencySetRoomEffectsTransparency: BG priorities by the room's
 * transparency (BG3 is always 3) and whether Samus is on top of them. */
static int runtime_bg_priority(const Room *room, int layer) {
    int t = room->transparency;
    if (layer == 3) return 3;
    if (t >= 5 && t <= 0x33 && (t & 3) == 3)
        return layer == 0 ? 2 : layer == 1 ? 0 : 1;
    if (t >= 5 && t <= 0x33 && (t & 3) != 0)
        return layer == 0 ? 1 : layer == 1 ? 0 : 2;
    return layer;
}

static bool runtime_samus_on_top(const Room *room) {
    int t = room->transparency;
    return t >= 6 && t <= 0x33 && (t & 3) >= 2;
}

/* The BG0 alpha blend (BLDALPHA): additive at EVA/16 when EVB is 16,
 * else a mix at EVA = 16 - EVB; none below 8 or with transparency
 * disabled. Returns false for an opaque BG0. */
static bool runtime_bg0_blend(const Room *room, int *eva, bool *additive) {
    int t = room->transparency & ~3;
    if (room->transparency < 8 || room->bg0_prop == 0x12) return false;
    static const int evas[] = {7, 10, 13, 16};
    static const int evbs[] = {0, 3, 6, 9, 11, 13, 16};
    if (t >= 0x8 && t <= 0x14) {
        *eva = evas[(t - 0x8) / 4];
        *additive = true;
    } else if (t >= 0x18 && t <= 0x30) {
        *eva = 16 - evbs[(t - 0x18) / 4];
        *additive = false;
    } else {
        return false;
    }
    return true;
}

/* ScrollBg3 (gBg3XPosition / gBg3YPosition) in pixels, from the camera
 * as gBg1 position. */
static void runtime_bg3_position(const Room *room, float cx, float cy, int *x, int *y) {
    static const int types[11][2] = {
        {0, 0}, {2, 0}, {0, 2}, {2, 2}, {1, 2}, {2, 1}, {1, 1}, {1, 0}, {1, 0}, {4, 0}, {1, 1},
    };
    int scrolling = room->bg3_scrolling >= 0 && room->bg3_scrolling <= 10 ? room->bg3_scrolling : 0;
    int x_type = types[scrolling][0], y_type = types[scrolling][1];
    int bg1_x = (int)cx * 4, bg1_y = (int)cy * 4;
    int sub_x = x_type == 1 ? bg1_x - 128 : x_type == 2 ? (bg1_x - 128) >> 1 :
                x_type == 4 ? (bg1_x - 128) >> 2 : 0;
    int sub_y;
    if (room->bg3_prop == 0x46) {
        /* BG_PROP_STARTS_FROM_BOTTOM */
        int size = (room->height / 16 - (10 + 2)) * 64;
        int offset = (room->text_size[3] & 2 ? 0x800 : 0x400) - 0x280;
        if (y_type == 0) offset = size = 0;
        else if (y_type == 1) size -= bg1_y;
        else size = (size - bg1_y) >> 4;
        sub_y = offset - size > 0 ? offset - size : 0;
    } else {
        sub_y = y_type == 1 ? bg1_y - 128 : y_type == 2 ? (bg1_y - 128) >> 1 :
                y_type == 4 ? (bg1_y - 128) >> 2 : 0;
    }
    *x = sub_x / 4;
    *y = sub_y / 4;
}

/* A destination rectangle whose edges are rounded to output pixels, so
 * that neighbouring blocks meet without seams at fractional scales. */
static SDL_FRect runtime_snap_rect(SDL_FRect viewport, float scale, float x, float y, float w,
                                   float h) {
    float x0 = SDL_roundf(viewport.x + x * scale), y0 = SDL_roundf(viewport.y + y * scale);
    float x1 = SDL_roundf(viewport.x + (x + w) * scale), y1 = SDL_roundf(viewport.y + (y + h) * scale);
    return (SDL_FRect){x0, y0, x1 - x0, y1 - y0};
}

/* One text background layer, wrapped at its size. */
static void runtime_draw_text_layer(SDL_Renderer *renderer, SDL_Texture *texture, int w, int h,
                                    int scroll_x, int scroll_y, SDL_FRect viewport, float scale) {
    int ox = ((scroll_x % w) + w) % w, oy = ((scroll_y % h) + h) % h;
    for (int y = -oy; y < 224; y += h)
        for (int x = -ox; x < 320; x += w) {
            SDL_FRect dst = runtime_snap_rect(viewport, scale, (float)x, (float)y, (float)w,
                                              (float)h);
            SDL_RenderTexture(renderer, texture, NULL, &dst);
        }
}

/* One block layer from the metatile sheet. */
static void runtime_draw_block_layer(SDL_Renderer *renderer, SDL_Texture *sheet, const Room *room,
                                     int layer, float cx, float cy, SDL_FRect viewport,
                                     float scale) {
    int w = room->bg_w[layer], h = room->bg_h[layer];
    int x0 = (int)cx / 16, y0 = (int)cy / 16;
    for (int by = y0; by <= y0 + 224 / 16 + 1 && by < h; ++by)
        for (int bx = x0; bx <= x0 + 320 / 16 + 1 && bx < w; ++bx) {
            int value = room->bg[layer][by * w + bx];
            SDL_FRect src = {(float)(value % 32 * 16), (float)(value / 32 * 16), 16.f, 16.f};
            SDL_FRect dst = runtime_snap_rect(viewport, scale, (float)(bx * 16) - cx,
                                              (float)(by * 16) - cy, 16.f, 16.f);
            SDL_RenderTexture(renderer, sheet, &src, &dst);
        }
}

/* The backgrounds whose priority lies in [low, high], back to front: by
 * priority, then BG3 to BG0 (a lower BG wins at equal priority). */
static void runtime_draw_backgrounds(SDL_Renderer *renderer, const RuntimeBgArt *art,
                                     const Room *room, float cx, float cy, SDL_FRect viewport,
                                     float scale, int low, int high) {
    if (!room->has_backgrounds || !art->sheet) return;
    for (int priority = high; priority >= low; --priority)
        for (int layer = 3; layer >= 0; --layer) {
            if (runtime_bg_priority(room, layer) != priority) continue;
            int eva = 16;
            bool additive = false, blend = layer == 0 && runtime_bg0_blend(room, &eva, &additive);
            SDL_Texture *texture = layer == 3 || (layer == 0 && art->text[0]) ? art->text[layer]
                                                                              : art->sheet;
            if (!texture) continue;
            if (blend) {
                Uint8 level = (Uint8)(eva * 255 / 16);
                SDL_SetTextureBlendMode(texture, additive ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
                if (additive) SDL_SetTextureColorMod(texture, level, level, level);
                else SDL_SetTextureAlphaMod(texture, level);
            }
            if (layer == 3 && art->text[3]) {
                int x, y;
                runtime_bg3_position(room, cx, cy, &x, &y);
                runtime_draw_text_layer(renderer, art->text[3], room->text_w[3], room->text_h[3],
                                        x, y, viewport, scale);
            } else if (layer == 0 && art->text[0]) {
                runtime_draw_text_layer(renderer, art->text[0], room->text_w[0], room->text_h[0],
                                        (int)cx, (int)cy, viewport, scale);
            } else if (layer < 3 && room->bg_present[layer]) {
                runtime_draw_block_layer(renderer, art->sheet, room, layer, cx, cy, viewport,
                                         scale);
            }
            if (blend) {
                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
                SDL_SetTextureColorMod(texture, 255, 255, 255);
                SDL_SetTextureAlphaMod(texture, 255);
            }
        }
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
    /* --input-script MASK:FRAMES,...: the GBA buttons of each captured frame
     * (after the script, the --capture buttons apply). */
    const char *input_script=NULL;
    unsigned long capture_buttons=0;
    bool check=false,animation_check=false,debug_at_start=false;
    int spawn_px=-1,spawn_py=-1;
    bool mute=false;
    const char *debug_input=NULL;
    /* --item NAME (repeatable): a debug menu item enabled at start, its label
     * in lower case with dashes (for example speed-booster). */
    uint32_t start_items=0;
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
        else if (!strcmp(argv[i],"--input-script") && i+1<argc) input_script=argv[++i];
        else if (!strcmp(argv[i],"--debug-menu")) debug_at_start=true;
        else if (!strcmp(argv[i],"--mute")) mute=true;
        else if (!strcmp(argv[i],"--item") && i+1<argc) {
            const char *name=argv[++i];
            bool found=false;
            for(int k=0;k<DEBUG_ITEMS_COUNT && !found;k++) {
                char slug[32];
                size_t n=0;
                for(const char *c=debug_items[k].label;*c && n+1<sizeof slug;c++)
                    slug[n++]=(char)(*c==' '?'-':(*c>='A'&&*c<='Z'?*c-'A'+'a':*c));
                slug[n]=0;
                if(!strcmp(slug,name)){start_items|=debug_items[k].item;found=true;}
            }
            if(!found){fprintf(stderr,"Unknown item: %s\n",name);return 2;}
        }
        else if (!strcmp(argv[i],"--spawn") && i+2<argc) {
            /* Test placement: Samus's feet at room pixel X, Y. */
            spawn_px=atoi(argv[++i]);
            spawn_py=atoi(argv[++i]);
        }
        else if (!strcmp(argv[i],"--debug-input") && i+1<argc) debug_input=argv[++i];
        else if (!strcmp(argv[i],"--input-map") && i+1<argc) {
            char error[256];
            if(!gba_pad_map_load(&pad_map,argv[++i],error,sizeof error)) {
                fprintf(stderr,"Input map: %s\n",error);
                return 2;
            }
        }
        else if (argv[i][0]=='-' || room_path) {
            fprintf(stderr,"Usage: %s [--check] [--background image.bmp] [--native-source source.tsv] [--samus-assets directory | --samus-library index.tsv --samus-map map.tsv] [--input-map map.txt] [--debug-menu] [--mute] [--item NAME] [--input-script MASK:FRAMES,...] [--debug-input MASK,...] [--spawn X Y] preview.tsv\n",argv[0]);
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
        fprintf(stderr,"Usage: %s [--check] [--background image.bmp] [--native-source source.tsv] [--samus-assets directory | --samus-library index.tsv --samus-map map.tsv] [--input-map map.txt] [--debug-menu] [--mute] [--item NAME] [--input-script MASK:FRAMES,...] [--debug-input MASK,...] [--spawn X Y] preview.tsv\n",argv[0]);
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
    /* Sound: none in captures (deterministic, no device) or with --mute. */
    SDL_AudioStream *audio=NULL;
    if (!capture_path && !mute) {
        SDL_AudioSpec spec={SDL_AUDIO_S16,2,RUNTIME_AUDIO_RATE};
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO) ||
            !(audio=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,NULL,NULL)))
            fprintf(stderr,"No audio device: %s\n",SDL_GetError());
        else if (!mzm_audio_open(RUNTIME_SOUND_BANK,RUNTIME_SOUND_TABLES)) {
            SDL_DestroyAudioStream(audio);
            audio=NULL;
        } else {
            mzm_audio_initialize();
            mzm_sfx_set_sink(runtime_sfx_sink,NULL);
            SDL_ResumeAudioStreamDevice(audio);
        }
    }
    SDL_Window *window=SDL_CreateWindow("Metroid Vania - experimental C11 runtime",
                                       960,640,SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer=window ? SDL_CreateRenderer(window,NULL) : NULL;
    SDL_Surface *surface=NULL; SDL_Texture *texture=NULL;
    int rc=1;
    RuntimeLibrary library = {0};
    RuntimeLibrary projectile_library = {0};
    RuntimeLibrary sprite_library = {0};
    static RuntimeSprites sprites;
    static RuntimeBgArt bg_art;
    float last_cx = 0.f, last_cy = 0.f;
    static RuntimePalettes palettes;
    RuntimePaletteSlots palette_slots={0};
    const char *palette_suit=NULL;
    unsigned int palette_frame=0;
    /* gPreventMovementTimer */
    uint16_t prevent_movement=0;
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
    /* Sprites (scripts/mzm_sprites.py); without them the rooms stay empty. */
    if (!mzm_power_bomb_load_windows(RUNTIME_POWER_BOMB_TABLE))
        fprintf(stderr, "No power bomb window table: python3 -m scripts.mzm_runtime_room --all\n");
    if (!runtime_block_tables_open(RUNTIME_BLOCK_TABLES))
        fprintf(stderr, "No Zero Mission block tables: python3 -m scripts.mzm_runtime_room --all\n");
    sprites.tables_ok = runtime_sprite_tables_open(&sprites, RUNTIME_SPRITE_TABLES);
    sprites.kinds_ok = sprites.tables_ok &&
                       runtime_library_open(&sprite_library, RUNTIME_SPRITE_LIBRARY) &&
                       runtime_sprite_kinds_open(&sprites, &sprite_library);
    if (!sprites.kinds_ok)
        fprintf(stderr, "No Zero Mission sprites: python3 -m scripts.mzm_sprites\n");
    runtime_sprites_spawn(&sprites, room);
    gba_input_init(&gamepad,&pad_map);
    MzmCollision collision={room,runtime_collision_blocked,runtime_collision_slope,
                            runtime_collision_point,runtime_collision_affect,
                            runtime_samus_clipdata,runtime_samus_affecting,
                            runtime_samus_destruct,runtime_samus_ground_effect};
    bool door_lock=false;
    MzmEquipment equipment={0};
    unsigned int suit_preset=0,spin_items=0;
    uint32_t toggled_items=start_items;
    runtime_apply_equipment(&equipment,suit_preset,toggled_items);
    equipment.max_energy=99;
    equipment.energy=equipment.max_energy;
    /* Diagnostic ammunition until item pickups exist. */
    equipment.max_missiles=10;
    equipment.missiles=equipment.max_missiles;
    equipment.max_super_missiles=2;
    equipment.super_missiles=equipment.max_super_missiles;
    equipment.max_power_bombs=2;
    equipment.power_bombs=equipment.max_power_bombs;
    MzmWeapons weapons;
    RuntimeBeamFx beam_fx={0};
    mzm_weapons_init(&weapons);
    MzmProjectileAnimation projectile_animation={&projectile_library,
                                                 runtime_projectile_durations};
    /* The registry suit selects the palettes. */
    struct { const char *suit; } pose_animation={runtime_suit_presets[0].registry};
    MzmAnimationSource animation_source={&library,runtime_pose_durations};
    MzmSamus samus;
    if(spawn_px>=0 && spawn_py>=0) {
        mzm_samus_init(&samus,(int32_t)spawn_px*MZM_SUBPIXELS_PER_PIXEL,
                       (int32_t)spawn_py*MZM_SUBPIXELS_PER_PIXEL,1);
    } else if(!runtime_spawn_samus(room,&samus)) {
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
    /* The first room's music starts at once. */
    if (runtime_room_music(room)) mzm_music_check_play_transition_track();
    int music_transition=0;
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
    runtime_blocks_load(room, current_room);
    runtime_hatches_load(room, current_room, -1);
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
    debug_menu_value(&debug,"Power bombs",&equipment.power_bombs,0,99,1,NULL,DEBUG_AMMO);
    debug_menu_value(&debug,"Max power bombs",&equipment.max_power_bombs,0,99,1,NULL,DEBUG_AMMO);
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
                equipment.power_bombs=equipment.max_power_bombs;
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
                if(equipment.power_bombs>equipment.max_power_bombs)
                    equipment.power_bombs=equipment.max_power_bombs;
                title_dirty=true;
                break;
            case DEBUG_REFILL:
                equipment.energy=equipment.max_energy;
                equipment.missiles=equipment.max_missiles;
                equipment.super_missiles=equipment.max_super_missiles;
                equipment.power_bombs=equipment.max_power_bombs;
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
                    runtime_sprites_spawn(&sprites, room);
                    runtime_blocks_load(room,alias);
                    runtime_hatches_load(room,alias,-1);
                    mzm_weapons_init(&weapons);
                    echo=(RuntimeEcho){0};
                    echo_visible=false;
                    door_lock=true;
                    title_dirty=true;
                    snprintf(current_room,sizeof current_room,"%s",alias);
                    music_transition=runtime_room_music(room);
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
                if(input_script) {
                    /* The entry covering this step, if any. */
                    const char *cursor=input_script;
                    long start=0;
                    while(*cursor) {
                        char *end=NULL;
                        unsigned long mask=strtoul(cursor,&end,0);
                        long count=*end==':'?strtol(end+1,&end,10):1;
                        if(capture_step>=start && capture_step<start+count) {
                            held=(uint16_t)(mask&0x1FFu);
                            break;
                        }
                        start+=count;
                        if(*end!=',')break;
                        cursor=end+1;
                    }
                }
                if(capture_repeat && capture_step%capture_repeat==capture_repeat-1)
                    held&=(uint16_t)~(MZM_KEY_A|MZM_KEY_B);
                capture_step++;
            }
            if(debug.open && !capture_path)held=0;     /* a menu frame step */
            MzmInput input={held,(uint16_t)((held&~previous_held)|latched),false,0,0};
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
                    equipment.power_bombs=equipment.max_power_bombs;
                    /* SamusDying stopped the music; there is no game over
                     * flow, so the room's track simply starts again. */
                    if(mzm_audio_ready() && room->has_music) {
                        mzm_music_check_set_new_track((uint16_t)room->music);
                        mzm_music_check_play_transition_track();
                        if(!mzm_audio_player_playing(0))
                            mzm_music_play((uint16_t)room->music,0);
                    }
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
            int32_t previous_x=samus.x;    /* gPreviousXPosition */
            /* gPreventMovementTimer: SamusUpdate (with its weapon checks)
             * waits while it runs down. */
            bool movement_prevented=prevent_movement!=0;
            if(movement_prevented) {
                prevent_movement--;
                input.new_projectile=false;
            } else {
                input.new_projectile=mzm_weapons_begin_frame(
                    &weapons,&samus,input.held,input.pressed,&equipment);
                input.weapon_highlighted=runtime_weapon_highlighted(&weapons);
                input.charge_counter=weapons.charge_counter;
                /* gBg1X/YPosition of the GBA's 240x160 screen centred in the
                 * wider view. */
                samus.camera_x=(int32_t)last_cx*4+(320-240)*2;
                samus.camera_y=(int32_t)last_cy*4+(224-160)*2;
                mzm_samus_update(&samus,&input,&equipment,&collision,&animation_source);
            }
            palette_frame++;
            /* ProjectileUpdate reads the arm cannon offset of the pose and
             * frame Samus has after her update. */
            int cannon_x=0,cannon_y=0;
            /* SamusUpdateArmCannonPositionOffset: the offset of the arm
             * cannon animation frame SamusUpdateGraphicsOam pairs with the
             * body. */
            RuntimeLibraryEntry *cannon_entry=library.count?runtime_samus_entry(
                &library,&samus,&equipment,runtime_weapon_highlighted(&weapons)):NULL;
            if(cannon_entry && cannon_entry->count>0) {
                int index=samus.anim_frame<cannon_entry->count?
                    samus.anim_frame:cannon_entry->count-1;
                if(cannon_entry->frames[index].has_cannon) {
                    cannon_x=cannon_entry->frames[index].cannon_x;
                    cannon_y=cannon_entry->frames[index].cannon_y;
                }
            }
            /* SpriteUpdate runs between SamusUpdate and ProjectileUpdate; the
             * arm cannon offset above is the one SamusUpdate left. */
            runtime_sprites_update(&sprites, room, last_cx, last_cy, &samus, previous_x,
                                   &equipment, &collision, &weapons, &input, movement_prevented,
                                   &prevent_movement);
            if(sprites.banner.exists && sprites.banner_ok) {
                /* MessageBanner, a sprite updated with the others. */
                bool keys=(input.held&(MZM_KEY_A|MZM_KEY_B|MZM_KEY_RIGHT|MZM_KEY_LEFT|
                                       MZM_KEY_UP|MZM_KEY_DOWN))!=0;
                unsigned events=mzm_banner_update(&sprites.banner,&sprites.banner_kind,keys,
                                                  runtime_tank_collection.collecting,
                                                  &prevent_movement);
                if(events&MZM_BANNER_FINISH_TANK) runtime_finish_collecting_tank(room);
            }
            weapons.samus_previous_x=previous_x;
            weapons.frame8=sprites.frame8;
            /* gBg1X/YPosition for ProjectileCheckDespawn. */
            weapons.has_camera=true;
            weapons.camera_x=(uint16_t)((int)last_cx*4);
            weapons.camera_y=(uint16_t)((int)last_cy*4);
            mzm_weapons_update(&weapons,&samus,&equipment,cannon_x,cannon_y,
                               &collision,&projectile_animation);
            {
                /* ParticleChargingBeam and ParticleShootingBeam*: their
                 * animations, at the arm cannon. */
                char key[160];
                const char *variant=runtime_beam_variant(equipment.items);
                uint8_t stage=weapons.charge_particle.stage;
                if(stage!=beam_fx.charge_stage){beam_fx.charge_frame=0;beam_fx.charge_counter=0;}
                beam_fx.charge_stage=stage;
                if(stage) {
                    snprintf(key,sizeof key,"Particle/ParticleChargingBeam%sOam/%s/none",
                             stage==1?"Begin":"Charged",variant);
                    runtime_anim_step(runtime_library_find(&projectile_library,key),
                                      &beam_fx.charge_frame,&beam_fx.charge_counter);
                }
                if(beam_fx.flash.active) {
                    snprintf(key,sizeof key,"Particle/%s/%s/%s",beam_fx.flash.table,variant,
                             beam_fx.flash.flip?"x":"none");
                    if(runtime_anim_step(runtime_library_find(&projectile_library,key),
                                         &beam_fx.flash.frame,&beam_fx.flash.counter))
                        beam_fx.flash.active=false;
                }
                if(weapons.shot_flash) {
                    MzmAim aim=weapons.shot_flash_aim;
                    beam_fx.flash.active=true;
                    beam_fx.flash.table=aim==MZM_AIM_UP?"ParticleShootingBeamUpOam":
                        aim==MZM_AIM_DOWN?"ParticleShootingBeamDownOam":
                        aim==MZM_AIM_DIAGONAL_UP?"ParticleShootingBeamDiagonallyUpOam":
                        aim==MZM_AIM_DIAGONAL_DOWN?"ParticleShootingBeamDiagonallyDownOam":
                        "ParticleShootingBeamHorizontalOam";
                    beam_fx.flash.flip=weapons.shot_flash_facing>0;
                    beam_fx.flash.x=(samus.x>>2)+cannon_x;
                    beam_fx.flash.y=((samus.y-1)>>2)+cannon_y;
                    beam_fx.flash.frame=0;
                    beam_fx.flash.counter=0;
                }
                /* gArmCannonX/Y for the charging particle. */
                beam_fx.cannon_x=(samus.x>>2)+cannon_x;
                beam_fx.cannon_y=((samus.y-1)>>2)+cannon_y;
            }
            /* ProjectileCheckSamusBombBounce -> SamusBombBounce */
            if(weapons.bomb_bounce)
                mzm_samus_bomb_bounce(&samus,&equipment,&collision,weapons.bomb_bounce);
            if(music_transition && --music_transition==0)
                mzm_music_check_play_transition_track();
            if(audio)runtime_audio_frame(audio);
            /* ConnectionUpdateHatches */
            mzm_hatches_update(room->native_hatches,room->native_hatch_count,&room->hatch_env);
            runtime_blocks_frame(room,current_room,&samus,&equipment,sprites.frame8);
            /* RoomUpdate: HazeProcess, then PowerBombExplosionProcess. */
            mzm_power_bomb_haze(&weapons.power_bomb);
            {
                MzmPowerBombRoom pb_room={room,room->width/16,room->height/16,
                                          runtime_block_get_clip,runtime_power_bomb_apply};
                mzm_power_bomb_process(&weapons.power_bomb,&pb_room,sprites.frame8);
            }
            if(sprites.kinds_ok) {
                /* The wall impacts' particles, then ProjectileCheckHittingSprite
                 * ends ProjectileUpdate; ParticleProcessAll follows. */
                static const uint8_t impact_effects[]={
                    [MZM_PROJECTILE_BEAM]=MZM_PE_HITTING_SOMETHING_WITH_NORMAL_BEAM,
                    [MZM_PROJECTILE_MISSILE]=MZM_PE_HITTING_SOMETHING_WITH_MISSILE,
                    [MZM_PROJECTILE_SUPER_MISSILE]=MZM_PE_HITTING_SOMETHING_WITH_SUPER_MISSILE,
                    [MZM_PROJECTILE_BOMB]=MZM_PE_BOMB,
                    [MZM_PROJECTILE_POWER_BOMB]=MZM_PE_BOMB,
                    [MZM_PROJECTILE_LONG_BEAM]=MZM_PE_HITTING_SOMETHING_WITH_LONG_BEAM,
                    [MZM_PROJECTILE_CHARGED_BEAM]=MZM_PE_HITTING_SOMETHING_WITH_NORMAL_BEAM,
                    [MZM_PROJECTILE_CHARGED_LONG_BEAM]=MZM_PE_HITTING_SOMETHING_WITH_LONG_BEAM,
                    [MZM_PROJECTILE_ICE_BEAM]=MZM_PE_HITTING_SOMETHING_WITH_ICE_BEAM,
                    [MZM_PROJECTILE_CHARGED_ICE_BEAM]=MZM_PE_HITTING_SOMETHING_WITH_ICE_BEAM,
                    [MZM_PROJECTILE_PLASMA_BEAM]=MZM_PE_HITTING_SOMETHING_WITH_PLASMA_BEAM,
                    [MZM_PROJECTILE_CHARGED_PLASMA_BEAM]=MZM_PE_HITTING_SOMETHING_WITH_PLASMA_BEAM};
                /* ProjectileSetTrail's particles come first (ProjectileProcess). */
                for(int i=0;i<weapons.trail_count;i++)
                    mzm_particle_set(&sprites.effects,(uint16_t)weapons.trails[i].y,
                                     (uint16_t)weapons.trails[i].x,
                                     weapons.trails[i].effect);
                for(int i=0;i<weapons.impact_count;i++)
                    mzm_particle_set(&sprites.effects,(uint16_t)weapons.impacts[i].y,
                                     (uint16_t)weapons.impacts[i].x,
                                     impact_effects[weapons.impacts[i].type]);
                mzm_sprites_projectile_hits(sprites.live,RUNTIME_MAX_SPRITES,&weapons,
                                            &sprites.tables,&sprites.effects);
                runtime_sprites_drop_samus(&sprites,&samus);
                mzm_particles_process(&sprites.effects,&sprites.effect_kinds,
                                      (uint16_t)((int)last_cx*4),(uint16_t)((int)last_cy*4));
            }
            /* BgClipCheckTouchingTransitionOrTank on odd frames: a revealed
             * tank stops Samus and opens its message banner. */
            if(samus.pose!=MZM_POSE_DYING && runtime_block_tables.tanks_ok &&
               ((uint8_t)(sprites.frame8-1)&1)) {
                MzmTouchProbes probes;
                runtime_touch_probes(room,&samus,&probes);
                int message=mzm_tank_touch(&runtime_block_tables.tanks,&runtime_tank_collection,
                                           &probes,&equipment);
                if(message>=0 && message<MZM_MESSAGE_COUNT) {
                    prevent_movement=MZM_SAMUS_ITEM_PMT;
                    mzm_banner_spawn(&sprites.banner,(uint8_t)message);
                    printf("Tank collected: message %s\n",sprites.message_names[message]);
                }
            }
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
                    runtime_sprites_spawn(&sprites, room);
                    runtime_sprites_door_arrival(&sprites, room, &used);
                    {
                        /* The arrival door: the new room's door at the exit. */
                        int arrival=-1;
                        for(int d=0;d<room->door_count;d++)
                            if(room->doors[d].x0==used.dest_x && room->doors[d].y1==used.dest_y_end)
                                arrival=room->doors[d].index;
                        runtime_blocks_load(room,used.destination);
                        runtime_hatches_load(room,used.destination,arrival);
                    }
                    mzm_weapons_init(&weapons);
                    echo=(RuntimeEcho){0};
                    echo_visible=false;
                    title_dirty=true;
                    snprintf(current_room,sizeof current_room,"%s",used.destination);
                    music_transition=runtime_room_music(room);
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
        last_cx = cx;
        last_cy = cy;
        /* SET_BACKDROP_COLOR(COLOR_BLACK) behind the native layers. */
        runtime_bg_art_sync(&bg_art,renderer,room,current_room);
        bool layered=room->has_backgrounds && bg_art.sheet;
        int sprite_priority=samus.pose==MZM_POSE_DYING?0:runtime_samus_on_top(room)?1:2;
        bool pb_active=weapons.power_bomb.state==MZM_PB_EXPLODING ||
                       weapons.power_bomb.state==MZM_PB_IMPLODING;
        if(pb_active)SDL_SetRenderDrawColor(renderer,255,255,255,255);  /* the white backdrop */
        else if(layered)SDL_SetRenderDrawColor(renderer,0,0,0,255);
        else SDL_SetRenderDrawColor(renderer,12,14,24,255);
        SDL_RenderClear(renderer);
        if(layered) {
            /* Backgrounds at or behind the sprites' priority. */
            runtime_draw_backgrounds(renderer,&bg_art,room,cx,cy,viewport,scale,
                                     sprite_priority,3);
        } else if (texture) {
            SDL_FRect src={cx,cy,320.f,224.f};
            if (src.w>room->width) src.w=(float)room->width;
            if (src.h>room->height) src.h=(float)room->height;
            SDL_FRect dst={viewport.x,viewport.y,src.w*scale,src.h*scale};
            SDL_RenderTexture(renderer,texture,&src,&dst);
        }
        if(pb_active)runtime_draw_power_bomb(renderer,&weapons.power_bomb,cx,cy,viewport,scale);
        RuntimeLibraryEntry *lib_entry=NULL;
        int frame=0;
        if(animation_browser && library.count>0) {
            lib_entry=&library.entries[browser_index];
            Uint64 elapsed_ms=SDL_GetTicks()-animation_start;
            frame=samus_timeline_frame(lib_entry->ticks,lib_entry->count,
                                       (unsigned int)(elapsed_ms*60u/1000u));
        } else if(library.count>0) {
            lib_entry=runtime_samus_entry(&library,&samus,&equipment,
                                          runtime_weapon_highlighted(&weapons));
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
                                  pose_animation.suit,palette_frame,equipment.items,
                                  weapons.charge_counter);
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
        /* Hatches are BG1 blocks; without the layered backgrounds the
         * closed ones are outlined with a diagnostic tint. */
        SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
        for(int i=0;i<room->hatch_count && !layered;i++) {
            const RoomHatch *hatch=&room->hatches[i];
            bool closed=false;
            for(int k=0;k<room->native_hatch_count;k++)
                if(room->native_hatches[k].source_door==hatch->door)
                    closed=room->native_hatches[k].state!=MZM_HATCH_OPENED;
            if(!closed && room->native_hatch_count)continue;
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
        /* Sprites: their OAM frame, mirrored by the X and Y flip statuses
         * (SpriteDraw); palette rows are baked per sprite by the exporter. */
        /* SpriteDrawAll_*: draw order 1 goes in front, so the higher orders
         * are painted first (slot order within an order). */
        for(int order=16;order>=1;order--)
        for(int i=0;i<RUNTIME_MAX_SPRITES && sprites.kinds_ok;i++) {
            const MzmSprite *sprite=&sprites.live[i];
            if((sprite->draw_order?sprite->draw_order:4)!=order)continue;
            const char *anim_name=mzm_sprite_anim_name(sprite);
            if(!(sprite->status&MZM_SPRITE_STATUS_EXISTS) ||
               sprite->status&MZM_SPRITE_STATUS_NOT_DRAWN || !anim_name)continue;
            bool fx=sprite->status&MZM_SPRITE_STATUS_X_FLIP,fy=sprite->status&MZM_SPRITE_STATUS_Y_FLIP;
            char key[160];
            /* Whole-sprite rotation ignores the flips (SpriteDraw). */
            bool rotated=sprite->status&MZM_SPRITE_STATUS_ROTATION_SCALING_WHOLE;
            if(rotated)fx=fy=false;
            /* The frozen (15 - slot) and stun (14 - slot) rows are baked
             * variants of the sprite's own rows. */
            const char *shade="";
            static const char *const statue_rows[4]={"","/row1","/row2","/row3"};
            if(mzm_sprite_family(sprite->sprite_id,sprite->secondary)==MZM_FAMILY_CHOZO_STATUE)
                shade=sprite->palette_row<4?statue_rows[sprite->palette_row]:"";
            else if(sprite->palette_row && sprite->palette_row==(uint8_t)(15-sprite->gfx_slot))shade="/frozen";
            else if(sprite->palette_row && sprite->palette_row==(uint8_t)(14-sprite->gfx_slot))shade="/stun";
            snprintf(key,sizeof key,"Sprite/%s/%s/%s%s",
                     sprites.names[mzm_sprite_graphics_owner(sprite)],
                     anim_name,fx&&fy?"xy":fx?"x":fy?"y":"none",shade);
            RuntimeLibraryEntry *entry=runtime_library_find(&sprite_library,key);
            if(!entry || entry->count==0)continue;
            int index=sprite->frame<entry->count?sprite->frame:0;
            SDL_Texture *art_texture=runtime_library_texture(renderer,entry,index,-1,NULL);
            if(!art_texture)continue;
            const RuntimeLibraryFrame *art=&entry->frames[index];
            SDL_FRect box={viewport.x+((float)(sprite->x>>2)+(float)art->offset_x-cx)*scale,
                           viewport.y+((float)(sprite->y>>2)+(float)art->offset_y-cy)*scale,
                           (float)art->w*scale,(float)art->h*scale};
            if(rotated) {
                /* Clockwise around the sprite position, 256 units a turn. */
                SDL_FPoint center={-(float)art->offset_x*scale,-(float)art->offset_y*scale};
                SDL_RenderTextureRotated(renderer,art_texture,NULL,&box,
                                         (double)(sprite->rotation&0xFF)*360.0/256.0,&center,
                                         SDL_FLIP_NONE);
            } else if(sprite->status&MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE) {
                /* Each part turns around its own centre after its flip; the
                 * composed frame turns around its centre (exact for
                 * single-part frames, unclipped). */
                SDL_RenderTextureRotated(renderer,art_texture,NULL,&box,
                                         (double)(sprite->rotation&0xFF)*360.0/256.0,NULL,
                                         SDL_FLIP_NONE);
            } else {
                SDL_RenderTexture(renderer,art_texture,NULL,&box);
            }
            if(show_hitbox) {
                SDL_FRect hit={viewport.x+((float)((sprite->x+sprite->hitbox_left)>>2)-cx)*scale,
                               viewport.y+((float)((sprite->y+sprite->hitbox_top)>>2)-cy)*scale,
                               (float)((sprite->hitbox_right-sprite->hitbox_left)>>2)*scale,
                               (float)((sprite->hitbox_bottom-sprite->hitbox_top)>>2)*scale};
                SDL_SetRenderDrawColor(renderer,240,220,60,255);
                SDL_RenderRect(renderer,&hit);
            }
        }
        /* SpriteDebrisDrawAll and ParticleDraw: the common sprite graphics. */
        for(int i=0;i<MZM_MAX_SPRITE_DEBRIS+MZM_MAX_PARTICLES && sprites.kinds_ok;i++) {
            const char *table; uint16_t px,py,frame;
            if(i<MZM_MAX_SPRITE_DEBRIS) {
                const MzmSpriteDebris *d=&sprites.effects.debris[i];
                if(!d->exists || d->frame_counter==0)continue;
                table="SpriteDebrisOam";px=d->x;py=d->y;frame=d->frame;
            } else {
                const MzmParticle *p=&sprites.effects.particles[i-MZM_MAX_SPRITE_DEBRIS];
                if((p->status&(MZM_PARTICLE_EXISTS|MZM_PARTICLE_ONSCREEN))!=
                   (MZM_PARTICLE_EXISTS|MZM_PARTICLE_ONSCREEN) ||
                   p->status&MZM_PARTICLE_NOT_DRAWN)continue;
                table=mzm_particle_table(p->effect);px=p->x;py=p->y;frame=p->frame;
            }
            if(!table)continue;
            char key[160];
            /* Particles drawn with the beam graphics or palette come from the
             * projectile library's loaded beam variant. */
            snprintf(key,sizeof key,"Particle/%s/%s/none",table,runtime_beam_variant(equipment.items));
            RuntimeLibraryEntry *entry=runtime_library_find(&projectile_library,key);
            if(!entry) {
                snprintf(key,sizeof key,"Particle/%s/none",table);
                entry=runtime_library_find(&sprite_library,key);
            }
            if(!entry || entry->count==0)continue;
            int index=frame<entry->count?frame:0;
            SDL_Texture *art_texture=runtime_library_texture(renderer,entry,index,-1,NULL);
            if(!art_texture)continue;
            const RuntimeLibraryFrame *art=&entry->frames[index];
            SDL_FRect box={viewport.x+((float)(px>>2)+(float)art->offset_x-cx)*scale,
                           viewport.y+((float)(py>>2)+(float)art->offset_y-cy)*scale,
                           (float)art->w*scale,(float)art->h*scale};
            SDL_RenderTexture(renderer,art_texture,NULL,&box);
        }
        if(projectile_library.count>0) {
            /* ParticleChargingBeam (hidden while spinning) and the shot flash. */
            const char *variant=runtime_beam_variant(equipment.items);
            for(int k=0;k<2;k++) {
                char key[160];
                int px,py;uint16_t fr;
                if(k==0) {
                    if(!beam_fx.charge_stage || samus.pose==MZM_POSE_SPINNING ||
                       samus.pose==MZM_POSE_STARTING_WALL_JUMP || samus.pose==MZM_POSE_SPACE_JUMPING ||
                       samus.pose==MZM_POSE_SCREW_ATTACKING)continue;
                    snprintf(key,sizeof key,"Particle/ParticleChargingBeam%sOam/%s/none",
                             beam_fx.charge_stage==1?"Begin":"Charged",variant);
                    px=beam_fx.cannon_x;py=beam_fx.cannon_y;fr=beam_fx.charge_frame;
                } else {
                    if(!beam_fx.flash.active)continue;
                    snprintf(key,sizeof key,"Particle/%s/%s/%s",beam_fx.flash.table,variant,
                             beam_fx.flash.flip?"x":"none");
                    px=beam_fx.flash.x;py=beam_fx.flash.y;fr=beam_fx.flash.frame;
                }
                RuntimeLibraryEntry *entry=runtime_library_find(&projectile_library,key);
                if(!entry || entry->count==0)continue;
                int index=fr<entry->count?fr:0;
                SDL_Texture *fx_texture=runtime_library_texture(renderer,entry,index,-1,NULL);
                if(!fx_texture)continue;
                const RuntimeLibraryFrame *art=&entry->frames[index];
                SDL_FRect box={viewport.x+((float)px+(float)art->offset_x-cx)*scale,
                               viewport.y+((float)py+(float)art->offset_y-cy)*scale,
                               (float)art->w*scale,(float)art->h*scale};
                SDL_RenderTexture(renderer,fx_texture,NULL,&box);
            }
        }
        /* PROJ_STATUS_ABOVE_BG1 (the Wave Beam, and the Plasma Beam with it)
         * draws in front of every background: those come after the front
         * layers. */
        for(int pass=0;pass<2;pass++) {
        if(pass==1 && layered)
            runtime_draw_backgrounds(renderer,&bg_art,room,cx,cy,viewport,scale,0,
                                     sprite_priority-1);
        for(int i=0;i<MZM_MAX_PROJECTILES && projectile_library.count>0;i++) {
            const MzmProjectile *projectile=&weapons.list[i];
            if(!projectile->active || projectile->stage==MZM_STAGE_INIT ||
               projectile->hidden)continue;
            bool above=projectile->type==MZM_PROJECTILE_WAVE_BEAM ||
                projectile->type==MZM_PROJECTILE_CHARGED_WAVE_BEAM || projectile->wave_oam ||
                projectile->stage==MZM_STAGE_TUMBLING;
            if(above!=(pass==1))continue;
            char key[160];
            runtime_projectile_key(projectile,equipment.items,key,sizeof key);
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
        if(sprites.banner.exists && sprites.banner.drawn && sprites.banner_ok) {
            /* The message banner: SP_ABSOLUTE_POSITION at (120, 54) of the
             * GBA screen, bgPriority 0; centred in the wider runtime view
             * (the GBA screen's 64 extra rows split above and below). */
            char key[160];
            snprintf(key,sizeof key,"Banner/%s/%s/none",
                     sprites.message_names[sprites.banner.message],
                     mzm_banner_tables[sprites.banner.table]);
            RuntimeLibraryEntry *entry=runtime_library_find(&sprite_library,key);
            if(entry && entry->count>0) {
                int index=sprites.banner.frame<entry->count?sprites.banner.frame:0;
                SDL_Texture *banner_texture=runtime_library_texture(renderer,entry,index,-1,NULL);
                const RuntimeLibraryFrame *art=&entry->frames[index];
                SDL_FRect box={viewport.x+(160.f+(float)art->offset_x)*scale,
                               viewport.y+(54.f+32.f+(float)art->offset_y)*scale,
                               (float)art->w*scale,(float)art->h*scale};
                if(banner_texture)SDL_RenderTexture(renderer,banner_texture,NULL,&box);
            }
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
            int live=0;
            for(int i=0;i<RUNTIME_MAX_SPRITES;i++)
                live+=(sprites.live[i].status&MZM_SPRITE_STATUS_EXISTS)!=0;
            printf("Captured frame %ld: %s (pose %s at %d,%d, energy %d, %d sprites)\n",
                   capture_step,capture_path,mzm_pose_name(samus.pose),(int)(samus.x>>2),
                   (int)(samus.y>>2),equipment.energy,live);
            if(samus.on_enemy)printf("  Samus stands on a sprite\n");
            if(prevent_movement && !sprites.banner.exists)
                printf("  movement prevented %u\n",prevent_movement);
            if(sprites.banner.exists)
                printf("  banner %s pose %#x table %s frame %u%s, movement prevented %u\n",
                       sprites.message_names[sprites.banner.message],sprites.banner.pose,
                       mzm_banner_tables[sprites.banner.table],sprites.banner.frame,
                       sprites.banner.drawn?"":" (not drawn)",prevent_movement);
            for(int i=0;i<MZM_MAX_PROJECTILES;i++)
                if(weapons.list[i].active)
                    printf("  projectile %d type %d at %d,%d\n",i,(int)weapons.list[i].type,
                           (int)(weapons.list[i].x>>2),(int)(weapons.list[i].y>>2));
            for(int i=0;i<RUNTIME_MAX_SPRITES;i++)
                if(sprites.live[i].status&MZM_SPRITE_STATUS_EXISTS)
                    printf("  sprite %d %s%s at %d,%d pose %#x status %#x health %d freeze %d\n",
                           i,sprites.names[mzm_sprite_graphics_owner(&sprites.live[i])],
                           sprites.live[i].secondary?" part":"",sprites.live[i].x>>2,
                           sprites.live[i].y>>2,sprites.live[i].pose,sprites.live[i].status,
                           sprites.live[i].health,sprites.live[i].freeze_timer);
            running=false;
        }
        SDL_RenderPresent(renderer);
        if(!capture_path)SDL_Delay(8);
    }
    rc=0;
cleanup:
    mzm_sfx_set_sink(NULL,NULL);
    if (audio) SDL_DestroyAudioStream(audio);
    mzm_audio_close();
    gba_input_close(&gamepad);
    runtime_library_free(&library);
    runtime_library_free(&projectile_library);
    runtime_library_free(&sprite_library);
    runtime_bg_art_free(&bg_art);
    if (texture) SDL_DestroyTexture(texture);
    if (surface) SDL_DestroySurface(surface);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();free(room);return rc;
}
#endif /* FUSION_RUNTIME_TEST */
