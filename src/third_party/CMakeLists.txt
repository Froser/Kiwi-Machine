# Copyright (C) 2023 Yisi Yu
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

function(KIWI_ADD_MGBA)
    # mGBA 0.10.5 uses CMake's pre-3.5 policy baseline.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(LIBMGBA_ONLY ON)
    set(SKIP_GIT ON)
    set(M_CORE_GBA ON)
    set(M_CORE_GB ON)

    # ios-cmake uses static-library try_compile checks, which incorrectly
    # report popcount32 as available without linking. Keep mGBA's inline
    # fallback enabled on iOS.
    if(IOS)
        set(HAVE_POPCOUNT32 OFF)
    endif()
    add_subdirectory(mgba-0.10.5)
endfunction()

KIWI_ADD_MGBA()
add_library(mGBA::core ALIAS mgba)

# Reuse Kiwi's zlib implementation instead of exporting mGBA's fallback
# crc32 symbol into the same final binary.
target_compile_definitions(mgba PRIVATE HAVE_CRC32)
target_link_libraries(mgba zlibstatic)

if(EMSCRIPTEN OR KIWI_SWITCH)
    target_compile_definitions(mgba PUBLIC _GNU_SOURCE)
endif()

if(KIWI_SWITCH)
    target_sources(mgba PRIVATE
            mgba-0.10.5/src/util/vfs/vfs-fd.c
            mgba-0.10.5/src/util/vfs/vfs-dirent.c
    )
endif()

# The vendored source has no .git directory. Prevent its version generator
# from finding Kiwi's repository metadata in a parent directory.
set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/mgba-0.10.5"
        PROPERTY RULE_LAUNCH_CUSTOM
        "${CMAKE_COMMAND} -E env GIT_CEILING_DIRECTORIES=${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(gflags)
if(KIWI_SWITCH)
    KIWI_SWITCH_PREPARE_IMGUI()
endif()
add_subdirectory(imgui)
if(KIWI_SWITCH)
    KIWI_SWITCH_CONFIGURE_IMGUI()
endif()
if(NOT KIWI_SWITCH)
    add_subdirectory(SDL_image-release-2.6.3)
    add_subdirectory(SDL_mixer-release-2.6.3)
    add_subdirectory(libjpeg-turbo-jpeg-9f)
    add_subdirectory(googletest-release-1.12.1 "googletest")
endif()

# Link Apple frameworks to SDL2_image for iOS
if(IOS AND TARGET ios_frameworks)
    target_link_libraries(SDL2_image PRIVATE ios_frameworks)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_target_properties(mgba mgba-version-info PROPERTIES FOLDER "3rdparty/mgba")
set_target_properties(gflags_nothreads_static PROPERTIES FOLDER "3rdparty/gflags")
set_target_properties(imgui PROPERTIES FOLDER "3rdparty/imgui")
if(NOT KIWI_SWITCH)
    set_target_properties(SDL2_image PROPERTIES FOLDER "3rdparty/sdl2")
    set_target_properties(SDL2_mixer PROPERTIES FOLDER "3rdparty/sdl2")
    set_target_properties(gtest PROPERTIES FOLDER "3rdparty/googletest-release-1.12.1")
    set_target_properties(gtest_main PROPERTIES FOLDER "3rdparty/googletest-release-1.12.1")
    set_target_properties(gmock PROPERTIES FOLDER "3rdparty/googletest-release-1.12.1")
    set_target_properties(gmock_main PROPERTIES FOLDER "3rdparty/googletest-release-1.12.1")
    set_target_properties(libjpeg PROPERTIES FOLDER "3rdparty/libjpeg-turbo-jpeg-9f")
endif()
