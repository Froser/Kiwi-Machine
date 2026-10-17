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

#ifndef UI_WIDGETS_PALETTE_WIDGET_H_
#define UI_WIDGETS_PALETTE_WIDGET_H_

#include "ui/widgets/widget.h"

#include <kiwi_nes.h>

class MainWindow;
class PaletteWidget : public Widget {
 public:
  explicit PaletteWidget(MainWindow* main_window);
  ~PaletteWidget() override;

 protected:
  // Widget:
  void Paint() override;

 private:
  MainWindow* main_window_ = nullptr;
};

#endif  // UI_WIDGETS_PALETTE_WIDGET_H_
