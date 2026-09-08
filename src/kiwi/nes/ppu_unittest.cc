// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/ppu.h"

#include <array>
#include <vector>

#include "nes/mappers/mapper_test_support.h"
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

class FramePPUObserver : public PPUObserver {
 public:
  void OnRenderReady(const Colors& frame) override {
    sampled_pixels.push_back(frame[16]);
    sampled_second_row_pixels.push_back(frame[256 + 16]);
  }

  std::vector<Color> sampled_pixels;
  std::vector<Color> sampled_second_row_pixels;
};

class PPURenderingTest : public MapperTest {};

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

TEST_F(PPURenderingTest, MasksConfiguredTopOverscanLine) {
  auto cartridge = LoadMapper(0, 2, 0);
  ASSERT_TRUE(cartridge);

  PPUBus bus;
  bus.SetMapper(cartridge->mapper());
  cartridge->mapper()->WriteCHR(0x0000, 0xff);
  cartridge->mapper()->WriteCHR(0x0001, 0xff);

  auto render_top_rows = [&bus](uint32_t crc) {
    PPU ppu(&bus);
    FramePPUObserver observer;
    ppu.SetPatch(crc);
    ppu.SetObserver(&observer);
    ppu.Write(static_cast<Address>(PPURegister::PPUMASK), 0x0a);

    constexpr int kMaxSteps = 262 * 341;
    for (int step = 0; step < kMaxSteps && observer.sampled_pixels.empty();
         ++step) {
      ppu.Step();
    }

    EXPECT_EQ(observer.sampled_pixels.size(), 1u);
    EXPECT_EQ(observer.sampled_second_row_pixels.size(), 1u);
    if (observer.sampled_pixels.empty() ||
        observer.sampled_second_row_pixels.empty()) {
      return std::array<Color, 2>{};
    }
    return std::array<Color, 2>{observer.sampled_pixels[0],
                                observer.sampled_second_row_pixels[0]};
  };

  const auto regular_pixels = render_top_rows(0);
  const auto patched_pixels = render_top_rows(0x2e1e7fd8);
  EXPECT_EQ(regular_pixels[0], regular_pixels[1]);
  EXPECT_NE(patched_pixels[0], patched_pixels[1]);
  EXPECT_EQ(patched_pixels[1], regular_pixels[1]);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
