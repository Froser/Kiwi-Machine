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

#ifndef UI_WIDGETS_MENU_BAR_H_
#define UI_WIDGETS_MENU_BAR_H_

#include <kiwi_nes.h>
#include <string>

#include "ui/widgets/widget.h"

class MenuBar : public Widget {
 public:
  struct MenuItem {
    std::string title;
    kiwi::base::RepeatingClosure callback;
    kiwi::base::RepeatingCallback<bool()> is_selected;
    kiwi::base::RepeatingCallback<bool()> is_enabled;
    std::string shortcut;
    std::vector<MenuItem> sub_items;
  };

  struct Menu {
    std::string title;
    std::vector<MenuItem> menu_items;
  };

 public:
  explicit MenuBar(WindowBase* window_base);
  ~MenuBar() override;

 public:
  void AddMenu(const Menu& menu);

 protected:
  void Paint() override;
  bool IsWindowless() override;

 private:
  void PaintMenuItems(const std::vector<MenuItem> items);

 private:
  bool menu_bar_active_ = false;
  std::vector<Menu> menu_;
};

// For sorting.
bool operator<(const MenuBar::MenuItem& lhs, const MenuBar::MenuItem& rhs);

#endif  // UI_WIDGETS_MENU_BAR_H_