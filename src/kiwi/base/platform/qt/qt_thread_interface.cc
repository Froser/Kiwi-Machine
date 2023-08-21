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

#include "base/platform/qt/qt_thread_interface.h"

#include <functional>

#include "base/platform/qt/qt_single_thread_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"

namespace kiwi::base {
namespace platform {
QtThreadInterface::QtThreadInterface() : thread_(std::make_unique<QThread>()) {}

QtThreadInterface::~QtThreadInterface() {
  QThread* thread = thread_.release();
  QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
  thread->exit();
}

void QtThreadInterface::SetThreadName(const std::string& name) {
  thread_->setObjectName(QString::fromUtf8(name.data()));
}

bool QtThreadInterface::StartWithOptions(Thread::Options options) {
  scoped_refptr<QtSingleThreadTaskRunner> task_runner =
      MakeRefCounted<QtSingleThreadTaskRunner>();

  // Store task runner before the thread start.
  task_runner_ = task_runner;

  // Capture task runner here is safe. QtThreadInterface owns qt thread instance
  // and task runners, they have the same lifecycle.
  QObject::connect(thread_.get(), &QThread::started, [this]() {
    SingleThreadTaskRunner::SetCurrentDefault(task_runner_);
    SequencedTaskRunner::SetCurrentDefault(task_runner_);
  });
  QObject::connect(thread_.get(), &QThread::finished, [this]() {
    SingleThreadTaskRunner::SetCurrentDefault(nullptr);
    SequencedTaskRunner::SetCurrentDefault(nullptr);
  });

  thread_->setStackSize(static_cast<uint>(options.stack_size));
  thread_->stackSize();
  thread_->start();

  DCHECK(task_runner);
  task_runner->moveToThread(thread_.get());
  return true;
}

void QtThreadInterface::Stop() {
  thread_->exit();
}

scoped_refptr<SingleThreadTaskRunner> QtThreadInterface::task_runner() const {
  return task_runner_;
}

}  // namespace platform
}  // namespace kiwi::base
