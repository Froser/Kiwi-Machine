// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

namespace kiwi::base {

SystemMemoryInfoKB::SystemMemoryInfoKB() = default;

SystemMemoryInfoKB::SystemMemoryInfoKB(const SystemMemoryInfoKB&) = default;

SystemMemoryInfoKB& SystemMemoryInfoKB::operator=(const SystemMemoryInfoKB&) =
    default;

}  // namespace kiwi::base