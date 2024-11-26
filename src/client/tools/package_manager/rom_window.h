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

struct SDL_Renderer;
struct ROM;
class ROMWindow {
 public:
  ROMWindow(SDL_Renderer* renderer, ROMS roms, kiwi::base::FilePath file);
  ~ROMWindow();

  ROMWindow(const ROMWindow&) = delete;
  ROMWindow(ROMWindow&&) = default;
  ROMWindow& operator=(ROMWindow&&) = default;

  void Paint();
  void Painted();
  void Close();
  int window_id() const { return window_id_; }
  bool closed() const { return closed_; }
  void NewRom();

 private:
  std::string GetUniqueName(const std::string& name, int unique_id);
  void FillCoverData(ROM& rom, const kiwi::base::FilePath& path);
  void FillCoverData(ROM& rom, const std::vector<uint8_t>& data);
  void UpdateCover(ROM& rom, const std::vector<uint8_t>& data);

 private:
  int window_id_ = 0;
  kiwi::base::FilePath file_;
  int to_be_deleted_ = -1;
  ROMS roms_;
  bool check_close_ = false;
  bool closed_ = false;
  SDL_Renderer* renderer_ = nullptr;

  bool show_message_box_ = false;
  kiwi::base::FilePath generated_packaged_path_;

  char save_path_[ROM::MAX] = {0};
  SDL_mutex* cover_update_mutex_ = nullptr;
};

#endif  // ROM_WINDOW_H_