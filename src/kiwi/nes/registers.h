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

#ifndef NES_REGISTERS_H_
#define NES_REGISTERS_H_

#include "nes/types.h"

#include <cstdint>

namespace kiwi {
namespace nes {

// See https://www.nesdev.org/wiki/PPU_registers for more details.
enum class PPURegister : Register {
  PPUCTRL = 0x2000,
  PPUMASK,
  PPUSTATUS,
  OAMADDR,
  OAMDATA,
  PPUSCROL,
  PPUADDR,
  PPUDATA,
};

enum class APURegister : Register {
  PULSE1_1 = 0x4000,
  PULSE1_2 ,
  PULSE1_3 ,
  PULSE1_4 ,
  PULSE2_1 ,
  PULSE2_2 ,
  PULSE2_3 ,
  PULSE2_4 ,
  TRIANGLE_1,
  TRIANGLE_2,
  TRIANGLE_3,
  TRIANGLE_4,
  NOISE_1,
  NOISE_2,
  NOISE_3,
  NOISE_4,
  DMC_1,
  DMC_2,
  DMC_3,
  DMC_4,
  STATUS = 0x4015,
  FRAME_COUNTER = 0x4017,
};
static_assert(static_cast<int>(APURegister::DMC_4) == 0x4013);

// The 2A03, short for RP2A03[G], is the common name of the NTSC NES CPU chip.
// See https://www.nesdev.org/wiki/2A03 for more details.
enum class IORegister : Register {
  OAMDMA = 0x4014,
  JOY1 = 0x4016,
  JOY2 = 0x4017,
};

// The registers on the NES CPU are just like on the 6502.
// See https://www.nesdev.org/wiki/CPU_registers for more details.
struct alignas(8) CPURegisters {
  Byte A;      // Accumulator
  Byte X;      // Indexes X
  Byte Y;      // Indexes Y
  Address PC;  // Program Counter
  Byte S;      // Stack Pointer

  union alignas(8) State_t {
    Byte value;
    struct {
      Bit C : 1;  // Carry
      Bit Z : 1;  // Zero
      Bit I : 1;  // Interrupt disable
      Bit D : 1;  // Decimal
      Bit B : 2;  // Break flag
      Bit V : 1;  // Overflow
      Bit N : 1;  // Negative
    };
  } P;  // Status Register. See https://www.nesdev.org/wiki/Status_flags
        // for more details.
};
static_assert(sizeof(CPURegisters::State_t) == 8);

struct alignas(8) PPURegisters {
  union alignas(8) PPUCTRL_t {
    Byte value;
    struct {
      Bit N : 2;  // Base nametable address
      Bit I : 1;  // VRAM address increment per CPU R/W of PPUDATA
      Bit S : 1;  // Sprite pattern table address for 8x8 sprites
      Bit B : 1;  // Background pattern table address
      Bit H : 1;  // Sprite size (0: 8x8 pixels; 1: 8x16 pixels)
      Bit P : 1;  // PPU master/slave select
      Bit V : 1;  // Generate an NMI at the start of the vblank
    };
  } PPUCTRL;

  union alignas(8) PPUMASK_t {
    Byte value;
    struct {
      Bit g : 1;  // Grayscale (0: normal color, 1: produce a grayscale display)
      Bit m : 1;  // 1: Show background in leftmost 8 pixels of screen, 0: Hide
      Bit M : 1;  // 1: Show sprites in leftmost 8 pixels of screen, 0: Hide
      Bit b : 1;  // 1: Show background
      Bit s : 1;  // 1: Show sprites
      Bit R : 1;  // Emphasize red (green on PAL/Dendy)
      Bit G : 1;  // Emphasize green (red on PAL/Dendy)
      Bit B : 1;  // Emphasize blue
    };
  } PPUMASK;

  union alignas(8) PPUSTATUS_t {
    Byte value;
    struct {
      Bit openbus : 5;
      Bit O : 1;  // Sprite overflow
      Bit S : 1;  // Sprite zero hit
      Bit V : 1;  // Vertical blank has started
    };
  } PPUSTATUS;

  Byte OAMADDR;
  Byte OAMDATA;
  Byte PPUSCROLL;
  Byte PPUADDR;
  Byte PPUDATA;
  Byte OAMDMA;
};
static_assert(sizeof(PPURegisters::PPUCTRL_t) == 8);
static_assert(sizeof(PPURegisters::PPUMASK_t) == 8);
static_assert(sizeof(PPURegisters::PPUSTATUS_t) == 8);

}  // namespace core
}  // namespace kiwi

#endif  // NES_REGISTERS_H_
