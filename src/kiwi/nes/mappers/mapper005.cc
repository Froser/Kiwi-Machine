// Copyright (C) 2025 Yisi Yu
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

#include "nes/mappers/mapper005.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {

constexpr Byte kOpenBus = 8;
constexpr size_t k8KBank = 8 * 1024;
constexpr size_t k16KBank = 16 * 1024;
constexpr size_t k32KBank = 32 * 1024;

Mapper005::Mapper005(Cartridge* cartridge) : Mapper(cartridge) {
  banks_in_8k_ = rom_data()->PRG.size() / k8KBank;
  ResetRegisters();
}

Mapper005::~Mapper005() = default;

void Mapper005::Reset() {
  ResetRegisters();

  // CRC check
  switch (sram_config_) {
    case SRAMConfiguration::kEKROM_8K:
      sram_.resize(8 * 1024);
      break;
    case SRAMConfiguration::kETROM_16K:
      sram_.resize(16 * 1024);
      break;
    case SRAMConfiguration::kEWROM_32k:
      sram_.resize(32 * 1024);
      break;
    case SRAMConfiguration::kSuperset_64k:
      sram_.resize(64 * 1024);
      break;
  }
}

void Mapper005::ResetRegisters() {
  DCHECK_GT(banks_in_8k_, 0);

  chr_mode_ = 3;
  prg_mode_ = 3;
  prg_mode_pending_ = prg_mode_;

  // Registers for PRG bank switching
  reg_5113_ = 0;
  reg_5114_ = 0;
  reg_5115_ = 0;
  reg_5116_ = 0;
  rom_seL_ = false;
  last_prg_reg_ = 0;

  sram_config_ = SRAMConfiguration::kEKROM_8K;

  sram_protect_[0] = 0;
  sram_protect_[1] = 0;
  graphic_mode_ = 0;
  sram_.clear();
  mul_[0] = 0;
  mul_[1] = 0;

  // IRQ
  irq_line_ = 0;
  irq_enabled_ = false;
  irq_status_ = 0;
  irq_scanline_ = 0;
  irq_clear_flag_ = 0;

  reg_5117_ = banks_in_8k_ - 1;
}

Byte Mapper005::SelectSRAM(kiwi::nes::Byte data) {
  Byte v = data & 0x7;
  // See PRG-RAM configurations
  switch (sram_config_) {
    case SRAMConfiguration::kEKROM_8K:
      return (v <= 3) ? v : kOpenBus;
    case SRAMConfiguration::kETROM_16K:
      return (v <= 3) ? 0 : 1;
    case SRAMConfiguration::kEWROM_32k:
      return (v <= 3) ? v : kOpenBus;
    case SRAMConfiguration::kSuperset_64k:
      return v;
    default:
      CHECK(false) << "Shouldn't be here";
      return kOpenBus;
  }
}

std::pair<bool, Byte> Mapper005::GetBank(ControlledBankSize cbs, Byte data) {
  bool is_rom = (data >> 7);
  if (is_rom) {
    Byte index;
    switch (cbs) {
      case ControlledBankSize::k8k:
        index = data & 0x7f;
        break;
      case ControlledBankSize::k16k:
        index = (data & 0x7f) >> 1;
        break;
      default:
        CHECK(false) << "Shouldn't be here";
        break;
    }
    return std::make_pair(is_rom, index);
  }

  return std::make_pair(is_rom, SelectSRAM(data));
}

void Mapper005::PRGBankSwitch(kiwi::nes::Address address) {
  Byte i = (address >> 12) & 0xf;
  if (rom_seL_ && last_prg_reg_ != i) {
    prg_mode_ = prg_mode_pending_;
    rom_seL_ = false;
  }
  last_prg_reg_ = i;
}

void Mapper005::WritePRG(Address address, Byte value) {}

