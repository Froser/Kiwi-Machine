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

#include "base/task/single_thread_task_executor.h"

namespace kiwi::base {
SingleThreadTaskExecutor::SingleThreadTaskExecutor(
    MessagePumpType message_pump_type) {
  executor_ =
      GetPlatformFactory()->CreateSingleThreadTaskExecutor(message_pump_type);
}

SingleThreadTaskExecutor::~SingleThreadTaskExecutor() = default;
}  // namespace kiwi::base