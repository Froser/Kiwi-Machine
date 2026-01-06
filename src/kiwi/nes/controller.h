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

#include <memory>

namespace kiwi {
namespace nes {
class Emulator;
class Controller {
 public:
  enum class Type {
    kStandard,
    kZapper,  // lightgun

    kLast = kZapper,
  };

  class Implementation {
   public:
    Implementation(Emulator* emulator, int id);
    virtual ~Implementation() = 0;

   public:
    virtual void Strobe(Byte b) = 0;
    virtual Byte Read() = 0;

   protected:
    Emulator* emulator() { return emulator_; }
    int id() { return id_; }

   private:
    Emulator* emulator_;
    int id_;
  };

 public:
  Controller(int id);
  ~Controller();

  void SetType(Emulator* emulator, Type type);
  Type type() { return type_; }
  void Strobe(Byte b);
  Byte Read();

 private:
  int id_;
  Type type_ = Type::kStandard;
  std::unique_ptr<Implementation> impl_;
};
}  // namespace nes
}  // namespace kiwi

#endif  // NES_CONTROLLER_H_
