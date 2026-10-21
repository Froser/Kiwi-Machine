// Copyright (C) 2026 Yisi Yu
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

#include "utility/switch/paths.h"

#include <optional>

#include <switch.h>

#include "utility/switch/paths_internal.h"

namespace {

std::optional<kiwi::switch_platform::internal::AccountId>
GetPreselectedAccountId() {
  Result initialize_result = accountInitialize(AccountServiceType_Application);
  if (R_FAILED(initialize_result))
    return std::nullopt;

  AccountUid uid{};
  Result account_result = accountGetPreselectedUser(&uid);
  accountExit();
  if (R_FAILED(account_result) || !accountUidIsValid(&uid))
    return std::nullopt;

  return kiwi::switch_platform::internal::AccountId{uid.uid[0], uid.uid[1]};
}

}  // namespace

namespace kiwi::switch_platform {

kiwi::base::FilePath GetApplicationDirectory() {
  return kiwi::base::FilePath::FromUTF8Unsafe("sdmc:/switch/KiwiMachine");
}

kiwi::base::FilePath GetResourceDirectory() {
  return kiwi::base::FilePath::FromUTF8Unsafe("romfs:/");
}

kiwi::base::FilePath GetUserDataDirectory() {
  static const kiwi::base::FilePath user_data_directory =
      internal::ResolveUserDataDirectory(GetApplicationDirectory(),
                                         GetPreselectedAccountId());
  return user_data_directory;
}

}  // namespace kiwi::switch_platform
