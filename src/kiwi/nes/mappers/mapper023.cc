// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper023.h"

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

}  // namespace

Mapper023::Mapper023(Cartridge* cartridge) : Mapper(cartridge) {
#if BUILDFLAG(ENABLE_MAPPER_021)
  if (rom_data()->mapper == 21) {
    switch (rom_data()->submapper) {
      case 1:
        wiring_ = Wiring::kVRC4a;
        break;
      case 2:
        wiring_ = Wiring::kVRC4c;
        break;
      default:
        wiring_ = Wiring::kMapper021Heuristic;
        break;
    }
  } else
#endif
#if BUILDFLAG(ENABLE_MAPPER_022)
      if (rom_data()->mapper == 22) {
    wiring_ = Wiring::kVRC2a;
  } else
#endif
#if BUILDFLAG(ENABLE_MAPPER_025)
      if (rom_data()->mapper == 25) {
    switch (rom_data()->submapper) {
      case 1:
        wiring_ = Wiring::kVRC4b;
        break;
      case 2:
        wiring_ = Wiring::kVRC4d;
        break;
      case 3:
        wiring_ = Wiring::kVRC2c;
        break;
      default:
        wiring_ = Wiring::kMapper025Heuristic;
        break;
    }
  } else
#endif
  {
    DCHECK_EQ(rom_data()->mapper, 23);
    switch (rom_data()->submapper) {
      case 1:
        wiring_ = Wiring::kVRC4f;
        break;
      case 2:
        wiring_ = Wiring::kVRC4e;
        break;
      case 3:
        wiring_ = Wiring::kVRC2b;
        break;
      default:
        wiring_ = Wiring::kMapper023Heuristic;
        break;
    }
  }

  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 2 * kPRGBankSize);

  uses_character_ram_ = rom_data()->CHR.empty();
  if (uses_character_ram_) {
    character_ram_.resize(kCHRRAMSize);
    chr_bank_count_ = kCHRRAMSize / kCHRBankSize;
  } else {
    DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
    chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  }
  DCHECK_GT(chr_bank_count_, 0u);

  ResetRegisters();
}

Mapper023::~Mapper023() = default;

void Mapper023::Reset() {
  ResetRegisters();
}

void Mapper023::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  const Address translated = TranslateAddress(address) & 0xf00f;
  if (translated >= 0x8000 && translated <= 0x8006) {
    prg_bank_0_ = value & 0x1f;
  } else if ((translated >= 0x9000 && translated <= 0x9001) ||
             (!SupportsVRC4Features() && translated >= 0x9000 &&
              translated <= 0x9003)) {
    SetMirroring(value);
  } else if (SupportsVRC4Features() && translated >= 0x9002 &&
             translated <= 0x9003) {
    prg_mode_ = (value & 0x02) != 0;
  } else if (translated >= 0xa000 && translated <= 0xa006) {
    prg_bank_1_ = value & 0x1f;
  } else if (translated >= 0xb000 && translated <= 0xe006) {
    const size_t bank =
        ((((translated >> 12) & 0x07) - 3) << 1) | ((translated >> 1) & 0x01);
    if (translated & 0x01) {
      chr_high_[bank] = value & (UsesVRC2Latch() ? 0x0f : 0x1f);
    } else {
      chr_low_[bank] = value & 0x0f;
    }
  } else if (SupportsVRC4Features()) {
    switch (translated) {
      case 0xf000:
        irq_reload_ = static_cast<Byte>((irq_reload_ & 0xf0) | (value & 0x0f));
        break;
      case 0xf001:
        irq_reload_ =
            static_cast<Byte>((irq_reload_ & 0x0f) | ((value & 0x0f) << 4));
        break;
      case 0xf002:
        SetIRQControl(value);
        break;
      case 0xf003:
        AcknowledgeIRQ();
        break;
    }
  }
}

Byte Mapper023::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank_count = rom_data()->PRG.size() / kPRGBankSize;
  const size_t window = (address - 0x8000) / kPRGBankSize;

  Byte bank = 0;
  switch (window) {
    case 0:
      bank = prg_mode_ ? static_cast<Byte>(bank_count - 2) : prg_bank_0_;
      break;
    case 1:
      bank = prg_bank_1_;
      break;
    case 2:
      bank = prg_mode_ ? prg_bank_0_ : static_cast<Byte>(bank_count - 2);
      break;
    case 3:
      bank = static_cast<Byte>(bank_count - 1);
      break;
  }
  return ReadPRGBank(bank, address);
}

