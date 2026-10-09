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

cmake_minimum_required(VERSION 3.13)

if(KIWI_SWITCH)
    include("${CMAKE_CURRENT_LIST_DIR}/build/cmake/switch_environment.cmake")
    KIWI_CHECK_SWITCH_TOOLCHAIN()
endif()

project(KiwiMachine)

OPTION(USE_SDL2 "Use SDL2 as Kiwi's backend." ON)
OPTION(KIWI_NS_NSP "Build a Nintendo Switch NRO forwarder NSP." ON)
set(KIWI_NS_TITLEID "" CACHE STRING
        "16-digit hexadecimal Title ID used by the Switch forwarder NSP")
set(KIWI_NS_PROD_KEYS "$ENV{KIWI_NS_PROD_KEYS}" CACHE FILEPATH
        "Path to prod.keys used only when KIWI_NS_NSP is enabled")
set(KIWI_NS_HACBREWPACK_EXECUTABLE "" CACHE FILEPATH
        "Path to the host hacbrewpack executable")
set(KIWI_NS_NRO_PATH "sdmc:/switch/KiwiMachine/KiwiMachine.nro"
        CACHE STRING "NRO path launched by the Switch forwarder NSP")

if(KIWI_SWITCH OR SWITCH OR CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
    set(KIWI_SWITCH ON)
    if(NOT COMMAND KIWI_CHECK_SWITCH_TOOLCHAIN)
        include("${CMAKE_CURRENT_LIST_DIR}/build/cmake/switch_environment.cmake")
    endif()
    KIWI_CHECK_SWITCH_TOOLCHAIN()

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
    set(KIWI_SWITCH_APP_VERSION "0.1.0" CACHE STRING
            "Version stored in the Switch NACP metadata")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-fexceptions>")

    find_package(PkgConfig QUIET)
    if(NOT PKG_CONFIG_FOUND)
        KIWI_SWITCH_ENVIRONMENT_ERROR(
                "pkg-config is required to locate Switch portlibs.")
    endif()

    add_library(kiwi_switch_platform INTERFACE)
    target_include_directories(kiwi_switch_platform SYSTEM INTERFACE
            "$ENV{DEVKITPRO}/libnx/include"
    )
    target_link_directories(kiwi_switch_platform INTERFACE
            "$ENV{DEVKITPRO}/libnx/lib"
    )
    target_link_libraries(kiwi_switch_platform INTERFACE nx)

    function(KIWI_IMPORT_SWITCH_PACKAGE target prefix package)
        pkg_check_modules(${prefix} QUIET ${package})
        if(NOT ${prefix}_FOUND)
            KIWI_SWITCH_ENVIRONMENT_ERROR(
                    "Missing devkitPro Switch portlib: ${package}")
        endif()
        add_library(${target} INTERFACE)
        target_include_directories(${target} SYSTEM INTERFACE
                ${${prefix}_INCLUDE_DIRS}
        )
        target_compile_options(${target} INTERFACE
                ${${prefix}_CFLAGS_OTHER}
        )
        target_link_directories(${target} INTERFACE
                ${${prefix}_STATIC_LIBRARY_DIRS}
        )
        target_link_libraries(${target} INTERFACE
                ${${prefix}_STATIC_LIBRARIES}
        )
        target_link_options(${target} INTERFACE
                ${${prefix}_STATIC_LDFLAGS_OTHER}
        )
    endfunction()

    KIWI_IMPORT_SWITCH_PACKAGE(kiwi_sdl2 KIWI_SDL2 sdl2)
    KIWI_IMPORT_SWITCH_PACKAGE(kiwi_sdl2_image KIWI_SDL2_IMAGE SDL2_image)
    KIWI_IMPORT_SWITCH_PACKAGE(kiwi_sdl2_mixer KIWI_SDL2_MIXER SDL2_mixer)

    set(KIWI_SDL2_TARGET kiwi_sdl2)
    set(KIWI_SDL2_IMAGE_TARGET kiwi_sdl2_image)
    set(KIWI_SDL2_MIXER_TARGET kiwi_sdl2_mixer)
    set(KIWI_PLATFORM_TARGET kiwi_switch_platform)
    include(build/cmake/switch_third_party.cmake)
else()
    set(KIWI_SDL2_TARGET SDL2-static)
    set(KIWI_SDL2_IMAGE_TARGET SDL2_image)
    set(KIWI_SDL2_MIXER_TARGET SDL2_mixer)
    set(KIWI_PLATFORM_TARGET "")
endif()

include(build/cmake/build_tools.cmake)

add_subdirectory(src)
