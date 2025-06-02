// Copyright (C) 2025 Yisi Yu
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

#include "base/synchronization/waitable_event.h"

namespace kiwi::base {

WaitableEvent::WaitableEvent(ResetPolicy reset_policy,
                             InitialState initial_state)
    : reset_policy_(reset_policy),
      signaled_(initial_state == InitialState::SIGNALED),
      initial_state_(initial_state) {}

WaitableEvent::~WaitableEvent() = default;

void WaitableEvent::Reset() {
  signaled_ = initial_state_ == InitialState::SIGNALED;
}

void WaitableEvent::Signal() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    signaled_ = true;
  }
  cv_.notify_all();
}

bool WaitableEvent::IsSignaled() {
  return signaled_;
}

void WaitableEvent::Wait() {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this] { return signaled_; });
  if (reset_policy_ == ResetPolicy::AUTOMATIC)
    signaled_ = false;
}

}  // namespace kiwi::base