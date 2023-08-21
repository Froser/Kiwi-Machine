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

#include "base/platform/sdl2/sdl2_single_thread_task_runner.h"

#include "base/platform/sdl2/sdl2_thread_interface.h"

namespace kiwi::base {
namespace platform {

SDL2SingleThreadTaskRunner::SDL2SingleThreadTaskRunner(
    PostTaskDelegate* delegate)
    : delegate_(delegate) {}

SDL2SingleThreadTaskRunner::~SDL2SingleThreadTaskRunner() = default;

bool SDL2SingleThreadTaskRunner::PostDelayedTask(const Location& from_here,
                                                 OnceClosure task,
                                                 base::TimeDelta delay) {
  return delegate_->PostTask(std::move(task), delay);
}

bool SDL2SingleThreadTaskRunner::PostTaskAndReply(const Location& from_here,
                                                  OnceClosure task,
                                                  OnceClosure reply) {
  scoped_refptr<SingleThreadTaskRunner> reply_task_runner =
      SingleThreadTaskRunner::GetCurrentDefault();
  base::OnceClosure c = std::move(task).Then(base::BindOnce(
      [](const Location& from_here,
         scoped_refptr<SingleThreadTaskRunner> reply_task_runner,
         OnceClosure reply) {
        reply_task_runner->PostTask(from_here, std::move(reply));
      },
      from_here, base::RetainedRef(reply_task_runner), std::move(reply)));
  return PostTask(from_here, std::move(c));
}

}  // namespace platform
}  // namespace kiwi::base