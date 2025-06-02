// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/access_token.h"

#include <windows.h>

#include <memory>
#include <utility>

// #include "base/numerics/checked_math.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace kiwi::base::win {

namespace {

// The SECURITY_IMPERSONATION_LEVEL type is an enum and therefore can't be
// forward declared in windows_types.h. Ensure our separate definition matches
// the existing values for simplicity.
static_assert(static_cast<int>(SecurityImpersonationLevel::kAnonymous) ==
              SecurityAnonymous);
static_assert(static_cast<int>(SecurityImpersonationLevel::kIdentification) ==
              SecurityIdentification);
static_assert(static_cast<int>(SecurityImpersonationLevel::kImpersonation) ==
              SecurityImpersonation);
static_assert(static_cast<int>(SecurityImpersonationLevel::kDelegation) ==
              SecurityDelegation);

typedef BOOL(WINAPI* CreateAppContainerTokenFunction)(
    HANDLE TokenHandle,
    PSECURITY_CAPABILITIES SecurityCapabilities,
    PHANDLE OutToken);

std::optional<std::vector<char>> GetTokenInfo(
    HANDLE token,
    TOKEN_INFORMATION_CLASS info_class) {
  // Get the buffer size. The call to GetTokenInformation should never succeed.
  DWORD size = 0;
  if (::GetTokenInformation(token, info_class, nullptr, 0, &size) || !size)
    return std::nullopt;

  std::vector<char> temp_buffer(size);
  if (!::GetTokenInformation(token, info_class, temp_buffer.data(), size,
                             &size)) {
    return std::nullopt;
  }

  return std::move(temp_buffer);
}

template <typename T>
std::optional<T> GetTokenInfoFixed(HANDLE token,
                                    TOKEN_INFORMATION_CLASS info_class) {
  T result;
  DWORD size = sizeof(T);
  if (!::GetTokenInformation(token, info_class, &result, size, &size))
    return std::nullopt;

  return result;
}

TOKEN_STATISTICS GetTokenStatistics(HANDLE token) {
  std::optional<TOKEN_STATISTICS> value =
      GetTokenInfoFixed<TOKEN_STATISTICS>(token, TokenStatistics);
  if (!value)
    return {};
  return *value;
}

template <typename T>
T* GetType(std::optional<std::vector<char>>& info) {
  // DCHECK(info);
  // DCHECK(info->size() >= sizeof(T));
  return reinterpret_cast<T*>(info->data());
}

CHROME_LUID ConvertLuid(const LUID& luid) {
  CHROME_LUID ret;
  ret.LowPart = luid.LowPart;
  ret.HighPart = luid.HighPart;
  return ret;
}

HANDLE DuplicateToken(HANDLE token,
                      ACCESS_MASK desired_access,
                      SECURITY_IMPERSONATION_LEVEL imp_level,
                      TOKEN_TYPE type) {
  HANDLE new_token;
  if (!::DuplicateTokenEx(token, TOKEN_QUERY | desired_access, nullptr,
                          imp_level, type, &new_token)) {
    return nullptr;
  }
  return new_token;
}

std::optional<LUID> LookupPrivilege(const std::wstring& name) {
  LUID luid;
  if (!::LookupPrivilegeValue(nullptr, name.c_str(), &luid)) {
    return std::nullopt;
  }
  return luid;
}

std::vector<LUID_AND_ATTRIBUTES> ConvertPrivileges(
    const std::vector<std::wstring>& privs,
    DWORD attributes) {
  std::vector<LUID_AND_ATTRIBUTES> ret;
  ret.reserve(privs.size());
  for (const std::wstring& priv : privs) {
    std::optional<LUID> luid = LookupPrivilege(priv);
    if (!luid) {
      return {};
    }
    LUID_AND_ATTRIBUTES entry = {};
    entry.Luid = *luid;
    entry.Attributes = attributes;
    ret.push_back(entry);
  }
  return ret;
}

template <typename T>
T* GetPointer(std::vector<T>& values) {
  if (values.empty()) {
    return nullptr;
  }
  return values.data();
}

template <typename T>
bool Set(const ScopedHandle& token,
         TOKEN_INFORMATION_CLASS info_class,
         T& value) {
  return !!::SetTokenInformation(token.get(), info_class, &value,
                                 sizeof(value));
}

std::optional<DWORD> AdjustPrivilege(const ScopedHandle& token,
                                      const std::wstring& priv,
                                      DWORD attributes) {
  TOKEN_PRIVILEGES token_privs = {};
  token_privs.PrivilegeCount = 1;
  std::optional<LUID> luid = LookupPrivilege(priv);
  if (!luid) {
    return std::nullopt;
  }
  token_privs.Privileges[0].Luid = *luid;
  token_privs.Privileges[0].Attributes = attributes;

  TOKEN_PRIVILEGES out_privs = {};
  DWORD out_length = 0;
  if (!::AdjustTokenPrivileges(token.get(), FALSE, &token_privs,
                               sizeof(out_privs), &out_privs, &out_length)) {
    return std::nullopt;
  }
  if (::GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
    return std::nullopt;
  }
  if (out_privs.PrivilegeCount == 1) {
    return out_privs.Privileges[0].Attributes;
  }
  return attributes;
}
}  // namespace

