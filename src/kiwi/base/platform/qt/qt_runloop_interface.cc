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

#include "base/platform/qt/qt_runloop_interface.h"

#include <QCoreApplication>

#include "base/functional/bind.h"

namespace kiwi::base {
namespace platform {
namespace {
// When g_runloop_nest_count is 1, it means we run the loop in QCoreApplication.
// When g_runloop_nest_count is more than 1, it means we run the loop in
// QEventLoop.
thread_local int g_runloop_nest_count = 0;
}  // namespace

QtRunLoopInterface::QtRunLoopInterface() {
  ++g_runloop_nest_count;
  if (g_runloop_nest_count > 1)
    event_loop_ = std::make_unique<QEventLoop>();
}
QtRunLoopInterface::~QtRunLoopInterface() {
  --g_runloop_nest_count;
}

RepeatingClosure QtRunLoopInterface::QuitClosure() {
  DCHECK_GE(g_runloop_nest_count, 1);
  if (g_runloop_nest_count == 1)
    return base::BindRepeating([]() { QCoreApplication::quit(); });

  CHECK(event_loop_);
  return base::BindRepeating(&QEventLoop::quit,
                             base::Unretained(event_loop_.get()));
}

void QtRunLoopInterface::Run() {
  DCHECK_GE(g_runloop_nest_count, 1);
  if (g_runloop_nest_count == 1)
    QCoreApplication::exec();

  CHECK(event_loop_);
  event_loop_->exec();
}

}  // namespace platform
}  // namespace kiwi::base