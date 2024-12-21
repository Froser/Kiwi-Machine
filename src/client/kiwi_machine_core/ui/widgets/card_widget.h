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

#ifndef UI_WIDGETS_CARD_WIDGET_H_
#define UI_WIDGETS_CARD_WIDGET_H_

#include <string>

#include "ui/widgets/widget.h"

class CardWidget : public Widget {
 public:
  explicit CardWidget(WindowBase* window_base);
  ~CardWidget() override;

 public:
  bool SetCurrentWidget(Widget* child_widget);
  Widget* GetCurrentWidget();
  bool HasWidgets();

 protected:
  bool IsWindowless() override;
  void OnWindowResized() override;
};

#endif  // UI_WIDGETS_CARD_WIDGET_H_