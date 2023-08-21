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

project(debugger CXX)

set(CMAKE_CXX_STANDARD 17)

# Added gflags
include_directories(
        ".."
        ${CMAKE_CURRENT_BINARY_DIR}/../third_party/gflags/include
)

list(APPEND
        CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/../../build/cmake"
)

find_package(Kiwi REQUIRED)

set(Sources
        debugger_debug_port.cc
        debugger_debug_port.h
        debugger_main.cc
)

KIWI_ADD_EXECUTABLE(${PROJECT_NAME} ${Sources})
target_link_libraries(${PROJECT_NAME} PRIVATE gflags_nothreads_static)

target_link_libraries(${PROJECT_NAME} PRIVATE Kiwi::kiwi)
target_include_directories(${PROJECT_NAME} PRIVATE Kiwi::kiwi)
target_compile_definitions(${PROJECT_NAME} INTERFACE Kiwi::kiwi)
if (USE_QT6)
    KIWI_USE_QT6(${PROJECT_NAME})
endif ()