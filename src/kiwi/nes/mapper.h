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

#ifndef NES_MAPPER_H_
#define NES_MAPPER_H_

#include <cstddef>
#include <cstdint>
#include <optional>

#include "base/functional/callback.h"
#include "nes/emulator_states.h"
#include "nes/nes_export.h"
#include "nes/rom_data.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {
class Cartridge;

// NES games come in cartridges, and inside of those cartridges are various
// circuits and hardware. Different games use different circuits and hardware,
// and the configuration and capabilities of such cartridges is commonly called
// their mapper. Mappers are designed to extend the system and bypass its
// limitations, such as by adding RAM to the cartridge or even extra sound
// channels. More commonly though, mappers are designed to allow games larger
// than 40K to be made.
// See https://www.nesdev.org/wiki/Mapper for more details.
class NES_EXPORT Mapper : public EmulatorStates::SerializableState {
 public:
  using MirroringChangedCallback = base::RepeatingClosure;
  using IRQCallback = base::RepeatingClosure;

  struct PRGNVRAMSnapshot {
    Bytes data;
    uint64_t generation = 0;
  };

  explicit Mapper(Cartridge* cartridge);
  ~Mapper() override;

  void set_mirroring_changed_callback(MirroringChangedCallback callback) {
    mirroring_changed_callback_ = callback;
  }

  void set_irq_callback(IRQCallback callback) { irq_callback_ = callback; }

  virtual void Reset();

  // CPU: $8000-$FFFF
  virtual void WritePRG(Address addr, Byte value) = 0;
  virtual Byte ReadPRG(Address addr) = 0;

  // PPU: $0000-$1FFF
  virtual void WriteCHR(Address addr, Byte value) = 0;
  virtual Byte ReadCHR(Address addr) = 0;

  virtual NametableMirroring GetNametableMirroring();
  virtual void ScanlineIRQ(int scanline, bool render_enabled);
  virtual void M2CycleIRQ();

  // MMC3 uses this.
  virtual void PPUAddressChanged(Address address);

  // CPU: $4020-$7FFF
  // The CPU bus forwards cartridge expansion and PRG-RAM accesses here. The
  // mapper decides whether an address selects RAM, ROM, registers, or open bus.
  virtual void WriteExtendedRAM(Address address, Byte value);

  // If the mapper does not provide PRG-RAM or another mapping, reads return
  // open-bus behavior.
  virtual Byte ReadExtendedRAM(Address address);

  virtual Byte* GetExtendedRAMPointer();
  bool HasPRGRAM() const;
  bool HasBatteryBackedRAM() const;
  size_t GetPRGNVRAMSize() const;

  // Returns no snapshot when the cartridge has no PRG-NVRAM, or when
  // |dirty_only| is true and the current data has already been persisted.
  std::optional<PRGNVRAMSnapshot> ExportPRGNVRAM(bool dirty_only);
  // Imports an exact-size snapshot and treats it as the persisted baseline.
  bool ImportPRGNVRAM(const Bytes& data);
  bool IsPRGNVRAMDirty() const;
  // Marks only the exported generation as persisted. Writes made after the
  // snapshot remain dirty.
  void AcknowledgePRGNVRAMSaved(uint64_t generation);

  static std::unique_ptr<Mapper> Create(Cartridge* cartridge, Byte mapper);
  static bool IsMapperSupported(Byte mapper);

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

  // For MMC5 only
  virtual bool IsMMC5() { return false; }
  virtual Byte ReadNametableByte(Byte* ram, Address address) { return 0; }
  virtual void WriteNametableByte(Byte* ram, Address address, Byte value) {}
  virtual void SetCurrentRenderState(bool is_background,
                                     bool is_8x16_sprite,
                                     int current_dot_in_scanline) {}
  virtual Byte GetFineXInSplitRegion(Byte ppu_x_fine) { return ppu_x_fine; }
  virtual Address GetDataAddressInSplitRegion(Address ppu_data_address) {
    return ppu_data_address;
  }

  // Append new virtual methods here to preserve existing vtable slots.
  virtual bool NeedsM2CycleIRQ() const;
  // Maps a PPU pattern-table address to its absolute byte offset in CHR-ROM.
  virtual uint32_t GetAbsoluteCHRAddress(Address address);
  // Returns true when the mapper stores PRG-RAM outside the base
  // implementation.
  virtual bool UsesCustomPRGRAM() const;

 protected:
  void EnsurePRGRAM(size_t minimum_size);
  void MarkPRGNVRAMDirty();

  // Custom PRG-RAM mappers override these to expose the complete persistent
  // storage rather than a currently selected CPU window.
  virtual Bytes CopyPRGNVRAM();
  virtual bool RestorePRGNVRAM(const Bytes& data);

  MirroringChangedCallback mirroring_changed_callback() {
    return mirroring_changed_callback_;
  }

  // A callback to set CPU's IRQ.
  IRQCallback irq_callback() { return irq_callback_; }

 private:
  void AllocatePRGRAMIfNeeded();

 protected:
  RomData* rom_data() { return rom_data_; }

 private:
  RomData* rom_data_ = nullptr;
  MirroringChangedCallback mirroring_changed_callback_;
  IRQCallback irq_callback_;
  // Persistent bytes come first so the default $6000 window reaches NVRAM
  // when a NES 2.0 image declares both volatile and non-volatile PRG-RAM.
  Bytes default_prg_ram_;
  uint64_t prg_nvram_generation_ = 0;
  uint64_t persisted_prg_nvram_generation_ = 0;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPER_H_
