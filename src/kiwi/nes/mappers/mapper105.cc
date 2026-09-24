// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper105.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;

}  // namespace

Mapper105::Mapper105(Cartridge* cartridge) : Mapper(cartridge) {
  EnsurePRGRAM(0x2000);
  character_ram_.resize(kCHRRAMSize);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  DCHECK_EQ(prg_bank_count_, 16u);
}

Mapper105::~Mapper105() = default;

void Mapper105::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  // MMC1 ignores the second write of a read-modify-write sequence. A reset
  // write remains effective even when it follows another write immediately.
  if (wrote_prg_this_cycle_ && !(value & 0x80))
    return;
  wrote_prg_this_cycle_ = true;

  if (value & 0x80) {
    shift_register_ = 0;
    write_count_ = 0;
    control_ |= 0x0c;
    UpdateEventState();
    return;
  }

  shift_register_ =
      static_cast<Byte>((shift_register_ >> 1) | ((value & 1) << 4));
  if (++write_count_ == 5) {
    WriteRegister(address, shift_register_);
    shift_register_ = 0;
    write_count_ = 0;
  }
}

Byte Mapper105::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = ResolvePRGBank(address) % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

void Mapper105::WriteCHR(Address address, Byte value) {
  character_ram_[address & 0x1fff] = value;
}

Byte Mapper105::ReadCHR(Address address) {
  return character_ram_[address & 0x1fff];
}

void Mapper105::WriteExtendedRAM(Address address, Byte value) {
  if (wram_disabled_ && address >= 0x6000 && address <= 0x7fff)
    return;
  Mapper::WriteExtendedRAM(address, value);
}

Byte Mapper105::ReadExtendedRAM(Address address) {
  if (wram_disabled_ && address >= 0x6000 && address <= 0x7fff)
    return static_cast<Byte>(address >> 8);
  return Mapper::ReadExtendedRAM(address);
}

NametableMirroring Mapper105::GetNametableMirroring() {
  return mirroring_;
}

bool Mapper105::NeedsM2CycleIRQ() const {
  return true;
}

void Mapper105::M2CycleIRQ() {
  wrote_prg_this_cycle_ = false;
  if (!irq_enabled_)
    return;

  if (++irq_counter_ >= kIRQThreshold) {
    irq_enabled_ = false;
    if (irq_callback())
      irq_callback().Run();
  }
}

void Mapper105::WriteRegister(Address address, Byte value) {
  if (address <= 0x9fff) {
    control_ = value & 0x1f;
    const NametableMirroring old_mirroring = mirroring_;
    switch (control_ & 0x03) {
      case 0:
        mirroring_ = NametableMirroring::kOneScreenLower;
        break;
      case 1:
        mirroring_ = NametableMirroring::kOneScreenHigher;
        break;
      case 2:
        mirroring_ = NametableMirroring::kVertical;
        break;
      case 3:
        mirroring_ = NametableMirroring::kHorizontal;
        break;
    }
    if (mirroring_ != old_mirroring && mirroring_changed_callback())
      mirroring_changed_callback().Run();
  } else if (address <= 0xbfff) {
    chr_reg_0_ = value & 0x1f;
  } else if (address <= 0xdfff) {
    chr_reg_1_ = value & 0x1f;
  } else {
    prg_reg_ = value & 0x0f;
    wram_disabled_ = (value & 0x10) != 0;
  }

  UpdateEventState();
}

void Mapper105::UpdateEventState() {
  if (initialization_state_ == 0 && !(chr_reg_0_ & 0x10)) {
    initialization_state_ = 1;
  } else if (initialization_state_ == 1 && (chr_reg_0_ & 0x10)) {
    initialization_state_ = 2;
  }

  if (chr_reg_0_ & 0x10) {
    irq_enabled_ = false;
    irq_counter_ = 0;
  } else {
    irq_enabled_ = true;
  }
}

size_t Mapper105::ResolvePRGBank(Address address) const {
  const size_t slot = address >= 0xc000 ? 1 : 0;
  if (initialization_state_ != 2)
    return slot;

  if (!(chr_reg_0_ & 0x08))
    return (chr_reg_0_ & 0x06) + slot;

  switch ((control_ >> 2) & 0x03) {
    case 0:
    case 1:
      return 8 + (prg_reg_ & 0x06) + slot;
    case 2:
      return slot == 0 ? 8 : 8 + (prg_reg_ & 0x07);
    case 3:
      return slot == 0 ? 8 + (prg_reg_ & 0x07) : 15;
  }
  return 0;
}

void Mapper105::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(irq_counter_)
      .WriteData(character_ram_)
      .WriteData(shift_register_)
      .WriteData(write_count_)
      .WriteData(control_)
      .WriteData(chr_reg_0_)
      .WriteData(chr_reg_1_)
      .WriteData(prg_reg_)
      .WriteData(initialization_state_)
      .WriteData(wram_disabled_)
      .WriteData(irq_enabled_)
      .WriteData(mirroring_);
  Mapper::Serialize(data);
}

bool Mapper105::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&irq_counter_)
      .ReadData(&character_ram_)
      .ReadData(&shift_register_)
      .ReadData(&write_count_)
      .ReadData(&control_)
      .ReadData(&chr_reg_0_)
      .ReadData(&chr_reg_1_)
      .ReadData(&prg_reg_)
      .ReadData(&initialization_state_)
      .ReadData(&wram_disabled_)
      .ReadData(&irq_enabled_)
      .ReadData(&mirroring_);
  wrote_prg_this_cycle_ = false;
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
