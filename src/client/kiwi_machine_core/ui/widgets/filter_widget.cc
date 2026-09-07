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

#include <imgui.h>

#include <algorithm>
#include <utility>

#include "build/kiwi_defines.h"
#include "ui/main_window.h"
#include "ui/styles.h"
#include "utility/fonts.h"
#include "utility/localization.h"

namespace {
int g_global_input = 0;

// Search box sizing (in unscaled pixels of the client area).
constexpr float kBoxWidthRatio = 0.6f;
constexpr float kBoxMinWidth = 300.f;
constexpr float kBoxHorizontalMargin = 40.f;
constexpr float kInputMinWidth = 120.f;
constexpr float kConfirmButtonMinWidth = 96.f;
constexpr float kInputButtonSpacing = 8.f;
constexpr float kControllerKeyboardMaxWidth = 720.f;
constexpr float kControllerKeyHeight = 38.f;
constexpr float kControllerKeySpacing = 6.f;
constexpr Sint16 kControllerDeadZone = SDL_JOYSTICK_AXIS_MAX / 3;
constexpr int kControllerRepeatDelayMs = 180;

// Keep the search action visually aligned with InGameMenu's primary buttons.
constexpr ImU32 kButtonTextColor = IM_COL32(245, 247, 245, 255);
constexpr ImU32 kButtonBrandColor = IM_COL32(101, 216, 75, 255);
constexpr ImU32 kButtonBrandOnColor = IM_COL32(16, 40, 12, 255);
constexpr float kButtonCornerRadius = 8.f;

#if KIWI_MOBILE
constexpr PreferredFontSize kSearchPrimaryFontSize = PreferredFontSize::k3x;
constexpr PreferredFontSize kSearchActionFontSize = PreferredFontSize::k3x;
#else
constexpr PreferredFontSize kSearchPrimaryFontSize = PreferredFontSize::k2x;
constexpr PreferredFontSize kSearchActionFontSize = PreferredFontSize::k1x;
#endif

constexpr int kControllerKeyboardRows = 5;
constexpr int kControllerKeyboardColumns = 10;
constexpr std::array<std::array<const char*, kControllerKeyboardColumns>,
                     kControllerKeyboardRows>
    kControllerKeys = {{
        {{"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"}},
        {{"A", "S", "D", "F", "G", "H", "J", "K", "L", nullptr}},
        {{"Z", "X", "C", "V", "B", "N", "M", "-", ".", "'"}},
        {{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"}},
        {{"SPACE", "DELETE", "CLEAR", "DONE", nullptr, nullptr, nullptr,
          nullptr, nullptr, nullptr}},
    }};
constexpr std::array<int, kControllerKeyboardRows> kControllerRowLengths = {
    10, 9, 10, 10, 4};

bool IsAsciiOnly(const char* text) {
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
       *p; ++p) {
    if (*p > 127)
      return false;
  }
  return true;
}

// Chooses a font that can actually render |text|. Unlike the shared
// GetPreferredFont(), which only switches to a CJK font based on the UI
// language, this forces a CJK-capable font whenever the typed content is
// non-ASCII, so the search box shows Chinese/Japanese even when the UI is in
// English. Falls back to the language-preferred font otherwise.
ScopedFont ScopedInputFont(PreferredFontSize size, const char* text) {
  if (!IsAsciiOnly(text)) {
#if !DISABLE_CHINESE_FONT
    return ScopedFont(FontType::kDefaultSimplifiedChinese, size);
#elif !DISABLE_JAPANESE_FONT
    return ScopedFont(FontType::kDefaultJapanese, size);
#endif
  }
  return GetPreferredFont(size, text);
}

bool DrawPrimaryButton(const char* id, const char* label, const ImVec2& size) {
  const bool pressed = ImGui::InvisibleButton(id, size);
  const bool highlighted = ImGui::IsItemHovered() || ImGui::IsItemActive();
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(min, max, kButtonBrandColor, kButtonCornerRadius);
  draw_list->AddRect(min, max,
                     highlighted ? kButtonTextColor : kButtonBrandColor,
                     kButtonCornerRadius, 0, highlighted ? 2.f : 1.f);

  const ImVec2 text_size = ImGui::CalcTextSize(label);
  draw_list->AddText(ImVec2(min.x + (size.x - text_size.x) / 2.f,
                            min.y + (size.y - text_size.y) / 2.f),
                     kButtonBrandOnColor, label);
  return pressed;
}
}  // namespace

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
    focus_requested_ = true;
    controller_mode_ = false;
    controller_row_ = 0;
    controller_column_ = 0;
    controller_axis_direction_.fill(0);
    pending_controller_edits_.clear();
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
  if (!input_started_)
    return;

