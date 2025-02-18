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

#include "nes/ppu.h"

#include "base/logging.h"
#include "nes/mapper.h"
#include "nes/palette.h"
#include "nes/ppu_bus.h"
#include "nes/registers.h"

namespace kiwi {
namespace nes {
// Each scanline has 256 dots (pixels).
constexpr int kScanlineVisibleDots = 256;
// Visible scanlines are from 0 to 239.
constexpr int kVisibleScanlines = 240;

PPU::PPU(PPUBus* bus)
    : ppu_bus_(bus), palette_(CreatePaletteFromPPUModel(PPUModel::k2C02)) {
  // Initialize buffers
  for (size_t i = 0; i < kMaxBufferSize; ++i) {
    screenbuffers_[i].resize(kVisibleScanlines * kScanlineVisibleDots);
  }
}

PPU::~PPU() = default;

void PPU::SetPatch(uint32_t crc) {
  crc_ = crc;
  patch_.Set(crc);
}

void PPU::PowerUp() {
  registers_.PPUCTRL.value = registers_.PPUMASK.value =
      registers_.PPUSTATUS.value = registers_.PPUSCROLL = registers_.OAMADDR =
          registers_.PPUADDR = 0;
  pipeline_state_ = PipelineState::kPreRender;
}

void PPU::Reset() {
  patch_.Set(crc_);
  registers_.PPUCTRL.value = registers_.PPUMASK.value =
      registers_.PPUSTATUS.value = registers_.PPUSCROLL = 0;
  pipeline_state_ = PipelineState::kPreRender;
  scanline_ = 0;
  nmi_delay_ = 0;
}

void PPU::Step() {
  // The PPU renders 262 scanlines per frame. Each scanline lasts for 341 PPU
  // clock cycles (113.667 CPU clock cycles; 1 CPU cycle = 3 PPU cycles), with
  // each clock cycle producing one pixel.
  // See https://www.nesdev.org/wiki/PPU_rendering,
  // https://www.nesdev.org/w/images/default/d/d1/Ntsc_timing.png for details.
  constexpr int kScanlineEndCycle = 340;

  if (nmi_delay_ > 0 && --nmi_delay_ == 0)
    cpu_nmi_callback_.Run();

  // Notify when scanline start.
  if (cycles_ == 0) {
    if (observer_)
      observer_->OnPPUScanlineStart(scanline_);
  }

  switch (pipeline_state_) {
    case PipelineState::kPreRender: {
      DCHECK(scanline_ == 0);
      if (cycles_ == 0) {
        if (observer_)
          observer_->OnPPUFrameStart();
      } else if (cycles_ == 1) {
        registers_.PPUSTATUS.V = registers_.PPUSTATUS.S =
            registers_.PPUSTATUS.O = 0;
      } else if (cycles_ == 257 && is_render_enabled()) {  // Dot 257
        data_address_ &= ~0x41f;
        data_address_ |= temp_address_ & 0x41f;
      } else if (cycles_ >= 280 && cycles_ <= 304 && is_render_enabled()) {
        // During dots 280 to 304 of the pre-render scanline (end of vblank):
        // v: GHIA.BC DEF..... <- t: GHIA.BC DEF.....
        data_address_ &= ~0x7be0;
        data_address_ |= temp_address_ & 0x7be0;
      }

      if (cycles_ >= (kScanlineEndCycle -
                      ((!is_even_frame_ && is_render_enabled()) ? 1 : 0))) {
        pipeline_state_ = PipelineState::kRender;
        if (observer_)
          observer_->OnPPUScanlineEnd(261);
        cycles_ = -1;
        scanline_ = 0;
      } else if (cycles_ == patch_.scanline_irq_dot && is_render_enabled()) {
        // add IRQ support for MMC3
        ppu_bus_->GetMapper()->ScanlineIRQ();
      }
    } break;
    case PipelineState::kRender: {
      // These are the visible scanlines, which contain the graphics to be
      // displayed on the screen. This includes the rendering of both the
      // background and the sprites. During these scanlines, the PPU is busy
      // fetching data, so the program should not access PPU memory during this
      // time, unless rendering is turned off.
      // See https://austinmorlan.com/posts/nes_rendering_overview/ for
      // rendering overview.
      if (cycles_ > 0 && cycles_ <= kScanlineVisibleDots) {
        // Cycle 1-256:
        // https://www.nesdev.org/wiki/PPU_rendering#Cycles_1-256
        Byte background_color = 0, sprite_color = 0;
        // The first cycle is the idle cycle, thus |x| equals to |cycles_| - 1.
        int x = cycles_ - 1;
        int y = scanline_;
        bool is_background_opaque = false;
        bool is_sprite_opaque = false;

        if (is_render_background()) {
          // Data address decoding:
          // yyy NN YYYYY XXXXX
          // ||| || ||||| +++++-- coarse X scroll
          // ||| || +++++-------- coarse Y scroll
          // ||| ++-------------- nametable select
          // +++----------------- fine Y scroll

          // PPU addresses within the pattern tables can be decoded as
          // follows:
          //  DCBA98 76543210
          //  ---------------
          //  0HRRRR CCCCPTTT
          //  |||||| |||||+++- T: Fine Y offset
          //  |||||| ||||+---- P: Bit plane (0: "lower"; 1: "upper")
          //  |||||| ++++----- C: Tile column
          //  ||++++---------- R: Tile row
          //  |+-------------- H: Half of pattern table (0: "l"; 1: "r")
          //  +--------------- 0: Pattern table is at $0000-$1FFF
          auto x_fine = (fine_scroll_pos_x_ + x) % 8;
          if (!is_hide_edge_background() || x >= 8) {
            // Punch-out or other games has to adjust its data address to make
            // sure PPU works correctly.
            if (patch_.data_address_patch)
              patch_.data_address_patch(&data_address_);

            // Fetch tile (nametable byte).
            Address pixel_address = 0x2000 | (data_address_ & 0x0fff);
            Byte tile = ppu_bus_->Read(pixel_address);
            // Gets tile address with fine Y scroll
            pixel_address = (tile << 4) + ((data_address_ >> 12) & 0x7);
            pixel_address += background_pattern_table_base_address();

            Byte pattern = ppu_bus_->Read(pixel_address);
            // Combines the tile and get background color index.
            background_color = (pattern >> (7 ^ x_fine)) & 1;
            background_color |=
                ((ppu_bus_->Read(pixel_address + 8) >> (7 ^ x_fine)) & 1) << 1;

            // If |background_color| is not 0, it is opaque.
            is_background_opaque = (background_color != 0);

            // Fetch attribute table and calculate higher two bits of palette:
            // https://www.nesdev.org/wiki/PPU_scrolling#Tile_and_attribute_fetching
            Address attribute_address = 0x23c0 | (data_address_ & 0x0c00) |
                                        ((data_address_ >> 4) & 0x38) |
                                        ((data_address_ >> 2) & 0x07);
            Byte attribute = ppu_bus_->Read(attribute_address);
            int shift = ((data_address_ >> 4) & 4) | (data_address_ & 2);
            // Extract and set the upper two bits for the color
            background_color |= ((attribute >> shift) & 0x3) << 2;
          }

          // Increment/wrap coarse X:
          // https://www.nesdev.org/wiki/PPU_scrolling#Wrapping_around
          if (x_fine == 7) {
            if ((data_address_ & 0x001f) == 31)  // if coarse X == 31
            {
              data_address_ &= ~0x001f;  // coarse X = 0
              data_address_ ^= 0x0400;   // switch horizontal nametable
            } else {
              data_address_ += 1;  // increment coarse X
            }
          }
        }

        // For sprites rendering, see https://www.nesdev.org/wiki/PPU_OAM.
        bool is_sprite_foreground = true;
        if (is_render_sprites() && (!is_hide_edge_sprites() || x >= 8)) {
          for (auto i : secondary_oam_) {
            Byte sprite_x = sprite_memory_[i * 4 + 3];

            if (0 > x - sprite_x || x - sprite_x >= 8)
              continue;

            Byte sprite_y = sprite_memory_[i * 4 + 0] + 1,
                 tile = sprite_memory_[i * 4 + 1],
                 attribute = sprite_memory_[i * 4 + 2];

            // Attribute layout:
            // 76543210
            // ||||||||
            // ||||||++- Palette (4 to 7) of sprite
            // |||+++--- Unimplemented (read 0)
            // ||+------ Priority (0:  front of background; 1: behind
            // ||        background)
            // |+------- Flip sprite horizontally
            // +-------- Flip sprite vertically
            int length = (is_long_sprite()) ? 16 : 8;
            int x_shift = (x - sprite_x) % 8,
                y_offset = (y - sprite_y) % length;

            if ((attribute & 0x40) == 0)  // If NOT flipping horizontally
              x_shift ^= 7;
            if ((attribute & 0x80) != 0)  // IF flipping vertically
              y_offset ^= (length - 1);

            Address pattern_address = 0;

            // For 8x8 sprites, this is the tile number of this sprite within
            // the pattern table selected in bit 3 of PPUCTRL ($2000).
            // For 8x16 sprites, the PPU ignores the pattern table selection and
            // selects a pattern table from bit 0 of this number.
            if (!is_long_sprite()) {
              pattern_address = (tile << 4) + y_offset;
              pattern_address += sprite_pattern_table_base_address();
            } else {
              // bit-3 is one if it is the bottom tile of the sprite, multiply
              // by two to get the next pattern
              y_offset = (y_offset & 7) | ((y_offset & 8) << 1);
              pattern_address = (tile >> 1) * 32 + y_offset;
              pattern_address |= (tile & 1) << 12;
            }

            sprite_color = (ppu_bus_->Read(pattern_address) >> (x_shift)) & 1;
            sprite_color |=
                ((ppu_bus_->Read(pattern_address + 8) >> (x_shift)) & 1) << 1;

            // If |sprite_color| is 0, it means this pixel is transparent.
            is_sprite_opaque = (sprite_color != 0);
            if (!is_sprite_opaque) {
              sprite_color = 0;
              continue;
            }

            // Select sprite palette
            sprite_color |= 0x10;
            // bits 2-3
            sprite_color |= (attribute & 0x3) << 2;
            // Gets priority of the sprite pixel.
            is_sprite_foreground = !(attribute & 0x20);

            // Sets S flag if zero hit.
            if (!registers_.PPUSTATUS.S && is_render_background() && i == 0 &&
                is_sprite_opaque /* && is_background_opaque*/) {
              registers_.PPUSTATUS.S = 1;
            }

            break;
          }
        }

        Byte palette_index = background_color;
        if ((!is_background_opaque && is_sprite_opaque) ||
            (is_background_opaque && is_sprite_opaque &&
             is_sprite_foreground)) {
          palette_index = sprite_color;
        } else if (!is_background_opaque && !is_sprite_opaque) {
          palette_index = 0;
        }

        DCHECK(palette_);
        // Map |palette_index| to PPU memory map's Palette RAM address.
        Color bgra = (palette_->GetColorBGRA(
            ppu_bus_->Read(static_cast<Address>(palette_index | 0x3f00))));
        DCHECK(static_cast<size_t>(y) * kScanlineVisibleDots +
                   static_cast<size_t>(x) <
               screenbuffers_[current_buffer_index_].size());
        screenbuffers_[current_buffer_index_][y * kScanlineVisibleDots + x] =
            bgra;

        if (cycles_ == kScanlineVisibleDots &&
            is_render_background()) {  // Dot 256
          // For increasing Y, see
          // https://www.nesdev.org/wiki/PPU_scrolling#Wrapping_around
          if ((data_address_ & 0x7000) != 0x7000) {  // if fine Y < 7
            data_address_ += 0x1000;                 // increment fine Y
          } else {
            data_address_ &= ~0x7000;                   // fine Y = 0
            int new_y = (data_address_ & 0x03e0) >> 5;  // let new_y = coarse Y
            if (new_y == 29) {
              new_y = 0;                // coarse Y = 0
              data_address_ ^= 0x0800;  // switch vertical nametable
            } else if (new_y == 31) {
              new_y = 0;  // coarse Y = 0, nametable not switched
            } else {
              new_y += 1;  // increment coarse Y
            }
            // put coarse Y back into |data_address_|.
            data_address_ = (data_address_ & ~0x03e0) | (new_y << 5);
          }
        }
      } else if (cycles_ == 257 && is_render_background()) {  // Dot 257
        data_address_ &= ~0x41f;
        data_address_ |= temp_address_ & 0x41f;
      }

      if (cycles_ == patch_.scanline_irq_dot && is_render_enabled()) {
        ppu_bus_->GetMapper()->ScanlineIRQ();
      }

      if (cycles_ >= kScanlineEndCycle) {
        // Each scanline, the PPU reads the spritelist (that is, Object
        // Attribute Memory) to see which to draw:
        // 1. it clears the list of sprites to draw.
        // 2. it reads through OAM, checking which sprites will be on this
        // scanline. It chooses the first eight it finds that do.
        // 3. if eight sprites were found, it checks (in a wrongly-implemented
        // fashion) for further sprites on the scanline to see if the sprite
        // overflow flag should be set.
        // 4. using the details for the eight (or fewer) sprites chosen, it
        // determines which pixels each has on the scanline and where to draw
        // them.
        secondary_oam_.resize(0);

        Byte range = is_long_sprite() ? 16 : 8;
        std::size_t j = 0;
        for (std::size_t i = sprite_data_address_ / 4; i < 64; ++i) {
          auto diff = (scanline_ - sprite_memory_[i * 4]);
          if (0 <= diff && diff < range) {
            // Sprite overflow shouldn't be set when all rendering is off
            if (j >= 8 && is_render_enabled()) {
              registers_.PPUSTATUS.O = 1;
              break;
            }
            secondary_oam_.push_back(static_cast<Byte>(i));
            ++j;
          }
        }

        IncreaseScanline();
      }

      if (scanline_ >= kVisibleScanlines)
        pipeline_state_ = PipelineState::kPostRender;
    } break;
    case PipelineState::kPostRender: {
      if (cycles_ >= kScanlineEndCycle) {
        IncreaseScanline();
        pipeline_state_ = PipelineState::kVerticalBlank;

        if (observer_) {
          observer_->OnRenderReady(screenbuffers_[current_buffer_index_]);
          current_buffer_index_ = (current_buffer_index_ + 1) % kMaxBufferSize;
        }
      }
    } break;
    case PipelineState::kVerticalBlank: {
      // The VBlank flag of the PPU is set at tick 1 (the second tick) of
      // scanline 241, where the VBlank NMI also occurs. The PPU makes no memory
      // accesses during these scanlines, so PPU memory can be freely accessed
      // by the program.
      if (cycles_ == 1 && scanline_ == 241) {
        registers_.PPUSTATUS.V = 1;
        if (registers_.PPUCTRL.V) {
          NMIChange();
        }
      }

      if (cycles_ >= kScanlineEndCycle) {
        IncreaseScanline();
      }

      if (scanline_ >= 261) {
        pipeline_state_ = PipelineState::kPreRender;
        scanline_ = 0;
        if (observer_)
          observer_->OnPPUFrameEnd();

        is_even_frame_ = !is_even_frame_;
      }
    } break;
    default:
      LOG(ERROR) << "Invalid pipeline state: "
                 << static_cast<int>(pipeline_state_);
      break;
  }

  ++cycles_;
  if (observer_) {
    observer_->OnPPUStepped();
  }
}

void PPU::StepScanline() {
  int s = scanline_;
  while (scanline_ == s) {
    Step();
  }
}

Byte PPU::Read(Address address) {
  switch (static_cast<PPURegister>(address)) {
    case PPURegister::PPUCTRL:
    case PPURegister::PPUMASK:
    case PPURegister::OAMADDR:
    case PPURegister::PPUSCROL:
    case PPURegister::PPUADDR:
      return data_buffer_;
    case PPURegister::PPUSTATUS:
      return GetStatus();
    case PPURegister::OAMDATA:
      return GetOAMData();
    case PPURegister::PPUDATA:
      return GetData();
    default:
      LOG(ERROR) << "Can't read address $" << Hex<16>{address} << ".";
      break;
  }
  return 0;
}

void PPU::Write(Address address, Byte value) {
  switch (static_cast<PPURegister>(address)) {
    case PPURegister::PPUCTRL:
      return SetCtrl(value);
    case PPURegister::PPUMASK:
      return SetMask(value);
    case PPURegister::PPUSTATUS:
      LOG(WARNING) << "PPUSTATUS is readonly.";
      break;
    case PPURegister::OAMADDR:
      return SetOAMAddress(value);
    case PPURegister::OAMDATA:
      return SetOAMData(value);
    case PPURegister::PPUSCROL:
      return SetScroll(value);
    case PPURegister::PPUADDR:
      return SetDataAddress(value);
    case PPURegister::PPUDATA:
      return SetData(value);
    default:
      LOG(ERROR) << "Can't write address $" << Hex<16>{address} << ".";
      break;
  }
}

void PPU::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(registers_)
      .WriteData(temp_address_)
      .WriteData(data_address_)
      .WriteData(sprite_data_address_)
      .WriteData(fine_scroll_pos_x_)
      .WriteData(data_buffer_)
      .WriteData(write_toggle_)
      .WriteData(nmi_delay_)
      .WriteData(sprite_memory_)
      .WriteData(pipeline_state_)
      .WriteData(cycles_)
      .WriteData(scanline_)
      .WriteData(is_even_frame_);
}

bool PPU::Deserialize(const EmulatorStates::Header& header,
                      EmulatorStates::DeserializableStateData& data) {
  if (header.version == 1) {
    data.ReadData(&registers_)
        .ReadData(&temp_address_)
        .ReadData(&data_address_)
        .ReadData(&sprite_data_address_)
        .ReadData(&fine_scroll_pos_x_)
        .ReadData(&data_buffer_)
        .ReadData(&write_toggle_)
        .ReadData(&nmi_delay_)
        .ReadData(&sprite_memory_)
        .ReadData(&pipeline_state_)
        .ReadData(&cycles_)
        .ReadData(&scanline_)
        .ReadData(&is_even_frame_);
    return true;
  }
  return false;
}

void PPU::DMA(Byte* source) {
  // The DMA transfer will begin at the current OAM write address.
  std::memcpy(sprite_memory_ + sprite_data_address_, source,
              256 - sprite_data_address_);

  if (sprite_data_address_) {
    std::memcpy(sprite_memory_, source + (256 - sprite_data_address_),
                sprite_data_address_);
  }
}

void PPU::SetObserver(PPUObserver* observer) {
  observer_ = observer;
}

void PPU::RemoveObserver() {
  observer_ = nullptr;
}

Byte PPU::GetStatus() {
  PPURegisters::PPUSTATUS_t status = registers_.PPUSTATUS;
  // Do not copy open bus.
  status.value &= 0xe0;

  // w:                  <- 0
  registers_.PPUSTATUS.V = 0;
  write_toggle_ = false;
  return status.value;
}

Byte PPU::GetData() {
  // The PPUDATA read buffer (post-fetch).
  auto data = ppu_bus_->Read(data_address_);

  if (data_address_ < 0x3f00) {
    // Return from the data buffer and store the current value in the buffer
    std::swap(data, data_buffer_);
  }

  data_address_ += data_address_increment();

  // Notify VRAM address changed, typically for MMC3.
  ppu_bus_->GetMapper()->PPUAddressChanged(data_address_);

  // Reading palette data from $3F00-$3FFF works differently. The palette data
  // is placed immediately on the data bus, and hence no priming read is
  // required.
  return data;
}

Byte PPU::ReadOAMData(Byte address) {
  return sprite_memory_[address];
}

Byte PPU::GetOAMData() {
  return ReadOAMData(sprite_data_address_);
}

void PPU::SetCtrl(Byte ctrl) {
  bool v_has_changed = !(registers_.PPUCTRL.value & 0x8) && (ctrl & 0x8);
  registers_.PPUCTRL.value = ctrl;

  // t: ...GH.. ........ <- d: ......GH
  //  <used elsewhere> <- d: ABCDEF..
  temp_address_ &= ~0xc00;              // Unset
  temp_address_ |= (ctrl & 0x3) << 10;  // Set according to ctrl bits

  // If the PPU is currently in vertical blank, and the PPUSTATUS ($2002) vblank
  // flag is still set (1), changing the NMI flag in bit 7 of $2000 from 0 to 1
  // will immediately generate an NMI.
  if (pipeline_state_ == PipelineState::kVerticalBlank &&
      registers_.PPUSTATUS.V == 1 && v_has_changed) {
    NMIChange();
  }
}

void PPU::SetMask(Byte mask) {
  registers_.PPUMASK.value = mask;
}

void PPU::SetDataAddress(Byte address) {
  // $2006 first write (w is 0)
  //   t: .CDEFGH ........ <- d: ..CDEFGH
  //          <unused>     <- d: AB......
  //   t: Z...... ........ <- 0 (bit Z is cleared)
  //   w:                  <- 1
  // $2006 second write (w is 1)
  //   t: ....... ABCDEFGH <- d: ABCDEFGH
  //   v: <...all bits...> <- t: <...all bits...>
  //   w:                  <- 0
  if (!write_toggle_) {
    temp_address_ =
        (temp_address_ & 0x00ff) | ((static_cast<Byte>(address) & 0x3f) << 8);
    write_toggle_ = true;
  } else {
    temp_address_ = (temp_address_ & 0xff00) | static_cast<Byte>(address);
    data_address_ = temp_address_;
    write_toggle_ = false;

    // Notify VRAM address changed, typically for MMC3.
    ppu_bus_->GetMapper()->PPUAddressChanged(data_address_);

    if (observer_)
      observer_->OnPPUADDR(data_address_);
  }
}

void PPU::SetOAMAddress(Byte address) {
  sprite_data_address_ = address;
}

void PPU::SetScroll(Byte scroll) {
  //$2005 first write (w is 0)
  //  t: ....... ...ABCDE <- d: ABCDE...
  //  x:              FGH <- d: .....FGH
  //  w:                  <- 1
  //$2005 second write (w is 1)
  //  t: FGH..AB CDE..... <- d: ABCDEFGH
  //  w:                  <- 0
  if (!write_toggle_) {
    temp_address_ &= ~0x1f;
    temp_address_ |= (scroll >> 3) & 0x1f;
    fine_scroll_pos_x_ = scroll & 0x7;
    write_toggle_ = true;
  } else {
    temp_address_ &= ~0x73e0;
    temp_address_ |= ((scroll & 0x7) << 12) | ((scroll & 0xf8) << 2);
    write_toggle_ = false;
  }
}

void PPU::SetData(Byte data) {
  if (write_toggle()) {
    LOG(WARNING) << "Attempting to write $" << Hex<16>{data}
                 << " to PPU address $" << Hex<16>{data}
                 << ", but PPUADDR is still in writing. This usually indicates "
                    "an error.";
  }

  ppu_bus_->Write(data_address_, data);
  data_address_ += data_address_increment();

  // Notify VRAM address changed, typically for MMC3.
  ppu_bus_->GetMapper()->PPUAddressChanged(data_address_);
}

void PPU::SetOAMData(Byte data) {
  sprite_memory_[sprite_data_address_++] = data;
}

void PPU::IncreaseScanline() {
  if (observer_)
    observer_->OnPPUScanlineEnd(scanline_);

  ++scanline_;
  cycles_ = -1;
}

void PPU::NMIChange() {
  nmi_delay_ = 15;
}

}  // namespace nes
}  // namespace kiwi
