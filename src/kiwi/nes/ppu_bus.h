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

#ifndef NES_PPU_BUS_H_
#define NES_PPU_BUS_H_

#include <array>

#include "nes/emulator_states.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {
class Mapper;

// PPU Bus is connected to the PPU.
// See https://www.nesdev.org/wiki/PPU_memory_map for more addressing details.
class PPUBus : public EmulatorStates::SerializableState {
 public:
  enum class CurrentPatternType {
    kBackground,
    kSprite,
    kNotRendering,  // Fetching nametable not during rendering
  };

  PPUBus();
  ~PPUBus() override;

  // Bus:
  void SetMapper(Mapper* mapper);
  Mapper* GetMapper();
  Byte Read(Address address);
  void Write(Address address, Byte value);
  // Palette-only fast paths used by the per-pixel PPU rendering loop.
  Byte ReadPalette(Byte palette_address) const {
    // $3F10/$3F14/$3F18/$3F1C mirror the corresponding background entries.
    if (palette_address >= 0x10 && palette_address % 4 == 0) {
      palette_address &= 0x0f;
    }
    // Palette values are six-bit even if a game writes a larger byte.
    return palette_[palette_address & 0x1f] & 0x3f;
  }
  // HD texture metadata capture caches the converted backdrop color to avoid
  // a palette lookup for every pixel. The revision invalidates that cache.
  Byte ReadUniversalBackgroundColor() const { return palette_[0] & 0x3f; }
  uint64_t backdrop_revision() const { return backdrop_revision_; }

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

  void UpdateMirroring();

  // Providing extra information for MMC5
  void SetCurrentPatternState(CurrentPatternType pattern_type,
                              bool is_8x16_sprite,
                              int current_dot_in_scanline);

  // MMC5 will use its own x_fine and data address when rendering split region.
  Byte GetAdjustedXFine(Byte x_fine_in);
  Address GetAdjustedDataAddress(Address data_address_in);

 private:
  void SetDefaultPalettes();

 private:
  Mapper* mapper_ = nullptr;
  std::array<Address, 4> nametable_{0};
  // Cartridge VRAM to store four screen mirroring nametable. Just put it into
  // PPUBus.
  static constexpr size_t RAM_SIZE = 0x800;
  std::array<Byte, RAM_SIZE> ram_{0};

  // Palette RAM takes 32 bytes.
  // See https://www.nesdev.org/wiki/PPU_palettes for more details.
  std::array<Byte, 0x20> palette_{0};
  // Increments whenever the visible universal background color may change.
  uint64_t backdrop_revision_ = 0;

  // For MMC5
  bool is_mmc5_ = false;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_PPU_BUS_H_
