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

add_library(fusion_native_editor_core STATIC
    src/core/native_map.c
    src/core/native_selection.c
)
target_include_directories(fusion_native_editor_core PUBLIC include)
fusion_enable_warnings(fusion_native_editor_core)

find_package(SDL3 CONFIG REQUIRED)
add_executable(fusion_mzm_room_viewer src/tools/mzm_room_viewer.c)
target_link_libraries(fusion_mzm_room_viewer PRIVATE SDL3::SDL3)
fusion_enable_warnings(fusion_mzm_room_viewer)

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
    add_executable(fusion_native_map_tests tests/test_native_map.c)
    target_link_libraries(fusion_native_map_tests PRIVATE fusion_native_editor_core)
    fusion_enable_warnings(fusion_native_map_tests)
    add_test(NAME native_map_contract COMMAND fusion_native_map_tests)

    add_executable(fusion_native_selection_tests tests/test_native_selection.c)
    target_link_libraries(fusion_native_selection_tests PRIVATE fusion_native_editor_core)
    fusion_enable_warnings(fusion_native_selection_tests)
    add_test(NAME native_selection_contract COMMAND fusion_native_selection_tests)

    if(GTK4_FOUND)
        add_executable(fusion_native_workspace_gtk_tests
            tests/test_native_workspace_gtk.c
            editor/native_workspace.c
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

    add_test(NAME python_tooling_and_data
        COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_SOURCE_DIR}
                python3 -m unittest discover -s tests -v)
    set_tests_properties(python_tooling_and_data PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_test(NAME no_proprietary_files
        COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/check_no_proprietary.py
                ${CMAKE_SOURCE_DIR})
endif()
