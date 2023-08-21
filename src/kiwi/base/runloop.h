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

#ifndef BASE_RUNLOOP_H_
#define BASE_RUNLOOP_H_

#include <memory>

#include "base/base_export.h"
#include "platform/platform_factory.h"

namespace kiwi::base {
class BASE_EXPORT RunLoop {
 public:
  RunLoop();
  ~RunLoop();

 public:
  RepeatingClosure QuitClosure();
  void Run();

 private:
  std::unique_ptr<platform::RunLoopInterface> runloop_;
};
}  // namespace kiwi::base

#endif  // BASE_RUNLOOP_H_
