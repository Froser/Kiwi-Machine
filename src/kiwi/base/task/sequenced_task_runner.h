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

#ifndef BASE_TASK_SEQUENCED_TASK_RUNNER_H_
#define BASE_TASK_SEQUENCED_TASK_RUNNER_H_

#include "base/base_export.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task/post_task_and_reply_with_result_internal.h"
#include "base/time/time.h"

namespace kiwi::base {

class BASE_EXPORT SequencedTaskRunner
    : public RefCountedThreadSafe<SequencedTaskRunner> {
 public:
  SequencedTaskRunner();

  // Posts the given task to be run.  Returns true if the task may be
  // run at some point in the future, and false if the task definitely
  // will not be run.
  //
  // Equivalent to PostDelayedTask(from_here, task, 0).
  bool PostTask(const Location& from_here, OnceClosure task);

  // Like PostTask, but tries to run the posted task only after |delay_ms|
  // has passed. Implementations should use a tick clock, rather than wall-
  // clock time, to implement |delay|.
  virtual bool PostDelayedTask(const Location& from_here,
                               OnceClosure task,
                               base::TimeDelta delay) = 0;

  // Posts |task| on the current TaskRunner.  On completion, |reply| is posted
  // to the sequence that called PostTaskAndReply().  On the success case,
  // |task| is destroyed on the target sequence and |reply| is destroyed on the
  // originating sequence immediately after their invocation.  If an error
  // happened on the onward PostTask, both |task| and |reply| are destroyed on
  // the originating sequence, and on an error on the backward PostTask, |reply|
  // is leaked rather than being destroyed on the wrong sequence.  This allows
  // objects that must be deleted on the originating sequence to be bound into
  // the |reply| Closures.  In particular, it can be useful to use WeakPtr<> in
  // the |reply| Closure so that the reply operation can be canceled. See the
  // following pseudo-code:
  //
  // class DataBuffer : public RefCountedThreadSafe<DataBuffer> {
  //  public:
  //   // Called to add data into a buffer.
  //   void AddData(void* buf, size_t length);
  //   ...
  // };
  //
  //
  // class DataLoader : public SupportsWeakPtr<DataLoader> {
  //  public:
  //    void GetData() {
  //      scoped_refptr<DataBuffer> buffer = new DataBuffer();
  //      target_thread_.task_runner()->PostTaskAndReply(
  //          FROM_HERE,
  //          base::BindOnce(&DataBuffer::AddData, buffer),
  //          base::BindOnce(&DataLoader::OnDataReceived, AsWeakPtr(), buffer));
  //    }
  //
  //  private:
  //    void OnDataReceived(scoped_refptr<DataBuffer> buffer) {
  //      // Do something with buffer.
  //    }
  // };
  //
  //
  // Things to notice:
  //   * Results of |task| are shared with |reply| by binding a shared argument
  //     (a DataBuffer instance).
  //   * The DataLoader object has no special thread safety.
  //   * The DataLoader object can be deleted while |task| is still running,
  //     and the reply will cancel itself safely because it is bound to a
  //     WeakPtr<>.
  virtual bool PostTaskAndReply(const Location& from_here,
                                OnceClosure task,
                                OnceClosure reply) = 0;

  // When you have these methods
  //
  //   R DoWorkAndReturn();
  //   void Callback(const R& result);
  //
  // and want to call them in a PostTaskAndReply kind of fashion where the
  // result of DoWorkAndReturn is passed to the Callback, you can use
  // PostTaskAndReplyWithResult as in this example:
  //
  // PostTaskAndReplyWithResult(
  //     target_thread_.task_runner(),
  //     FROM_HERE,
  //     BindOnce(&DoWorkAndReturn),
  //     BindOnce(&Callback));
  //
  // Templating on the types of `task` and `reply` allows template matching to
  // work for both base::RepeatingCallback and base::OnceCallback in each case.
  template <typename TaskReturnType,
            typename ReplyArgType,
            template <typename>
            class TaskCallbackType,
            template <typename>
            class ReplyCallbackType,
            typename = EnableIfIsBaseCallback<TaskCallbackType>,
            typename = EnableIfIsBaseCallback<ReplyCallbackType>>
  bool PostTaskAndReplyWithResult(const Location& from_here,
                                  TaskCallbackType<TaskReturnType()> task,
                                  ReplyCallbackType<void(ReplyArgType)> reply) {
    // DCHECK(task);
    // DCHECK(reply);
    // std::unique_ptr used to avoid the need of a default constructor.
    auto* result = new std::unique_ptr<TaskReturnType>();
    return PostTaskAndReply(
        from_here,
        BindOnce(&internal::ReturnAsParamAdapter<TaskReturnType>,
                 std::move(task), result),
        BindOnce(&internal::ReplyAdapter<TaskReturnType, ReplyArgType>,
                 std::move(reply), Owned(result)));
  }

  // Returns true iff tasks posted to this TaskRunner are sequenced
  // with this call.
  //
  // In particular:
  // - Returns true if this is a SequencedTaskRunner to which the
  //   current task was posted.
  // - Returns true if this is a SequencedTaskRunner bound to the
  //   same sequence as the SequencedTaskRunner to which the current
  //   task was posted.
  // - Returns true if this is a SingleThreadTaskRunner bound to
  //   the current thread.
  bool RunsTasksInCurrentSequence() const;

  [[nodiscard]] static const scoped_refptr<SequencedTaskRunner>&
  GetCurrentDefault();

  [[nodiscard]] static bool HasCurrentDefault();

  static void SetCurrentDefault(scoped_refptr<SequencedTaskRunner> task_runner);

 protected:
  virtual ~SequencedTaskRunner() = default;

  friend class RefCountedThreadSafe<SequencedTaskRunner>;
};

}  // namespace kiwi::base

#endif  // BASE_TASK_SEQUENCED_TASK_RUNNER_H_
