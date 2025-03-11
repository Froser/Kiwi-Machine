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

  explicit Mapper(Cartridge* cartridge);
  ~Mapper() override;

  void set_mirroring_changed_callback(MirroringChangedCallback callback) {
    mirroring_changed_callback_ = callback;
  }

  void set_irq_callback(IRQCallback callback) { iqr_callback_ = callback; }

  void set_irq_clear_callback(IRQCallback callback) {
    iqr_clear_callback_ = callback;
  }

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
  // If a ROM has extented RAM, when writing to $4010-$7FFF, WriteExtendedRAM()
  // will be invoked. Otherwise, WritePRG() will be invoked.
  virtual void WriteExtendedRAM(Address address, Byte value);

  // ReadExtendedRAM() will be invoked whenever read address from $4010 to
  // $7FFF. If there's no extended ram, an open bus behavior will be returned.
  virtual Byte ReadExtendedRAM(Address address);

  virtual Byte* GetExtendedRAMPointer();
  bool HasExtendedRAM();

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

 protected:
  MirroringChangedCallback mirroring_changed_callback() {
    return mirroring_changed_callback_;
  }

  // A callback set CPU's irq.
  IRQCallback irq_callback() { return iqr_callback_; }
  // A callback clear's CPU's irq pending flag
  IRQCallback irq_clear_callback() { return iqr_clear_callback_; }

 private:
  void CheckExtendedRAM();

 protected:
  RomData* rom_data() { return rom_data_; }

 private:
  RomData* rom_data_ = nullptr;
  MirroringChangedCallback mirroring_changed_callback_;
  IRQCallback iqr_callback_;
  IRQCallback iqr_clear_callback_;
  Bytes extended_ram_;
  bool force_use_extended_ram_ = false;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPER_H_
