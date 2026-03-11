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

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add test sources
set(Sources
    test_main.cc
    ${kiwi_machine_core_SOURCE_DIR}/utility/algorithm_unittest.cc
    ${kiwi_machine_core_SOURCE_DIR}/utility/math_unittest.cc
)

# Create test executable
add_executable(kiwi_machine_core_unittests ${Sources})

# Link against required libraries
target_link_libraries(kiwi_machine_core_unittests PRIVATE
    kiwi_machine_core
    gtest_main
    glog::glog
    SDL2-static
    SDL2_image
    SDL2_mixer
)

# Include directories
target_include_directories(kiwi_machine_core_unittests PRIVATE
    ../../../
    ${kiwi_machine_core_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SDL2_SOURCE_DIR}/include
    ../../../../src/third_party/imgui
    ${kiwi_SOURCE_DIR}
    ../../../../include
)
