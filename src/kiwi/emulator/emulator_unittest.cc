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

#include "emulator/emulator.h"

#include <type_traits>

#include "emulator/gba/emulator.h"
#include "emulator/gba/io_devices.h"
#include "emulator/nes/emulator.h"
#include "emulator/nes/io_devices.h"
#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace kiwi {
namespace emu {
namespace testing {

static_assert(std::is_same_v<Bytes, nes::Bytes>);
static_assert(std::is_same_v<Bytes, gba::Bytes>);
static_assert(std::is_same_v<Colors, nes::Colors>);
static_assert(std::is_same_v<Colors, gba::Colors>);
static_assert(std::is_same_v<Sample, nes::Sample>);
static_assert(std::is_same_v<Sample, gba::Sample>);
static_assert(std::is_same_v<ControllerButton, nes::ControllerButton>);
static_assert(std::is_same_v<ControllerButton, gba::ControllerButton>);
static_assert(
    std::is_same_v<Emulator::LoadCallback, nes::Emulator::LoadCallback>);
static_assert(
    std::is_same_v<Emulator::LoadCallback, gba::Emulator::LoadCallback>);
static_assert(std::is_same_v<Emulator::SaveStateCallback,
                             nes::Emulator::SaveStateCallback>);
static_assert(std::is_same_v<Emulator::SaveStateCallback,
                             gba::Emulator::SaveStateCallback>);
static_assert(std::is_base_of_v<RenderDevice, nes::RenderDevice>);
static_assert(std::is_base_of_v<RenderDevice, gba::RenderDevice>);
static_assert(std::is_base_of_v<AudioDevice, nes::AudioDevice>);
static_assert(std::is_base_of_v<AudioDevice, gba::AudioDevice>);
static_assert(std::is_base_of_v<InputDevice, nes::IODevices::InputDevice>);
static_assert(std::is_same_v<InputDevice, gba::IODevices::InputDevice>);
static_assert(std::is_same_v<nes::IODevices::RenderDevice, nes::RenderDevice>);
static_assert(std::is_same_v<gba::IODevices::RenderDevice, gba::RenderDevice>);
static_assert(std::is_same_v<nes::IODevices::AudioDevice, nes::AudioDevice>);
static_assert(std::is_same_v<gba::IODevices::AudioDevice, gba::AudioDevice>);
static_assert(
    std::is_same_v<gba::IODevices::LuminanceDevice, gba::LuminanceDevice>);
static_assert(std::is_same_v<gba::IODevices::MotionDevice, gba::MotionDevice>);
static_assert(std::is_same_v<gba::IODevices::RTCDevice, gba::RTCDevice>);
static_assert(std::is_same_v<gba::IODevices::RumbleDevice, gba::RumbleDevice>);
static_assert(std::is_same_v<nes::IODevices::RenderDevice::Buffer, Colors>);
static_assert(std::is_same_v<gba::IODevices::RenderDevice::Buffer, Colors>);

TEST(CommonEmulatorTest, IdentifiesAndCastsNESEmulator) {
  scoped_refptr<nes::Emulator> nes_emulator = nes::CreateEmulatorForTesting();
  scoped_refptr<Emulator> emulator = nes_emulator;

  EXPECT_EQ(emulator->GetType(), Emulator::Type::kNES);
  EXPECT_EQ(emulator->AsNESEmulator(), nes_emulator.get());

  const Emulator* const_emulator = emulator.get();
  EXPECT_EQ(const_emulator->AsNESEmulator(), nes_emulator.get());
}

TEST(CommonEmulatorTest, IdentifiesAndCastsGBAEmulator) {
  scoped_refptr<gba::Emulator> gba_emulator = gba::CreateEmulatorForTesting();
  scoped_refptr<Emulator> emulator = gba_emulator;

  EXPECT_EQ(emulator->GetType(), Emulator::Type::kGBA);
  EXPECT_EQ(emulator->AsGBAEmulator(), gba_emulator.get());

  const Emulator* const_emulator = emulator.get();
  EXPECT_EQ(const_emulator->AsGBAEmulator(), gba_emulator.get());
}

#if GTEST_HAS_DEATH_TEST
TEST(CommonEmulatorTest, RejectsMismatchedCasts) {
  scoped_refptr<Emulator> nes_emulator = nes::CreateEmulatorForTesting();
  scoped_refptr<Emulator> gba_emulator = gba::CreateEmulatorForTesting();

  EXPECT_DEATH(nes_emulator->AsGBAEmulator(), "GetType");
  EXPECT_DEATH(gba_emulator->AsNESEmulator(), "GetType");
}
#endif

}  // namespace testing
}  // namespace emu
}  // namespace kiwi
