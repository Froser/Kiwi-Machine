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

#include "ui/widgets/demo_widget.h"

#include <imgui.h>

DemoWidget::DemoWidget(WindowBase* window_base) : Widget(window_base) {}

DemoWidget::~DemoWidget() = default;

void DemoWidget::Paint() {
  ImGui::ShowDemoWindow();
}

bool DemoWidget::IsWindowless() {
  return true;
}
