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

#ifndef BASE_PLATFORM_SDL2_SDL2_RUNLOOP_INTERFACE_H_
#define BASE_PLATFORM_SDL2_SDL2_RUNLOOP_INTERFACE_H_

#include <SDL.h>
#include <chrono>
#include <memory>

#include "base/platform/platform_factory.h"
#include "third_party/SDL2/include/SDL_events.h"

namespace kiwi::base {
namespace platform {
class SDL2RunLoopInterface : public RunLoopInterface {
 public:
  SDL2RunLoopInterface();
  ~SDL2RunLoopInterface() override;

 public:
  static SDL_Event CreatePostTaskEvent();

 private:
  // RunLoopInterface:
  RepeatingClosure QuitClosure() override;
  void Run() override;

 private:
  void Quit();
  void TryRender();
  float GetNextRenderDelay();

 private:
  // When any event is generated, it will signal the condition variables, stop
  // waiting delay during rendering. See TryRender() for details.
  static int EventAddedWatcher(void* userdata, SDL_Event* event);
  void OnEventAdded();

 private:
  SDL_cond* cond_ = nullptr;
  SDL_mutex* frame_sync_mutex_ = nullptr;
  bool is_running_ = false;
  std::chrono::time_point<std::chrono::steady_clock> render_timestamp_ =
      std::chrono::steady_clock::now();
};
}  // namespace platform
}  // namespace kiwi::base

#endif  // BASE_PLATFORM_SDL2_SDL2_RUNLOOP_INTERFACE_H_