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

// Platform factory is an abstract interface to create concrete implementations
// of threading, task runners, etc.

#ifndef BASE_PLATFORM_QT_QT_FILE_INTERFACE_H_
#define BASE_PLATFORM_QT_QT_FILE_INTERFACE_H_

#include <QFile>

#include "base/platform/platform_factory.h"

namespace kiwi::base {
class FilePath;

namespace platform {
class QtFileInterface : public FileInterface {
 public:
  QtFileInterface();
  ~QtFileInterface() override;

 public:
  void Open(const FilePath& file_path, uint32_t flags);

 private:
  int ReadAtCurrentPos(char* data, int size) override;
  bool IsValid() const override;

 private:
  QFile file_;
};
}  // namespace platform
}  // namespace kiwi::base

#endif  // BASE_PLATFORM_QT_QT_FILE_INTERFACE_H_