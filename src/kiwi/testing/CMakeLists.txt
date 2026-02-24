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

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add test sources
set(Sources
    test_main.cc
)

# Create test executable
add_executable(kiwi_unittests ${Sources})

# Link against required libraries
target_link_libraries(kiwi_unittests PRIVATE
    kiwi_static
    gtest_main
    glog::glog
)

# Include directories
target_include_directories(kiwi_unittests PRIVATE
    ../..
    ..
    ${CMAKE_CURRENT_BINARY_DIR}
)
