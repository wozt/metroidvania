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
#include "aos_door.h"
#include "aos_enemy.h"
#include "aos_room.h"
#include "aos_soma.h"
#include "aos_weapon.h"

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
#define DEFAULT_OBJECTS "assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv"
#define DEFAULT_WEAPONS "assets/extracted/aria/metadata/weapons.tsv"
#define DEFAULT_WEAPON_FRAMES "assets/extracted/aria/metadata/weapon_frames.tsv"
#define DEFAULT_WEAPON_SPRITES "assets/extracted/aria/sprites/weapons/runtime/runtime_index.tsv"
#define DEFAULT_ENEMY_FRAMES "assets/extracted/aria/metadata/enemy_frames.tsv"
#define DEFAULT_ENEMY_STATS "assets/extracted/aria/metadata/enemies.tsv"
#define DOOR_STYLES 2
#define BAT_ANIMS 3
#define ARIA_KIND_ENEMY 1
#define ARIA_ENEMY_BAT 0x00


#define MAX_TRANSITIONS 32
#define MAX_ENTITIES 128
#define ARIA_KIND_SPECIAL 2
#define ARIA_OBJECT_WOODEN_DOOR 0x00

typedef struct {
    int kind, id, x, y, param0, param1, flags;
    bool spawned;           /* gEwramData + 0x3D0 bit of the record */
    AosDoor door;
    AosEnemy enemy;
} AriaEntity;

typedef struct {
    int area, number;
    int width_screens, height_screens, width_cells, height_cells;
    uint8_t *cells;
    uint8_t *blocks;        /* gEwramData + 0xF0C0, cleared on every load */
    AosTransition transitions[MAX_TRANSITIONS];
    size_t transition_count;
    AriaEntity entities[MAX_ENTITIES];
    size_t entity_count;
} AriaRoom;

static void free_room(AriaRoom *room) {
    free(room->cells);
    free(room->blocks);
    room->cells = room->blocks = NULL;
}

