// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include "kiwi/testing/rom_test.h"

namespace kiwi {
namespace nes {
namespace testing {

// Test for all_instrs.nes (tests all CPU instructions)
class CpuInstructionsTest : public RomTest {
 protected:
  base::FilePath GetRomPath() {
    // Get the directory of the current source file
    base::FilePath current_file(__FILE__);
    base::FilePath current_dir = current_file.DirName();

    // Construct the path to all_instrs.nes relative to the current source file
    // Current file: src/kiwi/nes/cpu_unittest.cc
    // Target ROM: src/kiwi/testing/roms/cpu/all_instrs.nes
    return current_dir
        .Append("../")  // Go up to src/kiwi
        .Append("testing")
        .Append("roms")
        .Append("cpu")
        .Append("all_instrs.nes");
  }
};

TEST_F(CpuInstructionsTest, RunAllInstructions) {
  RomTestResult result = RunRom(GetRomPath());
  // Print result and output regardless of success or failure
  std::cout << "CPU instruction test result: " << std::hex
            << static_cast<int>(result.status) << std::endl;
  if (!result.output.empty()) {
    std::cout << "Test output:\n" << result.output << std::endl;
  }
  EXPECT_EQ(result.status, 0x00);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
