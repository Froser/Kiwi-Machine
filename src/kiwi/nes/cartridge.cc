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
#include <string>

#include "base/check.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "nes/emulator_impl.h"
#include "nes/mapper.h"
#include "nes/rom_data.h"
#include "nes/rom_hash.h"
#include "nes/types.h"
#include "third_party/zlib-1.3.2/zlib.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kINESPRGRAMBankSize = 8 * 1024;

size_t DecodeNES20RAMSize(Byte shift_count) {
  return shift_count == 0 ? 0 : static_cast<size_t>(64) << shift_count;
}

std::string CalculateROMHash(const RomData& rom_data) {
  Bytes content;
  content.reserve(rom_data.PRG.size() + rom_data.CHR.size());
  content.insert(content.end(), rom_data.PRG.begin(), rom_data.PRG.end());
  content.insert(content.end(), rom_data.CHR.begin(), rom_data.CHR.end());
  return CalculateSha1Hex(content);
}

}  // namespace

Cartridge::Cartridge(EmulatorImpl* emulator) : emulator_(emulator) {}
Cartridge::~Cartridge() = default;

Cartridge::LoadResult Cartridge::Load(const base::FilePath& rom_path) {
  // This method must be called on IO thread, by emulator.
  if (emulator_->is_power_on()) {
    is_loaded_ = false;
    return LoadFromFileOnIOThread(rom_path);
  } else {
    LOG(ERROR) << "The emulator is power off yet. You should call "
                  "Emulator::PowerOn() first.";
    return Cartridge::LoadResult::failed();
  }
}

Cartridge::LoadResult Cartridge::Load(const Bytes& data) {
  if (emulator_->is_power_on()) {
    is_loaded_ = false;
    return LoadFromDataOnIOThread(data);
  } else {
    LOG(ERROR) << "The emulator is power off yet. You should call "
                  "Emulator::PowerOn() first.";
    Cartridge::LoadResult result;
    result.success = false;
    return result;
  }
}

RomData* Cartridge::GetRomData() {
  DCHECK(emulator_->is_power_on() && is_loaded_);
  return rom_data_.get();
}

void Cartridge::Reset() {
  if (mapper_) {
    mapper_->Reset();
  }
}

void Cartridge::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(crc_);
  mapper()->Serialize(data);
}

bool Cartridge::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  if (header.version == EmulatorStates::kCurrentVersion) {
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

  DCHECK(!rom_data_);
  rom_data_ = std::make_unique<RomData>();

  base::File rom_file(rom_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!rom_file.IsValid()) {
    LOG(ERROR) << "Could not open ROM file from path: "
               << rom_path.AsUTF8Unsafe();
    return LoadResult::failed();
  }

  Bytes headers;
  LOG(INFO) << "Reading ROM from path: " << rom_path.AsUTF8Unsafe();

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

  // Some roms don't have CHR.
  if (!rom_data_->CHR.empty())
    crc = crc32_z(crc, rom_data_->CHR.data(), rom_data_->CHR.size());
  crc_ = crc;
  rom_data_->crc = crc;
  rom_data_->sha1 = CalculateROMHash(*rom_data_);

  rom_path_ = rom_path;

  PatchHeaders();
  is_loaded_ = true;

  // Mapper::Create() may access |rom_data_|, and we have already filled
  // |rom_data_|, so set |is_loaded_| to true.
  return LoadResult{crc_, ProcessMapper()};
}

