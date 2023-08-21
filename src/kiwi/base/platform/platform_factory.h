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

// Platform factory is an abstract interface to create concrete implementations
// of threading, task runners, etc.

#ifndef BASE_PLATFORM_PLATFORM_FACTORY_H_
#define BASE_PLATFORM_PLATFORM_FACTORY_H_

#include <memory>
#include <string>

#include "base/base_export.h"
#include "base/message_loop/message_pump_type.h"
#include "base/threading/thread.h"

union SDL_Event;

namespace kiwi::base {
class FilePath;

namespace platform {
class BASE_EXPORT ThreadInterface {
 public:
  ThreadInterface();
  virtual ~ThreadInterface();

  virtual bool StartWithOptions(Thread::Options options) = 0;
  virtual void Stop() = 0;
  virtual scoped_refptr<SingleThreadTaskRunner> task_runner() const = 0;
};

class BASE_EXPORT FileInterface {
 public:
  FileInterface();
  virtual ~FileInterface();

 public:
  virtual int ReadAtCurrentPos(char* data, int size) = 0;
  virtual bool IsValid() const = 0;
};

class BASE_EXPORT RunLoopInterface {
 public:
  RunLoopInterface();
  virtual ~RunLoopInterface();

 public:
  virtual RepeatingClosure QuitClosure() = 0;
  virtual void Run() = 0;
};

class BASE_EXPORT SingleThreadTaskExecutorInterface {
 public:
  SingleThreadTaskExecutorInterface();
  virtual ~SingleThreadTaskExecutorInterface();
};
}  // namespace platform

class PlatformFactory {
 public:
  virtual std::unique_ptr<platform::ThreadInterface> CreateThreadInterface(
      const std::string& thread_name) = 0;

  virtual std::unique_ptr<platform::FileInterface> CreateFileInterface(
      const FilePath& file_path,
      uint32_t flags) = 0;

  // An application must run CreateSingleThreadTaskExecutor(int&, char**,
  // MessagePumpType) or CreateSingleThreadTaskExecutor() once.
  // If
  virtual std::unique_ptr<platform::SingleThreadTaskExecutorInterface>
  CreateSingleThreadTaskExecutor(MessagePumpType message_pump_type) = 0;

  virtual std::unique_ptr<platform::RunLoopInterface>
  CreateRunLoopInterface() = 0;
};

enum class PlatformFactoryBackend {
  kSDL2,
  kQt6,
};

BASE_EXPORT PlatformFactoryBackend InitializePlatformFactory(
    int& argc,
    char** argv,
    PlatformFactoryBackend backend = PlatformFactoryBackend::kSDL2);

BASE_EXPORT PlatformFactoryBackend GetPlatformFactoryBackend();

BASE_EXPORT PlatformFactory* GetPlatformFactory();

BASE_EXPORT int& GetStartupArgc();
BASE_EXPORT char** GetStartupArgv();

using SDL2EventHandler = RepeatingCallback<void(SDL_Event*)>;
using SDL2RenderHandler = RepeatingClosure;
using SDL2PostEventHandler = RepeatingClosure;
BASE_EXPORT void SetPreEventHandlerForSDL2(SDL2EventHandler handler);
BASE_EXPORT SDL2EventHandler GetPreEventHandlerForSDL2();
BASE_EXPORT void SetEventHandlerForSDL2(SDL2EventHandler handler);
BASE_EXPORT SDL2EventHandler GetEventHandlerForSDL2();
BASE_EXPORT void SetPostEventHandlerForSDL2(SDL2PostEventHandler handler);
BASE_EXPORT SDL2PostEventHandler GetPostEventHandlerForSDL2();
BASE_EXPORT void SetRenderHandlerForSDL2(SDL2RenderHandler handler);
BASE_EXPORT SDL2RenderHandler GetRenderHandlerForSDL2();

}  // namespace kiwi::base

#endif  // BASE_PLATFORM_PLATFORM_FACTORY_H_