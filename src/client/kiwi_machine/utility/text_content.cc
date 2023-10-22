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

#include "utility/text_content.h"

#include "ui/widgets/widget.h"

TextContent::TextContent(Widget* widget) : widget_(widget) {}
TextContent::~TextContent() = default;

void TextContent::AddContent(FontType font_type, const char* content) {
  if (contents_.empty())
    start_pos_y_ = ImGui::GetCursorPosY();
  else
    ImGui::SetCursorPosY(current_pos_y);
  const ImVec2 kSplashSize(widget_->bounds().w, widget_->bounds().h);
  ScopedFont font(font_type);
  ImVec2 fs = ImGui::CalcTextSize(content);
  ImGui::Dummy(fs);
  contents_.emplace_back(font.type(), (kSplashSize.x - fs.x) / 2, content);
  current_pos_y = ImGui::GetCursorPosY();
  ImGui::SetCursorPosY(start_pos_y_);
}

void TextContent::DrawContents(ImColor font_color) {
  int content_height = current_pos_y - start_pos_y_;
  const ImVec2 kSplashSize(widget_->bounds().w, widget_->bounds().h);
  ImGui::SetCursorPosY((kSplashSize.y - content_height) / 2);
  for (const auto& content : contents_) {
    ScopedFont font(std::get<0>(content));
    ImGui::SetCursorPosX(std::get<1>(content));
    ImGui::TextColored(font_color, "%s", std::get<2>(content));
  }
}
