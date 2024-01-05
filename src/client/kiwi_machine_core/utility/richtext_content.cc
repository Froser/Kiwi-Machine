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

#include "utility/richtext_content.h"

#include "ui/widgets/widget.h"

RichTextContent::RichTextContent(Widget* widget) : widget_(widget) {}
RichTextContent::~RichTextContent() = default;

void RichTextContent::AddContent(FontType font_type, const char* content) {
  if (contents_.empty())
    start_pos_y_ = ImGui::GetCursorPosY();
  else
    ImGui::SetCursorPosY(current_pos_y);
  const ImVec2 kSplashSize(widget_->bounds().w, widget_->bounds().h);
  ScopedFont font(font_type);
  ImVec2 fs = ImGui::CalcTextSize(content);
  ImGui::Dummy(fs);
  contents_.emplace_back(Content{
      ContentType::kText, {font.type(), (kSplashSize.x - fs.x) / 2, content}});
  current_pos_y = ImGui::GetCursorPosY();
  ImGui::SetCursorPosY(start_pos_y_);
}

void RichTextContent::AddImage(SDL_Texture* texture, const ImVec2& size) {
  if (contents_.empty())
    start_pos_y_ = ImGui::GetCursorPosY();
  else
    ImGui::SetCursorPosY(current_pos_y);
  contents_.emplace_back(Content{ContentType::kImage, {}, {texture, size}});
  ImGui::Dummy(size);
  current_pos_y = ImGui::GetCursorPosY();
  ImGui::SetCursorPosY(start_pos_y_);
}

void RichTextContent::DrawContents(ImColor mask_color) {
  int content_height = current_pos_y - start_pos_y_;
  const ImVec2 kSplashSize(widget_->bounds().w, widget_->bounds().h);
  ImGui::SetCursorPosY((kSplashSize.y - content_height) / 2);
  for (const auto& content : contents_) {
    if (content.type == ContentType::kText) {
      ScopedFont font(std::get<0>(content.text));
      ImGui::SetCursorPosX(std::get<1>(content.text));
      ImGui::TextColored(mask_color, "%s", std::get<2>(content.text));
    } else if (content.type == ContentType::kImage) {
      const ImVec2& size = std::get<1>(content.image);
      ImGui::SetCursorPosX((kSplashSize.x - size.x) / 2);
      ImGui::Image(std::get<0>(content.image), size, ImVec2(0, 0), ImVec2(1, 1),
                   ImVec4(255, 255, 255, 255 - 255 * mask_color.Value.z));
    } else {
      // Unknown content type.
      SDL_assert(false);
    }
  }
}
