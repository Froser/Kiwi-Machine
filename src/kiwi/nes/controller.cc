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

#include "nes/controller.h"

#include "base/check.h"
#include "nes/emulator.h"

#define NEXT_BUTTON(button) (button = (ControllerButton)((int)button + 1))

namespace kiwi {
namespace nes {

Controller::Controller(int id) : id_(id) {}
Controller::~Controller() = default;

// https://www.nesdev.org/wiki/Controller_reading_code describes the layout of
// the button.
void Controller::Strobe(Byte b) {
  strobe_ = (b & 1);
  if (!strobe_) {
    key_states_ = 0;
    int shift = 0;
    for (ControllerButton button = ControllerButton::kA;
         button < ControllerButton::kMax; NEXT_BUTTON(button)) {
      key_states_ |= (IsKeyPressed(button)) << shift;
      ++shift;
    }
  }
}

// While S (strobe) is high, the shift registers in the controllers are
// continuously reloaded from the button states, and reading $4016/$4017 will
// keep returning the current state of the first button (A). Once S goes low,
// this reloading will stop. Hence a 1/0 write sequence is required to get the
// button states, after which the buttons can be read back one at a time.
// See https://www.nesdev.org/wiki/Standard_controller,
// https://www.nesdev.org/wiki/Controller_reading_code for more details.
Byte Controller::Read() {
  Byte ret;
  if (strobe_)
    ret = IsKeyPressed(ControllerButton::kA);
  else {
    ret = (key_states_ & 1);
    key_states_ >>= 1;
  }

  return ret | 0x40;
}

// Returns 0 or 1.
int Controller::IsKeyPressed(ControllerButton button) {
  DCHECK(emulator_);
  if (emulator_->GetIODevices() && emulator_->GetIODevices()->input_device())
    return emulator_->GetIODevices()->input_device()->IsKeyDown(id_, button)
               ? 1
               : 0;

  return 0;
}
}  // namespace nes
}  // namespace kiwi
