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

#include "base/threading/thread.h"

#include "base/check.h"
#include "base/platform/platform_factory.h"

namespace kiwi::base {

Thread::Options::Options() = default;

Thread::Options::Options(MessagePumpType type, size_t size)
    : message_pump_type(type), stack_size(size) {}

Thread::Options::Options(Options&& other) = default;
Thread::Options& Thread::Options::operator=(Thread::Options&& other) = default;
Thread::Options::~Options() = default;

Thread::Thread(const std::string& name) {
  thread_interface_ = GetPlatformFactory()->CreateThreadInterface(name);
  CHECK(thread_interface_);
}

Thread::~Thread() {
  CHECK(thread_interface_);
  Stop();
}

bool Thread::StartWithOptions(Thread::Options options) {
  CHECK(thread_interface_);
  return thread_interface_->StartWithOptions(std::move(options));
}

void Thread::Stop() {
  CHECK(thread_interface_);
  thread_interface_->Stop();
}

scoped_refptr<SingleThreadTaskRunner> Thread::task_runner() const {
  CHECK(thread_interface_);
  return thread_interface_->task_runner();
}

}  // namespace kiwi::base