void Mapper023::WriteCHR(Address address, Byte value) {
  if (uses_character_ram_)
    character_ram_[address & 0x1fff] = value;
}

Byte Mapper023::ReadCHR(Address address) {
  if (uses_character_ram_)
    return character_ram_[address & 0x1fff];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper023::GetAbsoluteCHRAddress(Address address) {
  if (uses_character_ram_)
    return address & 0x1fff;

  const size_t window = (address >> 10) & 0x07;
  size_t bank = chr_low_[window] | (chr_high_[window] << 4);
#if BUILDFLAG(ENABLE_MAPPER_022)
  if (wiring_ == Wiring::kVRC2a)
    bank >>= 1;
#endif
  bank %= chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x03ff));
}

void Mapper023::WriteExtendedRAM(Address address, Byte value) {
  if (UsesVRC2Latch() && !HasPRGRAM() && address >= 0x6000 &&
      address <= 0x6fff) {
    latch_ = value & 0x01;
    return;
  }
  Mapper::WriteExtendedRAM(address, value);
}

Byte Mapper023::ReadExtendedRAM(Address address) {
  if (UsesVRC2Latch() && !HasPRGRAM() && address >= 0x6000 &&
      address <= 0x6fff) {
    Byte latch = latch_;
#if BUILDFLAG(ENABLE_MAPPER_022)
    if (wiring_ == Wiring::kVRC2a)
      latch = 0;
#endif
    return static_cast<Byte>(latch | ((address >> 8) & 0xfe));
  }
  return Mapper::ReadExtendedRAM(address);
}

NametableMirroring Mapper023::GetNametableMirroring() {
  return mirroring_;
}

bool Mapper023::NeedsM2CycleIRQ() const {
  return SupportsVRC4Features();
}

void Mapper023::M2CycleIRQ() {
  if (!irq_enabled_ || !SupportsVRC4Features())
    return;

  irq_prescaler_ -= 3;
  if (!irq_cycle_mode_ && irq_prescaler_ > 0)
    return;

  if (irq_counter_ == 0xff) {
    irq_counter_ = irq_reload_;
    if (irq_callback())
      irq_callback().Run();
  } else {
    ++irq_counter_;
  }
  irq_prescaler_ += 341;
}

void Mapper023::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(prg_bank_0_)
      .WriteData(prg_bank_1_)
      .WriteData(prg_mode_)
      .WriteData(chr_low_)
      .WriteData(chr_high_)
      .WriteData(latch_)
      .WriteData(irq_reload_)
      .WriteData(irq_counter_)
      .WriteData(irq_prescaler_)
      .WriteData(irq_enabled_)
      .WriteData(irq_enabled_after_ack_)
      .WriteData(irq_cycle_mode_)
      .WriteData(mirroring_);
  if (uses_character_ram_)
    data.WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper023::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&prg_bank_0_)
      .ReadData(&prg_bank_1_)
      .ReadData(&prg_mode_)
      .ReadData(&chr_low_)
      .ReadData(&chr_high_)
      .ReadData(&latch_)
      .ReadData(&irq_reload_)
      .ReadData(&irq_counter_)
      .ReadData(&irq_prescaler_)
      .ReadData(&irq_enabled_)
      .ReadData(&irq_enabled_after_ack_)
      .ReadData(&irq_cycle_mode_)
      .ReadData(&mirroring_);
  if (uses_character_ram_)
    data.ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

void Mapper023::ResetRegisters() {
  prg_bank_0_ = 0;
  prg_bank_1_ = 0;
  prg_mode_ = false;
  std::fill(std::begin(chr_low_), std::end(chr_low_), 0);
  std::fill(std::begin(chr_high_), std::end(chr_high_), 0);
  latch_ = 0;
  irq_reload_ = 0;
  irq_counter_ = 0;
  irq_prescaler_ = 0;
  irq_enabled_ = false;
  irq_enabled_after_ack_ = false;
  irq_cycle_mode_ = false;
  mirroring_ = rom_data()->name_table_mirroring;
}

