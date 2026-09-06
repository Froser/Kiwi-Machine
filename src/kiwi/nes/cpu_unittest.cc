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

class CountingDebugPort : public DebugPort {
 public:
  explicit CountingDebugPort(Emulator* emulator) : DebugPort(emulator) {}

  void OnCPUBeforeStep(CPUDebugState&) override { ++before_step_count; }

  void OnCPUStepped(const CPUContext&) override { ++stepped_count; }

  void OnPPUStepped(const PPUContext&) override { ++ppu_stepped_count; }

  int before_step_count = 0;
  int stepped_count = 0;
  int ppu_stepped_count = 0;
};

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

TEST_F(CpuInstructionsTest, NotifiesAttachedDebugPortWhenStepping) {
  bool loaded = false;
  emulator_->LoadFromFile(
      GetRomPath(),
      base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                     &loaded));
  ASSERT_TRUE(loaded);

  auto debug_port = std::make_unique<CountingDebugPort>(emulator_.get());
  CountingDebugPort* debug_port_ptr = debug_port.get();
  emulator_->SetDebugPort(debug_port_ptr);

  emulator_->Step();

  EXPECT_EQ(debug_port_ptr->before_step_count, 1);
  EXPECT_EQ(debug_port_ptr->stepped_count, 1);
  EXPECT_EQ(debug_port_ptr->ppu_stepped_count, 3);
  emulator_->SetDebugPort(nullptr);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
