// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper016.h"

#include <algorithm>
#include <iterator>

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kCHRBankSize = 0x0400;
constexpr size_t kCHRRAMSize = 0x2000;
constexpr size_t kSaveRAMSize = 0x2000;

}  // namespace

class BandaiEEPROM {
 public:
  enum class Type : Byte {
    k24C01,
    k24C02,
  };

  explicit BandaiEEPROM(Type type)
      : type_(type), memory_(type == Type::k24C01 ? 128 : 256) {}

  Byte Read() const { return output_; }

  bool Write(Byte scl, Byte sda) {
    return type_ == Type::k24C01 ? Write24C01(scl, sda) : Write24C02(scl, sda);
  }

  bool WriteSCL(Byte scl) { return Write(scl, previous_sda_); }
  bool WriteSDA(Byte sda) { return Write(previous_scl_, sda); }

  size_t size() const { return memory_.size(); }
  const Bytes& memory() const { return memory_; }

  bool Restore(const Byte* data, size_t size) {
    if (size != memory_.size())
      return false;
    std::copy(data, data + size, memory_.begin());
    return true;
  }

  void Serialize(EmulatorStates::SerializableStateData& data) {
    data.WriteData(memory_)
        .WriteData(mode_)
        .WriteData(next_mode_)
        .WriteData(chip_address_)
        .WriteData(address_)
        .WriteData(value_)
        .WriteData(counter_)
        .WriteData(output_)
        .WriteData(previous_scl_)
        .WriteData(previous_sda_);
  }

  void Deserialize(EmulatorStates::DeserializableStateData& data) {
    data.ReadData(&memory_)
        .ReadData(&mode_)
        .ReadData(&next_mode_)
        .ReadData(&chip_address_)
        .ReadData(&address_)
        .ReadData(&value_)
        .ReadData(&counter_)
        .ReadData(&output_)
        .ReadData(&previous_scl_)
        .ReadData(&previous_sda_);
  }

 private:
  enum class Mode : Byte {
    kIdle,
    kAddress,
    kRead,
    kWrite,
    kSendAck,
    kWaitAck,
    kChipAddress,
  };

  void WriteBitLSB(Byte& destination, Byte value) {
    if (counter_ >= 8)
      return;
    const Byte mask = static_cast<Byte>(~(1 << counter_));
    destination =
        static_cast<Byte>((destination & mask) | ((value & 0x01) << counter_));
    ++counter_;
  }

  void WriteBitMSB(Byte& destination, Byte value) {
    if (counter_ >= 8)
      return;
    const Byte shift = 7 - counter_;
    const Byte mask = static_cast<Byte>(~(1 << shift));
    destination =
        static_cast<Byte>((destination & mask) | ((value & 0x01) << shift));
    ++counter_;
  }

  void ReadBitLSB() {
    if (counter_ < 8) {
      output_ = (memory_[address_ & 0x7f] >> counter_) & 0x01;
      ++counter_;
    }
  }

  void ReadBitMSB() {
    if (counter_ < 8) {
      output_ = (memory_[address_] >> (7 - counter_)) & 0x01;
      ++counter_;
    }
  }

  bool Write24C01(Byte scl, Byte sda) {
    bool changed = false;
    if (previous_scl_ && scl && sda < previous_sda_) {
      mode_ = Mode::kAddress;
      address_ = 0;
      counter_ = 0;
      output_ = 1;
    } else if (previous_scl_ && scl && sda > previous_sda_) {
      mode_ = Mode::kIdle;
      output_ = 1;
    } else if (scl > previous_scl_) {
      switch (mode_) {
        case Mode::kAddress:
          if (counter_ < 7) {
            WriteBitLSB(address_, sda);
          } else if (counter_ == 7) {
            counter_ = 8;
            if (sda) {
              next_mode_ = Mode::kRead;
              value_ = memory_[address_ & 0x7f];
            } else {
              next_mode_ = Mode::kWrite;
            }
          }
          break;
        case Mode::kSendAck:
          output_ = 0;
          break;
        case Mode::kRead:
          ReadBitLSB();
          break;
        case Mode::kWrite:
          WriteBitLSB(value_, sda);
          break;
        case Mode::kWaitAck:
          if (!sda)
            next_mode_ = Mode::kIdle;
          break;
        default:
          break;
      }
    } else if (scl < previous_scl_) {
      switch (mode_) {
        case Mode::kAddress:
          if (counter_ == 8) {
            mode_ = Mode::kSendAck;
            output_ = 1;
          }
          break;
        case Mode::kSendAck:
          mode_ = next_mode_;
          counter_ = 0;
          output_ = 1;
          break;
        case Mode::kRead:
          if (counter_ == 8) {
            mode_ = Mode::kWaitAck;
            address_ = (address_ + 1) & 0x7f;
          }
          break;
        case Mode::kWrite:
          if (counter_ == 8) {
            mode_ = Mode::kSendAck;
            next_mode_ = Mode::kIdle;
            const size_t index = address_ & 0x7f;
            changed = memory_[index] != value_;
            memory_[index] = value_;
            address_ = (address_ + 1) & 0x7f;
          }
          break;
        default:
          break;
      }
    }
    previous_scl_ = scl;
    previous_sda_ = sda;
    return changed;
  }

