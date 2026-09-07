// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/ppu.h"

#include <array>

#include "nes/ppu_bus.h"
#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace kiwi {
namespace nes {
namespace testing {

class CountingPPUObserver : public PPUObserver {
 public:
  void OnPPUStepped() override { ++step_count; }

  int step_count = 0;
};

TEST(PPUTest, NotifiesStepObserverOnlyWhenEnabled) {
  PPUBus bus;
  PPU ppu(&bus);
  CountingPPUObserver observer;
  ppu.SetObserver(&observer);

  ppu.Step();
  EXPECT_EQ(observer.step_count, 0);

  ppu.SetStepObserverEnabled(true);
  ppu.Step();
  EXPECT_EQ(observer.step_count, 1);
}

TEST(PPUTest, DMAWrapsAtEndOfOAM) {
  PPUBus bus;
  PPU ppu(&bus);
  std::array<Byte, 256> source;
  for (std::size_t i = 0; i < source.size(); ++i)
    source[i] = static_cast<Byte>(i);

  ppu.Write(static_cast<Address>(PPURegister::OAMADDR), 0xfc);
  ppu.DMA(source.data());

  EXPECT_EQ(ppu.ReadOAMData(0xfc), 0x00);
  EXPECT_EQ(ppu.ReadOAMData(0xff), 0x03);
  EXPECT_EQ(ppu.ReadOAMData(0x00), 0x04);
  EXPECT_EQ(ppu.ReadOAMData(0xfb), 0xff);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
