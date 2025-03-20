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

#ifndef NES_CARTRIDGE_H_
#define NES_CARTRIDGE_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/nes_export.h"
#include "nes/types.h"

#include <atomic>

namespace kiwi {
namespace nes {
class Mapper;
class EmulatorImpl;
class RomData;

// Cartridge, or ROM cartridge, is the media container for the NES games.
// This class parse ROM files according to https://www.nesdev.org/wiki/INES.
class Cartridge : public base::RefCountedThreadSafe<Cartridge>,
                  public EmulatorStates::SerializableState {
  friend class base::RefCountedThreadSafe<Cartridge>;

 public:
  explicit Cartridge(EmulatorImpl* emulator);

 private:
  ~Cartridge();

 public:
  struct LoadResult {
    uint32_t crc32;
    bool success;

    static LoadResult failed() { return LoadResult{0, false}; }
  };
  using LoadCallback = base::OnceCallback<void(LoadResult)>;

  // These 2 methods must be called on IO thread. It will only will be called by
  // emulator instance.
  Cartridge::LoadResult Load(const base::FilePath& rom_path);
  Cartridge::LoadResult Load(const Bytes& data);

  bool is_loaded() { return is_loaded_; }
  uint32_t crc32() { return crc_; }

 public:
  // Following methods are called in IO thread.
  // An NES Game Pak has a PRG ROM connected to the CPU and either a second CHR
  // ROM or a CHR RAM (or, rarely, both) connected to the PPU. See
  // https://www.nesdev.org/wiki/ROM for more details.
  RomData* GetRomData();

  // Returns the mapper for current cartridge. If the cartridge is not loaded,
  // nullptr will be returned.
  Mapper* mapper() { return mapper_.get(); }

  void Reset();

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  // Load ROM from |rom_path|. File structures are following
  // https://www.nesdev.org/wiki/NES_2.0. Returns whether load succeed.
  LoadResult LoadFromFileOnIOThread(const base::FilePath& rom_path);

  // Load ROM from |data|.
  LoadResult LoadFromDataOnIOThread(const Bytes& data);

  bool ProcessHeaders(const Byte* headers);
  // Give a chance to adjust headers
  void PatchHeaders();
  bool ProcessMapper();

 private:
  EmulatorImpl* emulator_ = nullptr;
  base::FilePath rom_path_;
  std::atomic_bool is_loaded_ = false;
  std::unique_ptr<RomData> rom_data_;
  std::unique_ptr<Mapper> mapper_;
  uint32_t crc_;  // crc_ is the combination CRC32 of PRG and CHR
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_CARTRIDGE_H_
