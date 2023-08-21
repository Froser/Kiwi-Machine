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

#include "base/platform/sdl2/sdl2_thread_interface.h"

#include <SDL.h>

#include "base/platform/sdl2/sdl2_single_thread_task_runner.h"

namespace kiwi::base {
namespace platform {

namespace {

struct TimerContext {
  SDL2ThreadInterface* t;
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

int ThreadEventFunc(void* data) {
  SDL2ThreadInterface* t = reinterpret_cast<SDL2ThreadInterface*>(data);

  // Set task runner for current thread.
  SingleThreadTaskRunner::SetCurrentDefault(t->task_runner_);
  SequencedTaskRunner::SetCurrentDefault(t->task_runner_);

  while (t->is_running_) {
    SDL_SemWait(t->sem_);

    // When is_running_ is set to false, wake sem_.
    if (!t->is_running_)
      break;

    SDL_LockMutex(t->mutex_);
    base::OnceClosure task = std::move(t->tasks_.front());
    t->tasks_.pop();
    SDL_UnlockMutex(t->mutex_);
    std::move(task).Run();
  }

  // Cleanup task runner for current thread.
  SingleThreadTaskRunner::SetCurrentDefault(nullptr);
  SequencedTaskRunner::SetCurrentDefault(nullptr);
  return t->exit_code_;
}

SDL2ThreadInterface::SDL2ThreadInterface() {
  mutex_ = SDL_CreateMutex();
  sem_ = SDL_CreateSemaphore(0);
}

SDL2ThreadInterface::~SDL2ThreadInterface() {
  if (thread_) {
    ExitThread(0);

    int status = 0;
    SDL_WaitThread(thread_, &status);
    thread_ = nullptr;
  }
  SDL_DestroyMutex(mutex_);
  SDL_DestroySemaphore(sem_);
}

void SDL2ThreadInterface::SetThreadName(const std::string& name) {
  thread_name_ = name;
}

bool SDL2ThreadInterface::StartWithOptions(Thread::Options options) {
  scoped_refptr<SDL2SingleThreadTaskRunner> task_runner =
      MakeRefCounted<SDL2SingleThreadTaskRunner>(this);

  is_running_ = true;

  // Store task runner before the thread start.
  task_runner_ = task_runner;

  thread_ = SDL_CreateThreadWithStackSize(ThreadEventFunc, thread_name_.c_str(),
                                          options.stack_size, this);

  return true;
}

void SDL2ThreadInterface::Stop() {
  ExitThread(0);
}

scoped_refptr<SingleThreadTaskRunner> SDL2ThreadInterface::task_runner() const {
  return task_runner_;
}

void SDL2ThreadInterface::ExitThread(int exit_code) {
  // Set exit code first.
  exit_code_.store(exit_code);

  // Set terminate flag.
  is_running_.store(false);

  // Wake up event loop.
  SDL_SemPost(sem_);
}

bool SDL2ThreadInterface::PostTask(base::OnceClosure task,
                                   base::TimeDelta delay) {
  if (delay == base::TimeDelta()) {
    SDL_LockMutex(mutex_);
    tasks_.push(std::move(task));
    SDL_UnlockMutex(mutex_);
    SDL_SemPost(sem_);
  } else {
    TimerContext* ctx = new TimerContext{this, std::move(task)};
    SDL_AddTimer(delay.InMilliseconds(), OnShouldPostTask, ctx);
  }
  return true;
}

}  // namespace platform
}  // namespace kiwi::base