  // This widget is rendered twice per frame (once from the parent's child loop
  // and once from its PostPaint()). ImGui::InputText must be submitted only
  // once per frame, otherwise it hits an ID collision. Guard against the second
  // invocation.
  int current_frame = ImGui::GetFrameCount();
  if (current_frame == last_painted_frame_)
    return;
  last_painted_frame_ = current_frame;

  SDL_Rect client_bounds = window()->GetClientBounds();

  // The parent (FlexItemsWidget) window has the NoInputs flag, so we open our
  // own full-screen, input-accepting overlay window to host the text field.
  ImGui::SetNextWindowPos(ImVec2(client_bounds.x, client_bounds.y));
  ImGui::SetNextWindowSize(ImVec2(client_bounds.w, client_bounds.h));
  ImGui::SetNextWindowBgAlpha(0.f);  // We draw the dim rect ourselves.
  if (focus_requested_)
    ImGui::SetNextWindowFocus();

  constexpr ImGuiWindowFlags kOverlayFlags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
  if (!ImGui::Begin("##FilterOverlay", nullptr, kOverlayFlags)) {
    ImGui::End();
    return;
  }

  // Dim background.
  ImGui::GetWindowDrawList()->AddRectFilled(
      ImVec2(client_bounds.x, client_bounds.y),
      ImVec2(client_bounds.x + client_bounds.w,
             client_bounds.y + client_bounds.h),
      ImColor(0, 0, 0, 196));

  const float available_width =
      std::max(1.f, client_bounds.w - kBoxHorizontalMargin * 2);
  const float desired_input_width =
      std::min(std::max(kBoxMinWidth, client_bounds.w * kBoxWidthRatio),
               available_width);
  const std::string confirm_text =
      GetLocalizedString(string_resources::IDR_COMMON_CONFIRM);
  float confirm_button_width = kConfirmButtonMinWidth;
  {
    ScopedFont font(
        GetPreferredFont(kSearchActionFontSize, confirm_text.c_str()));
    confirm_button_width =
        std::max(kConfirmButtonMinWidth,
                 ImGui::CalcTextSize(confirm_text.c_str()).x + 32.f);
  }
  const float reserved_input_width =
      std::min(kInputMinWidth, available_width * .6f);
  confirm_button_width =
      std::min(confirm_button_width,
               std::max(1.f, available_width - kInputButtonSpacing -
                                 reserved_input_width));
  const float row_width =
      std::min(available_width, desired_input_width + kInputButtonSpacing +
                                    confirm_button_width);
  const float input_width =
      std::max(1.f, row_width - kInputButtonSpacing - confirm_button_width);
  float expected_input_row_height = 0.f;
  {
    ScopedFont font(
        ScopedInputFont(kSearchPrimaryFontSize, filter_buffer_.data()));
    expected_input_row_height = ImGui::GetFrameHeight();
  }

  // Title.
  std::string title =
      GetLocalizedString(string_resources::IDR_FILTER_WIDGET_TITLE);
  {
    ScopedFont font(GetPreferredFont(kSearchPrimaryFontSize));
    ImVec2 title_rect = ImGui::CalcTextSize(title.c_str());
    ImGui::SetCursorPosX((client_bounds.w - title_rect.x) / 2);
    ImVec2 combined_rect(std::max(title_rect.x, row_width),
                         title_rect.y + ImGui::GetStyle().ItemSpacing.y +
                             expected_input_row_height);
    if (controller_mode_) {
      combined_rect.y += kControllerKeySpacing +
                         kControllerKeyboardRows *
                             (kControllerKeyHeight + kControllerKeySpacing);
    }
    ImGui::SetCursorPosY(
        styles::filter_widget::GetTitleTop(GetLocalBounds(), combined_rect));
    ImGui::TextUnformatted(title.c_str());
  }

  bool confirm_requested = false;
  float input_row_top = ImGui::GetCursorPosY();
  float input_row_height = 0.f;

  // The text field and primary action share one centered row.
  {
    ScopedFont font(
        ScopedInputFont(kSearchPrimaryFontSize, filter_buffer_.data()));
    ImGui::SetCursorPosX((client_bounds.w - row_width) / 2);
    ImGui::SetNextItemWidth(input_width);

    // Move keyboard focus onto the field on the first frame after opening.
    if (focus_requested_) {
      ImGui::SetKeyboardFocusHere();
      focus_requested_ = false;
    }

    // Without EnterReturnsTrue, InputText returns true whenever the content is
    // modified, which gives us real-time filtering.
    constexpr ImGuiInputTextFlags kInputFlags =
        ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CallbackAlways;
    if (ImGui::InputText("##FilterInput", filter_buffer_.data(),
                         filter_buffer_.size(), kInputFlags,
                         &FilterWidget::HandleInputTextCallback, this)) {
      CommitFilterIfChanged();
    }
    input_row_height = ImGui::GetItemRectSize().y;
  }

