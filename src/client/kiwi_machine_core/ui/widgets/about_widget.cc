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

#include "ui/widgets/about_widget.h"

#include <SDL_image.h>
#include <imgui.h>
#include <nes/types.h>

#include "ui/main_window.h"
#include "ui/styles.h"
#include "ui/widgets/stack_widget.h"
#include "ui/window_base.h"
#include "utility/audio_effects.h"
#include "utility/images.h"
#include "utility/key_mapping_util.h"
#include "utility/localization.h"

template <size_t N, typename T>
constexpr size_t ArrayCount(const T (&)[N]) {
  return N;
}

AboutWidget::AboutWidget(MainWindow* main_window,
                         StackWidget* parent,
                         NESRuntimeID runtime_id)
    : Widget(main_window), parent_(parent), main_window_(main_window) {
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
  set_flags(window_flags);
  set_title("About");
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
}

AboutWidget::~AboutWidget() = default;

void AboutWidget::Close() {
  SDL_assert(parent_);
  parent_->PopWidget();
}

void AboutWidget::Paint() {
  ImGui::SetCursorPosY(20);

  DrawBackground();
  DrawTitle();
  Separator();
  DrawController();
  Separator();
  DrawGameSelection();
  Separator();
  DrawAbout();
}

void AboutWidget::OnWindowResized() {
  set_bounds(window()->GetClientBounds());
}

bool AboutWidget::OnKeyPressed(SDL_KeyboardEvent* event) {
  return HandleInputEvent(event, nullptr);
}

bool AboutWidget::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  return HandleInputEvent(nullptr, event);
}

void AboutWidget::OnWindowPreRender() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
}

void AboutWidget::OnWindowPostRender() {
  ImGui::PopStyleVar(2);
}

bool AboutWidget::OnMouseReleased(SDL_MouseButtonEvent* event) {
  if (event->button == SDL_BUTTON_RIGHT) {
    PlayEffect(audio_resources::AudioID::kBack);
    Close();
    return true;
  }
  return false;
}

#if KIWI_MOBILE
bool AboutWidget::OnTouchFingerUp(SDL_TouchFingerEvent* event) {
  PlayEffect(audio_resources::AudioID::kBack);
  Close();
  return true;
}
#endif

bool AboutWidget::HandleInputEvent(SDL_KeyboardEvent* k,
                                   SDL_ControllerButtonEvent* c) {
  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kB, k) ||
      (c && c->button == SDL_CONTROLLER_BUTTON_B)) {
    PlayEffect(audio_resources::AudioID::kBack);
    Close();
    return true;
  }

  return false;
}

PreferredFontSize AboutWidget::PreferredTitleFontSize() {
  return main_window_->window_scale() > 2.f ? PreferredFontSize::k2x
                                            : PreferredFontSize::k1x;
}

void AboutWidget::ResetCursorX() {
  ImGui::SetCursorPosX(
      styles::about_widget::GetMarginX(main_window_->window_scale()));
}

void AboutWidget::Separator() {
  if (main_window_->window_scale() > 2.f)
    ImGui::Dummy(ImVec2(1, 20));
  else
    ImGui::Dummy(ImVec2(1, 10));
}

void AboutWidget::DrawBackground() {
  SDL_Rect bounds_in_window = MapToWindow(bounds());
  ImGui::GetWindowDrawList()->AddRectFilled(
      ImVec2(bounds_in_window.x, bounds_in_window.y),
      ImVec2(bounds_in_window.x + bounds_in_window.w,
             bounds_in_window.y + bounds_in_window.h),
      IM_COL32(0, 0, 0, 196));
}

void AboutWidget::DrawTitle() {
  using namespace string_resources;
  ResetCursorX();
  ImGui::BeginGroup();
  ImGui::Image(
      GetImage(window()->renderer(), image_resources::ImageID::kKiwiMachine),
      ImVec2(80, 80));
  ImGui::EndGroup();
  ImGui::SameLine();

  ImGui::BeginGroup();
  {
    const char* title = GetLocalizedString(IDR_ABOUT_KIWI_MACHINE).c_str();
    ScopedFont font_title = GetPreferredFont(PreferredFontSize::k3x, title);
    ImGui::TextUnformatted(title);
  }
  {
    ScopedFont font_title =
        GetPreferredFont(PreferredFontSize::k1x,
                         GetLocalizedString(IDR_ABOUT_INSTRUCTIONS).c_str());
    ImGui::TextUnformatted(
        GetLocalizedString(string_resources::IDR_ABOUT_INSTRUCTIONS).c_str());
  }
  {
    ScopedFont font_title =
        GetPreferredFont(PreferredFontSize::k1x, FontType::kSystemDefault);
    ImGui::Text("V2.0.0");
  }
  ImGui::EndGroup();
}

