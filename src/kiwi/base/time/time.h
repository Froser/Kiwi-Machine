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

#ifndef BASE_TIME_TIME_H_
#define BASE_TIME_TIME_H_

#include <chrono>
#include <cstdint>

#include "base/base_export.h"

namespace kiwi::base {

// TimeDelta ------------------------------------------------------------------

class BASE_EXPORT TimeDelta {
 public:
  constexpr TimeDelta() = default;

  // Converts an integer value representing TimeDelta to a class. This is used
  // when deserializing a |TimeDelta| structure, using a value known to be
  // compatible. It is not provided as a constructor because the integer type
  // may be unclear from the perspective of a caller.
  static constexpr TimeDelta FromInternalValue(int64_t delta) {
    return TimeDelta(delta);
  }

  constexpr int64_t InSeconds() const;
  constexpr int64_t InMilliseconds() const;
  constexpr int64_t InMicroseconds() const { return delta_.count(); }
  constexpr int64_t InNanoseconds() const;

  // Comparison operators.
  constexpr bool operator==(TimeDelta other) const {
    return delta_ == other.delta_;
  }
  constexpr bool operator!=(TimeDelta other) const {
    return delta_ != other.delta_;
  }

 private:
  // Constructs a delta given the duration in microseconds. This is private
  // to avoid confusion by callers with an integer constructor. Use
  // base::Seconds, base::Milliseconds, etc. instead.
  constexpr explicit TimeDelta(int64_t delta_us) : delta_(delta_us) {}

  // Delta in microseconds.
  std::chrono::microseconds delta_ = std::chrono::microseconds(0);
};

template <typename T>
constexpr TimeDelta operator*(T a, TimeDelta td) {
  return td * a;
}

template <typename T>
constexpr TimeDelta Milliseconds(T n) {
  return TimeDelta::FromInternalValue(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::milliseconds(n))
          .count());
}
template <typename T>
constexpr TimeDelta Microseconds(T n) {
  return TimeDelta::FromInternalValue(n);
}

template <typename T>
constexpr TimeDelta Nanoseconds(T n) {
  return TimeDelta::FromInternalValue(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::nanoseconds(n))
          .count());
}

constexpr int64_t TimeDelta::InSeconds() const {
  return std::chrono::duration_cast<std::chrono::seconds>(delta_).count();
}

constexpr int64_t TimeDelta::InMilliseconds() const {
  return std::chrono::duration_cast<std::chrono::milliseconds>(delta_).count();
}

constexpr int64_t TimeDelta::InNanoseconds() const {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(delta_).count();
}

}  // namespace kiwi::base

#endif  // BASE_TIME_TIME_H_