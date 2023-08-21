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

#include "ui/widgets/stack_widget.h"

#include <imgui.h>

#include "ui/window_base.h"

StackWidget::StackWidget(WindowBase* window_base) : Widget(window_base) {}

StackWidget::~StackWidget() = default;

void StackWidget::PushWidget(std::unique_ptr<Widget> widget) {
  if (!children().empty()) {
    children().back()->set_enabled(false);
    children().back()->set_visible(false);
  }

  widget->set_enabled(true);
  widget->set_visible(true);
  AddWidget(std::move(widget));
}

void StackWidget::PopWidget() {
  if (!children().empty()) {
    RemoveWidget(children().back().get());
  }

  // RemoveWidget() just moves the widget to the pending removing list, but
  // still in children()'s list during this frame.
  if (children().size() > 1) {
    Widget* front = children()[children().size() - 2].get();
    front->set_enabled(true);
    front->set_visible(true);
  }
}

bool StackWidget::IsWindowless() {
  return true;
}

void StackWidget::OnWidgetsRemoved() {
  if (!children().empty()) {
    children().back()->set_enabled(true);
    children().back()->set_visible(true);
  }
}

void StackWidget::OnWindowResized() {
  if (children().size() > 0) {
    Widget* front = children()[children().size() - 1].get();
    SDL_Rect client_bounds = window()->GetClientBounds();
    front->set_bounds(SDL_Rect{0, 0, client_bounds.w, client_bounds.h});
  }
}