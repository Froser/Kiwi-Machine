// Copyright (C) 2023 Yisi Yu
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

#include "utility/timer.h"

Timer::Timer() {
  Start();
}

Timer::~Timer() = default;

void Timer::Start() {
  last_ = std::chrono::high_resolution_clock::now();
}

void Timer::Reset() {
  Start();
}

int Timer::ElapsedInMilliseconds() {
  std::chrono::duration<float, std::milli> elapsed_ms =
      std::chrono::high_resolution_clock::now() - last_;
  return static_cast<float>(elapsed_ms.count());
}

int Timer::ElapsedInMillisecondsAndReset() {
  int elapsed = ElapsedInMilliseconds();
  Reset();
  return elapsed;
}