bool AccessToken::Group::IsIntegrity() const {
  return !!(attributes_ & SE_GROUP_INTEGRITY);
}

bool AccessToken::Group::IsEnabled() const {
  return !!(attributes_ & SE_GROUP_ENABLED);
}

bool AccessToken::Group::IsDenyOnly() const {
  return !!(attributes_ & SE_GROUP_USE_FOR_DENY_ONLY);
}

bool AccessToken::Group::IsLogonId() const {
  return (attributes_ & SE_GROUP_LOGON_ID) == SE_GROUP_LOGON_ID;
}

// AccessToken::Group::Group(Sid&& sid, DWORD attributes)
//     : sid_(std::move(sid)), attributes_(attributes) {}
AccessToken::Group::Group(Group&&) = default;
AccessToken::Group::Group::~Group() = default;

std::wstring AccessToken::Privilege::GetName() const {
  WCHAR name[128];
  LUID luid;
  luid.LowPart = luid_.LowPart;
  luid.HighPart = luid_.HighPart;
  DWORD size = std::size(name);
  return ::LookupPrivilegeName(nullptr, &luid, name, &size)
             ? name
             : ASCIIToWide(
                   StringPrintf("%08lX-%08lX", luid.HighPart, luid.LowPart));
}

bool AccessToken::Privilege::IsEnabled() const {
  return !!(attributes_ & SE_PRIVILEGE_ENABLED);
}

AccessToken::Privilege::Privilege(CHROME_LUID luid, DWORD attributes)
    : luid_(luid), attributes_(attributes) {}

std::optional<AccessToken> AccessToken::FromToken(HANDLE token,
                                                   ACCESS_MASK desired_access) {
  HANDLE new_token;
  if (!::DuplicateHandle(::GetCurrentProcess(), token, ::GetCurrentProcess(),
                         &new_token, TOKEN_QUERY | desired_access, FALSE, 0)) {
    return std::nullopt;
  }
  return AccessToken(new_token);
}

std::optional<AccessToken> AccessToken::FromToken(ScopedHandle&& token) {
  if (!token.is_valid()) {
    ::SetLastError(ERROR_INVALID_HANDLE);
    return std::nullopt;
  }
  if (!GetTokenInfoFixed<TOKEN_STATISTICS>(token.get(), TokenStatistics)) {
    return std::nullopt;
  }
  return AccessToken(token.release());
}

std::optional<AccessToken> AccessToken::FromProcess(
    HANDLE process,
    bool impersonation,
    ACCESS_MASK desired_access) {
  HANDLE token = nullptr;
  if (impersonation) {
    if (!::OpenProcessToken(process, TOKEN_DUPLICATE, &token))
      return std::nullopt;
    ScopedHandle primary_token(token);
    token = DuplicateToken(primary_token.get(), desired_access,
                           SecurityIdentification, TokenImpersonation);
    if (!token) {
      return std::nullopt;
    }
  } else {
    if (!::OpenProcessToken(process, TOKEN_QUERY | desired_access, &token))
      return std::nullopt;
  }
  return AccessToken(token);
}

