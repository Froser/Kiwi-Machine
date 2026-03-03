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

# Find Python3 for auto_package
find_package(Python3 COMPONENTS Interpreter)

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

if (WIN32)
    list(APPEND Sources
            util_win.cc
    )
endif()
if (UNIX)
    if (APPLE)
        list(APPEND Sources
                util_mac.mm
        )
    else()
        list(APPEND Sources
                util_posix.cc
        )
    endif ()
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
        libminizipstatic
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

# Auto package target
if (APPLE)
    add_custom_target(auto_package
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/auto_package.py" "$<TARGET_FILE:${PROJECT_NAME}>" "$<TARGET_BUNDLE_DIR:kiwi_machine>/Contents/Resources"
            DEPENDS ${PROJECT_NAME} kiwi_machine
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Running auto_package.py to package workspace..."
            VERBATIM
    )
else ()
    add_custom_target(auto_package
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/auto_package.py" "$<TARGET_FILE:${PROJECT_NAME}>" "$<TARGET_FILE_DIR:kiwi_machine>"
            DEPENDS ${PROJECT_NAME} kiwi_machine
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Running auto_package.py to package workspace..."
            VERBATIM
    )
endif ()