static AosCollision room_layer(const AriaRoom *room) {
    return (AosCollision){room->width_screens, room->height_screens, room->width_cells,
                          room->height_cells, room->cells, room->blocks, false};
}

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
               &room->height_cells, &extra) != 7 || version != 3 ||
        room->width_cells < 1 || room->height_cells < 1 ||
        room->width_cells > 1024 || room->height_cells > 1024)
        goto failure;
    room->area = area;
    room->number = number;
    room->transition_count = 0;
    room->entity_count = 0;
    room->blocks = NULL;
    room->cells = calloc((size_t)room->width_cells * room->height_cells, 1);
    if (!room->cells) goto failure;
    {
        AosCollision probe = room_layer(room);
        room->blocks = calloc(aos_collision_block_bytes(&probe), 1);
        if (!room->blocks) goto failure;
    }
    while (fgets(line, sizeof line, f)) {
        if (!strcmp(line, "END\n") || !strcmp(line, "END")) { ended = true; break; }
        if (line[0] == 'E') {
            AriaEntity *e = &room->entities[room->entity_count];
            if (room->entity_count >= MAX_ENTITIES ||
                sscanf(line, "E\t%d\t%d\t%d\t%d\t%d\t%d\t%d %c", &e->kind, &e->id, &e->x, &e->y,
                       &e->param0, &e->param1, &e->flags, &extra) != 7 ||
                e->kind < 0 || e->kind > 255 || e->id < 0 || e->id > 255)
                goto failure;
            e->spawned = false;
            room->entity_count++;
            continue;
        }
        if (line[0] == 'T') {
            int sx, sy, adjust, load_x, load_y, target_area, target_room;
            if (room->transition_count >= MAX_TRANSITIONS ||
                sscanf(line, "T\t%d\t%d\t%d\t%d\t%d\t%d\t%d %c", &sx, &sy, &adjust, &load_x,
                       &load_y, &target_area, &target_room, &extra) != 7 ||
                sx < -128 || sx > 127 || sy < -128 || sy > 127 || adjust < -32768 ||
                adjust > 32767 || load_x < 0 || load_x > 0xFFFF || load_y < 0 ||
                load_y > 0xFFFF || target_area < 0 || target_area > 255 || target_room < 0 ||
                target_room > 255)
                goto failure;
            room->transitions[room->transition_count++] = (AosTransition){
                (int8_t)sx, (int8_t)sy, (int16_t)adjust, (uint16_t)load_x, (uint16_t)load_y,
                (uint8_t)target_area, (uint8_t)target_room};
            continue;
        }
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
    free_room(room);
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

static void room_folder(char *out, size_t size, int area, int number) {
    snprintf(out, size, "%s/area_%02d_room_%03d", DEFAULT_ROOMS, area, number);
}

static bool load_room_folder(const char *folder, AriaRoom *room) {
    char path[600];
    snprintf(path, sizeof path, "%s/room.tsv", folder);
    return load_room(path, room);
}

/* Leaves every room through the middle of each transition's screen edge,
 * checks that the arrival lies inside the target room and that leaving the
 * target again through the opposite edge returns to the source room. */
static int audit_transitions(void) {
    int total = 0, outside = 0, missing = 0, unmatched = 0, rooms = 0, cell_exits = 0,
        cell_outside = 0;
    for (int area = 0; area < 16; ++area) {
        for (int number = 0; number < 256; ++number) {
            char folder[512];
            room_folder(folder, sizeof folder, area, number);
            AriaRoom source = {0};
            FILE *probe = NULL;
            char path[600];
            snprintf(path, sizeof path, "%s/room.tsv", folder);
            if (!(probe = fopen(path, "rb"))) continue;
            fclose(probe);
            if (!load_room_folder(folder, &source)) return 1;
            ++rooms;
            int width = source.width_screens > 1 ? source.width_screens << 8 : 0xF0;
            int height = source.height_screens > 1 ? source.height_screens << 8 : 0x100;
            for (size_t i = 0; i < source.transition_count; ++i) {
                const AosTransition *t = &source.transitions[i];
                ++total;
                /* A point just outside the room on that screen's edge. */
                int32_t x, y;
                if (t->screen_x < 0) x = -2;
                else if (t->screen_x >= source.width_screens) x = width + 2;
                else x = (t->screen_x << 8) + 0x78;
                if (t->screen_y < 0) y = 0x2E;
                else if (t->screen_y >= source.height_screens)
                    y = source.height_screens > 1 ? height - 0x2E : 0xD2;
                else y = (t->screen_y << 8) + 0x80;
                AosCollision layer = room_layer(&source);
                bool inside_x = t->screen_x >= 0 && t->screen_x < source.width_screens;
                bool inside_y = t->screen_y >= 0 && t->screen_y < source.height_screens;
                if (inside_x && inside_y) {
                    /* An interior entry: leave through an exit cell of that screen. */
                    bool found = false;
                    for (int cy = 0; cy < 32 && !found; ++cy)
                        for (int cx = 0; cx < 32 && !found; ++cx) {
                            int px = (t->screen_x << 8) + cx * 8, py = (t->screen_y << 8) + cy * 8;
                            if (aos_collision_cell(&layer, px, py) == AOS_EXIT_CELL) {
                                x = px;
                                y = py;
                                found = true;
                            }
                        }
                }
                if (!aos_room_outside(&layer, source.width_screens, source.height_screens, x, y) ||
                    aos_room_find_exit(source.transitions, source.transition_count,
                                       source.width_screens, source.height_screens, x, y) != t) {
                    ++unmatched;
                    printf("unmatched exit %d/%d entry %zu (%d,%d)\n", area, number, i,
                           t->screen_x, t->screen_y);
                    continue;
                }
                char target_folder[512];
                room_folder(target_folder, sizeof target_folder, t->area, t->room);
                AriaRoom target = {0};
                if (!load_room_folder(target_folder, &target)) { ++missing; continue; }
                int32_t ax, ay;
                aos_room_arrival(t, target.width_screens, target.height_screens, x, y, &ax, &ay);
                bool cell_exit = inside_x && inside_y;
                cell_exits += cell_exit;
                if (aos_room_outside(NULL, target.width_screens, target.height_screens, ax, ay)) {
                    /* Exit cells sit behind doors whose entity is not ported. */
                    if (cell_exit) ++cell_outside;
                    else ++outside;
                    printf("arrival outside %d/%d -> %d/%d at %d,%d\n", area, number, t->area,
                           t->room, ax, ay);
                }
                free_room(&target);
            }
            free_room(&source);
        }
    }
    printf("Aria transitions: %d rooms, %d entries, %d unmatched exits, %d missing "
           "targets, %d arrivals outside; %d exit-cell entries (%d arrive outside from "
           "the first exit cell, door entities not ported)\n", rooms, total, unmatched, missing,
           outside, cell_exits, cell_outside);
    return unmatched || outside ? 1 : 0;
}

/* Camera: BG1 follows Soma at screen (0x78, 0x6F), the settle target of
 * the room transition (gEwramData + 0x13214 / 0x13216); BG1 stays within the
 * room, with y >= 0x30 and one-screen axes pinned like sub_0803FBBC. Scroll
 * speed limits are not modelled. */
static void follow_camera(const AriaRoom *room, int32_t x, int32_t y, int *cam_x, int *cam_y) {
    int cx = x - 0x78, cy = y - 0x6F;
    int max_x = (room->width_screens << 8) - VIEW_W;
    int max_y = (room->height_screens << 8) - 0x30 - VIEW_H;
    if (room->width_screens <= 1 || cx < 0) cx = 0;
    else if (cx > max_x) cx = max_x;
    if (room->height_screens <= 1 || cy < 0x30) cy = 0x30;
    else if (cy > max_y) cy = max_y;
    *cam_x = cx;
    *cam_y = cy;
}

static SDL_Texture *load_background(SDL_Renderer *renderer, const char *folder) {
    char path[600];
    snprintf(path, sizeof path, "%s/background.bmp", folder);
    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) { fprintf(stderr, "%s: %s\n", path, SDL_GetError()); return NULL; }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture) SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    return texture;
}

