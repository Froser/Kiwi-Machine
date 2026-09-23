// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper080.h"

#include <algorithm>
#include <iterator>

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x2000;
constexpr size_t kCHRBankSize = 0x0400;
constexpr size_t kCHRRAMSize = 0x2000;
constexpr size_t kX1005RAMSize = 0x0080;
constexpr size_t kX1017RAMSize = 0x1400;

}  // namespace

Mapper080::Mapper080(Cartridge* cartridge) : Mapper(cartridge) {
  switch (rom_data()->mapper) {
    case 82:
      variant_ = Variant::kX1017;
      break;
    case 207:
      variant_ = Variant::kX1005Mapper207;
      break;
    default:
      DCHECK_EQ(rom_data()->mapper, 80);
      break;
  }

  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  DCHECK_GE(prg_bank_count_, 4u);

  if (rom_data()->CHR.empty()) {
    character_ram_.resize(kCHRRAMSize);
    chr_bank_count_ = character_ram_.size() / kCHRBankSize;
  } else {
    DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
    chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  }
  DCHECK_GT(chr_bank_count_, 0u);

  ConfigureInternalRAM();
  Reset();
}

Mapper080::~Mapper080() = default;

void Mapper080::Reset() {
  std::fill(std::begin(prg_banks_), std::end(prg_banks_), 0);
  std::fill(std::begin(chr_banks_), std::end(chr_banks_), 0);
  std::fill(std::begin(ram_permissions_), std::end(ram_permissions_), 0);
  chr_mode_ = 0;
  x1005_ram_permission_ = 0;
  nametable_pages_[0] = 0;
  nametable_pages_[1] = 0;
  irq_latch_ = 0;
  irq_control_ = 0;
  irq_pending_ = false;
  ReloadIRQCounter(false);
  mirroring_ = rom_data()->name_table_mirroring;
}

void Mapper080::WritePRG(Address address, Byte value) {}

Byte Mapper080::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t window = (address - 0x8000) / kPRGBankSize;
  const size_t bank =
      window < 3 ? prg_banks_[window] % prg_bank_count_ : prg_bank_count_ - 1;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x1fff)];
}

void Mapper080::WriteCHR(Address address, Byte value) {
  if (!character_ram_.empty())
    character_ram_[GetAbsoluteCHRAddress(address)] = value;
}

Byte Mapper080::ReadCHR(Address address) {
  if (!character_ram_.empty())
    return character_ram_[GetAbsoluteCHRAddress(address)];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper080::GetAbsoluteCHRAddress(Address address) {
  const size_t bank = GetCHRBank(address) % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x03ff));
}

void Mapper080::WriteExtendedRAM(Address address, Byte value) {
  if (variant_ == Variant::kX1017) {
    if (address >= 0x7ef0 && address <= 0x7eff) {
      WriteX1017Register(address, value);
      return;
    }
  } else if ((address & 0xff70) == 0x7e70) {
    WriteX1005Register(address | 0x0080, value);
    return;
  }

  if (!IsRAMAddressEnabled(address))
    return;

  if (variant_ == Variant::kX1017) {
    Mapper::WriteExtendedRAM(address, value);
  } else {
    Mapper::WriteExtendedRAM(static_cast<Address>(0x6000 + (address & 0x007f)),
                             value);
  }
}

Byte Mapper080::ReadExtendedRAM(Address address) {
  if (IsRAMAddressEnabled(address)) {
    if (variant_ == Variant::kX1017)
      return Mapper::ReadExtendedRAM(address);
    return Mapper::ReadExtendedRAM(
        static_cast<Address>(0x6000 + (address & 0x007f)));
  }

  if (variant_ == Variant::kX1017)
    return 0;
  return static_cast<Byte>(address >> 8);
}

Byte* Mapper080::GetExtendedRAMPointer() {
  return nullptr;
}

