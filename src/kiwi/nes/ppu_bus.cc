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

#include "nes/ppu_bus.h"

#include "base/check.h"
#include "nes/mapper.h"
#include "nes/registers.h"

namespace kiwi {
namespace nes {
PPUBus::PPUBus() = default;
PPUBus::~PPUBus() = default;

void PPUBus::SetMapper(Mapper* mapper) {
  DCHECK(mapper);

  mapper_ = mapper;
  is_mmc5_ = mapper_->IsMMC5();

  UpdateMirroring();
  SetDefaultPalettes();
}

void PPUBus::SetCurrentPatternState(CurrentPatternType pattern_type,
                                    bool is_8x16_sprite,
                                    int current_dot_in_scanline) {
  if (is_mmc5_) {
    // MMC5 needs know whether is fetching background tile or sprite tile.
    // Uchuu Keibitai SDF (Japan) will fetch nametable and write bytes before
    // rendering, so the current pattern type will be kNotRendering, and it
    // will be treated as kBackground.
    mapper_->SetCurrentRenderState(
        pattern_type == CurrentPatternType::kBackground ||
            pattern_type == CurrentPatternType::kNotRendering,
        is_8x16_sprite, current_dot_in_scanline);
  }
}

Byte PPUBus::GetAdjustedXFine(Byte x_fine_in) {
  if (is_mmc5_)
    return mapper_->GetFineXInSplitRegion(x_fine_in);

  return x_fine_in;
}

Address PPUBus::GetAdjustedDataAddress(Address data_address_in) {
  if (is_mmc5_)
    return mapper_->GetDataAddressInSplitRegion(data_address_in);

  return data_address_in;
}

void PPUBus::SetDefaultPalettes() {
  // By default, the palettes are set to background=black (0x3f), other=white
  // (0x30).
  palette_[0] = 0x3f;
  for (Address i = 0x01; i < 0x20; ++i) {
    palette_[i] = 0x30;
  }
}

Mapper* PPUBus::GetMapper() {
  return mapper_;
}

Byte PPUBus::Read(Address address) {
  // The PPU addresses a 16kB space, $0000-3FFF, completely separate from the
  // CPU's address bus. It is either directly accessed by the PPU itself, or via
  // the CPU with memory mapped registers at $2006 and $2007.
  // The NES has 2kB of RAM dedicated to the PPU, normally mapped to the
  // nametable address space from $2000-2FFF, but this can be rerouted through
  // custom cartridge wiring.
  if (address < 0x2000) {
    return mapper_->ReadCHR(address);
  } else if (address < 0x3eff) {
    const auto index = address & 0x3ff;
    // Name tables upto 0x3000, then mirrored upto 3eff
    auto normalized_address = address;
    if (address >= 0x3000) {
      normalized_address -= 0x1000;
    }

    if (nametable_[0] >= RAM_SIZE) {
      return mapper_->ReadCHR(normalized_address);
    } else {
      if (!is_mmc5_) {
        if (normalized_address < 0x2400)  // NT0
          return ram_[nametable_[0] + index];
        else if (normalized_address < 0x2800)  // NT1
          return ram_[nametable_[1] + index];
        else if (normalized_address < 0x2c00)  // NT2
          return ram_[nametable_[2] + index];
        else /* if (normalized_address < 0x3000)*/  // NT3
          return ram_[nametable_[3] + index];
      } else {
        // mmc5 has its own nametable routine
        return mapper_->ReadNametableByte(ram_.data(), normalized_address);
      }
    }
  } else if (address < 0x3fff) {
    auto palette_address = address & 0x1f;
    return ReadPalette(palette_address);
  }
  return 0;
}

void PPUBus::Write(Address address, Byte value) {
  if (address < 0x2000) {
    mapper_->WriteCHR(address, value);
  } else if (address < 0x3f00) {
    const auto index = address & 0x03ff;
    // Name tables up to 0x3000, then mirrored up to 3eff
    auto normalized_address = address;
    if (address >= 0x3000) {
      normalized_address -= 0x1000;
    }

    if (nametable_[0] >= RAM_SIZE)
      mapper_->WriteCHR(normalized_address, value);
    else {
      if (!is_mmc5_) {
        if (normalized_address < 0x2400)  // Nametable 0
          ram_[nametable_[0] + index] = value;
        else if (normalized_address < 0x2800)  // Nametable 1
          ram_[nametable_[1] + index] = value;
        else if (normalized_address < 0x2c00)  // Nametable 2
          ram_[nametable_[2] + index] = value;
        else  // Nametable 3
          ram_[nametable_[3] + index] = value;
      } else {
        // mmc5 has its own nametable routine
        mapper_->WriteNametableByte(ram_.data(), normalized_address, value);
      }
    }
  } else if (address < 0x3fff) {
    auto palette = address & 0x1f;
    // Addresses $3F10/$3F14/$3F18/$3F1C are mirrors of $3F00/$3F04/$3F08/$3F0C
    if (palette >= 0x10 && address % 4 == 0) {
      palette = palette & 0xf;
    }

    palette_[palette] = value;
  }
}

void PPUBus::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(nametable_).WriteData(ram_).WriteData(palette_);
}

bool PPUBus::Deserialize(const EmulatorStates::Header& header,
                         EmulatorStates::DeserializableStateData& data) {
  if (header.version == 1) {
    data.ReadData(&nametable_).ReadData(&ram_).ReadData(&palette_);
    return true;
  }
  return false;
}

Byte PPUBus::ReadPalette(Byte palette_address) {
  // Addresses $3F10/$3F14/$3F18/$3F1C are mirrors of $3F00/$3F04/$3F08/$3F0C.
  // Palette details:
  // https://www.nesdev.org/wiki/PPU_palettes
  if (palette_address >= 0x10 && palette_address % 4 == 0) {
    palette_address = palette_address & 0xf;
  }

  // Some games (Such as Lunar Pool, The New Type, etc.) will write a 0xff to
  // the palette, which is larger than 0x3f and causes overflow, So we have to
  // limit it.
  return palette_[palette_address] & 0x3f;
}

void PPUBus::UpdateMirroring() {
  // Fill mirroring data by nametable mirroring type.
  // See https://www.nesdev.org/wiki/PPU_nametables for more details.
  switch (mapper_->GetNametableMirroring()) {
    case NametableMirroring::kHorizontal:
      nametable_[0] = nametable_[1] = 0;
      nametable_[2] = nametable_[3] = 0x400;
      break;
    case NametableMirroring::kVertical:
      nametable_[0] = nametable_[2] = 0;
      nametable_[1] = nametable_[3] = 0x400;
      break;
    case NametableMirroring::kOneScreenLower:
      nametable_[0] = nametable_[1] = nametable_[2] = nametable_[3] = 0;
      break;
    case NametableMirroring::kOneScreenHigher:
      nametable_[0] = nametable_[1] = nametable_[2] = nametable_[3] = 0x400;
      break;
    case NametableMirroring::kFourScreen:
      // the cartridge contains additional VRAM used for all nametables
      nametable_[0] = RAM_SIZE;
      break;
    default:
      nametable_[0] = nametable_[1] = nametable_[2] = nametable_[3] = 0;
      LOG(ERROR) << "Unsupported Name Table mirroring : "
                 << static_cast<int>(mapper_->GetNametableMirroring());
  }
}

}  // namespace nes
}  // namespace kiwi
