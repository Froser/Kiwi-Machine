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

#include "base/platform/qt/qt_single_thread_task_executor_interface.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QThread>

#include "base/platform/qt/qt_single_thread_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"

namespace kiwi::base {
namespace platform {
QtSingleThreadTaskExecutorInterface::QtSingleThreadTaskExecutorInterface(
    MessagePumpType type) {
  int& argc = GetStartupArgc();
  char** argv = GetStartupArgv();

  if (!QCoreApplication::instance()) {
    if (type == MessagePumpType::DEFAULT) {
      application_ = std::make_unique<QCoreApplication>(argc, argv);
    } else if (type == MessagePumpType::UI) {
      application_ = std::make_unique<QGuiApplication>(argc, argv);
    } else {
      CHECK(false) << "Invalid MessagePumpType";
    }
  } else {
    CHECK(QCoreApplication::instance()->thread() == QThread::currentThread())
        << "QtSingleThreadTaskExecutorInterface must run on the main thread "
           "when QCoreApplication already exists.";
  }

  CreateTaskRunner();
}

void QtSingleThreadTaskExecutorInterface::CreateTaskRunner() {
  scoped_refptr<QtSingleThreadTaskRunner> task_runner =
      MakeRefCounted<QtSingleThreadTaskRunner>();
  DCHECK(!SequencedTaskRunner::HasCurrentDefault());
  SequencedTaskRunner::SetCurrentDefault(task_runner);
  DCHECK(!SingleThreadTaskRunner::HasCurrentDefault());
  SingleThreadTaskRunner::SetCurrentDefault(task_runner);
}

QtSingleThreadTaskExecutorInterface::~QtSingleThreadTaskExecutorInterface() {
  SequencedTaskRunner::SetCurrentDefault(nullptr);
  SingleThreadTaskRunner::SetCurrentDefault(nullptr);
}

}  // namespace platform
}  // namespace kiwi::base