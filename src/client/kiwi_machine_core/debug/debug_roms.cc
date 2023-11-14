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

#include "debug/debug_roms.h"

#include <gflags/gflags.h>
#include <kiwi_nes.h>

namespace {
DEFINE_string(debug_roms,
              "",
              "Specify debug roms' directory, to build debug roms menu.");

// If there's no .nes in directory, and it has no subdirectory, this function
// returns false, and this menu item will be deleted, otherwise it returns true.
bool CreateMenuItemRecursively(MenuBar::MenuItem& menu_item,
                               const kiwi::base::FilePath& path,
                               DebugRomsLoadCallback open_callback) {
  kiwi::base::FileEnumerator fe(path, false,
                                kiwi::base::FileEnumerator::FILES |
                                    kiwi::base::FileEnumerator::DIRECTORIES);

  bool has_nes = false;
  bool has_subdirectory = false;
  kiwi::base::FilePath entry;
  while (entry = fe.Next(), !entry.empty()) {
    kiwi::base::FileEnumerator::FileInfo info = fe.GetInfo();
    if (info.IsDirectory()) {
      MenuBar::MenuItem new_item;
      new_item.title = entry.BaseName().AsUTF8Unsafe();
      if (CreateMenuItemRecursively(new_item, entry, open_callback)) {
        menu_item.sub_items.push_back(std::move(new_item));
        has_subdirectory = true;
      }
    } else {
      if (entry.FinalExtension() == FILE_PATH_LITERAL(".nes")) {
        menu_item.sub_items.push_back(
            {entry.BaseName().AsUTF8Unsafe(),
             kiwi::base::BindRepeating(open_callback, entry)});
        has_nes = true;
      }
    }
    std::sort(menu_item.sub_items.begin(), menu_item.sub_items.end());
  }

  return has_subdirectory || has_nes;
}

}  // namespace

bool HasDebugRoms() {
  return !FLAGS_debug_roms.empty();
}

MenuBar::MenuItem CreateDebugRomsMenu(DebugRomsLoadCallback open_callback) {
  MenuBar::MenuItem debug_roms_menu;
  debug_roms_menu.title = "Debug ROMs";
  CreateMenuItemRecursively(
      debug_roms_menu, kiwi::base::FilePath::FromUTF8Unsafe(FLAGS_debug_roms),
      open_callback);
  return debug_roms_menu;
}