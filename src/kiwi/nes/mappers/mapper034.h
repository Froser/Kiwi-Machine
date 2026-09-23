// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER034_H_
#define NES_MAPPERS_MAPPER034_H_

#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// Irem BNROM and AVE NINA-001.
// https://www.nesdev.org/wiki/INES_Mapper_034
class Mapper034 : public Mapper {
 public:
  explicit Mapper034(Cartridge* cartridge);
  ~Mapper034() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  void WriteExtendedRAM(Address address, Byte value) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  enum class Board {
    kBNROM,
    kNINA001,
  };

  static constexpr size_t kCHRRAMSize = 0x2000;

  Board board_ = Board::kBNROM;
  Byte selected_prg_bank_ = 0;
  Byte selected_chr_banks_[2]{};
  size_t prg_bank_count_ = 0;
  size_t chr_bank_count_ = 0;
  Bytes character_ram_;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER034_H_
