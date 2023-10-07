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

#include "nes/cartridge.h"

#include <memory>

#include "base/check.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "nes/emulator_impl.h"
#include "nes/mapper.h"
#include "nes/rom_data.h"
#include "nes/types.h"
#include "third_party/zlib-1.3/zlib.h"

namespace kiwi {
namespace nes {
Cartridge::Cartridge(EmulatorImpl* emulator) : emulator_(emulator) {}
Cartridge::~Cartridge() = default;

void Cartridge::Load(const base::FilePath& rom_path, LoadCallback callback) {
  if (emulator_->is_power_on()) {
    is_loaded_ = false;
    emulator_->io_task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&Cartridge::LoadFromFileOnIOThread,
                       base::RetainedRef(this), rom_path),
        base::BindOnce(std::move(callback)));
  } else {
    LOG(ERROR) << "The emulator is power off yet. You should call "
                  "Emulator::PowerOn() first.";
  }
}

void Cartridge::Load(const Bytes& data, LoadCallback callback) {
  if (emulator_->is_power_on()) {
    is_loaded_ = false;
    emulator_->io_task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&Cartridge::LoadFromDataOnIOThread,
                       base::RetainedRef(this), data),
        base::BindOnce(std::move(callback)));
  } else {
    LOG(ERROR) << "The emulator is power off yet. You should call "
                  "Emulator::PowerOn() first.";
  }
}

RomData* Cartridge::GetRomData() {
  DCHECK(emulator_->is_power_on() && is_loaded_);
  return rom_data_.get();
}

void Cartridge::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(crc_);
  mapper()->Serialize(data);
}

bool Cartridge::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  if (header.version == 1) {
    int32_t crc;
    data.ReadData(&crc);
    if (crc != crc_)
      return false;

    return mapper()->Deserialize(header, data);
  }

  return false;
}

Cartridge::LoadResult Cartridge::LoadFromFileOnIOThread(
    const base::FilePath& rom_path) {
  DCHECK(emulator_->is_power_on());
  DCHECK(emulator_->io_task_runner()->RunsTasksInCurrentSequence());

  DCHECK(!rom_data_);
  rom_data_ = std::make_unique<RomData>();

  base::File rom_file(rom_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!rom_file.IsValid()) {
    LOG(ERROR) << "Could not open ROM file from path: " << rom_path;
    return LoadResult::failed();
  }

  Bytes headers;
  LOG(INFO) << "Reading ROM from path: " << rom_path;

  // Header
  headers.resize(0x10);
  if (rom_file.ReadAtCurrentPos(reinterpret_cast<char*>(headers.data()),
                                0x10) == -1) {
    LOG(ERROR) << "Reading iNES header failed.";
    return LoadResult::failed();
  }
  rom_data_->raw_headers = headers;
  if (!ProcessHeaders(headers.data()))
    return LoadResult::failed();

  uLong crc = crc32_z(0L, Z_NULL, 0);
  // PRG-ROM 16KB banks
  Byte prg_banks = headers[4];
  rom_data_->PRG.resize(0x4000 * prg_banks);
  int read_bytes = 0;
  read_bytes = rom_file.ReadAtCurrentPos(
      reinterpret_cast<char*>(rom_data_->PRG.data()), 0x4000 * prg_banks);
  if (read_bytes == -1 || read_bytes != 0x4000 * prg_banks) {
    LOG(ERROR) << "Reading PRG-ROM from image file failed.";
    return LoadResult::failed();
  }
  crc = crc32_z(crc, rom_data_->PRG.data(), rom_data_->PRG.size());

  // CHR-ROM 8KB banks
  Byte chr_banks = headers[5];
  if (chr_banks) {
    rom_data_->CHR.resize(0x2000 * chr_banks);
    read_bytes = rom_file.ReadAtCurrentPos(
        reinterpret_cast<char*>(rom_data_->CHR.data()), 0x2000 * chr_banks);
    if (read_bytes == -1 || read_bytes != 0x2000 * chr_banks) {
      LOG(ERROR) << "Reading CHR-ROM from image file failed.";
      return LoadResult::failed();
    }
  } else {
    LOG(INFO) << "Cartridge with CHR-RAM.";
  }
  crc_ = crc32_z(crc, rom_data_->CHR.data(), rom_data_->CHR.size());
  rom_data_->crc = crc_;

  rom_path_ = rom_path;
  is_loaded_ = true;

  // Mapper::Create() may access |rom_data_|, and we have already filled
  // |rom_data_|, so set |is_loaded_| to true.
  ProcessMapper();
  return LoadResult{crc_, true};
}

