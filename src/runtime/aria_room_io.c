/* SPDX-License-Identifier: GPL-3.0-only */
/* Room I/O cluster of the Aria of Sorrow room runtime, moved
 * verbatim out of aria_runtime.c (AGENTS.md rule 8). See
 * aria_room_io.h for the cluster boundary. */
#include "aria_room_io.h"

#include "aos/core/aos_mp2k.h"
#include "aos/game/aos_save_room.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


AosRoomMusicFlags room_music_flags[MUSIC_AREAS][MUSIC_ROOMS];

bool load_room_music(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char line[128];
    unsigned rows = 0;
    while (fgets(line, sizeof line, f)) {
        unsigned area, room, save, warp, transition;
        if (sscanf(line, "%u\t%u\t%u\t%u\t%u", &area, &room, &save, &warp, &transition) != 5 ||
            area >= MUSIC_AREAS || room >= MUSIC_ROOMS)
            continue;
        room_music_flags[area][room] = (AosRoomMusicFlags){save != 0, warp != 0, transition != 0};
        rows++;
    }
    fclose(f);
    return rows > 0;
}

void start_mp2k_song(void *context, uint16_t song,
                     const AosAudioSongInfo *info) {
    (void)song;
    AosMp2kEngine *engine = context;
    (void)aos_mp2k_player_start(engine, info->player, info->header_pointer);
}

void free_room(AriaRoom *room) {
    free(room->cells);
    free(room->blocks);
    room->cells = room->blocks = NULL;
}

AosCollision room_layer(const AriaRoom *room) {
    return (AosCollision){room->width_screens, room->height_screens, room->width_cells,
                          room->height_cells, room->cells, room->blocks, false};
}

bool load_room(const char *path, AriaRoom *room) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return false; }
    char line[4096], extra;
    int version, area, number, room_flags = 0, row = 0;
    bool ended = false;
    if (!fgets(line, sizeof line, f)) goto failure;
    int fields = sscanf(line, "AOSROOM-NATIVE\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d %c", &version,
                        &area, &number, &room->width_screens, &room->height_screens,
                        &room->width_cells, &room->height_cells, &room_flags, &extra);
    bool valid_header = (version == 5 || version == 4) && fields == 8 &&
                        room_flags >= 0 && room_flags <= 3;
    if (!valid_header) {
        room_flags = 0;
        fields = sscanf(line, "AOSROOM-NATIVE\t%d\t%d\t%d\t%d\t%d\t%d\t%d %c", &version,
                        &area, &number, &room->width_screens, &room->height_screens,
                        &room->width_cells, &room->height_cells, &extra);
        valid_header = version == 3 && fields == 7;
    }
    if (!valid_header ||
        room->width_cells < 1 || room->height_cells < 1 ||
        room->width_cells > 1024 || room->height_cells > 1024)
        goto failure;
    room->area = area;
    room->number = number;
    room->room_flags = (uint8_t)room_flags;
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
            int entity_fields = version >= 5
                ? sscanf(line, "E\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d %c",
                         &e->kind, &e->id, &e->x, &e->y, &e->persistent_index,
                         &e->param0, &e->param1, &e->flags, &extra)
                : sscanf(line, "E\t%d\t%d\t%d\t%d\t%d\t%d\t%d %c",
                         &e->kind, &e->id, &e->x, &e->y,
                         &e->param0, &e->param1, &e->flags, &extra);
            if (room->entity_count >= MAX_ENTITIES || entity_fields != (version >= 5 ? 8 : 7) ||
                e->kind < 0 || e->kind > 255 || e->id < 0 || e->id > 255)
                goto failure;
            e->spawned = false;
            e->secret_mark = false;
            e->object20_anim = -1;
            e->wo_bars_count = 0;
            if (e->kind == ARIA_KIND_SPECIAL && e->id == AOS_OBJECT_SAVE_POINT)
                room->room_flags |= AOS_ROOM_FLAG_SAVE;
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

bool room_has_save_point(const AriaRoom *room) {
    return room && (room->room_flags & AOS_ROOM_FLAG_SAVE) != 0;
}

