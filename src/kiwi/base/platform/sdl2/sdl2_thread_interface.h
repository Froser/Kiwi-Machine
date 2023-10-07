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

#ifndef BASE_PLATFORM_SDL2_SDL2_THREAD_INTERFACE_H_
#define BASE_PLATFORM_SDL2_SDL2_THREAD_INTERFACE_H_

#include <SDL.h>
#include <atomic>
#include <queue>

#include "base/platform/platform_factory.h"
#include "base/platform/sdl2/sdl2_single_thread_task_runner.h"
#include "third_party/SDL2/include/SDL_mutex.h"
#include "third_party/SDL2/include/SDL_thread.h"

namespace kiwi::base {
namespace platform {
class SDL2ThreadInterface
    : public ThreadInterface,
      public SDL2SingleThreadTaskRunner::PostTaskDelegate {
 public:
  SDL2ThreadInterface();
  ~SDL2ThreadInterface() override;

  // ThreadInterface:
  bool StartWithOptions(Thread::Options options) override;
  void Stop() override;
  scoped_refptr<SingleThreadTaskRunner> task_runner() const override;

  // SDL2SingleThreadTaskRunner::PostTaskDelegate:
  bool PostTask(base::OnceClosure task, base::TimeDelta delay) override;

 public:
  void SetThreadName(const std::string& name);

 private:
  void ExitThread(int exit_code);

 private:
  friend int ThreadEventFunc(void*);
  friend class SDL2SingleThreadTaskRunner;

  SDL_TimerID timer_ = 0;
  std::atomic_bool is_running_;
  std::atomic_int exit_code_;
  std::queue<base::OnceClosure> tasks_;
  SDL_mutex* mutex_ = nullptr;
  SDL_sem* sem_ = nullptr;

  SDL_Thread* thread_ = nullptr;
  std::string thread_name_;
  scoped_refptr<SingleThreadTaskRunner> task_runner_;
};
}  // namespace platform
}  // namespace kiwi::base

#endif  // BASE_PLATFORM_SDL2_SDL2_THREAD_INTERFACE_H_