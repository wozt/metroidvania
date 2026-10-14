/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the verified MZM steep-floor Clipdata geometry and the
 * runtime's adapters around the native Samus pose controller. */
#define FUSION_RUNTIME_TEST 1
#include "../src/runtime/room_runtime.c"
#include <assert.h>

static bool test_no_event(void *context, uint8_t event) {
    (void)context;
    (void)event;
    return false;
}

int main(void) {
    /* Keep the translation-unit helpers referenced with -Werror enabled. */
    (void)parse_room; (void)parse_native_source; (void)find_spawn;
    (void)clampf; (void)runtime_blocks_frame; (void)runtime_blocks_load;
    (void)runtime_block_tables_open; (void)runtime_power_bomb_apply; (void)runtime_draw_power_bomb;
    /* The save file keeps the opened hatches and the destroyed
     * never-reform blocks. */
    {
        static MzmSaveData save;
        runtime_hatch_memory.opened[2][1] = 0x10;
        runtime_hatch_memory.destination_count = 1;
        strcpy(runtime_hatch_memory.destinations[0].alias, "kraid_003");
        runtime_hatch_memory.destinations[0].x = 5;
        runtime_hatch_memory.destinations[0].y = 6;
        runtime_never_reform_memory.count = 1;
        strcpy(runtime_never_reform_memory.blocks[0].alias, "norfair_010");
        runtime_never_reform_memory.blocks[0].x = 7;
        runtime_save_memories(&save);
        memset(&runtime_hatch_memory, 0, sizeof runtime_hatch_memory);
        memset(&runtime_never_reform_memory, 0, sizeof runtime_never_reform_memory);
        runtime_restore_memories(&save);
        assert(runtime_hatch_memory.opened[2][1] == 0x10 && runtime_hatch_memory.destination_count == 1 &&
               !strcmp(runtime_hatch_memory.destinations[0].alias, "kraid_003") &&
               runtime_hatch_memory.destinations[0].y == 6 && runtime_never_reform_memory.count == 1 &&
               !strcmp(runtime_never_reform_memory.blocks[0].alias, "norfair_010") &&
               runtime_never_reform_memory.blocks[0].x == 7);
        memset(&runtime_hatch_memory, 0, sizeof runtime_hatch_memory);
        memset(&runtime_never_reform_memory, 0, sizeof runtime_never_reform_memory);
    }
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

    /* SamusUpdateGraphicsOam's body animations as library keys. */
    {
        MzmSamus k;
        MzmEquipment suit={.suit=MZM_SUIT_NORMAL};
        char key[160];
        mzm_samus_init(&k,0,0,1);
        k.pose=MZM_POSE_RUNNING;
        k.aim=MZM_AIM_NONE;
        mzm_samus_graphics_key(&k,&suit,0,key,sizeof key);
        assert(strcmp(key,"PowerSuit/Running/ACD_NONE/right")==0);
        k.aim=MZM_AIM_DIAGONAL_UP;
        k.facing=-1;
        k.speedboosting=1;
        mzm_samus_graphics_key(&k,&suit,0,key,sizeof key);
        assert(strcmp(key,"PowerSuit/Running_Speedboosting/ACD_DIAGONALLY_UP/left")==0);
        k.pose=MZM_POSE_SCREW_ATTACKING;
        suit.items=MZM_ITEM_SPACE_JUMP;
        suit.suit=MZM_SUIT_FULLY_POWERED;
        mzm_samus_graphics_key(&k,&suit,0,key,sizeof key);
        assert(strcmp(key,"FullSuit/ScrewAttacking/TRUE/left")==0);
        k.pose=MZM_POSE_AIMING_WHILE_HANGING;
        k.aim=MZM_AIM_UP;
        mzm_samus_graphics_key(&k,&suit,0,key,sizeof key);
        assert(strcmp(key,"FullSuit/AimingWhileHanging/ACD_UP/left")==0);
        k.pose=MZM_POSE_SKIDDING;
        mzm_samus_graphics_key(&k,&suit,MZM_WH_MISSILE,key,sizeof key);
        assert(strcmp(key,"FullSuit/Skidding/TRUE/left")==0);
        k.pose=MZM_POSE_SPINNING;
        suit.suit=MZM_SUIT_SUITLESS;
        mzm_samus_graphics_key(&k,&suit,0,key,sizeof key);
        assert(strcmp(key,"Suitless/pose/SPOSE_SPINNING/left")==0);
        assert(mzm_samus_armed_graphics(MZM_WH_SUPER_MISSILE) && !mzm_samus_armed_graphics(MZM_WH_POWER_BOMB));
    }
    (void)runtime_samus_destruct;
    /* ClipdataUpdateCurrentAffecting: a water room (EFFECT_WATER's haze row
     * gives the water damage effect) below its effectY, needing a BG0. */
    {
        Room *liquid = calloc(1, sizeof *liquid);
        assert(liquid);
        liquid->width = liquid->height = 64;
        liquid->visual_effect = 1;
        liquid->effect_y = 2;
        runtime_haze_ok = true;
        runtime_haze[1][0] = 2;
        runtime_haze[1][1] = 1;
        assert(runtime_hazard(liquid, 3 * 64, 10) == MZM_HAZARD_NONE);  /* no BG0 */
        liquid->bg0_prop = 0x10;
        assert(runtime_hazard(liquid, 2 * 64 - 1, 10) == MZM_HAZARD_NONE);
        assert(runtime_hazard(liquid, 2 * 64, 10) == MZM_HAZARD_WATER);
        runtime_haze[1][0] = 0;     /* HAZE_VALUE_NONE: no damage effect */
        assert(runtime_hazard(liquid, 3 * 64, 10) == MZM_HAZARD_NONE);
        free(liquid);
    }

    /* A new game's equipment (SamusInit): 99 energy, nothing else. The
     * debug suit presets set the suit type and the Varia and Gravity Suits
     * only, the collected items kept; the graphics follow the equipment. */
    MzmEquipment equipment;
    runtime_new_equipment(&equipment);
    assert(equipment.energy==99 && equipment.max_energy==99 && equipment.items==0 &&
           equipment.suit==MZM_SUIT_NORMAL && equipment.max_missiles==0 &&
           equipment.max_super_missiles==0 && equipment.max_power_bombs==0);
    assert(!strcmp(runtime_suit_registry(&equipment),"PowerSuit"));
    equipment.items=MZM_ITEM_SPACE_JUMP|MZM_ITEM_MORPH_BALL;
    runtime_apply_suit_preset(&equipment,2);
    assert(equipment.suit==MZM_SUIT_FULLY_POWERED);
    assert(equipment.items&MZM_ITEM_GRAVITY_SUIT && equipment.items&MZM_ITEM_SPACE_JUMP &&
           equipment.items&MZM_ITEM_MORPH_BALL);
    assert(!strcmp(runtime_suit_registry(&equipment),"GravitySuit"));
    runtime_apply_suit_preset(&equipment,4);
    assert(equipment.suit==MZM_SUIT_SUITLESS &&
           equipment.items==(MZM_ITEM_SPACE_JUMP|MZM_ITEM_MORPH_BALL));
    assert(!strcmp(runtime_suit_registry(&equipment),"Suitless"));
    runtime_apply_suit_preset(&equipment,1);
    assert(!strcmp(runtime_suit_registry(&equipment),"VariaSuit"));

    /* A new game's state: Brinstar's room 0, nothing collected, the
     * difficulty kept. */
    {
        static MzmSaveData fresh;
        memset(&fresh, 0xA5, sizeof fresh);
        runtime_new_game_save(&fresh, 2);
        assert(!strcmp(fresh.room, "brinstar_000") && fresh.difficulty == 2 && fresh.area == 0 &&
               fresh.equipment.energy == 99 && fresh.equipment.items == 0 &&
               fresh.equipment.max_missiles == 0 && fresh.hatch_destination_count == 0 &&
               fresh.never_reform_count == 0 && !fresh.escaped_zebes && fresh.igt.hours == 0);
        for (int i = 0; i < MZM_SAVE_EVENTS; ++i) assert(fresh.events[i] == 0);
    }
    /* RoomLoad at door 0 (Brinstar's first door: x 38, y 2, exit 16, 16):
     * x = 38 * 64 + (16 + 8) * 4, native y = 3 * 64 + 16 * 4 - 1, facing
     * the foreground, facing left. */
    {
        Room *start = calloc(1, sizeof *start);
        assert(start);
        start->door_count = 1;
        start->doors[0] = (RoomDoor){.index = 0, .x0 = 38, .x1 = 40, .y0 = 2, .y1 = 2,
                                     .exit_x = 16, .exit_y = 16, .has_exit = true};
        MzmCollision none = {start, runtime_collision_blocked, runtime_collision_slope,
                             runtime_collision_point, NULL, NULL, NULL, NULL, NULL};
        MzmEquipment fresh_equipment;
        runtime_new_equipment(&fresh_equipment);
        MzmSamus placed;
        assert(runtime_place_at_door(start, 0, &placed, &fresh_equipment, &none));
        assert(placed.x == 38 * 64 + 24 * 4 && placed.y == 3 * 64 + 16 * 4 - 1 + 1 &&
               placed.pose == MZM_POSE_FACING_THE_FOREGROUND && placed.facing == -1);
        assert(!runtime_place_at_door(start, 1, &placed, &fresh_equipment, &none));
        start->doors[0].has_exit = false;
        assert(!runtime_place_at_door(start, 0, &placed, &fresh_equipment, &none));
        free(start);
    }

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
    /* An elevator ride (BgClipCheckTouchingTransitionOnElevator): an area
     * connection door with a destination is taken through the vertical
     * transition two blocks under Samus going up, without offset; RoomLoad
     * then moves her three blocks on. Elevator blocks always work without
     * the ROM bank. */
    {
        Room *shaft = calloc(1, sizeof *shaft);
        assert(shaft);
        shaft->width = 128;
        shaft->height = 160;
        snprintf(shaft->area, sizeof shaft->area, "Brinstar");
        shaft->behaviors[8 * 8 + 3] = BEHAVIOR_UP;
        shaft->doors[0] = (RoomDoor){.index = 0, .kind = "area", .x0 = 2, .x1 = 4, .y0 = 8, .y1 = 8,
            .destination = "crateria_003", .dest_x = 8, .dest_y_end = 21, .dest_x_exit = 0,
            .dest_y_exit = 0};
        shaft->doors[1] = (RoomDoor){.index = 1, .kind = "area", .x0 = 2, .x1 = 4, .y0 = 0, .y1 = 0,
            .destination = "-"};
        shaft->door_count = 2;
        MzmSamus rider;
        mzm_samus_init(&rider, 3 * 64 + 32, 6 * 64 + 1, 1);
        rider.pose = MZM_POSE_USING_AN_ELEVATOR;
        rider.elevator_direction = MZM_KEY_UP;
        int ride_offset = -1;
        const RoomDoor *ride = runtime_door_touched(shaft, &rider, &ride_offset);
        assert(ride && ride->index == 0 && ride_offset == 0);
        runtime_place_after_door(&rider, ride, ride_offset);
        assert(rider.x == 8 * 64 + 8 * 4 && rider.y == 22 * 64 - 1 + 1 + 3 * 64);
        /* Not past a transition, or without a destination: nothing. */
        mzm_samus_init(&rider, 3 * 64 + 32, 3 * 64 + 1, 1);
        rider.pose = MZM_POSE_USING_AN_ELEVATOR;
        rider.elevator_direction = MZM_KEY_UP;
        assert(!runtime_door_touched(shaft, &rider, &ride_offset));
        assert(!runtime_door_at(shaft, 3, 0) && runtime_door_at(shaft, 3, 8) == &shaft->doors[0]);
        assert(!runtime_cant_use_elevator(shaft));
        free(shaft);
    }
    /* A beam starts opening the normal hatch (BgClipCheckOpeningHatch);
     * ConnectionUpdateHatches plays its frames, writing the opening
     * blocks' collision (air here); the locked one stops the shot but
     * stays shut. */
    doors->has_hatch_tables = true;
    for (int i = 0; i < MZM_HATCH_COUNT; ++i) {
        doors->hatch_env.tilemap_values[i] = (uint16_t)(0x420 + i);
        doors->hatch_env.weakness[i] = DAMAGE_BEAM | DAMAGE_MISSILE;
    }
    for (int i = 0; i < 5; ++i) doors->hatch_env.durations[i] = 3;
    doors->hatch_env.durations[5] = 255;
    runtime_hatches_load(doors, "test", -1);
    assert(doors->native_hatch_count == 2 && !doors->native_hatches[0].facing_right &&
           doors->native_hatches[0].type == MZM_HATCH_NORMAL &&
           doors->native_hatches[1].type == MZM_HATCH_LOCKED);
    assert(doors->count == 4);
    runtime_collision_affect(doors, 6 * 64 + 10, 3 * 64 + 5, DAMAGE_BEAM);
    assert(doors->native_hatches[0].state == MZM_HATCH_OPENING && doors->count == 4);
    int frames = 0;
    while (doors->native_hatches[0].state == MZM_HATCH_OPENING && frames < 100) {
        mzm_hatches_update(doors->native_hatches, doors->native_hatch_count, &doors->hatch_env);
        ++frames;
    }
    /* One frame to start, then four frames of 3 + 1 ticks. */
    assert(doors->native_hatches[0].state == MZM_HATCH_OPENED && frames == 17);
    assert(doors->count == 0);
    assert(room_cell(doors, 6, 2) == CLIP_AIR && room_cell(doors, 6, 5) == CLIP_AIR);
    assert(runtime_hit_hatch(doors, 1 * 64 + 10, 3 * 64, DAMAGE_BEAM));
    assert(doors->native_hatches[1].state == MZM_HATCH_CLOSED);
    /* The gray hatch waits for the door timer (ConnectionLoadDoors'
     * hatchesLockedWithTimer): a sprite's countdown unlocks it
     * (ConnectionCheckUnlockDoors), then a beam opens it. */
    assert(runtime_hatch_locks.with_timer == 2 && runtime_hatch_locks.with_event == 0);
    runtime_hatch_locks.door_unlock_timer = -3;
    mzm_sfx_clear();
    assert(!mzm_hatches_check_unlock(&runtime_hatch_locks, &doors->hatch_env));
    assert(!mzm_hatches_check_unlock(&runtime_hatch_locks, &doors->hatch_env));
    assert(mzm_hatches_check_unlock(&runtime_hatch_locks, &doors->hatch_env) &&
           doors->hatch_env.unlocking && mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_DOORS_UNLOCKING));
    assert(!mzm_hatches_check_unlock(&runtime_hatch_locks, &doors->hatch_env));
    assert(runtime_hit_hatch(doors, 1 * 64 + 10, 3 * 64, DAMAGE_BEAM));
    assert(doors->native_hatches[1].state == MZM_HATCH_OPENING);
    /* An event lock before its event (ConnectionCheckHatchLockEvents):
     * the opened hatch shuts as a navigation lock. */
    doors->hatch_lock_events[0].door = doors->native_hatches[0].source_door;
    doors->hatch_lock_events[0].event = 42;
    doors->hatch_lock_events[0].before = true;
    doors->hatch_lock_events[0].unlockable = false;
    doors->hatch_lock_event_count = 1;
    runtime_hatch_lock_events(doors, test_no_event, NULL);
    assert(doors->native_hatches[0].type == MZM_HATCH_LOCKED_NAVIGATION &&
           doors->native_hatches[0].state == MZM_HATCH_CLOSED && runtime_hatch_locks.with_event == 1);
    /* LOCK_DOORS: every other hatch locks, the arrival one closing. */
    runtime_last_door = doors->native_hatches[1].source_door;
    mzm_hatches_lock_with_timer(doors->native_hatches, doors->native_hatch_count, &doors->hatch_env,
                                &runtime_hatch_locks, runtime_last_door);
    assert(runtime_hatch_locks.door_unlock_timer == 2 && runtime_hatch_locks.with_timer == 2 &&
           doors->native_hatches[1].type == MZM_HATCH_LOCKED &&
           doors->native_hatches[1].state == MZM_HATCH_CLOSING &&
           doors->native_hatches[0].type == MZM_HATCH_LOCKED_NAVIGATION);
    free(doors);

    /* ScrollBg2 for a BG_PROP_MOVING BG2 (the rising Chozo pillar): BG1
     * (the runtime camera plus the centred GBA screen) plus gBg2Movement,
     * kept within BG2. */
    {
        Room *bg2 = calloc(1, sizeof *bg2);
        assert(bg2);
        bg2->bg_w[2] = 40;
        bg2->bg_h[2] = 60;
        int x, y;
        runtime_bg2_moving_position(bg2, 100.f, 200.f, 0, 0, &x, &y);
        assert(x == 100 && y == 200);
        runtime_bg2_moving_position(bg2, 100.f, 200.f, 0, 1408, &x, &y);
        assert(x == 100 && y == 200 + 352);
        /* Past BG2's last screen (40 - 15 blocks across, 60 - 10 down). */
        runtime_bg2_moving_position(bg2, 500.f, 800.f, 0, 1408, &x, &y);
        assert(x == 25 * 16 - 40 && y == 50 * 16 - 32);
        runtime_bg2_moving_position(bg2, 0.f, 0.f, -400, -400, &x, &y);
        assert(x == -40 && y == -32);
        free(bg2);
    }

    /* SamusUpdatePalette row selection and the echo's palette bank 1. */
    static RuntimePalettes palettes;
    assert(runtime_palettes_open(&palettes, FIXTURE_DIR "/samus_palettes.tsv"));
    assert(runtime_palette_row(&palettes, "VariaSuit", "Flashing", 1));
    assert(!runtime_palette_row(&palettes, "VariaSuit", "Flashing", 2));
    mzm_samus_init(&samus, 0, 0, 1);
    RuntimePaletteChoice choice = runtime_samus_palette(&samus, false, "VariaSuit", 0, 0, 0);
    assert(!strcmp(choice.kind0, "Default") && choice.row0 == 0 && choice.row1 == 1);
    samus.invincibility = 10;
    choice = runtime_samus_palette(&samus, false, "VariaSuit", 2, 0, 0);
    assert(!strcmp(choice.kind0, "Flashing") && choice.row0 == 1);
    assert(!strcmp(choice.kind1, "Default") && choice.row1 == 1);
    samus.invincibility = 0;
    choice = runtime_samus_palette(&samus, true, "VariaSuit", 0, 0, 0);
    assert(!strcmp(choice.kind0, "BeamRelease"));
    samus.unmorph_palette_timer = 7;
    choice = runtime_samus_palette(&samus, false, "Suitless", 0, 0, 0);
    assert(!strcmp(choice.suit0, "PowerSuit") && !strcmp(choice.kind0, "Unmorph") &&
           choice.row0 == 1);
    samus.unmorph_palette_timer = 12;
    assert(runtime_samus_palette(&samus, false, "VariaSuit", 0, 0, 0).row0 == 0);
    /* The release row of the Long Beam, and the charging rows: two
     * consecutive rows by 16-frame step, none on the fourth. */
    samus.unmorph_palette_timer = 0;
    assert(runtime_samus_palette(&samus, true, "VariaSuit", 0, MZM_ITEM_LONG_BEAM, 0).row0 == 1);
    choice = runtime_samus_palette(&samus, false, "VariaSuit", 0, 0, MZM_CHARGE_FULL + 16);
    assert(!strcmp(choice.kind0, "ChargingBeam") && choice.row0 == 1 && choice.row1 == 2);
    choice = runtime_samus_palette(&samus, false, "VariaSuit", 0, MZM_ITEM_LONG_BEAM, MZM_CHARGE_FULL);
    assert(choice.row0 == 2 && choice.row1 == 3);
    choice = runtime_samus_palette(&samus, false, "VariaSuit", 0, 0, MZM_CHARGE_FULL + 48);
    assert(!strcmp(choice.kind0, "Default"));
    samus.unmorph_palette_timer = 0;
    samus.pose = MZM_POSE_SCREW_ATTACKING;
    samus.anim_frame = 3;
    assert(!strcmp(runtime_samus_palette(&samus, false, "VariaSuit", 0, 0, 0).kind0, "Flashing"));
    samus.pose = MZM_POSE_DYING;
    samus.anim_frame = 13;
    choice = runtime_samus_palette(&samus, false, "VariaSuit", 0, 0, 0);
    assert(!strcmp(choice.suit0, "PowerSuit") && !strcmp(choice.suit1, "Generic") &&
           choice.row1 == 5);
    samus.anim_frame = 2;
    choice = runtime_samus_palette(&samus, false, "VariaSuit", 0, 0, 0);
    assert(!strcmp(choice.suit1, "VariaSuit") && !strcmp(choice.kind1, "Dying"));
    samus.pose = MZM_POSE_STANDING;
    choice = runtime_samus_palette(&samus, false, "VariaSuit", 0, 0, 0);
    SDL_Color body[32], echo_colors[32];
    assert(runtime_palette_colors(&palettes, &choice, false, body));
    assert(runtime_palette_colors(&palettes, &choice, true, echo_colors));
    assert(!memcmp(echo_colors, echo_colors + 16, sizeof(SDL_Color) * 16));
    assert(!memcmp(body + 16, echo_colors + 16, sizeof(SDL_Color) * 16));
    assert(memcmp(body, echo_colors, sizeof(SDL_Color) * 16));
    /* BgClipCheckTouchingTransitionOrTank's probes, the tank collection,
     * BgClipFinishCollectingTank and BgClipRemoveCollectedTanks. */
    Room *tanks = calloc(1, sizeof *tanks);
    assert(tanks);
    tanks->width = 128;
    tanks->height = 96;
    tanks->room = 7;
    snprintf(tanks->area, sizeof tanks->area, "brinstar");
    runtime_block_tables.ok = true;
    runtime_block_tables.tanks_ok = true;
    runtime_block_tables.main_count = 64;
    runtime_block_tables.tilemap_count = 1;
    runtime_block_tables.main_behaviors[50] = MZM_TANK_BEHAVIOR_START + 4;  /* energy tank */
    runtime_block_tables.tanks.item_type[4] = MZM_ITEM_TYPE_ENERGY;
    runtime_block_tables.tanks.message[4] = MZM_MESSAGE_ENERGY_TANK_ACQUIRED;
    runtime_block_tables.tanks.total[MZM_TANK_ENERGY] = 12;
    runtime_block_tables.tanks.increase[MZM_TANK_ENERGY] = 100;
    runtime_block_tables.tanks.start[MZM_TANK_ENERGY] = 99;
    tanks->clip[4 * 8 + 3] = 50;
    /* Samus's right edge in block 3, her center row (top / 4) in row 4. */
    mzm_samus_init(&samus, 3 * 64, 5 * 64, 1);
    MzmTouchProbes probes;
    runtime_touch_probes(tanks, &samus, &probes);
    assert(probes.x[0] == 3 && probes.y[0] == 4 && probes.behaviors[0] == MZM_TANK_BEHAVIOR_START + 4);
    assert(probes.x[1] == 2 && probes.behaviors[1] == 0);
    MzmEquipment tank_equipment = {.energy = 10, .max_energy = 99};
    /* MinimapCheckIsTileExplored: no tank on an unexplored minimap tile
     * (block (3, 4) is tile (0, 0) of a room at map (0, 0)). */
    runtime_hud.ready = true;
    runtime_hud.ppu = &runtime_hud_ppu;
    runtime_hud.progress = &runtime_minimap;
    assert(mzm_tank_touch(&runtime_block_tables.tanks, &runtime_tank_collection, &probes,
                          &tank_equipment, runtime_hud_explored, tanks) == -1 &&
           !runtime_tank_collection.collecting);
    runtime_minimap.visited[0][0] = 1;
    assert(mzm_tank_touch(&runtime_block_tables.tanks, &runtime_tank_collection, &probes,
                          &tank_equipment, runtime_hud_explored, tanks) ==
               MZM_MESSAGE_ENERGY_TANK_ACQUIRED &&
           tank_equipment.max_energy == 199 && runtime_tank_collection.x == 3 &&
           runtime_tank_collection.y == 4);
    runtime_finish_collecting_tank(tanks);
    assert(!runtime_tank_collection.collecting && tanks->clip[4 * 8 + 3] == 0 &&
           runtime_collected_items.count[0] == 1 && runtime_collected_items.items[0][0].room == 7);
    /* MinimapUpdateForCollectedItem, and BgClipFinishCollectingAbility at
     * Samus's block. */
    assert(runtime_minimap.obtained[0][0] == 1);
    runtime_finish_collecting_ability(tanks, &samus);
    assert(runtime_collected_items.count[0] == 2 &&
           runtime_collected_items.items[0][1].type == MZM_ITEM_TYPE_ABILITY &&
           runtime_collected_items.items[0][1].x == 3 && runtime_collected_items.items[0][1].y == 4);
    runtime_collected_items.count[0] = 1;
    runtime_hud.ready = false;
    /* Reloaded, the collected tank is gone again. */
    tanks->clip[4 * 8 + 3] = 50;
    runtime_blocks_load(tanks, "brinstar_007");
    assert(tanks->clip[4 * 8 + 3] == 0);
    tanks->room = 8;
    tanks->clip[4 * 8 + 3] = 50;
    runtime_blocks_load(tanks, "brinstar_008");
    assert(tanks->clip[4 * 8 + 3] == 50);
    free(tanks);
    free(r);
    return 0;
}
