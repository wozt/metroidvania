cmake_minimum_required(VERSION 3.25)
project(metroidvania_fusion VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(FUSION_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

add_library(fusion_core STATIC
    src/core/session.c
    src/core/save.c
    src/core/sha1.c
    src/core/rom.c
    src/backends/castlevania/aria_runtime.c
)
target_include_directories(fusion_core PUBLIC include)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(fusion_core PRIVATE -Wall -Wextra -Wpedantic)
    if(FUSION_WARNINGS_AS_ERRORS)
        target_compile_options(fusion_core PRIVATE -Werror)
    endif()
endif()

find_package(SDL3 CONFIG REQUIRED)

add_executable(fusion_dev
    src/main.c
    src/core/room_sim.c
    src/backends/metroid/metroid_backend.c
    src/backends/castlevania/castlevania_backend.c
    src/backends/castlevania/aria_sdl3_video.c
)
target_include_directories(fusion_dev PRIVATE include src)
target_link_libraries(fusion_dev PRIVATE fusion_core SDL3::SDL3)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(fusion_dev PRIVATE -Wall -Wextra -Wpedantic)
    if(FUSION_WARNINGS_AS_ERRORS)
        target_compile_options(fusion_dev PRIVATE -Werror)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(fusion_aria_runtime_tests tests/test_aria_runtime.c)
    target_link_libraries(fusion_aria_runtime_tests PRIVATE fusion_core)
    add_test(NAME aria_runtime_contract COMMAND fusion_aria_runtime_tests)
    add_executable(fusion_aria_sdl3_video_tests
        tests/test_aria_sdl3_video.c
        src/backends/castlevania/aria_sdl3_video.c)
    target_include_directories(fusion_aria_sdl3_video_tests PRIVATE include)
    target_link_libraries(fusion_aria_sdl3_video_tests PRIVATE SDL3::SDL3)
    target_compile_options(fusion_aria_sdl3_video_tests PRIVATE -Wall -Wextra -Wpedantic)
    if(FUSION_WARNINGS_AS_ERRORS)
        target_compile_options(fusion_aria_sdl3_video_tests PRIVATE -Werror)
    endif()
    add_test(NAME aria_sdl3_video COMMAND fusion_aria_sdl3_video_tests)
    add_executable(fusion_aria_pipeline_tests
        tests/test_aria_pipeline.c
        src/backends/castlevania/aria_sdl3_video.c)
    target_include_directories(fusion_aria_pipeline_tests PRIVATE include)
    target_link_libraries(fusion_aria_pipeline_tests PRIVATE fusion_core SDL3::SDL3)
    target_compile_options(fusion_aria_pipeline_tests PRIVATE -Wall -Wextra -Wpedantic)
    if(FUSION_WARNINGS_AS_ERRORS)
        target_compile_options(fusion_aria_pipeline_tests PRIVATE -Werror)
    endif()
    add_test(NAME aria_runtime_sdl3_pipeline COMMAND fusion_aria_pipeline_tests)
    set_tests_properties(aria_runtime_sdl3_pipeline PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")
    add_executable(fusion_core_tests tests/test_core.c)
    target_link_libraries(fusion_core_tests PRIVATE fusion_core)
    target_compile_options(fusion_core_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME fusion_core COMMAND fusion_core_tests)
    add_executable(fusion_room_tests tests/test_room.c src/core/room_sim.c)
    target_include_directories(fusion_room_tests PRIVATE include src)
    target_link_libraries(fusion_room_tests PRIVATE fusion_core SDL3::SDL3)
    target_compile_options(fusion_room_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME fusion_room_collisions COMMAND fusion_room_tests)
    add_test(NAME verify_roms_python
        COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_SOURCE_DIR}
                python3 -m unittest tests.test_verify_roms)
    add_test(NAME no_proprietary_files
        COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/check_no_proprietary.py
                ${CMAKE_SOURCE_DIR})
endif()
