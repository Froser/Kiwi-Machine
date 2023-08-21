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

#include "nes/opcodes.h"

#include "base/logging.h"

#define IS_EVEN(x) (!(x & 0x1))
#define IS_ODD(x) (x & 0x1)
#define HIGH(x) (x >> 4)
#define LOW(x) (0x0f & x)
#define LOW_EQUAL(x, l) !(LOW(x) ^ LOW(l))

namespace kiwi {
namespace nes {
namespace {

// 0 implies unused opcode
constexpr int g_operation_cycles[0x100] = {
    7, 6, 0, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6, 2, 5, 0, 8, 4, 4, 6, 6,
    2, 4, 2, 7, 4, 4, 7, 7, 6, 6, 0, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7, 6, 6, 0, 8, 3, 3, 5, 5,
    3, 2, 2, 2, 3, 4, 6, 6, 2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    6, 6, 0, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6, 2, 5, 0, 8, 4, 4, 6, 6,
    2, 4, 2, 7, 4, 4, 7, 7, 2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
    2, 6, 0, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5, 2, 6, 2, 6, 3, 3, 3, 3,
    2, 2, 2, 2, 4, 4, 4, 4, 2, 5, 0, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6, 2, 5, 0, 8, 4, 4, 6, 6,
    2, 4, 2, 7, 4, 4, 7, 7, 2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
};

constexpr char g_op_names[][0x100] = {
    "BRK", "ORA", "KIL", "SLO", "NOP", "ORA", "ASL", "SLO", "PHP", "ORA", "ASL",
    "ANC", "NOP", "ORA", "ASL", "SLO", "BPL", "ORA", "KIL", "SLO", "NOP", "ORA",
    "ASL", "SLO", "CLC", "ORA", "NOP", "SLO", "NOP", "ORA", "ASL", "SLO", "JSR",
    "AND", "KIL", "RLA", "BIT", "AND", "ROL", "RLA", "PLP", "AND", "ROL", "ANC",
    "BIT", "AND", "ROL", "RLA", "BMI", "AND", "KIL", "RLA", "NOP", "AND", "ROL",
    "RLA", "SEC", "AND", "NOP", "RLA", "NOP", "AND", "ROL", "RLA", "RTI", "EOR",
    "KIL", "SRE", "NOP", "EOR", "LSR", "SRE", "PHA", "EOR", "LSR", "ALR", "JMP",
    "EOR", "LSR", "SRE", "BVC", "EOR", "KIL", "SRE", "NOP", "EOR", "LSR", "SRE",
    "CLI", "EOR", "NOP", "SRE", "NOP", "EOR", "LSR", "SRE", "RTS", "ADC", "KIL",
    "RRA", "NOP", "ADC", "ROR", "RRA", "PLA", "ADC", "ROR", "ARR", "JMP", "ADC",
    "ROR", "RRA", "BVS", "ADC", "KIL", "RRA", "NOP", "ADC", "ROR", "RRA", "SEI",
    "ADC", "NOP", "RRA", "NOP", "ADC", "ROR", "RRA", "NOP", "STA", "NOP", "SAX",
    "STY", "STA", "STX", "SAX", "DEY", "NOP", "TXA", "XAA", "STY", "STA", "STX",
    "SAX", "BCC", "STA", "KIL", "AHX", "STY", "STA", "STX", "SAX", "TYA", "STA",
    "TXS", "TAS", "SHY", "STA", "SHX", "AHX", "LDY", "LDA", "LDX", "LAX", "LDY",
    "LDA", "LDX", "LAX", "TAY", "LDA", "TAX", "LAX", "LDY", "LDA", "LDX", "LAX",
    "BCS", "LDA", "KIL", "LAX", "LDY", "LDA", "LDX", "LAX", "CLV", "LDA", "TSX",
    "LAS", "LDY", "LDA", "LDX", "LAX", "CPY", "CMP", "NOP", "DCP", "CPY", "CMP",
    "DEC", "DCP", "INY", "CMP", "DEX", "AXS", "CPY", "CMP", "DEC", "DCP", "BNE",
    "CMP", "KIL", "DCP", "NOP", "CMP", "DEC", "DCP", "CLD", "CMP", "NOP", "DCP",
    "NOP", "CMP", "DEC", "DCP", "CPX", "SBC", "NOP", "ISC", "CPX", "SBC", "INC",
    "ISC", "INX", "SBC", "NOP", "SBC", "CPX", "SBC", "INC", "ISC", "BEQ", "SBC",
    "KIL", "ISC", "NOP", "SBC", "INC", "ISC", "SED", "SBC", "NOP", "ISC", "NOP",
    "SBC", "INC", "ISC",
};

constexpr uint8_t g_add_cycle_opcodes[0x100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,
};
}  // namespace

int GetOpcodeCycle(uint8_t opcode) {
  return g_operation_cycles[static_cast<int>(opcode)];
}

AddressingMode GetOpcodeAddressingMode(uint8_t opcode) {
  // https://www.nesdev.org/wiki/CPU_unofficial_opcodes
  switch (static_cast<Opcode>(opcode)) {
    case Opcode::BRK:
    case Opcode::RTI:
    case Opcode::RTS:
      return AddressingMode::kNONE;
    case Opcode::JSR:
      return AddressingMode::kABS;
    default:
      break;
  }

  switch (opcode) {
    case 0x96:
    case 0x97:
    case 0xb6:
    case 0xb7:
      return AddressingMode::kZPY;
    case 0x9e:
    case 0x9f:
    case 0xbe:
    case 0xbf:
      return AddressingMode::kABY;
    case 0x80:
    case 0xa0:
    case 0xc0:
    case 0xe0:
    case 0x82:
    case 0xa2:
    case 0xc2:
    case 0xe2:
      return AddressingMode::kIMM;
    case 0x6c:
      return AddressingMode::kIND;
    default:
      break;
  }

  if (LOW_EQUAL(opcode, 0x08) || (LOW_EQUAL(opcode, 0x0a))) {
    // 0x08, 0x18, ..., 0xf8
    // 0x0a, 0x1a, ..., 0xfa
    return AddressingMode::kNONE;
  } else if (LOW_EQUAL(opcode, 0x02) && (opcode != 0x82 && opcode != 0xa2 &&
                                         opcode != 0xc2 && opcode != 0xe2)) {
    // KIL
    return AddressingMode::kNONE;
  }

  if (IS_EVEN(HIGH(opcode)) && LOW_EQUAL(opcode, 0x09)) {
    // 0x09, 0x29, 0x49, ..., 0xe9
    return AddressingMode::kIMM;
  } else if (IS_EVEN(HIGH(opcode)) && LOW_EQUAL(opcode, 0x0b)) {
    // 0x0b, 0x2b, 0x4b, ..., 0xeb
    return AddressingMode::kIMM;
  }

  if (IS_ODD(HIGH(opcode)) && !(LOW(opcode))) {
    // 0x10, 0x30, ..., 0xe0
    return AddressingMode::kREL;
  }

  if (IS_EVEN(HIGH(opcode)) && LOW_EQUAL(opcode, 0x0c) && opcode != 0x6c) {
    // 0x0c, 0x2c, 0x4c, 0x8c, 0xac, 0xcc, 0xec (except 0x6c)
    return AddressingMode::kABS;
  } else if (IS_EVEN(HIGH(opcode)) && LOW_EQUAL(opcode, 0x0d)) {
    // 0x0d, 0x2d, 0x4d, 0x6d, 0x8d, 0xad, 0xcd, 0xed
    return AddressingMode::kABS;
  } else if (IS_EVEN(HIGH(opcode)) && LOW_EQUAL(opcode, 0x0e)) {
    // 0x0e, 0x2e, 0x4e, 0x6e, 0x8e, 0xae, 0xce, 0xee
    return AddressingMode::kABS;
  } else if (IS_EVEN(HIGH(opcode)) && LOW_EQUAL(opcode, 0x0f)) {
    // 0x0f, 0x2f, 0x4f, 0x6f, 0x8f, 0xaf, 0xcf, 0xef
    return AddressingMode::kABS;
  }

  if (IS_ODD(HIGH(opcode)) && LOW_EQUAL(opcode, 0x0c)) {
    // 0x1c, 0x3c, 0x5c, ..., 0xfc
    return AddressingMode::kABX;
  } else if (IS_ODD(HIGH(opcode)) && LOW_EQUAL(opcode, 0x0d)) {
    // 0x1d, 0x3d, 0x5d, ..., 0xfd
    return AddressingMode::kABX;
  } else if (IS_ODD(HIGH(opcode)) && LOW_EQUAL(opcode, 0x0e) &&
             (opcode != 0x9e) && (opcode != 0xbe)) {
    // 0x1e, 0x3e, 0x5e, ..., 0xfe (except 0x9e, 0xbe)
    return AddressingMode::kABX;
  } else if (IS_ODD(HIGH(opcode)) && LOW_EQUAL(opcode, 0x0f) &&
             (opcode != 0x9f) && (opcode != 0xbf)) {
    // 0x1f, 0x3f, 0x5f, ..., 0xff (except 0x9f, 0xbf)
    return AddressingMode::kABX;
  }

  if (IS_ODD(HIGH(opcode)) && LOW_EQUAL(opcode, 0x09)) {
    // 0x19, 0x39, 0x59, ..., 0xf9
    return AddressingMode::kABY;
  } else if (IS_ODD(HIGH(opcode)) && LOW_EQUAL(opcode, 0x0b)) {
    // 0x1b, 0x3b, 0x5b, ..., 0xfb
    return AddressingMode::kABY;
  }

  if (IS_EVEN(HIGH(opcode)) && LOW_EQUAL(opcode, 0x01)) {
    // 0x01, 0x21, 0x41, ..., 0xe1
    return AddressingMode::kIZX;
  } else if (IS_EVEN(HIGH(opcode)) && LOW_EQUAL(opcode, 0x03)) {
    // 0x03, 0x23, 0x43, ..., 0xe3
    return AddressingMode::kIZX;
  }

  if (IS_ODD(HIGH(opcode)) && LOW_EQUAL(opcode, 0x01)) {
    // 0x11, 0x31, 0x51, ..., 0xf1
    return AddressingMode::kIZY;
  } else if (IS_ODD(HIGH(opcode)) && LOW_EQUAL(opcode, 0x03)) {
    // 0x13, 0x33, 0x53, ..., 0xf3
    return AddressingMode::kIZY;
  }

  if (IS_EVEN(HIGH(opcode)) && LOW_EQUAL(opcode, 0x04)) {
    // 0x04, 0x24, 0x44, ..., 0xe4
    return AddressingMode::kZP;
  } else if (IS_EVEN(HIGH(opcode)) && LOW_EQUAL(opcode, 0x05)) {
    // 0x05, 0x25, 0x45, ..., 0xe5
    return AddressingMode::kZP;
  } else if (IS_EVEN(HIGH(opcode)) && LOW_EQUAL(opcode, 0x06)) {
    // 0x06, 0x26, 0x46, ..., 0xe6
    return AddressingMode::kZP;
  } else if (IS_EVEN(HIGH(opcode)) && LOW_EQUAL(opcode, 0x07)) {
    // 0x07, 0x27, 0x47, ..., 0xe7
    return AddressingMode::kZP;
  }

  if (IS_ODD(HIGH(opcode)) && LOW_EQUAL(opcode, 0x04)) {
    // 0x14, 0x34, 0x54, ..., 0xf4
    return AddressingMode::kZPX;
  } else if (IS_ODD(HIGH(opcode)) && LOW_EQUAL(opcode, 0x05)) {
    // 0x15, 0x35, 0x55, ..., 0xf5
    return AddressingMode::kZPX;
  } else if (IS_ODD(HIGH(opcode)) && LOW_EQUAL(opcode, 0x06) &&
             opcode != 0x96 && opcode != 0xb6) {
    // 0x16, 0x36, 0x56, ..., 0xf6 (except 0x96, 0xb6)
    return AddressingMode::kZPX;
  } else if (IS_ODD(HIGH(opcode)) && LOW_EQUAL(opcode, 0x07) &&
             opcode != 0x97 && opcode != 0xb7) {
    // 0x17, 0x37, 0x57, ..., 0xf7 (except 0x97, 0xb7)
    return AddressingMode::kZPX;
  }

  LOG(ERROR) << "Unrecognize opcode: " << opcode;
  return AddressingMode::kNONE;
}

const char* GetOpcodeName(uint8_t opcode) {
  return g_op_names[opcode];
}

bool IsNeedAddOneCycleWhenCrossingPage(uint8_t opcode) {
  return !!g_add_cycle_opcodes[opcode];
}
}  // namespace core
}  // namespace kiwi
