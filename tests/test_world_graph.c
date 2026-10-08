#include "core/world_graph.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    FusionWorldGraph graph = {0};
    FusionWorldGraph restored = {0};
    char error[160] = {0};
    const char *a = "metroid:demo:save_01";
    const char *b = "aria:demo:save_01";
    const char *path = "graph-test-output.mvg";
    assert(fusion_graph_load(&graph, "tests/fixtures/world_graph_sample.mvg", error, sizeof(error)));
    assert(fusion_graph_valid(&graph));
    assert(graph.room_count == 2 && graph.link_count == 1);
    assert(fusion_graph_travel_allowed(&graph, a, b, true));
    assert(fusion_graph_travel_allowed(&graph, b, a, true));
    assert(!fusion_graph_travel_allowed(&graph, a, b, false));
    assert(!fusion_graph_travel_allowed(&graph, a, a, true));
    assert(!fusion_graph_travel_allowed(&graph, a, "unknown", true));
    graph.links[0].enabled = false;
    assert(!fusion_graph_travel_allowed(&graph, a, b, true));
    assert(fusion_graph_save(&graph, path, error, sizeof(error)));
    assert(fusion_graph_load(&restored, path, error, sizeof(error)));
    assert(!fusion_graph_travel_allowed(&restored, a, b, true));
    assert(restored.rooms[1].graph_x == 550);
    remove(path);
    graph.links[0].enabled = true;
    graph.links[0].to = 0;
    assert(!fusion_graph_valid(&graph));
    puts("World graph save-room contracts passed.");
    return 0;
}
