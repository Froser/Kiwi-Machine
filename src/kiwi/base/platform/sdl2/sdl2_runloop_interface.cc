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

#include "base/platform/sdl2/sdl2_runloop_interface.h"

#include <stack>

#include "base/check.h"
#include "base/platform/sdl2/sdl2_single_thread_task_executor_interface.h"
#include "third_party/SDL2/include/SDL_events.h"
#include "third_party/SDL2/include/SDL_timer.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace kiwi::base {
namespace platform {
namespace events {

int PostTask() {
  static int kEvent = SDL_RegisterEvents(1);
  return kEvent;
}
}  // namespace events

namespace {
constexpr int kFPS = 60;
thread_local std::stack<SDL2RunLoopInterface*> g_thread_run_loops;

#if __EMSCRIPTEN__
void EmscriptenMainLoop() {
  CHECK(g_thread_run_loops.top());
  g_thread_run_loops.top()->HandleEvents();
}
#endif

}  // namespace

SDL2RunLoopInterface::SDL2RunLoopInterface() {
  g_thread_run_loops.push(this);
  cond_ = SDL_CreateCond();
  frame_sync_mutex_ = SDL_CreateMutex();

  // Lock sync mutex, waiting for signaling, or timeout.
  SDL_LockMutex(frame_sync_mutex_);

  SDL_AddEventWatch(&SDL2RunLoopInterface::EventAddedWatcher, this);
}

SDL2RunLoopInterface::~SDL2RunLoopInterface() {
  SDL_assert(cond_);
  SDL_DestroyCond(cond_);

  SDL_assert(frame_sync_mutex_);
  SDL_DestroyMutex(frame_sync_mutex_);
  SDL_AddEventWatch(&SDL2RunLoopInterface::EventAddedWatcher, this);
  g_thread_run_loops.pop();
}

RepeatingClosure SDL2RunLoopInterface::QuitClosure() {
  return base::BindRepeating(&SDL2RunLoopInterface::Quit,
                             base::Unretained(this));
}

SDL_Event SDL2RunLoopInterface::CreatePostTaskEvent() {
  SDL_Event event;
  event.user.type = events::PostTask();
  event.user.timestamp = SDL_GetTicks();
  return event;
}

void SDL2RunLoopInterface::Run() {
  is_running_ = true;
#if __EMSCRIPTEN__
  emscripten_set_main_loop(EmscriptenMainLoop, 0, true);
#else
  while (is_running_) {
    HandleEvents();
  }
#endif
}

void SDL2RunLoopInterface::HandleEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (GetPreEventHandlerForSDL2()) {
      GetPreEventHandlerForSDL2().Run(&event);
    }

    if (event.type == SDL_QUIT) {
      is_running_ = false;
      break;
    } else if (event.user.type == events::PostTask()) {
      auto* single_thread_task_executor =
          reinterpret_cast<SDL2SingleThreadTaskExecutorInterface*>(
              event.user.data1);
      CHECK(single_thread_task_executor);
      single_thread_task_executor->RunTask();

      TryRender();
      continue;
    }

    // Propagate event to registered handler.
    if (GetEventHandlerForSDL2()) {
      GetEventHandlerForSDL2().Run(&event);
    }
  }

  if (GetPostEventHandlerForSDL2())
    GetPostEventHandlerForSDL2().Run();
  TryRender();
}

void SDL2RunLoopInterface::Quit() {
  SDL_Event event;
  event.type = SDL_QUIT;
  SDL_PushEvent(&event);
}

void SDL2RunLoopInterface::TryRender() {
#if __EMSCRIPTEN__
  // Emscripten uses emscripten_set_main_loop() to control fps, so we just
  // render here.
  GetRenderHandlerForSDL2().Run();
#else
  float delay = GetNextRenderDelay();
  if (delay < 0) {
    if (GetRenderHandlerForSDL2())
      GetRenderHandlerForSDL2().Run();
  } else {
    // When condition variable is signaled. It means a PostTask is called in
    // another thread. When condition variable is timeout, no task is post
    // duration this 'delay'.
    int result = SDL_CondWaitTimeout(cond_, frame_sync_mutex_, delay);
    if (result < 0)
      LOG(ERROR) << " Condition wait timeout failed:" << SDL_GetError();
  }
#endif
}

// Gets the recommended delay duration. Window should render if duration <= 0.
float SDL2RunLoopInterface::GetNextRenderDelay() {
  constexpr auto kRenderDuration = std::chrono::milliseconds(1000 / kFPS);
  const std::chrono::duration<float, std::milli> frame_elapsed =
      std::chrono::steady_clock::now() - render_timestamp_;
  bool need_render = frame_elapsed >= kRenderDuration;
  if (need_render)
    render_timestamp_ = std::chrono::steady_clock::now();
  return (kRenderDuration - frame_elapsed).count();
}

int SDL2RunLoopInterface::EventAddedWatcher(void* userdata, SDL_Event* event) {
  SDL2RunLoopInterface* i = reinterpret_cast<SDL2RunLoopInterface*>(userdata);
  SDL_assert(i);
  i->OnEventAdded();

  // Return value is ignored.
  return true;
}

void SDL2RunLoopInterface::OnEventAdded() {
  SDL_CondSignal(cond_);
}

}  // namespace platform
}  // namespace kiwi::base