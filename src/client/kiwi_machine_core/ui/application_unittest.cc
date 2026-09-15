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

#include <SDL.h>

#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"
#include "ui/application_events.h"

TEST(ApplicationEventsTest, FlushesBeforeBackgroundOrTermination) {
  EXPECT_TRUE(application_events::RequiresBatterySaveFlush(
      SDL_APP_WILLENTERBACKGROUND));
  EXPECT_TRUE(
      application_events::RequiresBatterySaveFlush(SDL_APP_TERMINATING));
}

TEST(ApplicationEventsTest, IgnoresUnrelatedLifecycleEvents) {
  EXPECT_FALSE(
      application_events::RequiresBatterySaveFlush(SDL_APP_DIDENTERBACKGROUND));
  EXPECT_FALSE(application_events::RequiresBatterySaveFlush(
      SDL_APP_WILLENTERFOREGROUND));
  EXPECT_FALSE(
      application_events::RequiresBatterySaveFlush(SDL_APP_DIDENTERFOREGROUND));
  EXPECT_FALSE(application_events::RequiresBatterySaveFlush(SDL_QUIT));
}
