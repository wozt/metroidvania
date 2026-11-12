cmake_minimum_required(VERSION 3.25)
project(metroidvania_fusion VERSION 0.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(FUSION_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

function(fusion_enable_warnings target)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(FUSION_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(fusion_json STATIC src/core/json.c src/core/toml.c)
target_include_directories(fusion_json PUBLIC include)
if(NOT WIN32)
    target_link_libraries(fusion_json PUBLIC m)
endif()
fusion_enable_warnings(fusion_json)

add_library(fusion_editor_backend STATIC
    src/editor_backend/catalog.c
    src/editor_backend/cli.c
    src/editor_backend/commands.c
    src/editor_backend/commands_room.c
    src/editor_backend/project_room.c
    src/editor_backend/story.c
    src/editor_backend/common.c
    src/editor_backend/drafts.c
    src/editor_backend/object_catalog.c
    src/editor_backend/workspace.c
    src/editor_backend/generated.c
    src/editor_backend/mzm_render.c
    src/editor_backend/mzm_workspace.c
    src/editor_backend/aria_render.c
    src/editor_backend/audit.c
    src/editor_backend/world_overview.c
    src/core/native_map.c
)
find_package(ZLIB REQUIRED)
target_link_libraries(fusion_editor_backend PUBLIC fusion_json PRIVATE fusion_assets ZLIB::ZLIB)
fusion_enable_warnings(fusion_editor_backend)

add_library(fusion_native_editor_core STATIC
    src/core/editor_cli_launcher.c
    src/core/native_selection.c
)
target_include_directories(fusion_native_editor_core PUBLIC include)
target_link_libraries(fusion_native_editor_core PUBLIC fusion_editor_backend)
target_compile_definitions(fusion_native_editor_core
    PRIVATE
        FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        FUSION_BINARY_DIR="${CMAKE_BINARY_DIR}")
fusion_enable_warnings(fusion_native_editor_core)

add_library(fusion_assets STATIC
    src/assets/aos/catalog.c
    src/assets/aos/runtime_room.c
    src/assets/aos/export.c
    src/assets/aos/objects.c
    src/assets/aos/sound.c
    src/assets/aos/soma.c
    src/assets/aos/weapons.c
    src/assets/aos/world.c
    src/assets/core/output.c
    src/assets/core/sha1.c
    src/assets/core/sha256.c
    src/assets/core/export.c
    src/assets/core/sprite_library.c
    src/assets/core/rom.c
    src/assets/gba/decompress.c
    src/assets/gba/header.c
    src/assets/mzm/rom_bank.c
    src/assets/mzm/rooms.c
    src/assets/mzm/runtime_room.c
    src/assets/mzm/primary_sprites.c
    src/assets/mzm/export.c
    src/assets/mzm/samus.c
    src/assets/mzm/samus_tables.c
    src/assets/mzm/message_text.c
    src/assets/mzm/sprites.c
    src/assets/mzm/sprite_tables.c
    src/assets/mzm/music_names.c
    src/assets/mzm/sound.c
    src/assets/core/raw_import.c
)
target_include_directories(fusion_assets PUBLIC include)
target_include_directories(fusion_assets PRIVATE src/assets/core)
target_link_libraries(fusion_assets PUBLIC fusion_json)
fusion_enable_warnings(fusion_assets)

add_library(fusion_pipeline STATIC
    src/pipeline/tasks.c
    src/pipeline/graph.c
    src/pipeline/cache.c
    src/pipeline/plan.c
    src/pipeline/actions.c
    src/pipeline/checklist.c
    src/pipeline/inventory.c
    src/pipeline/inventory_writer.c
)
target_include_directories(fusion_pipeline PUBLIC include)
target_include_directories(fusion_pipeline PRIVATE src/pipeline src/assets/core)
target_link_libraries(fusion_pipeline PUBLIC fusion_assets fusion_json)
fusion_enable_warnings(fusion_pipeline)

add_executable(fusion_rebuild src/tools/fusion_rebuild.c)
target_link_libraries(fusion_rebuild PRIVATE fusion_pipeline)
target_compile_definitions(fusion_rebuild PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
fusion_enable_warnings(fusion_rebuild)

find_library(MGBA_LIBRARY NAMES mgba REQUIRED)
find_library(SQLITE3_LIBRARY NAMES sqlite3 REQUIRED)
add_library(aos_mgba_session OBJECT src/tools/aos_mgba_session.c)
target_include_directories(aos_mgba_session PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/tools)
target_link_libraries(aos_mgba_session PRIVATE ${MGBA_LIBRARY} pthread)
add_library(aos_lab_db OBJECT src/tools/aos_lab_db.c)
target_include_directories(aos_lab_db PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/tools)
target_link_libraries(aos_lab_db PRIVATE ${SQLITE3_LIBRARY})
add_library(aos_lab_vision OBJECT src/tools/aos_lab_vision.c)
target_include_directories(aos_lab_vision PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/tools)
target_link_libraries(aos_lab_vision PRIVATE m)
add_executable(aos_mgba_lab src/tools/aos_mgba_lab.c
    $<TARGET_OBJECTS:aos_mgba_session>)
target_include_directories(aos_mgba_lab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/tools)
target_link_libraries(aos_mgba_lab PRIVATE ${MGBA_LIBRARY} pthread)
fusion_enable_warnings(aos_mgba_lab)
add_executable(aos_lab_frame src/tools/aos_lab_frame.c)
target_include_directories(aos_lab_frame PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/tools)
target_link_libraries(aos_lab_frame PRIVATE png)
fusion_enable_warnings(aos_lab_frame)
add_executable(aos_lab_worker src/tools/aos_lab_worker.c
    $<TARGET_OBJECTS:aos_mgba_session> $<TARGET_OBJECTS:aos_lab_db>
    $<TARGET_OBJECTS:aos_lab_vision>)
target_include_directories(aos_lab_worker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/tools)
target_link_libraries(aos_lab_worker PRIVATE ${MGBA_LIBRARY} ${SQLITE3_LIBRARY} pthread m)
fusion_enable_warnings(aos_lab_worker)

add_executable(fusion_checklist src/tools/fusion_checklist.c)
target_link_libraries(fusion_checklist PRIVATE fusion_pipeline)
target_include_directories(fusion_checklist PRIVATE src/pipeline)
target_compile_definitions(fusion_checklist PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
fusion_enable_warnings(fusion_checklist)

add_executable(fusion_inventory src/tools/fusion_inventory.c)
target_link_libraries(fusion_inventory PRIVATE fusion_pipeline)
target_compile_definitions(fusion_inventory PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
fusion_enable_warnings(fusion_inventory)

add_executable(fusion_sound_ids src/tools/sound_ids.c)
target_compile_definitions(fusion_sound_ids PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
fusion_enable_warnings(fusion_sound_ids)

add_executable(fusion_importer src/tools/fusion_importer.c)
target_link_libraries(fusion_importer PRIVATE fusion_assets)
fusion_enable_warnings(fusion_importer)

add_executable(fusion_dashboard src/tools/fusion_dashboard.c)
target_link_libraries(fusion_dashboard PRIVATE fusion_pipeline fusion_editor_backend)
target_compile_definitions(fusion_dashboard PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
fusion_enable_warnings(fusion_dashboard)

add_executable(fusion_check_generated src/tools/check_generated.c)
target_compile_definitions(fusion_check_generated
    PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}" FUSION_BINARY_DIR="${CMAKE_BINARY_DIR}")
fusion_enable_warnings(fusion_check_generated)

add_executable(fusion_check_no_proprietary src/tools/check_no_proprietary.c)
fusion_enable_warnings(fusion_check_no_proprietary)

add_executable(fusion_world_overview src/tools/world_overview.c)
target_link_libraries(fusion_world_overview PRIVATE fusion_editor_backend)
target_compile_definitions(fusion_world_overview PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
fusion_enable_warnings(fusion_world_overview)

add_executable(fusion_editor_cli src/tools/editor_cli.c)
target_link_libraries(fusion_editor_cli PRIVATE fusion_native_editor_core)
fusion_enable_warnings(fusion_editor_cli)

add_executable(fusion_native_map_cli src/tools/native_map_cli.c)
target_link_libraries(fusion_native_map_cli PRIVATE fusion_native_editor_core)
fusion_enable_warnings(fusion_native_map_cli)

find_package(SDL3 CONFIG REQUIRED)
add_executable(fusion_mzm_room_viewer src/tools/mzm_room_viewer.c)
target_link_libraries(fusion_mzm_room_viewer PRIVATE SDL3::SDL3)
fusion_enable_warnings(fusion_mzm_room_viewer)

add_executable(fusion_room_package_viewer src/tools/room_package_viewer.c)
target_link_libraries(fusion_room_package_viewer PRIVATE SDL3::SDL3)
fusion_enable_warnings(fusion_room_package_viewer)

add_executable(fusion_room_runtime src/runtime/room_runtime.c src/runtime/fusion_host.c src/runtime/mzm/core/mzm_room_overlay.c src/runtime/project_room_document.c src/core/native_map.c src/runtime/mzm/game/mzm_samus.c
               src/runtime/mzm/weapons/mzm_projectiles.c src/runtime/gba_input.c src/runtime/debug_menu.c
               src/runtime/mzm/core/mzm_sprites.c src/runtime/mzm/enemies/mzm_space_pirate.c src/runtime/mzm/enemies/mzm_parasite.c src/runtime/mzm/core/mzm_atomic.c src/runtime/mzm/enemies/mzm_polyp.c src/runtime/mzm/game/mzm_tourian.c src/runtime/mzm/enemies/mzm_worker_robot.c src/runtime/mzm/enemies/mzm_metroid.c src/runtime/mzm/enemies/mzm_kraid.c src/runtime/mzm/enemies/mzm_ridley.c src/runtime/mzm/enemies/mzm_mother_brain.c src/runtime/mzm/enemies/mzm_deorem.c src/runtime/mzm/enemies/mzm_acid_worm.c src/runtime/mzm/enemies/mzm_gadora.c src/runtime/mzm/enemies/mzm_imago_larva.c src/runtime/mzm/enemies/mzm_imago_cocoon.c src/runtime/mzm/enemies/mzm_mecha_ridley.c src/runtime/mzm/enemies/mzm_hive.c src/runtime/mzm/enemies/mzm_baristute.c src/runtime/mzm/enemies/mzm_tangle_vine.c src/runtime/mzm/game/mzm_ruins_test.c src/runtime/mzm/cutscenes/mzm_chozodia_scripted.c src/runtime/mzm/cutscenes/mzm_elevator_statue.c src/runtime/mzm/enemies/mzm_imago.c src/runtime/mzm/cutscenes/mzm_escape_ship.c src/runtime/mzm/enemies/mzm_crocomire.c
                   src/runtime/mzm/cutscenes/mzm_chozodia_security.c src/runtime/mzm/cutscenes/mzm_chozo_statue.c src/runtime/mzm/menus/mzm_item_sprites.c src/runtime/mzm/weapons/mzm_gadget_sprites.c src/runtime/mzm/cutscenes/mzm_gunship.c src/runtime/mzm/game/mzm_hatches.c
               src/runtime/mzm/core/mzm_blocks.c
               src/runtime/mzm/core/mzm_effects.c src/runtime/mzm/core/mzm_audio.c src/runtime/gba_psg.c
               src/runtime/mzm/core/mzm_sfx.c src/runtime/mzm/core/mzm_screen_shake.c src/runtime/mzm/game/mzm_background_effect.c src/runtime/mzm/cutscenes/mzm_escape.c src/runtime/mzm/weapons/mzm_power_bomb.c
               src/runtime/mzm/menus/mzm_items.c src/runtime/mzm/menus/mzm_message_banner.c
               src/runtime/gba_ppu.c src/runtime/mzm/core/mzm_rom.c src/runtime/mzm/menus/mzm_hud.c
               src/runtime/mzm/core/mzm_screen.c src/runtime/mzm/title/mzm_title_screen.c src/runtime/mzm/core/mzm_room_gfx.c src/runtime/mzm/cutscenes/mzm_tourian_escape.c src/runtime/mzm/cutscenes/mzm_chozodia_escape.c src/runtime/mzm/title/mzm_credits.c
               src/runtime/mzm/menus/mzm_pause_screen.c src/runtime/mzm/core/mzm_text.c src/runtime/mzm/game/mzm_save.c src/runtime/mzm/menus/mzm_time_attack.c
               src/runtime/mzm/menus/mzm_file_select.c src/runtime/mzm/game/mzm_game_over.c
               src/runtime/mzm/title/mzm_intro.c src/runtime/mzm/cutscenes/mzm_cutscene.c src/runtime/mzm/cutscenes/mzm_in_game_cutscene.c src/runtime/mzm/game/mzm_door_fade.c)
target_include_directories(fusion_room_runtime PRIVATE include)
target_link_libraries(fusion_room_runtime PRIVATE SDL3::SDL3 fusion_json m)
fusion_enable_warnings(fusion_room_runtime)

set(AOS_ENEMY_SOURCES
    src/runtime/aos/core/aos_enemy.c
    src/runtime/aos/math/aos_math.c
    src/runtime/aos/enemies/sky_fish.c
    src/runtime/aos/enemies/ectoplasm.c
    src/runtime/aos/enemies/ghost_dancer.c
    src/runtime/aos/enemies/medusa_head.c
    src/runtime/aos/enemies/ghost.c
    src/runtime/aos/enemies/zombie_officer.c
    src/runtime/aos/enemies/curly.c
    src/runtime/aos/enemies/needles.c
    src/runtime/aos/enemies/gorgon.c
    src/runtime/aos/enemies/dead_warrior.c
    src/runtime/aos/enemies/mimic.c
    src/runtime/aos/enemies/fleaman.c
    src/runtime/aos/enemies/skeleton_knight.c
    src/runtime/aos/enemies/killer_doll.c
    src/runtime/aos/enemies/lightning_doll.c
    src/runtime/aos/enemies/beam_skeleton.c
    src/runtime/aos/enemies/skull_millione.c
    src/runtime/aos/enemies/dead_crusader.c
    src/runtime/aos/enemies/gargoyle.c
    src/runtime/aos/enemies/axe_armor.c
    src/runtime/aos/enemies/rock_armor.c
    src/runtime/aos/enemies/skull_archer.c
    src/runtime/aos/enemies/skeleton.c
    src/runtime/aos/enemies/killer_fish.c
    src/runtime/aos/enemies/winged_skeleton.c
    src/runtime/aos/enemies/harpy.c
    src/runtime/aos/enemies/tiny_devil.c
    src/runtime/aos/enemies/ripper.c
    src/runtime/aos/enemies/merman.c
    src/runtime/aos/enemies/peeping_eye.c
    src/runtime/aos/enemies/buer.c
    src/runtime/aos/enemies/white_dragon.c
    src/runtime/aos/enemies/bone_pillar.c
    src/runtime/aos/enemies/giant_ghost.c
    src/runtime/aos/enemies/minotaur.c
    src/runtime/aos/enemies/worm.c
    src/runtime/aos/enemies/biphron.c
    src/runtime/aos/enemies/bomber_armor.c
    src/runtime/aos/enemies/evil_butcher.c
    src/runtime/aos/enemies/killer_mantle.c
    src/runtime/aos/enemies/dryad.c
    src/runtime/aos/enemies/tsuchinoko.c
    src/runtime/aos/enemies/kicker_skeleton.c
    src/runtime/aos/enemies/waiter_skeleton.c
    src/runtime/aos/enemies/cockatrice.c
    src/runtime/aos/enemies/chronomage.c
    src/runtime/aos/enemies/mandragora.c
    src/runtime/aos/enemies/disc_armor.c
    src/runtime/aos/enemies/alura_une.c
    src/runtime/aos/enemies/ukoback.c
    src/runtime/aos/enemies/student_witch.c
    src/runtime/aos/enemies/witch.c
    src/runtime/aos/enemies/mudman.c
    src/runtime/aos/enemies/were.c
    src/runtime/aos/enemies/werewolf.c
    src/runtime/aos/enemies/slime.c
    src/runtime/aos/enemies/kyoma_demon.c
    src/runtime/aos/enemies/altair.c
    src/runtime/aos/enemies/bael.c
    src/runtime/aos/enemies/nightmare.c
    src/runtime/aos/enemies/une.c
    src/runtime/aos/enemies/arachne.c
    src/runtime/aos/enemies/persephone.c
    src/runtime/aos/enemies/lilith.c
    src/runtime/aos/enemies/succubus.c
    src/runtime/aos/enemies/triton.c
    src/runtime/aos/enemies/devil.c
    src/runtime/aos/enemies/quezlcoatl.c
    src/runtime/aos/enemies/alastor.c
    src/runtime/aos/enemies/maneater.c
    src/runtime/aos/enemies/gladiator.c
    src/runtime/aos/enemies/golem.c
    src/runtime/aos/enemies/stolas.c
    src/runtime/aos/enemies/death.c
    src/runtime/aos/enemies/manticore.c
    src/runtime/aos/enemies/balore.c
    src/runtime/aos/enemies/creaking_skull.c
    src/runtime/aos/enemies/legion.c
    src/runtime/aos/enemies/belmont.c
    src/runtime/aos/enemies/big_golem.c
    src/runtime/aos/enemies/great_armor.c
    src/runtime/aos/enemies/graham.c
    src/runtime/aos/enemies/chaos.c
    src/runtime/aos/enemies/headhunter.c
    src/runtime/aos/enemies/zombie.c
    src/runtime/aos/enemies/nemesis.c
    src/runtime/aos/enemies/crow.c
    src/runtime/aos/enemies/zombie_soldier.c
    src/runtime/aos/enemies/valkyrie.c
    src/runtime/aos/enemies/bat.c)

add_executable(fusion_aria_runtime src/runtime/aria_runtime.c src/runtime/aos/red_souls/aos_soul_red_arrow.c src/runtime/fusion_host.c src/runtime/aos/game/aos_room_overlay.c src/runtime/project_room_document.c src/runtime/aos/core/aos_audio.c src/runtime/aos/core/aos_room_music.c src/runtime/aos/core/aos_mp2k.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c src/runtime/aos/souls/aos_soul_notice.c src/runtime/aos/blue_souls/aos_soul_blue_buer.c src/runtime/aos/blue_souls/aos_soul_blue_imp.c src/runtime/aos/blue_souls/aos_soul_blue_ghost.c src/runtime/aos/blue_souls/aos_soul_blue_death.c src/runtime/aos/blue_souls/aos_soul_blue_sky_fish.c src/runtime/aos/blue_souls/aos_soul_blue_creaking_skull.c src/runtime/aos/blue_souls/aos_soul_blue_alastor.c src/runtime/aos/blue_souls/aos_soul_blue_giant_bat.c src/runtime/aos/blue_souls/aos_soul_blue_final_guard.c src/runtime/aos/red_souls/aos_soul_red_lightning.c src/runtime/aos/red_souls/aos_soul_red_killer_doll.c src/runtime/aos/red_souls/aos_soul_red_nemesis.c src/runtime/aos/red_souls/aos_soul_red_throw.c src/runtime/aos/red_souls/aos_soul_red_merman.c src/runtime/aos/red_souls/aos_soul_red_axe.c src/runtime/aos/red_souls/aos_soul_red_spear.c src/runtime/aos/red_souls/aos_soul_red_shot.c src/runtime/aos/red_souls/aos_soul_red_dagger.c src/runtime/aos/red_souls/aos_soul_red_devil.c src/runtime/aos/red_souls/aos_soul_red_crow.c src/runtime/aos/red_souls/aos_soul_red_flea.c src/runtime/aos/red_souls/aos_soul_red_skull.c src/runtime/aos/red_souls/aos_soul_red_valkyrie.c src/runtime/aos/red_souls/aos_soul_red_needles.c src/runtime/aos/red_souls/aos_soul_red_ghost.c src/runtime/aos/red_souls/aos_soul_red_siren.c src/runtime/aos/red_souls/aos_soul_red_ripper.c src/runtime/aos/red_souls/aos_soul_red_millione.c src/runtime/aos/red_souls/aos_soul_red_fish_head.c src/runtime/aos/red_souls/aos_soul_red_killer_fish.c src/runtime/aos/red_souls/aos_soul_red_gladiator.c src/runtime/aos/red_souls/aos_soul_red_werewolf.c src/runtime/aos/red_souls/aos_soul_red_killer_mantle.c src/runtime/aos/red_souls/aos_soul_red_were_beast.c src/runtime/aos/red_souls/aos_soul_red_flame_demon.c src/runtime/aos/red_souls/aos_soul_red_biphron.c src/runtime/aos/red_souls/aos_soul_red_kyoma_demon.c src/runtime/aos/red_souls/aos_soul_red_student_witch.c src/runtime/aos/red_souls/aos_soul_red_slime.c src/runtime/aos/red_souls/aos_soul_red_ukoback.c src/runtime/aos/red_souls/aos_soul_red_altair.c src/runtime/aos/red_souls/aos_soul_red_une.c src/runtime/aos/red_souls/aos_soul_red_mandragora.c src/runtime/aos/red_souls/aos_soul_red_chronomage.c src/runtime/aos/red_souls/aos_soul_red_disc_armor.c src/runtime/aos/red_souls/aos_soul_red_cockatrice.c src/runtime/aos/red_souls/aos_soul_red_dryad.c src/runtime/aos/red_souls/aos_soul_red_arachne.c src/runtime/aos/red_souls/aos_soul_red_minotaur.c src/runtime/aos/red_souls/aos_soul_red_nightmare.c src/runtime/aos/red_souls/aos_soul_red_man_eater.c src/runtime/aos/menus/aos_portrait.c src/runtime/aos/core/aos_rom_graphics.c src/runtime/aos/menus/aos_pause_graphics.c src/runtime/aos/menus/aos_quick_map.c src/runtime/aos/menus/aos_window_frame.c src/runtime/aos/menus/aos_menu_cells.c
               src/runtime/aos/core/aos_stats.c src/runtime/aos/game/aos_recent_items.c src/runtime/aos/game/aos_pickup.c src/runtime/aos/menus/aos_hud.c src/runtime/aos/souls/aos_soul_menu.c
               src/runtime/aos/intro/aos_levelup.c
               src/runtime/aos/game/aos_bestiary.c
               src/runtime/aos/menus/aos_button_config.c
               src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c src/runtime/aos/game/aos_room.c
               src/runtime/aos/game/aos_door.c src/runtime/aos/game/aos_object01.c src/runtime/aos/game/aos_object02.c src/runtime/aos/game/aos_object03.c src/runtime/aos/game/aos_object04.c src/runtime/aos/game/aos_object05.c src/runtime/aos/game/aos_object06.c src/runtime/aos/game/aos_object07.c src/runtime/aos/game/aos_object0c.c src/runtime/aos/game/aos_object0f.c src/runtime/aos/game/aos_object0d.c src/runtime/aos/game/aos_object0e.c src/runtime/aos/game/aos_object11.c src/runtime/aos/game/aos_object12.c src/runtime/aos/game/aos_object13.c src/runtime/aos/game/aos_object19.c src/runtime/aos/core/aos_wall.c src/runtime/aos/weapons/aos_weapon.c ${AOS_ENEMY_SOURCES} src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/core/aos_effect.c
               src/runtime/gba_input.c src/runtime/aos/core/aos_input.c src/runtime/aos/game/aos_save.c
               src/runtime/aos/game/aos_game_mode.c src/runtime/aos/game/aos_game_lifecycle.c src/runtime/aos/intro/aos_game_over.c src/runtime/aos/menus/aos_main_menu.c
               src/runtime/aos/intro/aos_intro_cutscene.c src/runtime/aos/intro/aos_intro_entities.c
               src/runtime/aos/intro/aos_intro_resources.c src/runtime/aos/intro/aos_intro_font.c
               src/runtime/aos/title/aos_opening_event.c
               src/runtime/aos/title/aos_title_fade.c src/runtime/aos/title/aos_title_screen.c src/runtime/aos/game/aos_obj.c
               src/runtime/aos/game/aos_obj_rom.c
               src/runtime/aos/game/aos_save_room.c
               src/runtime/debug_menu.c)
add_executable(fusion_aria_title_preview src/runtime/aos/title/aos_title_preview.c src/runtime/fusion_host.c src/runtime/gba_input.c
               src/runtime/aos/title/aos_title_fade.c src/runtime/aos/title/aos_title_screen.c src/runtime/aos/math/aos_math.c src/runtime/aos/game/aos_obj.c src/runtime/aos/core/aos_anim.c
               src/runtime/aos/game/aos_game_mode.c src/runtime/aos/game/aos_game_lifecycle.c src/runtime/aos/intro/aos_game_over.c
               src/runtime/aos/menus/aos_main_menu.c src/runtime/aos/intro/aos_intro_cutscene.c
               src/runtime/aos/intro/aos_intro_entities.c src/runtime/aos/intro/aos_intro_resources.c
               src/runtime/aos/intro/aos_intro_font.c src/runtime/aos/intro/aos_intro_graphics.c
               src/runtime/aos/core/aos_rom_graphics.c src/runtime/aos/menus/aos_main_menu_graphics.c
               src/runtime/aos/menus/aos_main_menu_objects.c src/runtime/aos/menus/aos_main_menu_sprites.c
               src/runtime/aos/menus/aos_main_menu_files.c src/runtime/aos/core/aos_stats.c
               src/runtime/aos/title/aos_title_graphics.c src/runtime/aos/game/aos_obj_rom.c src/runtime/aos/title/aos_logo_graphics.c
               src/runtime/aos/core/aos_input.c src/runtime/aos/menus/aos_text_window.c
               src/runtime/aos/menus/aos_menu_bg0.c src/runtime/aos/menus/aos_window_frame.c
               src/runtime/aos/intro/aos_game_over_graphics.c
               src/runtime/aos/game/aos_save.c src/runtime/aos/menus/aos_menu_save_bridge.c
               src/runtime/aos/core/aos_audio.c src/runtime/aos/core/aos_mp2k.c)
target_include_directories(fusion_aria_title_preview PRIVATE src/runtime)
target_link_libraries(fusion_aria_title_preview PRIVATE SDL3::SDL3 fusion_assets m)
fusion_enable_warnings(fusion_aria_title_preview)


target_include_directories(fusion_aria_runtime PRIVATE include)
target_link_libraries(fusion_aria_runtime PRIVATE SDL3::SDL3 fusion_assets fusion_json m)
fusion_enable_warnings(fusion_aria_runtime)

# The launcher runs the three frontends above in its own process and window
# (fusion_host), so it builds their sources with FUSION_EMBEDDED.
get_target_property(fusion_mzm_frontend_sources fusion_room_runtime SOURCES)
get_target_property(fusion_aria_frontend_sources fusion_aria_runtime SOURCES)
get_target_property(fusion_aria_menu_sources fusion_aria_title_preview SOURCES)
set(fusion_rotating_line_sources
    src/runtime/rotating_line/curve.c src/runtime/rotating_line/config.c
    src/runtime/rotating_line/expression.c src/runtime/rotating_line/parametric.c)
set(fusion_metroidvania_sources src/runtime/metroidvania_launcher.c
    src/runtime/metroidvania_split.c ${fusion_rotating_line_sources}
    ${fusion_mzm_frontend_sources}
    ${fusion_aria_frontend_sources} ${fusion_aria_menu_sources})
list(REMOVE_DUPLICATES fusion_metroidvania_sources)
add_executable(fusion_metroidvania ${fusion_metroidvania_sources})
target_compile_definitions(fusion_metroidvania PRIVATE FUSION_EMBEDDED=1)
target_include_directories(fusion_metroidvania PRIVATE src/runtime include)
target_link_libraries(fusion_metroidvania PRIVATE SDL3::SDL3 fusion_assets fusion_json m)
fusion_enable_warnings(fusion_metroidvania)

include(CTest)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(GTK4 QUIET IMPORTED_TARGET gtk4)
endif()

if(GTK4_FOUND)
    add_executable(fusion_map_editor
        editor/main.c
        editor/native_workspace.c
        editor/world_atlas.c
        editor/object_catalog.c
        editor/story_workspace.c
        editor/room_browser.c
    )
    target_include_directories(fusion_map_editor PRIVATE include editor)
    target_link_libraries(fusion_map_editor
        PRIVATE fusion_native_editor_core PkgConfig::GTK4)
    fusion_enable_warnings(fusion_map_editor)

    add_executable(fusion_sfx_browser
        src/tools/sfx_browser_gtk.c
        src/runtime/aos/core/aos_audio.c
        src/runtime/aos/core/aos_mp2k.c
        src/runtime/mzm/core/mzm_audio.c
        src/runtime/gba_psg.c)
    target_include_directories(fusion_sfx_browser PRIVATE src/runtime)
    target_compile_definitions(fusion_sfx_browser PRIVATE
        FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(fusion_sfx_browser
        PRIVATE PkgConfig::GTK4 SDL3::SDL3 m)
    fusion_enable_warnings(fusion_sfx_browser)

    add_executable(aos_lab_viewer src/tools/aos_lab_viewer.c
        $<TARGET_OBJECTS:aos_lab_db>)
    target_include_directories(aos_lab_viewer PRIVATE src/tools)
    target_link_libraries(aos_lab_viewer
        PRIVATE PkgConfig::GTK4 ${SQLITE3_LIBRARY})
    fusion_enable_warnings(aos_lab_viewer)
elseif(NOT PkgConfig_FOUND)
    message(STATUS "pkg-config not found: optional fusion_map_editor disabled")
else()
    message(STATUS "GTK4 not found: optional fusion_map_editor disabled")
endif()

if(BUILD_TESTING)
    add_executable(fusion_assets_rom_tests tests/test_fusion_assets_rom.c)
    target_link_libraries(fusion_assets_rom_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_assets_rom_tests)
    add_test(NAME fusion_assets_rom COMMAND fusion_assets_rom_tests)
    add_executable(fusion_assets_gba_tests tests/test_fusion_assets_gba.c)
    target_link_libraries(fusion_assets_gba_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_assets_gba_tests)
    add_test(NAME fusion_assets_gba COMMAND fusion_assets_gba_tests)
    add_executable(fusion_assets_gba_header_tests tests/test_fusion_assets_gba_header.c)
    target_link_libraries(fusion_assets_gba_header_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_assets_gba_header_tests)
    add_test(NAME fusion_assets_gba_header COMMAND fusion_assets_gba_header_tests)
    add_executable(fusion_assets_aos_tests tests/test_fusion_assets_aos.c)
    target_compile_definitions(fusion_assets_aos_tests PRIVATE
        FUSION_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(fusion_assets_aos_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_assets_aos_tests)
    add_test(NAME fusion_assets_aos COMMAND fusion_assets_aos_tests)
    add_executable(fusion_sprite_library_tests tests/test_fusion_sprite_library.c)
    target_include_directories(fusion_sprite_library_tests PRIVATE src/assets/core)
    target_link_libraries(fusion_sprite_library_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_sprite_library_tests)
    add_test(NAME fusion_sprite_library COMMAND fusion_sprite_library_tests)
    add_executable(fusion_mzm_samus_assets_tests tests/test_fusion_mzm_samus_assets.c)
    target_include_directories(fusion_mzm_samus_assets_tests PRIVATE src/assets/core src/assets/mzm)
    target_compile_definitions(fusion_mzm_samus_assets_tests PRIVATE
        FUSION_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(fusion_mzm_samus_assets_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_mzm_samus_assets_tests)
    add_test(NAME fusion_mzm_samus_assets COMMAND fusion_mzm_samus_assets_tests)
    add_executable(fusion_mzm_sprites_assets_tests tests/test_fusion_mzm_sprites_assets.c)
    target_include_directories(fusion_mzm_sprites_assets_tests PRIVATE
        src/assets/core src/assets/mzm)
    target_compile_definitions(fusion_mzm_sprites_assets_tests PRIVATE
        FUSION_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(fusion_mzm_sprites_assets_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_mzm_sprites_assets_tests)
    add_test(NAME fusion_mzm_sprites_assets COMMAND fusion_mzm_sprites_assets_tests)
    add_executable(fusion_raw_import_tests tests/test_fusion_raw_import.c)
    target_include_directories(fusion_raw_import_tests PRIVATE src/assets/core)
    target_link_libraries(fusion_raw_import_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_raw_import_tests)
    add_test(NAME fusion_raw_import COMMAND fusion_raw_import_tests)
    add_executable(fusion_assets_mzm_tests tests/test_fusion_assets_mzm.c)
    target_link_libraries(fusion_assets_mzm_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_assets_mzm_tests)
    add_test(NAME fusion_assets_mzm COMMAND fusion_assets_mzm_tests)
    add_test(NAME mzm_sound_ids_fresh COMMAND fusion_sound_ids --check)
    add_executable(fusion_mzm_sound_tests tests/test_fusion_mzm_sound.c)
    target_link_libraries(fusion_mzm_sound_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_mzm_sound_tests)
    add_test(NAME fusion_mzm_sound COMMAND fusion_mzm_sound_tests)
    add_test(NAME fusion_importer_list COMMAND fusion_importer --list)
    add_test(NAME fusion_importer_dry_run COMMAND fusion_importer --dry-run --force)
    add_executable(fusion_rebuild_tests tests/test_fusion_rebuild.c)
    target_include_directories(fusion_rebuild_tests PRIVATE src/pipeline)
    target_link_libraries(fusion_rebuild_tests PRIVATE fusion_pipeline)
    fusion_enable_warnings(fusion_rebuild_tests)
    add_test(NAME fusion_rebuild_graph COMMAND fusion_rebuild_tests)
    add_test(NAME fusion_rebuild_list COMMAND fusion_rebuild --list)
    add_test(NAME fusion_checklist_current COMMAND fusion_checklist --check)

    add_executable(fusion_inventory_writer_tests tests/test_inventory_writer.c)
    target_link_libraries(fusion_inventory_writer_tests PRIVATE fusion_pipeline)
    target_compile_definitions(fusion_inventory_writer_tests
        PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}" FUSION_BINARY_DIR="${CMAKE_BINARY_DIR}")
    fusion_enable_warnings(fusion_inventory_writer_tests)
    add_test(NAME native_inventory_writer COMMAND fusion_inventory_writer_tests)
    add_test(NAME fusion_inventory_routines_current
             COMMAND fusion_inventory --audit-routines)

    add_executable(fusion_inventory_tests tests/test_fusion_inventory.c)
    target_link_libraries(fusion_inventory_tests PRIVATE fusion_pipeline)
    fusion_enable_warnings(fusion_inventory_tests)
    add_test(NAME fusion_inventory_scanner COMMAND fusion_inventory_tests)

    add_test(NAME editor_cli_headless
        COMMAND ${CMAKE_COMMAND} -E env --unset=DISPLAY --unset=WAYLAND_DISPLAY
                $<TARGET_FILE:fusion_editor_cli>
                --command=project-info --format=json)
    set_tests_properties(editor_cli_headless PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(fusion_editor_backend_tests tests/test_editor_backend.c)
    target_include_directories(fusion_editor_backend_tests PRIVATE src/editor_backend)
    target_compile_definitions(fusion_editor_backend_tests
        PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(fusion_editor_backend_tests PRIVATE fusion_editor_backend)
    fusion_enable_warnings(fusion_editor_backend_tests)
    add_test(NAME editor_backend_native_contract COMMAND fusion_editor_backend_tests)

    add_executable(fusion_editor_render_tests tests/test_editor_render.c)
    target_include_directories(fusion_editor_render_tests PRIVATE src/editor_backend)
    target_compile_definitions(fusion_editor_render_tests
        PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(fusion_editor_render_tests PRIVATE fusion_editor_backend fusion_assets
                          ZLIB::ZLIB)
    fusion_enable_warnings(fusion_editor_render_tests)
    add_test(NAME editor_render_native_contract COMMAND fusion_editor_render_tests)

    add_executable(fusion_source_contract_tests tests/test_source_contracts.c)
    target_compile_definitions(fusion_source_contract_tests
        PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(fusion_source_contract_tests PRIVATE fusion_json)
    fusion_enable_warnings(fusion_source_contract_tests)
    add_test(NAME source_contracts COMMAND fusion_source_contract_tests)

    add_executable(fusion_world_inventory_tests tests/test_world_inventory.c)
    target_compile_definitions(fusion_world_inventory_tests
        PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(fusion_world_inventory_tests PRIVATE fusion_json)
    fusion_enable_warnings(fusion_world_inventory_tests)
    add_test(NAME world_inventory_data COMMAND fusion_world_inventory_tests)

    add_executable(fusion_tool_contract_tests tests/test_tool_contracts.c)
    target_compile_definitions(fusion_tool_contract_tests
        PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}" FUSION_BINARY_DIR="${CMAKE_BINARY_DIR}")
    fusion_enable_warnings(fusion_tool_contract_tests)
    add_test(NAME tool_contracts COMMAND fusion_tool_contract_tests)

    add_executable(fusion_native_golden_tests tests/test_native_golden.c)
    target_compile_definitions(fusion_native_golden_tests
        PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}" FUSION_BINARY_DIR="${CMAKE_BINARY_DIR}")
    target_link_libraries(fusion_native_golden_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_native_golden_tests)
    # The wrapper caches only *successful* real-ROM verifications whose
    # importer/test binaries, ROMs, fixtures and external input databases are
    # byte-identical to the previous pass; other tests still execute normally.
    add_test(NAME native_golden_aos
             COMMAND sh "${CMAKE_SOURCE_DIR}/scripts/native_golden_cached.sh" aos
                     "$<TARGET_FILE:fusion_native_golden_tests>")
    add_test(NAME native_golden_mzm
             COMMAND sh "${CMAKE_SOURCE_DIR}/scripts/native_golden_cached.sh" mzm
                     "$<TARGET_FILE:fusion_native_golden_tests>")
    set_tests_properties(native_golden_aos native_golden_mzm PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME native_golden_cache_contract
             COMMAND sh "${CMAKE_SOURCE_DIR}/tests/test_native_golden_cache.sh")

    add_executable(fusion_editor_cli_tests tests/test_editor_cli.c)
    target_compile_definitions(fusion_editor_cli_tests
        PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}" FUSION_BINARY_DIR="${CMAKE_BINARY_DIR}")
    target_link_libraries(fusion_editor_cli_tests PRIVATE fusion_json)
    fusion_enable_warnings(fusion_editor_cli_tests)
    add_test(NAME editor_cli_contracts COMMAND fusion_editor_cli_tests)

    add_executable(fusion_dashboard_tests tests/test_dashboard.c)
    target_link_libraries(fusion_dashboard_tests PRIVATE fusion_pipeline fusion_editor_backend)
    target_compile_definitions(fusion_dashboard_tests PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    fusion_enable_warnings(fusion_dashboard_tests)
    add_test(NAME dashboard_generator COMMAND fusion_dashboard_tests)
    set_tests_properties(editor_cli_contracts PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(fusion_native_map_tests tests/test_native_map.c)
    target_link_libraries(fusion_native_map_tests PRIVATE fusion_native_editor_core)
    fusion_enable_warnings(fusion_native_map_tests)
    add_test(NAME native_map_contract COMMAND fusion_native_map_tests)

    add_test(NAME native_map_cli_contract
        COMMAND ${CMAKE_COMMAND}
                -DTOOL=$<TARGET_FILE:fusion_native_map_cli>
                -DWORK=${CMAKE_BINARY_DIR}/native-map-cli-test
                -P ${CMAKE_SOURCE_DIR}/tests/native_map_cli_contract.cmake)

    add_executable(fusion_native_selection_tests tests/test_native_selection.c)
    target_link_libraries(fusion_native_selection_tests PRIVATE fusion_native_editor_core)
    fusion_enable_warnings(fusion_native_selection_tests)
    add_test(NAME native_selection_contract COMMAND fusion_native_selection_tests)

    if(GTK4_FOUND)
        add_test(NAME sfx_browser_native_banks
            COMMAND ${CMAKE_COMMAND} -E env SDL_AUDIODRIVER=dummy
                    $<TARGET_FILE:fusion_sfx_browser> --check)
        set_tests_properties(sfx_browser_native_banks PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

        add_test(NAME map_editor_headless
            COMMAND ${CMAKE_COMMAND} -E env --unset=DISPLAY --unset=WAYLAND_DISPLAY
                    $<TARGET_FILE:fusion_map_editor> --headless
                    --command=project-info --format=json)
        set_tests_properties(map_editor_headless PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

        add_executable(fusion_native_workspace_gtk_tests
            tests/test_native_workspace_gtk.c
            editor/native_workspace.c
            editor/world_atlas.c
        )
        target_compile_definitions(fusion_native_workspace_gtk_tests
            PRIVATE FUSION_NATIVE_WORKSPACE_TESTING)
        target_include_directories(fusion_native_workspace_gtk_tests
            PRIVATE include editor)
        target_link_libraries(fusion_native_workspace_gtk_tests
            PRIVATE fusion_native_editor_core PkgConfig::GTK4)
        fusion_enable_warnings(fusion_native_workspace_gtk_tests)

        add_executable(fusion_room_browser_gtk_tests
            tests/test_room_browser_gtk.c
            editor/room_browser.c
            editor/native_workspace.c
            editor/world_atlas.c
        )
        target_include_directories(fusion_room_browser_gtk_tests PRIVATE include editor)
        target_compile_definitions(fusion_room_browser_gtk_tests
            PRIVATE FUSION_ROOM_BROWSER_TESTING)
        target_link_libraries(fusion_room_browser_gtk_tests
            PRIVATE fusion_native_editor_core PkgConfig::GTK4)
        fusion_enable_warnings(fusion_room_browser_gtk_tests)

        find_program(XVFB_RUN_EXECUTABLE xvfb-run)
        if(XVFB_RUN_EXECUTABLE)
            add_test(NAME native_workspace_gtk_lifecycle
                COMMAND ${XVFB_RUN_EXECUTABLE} -a
                        $<TARGET_FILE:fusion_native_workspace_gtk_tests>)
            add_test(NAME room_browser_gtk_contract
                COMMAND ${XVFB_RUN_EXECUTABLE} -a
                        $<TARGET_FILE:fusion_room_browser_gtk_tests>)
            set_tests_properties(room_browser_gtk_contract PROPERTIES
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                ENVIRONMENT
                    "XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/test-config;GTK_A11Y=test")
            set_tests_properties(native_workspace_gtk_lifecycle PROPERTIES
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                ENVIRONMENT
                    "XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/test-config;GTK_A11Y=test;ASAN_OPTIONS=detect_leaks=0:abort_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
        endif()
    endif()

    # ROM-free native collision shape regression.
    add_executable(fusion_runtime_steep_slopes_tests tests/test_runtime_steep_slopes.c
                   src/runtime/mzm/game/mzm_samus.c src/runtime/mzm/weapons/mzm_projectiles.c src/runtime/mzm/game/mzm_hatches.c
                   src/runtime/mzm/core/mzm_blocks.c src/runtime/mzm/core/mzm_sfx.c src/runtime/mzm/core/mzm_screen_shake.c src/runtime/mzm/weapons/mzm_power_bomb.c
                   src/runtime/mzm/menus/mzm_items.c src/runtime/mzm/menus/mzm_message_banner.c
                   src/runtime/gba_ppu.c src/runtime/mzm/core/mzm_rom.c src/runtime/mzm/menus/mzm_hud.c
                   src/runtime/mzm/core/mzm_room_gfx.c src/runtime/mzm/game/mzm_save.c src/runtime/mzm/menus/mzm_time_attack.c
                   src/runtime/mzm/core/mzm_screen.c)
    target_include_directories(fusion_runtime_steep_slopes_tests PRIVATE src/runtime)
    target_compile_definitions(fusion_runtime_steep_slopes_tests PRIVATE
                               FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
    target_link_libraries(fusion_runtime_steep_slopes_tests PRIVATE SDL3::SDL3)
    fusion_enable_warnings(fusion_runtime_steep_slopes_tests)
    add_test(NAME runtime_steep_slopes COMMAND fusion_runtime_steep_slopes_tests)

    add_executable(fusion_mzm_blocks_tests tests/test_mzm_blocks.c src/runtime/mzm/core/mzm_blocks.c
                   src/runtime/mzm/core/mzm_sfx.c src/runtime/mzm/core/mzm_screen_shake.c)
    target_include_directories(fusion_mzm_blocks_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_blocks_tests)
    add_test(NAME mzm_blocks COMMAND fusion_mzm_blocks_tests)

    add_executable(fusion_mzm_power_bomb_tests tests/test_mzm_power_bomb.c
                   src/runtime/mzm/weapons/mzm_power_bomb.c src/runtime/mzm/core/mzm_sfx.c src/runtime/mzm/core/mzm_screen_shake.c)
    target_include_directories(fusion_mzm_power_bomb_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_power_bomb_tests)
    add_test(NAME mzm_power_bomb COMMAND fusion_mzm_power_bomb_tests)

    add_executable(fusion_mzm_background_effect_tests tests/test_mzm_background_effect.c
                   src/runtime/mzm/game/mzm_background_effect.c)
    target_include_directories(fusion_mzm_background_effect_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_background_effect_tests)
    add_test(NAME mzm_background_effect COMMAND fusion_mzm_background_effect_tests)

    add_executable(fusion_mzm_screens_tests tests/test_mzm_screens.c src/runtime/gba_ppu.c
                   src/runtime/mzm/core/mzm_rom.c src/runtime/mzm/core/mzm_screen.c src/runtime/mzm/title/mzm_title_screen.c
                   src/runtime/mzm/core/mzm_sfx.c src/runtime/mzm/core/mzm_screen_shake.c src/runtime/mzm/menus/mzm_hud.c src/runtime/mzm/menus/mzm_pause_screen.c
                   src/runtime/mzm/core/mzm_text.c src/runtime/mzm/menus/mzm_file_select.c src/runtime/mzm/game/mzm_game_over.c
                   src/runtime/mzm/title/mzm_intro.c src/runtime/mzm/cutscenes/mzm_cutscene.c src/runtime/mzm/cutscenes/mzm_in_game_cutscene.c src/runtime/mzm/game/mzm_door_fade.c
                   src/runtime/mzm/core/mzm_room_gfx.c src/runtime/mzm/cutscenes/mzm_tourian_escape.c src/runtime/mzm/cutscenes/mzm_chozodia_escape.c src/runtime/mzm/title/mzm_credits.c)
    target_include_directories(fusion_mzm_screens_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_screens_tests)
    add_test(NAME mzm_screens COMMAND fusion_mzm_screens_tests
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(fusion_mzm_save_tests tests/test_mzm_save.c src/runtime/mzm/game/mzm_save.c src/runtime/mzm/menus/mzm_time_attack.c)
    target_include_directories(fusion_mzm_save_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_save_tests)
    add_test(NAME mzm_save COMMAND fusion_mzm_save_tests)

    add_executable(fusion_mzm_room_gfx_tests tests/test_mzm_room_gfx.c src/runtime/gba_ppu.c
                   src/runtime/mzm/core/mzm_rom.c src/runtime/mzm/core/mzm_room_gfx.c src/runtime/mzm/core/mzm_screen.c)
    target_include_directories(fusion_mzm_room_gfx_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_room_gfx_tests)
    add_test(NAME mzm_room_gfx COMMAND fusion_mzm_room_gfx_tests
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(fusion_mzm_hud_tests tests/test_mzm_hud.c src/runtime/gba_ppu.c
                   src/runtime/mzm/core/mzm_rom.c src/runtime/mzm/menus/mzm_hud.c)
    target_include_directories(fusion_mzm_hud_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_hud_tests)
    add_test(NAME mzm_hud COMMAND fusion_mzm_hud_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(fusion_mzm_audio_tests tests/test_mzm_audio.c src/runtime/mzm/core/mzm_audio.c
                   src/runtime/gba_psg.c)
    target_include_directories(fusion_mzm_audio_tests PRIVATE src/runtime)
    target_link_libraries(fusion_mzm_audio_tests PRIVATE m)
    fusion_enable_warnings(fusion_mzm_audio_tests)
    add_executable(fusion_mzm_time_attack_tests tests/test_mzm_time_attack.c src/runtime/mzm/menus/mzm_time_attack.c)
    target_include_directories(fusion_mzm_time_attack_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_time_attack_tests)
    add_test(NAME mzm_time_attack COMMAND fusion_mzm_time_attack_tests)

    add_test(NAME mzm_audio COMMAND fusion_mzm_audio_tests
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(fusion_mzm_hatches_tests tests/test_mzm_hatches.c src/runtime/mzm/game/mzm_hatches.c
                   src/runtime/mzm/core/mzm_sfx.c src/runtime/mzm/core/mzm_screen_shake.c)
    target_include_directories(fusion_mzm_hatches_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_hatches_tests)
    add_test(NAME mzm_hatches COMMAND fusion_mzm_hatches_tests)

    add_executable(fusion_mzm_items_tests tests/test_mzm_items.c src/runtime/mzm/menus/mzm_items.c
                   src/runtime/mzm/menus/mzm_message_banner.c src/runtime/mzm/core/mzm_sfx.c src/runtime/mzm/core/mzm_screen_shake.c)
    target_include_directories(fusion_mzm_items_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_items_tests)
    add_test(NAME mzm_items COMMAND fusion_mzm_items_tests)

    add_executable(fusion_mzm_sprites_tests tests/test_mzm_sprites.c src/runtime/mzm/core/mzm_sprites.c
                   src/runtime/mzm/enemies/mzm_space_pirate.c src/runtime/mzm/enemies/mzm_parasite.c src/runtime/mzm/core/mzm_atomic.c src/runtime/mzm/enemies/mzm_polyp.c src/runtime/mzm/game/mzm_tourian.c src/runtime/mzm/enemies/mzm_worker_robot.c src/runtime/mzm/enemies/mzm_metroid.c src/runtime/mzm/enemies/mzm_kraid.c src/runtime/mzm/enemies/mzm_ridley.c src/runtime/mzm/enemies/mzm_mother_brain.c src/runtime/mzm/enemies/mzm_deorem.c src/runtime/mzm/enemies/mzm_acid_worm.c src/runtime/mzm/enemies/mzm_gadora.c src/runtime/mzm/enemies/mzm_imago_larva.c src/runtime/mzm/enemies/mzm_imago_cocoon.c src/runtime/mzm/enemies/mzm_mecha_ridley.c src/runtime/mzm/enemies/mzm_hive.c src/runtime/mzm/enemies/mzm_baristute.c src/runtime/mzm/enemies/mzm_tangle_vine.c src/runtime/mzm/game/mzm_ruins_test.c src/runtime/mzm/cutscenes/mzm_chozodia_scripted.c src/runtime/mzm/cutscenes/mzm_elevator_statue.c src/runtime/mzm/enemies/mzm_imago.c src/runtime/mzm/cutscenes/mzm_escape_ship.c src/runtime/mzm/enemies/mzm_crocomire.c
                   src/runtime/mzm/cutscenes/mzm_chozodia_security.c src/runtime/mzm/cutscenes/mzm_chozo_statue.c src/runtime/mzm/menus/mzm_item_sprites.c src/runtime/mzm/weapons/mzm_gadget_sprites.c src/runtime/mzm/cutscenes/mzm_gunship.c
                   src/runtime/mzm/core/mzm_effects.c src/runtime/mzm/core/mzm_sfx.c src/runtime/mzm/core/mzm_screen_shake.c
                   src/runtime/mzm/game/mzm_background_effect.c
                   src/runtime/mzm/cutscenes/mzm_escape.c
                   src/runtime/mzm/weapons/mzm_projectiles.c src/runtime/mzm/game/mzm_samus.c
                   src/runtime/mzm/weapons/mzm_power_bomb.c)
    target_include_directories(fusion_mzm_sprites_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_sprites_tests)
    add_test(NAME mzm_sprites COMMAND fusion_mzm_sprites_tests)

    # Device-free shared gamepad mapping (SDL3 headers and string tables only).
    add_executable(fusion_aos_menu_preview_tests tests/test_aos_menu_preview.c src/runtime/aos/menus/aos_menu_preview.c)
    target_include_directories(fusion_aos_menu_preview_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_menu_preview_tests)
    add_test(NAME aos_menu_preview COMMAND fusion_aos_menu_preview_tests)

    add_executable(fusion_aos_save_tests tests/test_aos_save.c src/runtime/aos/game/aos_save.c)
    target_include_directories(fusion_aos_save_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_save_tests)
    add_test(NAME aos_save COMMAND fusion_aos_save_tests
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_executable(fusion_aos_menu_save_bridge_tests
                   tests/test_aos_menu_save_bridge.c
                   src/runtime/aos/menus/aos_menu_save_bridge.c src/runtime/aos/game/aos_save.c)
    target_include_directories(fusion_aos_menu_save_bridge_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_menu_save_bridge_tests)
    add_test(NAME aos_menu_save_bridge COMMAND fusion_aos_menu_save_bridge_tests)

    add_executable(fusion_aos_save_room_tests tests/test_aos_save_room.c
                   src/runtime/aos/game/aos_save_room.c)
    target_include_directories(fusion_aos_save_room_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_save_room_tests)
    add_test(NAME aos_save_room COMMAND fusion_aos_save_room_tests)

    add_executable(fusion_aos_pickup_tests tests/test_aos_pickup.c src/runtime/aos/game/aos_recent_items.c src/runtime/aos/game/aos_pickup.c
                   src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c ${AOS_ENEMY_SOURCES} src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c
                   src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c src/runtime/aos/core/aos_collision.c
                   src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c)
    target_include_directories(fusion_aos_pickup_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_pickup_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_pickup_tests)
    add_test(NAME aos_pickup COMMAND fusion_aos_pickup_tests)

    add_executable(fusion_aos_game_over_tests tests/test_aos_game_over.c
                   src/runtime/aos/intro/aos_game_over.c)
    target_include_directories(fusion_aos_game_over_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_game_over_tests)
    add_test(NAME aos_game_over COMMAND fusion_aos_game_over_tests)

    add_executable(fusion_aos_game_over_graphics_tests tests/test_aos_game_over_graphics.c
                   src/runtime/aos/intro/aos_game_over_graphics.c src/runtime/aos/core/aos_rom_graphics.c)
    target_include_directories(fusion_aos_game_over_graphics_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_game_over_graphics_tests PRIVATE fusion_assets m)
    fusion_enable_warnings(fusion_aos_game_over_graphics_tests)
    add_test(NAME aos_game_over_graphics COMMAND fusion_aos_game_over_graphics_tests)

    add_executable(fusion_aos_hud_tests tests/test_aos_hud.c src/runtime/aos/menus/aos_hud.c)
    target_include_directories(fusion_aos_hud_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_hud_tests)
    add_test(NAME aos_hud COMMAND fusion_aos_hud_tests)

    add_executable(fusion_aos_levelup_tests tests/test_aos_levelup.c src/runtime/aos/intro/aos_levelup.c)
    target_include_directories(fusion_aos_levelup_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_levelup_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_levelup_tests)
    add_test(NAME aos_levelup COMMAND fusion_aos_levelup_tests)

    add_executable(fusion_aos_soul_tests tests/test_aos_soul.c src/runtime/aos/souls/aos_soul.c src/runtime/aos/core/aos_stats.c
                   src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_tests)
    add_test(NAME aos_soul COMMAND fusion_aos_soul_tests)

    add_executable(fusion_aos_soul_menu_tests tests/test_aos_soul_menu.c
                   src/runtime/aos/souls/aos_soul_menu.c src/runtime/aos/game/aos_recent_items.c src/runtime/aos/core/aos_stats.c
                   src/runtime/aos/menus/aos_button_config.c src/runtime/aos/game/aos_bestiary.c)
    target_include_directories(fusion_aos_soul_menu_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_menu_tests)
    add_test(NAME aos_soul_menu COMMAND fusion_aos_soul_menu_tests)

    add_executable(fusion_aos_soul_red_throw_tests tests/test_aos_soul_red_throw.c
                   src/runtime/aos/red_souls/aos_soul_red_throw.c ${AOS_ENEMY_SOURCES} src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c
                   src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c
                   src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_throw_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_throw_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_throw_tests)
    add_test(NAME aos_soul_red_throw COMMAND fusion_aos_soul_red_throw_tests)

    add_executable(fusion_aos_soul_red_merman_tests tests/test_aos_soul_red_merman.c
                   src/runtime/aos/red_souls/aos_soul_red_merman.c)
    target_include_directories(fusion_aos_soul_red_merman_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_merman_tests)
    add_test(NAME aos_soul_red_merman COMMAND fusion_aos_soul_red_merman_tests)

    add_executable(fusion_aos_audio_tests tests/test_aos_audio.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_audio_tests PRIVATE src/runtime)
    target_compile_definitions(fusion_aos_audio_tests PRIVATE
        FUSION_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    fusion_enable_warnings(fusion_aos_audio_tests)
    add_test(NAME aos_audio COMMAND fusion_aos_audio_tests)

    add_executable(fusion_aos_mp2k_tests tests/test_aos_mp2k.c src/runtime/aos/core/aos_mp2k.c)
    target_include_directories(fusion_aos_mp2k_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_mp2k_tests)
    add_test(NAME aos_mp2k COMMAND fusion_aos_mp2k_tests
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(fusion_aos_soul_red_axe_tests tests/test_aos_soul_red_axe.c
                   src/runtime/aos/red_souls/aos_soul_red_axe.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_axe_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_axe_tests)
    add_test(NAME aos_soul_red_axe COMMAND fusion_aos_soul_red_axe_tests)

    add_executable(fusion_aos_soul_red_spear_tests tests/test_aos_soul_red_spear.c
                   src/runtime/aos/red_souls/aos_soul_red_spear.c src/runtime/aos/core/aos_audio.c src/runtime/aos/math/aos_math.c src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c
                   src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c
                   src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_stats.c)
    target_include_directories(fusion_aos_soul_red_spear_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_spear_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_spear_tests)
    add_test(NAME aos_soul_red_spear COMMAND fusion_aos_soul_red_spear_tests)

    add_executable(fusion_aos_soul_red_shot_tests tests/test_aos_soul_red_shot.c src/runtime/aos/red_souls/aos_soul_red_shot.c)
    target_include_directories(fusion_aos_soul_red_shot_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_shot_tests)
    add_test(NAME aos_soul_red_shot COMMAND fusion_aos_soul_red_shot_tests)

    add_executable(fusion_aos_soul_red_arrow_tests tests/test_aos_soul_red_arrow.c src/runtime/aos/red_souls/aos_soul_red_arrow.c)
    target_include_directories(fusion_aos_soul_red_arrow_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_arrow_tests)
    add_test(NAME aos_soul_red_arrow COMMAND fusion_aos_soul_red_arrow_tests)

    add_executable(fusion_aos_soul_red_dagger_tests tests/test_aos_soul_red_dagger.c
                   src/runtime/aos/red_souls/aos_soul_red_dagger.c src/runtime/aos/core/aos_collision.c)
    target_include_directories(fusion_aos_soul_red_dagger_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_dagger_tests)
    add_test(NAME aos_soul_red_dagger COMMAND fusion_aos_soul_red_dagger_tests)

    add_executable(fusion_aos_room_music_tests tests/test_aos_room_music.c
                   src/runtime/aos/core/aos_room_music.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_room_music_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_room_music_tests)
    add_test(NAME aos_room_music COMMAND fusion_aos_room_music_tests)

    add_executable(fusion_aos_soul_red_devil_tests tests/test_aos_soul_red_devil.c
                   src/runtime/aos/red_souls/aos_soul_red_devil.c src/runtime/aos/math/aos_math.c src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c
                   src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c
                   src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_devil_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_devil_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_devil_tests)
    add_test(NAME aos_soul_red_devil COMMAND fusion_aos_soul_red_devil_tests)

    add_executable(fusion_aos_soul_red_crow_tests tests/test_aos_soul_red_crow.c
                   src/runtime/aos/red_souls/aos_soul_red_crow.c src/runtime/aos/math/aos_math.c src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c
                   src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c
                   src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_crow_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_crow_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_crow_tests)
    add_test(NAME aos_soul_red_crow COMMAND fusion_aos_soul_red_crow_tests)

    add_executable(fusion_aos_soul_red_flea_tests tests/test_aos_soul_red_flea.c
                   src/runtime/aos/red_souls/aos_soul_red_flea.c ${AOS_ENEMY_SOURCES} src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c
                   src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c
                   src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_flea_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_flea_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_flea_tests)
    add_test(NAME aos_soul_red_flea COMMAND fusion_aos_soul_red_flea_tests)

    add_executable(fusion_aos_soul_red_skull_tests tests/test_aos_soul_red_skull.c
                   src/runtime/aos/red_souls/aos_soul_red_skull.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_skull_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_skull_tests)
    add_test(NAME aos_soul_red_skull COMMAND fusion_aos_soul_red_skull_tests)

    add_executable(fusion_aos_soul_red_valkyrie_tests tests/test_aos_soul_red_valkyrie.c
                   src/runtime/aos/red_souls/aos_soul_red_valkyrie.c)
    target_include_directories(fusion_aos_soul_red_valkyrie_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_valkyrie_tests)
    add_test(NAME aos_soul_red_valkyrie COMMAND fusion_aos_soul_red_valkyrie_tests)

    add_executable(fusion_aos_soul_red_needles_tests tests/test_aos_soul_red_needles.c
                   src/runtime/aos/red_souls/aos_soul_red_needles.c src/runtime/aos/math/aos_math.c src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c
                   src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c
                   src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_needles_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_needles_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_needles_tests)
    add_test(NAME aos_soul_red_needles COMMAND fusion_aos_soul_red_needles_tests)

    add_executable(fusion_aos_soul_blue_buer_tests tests/test_aos_soul_blue_buer.c
                   src/runtime/aos/blue_souls/aos_soul_blue_buer.c src/runtime/aos/math/aos_math.c src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c
                   src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c
                   src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_blue_buer_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_blue_buer_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_blue_buer_tests)
    add_test(NAME aos_soul_blue_buer COMMAND fusion_aos_soul_blue_buer_tests)

    add_executable(fusion_aos_soul_blue_ghost_tests tests/test_aos_soul_blue_ghost.c
                   src/runtime/aos/blue_souls/aos_soul_blue_ghost.c src/runtime/aos/math/aos_math.c src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c
                   src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c
                   src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_blue_ghost_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_blue_ghost_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_blue_ghost_tests)
    add_test(NAME aos_soul_blue_ghost COMMAND fusion_aos_soul_blue_ghost_tests)
    add_executable(fusion_aos_soul_blue_sky_fish_tests tests/test_aos_soul_blue_sky_fish.c
                   src/runtime/aos/blue_souls/aos_soul_blue_sky_fish.c src/runtime/aos/math/aos_math.c src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c
                   src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c
                   src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_blue_sky_fish_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_blue_sky_fish_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_blue_sky_fish_tests)
    add_test(NAME aos_soul_blue_sky_fish COMMAND fusion_aos_soul_blue_sky_fish_tests)
    add_executable(fusion_aos_soul_blue_creaking_skull_tests tests/test_aos_soul_blue_creaking_skull.c
                   src/runtime/aos/blue_souls/aos_soul_blue_creaking_skull.c src/runtime/aos/math/aos_math.c src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c
                   src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c
                   src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_blue_creaking_skull_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_blue_creaking_skull_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_blue_creaking_skull_tests)
    add_test(NAME aos_soul_blue_creaking_skull COMMAND fusion_aos_soul_blue_creaking_skull_tests)
    add_executable(fusion_aos_soul_blue_alastor_tests tests/test_aos_soul_blue_alastor.c
                   src/runtime/aos/blue_souls/aos_soul_blue_alastor.c src/runtime/aos/math/aos_math.c src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c
                   src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c
                   src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_blue_alastor_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_blue_alastor_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_blue_alastor_tests)
    add_test(NAME aos_soul_blue_alastor COMMAND fusion_aos_soul_blue_alastor_tests)
    add_executable(fusion_aos_soul_blue_giant_bat_tests tests/test_aos_soul_blue_giant_bat.c
                   src/runtime/aos/blue_souls/aos_soul_blue_giant_bat.c src/runtime/aos/math/aos_math.c src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c
                   src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c
                   src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_blue_giant_bat_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_blue_giant_bat_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_blue_giant_bat_tests)
    add_test(NAME aos_soul_blue_giant_bat COMMAND fusion_aos_soul_blue_giant_bat_tests)
    add_executable(fusion_aos_soul_blue_final_guard_tests tests/test_aos_soul_blue_final_guard.c
                   src/runtime/aos/blue_souls/aos_soul_blue_final_guard.c src/runtime/aos/math/aos_math.c src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c
                   src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c
                   src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_blue_final_guard_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_blue_final_guard_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_blue_final_guard_tests)
    add_test(NAME aos_soul_blue_final_guard COMMAND fusion_aos_soul_blue_final_guard_tests)

    add_executable(fusion_aos_soul_blue_imp_tests tests/test_aos_soul_blue_imp.c
                   src/runtime/aos/blue_souls/aos_soul_blue_imp.c src/runtime/aos/math/aos_math.c src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c
                   src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c
                   src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_blue_imp_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_blue_imp_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_blue_imp_tests)
    add_test(NAME aos_soul_blue_imp COMMAND fusion_aos_soul_blue_imp_tests)

    add_executable(fusion_aos_soul_blue_death_tests tests/test_aos_soul_blue_death.c
                   src/runtime/aos/blue_souls/aos_soul_blue_death.c src/runtime/aos/math/aos_math.c src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c
                   src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c
                   src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_blue_death_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_blue_death_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_blue_death_tests)
    add_test(NAME aos_soul_blue_death COMMAND fusion_aos_soul_blue_death_tests)

    add_executable(fusion_aos_soul_red_lightning_tests tests/test_aos_soul_red_lightning.c
                   src/runtime/aos/red_souls/aos_soul_red_lightning.c)
    target_include_directories(fusion_aos_soul_red_lightning_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_lightning_tests)
    add_test(NAME aos_soul_red_lightning COMMAND fusion_aos_soul_red_lightning_tests)

    add_executable(fusion_aos_soul_red_killer_doll_tests tests/test_aos_soul_red_killer_doll.c
                   src/runtime/aos/red_souls/aos_soul_red_killer_doll.c src/runtime/aos/core/aos_collision.c)
    target_include_directories(fusion_aos_soul_red_killer_doll_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_killer_doll_tests)
    add_test(NAME aos_soul_red_killer_doll COMMAND fusion_aos_soul_red_killer_doll_tests)

    add_executable(fusion_aos_soul_red_nemesis_tests tests/test_aos_soul_red_nemesis.c
                   src/runtime/aos/red_souls/aos_soul_red_nemesis.c)
    target_include_directories(fusion_aos_soul_red_nemesis_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_nemesis_tests)
    add_test(NAME aos_soul_red_nemesis COMMAND fusion_aos_soul_red_nemesis_tests)

    add_executable(fusion_aos_soul_red_ripper_tests tests/test_aos_soul_red_ripper.c
                   src/runtime/aos/red_souls/aos_soul_red_ripper.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_ripper_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_ripper_tests)
    add_test(NAME aos_soul_red_ripper COMMAND fusion_aos_soul_red_ripper_tests)

    add_executable(fusion_aos_soul_red_millione_tests tests/test_aos_soul_red_millione.c
                   src/runtime/aos/red_souls/aos_soul_red_millione.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_millione_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_millione_tests)
    add_test(NAME aos_soul_red_millione COMMAND fusion_aos_soul_red_millione_tests)

    add_executable(fusion_aos_soul_red_fish_head_tests tests/test_aos_soul_red_fish_head.c
                   src/runtime/aos/red_souls/aos_soul_red_fish_head.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_fish_head_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_fish_head_tests)
    add_test(NAME aos_soul_red_fish_head COMMAND fusion_aos_soul_red_fish_head_tests)

    add_executable(fusion_aos_soul_red_killer_fish_tests tests/test_aos_soul_red_killer_fish.c
                   src/runtime/aos/red_souls/aos_soul_red_killer_fish.c src/runtime/aos/core/aos_collision.c
                   src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_killer_fish_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_killer_fish_tests)
    add_test(NAME aos_soul_red_killer_fish COMMAND fusion_aos_soul_red_killer_fish_tests)

    add_executable(fusion_aos_soul_red_werewolf_tests tests/test_aos_soul_red_werewolf.c
                   src/runtime/aos/red_souls/aos_soul_red_werewolf.c ${AOS_ENEMY_SOURCES} src/runtime/aos/core/aos_damage_popup.c
                   src/runtime/aos/weapons/aos_weapon.c src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c
                   src/runtime/aos/souls/aos_soul.c src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c
                   src/runtime/aos/core/aos_anim.c src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_werewolf_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_werewolf_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_werewolf_tests)
    add_test(NAME aos_soul_red_werewolf COMMAND fusion_aos_soul_red_werewolf_tests)
    add_executable(fusion_aos_soul_red_killer_mantle_tests tests/test_aos_soul_red_killer_mantle.c
                   src/runtime/aos/red_souls/aos_soul_red_killer_mantle.c)
    target_include_directories(fusion_aos_soul_red_killer_mantle_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_killer_mantle_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_killer_mantle_tests)
    add_test(NAME aos_soul_red_killer_mantle COMMAND fusion_aos_soul_red_killer_mantle_tests)
    add_executable(fusion_aos_soul_red_were_beast_tests tests/test_aos_soul_red_were_beast.c
                   src/runtime/aos/red_souls/aos_soul_red_were_beast.c)
    target_include_directories(fusion_aos_soul_red_were_beast_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_were_beast_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_were_beast_tests)
    add_test(NAME aos_soul_red_were_beast COMMAND fusion_aos_soul_red_were_beast_tests)
    add_executable(fusion_aos_soul_red_flame_demon_tests tests/test_aos_soul_red_flame_demon.c
                   src/runtime/aos/red_souls/aos_soul_red_flame_demon.c src/runtime/aos/math/aos_math.c)
    target_include_directories(fusion_aos_soul_red_flame_demon_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_flame_demon_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_flame_demon_tests)
    add_test(NAME aos_soul_red_flame_demon COMMAND fusion_aos_soul_red_flame_demon_tests)
    add_executable(fusion_aos_soul_red_biphron_tests tests/test_aos_soul_red_biphron.c
                   src/runtime/aos/red_souls/aos_soul_red_biphron.c ${AOS_ENEMY_SOURCES} src/runtime/aos/core/aos_damage_popup.c
                   src/runtime/aos/weapons/aos_weapon.c src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c
                   src/runtime/aos/souls/aos_soul.c src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c
                   src/runtime/aos/core/aos_anim.c src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_biphron_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_biphron_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_biphron_tests)
    add_test(NAME aos_soul_red_biphron COMMAND fusion_aos_soul_red_biphron_tests)
    add_executable(fusion_aos_soul_red_kyoma_demon_tests tests/test_aos_soul_red_kyoma_demon.c
                   src/runtime/aos/red_souls/aos_soul_red_kyoma_demon.c)
    target_include_directories(fusion_aos_soul_red_kyoma_demon_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_kyoma_demon_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_kyoma_demon_tests)
    add_test(NAME aos_soul_red_kyoma_demon COMMAND fusion_aos_soul_red_kyoma_demon_tests)
    add_executable(fusion_aos_soul_red_student_witch_tests tests/test_aos_soul_red_student_witch.c
                   src/runtime/aos/red_souls/aos_soul_red_student_witch.c ${AOS_ENEMY_SOURCES} src/runtime/aos/core/aos_damage_popup.c
                   src/runtime/aos/weapons/aos_weapon.c src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c
                   src/runtime/aos/souls/aos_soul.c src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c
                   src/runtime/aos/core/aos_anim.c src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_student_witch_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_student_witch_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_student_witch_tests)
    add_test(NAME aos_soul_red_student_witch COMMAND fusion_aos_soul_red_student_witch_tests)
    add_executable(fusion_aos_soul_red_slime_tests tests/test_aos_soul_red_slime.c
                   src/runtime/aos/red_souls/aos_soul_red_slime.c src/runtime/aos/math/aos_math.c
                   src/runtime/aos/core/aos_collision.c)
    target_include_directories(fusion_aos_soul_red_slime_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_slime_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_slime_tests)
    add_test(NAME aos_soul_red_slime COMMAND fusion_aos_soul_red_slime_tests)
    add_executable(fusion_aos_soul_red_ukoback_tests tests/test_aos_soul_red_ukoback.c
                   src/runtime/aos/red_souls/aos_soul_red_ukoback.c)
    target_include_directories(fusion_aos_soul_red_ukoback_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_ukoback_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_ukoback_tests)
    add_test(NAME aos_soul_red_ukoback COMMAND fusion_aos_soul_red_ukoback_tests)
    add_executable(fusion_aos_soul_red_altair_tests tests/test_aos_soul_red_altair.c
                   src/runtime/aos/red_souls/aos_soul_red_altair.c)
    target_include_directories(fusion_aos_soul_red_altair_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_altair_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_altair_tests)
    add_test(NAME aos_soul_red_altair COMMAND fusion_aos_soul_red_altair_tests)
    add_executable(fusion_aos_soul_red_une_tests tests/test_aos_soul_red_une.c
                   src/runtime/aos/red_souls/aos_soul_red_une.c ${AOS_ENEMY_SOURCES} src/runtime/aos/core/aos_damage_popup.c
                   src/runtime/aos/weapons/aos_weapon.c src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c
                   src/runtime/aos/souls/aos_soul.c src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c
                   src/runtime/aos/core/aos_anim.c src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_une_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_une_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_une_tests)
    add_test(NAME aos_soul_red_une COMMAND fusion_aos_soul_red_une_tests)
    add_executable(fusion_aos_soul_red_mandragora_tests tests/test_aos_soul_red_mandragora.c
                   src/runtime/aos/red_souls/aos_soul_red_mandragora.c src/runtime/aos/math/aos_math.c)
    target_include_directories(fusion_aos_soul_red_mandragora_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_mandragora_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_mandragora_tests)
    add_test(NAME aos_soul_red_mandragora COMMAND fusion_aos_soul_red_mandragora_tests)
    add_executable(fusion_aos_soul_red_chronomage_tests tests/test_aos_soul_red_chronomage.c
                   src/runtime/aos/red_souls/aos_soul_red_chronomage.c)
    target_include_directories(fusion_aos_soul_red_chronomage_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_chronomage_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_chronomage_tests)
    add_test(NAME aos_soul_red_chronomage COMMAND fusion_aos_soul_red_chronomage_tests)
    add_executable(fusion_aos_soul_red_disc_armor_tests tests/test_aos_soul_red_disc_armor.c
                   src/runtime/aos/red_souls/aos_soul_red_disc_armor.c src/runtime/aos/math/aos_math.c)
    target_include_directories(fusion_aos_soul_red_disc_armor_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_disc_armor_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_disc_armor_tests)
    add_test(NAME aos_soul_red_disc_armor COMMAND fusion_aos_soul_red_disc_armor_tests)
    add_executable(fusion_aos_soul_red_cockatrice_tests tests/test_aos_soul_red_cockatrice.c
                   src/runtime/aos/red_souls/aos_soul_red_cockatrice.c src/runtime/aos/math/aos_math.c)
    target_include_directories(fusion_aos_soul_red_cockatrice_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_cockatrice_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_cockatrice_tests)
    add_test(NAME aos_soul_red_cockatrice COMMAND fusion_aos_soul_red_cockatrice_tests)
    add_executable(fusion_aos_soul_red_dryad_tests tests/test_aos_soul_red_dryad.c
                   src/runtime/aos/red_souls/aos_soul_red_dryad.c src/runtime/aos/math/aos_math.c
                   src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_dryad_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_dryad_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_dryad_tests)
    add_test(NAME aos_soul_red_dryad COMMAND fusion_aos_soul_red_dryad_tests)
    add_executable(fusion_aos_soul_red_arachne_tests tests/test_aos_soul_red_arachne.c
                   src/runtime/aos/red_souls/aos_soul_red_arachne.c)
    target_include_directories(fusion_aos_soul_red_arachne_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_arachne_tests)
    add_test(NAME aos_soul_red_arachne COMMAND fusion_aos_soul_red_arachne_tests)
    add_executable(fusion_aos_soul_red_minotaur_tests tests/test_aos_soul_red_minotaur.c
                   src/runtime/aos/red_souls/aos_soul_red_minotaur.c src/runtime/aos/math/aos_math.c)
    target_include_directories(fusion_aos_soul_red_minotaur_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_minotaur_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_minotaur_tests)
    add_test(NAME aos_soul_red_minotaur COMMAND fusion_aos_soul_red_minotaur_tests)
    add_executable(fusion_aos_soul_red_nightmare_tests tests/test_aos_soul_red_nightmare.c
                   src/runtime/aos/red_souls/aos_soul_red_nightmare.c)
    target_include_directories(fusion_aos_soul_red_nightmare_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_nightmare_tests)
    add_test(NAME aos_soul_red_nightmare COMMAND fusion_aos_soul_red_nightmare_tests)

    add_executable(fusion_aos_soul_red_man_eater_tests tests/test_aos_soul_red_man_eater.c
                   src/runtime/aos/red_souls/aos_soul_red_man_eater.c)
    target_include_directories(fusion_aos_soul_red_man_eater_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soul_red_man_eater_tests)
    add_test(NAME aos_soul_red_man_eater COMMAND fusion_aos_soul_red_man_eater_tests)

    add_executable(fusion_aos_soul_red_gladiator_tests tests/test_aos_soul_red_gladiator.c
                   src/runtime/aos/red_souls/aos_soul_red_gladiator.c ${AOS_ENEMY_SOURCES} src/runtime/aos/core/aos_damage_popup.c
                   src/runtime/aos/weapons/aos_weapon.c src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c
                   src/runtime/aos/souls/aos_soul.c src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c
                   src/runtime/aos/core/aos_anim.c src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_gladiator_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_gladiator_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_gladiator_tests)
    add_test(NAME aos_soul_red_gladiator COMMAND fusion_aos_soul_red_gladiator_tests)

    add_executable(fusion_aos_soul_red_ghost_tests tests/test_aos_soul_red_ghost.c
                   src/runtime/aos/red_souls/aos_soul_red_ghost.c src/runtime/aos/red_souls/aos_soul_red_devil.c
                   src/runtime/aos/math/aos_math.c src/runtime/aos/core/aos_damage_popup.c src/runtime/aos/weapons/aos_weapon.c src/runtime/aos/core/aos_effect.c
                   src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c src/runtime/aos/core/aos_collision.c
                   src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c src/runtime/aos/core/aos_stats.c
                   src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_ghost_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_ghost_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_ghost_tests)
    add_test(NAME aos_soul_red_ghost COMMAND fusion_aos_soul_red_ghost_tests)

    add_executable(fusion_aos_soul_red_siren_tests tests/test_aos_soul_red_siren.c
                   src/runtime/aos/red_souls/aos_soul_red_siren.c src/runtime/aos/math/aos_math.c src/runtime/aos/core/aos_damage_popup.c
                   src/runtime/aos/weapons/aos_weapon.c src/runtime/aos/core/aos_effect.c src/runtime/aos/core/aos_soma.c
                   src/runtime/aos/souls/aos_soul.c src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c
                   src/runtime/aos/core/aos_anim.c src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soul_red_siren_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_red_siren_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_soul_red_siren_tests)
    add_test(NAME aos_soul_red_siren COMMAND fusion_aos_soul_red_siren_tests)

    add_executable(fusion_aos_stats_tests tests/test_aos_stats.c src/runtime/aos/core/aos_stats.c)
    target_include_directories(fusion_aos_stats_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_stats_tests)
    add_test(NAME aos_stats COMMAND fusion_aos_stats_tests)

    add_executable(fusion_aos_main_menu_sprites_tests tests/test_aos_main_menu_sprites.c
                   src/runtime/aos/menus/aos_main_menu_sprites.c src/runtime/aos/menus/aos_main_menu_files.c
                   src/runtime/aos/game/aos_obj.c src/runtime/aos/core/aos_anim.c src/runtime/aos/menus/aos_menu_bg0.c
                   src/runtime/aos/core/aos_rom_graphics.c)
    target_include_directories(fusion_aos_main_menu_sprites_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_main_menu_sprites_tests PRIVATE fusion_assets m)
    fusion_enable_warnings(fusion_aos_main_menu_sprites_tests)
    add_test(NAME aos_main_menu_sprites COMMAND fusion_aos_main_menu_sprites_tests)

    add_executable(fusion_aos_menu_bg0_tests tests/test_aos_menu_bg0.c
                   src/runtime/aos/menus/aos_menu_bg0.c src/runtime/aos/core/aos_rom_graphics.c)
    target_include_directories(fusion_aos_menu_bg0_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_menu_bg0_tests PRIVATE fusion_assets m)
    fusion_enable_warnings(fusion_aos_menu_bg0_tests)
    add_test(NAME aos_menu_bg0 COMMAND fusion_aos_menu_bg0_tests)

    add_executable(fusion_aos_window_frame_tests tests/test_aos_window_frame.c
                   src/runtime/aos/menus/aos_window_frame.c src/runtime/aos/core/aos_rom_graphics.c
                   src/runtime/aos/intro/aos_intro_font.c)
    target_include_directories(fusion_aos_window_frame_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_window_frame_tests PRIVATE fusion_assets m)
    fusion_enable_warnings(fusion_aos_window_frame_tests)
    add_test(NAME aos_window_frame COMMAND fusion_aos_window_frame_tests)

    add_executable(fusion_aos_soul_notice_tests tests/test_aos_soul_notice.c
                   src/runtime/aos/souls/aos_soul_notice.c src/runtime/aos/core/aos_rom_graphics.c
                   src/runtime/aos/intro/aos_intro_font.c)
    target_include_directories(fusion_aos_soul_notice_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_soul_notice_tests PRIVATE fusion_assets m)
    fusion_enable_warnings(fusion_aos_soul_notice_tests)
    add_test(NAME aos_soul_notice COMMAND fusion_aos_soul_notice_tests)

    add_executable(fusion_aos_text_window_tests tests/test_aos_text_window.c
                   src/runtime/aos/menus/aos_text_window.c src/runtime/aos/intro/aos_intro_resources.c
                   src/runtime/aos/intro/aos_intro_font.c src/runtime/aos/intro/aos_intro_entities.c)
    target_include_directories(fusion_aos_text_window_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_text_window_tests PRIVATE fusion_assets m)
    fusion_enable_warnings(fusion_aos_text_window_tests)
    add_test(NAME aos_text_window COMMAND fusion_aos_text_window_tests)

    add_executable(fusion_aos_title_fade_tests tests/test_aos_title_fade.c src/runtime/aos/title/aos_title_fade.c)
    target_include_directories(fusion_aos_title_fade_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_title_fade_tests)
    add_test(NAME aos_title_fade COMMAND fusion_aos_title_fade_tests)

    add_executable(fusion_aos_game_mode_tests tests/test_aos_game_mode.c
                   src/runtime/aos/game/aos_game_mode.c src/runtime/aos/title/aos_title_fade.c
                   src/runtime/aos/core/aos_input.c)
    target_include_directories(fusion_aos_game_mode_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_game_mode_tests)
    add_test(NAME aos_game_mode COMMAND fusion_aos_game_mode_tests)

    add_executable(fusion_mzm_room_overlay_tests tests/test_mzm_room_overlay.c
                   src/runtime/mzm/core/mzm_room_overlay.c src/runtime/project_room_document.c
                   src/runtime/aos/game/aos_room_overlay.c src/core/native_map.c)
    target_include_directories(fusion_mzm_room_overlay_tests PRIVATE src/runtime include)
    target_link_libraries(fusion_mzm_room_overlay_tests PRIVATE fusion_json)
    fusion_enable_warnings(fusion_mzm_room_overlay_tests)
    add_test(NAME mzm_room_overlay COMMAND fusion_mzm_room_overlay_tests
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

    add_executable(fusion_aos_room_overlay_tests tests/test_aos_room_overlay.c
                   src/runtime/aos/game/aos_room_overlay.c src/runtime/project_room_document.c)
    target_include_directories(fusion_aos_room_overlay_tests PRIVATE src/runtime include)
    target_link_libraries(fusion_aos_room_overlay_tests PRIVATE fusion_json)
    fusion_enable_warnings(fusion_aos_room_overlay_tests)
    add_test(NAME aos_room_overlay COMMAND fusion_aos_room_overlay_tests
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

    add_executable(fusion_host_tests tests/test_fusion_host.c src/runtime/fusion_host.c)
    target_include_directories(fusion_host_tests PRIVATE src/runtime)
    target_link_libraries(fusion_host_tests PRIVATE SDL3::SDL3)
    fusion_enable_warnings(fusion_host_tests)
    add_test(NAME fusion_host COMMAND fusion_host_tests)

    add_executable(fusion_metroidvania_split_tests tests/test_metroidvania_split.c
                   src/runtime/metroidvania_split.c ${fusion_rotating_line_sources})
    target_include_directories(fusion_metroidvania_split_tests PRIVATE src/runtime)
    target_compile_definitions(fusion_metroidvania_split_tests PRIVATE
        MV_SPLIT_SAMPLE_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/config/metroidvania_rotating_line.json")
    target_link_libraries(fusion_metroidvania_split_tests PRIVATE m)
    fusion_enable_warnings(fusion_metroidvania_split_tests)
    add_test(NAME metroidvania_split COMMAND fusion_metroidvania_split_tests)

    add_executable(fusion_aos_title_screen_tests tests/test_aos_title_screen.c
                   src/runtime/aos/title/aos_title_screen.c src/runtime/aos/math/aos_math.c src/runtime/aos/game/aos_obj.c src/runtime/aos/core/aos_anim.c)
    target_include_directories(fusion_aos_title_screen_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_title_screen_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_title_screen_tests)
    add_test(NAME aos_title_screen COMMAND fusion_aos_title_screen_tests)

    add_executable(fusion_aos_game_lifecycle_tests tests/test_aos_game_lifecycle.c
                   src/runtime/aos/game/aos_game_lifecycle.c src/runtime/aos/intro/aos_game_over.c src/runtime/aos/game/aos_game_mode.c
                   src/runtime/aos/menus/aos_main_menu.c
                   src/runtime/aos/title/aos_title_fade.c src/runtime/aos/title/aos_title_screen.c src/runtime/aos/math/aos_math.c src/runtime/aos/game/aos_obj.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_input.c src/runtime/aos/intro/aos_intro_cutscene.c
                   src/runtime/aos/intro/aos_intro_entities.c)
    target_include_directories(fusion_aos_game_lifecycle_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_game_lifecycle_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_game_lifecycle_tests)
    add_test(NAME aos_game_lifecycle COMMAND fusion_aos_game_lifecycle_tests)

    add_executable(fusion_aos_intro_cutscene_tests
                   tests/test_aos_intro_cutscene.c
                   src/runtime/aos/intro/aos_intro_cutscene.c
                   src/runtime/aos/intro/aos_intro_entities.c)
    target_include_directories(fusion_aos_intro_cutscene_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_intro_cutscene_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_intro_cutscene_tests)
    add_test(NAME aos_intro_cutscene COMMAND fusion_aos_intro_cutscene_tests)

    add_executable(fusion_aos_intro_entities_tests
                   tests/test_aos_intro_entities.c
                   src/runtime/aos/intro/aos_intro_entities.c)
    target_include_directories(fusion_aos_intro_entities_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_intro_entities_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_intro_entities_tests)
    add_test(NAME aos_intro_entities COMMAND fusion_aos_intro_entities_tests)

    add_executable(fusion_aos_intro_resources_tests
                   tests/test_aos_intro_resources.c
                   src/runtime/aos/intro/aos_intro_resources.c
                   src/runtime/aos/intro/aos_intro_entities.c)
    target_include_directories(fusion_aos_intro_resources_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_intro_resources_tests PRIVATE fusion_assets m)
    fusion_enable_warnings(fusion_aos_intro_resources_tests)
    add_test(NAME aos_intro_resources COMMAND fusion_aos_intro_resources_tests)

    add_executable(fusion_aos_intro_font_tests
                   tests/test_aos_intro_font.c
                   src/runtime/aos/intro/aos_intro_font.c)
    target_include_directories(fusion_aos_intro_font_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_intro_font_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_aos_intro_font_tests)
    add_test(NAME aos_intro_font COMMAND fusion_aos_intro_font_tests)

    add_executable(fusion_aos_opening_event_tests
                   tests/test_aos_opening_event.c
                   src/runtime/aos/title/aos_opening_event.c src/runtime/aos/core/aos_stats.c
                   src/runtime/aos/intro/aos_intro_resources.c
                   src/runtime/aos/intro/aos_intro_entities.c)
    target_include_directories(fusion_aos_opening_event_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_opening_event_tests PRIVATE fusion_assets m)
    fusion_enable_warnings(fusion_aos_opening_event_tests)
    add_test(NAME aos_opening_event COMMAND fusion_aos_opening_event_tests)

    add_executable(fusion_aos_portrait_tests
                   tests/test_aos_portrait.c
                   src/runtime/aos/menus/aos_portrait.c
                   src/runtime/aos/core/aos_rom_graphics.c)
    target_include_directories(fusion_aos_portrait_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_portrait_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_aos_portrait_tests)
    add_test(NAME aos_portrait COMMAND fusion_aos_portrait_tests)

    add_executable(fusion_aos_intro_graphics_tests
                   tests/test_aos_intro_graphics.c
                   src/runtime/aos/intro/aos_intro_graphics.c
                   src/runtime/aos/game/aos_obj_rom.c
                   src/runtime/aos/core/aos_rom_graphics.c)
    target_include_directories(fusion_aos_intro_graphics_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_intro_graphics_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_aos_intro_graphics_tests)
    add_test(NAME aos_intro_graphics COMMAND fusion_aos_intro_graphics_tests)

    add_executable(fusion_aos_logo_graphics_tests tests/test_aos_logo_graphics.c
                   src/runtime/aos/title/aos_logo_graphics.c src/runtime/aos/title/aos_title_graphics.c src/runtime/aos/game/aos_obj_rom.c
                   src/runtime/aos/core/aos_rom_graphics.c)
    target_include_directories(fusion_aos_logo_graphics_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_logo_graphics_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_aos_logo_graphics_tests)
    add_test(NAME aos_logo_graphics COMMAND fusion_aos_logo_graphics_tests)

    add_executable(fusion_aos_main_menu_graphics_tests
                   tests/test_aos_main_menu_graphics.c
                   src/runtime/aos/menus/aos_main_menu_graphics.c
                   src/runtime/aos/core/aos_rom_graphics.c)
    target_include_directories(fusion_aos_main_menu_graphics_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_main_menu_graphics_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_aos_main_menu_graphics_tests)
    add_test(NAME aos_main_menu_graphics COMMAND fusion_aos_main_menu_graphics_tests)

    add_executable(fusion_aos_pause_graphics_tests
                   tests/test_aos_pause_graphics.c
                   src/runtime/aos/menus/aos_pause_graphics.c
                   src/runtime/aos/game/aos_obj_rom.c
                   src/runtime/aos/core/aos_rom_graphics.c
                   src/runtime/aos/intro/aos_intro_resources.c
                   src/runtime/aos/intro/aos_intro_entities.c
                   src/runtime/aos/intro/aos_intro_font.c
                   src/runtime/aos/menus/aos_menu_cells.c)
    target_include_directories(fusion_aos_pause_graphics_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_pause_graphics_tests PRIVATE fusion_assets m)
    fusion_enable_warnings(fusion_aos_pause_graphics_tests)
    add_test(NAME aos_pause_graphics COMMAND fusion_aos_pause_graphics_tests)

    add_executable(fusion_aos_menu_cells_tests
                   tests/test_aos_menu_cells.c
                   src/runtime/aos/menus/aos_menu_cells.c)
    target_include_directories(fusion_aos_menu_cells_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_menu_cells_tests)
    add_test(NAME aos_menu_cells COMMAND fusion_aos_menu_cells_tests)

    add_executable(fusion_aos_quick_map_tests
                   tests/test_aos_quick_map.c
                   src/runtime/aos/menus/aos_quick_map.c
                   src/runtime/aos/core/aos_rom_graphics.c)
    target_include_directories(fusion_aos_quick_map_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_quick_map_tests PRIVATE fusion_assets)
    fusion_enable_warnings(fusion_aos_quick_map_tests)
    add_test(NAME aos_quick_map COMMAND fusion_aos_quick_map_tests)

    add_executable(fusion_aos_main_menu_tests tests/test_aos_main_menu.c
                   src/runtime/aos/menus/aos_main_menu.c)
    target_include_directories(fusion_aos_main_menu_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_main_menu_tests)
    add_test(NAME aos_main_menu COMMAND fusion_aos_main_menu_tests)

    add_executable(fusion_aos_input_tests tests/test_aos_input.c src/runtime/aos/core/aos_input.c)
    target_include_directories(fusion_aos_input_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_input_tests)
    add_test(NAME aos_input COMMAND fusion_aos_input_tests)

    add_executable(fusion_gba_input_tests tests/test_gba_input.c src/runtime/gba_input.c)
    target_include_directories(fusion_gba_input_tests PRIVATE src/runtime)
    target_link_libraries(fusion_gba_input_tests PRIVATE SDL3::SDL3)
    fusion_enable_warnings(fusion_gba_input_tests)
    add_test(NAME gba_input COMMAND fusion_gba_input_tests
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_executable(fusion_debug_menu_tests tests/test_debug_menu.c src/runtime/debug_menu.c)
    target_include_directories(fusion_debug_menu_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_debug_menu_tests)
    add_test(NAME debug_menu COMMAND fusion_debug_menu_tests)

    # ROM-free Aria of Sorrow BG1 collision primitives.
    add_executable(fusion_aos_collision_tests tests/test_aos_collision.c
                   src/runtime/aos/core/aos_collision.c)
    target_include_directories(fusion_aos_collision_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_collision_tests)
    add_test(NAME aos_collision COMMAND fusion_aos_collision_tests)

    add_executable(fusion_aos_soma_tests tests/test_aos_soma.c
                   src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c
                   src/runtime/aos/core/aos_anim.c src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_soma_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soma_tests)
    add_test(NAME aos_soma COMMAND fusion_aos_soma_tests)

    add_executable(fusion_aos_room_tests tests/test_aos_room.c src/runtime/aos/game/aos_room.c
                   src/runtime/aos/core/aos_collision.c)
    target_include_directories(fusion_aos_room_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_room_tests)
    add_test(NAME aos_room COMMAND fusion_aos_room_tests)

    add_executable(fusion_aos_door_tests tests/test_aos_door.c src/runtime/aos/game/aos_door.c
                   src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_door_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_door_tests)
    add_test(NAME aos_door COMMAND fusion_aos_door_tests)

    add_executable(fusion_aos_object02_tests tests/test_aos_object02.c
                   src/runtime/aos/game/aos_object02.c src/runtime/aos/core/aos_collision.c)
    target_include_directories(fusion_aos_object02_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_object02_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_object02_tests)
    add_test(NAME aos_object02 COMMAND fusion_aos_object02_tests)

    add_executable(fusion_aos_object01_tests tests/test_aos_object01.c
                   src/runtime/aos/game/aos_object01.c)
    target_include_directories(fusion_aos_object01_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_object01_tests)
    add_test(NAME aos_object01 COMMAND fusion_aos_object01_tests)

    add_executable(fusion_aos_object04_tests tests/test_aos_object04.c
                   src/runtime/aos/game/aos_object04.c src/runtime/aos/core/aos_collision.c)
    target_include_directories(fusion_aos_object04_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_object04_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_object04_tests)
    add_test(NAME aos_object04 COMMAND fusion_aos_object04_tests)

    add_executable(fusion_aos_object03_tests tests/test_aos_object03.c
                   src/runtime/aos/game/aos_object03.c src/runtime/aos/core/aos_collision.c)
    target_include_directories(fusion_aos_object03_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_object03_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_object03_tests)
    add_test(NAME aos_object03 COMMAND fusion_aos_object03_tests)

    add_executable(fusion_aos_object05_tests tests/test_aos_object05.c
                   src/runtime/aos/game/aos_object05.c)
    target_include_directories(fusion_aos_object05_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_object05_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_object05_tests)
    add_test(NAME aos_object05 COMMAND fusion_aos_object05_tests)

    add_executable(fusion_aos_object06_tests tests/test_aos_object06.c
                   src/runtime/aos/game/aos_object06.c src/runtime/aos/core/aos_collision.c)
    target_include_directories(fusion_aos_object06_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_object06_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_object06_tests)
    add_test(NAME aos_object06 COMMAND fusion_aos_object06_tests)

    add_executable(fusion_aos_object07_tests tests/test_aos_object07.c
                   src/runtime/aos/game/aos_object07.c)
    target_include_directories(fusion_aos_object07_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_object07_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_object07_tests)
    add_test(NAME aos_object07 COMMAND fusion_aos_object07_tests)

    add_executable(fusion_aos_object0c_tests tests/test_aos_object0c.c
                   src/runtime/aos/game/aos_object0c.c
                   src/runtime/aos/game/aos_object01.c)
    target_include_directories(fusion_aos_object0c_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_object0c_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_object0c_tests)
    add_test(NAME aos_object0c COMMAND fusion_aos_object0c_tests)

    add_executable(fusion_aos_object0f_tests tests/test_aos_object0f.c
                   src/runtime/aos/game/aos_object0f.c)
    target_include_directories(fusion_aos_object0f_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_object0f_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_object0f_tests)
    add_test(NAME aos_object0f COMMAND fusion_aos_object0f_tests)

    add_executable(fusion_aos_object0d_tests tests/test_aos_object0d.c
                   src/runtime/aos/game/aos_object0d.c)
    target_include_directories(fusion_aos_object0d_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_object0d_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_object0d_tests)
    add_test(NAME aos_object0d COMMAND fusion_aos_object0d_tests)

    add_executable(fusion_aos_object0e_tests tests/test_aos_object0e.c
                   src/runtime/aos/game/aos_object0e.c)
    target_include_directories(fusion_aos_object0e_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_object0e_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_object0e_tests)
    add_test(NAME aos_object0e COMMAND fusion_aos_object0e_tests)

    add_executable(fusion_aos_object11_tests tests/test_aos_object11.c
                   src/runtime/aos/game/aos_object11.c)
    target_include_directories(fusion_aos_object11_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_object11_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_object11_tests)
    add_test(NAME aos_object11 COMMAND fusion_aos_object11_tests)

    add_executable(fusion_aos_object12_tests tests/test_aos_object12.c
                   src/runtime/aos/game/aos_object12.c)
    target_include_directories(fusion_aos_object12_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_object12_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_object12_tests)
    add_test(NAME aos_object12 COMMAND fusion_aos_object12_tests)

    add_executable(fusion_aos_object13_tests tests/test_aos_object13.c
                   src/runtime/aos/game/aos_object13.c
                   src/runtime/aos/math/aos_math.c)
    target_include_directories(fusion_aos_object13_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_object13_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_object13_tests)
    add_test(NAME aos_object13 COMMAND fusion_aos_object13_tests)

    add_executable(fusion_aos_object19_tests tests/test_aos_object19.c
                   src/runtime/aos/game/aos_object19.c)
    target_include_directories(fusion_aos_object19_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_object19_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_object19_tests)
    add_test(NAME aos_object19 COMMAND fusion_aos_object19_tests)

    add_executable(fusion_aos_weapon_tests tests/test_aos_weapon.c src/runtime/aos/weapons/aos_weapon.c src/runtime/aos/core/aos_effect.c
                   src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_weapon_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_weapon_tests)
    add_test(NAME aos_weapon COMMAND fusion_aos_weapon_tests)

    add_executable(fusion_aos_wall_tests tests/test_aos_wall.c src/runtime/aos/core/aos_wall.c
                   src/runtime/aos/weapons/aos_weapon.c src/runtime/aos/core/aos_effect.c
                   src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c
                   src/runtime/aos/core/aos_audio.c)
    target_include_directories(fusion_aos_wall_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_wall_tests)
    add_test(NAME aos_wall COMMAND fusion_aos_wall_tests)

    add_executable(fusion_aos_enemy_tests tests/test_aos_enemy.c ${AOS_ENEMY_SOURCES} src/runtime/aos/core/aos_damage_popup.c
                   src/runtime/aos/core/aos_audio.c src/runtime/aos/weapons/aos_weapon.c src/runtime/aos/core/aos_effect.c
                   src/runtime/aos/core/aos_soma.c src/runtime/aos/souls/aos_soul.c src/runtime/aos/core/aos_stats.c src/runtime/aos/core/aos_collision.c src/runtime/aos/core/aos_combat.c src/runtime/aos/core/aos_anim.c)
    target_include_directories(fusion_aos_enemy_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_enemy_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_enemy_tests)
    add_test(NAME aos_enemy COMMAND fusion_aos_enemy_tests)

    add_executable(fusion_aos_damage_popup_tests tests/test_aos_damage_popup.c
                   src/runtime/aos/core/aos_damage_popup.c)
    target_include_directories(fusion_aos_damage_popup_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_damage_popup_tests)
    add_test(NAME aos_damage_popup COMMAND fusion_aos_damage_popup_tests)

    # ROM-free native Zero Mission pose controller regression.
    add_executable(fusion_mzm_samus_tests tests/test_mzm_samus.c src/runtime/mzm/game/mzm_samus.c
                   src/runtime/mzm/weapons/mzm_projectiles.c src/runtime/mzm/core/mzm_sfx.c src/runtime/mzm/core/mzm_screen_shake.c
                   src/runtime/mzm/weapons/mzm_power_bomb.c)
    target_include_directories(fusion_mzm_samus_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_samus_tests)
    add_test(NAME mzm_samus_controller COMMAND fusion_mzm_samus_tests)

    add_test(NAME room_runtime_loader
        COMMAND $<TARGET_FILE:fusion_room_runtime> --check
                ${CMAKE_SOURCE_DIR}/tests/fixtures/project_room_0124.preview.tsv)

    add_test(NAME room_runtime_native_source_0138
        COMMAND $<TARGET_FILE:fusion_room_runtime> --check
                --native-source ${CMAKE_SOURCE_DIR}/tests/fixtures/runtime_native_clip_0138.tsv
                ${CMAKE_SOURCE_DIR}/tests/fixtures/project_room_0124.preview.tsv)

    add_test(NAME room_runtime_animation_registry
        COMMAND $<TARGET_FILE:fusion_room_runtime> --check-animations
                --samus-library ${CMAKE_SOURCE_DIR}/tests/fixtures/samus_animation_registry.index.tsv
                --samus-map ${CMAKE_SOURCE_DIR}/tests/fixtures/samus_animation_registry.map.tsv)

    add_test(NAME room_runtime_native_room
        COMMAND $<TARGET_FILE:fusion_room_runtime> --check
                ${CMAKE_SOURCE_DIR}/tests/fixtures/runtime_native_room.tsv)

    add_test(NAME room_runtime_sprite_library
        COMMAND $<TARGET_FILE:fusion_room_runtime> --check-library
                ${CMAKE_SOURCE_DIR}/tests/fixtures/samus_animation_registry.index.tsv)

    add_test(NAME room_package_loader
        COMMAND $<TARGET_FILE:fusion_room_package_viewer> --check
                ${CMAKE_SOURCE_DIR}/tests/fixtures/project_room_0124.preview.tsv)

    add_test(NAME no_proprietary_files
        COMMAND fusion_check_no_proprietary ${CMAKE_SOURCE_DIR})
endif()
