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

#include "nes/debug/disassembly.h"

#include <sstream>

#include "base/logging.h"
#include "nes/debug/debug_port.h"

namespace kiwi {
namespace nes {

#define ADDR(high, low) (Address)((high) << 8 | (low))

Disassembly Disassemble(DebugPort* debug_port, Address address) {
  Byte opcode = debug_port->CPUReadByte(address);
  Byte next0 = 0, next1 = 0;
  // Fill |next0| and |next1| if not overflow.
  if (address + 1 > address) {
    next0 = debug_port->CPUReadByte(address + 1);
  }
  if (address + 2 > address) {
    next1 = debug_port->CPUReadByte(address + 2);
  }

  Disassembly disassembly;
  disassembly.operand_size = 0;
  disassembly.name = GetOpcodeName(opcode);
  disassembly.addressing_mode = GetOpcodeAddressingMode(opcode);
  disassembly.opcode = opcode;
  disassembly.cycle = GetOpcodeCycle(opcode);

  std::stringstream ss;
  switch (disassembly.addressing_mode) {
    case AddressingMode::kNONE:
      ss << disassembly.name;
      break;
    case AddressingMode::kIMM:
      ss << disassembly.name << " #$" << Hex<8>{next0};
      disassembly.operand = next0;
      disassembly.operand_size = 1;
      break;
    case AddressingMode::kZP:
      ss << disassembly.name << " $" << Hex<8>{next0};
      disassembly.operand = next0;
      disassembly.operand_size = 1;
      break;
    case AddressingMode::kZPX:
      ss << disassembly.name << " $" << Hex<8>{next0} << ",X";
      disassembly.operand = next0;
      disassembly.operand_size = 1;
      break;
    case AddressingMode::kZPY:
      ss << disassembly.name << " $" << Hex<8>{next0} << ",Y";
      disassembly.operand = next0;
      disassembly.operand_size = 1;
      break;
    case AddressingMode::kIZX:
      ss << disassembly.name << " ($" << Hex<8>{next0} << ",X)";
      disassembly.operand = next0;
      disassembly.operand_size = 1;
      break;
    case AddressingMode::kIZY:
      ss << disassembly.name << " ($" << Hex<8>{next0} << "),Y";
      disassembly.operand = next0;
      disassembly.operand_size = 1;
      break;
    case AddressingMode::kABS:
      ss << disassembly.name << " $" << Hex<16>{ADDR(next1, next0)};
      disassembly.operand = ADDR(next1, next0);
      disassembly.operand_size = 2;
      break;
    case AddressingMode::kABX:
      ss << disassembly.name << " $" << Hex<16>{ADDR(next1, next0)} << ",X";
      disassembly.operand = ADDR(next1, next0);
      disassembly.operand_size = 2;
      break;
    case AddressingMode::kABY:
      ss << disassembly.name << " $" << Hex<16>{ADDR(next1, next0)} << ",Y";
      disassembly.operand = ADDR(next1, next0);
      disassembly.operand_size = 2;
      break;
    case AddressingMode::kIND:
      ss << disassembly.name << " $(" << Hex<16>{ADDR(next1, next0)} << ")";
      disassembly.operand = ADDR(next1, next0);
      disassembly.operand_size = 2;
      break;
    case AddressingMode::kREL:
      ss << disassembly.name << " $" << Hex<8>{next0} << " (PC-relative)";
      disassembly.operand = next0;
      disassembly.operand_size = 1;
      break;
    default:
      CHECK(false);
      break;
  }

  disassembly.next_instruction =
      address + disassembly.operand_size + 1;  // Instruction takes one byte.
  strcpy(disassembly.pretty_print, ss.str().c_str());
  return disassembly;
}

}  // namespace core
}  // namespace kiwi
