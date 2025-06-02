// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/timer.h"

#include "base/check.h"
#include "base/task/sequenced_task_runner.h"

namespace kiwi::base {
namespace internal {

TimerBase::TimerBase(const Location& posted_from) : posted_from_(posted_from) {
  // It is safe for the timer to be created on a different thread/sequence than
  // the one from which the timer APIs are called. The first call to the
  // checker's CalledOnValidSequence() method will re-bind the checker, and
  // later calls will verify that the same task runner is used.
  // DETACH_FROM_SEQUENCE(sequence_checker_);
}

TimerBase::~TimerBase() {
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AbandonScheduledTask();
}

bool TimerBase::IsRunning() const {
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delayed_task_handle_.IsValid();
}

void TimerBase::SetTaskRunner(scoped_refptr<SequencedTaskRunner> task_runner) {
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  DCHECK(!IsRunning());
  task_runner_.swap(task_runner);
}

scoped_refptr<SequencedTaskRunner> TimerBase::GetTaskRunner() {
  return task_runner_ ? task_runner_ : SequencedTaskRunner::GetCurrentDefault();
}

void TimerBase::Stop() {
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AbandonScheduledTask();

  OnStop();
  // No more member accesses here: |this| could be deleted after Stop() call.
}

void TimerBase::AbandonScheduledTask() {
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (delayed_task_handle_.IsValid())
    delayed_task_handle_.CancelTask();

  // It's safe to destroy or restart Timer on another sequence after the task is
  // abandoned.
  // DETACH_FROM_SEQUENCE(sequence_checker_);
}

DelayTimerBase::DelayTimerBase(const TickClock* tick_clock)
    : tick_clock_(tick_clock) {}

DelayTimerBase::DelayTimerBase(const Location& posted_from,
                               TimeDelta delay,
                               const TickClock* tick_clock)
    : TimerBase(posted_from), delay_(delay), tick_clock_(tick_clock) {}

DelayTimerBase::~DelayTimerBase() = default;

TimeDelta DelayTimerBase::GetCurrentDelay() const {
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delay_;
}

void DelayTimerBase::StartInternal(const Location& posted_from,
                                   TimeDelta delay) {
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  posted_from_ = posted_from;
  delay_ = delay;

  Reset();
}

void DelayTimerBase::AbandonAndStop() {
  Stop();
}

void DelayTimerBase::Reset() {
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  EnsureNonNullUserTask();

  // We can't reuse the |scheduled_task_|, so abandon it and post a new one.
  AbandonScheduledTask();
  ScheduleNewTask(delay_);
}

void DelayTimerBase::ScheduleNewTask(TimeDelta delay) {
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!delayed_task_handle_.IsValid());

  // Ignore negative deltas.
  // TODO(pmonette): Fix callers providing negative deltas and ban passing them.
  if (delay < TimeDelta())
    delay = TimeDelta();

  if (!timer_callback_) {
    timer_callback_ = BindRepeating(&DelayTimerBase::OnScheduledTaskInvoked,
                                    Unretained(this));
  }
  delayed_task_handle_ = GetTaskRunner()->PostCancelableDelayedTask(
      base::subtle::PostDelayedTaskPassKey(), posted_from_, timer_callback_,
      delay);
  desired_run_time_ = Now() + delay;
}

TimeTicks DelayTimerBase::Now() const {
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tick_clock_ ? tick_clock_->NowTicks() : TimeTicks::Now();
}

void DelayTimerBase::OnScheduledTaskInvoked() {
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!delayed_task_handle_.IsValid()) << posted_from_.ToString();

  RunUserTask();
  // No more member accesses here: |this| could be deleted at this point.
}

}  // namespace internal


RepeatingTimer::RepeatingTimer() = default;
RepeatingTimer::RepeatingTimer(const TickClock* tick_clock)
    : internal::DelayTimerBase(tick_clock) {}
RepeatingTimer::~RepeatingTimer() = default;

RepeatingTimer::RepeatingTimer(const Location& posted_from,
                               TimeDelta delay,
                               RepeatingClosure user_task)
    : internal::DelayTimerBase(posted_from, delay),
      user_task_(std::move(user_task)) {}
RepeatingTimer::RepeatingTimer(const Location& posted_from,
                               TimeDelta delay,
                               RepeatingClosure user_task,
                               const TickClock* tick_clock)
    : internal::DelayTimerBase(posted_from, delay, tick_clock),
      user_task_(std::move(user_task)) {}

void RepeatingTimer::Start(const Location& posted_from,
                           TimeDelta delay,
                           RepeatingClosure user_task) {
  user_task_ = std::move(user_task);
  StartInternal(posted_from, delay);
}

void RepeatingTimer::OnStop() {}

void RepeatingTimer::RunUserTask() {
  // Make a local copy of the task to run in case the task destroy the timer
  // instance.
  RepeatingClosure task = user_task_;
  ScheduleNewTask(GetCurrentDelay());
  task.Run();
  // No more member accesses here: |this| could be deleted at this point.
}

void RepeatingTimer::EnsureNonNullUserTask() {
  DCHECK(user_task_);
}

}  // namespace kiwi::base