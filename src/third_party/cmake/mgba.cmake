# Copyright (C) 2026 Yisi Yu
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

function(KIWI_ADD_MGBA mgba_source_dir)
    # mGBA 0.10.5 uses CMake's pre-3.5 policy baseline.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(LIBMGBA_ONLY ON)
    set(SKIP_GIT ON)
    set(M_CORE_GBA ON)
    set(M_CORE_GB ON)

    # Keep the vendored mGBA project core-only. These function-scoped values
    # override stale cache entries without affecting Kiwi's other targets.
    set(DISABLE_FRONTENDS ON)
    set(DISABLE_DEPS ON)
    set(BUILD_QT OFF)
    set(BUILD_SDL OFF)
    set(BUILD_LIBRETRO OFF)
    set(BUILD_OPENEMU OFF)
    set(BUILD_PYTHON OFF)
    set(BUILD_EXAMPLE OFF)
    set(BUILD_UPDATER OFF)
    set(BUILD_PERF OFF)
    set(BUILD_TEST OFF)
    set(BUILD_SUITE OFF)
    set(BUILD_CINEMA OFF)
    set(BUILD_ROM_TEST OFF)
    set(BUILD_DOCGEN OFF)
    set(ENABLE_SCRIPTING OFF)
    set(USE_DEBUGGERS OFF)
    set(USE_GDB_STUB OFF)
    set(USE_EDITLINE OFF)

    # ios-cmake uses static-library try_compile checks, which incorrectly
    # report popcount32 as available without linking. Keep mGBA's inline
    # fallback enabled on iOS.
    if(IOS)
        set(HAVE_POPCOUNT32 OFF)
    endif()

    add_subdirectory(
            "${mgba_source_dir}"
            "${CMAKE_CURRENT_BINARY_DIR}/mgba-0.10.5"
    )
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
                "${mgba_source_dir}/src/util/vfs/vfs-fd.c"
                "${mgba_source_dir}/src/util/vfs/vfs-dirent.c"
        )
    endif()

    # The vendored source has no .git directory. Prevent its version generator
    # from finding Kiwi's repository metadata in a parent directory.
    get_filename_component(mgba_parent_dir "${mgba_source_dir}" DIRECTORY)
    set_property(DIRECTORY "${mgba_source_dir}"
            PROPERTY RULE_LAUNCH_CUSTOM
            "${CMAKE_COMMAND} -E env GIT_CEILING_DIRECTORIES=${mgba_parent_dir}")
endfunction()
