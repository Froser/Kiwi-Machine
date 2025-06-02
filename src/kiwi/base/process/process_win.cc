// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include "base/immediate_crash.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/win/windows_version.h"

#include <windows.h>

namespace {

DWORD kBasicProcessAccess =
    PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | SYNCHRONIZE;

}  // namespace

namespace kiwi::base {

Process::Process(ProcessHandle handle)
    : process_(handle), is_current_process_(false) {
  CHECK_NE(handle, ::GetCurrentProcess());
}

Process::Process(Process&& other)
    : process_(other.process_.release()),
      is_current_process_(other.is_current_process_) {
  other.Close();
}

Process::~Process() {}

Process& Process::operator=(Process&& other) {
  DCHECK_NE(this, &other);
  process_.Set(other.process_.release());
  is_current_process_ = other.is_current_process_;
  other.Close();
  return *this;
}

// static
Process Process::Current() {
  Process process;
  process.is_current_process_ = true;
  return process;
}

// static
Process Process::Open(ProcessId pid) {
  return Process(::OpenProcess(kBasicProcessAccess, FALSE, pid));
}

// static
Process Process::OpenWithAccess(ProcessId pid, DWORD desired_access) {
  return Process(::OpenProcess(desired_access, FALSE, pid));
}

// static
void Process::TerminateCurrentProcessImmediately(int exit_code) {
  ::TerminateProcess(GetCurrentProcess(), static_cast<UINT>(exit_code));
  // There is some ambiguity over whether the call above can return. Rather than
  // hitting confusing crashes later on we should crash right here.
  ImmediateCrash();
}

bool Process::IsValid() const {
  return process_.is_valid() || is_current();
}

ProcessHandle Process::Handle() const {
  return is_current_process_ ? GetCurrentProcess() : process_.get();
}

ProcessId Process::Pid() const {
  DCHECK(IsValid());
  return GetProcId(Handle());
}

bool Process::is_current() const {
  return is_current_process_;
}

void Process::Close() {
  is_current_process_ = false;
  if (!process_.is_valid())
    return;

  process_.Close();
}

bool Process::WaitForExit(int* exit_code) const {
  return WaitForExitWithTimeout(TimeDelta::Max(), exit_code);
}

bool Process::WaitForExitWithTimeout(TimeDelta timeout, int* exit_code) const {
  if (!timeout.is_zero()) {
    // Assert that this thread is allowed to wait below. This intentionally
    // doesn't use ScopedBlockingCallWithBaseSyncPrimitives because the process
    // being waited upon tends to itself be using the CPU and considering this
    // thread non-busy causes more issue than it fixes: http://crbug.com/905788
    // internal::AssertBaseSyncPrimitivesAllowed();
  }

  // Limit timeout to INFINITE.
  DWORD timeout_ms = saturated_cast<DWORD>(timeout.InMilliseconds());
  if (::WaitForSingleObject(Handle(), timeout_ms) != WAIT_OBJECT_0)
    return false;

  DWORD temp_code;  // Don't clobber out-parameters in case of failure.
  if (!::GetExitCodeProcess(Handle(), &temp_code))
    return false;

  if (exit_code)
    *exit_code = static_cast<int>(temp_code);

  Exited(static_cast<int>(temp_code));
  return true;
}

bool Process::Terminate(int exit_code, bool wait) const {
  constexpr DWORD kWaitMs = 60 * 1000;

  DCHECK(IsValid());
  bool result =
      ::TerminateProcess(Handle(), static_cast<UINT>(exit_code)) != FALSE;
  if (result) {
    // The process may not end immediately due to pending I/O
    if (wait && ::WaitForSingleObject(Handle(), kWaitMs) != WAIT_OBJECT_0)
      // DPLOG(ERROR) << "Error waiting for process exit";
    Exited(exit_code);
  } else {
    // The process can't be terminated, perhaps because it has already exited or
    // is in the process of exiting. An error code of ERROR_ACCESS_DENIED is the
    // undocumented-but-expected result if the process has already exited or
    // started exiting when TerminateProcess is called, so don't print an error
    // message in that case.
    if (GetLastError() != ERROR_ACCESS_DENIED)
      // DPLOG(ERROR) << "Unable to terminate process";

    // A non-zero timeout is necessary here for the same reasons as above.
    if (::WaitForSingleObject(Handle(), kWaitMs) == WAIT_OBJECT_0) {
      DWORD actual_exit;
      Exited(::GetExitCodeProcess(Handle(), &actual_exit)
                 ? static_cast<int>(actual_exit)
                 : exit_code);
      result = true;
    }
  }
  return result;
}

void Process::Exited(int exit_code) const {}

}  // namespace kiwi::base
