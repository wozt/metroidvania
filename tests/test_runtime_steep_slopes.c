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
    room_set_cell(r, 1, 1, CLIP_RIGHT_STEEP);
    room_set_cell(r, 2, 1, CLIP_LEFT_STEEP);
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

    /* ClipdataConvertToCollision for the slight slopes (two blocks per
     * slope) and the actor-dependent types. */
    assert(!clip_solid(CLIP_LEFT_UPPER_SLIGHT, 62, 30, ACTOR_SAMUS));
    assert(clip_solid(CLIP_LEFT_UPPER_SLIGHT, 62, 31, ACTOR_SAMUS));
    assert(!clip_solid(CLIP_LEFT_LOWER_SLIGHT, 0, 30, ACTOR_SAMUS));
    assert(clip_solid(CLIP_LEFT_LOWER_SLIGHT, 0, 31, ACTOR_SAMUS));
    assert(clip_solid(CLIP_RIGHT_LOWER_SLIGHT, 0, 63, ACTOR_SAMUS));
    assert(!clip_solid(CLIP_RIGHT_LOWER_SLIGHT, 0, 62, ACTOR_SAMUS));
    assert(clip_solid(CLIP_RIGHT_UPPER_SLIGHT, 63, 0, ACTOR_SAMUS));
    assert(!clip_solid(CLIP_RIGHT_UPPER_SLIGHT, 0, 30, ACTOR_SAMUS));
    assert(clip_solid(CLIP_DOOR, 10, 10, ACTOR_SAMUS));
    assert(clip_solid(CLIP_ENEMY_ONLY, 10, 10, ACTOR_SAMUS));
    assert(!clip_solid(CLIP_ENEMY_ONLY, 10, 10, ACTOR_SPRITE));
    assert(!clip_solid(CLIP_STOP_ENEMY, 10, 10, ACTOR_NON_SPRITE));
    assert(clip_solid(CLIP_STOP_ENEMY, 10, 10, ACTOR_SPRITE));
    assert(!clip_solid(CLIP_TANK, 10, 10, ACTOR_SAMUS));
    assert(clip_solid(CLIP_TANK, 10, 10, ACTOR_NON_SPRITE));
    assert(!clip_solid(CLIP_PASS_THROUGH_BOTTOM, 10, 10, ACTOR_SAMUS));
    /* Point queries: Samus sees solid columns outside the room. */
    assert(solid_point(r, -1, 10, ACTOR_SAMUS));
    assert(!solid_point(r, -1, 10, ACTOR_NON_SPRITE));
    assert(!solid_point(r, 10, 64 * 4, ACTOR_SAMUS));
    assert(solid_point(r, 16 * 4 + 63, 16 * 4 + 63, ACTOR_SAMUS));
    assert(runtime_collision_point(r, 16 * 4 + 63, 16 * 4 + 63, 1));
    room_set_cell(r, 0, 3, CLIP_TANK);
    assert(!blocked(r, 0.f, 48.f, 8.f, 8.f));
    room_set_cell(r, 0, 3, CLIP_AIR);

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
    memset(r->types, 0, sizeof r->types);
    for (int i = 0; i < 4; ++i) room_set_cell(r, i, 3, CLIP_SOLID);
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
    /* Exported native rooms keep every resolved Clipdata type. */
    Room *native = calloc(1, sizeof *native);
    assert(native);
    assert(parse_native_room(FIXTURE_DIR "/runtime_native_room.tsv", native));
    assert(native->width == 64 && native->height == 48 && native->count == 4);
    assert(room_cell(native, 0, 2) == CLIP_SOLID);
    assert(room_cell(native, 2, 2) == CLIP_RIGHT_STEEP);
    assert(room_cell(native, 3, 2) == CLIP_DOOR);
    assert(room_cell(native, 3, 1) == CLIP_AIR);
    free(native);

    /* Doors: BgClipCheckTouchingTransitionOrTank + ConnectionCheckEnterDoor,
     * the RoomLoad exit placement and hatches opened by shots. */
    Room *doors = calloc(1, sizeof *doors);
    assert(doors);
    doors->width = 128;
    doors->height = 96;
    for (int row = 2; row < 6; ++row) {
        doors->behaviors[row * 8 + 7] = BEHAVIOR_DOOR;
        room_set_cell(doors, 6, row, CLIP_DOOR);
    }
    doors->doors[0] = (RoomDoor){.index = 71, .kind = "hatch", .x0 = 7, .x1 = 7,
        .y0 = 2, .y1 = 5, .destination = "brinstar_031", .dest_x = 2,
        .dest_y_end = 18, .dest_x_exit = 32, .dest_y_exit = 0};
    doors->door_count = 1;
    doors->hatches[0] = (RoomHatch){.door = 71, .x = 6, .y = 2, .type = "normal",
        .weakness = DAMAGE_BEAM | DAMAGE_MISSILE, .health = 0};
    doors->hatches[1] = (RoomHatch){.door = 72, .x = 1, .y = 2, .type = "locked",
        .weakness = DAMAGE_BEAM, .health = 0};
    doors->hatch_count = 2;
    mzm_samus_init(&samus, 7 * 64 - 7, 6 * 64, 1);
    int offset = -1;
    const RoomDoor *door = runtime_door_touched(doors, &samus, &offset);
    assert(door && door->index == 71 && offset == 0);
    samus.x -= 1;
    assert(!runtime_door_touched(doors, &samus, &offset));
    samus.x += 1;
    /* Jumping through the door keeps Samus's height inside it. */
    samus.y -= 40;
    door = runtime_door_touched(doors, &samus, &offset);
    assert(door && offset == 40);
    runtime_place_after_door(&samus, door, offset);
    assert(samus.x == 2 * 64 + (32 + 8) * 4);
    assert(samus.y == 19 * 64 - 1 - 40 + 1);
    /* A beam opens the normal hatch; the locked one stays shut. */
    assert(doors->count == 4);
    runtime_collision_affect(doors, 6 * 64 + 10, 3 * 64 + 5, DAMAGE_BEAM);
    assert(doors->hatches[0].open && doors->count == 0);
    assert(room_cell(doors, 6, 2) == CLIP_AIR && room_cell(doors, 6, 5) == CLIP_AIR);
    assert(!runtime_hit_hatch(doors, 1 * 64 + 10, 3 * 64, DAMAGE_BEAM));
    assert(!doors->hatches[1].open);
    free(doors);

    /* SamusUpdatePalette row selection and the echo's palette bank 1. */
    static RuntimePalettes palettes;
    assert(runtime_palettes_open(&palettes, FIXTURE_DIR "/samus_palettes.tsv"));
    assert(runtime_palette_row(&palettes, "VariaSuit", "Flashing", 1));
    assert(!runtime_palette_row(&palettes, "VariaSuit", "Flashing", 2));
    mzm_samus_init(&samus, 0, 0, 1);
    RuntimePaletteChoice choice = runtime_samus_palette(&samus, false, "VariaSuit", 0);
    assert(!strcmp(choice.kind0, "Default") && choice.row0 == 0 && choice.row1 == 1);
    samus.invincibility = 10;
    choice = runtime_samus_palette(&samus, false, "VariaSuit", 2);
    assert(!strcmp(choice.kind0, "Flashing") && choice.row0 == 1);
    assert(!strcmp(choice.kind1, "Default") && choice.row1 == 1);
    samus.invincibility = 0;
    choice = runtime_samus_palette(&samus, true, "VariaSuit", 0);
    assert(!strcmp(choice.kind0, "BeamRelease"));
    samus.unmorph_palette_timer = 7;
    choice = runtime_samus_palette(&samus, false, "Suitless", 0);
    assert(!strcmp(choice.suit0, "PowerSuit") && !strcmp(choice.kind0, "Unmorph") &&
           choice.row0 == 1);
    samus.unmorph_palette_timer = 12;
    assert(runtime_samus_palette(&samus, false, "VariaSuit", 0).row0 == 0);
    samus.unmorph_palette_timer = 0;
    samus.pose = MZM_POSE_SCREW_ATTACKING;
    samus.anim_frame = 3;
    assert(!strcmp(runtime_samus_palette(&samus, false, "VariaSuit", 0).kind0, "Flashing"));
    samus.pose = MZM_POSE_DYING;
    samus.anim_frame = 13;
    choice = runtime_samus_palette(&samus, false, "VariaSuit", 0);
    assert(!strcmp(choice.suit0, "PowerSuit") && !strcmp(choice.suit1, "Generic") &&
           choice.row1 == 5);
    samus.anim_frame = 2;
    choice = runtime_samus_palette(&samus, false, "VariaSuit", 0);
    assert(!strcmp(choice.suit1, "VariaSuit") && !strcmp(choice.kind1, "Dying"));
    samus.pose = MZM_POSE_STANDING;
    choice = runtime_samus_palette(&samus, false, "VariaSuit", 0);
    SDL_Color body[32], echo_colors[32];
    assert(runtime_palette_colors(&palettes, &choice, false, body));
    assert(runtime_palette_colors(&palettes, &choice, true, echo_colors));
    assert(!memcmp(echo_colors, echo_colors + 16, sizeof(SDL_Color) * 16));
    assert(!memcmp(body + 16, echo_colors + 16, sizeof(SDL_Color) * 16));
    assert(memcmp(body, echo_colors, sizeof(SDL_Color) * 16));
    free(r);
    return 0;
}
