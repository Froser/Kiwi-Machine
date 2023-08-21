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

#include "ui/widgets/controller_widget.h"

#include "ui/application.h"
#include "ui/main_window.h"
#include "ui/widgets/stack_widget.h"
#include "utility/audio_effects.h"
#include "utility/images.h"
#include "utility/key_mapping_util.h"

namespace {
void Nextline(int& y) {
  constexpr int kSpacing = 5;
  y += ImGui::GetFont()->FontSize + kSpacing;
}

bool HasGameController(NESRuntime::Data* runtime_data, int player) {
  SDL_assert(player == 0 || player == 1);
  return runtime_data->joystick_mappings[player].which;
}

std::string TranslateKey(char i) {
  if (SDL_isalpha(i))
    return std::string() + i;

  switch (i) {
    case SDLK_RETURN:
      return "Enter";
    case SDLK_DELETE:
      return "Del";
    case SDLK_END:
      return "End";
    case SDLK_PAGEDOWN:
      return "PgDown";
    case SDLK_HOME:
      return "Home";
    case SDLK_UP:
      return "Up";
    case SDLK_DOWN:
      return "Down";
    case SDLK_LEFT:
      return "Left";
    case SDLK_RIGHT:
      return "Right";
    default:
      SDL_assert(false);
      return kiwi::base::NumberToString(i);
  }
}

std::string TranslateButton(int button) {
  switch (button) {
    case SDL_CONTROLLER_BUTTON_A:
      return "A";
    case SDL_CONTROLLER_BUTTON_B:
      return "B";
    case SDL_CONTROLLER_BUTTON_X:
      return "X";
    case SDL_CONTROLLER_BUTTON_Y:
      return "Y";
    default:
      SDL_assert(false);
      return kiwi::base::NumberToString(button);
  }
}

}  // namespace

ControllerWidget::ControllerWidget(MainWindow* main_window,
                                   StackWidget* parent,
                                   NESRuntimeID runtime_id)
    : Widget(main_window), parent_(parent), main_window_(main_window) {
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground;
  set_flags(window_flags);
  set_title("Controller");
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
}

ControllerWidget::~ControllerWidget() = default;

void ControllerWidget::Close() {
  SDL_assert(parent_);
  parent_->PopWidget();
}

