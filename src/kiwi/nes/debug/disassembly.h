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

#ifndef NES_DEBUG_DISASSEMBLY_H_
#define NES_DEBUG_DISASSEMBLY_H_

#include "nes/nes_export.h"

#include <string>

#include "nes/opcodes.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {
class DebugPort;
struct Disassembly {
  const char* name;
  uint8_t opcode;
  AddressingMode addressing_mode;
  uint16_t operand;
  Byte operand_size;  // How many bytes does the operand take.
  char pretty_print[32];
  Address next_instruction;
  uint8_t cycle;
};

NES_EXPORT Disassembly Disassemble(DebugPort* debug_port, Address address);
}  // namespace core
}  // namespace kiwi

#endif  // NES_DEBUG_DISASSEMBLY_H_