std::optional<AccessToken> AccessToken::FromCurrentProcess(
    bool impersonation,
    ACCESS_MASK desired_access) {
  return FromProcess(::GetCurrentProcess(), impersonation, desired_access);
}

std::optional<AccessToken> AccessToken::FromThread(
    HANDLE thread,
    bool open_as_self,
    ACCESS_MASK desired_access) {
  HANDLE token;
  if (!::OpenThreadToken(thread, TOKEN_QUERY | desired_access, open_as_self,
                         &token))
    return std::nullopt;
  return AccessToken(token);
}

std::optional<AccessToken> AccessToken::FromCurrentThread(
    bool open_as_self,
    ACCESS_MASK desired_access) {
  return FromThread(::GetCurrentThread(), open_as_self, desired_access);
}

std::optional<AccessToken> AccessToken::FromEffective(
    ACCESS_MASK desired_access) {
  std::optional<AccessToken> token = FromCurrentThread(true, desired_access);
  if (token)
    return token;
  if (::GetLastError() != ERROR_NO_TOKEN)
    return std::nullopt;
  return FromCurrentProcess(false, desired_access);
}

AccessToken::AccessToken(AccessToken&&) = default;
AccessToken& AccessToken::operator=(AccessToken&&) = default;
AccessToken::~AccessToken() = default;

// Sid AccessToken::User() const {
//   return UserGroup().GetSid().Clone();
// }

// std::optional<Sid> AccessToken::LogonId() const {
//   std::vector<AccessToken::Group> groups =
//       GetGroupsFromToken(token_.get(), TokenLogonSid);
//   for (const AccessToken::Group& group : groups) {
//     if (group.IsLogonId())
//       return group.GetSid().Clone();
//   }
//   return std::nullopt;
// }

DWORD AccessToken::IntegrityLevel() const {
  std::optional<std::vector<char>> buffer =
      GetTokenInfo(token_.get(), TokenIntegrityLevel);
  if (!buffer)
    return MAXDWORD;

  PSID il_sid = GetType<TOKEN_MANDATORY_LABEL>(buffer)->Label.Sid;
  return *::GetSidSubAuthority(
      il_sid, static_cast<DWORD>(*::GetSidSubAuthorityCount(il_sid) - 1));
}

DWORD AccessToken::SessionId() const {
  std::optional<DWORD> value =
      GetTokenInfoFixed<DWORD>(token_.get(), TokenSessionId);
  if (!value)
    return MAXDWORD;
  return *value;
}

bool AccessToken::IsRestricted() const {
  return !!::IsTokenRestricted(token_.get());
}

bool AccessToken::IsAppContainer() const {
  std::optional<DWORD> value =
      GetTokenInfoFixed<DWORD>(token_.get(), TokenIsAppContainer);
  if (!value)
    return false;
  return !!*value;
}

std::optional<AccessToken> AccessToken::LinkedToken() const {
  std::optional<TOKEN_LINKED_TOKEN> value =
      GetTokenInfoFixed<TOKEN_LINKED_TOKEN>(token_.get(), TokenLinkedToken);
  if (!value)
    return std::nullopt;
  return AccessToken(value->LinkedToken);
}

// std::optional<AccessControlList> AccessToken::DefaultDacl() const {
//   std::optional<std::vector<char>> dacl_buffer =
//       GetTokenInfo(token_.get(), TokenDefaultDacl);
//   if (!dacl_buffer)
//     return std::nullopt;
//   TOKEN_DEFAULT_DACL* dacl_ptr = GetType<TOKEN_DEFAULT_DACL>(dacl_buffer);
//   return AccessControlList::FromPACL(dacl_ptr->DefaultDacl);
// }

