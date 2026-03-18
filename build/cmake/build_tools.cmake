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

# Set global system variables.
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOSX TRUE)
endif()

function(__make_source_group)
    foreach (_source IN ITEMS ${ARGN})
        if (IS_ABSOLUTE "${_source}")
            file(RELATIVE_PATH _source_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${_source}")
        else ()
            set(_source_rel "${_source}")
        endif ()
        get_filename_component(_source_path "${_source_rel}" PATH)
        string(REPLACE "/" "\\" _source_path_msvc "${_source_path}")
        source_group("${_source_path_msvc}" FILES "${_source}")
    endforeach ()
endfunction()

# Add an executable with source group.
function(KIWI_ADD_EXECUTABLE)
    foreach (_source IN ITEMS ${ARGN})
        __make_source_group(${_source})
    endforeach ()
    add_executable(${ARGV})
endfunction()

# Add a library with source group.
function(KIWI_ADD_LIBRARY)
    foreach (_source IN ITEMS ${ARGN})
        __make_source_group(${_source})
    endforeach ()
    add_library(${ARGV})
endfunction()

macro(KIWI_TARGET_FOLDER target folder)
    if (TARGET ${target})
        set_target_properties(${target} PROPERTIES FOLDER ${folder})
    endif()
endmacro()

if(IOS)
    find_library(IMAGEIO_FRAMEWORK ImageIO)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
    find_library(CORESERVICES_FRAMEWORK CoreServices)
    find_library(UNIFORMTYPEIDENTIFIERS_FRAMEWORK UniformTypeIdentifiers)
    
    if(NOT TARGET ios_frameworks)
        add_library(ios_frameworks INTERFACE)
        target_link_libraries(ios_frameworks INTERFACE
            ${IMAGEIO_FRAMEWORK}
            ${COREGRAPHICS_FRAMEWORK}
            ${CORESERVICES_FRAMEWORK}
        )
        if(UNIFORMTYPEIDENTIFIERS_FRAMEWORK)
            target_link_libraries(ios_frameworks INTERFACE ${UNIFORMTYPEIDENTIFIERS_FRAMEWORK})
        endif()
    endif()
endif()
