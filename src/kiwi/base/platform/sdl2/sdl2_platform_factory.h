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

#ifndef BASE_PLATFORM_SDL2_SDL2_PLATFORM_FACTORY_H_
#define BASE_PLATFORM_SDL2_SDL2_PLATFORM_FACTORY_H_

#include "base/platform/platform_factory.h"

namespace kiwi::base {
class SDL2PlatformFactory : public PlatformFactory {
 public:
  std::unique_ptr<platform::ThreadInterface> CreateThreadInterface(
      const std::string& thread_name) override;

  virtual std::unique_ptr<platform::SingleThreadTaskExecutorInterface>
  CreateSingleThreadTaskExecutor(MessagePumpType message_pump_type) override;

  std::unique_ptr<platform::RunLoopInterface> CreateRunLoopInterface() override;
};
}  // namespace kiwi::base

#endif  // BASE_PLATFORM_SDL2_SDL2_PLATFORM_FACTORY_H_