// bool AccessToken::SetDefaultDacl(const AccessControlList& default_dacl) {
//   TOKEN_DEFAULT_DACL set_default_dacl = {};
//   set_default_dacl.DefaultDacl = default_dacl.get();
//   return Set(token_, TokenDefaultDacl, set_default_dacl);
// }

CHROME_LUID AccessToken::Id() const {
  return ConvertLuid(GetTokenStatistics(token_.get()).TokenId);
}

CHROME_LUID AccessToken::AuthenticationId() const {
  return ConvertLuid(GetTokenStatistics(token_.get()).AuthenticationId);
}

std::vector<AccessToken::Privilege> AccessToken::Privileges() const {
  std::optional<std::vector<char>> privileges =
      GetTokenInfo(token_.get(), TokenPrivileges);
  if (!privileges)
    return {};
  TOKEN_PRIVILEGES* privileges_ptr = GetType<TOKEN_PRIVILEGES>(privileges);
  std::vector<AccessToken::Privilege> ret;
  ret.reserve(privileges_ptr->PrivilegeCount);
  for (DWORD index = 0; index < privileges_ptr->PrivilegeCount; ++index) {
    ret.emplace_back(ConvertLuid(privileges_ptr->Privileges[index].Luid),
                     privileges_ptr->Privileges[index].Attributes);
  }
  return ret;
}

bool AccessToken::IsElevated() const {
  std::optional<TOKEN_ELEVATION> value =
      GetTokenInfoFixed<TOKEN_ELEVATION>(token_.get(), TokenElevation);
  if (!value)
    return false;
  return !!value->TokenIsElevated;
}

bool AccessToken::IsImpersonation() const {
  return GetTokenStatistics(token_.get()).TokenType == TokenImpersonation;
}

bool AccessToken::IsIdentification() const {
  return ImpersonationLevel() < SecurityImpersonationLevel::kImpersonation;
}

SecurityImpersonationLevel AccessToken::ImpersonationLevel() const {
  TOKEN_STATISTICS stats = GetTokenStatistics(token_.get());
  if (stats.TokenType != TokenImpersonation) {
    return SecurityImpersonationLevel::kImpersonation;
  }

  return static_cast<SecurityImpersonationLevel>(
      GetTokenStatistics(token_.get()).ImpersonationLevel);
}

std::optional<AccessToken> AccessToken::DuplicatePrimary(
    ACCESS_MASK desired_access) const {
  HANDLE token = DuplicateToken(token_.get(), desired_access, SecurityAnonymous,
                                TokenPrimary);
  if (!token) {
    return std::nullopt;
  }
  return AccessToken{token};
}

std::optional<AccessToken> AccessToken::DuplicateImpersonation(
    SecurityImpersonationLevel impersonation_level,
    ACCESS_MASK desired_access) const {
  HANDLE token = DuplicateToken(
      token_.get(), desired_access,
      static_cast<SECURITY_IMPERSONATION_LEVEL>(impersonation_level),
      TokenImpersonation);
  if (!token) {
    return std::nullopt;
  }
  return AccessToken(token);
}

std::optional<bool> AccessToken::SetPrivilege(const std::wstring& name,
                                               bool enable) {
  std::optional<DWORD> attrs =
      AdjustPrivilege(token_, name.c_str(), enable ? SE_PRIVILEGE_ENABLED : 0);
  if (!attrs) {
    return std::nullopt;
  }
  return !!(*attrs & SE_PRIVILEGE_ENABLED);
}

bool AccessToken::RemovePrivilege(const std::wstring& name) {
  return AdjustPrivilege(token_, name.c_str(), SE_PRIVILEGE_REMOVED)
      .has_value();
}

bool AccessToken::is_valid() const {
  return token_.is_valid();
}

HANDLE AccessToken::get() const {
  return token_.get();
}

ScopedHandle AccessToken::release() {
  return ScopedHandle(token_.release());
}

AccessToken::AccessToken(HANDLE token) : token_(token) {}

}  // namespace base::win
