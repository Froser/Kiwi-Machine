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

#ifndef UI_WIDGETS_SIDE_MENU_H_
#define UI_WIDGETS_SIDE_MENU_H_

#include <kiwi_nes.h>

#include "models/nes_runtime.h"
#include "ui/widgets/widget.h"
#include "utility/timer.h"

class MainWindow;
class SideMenu : public Widget {
 public:
  explicit SideMenu(MainWindow* main_window, NESRuntimeID runtime_id);
  ~SideMenu() override;

 private:
  bool IsWindowless() override;

 private:
  MainWindow* main_window_ = nullptr;
  NESRuntime::Data* runtime_data_ = nullptr;
};

#endif  // UI_WIDGETS_SIDE_MENU_H_