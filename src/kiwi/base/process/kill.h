// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains routines to kill processes and get the exit code and
// termination status.

#ifndef BASE_PROCESS_KILL_H_
#define BASE_PROCESS_KILL_H_

#include "base/base_export.h"
#include "base/files/file_path.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace kiwi::base {

class ProcessFilter;

#if BUILDFLAG(IS_WIN)
namespace win {

// See definition in sandbox/win/src/sandbox_types.h
const DWORD kSandboxFatalMemoryExceeded = 7012;

// Exit codes with special meanings on Windows.
const DWORD kNormalTerminationExitCode = 0;
const DWORD kDebuggerInactiveExitCode = 0xC0000354;
const DWORD kKeyboardInterruptExitCode = 0xC000013A;
const DWORD kDebuggerTerminatedExitCode = 0x40010004;
const DWORD kStatusInvalidImageHashExitCode = 0xC0000428;

// This exit code is used by the Windows task manager when it kills a
// process.  It's value is obviously not that unique, and it's
// surprising to me that the task manager uses this value, but it
// seems to be common practice on Windows to test for it as an
// indication that the task manager has killed something if the
// process goes away.
const DWORD kProcessKilledExitCode = 1;

}  // namespace win

#endif  // BUILDFLAG(IS_WIN)

// Return status values from GetTerminationStatus.  Don't use these as
// exit code arguments to KillProcess*(), use platform/application
// specific values instead.
enum TerminationStatus {
  // clang-format off
  TERMINATION_STATUS_NORMAL_TERMINATION,   // zero exit status
  TERMINATION_STATUS_ABNORMAL_TERMINATION, // non-zero exit status
  TERMINATION_STATUS_PROCESS_WAS_KILLED,   // e.g. SIGKILL or task manager kill
  TERMINATION_STATUS_PROCESS_CRASHED,      // e.g. Segmentation fault
  TERMINATION_STATUS_STILL_RUNNING,        // child hasn't exited yet
#if BUILDFLAG(IS_CHROMEOS)
  // Used for the case when oom-killer kills a process on ChromeOS.
  TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM,
#endif
#if BUILDFLAG(IS_ANDROID)
  // On Android processes are spawned from the system Zygote and we do not get
  // the termination status.  We can't know if the termination was a crash or an
  // oom kill for sure, but we can use status of the strong process bindings as
  // a hint.
  TERMINATION_STATUS_OOM_PROTECTED,        // child was protected from oom kill
#endif
  TERMINATION_STATUS_LAUNCH_FAILED,        // child process never launched
  TERMINATION_STATUS_OOM,                  // Process died due to oom
#if BUILDFLAG(IS_WIN)
  // On Windows, the OS terminated process due to code integrity failure.
  TERMINATION_STATUS_INTEGRITY_FAILURE,
#endif
  TERMINATION_STATUS_MAX_ENUM
  // clang-format on
};

// Attempts to kill all the processes on the current machine that were launched
// from the given executable name, ending them with the given exit code.  If
// filter is non-null, then only processes selected by the filter are killed.
// Returns true if all processes were able to be killed off, false if at least
// one couldn't be killed.
BASE_EXPORT bool KillProcesses(const FilePath::StringType& executable_name,
                               int exit_code,
                               const ProcessFilter* filter);

// Get the termination status of the process by interpreting the
// circumstances of the child process' death. |exit_code| is set to
// the status returned by waitpid() on POSIX, and from GetExitCodeProcess() on
// Windows, and may not be null.  Note that on Linux, this function
// will only return a useful result the first time it is called after
// the child exits (because it will reap the child and the information
// will no longer be available).
BASE_EXPORT TerminationStatus GetTerminationStatus(ProcessHandle handle,
                                                   int* exit_code);


}  // namespace base

#endif  // BASE_PROCESS_KILL_H_
