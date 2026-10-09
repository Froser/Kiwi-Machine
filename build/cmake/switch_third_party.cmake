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

set(KIWI_SWITCH_COMPAT_DIR
        "${CMAKE_CURRENT_LIST_DIR}/../../src/kiwi/base/platform/switch")

function(KIWI_SWITCH_PREPARE_GLOG binary_dir)
    # libnx headers expose these APIs, but newlib does not implement them.
    # Supplying Kiwi's implementations avoids glog defining conflicting
    # static replacements in logging.cc.
    set(HAVE_PREAD ON CACHE INTERNAL "" FORCE)
    set(HAVE_PWRITE ON CACHE INTERNAL "" FORCE)
    set(HAVE_PWD_H OFF CACHE INTERNAL "" FORCE)

    # glog searches its generated include directory before its source tree.
    file(MAKE_DIRECTORY "${binary_dir}/glog")
    configure_file(
            "${KIWI_SWITCH_COMPAT_DIR}/glog_platform.h"
            "${binary_dir}/glog/platform.h"
            COPYONLY
    )
endfunction()

function(KIWI_SWITCH_CONFIGURE_GLOG)
    target_include_directories(glog BEFORE PUBLIC
            "$<BUILD_INTERFACE:${glog_BINARY_DIR}>"
    )
    target_include_directories(glog_internal BEFORE PRIVATE
            "${glog_BINARY_DIR}"
    )
    target_compile_options(glog INTERFACE
            "$<$<COMPILE_LANGUAGE:CXX>:-include>"
            "$<$<COMPILE_LANGUAGE:CXX>:${KIWI_SWITCH_COMPAT_DIR}/glog_platform.h>"
    )
    target_sources(glog_internal PRIVATE
            "${KIWI_SWITCH_COMPAT_DIR}/glog_posix_compat.cc"
    )

    # This private compatibility define only disables glog functionality that
    # shells out to /bin/mail and uses unsupported syscall-based writes.
    target_compile_definitions(glog_internal PRIVATE GLOG_OS_EMSCRIPTEN)
endfunction()

function(KIWI_SWITCH_PREPARE_IMGUI)
    set(SDL2_SOURCE_DIR "$ENV{DEVKITPRO}/portlibs/switch" PARENT_SCOPE)
endfunction()

function(KIWI_SWITCH_CONFIGURE_IMGUI)
    target_compile_definitions(imgui
            PRIVATE IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS)
    target_link_libraries(imgui PRIVATE ${KIWI_SDL2_TARGET})
endfunction()
