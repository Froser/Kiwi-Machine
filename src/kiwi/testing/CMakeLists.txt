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

    ../emulator/emulator_unittest.cc
    ../emulator/gba/emulator_unittest.cc
    ../emulator/nes/cpu_unittest.cc
    ../emulator/nes/emulator_nvram_unittest.cc
    ../emulator/nes/testing/rom_test.cc
    ../emulator/nes/mappers/mapper_test_support.cc
    ../emulator/nes/mappers/mapper000_unittest.cc
    ../emulator/nes/mappers/mapper001_unittest.cc
    ../emulator/nes/mappers/mapper002_unittest.cc
    ../emulator/nes/mappers/mapper003_unittest.cc
    ../emulator/nes/mappers/mapper004_unittest.cc
    ../emulator/nes/mappers/mapper005_unittest.cc
    ../emulator/nes/mappers/mapper007_unittest.cc
    ../emulator/nes/mappers/mapper009_unittest.cc
    ../emulator/nes/mappers/mapper010_unittest.cc
    ../emulator/nes/mappers/mapper011_unittest.cc
    ../emulator/nes/mappers/mapper_nvram_compatibility_unittest.cc
    ../emulator/nes/mappers/mapper033_unittest.cc
    ../emulator/nes/mappers/mapper040_unittest.cc
    ../emulator/nes/mappers/mapper048_unittest.cc
    ../emulator/nes/mappers/mapper066_unittest.cc
    ../emulator/nes/mappers/mapper074_unittest.cc
    ../emulator/nes/mappers/mapper075_unittest.cc
    ../emulator/nes/mappers/mapper087_unittest.cc
    ../emulator/nes/mappers/mapper185_unittest.cc
    ../emulator/nes/mappers/mapper013_unittest.cc
    ../emulator/nes/mappers/mapper016_unittest.cc
    ../emulator/nes/mappers/mapper018_unittest.cc
    ../emulator/nes/mappers/mapper019_unittest.cc
    ../emulator/nes/mappers/mapper021_unittest.cc
    ../emulator/nes/mappers/mapper022_unittest.cc
    ../emulator/nes/mappers/mapper023_unittest.cc
    ../emulator/nes/mappers/mapper024_unittest.cc
    ../emulator/nes/mappers/mapper025_unittest.cc
    ../emulator/nes/mappers/mapper032_unittest.cc
    ../emulator/nes/mappers/mapper034_unittest.cc
    ../emulator/nes/mappers/mapper037_unittest.cc
    ../emulator/nes/mappers/mapper038_unittest.cc
    ../emulator/nes/mappers/mapper041_unittest.cc
    ../emulator/nes/mappers/mapper046_unittest.cc
    ../emulator/nes/mappers/mapper047_unittest.cc
    ../emulator/nes/mappers/mapper064_unittest.cc
    ../emulator/nes/mappers/mapper065_unittest.cc
    ../emulator/nes/mappers/mapper067_unittest.cc
    ../emulator/nes/mappers/mapper068_unittest.cc
    ../emulator/nes/mappers/mapper069_unittest.cc
    ../emulator/nes/mappers/mapper070_unittest.cc
    ../emulator/nes/mappers/mapper071_unittest.cc
    ../emulator/nes/mappers/mapper072_unittest.cc
    ../emulator/nes/mappers/mapper073_unittest.cc
    ../emulator/nes/mappers/mapper077_unittest.cc
    ../emulator/nes/mappers/mapper078_unittest.cc
    ../emulator/nes/mappers/mapper079_unittest.cc
    ../emulator/nes/mappers/mapper080_unittest.cc
    ../emulator/nes/mappers/mapper085_unittest.cc
    ../emulator/nes/mappers/mapper086_unittest.cc
    ../emulator/nes/mappers/mapper089_unittest.cc
    ../emulator/nes/mappers/mapper092_unittest.cc
    ../emulator/nes/mappers/mapper093_unittest.cc
    ../emulator/nes/mappers/mapper094_unittest.cc
    ../emulator/nes/mappers/mapper096_unittest.cc
    ../emulator/nes/mappers/mapper097_unittest.cc
    ../emulator/nes/mappers/mapper104_unittest.cc
    ../emulator/nes/mappers/mapper105_unittest.cc
    ../emulator/nes/mappers/mapper112_unittest.cc
    ../emulator/nes/mappers/mapper113_unittest.cc
    ../emulator/nes/mappers/mapper115_unittest.cc
    ../emulator/nes/mappers/mapper118_unittest.cc
    ../emulator/nes/mappers/mapper119_unittest.cc
    ../emulator/nes/mappers/mapper132_unittest.cc
    ../emulator/nes/mappers/mapper133_unittest.cc
    ../emulator/nes/mappers/mapper137_unittest.cc
    ../emulator/nes/mappers/mapper139_unittest.cc
    ../emulator/nes/mappers/mapper140_unittest.cc
    ../emulator/nes/mappers/mapper141_unittest.cc
    ../emulator/nes/mappers/mapper143_unittest.cc
    ../emulator/nes/mappers/mapper146_unittest.cc
    ../emulator/nes/mappers/mapper147_unittest.cc
    ../emulator/nes/mappers/mapper148_unittest.cc
    ../emulator/nes/mappers/mapper150_unittest.cc
    ../emulator/nes/mappers/mapper156_unittest.cc
    ../emulator/nes/mappers/mapper168_unittest.cc
    ../emulator/nes/mappers/mapper176_unittest.cc
    ../emulator/nes/mappers/mapper177_unittest.cc
    ../emulator/nes/mappers/mapper178_unittest.cc
    ../emulator/nes/mappers/mapper180_unittest.cc
    ../emulator/nes/mappers/mapper184_unittest.cc
    ../emulator/nes/mappers/mapper186_unittest.cc
    ../emulator/nes/mappers/mapper188_unittest.cc
    ../emulator/nes/mappers/mapper189_unittest.cc
    ../emulator/nes/mappers/mapper193_unittest.cc
    ../emulator/nes/mappers/mapper206_unittest.cc
    ../emulator/nes/mappers/mapper216_unittest.cc
    ../emulator/nes/mappers/mapper228_unittest.cc
    ../emulator/nes/mappers/mapper232_unittest.cc
    ../emulator/nes/mappers/mapper234_unittest.cc
    ../emulator/nes/mappers/mapper240_unittest.cc
    ../emulator/nes/mappers/mapper241_unittest.cc
    ../emulator/nes/mappers/mapper244_unittest.cc
    ../emulator/nes/mappers/mapper245_unittest.cc
    ../emulator/nes/mappers/mapper246_unittest.cc
    ../emulator/nes/mappers/mapper355_unittest.cc
    ../emulator/nes/mappers/mapper512_unittest.cc
    ../emulator/nes/components/mesen_hd_pack/hires_parser_unittest.cc
    ../emulator/nes/palette_unittest.cc
    ../emulator/nes/ppu_unittest.cc
    ../emulator/nes/ppu_bus_unittest.cc
    ../emulator/nes/rom_hash_unittest.cc
)

if (KIWI_ENABLE_APU_FRAME_BOUNDARY_CLOCK_FIX)
    list(APPEND Sources
        ../emulator/nes/apu_unittest.cc
    )
endif ()

if (KIWI_ENABLE_INES1_TV_SYSTEM_FIX)
    list(APPEND Sources
        ../emulator/nes/cartridge_unittest.cc
    )
endif ()

# Create test executable
add_executable(kiwi_unittests ${Sources})

# Link against required libraries
target_link_libraries(kiwi_unittests PRIVATE
    kiwi_static
    mesen_hd_pack
    gtest_main
    glog::glog
)

# Include directories
target_include_directories(kiwi_unittests PRIVATE
    ../..
    ..
    ${CMAKE_CURRENT_BINARY_DIR}
)
