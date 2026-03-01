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

#ifndef TESTING_ROM_TEST_H_
#define TESTING_ROM_TEST_H_

#include <limits>
#include <memory>

#include "base/files/file_path.h"
#include "base/task/single_thread_task_executor.h"
#include "kiwi/nes/debug/debug_port.h"
#include "kiwi/nes/emulator.h"
#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace kiwi {
namespace nes {
namespace testing {

// Struct to represent test result
struct RomTestResult {
  Byte status;         // Result code from $6000
  std::string output;  // Test output or error message
};

// Base class for ROM tests
// This class assumes the ROM follows the following convention:
// - $6000: Status register
//   - 0x80: Test is running
//   - 0x00-0x7F: Test completed with result code
// - $6004: Text output buffer (null-terminated string)
class RomTest : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  // Load and run a ROM until it completes or times out
  // Returns a RomTestResult with status and output
  // Status codes:
  // - 0x80: Test is running
  // - 0x00-0x7F: Test completed with result code
  // - 0xFF: Error (e.g., ROM not found, test timed out)
  RomTestResult RunRom(const base::FilePath& rom_path,
                       int max_frames = std::numeric_limits<int>::max());

  scoped_refptr<Emulator> emulator_;
  std::unique_ptr<DebugPort> debug_port_;
  std::unique_ptr<base::SingleThreadTaskExecutor> task_executor_;
};

}  // namespace testing
}  // namespace nes
}  // namespace kiwi

#endif  // TESTING_ROM_TEST_H_
