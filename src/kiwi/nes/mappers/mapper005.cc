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
constexpr size_t k1KBank = 1 * 1024;
constexpr size_t k2KBank = 4 * 1024;
constexpr size_t k4KBank = 4 * 1024;
constexpr size_t k8KBank = 8 * 1024;
constexpr size_t k16KBank = 16 * 1024;
constexpr size_t k32KBank = 32 * 1024;

Mapper005::Mapper005(Cartridge* cartridge) : Mapper(cartridge) {
  banks_in_8k_ = rom_data()->PRG.size() / k8KBank;
  banks_in_16k_ = rom_data()->PRG.size() / k16KBank;
  ResetRegisters();
}

Mapper005::~Mapper005() = default;

void Mapper005::Reset() {
  ResetRegisters();
}

void Mapper005::ResetRegisters() {
  DCHECK_GT(banks_in_8k_, 0);
  current_pattern_is_background_ = true;
  current_pattern_is_8x16_sprite_ = false;
  current_dot_in_scanline_ = 0;

  chr_mode_ = 3;
  prg_mode_ = 3;
  prg_mode_pending_ = prg_mode_;

  // Registers for PRG bank switching
  reg_5113_ = 0;
  reg_5114_ = 0;
  reg_5115_ = 0;
  reg_5116_ = 0;
  rom_sel_ = false;
  last_prg_reg_ = 0;

  split_mode_ = split_scroll_ = split_bank_ = 0;
  split_data_address_ = split_fine_y_ = 0;

  // CHR registers
  memset(chr_regs_, 0, sizeof(chr_regs_));
  memset(nametable_sel_, 0, sizeof(nametable_sel_));
  fill_mode_tile_ = fill_mode_color_ = 0;

  // Internal VRAM
  internal_vram_.clear();
  internal_vram_.resize(1024);

  // Because no ExROM game is known to write PRG-RAM with one bank value and
  // then attempt to read back the same data with a different bank value,
  // emulating the PRG-RAM as 64K at all times can be used as a compatible
  // superset for all games.
  sram_config_ = SRAMConfiguration::kSuperset_64k;
  sram_.clear();
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

  sram_protect_[0] = 0;
  sram_protect_[1] = 0;
  graphic_mode_ = 0;
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

bool Mapper005::SplitIsOn() {
  return ((split_mode_ & 0x80) && graphic_mode_ <= 1);
}

bool Mapper005::InSplitRegion() {
  if (SplitIsOn()) {
    bool is_left_side = !(split_mode_ & 0x40);
    Byte threshold_tile = split_mode_ & 0x1f;
    if (is_left_side) {
      return (current_dot_in_scanline_ / 8) < threshold_tile;
    }

    return (current_dot_in_scanline_ / 8) >= threshold_tile;
  }
  return false;
}

std::pair<bool, Byte> Mapper005::GetBank(ControlledBankSize cbs, Byte data) {
  bool is_rom = (data >> 7);
  if (is_rom) {
    Byte index;
    switch (cbs) {
      case ControlledBankSize::k8k:
        index = (data & 0x7f) % banks_in_8k_;
        break;
      case ControlledBankSize::k16k:
        index = ((data & 0x7f) >> 1) % banks_in_16k_;
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
  if (rom_sel_ && last_prg_reg_ != i) {
    prg_mode_ = prg_mode_pending_;
    rom_sel_ = false;
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
      return rom_data()
          ->PRG[(k32KBank * (((reg_5117_ & 0x7f) >> 2)) + (address - 0x8000)) %
                k32BankCount];
    }
    case 1:
      // CPU $8000-$BFFF: 16 KB switchable PRG ROM/RAM bank
      // CPU $C000-$FFFF: 16 KB switchable PRG ROM bank
      if (address < 0xc000) {
        auto [is_rom, bank] = GetBank(ControlledBankSize::k16k, reg_5115_);
        return is_rom ? rom_data()->PRG[k16KBank * bank + (address - 0x8000)]
                      : sram_[k16KBank * bank + (address - 0x8000)];
      }
      return rom_data()
          ->PRG[k16KBank * ((reg_5117_ & 0x7f) >> 1) % (banks_in_16k_) +
                (address - 0xc000)];
    case 2:
      // CPU $8000-$BFFF: 16 KB switchable PRG ROM/RAM bank
      // CPU $C000-$DFFF: 8 KB switchable PRG ROM/RAM bank
      // CPU $E000-$FFFF: 8 KB switchable PRG ROM bank
      if (address < 0xc000) {
        auto [is_rom, bank] = GetBank(ControlledBankSize::k16k, reg_5115_);
        return is_rom ? rom_data()->PRG[k16KBank * bank + (address - 0x8000)]
                      : sram_[k16KBank * bank + (address - 0x8000)];
      }
      if (address < 0xe000) {
        auto [is_rom, bank] = GetBank(ControlledBankSize::k8k, reg_5116_);
        return is_rom ? rom_data()->PRG[k8KBank * bank + (address - 0xc000)]
                      : sram_[k8KBank * bank + (address - 0xc000)];
      }
      return rom_data()->PRG[k8KBank * ((reg_5117_ & 0x7f) % (banks_in_8k_)) +
                             (address - 0xe000)];
    case 3:
      // CPU $8000-$9FFF: 8 KB switchable PRG ROM/RAM bank
      // CPU $A000-$BFFF: 8 KB switchable PRG ROM/RAM bank
      // CPU $C000-$DFFF: 8 KB switchable PRG ROM/RAM bank
      // CPU $E000-$FFFF: 8 KB switchable PRG ROM bank
      if (address < 0xa000) {
        auto [is_rom, bank] = GetBank(ControlledBankSize::k8k, reg_5114_);
        return is_rom ? rom_data()->PRG[k8KBank * bank + (address - 0x8000)]
                      : sram_[k8KBank * bank + (address - 0x8000)];
      }
      if (address < 0xc000) {
        auto [is_rom, bank] = GetBank(ControlledBankSize::k8k, reg_5115_);
        return is_rom ? rom_data()->PRG[k8KBank * bank + (address - 0xa000)]
                      : sram_[k8KBank * bank + (address - 0xa000)];
      }
      if (address < 0xe000) {
        auto [is_rom, bank] = GetBank(ControlledBankSize::k8k, reg_5116_);
        return is_rom ? rom_data()->PRG[k8KBank * bank + (address - 0xc000)]
                      : sram_[k8KBank * bank + (address - 0xc000)];
      }
      return rom_data()->PRG[k8KBank * ((reg_5117_ & 0x7f) % (banks_in_8k_)) +
                             (address - 0xe000)];
    default:
      CHECK(false) << "Shouldn't be here";
  }
}

void Mapper005::WriteCHR(Address address, Byte value) {
  if (!(irq_status_ & 0x40)) {
    // Blanking
    internal_vram_[address & 0x3ff] = value;
  }
}

Byte Mapper005::ReadCHR(Address address) {
  // Split mode always uses 4K CHR bank by $5202
  if (InSplitRegion()) {
    // todo graphic mode ==1

    return rom_data()->CHR[k4KBank * split_bank_ + (address & 0xfff)];
  }

  switch (chr_mode_) {
    case 0:
      // PPU $0000-$1FFF: 8 KB switchable CHR bank
      if (!current_pattern_is_8x16_sprite_ || !current_pattern_is_background_) {
        return rom_data()->CHR[k8KBank * chr_regs_[7] + (address)];
      } else {
        return rom_data()->CHR[k8KBank * chr_regs_[0xb] + (address)];
      }
    case 1:
      // PPU $0000-$0FFF: 4 KB switchable CHR bank
      // PPU $1000-$1FFF: 4 KB switchable CHR bank
      if (!current_pattern_is_8x16_sprite_ || !current_pattern_is_background_) {
        return rom_data()->CHR[k4KBank * chr_regs_[(address >> 12) * 4 + 3] +
                               (address & 0xfff)];
      } else {
        return rom_data()->CHR[k4KBank * chr_regs_[0xb] + (address & 0xfff)];
      }
    case 2:
      // PPU $0000-$07FF: 2 KB switchable CHR bank
      // PPU $0800-$0FFF: 2 KB switchable CHR bank
      // PPU $1000-$17FF: 2 KB switchable CHR bank
      // PPU $1800-$1FFF: 2 KB switchable CHR bank
      if (!current_pattern_is_8x16_sprite_ || !current_pattern_is_background_) {
        return rom_data()->CHR[k2KBank * chr_regs_[(address >> 11) * 2 + 1] +
                               (address & 0x7ff)];
      } else {
        return rom_data()
            ->CHR[k2KBank * chr_regs_[((address & 0xfff) >> 11) * 2 + 9] +
                  (address & 0x7ff)];
      }
    case 3: {
      // PPU $0000-$03FF: 1 KB switchable CHR bank
      // PPU $0400-$07FF: 1 KB switchable CHR bank
      // PPU $0800-$0BFF: 1 KB switchable CHR bank
      // PPU $0C00-$0FFF: 1 KB switchable CHR bank
      // PPU $1000-$13FF: 1 KB switchable CHR bank
      // PPU $1400-$17FF: 1 KB switchable CHR bank
      // PPU $1800-$1BFF: 1 KB switchable CHR bank
      // PPU $1C00-$1FFF: 1 KB switchable CHR bank
      if (!current_pattern_is_8x16_sprite_ || !current_pattern_is_background_) {
        return rom_data()
            ->CHR[k1KBank * chr_regs_[address >> 10] + (address & 0x3ff)];
      } else {
        return rom_data()
            ->CHR[k1KBank * chr_regs_[((address & 0xfff) >> 10) + 8] +
                  (address & 0x3ff)];
      }
    }
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
    case 0x5105:
      // 7  bit  0
      //---- ----
      // DDCC BBAA
      // |||| ||||
      // |||| ||++- Select nametable at PPU $2000-$23FF
      // |||| ++--- Select nametable at PPU $2400-$27FF
      // ||++------ Select nametable at PPU $2800-$2BFF
      // ++-------- Select nametable at PPU $2C00-$2FFF
      nametable_sel_[0] = value & 0x03;
      nametable_sel_[1] = (value >> 2) & 0x03;
      nametable_sel_[2] = (value >> 4) & 0x03;
      nametable_sel_[3] = (value >> 6) & 0x03;
      break;
    case 0x5106:
      fill_mode_tile_ = value;
      break;
    case 0x5107:
      // 7  bit  0
      //---- ----
      // xxxx xxAA
      //       ||
      //       ++- Specify background palette index to use for fill-mode
      //       nametable
      fill_mode_color_ = value & 0x3;
      break;

    // Following are bank switching:
    case 0x5113:
      reg_5113_ = SelectSRAM(value);
      break;
    case 0x5114:
      rom_sel_ = true;
      reg_5114_ = value;
      break;
    case 0x5115:
      rom_sel_ = true;
      reg_5115_ = value;
      break;
    case 0x5116:
      rom_sel_ = true;
      reg_5116_ = value;
      break;
    case 0x5117:
      rom_sel_ = true;
      reg_5117_ = value;
      break;
    case 0x5120:
    case 0x5121:
    case 0x5122:
    case 0x5123:
    case 0x5124:
    case 0x5125:
    case 0x5126:
    case 0x5127:
    case 0x5128:
    case 0x5129:
    case 0x512a:
    case 0x512b:
      chr_regs_[address & 0xf] = value;
      break;

    case 0x5200:
      // 7  bit  0
      // ---- ----
      // ESxW WWWW
      // || | ||||
      // || +-++++- Specify vertical split threshold tile count
      // |+-------- Specify vertical split region screen side (0:left; 1:right)
      // +--------- Enable vertical split mode
      split_mode_ = value;
      break;
    case 0x5201:
      split_scroll_ = value;
      break;
    case 0x5202:
      split_bank_ = value & 0x7f;
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
    // todo
  } else if (address >= 0x5c00 && address < 0x6000) {
    if (graphic_mode_ == 2) {
      internal_vram_[address & 0x3ff] = value;
    } else {
      if (irq_status_ & 0x40) {
        // In frame, rendering
        internal_vram_[address & 0x3ff] = value;
      } else {
        // Not allowed, bus open
        internal_vram_[address & 0x3ff] = 0;
      }
    }
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

  if (address >= 0x5c00 && address < 0x6000) {
    if (graphic_mode_ >= 2) {
      return internal_vram_[address & 0x3ff];
    }
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

  if (irq_scanline_ == irq_line_ + 1) {
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

  // Split mode:
  // The MMC5 keeps track of the scanline count and adds this to the vertical
  // scrolling value in $5201 in order to know what nametable data to substitute
  // in the split region on each scanline.
  // Though the PPU continues requesting nametable data corresponding to its
  // normal vertical scrolling in the split region, the MMC5 is ignoring the
  // nametable address requested in this case and directly substituting its own
  // nametable data.
  bool is_left_side = !(split_mode_ & 0x40);
  Byte threshold_tile = split_mode_ & 0x1f;

  if (SplitIsOn()) {
    if (scanline == -1) {
      split_fine_y_ = split_scroll_ & 0x7;
      // This represents coarse Y scroll in data address
      if (is_left_side)
        split_data_address_ = (split_scroll_ & 0xf8) << 2;
      else
        split_data_address_ = ((split_scroll_ & 0xf8) << 2) | (threshold_tile);
    } else {
      // For each scanline, adjust fine y and coarse y
      if (split_fine_y_ == 7) {
        split_fine_y_ = 0;
        // Increase split_y_address_ or wrap
        if ((split_data_address_ & 0x03a0) == 0x03a0) {
          // Wrap to the top nametable byte
          split_data_address_ &= 0x001f;
        } else {
          // Increase coarse y
          split_data_address_ += 0x0020;
          split_data_address_ |= (threshold_tile);
        }
      } else {
        ++split_fine_y_;
      }
    }
  }
}

void Mapper005::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(chr_mode_)
      .WriteData(prg_mode_)
      .WriteData(prg_mode_pending_)
      .WriteData(rom_sel_)
      .WriteData(last_prg_reg_)
      .WriteData(reg_5113_)
      .WriteData(reg_5114_)
      .WriteData(reg_5115_)
      .WriteData(reg_5116_)
      .WriteData(reg_5117_)
      .WriteData(chr_regs_)
      .WriteData(nametable_sel_)
      .WriteData(fill_mode_tile_)
      .WriteData(fill_mode_color_)
      .WriteData(split_mode_)
      .WriteData(split_scroll_)
      .WriteData(split_bank_)
      .WriteData(split_fine_y_)
      .WriteData(split_data_address_)
      .WriteData(internal_vram_)
      .WriteData(sram_config_)
      .WriteData(sram_protect_)
      .WriteData(graphic_mode_)
      .WriteData(sram_)
      .WriteData(mul_)
      .WriteData(irq_line_)
      .WriteData(irq_enabled_)
      .WriteData(irq_status_)
      .WriteData(irq_scanline_)
      .WriteData(irq_clear_flag_);
}

bool Mapper005::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&chr_mode_)
      .ReadData(&prg_mode_)
      .ReadData(&prg_mode_pending_)
      .ReadData(&rom_sel_)
      .ReadData(&last_prg_reg_)
      .ReadData(&reg_5113_)
      .ReadData(&reg_5114_)
      .ReadData(&reg_5115_)
      .ReadData(&reg_5116_)
      .ReadData(&reg_5117_)
      .ReadData(&chr_regs_)
      .ReadData(&nametable_sel_)
      .ReadData(&fill_mode_tile_)
      .ReadData(&fill_mode_color_)
      .ReadData(&split_mode_)
      .ReadData(&split_scroll_)
      .ReadData(&split_bank_)
      .ReadData(&split_fine_y_)
      .ReadData(&split_data_address_)
      .ReadData(&internal_vram_)
      .ReadData(&sram_config_)
      .ReadData(&sram_protect_)
      .ReadData(&graphic_mode_)
      .ReadData(&sram_)
      .ReadData(&mul_)
      .ReadData(&irq_line_)
      .ReadData(&irq_enabled_)
      .ReadData(&irq_status_)
      .ReadData(&irq_scanline_)
      .ReadData(&irq_clear_flag_);
  return true;
}

bool Mapper005::IsMMC5() {
  return true;
}

Byte Mapper005::ReadNametableByte(Byte* ram, Address address) {
  if (InSplitRegion()) {
    // Ignoring PPU's data address, scrolling registers because this mapper
    // has its own
    bool is_fetching_attribute = (address & 0x3ff) >= 0x3c0;
    if (!is_fetching_attribute) {
      return internal_vram_[address & 0x3ff];
    }

    return internal_vram_[address & 0x3ff];
  }

  Address nt_address = address & 0x3ff;
  Byte nt_reg_index = (address & 0x0f00) >> 10;
  switch (nametable_sel_[nt_reg_index]) {
    case 0:  // 0 - CIRAM page 0
      return ram[nt_address];
    case 1:  // 1 - CIRAM page 1
      return ram[0x400 + nt_address];
    case 2:  // 2 - Internal extended RAM
      DCHECK(address >= 0x2000 && address < 0x3000);
      if (graphic_mode_ >= 2)
        return 0;
      return internal_vram_[address & 0x3ff];
    case 3: {  // 3 - Fill-mode data
      // Is nametable or attribute table
      bool is_nametable = nt_address < 0x3c0;
      if (is_nametable)
        return fill_mode_tile_;

      if (graphic_mode_ != 1)
        return fill_mode_color_;

      // todo Extended attributes

      break;
    }
    default:
      CHECK(false) << "Shouldn't be here";
  }

  return 0;
}

void Mapper005::WriteNametableByte(Byte* ram, Address address, Byte value) {
  Byte nt_reg_index = (address & 0x0f00) >> 10;
  Address nt_address = address & 0x3ff;
  switch (nametable_sel_[nt_reg_index]) {
    case 0:  // 0 - CIRAM page 0
      ram[nt_address] = value;
      break;
    case 1:  // 1 - CIRAM page 1
      ram[0x400 + nt_address] = value;
      break;
    case 2:  // 2 - Internal extended RAM
      DCHECK(address >= 0x2000 && address < 0x3000);
      if (graphic_mode_ >= 2)
        return;

      if (!(irq_status_ & 0x40)) {
        // Blanking
        internal_vram_[address & 0x3ff] = value;
      }
      break;
    case 3:  // 3 - Fill-mode data
      break;
    default:
      CHECK(false) << "Shouldn't be here";
  }
}

void Mapper005::SetCurrentRenderState(bool is_background,
                                      bool is_8x16_sprite,
                                      int current_dot_in_scanline) {
  current_pattern_is_background_ = is_background;
  current_pattern_is_8x16_sprite_ = is_8x16_sprite;
  current_dot_in_scanline_ = current_dot_in_scanline;

  split_data_address_ =
      (split_data_address_ & 0xffe0) | ((current_dot_in_scanline_ / 8) & 0x1f);
}

Byte Mapper005::GetFineXInSplitRegion(Byte ppu_x_fine) {
  if (InSplitRegion())
    return current_dot_in_scanline_ % 8;

  return ppu_x_fine;
}

Address Mapper005::GetDataAddressInSplitRegion(Address ppu_data_address) {
  if (InSplitRegion())
    return ((split_data_address_ & 0xfff) | (split_fine_y_ << 12));

  return ppu_data_address;
}

}  // namespace nes
}  // namespace kiwi