/* sub_08011A44 .. sub_08010350: leaves the room when Soma is outside it or
 * on an exit cell; returns false when the exit or its target is missing. */
static bool take_exit(SDL_Renderer *renderer, AriaRoom *room, SDL_Texture **background,
                      AosSoma *soma, int cam_x, int cam_y) {
    int32_t x = soma->x >> 16, y = soma->y >> 16;
    const AosTransition *exit = aos_room_find_exit(room->transitions, room->transition_count,
                                                   room->width_screens, room->height_screens,
                                                   x, y);
    if (!exit) {
        fprintf(stderr, "No transition for %d/%d at %d,%d\n", room->area, room->number, x, y);
        return false;
    }
    char folder[512];
    room_folder(folder, sizeof folder, exit->area, exit->room);
    AriaRoom target = {0};
    if (!load_room_folder(folder, &target)) return false;
    SDL_Texture *texture = load_background(renderer, folder);
    if (!texture) { free_room(&target); return false; }
    aos_room_exit_velocity(soma, x - cam_x, y - cam_y);
    int32_t ax, ay;
    aos_room_arrival(exit, target.width_screens, target.height_screens, x, y, &ax, &ay);
    soma->x = (int32_t)((uint32_t)ax << 16) | (soma->x & 0xFFFF);
    soma->y = (int32_t)((uint32_t)ay << 16) | (soma->y & 0xFFFF);
    printf("Room %d/%d -> %d/%d, arrival %d,%d\n", room->area, room->number, target.area,
           target.number, ax, ay);
    free_room(room);
    *room = target;
    SDL_DestroyTexture(*background);
    *background = texture;
    return true;
}

/* The player's combat stats: diagnostic inputs, the new-game values are
 * not traced. */
typedef struct {
    int atk, def, hp, max_hp;
} AriaPlayerStats;

/* Everything the enemies need from the frame. */
typedef struct {
    const AosEnemyKind *bat;
    const AosEnemyStats *bat_stats;
    const AosWeaponEntity *weapon;
    const AosWeaponFrames *weapon_frames;
    AriaPlayerStats *player;
} AriaCombat;

/* sub_0800F4F8 / sub_0800F1FC: records whose X is within the camera window
 * spawn once per room visit; the wooden door and the bat are ported. */
static int update_entities(AriaRoom *room, AosCollision *layer, AosSoma *soma,
                           int cam_x, int cam_y, AosForcedInput *input, AriaCombat *combat) {
    int sound = 0;
    for (size_t i = 0; i < room->entity_count; ++i) {
        AriaEntity *e = &room->entities[i];
        bool door = e->kind == ARIA_KIND_SPECIAL && e->id == ARIA_OBJECT_WOODEN_DOOR;
        bool bat = e->kind == ARIA_KIND_ENEMY && e->id == ARIA_ENEMY_BAT && combat->bat;
        if (bat) {
            if (!e->spawned) {
                if (e->x < cam_x - 80 || e->x > cam_x + 320) continue;
                e->spawned = aos_enemy_create(&e->enemy, (uint8_t)e->id, e->x, e->y, soma, layer,
                                              combat->bat, combat->bat_stats);
                continue;
            }
            AosHitReport hit = aos_enemy_update(&e->enemy, soma, combat->bat, combat->weapon,
                                                combat->weapon_frames, combat->player->atk,
                                                combat->player->def, cam_x, cam_y, aos_random);
            if (hit.enemy_hit)
                printf("Bat hit: %d damage%s\n", hit.enemy_damage, hit.killed ? ", killed" : "");
            if (hit.soma_hit)
                printf("Soma hit: %d damage, HP %d/%d\n", hit.soma_damage, soma->hp, soma->max_hp);
            continue;
        }
        if (!door) continue;
        if (!e->spawned) {
            if (e->x < cam_x - 80 || e->x > cam_x + 320) continue;
            e->spawned = true;
            aos_door_create(&e->door, layer, e->x, e->y, soma, cam_x, input);
            continue;
        }
        int played = aos_door_update(&e->door, layer, soma, input);
        if (played) sound = played;
    }
    return sound;
}

/* Doors use their native sprite (frame 0 or 5 by parameter 0) and palette
 * cycle; without the object library an outline marks the blocked area. */
static void draw_doors(SDL_Renderer *renderer, const AriaRoom *room, const void *objects_ptr,
                       long frame_count, int cam_x, int cam_y);

/* Object sequences of scripts/aos_object_sprites.py: WoodenDoor/style_N
 * (the door frame under each step of its palette cycle). */
typedef struct {
    AriaFrame frames[DOOR_STYLES][MAX_FRAMES];
    uint8_t ticks[DOOR_STYLES][MAX_FRAMES];
    int counts[DOOR_STYLES];
    AriaFrame bat_frames[BAT_ANIMS][MAX_FRAMES];
    uint8_t bat_ticks[BAT_ANIMS][MAX_FRAMES];
    AosAnimDef bat_defs[BAT_ANIMS];
    AosAnimSet bat;
    AosEnemyKind bat_kind;
    AosEnemyStats bat_stats;
} AriaObjects;

