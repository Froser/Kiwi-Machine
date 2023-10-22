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

#ifndef UTILITY_TEXT_CONTENT_
#define UTILITY_TEXT_CONTENT_

#include "utility/fonts.h"

#include <tuple>
#include <vector>

class Widget;
struct TextContent {
 public:
  TextContent(Widget* widget);
  ~TextContent();

  void AddContent(FontType font_type, const char* content);
  void DrawContents(ImColor font_color);

 private:
  Widget* widget_ = nullptr;
  int start_pos_y_ = 0;
  int current_pos_y = 0;
  std::vector<std::tuple<FontType, int, const char*>> contents_;
};

#endif  // UTILITY_TEXT_CONTENT_
