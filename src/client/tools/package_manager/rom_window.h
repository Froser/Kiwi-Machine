// Copyright (C) 2024 Yisi Yu
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

#ifndef ROM_WINDOW_H_
#define ROM_WINDOW_H_

#include "base/files/file_path.h"
#include "util.h"

struct ROM;
class ROMWindow {
 public:
  ROMWindow(ROMS roms, kiwi::base::FilePath file);
  ~ROMWindow();

  void Paint();
  void Painted();
  void Close();
  int window_id() const { return window_id_; }
  bool closed() const { return closed_; }

 private:
  std::string GetUniqueName(const std::string& name, int unique_id);

 private:
  int window_id_ = 0;
  kiwi::base::FilePath file_;
  std::vector<ROM> current_manifest_;
  std::vector<ROM*> to_be_deleted_;
  ROMS roms_;
  bool check_close_ = false;
  bool closed_ = false;
};

#endif  // ROM_WINDOW_H_