/* enemy_frames.tsv (boxes, death blink) and enemies.tsv (stats). */
static bool load_enemy_data(AriaObjects *objects) {
    FILE *f = fopen(DEFAULT_ENEMY_FRAMES, "rb");
    if (!f) return false;
    char line[256];
    AosEnemyKind *kind = &objects->bat_kind;
    kind->anims = &objects->bat;
    while (fgets(line, sizeof line, f)) {
        char name[16], blink[64];
        int anim, index, frame, ticks, mode, x, y, w, h;
        if (sscanf(line, "blink\t%63s", blink) == 1) {
            for (int i = 0; i < 40 && blink[i]; ++i) kind->blink[i] = (uint8_t)(blink[i] == '1');
            continue;
        }
        if (sscanf(line, "%15s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d", name, &anim, &index, &frame,
                   &ticks, &mode, &x, &y, &w, &h) != 10 || strcmp(name, "bat") ||
            anim < 0 || anim >= AOS_ENEMY_MAX_ANIMS || index < 0 || index >= AOS_ENEMY_MAX_FRAMES)
            continue;
        /* Frame record + 4 = 1: one box serves as hurtbox and attack box. */
        kind->modes[anim][index] = (uint8_t)mode;
        kind->hurt[anim][index] = kind->attack[anim][index] =
            (AosBox){(int8_t)x, (int8_t)y, (uint8_t)w, (uint8_t)h};
    }
    fclose(f);
    f = fopen(DEFAULT_ENEMY_STATS, "rb");
    if (!f) return false;
    bool found = false;
    while (!found && fgets(line, sizeof line, f)) {
        int id, hp, field_e, contact, defence;
        unsigned weak, resist;
        if (sscanf(line, "%d\t%d\t%d\t%d\t%d\t%x\t%x", &id, &hp, &field_e, &contact, &defence,
                   &weak, &resist) == 7 && id == ARIA_ENEMY_BAT) {
            objects->bat_stats = (AosEnemyStats){(int16_t)hp, (uint8_t)contact, (uint8_t)defence,
                                                 (uint16_t)weak, (uint16_t)resist};
            found = true;
        }
    }
    fclose(f);
    return found;
}

static bool load_frame(AriaFrame *out, const char *path, int ox, int oy, SDL_Renderer *renderer) {
    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) return false;
    out->texture = SDL_CreateTextureFromSurface(renderer, surface);
    out->w = (float)surface->w;
    out->h = (float)surface->h;
    out->offset_x = ox;
    out->offset_y = oy;
    SDL_DestroySurface(surface);
    if (!out->texture) return false;
    SDL_SetTextureScaleMode(out->texture, SDL_SCALEMODE_NEAREST);
    return true;
}

static bool load_objects(const char *path, AriaObjects *objects, SDL_Renderer *renderer) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char line[1024];
    bool ok = fgets(line, sizeof line, f) && !strncmp(line, INDEX_SCHEMA, strlen(INDEX_SCHEMA));
    while (ok && fgets(line, sizeof line, f)) {
        char frame_path[512];
        int style, frame, ticks, ox, oy;
        if (sscanf(line, "Enemy/bat/anim_%d\t%d\t%d\t%d\t%d\t%511s", &style, &frame, &ticks,
                   &ox, &oy, frame_path) == 6) {
            if (style < 0 || style >= BAT_ANIMS || frame != objects->bat_defs[style].count ||
                frame >= MAX_FRAMES || ticks < 1 || ticks > 255 ||
                !load_frame(&objects->bat_frames[style][frame], frame_path, ox, oy, renderer)) {
                ok = false;
                break;
            }
            objects->bat_ticks[style][frame] = (uint8_t)ticks;
            objects->bat_defs[style].durations = objects->bat_ticks[style];
            objects->bat_defs[style].count++;
            continue;
        }
        if (sscanf(line, "WoodenDoor/style_%d\t%d\t%d\t%d\t%d\t%511s", &style, &frame, &ticks,
                   &ox, &oy, frame_path) != 6)
            continue;
        if (style < 0 || style >= DOOR_STYLES || frame != objects->counts[style] ||
            frame >= MAX_FRAMES || ticks < 1 || ticks > 255) { ok = false; break; }
        SDL_Surface *surface = SDL_LoadBMP(frame_path);
        if (!surface) { ok = false; break; }
        AriaFrame *out = &objects->frames[style][frame];
        out->texture = SDL_CreateTextureFromSurface(renderer, surface);
        out->w = (float)surface->w;
        out->h = (float)surface->h;
        out->offset_x = ox;
        out->offset_y = oy;
        SDL_DestroySurface(surface);
        if (!out->texture) { ok = false; break; }
        SDL_SetTextureScaleMode(out->texture, SDL_SCALEMODE_NEAREST);
        objects->ticks[style][frame] = (uint8_t)ticks;
        objects->counts[style]++;
    }
    fclose(f);
    objects->bat = (AosAnimSet){objects->bat_defs, BAT_ANIMS};
    return ok;
}

