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

#ifndef NES_PPU_H_
#define NES_PPU_H_

#include "base/check.h"
#include "nes/cpu_bus.h"
#include "nes/emulator_states.h"
#include "nes/ppu_observer.h"
#include "nes/ppu_patch.h"
#include "nes/registers.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {
class Bus;
class CPU;
class Palette;

class PPU : public Device, public EmulatorStates::SerializableState {
 public:
  enum class PipelineState {
    kPreRender,
    kRender,
    kPostRender,
    kVerticalBlank,
  };

  explicit PPU(Bus* ppu_bus);
  ~PPU() override;

 public:
  void SetPatch(uint32_t crc);
  // Power up and reset states:
  // See https://www.nesdev.org/wiki/PPU_power_up_state for more details.
  void PowerUp();
  void Reset();
  void Step();
  void StepScanline();
  void DMA(Byte* source);
  PPURegisters registers() { return registers_; }
  Address data_address() { return data_address_; }
  Address sprite_data_address() { return sprite_data_address_; }
  Palette* palette() { return palette_.get(); }
  bool write_toggle() { return write_toggle_; }
  const Colors& current_frame() { return screenbuffer_; }
  int pixel() { return cycles_; }
  int scanline() {
    return pipeline_state_ == PipelineState::kPreRender ? 261 : scanline_;
  }
  void set_cpu_nmi_callback(base::RepeatingClosure callback) {
    cpu_nmi_callback_ = callback;
  }
  PPUPatch* patch() { return &patch_; }

  void SetObserver(PPUObserver* observer);
  void RemoveObserver();
  Byte ReadOAMData(Byte address);

 public:
  // Device:
  Byte Read(Address address) override;
  void Write(Address address, Byte value) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  Byte GetStatus();
  Byte GetData();
  Byte GetOAMData();

  void SetCtrl(Byte ctrl);
  void SetMask(Byte mask);
  void SetDataAddress(Byte address);
  void SetOAMAddress(Byte address);
  void SetScroll(Byte scroll);
  void SetData(Byte data);
  void SetOAMData(Byte data);

  Byte data_address_increment() { return registers_.PPUCTRL.I ? 0x20 : 0x1; }
  bool is_render_background() { return registers_.PPUMASK.b; }
  bool is_render_sprites() { return registers_.PPUMASK.s; }
  bool is_hide_edge_background() { return !registers_.PPUMASK.m; }
  bool is_hide_edge_sprites() { return !registers_.PPUMASK.M; }
  // Returns true if background or sprites will render.
  bool is_render_enabled() {
    return is_render_background() || is_render_sprites();
  }
  Address background_pattern_table_base_address() {
    return registers_.PPUCTRL.B == 0 ? 0x0000 : 0x1000;
  }
  Address sprite_pattern_table_base_address() {
    return registers_.PPUCTRL.S == 0 ? 0x0000 : 0x1000;
  }
  bool is_long_sprite() { return !!registers_.PPUCTRL.H; }

  // Increase scanline and notify observers that the scanline has finished.
  void IncreaseScanline();

  void NMIChange();

 private:
  base::RepeatingClosure cpu_nmi_callback_;
  Bus* ppu_bus_ = nullptr;
  PPURegisters registers_{};

  // Internal registers
  Address temp_address_ = 0;
  Address data_address_ = 0;
  Byte sprite_data_address_ = 0;
  Byte fine_scroll_pos_x_ = 0;
  Byte data_buffer_ = 0xff;
  bool write_toggle_ = false;
  int nmi_delay_ = 0;

  // The OAM (Object Attribute Memory) is internal memory inside the PPU that
  // contains a display list of up to 64 sprites, where each sprite's
  // information occupies 4 bytes.
  Byte sprite_memory_[64 * 4] = {0};
  Bytes secondary_oam_;

  PipelineState pipeline_state_ = PipelineState::kPreRender;
  int cycles_ = 0;
  int scanline_ = 0;
  bool is_even_frame_ = false;
  std::unique_ptr<Palette> palette_;
  // |screenbuffer_| is the current framebuffer that PPU is writing to.
  Colors screenbuffer_;

  PPUPatch patch_;
  uint32_t crc_;

  PPUObserver* observer_ = nullptr;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_PPU_H_
