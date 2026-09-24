// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/emulator.h"

#include <algorithm>
#include <array>
#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_executor.h"
#include "nes/mappers/mapper_test_support.h"
#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t kINESHeaderSize = 0x10;
constexpr size_t k32K = 0x8000;

Bytes MakeAPUStatusPollingROM() {
  Bytes rom = MakeTestROM(0, 2, 1);
  constexpr std::array<Byte, 7> kProgram = {
      0x78,              // SEI
      0xad, 0x15, 0x40,  // LDA $4015
      0x4c, 0x01, 0x80,  // JMP $8001
  };
  std::copy(kProgram.begin(), kProgram.end(), rom.begin() + kINESHeaderSize);

  constexpr size_t kVectorOffset = kINESHeaderSize + k32K - 6;
  for (size_t offset = 0; offset < 6; offset += 2) {
    rom[kVectorOffset + offset] = 0x00;
    rom[kVectorOffset + offset + 1] = 0x80;
  }
  return rom;
}

}  // namespace

TEST(APUFrameBoundaryTest, StatusReadKeepsClockMonotonicAcrossPPUFrameEnd) {
  auto task_executor = std::make_unique<base::SingleThreadTaskExecutor>();
  scoped_refptr<Emulator> emulator = CreateEmulatorForTesting();
  emulator->PowerOn();

  bool loaded = false;
  emulator->LoadFromBinary(
      MakeAPUStatusPollingROM(),
      base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                     &loaded));
  ASSERT_TRUE(loaded);

  for (int cycle = 0; cycle < 200000; ++cycle)
    emulator->Step();

  emulator->PowerOff();
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
