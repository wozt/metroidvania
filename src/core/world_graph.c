/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/world_graph.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void fail(char *error, size_t size, const char *message)
{
    if (error && size) snprintf(error, size, "%s", message);
}

static int room_index(const FusionWorldGraph *graph, const char *id)
{
    unsigned i;
    if (!graph || !id) return -1;
    for (i = 0; i < graph->room_count; ++i)
        if (strcmp(graph->rooms[i].id, id) == 0) return (int)i;
    return -1;
}

bool fusion_graph_valid(const FusionWorldGraph *graph)
{
    unsigned i, j;
    if (!graph || !graph->room_count ||
        graph->room_count > FUSION_GRAPH_ROOMS_MAX ||
        graph->link_count > FUSION_GRAPH_LINKS_MAX)
        return false;
    for (i = 0; i < graph->room_count; ++i) {
        const FusionGraphRoom *r = &graph->rooms[i];
        if (!r->id[0] || !memchr(r->id, 0, sizeof(r->id)) ||
            (r->world != WORLD_METROID && r->world != WORLD_CASTLEVANIA))
            return false;
        for (j = 0; j < i; ++j)
            if (strcmp(r->id, graph->rooms[j].id) == 0) return false;
    }
    for (i = 0; i < graph->link_count; ++i) {
        const FusionGraphLink *link = &graph->links[i];
        if (link->from >= graph->room_count ||
            link->to >= graph->room_count || link->from == link->to ||
            !graph->rooms[link->from].save_room ||
            !graph->rooms[link->to].save_room ||
            graph->rooms[link->from].world == graph->rooms[link->to].world)
            return false;
        for (j = 0; j < i; ++j)
            if ((graph->links[j].from == link->from &&
                 graph->links[j].to == link->to) ||
                (graph->links[j].to == link->from &&
                 graph->links[j].from == link->to))
                return false;
    }
    return true;
}

bool fusion_graph_travel_allowed(const FusionWorldGraph *graph,
                                  const char *from, const char *to,
                                  bool standing_at_save_point)
{
    unsigned i;
    int a, b;
    if (!standing_at_save_point || !fusion_graph_valid(graph)) return false;
    a = room_index(graph, from);
    b = room_index(graph, to);
    if (a < 0 || b < 0 || !graph->rooms[a].save_room ||
        !graph->rooms[b].save_room) return false;
    for (i = 0; i < graph->link_count; ++i) {
        const FusionGraphLink *link = &graph->links[i];
        if (link->enabled &&
            ((link->from == (unsigned)a && link->to == (unsigned)b) ||
             (link->from == (unsigned)b && link->to == (unsigned)a)))
            return true;
    }
    return false;
}

/* Deliberately small, strict, versioned plain-text format for the editor and
 * PC game. Initial rooms are DEMOS, not verified original save rooms.
 * MVGRAPH 1
 * ROOM id world(0/1) save(0/1) x y
 * LINK from-id to-id enabled(0/1)
 */
bool fusion_graph_load(FusionWorldGraph *out, const char *path,
                       char *error, size_t error_size)
{
    FusionWorldGraph candidate = {0};
    FILE *file;
    char line[320];
    unsigned line_number = 0;
    bool version_seen = false;
    bool links_started = false;
    if (!out || !path) { fail(error, error_size, "invalid graph input"); return false; }
    file = fopen(path, "r");
    if (!file) { fail(error, error_size, "cannot open graph file"); return false; }
    while (fgets(line, sizeof(line), file)) {
        char a[64], b[64], extra[2];
        int w, save, x, y, enabled;
        size_t length = strlen(line);
        ++line_number;
        if (length && line[length - 1] != '\n' && !feof(file)) goto malformed;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (!version_seen) {
            if (strcmp(line, "MVGRAPH 1\n") != 0 &&
                strcmp(line, "MVGRAPH 1") != 0) goto malformed;
            version_seen = true;
        } else if (sscanf(line, "ROOM %63s %d %d %d %d %1s",
                          a, &w, &save, &x, &y, extra) == 5) {
            FusionGraphRoom *room;
            if (links_started || candidate.room_count == FUSION_GRAPH_ROOMS_MAX ||
                (w != 0 && w != 1) || (save != 0 && save != 1) ||
                room_index(&candidate, a) >= 0) goto malformed;
            room = &candidate.rooms[candidate.room_count++];
            snprintf(room->id, sizeof(room->id), "%s", a);
            room->world = (WorldKind)w;
            room->save_room = save != 0;
            room->graph_x = x;
            room->graph_y = y;
        } else if (sscanf(line, "LINK %63s %63s %d %1s",
                          a, b, &enabled, extra) == 3) {
            FusionGraphLink *link;
            int from, to;
            links_started = true;
            from = room_index(&candidate, a);
            to = room_index(&candidate, b);
            if (candidate.link_count == FUSION_GRAPH_LINKS_MAX ||
                (enabled != 0 && enabled != 1) || from < 0 || to < 0)
                goto malformed;
            link = &candidate.links[candidate.link_count++];
            link->from = (unsigned)from;
            link->to = (unsigned)to;
            link->enabled = enabled != 0;
        } else goto malformed;
    }
    if (ferror(file) || !version_seen || !fusion_graph_valid(&candidate))
        goto malformed;
    fclose(file);
    *out = candidate;
    fail(error, error_size, "");
    return true;
malformed:
    fclose(file);
    if (error && error_size)
        snprintf(error, error_size, "invalid graph format near line %u", line_number);
    return false;
}

bool fusion_graph_save(const FusionWorldGraph *graph, const char *path,
                       char *error, size_t error_size)
{
    FILE *file;
    unsigned i;
    char temporary[512];
    if (!fusion_graph_valid(graph) || !path ||
        snprintf(temporary, sizeof(temporary), "%s.tmp", path) >=
            (int)sizeof(temporary)) {
        fail(error, error_size, "invalid graph or output path");
        return false;
    }
    file = fopen(temporary, "w");
    if (!file) { fail(error, error_size, "cannot write graph file"); return false; }
    fprintf(file, "MVGRAPH 1\n");
    for (i = 0; i < graph->room_count; ++i) {
        const FusionGraphRoom *r = &graph->rooms[i];
        fprintf(file, "ROOM %s %d %d %d %d\n", r->id, (int)r->world,
                (int)r->save_room, r->graph_x, r->graph_y);
    }
    for (i = 0; i < graph->link_count; ++i) {
        const FusionGraphLink *l = &graph->links[i];
        fprintf(file, "LINK %s %s %d\n", graph->rooms[l->from].id,
                graph->rooms[l->to].id, (int)l->enabled);
    }
    {
        const bool failed = ferror(file) != 0;
        const bool closed = fclose(file) == 0;
        if (failed || !closed) {
        remove(temporary);
        fail(error, error_size, "graph write failed");
        return false;
        }
    }
    if (rename(temporary, path) != 0) {
        remove(temporary);
        fail(error, error_size, "cannot replace graph file");
        return false;
    }
    fail(error, error_size, "");
    return true;
}
