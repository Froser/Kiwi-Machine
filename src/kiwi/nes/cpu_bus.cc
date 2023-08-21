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

#include "nes/cpu_bus.h"

#include "base/logging.h"
#include "nes/mapper.h"
#include "nes/registers.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {
CPUBus::CPUBus() = default;
CPUBus::~CPUBus() = default;

void CPUBus::SetMapper(Mapper* mapper) {
  DCHECK(mapper);
  mapper_ = mapper;
}

Mapper* CPUBus::GetMapper() {
  return mapper_;
}

// Memory map: https://www.nesdev.org/wiki/CPU_memory_map
Byte CPUBus::Read(Address address) {
  if (address < 0x2000) {  // $0000-$1FFF
    return ram_[address & 0x7ff];
  } else if (address < 0x4000) {  // $2000-$3FFF
    // PPU registers, mirrored
    DCHECK(ppu_) << "PPU must be set.";
    return ppu_->Read(address & 0xe007);
  } else if (address < 0x4020) {  // $4000-$401F
    DCHECK(emulator_) << "Emulator must be set.";
    return emulator_->Read(address);
  } else if (address < 0x8000) {  // $4020-$7FFF, battery backed save / work RAM
    return mapper_->ReadExtendedRAM(address);
  } else {  // $8000-$FFFF, Usual ROM, commonly with Mapper Registers
    return mapper_->ReadPRG(address);
  }
}

void CPUBus::Write(Address address, Byte value) {
  if (address < 0x2000) {  // $0000-$1FFF
    ram_[address & 0x7ff] = value;
  } else if (address < 0x4000) {  // $2000-$3FFF
    // PPU registers, mirrored
    DCHECK(ppu_) << "PPU must be set.";
    ppu_->Write(address & 0xe007, value);
  } else if (address < 0x4020) {  // $4000-$401F
    DCHECK(emulator_) << "Emulator must be set.";
    emulator_->Write(address, value);
  } else if (address < 0x8000) {  // $4020-$7FFF, battery backed save / work RAM
    mapper_->WriteExtendedRAM(address, value);
  } else {  // $8000-$FFFF, Usual ROM, commonly with Mapper Registers
    mapper_->WritePRG(address, value);
  }
}

Byte* CPUBus::GetPagePointer(Byte page) {
  // The start of a page is $XX00.
  Address address = page << 8;
  if (address < 0x2000) {
    return &ram_[address & 0x7ff];
  } else if (address < 0x4020) {
    LOG(ERROR) << "Register address memory pointer access attempt.";
  } else if (address < 0x6000) {
    LOG(ERROR) << "Expansion ROM access attempted, which is unsupported.";
  } else if (address < 0x8000) {
    return mapper_->GetExtendedRAMPointer() + (address - 0x6000);
  } else {
    LOG(ERROR) << "Unexpected DMA request: " << Hex<16>{address} << " at page "
               << Hex<16>{page};
  }
  return nullptr;
}

void CPUBus::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(ram_);
}

bool CPUBus::Deserialize(const EmulatorStates::Header& header,
                         EmulatorStates::DeserializableStateData& data) {
  if (header.version == 1) {
    data.ReadData(&ram_);
    return true;
  }
  return false;
}
}  // namespace nes
}  // namespace kiwi
