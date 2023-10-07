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

#include "base/platform/platform_factory.h"

#include "base/check.h"
#include "base/logging.h"

#if defined(USE_SDL2)
#include "base/platform/sdl2/sdl2_platform_factory.h"
#endif

#if defined(USE_QT6)
#include "base/platform/qt/qt_platform_factory.h"
#endif

namespace kiwi::base {
namespace platform {
namespace {
PlatformFactoryBackend g_platform_factory_backend =
    PlatformFactoryBackend::kSDL2;
int* g_startup_argc = nullptr;
char** g_startup_argv = nullptr;
SDL2EventHandler g_sdl2_event_handler;
SDL2EventHandler g_sdl2_pre_event_handler;
SDL2PostEventHandler g_sdl2_post_event_handler;
SDL2RenderHandler g_sdl2_render_handler;

}  // namespace

ThreadInterface::ThreadInterface() = default;
ThreadInterface::~ThreadInterface() = default;

RunLoopInterface::RunLoopInterface() = default;
RunLoopInterface::~RunLoopInterface() = default;

SingleThreadTaskExecutorInterface::SingleThreadTaskExecutorInterface() =
    default;
SingleThreadTaskExecutorInterface::~SingleThreadTaskExecutorInterface() =
    default;
}  // namespace platform

BASE_EXPORT PlatformFactoryBackend
InitializePlatformFactory(int& argc,
                          char** argv,
                          PlatformFactoryBackend backend) {
  google::InitGoogleLogging(argv[0]);

  platform::g_startup_argc = &argc;
  platform::g_startup_argv = argv;

#if defined(USE_SDL2)
  if (backend == PlatformFactoryBackend::kSDL2) {
    platform::g_platform_factory_backend = PlatformFactoryBackend::kSDL2;
    return platform::g_platform_factory_backend;
  }
#endif

#if defined(USE_QT6)
  if (backend == PlatformFactoryBackend::kQt6) {
    platform::g_platform_factory_backend = PlatformFactoryBackend::kQt6;
    return platform::g_platform_factory_backend;
  }
#endif

  LOG(WARNING) << "Unsupported backend type: " << static_cast<int>(backend)
               << ", fallback to default.";

  platform::g_platform_factory_backend = PlatformFactoryBackend::kSDL2;
  return platform::g_platform_factory_backend;
}

PlatformFactoryBackend GetPlatformFactoryBackend() {
  return platform::g_platform_factory_backend;
}

PlatformFactory* GetPlatformFactory() {
#if defined(USE_SDL2)
  static SDL2PlatformFactory s_sdl2;
  if (GetPlatformFactoryBackend() == PlatformFactoryBackend::kSDL2)
    return &s_sdl2;
#endif

#if defined(USE_QT6)
  static QtPlatformFactory s_qt6;
  if (GetPlatformFactoryBackend() == PlatformFactoryBackend::kQt6)
    return &s_qt6;
#endif

  static PlatformFactory* factories[] = {
#if defined(USE_SDL2)
    &s_sdl2,
#endif
#if defined(USE_QT6)
    &s_qt6,
#endif
  };

  static_assert(sizeof(factories) > 0,
                "At least one backend should be available.");

  if (GetPlatformFactoryBackend() != PlatformFactoryBackend::kSDL2) {
    LOG(WARNING) << " backend type "
                 << static_cast<int>(GetPlatformFactoryBackend())
                 << " is not support. Use SDL2 backend.";
  }
  return factories[0];
}

int& GetStartupArgc() {
  CHECK(platform::g_startup_argc)
      << "Argc is null. Did you forget to call InitializePlatformFactory()?";
  return *platform::g_startup_argc;
}

char** GetStartupArgv() {
  CHECK(platform::g_startup_argv)
      << "Argc is null. Did you forget to call InitializePlatformFactory()?";
  return platform::g_startup_argv;
}

void SetPreEventHandlerForSDL2(SDL2EventHandler handler) {
  DCHECK(GetPlatformFactoryBackend() == PlatformFactoryBackend::kSDL2)
      << "Only backend with SDL2 should use this function.";
  platform::g_sdl2_pre_event_handler = handler;
}

SDL2EventHandler GetPreEventHandlerForSDL2() {
  DCHECK(GetPlatformFactoryBackend() == PlatformFactoryBackend::kSDL2)
      << "Only backend with SDL2 should use this function.";
  return platform::g_sdl2_pre_event_handler;
}

void SetEventHandlerForSDL2(SDL2EventHandler handler) {
  DCHECK(GetPlatformFactoryBackend() == PlatformFactoryBackend::kSDL2)
      << "Only backend with SDL2 should use this function.";
  platform::g_sdl2_event_handler = handler;
}

SDL2EventHandler GetEventHandlerForSDL2() {
  DCHECK(GetPlatformFactoryBackend() == PlatformFactoryBackend::kSDL2)
      << "Only backend with SDL2 should use this function.";
  return platform::g_sdl2_event_handler;
}

void SetPostEventHandlerForSDL2(SDL2PostEventHandler handler) {
  DCHECK(GetPlatformFactoryBackend() == PlatformFactoryBackend::kSDL2)
      << "Only backend with SDL2 should use this function.";
  platform::g_sdl2_post_event_handler = handler;
}

SDL2PostEventHandler GetPostEventHandlerForSDL2() {
  DCHECK(GetPlatformFactoryBackend() == PlatformFactoryBackend::kSDL2)
      << "Only backend with SDL2 should use this function.";
  return platform::g_sdl2_post_event_handler;
}

void SetRenderHandlerForSDL2(SDL2RenderHandler handler) {
  DCHECK(GetPlatformFactoryBackend() == PlatformFactoryBackend::kSDL2)
      << "Only backend with SDL2 should use this function.";
  platform::g_sdl2_render_handler = handler;
}

SDL2RenderHandler GetRenderHandlerForSDL2() {
  DCHECK(GetPlatformFactoryBackend() == PlatformFactoryBackend::kSDL2)
      << "Only backend with SDL2 should use this function.";
  return platform::g_sdl2_render_handler;
}

}  // namespace kiwi::base