Address Mapper023::TranslateAddress(Address address) const {
  Byte a0 = 0;
  Byte a1 = 0;
  switch (wiring_) {
#if BUILDFLAG(ENABLE_MAPPER_021)
    case Wiring::kVRC4a:
      a0 = (address >> 1) & 0x01;
      a1 = (address >> 2) & 0x01;
      break;
    case Wiring::kVRC4c:
      a0 = (address >> 6) & 0x01;
      a1 = (address >> 7) & 0x01;
      break;
    case Wiring::kMapper021Heuristic:
      a0 = static_cast<Byte>(((address >> 1) & 0x01) | ((address >> 6) & 0x01));
      a1 = static_cast<Byte>(((address >> 2) & 0x01) | ((address >> 7) & 0x01));
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_022)
    case Wiring::kVRC2a:
      a0 = (address >> 1) & 0x01;
      a1 = address & 0x01;
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_025)
    case Wiring::kVRC4b:
    case Wiring::kVRC2c:
      a0 = (address >> 1) & 0x01;
      a1 = address & 0x01;
      break;
    case Wiring::kVRC4d:
      a0 = (address >> 3) & 0x01;
      a1 = (address >> 2) & 0x01;
      break;
#endif
    case Wiring::kVRC4e:
      a0 = (address >> 2) & 0x01;
      a1 = (address >> 3) & 0x01;
      break;
    case Wiring::kVRC4f:
    case Wiring::kVRC2b:
      a0 = address & 0x01;
      a1 = (address >> 1) & 0x01;
      break;
    case Wiring::kMapper023Heuristic:
      a0 = static_cast<Byte>((address & 0x01) | ((address >> 2) & 0x01));
      a1 = static_cast<Byte>(((address >> 1) & 0x01) | ((address >> 3) & 0x01));
      break;
#if BUILDFLAG(ENABLE_MAPPER_025)
    case Wiring::kMapper025Heuristic:
      a0 = static_cast<Byte>(((address >> 1) & 0x01) | ((address >> 3) & 0x01));
      a1 = static_cast<Byte>((address & 0x01) | ((address >> 2) & 0x01));
      break;
#endif
  }
  return static_cast<Address>((address & 0xff00) | (a1 << 1) | a0);
}

bool Mapper023::SupportsVRC4Features() const {
  return !UsesVRC2Latch();
}

bool Mapper023::UsesVRC2Latch() const {
#if BUILDFLAG(ENABLE_MAPPER_022)
  if (wiring_ == Wiring::kVRC2a)
    return true;
#endif
#if BUILDFLAG(ENABLE_MAPPER_025)
  return wiring_ == Wiring::kVRC2b || wiring_ == Wiring::kVRC2c;
#else
  return wiring_ == Wiring::kVRC2b;
#endif
}

Byte Mapper023::ReadPRGBank(Byte bank, Address address) {
  const size_t index =
      static_cast<size_t>(bank) * kPRGBankSize + (address & 0x1fff);
  return rom_data()->PRG[index % rom_data()->PRG.size()];
}

void Mapper023::SetMirroring(Byte value) {
  const Byte mask = UsesVRC2Latch() ? 0x01 : 0x03;
  NametableMirroring mirroring = NametableMirroring::kVertical;
  switch (value & mask) {
    case 0:
      mirroring = NametableMirroring::kVertical;
      break;
    case 1:
      mirroring = NametableMirroring::kHorizontal;
      break;
    case 2:
      mirroring = NametableMirroring::kOneScreenLower;
      break;
    case 3:
      mirroring = NametableMirroring::kOneScreenHigher;
      break;
  }
  if (mirroring_ != mirroring) {
    mirroring_ = mirroring;
    if (mirroring_changed_callback())
      mirroring_changed_callback().Run();
  }
}

void Mapper023::SetIRQControl(Byte value) {
  irq_enabled_after_ack_ = (value & 0x01) != 0;
  irq_enabled_ = (value & 0x02) != 0;
  irq_cycle_mode_ = (value & 0x04) != 0;
  if (irq_enabled_) {
    irq_counter_ = irq_reload_;
    irq_prescaler_ = 341;
  }
}

void Mapper023::AcknowledgeIRQ() {
  irq_enabled_ = irq_enabled_after_ack_;
}

}  // namespace nes
}  // namespace kiwi
