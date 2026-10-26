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

#include "utility/localization.h"

#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace {

class LocalizationTest : public testing::Test {
 protected:
  void TearDown() override { SetLanguage(nullptr); }
};

TEST_F(LocalizationTest, SelectsJapaneseForJapaneseLocale) {
  SetLanguage("ja-JP");

  EXPECT_EQ(GetCurrentSupportedLanguage(), SupportedLanguage::kJapanese);
}

TEST_F(LocalizationTest, SelectsChineseForSwitchChineseLocales) {
  for (const char* locale : {"zh-CN", "zh-TW", "zh-Hans", "zh-Hant"}) {
    SetLanguage(locale);
    EXPECT_EQ(GetCurrentSupportedLanguage(),
              SupportedLanguage::kSimplifiedChinese);
  }
}

TEST_F(LocalizationTest, FallsBackToEnglishForUnsupportedLocale) {
  SetLanguage("ko");

  EXPECT_EQ(GetCurrentSupportedLanguage(), SupportedLanguage::kEnglish);
}

}  // namespace
