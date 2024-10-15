#===============================================================================
#
# Sample Tonclib GBA project based on https://github.com/exelotl/libtonc-template
#
# Copyright (C) 2021-2022 gba-toolchain contributors
# For conditions of distribution and use, see copyright notice in LICENSE.md
#
#===============================================================================

set(USE_OPTIMIZED_SCALER ON CACHE BOOL "")

include(commands.cmake)

cmake_minimum_required(VERSION 3.18)

project(AREN22 C CXX)

gba_add_library_subdirectory(rom tonc)

python_generate_file("make-luts.py" "${CMAKE_CURRENT_BINARY_DIR}/luts.cpp")
python_generate_file("make-scalers.py" "${CMAKE_CURRENT_BINARY_DIR}/scalers_2col.cpp")

set(PAL assets/forest.pal)
jasc_to_gba_palette("${PAL}" "${CMAKE_CURRENT_BINARY_DIR}/pal_forest.h")
tiled_to_h(forest.tmj "${CMAKE_CURRENT_BINARY_DIR}/map_forest.h" map_forest)
sprite_compiler("assets/BugBearSheet-decimated.png"
                "${PAL}"
                "${CMAKE_CURRENT_BINARY_DIR}/spr_bugbear.h"
                spr_bugbear
                --frames 2)
sprite_compiler("assets/SpiderSheet-8col.png"
                "${PAL}"
                "${CMAKE_CURRENT_BINARY_DIR}/spr_spider.h"
                spr_spider
                --frames 2)
texture_compiler("assets/FNTSY_TX_SET.png"
        "${PAL}"
        "${CMAKE_CURRENT_BINARY_DIR}/tea_fantasy.h"
        tea_fantasy
        )
#image_compiler("assets/crossbow.png"
#        "${PAL}"
#        "${CMAKE_CURRENT_BINARY_DIR}/img_crossbow.h"
#        img_crossbow
#        --frames 3
#        )

add_executable(helloTonc
        benchmark.cpp
        dbgmenu.cpp
#        draw.cpp
        ent.cpp
        main.cpp
        raycast.cpp
        mgba/mgba.c

        # These must be generated manually until Grit is integrated into build system
        pregenerated/crossbow.c

        "${CMAKE_CURRENT_BINARY_DIR}/luts.cpp"

        # Unavoidable to list all the generated files here?
        # (if we don't, they won't be built before .cpp -- at least initially)
#        "${CMAKE_CURRENT_BINARY_DIR}/img_crossbow.h"
        "${CMAKE_CURRENT_BINARY_DIR}/spr_bugbear.h"
        "${CMAKE_CURRENT_BINARY_DIR}/spr_spider.h"
        "${CMAKE_CURRENT_BINARY_DIR}/map_forest.h"
        "${CMAKE_CURRENT_BINARY_DIR}/pal_forest.h"
        "${CMAKE_CURRENT_BINARY_DIR}/tea_fantasy.h"
        )

if (USE_OPTIMIZED_SCALER)
    target_sources(helloTonc PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/scalers_2col.cpp")
else()
    target_sources(helloTonc PRIVATE kernels/scalers_2col.cpp)
endif()

set_target_properties(helloTonc PROPERTIES SUFFIX ".elf")

target_compile_options(helloTonc PRIVATE -mthumb -Wall)
target_include_directories(helloTonc PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_LIST_DIR}")

if (USE_DEVKITARM)
    target_compile_definitions(helloTonc PRIVATE -DHAVE_LIBSYSBASE)
endif()

target_link_libraries(helloTonc PRIVATE rom tonc)

target_link_options(helloTonc PUBLIC LINKER:-Map=helloTonc.map)

gba_target_objcopy(helloTonc FIX_HEADER)

if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.20.0")
    set(EXECUTABLE_PATH $<TARGET_FILE:helloTonc>)
    cmake_path(REPLACE_EXTENSION EXECUTABLE_PATH ".s" OUTPUT_VARIABLE DISASSEMBLY_PATH)
    cmake_path(REPLACE_EXTENSION EXECUTABLE_PATH ".lst" OUTPUT_VARIABLE LISTING_PATH)
else()
    set(DISASSEMBLY_PATH "helloTonc.s")
    set(LISTING_PATH "helloTonc.lst")
endif()

add_custom_command(
    TARGET helloTonc POST_BUILD
    COMMAND
        ${CMAKE_OBJDUMP} $<TARGET_FILE:helloTonc> --all-headers --disassemble > ${DISASSEMBLY_PATH}
# TODO: find_program
#    COMMAND
#        size --format=sysv --radix=16 $<TARGET_FILE:helloTonc> > ${LISTING_PATH}
    COMMAND
        ${CMAKE_NM} --demangle --format=bsd --print-size --size-sort --reverse-sort $<TARGET_FILE:helloTonc> > ${LISTING_PATH}
    COMMENT
        "Disassembling executable")
