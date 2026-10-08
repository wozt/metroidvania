#include "core/native_map.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void)
{
    NativeMap *m=calloc(1,sizeof(*m)),*r=calloc(1,sizeof(*r));
    char err[160]={0};unsigned picked=999;
    assert(m&&r);
    strcpy(m->room_id,"mzm:brinstar:033");
    strcpy(m->atlas,"rooms/metroid/tilesets/27_atlas.bmp");
    m->tileset=27;m->tile_count=40;
    m->width[0]=3;m->height[0]=2;m->width[1]=3;m->height[1]=2;
    assert(native_map_edit(m,0,1,0,0,9,NULL));
    assert(native_map_edit(m,0,1,0,3,0,&picked)==false&&picked==9);
    assert(native_map_edit(m,0,1,0,2,2,NULL));
    assert(m->blocks[0][1]==2&&m->blocks[0][0]==0);
    assert(!native_map_edit(m,0,0,0,0,40,NULL));
    assert(native_map_edit(m,1,0,0,0,3,NULL));
    assert(native_map_save(m,"test-native-workspace.mvnative",err,sizeof(err)));
    assert(native_map_load(r,"test-native-workspace.mvnative",err,sizeof(err)));
    assert(r->blocks[0][1]==2&&r->blocks[1][0]==3);
    FILE *f=fopen("test-native-workspace.mvnative","a");assert(f);
    fputs("EXTRA\n",f);fclose(f);
    assert(!native_map_load(r,"test-native-workspace.mvnative",err,sizeof(err)));
    remove("test-native-workspace.mvnative");free(m);free(r);
    puts("Native room edit, fill, pick, reject and roundtrip tests passed.");return 0;
}
