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

#ifndef NES_MAPPERS_MAPPER005_H_
#define NES_MAPPERS_MAPPER005_H_

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

#include <memory>
#include <utility>

namespace kiwi {
namespace nes {
// https://www.nesdev.org/wiki/INES_Mapper_005
class Mapper005 : public Mapper {
 public:
  explicit Mapper005(Cartridge* cartridge);
  ~Mapper005() override;

 public:
  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;

  void WriteExtendedRAM(Address address, Byte value) override;
  Byte ReadExtendedRAM(Address address) override;

  void ScanlineIRQ(int scanline, bool render_enabled) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

  // MMC5
  bool IsMMC5() override;
  Byte ReadNametableByte(Byte* ram, Address address) override;
  void WriteNametableByte(Byte* ram, Address address, Byte value) override;
  void SetCurrentRenderState(bool is_background,
                             bool is_8x16_sprite,
                             int current_dot_in_scanline) override;
  Byte GetFineXInSplitRegion(Byte ppu_x_fine) override;
  Address GetDataAddressInSplitRegion(Address ppu_data_address) override;

 private:
  void ResetRegisters();
  Byte SelectSRAM(Byte data);
  bool SplitIsOn();
  bool InSplitRegion();

  // Gets bank index from $5113-$5117's data.
  enum class ControlledBankSize {
    k8k,
    k16k,
  };
  std::pair<bool, Byte> GetBank(ControlledBankSize cbs, Byte data);
  void PRGBankSwitch(Address address);

 private:
  // These variables should not be serialized or deserialized.
  int banks_in_8k_;
  int banks_in_16k_;
  // Whether is fetching a background tile
  bool current_pattern_is_background_ = true;
  // Whether is fetching a sprite tile
  bool current_pattern_is_8x16_sprite_ = false;
  int current_dot_in_scanline_ = 0;

  // These variables should store their states
  Byte chr_mode_;
  Byte prg_mode_;
  Byte prg_mode_pending_;

  // Bank won't switch immediately when $5113-$5117 is written.
  // It will have at least one instruction to run, then switch.
  // For example:
  // 1. In Castlevania III - Dracula's Curse (USA):
  // $E2DA:A9 9E     LDA #$9E           ; A = $02
  // $E2DC:8D 16 51  STA $5116 = #$9E   ; Set value $02 to $5116
  // $E2DF:60        RTS                ; Switch bank after this instruction
  //
  // 2. In mmc5test.nes has following instructions:
  // $FFED:A9 00     LDA #$00
  // $FFEF:8D 00 51  STA $5100 = #$00
  // $FFF2:A9 10     LDA #$10
  // $FFF4:8D 17 51  STA $5117 = #$10
  // $FFF7:4C 00 80  JMP $8000         ; Switch bank after these instructions
  // Stores the last regs from $5133-$5117. If it has changed, switch the bank.
  bool rom_sel_;
  Byte last_prg_reg_;

  // Registers for PRG bank switching
  Byte reg_5113_;
  Byte reg_5114_;
  Byte reg_5115_;
  Byte reg_5116_;
  Byte reg_5117_;

  // CHR
  Byte chr_regs_[0xc];
  Byte nametable_sel_[4];
  Byte fill_mode_tile_;
  Byte fill_mode_color_;

  Byte split_mode_;
  Byte split_scroll_;
  Byte split_bank_;
  Byte split_fine_y_;           // Fine y for current frame
  Address split_data_address_;  // Split tile address for current frame

  // Extended VRAM
  Bytes internal_vram_;

  // MMC5 has its own SRAM
  enum class SRAMConfiguration {
    kEKROM_8K,      // 8K
    kETROM_16K,     // 2x8K
    kEWROM_32k,     // 32K
    kSuperset_64k,  // 2x32K
  };
  SRAMConfiguration sram_config_;

  // SRAM and multiplier
  Byte sram_protect_[2]{0};
  Byte graphic_mode_ = 0;
  Bytes sram_;
  Byte mul_[2]{0};

  // IRQ
  Byte irq_line_;
  bool irq_enabled_;
  Byte irq_status_;
  int irq_scanline_;
  int irq_clear_flag_;
};
}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER005_H_