  bool Write24C02(Byte scl, Byte sda) {
    bool changed = false;
    if (previous_scl_ && scl && sda < previous_sda_) {
      mode_ = Mode::kChipAddress;
      counter_ = 0;
      output_ = 1;
    } else if (previous_scl_ && scl && sda > previous_sda_) {
      mode_ = Mode::kIdle;
      output_ = 1;
    } else if (scl > previous_scl_) {
      switch (mode_) {
        case Mode::kChipAddress:
          WriteBitMSB(chip_address_, sda);
          break;
        case Mode::kAddress:
          WriteBitMSB(address_, sda);
          break;
        case Mode::kRead:
          ReadBitMSB();
          break;
        case Mode::kWrite:
          WriteBitMSB(value_, sda);
          break;
        case Mode::kSendAck:
          output_ = 0;
          break;
        case Mode::kWaitAck:
          if (!sda) {
            next_mode_ = Mode::kRead;
            value_ = memory_[address_];
          }
          break;
        default:
          break;
      }
    } else if (scl < previous_scl_) {
      switch (mode_) {
        case Mode::kChipAddress:
          if (counter_ == 8) {
            if ((chip_address_ & 0xa0) == 0xa0) {
              mode_ = Mode::kSendAck;
              counter_ = 0;
              output_ = 1;
              if (chip_address_ & 0x01) {
                next_mode_ = Mode::kRead;
                value_ = memory_[address_];
              } else {
                next_mode_ = Mode::kAddress;
              }
            } else {
              mode_ = Mode::kIdle;
              counter_ = 0;
              output_ = 1;
            }
          }
          break;
        case Mode::kAddress:
          if (counter_ == 8) {
            counter_ = 0;
            mode_ = Mode::kSendAck;
            next_mode_ = Mode::kWrite;
            output_ = 1;
          }
          break;
        case Mode::kRead:
          if (counter_ == 8) {
            mode_ = Mode::kWaitAck;
            ++address_;
          }
          break;
        case Mode::kWrite:
          if (counter_ == 8) {
            counter_ = 0;
            mode_ = Mode::kSendAck;
            next_mode_ = Mode::kWrite;
            changed = memory_[address_] != value_;
            memory_[address_] = value_;
            ++address_;
          }
          break;
        case Mode::kSendAck:
        case Mode::kWaitAck:
          mode_ = next_mode_;
          counter_ = 0;
          output_ = 1;
          break;
        default:
          break;
      }
    }
    previous_scl_ = scl;
    previous_sda_ = sda;
    return changed;
  }

  const Type type_;
  Bytes memory_;
  Mode mode_ = Mode::kIdle;
  Mode next_mode_ = Mode::kIdle;
  Byte chip_address_ = 0;
  Byte address_ = 0;
  Byte value_ = 0;
  Byte counter_ = 0;
  Byte output_ = 0;
  Byte previous_scl_ = 0;
  Byte previous_sda_ = 0;
};

