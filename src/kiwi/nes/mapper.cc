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
#include "nes/nes_buildflags.h"
#if BUILDFLAG(ENABLE_MAPPER_013)
#include "nes/mappers/mapper013.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_016)
#include "nes/mappers/mapper016.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_018)
#include "nes/mappers/mapper018.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_019)
#include "nes/mappers/mapper019.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_021) || BUILDFLAG(ENABLE_MAPPER_022) || \
    BUILDFLAG(ENABLE_MAPPER_023) || BUILDFLAG(ENABLE_MAPPER_025)
#include "nes/mappers/mapper023.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_024) || BUILDFLAG(ENABLE_MAPPER_026)
#include "nes/mappers/mapper024.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_032)
#include "nes/mappers/mapper032.h"
#endif
#include "nes/mappers/mapper033.h"
#if BUILDFLAG(ENABLE_MAPPER_034)
#include "nes/mappers/mapper034.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_037)
#include "nes/mappers/mapper037.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_038)
#include "nes/mappers/mapper038.h"
#endif
#include "nes/mappers/mapper040.h"
#if BUILDFLAG(ENABLE_MAPPER_041)
#include "nes/mappers/mapper041.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_046)
#include "nes/mappers/mapper046.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_047)
#include "nes/mappers/mapper047.h"
#endif
#include "nes/mappers/mapper048.h"
#if BUILDFLAG(ENABLE_MAPPER_064)
#include "nes/mappers/mapper064.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_065)
#include "nes/mappers/mapper065.h"
#endif
#include "nes/mappers/mapper066.h"
#if BUILDFLAG(ENABLE_MAPPER_067)
#include "nes/mappers/mapper067.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_068)
#include "nes/mappers/mapper068.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_069)
#include "nes/mappers/mapper069.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_070)
#include "nes/mappers/mapper070.h"
#endif
#include "nes/mappers/mapper074.h"
#include "nes/mappers/mapper075.h"
#if BUILDFLAG(ENABLE_MAPPER_071)
#include "nes/mappers/mapper071.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_072)
#include "nes/mappers/mapper072.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_073)
#include "nes/mappers/mapper073.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_077)
#include "nes/mappers/mapper077.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_078)
#include "nes/mappers/mapper078.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_079) || BUILDFLAG(ENABLE_MAPPER_146)
#include "nes/mappers/mapper079.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_080)
#include "nes/mappers/mapper080.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_085)
#include "nes/mappers/mapper085.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_086)
#include "nes/mappers/mapper086.h"
#endif
#include "nes/mappers/mapper087.h"
#if BUILDFLAG(ENABLE_MAPPER_089)
#include "nes/mappers/mapper089.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_092)
#include "nes/mappers/mapper092.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_093)
#include "nes/mappers/mapper093.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_094)
#include "nes/mappers/mapper094.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_096)
#include "nes/mappers/mapper096.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_097)
#include "nes/mappers/mapper097.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_104)
#include "nes/mappers/mapper104.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_105)
#include "nes/mappers/mapper105.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_112)
#include "nes/mappers/mapper112.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_113)
#include "nes/mappers/mapper113.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_115)
#include "nes/mappers/mapper115.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_118)
#include "nes/mappers/mapper118.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_119)
#include "nes/mappers/mapper119.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_132)
#include "nes/mappers/mapper132.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_133)
#include "nes/mappers/mapper133.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_137)
#include "nes/mappers/mapper137.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_139)
#include "nes/mappers/mapper139.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_140)
#include "nes/mappers/mapper140.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_141)
#include "nes/mappers/mapper141.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_143)
#include "nes/mappers/mapper143.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_147)
#include "nes/mappers/mapper147.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_148)
#include "nes/mappers/mapper148.h"
#endif
#include "nes/mappers/mapper185.h"
#if BUILDFLAG(ENABLE_MAPPER_150)
#include "nes/mappers/mapper150.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_156)
#include "nes/mappers/mapper156.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_168)
#include "nes/mappers/mapper168.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_176)
#include "nes/mappers/mapper176.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_177)
#include "nes/mappers/mapper177.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_178)
#include "nes/mappers/mapper178.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_180)
#include "nes/mappers/mapper180.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_184)
#include "nes/mappers/mapper184.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_186)
#include "nes/mappers/mapper186.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_188)
#include "nes/mappers/mapper188.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_189)
#include "nes/mappers/mapper189.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_193)
#include "nes/mappers/mapper193.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_206)
#include "nes/mappers/mapper206.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_216)
#include "nes/mappers/mapper216.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_228)
#include "nes/mappers/mapper228.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_232)
#include "nes/mappers/mapper232.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_234)
#include "nes/mappers/mapper234.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_240)
#include "nes/mappers/mapper240.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_241)
#include "nes/mappers/mapper241.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_244)
#include "nes/mappers/mapper244.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_245)
#include "nes/mappers/mapper245.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_246)
#include "nes/mappers/mapper246.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_355)
#include "nes/mappers/mapper355.h"
#endif
#if BUILDFLAG(ENABLE_MAPPER_512)
#include "nes/mappers/mapper512.h"
#endif

