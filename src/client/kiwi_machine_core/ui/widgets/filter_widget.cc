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

#include "ui/widgets/filter_widget.h"

#include "ui/main_window.h"
#include "utility/localization.h"

namespace {
int g_global_input = 0;
}

FilterWidget::FilterWidget(MainWindow* window_base, FilterCallback callback)
    : Widget(window_base),
      parent_window_(window_base),
      callback_(std::move(callback)) {
  set_flags(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove);
  set_title("FilterWidget");
}

FilterWidget::~FilterWidget() {
  if (input_started_)
    StopTextInput();
}

void FilterWidget::BeginFilter() {
  if (!input_started_) {
    set_visible(true);
    filter_contents_.clear();
    StartTextInput();
  }
}

void FilterWidget::EndFilter() {
  if (input_started_) {
    set_visible(false);
    StopTextInput();
  }
}

void FilterWidget::Paint() {
  SDL_Rect bounds_to_window = MapToWindow(bounds());
  ImGui::GetWindowDrawList()->AddRectFilled(
      ImVec2(bounds_to_window.x, bounds_to_window.y),
      ImVec2(bounds_to_window.x + bounds_to_window.w,
             bounds_to_window.y + bounds_to_window.h),
      ImColor(0, 0, 0, 196));

  ImVec2 title_rect, contents_rect;
  std::string title =
      GetLocalizedString(string_resources::IDR_ITEMS_WIGDET_FILTER);
  {
    ScopedFont font(GetPreferredFont(PreferredFontSize::k2x));
    title_rect = ImGui::CalcTextSize(title.c_str());
  }
  {
    ScopedFont font(FontType::kDefault);
    contents_rect = ImGui::CalcTextSize(filter_contents_.c_str());
  }

  ImVec2 combined_rect = {std::max(title_rect.x, contents_rect.x),
                          title_rect.y + contents_rect.y};

  ImGui::SetCursorPosX((GetLocalBounds().w - title_rect.x) / 2);
  ImGui::SetCursorPosY((GetLocalBounds().h - combined_rect.y) / 2);
  {
    ScopedFont font(GetPreferredFont(PreferredFontSize::k2x));
    ImGui::TextUnformatted(title.c_str());
  }

  {
    ImGui::SetCursorPosX((GetLocalBounds().w - contents_rect.x) / 2);
    ScopedFont font(FontType::kDefault);
    ImGui::TextUnformatted(filter_contents_.c_str());
  }
}

bool FilterWidget::OnMousePressed(SDL_MouseButtonEvent* event) {
  if (input_started_)
    return true;

  return false;
}

bool FilterWidget::OnMouseMove(SDL_MouseMotionEvent* event) {
  if (input_started_)
    return true;

  return false;
}

bool FilterWidget::OnMouseWheel(SDL_MouseWheelEvent* event) {
  if (input_started_)
    return true;

  return false;
}

bool FilterWidget::OnMouseReleased(SDL_MouseButtonEvent* event) {
  if (input_started_)
    return true;

  return false;
}

bool FilterWidget::OnKeyPressed(SDL_KeyboardEvent* event) {
  // If input has stared, eats all keys, unless special key is pressed.
  if (input_started_) {
    switch (event->keysym.sym) {
      case SDLK_ESCAPE: {
        EndFilter();
      } break;
      case SDLK_BACKSPACE: {
        if (!filter_contents_.empty())
          filter_contents_.resize(filter_contents_.size() - 1);
      } break;
      case SDLK_RETURN:
      case SDLK_KP_ENTER: {
        SDL_assert(callback_);
        callback_.Run(filter_contents_);
        EndFilter();
        break;
      }
      default:
        break;
    }
    return true;
  }

  return false;
}

bool FilterWidget::OnTextInput(SDL_TextInputEvent* event) {
  SDL_assert(input_started_);
  // Only accept visible ASCII characters, because Unicode is too hard.
  if (strlen(event->text) == 1 && event->text[0] >= 32 &&
      event->text[0] < 127) {
    filter_contents_ += event->text;
  }
  return true;
}

void FilterWidget::StartTextInput() {
  SDL_assert(g_global_input == 0);
  ++g_global_input;
  input_started_ = true;
  SDL_StartTextInput();
}

void FilterWidget::StopTextInput() {
  --g_global_input;
  input_started_ = false;
  SDL_StopTextInput();
  SDL_assert(g_global_input == 0);
}