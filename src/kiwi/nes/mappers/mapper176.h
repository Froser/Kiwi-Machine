// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER176_H_
#define NES_MAPPERS_MAPPER176_H_

#include <array>
#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// FK23C/FS005 enhanced MMC3 family.
// https://www.nesdev.org/wiki/INES_Mapper_176
class Mapper176 : public Mapper {
 public:
  explicit Mapper176(Cartridge* cartridge);
  ~Mapper176() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  void WriteExtendedRAM(Address address, Byte value) override;
  Byte ReadExtendedRAM(Address address) override;
  Byte* GetExtendedRAMPointer() override;

  NametableMirroring GetNametableMirroring() override;
  void ScanlineIRQ(int scanline, bool render_enabled) override;
  void PPUAddressChanged(Address address) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  void WriteOuterRegister(Address address, Byte value);
  void UpdateMirroring();
  void ClockIRQCounter();
  size_t GetPRGBank(Address address) const;
  size_t GetCHRBank(Address address) const;
  size_t GetCHRAddress(Address address) const;
  size_t GetWorkRAMAddress(Address address) const;
  bool UsesCHRRAM(size_t bank) const;

  std::array<Byte, 12> bank_registers_{};
  Byte prg_banking_mode_ = 0;
  bool outer_chr_bank_size_ = false;
  bool select_chr_ram_ = false;
  bool mmc3_chr_mode_ = true;
  bool cnrom_chr_mode_ = false;
  uint16_t prg_base_bits_ = 0;
  Byte chr_base_bits_ = 0;
  bool extended_mmc3_mode_ = false;

  Byte wram_bank_select_ = 0;
  bool ram_in_first_chr_bank_ = false;
  bool allow_one_screen_mirroring_ = false;
  bool outer_registers_enabled_ = false;
  bool wram_config_enabled_ = false;
  bool wram_enabled_ = false;
  bool wram_write_protected_ = false;

  bool invert_prg_a14_ = false;
  bool invert_chr_a12_ = false;
  Byte selected_register_ = 0;
  Byte cnrom_chr_register_ = 0;
  Byte mirroring_register_ = 0;
  NametableMirroring mirroring_ = NametableMirroring::kVertical;

  Address last_vram_address_ = 0;
  Byte irq_reload_value_ = 0;
  Byte irq_counter_ = 0;
  bool irq_reload_pending_ = false;
  bool irq_enabled_ = false;

  size_t prg_bank_count_ = 0;
  size_t chr_rom_bank_count_ = 0;
  size_t chr_ram_bank_count_ = 0;
  size_t work_ram_bank_count_ = 0;
  Bytes character_ram_;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER176_H_