Byte Mapper005::ReadPRG(Address address) {
  PRGBankSwitch(address);

  // $6000-$7FFF handles in ReadExtendedRAM(), so we don't handle here.
  // CPU $6000-$7FFF: 8 KB switchable PRG RAM bank
  DCHECK(address >= 0x8000);
  switch (prg_mode_) {
    case 0: {
      // CPU $8000-$FFFF: 32 KB switchable PRG ROM bank
      const size_t k32BankCount = rom_data()->PRG.size() / k32KBank;
      return rom_data()->PRG.at(
          (k32KBank * (((reg_5117_ & 0x7f) >> 2)) + (address - 0x8000)) %
          k32BankCount);
    }
    case 1:
      // CPU $8000-$BFFF: 16 KB switchable PRG ROM/RAM bank
      // CPU $C000-$FFFF: 16 KB switchable PRG ROM bank
      if (address < 0xc000) {
        auto [is_rom, bank] = GetBank(ControlledBankSize::k16k, reg_5115_);
        return is_rom ? rom_data()->PRG.at(k16KBank * bank + (address - 0x8000))
                      : sram_.at(k16KBank * bank + (address - 0x8000));
      }
      return rom_data()
          ->PRG[k16KBank * ((reg_5117_ & 0x7f) >> 1) + (address - 0xc000)];
    case 2:
      // CPU $8000-$BFFF: 16 KB switchable PRG ROM/RAM bank
      // CPU $C000-$DFFF: 8 KB switchable PRG ROM/RAM bank
      // CPU $E000-$FFFF: 8 KB switchable PRG ROM bank
      if (address < 0xc000) {
        auto [is_rom, bank] = GetBank(ControlledBankSize::k16k, reg_5115_);
        return is_rom ? rom_data()->PRG.at(k16KBank * bank + (address - 0x8000))
                      : sram_.at(k16KBank * bank + (address - 0x8000));
      }
      if (address < 0xe000) {
        auto [is_rom, bank] = GetBank(ControlledBankSize::k8k, reg_5116_);
        return is_rom ? rom_data()->PRG.at(k8KBank * bank + (address - 0xc000))
                      : sram_.at(k8KBank * bank + (address - 0xc000));
      }
      return rom_data()->PRG.at(k8KBank * ((reg_5117_ & 0x7f)) +
                                (address - 0xe000));
    case 3:
      // CPU $8000-$9FFF: 8 KB switchable PRG ROM/RAM bank
      // CPU $A000-$BFFF: 8 KB switchable PRG ROM/RAM bank
      // CPU $C000-$DFFF: 8 KB switchable PRG ROM/RAM bank
      // CPU $E000-$FFFF: 8 KB switchable PRG ROM bank
      if (address < 0xa000) {
        auto [is_rom, bank] = GetBank(ControlledBankSize::k8k, reg_5114_);
        return is_rom ? rom_data()->PRG.at(k8KBank * bank + (address - 0x8000))
                      : sram_.at(k8KBank * bank + (address - 0x8000));
      }
      if (address < 0xc000) {
        auto [is_rom, bank] = GetBank(ControlledBankSize::k8k, reg_5115_);
        return is_rom ? rom_data()->PRG.at(k8KBank * bank + (address - 0xa000))
                      : sram_.at(k8KBank * bank + (address - 0xa000));
      }
      if (address < 0xe000) {
        auto [is_rom, bank] = GetBank(ControlledBankSize::k8k, reg_5116_);
        return is_rom ? rom_data()->PRG.at(k8KBank * bank + (address - 0xc000))
                      : sram_.at(k8KBank * bank + (address - 0xc000));
      }
      return rom_data()->PRG.at(k8KBank * ((reg_5117_ & 0x7f)) +
                                (address - 0xe000));
    default:
      CHECK(false) << "Shouldn't be here";
  }
}

void Mapper005::WriteCHR(Address address, Byte value) {}

Byte Mapper005::ReadCHR(Address address) {
  switch (chr_mode_) {
    case 0:
      // PPU $0000-$1FFF: 8 KB switchable CHR bank
      break;
    case 1:
      // PPU $0000-$0FFF: 4 KB switchable CHR bank
      // PPU $1000-$1FFF: 4 KB switchable CHR bank
    case 2:
      // PPU $0000-$07FF: 2 KB switchable CHR bank
      // PPU $0800-$0FFF: 2 KB switchable CHR bank
      // PPU $1000-$17FF: 2 KB switchable CHR bank
      // PPU $1800-$1FFF: 2 KB switchable CHR bank
    case 3:
      // PPU $0000-$03FF: 1 KB switchable CHR bank
      // PPU $0400-$07FF: 1 KB switchable CHR bank
      // PPU $0800-$0BFF: 1 KB switchable CHR bank
      // PPU $0C00-$0FFF: 1 KB switchable CHR bank
      // PPU $1000-$13FF: 1 KB switchable CHR bank
      // PPU $1400-$17FF: 1 KB switchable CHR bank
      // PPU $1800-$1BFF: 1 KB switchable CHR bank
      // PPU $1C00-$1FFF: 1 KB switchable CHR bank
      break;
    default:
      CHECK(false) << "Shouldn't be here";
  }
  return 0;
}

