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

#include "base/files/file_util.h"

namespace kiwi {
namespace nes {
namespace testing {

void RomTest::SetUp() {
  // Initialize task executor
  task_executor_ = std::make_unique<base::SingleThreadTaskExecutor>();

  emulator_ = CreateEmulatorForTesting();
  ASSERT_TRUE(emulator_) << "Failed to create emulator";
  emulator_->PowerOn();
  debug_port_ = std::make_unique<DebugPort>(emulator_.get());
}

void RomTest::TearDown() {
  emulator_->PowerOff();
}

RomTestResult RomTest::RunRom(const base::FilePath& rom_path, int max_frames) {
  // Check if ROM exists
  if (!base::PathExists(rom_path)) {
    std::string error_message = "Test ROM not found at " + rom_path.value();
    return {0xFF, error_message};
  }

  // Load ROM from file
  emulator_->LoadFromFile(rom_path, base::BindOnce([](bool success) {
                            EXPECT_TRUE(success) << "Failed to load ROM";
                          }));

  // Run the emulator
  emulator_->Run();

  bool test_is_running = false;

  // Run until the test completes or times out
  for (int i = 0; i < max_frames; ++i) {
    emulator_->RunOneFrame();

    // Check if the test is complete (result code is 0x00-0x7F)
    Byte status = debug_port_->CPUReadByte(0x6000);

    // $80 means the test is running
    if (status == 0x80) {
      test_is_running = true;
    } else if (status < 0x80 && test_is_running) {
      // Read text output from $6004 until zero-byte terminator
      std::string output;
      Address addr = 0x6004;
      Byte byte;
      while ((byte = debug_port_->CPUReadByte(addr)) != 0) {
        output += static_cast<char>(byte);
        addr++;
      }

      return {status, output};
    }
  }

  // If we timed out, return 0xFF as an error
  std::string error_message = "Test timed out";
  return {0xFF, error_message};
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