Cartridge::LoadResult Cartridge::LoadFromDataOnIOThread(const Bytes& data) {
  DCHECK(emulator_->is_power_on());

  DCHECK(!rom_data_);
  rom_data_ = std::make_unique<RomData>();

  constexpr size_t kINESHeaderSize = 0x10;
  if (data.size() < kINESHeaderSize || !ProcessHeaders(data.data())) {
    return LoadResult::failed();
  }

  const size_t prg_size = static_cast<size_t>(data[4]) * 0x4000;
  const size_t chr_size = static_cast<size_t>(data[5]) * 0x2000;
  if (data.size() < kINESHeaderSize + prg_size + chr_size) {
    LOG(ERROR) << "ROM image is smaller than its declared PRG/CHR data.";
    return LoadResult::failed();
  }

  const Byte* data_ptr = data.data();
  rom_data_->raw_headers.assign(data_ptr, data_ptr + kINESHeaderSize);
  data_ptr += kINESHeaderSize;
  const Byte* crc32_prg_chr_start = data_ptr;

  // PRG-ROM 16KB banks
  rom_data_->PRG.resize(prg_size);
  memcpy(rom_data_->PRG.data(), data_ptr, rom_data_->PRG.size());
  data_ptr += rom_data_->PRG.size();

  // CHR-ROM 8KB banks
  if (chr_size != 0) {
    rom_data_->CHR.resize(chr_size);
    memcpy(rom_data_->CHR.data(), data_ptr, rom_data_->CHR.size());
  } else {
    LOG(INFO) << "Cartridge with CHR-RAM.";
  }
  data_ptr += rom_data_->CHR.size();

  const Byte* crc32_prg_chr_end = data_ptr;
  uLong crc = crc32_z(0L, Z_NULL, 0);
  // Some roms don't have CHR.
  if (crc32_prg_chr_end - crc32_prg_chr_start > 0) {
    crc = crc32_z(crc, crc32_prg_chr_start,
                  crc32_prg_chr_end - crc32_prg_chr_start);
  }
  crc_ = crc;
  rom_data_->crc = crc_;
  rom_data_->sha1 = CalculateROMHash(*rom_data_);

  PatchHeaders();
  is_loaded_ = true;
  // Mapper::Create() may access |rom_data_|, and we have already fill
  // |rom_data_|, so set |is_loaded_| to true.
  return LoadResult{crc_, ProcessMapper()};
}

bool Cartridge::ProcessHeaders(const Byte* headers) {
  if (memcmp(headers, "NES\x1A", 4) != 0) {
    LOG(ERROR) << "Not a valid iNES image. Header: " << headers[0] << headers[1]
               << headers[2] << headers[3];
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

  rom_data_->is_nes_20 = (headers[7] & 0x0C) == 0x08;
  rom_data_->submapper = rom_data_->is_nes_20 ? (headers[8] >> 4) & 0xf : 0;

  rom_data_->has_battery = (headers[6] & 0x2) != 0;
  if (rom_data_->is_nes_20) {
    rom_data_->prg_ram_size = DecodeNES20RAMSize(headers[10] & 0x0f);
    rom_data_->prg_nvram_size = DecodeNES20RAMSize(headers[10] >> 4);
  } else {
    const size_t declared_prg_ram_size =
        static_cast<size_t>(headers[8]) * kINESPRGRAMBankSize;
    if (rom_data_->has_battery) {
      // iNES 1.0 cannot distinguish volatile PRG-RAM from PRG-NVRAM.
      // A zero byte 8 conventionally means one 8 KiB bank.
      rom_data_->prg_nvram_size =
          declared_prg_ram_size ? declared_prg_ram_size : kINESPRGRAMBankSize;
    } else {
      // A zero byte 8 is ambiguous. Let the mapper declare Work RAM when its
      // board requires it instead of assigning RAM to every legacy ROM.
      rom_data_->prg_ram_size = declared_prg_ram_size;
    }
  }
  LOG(INFO) << "Battery-backed memory: " << rom_data_->has_battery;
  LOG(INFO) << "PRG-RAM: " << rom_data_->prg_ram_size
            << " bytes, PRG-NVRAM: " << rom_data_->prg_nvram_size << " bytes";

  if (headers[6] & 0x4) {
    LOG(ERROR) << "Trainer is not supported.";
    return false;
  }

  const Byte timing_mode =
      rom_data_->is_nes_20 ? headers[12] & 0x03 : headers[10] & 0x03;
  if (timing_mode != 0) {
    LOG(ERROR) << "PAL ROM not supported.";
    return false;
  } else {
    LOG(INFO) << "ROM is NTSC compatible.\n";
    return true;
  }
}

void Cartridge::PatchHeaders() {
  switch (crc_) {
    // Most dumps of mapper 048 games floating around are erroneously labelled
    // as mapper 033.
    case 0xaebd6549:  // Bakushou!! Jinsei Gekijou 3 (Japan)
    case 0xa7b0536c:  // Don Doko Don 2 (Japan)
    case 0x1500e835:  // The Jetsons - Cogswell's Caper! (Japan)
    case 0x40c0ad47:  // The Flintstones - The Rescue of Dino & Hoppy (Japan)
      DCHECK_EQ(rom_data_->mapper, 33);
      rom_data_->mapper = 48;
      break;
  }
}

bool Cartridge::ProcessMapper() {
  mapper_ = Mapper::Create(this, rom_data_->mapper);
  return !!mapper_;
}

}  // namespace nes
}  // namespace kiwi
