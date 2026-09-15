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

#include <algorithm>
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

#define MAPPER(mapper, type) {mapper, new MapperFactoryBuilder<type, mapper>()}

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

bool Mapper::NeedsM2CycleIRQ() const {
  return false;
}

uint32_t Mapper::GetAbsoluteCHRAddress(Address address) {
  return address & 0x1fff;
}

bool Mapper::UsesCustomPRGRAM() const {
  return false;
}

void Mapper::M2CycleIRQ() {}

bool Mapper::HasPRGRAM() const {
  DCHECK(rom_data_);
  return rom_data_->prg_ram_size != 0 || rom_data_->prg_nvram_size != 0;
}

bool Mapper::HasBatteryBackedRAM() const {
  DCHECK(rom_data_);
  return rom_data_->prg_nvram_size != 0;
}

size_t Mapper::GetPRGNVRAMSize() const {
  DCHECK(rom_data_);
  return rom_data_->prg_nvram_size;
}

std::optional<Mapper::PRGNVRAMSnapshot> Mapper::ExportPRGNVRAM(
    bool dirty_only) {
  if (!HasBatteryBackedRAM() || (dirty_only && !IsPRGNVRAMDirty())) {
    return std::nullopt;
  }

  Bytes data = CopyPRGNVRAM();
  if (data.size() != GetPRGNVRAMSize()) {
    LOG(ERROR) << "Mapper returned an invalid PRG-NVRAM snapshot size.";
    return std::nullopt;
  }

  return PRGNVRAMSnapshot{std::move(data), prg_nvram_generation_};
}

bool Mapper::ImportPRGNVRAM(const Bytes& data) {
  if (!HasBatteryBackedRAM() || data.size() != GetPRGNVRAMSize()) {
    return false;
  }

  if (CopyPRGNVRAM() != data) {
    if (!RestorePRGNVRAM(data)) {
      return false;
    }
    MarkPRGNVRAMDirty();
  }
  persisted_prg_nvram_generation_ = prg_nvram_generation_;
  return true;
}

bool Mapper::IsPRGNVRAMDirty() const {
  return HasBatteryBackedRAM() &&
         prg_nvram_generation_ != persisted_prg_nvram_generation_;
}

void Mapper::AcknowledgePRGNVRAMSaved(uint64_t generation) {
  if (generation > persisted_prg_nvram_generation_ &&
      generation <= prg_nvram_generation_) {
    persisted_prg_nvram_generation_ = generation;
  }
}

void Mapper::EnsurePRGRAM(size_t minimum_size) {
  DCHECK(rom_data_);
  const size_t configured_size =
      rom_data_->prg_ram_size + rom_data_->prg_nvram_size;
  if (configured_size >= minimum_size) {
    return;
  }

  if (!rom_data_->is_nes_20 && rom_data_->has_battery) {
    rom_data_->prg_nvram_size += minimum_size - configured_size;
  } else {
    rom_data_->prg_ram_size += minimum_size - configured_size;
  }
}

void Mapper::MarkPRGNVRAMDirty() {
  if (HasBatteryBackedRAM()) {
    ++prg_nvram_generation_;
  }
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
  if (HasPRGRAM()) {
    if (address >= 0x6000) {
      AllocatePRGRAMIfNeeded();
      const size_t index = (address - 0x6000) % default_prg_ram_.size();
      if (default_prg_ram_[index] != value) {
        default_prg_ram_[index] = value;
        if (index < rom_data_->prg_nvram_size) {
          MarkPRGNVRAMDirty();
        }
      }
    }
  } else {
    WritePRG(address, value);
  }
}

Byte Mapper::ReadExtendedRAM(Address address) {
  if (HasPRGRAM() && address >= 0x6000 && address <= 0x7fff) {
    AllocatePRGRAMIfNeeded();
    return default_prg_ram_[(address - 0x6000) % default_prg_ram_.size()];
  }

  // Open bus behavior:
  // https://www.nesdev.org/wiki/Open_bus_behavior#CPU_open_bus
  // Absolute addressed instructions will read the high byte of the address
  // (the last byte of the operand).
  return static_cast<Byte>(address >> 8);
}

void Mapper::Serialize(EmulatorStates::SerializableStateData& data) {
  if (!UsesCustomPRGRAM() && HasPRGRAM()) {
    AllocatePRGRAMIfNeeded();
    data.WriteData(default_prg_ram_);
  }
}

bool Mapper::Deserialize(const EmulatorStates::Header& header,
                         EmulatorStates::DeserializableStateData& data) {
  if (!UsesCustomPRGRAM() && HasPRGRAM()) {
    AllocatePRGRAMIfNeeded();
    Bytes previous_nvram;
    if (HasBatteryBackedRAM()) {
      previous_nvram = CopyPRGNVRAM();
    }
    data.ReadData(&default_prg_ram_);
    if (HasBatteryBackedRAM() && previous_nvram != CopyPRGNVRAM()) {
      MarkPRGNVRAMDirty();
    }
  }

  return true;
}

Bytes Mapper::CopyPRGNVRAM() {
  if (!HasBatteryBackedRAM()) {
    return {};
  }

  AllocatePRGRAMIfNeeded();
  const size_t end = rom_data_->prg_nvram_size;
  DCHECK_LE(end, default_prg_ram_.size());
  return Bytes(default_prg_ram_.begin(), default_prg_ram_.begin() + end);
}

bool Mapper::RestorePRGNVRAM(const Bytes& data) {
  if (data.size() != GetPRGNVRAMSize()) {
    return false;
  }
  AllocatePRGRAMIfNeeded();
  std::copy(data.begin(), data.end(), default_prg_ram_.begin());
  return true;
}

void Mapper::AllocatePRGRAMIfNeeded() {
  if (!default_prg_ram_.empty() || !HasPRGRAM()) {
    return;
  }

  default_prg_ram_.resize(rom_data_->prg_ram_size + rom_data_->prg_nvram_size);
}

Byte* Mapper::GetExtendedRAMPointer() {
  AllocatePRGRAMIfNeeded();
  return default_prg_ram_.empty() ? nullptr : default_prg_ram_.data();
}

}  // namespace nes
}  // namespace kiwi
