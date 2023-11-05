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

#include "ui/widgets/menu_bar.h"

#include <imgui.h>

#include "ui/window_base.h"

MenuBar::MenuBar(WindowBase* window_base) : Widget(window_base) {}

MenuBar::~MenuBar() = default;

void MenuBar::AddMenu(const Menu& menu) {
  menu_.push_back(menu);
}

void MenuBar::Paint() {
  if (ImGui::BeginMainMenuBar()) {
    int width = window()->GetClientBounds().w;
    ImFont* current_font = ImGui::GetIO().FontDefault
                               ? ImGui::GetIO().FontDefault
                               : ImGui::GetIO().Fonts->Fonts[0];
    set_bounds(
        SDL_Rect{0, 0, width, static_cast<int>(ImGui::GetFrameHeight())});
    for (const auto& menu : menu_) {
      // Creates each top menu.
      if (ImGui::BeginMenu(menu.title.c_str())) {
        // Creates each menu item.
        PaintMenuItems(menu.menu_items);

        ImGui::EndMenu();
      }
    }

    ImGui::EndMainMenuBar();
  }
}

bool MenuBar::IsWindowless() {
  return true;
}

void MenuBar::PaintMenuItems(const std::vector<MenuItem> items) {
  for (const auto& item : items) {
    if (item.sub_items.empty()) {
      // There's no sub menu, just paint menu items.
      if (ImGui::MenuItem(item.title.c_str(),
                          ((item.shortcut.empty() || item.shortcut == "")
                               ? nullptr
                               : item.shortcut.c_str()),
                          item.is_selected && item.is_selected.Run(),
                          !item.is_enabled ? true : item.is_enabled.Run())) {
        item.callback.Run();
      }
    } else {
      // Paint menu, then menu items.
      if (ImGui::BeginMenu(item.title.c_str())) {
        PaintMenuItems(item.sub_items);
        ImGui::EndMenu();
      }
    }
  }
}

// For sorting.
bool operator<(const MenuBar::MenuItem& lhs, const MenuBar::MenuItem& rhs) {
  return lhs.title < rhs.title;
}
