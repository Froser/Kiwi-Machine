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

#include "base/platform/qt/qt_platform_factory.h"
#include "base/platform/qt/qt_file_interface.h"
#include "base/platform/qt/qt_runloop_interface.h"
#include "base/platform/qt/qt_single_thread_task_executor_interface.h"
#include "base/platform/qt/qt_thread_interface.h"

namespace kiwi::base {
std::unique_ptr<platform::ThreadInterface>
QtPlatformFactory::CreateThreadInterface(const std::string& thread_name) {
  auto thread_interface = std::make_unique<platform::QtThreadInterface>();
  thread_interface->SetThreadName(thread_name);
  return thread_interface;
}

std::unique_ptr<platform::SingleThreadTaskExecutorInterface>
QtPlatformFactory::CreateSingleThreadTaskExecutor(
    MessagePumpType message_pump_type) {
  return std::make_unique<platform::QtSingleThreadTaskExecutorInterface>(
      message_pump_type);
}

std::unique_ptr<platform::RunLoopInterface>
QtPlatformFactory::CreateRunLoopInterface() {
  return std::make_unique<platform::QtRunLoopInterface>();
}
}  // namespace kiwi::base