Cartridge::LoadResult Cartridge::LoadFromDataOnIOThread(const Bytes& data) {
  DCHECK(emulator_->is_power_on() &&
         emulator_->io_task_runner()->RunsTasksInCurrentSequence());

  DCHECK(!rom_data_);
  rom_data_ = std::make_unique<RomData>();

  const Byte* data_ptr = data.data();
  ProcessHeaders(data_ptr);
  for (size_t i = 0; i < 0x10; ++i) {
    rom_data_->raw_headers.push_back(*(data_ptr + i));
  }
  data_ptr += 0x10;
  const Byte* crc32_prg_chr_start = data_ptr;

  // PRG-ROM 16KB banks
  Byte prg_banks = rom_data_->raw_headers[4];
  rom_data_->PRG.resize(0x4000 * prg_banks);
  memcpy(rom_data_->PRG.data(), data_ptr, rom_data_->PRG.size());
  data_ptr += rom_data_->PRG.size();

  // CHR-ROM 8KB banks
  Byte chr_banks = rom_data_->raw_headers[5];
  if (chr_banks) {
    rom_data_->CHR.resize(0x2000 * chr_banks);
    memcpy(rom_data_->CHR.data(), data_ptr, rom_data_->CHR.size());
  } else {
    LOG(INFO) << "Cartridge with CHR-RAM.";
  }
  data_ptr += rom_data_->CHR.size();

  const Byte* crc32_prg_chr_end = data_ptr;
  uLong crc = crc32_z(0L, Z_NULL, 0);
  crc_ = crc32_z(crc, crc32_prg_chr_start,
                 crc32_prg_chr_end - crc32_prg_chr_start);
  rom_data_->crc = crc_;

  is_loaded_ = true;
  // Mapper::Create() may access |rom_data_|, and we have already fill
  // |rom_data_|, so set |is_loaded_| to true.
  ProcessMapper();
  return LoadResult{crc_, true};
}

bool Cartridge::ProcessHeaders(const Byte* headers) {
  if (memcmp(headers, "NES\x1A", 4) != 0) {
    LOG(ERROR) << "Not a valid iNES image.";
    return false;
  }

  Byte prg_banks = headers[4];
  LOG(INFO) << "16KB PRG-ROM Banks: " << static_cast<int>(prg_banks);
  if (!prg_banks) {
    LOG(ERROR) << "ROM has no PRG-ROM banks. Loading ROM failed.";
    return false;
  }

  Byte chr_banks = headers[5];
  LOG(INFO) << "8KB CHR-ROM Banks: " << static_cast<int>(chr_banks);

  // 6     Flags 6
  //     D~7654 3210
  //       ---------
  //       NNNN FTBM
  //       |||| |||+-- Hard-wired nametable mirroring type
  //       |||| |||     0: Horizontal or mapper-controlled
  //       |||| |||     1: Vertical
  //       |||| ||+--- "Battery" and other non-volatile memory
  //       |||| ||      0: Not present
  //       |||| ||      1: Present
  //       |||| |+--- 512-byte Trainer
  //       |||| |      0: Not present
  //       |||| |      1: Present between Header and PRG-ROM data
  //       |||| +---- Hard-wired four-screen mode
  //       ||||        0: No
  //       ||||        1: Yes
  //       ++++------ Mapper Number D0..D3
  // 7      Flags 7
  //      D~7654 3210
  //        ---------
  //        NNNN 10TT
  //        |||| ||++- Console type
  //        |||| ||     0: Nintendo Entertainment System/Family Computer
  //        |||| ||     1: Nintendo Vs. System
  //        |||| ||     2: Nintendo Playchoice 10
  //        |||| ||     3: Extended Console Type
  //        |||| ++--- NES 2.0 identifier
  //        ++++------ Mapper Number D4..D7
  // 8      Mapper MSB/Submapper
  //      D~7654 3210
  //        ---------
  //        SSSS NNNN
  //        |||| ++++- Mapper number D8..D11
  //        ++++------ Submapper number
  if (headers[6] & 0x8) {
    rom_data_->name_table_mirroring = NametableMirroring::kFourScreen;
    LOG(INFO) << "Name Table Mirroring: FourScreen";
  } else {
    rom_data_->name_table_mirroring =
        static_cast<NametableMirroring>(headers[6] & 0x1);
    LOG(INFO) << "Name Table Mirroring: "
              << (rom_data_->name_table_mirroring ==
                          NametableMirroring::kHorizontal
                      ? "Horizontal"
                      : "Vertical");
  }
  rom_data_->console_type = static_cast<ConsoleType>(headers[7] & 0x3);

  rom_data_->mapper = ((headers[6] >> 4) & 0xf) | (headers[7] & 0xf0);
  LOG(INFO) << "Mapper #" << static_cast<int>(rom_data_->mapper);

  rom_data_->submapper = (headers[8] >> 4) & 0xf;

  rom_data_->has_extended_ram = headers[6] & 0x2;
  LOG(INFO) << "Extended (CPU) RAM: " << rom_data_->has_extended_ram;

  if (headers[6] & 0x4) {
    LOG(ERROR) << "Trainer is not supported.";
    return false;
  }

  // Mappers (Flags 7, D4-D7) and sub mappers are ignored.
  rom_data_->is_nes_20 = (headers[7] & 0x0C) == 0x08;

  if ((headers[0xA] & 0x3) == 0x2 || (headers[0xA] & 0x1)) {
    LOG(ERROR) << "PAL ROM not supported.";
    return false;
  } else {
    LOG(INFO) << "ROM is NTSC compatible.\n";
    return true;
  }
}

void Cartridge::ProcessMapper() {
  mapper_ = Mapper::Create(this, rom_data_->mapper);
}

}  // namespace nes
}  // namespace kiwi
