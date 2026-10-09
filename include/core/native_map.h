#ifndef FUSION_NATIVE_MAP_H
#define FUSION_NATIVE_MAP_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define NATIVE_MAX_CELLS 6144u
#define NATIVE_MAX_TILES 1024u
#define NATIVE_MAX_DIMENSION 255u
typedef struct {
    char room_id[64];
    char atlas[192];
    unsigned tileset, tile_count;
    unsigned width[2], height[2];
    uint16_t blocks[2][NATIVE_MAX_CELLS];
} NativeMap;
bool native_map_load(NativeMap *out,const char *path,char *error,size_t length);
bool native_map_save(const NativeMap *map,const char *path,char *error,size_t length);
bool native_map_edit(NativeMap *map,unsigned layer,int x,int y,
                     unsigned tool,unsigned brush,unsigned *picked);
#endif
