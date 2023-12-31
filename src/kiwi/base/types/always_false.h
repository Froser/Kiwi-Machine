// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TYPES_ALWAYS_FALSE_H_
#define BASE_TYPES_ALWAYS_FALSE_H_

namespace kiwi::base {

// A helper that can be used with a static_assert() that must always fail (e.g.
// for an undesirable template instantiation). Such a static_assert() cannot
// simply be written as static_assert(false, ...) because that would always fail
// to compile, even if the template was never instantiated. Instead, a common
// idiom is to force the static_assert() to depend on a template parameter so
// that it is only evaluated when the template is instantiated:
//
// template <typename U = T>
// void SomeDangerousMethodThatShouldNeverCompile() {
//   static_assert(base::AlwaysFalse<U>, "explanatory message here");
// }

namespace internal {

template <typename... Args>
struct AlwaysFalseHelper {
  static constexpr bool kValue = false;
};

}  // namespace internal

template <typename... Args>
inline constexpr bool AlwaysFalse =
    internal::AlwaysFalseHelper<Args...>::kValue;

}  // namespace kiwi::base

#endif  // BASE_TYPES_ALWAYS_FALSE_H_
