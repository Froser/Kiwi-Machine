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

#include "base/files/file_path.h"

namespace kiwi::base {
FilePath::FilePath(const std::filesystem::path& path)
    : std::filesystem::path(path) {}

std::string FilePath::AsUTF8Unsafe() const {
  return this->u8string();
}

FilePath FilePath::DirName() const {
  return this->parent_path();
}

FilePath FilePath::BaseName() const {
  return this->filename().string();
}

FilePath::StringType FilePath::FinalExtension() const {
  return this->extension().c_str();
}

FilePath FilePath::RemoveExtension() const {
  return this->DirName().Append(this->stem());
}

FilePath FilePath::FromUTF8Unsafe(StringPiece utf8) {
  return FilePath(utf8);
}

FilePath FilePath::Append(const FilePath& component) const {
  std::filesystem::path t = *this;
  t.append(component.AsUTF8Unsafe());
  return FilePath(t);
}

}  // namespace kiwi::base