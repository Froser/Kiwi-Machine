// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_HANDLE_VERIFIER_H_
#define BASE_WIN_SCOPED_HANDLE_VERIFIER_H_

#include <memory>
#include <unordered_map>

#include "base/base_export.h"
// #include "base/debug/stack_trace.h"
#include "base/hash/hash.h"
// #include "base/memory/raw_ptr.h"
#include "base/synchronization/lock_impl.h"
#include "base/win/windows_types.h"

namespace kiwi::base {
namespace win {
enum class HandleOperation;
namespace internal {

struct HandleHash {
  size_t operator()(const HANDLE& handle) const {
    std::span s = std::as_bytes(std::span(&handle, 1u));
    return base::FastHash(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(s.data()), s.size_bytes()));
  }
};

struct ScopedHandleVerifierInfo {
  ScopedHandleVerifierInfo(const void* owner,
                           const void* pc1,
                           const void* pc2,
                           //  std::unique_ptr<debug::StackTrace> stack,
                           DWORD thread_id);
  ~ScopedHandleVerifierInfo();

  ScopedHandleVerifierInfo(const ScopedHandleVerifierInfo&) = delete;
  ScopedHandleVerifierInfo& operator=(const ScopedHandleVerifierInfo&) = delete;
  ScopedHandleVerifierInfo(ScopedHandleVerifierInfo&&) noexcept;
  ScopedHandleVerifierInfo& operator=(ScopedHandleVerifierInfo&&) noexcept;

  const void* owner;
  const void* pc1;
  const void* pc2;
  // std::unique_ptr<debug::StackTrace> stack;
  DWORD thread_id;
};

// Implements the actual object that is verifying handles for this process.
// The active instance is shared across the module boundary but there is no
// way to delete this object from the wrong side of it (or any side, actually).
// We need [[clang::lto_visibility_public]] because instances of this class are
// passed across module boundaries. This means different modules must have
// compatible definitions of the class even when whole program optimization is
// enabled - which is what this attribute accomplishes. The pragma stops MSVC
// from emitting an unrecognized attribute warning.
#pragma warning(push)
#pragma warning(disable : 5030)
class [[clang::lto_visibility_public, nodiscard]] ScopedHandleVerifier {
#pragma warning(pop)
 public:
  ScopedHandleVerifier(const ScopedHandleVerifier&) = delete;
  ScopedHandleVerifier& operator=(const ScopedHandleVerifier&) = delete;

  // Retrieves the current verifier.
  static ScopedHandleVerifier* Get();

  // The methods required by HandleTraits. They are virtual because we need to
  // forward the call execution to another module, instead of letting the
  // compiler call the version that is linked in the current module.
  virtual bool CloseHandle(HANDLE handle);
  virtual void StartTracking(HANDLE handle, const void* owner, const void* pc1,
                             const void* pc2);
  virtual void StopTracking(HANDLE handle, const void* owner, const void* pc1,
                            const void* pc2);
  virtual void Disable();
  virtual void OnHandleBeingClosed(HANDLE handle, HandleOperation operation);
  virtual HMODULE GetModule() const;

 private:
  explicit ScopedHandleVerifier(bool enabled);
  ~ScopedHandleVerifier();  // Not implemented.

  void StartTrackingImpl(HANDLE handle, const void* owner, const void* pc1,
                         const void* pc2);
  void StopTrackingImpl(HANDLE handle, const void* owner, const void* pc1,
                        const void* pc2);
  void OnHandleBeingClosedImpl(HANDLE handle, HandleOperation operation);

  static kiwi::base::internal::LockImpl* GetLock();
  static void InstallVerifier();
  static void ThreadSafeAssignOrCreateScopedHandleVerifier(
      ScopedHandleVerifier * existing_verifier, bool enabled);

  // base::debug::StackTrace creation_stack_;
  bool enabled_;
  kiwi::base::internal::LockImpl* lock_;
  std::unordered_map<HANDLE, ScopedHandleVerifierInfo, HandleHash> map_;
};

// This testing function returns the module that the HandleVerifier concrete
// implementation was instantiated in.
BASE_EXPORT HMODULE GetHandleVerifierModuleForTesting();

}  // namespace internal
}  // namespace win
}  // namespace kiwi::base

#endif  // BASE_WIN_SCOPED_HANDLE_VERIFIER_H_
