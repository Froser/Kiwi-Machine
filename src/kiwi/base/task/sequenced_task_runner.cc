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

#include "base/task/sequenced_task_runner.h"

#include <memory>

#include "base/check.h"
#include "base/task/default_delayed_task_handle_delegate.h"

namespace kiwi::base {
namespace {
thread_local scoped_refptr<SequencedTaskRunner> g_task_runner_for_this_thread;
}

SequencedTaskRunner::SequencedTaskRunner() = default;
bool SequencedTaskRunner::RunsTasksInCurrentSequence() const {
  CHECK(HasCurrentDefault());
  return this == g_task_runner_for_this_thread.get();
}

DelayedTaskHandle SequencedTaskRunner::PostCancelableDelayedTask(
    subtle::PostDelayedTaskPassKey,
    const Location& from_here,
    OnceClosure task,
    TimeDelta delay) {
  auto delayed_task_handle_delegate =
      std::make_unique<DefaultDelayedTaskHandleDelegate>();

  task = delayed_task_handle_delegate->BindCallback(std::move(task));

  DelayedTaskHandle delayed_task_handle(
      std::move(delayed_task_handle_delegate));

  PostDelayedTask(from_here, std::move(task), delay);

  return delayed_task_handle;
}

bool SequencedTaskRunner::PostTask(const Location& from_here,
                                   OnceClosure task) {
  return PostDelayedTask(from_here, std::move(task), base::TimeDelta());
}

const scoped_refptr<SequencedTaskRunner>&
SequencedTaskRunner::GetCurrentDefault() {
  CHECK(HasCurrentDefault());
  return g_task_runner_for_this_thread;
}

bool SequencedTaskRunner::HasCurrentDefault() {
  return !!g_task_runner_for_this_thread;
}

void SequencedTaskRunner::SetCurrentDefault(
    scoped_refptr<SequencedTaskRunner> task_runner) {
  g_task_runner_for_this_thread = task_runner;
}
}  // namespace kiwi::base
