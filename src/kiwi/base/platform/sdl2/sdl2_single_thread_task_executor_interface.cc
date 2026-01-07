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

#include "base/platform/sdl2/sdl2_single_thread_task_executor_interface.h"

#include <atomic>

#include "base/check.h"
#include "base/platform/sdl2/sdl2_runloop_interface.h"
#include "third_party/SDL2/include/SDL.h"

namespace kiwi::base {
namespace platform {

namespace {

struct TimerContext {
  SDL2SingleThreadTaskExecutorInterface* t;
  base::OnceClosure task;
};

Uint32 OnShouldPostTask(Uint32 interval, void* param) {
  TimerContext* context = reinterpret_cast<TimerContext*>(param);
  SDL_assert(context);
  context->t->PostTask(std::move(context->task), base::TimeDelta());
  delete context;

  // Do not run this timer again. Returns zero.
  return 0;
}

}  // namespace

SDL2SingleThreadTaskExecutorInterface::SDL2SingleThreadTaskExecutorInterface(
    MessagePumpType type) {
  DCHECK(type == MessagePumpType::DEFAULT || type == MessagePumpType::UI)
      << "MessagePumpType is useless here, but DEFAULT and UI is suggested.";
  if (type == MessagePumpType::DEFAULT || type == MessagePumpType::UI) {
    int init_result = SDL_Init(SDL_INIT_EVERYTHING);
    if (init_result < 0) {
      init_result = SDL_Init(SDL_INIT_EVERYTHING & ~SDL_INIT_AUDIO);
      LOG(WARNING) << "Failed to init everything. Remove audio and try again.";
      if (init_result < 0) {
        CHECK(false) << SDL_GetError();
      }
    }
  }
  mutex_ = SDL_CreateMutex();

  scoped_refptr<SDL2SingleThreadTaskRunner> task_runner =
      base::MakeRefCounted<SDL2SingleThreadTaskRunner>(this);
  SingleThreadTaskRunner::SetCurrentDefault(task_runner);
  SequencedTaskRunner::SetCurrentDefault(task_runner);
  task_runner_ = task_runner;
}

SDL2SingleThreadTaskExecutorInterface::
    ~SDL2SingleThreadTaskExecutorInterface() {
  if (timer_)
    SDL_RemoveTimer(timer_);
  SingleThreadTaskRunner::SetCurrentDefault(nullptr);
  SequencedTaskRunner::SetCurrentDefault(nullptr);
  SDL_DestroyMutex(mutex_);
}

bool SDL2SingleThreadTaskExecutorInterface::PostTask(base::OnceClosure task,
                                                     base::TimeDelta delay) {
  if (delay == base::TimeDelta()) {
    SDL_Event event = SDL2RunLoopInterface::CreatePostTaskEvent();
    // Push the task into the queue, waiting for poll.
    SDL_LockMutex(mutex_);
    tasks_.push(std::move(task));
    SDL_UnlockMutex(mutex_);
    event.user.data1 = this;
    SDL_PushEvent(&event);
  } else {
    TimerContext* ctx = new TimerContext{this, std::move(task)};
    timer_ = SDL_AddTimer(delay.InMilliseconds(), OnShouldPostTask, ctx);
  }
  return true;
}

void SDL2SingleThreadTaskExecutorInterface::RunTask() {
  SDL_LockMutex(mutex_);
  base::OnceClosure task = std::move(tasks_.front());
  tasks_.pop();
  SDL_UnlockMutex(mutex_);
  std::move(task).Run();
}

}  // namespace platform
}  // namespace kiwi::base
