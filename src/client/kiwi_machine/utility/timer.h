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

#ifndef UTILITY_TIMER_H_
#define UTILITY_TIMER_H_

#include <chrono>

class Timer {
 public:
  Timer();
  ~Timer();

 public:
  void Start();
  // Same meaning as Start()
  void Reset();

  int ElapsedInMilliseconds();
  int ElapsedInMillisecondsAndReset();

 private:
  std::chrono::high_resolution_clock::time_point last_;
};

#endif  // UTILITY_TIMER_H_