NametableMirroring Mapper080::GetNametableMirroring() {
  if (variant_ != Variant::kX1005Mapper207)
    return mirroring_;
  if (nametable_pages_[0] == nametable_pages_[1]) {
    return nametable_pages_[0] ? NametableMirroring::kOneScreenHigher
                               : NametableMirroring::kOneScreenLower;
  }
  return NametableMirroring::kHorizontal;
}

bool Mapper080::NeedsM2CycleIRQ() const {
  return variant_ == Variant::kX1017;
}

void Mapper080::M2CycleIRQ() {
  if (variant_ != Variant::kX1017 || (irq_control_ & 0x05) != 0x01 ||
      irq_counter_ == 0) {
    return;
  }

  --irq_counter_;
  if (irq_counter_ == 0 && (irq_control_ & 0x02) && !irq_pending_) {
    irq_pending_ = true;
    if (irq_callback())
      irq_callback().Run();
  }
}

bool Mapper080::UsesCustomPPUMemoryMapping() const {
  return variant_ == Variant::kX1005Mapper207;
}

Byte Mapper080::ReadPPUMemoryByte(Byte* ciram, Address address) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000)
    return ReadCHR(address);
  const Byte page = nametable_pages_[(address >> 11) & 0x01];
  return ciram[page * kCHRBankSize + (address & 0x03ff)];
}

void Mapper080::WritePPUMemoryByte(Byte* ciram, Address address, Byte value) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000) {
    WriteCHR(address, value);
    return;
  }
  const Byte page = nametable_pages_[(address >> 11) & 0x01];
  ciram[page * kCHRBankSize + (address & 0x03ff)] = value;
}

void Mapper080::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(prg_banks_)
      .WriteData(chr_banks_)
      .WriteData(chr_mode_)
      .WriteData(ram_permissions_)
      .WriteData(x1005_ram_permission_)
      .WriteData(nametable_pages_)
      .WriteData(irq_latch_)
      .WriteData(irq_control_)
      .WriteData(irq_counter_)
      .WriteData(irq_pending_)
      .WriteData(mirroring_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper080::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&prg_banks_)
      .ReadData(&chr_banks_)
      .ReadData(&chr_mode_)
      .ReadData(&ram_permissions_)
      .ReadData(&x1005_ram_permission_)
      .ReadData(&nametable_pages_)
      .ReadData(&irq_latch_)
      .ReadData(&irq_control_)
      .ReadData(&irq_counter_)
      .ReadData(&irq_pending_)
      .ReadData(&mirroring_)
      .ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

void Mapper080::ConfigureInternalRAM() {
  const size_t size =
      variant_ == Variant::kX1017 ? kX1017RAMSize : kX1005RAMSize;
  if (rom_data()->has_battery) {
    rom_data()->prg_ram_size = 0;
    rom_data()->prg_nvram_size = size;
  } else {
    rom_data()->prg_ram_size = size;
    rom_data()->prg_nvram_size = 0;
  }
}

void Mapper080::WriteX1005Register(Address address, Byte value) {
  switch (address) {
    case 0x7ef0:
    case 0x7ef1: {
      const size_t index = address - 0x7ef0;
      chr_banks_[index] = value;
      if (variant_ == Variant::kX1005Mapper207) {
        const Byte page = value >> 7;
        if (nametable_pages_[index] != page) {
          nametable_pages_[index] = page;
          if (mirroring_changed_callback())
            mirroring_changed_callback().Run();
        }
      }
      break;
    }
    case 0x7ef2:
    case 0x7ef3:
    case 0x7ef4:
    case 0x7ef5:
      chr_banks_[address - 0x7ef0] = value;
      break;
    case 0x7ef6:
      if (variant_ == Variant::kX1005) {
        SetMirroring(value & 0x01 ? NametableMirroring::kHorizontal
                                  : NametableMirroring::kVertical);
      }
      break;
    case 0x7ef8:
    case 0x7ef9:
      x1005_ram_permission_ = value;
      break;
    case 0x7efa:
    case 0x7efb:
      prg_banks_[0] = value & 0x3f;
      break;
    case 0x7efc:
    case 0x7efd:
      prg_banks_[1] = value & 0x3f;
      break;
    case 0x7efe:
    case 0x7eff:
      prg_banks_[2] = value & 0x3f;
      break;
  }
}

