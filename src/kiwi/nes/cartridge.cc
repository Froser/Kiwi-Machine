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
#include "nes/nes_buildflags.h"
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

  rom_data_->is_nes_20 = (headers[7] & 0x0C) == 0x08;
  rom_data_->mapper =
      static_cast<MapperId>(((headers[6] >> 4) & 0x0f) | (headers[7] & 0xf0));
  if (rom_data_->is_nes_20) {
    rom_data_->mapper |= static_cast<MapperId>(headers[8] & 0x0f) << 8;
  }
  LOG(INFO) << "Mapper #" << rom_data_->mapper;

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

  const Byte timing_mode = rom_data_->is_nes_20 ? headers[12] & 0x03
#if BUILDFLAG(ENABLE_INES1_TV_SYSTEM_FIX)
                                                : headers[9] & 0x01;
#else
                                                : headers[10] & 0x03;
#endif
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
#if BUILDFLAG(ENABLE_MAPPER_355)
    case 0x86dba660: {  // 3-D Block (Asia) (Unl) (Hwang Shinwei)
      DCHECK_EQ(rom_data_->mapper, 219);
      rom_data_->mapper = 355;
      rom_data_->name_table_mirroring = NametableMirroring::kVertical;
      rom_data_->has_battery = false;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0;
      break;
    }
#endif
#if BUILDFLAG(ENABLE_MAPPER_245)
    case 0xdfad3f66: {  // Ying Xiong Yuan Yi Jing Chuan Qi (China) (Unl)
      DCHECK_EQ(rom_data_->mapper, 245);
      rom_data_->mapper = 4;
      rom_data_->name_table_mirroring = NametableMirroring::kHorizontal;
      rom_data_->has_battery = true;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0x2000;
      break;
    }
#endif
#if BUILDFLAG(ENABLE_MAPPER_512)
    case 0x037006f7: {  // Zhongguo Daheng (Asia) (Unl)
      DCHECK_EQ(rom_data_->mapper, 116);
      rom_data_->mapper = 512;
      rom_data_->name_table_mirroring = NametableMirroring::kHorizontal;
      rom_data_->has_battery = true;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0x2000;
      break;
    }
#endif
#if BUILDFLAG(ENABLE_MAPPER_156)
    case 0xccc03440: {  // Buzz & Waldog (USA) (Unl) (Proto)
      DCHECK_EQ(rom_data_->mapper, 255);
      rom_data_->mapper = 156;
      break;
    }
#endif
#if BUILDFLAG(ENABLE_MAPPER_079)
    case 0x58152b42: {  // Pipe 5 (Asia) (Unl)
      DCHECK_EQ(rom_data_->mapper, 142);
      rom_data_->mapper = 79;
      break;
    }
#endif
    // Most dumps of mapper 048 games floating around are erroneously labelled
    // as mapper 033.
    case 0xaebd6549:  // Bakushou!! Jinsei Gekijou 3 (Japan)
    case 0xa7b0536c:  // Don Doko Don 2 (Japan)
    case 0x1500e835:  // The Jetsons - Cogswell's Caper! (Japan)
    case 0x40c0ad47:  // The Flintstones - The Rescue of Dino & Hoppy (Japan)
      DCHECK_EQ(rom_data_->mapper, 33);
      rom_data_->mapper = 48;
      break;