static void free_objects(AriaObjects *objects) {
    for (int i = 0; i < DOOR_STYLES; ++i)
        for (int j = 0; j < MAX_FRAMES; ++j)
            if (objects->frames[i][j].texture) SDL_DestroyTexture(objects->frames[i][j].texture);
    for (int i = 0; i < BAT_ANIMS; ++i)
        for (int j = 0; j < MAX_FRAMES; ++j)
            if (objects->bat_frames[i][j].texture) SDL_DestroyTexture(objects->bat_frames[i][j].texture);
}

static void draw_doors(SDL_Renderer *renderer, const AriaRoom *room, const void *objects_ptr,
                       long frame_count, int cam_x, int cam_y) {
    const AriaObjects *objects = objects_ptr;
    for (size_t i = 0; i < room->entity_count; ++i) {
        const AriaEntity *e = &room->entities[i];
        if (!e->spawned || e->kind != ARIA_KIND_SPECIAL || e->id != ARIA_OBJECT_WOODEN_DOOR)
            continue;
        int style = e->param0 ? 1 : 0;
        if (objects && objects->counts[style]) {
            /* sub_0803C150: the palette script loops (phase from the run start). */
            int total = 0;
            for (int k = 0; k < objects->counts[style]; ++k) total += objects->ticks[style][k];
            long t = frame_count % total;
            int k = 0;
            while (t >= objects->ticks[style][k]) t -= objects->ticks[style][k++];
            const AriaFrame *frame = &objects->frames[style][k];
            /* Mirrored when facing left, like Soma's frames; the generic draw
             * (0x03004564) is not traced, this matches the room art at both
             * room edges. */
            bool flip = e->door.facing_left;
            float x = flip ? (float)(e->x - frame->offset_x) - frame->w
                           : (float)(e->x + frame->offset_x);
            SDL_FRect rect = {x - (float)cam_x, (float)(e->y + frame->offset_y - cam_y), frame->w,
                              frame->h};
            SDL_RenderTextureRotated(renderer, frame->texture, NULL, &rect, 0, NULL,
                                     flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
            continue;
        }
        SDL_FRect rect = {(float)((e->x & ~15) - cam_x), (float)(e->y - 48 - cam_y), 16, 48};
        SDL_SetRenderDrawColor(renderer, 230, 190, 120, 255);
        SDL_RenderRect(renderer, &rect);
    }
}

/* Enemies: sprites face left; mirrored ones are flipped around x. */
static void draw_enemies(SDL_Renderer *renderer, const AriaRoom *room, const AriaObjects *objects,
                         int cam_x, int cam_y) {
    for (size_t i = 0; i < room->entity_count; ++i) {
        const AriaEntity *e = &room->entities[i];
        if (!e->spawned || e->kind != ARIA_KIND_ENEMY || e->id != ARIA_ENEMY_BAT ||
            e->enemy.removed || e->enemy.hidden || e->enemy.anim.id >= BAT_ANIMS ||
            e->enemy.anim.frame >= MAX_FRAMES)
            continue;
        const AriaFrame *frame = &objects->bat_frames[e->enemy.anim.id][e->enemy.anim.frame];
        if (!frame->texture) continue;
        int ex = e->enemy.x >> 16, ey = e->enemy.y >> 16;
        bool flip = e->enemy.mirrored;
        float x = flip ? (float)(ex - frame->offset_x) - frame->w : (float)(ex + frame->offset_x);
        SDL_FRect rect = {x - (float)cam_x, (float)(ey + frame->offset_y - cam_y), frame->w, frame->h};
        if (e->enemy.vflip) rect.y = (float)(ey - frame->offset_y - cam_y) - frame->h;
        SDL_FlipMode mode = (SDL_FlipMode)((flip ? SDL_FLIP_HORIZONTAL : 0) |
                                           (e->enemy.vflip ? SDL_FLIP_VERTICAL : 0));
        SDL_RenderTextureRotated(renderer, frame->texture, NULL, &rect, 0, NULL, mode);
    }
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
    if (keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_F]) held |= AOS_KEY_ATTACK;
    if (keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_LSHIFT]) held |= AOS_KEY_ABILITY;
    if (keys[SDL_SCANCODE_E] || keys[SDL_SCANCODE_V]) held |= AOS_KEY_GUARDIAN;
    return held;
}

