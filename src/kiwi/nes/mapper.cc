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

#include <map>

#include "base/logging.h"
#include "nes/cartridge.h"
#include "nes/mappers/mapper000.h"
#include "nes/mappers/mapper001.h"
#include "nes/mappers/mapper002.h"
#include "nes/mappers/mapper003.h"
#include "nes/mappers/mapper004.h"
#include "nes/mappers/mapper005.h"
#include "nes/mappers/mapper007.h"
#include "nes/mappers/mapper009.h"
#include "nes/mappers/mapper010.h"
#include "nes/mappers/mapper011.h"
#include "nes/mappers/mapper033.h"
#include "nes/mappers/mapper040.h"
#include "nes/mappers/mapper048.h"
#include "nes/mappers/mapper066.h"
#include "nes/mappers/mapper074.h"
#include "nes/mappers/mapper075.h"
#include "nes/mappers/mapper087.h"
#include "nes/mappers/mapper185.h"

namespace kiwi {
namespace nes {
namespace {
struct MapperFactory {
  MapperFactory() = default;
  virtual ~MapperFactory() = default;
  virtual std::unique_ptr<Mapper> CreateMapper(Cartridge* cartridge) = 0;
};

template <typename MapperType, Byte mapper>
struct MapperFactoryBuilder : MapperFactory {
  MapperFactoryBuilder() = default;
  ~MapperFactoryBuilder() override = default;
  std::unique_ptr<Mapper> CreateMapper(Cartridge* cartridge) override {
    return std::make_unique<MapperType>(cartridge);
  }
};

#define MAPPER(mapper, type) \
  { mapper, new MapperFactoryBuilder<type, mapper>() }

// Leaks mappers by purpose.
std::map<Byte, MapperFactory*> mapper_factories = {
    MAPPER(0, Mapper000),  MAPPER(1, Mapper001),  MAPPER(2, Mapper002),
    MAPPER(3, Mapper003),  MAPPER(4, Mapper004),  MAPPER(5, Mapper005),
    MAPPER(7, Mapper007),  MAPPER(9, Mapper009),  MAPPER(10, Mapper010),
    MAPPER(11, Mapper011), MAPPER(33, Mapper033), MAPPER(40, Mapper040),
    MAPPER(48, Mapper048), MAPPER(66, Mapper066), MAPPER(74, Mapper074),
    MAPPER(75, Mapper075), MAPPER(87, Mapper087), MAPPER(185, Mapper185),
};
}  // namespace

Mapper::Mapper(Cartridge* cartridge) {
  DCHECK(cartridge);
  rom_data_ = cartridge->GetRomData();
}
Mapper::~Mapper() = default;

NametableMirroring Mapper::GetNametableMirroring() {
  DCHECK(rom_data_);
  return rom_data_->name_table_mirroring;
}

void Mapper::Reset() {}

void Mapper::ScanlineIRQ(int scanline, bool render_enabled) {}

void Mapper::M2CycleIRQ() {}

bool Mapper::HasExtendedRAM() {
  DCHECK(rom_data_);
  return force_use_extended_ram_ || rom_data_->has_extended_ram;
}

void Mapper::PPUAddressChanged(Address address) {}

std::unique_ptr<Mapper> Mapper::Create(Cartridge* cartridge, Byte mapper) {
  auto iter = mapper_factories.find(mapper);
  if (iter != mapper_factories.cend()) {
    MapperFactory* factory = iter->second;
    CHECK(factory);
    return factory->CreateMapper(cartridge);
  }

  LOG(ERROR) << "Unsupported mapper: " << static_cast<int>(mapper);
  return nullptr;
}

bool Mapper::IsMapperSupported(Byte mapper) {
  auto iter = mapper_factories.find(mapper);
  return (iter != mapper_factories.cend());
}

void Mapper::WriteExtendedRAM(Address address, Byte value) {
  if (HasExtendedRAM()) {
    if (address >= 0x6000) {
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
  data.WriteData(force_use_extended_ram_);
  if (HasExtendedRAM()) {
    CheckExtendedRAM();
    data.WriteData(extended_ram_);
  }
}

bool Mapper::Deserialize(const EmulatorStates::Header& header,
                         EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&force_use_extended_ram_);
  if (HasExtendedRAM()) {
    CheckExtendedRAM();
    data.ReadData(&extended_ram_);
  }

  return true;
}

void Mapper::CheckExtendedRAM() {
  if (extended_ram_.empty()) {
    if (!HasExtendedRAM()) {
      LOG(WARNING)
          << "This ROM will read/write to extended RAM, but the NES file "
             "indicates no extended RAM exists. Perhaps the NES file is "
             "incorrect, but the emulator still created extended RAM for it.";
      force_use_extended_ram_ = true;
    }
    extended_ram_.resize(0x2000);
  }
}

Byte* Mapper::GetExtendedRAMPointer() {
  return const_cast<Byte*>(extended_ram_.data());
}

}  // namespace nes
}  // namespace kiwi
