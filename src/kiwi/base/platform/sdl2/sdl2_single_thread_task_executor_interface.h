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

#ifndef BASE_PLATFORM_SDL2_SDL2_SINGLE_THREAD_TASK_EXECUTOR_INTERFACE_H_
#define BASE_PLATFORM_SDL2_SDL2_SINGLE_THREAD_TASK_EXECUTOR_INTERFACE_H_

#include <SDL.h>
#include <queue>
#include <set>

#include "base/message_loop/message_pump_type.h"
#include "base/platform/platform_factory.h"
#include "base/platform/sdl2/sdl2_single_thread_task_runner.h"
#include "base/time/time.h"

namespace kiwi::base {
namespace platform {
class SDL2SingleThreadTaskExecutorInterface
    : public SingleThreadTaskExecutorInterface,
      public SDL2SingleThreadTaskRunner::PostTaskDelegate {
 public:
  explicit SDL2SingleThreadTaskExecutorInterface(MessagePumpType type);
  ~SDL2SingleThreadTaskExecutorInterface() override;

 public:
  // SDL2SingleThreadTaskRunner::PostTaskDelegate:
  bool PostTask(base::OnceClosure task, base::TimeDelta delay) override;

 public:
  // Run the front task of the task queue, and remove it from the task queue.
  void RunTask();

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SDL_TimerID timer_ = 0;

  // Tasks implementation.
  std::queue<base::OnceClosure> tasks_;
  SDL_mutex* mutex_ = nullptr;
};
}  // namespace platform
}  // namespace kiwi::base

#endif  // BASE_PLATFORM_SDL2_SDL2_SINGLE_THREAD_TASK_EXECUTOR_INTERFACE_H_