/* weapons.tsv of scripts/aos_weapons.py: "none" is the unarmed record. */
static bool load_weapon(const char *path, const char *name, AosWeapon *weapon) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return false; }
    char line[256];
    bool found = false;
    static const char schema[] = "schema\tmetroidvania-aos-weapons-v2";
    if (!fgets(line, sizeof line, f) || strncmp(line, schema, strlen(schema))) {
        fclose(f);
        fprintf(stderr, "Not a weapon table: %s\n", path);
        return false;
    }
    while (!found && fgets(line, sizeof line, f)) {
        char key[16];
        unsigned item, flags;
        int cls, variant, a[5], interval;
        if (line[0] == '#' ||
            sscanf(line, "%15s\t%x\t%d\t%d\t%x\t%d\t%d\t%d\t%d\t%d\t%d", key, &item, &cls,
                   &variant, &flags, &a[0], &a[1], &a[2], &a[3], &a[4], &interval) != 11 ||
            strcmp(key, name))
            continue;
        *weapon = (AosWeapon){(uint8_t)cls, (uint16_t)flags,
                              {(uint8_t)a[0], (uint8_t)a[1], (uint8_t)a[2], (uint8_t)a[3],
                               (uint8_t)a[4]}, (uint8_t)interval};
        found = true;
    }
    fclose(f);
    if (!found) fprintf(stderr, "No weapon %s in %s\n", name, path);
    return found;
}

/* The weapon entity's sprite sequence and hitboxes (classes 0, 2, 3). */
typedef struct {
    AriaFrame frames[MAX_FRAMES];
    uint8_t ticks[MAX_FRAMES];
    AosHitbox hitboxes[MAX_FRAMES];
    AosAnimDef def;
    AosAnimSet set;
    AosWeaponFrames data;
    int count;
} AriaWeaponSprite;

static bool load_weapon_sprite(const char *name, AriaWeaponSprite *out, SDL_Renderer *renderer) {
    char key[32], line[1024];
    snprintf(key, sizeof key, "Weapon/%s\t", name);
    FILE *f = fopen(DEFAULT_WEAPON_SPRITES, "rb");
    if (!f) return false;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, key, strlen(key))) continue;
        char frame_path[512];
        int frame, ticks, ox, oy;
        if (sscanf(line + strlen(key), "%d\t%d\t%d\t%d\t%511s", &frame, &ticks, &ox, &oy,
                   frame_path) != 5 || frame != out->count || frame >= MAX_FRAMES)
            break;
        SDL_Surface *surface = SDL_LoadBMP(frame_path);
        if (!surface) break;
        AriaFrame *dst = &out->frames[frame];
        dst->texture = SDL_CreateTextureFromSurface(renderer, surface);
        dst->w = (float)surface->w;
        dst->h = (float)surface->h;
        dst->offset_x = ox;
        dst->offset_y = oy;
        SDL_DestroySurface(surface);
        if (!dst->texture) break;
        SDL_SetTextureScaleMode(dst->texture, SDL_SCALEMODE_NEAREST);
        out->ticks[frame] = (uint8_t)ticks;
        out->count++;
    }
    fclose(f);
    if (!out->count) return false;
    f = fopen(DEFAULT_WEAPON_FRAMES, "rb");
    if (f) {
        while (fgets(line, sizeof line, f)) {
            char weapon[16];
            int index, frame, ticks, hit, x, y, w, h;
            if (sscanf(line, "%15s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d", weapon, &index, &frame,
                       &ticks, &hit, &x, &y, &w, &h) == 9 &&
                !strcmp(weapon, name) && index >= 0 && index < out->count)
                out->hitboxes[index] = (AosHitbox){(int8_t)x, (int8_t)y, (uint8_t)w, (uint8_t)h,
                                                   hit != 0};
        }
        fclose(f);
    }
    out->def = (AosAnimDef){(uint16_t)out->count, out->ticks};
    out->set = (AosAnimSet){&out->def, 1};
    out->data = (AosWeaponFrames){&out->set, out->hitboxes};
    return true;
}

static void free_weapon_sprite(AriaWeaponSprite *sprite) {
    for (int i = 0; i < sprite->count; ++i)
        if (sprite->frames[i].texture) SDL_DestroyTexture(sprite->frames[i].texture);
}

static void usage(const char *name) {
    fprintf(stderr,
            "Usage: %s [--check] [--library index.tsv] [--spawn X Y] [--moves MASK]\n"
            "       [--weapon none|INDEX] [--hitboxes] [--atk N] [--def N] [--hp N]\n"
            "       [--repeat N: release the capture buttons one frame in N]\n"
            "       [--capture out.bmp FRAMES BUTTONS] (--area A --room R | room-folder)\n",
            name);
}

