// Copyright (C) 2026 Yisi Yu
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

#include "ui/widgets/canvas.h"

namespace {

constexpr int kQuickLoadHoldDurationMs = 600;
constexpr Sint16 kTriggerChordPressThreshold = 16000;
constexpr Sint16 kTriggerChordReleaseThreshold = 8000;

}  // namespace

bool Canvas::OnControllerAxisMotionEvent(SDL_ControllerAxisEvent* event) {
  if (emulation_system_ == kiwi::emu::Emulator::Type::kGBA ||
      (event->axis != SDL_CONTROLLER_AXIS_TRIGGERLEFT &&
       event->axis != SDL_CONTROLLER_AXIS_TRIGGERRIGHT)) {
    return false;
  }

  SDL_GameController* controller =
      SDL_GameControllerFromInstanceID(event->which);
  if (!controller)
    return true;

  if (event->value >= kTriggerChordPressThreshold) {
    const bool left_trigger =
        event->axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ||
        SDL_GameControllerGetAxis(controller,
                                  SDL_CONTROLLER_AXIS_TRIGGERLEFT) >=
            kTriggerChordPressThreshold;
    const bool right_trigger =
        event->axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT ||
        SDL_GameControllerGetAxis(controller,
                                  SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >=
            kTriggerChordPressThreshold;
    if (left_trigger && right_trigger && !trigger_chord_pending_) {
      trigger_chord_controller_id_ = event->which;
      trigger_chord_pending_ = true;
      trigger_chord_triggered_ = false;
      trigger_chord_timer_.Reset();
    }
  } else if (event->value <= kTriggerChordReleaseThreshold &&
             trigger_chord_pending_ &&
             event->which == trigger_chord_controller_id_) {
    const bool open_regular_menu = !trigger_chord_triggered_;
    trigger_chord_pending_ = false;
    trigger_chord_triggered_ = false;
    trigger_chord_controller_id_ = -1;
    if (open_regular_menu)
      InvokeInGameMenu();
  }
  return true;
}

void Canvas::UpdateTriggerChord() {
  if (!trigger_chord_pending_ || trigger_chord_triggered_)
    return;

  SDL_GameController* controller =
      SDL_GameControllerFromInstanceID(trigger_chord_controller_id_);
  if (!controller) {
    trigger_chord_pending_ = false;
    trigger_chord_triggered_ = false;
    trigger_chord_controller_id_ = -1;
    return;
  }

  const bool left_trigger =
      SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) >=
      kTriggerChordPressThreshold;
  const bool right_trigger =
      SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >=
      kTriggerChordPressThreshold;
  if (!left_trigger || !right_trigger)
    return;

  if (trigger_chord_timer_.ElapsedInMilliseconds() >=
      kQuickLoadHoldDurationMs) {
    trigger_chord_triggered_ = true;
    InvokeQuickLoad();
  }
}

void Canvas::InvokeQuickLoad() {
  if (on_quick_load_trigger_)
    on_quick_load_trigger_.Run();
}
