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

#include "utility/runtime_util.h"

#include "base/functional/bind.h"
#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"
#include "utility/save_state_util.h"

TEST(RuntimeUtilTest, DetectsGBAExtensionCaseInsensitively) {
  EXPECT_EQ(runtime_util::GetEmulationSystemForROMPath(
                kiwi::base::FilePath::FromUTF8Unsafe("game.gba")),
            kiwi::emu::Emulator::Type::kGBA);
  EXPECT_EQ(runtime_util::GetEmulationSystemForROMPath(
                kiwi::base::FilePath::FromUTF8Unsafe("game.GBA")),
            kiwi::emu::Emulator::Type::kGBA);
}

TEST(RuntimeUtilTest, TreatsOtherExtensionsAsNES) {
  EXPECT_EQ(runtime_util::GetEmulationSystemForROMPath(
                kiwi::base::FilePath::FromUTF8Unsafe("game.nes")),
            kiwi::emu::Emulator::Type::kNES);
  EXPECT_EQ(runtime_util::GetEmulationSystemForROMPath(
                kiwi::base::FilePath::FromUTF8Unsafe("game")),
            kiwi::emu::Emulator::Type::kNES);
}

TEST(SaveStateUtilTest, RejectsOperationsWithoutRuntime) {
  EXPECT_FALSE(save_state_util::CanSaveOrLoadState(nullptr));

  bool save_called = false;
  save_state_util::SaveState(
      nullptr, 2, NESRuntime::GetThumbnailCallback(),
      kiwi::base::BindOnce(
          [](bool* called, int value, bool success) {
            *called = true;
            EXPECT_EQ(value, 2);
            EXPECT_FALSE(success);
          },
          &save_called));
  EXPECT_TRUE(save_called);

  bool load_called = false;
  save_state_util::LoadState(
      nullptr, 3, kiwi::base::OnceClosure(),
      kiwi::base::BindOnce(
          [](bool* called, int value, bool success) {
            *called = true;
            EXPECT_EQ(value, 3);
            EXPECT_FALSE(success);
          },
          &load_called));
  EXPECT_TRUE(load_called);
}
