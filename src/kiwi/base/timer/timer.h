// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TIMER_TIMER_H_
#define BASE_TIMER_TIMER_H_

#include "base/base_export.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/delayed_task_handle.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"

namespace kiwi::base {

namespace internal {

// This class wraps logic shared by all timers.
class BASE_EXPORT TimerBase {
 public:
  TimerBase(const TimerBase&) = delete;
  TimerBase& operator=(const TimerBase&) = delete;

  virtual ~TimerBase();

  // Returns true if the timer is running (i.e., not stopped).
  bool IsRunning() const;

  // Sets the task runner on which the delayed task should be scheduled when
  // this Timer is running. This method can only be called while this Timer
  // isn't running. If this is used to mock time in tests, the modern and
  // preferred approach is to use TaskEnvironment::TimeSource::MOCK_TIME. To
  // avoid racy usage of Timer, |task_runner| must run tasks on the same
  // sequence which this Timer is bound to (started from). TODO(gab): Migrate
  // callers using this as a test seam to
  // TaskEnvironment::TimeSource::MOCK_TIME.
  virtual void SetTaskRunner(scoped_refptr<SequencedTaskRunner> task_runner);

  // Call this method to stop the timer and cancel all previously scheduled
  // tasks. It is a no-op if the timer is not running.
  virtual void Stop();

 protected:
  // Constructs a timer. Start must be called later to set task info.
  explicit TimerBase(const Location& posted_from = Location());

  virtual void OnStop() = 0;

  // Disables the scheduled task and abandons it so that it no longer refers
  // back to this object.
  void AbandonScheduledTask();

  // Returns the task runner on which the task should be scheduled. If the
  // corresponding |task_runner_| field is null, the task runner for the current
  // sequence is returned.
  scoped_refptr<SequencedTaskRunner> GetTaskRunner();

  // The task runner on which the task should be scheduled. If it is null, the
  // task runner for the current sequence will be used.
  scoped_refptr<SequencedTaskRunner> task_runner_;

  // Timer isn't thread-safe and while it is running, it must only be used on
  // the same sequence until fully Stop()'ed. Once stopped, it may be destroyed
  // or restarted on another sequence.
  // SEQUENCE_CHECKER(sequence_checker_);

  // Location in user code.
  Location posted_from_;  // GUARDED_BY_CONTEXT(sequence_checker_);

  // The handle to the posted delayed task.
  DelayedTaskHandle
      delayed_task_handle_;  // GUARDED_BY_CONTEXT(sequence_checker_);

  // Callback invoked when the timer is ready. This is saved as a member to
  // avoid rebinding every time the Timer fires. Lazy initialized the first time
  // the Timer is started.
  RepeatingClosure timer_callback_;
};

//-----------------------------------------------------------------------------
// This class wraps logic shared by (Retaining)OneShotTimer and RepeatingTimer.
class BASE_EXPORT DelayTimerBase : public TimerBase {
 public:
  DelayTimerBase(const DelayTimerBase&) = delete;
  DelayTimerBase& operator=(const DelayTimerBase&) = delete;

  ~DelayTimerBase() override;

  // Returns the current delay for this timer.
  TimeDelta GetCurrentDelay() const;

  // Call this method to reset the timer delay. The user task must be set. If
  // the timer is not running, this will start it by posting a task.
  virtual void Reset();

  // DEPRECATED. Call Stop() instead.
  // TODO(1262205): Remove this method and all callers.
  void AbandonAndStop();

  TimeTicks desired_run_time() const {
    // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return desired_run_time_;
  }

 protected:
  // Constructs a timer. Start must be called later to set task info.
  // If |tick_clock| is provided, it is used instead of TimeTicks::Now() to get
  // TimeTicks when scheduling tasks.
  explicit DelayTimerBase(const TickClock* tick_clock = nullptr);

  // Construct a timer with task info.
  // If |tick_clock| is provided, it is used instead of TimeTicks::Now() to get
  // TimeTicks when scheduling tasks.
  DelayTimerBase(const Location& posted_from,
                 TimeDelta delay,
                 const TickClock* tick_clock = nullptr);

  virtual void RunUserTask() = 0;

  // Schedules |OnScheduledTaskInvoked()| to run on the current sequence with
  // the given |delay|. |desired_run_time_| is reset to Now() + delay.
  void ScheduleNewTask(TimeDelta delay);

  void StartInternal(const Location& posted_from, TimeDelta delay);

 private:
  // DCHECKs that the user task is not null. Used to diagnose a recurring bug
  // where Reset() is called on a OneShotTimer that has already fired.
  virtual void EnsureNonNullUserTask() = 0;

  // Returns the current tick count.
  TimeTicks Now() const;

  // Called when the scheduled task is invoked. Will run the  |user_task| if the
  // timer is still running and |desired_run_time_| was reached.
  void OnScheduledTaskInvoked();

  // Delay requested by user.
  TimeDelta delay_;  // GUARDED_BY_CONTEXT(sequence_checker_);

  // The desired run time of |user_task_|. The user may update this at any time,
  // even if their previous request has not run yet. This time can be a "zero"
  // TimeTicks if the task must be run immediately.
  TimeTicks desired_run_time_;  // GUARDED_BY_CONTEXT(sequence_checker_);

  // The tick clock used to calculate the run time for scheduled tasks.
  const TickClock* const tick_clock_;
  // GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace internal

//-----------------------------------------------------------------------------
// A simple, repeating timer.  See usage notes at the top of the file.
class BASE_EXPORT RepeatingTimer : public internal::DelayTimerBase {
 public:
  RepeatingTimer();
  explicit RepeatingTimer(const TickClock* tick_clock);

  RepeatingTimer(const RepeatingTimer&) = delete;
  RepeatingTimer& operator=(const RepeatingTimer&) = delete;

  ~RepeatingTimer() override;

  RepeatingTimer(const Location& posted_from,
                 TimeDelta delay,
                 RepeatingClosure user_task);
  RepeatingTimer(const Location& posted_from,
                 TimeDelta delay,
                 RepeatingClosure user_task,
                 const TickClock* tick_clock);

  // Start the timer to run at the given |delay| from now. If the timer is
  // already running, it will be replaced to call the given |user_task|.
  virtual void Start(const Location& posted_from,
                     TimeDelta delay,
                     RepeatingClosure user_task);

  // Start the timer to run at the given |delay| from now. If the timer is
  // already running, it will be replaced to call a task formed from
  // |receiver->*method|.
  template <class Receiver>
  void Start(const Location& posted_from,
             TimeDelta delay,
             Receiver* receiver,
             void (Receiver::*method)()) {
    Start(posted_from, delay, BindRepeating(method, Unretained(receiver)));
  }

  const RepeatingClosure& user_task() const { return user_task_; }

 private:
  // Mark this final, so that the destructor can call this safely.
  void OnStop() final;
  void RunUserTask() override;
  void EnsureNonNullUserTask() final;

  RepeatingClosure user_task_;
};

}  // namespace kiwi::base

#endif  // BASE_TIMER_TIMER_H_