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

#ifndef UI_WIDGETS_PATTERN_WIDGET_H_
#define UI_WIDGETS_PATTERN_WIDGET_H_

#include "ui/widgets/widget.h"

#include <kiwi_nes.h>

class PatternWidget : public Widget {
 public:
  explicit PatternWidget(WindowBase* window_base,
                         kiwi::nes::DebugPort* debug_port);
  ~PatternWidget() override;

 protected:
  // Widget:
  void Paint() override;

 private:
  kiwi::nes::DebugPort* debug_port_ = nullptr;
  bool first_paint_ = true;
  SDL_Texture* pattern_tables_[8] = {nullptr};
};

#endif  // UI_WIDGETS_PATTERN_WIDGET_H_