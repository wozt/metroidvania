# SPDX-License-Identifier: GPL-3.0-only
# Functional contract for the private native-map CLI adapter.
if(NOT DEFINED TOOL OR NOT DEFINED WORK)
    message(FATAL_ERROR "TOOL and WORK are required")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(INPUT "${WORK}/input.mvnative")
set(OUTPUT "${WORK}/output.mvnative")
file(WRITE "${INPUT}"
"MVNATIVE 1\nROOM mzm:test:001\nTILESET 1\nTILES 4\nATLAS rooms/metroid/tilesets/1_atlas.bmp\nLAYER BG1 2 2\n0000 0001\n0001 0000\nLAYER BG2 2 2\n0000 0000\n0000 0000\nEND\n")

execute_process(
    COMMAND "${TOOL}" --command=get --input=${INPUT} --layer=bg1 --x=1 --y=0
    RESULT_VARIABLE GET_RESULT OUTPUT_VARIABLE GET_OUTPUT ERROR_VARIABLE GET_ERROR)
if(NOT GET_RESULT EQUAL 0 OR NOT GET_OUTPUT STREQUAL "VALUE\t1\n")
    message(FATAL_ERROR "native get failed: ${GET_RESULT}: ${GET_ERROR}${GET_OUTPUT}")
endif()

execute_process(
    COMMAND "${TOOL}" --command=set --input=${INPUT} --output=${OUTPUT}
            --layer=bg1 --x=0 --y=0 --tile=3 --dry-run=false
    RESULT_VARIABLE SET_RESULT OUTPUT_VARIABLE SET_OUTPUT ERROR_VARIABLE SET_ERROR)
if(NOT SET_RESULT EQUAL 0 OR NOT SET_OUTPUT STREQUAL "CHANGED\t1\n" OR
   NOT EXISTS "${OUTPUT}")
    message(FATAL_ERROR "native set failed: ${SET_RESULT}: ${SET_ERROR}${SET_OUTPUT}")
endif()

execute_process(
    COMMAND "${TOOL}" --command=get --input=${OUTPUT} --layer=bg1 --x=0 --y=0
    RESULT_VARIABLE CHECK_RESULT OUTPUT_VARIABLE CHECK_OUTPUT ERROR_VARIABLE CHECK_ERROR)
if(NOT CHECK_RESULT EQUAL 0 OR NOT CHECK_OUTPUT STREQUAL "VALUE\t3\n")
    message(FATAL_ERROR "native round trip failed: ${CHECK_RESULT}: ${CHECK_ERROR}${CHECK_OUTPUT}")
endif()

execute_process(
    COMMAND "${TOOL}" --command=fill --input=${INPUT} --output=${OUTPUT}
            --layer=bg1 --x=9 --y=0 --tile=2 --dry-run=false
    RESULT_VARIABLE BOUNDS_RESULT OUTPUT_VARIABLE BOUNDS_OUTPUT ERROR_VARIABLE BOUNDS_ERROR)
if(BOUNDS_RESULT EQUAL 0)
    message(FATAL_ERROR "out-of-bounds fill was accepted: ${BOUNDS_OUTPUT}${BOUNDS_ERROR}")
endif()