namespace kiwi {
namespace nes {
namespace {
struct MapperFactory {
  MapperFactory() = default;
  virtual ~MapperFactory() = default;
  virtual std::unique_ptr<Mapper> CreateMapper(Cartridge* cartridge) = 0;
};

template <typename MapperType, MapperId mapper>
struct MapperFactoryBuilder : MapperFactory {
  MapperFactoryBuilder() = default;
  ~MapperFactoryBuilder() override = default;
  std::unique_ptr<Mapper> CreateMapper(Cartridge* cartridge) override {
    return std::make_unique<MapperType>(cartridge);
  }
};

#define MAPPER(mapper, type) {mapper, new MapperFactoryBuilder<type, mapper>()}

// Leaks mappers by purpose.
std::map<MapperId, MapperFactory*> mapper_factories = {
    MAPPER(0, Mapper000),   MAPPER(1, Mapper001),   MAPPER(2, Mapper002),
    MAPPER(3, Mapper003),   MAPPER(4, Mapper004),   MAPPER(5, Mapper005),
    MAPPER(7, Mapper007),   MAPPER(9, Mapper009),   MAPPER(10, Mapper010),
    MAPPER(11, Mapper011),  MAPPER(33, Mapper033),  MAPPER(40, Mapper040),
    MAPPER(48, Mapper048),  MAPPER(66, Mapper066),  MAPPER(74, Mapper074),
    MAPPER(75, Mapper075),  MAPPER(87, Mapper087),  MAPPER(185, Mapper185),
#if BUILDFLAG(ENABLE_MAPPER_013)
    MAPPER(13, Mapper013),
#endif
#if BUILDFLAG(ENABLE_MAPPER_018)
    MAPPER(18, Mapper018),
#endif
#if BUILDFLAG(ENABLE_MAPPER_019)
    MAPPER(19, Mapper019),  MAPPER(210, Mapper019),
#endif
#if BUILDFLAG(ENABLE_MAPPER_016)
    MAPPER(16, Mapper016),  MAPPER(153, Mapper016), MAPPER(157, Mapper016),
    MAPPER(159, Mapper016),
#endif
#if BUILDFLAG(ENABLE_MAPPER_021)
    MAPPER(21, Mapper023),
#endif
#if BUILDFLAG(ENABLE_MAPPER_022)
    MAPPER(22, Mapper023),
#endif
#if BUILDFLAG(ENABLE_MAPPER_023)
    MAPPER(23, Mapper023),
#endif
#if BUILDFLAG(ENABLE_MAPPER_024)
    MAPPER(24, Mapper024),
#endif
#if BUILDFLAG(ENABLE_MAPPER_025)
    MAPPER(25, Mapper023),
#endif
#if BUILDFLAG(ENABLE_MAPPER_026)
    MAPPER(26, Mapper024),
#endif
#if BUILDFLAG(ENABLE_MAPPER_032)
    MAPPER(32, Mapper032),
#endif
#if BUILDFLAG(ENABLE_MAPPER_034)
    MAPPER(34, Mapper034),
#endif
#if BUILDFLAG(ENABLE_MAPPER_037)
    MAPPER(37, Mapper037),
#endif
#if BUILDFLAG(ENABLE_MAPPER_038)
    MAPPER(38, Mapper038),
#endif
#if BUILDFLAG(ENABLE_MAPPER_041)
    MAPPER(41, Mapper041),
#endif
#if BUILDFLAG(ENABLE_MAPPER_046)
    MAPPER(46, Mapper046),
#endif
#if BUILDFLAG(ENABLE_MAPPER_047)
    MAPPER(47, Mapper047),
#endif
#if BUILDFLAG(ENABLE_MAPPER_064)
    MAPPER(64, Mapper064),
#endif
#if BUILDFLAG(ENABLE_MAPPER_065)
    MAPPER(65, Mapper065),
#endif
#if BUILDFLAG(ENABLE_MAPPER_067)
    MAPPER(67, Mapper067),
#endif
#if BUILDFLAG(ENABLE_MAPPER_068)
    MAPPER(68, Mapper068),
#endif
#if BUILDFLAG(ENABLE_MAPPER_069)
    MAPPER(69, Mapper069),
#endif
#if BUILDFLAG(ENABLE_MAPPER_070)
    MAPPER(70, Mapper070),  MAPPER(152, Mapper070),
#endif
#if BUILDFLAG(ENABLE_MAPPER_071)
    MAPPER(71, Mapper071),
#endif
#if BUILDFLAG(ENABLE_MAPPER_072)
    MAPPER(72, Mapper072),
#endif
#if BUILDFLAG(ENABLE_MAPPER_073)
    MAPPER(73, Mapper073),
#endif
#if BUILDFLAG(ENABLE_MAPPER_077)
    MAPPER(77, Mapper077),
#endif
#if BUILDFLAG(ENABLE_MAPPER_078)
    MAPPER(78, Mapper078),
#endif
#if BUILDFLAG(ENABLE_MAPPER_079)
    MAPPER(79, Mapper079),
#endif
#if BUILDFLAG(ENABLE_MAPPER_080)
    MAPPER(80, Mapper080),  MAPPER(82, Mapper080),  MAPPER(207, Mapper080),
#endif
#if BUILDFLAG(ENABLE_MAPPER_085)
    MAPPER(85, Mapper085),
#endif
#if BUILDFLAG(ENABLE_MAPPER_086)
    MAPPER(86, Mapper086),
#endif
#if BUILDFLAG(ENABLE_MAPPER_089)
    MAPPER(89, Mapper089),
#endif
#if BUILDFLAG(ENABLE_MAPPER_092)
    MAPPER(92, Mapper092),
#endif
#if BUILDFLAG(ENABLE_MAPPER_093)
    MAPPER(93, Mapper093),
#endif
#if BUILDFLAG(ENABLE_MAPPER_094)
    MAPPER(94, Mapper094),
#endif
#if BUILDFLAG(ENABLE_MAPPER_096)
    MAPPER(96, Mapper096),
#endif
#if BUILDFLAG(ENABLE_MAPPER_097)
    MAPPER(97, Mapper097),
#endif
#if BUILDFLAG(ENABLE_MAPPER_104)
    MAPPER(104, Mapper104),
#endif
#if BUILDFLAG(ENABLE_MAPPER_105)
    MAPPER(105, Mapper105),
#endif
#if BUILDFLAG(ENABLE_MAPPER_112)
    MAPPER(112, Mapper112),
#endif
#if BUILDFLAG(ENABLE_MAPPER_113)
    MAPPER(113, Mapper113),
#endif
#if BUILDFLAG(ENABLE_MAPPER_115)
    MAPPER(115, Mapper115),
#endif
#if BUILDFLAG(ENABLE_MAPPER_118)
    MAPPER(118, Mapper118),
#endif
#if BUILDFLAG(ENABLE_MAPPER_119)
    MAPPER(119, Mapper119),
#endif
#if BUILDFLAG(ENABLE_MAPPER_132)
    MAPPER(132, Mapper132),
#endif
#if BUILDFLAG(ENABLE_MAPPER_133)
    MAPPER(133, Mapper133),
#endif
#if BUILDFLAG(ENABLE_MAPPER_137)
    MAPPER(137, Mapper137),
#endif
#if BUILDFLAG(ENABLE_MAPPER_139)
    MAPPER(139, Mapper139),
#endif
#if BUILDFLAG(ENABLE_MAPPER_140)
    MAPPER(140, Mapper140),
#endif
#if BUILDFLAG(ENABLE_MAPPER_141)
    MAPPER(141, Mapper141),
#endif
#if BUILDFLAG(ENABLE_MAPPER_143)
    MAPPER(143, Mapper143),
#endif
#if BUILDFLAG(ENABLE_MAPPER_146)
    MAPPER(146, Mapper079),
#endif
#if BUILDFLAG(ENABLE_MAPPER_147)
    MAPPER(147, Mapper147),
#endif
#if BUILDFLAG(ENABLE_MAPPER_148)
    MAPPER(148, Mapper148),
#endif
#if BUILDFLAG(ENABLE_MAPPER_150)
    MAPPER(150, Mapper150), MAPPER(243, Mapper150),
#endif
#if BUILDFLAG(ENABLE_MAPPER_156)
    MAPPER(156, Mapper156),
#endif
#if BUILDFLAG(ENABLE_MAPPER_168)
    MAPPER(168, Mapper168),
#endif
#if BUILDFLAG(ENABLE_MAPPER_176)
    MAPPER(176, Mapper176),
#endif
#if BUILDFLAG(ENABLE_MAPPER_177)
    MAPPER(177, Mapper177),
#endif
#if BUILDFLAG(ENABLE_MAPPER_178)
    MAPPER(178, Mapper178),
#endif
#if BUILDFLAG(ENABLE_MAPPER_180)
    MAPPER(180, Mapper180),
#endif
#if BUILDFLAG(ENABLE_MAPPER_184)
    MAPPER(184, Mapper184),
#endif
#if BUILDFLAG(ENABLE_MAPPER_186)
    MAPPER(186, Mapper186),
#endif
#if BUILDFLAG(ENABLE_MAPPER_188)
    MAPPER(188, Mapper188),
#endif
#if BUILDFLAG(ENABLE_MAPPER_189)
    MAPPER(189, Mapper189),
#endif
#if BUILDFLAG(ENABLE_MAPPER_193)
    MAPPER(193, Mapper193),
#endif
#if BUILDFLAG(ENABLE_MAPPER_206)
    MAPPER(76, Mapper206),  MAPPER(88, Mapper206),  MAPPER(95, Mapper206),
    MAPPER(154, Mapper206), MAPPER(206, Mapper206),
#endif
#if BUILDFLAG(ENABLE_MAPPER_216)
    MAPPER(216, Mapper216),
#endif
#if BUILDFLAG(ENABLE_MAPPER_228)
    MAPPER(228, Mapper228),
#endif
#if BUILDFLAG(ENABLE_MAPPER_232)
    MAPPER(232, Mapper232),
#endif
#if BUILDFLAG(ENABLE_MAPPER_234)
    MAPPER(234, Mapper234),
#endif
#if BUILDFLAG(ENABLE_MAPPER_240)
    MAPPER(240, Mapper240),
#endif
#if BUILDFLAG(ENABLE_MAPPER_241)
    MAPPER(241, Mapper241),
#endif
#if BUILDFLAG(ENABLE_MAPPER_244)
    MAPPER(244, Mapper244),
#endif
#if BUILDFLAG(ENABLE_MAPPER_245)
    MAPPER(245, Mapper245),
#endif
#if BUILDFLAG(ENABLE_MAPPER_246)
    MAPPER(246, Mapper246),
#endif
#if BUILDFLAG(ENABLE_MAPPER_355)
    MAPPER(355, Mapper355),
#endif
#if BUILDFLAG(ENABLE_MAPPER_512)
    MAPPER(512, Mapper512),
#endif
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

#if BUILDFLAG(ENABLE_MAPPER_096)
bool Mapper::NeedsPPUAddressNotifications() const {
  return false;
}
#endif

#if BUILDFLAG(ENABLE_MAPPER_019) || BUILDFLAG(ENABLE_MAPPER_024) || \
    BUILDFLAG(ENABLE_MAPPER_026) || BUILDFLAG(ENABLE_MAPPER_068) || \
    BUILDFLAG(ENABLE_MAPPER_077) || BUILDFLAG(ENABLE_MAPPER_080) || \
    BUILDFLAG(ENABLE_MAPPER_118) || BUILDFLAG(ENABLE_MAPPER_137) || \
    BUILDFLAG(ENABLE_MAPPER_139) || BUILDFLAG(ENABLE_MAPPER_141) || \
    BUILDFLAG(ENABLE_MAPPER_150) || BUILDFLAG(ENABLE_MAPPER_177) || \
    BUILDFLAG(ENABLE_MAPPER_206)
bool Mapper::UsesCustomPPUMemoryMapping() const {
  return false;
}

Byte Mapper::ReadPPUMemoryByte(Byte* ciram, Address address) {
  return address < 0x2000 ? ReadCHR(address) : 0;
}

void Mapper::WritePPUMemoryByte(Byte* ciram, Address address, Byte value) {
  if (address < 0x2000)
    WriteCHR(address, value);
}
#endif

#if BUILDFLAG(ENABLE_MAPPER_019) || BUILDFLAG(ENABLE_MAPPER_024) || \
    BUILDFLAG(ENABLE_MAPPER_026) || BUILDFLAG(ENABLE_MAPPER_085)
void Mapper::SetExpansionAudioOutput(Blip_Buffer* output) {}

void Mapper::SetExpansionAudioVolume(float volume) {}

void Mapper::EndExpansionAudioFrame(int64_t cycles) {}
#endif

void Mapper::M2CycleIRQ() {}

bool Mapper::HasPRGRAM() const {
  DCHECK(rom_data_);
  return rom_data_->prg_ram_size != 0 || rom_data_->prg_nvram_size != 0;
}

bool Mapper::HasBatteryBackedRAM() const {
  DCHECK(rom_data_);
#if BUILDFLAG(ENABLE_MAPPER_168)
  if (GetCustomNVRAMSize() != 0)
    return true;
#endif
  return rom_data_->prg_nvram_size != 0;
}

size_t Mapper::GetPRGNVRAMSize() const {
  DCHECK(rom_data_);
#if BUILDFLAG(ENABLE_MAPPER_168)
  const size_t custom_nvram_size = GetCustomNVRAMSize();
  if (custom_nvram_size != 0)
    return custom_nvram_size;
#endif
  return rom_data_->prg_nvram_size;
}

#if BUILDFLAG(ENABLE_MAPPER_168)
size_t Mapper::GetCustomNVRAMSize() const {
  return 0;
}
#endif

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

std::unique_ptr<Mapper> Mapper::Create(Cartridge* cartridge, MapperId mapper) {
  auto iter = mapper_factories.find(mapper);
  if (iter != mapper_factories.cend()) {
    MapperFactory* factory = iter->second;
    CHECK(factory);
    return factory->CreateMapper(cartridge);
  }

  LOG(ERROR) << "Unsupported mapper: " << static_cast<int>(mapper);
  return nullptr;
}

bool Mapper::IsMapperSupported(MapperId mapper) {
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
