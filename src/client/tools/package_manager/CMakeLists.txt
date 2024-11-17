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

set(CMAKE_CXX_STANDARD 17)

include_directories(
        .

        # Generation directory
        ${CMAKE_CURRENT_BINARY_DIR}/
)

# Include zlib directory
include_directories(${zlib_BINARY_DIR})

list(APPEND
        CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/cmake"
)

set(Sources
        main.cc
)


if (WIN32)
    KIWI_ADD_EXECUTABLE(${PROJECT_NAME} WIN32 ${Sources})
else ()
    KIWI_ADD_EXECUTABLE(${PROJECT_NAME} ${Sources})
endif ()

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        imgui
        gflags_nothreads_static
        SDL2_image
        minizip
)
target_include_directories(${PROJECT_NAME}
        PRIVATE
        "../../third_party/imgui"
)