void ControllerWidget::Paint() {
  constexpr int kContentWidth = 480;
  constexpr int kContentHeight = 620;
  constexpr float kJoystickImageScale = 0.5;
  SDL_Rect client_bounds = window()->GetClientBounds();
  set_bounds(SDL_Rect{0, 0, client_bounds.w, client_bounds.h});
  SDL_Rect content_center{(bounds().w - kContentWidth) / 2,
                          (bounds().h - kContentHeight) / 2, kContentWidth,
                          kContentHeight};

  SDL_Texture* joystick =
      GetImage(window()->renderer(), image_resources::ImageID::kJoystickLogo);
  int joystick_width, joystick_height;
  SDL_QueryTexture(joystick, nullptr, nullptr, &joystick_width,
                   &joystick_height);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 image_center(
      content_center.x +
          (content_center.w - joystick_width * kJoystickImageScale) / 2,
      content_center.y);

  // Show a joystick for demonstration.
  draw_list->AddImage(
      joystick, image_center,
      ImVec2(image_center.x + joystick_width * kJoystickImageScale,
             image_center.y + joystick_height * kJoystickImageScale));

  constexpr int kSpacing = 10;
  int top = image_center.y + joystick_height * kJoystickImageScale + kSpacing;
  int stored_top = top;
  int left[] = {content_center.x, content_center.x + content_center.w / 2};
  const int kDefaultLeft1 = left[0];
  const int kDefaultLeft2 = left[1];

  // Draws selection rect:
  constexpr int kRounding = 10;
  constexpr ImU32 kBackgroundColor = IM_COL32(117, 130, 252, 128);
  SDL_Rect selected_player_rect =
      selected_player_ == 0
          ? SDL_Rect{kDefaultLeft1, top, kDefaultLeft2 - kDefaultLeft1,
                     kContentHeight - top}
          : SDL_Rect{kDefaultLeft2, top, kDefaultLeft2 - kDefaultLeft1,
                     kContentHeight - top};
  draw_list->AddRectFilled(
      ImVec2(selected_player_rect.x, selected_player_rect.y),
      ImVec2(selected_player_rect.x + selected_player_rect.w,
             selected_player_rect.y + selected_player_rect.h),
      kBackgroundColor, kRounding);

  char buffer[64];
#define TEXT(player, ...)                        \
  snprintf(buffer, sizeof(buffer), __VA_ARGS__); \
  draw_list->AddText(ImVec2(left[(player - 1)], top), IM_COL32_BLACK, buffer);
#define HEADER(player, ...)                                                 \
  snprintf(buffer, sizeof(buffer), __VA_ARGS__);                            \
  draw_list->AddText(ImGui::GetFont(), 18, ImVec2(left[(player - 1)], top), \
                     IM_COL32_BLACK, buffer)
#define JOYNAME(player)                                                \
  runtime_data_->joystick_mappings[(player - 1)].which                 \
      ? (SDL_GameControllerName(reinterpret_cast<SDL_GameController*>( \
            runtime_data_->joystick_mappings[(player - 1)].which)))    \
      : "None"
#define JOYBUTTON(player, button)                                    \
  TranslateButton(                                                   \
      runtime_data_->joystick_mappings[(player - 1)].mapping.button) \
      .c_str()

#define MAPPING(player, button) \
  TranslateKey(runtime_data_->keyboard_mappings[(player - 1)].button).c_str()
#define IMAGE(player, image_id)                                    \
  {                                                                \
    int w, h;                                                      \
    SDL_Texture* tex =                                             \
        GetImage(window()->renderer(), image_resources::image_id); \
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);               \
    draw_list->AddImage(tex, ImVec2(left[(player - 1)], top),      \
                        ImVec2(left[(player - 1)] + w, top + h));  \
    left[(player - 1)] += w;                                       \
  }
#define SPACING(player, spacing) left[(player - 1)] += spacing;
#define RET() Nextline(top)
#define CARRIAGE()        \
  left_1 = kDefaultLeft1; \
  left_2 = kDefaultLeft2;
#define SAVE_TOP() stored_top = top
#define RESTORE_TOP() top = stored_top

  // Player 1 & 2
  HEADER(1, "Player1");
  HEADER(2, "Player2");
  RET();
  RET();

  // Mapping
  TEXT(1, "Up        %s", MAPPING(1, Up));
  TEXT(2, "Up        %s", MAPPING(2, Up));
  RET();
  TEXT(1, "Down      %s", MAPPING(1, Down));
  TEXT(2, "Down      %s", MAPPING(2, Down));
  RET();
  TEXT(1, "Left      %s", MAPPING(1, Left));
  TEXT(2, "Left      %s", MAPPING(2, Left));
  RET();
  TEXT(1, "Right     %s", MAPPING(1, Right));
  TEXT(2, "Right     %s", MAPPING(2, Right));
  RET();
  TEXT(1, "A         %s", MAPPING(1, A));
  TEXT(2, "A         %s", MAPPING(2, A));
  RET();
  TEXT(1, "B         %s", MAPPING(1, B));
  TEXT(2, "B         %s", MAPPING(2, B));
  RET();
  TEXT(1, "Select    %s", MAPPING(1, Select));
  TEXT(2, "Select    %s", MAPPING(2, Select));
  RET();
  TEXT(1, "Start     %s", MAPPING(1, Start));
  TEXT(2, "Start     %s", MAPPING(2, Start));
  RET();
  RET();

  // Joysticks
  HEADER(1, "Joystick:");
  HEADER(2, "Joystick:");
  RET();
  RET();
  TEXT(1, "%s", JOYNAME(1));
  TEXT(2, "%s", JOYNAME(2));
  RET();

  if (selected_player_ == 0) {
    TEXT(1, "Press A To Change Joystick");
  } else {
    TEXT(2, "Press A To Change Joystick");
  }

  RET();
  RET();

  SAVE_TOP();
  for (int i = 1; i <= 2; ++i) {
    RESTORE_TOP();
    if (HasGameController(runtime_data_, i - 1)) {
      TEXT(i, "Joystick Mapping:")
      RET();
      IMAGE(i, ImageID::kXboxOneControllerLogo);
      SPACING(i, 5);
      TEXT(i, "A => XBOX %s", JOYBUTTON(i, A));
      RET();
      TEXT(i, "B => XBOX %s", JOYBUTTON(i, B));
      RET();
      RET();
      TEXT(i, "Press X To\nReverse AB");
      RET();
    }
  }

