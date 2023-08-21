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

#ifndef MODELS_NES_RUNTIME_H_
#define MODELS_NES_RUNTIME_H_

#include <kiwi_nes.h>

#include "debug/debug_port.h"

using NESRuntimeID = size_t;
class NESRuntime {
 public:
  struct Data {
    union ControllerMapping {
      int mapping[8];
      struct {
        int A;
        int B;
        int Select;
        int Start;
        int Up;
        int Down;
        int Left;
        int Right;
      };

      NESRuntime::Data::ControllerMapping& operator=(
          const NESRuntime::Data::ControllerMapping& rhs) {
        memcpy(this, &rhs, sizeof(rhs));
        return *this;
      }
    };

    struct JoystickMapping {
      void* which = nullptr;
      ControllerMapping mapping;
    };

    ControllerMapping keyboard_mappings[2] = {0};
    JoystickMapping joystick_mappings[2] = {nullptr, {0}};
    scoped_refptr<kiwi::nes::Emulator> emulator;
    std::unique_ptr<DebugPort> debug_port;
    kiwi::nes::Bytes saved_state;
    kiwi::nes::IODevices::RenderDevice::Buffer saved_state_thumbnail;
  };

 private:
  NESRuntime();
  ~NESRuntime();

 public:
  Data* GetDataById(NESRuntimeID id);
  NESRuntimeID CreateData();

 public:
  static NESRuntime* GetInstance();
};

#endif  // MODELS_NES_RUNTIME_H_