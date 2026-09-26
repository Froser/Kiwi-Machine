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

set(KIWI_SWITCH_SETUP_HELP [=[
Run the automated Nintendo Switch Homebrew setup:
  python3 build/setup_switch.py

Setup and build in one command:
  python3 build/setup_switch.py --build

Validate an existing setup without modifying it:
  python3 build/setup_switch.py --check-only

After setup, the regular build entry point remains available:
  python3 build.py switch --build
]=])

function(KIWI_SWITCH_ENVIRONMENT_ERROR reason)
    message(FATAL_ERROR "${reason}\n\n${KIWI_SWITCH_SETUP_HELP}")
endfunction()

function(KIWI_CHECK_SWITCH_TOOLCHAIN)
    if("$ENV{DEVKITPRO}" STREQUAL "")
        KIWI_SWITCH_ENVIRONMENT_ERROR(
                "DEVKITPRO is not set for the native Switch build.")
    endif()

    set(_devkitpro "$ENV{DEVKITPRO}")
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE OR
            "${CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
        KIWI_SWITCH_ENVIRONMENT_ERROR(
                "CMAKE_TOOLCHAIN_FILE is not set for the Switch build.")
    endif()
    if(NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
        KIWI_SWITCH_ENVIRONMENT_ERROR(
                "Switch toolchain not found: ${CMAKE_TOOLCHAIN_FILE}")
    endif()

    foreach(_required_path
            "${_devkitpro}/libnx/include/switch.h"
            "${_devkitpro}/libnx/lib/libnx.a"
            "${_devkitpro}/libnx/switch.specs")
        if(NOT EXISTS "${_required_path}")
            KIWI_SWITCH_ENVIRONMENT_ERROR(
                    "Required Switch toolchain file not found: ${_required_path}")
        endif()
    endforeach()
endfunction()

if(CMAKE_SCRIPT_MODE_FILE AND DEFINED KIWI_SWITCH_FAILURE_REASON)
    KIWI_SWITCH_ENVIRONMENT_ERROR("${KIWI_SWITCH_FAILURE_REASON}")
endif()
