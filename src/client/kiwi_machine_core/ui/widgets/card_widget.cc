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

#include "ui/widgets/card_widget.h"

#include "ui/window_base.h"

CardWidget::CardWidget(WindowBase* window_base) : Widget(window_base) {}

CardWidget::~CardWidget() = default;

bool CardWidget::IsWindowless() {
  return true;
}

bool CardWidget::SetCurrentWidget(Widget* child_widget) {
  if (std::find_if(children().begin(), children().end(),
                   [child_widget](const std::unique_ptr<Widget>& lhs) {
                     return child_widget == lhs.get();
                   }) == children().cend()) {
    return false;
  }

  for (auto& child : children()) {
    child->set_visible(child_widget == child.get());
  }
  return true;
}

Widget* CardWidget::GetCurrentWidget() {
  for (auto& child : children()) {
    if (child->visible())
      return child.get();
  }
  return nullptr;
}

void CardWidget::OnWindowResized() {
  if (children().size() > 0) {
    for (auto& child : children()) {
      child->set_bounds(SDL_Rect{0, 0, bounds().w, bounds().h});
    }
  }
}