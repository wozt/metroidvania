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

add_library(fusion_json STATIC src/core/json.c)
target_include_directories(fusion_json PUBLIC include)
if(NOT WIN32)
    target_link_libraries(fusion_json PUBLIC m)
endif()
fusion_enable_warnings(fusion_json)

add_library(fusion_editor_backend STATIC
    src/editor_backend/catalog.c
    src/editor_backend/cli.c
    src/editor_backend/commands.c
    src/editor_backend/common.c
    src/editor_backend/drafts.c
)
target_link_libraries(fusion_editor_backend PUBLIC fusion_json)
fusion_enable_warnings(fusion_editor_backend)

add_library(fusion_native_editor_core STATIC
    src/core/editor_cli_launcher.c
    src/core/native_map.c
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
)
target_include_directories(fusion_pipeline PUBLIC include)
target_include_directories(fusion_pipeline PRIVATE src/pipeline src/assets/core)
target_link_libraries(fusion_pipeline PUBLIC fusion_assets fusion_json)
fusion_enable_warnings(fusion_pipeline)

add_executable(fusion_rebuild src/tools/fusion_rebuild.c)
target_link_libraries(fusion_rebuild PRIVATE fusion_pipeline)
target_compile_definitions(fusion_rebuild PRIVATE FUSION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
fusion_enable_warnings(fusion_rebuild)

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

add_executable(fusion_room_runtime src/runtime/room_runtime.c src/runtime/mzm_samus.c
               src/runtime/mzm_projectiles.c src/runtime/gba_input.c src/runtime/debug_menu.c
               src/runtime/mzm_sprites.c src/runtime/mzm_space_pirate.c src/runtime/mzm_parasite.c src/runtime/mzm_atomic.c src/runtime/mzm_polyp.c src/runtime/mzm_tourian.c src/runtime/mzm_worker_robot.c src/runtime/mzm_metroid.c src/runtime/mzm_kraid.c src/runtime/mzm_ridley.c src/runtime/mzm_mother_brain.c src/runtime/mzm_deorem.c src/runtime/mzm_acid_worm.c src/runtime/mzm_gadora.c src/runtime/mzm_imago_larva.c src/runtime/mzm_imago_cocoon.c src/runtime/mzm_mecha_ridley.c src/runtime/mzm_hive.c src/runtime/mzm_baristute.c src/runtime/mzm_tangle_vine.c src/runtime/mzm_ruins_test.c src/runtime/mzm_chozodia_scripted.c src/runtime/mzm_elevator_statue.c src/runtime/mzm_imago.c src/runtime/mzm_escape_ship.c src/runtime/mzm_crocomire.c
                   src/runtime/mzm_chozodia_security.c src/runtime/mzm_chozo_statue.c src/runtime/mzm_item_sprites.c src/runtime/mzm_gadget_sprites.c src/runtime/mzm_gunship.c src/runtime/mzm_hatches.c
               src/runtime/mzm_blocks.c
               src/runtime/mzm_effects.c src/runtime/mzm_audio.c src/runtime/gba_psg.c
               src/runtime/mzm_sfx.c src/runtime/mzm_screen_shake.c src/runtime/mzm_background_effect.c src/runtime/mzm_escape.c src/runtime/mzm_power_bomb.c
               src/runtime/mzm_items.c src/runtime/mzm_message_banner.c
               src/runtime/gba_ppu.c src/runtime/mzm_rom.c src/runtime/mzm_hud.c
               src/runtime/mzm_screen.c src/runtime/mzm_title_screen.c src/runtime/mzm_room_gfx.c src/runtime/mzm_tourian_escape.c src/runtime/mzm_chozodia_escape.c src/runtime/mzm_credits.c
               src/runtime/mzm_pause_screen.c src/runtime/mzm_text.c src/runtime/mzm_save.c src/runtime/mzm_time_attack.c
               src/runtime/mzm_file_select.c src/runtime/mzm_game_over.c
               src/runtime/mzm_intro.c src/runtime/mzm_cutscene.c src/runtime/mzm_in_game_cutscene.c src/runtime/mzm_door_fade.c)
target_link_libraries(fusion_room_runtime PRIVATE SDL3::SDL3 m)
fusion_enable_warnings(fusion_room_runtime)

add_executable(fusion_aria_runtime src/runtime/aria_runtime.c src/runtime/aos_soma.c
               src/runtime/aos_collision.c src/runtime/aos_combat.c src/runtime/aos_anim.c src/runtime/aos_room.c
               src/runtime/aos_door.c src/runtime/aos_weapon.c src/runtime/aos_enemy.c src/runtime/aos_effect.c
               src/runtime/gba_input.c src/runtime/aos_input.c src/runtime/aos_save.c
               src/runtime/aos_save_room.c
               src/runtime/debug_menu.c)
add_executable(fusion_aria_title_preview src/runtime/aos_title_preview.c
               src/runtime/aos_title_fade.c src/runtime/aos_title_input.c
               src/runtime/aos_menu_preview.c src/runtime/aos_save.c)
target_include_directories(fusion_aria_title_preview PRIVATE src/runtime)
target_link_libraries(fusion_aria_title_preview PRIVATE SDL3::SDL3)
fusion_enable_warnings(fusion_aria_title_preview)

target_link_libraries(fusion_aria_runtime PRIVATE SDL3::SDL3 m)
fusion_enable_warnings(fusion_aria_runtime)

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
                   src/runtime/mzm_samus.c src/runtime/mzm_projectiles.c src/runtime/mzm_hatches.c
                   src/runtime/mzm_blocks.c src/runtime/mzm_sfx.c src/runtime/mzm_screen_shake.c src/runtime/mzm_power_bomb.c
                   src/runtime/mzm_items.c src/runtime/mzm_message_banner.c
                   src/runtime/gba_ppu.c src/runtime/mzm_rom.c src/runtime/mzm_hud.c
                   src/runtime/mzm_room_gfx.c src/runtime/mzm_save.c src/runtime/mzm_time_attack.c
                   src/runtime/mzm_screen.c)
    target_include_directories(fusion_runtime_steep_slopes_tests PRIVATE src/runtime)
    target_compile_definitions(fusion_runtime_steep_slopes_tests PRIVATE
                               FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
    target_link_libraries(fusion_runtime_steep_slopes_tests PRIVATE SDL3::SDL3)
    fusion_enable_warnings(fusion_runtime_steep_slopes_tests)
    add_test(NAME runtime_steep_slopes COMMAND fusion_runtime_steep_slopes_tests)

    add_executable(fusion_mzm_blocks_tests tests/test_mzm_blocks.c src/runtime/mzm_blocks.c
                   src/runtime/mzm_sfx.c src/runtime/mzm_screen_shake.c)
    target_include_directories(fusion_mzm_blocks_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_blocks_tests)
    add_test(NAME mzm_blocks COMMAND fusion_mzm_blocks_tests)

    add_executable(fusion_mzm_power_bomb_tests tests/test_mzm_power_bomb.c
                   src/runtime/mzm_power_bomb.c src/runtime/mzm_sfx.c src/runtime/mzm_screen_shake.c)
    target_include_directories(fusion_mzm_power_bomb_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_power_bomb_tests)
    add_test(NAME mzm_power_bomb COMMAND fusion_mzm_power_bomb_tests)

    add_executable(fusion_mzm_background_effect_tests tests/test_mzm_background_effect.c
                   src/runtime/mzm_background_effect.c)
    target_include_directories(fusion_mzm_background_effect_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_background_effect_tests)
    add_test(NAME mzm_background_effect COMMAND fusion_mzm_background_effect_tests)

    add_executable(fusion_mzm_screens_tests tests/test_mzm_screens.c src/runtime/gba_ppu.c
                   src/runtime/mzm_rom.c src/runtime/mzm_screen.c src/runtime/mzm_title_screen.c
                   src/runtime/mzm_sfx.c src/runtime/mzm_screen_shake.c src/runtime/mzm_hud.c src/runtime/mzm_pause_screen.c
                   src/runtime/mzm_text.c src/runtime/mzm_file_select.c src/runtime/mzm_game_over.c
                   src/runtime/mzm_intro.c src/runtime/mzm_cutscene.c src/runtime/mzm_in_game_cutscene.c src/runtime/mzm_door_fade.c
                   src/runtime/mzm_room_gfx.c src/runtime/mzm_tourian_escape.c src/runtime/mzm_chozodia_escape.c src/runtime/mzm_credits.c)
    target_include_directories(fusion_mzm_screens_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_screens_tests)
    add_test(NAME mzm_screens COMMAND fusion_mzm_screens_tests
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(fusion_mzm_save_tests tests/test_mzm_save.c src/runtime/mzm_save.c src/runtime/mzm_time_attack.c)
    target_include_directories(fusion_mzm_save_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_save_tests)
    add_test(NAME mzm_save COMMAND fusion_mzm_save_tests)

    add_executable(fusion_mzm_room_gfx_tests tests/test_mzm_room_gfx.c src/runtime/gba_ppu.c
                   src/runtime/mzm_rom.c src/runtime/mzm_room_gfx.c src/runtime/mzm_screen.c)
    target_include_directories(fusion_mzm_room_gfx_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_room_gfx_tests)
    add_test(NAME mzm_room_gfx COMMAND fusion_mzm_room_gfx_tests
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(fusion_mzm_hud_tests tests/test_mzm_hud.c src/runtime/gba_ppu.c
                   src/runtime/mzm_rom.c src/runtime/mzm_hud.c)
    target_include_directories(fusion_mzm_hud_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_hud_tests)
    add_test(NAME mzm_hud COMMAND fusion_mzm_hud_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(fusion_mzm_audio_tests tests/test_mzm_audio.c src/runtime/mzm_audio.c
                   src/runtime/gba_psg.c)
    target_include_directories(fusion_mzm_audio_tests PRIVATE src/runtime)
    target_link_libraries(fusion_mzm_audio_tests PRIVATE m)
    fusion_enable_warnings(fusion_mzm_audio_tests)
    add_executable(fusion_mzm_time_attack_tests tests/test_mzm_time_attack.c src/runtime/mzm_time_attack.c)
    target_include_directories(fusion_mzm_time_attack_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_time_attack_tests)
    add_test(NAME mzm_time_attack COMMAND fusion_mzm_time_attack_tests)

    add_test(NAME mzm_audio COMMAND fusion_mzm_audio_tests
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(fusion_mzm_hatches_tests tests/test_mzm_hatches.c src/runtime/mzm_hatches.c
                   src/runtime/mzm_sfx.c src/runtime/mzm_screen_shake.c)
    target_include_directories(fusion_mzm_hatches_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_hatches_tests)
    add_test(NAME mzm_hatches COMMAND fusion_mzm_hatches_tests)

    add_executable(fusion_mzm_items_tests tests/test_mzm_items.c src/runtime/mzm_items.c
                   src/runtime/mzm_message_banner.c src/runtime/mzm_sfx.c src/runtime/mzm_screen_shake.c)
    target_include_directories(fusion_mzm_items_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_items_tests)
    add_test(NAME mzm_items COMMAND fusion_mzm_items_tests)

    add_executable(fusion_mzm_sprites_tests tests/test_mzm_sprites.c src/runtime/mzm_sprites.c
                   src/runtime/mzm_space_pirate.c src/runtime/mzm_parasite.c src/runtime/mzm_atomic.c src/runtime/mzm_polyp.c src/runtime/mzm_tourian.c src/runtime/mzm_worker_robot.c src/runtime/mzm_metroid.c src/runtime/mzm_kraid.c src/runtime/mzm_ridley.c src/runtime/mzm_mother_brain.c src/runtime/mzm_deorem.c src/runtime/mzm_acid_worm.c src/runtime/mzm_gadora.c src/runtime/mzm_imago_larva.c src/runtime/mzm_imago_cocoon.c src/runtime/mzm_mecha_ridley.c src/runtime/mzm_hive.c src/runtime/mzm_baristute.c src/runtime/mzm_tangle_vine.c src/runtime/mzm_ruins_test.c src/runtime/mzm_chozodia_scripted.c src/runtime/mzm_elevator_statue.c src/runtime/mzm_imago.c src/runtime/mzm_escape_ship.c src/runtime/mzm_crocomire.c
                   src/runtime/mzm_chozodia_security.c src/runtime/mzm_chozo_statue.c src/runtime/mzm_item_sprites.c src/runtime/mzm_gadget_sprites.c src/runtime/mzm_gunship.c
                   src/runtime/mzm_effects.c src/runtime/mzm_sfx.c src/runtime/mzm_screen_shake.c
                   src/runtime/mzm_background_effect.c
                   src/runtime/mzm_escape.c
                   src/runtime/mzm_projectiles.c src/runtime/mzm_samus.c
                   src/runtime/mzm_power_bomb.c)
    target_include_directories(fusion_mzm_sprites_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_mzm_sprites_tests)
    add_test(NAME mzm_sprites COMMAND fusion_mzm_sprites_tests)

    # Device-free shared gamepad mapping (SDL3 headers and string tables only).
    add_executable(fusion_aos_menu_preview_tests tests/test_aos_menu_preview.c src/runtime/aos_menu_preview.c)
    target_include_directories(fusion_aos_menu_preview_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_menu_preview_tests)
    add_test(NAME aos_menu_preview COMMAND fusion_aos_menu_preview_tests)

    add_executable(fusion_aos_save_tests tests/test_aos_save.c src/runtime/aos_save.c)
    target_include_directories(fusion_aos_save_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_save_tests)
    add_test(NAME aos_save COMMAND fusion_aos_save_tests
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_executable(fusion_aos_save_room_tests tests/test_aos_save_room.c
                   src/runtime/aos_save_room.c)
    target_include_directories(fusion_aos_save_room_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_save_room_tests)
    add_test(NAME aos_save_room COMMAND fusion_aos_save_room_tests)

    add_executable(fusion_aos_title_stages_tests tests/test_aos_title_stages.c src/runtime/aos_title_stages.c)
    target_include_directories(fusion_aos_title_stages_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_title_stages_tests)
    add_test(NAME aos_title_stages COMMAND fusion_aos_title_stages_tests)

    add_executable(fusion_aos_title_motion_tests tests/test_aos_title_motion.c src/runtime/aos_title_motion.c)
    target_include_directories(fusion_aos_title_motion_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_title_motion_tests)
    add_test(NAME aos_title_motion COMMAND fusion_aos_title_motion_tests)

    add_executable(fusion_aos_title_fade_tests tests/test_aos_title_fade.c src/runtime/aos_title_fade.c)
    target_include_directories(fusion_aos_title_fade_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_title_fade_tests)
    add_test(NAME aos_title_fade COMMAND fusion_aos_title_fade_tests)

    add_executable(fusion_aos_title_input_tests tests/test_aos_title_input.c src/runtime/aos_title_input.c)
    target_include_directories(fusion_aos_title_input_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_title_input_tests)
    add_test(NAME aos_title_input COMMAND fusion_aos_title_input_tests)

    add_executable(fusion_aos_game_mode_tests tests/test_aos_game_mode.c src/runtime/aos_game_mode.c)
    target_include_directories(fusion_aos_game_mode_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_game_mode_tests)
    add_test(NAME aos_game_mode COMMAND fusion_aos_game_mode_tests)

    add_executable(fusion_aos_input_tests tests/test_aos_input.c src/runtime/aos_input.c)
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
                   src/runtime/aos_collision.c)
    target_include_directories(fusion_aos_collision_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_collision_tests)
    add_test(NAME aos_collision COMMAND fusion_aos_collision_tests)

    add_executable(fusion_aos_soma_tests tests/test_aos_soma.c
                   src/runtime/aos_soma.c src/runtime/aos_collision.c src/runtime/aos_combat.c
                   src/runtime/aos_anim.c)
    target_include_directories(fusion_aos_soma_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_soma_tests)
    add_test(NAME aos_soma COMMAND fusion_aos_soma_tests)

    add_executable(fusion_aos_room_tests tests/test_aos_room.c src/runtime/aos_room.c
                   src/runtime/aos_collision.c)
    target_include_directories(fusion_aos_room_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_room_tests)
    add_test(NAME aos_room COMMAND fusion_aos_room_tests)

    add_executable(fusion_aos_door_tests tests/test_aos_door.c src/runtime/aos_door.c
                   src/runtime/aos_soma.c src/runtime/aos_collision.c src/runtime/aos_combat.c src/runtime/aos_anim.c)
    target_include_directories(fusion_aos_door_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_door_tests)
    add_test(NAME aos_door COMMAND fusion_aos_door_tests)

    add_executable(fusion_aos_weapon_tests tests/test_aos_weapon.c src/runtime/aos_weapon.c src/runtime/aos_effect.c
                   src/runtime/aos_soma.c src/runtime/aos_collision.c src/runtime/aos_combat.c src/runtime/aos_anim.c)
    target_include_directories(fusion_aos_weapon_tests PRIVATE src/runtime)
    fusion_enable_warnings(fusion_aos_weapon_tests)
    add_test(NAME aos_weapon COMMAND fusion_aos_weapon_tests)

    add_executable(fusion_aos_enemy_tests tests/test_aos_enemy.c src/runtime/aos_enemy.c
                   src/runtime/aos_weapon.c src/runtime/aos_effect.c
                   src/runtime/aos_soma.c src/runtime/aos_collision.c src/runtime/aos_combat.c src/runtime/aos_anim.c)
    target_include_directories(fusion_aos_enemy_tests PRIVATE src/runtime)
    target_link_libraries(fusion_aos_enemy_tests PRIVATE m)
    fusion_enable_warnings(fusion_aos_enemy_tests)
    add_test(NAME aos_enemy COMMAND fusion_aos_enemy_tests)

    # ROM-free native Zero Mission pose controller regression.
    add_executable(fusion_mzm_samus_tests tests/test_mzm_samus.c src/runtime/mzm_samus.c
                   src/runtime/mzm_projectiles.c src/runtime/mzm_sfx.c src/runtime/mzm_screen_shake.c
                   src/runtime/mzm_power_bomb.c)
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

    add_test(NAME python_tooling_and_data
        COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_SOURCE_DIR}
                python3 -m unittest discover -s tests -v)
    set_tests_properties(python_tooling_and_data PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_test(NAME no_proprietary_files
        COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/check_no_proprietary.py
                ${CMAKE_SOURCE_DIR})
endif()