void Mapper005::WriteExtendedRAM(Address address, Byte value) {
  // Registers
  DCHECK(address < 0x8000);
  switch (address) {
    case 0x5100:
      // 7  bit  0
      //---- ----
      // xxxx xxPP
      //        ||
      //        ++- Select PRG banking mode
      prg_mode_pending_ = value & 0x3;
      break;
    case 0x5101:
      // 7  bit  0
      //---- ----
      // xxxx xxCC
      //        ||
      //        ++- Select CHR banking mode
      chr_mode_ = value & 0x3;
      break;
    case 0x5102:
    case 0x5103:
      // 7  bit  0 ($5102, $5103)
      // ---- ----
      // xxxx xxWW
      //       ||
      //       ++- RAM protect 1
      sram_protect_[address - 0x5102] = value & 0x3;
      break;
    case 0x5104:
      // 7  bit  0
      // ---- ----
      // xxxx xxXX
      //        ||
      //        ++- Specify extended RAM usage
      graphic_mode_ = value & 0x3;
      break;
      // Following are bank switching
    case 0x5113:
      reg_5113_ = SelectSRAM(value);
      break;
    case 0x5114:
      rom_seL_ = true;
      reg_5114_ = value;
      break;
    case 0x5115:
      rom_seL_ = true;
      reg_5115_ = value;
      break;
    case 0x5116:
      rom_seL_ = true;
      reg_5116_ = value;
      break;
    case 0x5117:
      rom_seL_ = true;
      reg_5117_ = value;
      break;
    case 0x5203:
      irq_line_ = value;
      break;
    case 0x5204:
      // 7  bit  0
      // ---- ----
      // Exxx xxxx
      // |
      // +--------- Scanline IRQ Enable flag (1=enabled)
      irq_enabled_ = value >> 7;
      irq_clear_callback().Run();
      break;
    case 0x5205:
      mul_[0] = value;
      break;
    case 0x5206:
      mul_[1] = value;
      break;
    default:
      break;
  }

  if (address >= 0x5000 && address <= 0x5015) {
    int i = 0;
    ++i;
    // APU HERE
  } else if (address >= 0x6000) {
    if (sram_protect_[0] == 0x02 && sram_protect_[1] == 0x01) {
      sram_[address - 0x6000] = value;
    }
  }
}

Byte Mapper005::ReadExtendedRAM(Address address) {
  DCHECK(address < 0x8000);
  switch (address) {
    case 0x5204: {
      // 7  bit  0
      // ---- ----
      // SVxx xxxx  MMC5A default power-on value = $00
      // ||
      // |+-------- "In Frame" flag
      // +--------- Scanline IRQ Pending flag
      Byte ret = irq_status_;
      irq_status_ &= ~0x80;
      irq_clear_callback().Run();
      return ret;
    }
    case 0x5205:
      return mul_[0] * mul_[1];
    case 0x5206:
      return static_cast<Byte>(
          (static_cast<Address>(mul_[0]) * static_cast<Address>(mul_[1])) >> 8);
    default:
      break;
  }
  if (address > 0x6000) {
    return sram_[k8KBank * reg_5113_ + (address - 0x6000)];
  }

  return 0;
}

// 7  bit  0
// ---- ----
// SVxx xxxx  MMC5A default power-on value = $00
// ||
// |+-------- "In Frame" flag
// +--------- Scanline IRQ Pending flag

// The Scanline IRQ Pending flag becomes set at any time that the internal
// scanline counter matches the value written to register $5203. If the
// scanline IRQ is enabled, it will also generate /IRQ to the system.

// The "In Frame" flag is set when the PPU is actively rendering visible
// scanlines and cleared when not rendering; for example, vertical blank
// scanlines. This flag does not clear for the short H-Blank period between
// visible scanlines. When pin 92 is driven low, this flag remains low at all
// times. Additionally, scanline IRQs no longer occur when pin 92 is driven
// low.
void Mapper005::ScanlineIRQ(int scanline, bool render_enabled) {
  if (render_enabled && scanline < 240) {
    irq_scanline_++;
    irq_status_ |= 0x40;
    irq_clear_flag_ = 0;
  }

  if (irq_scanline_ == irq_line_) {
    irq_status_ |= 0x80;
  }

  if (++irq_clear_flag_ > 2) {
    irq_scanline_ = 0;
    irq_status_ &= ~0x80;
    irq_status_ &= ~0x40;

    irq_clear_callback().Run();
  }

  if (irq_enabled_ && (irq_status_ & 0x80) && (irq_status_ & 0x40))
    irq_callback().Run();
}

void Mapper005::Serialize(EmulatorStates::SerializableStateData& data) {
  Mapper::Serialize(data);
}

bool Mapper005::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
