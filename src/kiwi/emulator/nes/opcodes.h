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

#ifndef NES_OPCODES_H_
#define NES_OPCODES_H_

#include <stdint.h>
#include <string>

namespace kiwi {
namespace nes {

// Opcode defines instructions for CPU. See
// https://www.nesdev.org/wiki/CPU_unofficial_opcodes for more details.
// Some instructions have more than one opcode, such as BIT, LDA, STA, etc..
// Those are not in following list.

enum class Opcode : uint8_t {
  BRK = 0x00,
  JSR = 0x20,
  RTI = 0x40,
  RTS = 0x60,

  BPL = 0x10,
  BMI = 0x30,
  BVC = 0x50,
  BVS = 0x70,
  BCC = 0x90,
  BCS = 0xb0,
  BNE = 0xd0,
  BEQ = 0xf0,

  JMP = 0x4c,
  JMPI = 0x6c,  // JMP Indirect

  PHP = 0x08,
  PLP = 0x28,
  PHA = 0x48,
  PLA = 0x68,

  DEY = 0x88,
  DEX = 0xca,

  SHY = 0x9c,
  SHX = 0x9e,

  NOP_UNOFFICIAL_0 = 0x1a,
  NOP_UNOFFICIAL_1 = 0x3a,
  NOP_UNOFFICIAL_2 = 0x5a,
  NOP_UNOFFICIAL_3 = 0x7a,
  NOP_UNOFFICIAL_4 = 0xda,
  NOP_UNOFFICIAL_5 = 0xfa,

  // NOP with fetching byte (#imm)
  NOP_TYPE0_0 = 0x80,
  NOP_TYPE0_1 = 0x82,
  NOP_TYPE0_2 = 0x89,
  NOP_TYPE0_3 = 0xc2,
  NOP_TYPE0_4 = 0xe2,
  NOP_TYPE0_5 = 0x04,
  NOP_TYPE0_6 = 0x44,
  NOP_TYPE0_7 = 0x64,
  NOP_TYPE0_8 = 0x14,
  NOP_TYPE0_9 = 0x34,
  NOP_TYPE0_10 = 0x54,
  NOP_TYPE0_11 = 0x74,
  NOP_TYPE0_12 = 0xd4,
  NOP_TYPE0_13 = 0xf4,

  // NOP with fetching address
  NOP_TYPE1_0 = 0x0c,
  NOP_TYPE1_1 = 0x1c,
  NOP_TYPE1_2 = 0x3c,
  NOP_TYPE1_3 = 0x5c,
  NOP_TYPE1_4 = 0x7c,
  NOP_TYPE1_5 = 0xdc,
  NOP_TYPE1_6 = 0xfc,

  // NOP only
  NOP_TYPE2_0 = 0xea,

  TAY = 0xa8,
  INY = 0xc8,
  INX = 0xe8,

  CLC = 0x18,
  SEC = 0x38,
  CLI = 0x58,
  SEI = 0x78,
  TYA = 0x98,
  CLV = 0xb8,
  CLD = 0xd8,
  SED = 0xf8,

  TXA = 0x8a,
  TXS = 0x9a,
  TAX = 0xaa,
  TSX = 0xba,

  LAS = 0xbb,
  ALR = 0x4b,
  ARR = 0x6b,
  AXS = 0xcb,

  ANC_0 = 0x0b,
  ANC_1 = 0x2b,
  SBC_UNOFFICAL = 0xeb,
};

// Naming is following http://www.oxyron.de/html/opcodes02.html
enum class AddressingMode {
  kNONE,  // No addressing modes, such as BRK
  kIMM,   // Immediate
  kZP,    // Zero page
  kZPX,   // Zero page x indexed
  kZPY,   // Zero page y indexed
  kABS,   // Absolute
  kABX,   // Absolute x indexed
  kABY,   // Absolute y indexed
  kIZX,   // Indexed indirected
  kIZY,   // Indirect indexed
  kIND,   // Indirect
  kREL,   // Relative
};

// Get operation cycle cost for |code|.
// All cycles are listed in http://www.oxyron.de/html/opcodes02.html
int GetOpcodeCycle(uint8_t opcode);
AddressingMode GetOpcodeAddressingMode(uint8_t opcode);
const char* GetOpcodeName(uint8_t opcode);
bool IsNeedAddOneCycleWhenCrossingPage(uint8_t opcode);

}  // namespace core
}  // namespace kiwi

#endif  // NES_OPCODES_H_