void Mapper080::WriteX1017Register(Address address, Byte value) {
  if (address <= 0x7ef5) {
    chr_banks_[address - 0x7ef0] = value;
    return;
  }

  switch (address) {
    case 0x7ef6:
      chr_mode_ = (value >> 1) & 0x01;
      SetMirroring(value & 0x01 ? NametableMirroring::kVertical
                                : NametableMirroring::kHorizontal);
      break;
    case 0x7ef7:
    case 0x7ef8:
    case 0x7ef9:
      ram_permissions_[address - 0x7ef7] = value;
      break;
    case 0x7efa:
    case 0x7efb:
    case 0x7efc:
      prg_banks_[address - 0x7efa] = (value >> 2) & 0x0f;
      break;
    case 0x7efd:
      irq_latch_ = value;
      break;
    case 0x7efe: {
      const bool was_assert_enabled = (irq_control_ & 0x02) != 0;
      irq_control_ = value & 0x07;
      if ((irq_control_ & 0x01) == 0)
        ReloadIRQCounter(false);
      if ((irq_control_ & 0x02) == 0) {
        irq_pending_ = false;
      } else if (!was_assert_enabled && irq_counter_ == 0 &&
                 (irq_control_ & 0x05) == 0x01) {
        irq_pending_ = true;
        if (irq_callback())
          irq_callback().Run();
      }
      break;
    }
    case 0x7eff:
      irq_pending_ = false;
      ReloadIRQCounter(true);
      break;
  }
}

size_t Mapper080::GetCHRBank(Address address) const {
  const size_t window = (address & 0x1fff) / kCHRBankSize;
  Byte bank = 0;
  if (variant_ == Variant::kX1017 && chr_mode_) {
    if (window < 4) {
      bank = chr_banks_[window + 2];
    } else if (window < 6) {
      bank = static_cast<Byte>((chr_banks_[0] & 0xfe) + (window - 4));
    } else {
      bank = static_cast<Byte>((chr_banks_[1] & 0xfe) + (window - 6));
    }
  } else if (window < 2) {
    bank = static_cast<Byte>((chr_banks_[0] & 0xfe) + window);
  } else if (window < 4) {
    bank = static_cast<Byte>((chr_banks_[1] & 0xfe) + (window - 2));
  } else {
    bank = chr_banks_[window - 2];
  }

  if (variant_ == Variant::kX1005Mapper207)
    bank &= 0x7f;
  return bank;
}

bool Mapper080::IsRAMAddressEnabled(Address address) const {
  if (variant_ != Variant::kX1017) {
    return address >= 0x7f00 && x1005_ram_permission_ == 0xa3;
  }
  if (address >= 0x6000 && address <= 0x67ff)
    return ram_permissions_[0] == 0xca;
  if (address >= 0x6800 && address <= 0x6fff)
    return ram_permissions_[1] == 0x69;
  if (address >= 0x7000 && address <= 0x73ff)
    return ram_permissions_[2] == 0x84;
  return false;
}

void Mapper080::SetMirroring(NametableMirroring mirroring) {
  if (mirroring_ == mirroring)
    return;
  mirroring_ = mirroring;
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
}

void Mapper080::ReloadIRQCounter(bool acknowledge) {
  if (irq_latch_ == 0) {
    irq_counter_ = acknowledge ? 1 : 17;
  } else {
    irq_counter_ = static_cast<uint16_t>(
        (static_cast<uint16_t>(irq_latch_) + (acknowledge ? 1 : 2)) * 16);
  }
}

}  // namespace nes
}  // namespace kiwi
