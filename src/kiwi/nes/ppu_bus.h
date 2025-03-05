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

// PPU Bus is connected to PPU.
// See https://www.nesdev.org/wiki/PPU_memory_map for more addressing details.
class PPUBus : public EmulatorStates::SerializableState {
 public:
  enum class CurrentPatternType {
    kBackground,
    kSprite,
  };

  PPUBus();
  ~PPUBus() override;

  // Bus:
  void SetMapper(Mapper* mapper);
  Mapper* GetMapper();
  Byte Read(Address address);
  void Write(Address address, Byte value);

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

  void UpdateMirroring();

  // Providing extra information for MMC5
  void set_current_pattern_state(CurrentPatternType pattern_type,
                                 bool is_8x16_sprite) {
    current_pattern_type_ = pattern_type;
    current_is_8x16_sprite_ = is_8x16_sprite;
  }

 private:
  void SetDefaultPalettes();
  Byte ReadPalette(Byte palette_address);

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

  // For MMC5
  CurrentPatternType current_pattern_type_ = CurrentPatternType::kBackground;
  bool current_is_8x16_sprite_ = false;
  bool is_mmc5_ = false;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_PPU_BUS_H_
