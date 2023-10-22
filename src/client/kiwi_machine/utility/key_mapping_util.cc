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

#include "utility/key_mapping_util.h"
#include "ui/application.h"
#include "utility/timer.h"

bool IsJoystickButtonMatch(NESRuntime::Data* runtime_data,
                           kiwi::nes::ControllerButton button,
                           SDL_Keysym key) {
  for (auto mapping : runtime_data->keyboard_mappings) {
    if (mapping.mapping[static_cast<int>(button)] == key.sym)
      return true;
  }

  return false;
}

bool IsJoystickAxisMotionMatch(NESRuntime::Data* runtime_data,
                               kiwi::nes::ControllerButton button) {
  // An array to cache last trigger timestamp for X and Y axis motion, avoiding
  // trigger to fast.
  enum TriggerCache { kX, kY, kMax };
  static Timer g_last_trigger[kMax];
  constexpr int kGapMs = 100;

  bool matched = false;
  for (auto joystick_mappings : runtime_data->joystick_mappings) {
    // This mapping is not available, because its controller is not connected.
    if (!joystick_mappings.which)
      continue;

    SDL_GameController* game_controller =
        reinterpret_cast<SDL_GameController*>(joystick_mappings.which);
    constexpr Sint16 kDeadZoom = SDL_JOYSTICK_AXIS_MAX / 3;
    switch (button) {
      case kiwi::nes::ControllerButton::kLeft: {
        if (g_last_trigger[kX].ElapsedInMilliseconds() < kGapMs)
          break;

        Sint16 x = SDL_GameControllerGetAxis(game_controller,
                                             SDL_CONTROLLER_AXIS_LEFTX);
        if (SDL_JOYSTICK_AXIS_MIN <= x && x <= -kDeadZoom) {
          matched = true;
          g_last_trigger[kX].Reset();
        }
      } break;
      case kiwi::nes::ControllerButton::kRight: {
        if (g_last_trigger[kX].ElapsedInMilliseconds() < kGapMs)
          break;

        Sint16 x = SDL_GameControllerGetAxis(game_controller,
                                             SDL_CONTROLLER_AXIS_LEFTX);
        if (kDeadZoom <= x && x <= SDL_JOYSTICK_AXIS_MAX) {
          matched = true;
          g_last_trigger[kX].Reset();
        }
      } break;
      case kiwi::nes::ControllerButton::kUp: {
        if (g_last_trigger[kY].ElapsedInMilliseconds() < kGapMs)
          break;

        Sint16 y = SDL_GameControllerGetAxis(game_controller,
                                             SDL_CONTROLLER_AXIS_LEFTY);
        if (SDL_JOYSTICK_AXIS_MIN <= y && y <= -kDeadZoom) {
          matched = true;
          g_last_trigger[kY].Reset();
        }
      } break;
      case kiwi::nes::ControllerButton::kDown: {
        if (g_last_trigger[kY].ElapsedInMilliseconds() < kGapMs)
          break;

        Sint16 y = SDL_GameControllerGetAxis(game_controller,
                                             SDL_CONTROLLER_AXIS_LEFTY);
        if (kDeadZoom <= y && y <= SDL_JOYSTICK_AXIS_MAX) {
          matched = true;
          g_last_trigger[kY].Reset();
        }
      } break;
      default:
        break;
    }

    if (matched)
      return true;
  }

  return false;
}

bool IsKeyboardOrControllerAxisMotionMatch(NESRuntime::Data* runtime_data,
                                           kiwi::nes::ControllerButton button,
                                           SDL_KeyboardEvent* k) {
  return (k && IsJoystickButtonMatch(runtime_data, button, k->keysym)) ||
         IsJoystickAxisMotionMatch(runtime_data, button);
}

void SetControllerMapping(NESRuntime::Data* runtime_data,
                          int player,
                          SDL_GameController* controller,
                          bool ab_reverse) {
  SDL_assert(player == 0 || player == 1);
  NESRuntime::Data::ControllerMapping joy_mapping =
      !ab_reverse ? NESRuntime::Data::
                        ControllerMapping{SDL_CONTROLLER_BUTTON_A,
                                          SDL_CONTROLLER_BUTTON_X,
                                          SDL_CONTROLLER_BUTTON_BACK,
                                          SDL_CONTROLLER_BUTTON_START,
                                          SDL_CONTROLLER_BUTTON_DPAD_UP,
                                          SDL_CONTROLLER_BUTTON_DPAD_DOWN,
                                          SDL_CONTROLLER_BUTTON_DPAD_LEFT,
                                          SDL_CONTROLLER_BUTTON_DPAD_RIGHT}
                  : NESRuntime::Data::ControllerMapping{
                        SDL_CONTROLLER_BUTTON_X,
                        SDL_CONTROLLER_BUTTON_A,
                        SDL_CONTROLLER_BUTTON_BACK,
                        SDL_CONTROLLER_BUTTON_START,
                        SDL_CONTROLLER_BUTTON_DPAD_UP,
                        SDL_CONTROLLER_BUTTON_DPAD_DOWN,
                        SDL_CONTROLLER_BUTTON_DPAD_LEFT,
                        SDL_CONTROLLER_BUTTON_DPAD_RIGHT};
  runtime_data->joystick_mappings[player] = {controller, joy_mapping};
}

std::vector<SDL_GameController*> GetControllerList() {
  std::vector<SDL_GameController*> result;
  std::set<SDL_GameController*> controllers =
      Application::Get()->game_controllers();

  // First controller means no joystick, or doesn't use any joysticks.
  result.push_back(nullptr);

  for (auto* controller : controllers) {
    result.push_back(controller);
  }

  return result;
}
