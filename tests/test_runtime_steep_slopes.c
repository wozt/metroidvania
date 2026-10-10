/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the verified MZM steep-floor Clipdata geometry and the
 * runtime's adapters around the native Samus pose controller. */
#define FUSION_RUNTIME_TEST 1
#include "../src/runtime/room_runtime.c"
#include <assert.h>

int main(void) {
    /* Keep the translation-unit helpers referenced with -Werror enabled. */
    (void)parse_room; (void)parse_native_source; (void)find_spawn;
    (void)clampf;
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
    assert(near_steep_slope(r,16.f,0.f,8.f,16.f));
    assert(!near_steep_slope(r,0.f,40.f,8.f,8.f));

    /* PATCH_0147: cycle and boundary tests for the catalogue browser. */
    const unsigned int timing[] = {2, 3, 1};
    assert(samus_timeline_frame(timing,3,0)==0);
    assert(samus_timeline_frame(timing,3,1)==0);
    assert(samus_timeline_frame(timing,3,2)==1);
    assert(samus_timeline_frame(timing,3,4)==1);
    assert(samus_timeline_frame(timing,3,5)==2);
    assert(samus_timeline_frame(timing,3,6)==0);

    /* Every spin pose keeps its own action; generic MidAir is only the
     * fallback of non-spinning airborne poses. */
    assert(strcmp(runtime_pose_action(MZM_POSE_SPINNING,0),"spin")==0);
    assert(strcmp(runtime_pose_action(MZM_POSE_STARTING_SPIN_JUMP,0),"spin_start")==0);
    assert(strcmp(runtime_pose_action(MZM_POSE_SCREW_ATTACKING,0),"screw_attack")==0);
    assert(strcmp(runtime_pose_action(MZM_POSE_SCREW_ATTACKING,MZM_ITEM_SPACE_JUMP),
                  "screw_attack_space")==0);
    assert(strcmp(runtime_pose_action(MZM_POSE_ROLLING,0),"rolling")==0);
    assert(strcmp(runtime_pose_fallback_action(MZM_POSE_ROLLING),"morph_ball")==0);
    assert(strcmp(runtime_pose_fallback_action(MZM_POSE_SPACE_JUMPING),"spin")==0);
    assert(strcmp(runtime_pose_fallback_action(MZM_POSE_TURNING_AROUND_MIDAIR),
                  "midair")==0);
    for (int pose = 0; pose < MZM_POSE_COUNT; ++pose)
        if (mzm_pose_is_spinning((MzmPose)pose))
            assert(strcmp(runtime_pose_action((MzmPose)pose,0),"midair")!=0 &&
                   strcmp(runtime_pose_fallback_action((MzmPose)pose),"midair")!=0);
    assert(strcmp(runtime_aim_name(MZM_AIM_UP),"up")==0);
    assert(strcmp(runtime_aim_name(MZM_AIM_FORWARD),"forward")==0);

    /* Diagnostic presets drive native suit types and item flags. */
    MzmEquipment equipment={0};
    runtime_apply_equipment(&equipment,2,MZM_ITEM_SPACE_JUMP);
    assert(equipment.suit==MZM_SUIT_FULLY_POWERED);
    assert(equipment.items&MZM_ITEM_GRAVITY_SUIT);
    assert(equipment.items&MZM_ITEM_SPACE_JUMP);
    runtime_apply_equipment(&equipment,4,MZM_ITEM_SPACE_JUMP);
    assert(equipment.suit==MZM_SUIT_SUITLESS && equipment.items==0);

    /* The safe spawn puts the native 14x31 standing box on the floor. */
    r->count=4;
    for (int i = 0; i < 4; ++i) r->collisions[i]=(Collision){16*i,48,16,16,1};
    MzmSamus samus;
    assert(runtime_spawn_samus(r,&samus));
    assert(samus.y==48*MZM_SUBPIXELS_PER_PIXEL);
    float bx,by,bw,bh;
    mzm_samus_box(&samus,&bx,&by,&bw,&bh);
    assert(bw==14.f && bh==31.f);
    assert(!runtime_collision_blocked(r,bx,by,bw,bh));
    assert(runtime_collision_blocked(r,bx,by+1.f,bw,bh));
    assert(!runtime_collision_slope(r,bx,by,bw,bh));

    /* SamusUpdateGraphicsOam echo trigger threshold. */
    samus.pose=MZM_POSE_SPINNING;
    samus.y_velocity=81;
    assert(runtime_echo_fast_ascent(&samus));
    samus.y_velocity=80;
    assert(!runtime_echo_fast_ascent(&samus));
    samus.pose=MZM_POSE_TURNING_AROUND_MIDAIR;
    samus.y_velocity=200;
    assert(!runtime_echo_fast_ascent(&samus));

    /* PATCH_0187: the native echo waits for three history entries, cycles
     * four distance-two samples, and expires after its countdown. */
    RuntimeEcho echo={0};
    float echo_x=0.f,echo_y=0.f;
    runtime_echo_step(&echo,10.f,20.f,true);
    runtime_echo_step(&echo,11.f,21.f,true);
    assert(!runtime_echo_sample(&echo,2,&echo_x,&echo_y));
    runtime_echo_step(&echo,12.f,22.f,true);
    assert(echo.timer==5);
    assert(runtime_echo_sample(&echo,2,&echo_x,&echo_y));
    assert(echo_x==10.f && echo_y==20.f && echo.position==1u);
    for(int i=0;i<5;i++)runtime_echo_step(&echo,13.f+(float)i,23.f,false);
    assert(echo.active && echo.timer==0);
    runtime_echo_step(&echo,20.f,23.f,false);
    assert(!echo.active);
    free(r);
    return 0;
}
