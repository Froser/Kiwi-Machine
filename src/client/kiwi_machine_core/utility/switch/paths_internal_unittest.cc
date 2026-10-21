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

#include "utility/switch/paths_internal.h"

#include <optional>

#include "gtest/gtest.h"

namespace kiwi::switch_platform::internal {
namespace {

const kiwi::base::FilePath kApplicationDirectory =
    kiwi::base::FilePath::FromUTF8Unsafe("sdmc:/switch/KiwiMachine");

TEST(SwitchPathsInternalTest, KeepsSharedDirectoryWithoutSelectedAccount) {
  EXPECT_EQ(ResolveUserDataDirectory(kApplicationDirectory, std::nullopt),
            kiwi::base::FilePath::FromUTF8Unsafe(
                "sdmc:/switch/KiwiMachine/userdata"));
}

TEST(SwitchPathsInternalTest, IsolatesDirectoryBySelectedAccount) {
  constexpr AccountId kAccountId{0x0123456789abcdefULL, 0xfedcba9876543210ULL};

  EXPECT_EQ(
      ResolveUserDataDirectory(kApplicationDirectory, kAccountId),
      kiwi::base::FilePath::FromUTF8Unsafe("sdmc:/switch/KiwiMachine/userdata/"
                                           "0123456789abcdeffedcba9876543210"));
}

}  // namespace
}  // namespace kiwi::switch_platform::internal