int main(int argc, char **argv) {
    const char *folder = NULL, *library_path = DEFAULT_LIBRARY, *capture_path = NULL;
    const char *weapon_name = "none";
    long area = -1, room_number = -1, capture_frames = 0;
    unsigned long capture_buttons = 0;
    /* No soul inventory yet: every ported ability move is enabled. */
    unsigned long moves = AOS_MOVE_BACKDASH | AOS_MOVE_SLIDE | AOS_MOVE_AIR_JUMP |
                          AOS_MOVE_DIVE_KICK | AOS_MOVE_HIGH_JUMP;
    int spawn_x = -1, spawn_y = -1;
    bool check = false, show_hitboxes = false;
    /* Diagnostic combat stats (new-game values are not traced). */
    AriaPlayerStats player_stats = {10, 4, 320, 320};
    long repeat = 0;
    for (int i = 1; i < argc; ++i) {
        char *end = NULL;
        if (!strcmp(argv[i], "--check")) check = true;
        else if (!strcmp(argv[i], "--audit-transitions")) return audit_transitions();
        else if (!strcmp(argv[i], "--library") && i + 1 < argc) library_path = argv[++i];
        else if (!strcmp(argv[i], "--area") && i + 1 < argc) area = strtol(argv[++i], &end, 10);
        else if (!strcmp(argv[i], "--room") && i + 1 < argc) room_number = strtol(argv[++i], &end, 10);
        else if (!strcmp(argv[i], "--weapon") && i + 1 < argc) weapon_name = argv[++i];
        else if (!strcmp(argv[i], "--hitboxes")) show_hitboxes = true;
        else if (!strcmp(argv[i], "--repeat") && i + 1 < argc) repeat = atol(argv[++i]);
        else if (!strcmp(argv[i], "--atk") && i + 1 < argc) player_stats.atk = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--def") && i + 1 < argc) player_stats.def = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--hp") && i + 1 < argc)
            player_stats.hp = player_stats.max_hp = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--moves") && i + 1 < argc) {
            moves = strtoul(argv[++i], &end, 0);
            if (*end || moves > 0x1F) { usage(argv[0]); return 2; }
            end = NULL;
        } else if (!strcmp(argv[i], "--spawn") && i + 2 < argc) {
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
    char folder_buffer[512], room_path[600];
    if (!folder) {
        if (area < 0 || area > 99 || room_number < 0 || room_number > 999) { usage(argv[0]); return 2; }
        room_folder(folder_buffer, sizeof folder_buffer, (int)area, (int)room_number);
        folder = folder_buffer;
    }
    snprintf(room_path, sizeof room_path, "%s/room.tsv", folder);

    AriaRoom room = {0};
    if (!load_room(room_path, &room)) {
        fprintf(stderr, "Export the room first: python3 -m scripts.aos_runtime_room "
                        "--area <A> --room <R>\n");
        return 1;
    }
    AosCollision layer = room_layer(&room);
    if (spawn_x < 0 && !find_spawn(&layer, &spawn_x, &spawn_y)) {
        fprintf(stderr, "No floor with headroom found in %s\n", room_path);
        free_room(&room);
        return 1;
    }
    static AriaLibrary library;
    if (check) {
        bool ok = load_library(library_path, &library, NULL);
        if (ok) printf("Aria room %s: %d x %d cells, Soma library %d animations, spawn %d,%d\n",
                       folder, room.width_cells, room.height_cells, library.set.count,
                       spawn_x, spawn_y);
        free_room(&room);
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
    background = load_background(renderer, folder);
    if (!background || !load_library(library_path, &library, renderer)) goto cleanup;
    static AriaObjects objects;
    bool objects_ok = load_objects(DEFAULT_OBJECTS, &objects, renderer);
    bool enemies_ok = objects_ok && load_enemy_data(&objects);
    if (!enemies_ok) fprintf(stderr, "No object library: python3 -m scripts.aos_object_sprites\n");
    int cam_x = 0, cam_y = 0;

    AosSoma soma = aos_soma_spawn(spawn_x << 16, spawn_y << 16, &library.set);
    soma.moves = (uint32_t)moves;
    soma.hp = (int16_t)player_stats.hp;
    soma.max_hp = (int16_t)player_stats.max_hp;
    if (!load_weapon(DEFAULT_WEAPONS, weapon_name, &soma.weapon)) {
        fprintf(stderr, "Export the weapons first: python3 -m scripts.aos_weapons\n");
        goto cleanup;
    }
    /* Only the class 0, 2 and 3 entity (sub_080221CC) is ported. */
    static AriaWeaponSprite weapon_sprite;
    bool weapon_ported = soma.weapon.weapon_class == 0 || soma.weapon.weapon_class == 2 ||
                         soma.weapon.weapon_class == 3;
    bool weapon_loaded = weapon_ported && load_weapon_sprite(weapon_name, &weapon_sprite, renderer);
    AosWeaponEntity weapon_entity = {0};
    AriaCombat combat = {enemies_ok ? &objects.bat_kind : NULL, &objects.bat_stats,
                         &weapon_entity, weapon_loaded ? &weapon_sprite.data : NULL,
                         &player_stats};
    follow_camera(&room, spawn_x, spawn_y, &cam_x, &cam_y);
    uint16_t previous = 0;
    AosForcedInput forced = {0};
    bool announced_death = false;
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
            if (capture_path && repeat > 0 && step % repeat == repeat - 1) held = 0;
            if (forced.active) held = forced.held;
            forced.active = false;
            aos_soma_update(&soma, &layer, held, (uint16_t)(held & ~previous));
            previous = held;
            ++step;
            if (soma.state == 16 && !announced_death) {
                /* The game-over mode (0x42C |= 0x10) is not ported. */
                printf("Soma died (frame %ld)\n", step);
                announced_death = true;
            }
            if (aos_room_outside(&layer, room.width_screens, room.height_screens, soma.x >> 16,
                                 soma.y >> 16)) {
                if (!take_exit(renderer, &room, &background, &soma, cam_x, cam_y)) goto cleanup;
                layer = room_layer(&room);
            }
            /* sub_080426B0 after the player update. */
            aos_combat_tick(&soma.combat);
            follow_camera(&room, soma.x >> 16, soma.y >> 16, &cam_x, &cam_y);
            /* Entity slots: the weapon entity updates before the enemies. */
            if (weapon_loaded) aos_weapon_update(&weapon_entity, &soma, &weapon_sprite.data);
            update_entities(&room, &layer, &soma, cam_x, cam_y, &forced, &combat);
        }

        int sx = soma.x >> 16, sy = soma.y >> 16;
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_FRect src = {(float)cam_x, (float)cam_y, VIEW_W, VIEW_H}, dst = {0, 0, VIEW_W, VIEW_H};
        SDL_RenderTexture(renderer, background, &src, &dst);
        draw_doors(renderer, &room, objects_ok ? &objects : NULL, step, cam_x, cam_y);
        if (objects_ok) draw_enemies(renderer, &room, &objects, cam_x, cam_y);
        if (show_hitboxes) {
            AosRect hurt = aos_player_rect(soma.hurtbox, soma.x >> 16, soma.y >> 16,
                                           soma.facing_left, false);
            SDL_FRect r = {(float)(hurt.x1 - cam_x), (float)(hurt.y1 - cam_y),
                           (float)(hurt.x2 - hurt.x1), (float)(hurt.y2 - hurt.y1)};
            SDL_SetRenderDrawColor(renderer, 80, 230, 120, 255);
            SDL_RenderRect(renderer, &r);
            for (size_t i = 0; enemies_ok && i < room.entity_count; ++i) {
                const AriaEntity *e = &room.entities[i];
                if (!e->spawned || e->kind != ARIA_KIND_ENEMY || e->enemy.removed ||
                    e->enemy.anim.id >= AOS_ENEMY_MAX_ANIMS ||
                    e->enemy.anim.frame >= AOS_ENEMY_MAX_FRAMES ||
                    !objects.bat_kind.modes[e->enemy.anim.id][e->enemy.anim.frame])
                    continue;
                AosRect b = aos_entity_rect(objects.bat_kind.hurt[e->enemy.anim.id][e->enemy.anim.frame],
                                            e->enemy.x >> 16, e->enemy.y >> 16, e->enemy.mirrored,
                                            e->enemy.vflip);
                SDL_FRect er = {(float)(b.x1 - cam_x), (float)(b.y1 - cam_y),
                                (float)(b.x2 - b.x1 + 1), (float)(b.y2 - b.y1 + 1)};
                SDL_SetRenderDrawColor(renderer, 240, 220, 60, 255);
                SDL_RenderRect(renderer, &er);
            }
        }
        const AriaFrame *frame = NULL;
        if (soma.anim.frame < MAX_FRAMES)
            frame = &library.frames[soma.anim.id][soma.anim.frame];
        if (frame && frame->texture) {
            /* Library offsets face right; facing left mirrors around x. */
            float x = soma.facing_left ? (float)(sx - frame->offset_x) - frame->w
                                       : (float)(sx + frame->offset_x);
            SDL_FRect rect = {x - (float)cam_x, (float)(sy + frame->offset_y - cam_y), frame->w,
                              frame->h};
            SDL_RenderTextureRotated(renderer, frame->texture, NULL, &rect, 0, NULL,
                                     soma.facing_left ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        }
        if (weapon_loaded && weapon_entity.active && weapon_entity.anim.frame < weapon_sprite.count) {
            const AriaFrame *blade = &weapon_sprite.frames[weapon_entity.anim.frame];
            int wy = sy + weapon_entity.y_offset;
            float x = weapon_entity.facing_left ? (float)(sx - blade->offset_x) - blade->w
                                                : (float)(sx + blade->offset_x);
            SDL_FRect rect = {x - (float)cam_x, (float)(wy + blade->offset_y - cam_y), blade->w,
                              blade->h};
            SDL_RenderTextureRotated(renderer, blade->texture, NULL, &rect, 0, NULL,
                                     weapon_entity.facing_left ? SDL_FLIP_HORIZONTAL
                                                               : SDL_FLIP_NONE);
            int hx, hy, hw, hh;
            if (show_hitboxes && aos_weapon_hitbox(&weapon_entity, &soma, &weapon_sprite.data,
                                                   &hx, &hy, &hw, &hh)) {
                SDL_FRect box = {(float)(hx - cam_x), (float)(hy - cam_y), (float)hw, (float)hh};
                SDL_SetRenderDrawColor(renderer, 255, 60, 60, 255);
                SDL_RenderRect(renderer, &box);
            }
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
    free_weapon_sprite(&weapon_sprite);
    free_objects(&objects);
    if (background) SDL_DestroyTexture(background);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    free_room(&room);
    return rc;
}
