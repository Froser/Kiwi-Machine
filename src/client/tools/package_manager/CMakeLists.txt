# Copyright (C) 2024 Yisi Yu
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

project(package_manager CXX)

set(CMAKE_CXX_STANDARD 20)

list(APPEND
        CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../../build/cmake"
)
find_package(Kiwi REQUIRED)

include_directories(
        .

        # Generation directory
        ${CMAKE_CURRENT_BINARY_DIR}/

        ${CMAKE_CURRENT_BINARY_DIR}/../third_party/gflags/include
)

# Include zlib directory
include_directories(${zlib_BINARY_DIR})

list(APPEND
        CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/cmake"
)

set(Sources
        main.cc
        rom_window.cc
        rom_window.h
        util.cc
        util.h
        workspace.cc
        workspace.h
)

if (APPLE)
    list(APPEND Sources
            util.mm
    )
endif ()
if (WIN32)
    list(APPEND Sources
            util_win.cc
    )
endif()

if (WIN32)
    KIWI_ADD_EXECUTABLE(${PROJECT_NAME} WIN32 ${Sources})
else ()
    KIWI_ADD_EXECUTABLE(${PROJECT_NAME} ${Sources})
endif ()

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        Kiwi::kiwi_static
        gflags_nothreads_static
        imgui
        SDL2-static
        SDL2_image
        minizip
        libjpeg
)

target_include_directories(${PROJECT_NAME} PRIVATE Kiwi::kiwi_static)

target_include_directories(${PROJECT_NAME}
        PRIVATE
        "../../../third_party/imgui"
)

add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ARGS "${${PROJECT_NAME}_SOURCE_DIR}/db.json" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/db.json"
        VERBATIM
)