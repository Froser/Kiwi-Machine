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

#ifndef BASE_PLATFORM_SDL2_SDL2_SINGLE_THREAD_TASK_RUNNER_H
#define BASE_PLATFORM_SDL2_SDL2_SINGLE_THREAD_TASK_RUNNER_H

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"

namespace kiwi::base {
namespace platform {
class SDL2SingleThreadTaskRunner : public SingleThreadTaskRunner {
 public:
  class PostTaskDelegate {
   public:
    PostTaskDelegate() = default;
    virtual ~PostTaskDelegate() = default;
    virtual bool PostTask(base::OnceClosure task, base::TimeDelta delay) = 0;
  };

 public:
  explicit SDL2SingleThreadTaskRunner(PostTaskDelegate* delegate);
  ~SDL2SingleThreadTaskRunner() override;

  bool PostDelayedTask(const Location& from_here,
                       OnceClosure task,
                       base::TimeDelta delay) override;
  bool PostTaskAndReply(const Location& from_here,
                        OnceClosure task,
                        OnceClosure reply) override;

 private:
  PostTaskDelegate* delegate_ = nullptr;
};

}  // namespace platform
}  // namespace kiwi::base

#endif  // BASE_PLATFORM_SDL2_SDL2_SINGLE_THREAD_TASK_RUNNER_H