  ImGui::SameLine(0.f, kInputButtonSpacing);
  {
    ScopedFont font(
        GetPreferredFont(kSearchActionFontSize, confirm_text.c_str()));
    confirm_requested =
        DrawPrimaryButton("##FilterConfirm", confirm_text.c_str(),
                          ImVec2(confirm_button_width, input_row_height));
  }

  if (controller_mode_) {
    DrawControllerKeyboard(
        input_row_top + input_row_height + kControllerKeySpacing, row_width);
  }

  ImGui::End();
  if (confirm_requested) {
    CommitFilterIfChanged();
    EndFilter();
  }
}

bool FilterWidget::OnMousePressed(SDL_MouseButtonEvent* event) {
  controller_mode_ = false;
  return input_started_;
}

bool FilterWidget::OnMouseMove(SDL_MouseMotionEvent* event) {
  return input_started_;
}

bool FilterWidget::OnMouseWheel(SDL_MouseWheelEvent* event) {
  return input_started_;
}

bool FilterWidget::OnMouseReleased(SDL_MouseButtonEvent* event) {
  return input_started_;
}

bool FilterWidget::OnKeyPressed(SDL_KeyboardEvent* event) {
  // While the filter is active, eat all keys so the underlying items widget
  // doesn't navigate. Text editing (typing, backspace, cursor movement) is
  // handled internally by ImGui::InputText through the ImGui SDL2 backend.
  if (input_started_) {
    controller_mode_ = false;
    switch (event->keysym.sym) {
      case SDLK_ESCAPE:
        EndFilter();
        break;
      case SDLK_RETURN:
      case SDLK_KP_ENTER:
        CommitFilterIfChanged();
        EndFilter();
        break;
      default:
        break;
    }
    return true;
  }

  return false;
}

bool FilterWidget::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  if (!input_started_)
    return false;

  controller_mode_ = true;
  focus_requested_ = true;
  switch (event->button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
      MoveControllerSelection(-1, 0);
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
      MoveControllerSelection(1, 0);
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
      MoveControllerSelection(0, -1);
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      MoveControllerSelection(0, 1);
      break;
    case SDL_CONTROLLER_BUTTON_A:
      ActivateControllerKey();
      break;
    case SDL_CONTROLLER_BUTTON_B:
      QueueControllerEdit(ControllerEditType::kBackspace);
      break;
    case SDL_CONTROLLER_BUTTON_X:
      QueueControllerEdit(ControllerEditType::kClear);
      break;
    case SDL_CONTROLLER_BUTTON_Y:
      QueueControllerEdit(ControllerEditType::kInsert, " ");
      break;
    case SDL_CONTROLLER_BUTTON_START:
    case SDL_CONTROLLER_BUTTON_BACK:
      EndFilter();
      break;
    default:
      break;
  }
  return true;
}

bool FilterWidget::OnControllerAxisMotionEvent(SDL_ControllerAxisEvent* event) {
  if (!input_started_)
    return false;

  int axis_index = -1;
  int row_delta = 0;
  int column_delta = 0;
  if (event->axis == SDL_CONTROLLER_AXIS_LEFTX) {
    axis_index = 0;
  } else if (event->axis == SDL_CONTROLLER_AXIS_LEFTY) {
    axis_index = 1;
  } else {
    return false;
  }

  int direction = 0;
  if (event->value <= -kControllerDeadZone)
    direction = -1;
  else if (event->value >= kControllerDeadZone)
    direction = 1;

  if (direction == 0) {
    controller_axis_direction_[axis_index] = 0;
    return true;
  }

  if (controller_axis_direction_[axis_index] == direction &&
      controller_axis_repeat_timer_[axis_index].ElapsedInMilliseconds() <
          kControllerRepeatDelayMs) {
    return true;
  }

  controller_mode_ = true;
  focus_requested_ = true;
  controller_axis_direction_[axis_index] = direction;
  controller_axis_repeat_timer_[axis_index].Reset();
  if (axis_index == 0)
    column_delta = direction;
  else
    row_delta = direction;
  MoveControllerSelection(row_delta, column_delta);
  return true;
}

void FilterWidget::CommitFilterIfChanged() {
  std::string current(filter_buffer_.data());
  if (current == filter_contents_)
    return;

  filter_contents_ = std::move(current);
  SDL_assert(callback_);
  callback_.Run(filter_contents_);
}

