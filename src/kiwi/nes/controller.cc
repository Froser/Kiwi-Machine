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

namespace {
class StandardController : public Controller::Implementation {
 public:
  StandardController(Emulator* emulator, int id)
      : Controller::Implementation(emulator, id) {}
  ~StandardController() override = default;

 private:
  Byte Read() override;
  void Strobe(Byte b) override;

 private:
  int IsKeyPressed(ControllerButton button);

 private:
  bool strobe_ = false;
  unsigned int key_states_ = 0;
};

class ZapperController : public Controller::Implementation {
 public:
  ZapperController(Emulator* emulator, int id)
      : Controller::Implementation(emulator, id) {}
  ~ZapperController() override = default;

 private:
  Byte Read() override;
  void Strobe(Byte b) override;
};

void ZapperController::Strobe(Byte b) {}

Byte ZapperController::Read() {
  DCHECK(emulator());
  if (emulator()->GetIODevices()) {
    IODevices::InputDevice* input_device =
        emulator()->GetIODevices()->input_device();
    int zapper_state = input_device->GetZapperState();
    // 7  bit  0
    // ---- ----
    // xxxT WxxS
    //    | |  |
    //    | |  +- Serial data (Vs.)
    //    | +---- Light sensed at the current scanline (0: detected; 1: not
    //    detected) (NES/FC)
    //    +------ Trigger (0: released or fully pulled; 1: half-pulled) (NES/FC)
    Byte ret = 0x8;
    if (zapper_state & IODevices::InputDevice::ZapperState::kTriggered)
      ret |= 0x10;
    if (zapper_state & IODevices::InputDevice::ZapperState::kLightSensed)
      ret &= 0xf7;
    return ret;
  }
  return 0;
}

// While S (strobe) is high, the shift registers in the controllers are
// continuously reloaded from the button states, and reading $4016/$4017 will
// keep returning the current state of the first button (A). Once S goes low,
// this reloading will stop. Hence a 1/0 write sequence is required to get the
// button states, after which the buttons can be read back one at a time.
// See https://www.nesdev.org/wiki/Standard_controller,
// https://www.nesdev.org/wiki/Controller_reading_code for more details.
Byte StandardController::Read() {
  Byte ret;
  if (strobe_) {
    ret = IsKeyPressed(ControllerButton::kA);
  } else {
    ret = (key_states_ & 1);
    key_states_ >>= 1;
  }

  return ret | 0x40;
}

// https://www.nesdev.org/wiki/Controller_reading_code describes the layout of
// the button.
void StandardController::Strobe(Byte b) {
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

// Returns 0 or 1.
int StandardController::IsKeyPressed(ControllerButton button) {
  DCHECK(emulator());
  if (emulator()->GetIODevices()) {
    IODevices::InputDevice* input_device =
        emulator()->GetIODevices()->input_device();

    if (input_device) {
      // Press Up/Down or Left/Right simultaneously is not allowed. It will
      // cause some bugs, for example: Zelda II - The Adventure of Link.
      if (button == ControllerButton::kLeft &&
          input_device->IsKeyDown(id(), ControllerButton::kRight))
        return 0;

      if (button == ControllerButton::kRight &&
          input_device->IsKeyDown(id(), ControllerButton::kLeft))
        return 0;

      if (button == ControllerButton::kUp &&
          input_device->IsKeyDown(id(), ControllerButton::kDown))
        return 0;

      if (button == ControllerButton::kDown &&
          input_device->IsKeyDown(id(), ControllerButton::kUp))
        return 0;

      return input_device->IsKeyDown(id(), button) ? 1 : 0;
    }
  }

  return 0;
}

}  // namespace

Controller::Controller(int id) : id_(id) {}
Controller::~Controller() = default;

void Controller::SetType(Emulator* emulator, Type type) {
  type_ = type;
  if (type == Type::kStandard)
    impl_ = std::make_unique<StandardController>(emulator, id_);
  else
    impl_ = std::make_unique<ZapperController>(emulator, id_);
}

void Controller::Strobe(Byte b) {
  impl_->Strobe(b);
}

Byte Controller::Read() {
  return impl_->Read();
}

Controller::Implementation::Implementation(Emulator* emulator, int id)
    : emulator_(emulator), id_(id) {}
Controller::Implementation::~Implementation() = default;

}  // namespace nes
}  // namespace kiwi
