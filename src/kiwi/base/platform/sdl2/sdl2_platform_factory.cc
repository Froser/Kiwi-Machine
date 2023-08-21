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

#include "base/platform/sdl2/sdl2_platform_factory.h"

#include "base/platform/default/default_file_interface.h"
#include "base/platform/sdl2/sdl2_runloop_interface.h"
#include "base/platform/sdl2/sdl2_single_thread_task_executor_interface.h"
#include "base/platform/sdl2/sdl2_thread_interface.h"

namespace kiwi::base {
std::unique_ptr<platform::ThreadInterface>
SDL2PlatformFactory::CreateThreadInterface(const std::string& thread_name) {
  auto thread_interface = std::make_unique<platform::SDL2ThreadInterface>();
  thread_interface->SetThreadName(thread_name);
  return thread_interface;
}

std::unique_ptr<platform::FileInterface>
SDL2PlatformFactory::CreateFileInterface(const FilePath& file_path,
                                         uint32_t flags) {
  auto file_interface = std::make_unique<platform::DefaultFileInterface>();
  file_interface->Open(file_path, flags);
  return file_interface;
}

std::unique_ptr<platform::SingleThreadTaskExecutorInterface>
SDL2PlatformFactory::CreateSingleThreadTaskExecutor(
    MessagePumpType message_pump_type) {
  return std::make_unique<platform::SDL2SingleThreadTaskExecutorInterface>(
      message_pump_type);
}

std::unique_ptr<platform::RunLoopInterface>
SDL2PlatformFactory::CreateRunLoopInterface() {
  return std::make_unique<platform::SDL2RunLoopInterface>();
}
}  // namespace kiwi::base