Mapper016::Mapper016(Cartridge* cartridge) : Mapper(cartridge) {
  const Byte mapper = rom_data()->mapper;
  if (mapper == 153) {
    EnsurePRGRAM(kSaveRAMSize);
    Byte* ram = GetExtendedRAMPointer();
    DCHECK(ram);
    std::fill(ram, ram + kSaveRAMSize, 0xff);
  } else if (mapper == 157) {
    standard_eeprom_ =
        std::make_unique<BandaiEEPROM>(BandaiEEPROM::Type::k24C02);
    if (rom_data()->prg_nvram_size > 256) {
      extra_eeprom_ =
          std::make_unique<BandaiEEPROM>(BandaiEEPROM::Type::k24C01);
    }
  } else if (mapper == 159) {
    standard_eeprom_ =
        std::make_unique<BandaiEEPROM>(BandaiEEPROM::Type::k24C01);
  } else if (mapper == 16 && (rom_data()->submapper == 0 ||
                              (rom_data()->submapper == 5 &&
                               rom_data()->prg_nvram_size == 256))) {
    standard_eeprom_ =
        std::make_unique<BandaiEEPROM>(BandaiEEPROM::Type::k24C02);
  }

  if (standard_eeprom_) {
    rom_data()->prg_ram_size = 0;
    rom_data()->prg_nvram_size =
        standard_eeprom_->size() + (extra_eeprom_ ? extra_eeprom_->size() : 0);
    rom_data()->has_battery = true;
  }

  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), kPRGBankSize);

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

Mapper016::~Mapper016() = default;

void Mapper016::Reset() {
  ResetRegisters();
}

void Mapper016::WritePRG(Address address, Byte value) {
  if (CanWriteRegister(address))
    WriteRegister(address, value);
}

Byte Mapper016::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const Byte bank = address < 0xc000
                        ? static_cast<Byte>(prg_page_ | prg_bank_select_)
                        : static_cast<Byte>(0x0f | prg_bank_select_);
  const Byte value = ReadPRGBank(bank, address);
  return value;
}

void Mapper016::WriteCHR(Address address, Byte value) {
  if (uses_character_ram_)
    character_ram_[address & 0x1fff] = value;
}

Byte Mapper016::ReadCHR(Address address) {
  if (uses_character_ram_)
    return character_ram_[address & 0x1fff];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper016::GetAbsoluteCHRAddress(Address address) {
  if (uses_character_ram_)
    return address & 0x1fff;

  const size_t bank = chr_banks_[(address >> 10) & 0x07] % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x03ff));
}

void Mapper016::WriteExtendedRAM(Address address, Byte value) {
  if (rom_data()->mapper == 153) {
    if (ram_enabled_)
      Mapper::WriteExtendedRAM(address, value);
    return;
  }
  if (CanWriteRegister(address))
    WriteRegister(address, value);
}

Byte Mapper016::ReadExtendedRAM(Address address) {
  if (rom_data()->mapper == 153) {
    return ram_enabled_ ? Mapper::ReadExtendedRAM(address)
                        : static_cast<Byte>(address >> 8);
  }
  if (address >= 0x6000 && address <= 0x7fff)
    return ReadRegister(address);
  return Mapper::ReadExtendedRAM(address);
}

NametableMirroring Mapper016::GetNametableMirroring() {
  return mirroring_;
}

bool Mapper016::NeedsM2CycleIRQ() const {
  return true;
}

void Mapper016::M2CycleIRQ() {
  if (!irq_enabled_)
    return;

  if (irq_counter_ == 0 && irq_callback())
    irq_callback().Run();
  --irq_counter_;
}

bool Mapper016::UsesCustomPRGRAM() const {
  return standard_eeprom_ != nullptr;
}

void Mapper016::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(prg_page_)
      .WriteData(prg_bank_select_)
      .WriteData(chr_banks_)
      .WriteData(irq_counter_)
      .WriteData(irq_reload_)
      .WriteData(irq_enabled_)
      .WriteData(ram_enabled_)
      .WriteData(mirroring_);
  if (uses_character_ram_)
    data.WriteData(character_ram_);
  if (standard_eeprom_)
    standard_eeprom_->Serialize(data);
  if (extra_eeprom_)
    extra_eeprom_->Serialize(data);
  Mapper::Serialize(data);
}

bool Mapper016::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&prg_page_)
      .ReadData(&prg_bank_select_)
      .ReadData(&chr_banks_)
      .ReadData(&irq_counter_)
      .ReadData(&irq_reload_)
      .ReadData(&irq_enabled_)
      .ReadData(&ram_enabled_)
      .ReadData(&mirroring_);
  if (uses_character_ram_)
    data.ReadData(&character_ram_);
  if (standard_eeprom_)
    standard_eeprom_->Deserialize(data);
  if (extra_eeprom_)
    extra_eeprom_->Deserialize(data);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

Bytes Mapper016::CopyPRGNVRAM() {
  if (!standard_eeprom_)
    return Mapper::CopyPRGNVRAM();

  Bytes result = standard_eeprom_->memory();
  if (extra_eeprom_) {
    result.insert(result.end(), extra_eeprom_->memory().begin(),
                  extra_eeprom_->memory().end());
  }
  return result;
}

