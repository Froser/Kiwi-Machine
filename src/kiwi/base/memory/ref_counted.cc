// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"

#include <limits>
#include <ostream>
#include <type_traits>

namespace kiwi::base {

namespace subtle {

bool RefCountedThreadSafeBase::HasOneRef() const {
  return ref_count_.IsOne();
}

bool RefCountedThreadSafeBase::HasAtLeastOneRef() const {
  return !ref_count_.IsZero();
}

bool RefCountedThreadSafeBase::Release() const {
  return ReleaseImpl();
}
void RefCountedThreadSafeBase::AddRef() const {
  AddRefImpl();
}
void RefCountedThreadSafeBase::AddRefWithCheck() const {
  AddRefWithCheckImpl();
}
}  // namespace subtle
}  // namespace kiwi::base
