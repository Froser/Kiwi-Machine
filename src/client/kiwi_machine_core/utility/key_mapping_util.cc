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

#include <algorithm>

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

bool IsGameControllerInputSupported(SDL_GameController* controller) {
  if (!controller)
    return false;

#if KIWI_SWITCH
  // devkitPro's generic "Switch Controller" can have an unknown SDL type even
  // though its fixed mapping provides valid buttons and leftx/lefty axes.
  return true;
#else
  // Unknown desktop controllers may expose axes with incorrect semantics.
  return SDL_GameControllerGetType(controller) != SDL_CONTROLLER_TYPE_UNKNOWN;
#endif
}

bool IsJoystickAxisMotionMatch(kiwi::nes::ControllerButton button) {
  // An array to cache last trigger timestamp for X and Y axis motion, avoiding
  // trigger to fast.
  enum TriggerCache { kX, kY, kMax };
  static Timer g_last_trigger[kMax];
  constexpr int kGapMs = 100;

  bool matched = false;
  std::set<SDL_GameController*> controllers =
      Application::Get()->game_controllers();
  for (auto game_controller : controllers) {
    if (!IsGameControllerInputSupported(game_controller))
      continue;

    constexpr Sint16 kDeadZoom = SDL_JOYSTICK_AXIS_MAX / 3;
    switch (button) {
      case kiwi::nes::ControllerButton::kLeft: {
        if (!SDL_GameControllerHasAxis(game_controller,
                                       SDL_CONTROLLER_AXIS_LEFTX))
          break;

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
        if (!SDL_GameControllerHasAxis(game_controller,
                                       SDL_CONTROLLER_AXIS_LEFTX))
          break;

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
        if (!SDL_GameControllerHasAxis(game_controller,
                                       SDL_CONTROLLER_AXIS_LEFTY))
          break;

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
        if (!SDL_GameControllerHasAxis(game_controller,
                                       SDL_CONTROLLER_AXIS_LEFTY))
          break;

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
         IsJoystickAxisMotionMatch(button);
}

bool IsGameSelectionConfirmButton(const SDL_ControllerButtonEvent* event) {
  if (!event)
    return false;

#if KIWI_SWITCH
  // The Switch SDL2 mapping exposes the physical A button as SDL B.
  return event->button == SDL_CONTROLLER_BUTTON_B;
#else
  return event->button == SDL_CONTROLLER_BUTTON_A ||
         event->button == SDL_CONTROLLER_BUTTON_START;
#endif
}

bool IsGameSelectionBackButton(const SDL_ControllerButtonEvent* event) {
  if (!event)
    return false;

#if KIWI_SWITCH
  // The Switch SDL2 mapping exposes the physical B button as SDL A.
  return event->button == SDL_CONTROLLER_BUTTON_A;
#else
  return event->button == SDL_CONTROLLER_BUTTON_X;
#endif
}

bool IsGameSelectionSearchButton(const SDL_ControllerButtonEvent* event) {
  if (!event)
    return false;

#if KIWI_SWITCH
  // Switch SDL uses Xbox-style positions, so physical Y is exposed as SDL X.
  return event->button == SDL_CONTROLLER_BUTTON_X;
#else
  return false;
#endif
}

bool IsGameSelectionVersionButton(const SDL_ControllerButtonEvent* event) {
  if (!event)
    return false;

#if KIWI_SWITCH
  return event->button == SDL_CONTROLLER_BUTTON_BACK;
#else
  return event->button == SDL_CONTROLLER_BUTTON_Y;
#endif
}

void SetControllerMapping(NESRuntime::Data* runtime_data,
                          int player,
                          SDL_GameController* controller,
                          bool ab_reverse) {
  SDL_assert(player == 0 || player == 1);
#if KIWI_SWITCH
  // devkitPro's SDL mapping uses Xbox-style button positions: Nintendo A/B
  // arrive as SDL B/A. BACK and START are the physical minus and plus buttons.
  // Keep this gameplay mapping separate from the fixed game-selection UI.
  NESRuntime::Data::ControllerMapping joy_mapping =
      !ab_reverse
          ? NESRuntime::Data::ControllerMapping{
                SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_A,
                SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START,
                SDL_CONTROLLER_BUTTON_DPAD_UP,
                SDL_CONTROLLER_BUTTON_DPAD_DOWN,
                SDL_CONTROLLER_BUTTON_DPAD_LEFT,
                SDL_CONTROLLER_BUTTON_DPAD_RIGHT}
          : NESRuntime::Data::ControllerMapping{
                SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B,
                SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START,
                SDL_CONTROLLER_BUTTON_DPAD_UP,
                SDL_CONTROLLER_BUTTON_DPAD_DOWN,
                SDL_CONTROLLER_BUTTON_DPAD_LEFT,
                SDL_CONTROLLER_BUTTON_DPAD_RIGHT};
#else
  NESRuntime::Data::ControllerMapping joy_mapping =
      !ab_reverse ? NESRuntime::Data::
                        ControllerMapping{SDL_CONTROLLER_BUTTON_A,
                                          SDL_CONTROLLER_BUTTON_X,
                                          SDL_CONTROLLER_BUTTON_Y,
                                          SDL_CONTROLLER_BUTTON_START,
                                          SDL_CONTROLLER_BUTTON_DPAD_UP,
                                          SDL_CONTROLLER_BUTTON_DPAD_DOWN,
                                          SDL_CONTROLLER_BUTTON_DPAD_LEFT,
                                          SDL_CONTROLLER_BUTTON_DPAD_RIGHT}
                  : NESRuntime::Data::ControllerMapping{
                        SDL_CONTROLLER_BUTTON_X,
                        SDL_CONTROLLER_BUTTON_A,
                        SDL_CONTROLLER_BUTTON_Y,
                        SDL_CONTROLLER_BUTTON_START,
                        SDL_CONTROLLER_BUTTON_DPAD_UP,
                        SDL_CONTROLLER_BUTTON_DPAD_DOWN,
                        SDL_CONTROLLER_BUTTON_DPAD_LEFT,
                        SDL_CONTROLLER_BUTTON_DPAD_RIGHT};
#endif
  runtime_data->joystick_mappings[player] = {controller, joy_mapping};
}

std::vector<SDL_GameController*> GetControllerList() {
  std::vector<SDL_GameController*> result;
  std::set<SDL_GameController*> controllers =
      Application::Get()->game_controllers();

  for (auto* controller : controllers) {
    if (controller)
      result.push_back(controller);
  }

  // Application stores controllers by pointer. Use SDL instance IDs instead so
  // the Switch primary controller (slot 0) is considered before idle slots.
  std::sort(result.begin(), result.end(),
            [](SDL_GameController* lhs, SDL_GameController* rhs) {
              return SDL_JoystickInstanceID(
                         SDL_GameControllerGetJoystick(lhs)) <
                     SDL_JoystickInstanceID(
                         SDL_GameControllerGetJoystick(rhs));
            });

  // The first entry means no joystick, or doesn't use any joysticks.
  result.insert(result.begin(), nullptr);
  return result;
}
