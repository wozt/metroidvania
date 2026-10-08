#ifndef FUSION_WORLD_GRAPH_H
#define FUSION_WORLD_GRAPH_H

#include "core/types.h"
#include <stdbool.h>
#include <stddef.h>

/* Project-authored PC data, NOT a decoded original GBA room format. */
#define FUSION_GRAPH_ROOMS_MAX 32u
#define FUSION_GRAPH_LINKS_MAX 32u
#define FUSION_GRAPH_ID_LEN 64u

typedef struct {
    char id[FUSION_GRAPH_ID_LEN];
    WorldKind world;
    bool save_room;
    int graph_x;
    int graph_y;
} FusionGraphRoom;

typedef struct {
    unsigned from;
    unsigned to;
    bool enabled;
} FusionGraphLink;

typedef struct {
    FusionGraphRoom rooms[FUSION_GRAPH_ROOMS_MAX];
    FusionGraphLink links[FUSION_GRAPH_LINKS_MAX];
    unsigned room_count;
    unsigned link_count;
} FusionWorldGraph;

bool fusion_graph_valid(const FusionWorldGraph *graph);
bool fusion_graph_load(FusionWorldGraph *out, const char *path,
                       char *error, size_t error_size);
bool fusion_graph_save(const FusionWorldGraph *graph, const char *path,
                       char *error, size_t error_size);
bool fusion_graph_travel_allowed(const FusionWorldGraph *graph,
                                  const char *from, const char *to,
                                  bool standing_at_save_point);

#endif
