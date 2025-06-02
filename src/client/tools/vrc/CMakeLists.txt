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

project(vrc CXX)

set(CMAKE_CXX_STANDARD 20)

include_directories(
        .
        ../../

        # kiwi
        ${kiwi_SOURCE_DIR}

        # Generation directory
        ${CMAKE_CURRENT_BINARY_DIR}/
)

set(Sources
        main.cc
)


KIWI_ADD_EXECUTABLE(${PROJECT_NAME} ${Sources})

target_link_libraries(${PROJECT_NAME} PRIVATE kiwi_static minizip)