#undef SAVE_TOP
#undef RESTORE_TOP
#undef CARRIAGE
#undef RET
#undef JOYNAME
#undef JOYBUTTON
#undef MAPPING
#undef HEADER
#undef IMAGE
#undef TEXT
}

void ControllerWidget::OnWindowResized() {
  set_bounds(window()->GetClientBounds());
}

bool ControllerWidget::OnKeyPressed(SDL_KeyboardEvent* event) {
  return HandleInputEvents(event, nullptr);
}

bool ControllerWidget::OnControllerButtonPressed(
    SDL_ControllerButtonEvent* event) {
  return HandleInputEvents(nullptr, event);
}

bool ControllerWidget::OnControllerAxisMotionEvents(
    SDL_ControllerAxisEvent* event) {
  return HandleInputEvents(nullptr, nullptr);
}

bool ControllerWidget::HandleInputEvents(SDL_KeyboardEvent* k,
                                         SDL_ControllerButtonEvent* c) {
  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kB, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_B) {
    PlayEffect(audio_resources::AudioID::kBack);
    Close();
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kA, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_A) {
    PlayEffect(audio_resources::AudioID::kSelect);
    SwitchGameController(selected_player_);
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kSelect, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_X) {
    PlayEffect(audio_resources::AudioID::kSelect);
    ReverseGameControllerAB(selected_player_);
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kLeft, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
    selected_player_--;
    if (selected_player_ < 0)
      selected_player_ = 0;
    else
      PlayEffect(audio_resources::AudioID::kSelect);
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kRight, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
    selected_player_++;
    if (selected_player_ > 1)
      selected_player_ = 1;
    else
      PlayEffect(audio_resources::AudioID::kSelect);
    return true;
  }

  return false;
}

void ControllerWidget::SwitchGameController(int player) {
  SDL_assert(player == 0 || player == 1);
  const auto& controllers = Application::Get()->game_controllers();
  auto* current_controller = runtime_data_->joystick_mappings[player].which;

  std::vector<SDL_GameController*> controller_list{nullptr};
  for (auto controller : controllers) {
    controller_list.push_back(controller);
  }

  int next_controller_id = 0;
  for (size_t i = 0; i < controller_list.size(); ++i) {
    if (controller_list[i] == current_controller) {
      next_controller_id = (i + 1) % controller_list.size();
      break;
    }
  }
  SetControllerMapping(runtime_data_, player,
                       controller_list[next_controller_id], false);
}

void ControllerWidget::ReverseGameControllerAB(int player) {
  auto* current_controller = runtime_data_->joystick_mappings[player].which;
  if (current_controller) {
    bool current_is_reversed =
        runtime_data_->joystick_mappings[player].mapping.A !=
        SDL_CONTROLLER_BUTTON_A;
    SetControllerMapping(
        runtime_data_, player,
        reinterpret_cast<SDL_GameController*>(current_controller),
        !current_is_reversed);
  }
}