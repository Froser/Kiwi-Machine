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

#ifndef BASE_THREADING_THREAD_H_
#define BASE_THREADING_THREAD_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/base_export.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"

namespace kiwi::base {
namespace platform {
class ThreadInterface;
}
class BASE_EXPORT Thread {
 public:
  struct BASE_EXPORT Options {
    Options();
    Options(MessagePumpType type, size_t size);
    Options(Options&& other);
    Options& operator=(Options&& other);
    ~Options();

    // Specifies the type of message pump that will be allocated on the thread.
    // This is ignored if message_pump_factory.is_null() is false.
    MessagePumpType message_pump_type = MessagePumpType::DEFAULT;

    size_t stack_size = 0;
  };

  // Constructor.
  // name is a display string to identify the thread.
  explicit Thread(const std::string& name);

  Thread(const Thread&) = delete;
  Thread& operator=(const Thread&) = delete;

  ~Thread();

  bool StartWithOptions(Options options);

  void Stop();

  scoped_refptr<SingleThreadTaskRunner> task_runner() const;

 private:
  std::unique_ptr<platform::ThreadInterface> thread_interface_;
};

}  // namespace kiwi::base

#endif  // BASE_THREADING_THREAD_H_
