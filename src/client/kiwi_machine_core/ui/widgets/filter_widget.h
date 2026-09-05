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

#ifndef UI_WIDGETS_FILTER_WIDGET_H_
#define UI_WIDGETS_FILTER_WIDGET_H_

#include <kiwi_nes.h>

#include <array>
#include <string>
#include <vector>

#include "ui/widgets/widget.h"
#include "utility/timer.h"

class MainWindow;
class FilterWidget : public Widget {
 public:
  using FilterCallback =
      kiwi::base::RepeatingCallback<void(const std::string&)>;

  explicit FilterWidget(MainWindow* window_base, FilterCallback callback);
  ~FilterWidget() override;

 public:
  void BeginFilter();
  void EndFilter();
  bool has_begun() { return input_started_; }

 public:
  bool OnMousePressed(SDL_MouseButtonEvent* event) override;
  bool OnMouseMove(SDL_MouseMotionEvent* event) override;
  bool OnMouseWheel(SDL_MouseWheelEvent* event) override;
  bool OnMouseReleased(SDL_MouseButtonEvent* event) override;

 private:
  void Paint() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event) override;
  bool OnControllerAxisMotionEvent(SDL_ControllerAxisEvent* event) override;

 private:
  enum class ControllerEditType {
    kInsert,
    kBackspace,
    kClear,
  };

  struct ControllerEdit {
    ControllerEditType type;
    std::string text;
  };

  void StartTextInput();
  void StopTextInput();
  void CommitFilterIfChanged();
  void DrawControllerKeyboard(float top, float width);
  void MoveControllerSelection(int row_delta, int column_delta);
  void ActivateControllerKey();
  void QueueControllerEdit(ControllerEditType type,
                           const char* text = nullptr);
  static int HandleInputTextCallback(ImGuiInputTextCallbackData* data);

 private:
  bool input_started_ = false;
  MainWindow* parent_window_ = nullptr;
  FilterCallback callback_ = kiwi::base::DoNothing();

  // Text buffer edited by ImGui::InputText. It persists between sessions and
  // stores UTF-8 so Chinese, Japanese and other IME input is preserved.
  std::array<char, 256> filter_buffer_ = {};
  // Last value already sent to |callback_|, used to detect real changes.
  std::string filter_contents_;
  // Whether keyboard focus should be forced onto the input on the next paint.
  bool focus_requested_ = false;
  bool controller_mode_ = false;
  int controller_row_ = 0;
  int controller_column_ = 0;
  std::array<int, 2> controller_axis_direction_ = {};
  std::array<Timer, 2> controller_axis_repeat_timer_;
  std::vector<ControllerEdit> pending_controller_edits_;
  // Guards against the widget being painted twice in the same ImGui frame
  // (it is rendered both from the child loop and from the parent's PostPaint).
  int last_painted_frame_ = -1;
};

#endif  // UI_WIDGETS_FILTER_WIDGET_H_
