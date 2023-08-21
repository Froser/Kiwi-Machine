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

#ifndef NES_BUS_H_
#define NES_BUS_H_

#include "nes/types.h"

namespace kiwi {
namespace nes {
class Mapper;

class Bus {
 public:
  Bus();
  virtual ~Bus();

 public:
  virtual void SetMapper(Mapper* mapper) = 0;
  virtual Mapper* GetMapper() = 0;
  virtual Byte Read(Address address) = 0;
  virtual void Write(Address address, Byte value) = 0;
  // Get the pointer of the page. This is used to copy whole page's memory
  // during DMA.
  virtual Byte* GetPagePointer(Byte page) = 0;

  Word ReadWord(Address address);
};

}  // namespace core
}  // namespace kiwi

#endif  // NES_BUS_H_
