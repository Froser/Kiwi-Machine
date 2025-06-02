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

#include "base/time/time.h"

namespace kiwi::base {

TimeTicks::TimeTicks() = default;
TimeTicks::~TimeTicks() = default;

TimeTicks::TimeTicks(std::chrono::steady_clock::time_point tick)
    : tick_(tick) {}

TimeTicks TimeTicks::Now() {
  return TimeTicks(std::chrono::steady_clock::now());
}

TimeTicks TimeTicks::operator+(TimeDelta delta) {
  return TimeTicks(tick_ +
                   std::chrono::steady_clock::duration(delta.InNanoseconds()));
}

TimeDelta TimeTicks::operator-(TimeTicks ticks) {
  return TimeDelta::FromInternalValue(
      std::chrono::duration_cast<std::chrono::microseconds>(tick_ - ticks.tick_)
          .count());
}

Time Time::Now() {
  Time t;
  t.time_ = std::chrono::system_clock::now();
  return t;
}

TimeDelta Time::operator-(Time time) {
  return TimeDelta::FromInternalValue(
      std::chrono::duration_cast<std::chrono::microseconds>(time_ - time.time_)
          .count());
}

}  // namespace kiwi::base