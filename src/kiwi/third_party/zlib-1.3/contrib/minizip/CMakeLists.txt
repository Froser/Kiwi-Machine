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

project(minizip)

set(SOURCES
        ioapi.c
        ioapi.h
        unzip.c
        unzip.h
        zip.c
        zip.h
)

add_library(${PROJECT_NAME} STATIC ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE zlibstatic)
target_include_directories(${PROJECT_NAME} PUBLIC ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})