void AboutWidget::DrawController() {
  using namespace string_resources;
  {
    ResetCursorX();
    ScopedFont font_title = GetPreferredFont(
        PreferredTitleFontSize(),
        GetLocalizedString(string_resources::IDR_ABOUT_CONTROLLER).c_str());
    ImGui::TextUnformatted(
        GetLocalizedString(string_resources::IDR_ABOUT_CONTROLLER).c_str());
  }

  ResetCursorX();
  ImGui::BeginGroup();
  ImGui::Image(
      GetImage(window()->renderer(),
               image_resources::ImageID::kAboutNesJoysticks),
      main_window_->window_scale() > 2.f ? ImVec2(250, 150) : ImVec2(120, 72));
  ImGui::EndGroup();
  ImGui::SameLine(0, main_window_->window_scale() > 2.f ? 40 : 15);

  ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, IM_COL32_WHITE);
  ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, IM_COL32_WHITE);
  ImGui::PushStyleColor(ImGuiCol_TableBorderLight, IM_COL32_WHITE);
  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(10, 4));

  const char* kKeyboardHeaderStrings[] = {
      GetLocalizedString(IDR_ABOUT_CONTROLLER_INPUT).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_UP).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_DOWN).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_LEFT).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_RIGHT).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_A).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_B).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_SELECT).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_START).c_str(),
  };

  const char* kKeyboardStringsP1[] = {
      GetLocalizedString(IDR_ABOUT_CONTROLLER_KEYBOARD_1).c_str(),
      "W",
      "S",
      "A",
      "D",
      "J",
      "K",
      "L",
      "Enter",
  };

  const char* kKeyboardStringsP2[] = {
      GetLocalizedString(IDR_ABOUT_CONTROLLER_KEYBOARD_2).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_KEY_UP).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_KEY_DOWN).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_KEY_LEFT).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_KEY_RIGHT).c_str(),
      "Delete",
      "End",
      "PageDown",
      "Home",
  };

  const char* kGamepadHeaderStrings[] = {
      GetLocalizedString(IDR_ABOUT_CONTROLLER_INPUT).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_DIRECTION).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_A).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_B).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_SELECT).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_START).c_str(),
  };

  const char* kXBoxStrings[] = {
      GetLocalizedString(IDR_ABOUT_CONTROLLER_XBOX).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_XBOX_DIRECTION).c_str(),
      "A",
      "X",
      GetLocalizedString(IDR_ABOUT_CONTROLLER_XBOX_VIEW).c_str(),
      GetLocalizedString(IDR_ABOUT_CONTROLLER_XBOX_MENU).c_str(),
  };
  static_assert(
      ArrayCount(kKeyboardHeaderStrings) == ArrayCount(kKeyboardStringsP1) &&
      ArrayCount(kKeyboardHeaderStrings) == ArrayCount(kKeyboardStringsP2));
  static_assert(ArrayCount(kGamepadHeaderStrings) == ArrayCount(kXBoxStrings));
  const char** kKeyboardRowContents[] = {
      kKeyboardHeaderStrings, kKeyboardStringsP1, kKeyboardStringsP2};

  ImGui::BeginGroup();
  {
    ScopedFont font = GetPreferredFont(
        PreferredFontSize::k1x,
        GetLocalizedString(IDR_ABOUT_CONTROLLER_KEYBOARD).c_str());
    ImGui::TextUnformatted(
        GetLocalizedString(IDR_ABOUT_CONTROLLER_KEYBOARD).c_str());
  }
  if (ImGui::BeginTable("keyboard_table", ArrayCount(kKeyboardHeaderStrings),
                        ImGuiTableFlags_Borders |
                            ImGuiTableFlags_NoHostExtendX |
                            ImGuiTableFlags_SizingFixedFit)) {
    for (const auto& kRowContent : kKeyboardRowContents) {
      (kRowContent == kKeyboardRowContents[0]) ? ImGui::TableHeadersRow()
                                               : ImGui::TableNextRow();
      for (int column = 0; column < ArrayCount(kKeyboardHeaderStrings);
           ++column) {
        ScopedFont font = GetPreferredFont(PreferredFontSize::k1x);
        ImGui::TableSetColumnIndex(column);
        (kRowContent == kKeyboardRowContents[0])
            ? ImGui::TextColored(ImVec4(0, 0, 0, 1), "%s", kRowContent[column])
            : ImGui::TextUnformatted(kRowContent[column]);
      }
    }
    ImGui::EndTable();
  }

  {
    ScopedFont font = GetPreferredFont(
        PreferredFontSize::k1x,
        GetLocalizedString(IDR_ABOUT_CONTROLLER_GAMEPAD).c_str());
    ImGui::TextUnformatted(
        GetLocalizedString(IDR_ABOUT_CONTROLLER_GAMEPAD).c_str());
  }
  const char** kGamepadRowContents[] = {kGamepadHeaderStrings, kXBoxStrings};
  if (ImGui::BeginTable("gamepad_table", ArrayCount(kGamepadHeaderStrings),
                        ImGuiTableFlags_Borders |
                            ImGuiTableFlags_NoHostExtendX |
                            ImGuiTableFlags_SizingFixedFit)) {
    for (const auto& kRowContent : kGamepadRowContents) {
      (kRowContent == kGamepadRowContents[0]) ? ImGui::TableHeadersRow()
                                              : ImGui::TableNextRow();
      for (int column = 0; column < ArrayCount(kGamepadHeaderStrings);
           ++column) {
        ScopedFont font = GetPreferredFont(PreferredFontSize::k1x);
        ImGui::TableSetColumnIndex(column);
        (kRowContent == kGamepadRowContents[0])
            ? ImGui::TextColored(ImVec4(0, 0, 0, 1), "%s", kRowContent[column])
            : ImGui::TextUnformatted(kRowContent[column]);
      }
    }
    ImGui::EndTable();
  }
  ImGui::EndGroup();
  ImGui::PopStyleVar(1);
  ImGui::PopStyleColor(3);
}