/* Reads the sprite index; textures are created only with a renderer. */
bool load_library(const char *path, AriaLibrary *lib, SDL_Renderer *renderer) {
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

void room_folder(char *out, size_t size, int area, int number) {
    snprintf(out, size, "%s/area_%02d_room_%03d", DEFAULT_ROOMS, area, number);
}

/* The map editor's project collision (aos_room_overlay), applied to every
 * room loaded for play; --no-overlays and the transition audit leave the
 * native rooms, --overlay-dir DIR reads another directory. */
bool aria_overlays_enabled = true;
const char *aria_overlay_directory = AOS_ROOM_OVERLAY_DIRECTORY;

void apply_room_overlay(AriaRoom *room) {
    char path[600], message[800];
    if (!aria_overlays_enabled ||
        !aos_room_document_path(path, sizeof path, aria_overlay_directory, room->area,
                                room->number))
        return;
    int changed = 0;
    if (aos_room_collision_apply(path, room->area, room->number, room->width_cells,
                                 room->height_cells, room->cells, &changed, message,
                                 sizeof message) != AOS_OVERLAY_NONE)
        fprintf(stderr, "%s\n", message);
    /* The editor's transitions: a redirected native entry takes the target
     * and the spawn point, a new door joins the room's project doors. */
    AosProjectDoor doors[MAX_TRANSITIONS];
    int door_count = aos_room_project_doors(path, aria_overlay_directory, room->area, room->number,
                                            room->width_cells, room->height_cells,
                                            (int)room->transition_count, doors, MAX_TRANSITIONS,
                                            message, sizeof message);
    if (door_count != 0) fprintf(stderr, "%s\n", message);
    for (int k = 0; k < door_count; ++k) {
        const AosProjectDoor *d = &doors[k];
        if (d->target_area > 255 || d->target_room > 255) {
            fprintf(stderr, "editor door %d ignored: room %d/%d is beyond the native ids\n",
                    d->id, d->target_area, d->target_room);
            continue;
        }
        if (d->native_index >= 0) {
            AosTransition *t = &room->transitions[d->native_index];
            t->area = (uint8_t)d->target_area;
            t->room = (uint8_t)d->target_room;
            room->arrivals[d->native_index].set = true;
            room->arrivals[d->native_index].x = d->spawn_x;
            room->arrivals[d->native_index].y = d->spawn_y;
        } else {
            room->project_doors[room->project_door_count++] = *d;
        }
        printf("Editor door %d (%s) -> %d/%d at (%d, %d)\n", d->id,
               d->native_index >= 0 ? "native entry" : "new door", d->target_area,
               d->target_room, d->spawn_x, d->spawn_y);
    }
    /* The editor's entities join the room's records, after the native ones
     * (no persistent index; the runtime plays the kinds it has ported). */
    AosProjectEntity placed[MAX_ENTITIES];
    int count = aos_room_project_entities(path, room->area, room->number, room->width_cells,
                                          room->height_cells, placed, MAX_ENTITIES, message,
                                          sizeof message);
    if (count != 0) fprintf(stderr, "%s\n", message);
    for (int k = 0; k < count; ++k) {
        if (room->entity_count >= MAX_ENTITIES) {
            fprintf(stderr, "editor entities beyond the room's %d entity slots skipped\n",
                    MAX_ENTITIES);
            break;
        }
        const AosProjectEntity *p = &placed[k];
        AriaEntity *e = &room->entities[room->entity_count++];
        *e = (AriaEntity){.kind = p->kind, .id = p->id, .x = p->x, .y = p->y,
                          .persistent_index = -1, .param0 = p->param0, .param1 = p->param1,
                          .flags = p->flags, .spawner = -1, .object20_anim = -1};
        if (e->kind == ARIA_KIND_SPECIAL && e->id == AOS_OBJECT_SAVE_POINT)
            room->room_flags |= AOS_ROOM_FLAG_SAVE;
    }
}

bool load_room_folder(const char *folder, AriaRoom *room) {
    char path[600];
    snprintf(path, sizeof path, "%s/room.tsv", folder);
    if (!load_room(path, room)) return false;
    apply_room_overlay(room);
    return true;
}

/* Leaves every room through the middle of each transition's screen edge,
 * checks that the arrival lies inside the target room and that leaving the
 * target again through the opposite edge returns to the source room. */
int audit_transitions(void) {
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
void follow_camera(const AriaRoom *room, int32_t x, int32_t y, int *cam_x, int *cam_y) {
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

SDL_Texture *load_background(SDL_Renderer *renderer, const char *folder) {
    char path[600];
    snprintf(path, sizeof path, "%s/background.bmp", folder);
    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) { fprintf(stderr, "%s: %s\n", path, SDL_GetError()); return NULL; }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture) SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    return texture;
}

/* The editor's new door Soma's body touches (his column at x, the 32
 * pixels above his feet at y), or NULL. */
const AosProjectDoor *project_door_touched(const AriaRoom *room, int32_t x, int32_t y) {
    for (size_t k = 0; k < room->project_door_count; ++k) {
        const AosProjectDoor *d = &room->project_doors[k];
        if (x >= d->x && x < d->x + d->width && y >= d->y && y - 32 < d->y + d->height) return d;
    }
    return NULL;
}

/* A floor pixel with 40 free pixels above it, searched from the room
 * centre outward; this is a test placement, not native spawn data. */
bool find_spawn(const AosCollision *layer, int *out_x, int *out_y) {
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
