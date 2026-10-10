/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the verified MZM steep-floor Clipdata geometry. */
#define FUSION_RUNTIME_TEST 1
#include "../src/runtime/room_runtime.c"
#include <assert.h>

int main(void) {
    /* Keep the translation-unit helpers referenced with -Werror enabled. */
    (void)parse_room; (void)parse_native_source; (void)find_spawn;
    (void)clampf; (void)move_axis; (void)move_grounded_x;
    Room *r = calloc(1, sizeof *r);
    assert(r);
    r->width = 64;
    r->height = 64;
    r->count = 2;
    r->collisions[0] = (Collision){16,16,16,16,17};
    r->collisions[1] = (Collision){32,16,16,16,18};
    /* RIGHT_STEEP: lower right solid, upper left air. */
    assert(!blocked(r,16.f,16.f,1.f,1.f));
    assert(blocked(r,31.f,16.f,1.f,1.f));
    assert(blocked(r,16.f,31.f,1.f,1.f));
    /* LEFT_STEEP: lower left solid, upper right air. */
    assert(blocked(r,32.f,16.f,1.f,1.f));
    assert(!blocked(r,47.f,16.f,1.f,1.f));
    assert(blocked(r,47.f,31.f,1.f,1.f));
    /* Neither slope should become a full 16x16 wall. */
    assert(!blocked(r,20.f,17.f,1.f,1.f));
    assert(!blocked(r,44.f,17.f,1.f,1.f));
    /* PATCH_0144: horizontal motion without SDL or a ROM. */
    assert(update_horizontal_velocity(0.f, 1.f, 0.1f, 100.f, 200.f, 300.f) == 20.f);
    assert(update_horizontal_velocity(95.f, 1.f, 0.1f, 100.f, 200.f, 300.f) == 100.f);
    assert(update_horizontal_velocity(30.f, 0.f, 0.1f, 100.f, 200.f, 300.f) == 0.f);
    assert(update_horizontal_velocity(-20.f, 1.f, 0.1f, 100.f, 200.f, 300.f) == 0.f);
    assert(update_horizontal_velocity(-95.f, -1.f, 0.1f, 100.f, 200.f, 300.f) == -100.f);
    /* PATCH_0145: verify the provisional state machine, no ROM. */
    assert(runtime_movement_state(true,0.f,0.f,0.f)==RUNTIME_IDLE);
    assert(runtime_movement_state(true,40.f,0.f,1.f)==RUNTIME_RUNNING);
    assert(runtime_movement_state(true,40.f,0.f,-1.f)==RUNTIME_TURNING);
    assert(runtime_movement_state(false,40.f,-25.f,1.f)==RUNTIME_JUMPING);
    assert(runtime_movement_state(false,0.f,0.f,0.f)==RUNTIME_FALLING);
    assert(strcmp(runtime_movement_state_name(RUNTIME_TURNING),"turning")==0);
    /* PATCH_0146: animation selection is independent of ROM assets. */
    (void)samus_frames_free;
    (void)samus_frames_load;
    assert(samus_animation_group(RUNTIME_IDLE)==0);
    assert(samus_animation_group(RUNTIME_RUNNING)==1);
    assert(samus_animation_group(RUNTIME_TURNING)==1);
    assert(samus_animation_group(RUNTIME_JUMPING)==2);
    assert(samus_animation_group(RUNTIME_FALLING)==2);
    /* PATCH_0147: cycle and boundary tests for original durations. */
    const unsigned int timing[] = {2, 3, 1};
    assert(samus_timeline_frame(timing,3,0)==0);
    assert(samus_timeline_frame(timing,3,1)==0);
    assert(samus_timeline_frame(timing,3,2)==1);
    assert(samus_timeline_frame(timing,3,4)==1);
    assert(samus_timeline_frame(timing,3,5)==2);
    assert(samus_timeline_frame(timing,3,6)==0);
    assert(samus_timeline_frame_once(timing,3,0)==0);
    assert(samus_timeline_frame_once(timing,3,5)==2);
    assert(samus_timeline_frame_once(timing,3,6)==2);
    assert(samus_timeline_frame_once(timing,3,100)==2);

    /* PATCH_0185: Morph Ball keeps its feet planted and cannot expand through
     * a low ceiling. Wall contact reports exactly one blocking side. */
    r->count=0;
    float shape_y=32.f,shape_height=16.f;
    assert(runtime_resize_height(r,16.f,&shape_y,12.f,&shape_height,10.f));
    assert(shape_y==38.f && shape_height==10.f);
    r->count=1;
    r->collisions[0]=(Collision){16,32,16,6,1};
    assert(!runtime_resize_height(r,16.f,&shape_y,12.f,&shape_height,16.f));
    assert(shape_y==38.f && shape_height==10.f);
    r->count=0;
    assert(runtime_resize_height(r,16.f,&shape_y,12.f,&shape_height,16.f));
    assert(shape_y==32.f && shape_height==16.f);
    r->count=1;
    r->collisions[0]=(Collision){0,0,16,64,1};
    assert(runtime_wall_side(r,16.f,16.f,12.f,16.f)==-1);
    r->collisions[0]=(Collision){28,0,16,64,1};
    assert(runtime_wall_side(r,16.f,16.f,12.f,16.f)==1);
    r->count=2;
    r->collisions[1]=(Collision){0,0,16,64,1};
    assert(runtime_wall_side(r,16.f,16.f,12.f,16.f)==0);

    /* PATCH_0186: only a clear solid-to-air corner with a supported standing
     * destination is a valid ledge. Both facing directions use the same rule. */
    r->width=96;
    r->height=96;
    r->count=1;
    r->collisions[0]=(Collision){48,32,16,32,1};
    RuntimeLedge ledge={0};
    assert(runtime_find_ledge(r,36.f,32.f,12.f,16.f,1,&ledge));
    assert(ledge.side==1);
    assert(ledge.hang_x==36.f && ledge.hang_y==32.f);
    assert(ledge.stand_x==48.f && ledge.stand_y==16.f);
    r->collisions[0]=(Collision){16,32,16,32,1};
    assert(runtime_find_ledge(r,32.f,32.f,12.f,16.f,-1,&ledge));
    assert(ledge.side==-1);
    assert(ledge.hang_x==32.f && ledge.stand_x==20.f);
    r->count=2;
    r->collisions[0]=(Collision){48,32,16,32,1};
    r->collisions[1]=(Collision){48,0,16,20,1};
    assert(!runtime_find_ledge(r,36.f,32.f,12.f,16.f,1,&ledge));
    assert(!runtime_find_ledge(r,36.f,32.f,12.f,16.f,0,&ledge));
    free(r);
    return 0;
}
