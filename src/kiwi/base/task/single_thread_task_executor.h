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

#ifndef BASE_TASK_SINGLE_THREAD_TASK_EXECUTOR
#define BASE_TASK_SINGLE_THREAD_TASK_EXECUTOR

#include "base/base_export.h"
#include "base/message_loop/message_pump_type.h"
#include "base/platform/platform_factory.h"

namespace kiwi::base {
// It is mandatory to construct SingleThreadTaskExecutor instance when program
// starts.
class BASE_EXPORT SingleThreadTaskExecutor {
 public:
  SingleThreadTaskExecutor(
      MessagePumpType message_pump_type = MessagePumpType::DEFAULT);
  SingleThreadTaskExecutor(const SingleThreadTaskExecutor&) = delete;
  SingleThreadTaskExecutor& operator=(const SingleThreadTaskExecutor&) = delete;
  ~SingleThreadTaskExecutor();

 private:
  std::unique_ptr<platform::SingleThreadTaskExecutorInterface> executor_;
};
}  // namespace kiwi::base

#endif  // BASE_TASK_SINGLE_THREAD_TASK_EXECUTOR