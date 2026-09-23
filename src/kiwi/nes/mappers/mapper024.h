// Copyright (C) 2026 Yisi Yu
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

#ifndef NES_MAPPERS_MAPPER024_H_
#define NES_MAPPERS_MAPPER024_H_

#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"
#include "third_party/nes_apu/Nes_Vrc6.h"

namespace kiwi {
namespace nes {

// Konami VRC6a/VRC6b.
// https://www.nesdev.org/wiki/VRC6
class Mapper024 : public Mapper {
 public:
  explicit Mapper024(Cartridge* cartridge);
  ~Mapper024() override;

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
  bool NeedsM2CycleIRQ() const override;
  void M2CycleIRQ() override;

  bool UsesCustomPPUMemoryMapping() const override;
  Byte ReadPPUMemoryByte(Byte* ciram, Address address) override;
  void WritePPUMemoryByte(Byte* ciram, Address address, Byte value) override;

  void SetExpansionAudioOutput(Blip_Buffer* output) override;
  void SetExpansionAudioVolume(float volume) override;
  void EndExpansionAudioFrame(int64_t cycles) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  Address TranslateAddress(Address address) const;
  size_t GetCHRAddress(Address address) const;
  Byte GetNametableBank(Address address) const;
  void SetIRQControl(Byte value);
  void AcknowledgeIRQ();

  bool is_vrc6b_ = false;
  Byte prg_bank_16k_ = 0;
  Byte prg_bank_8k_ = 0;
  Byte chr_banks_[8]{};
  Byte banking_mode_ = 0;
  Byte irq_reload_ = 0;
  Byte irq_counter_ = 0;
  int16_t irq_prescaler_ = 0;
  bool irq_enabled_ = false;
  bool irq_enabled_after_ack_ = false;
  bool irq_cycle_mode_ = false;
  bool prg_ram_enabled_ = false;
  size_t prg_bank_count_ = 0;
  size_t chr_bank_count_ = 0;
  Bytes character_ram_;
  Nes_Vrc6 audio_;
  int64_t audio_cycles_ = 0;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER024_H_
