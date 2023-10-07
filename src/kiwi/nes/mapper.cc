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

#include "nes/mapper.h"

#include "base/logging.h"
#include "nes/cartridge.h"
#include "nes/mappers/mapper000.h"
#include "nes/mappers/mapper001.h"
#include "nes/mappers/mapper002.h"
#include "nes/mappers/mapper003.h"
#include "nes/mappers/mapper004.h"
#include "nes/mappers/mapper040.h"
#include "nes/mappers/mapper087.h"

namespace kiwi {
namespace nes {
Mapper::Mapper(Cartridge* cartridge) : cartridge_(cartridge) {}
Mapper::~Mapper() = default;

NametableMirroring Mapper::GetNametableMirroring() {
  DCHECK(cartridge_ && cartridge_->GetRomData());
  return cartridge_->GetRomData()->name_table_mirroring;
}

void Mapper::ScanlineIRQ() {}

void Mapper::M2CycleIRQ() {}

bool Mapper::HasExtendedRAM() {
  DCHECK(cartridge_ && cartridge_->GetRomData());
  return cartridge_->GetRomData()->has_extended_ram;
}

void Mapper::PPUAddressChanged(Address address) {}

std::unique_ptr<Mapper> Mapper::Create(Cartridge* cartridge, Byte mapper) {
  switch (mapper) {
    case 0:
      return std::make_unique<Mapper000>(cartridge);
    case 1:
      return std::make_unique<Mapper001>(cartridge);
    case 2:
      return std::make_unique<Mapper002>(cartridge);
    case 3:
      return std::make_unique<Mapper003>(cartridge);
    case 4:
      return std::make_unique<Mapper004>(cartridge);
      break;
    /*
  case Type::kAxROM:
    ret.reset(new MapperAxROM(cart, mirroring_cb));
    break;
  case Type::kColorDreams:
    ret.reset(new MapperColorDreams(cart, mirroring_cb));
    break;
  case Type::kGxROM:
    ret.reset(new MapperGxROM(cart, mirroring_cb));
    break;
    */
    case 40:
      return std::make_unique<Mapper040>(cartridge);
    case 87:
      return std::make_unique<Mapper087>(cartridge);
    default:
      break;
  }
  LOG(ERROR) << "Unsupported mapper: " << static_cast<int>(mapper);
  return nullptr;
}

void Mapper::WriteExtendedRAM(Address address, Byte value) {
  if (HasExtendedRAM()) {
    if (address >= 0x6000 && address <= 0x7fff) {
      CheckExtendedRAM();
      extended_ram_[address - 0x6000] = value;
    }
  } else {
    WritePRG(address, value);
  }
}

Byte Mapper::ReadExtendedRAM(Address address) {
  if (address >= 0x6000 && address <= 0x7fff) {
    CheckExtendedRAM();
    return extended_ram_[address - 0x6000];
  }

  // Open bus behavior:
  // https://www.nesdev.org/wiki/Open_bus_behavior#CPU_open_bus
  // Absolute addressed instructions will read the high byte of the address (the
  // last byte of the operand).
  return static_cast<Byte>(address >> 8);
}

void Mapper::Serialize(EmulatorStates::SerializableStateData& data) {
  if (HasExtendedRAM())
    data.WriteData(extended_ram_);
}

bool Mapper::Deserialize(const EmulatorStates::Header& header,
                         EmulatorStates::DeserializableStateData& data) {
  if (HasExtendedRAM())
    data.ReadData(&extended_ram_);

  return true;
}

void Mapper::CheckExtendedRAM() {
  if (extended_ram_.empty()) {
    if (!HasExtendedRAM()) {
      LOG(WARNING)
          << "This ROM will read/write to extended RAM, but the NES file "
             "indicates no extended RAM exists. Perhaps the NES file is "
             "incorrect, but the emulator still created extended RAM for it.";
    }
    extended_ram_.resize(0x2000);
  }
}

Byte* Mapper::GetExtendedRAMPointer() {
  return const_cast<Byte*>(extended_ram_.data());
}

}  // namespace nes
}  // namespace kiwi
