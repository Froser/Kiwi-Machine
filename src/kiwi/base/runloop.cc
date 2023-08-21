// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/runloop.h"

namespace kiwi::base {
RunLoop::RunLoop() {
  runloop_ = GetPlatformFactory()->CreateRunLoopInterface();
}

RunLoop::~RunLoop() = default;

RepeatingClosure RunLoop::QuitClosure() {
  return runloop_->QuitClosure();
}

void RunLoop::Run() {
  runloop_->Run();
}
}  // namespace kiwi::base
