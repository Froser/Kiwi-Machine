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

#ifndef UTILITY_RICHTEXT_CONTENT_
#define UTILITY_RICHTEXT_CONTENT_

#include "utility/fonts.h"

#include <imgui.h>
#include <tuple>
#include <vector>

class Widget;
struct SDL_Texture;
struct RichTextContent {
 public:
  RichTextContent(Widget* widget);
  ~RichTextContent();

  void AddContent(FontType font_type, const char* content);
  void AddImage(SDL_Texture* texture, const ImVec2& size);
  void DrawContents(ImColor mask_color);

 private:
  Widget* widget_ = nullptr;
  int start_pos_y_ = 0;
  int current_pos_y = 0;

  enum class ContentType {
    kText,
    kImage,
  };
  struct Content {
    ContentType type;
    std::tuple<FontType, int, const char*> text;
    std::tuple<SDL_Texture*, ImVec2> image;
  };
  std::vector<Content> contents_;
};

#endif  // UTILITY_RICHTEXT_CONTENT_
