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

// Provides value storage and comparison/math operations common to all time
// classes. Each subclass provides for strong type-checking to ensure
// semantically meaningful comparison/math of time values from the same clock
// source or timeline.
class TimeBase {
 public:
  static constexpr int64_t kHoursPerDay = 24;
  static constexpr int64_t kSecondsPerMinute = 60;
  static constexpr int64_t kMinutesPerHour = 60;
  static constexpr int64_t kSecondsPerHour =
      kSecondsPerMinute * kMinutesPerHour;
  static constexpr int64_t kMillisecondsPerSecond = 1000;
  static constexpr int64_t kMillisecondsPerDay =
      kMillisecondsPerSecond * kSecondsPerHour * kHoursPerDay;
  static constexpr int64_t kMicrosecondsPerMillisecond = 1000;
  static constexpr int64_t kMicrosecondsPerSecond =
      kMicrosecondsPerMillisecond * kMillisecondsPerSecond;
  static constexpr int64_t kMicrosecondsPerMinute =
      kMicrosecondsPerSecond * kSecondsPerMinute;
  static constexpr int64_t kMicrosecondsPerHour =
      kMicrosecondsPerMinute * kMinutesPerHour;
  static constexpr int64_t kMicrosecondsPerDay =
      kMicrosecondsPerHour * kHoursPerDay;
  static constexpr int64_t kMicrosecondsPerWeek = kMicrosecondsPerDay * 7;
  static constexpr int64_t kNanosecondsPerMicrosecond = 1000;
  static constexpr int64_t kNanosecondsPerSecond =
      kNanosecondsPerMicrosecond * kMicrosecondsPerSecond;
};

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

  // Returns the maximum time delta, which should be greater than any reasonable
  // time delta we might compare it to. If converted to double with ToDouble()
  // it becomes an IEEE double infinity. Use FiniteMax() if you want a very
  // large number that doesn't do this. TimeDelta math saturates at the end
  // points so adding to TimeDelta::Max() leaves the value unchanged.
  // Subtracting should leave the value unchanged but currently changes it
  // TODO(https://crbug.com/869387).
  static constexpr TimeDelta Max();
  static constexpr TimeDelta Min();

  constexpr bool is_positive() const { return delta_.count() > 0; }

  constexpr int64_t InSeconds() const;
  constexpr double InSecondsF() const;
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
  constexpr bool operator<(TimeDelta other) const {
    return delta_ < other.delta_;
  }

  // Returns true if the time delta is a zero, positive or negative time delta.
  constexpr bool is_zero() const { return delta_.count() == 0; }

 private:
  // Constructs a delta given the duration in microseconds. This is private
  // to avoid confusion by callers with an integer constructor. Use
  // base::Seconds, base::Milliseconds, etc. instead.
  constexpr explicit TimeDelta(int64_t delta_us) : delta_(delta_us) {}

  // Delta in microseconds.
  std::chrono::microseconds delta_ = std::chrono::microseconds(0);
};

// Time ---- ------------------------------------------------------------------

// Represents a wall clock time in UTC. Values are not guaranteed to be
// monotonically non-decreasing and are subject to large amounts of skew.
// Time is stored internally as microseconds since the Windows epoch (1601).
class BASE_EXPORT Time {
 public:
  constexpr Time() = default;

  // Returns the current time. Watch out, the system might adjust its clock
  // in which case time will actually go backwards. We don't guarantee that
  // times are increasing, or that two calls to Now() won't be the same.
  static Time Now();

  TimeDelta operator-(Time time);

  // TODO(https://crbug.com/1392437): Remove concept of "null" from base::Time.
  //
  // Warning: Be careful when writing code that performs math on time values,
  // since it's possible to produce a valid "zero" result that should not be
  // interpreted as a "null" value. If you find yourself using this method or
  // the zero-arg default constructor, please consider using an optional to
  // express the null state.
  //
  // Returns true if this object has not been initialized (probably).
  constexpr bool is_null() const {
    return time_ == std::chrono::system_clock::time_point();
  }

 private:
  std::chrono::system_clock::time_point time_;
};

template <typename T>
constexpr TimeDelta operator*(T a, TimeDelta td) {
  return td * a;
}

template <typename T>
constexpr TimeDelta Seconds(T n) {
  return TimeDelta::FromInternalValue(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::seconds(n))
          .count());
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

constexpr double TimeDelta::InSecondsF() const {
  if (*this != Max() && *this != Min())
    return static_cast<double>(InMicroseconds()) /
           TimeBase::kMicrosecondsPerSecond;
  return (delta_.count() < 0) ? -std::numeric_limits<double>::infinity()
                              : std::numeric_limits<double>::infinity();
}

constexpr int64_t TimeDelta::InMilliseconds() const {
  return std::chrono::duration_cast<std::chrono::milliseconds>(delta_).count();
}

constexpr int64_t TimeDelta::InNanoseconds() const {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(delta_).count();
}

// static
constexpr TimeDelta TimeDelta::Max() {
  return TimeDelta((std::numeric_limits<int64_t>::max)());
}

constexpr TimeDelta TimeDelta::Min() {
  return TimeDelta((std::numeric_limits<int64_t>::min)());
}

// TimeTicks ------------------------------------------------------------------

class BASE_EXPORT TimeTicks {
 public:
  TimeTicks();
  ~TimeTicks();

  static TimeTicks Now();

  TimeTicks operator+(TimeDelta delta);
  TimeDelta operator-(TimeTicks ticks);

 private:
  TimeTicks(std::chrono::steady_clock::time_point tick);

 private:
  std::chrono::steady_clock::time_point tick_;
};

}  // namespace kiwi::base

#endif  // BASE_TIME_TIME_H_