bool Mapper016::RestorePRGNVRAM(const Bytes& data) {
  if (!standard_eeprom_)
    return Mapper::RestorePRGNVRAM(data);
  if (data.size() != GetPRGNVRAMSize())
    return false;

  size_t offset = 0;
  if (!standard_eeprom_->Restore(data.data(), standard_eeprom_->size()))
    return false;
  offset += standard_eeprom_->size();
  return !extra_eeprom_ ||
         extra_eeprom_->Restore(data.data() + offset, extra_eeprom_->size());
}

void Mapper016::ResetRegisters() {
  prg_page_ = 0;
  prg_bank_select_ = 0;
  std::fill(std::begin(chr_banks_), std::end(chr_banks_), 0);
  irq_counter_ = 0;
  irq_reload_ = 0;
  irq_enabled_ = false;
  ram_enabled_ = false;
  mirroring_ = rom_data()->name_table_mirroring;
}

bool Mapper016::CanWriteRegister(Address address) {
  if (address < 0x6000)
    return false;
  if (rom_data()->mapper != 16)
    return address >= 0x8000;
  if (rom_data()->submapper == 4)
    return address < 0x8000;
  if (rom_data()->submapper == 5)
    return address >= 0x8000;
  return true;
}

void Mapper016::WriteRegister(Address address, Byte value) {
  const Byte command = address & 0x0f;

  if (command <= 0x07) {
    chr_banks_[command] = value;
    if (rom_data()->mapper == 153 ||
        rom_data()->PRG.size() / kPRGBankSize >= 0x20) {
      prg_bank_select_ = 0;
      for (Byte bank : chr_banks_)
        prg_bank_select_ |= (bank & 0x01) << 4;
    }

    if (extra_eeprom_ && rom_data()->mapper == 157 && command <= 3 &&
        extra_eeprom_->WriteSCL((value >> 3) & 0x01)) {
      MarkPRGNVRAMDirty();
    }
    return;
  }

  switch (command) {
    case 0x08:
      prg_page_ = value & 0x0f;
      break;
    case 0x09: {
      NametableMirroring mirroring = NametableMirroring::kVertical;
      switch (value & 0x03) {
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
      break;
    }
    case 0x0a:
      irq_enabled_ = (value & 0x01) != 0;
      if (rom_data()->mapper != 16 || rom_data()->submapper != 4)
        irq_counter_ = irq_reload_;
      break;
    case 0x0b:
      if (rom_data()->mapper == 16 && rom_data()->submapper == 4) {
        irq_counter_ = static_cast<uint16_t>((irq_counter_ & 0xff00) | value);
      } else {
        irq_reload_ = static_cast<uint16_t>((irq_reload_ & 0xff00) | value);
      }
      break;
    case 0x0c:
      if (rom_data()->mapper == 16 && rom_data()->submapper == 4) {
        irq_counter_ =
            static_cast<uint16_t>((irq_counter_ & 0x00ff) | (value << 8));
      } else {
        irq_reload_ =
            static_cast<uint16_t>((irq_reload_ & 0x00ff) | (value << 8));
      }
      break;
    case 0x0d:
      if (rom_data()->mapper == 153) {
        ram_enabled_ = (value & 0x20) != 0;
      } else {
        const Byte scl = (value >> 5) & 0x01;
        const Byte sda = (value >> 6) & 0x01;
        bool changed = false;
        if (standard_eeprom_)
          changed |= standard_eeprom_->Write(scl, sda);
        if (extra_eeprom_)
          changed |= extra_eeprom_->WriteSDA(sda);
        if (changed)
          MarkPRGNVRAMDirty();
      }
      break;
  }
}

Byte Mapper016::ReadRegister(Address address) const {
  Byte output = static_cast<Byte>((address >> 8) & 0xe7);
  if (standard_eeprom_) {
    Byte eeprom_output = standard_eeprom_->Read();
    if (extra_eeprom_)
      eeprom_output &= extra_eeprom_->Read();
    output |= eeprom_output << 4;
  }
  return output;
}

Byte Mapper016::ReadPRGBank(Byte bank, Address address) {
  const size_t index =
      static_cast<size_t>(bank) * kPRGBankSize + (address & 0x3fff);
  return rom_data()->PRG[index % rom_data()->PRG.size()];
}

}  // namespace nes
}  // namespace kiwi
