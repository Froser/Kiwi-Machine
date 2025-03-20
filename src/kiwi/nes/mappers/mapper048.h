// Copyright (C) 2023 Yisi Yu
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

#ifndef NES_MAPPERS_MAPPER048_H_
#define NES_MAPPERS_MAPPER048_H_

#include "nes/mapper.h"
#include "nes/types.h"

#include <memory>

namespace kiwi {
namespace nes {
class Mapper048 : public Mapper {
 public:
  explicit Mapper048(Cartridge* cartridge);
  ~Mapper048() override;

 public:
  void ResetRegisters();
  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;

  NametableMirroring GetNametableMirroring() override;
  void ScanlineIRQ(int scanline, bool render_enabled) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  size_t prg_8k_bank_count_;
  size_t chr_1k_bank_count_;
  size_t chr_2k_bank_count_;

  Byte prg_regs_[2];
  Byte chr_regs_[6];
  NametableMirroring mirroring_;

  Byte irq_counter_;
  Byte irq_latch_;
  bool irq_enabled_;

 protected:
  enum class Type {
    kMapper33,
    kMapper48,
  };
  Type type_ = Type::kMapper48;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER048_H_
