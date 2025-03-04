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

 private:
  void ResetRegisters();
  Byte SelectSRAM(Byte data);

  // Gets bank index from $5113-$5117's data.
  enum class ControlledBankSize {
    k8k,
    k16k,
  };
  std::pair<bool, Byte> GetBank(ControlledBankSize cbs, Byte data);
  void PRGBankSwitch(Address address);

 private:
  // This variable should not be serialized or deserialized.
  int banks_in_8k_ = 0;

  // Castlevania III uses mode 2, which is similar to VRC6 PRG banking. All
  // other games use mode 3. The Koei games never write to this register,
  // apparently relying on the MMC5 defaulting to mode 3 at power on.
  Byte chr_mode_ = 3;
  Byte prg_mode_ = 3;
  Byte prg_mode_pending_ = prg_mode_;

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
  bool rom_seL_ = false;
  Byte last_prg_reg_ = 0;

  // Registers for PRG bank switching
  Byte reg_5113_ = 0;
  Byte reg_5114_ = 0;
  Byte reg_5115_ = 0;
  Byte reg_5116_ = 0;
  Byte reg_5117_ = 0;

  // MMC5 has its own SRAM
  enum class SRAMConfiguration {
    kEKROM_8K,      // 8K
    kETROM_16K,     // 2x8K
    kEWROM_32k,     // 32K
    kSuperset_64k,  // 2x32K
  };
  SRAMConfiguration sram_config_ = SRAMConfiguration::kEKROM_8K;

  Byte sram_protect_[2]{0};
  Byte graphic_mode_ = 0;
  Bytes sram_;
  Byte mul_[2]{0};

  // IRQ
  Byte irq_line_ = 0;
  bool irq_enabled_ = false;
  Byte irq_status_ = 0;
  int irq_scanline_ = 0;
  int irq_clear_flag_ = 0;
};
}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER005_H_