#if BUILDFLAG(ENABLE_MAPPER_078)
    case 0x3d1c3137:  // Uchuusen - Cosmo Carrier (Japan)
      DCHECK_EQ(rom_data_->mapper, 78);
      rom_data_->submapper = 1;
      rom_data_->name_table_mirroring = NametableMirroring::kHorizontal;
      break;
    case 0xba51ac6f:  // Holy Diver (Japan)
      DCHECK_EQ(rom_data_->mapper, 78);
      rom_data_->submapper = 3;
      rom_data_->name_table_mirroring = NametableMirroring::kHorizontal;
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_016)
    case 0x2c4421b2:  // Akuma-kun - Makai no Wana (Japan)
    case 0x33b899c9:  // Dragon Ball - Dai Maou Fukkatsu (Japan)
    case 0x6e68e31a:  // Dragon Ball 3 - Gokuu Den (Japan)
    case 0x7b5206af:  // Meimon! Dai San Yakyuu Bu (Japan)
    case 0x9c04c8d5:  // Sakigake!! Otoko Juku (Japan)
    case 0xa851cae9:  // Nishimura Kyoutarou Mystery (Japan)
    case 0xd343c66a:  // Famicom Jump - Eiyuu Retsuden (Japan)
      DCHECK_EQ(rom_data_->mapper, 16);
      rom_data_->submapper = 4;
      break;
    case 0xdb05106e:  // Crayon Shin-Chan - Ora to Poi Poi (Japan)
      DCHECK_EQ(rom_data_->mapper, 16);
      rom_data_->submapper = 5;
      rom_data_->has_battery = false;
      rom_data_->prg_nvram_size = 0;
      break;
    case 0x136ca449:  // Dragon Ball Z Gaiden (Japan)
    case 0xa262a81f:  // Rokudenashi Blues (Japan)
    case 0xa9541452:  // Dragon Ball Z II (Japan) (Rev 1)
    case 0xb049a8c4:  // SD Gundam Gaiden 2 (Japan)
    case 0xc2840372:  // SD Gundam Gaiden 3 (Japan)
    case 0xdc52bf0c:  // Dragon Ball Z III (Japan)
      DCHECK_EQ(rom_data_->mapper, 16);
      rom_data_->submapper = 5;
      rom_data_->has_battery = true;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 256;
      break;
    case 0x0cf42e69:  // Magical Taruruuto Kun (Japan)
    case 0x183859d2:  // Dragon Ball Z - Kyoushuu! Saiya Jin (Japan)
    case 0x276ac722:  // SD Gundam Gaiden (Japan) (Rev 1)
    case 0xb7f28915:  // Magical Taruruuto Kun 2 (Japan)
    case 0xdcb972ce:  // Magical Taruruuto Kun (Japan) (Rev 1)
    case 0xe170404c:  // SD Gundam Gaiden (Japan)
      DCHECK(rom_data_->mapper == 16 || rom_data_->mapper == 159);
      rom_data_->mapper = 159;
      rom_data_->has_battery = true;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 128;
      break;
    case 0x3f15d20d:  // Famicom Jump II (Japan)
      DCHECK_EQ(rom_data_->mapper, 16);
      rom_data_->mapper = 153;
      rom_data_->has_battery = true;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0x2000;
      break;
    case 0x983d8175:  // Datach - Battle Rush (Japan)
      DCHECK_EQ(rom_data_->mapper, 16);
      rom_data_->mapper = 157;
      rom_data_->has_battery = true;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 384;
      break;
    case 0x0be0a328:  // Datach - SD Gundam (Japan)
    case 0x19e81461:  // Datach - Dragon Ball Z (Japan)
    case 0x5b457641:  // Datach - Ultraman Club (Japan)
    case 0x894efdbc:  // Datach - Crayon Shin Chan (Japan)
    case 0xbe06853f:  // Datach - J League (Japan)
    case 0xf51a7f46:  // Datach - Yuu Yuu Hakusho (Japan)
      DCHECK_EQ(rom_data_->mapper, 16);
      rom_data_->mapper = 157;
      rom_data_->has_battery = true;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 256;
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_176)
    case 0x02c41438:  // Xing He Zhan Shi (China) (Unl)
      DCHECK_EQ(rom_data_->mapper, 177);
      rom_data_->mapper = 176;
      rom_data_->submapper = 2;
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_177)
    case 0xb5e83c9a:  // Xing Ji Zheng Ba (China) (Unl)
      DCHECK_EQ(rom_data_->mapper, 178);
      rom_data_->mapper = 177;
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_241)
    case 0xfb2b6b10:  // Fan Kong Jing Ying (China) (Unl)
      DCHECK_EQ(rom_data_->mapper, 178);
      rom_data_->mapper = 241;
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_019)
    case 0x0c47946d:  // Chibi Maruko-Chan - Uki Uki Shopping (Japan)
    case 0x808606f0:  // Famista '91 (Japan)
    case 0x81b7f1a8:  // Heisei Tensai Bakabon (Japan)
      DCHECK_EQ(rom_data_->mapper, 19);
      rom_data_->mapper = 210;
      rom_data_->submapper = 1;
      rom_data_->has_battery = false;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0;
      break;
    case 0xc247cc80:  // Family Circuit '91 (Japan)
      DCHECK_EQ(rom_data_->mapper, 19);
      rom_data_->mapper = 210;
      rom_data_->submapper = 1;
      rom_data_->has_battery = true;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0x0800;
      break;
    case 0x1dc0f740:  // Wagan Land 2 (Japan)
    case 0x2447e03b:  // Top Striker (Japan)
    case 0x429103c9:  // Famista '94 (Japan)
    case 0x46fd7843:  // Splatter House - Wanpaku Graffiti (Japan)
    case 0x6ec51de5:  // Famista '92 (Japan)
    case 0xadffd64f:  // Famista '93 (Japan)
    case 0xbd523011:  // Dream Master (Japan)
    case 0xd323b806:  // Wagan Land 3 (Japan)
      DCHECK_EQ(rom_data_->mapper, 19);
      rom_data_->mapper = 210;
      rom_data_->submapper = 2;
      rom_data_->has_battery = false;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0;
      break;
    case 0x2a7d3adf:  // Dragon Ninja (Japan)
    case 0x4c5836bd:  // Namco Classic (Japan)
    case 0xca69751b:  // Star Wars (Japan) (Namco)
      DCHECK_EQ(rom_data_->mapper, 19);
      rom_data_->submapper = 2;
      rom_data_->has_battery = false;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0;
      break;
    case 0x0c1792da:  // Famista '90 (Japan) (Rev 0A)
    case 0x10c8f2fa:  // Dokuganryuu Masamune (Japan)
    case 0x47c2020b:  // Hydlide 3 - Yami Kara no Houmonsha (Japan)
    case 0xace56f39:  // Mindseeker (Japan)
    case 0xb5ff71ab:  // Battle Fleet (Japan)
    case 0xbc11e61a:  // Kaijuu Monogatari (Japan)
      DCHECK_EQ(rom_data_->mapper, 19);
      rom_data_->submapper = 2;
      rom_data_->has_battery = true;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0;
      break;
    case 0xcf23290f:  // Juvei Quest (Japan)
      DCHECK_EQ(rom_data_->mapper, 19);
      rom_data_->submapper = 2;
      rom_data_->has_battery = true;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0x2000;
      break;
    case 0x5746a461:  // Final Lap (Japan)
    case 0x684b292f:  // Namco Classic II (Japan)
      DCHECK_EQ(rom_data_->mapper, 19);
      rom_data_->submapper = 3;
      rom_data_->has_battery = false;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0;
      break;
    case 0x098c672a:  // Sangokushi 2 - Haou no Tairiku (Japan)
    case 0x96773f32:  // Digital Devil Story - Megami Tensei II (Japan)
      DCHECK_EQ(rom_data_->mapper, 19);
      rom_data_->submapper = 3;
      rom_data_->has_battery = true;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0x2000;
      break;
    case 0x2565786d:  // Rolling Thunder (Japan)
      DCHECK_EQ(rom_data_->mapper, 19);
      rom_data_->submapper = 4;
      rom_data_->has_battery = false;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0;
      break;
    case 0x35d8c961:  // Mappy Kids (Japan)
    case 0xc811dc7a:  // Youkai Douchuuki (Japan)
    case 0xef7996bf:  // Erika to Satoru no Yume Bouken (Japan)
      DCHECK_EQ(rom_data_->mapper, 19);
      rom_data_->submapper = 5;
      rom_data_->has_battery = false;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0;
      break;
    case 0x369da42d:  // King of Kings (Japan)
    case 0xe64b8975:  // Sangokushi - Chuugen no Hasha (Japan)
      DCHECK_EQ(rom_data_->mapper, 19);
      rom_data_->submapper = 5;
      rom_data_->has_battery = true;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0x2000;
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_023)
    case 0xd467c0cc:  // Parodius da! (Japan)
    case 0xfcbf28b1:  // Crisis Force (Japan)
      if (rom_data_->prg_ram_size < 0x0800)
        rom_data_->prg_ram_size = 0x0800;
      [[fallthrough]];
    case 0x91328c1d:  // Tiny Toon Adventures (Japan)
    case 0xc1fbf659:  // Akumajou Special - Boku Dracula-kun (Japan)
      DCHECK_EQ(rom_data_->mapper, 23);
      rom_data_->submapper = 2;
      break;
    case 0x0cc9ffec:  // Ganbare Goemon 2 (Japan)
    case 0x203583d5:  // TwinBee 3 - Poko Poko Dai Maou (Japan) (Beta)
    case 0x39b68aa3:  // Jarinko Chie (Japan)
    case 0x49123146:  // Getsufuu Maden (Japan)
    case 0x8a96e00d:  // Wai Wai World (Japan)
    case 0xaa9f9765:  // Mad City (Japan) (Beta)
    case 0xac9895cc:  // Dragon Scroll (Japan)
    case 0xb27b8cf4:  // Gryzor (Japan)
      DCHECK_EQ(rom_data_->mapper, 23);
      rom_data_->submapper = 3;
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_032)
    case 0x243a8735:  // Major League (Japan)
      DCHECK_EQ(rom_data_->mapper, 32);
      rom_data_->submapper = 1;
      rom_data_->name_table_mirroring = NametableMirroring::kOneScreenHigher;
      break;
    case 0xd2038fc5:  // Image Fight (Japan)
      DCHECK_EQ(rom_data_->mapper, 32);
      if (rom_data_->prg_ram_size < 0x2000)
        rom_data_->prg_ram_size = 0x2000;
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_112)
    case 0xfd55dd33:  // Fighting Hero III (Asia) (Unl)
      DCHECK_EQ(rom_data_->mapper, 112);
      if (rom_data_->prg_ram_size < 0x2000)
        rom_data_->prg_ram_size = 0x2000;
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_070)
    // These early iNES dumps predate mapper 152 and use mapper 70 despite
    // having mapper-controlled one-screen mirroring.
    case 0x026c5fca:  // Saint Seiya - Ougon Densetsu (Japan)
    case 0xb1a94b82:  // Pocket Zaurus - Juu Ouken no Nazo (Japan)
    case 0xbda8f8e4:  // Gegege no Kitarou 2 (Japan)
      DCHECK_EQ(rom_data_->mapper, 70);
      rom_data_->mapper = 152;
      break;
    // The fixed-mirroring mapper 70 dumps carry incorrect horizontal flags.
    case 0x370ceb65:  // Family Trainer - Meiro Daisakusen (Japan)
    case 0xad9c63e2:  // Space Shadow (Japan)
    case 0xbba58be5:  // Family Trainer - Manhattan Police (Japan)
    case 0xdd8ed0f7:  // Kamen Rider Club (Japan)
      DCHECK_EQ(rom_data_->mapper, 70);
      rom_data_->name_table_mirroring = NametableMirroring::kVertical;
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_080)
    case 0x7678f1d5:  // Fudou Myouou Den (Japan)
      DCHECK_EQ(rom_data_->mapper, 80);
      rom_data_->mapper = 207;
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_085)
    case 0x33ce3ff0:  // Lagrange Point (Japan)
      DCHECK_EQ(rom_data_->mapper, 85);
      rom_data_->submapper = 2;
      break;
    case 0xe4362167:  // Tiny Toon Adventures 2 (Japan)
      DCHECK_EQ(rom_data_->mapper, 85);
      rom_data_->submapper = 1;
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_133)
    case 0x5aefbc94:  // Jovial Race (Asia) (Unl) (NES)
      DCHECK_EQ(rom_data_->mapper, 150);
      rom_data_->mapper = 133;
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_150)
    // These SA-015/SA-630 dumps predate mapper 150 and were distributed with
    // the SA-020A mapper number.
    case 0x2394ae1c:  // Happy Pairs (Asia) (Unl) (NES)
    case 0x247cc73d:  // Poker II (Asia) (Unl)
    case 0x34ddf806:  // Strategist (Asia) (Unl) (Famicom)
    case 0x40dbf7a2:  // Olympic I.Q. (Asia) (Unl) (NES)
    case 0x47918d84:  // Auto-Upturn (Asia) (Unl) (NES)
    case 0x73fb55ac:  // 2 in 1 Lightgun Game - Cosmocop + Cyber Monster
    case 0xa95a915a:  // Tasac (Asia) (Unl)
    case 0xbe17e27b:  // Poker III 5 in 1 (Asia) (Unl)
    case 0xc06facfc:  // Strategist (Asia) (Unl) (NES)
    case 0xcab40a6c:  // Magic Cube (Asia) (Unl) (NES)
    case 0xddcbda16:  // 2 in 1 Lightgun Game - Tough Cop + Super Tough Cop
      DCHECK_EQ(rom_data_->mapper, 243);
      rom_data_->mapper = 150;
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_206)
    case 0x0ffde258:  // Fantasy Zone (USA) (Unl)
    case 0x139eb5b5:  // Indiana Jones and the Temple of Doom (USA) (Unl)
    case 0x22d6d5bd:  // Jikuu Yuuden - Debias (Japan)
    case 0x2a01f9d1:  // Wagan Land (Japan)
    case 0x2e326a1d:  // R.B.I. Baseball (USA) (Unl)
    case 0x5397e80b:  // Tenkaichi Bushi - Keru Naguuru (Japan)
    case 0x5b4c6146:  // Family Boxing (Japan)
    case 0x5bb62688:  // Ring King (USA)
    case 0x9cbc8253:  // Family Circuit (Japan)
    case 0x9d21fe96:  // Lupin Sansei - Pandora no Isan (Japan)
    case 0xa49253c6:  // Family Tennis (Japan)
    case 0xa5e6baf9:  // Dragon Slayer 4 - Drasle Family (Japan)
    case 0xa8f5c2ab:  // Vindicators (USA) (Unl)
    case 0xcd50a092:  // Gauntlet (USA) (Unl)
    case 0xd97c31b0:  // Lasa-r Ishii no Childs Quest (Japan)
    case 0xe1526228:  // The Quest of Ki (Japan)
      DCHECK_EQ(rom_data_->mapper, 4);
      rom_data_->mapper = 206;
      rom_data_->submapper = 0;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0;
      break;
    case 0xd7aa0b6d:  // Dragon Buster II - Yami no Fuuin (Japan)
      DCHECK_EQ(rom_data_->mapper, 88);
      rom_data_->mapper = 206;
      rom_data_->submapper = 0;
      break;
    case 0xca6a7bf1:  // Sky Kid (Japan)
      DCHECK_EQ(rom_data_->mapper, 4);
      rom_data_->mapper = 206;
      rom_data_->submapper = 1;
      rom_data_->prg_ram_size = 0;
      rom_data_->prg_nvram_size = 0;
      break;
    case 0xd1691028:  // Devil Man (Japan)
      DCHECK_EQ(rom_data_->mapper, 88);
      rom_data_->mapper = 154;
      break;
#endif
#if BUILDFLAG(ENABLE_MAPPER_104)
    case 0x6096f84e:  // Pegasus 5 in 1 (Unl)
      DCHECK_EQ(rom_data_->mapper, 71);
      rom_data_->mapper = 104;
      break;
#endif
  }
}

bool Cartridge::ProcessMapper() {
  mapper_ = Mapper::Create(this, rom_data_->mapper);
  return !!mapper_;
}

}  // namespace nes
}  // namespace kiwi
