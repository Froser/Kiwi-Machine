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

#ifndef BASE_PLATFORM_QT_QT_RUNLOOP_INTERFACE_H_
#define BASE_PLATFORM_QT_QT_RUNLOOP_INTERFACE_H_

#include <memory>

#include <QEventLoop>

#include "base/platform/platform_factory.h"

namespace kiwi::base {
namespace platform {
class QtRunLoopInterface : public RunLoopInterface {
 public:
  QtRunLoopInterface();
  ~QtRunLoopInterface() override;

 private:
  RepeatingClosure QuitClosure() override;
  void Run() override;

 private:
  std::unique_ptr<QEventLoop> event_loop_;
};
}  // namespace platform
}  // namespace kiwi::base

#endif  // BASE_PLATFORM_QT_QT_RUNLOOP_INTERFACE_H_