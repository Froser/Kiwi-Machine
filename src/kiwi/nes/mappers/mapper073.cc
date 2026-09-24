// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper073.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kWorkRAMSize = 0x2000;
constexpr size_t kCHRRAMSize = 0x2000;

}  // namespace

Mapper073::Mapper073(Cartridge* cartridge)
    : Mapper(cartridge), character_ram_(kCHRRAMSize) {
  DCHECK_EQ(rom_data()->mapper, 73);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 2 * kPRGBankSize);

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  EnsurePRGRAM(kWorkRAMSize);
  ResetRegisters();
}

Mapper073::~Mapper073() = default;

void Mapper073::Reset() {
  ResetRegisters();
}

void Mapper073::WritePRG(Address address, Byte value) {
  const uint16_t shift = static_cast<uint16_t>((address >> 10) & 0x0c);
  switch (address & 0xf000) {
    case 0x8000:
    case 0x9000:
    case 0xa000:
    case 0xb000:
      irq_latch_ = static_cast<uint16_t>(
          (irq_latch_ & ~(static_cast<uint16_t>(0x000f) << shift)) |
          ((value & 0x0f) << shift));
      break;
    case 0xc000:
      irq_enable_after_ack_ = (value & 0x01) != 0;
      irq_enabled_ = (value & 0x02) != 0;
      irq_8_bit_mode_ = (value & 0x04) != 0;
      if (irq_enabled_)
        irq_counter_ = irq_latch_;
      break;
    case 0xd000:
      irq_enabled_ = irq_enable_after_ack_;
      break;
    case 0xf000:
      prg_bank_ = value & 0x07;
      break;
  }
}

Byte Mapper073::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank =
      address < 0xc000 ? prg_bank_ : static_cast<Byte>(prg_bank_count_ - 1);
  const size_t index =
      (bank % prg_bank_count_) * kPRGBankSize + (address & 0x3fff);
  return rom_data()->PRG[index];
}

void Mapper073::WriteCHR(Address address, Byte value) {
  character_ram_[address & 0x1fff] = value;
}

Byte Mapper073::ReadCHR(Address address) {
  return character_ram_[address & 0x1fff];
}

uint32_t Mapper073::GetAbsoluteCHRAddress(Address address) {
  return address & 0x1fff;
}

bool Mapper073::NeedsM2CycleIRQ() const {
  return true;
}

void Mapper073::M2CycleIRQ() {
  if (!irq_enabled_)
    return;

  if (irq_8_bit_mode_) {
    const Byte counter = static_cast<Byte>(irq_counter_);
    if (counter == 0xff) {
      irq_counter_ = static_cast<uint16_t>((irq_counter_ & 0xff00) |
                                           (irq_latch_ & 0x00ff));
      if (irq_callback())
        irq_callback().Run();
    } else {
      irq_counter_ = static_cast<uint16_t>((irq_counter_ & 0xff00) |
                                           static_cast<Byte>(counter + 1));
    }
    return;
  }

  if (irq_counter_ == 0xffff) {
    irq_counter_ = irq_latch_;
    if (irq_callback())
      irq_callback().Run();
  } else {
    ++irq_counter_;
  }
}

void Mapper073::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(prg_bank_)
      .WriteData(irq_latch_)
      .WriteData(irq_counter_)
      .WriteData(irq_enable_after_ack_)
      .WriteData(irq_enabled_)
      .WriteData(irq_8_bit_mode_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper073::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&prg_bank_)
      .ReadData(&irq_latch_)
      .ReadData(&irq_counter_)
      .ReadData(&irq_enable_after_ack_)
      .ReadData(&irq_enabled_)
      .ReadData(&irq_8_bit_mode_)
      .ReadData(&character_ram_);
  return Mapper::Deserialize(header, data);
}

void Mapper073::ResetRegisters() {
  prg_bank_ = 0;
  irq_latch_ = 0;
  irq_counter_ = 0;
  irq_enable_after_ack_ = false;
  irq_enabled_ = false;
  irq_8_bit_mode_ = false;
}

}  // namespace nes
}  // namespace kiwi
