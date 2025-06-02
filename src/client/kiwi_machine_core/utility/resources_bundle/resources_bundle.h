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

#ifndef UTILITY_RESOURCES_BUNDLE_RESOURCES_BUNDLE_H_
#define UTILITY_RESOURCES_BUNDLE_RESOURCES_BUNDLE_H_

#include <cstddef>
#include <optional>
#include <type_traits>
#include <vector>

#include "base/files/file_path.h"

namespace vita::resources_bundle {

bool LoadResourceFromPackage(const kiwi::base::FilePath& package);

std::optional<std::vector<std::byte>> GetResource(int id);

void ClosePackage();

template <typename T>
std::optional<std::vector<std::byte>> GetResource(T id) {
  static_assert(std::is_enum_v<T>);
  return GetResource(static_cast<int>(id));
}

}  // namespace vita::resources_bundle

#endif  // UTILITY_RESOURCES_BUNDLE_RESOURCES_BUNDLE_H_