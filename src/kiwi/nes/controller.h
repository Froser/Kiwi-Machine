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

#ifndef NES_CONTROLLER_H_
#define NES_CONTROLLER_H_

#include "nes/types.h"

namespace kiwi {
namespace nes {
class Emulator;
class Controller {
 public:
  Controller(int id);
  ~Controller();

  void set_emulator(Emulator* emulator) { emulator_ = emulator; }

  void Strobe(Byte b);
  Byte Read();

 private:
  int IsKeyPressed(ControllerButton button);

 private:
  int id_ = 0;
  bool strobe_ = false;
  unsigned int key_states_ = 0;
  Emulator* emulator_ = nullptr;
};
}  // namespace nes
}  // namespace kiwi

#endif  // NES_CONTROLLER_H_
