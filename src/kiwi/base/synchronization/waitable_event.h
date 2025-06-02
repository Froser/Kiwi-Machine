// Copyright (C) 2025 Yisi Yu
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

#ifndef BASE_SYNCHRONIZATION_WAITABLE_EVENT_H_
#define BASE_SYNCHRONIZATION_WAITABLE_EVENT_H_

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

#include <mutex>

namespace kiwi::base {

// A WaitableEvent can be a useful thread synchronization tool when you want to
// allow one thread to wait for another thread to finish some work. For
// non-Windows systems, this can only be used from within a single address
// space.
//
// Use a WaitableEvent when you would otherwise use a Lock+ConditionVariable to
// protect a simple boolean value.  However, if you find yourself using a
// WaitableEvent in conjunction with a Lock to wait for a more complex state
// change (e.g., for an item to be added to a queue), then you should probably
// be using a ConditionVariable instead of a WaitableEvent.
//
// NOTE: On Windows, this class provides a subset of the functionality afforded
// by a Windows event object.  This is intentional.  If you are writing Windows
// specific code and you need other features of a Windows event, then you might
// be better off just using an Windows event directly.
class BASE_EXPORT WaitableEvent {
 public:
  // Indicates whether a WaitableEvent should automatically reset the event
  // state after a single waiting thread has been released or remain signaled
  // until Reset() is manually invoked.
  enum class ResetPolicy { MANUAL, AUTOMATIC };

  // Indicates whether a new WaitableEvent should start in a signaled state or
  // not.
  enum class InitialState { SIGNALED, NOT_SIGNALED };

  // Constructs a WaitableEvent with policy and initial state as detailed in
  // the above enums.
  WaitableEvent(ResetPolicy reset_policy = ResetPolicy::MANUAL,
                InitialState initial_state = InitialState::NOT_SIGNALED);

  WaitableEvent(const WaitableEvent&) = delete;
  WaitableEvent& operator=(const WaitableEvent&) = delete;

  ~WaitableEvent();

  // Put the event in the un-signaled state.
  void Reset();

  // Put the event in the signaled state.  Causing any thread blocked on Wait
  // to be woken up.
  void Signal();

  // Returns true if the event is in the signaled state, else false.  If this
  // is not a manual reset event, then this test will cause a reset.
  bool IsSignaled();

  // Wait indefinitely for the event to be signaled. Wait's return "happens
  // after" |Signal| has completed. This means that it's safe for a
  // WaitableEvent to synchronise its own destruction, like this:
  //
  //   WaitableEvent *e = new WaitableEvent;
  //   SendToOtherThread(e);
  //   e->Wait();
  //   delete e;
  NOT_TAIL_CALLED void Wait();

 private:
  std::condition_variable cv_;
  std::mutex mutex_;
  ResetPolicy reset_policy_;
  InitialState initial_state_;
  bool signaled_;
};

}  // namespace kiwi::base

#endif  // BASE_SYNCHRONIZATION_WAITABLE_EVENT_H_
