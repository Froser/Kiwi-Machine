# Copyright (C) 2025 Yisi Yu
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

project(modp_b64 CXX)

set(CMAKE_CXX_STANDARD 20)

set(Sources
        modp_b64.cc
        modp_b64.h
        modp_b64_data.h
)

KIWI_ADD_LIBRARY(${PROJECT_NAME} STATIC ${Sources})

target_include_directories(${PROJECT_NAME}
        PRIVATE
        ../../../
        "${kiwi_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE

        kiwi_static
)
