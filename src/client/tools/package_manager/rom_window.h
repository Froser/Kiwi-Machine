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

#include <optional>
#include <string>

struct SDL_Renderer;
struct ROM;
class ROMWindow {
 public:
  ROMWindow(SDL_Renderer* renderer,
            ROMSystem system,
            ROMS roms,
            kiwi::base::FilePath file);
  ~ROMWindow();

  ROMWindow(const ROMWindow&) = delete;
  ROMWindow(ROMWindow&&) = default;
  ROMWindow& operator=(ROMWindow&&) = default;

  void PaintEditor();
  void PaintInspector();
  void PaintFooter();
  void PaintDialogs();
  void Painted();
  void Close();
  int window_id() const { return window_id_; }
  bool closed() const { return closed_; }
  bool dirty() const { return dirty_; }
  std::string GetTabTitle() const;
  void NewRom();
  kiwi::base::FilePath TryFetchCoverByName(
      ROM& rom,
      const kiwi::base::FilePath& rom_base_name);
  ROM* first_rom() { return roms_.empty() ? nullptr : &roms_[0]; }

 private:
  ROM* selected_rom();
  const ROM* selected_rom() const;
  std::string GetUniqueName(const std::string& name, int unique_id);
  void PaintROMSelector();
  void PaintLocalizedFields(ROM& rom, int id);
  void PaintROMFile(ROM& rom, int id);
  void PaintCover(ROM& rom, int id);
  void PaintValidation(const ROM& rom);
  void TestROM(const ROM& rom);
  void Save();
  void SaveAndTest();
  void FillCoverData(ROM& rom, const kiwi::base::FilePath& path);
  void FillCoverData(ROM& rom, const std::vector<uint8_t>& data);
  void UpdateCover(ROM& rom, const std::vector<uint8_t>& data);
  kiwi::base::FilePath SaveROMs(const kiwi::base::FilePath& save_path,
                                const ROMS& roms);

 private:
  int window_id_ = 0;
  ROMSystem system_ = ROMSystem::kNES;
  kiwi::base::FilePath file_;
  int to_be_deleted_ = -1;
  int selected_rom_index_ = 0;
  ROMS roms_;
  bool closed_ = false;
  bool dirty_ = false;
  bool tested_ = false;
  SDL_Renderer* renderer_ = nullptr;

  bool show_message_box_ = false;
  kiwi::base::FilePath generated_packaged_path_;

  SDL_mutex* cover_update_mutex_ = nullptr;
};

#endif  // ROM_WINDOW_H_
