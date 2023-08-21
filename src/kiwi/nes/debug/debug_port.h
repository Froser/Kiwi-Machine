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

#ifndef NES_DEBUG_DEBUG_PORT_H_
#define NES_DEBUG_DEBUG_PORT_H_

#include <cstdint>
#include <vector>

#include "base/task/single_thread_task_runner.h"
#include "nes/cpu.h"
#include "nes/io_devices.h"
#include "nes/nes_export.h"
#include "nes/registers.h"

namespace kiwi {
namespace nes {
class RomData;
class Palette;
class Emulator;

struct NES_EXPORT CPUDebugState {
  bool should_break = false;
};

enum AudioChannel {
  kNoChannel = 0,
  kSquare_1 = 1 << 0,
  kSquare_2 = 1 << 1,
  kTriangle = 1 << 2,
  kNoise = 1 << 3,
  kDMC = 1 << 4,
  kAll = kSquare_1 | kSquare_2 | kTriangle | kNoise | kDMC,
};

struct PPUContext {
  PPURegisters registers;
  Address data_address;
  bool is_data_address_writing;
  Address sprite_data_address;
  Palette* palette;
  int scanline;
  int pixel;
};

struct CPUContext {
  CPURegisters registers;
  CPU::LastAction last_action;
};

union Attribute {
  Byte value;
  struct alignas(8) {
    Bit TL : 2;
    Bit TR : 2;
    Bit BL : 2;
    Bit BR : 2;
  };
};
static_assert(sizeof(Attribute) == 8);
using Attributes = std::vector<Attribute>;

enum class PaletteName {
  kIndexOnly = -1,  // Just return the frame palette index, not real color.
  kBackgroundPalette0,
  kBackgroundPalette1,
  kBackgroundPalette2,
  kBackgroundPalette3,
  kSpritePalette0,
  kSpritePalette1,
  kSpritePalette2,
  kSpritePalette3,
};

struct NES_EXPORT Sprite {
  Sprite();
  ~Sprite();
  Sprite(const Sprite& sprite);
  Sprite(Sprite&& sprite);

  Colors bgra;
  Point position;
  bool is_8x8;
};

class NES_EXPORT DebugPort {
 public:
  DebugPort(Emulator* emulator);
  virtual ~DebugPort();

 public:
  virtual void OnRomLoaded(bool success, const RomData* rom_data) {}
  virtual void OnCPUPowerOn(const CPUContext& cpu_context) {}
  virtual void OnCPUReset(const CPUContext& cpu_context) {}
  virtual void OnPPUPowerOn(const PPUContext& ppu_context) {}
  virtual void OnPPUReset(const PPUContext& ppu_context) {}
  virtual void OnCPUStepped(const CPUContext& cpu_context) {}
  virtual void OnPPUStepped(const PPUContext& ppu_context) {}
  virtual void OnCPUNMI() {}
  virtual void OnCPUBeforeStep(CPUDebugState& state) {}
  virtual void OnPPUADDR(Address address) {}
  virtual void OnScanlineStart(int scanline) {}
  virtual void OnScanlineEnd(int scanline) {}
  virtual void OnFrameStart() {}
  virtual void OnFrameEnd() {}
  virtual void OnEmulatorStepped(const CPUContext& cpu_context,
                                 const PPUContext& ppu_context) {}
  virtual void OnNametableRenderReady();

  // Accesses to CPU/PPU
 public:
  Emulator* emulator() { return emulator_; }
  PPUContext GetPPUContext();
  CPUContext GetCPUContext();
  Byte CPUReadByte(Address address, bool* can_read = nullptr);
  Byte PPUReadByte(Address address, bool* can_read = nullptr);
  Byte OAMReadByte(Address address, bool* can_read = nullptr);
  void SetNametableRenderer(IODevices::RenderDevice* render_device);

  // Returns the pattern table of the ROM, in RGBA.
  // The pattern table is divided into two 256-tile sections: $0000-$0FFF,
  // nicknamed "left", and $1000-$1FFF, nicknamed "right". The nicknames come
  // from how emulators with a debugger display the pattern table.
  // Traditionally, they are displayed as two side-by-side 128x128 pixel
  // sections, each representing 16x16 tiles from the pattern table, with
  // $0000-$0FFF on the left and $1000-$1FFF on the right.
  Colors GetPatternTableBGRA(PaletteName palette_name);
  void GetPaletteIndices(PaletteName palette_name, Byte (&indices)[4]);

  // Gets nametable of the current PPU.
  Colors GetNametableBGRA();

  // Gets sprite information specified by |index|, which is from 0 to 63.
  Sprite GetSpriteInfo(Byte index);

  // Gets current frame
  Colors GetCurrentFrame();

  // Set audio channels.
  void SetAudioChannelMasks(int audio_channels);
  int GetAudioChannelMasks();

 private:
  Attributes GetNametableAttributes(Address nametable_start);

  // Copy 8x8 tile as BGRA format, from |source_indices| to |destination|.
  void CopyTileBGRA(const Colors& source_indices,
                    Colors* destination,
                    PaletteName palette_name,
                    int source_width,
                    int dest_width,
                    int source_x,
                    int source_y,
                    int dest_x,
                    int dest_y);

 private:
  IODevices::RenderDevice* nametable_render_device_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  Emulator* emulator_ = nullptr;
};
}  // namespace nes
}  // namespace kiwi

#endif  // NES_DEBUG_DEBUG_PORT_H_