void FilterWidget::DrawControllerKeyboard(float top, float width) {
  const float keyboard_width = std::min(width, kControllerKeyboardMaxWidth);
  const ImVec2 window_pos = ImGui::GetWindowPos();
  const float available_height =
      std::max(1.f, window()->GetClientBounds().h - top - 12.f);
  const float key_height = std::max(
      24.f, std::min(kControllerKeyHeight,
                     (available_height -
                      kControllerKeySpacing * (kControllerKeyboardRows - 1)) /
                         kControllerKeyboardRows));
  const float left =
      window_pos.x + (window()->GetClientBounds().w - keyboard_width) / 2;
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ScopedFont font(GetPreferredFont(PreferredFontSize::k1x));

  for (int row = 0; row < kControllerKeyboardRows; ++row) {
    const int key_count = kControllerRowLengths[row];
    const float key_width =
        (keyboard_width - kControllerKeySpacing * (key_count - 1)) / key_count;
    for (int column = 0; column < key_count; ++column) {
      const float x = left + column * (key_width + kControllerKeySpacing);
      const float y =
          window_pos.y + top + row * (key_height + kControllerKeySpacing);
      const ImVec2 min(x, y);
      const ImVec2 max(x + key_width, y + key_height);
      const bool selected =
          row == controller_row_ && column == controller_column_;
      draw_list->AddRectFilled(
          min, max, selected ? ImColor(227, 179, 65) : ImColor(36, 40, 56),
          4.f);
      draw_list->AddRect(
          min, max, selected ? ImColor(255, 226, 140) : ImColor(75, 81, 105),
          4.f);

      const char* label = kControllerKeys[row][column];
      ImVec2 label_size = ImGui::CalcTextSize(label);
      draw_list->AddText(
          font.GetFont(), font.GetFontSize(),
          ImVec2(x + (key_width - label_size.x) / 2,
                 y + (key_height - label_size.y) / 2),
          selected ? ImColor(20, 20, 20) : ImColor(235, 237, 244), label);
    }
  }
}

void FilterWidget::MoveControllerSelection(int row_delta, int column_delta) {
  if (row_delta != 0) {
    controller_row_ = (controller_row_ + row_delta + kControllerKeyboardRows) %
                      kControllerKeyboardRows;
    controller_column_ = std::min(controller_column_,
                                  kControllerRowLengths[controller_row_] - 1);
  }
  if (column_delta != 0) {
    const int row_length = kControllerRowLengths[controller_row_];
    controller_column_ =
        (controller_column_ + column_delta + row_length) % row_length;
  }
}

void FilterWidget::ActivateControllerKey() {
  const char* key = kControllerKeys[controller_row_][controller_column_];
  if (controller_row_ < kControllerKeyboardRows - 1) {
    QueueControllerEdit(ControllerEditType::kInsert, key);
    return;
  }

  switch (controller_column_) {
    case 0:
      QueueControllerEdit(ControllerEditType::kInsert, " ");
      break;
    case 1:
      QueueControllerEdit(ControllerEditType::kBackspace);
      break;
    case 2:
      QueueControllerEdit(ControllerEditType::kClear);
      break;
    case 3:
      EndFilter();
      break;
    default:
      SDL_assert(false);
      break;
  }
}

void FilterWidget::QueueControllerEdit(ControllerEditType type,
                                       const char* text) {
  pending_controller_edits_.push_back({type, std::string(text ? text : "")});
}

int FilterWidget::HandleInputTextCallback(ImGuiInputTextCallbackData* data) {
  auto* widget = static_cast<FilterWidget*>(data->UserData);
  for (const ControllerEdit& edit : widget->pending_controller_edits_) {
    switch (edit.type) {
      case ControllerEditType::kInsert:
        data->InsertChars(data->CursorPos, edit.text.c_str());
        break;
      case ControllerEditType::kBackspace: {
        if (data->SelectionStart != data->SelectionEnd) {
          const int start = std::min(data->SelectionStart, data->SelectionEnd);
          const int end = std::max(data->SelectionStart, data->SelectionEnd);
          data->DeleteChars(start, end - start);
          break;
        }
        if (data->CursorPos <= 0)
          break;
        int start = data->CursorPos - 1;
        while (start > 0 &&
               (static_cast<unsigned char>(data->Buf[start]) & 0xC0) == 0x80) {
          --start;
        }
        data->DeleteChars(start, data->CursorPos - start);
        break;
      }
      case ControllerEditType::kClear:
        data->DeleteChars(0, data->BufTextLen);
        break;
    }
  }
  widget->pending_controller_edits_.clear();
  return 0;
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
