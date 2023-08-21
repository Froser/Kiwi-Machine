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

#ifndef BASE_TASK_SINGLE_THREAD_TASK_RUNNER_H_
#define BASE_TASK_SINGLE_THREAD_TASK_RUNNER_H_

#include "base/base_export.h"
#include "base/task/sequenced_task_runner.h"

namespace kiwi::base {

class BASE_EXPORT SingleThreadTaskRunner : public SequencedTaskRunner {
 public:
  SingleThreadTaskRunner();

  // A more explicit alias to RunsTasksInCurrentSequence().
  bool BelongsToCurrentThread() const { return RunsTasksInCurrentSequence(); }

  [[nodiscard]] static const scoped_refptr<SingleThreadTaskRunner>&
  GetCurrentDefault();

  [[nodiscard]] static bool HasCurrentDefault();

  static void SetCurrentDefault(
      scoped_refptr<SingleThreadTaskRunner> task_runner);

 protected:
  ~SingleThreadTaskRunner() override = default;
};

}  // namespace kiwi::base

#endif  // BASE_TASK_SINGLE_THREAD_TASK_RUNNER_H_