void AboutWidget::DrawGameSelection() {
  using namespace string_resources;
  {
    ResetCursorX();
    const char* title = GetLocalizedString(IDR_ABOUT_GAME_SELECTION).c_str();
    ScopedFont font = GetPreferredFont(PreferredTitleFontSize(), title);
    ImGui::TextUnformatted(title);
  }
  {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 4));
    ResetCursorX();
    ScopedFont font = GetPreferredFont(
        PreferredFontSize::k1x,
        GetLocalizedString(IDR_ABOUT_GAME_SELECTION_CHANGE_VERSION_0).c_str());
    ImGui::TextUnformatted(
        GetLocalizedString(IDR_ABOUT_GAME_SELECTION_CHANGE_VERSION_0).c_str());
    ImGui::SameLine();
    ImGui::Image(
        GetImage(window()->renderer(), image_resources::ImageID::kItemBadge),
        ImVec2(font.GetFont()->FontSize, font.GetFont()->FontSize));
    ImGui::SameLine();
    ImGui::TextUnformatted(
        GetLocalizedString(IDR_ABOUT_GAME_SELECTION_CHANGE_VERSION_1).c_str());
    ResetCursorX();
    ImGui::TextUnformatted(
        GetLocalizedString(IDR_ABOUT_GAME_SELECTION_CHANGE_VERSION_2).c_str());
    ImGui::PopStyleVar(1);
  }
}

void AboutWidget::DrawAbout() {
  using namespace string_resources;
  {
    ResetCursorX();
    const char* title = GetLocalizedString(IDR_ABOUT_ABOUT).c_str();
    ScopedFont font = GetPreferredFont(PreferredTitleFontSize(), title);
    ImGui::TextUnformatted(title);
  }
  {
    ScopedFont font = GetPreferredFont(PreferredFontSize::k1x);
    ResetCursorX();
    ImGui::TextUnformatted(GetLocalizedString(IDR_ABOUT_ABOUT_GITHUB).c_str());
    ResetCursorX();
    ImGui::TextUnformatted(GetLocalizedString(IDR_ABOUT_ABOUT_AUTHOR).c_str());
  }
}
