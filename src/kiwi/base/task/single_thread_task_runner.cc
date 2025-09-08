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

#include "base/task/single_thread_task_runner.h"

#include "base/check.h"

namespace kiwi::base {
namespace {
THREAD_LOCAL scoped_refptr<SingleThreadTaskRunner>
    g_task_runner_for_this_thread;
}
SingleThreadTaskRunner::SingleThreadTaskRunner() = default;

const scoped_refptr<SingleThreadTaskRunner>&
SingleThreadTaskRunner::GetCurrentDefault() {
  CHECK(HasCurrentDefault());
  return g_task_runner_for_this_thread;
}

bool SingleThreadTaskRunner::HasCurrentDefault() {
  return !!g_task_runner_for_this_thread;
}

void SingleThreadTaskRunner::SetCurrentDefault(
    scoped_refptr<SingleThreadTaskRunner> task_runner) {
  g_task_runner_for_this_thread = task_runner;
}

}  // namespace kiwi::base
