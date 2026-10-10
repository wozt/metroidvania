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
    free(r);
    return 0;
}
