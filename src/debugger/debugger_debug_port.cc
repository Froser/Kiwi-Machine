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

#include "debugger/debugger_debug_port.h"

#include <assert.h>
#include <bitset>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "nes/debug/debug_port.h"
#include "nes/registers.h"

#if USE_QT6
#include <QImage>
#include <QPainter>
#endif

#define CHECK_BREAK()                                                   \
  if (break_) {                                                         \
    break_ = false;                                                     \
    std::cout << "Breakpoint triggered:" << break_reason_ << std::endl; \
    break;                                                              \
  }                                                                     \
  if (break_because_nmi_) {                                             \
    break_ = true;                                                      \
    break_reason_ = "NMI occurs.";                                      \
    break_because_nmi_ = false;                                         \
  }

// Gets 4 bits from Byte:
#define HIGH_HALF_BYTE(x) (static_cast<kiwi::nes::Byte>(x >> 4))
#define LOW_HALF_BYTE(x) (static_cast<kiwi::nes::Byte>(x & 0x7))

// Gets Byte from Address:
#define HIGH_BYTE(x) static_cast<kiwi::nes::Byte>((x >> 8))
#define LOW_BYTE(x) static_cast<kiwi::nes::Byte>((x & 0xff))

namespace {
bool IsQtBackend() {
  return kiwi::base::GetPlatformFactoryBackend() ==
         kiwi::base::PlatformFactoryBackend::kQt6;
}

void PrintSeparator() {
  std::cout << std::endl << "==================" << std::endl;
}

template <int w>
struct TypeMatcher;

template <>
struct TypeMatcher<8> {
  using Type = uint8_t;
};
template <>
struct TypeMatcher<16> {
  using Type = uint16_t;
};
template <>
struct TypeMatcher<32> {
  using Type = uint32_t;
};

template <int w>
struct Hex {
  static constexpr int W = w;
  typename TypeMatcher<w>::Type hex;
};

template <int w>
std::ostream& operator<<(std::ostream& s, const Hex<w>& h) {
  return s << std::hex << std::setfill('0') << std::setw(Hex<w>::W / 4)
           << static_cast<int>(h.hex);
}

#if USE_QT6
// https://24ways.org/2010/calculating-color-contrast
bool IsColorTooLight(QColor color) {
  return ((color.red() * 299 + color.green() * 587 + color.blue() * 114) /
          1000) >= 128;
}

void DrawPaletteIndex(QPainter& painter,
                      kiwi::nes::Byte index,
                      QColor palette_color,
                      int palette_x,
                      int palette_y,
                      int palette_width,
                      int palette_height) {
  std::stringstream ss;
  ss << Hex<8>{index};
  std::string str = ss.str();

  painter.save();
  QPen p;
  p.setColor(IsColorTooLight(palette_color) ? QColorConstants::Black
                                            : QColorConstants::White);
  p.setStyle(Qt::SolidLine);
  painter.setPen(p);

  constexpr int kFontDefaultPointSize = 12;
  QFont font;
  font.setPixelSize(kFontDefaultPointSize);
  QFontMetrics metrics(font, painter.device());
  QRect bounds =
      metrics.boundingRect(QString::fromUtf8(str.c_str(), str.size()));
  painter.setFont(font);
  painter.drawText(QPoint(palette_x + (palette_width - bounds.width()) / 2,
                          palette_y + (palette_height - bounds.height()) / 2),
                   QString::fromUtf8(ss.str()));
  painter.restore();
}
#endif

void PrettyPrintRegisters(const kiwi::nes::CPURegisters& registers) {
  std::cout << "A: $" << Hex<8>{registers.A} << " (" << std::dec
            << static_cast<int>(registers.A) << ") " << std::endl;
  std::cout << "X: $" << Hex<8>{registers.X} << " (" << std::dec
            << static_cast<int>(registers.X) << ") " << std::endl;
  std::cout << "Y: $" << Hex<8>{registers.Y} << " (" << std::dec
            << static_cast<int>(registers.Y) << ") " << std::endl;
  std::cout << "PC: $" << Hex<16>{registers.PC} << std::endl;
  std::cout << "Stack Pointer: $" << Hex<8>{registers.S} << " (" << std::dec
            << static_cast<int>(registers.S) << ") " << std::endl;

  std::cout << "State flags: $" << Hex<8>{registers.P.value} << std::endl;
  std::cout
      << " NVBB DIZC (N=negative, V=overflow, B=break, D=decimal, I=Interrupt "
         "disabled, Z=zero, C=carry)"
      << std::endl
      << " " << std::bitset<4>{HIGH_HALF_BYTE(registers.P.value)} << " "
      << std::bitset<4>{LOW_HALF_BYTE(registers.P.value)} << std::endl;
}

void PrettyPrintRegisters(const kiwi::nes::CPUContext& cpu_context) {
  PrettyPrintRegisters(cpu_context.registers);
}

void PrettyPrintRegisters(const kiwi::nes::PPURegisters& registers) {
  std::cout << "PPUCTRL flags: $" << Hex<8>{registers.PPUCTRL.value}
            << std::endl;
  std::cout
      << " VPHB SINN (V=gen NMI, P=master/slave, H=sprite size, B=bg pattern "
         "addr, S=sprite pattern addr, I=VRAM addr inc, N=base nametable addr)"
      << std::endl
      << " " << std::bitset<4>{HIGH_HALF_BYTE(registers.PPUCTRL.value)} << " "
      << std::bitset<4>{LOW_HALF_BYTE(registers.PPUCTRL.value)} << std::endl;

  std::cout << "PPUMASK flags: $" << Hex<8>{registers.PPUMASK.value}
            << std::endl;
  std::cout << " BGRs bMmg (B=blue, G=green, R=red, s=sprites, b=bg, M=sprites "
               "leftmost 8px, m=bg leftmost 8px, g=Grayscale)"
            << std::endl
            << " " << std::bitset<4>{HIGH_HALF_BYTE(registers.PPUMASK.value)}
            << " " << std::bitset<4>{LOW_HALF_BYTE(registers.PPUMASK.value)}
            << std::endl;

  std::cout << "PPUSTATUS flags: $" << Hex<8>{registers.PPUSTATUS.value}
            << std::endl;
  std::cout << " VSOB BBBB (V=Vertical blank started, S=Sprite zero hit, "
               "O=Sprite overflow, B=PPU Open bus)"
            << std::endl
            << " " << std::bitset<4>{HIGH_HALF_BYTE(registers.PPUSTATUS.value)}
            << " " << std::bitset<4>{LOW_HALF_BYTE(registers.PPUSTATUS.value)}
            << std::endl;
}

void PrettyPrintRegisters(const kiwi::nes::PPUContext& ppu_context) {
  std::cout << "PPUADDR: $" << Hex<16>{ppu_context.data_address}
            << ". Is writing: "
            << (ppu_context.is_data_address_writing ? "true" : "false")
            << std::endl;

  PrettyPrintRegisters(ppu_context.registers);
  std::cout << "Scanline: " << std::dec << ppu_context.scanline << std::endl;
  std::cout << "Pixel: " << std::dec << ppu_context.pixel << std::endl;
}

void PrettyPrintCartridge(const kiwi::nes::RomData* rom_data) {
  PrintSeparator();
  std::cout << "ROM loaded: " << std::endl;
  std::cout << "ROM format: " << (rom_data->is_nes_20 ? "NES 2.0" : "iNES")
            << std::endl;
  std::cout << "CHR size: $"
            << Hex<16>{(kiwi::nes::Address)(rom_data->CHR.size())} << "("
            << std::dec << rom_data->CHR.size() / 1024 << " KB)" << std::endl;
  std::cout << "PRG size: $"
            << Hex<16>{(kiwi::nes::Address)(rom_data->PRG.size())} << "("
            << std::dec << rom_data->PRG.size() / 1024 << " KB)" << std::endl;
  std::cout << "Mapper: " << static_cast<int>(rom_data->mapper) << std::endl;
  std::cout << "Nametable mirroring: ";
  switch (rom_data->name_table_mirroring) {
    case kiwi::nes::NametableMirroring::kHorizontal:
      std::cout << "Horizontal";
      break;
    case kiwi::nes::NametableMirroring::kVertical:
      std::cout << "Vertical";
      break;
    case kiwi::nes::NametableMirroring::kFourScreen:
      std::cout << "Four screen";
      break;
    case kiwi::nes::NametableMirroring::kOneScreenLower:
      std::cout << "One screen lower";
      break;
    case kiwi::nes::NametableMirroring::kOneScreenHigher:
      std::cout << "One screen higher";
      break;
    default:
      std::cout << "Invalid nametable mirroring.";
      break;
  }
  std::cout << std::endl;

  switch (rom_data->console_type) {
    case kiwi::nes::ConsoleType::kNESFC:
      std::cout << "Nintendo Entertainment System/Family Computer";
      break;
    case kiwi::nes::ConsoleType::kNVS:
      std::cout << "Nintendo Vs. System";
      break;
    case kiwi::nes::ConsoleType::kPlaychoice10:
      std::cout << "Nintendo Playchoice 10";
      break;
    case kiwi::nes::ConsoleType::kExtend:
      std::cout << "Extended Console Type";
      break;
    default:
      std::cout << "Unknown console system.";
      break;
  }
  std::cout << std::endl;

  std::cout << "Has battery or other non-volatile memory: " << std::boolalpha
            << rom_data->has_extended_ram << std::endl
            << std::endl;
}

}  // namespace

DebuggerDebugPort::DebuggerDebugPort(kiwi::nes::Emulator* emulator)
    : DebugPort(emulator) {}
DebuggerDebugPort::~DebuggerDebugPort() = default;

void DebuggerDebugPort::OnCPUReset(const kiwi::nes::CPUContext& cpu_context) {
  PrintSeparator();
  std::cout << "CPU has been reset: " << std::endl;
  PrettyPrintRegisters(cpu_context);
}

void DebuggerDebugPort::OnPPUReset(const kiwi::nes::PPUContext& ppu_context) {
  PrintSeparator();
  std::cout << "PPU has been reset: " << std::endl;
  PrettyPrintRegisters(ppu_context);
}

void DebuggerDebugPort::OnRomLoaded(bool success,
                                    const kiwi::nes::RomData* rom_data) {
  rom_data_ = rom_data;
  PrettyPrintCartridge(rom_data);
}

void DebuggerDebugPort::OnPPUADDR(kiwi::nes::Address address) {
  if (breakpoints_PPUADDR_.find(address) != breakpoints_PPUADDR_.end()) {
    break_ = true;
    std::stringstream s;
    s << Hex<16>{address};
    break_reason_ = "PPUADDR is set to " + s.str();
  }
}

void DebuggerDebugPort::OnCPUNMI() {
  if (break_on_nmi_) {
    // NMI will be pending until next cycle.
    break_because_nmi_ = true;
  }
}

void DebuggerDebugPort::OnScanlineStart(int scanline) {
  if (breakpoints_scanline_start_.find(scanline) !=
      breakpoints_scanline_start_.end()) {
    break_ = true;
    break_reason_ =
        "Scanline started at " + kiwi::base::NumberToString(scanline);
  }
}

void DebuggerDebugPort::OnScanlineEnd(int scanline) {
  ++scanline_counter_;
  if (breakpoints_scanline_end_.find(scanline) !=
      breakpoints_scanline_end_.end()) {
    break_ = true;
    break_reason_ = "Scanline ended at " + kiwi::base::NumberToString(scanline);
  }
}

void DebuggerDebugPort::OnFrameEnd() {
  ++frame_counter_;
}

int64_t DebuggerDebugPort::StepToNextCPUInstruction() {
  int64_t cycle = 1;
  emulator()->Step();
  kiwi::nes::CPUContext context = GetCPUContext();
  while (context.last_action.cycles_to_wait) {
    emulator()->Step();
    context = GetCPUContext();
    ++cycle;
  }
  return cycle;
}

int64_t DebuggerDebugPort::StepInstructionCount(uint64_t count) {
  int64_t cycle = 0;
  for (uint64_t i = 0; i < count; ++i) {
    if (count - i < 3) {
      PrintDisassembly(GetCPUContext().registers.PC, 1);
    }
    cycle += StepToNextCPUInstruction();
    CHECK_BREAK();
  }
  return cycle;
}

int64_t DebuggerDebugPort::StepToInstruction(kiwi::nes::Byte opcode) {
  // Step to the next instruction |opcode| before it runs.
  int64_t cycle = 0;
  while (opcode != CPUReadByte(GetCPUContext().registers.PC)) {
    cycle += StepToNextCPUInstruction();

    CHECK_BREAK();
  }
  return cycle;
}

int64_t DebuggerDebugPort::StepToNextScanline(uint64_t scanline) {
  int64_t cycle = 0;
  uint64_t scanline_now = scanline_counter_;
  while (scanline_counter_ - scanline_now < scanline) {
    cycle += StepToNextCPUInstruction();

    CHECK_BREAK();
  }
  return cycle;
}

int64_t DebuggerDebugPort::StepToNextFrame(uint64_t frame) {
  int64_t cycle = 0;
  uint64_t frame_now = frame_counter_;
  while (frame_counter_ - frame_now < frame) {
    cycle += StepToNextCPUInstruction();

    CHECK_BREAK();
  }
  return cycle;
}

void DebuggerDebugPort::PrintCPURegisters() {
  PrettyPrintRegisters(GetCPUContext());
}

void DebuggerDebugPort::PrintPPURegisters() {
  PrettyPrintRegisters(GetPPUContext());
}

void DebuggerDebugPort::PrintDisassembly(kiwi::nes::Address address,
                                         int instruction_count) {
  for (int i = 0; i < instruction_count; ++i) {
    kiwi::nes::Disassembly disassembly = kiwi::nes::Disassemble(this, address);
    if (i == 0) {
      std::cout << "--> ";
    } else {
      std::cout << "    ";
    }
    std::cout << "$" << Hex<16>{address} << ": " << Hex<8>{disassembly.opcode};
    if (disassembly.operand_size == 1) {
      std::cout << " " << Hex<8>{LOW_BYTE(disassembly.operand)} << "     ";
    } else if (disassembly.operand_size == 2) {
      std::cout << " " << Hex<8>{LOW_BYTE(disassembly.operand)} << " "
                << Hex<8>{HIGH_BYTE(disassembly.operand)} << "  ";
    } else {
      assert(disassembly.operand_size == 0);
      std::cout << "        ";
    }
    std::cout << " <" << std::dec << static_cast<int>(disassembly.cycle) << "> "
              << disassembly.pretty_print << std::endl;

    if (disassembly.next_instruction < address) {
      // Overflow, break.
      break;
    } else {
      address = disassembly.next_instruction;
    }
  }
}

void DebuggerDebugPort::PrintROM() {
  if (!rom_data_) {
    std::cout << "No rom is loaded." << std::endl;
  } else {
    PrettyPrintCartridge(rom_data_);
  }
}

void DebuggerDebugPort::PrintPatternTable() {
  kiwi::nes::Colors color_indices =
      GetPatternTableBGRA(kiwi::nes::PaletteName::kIndexOnly);
  static char s_map[] = {' ', '1', '2', '3'};
  for (kiwi::nes::Address i = 0; i < 128 * 128 * 2; ++i) {
    assert(color_indices[i] < 4);
    std::cout << s_map[color_indices[i]];
    if ((i + 1) % 256 == 0)
      std::cout << std::endl;
  }
}

kiwi::base::FilePath DebuggerDebugPort::SavePatternTable(
    const kiwi::base::FilePath& file_path) {
#if USE_QT6
  if (IsQtBackend()) {
    // Save all 8 pattern tables and palettes in a big canvas.
    constexpr int kPatternTableWidth = 256;
    constexpr int kPatternTableHeight = 128;
    constexpr int kMargin = 10;
    constexpr int kSpacing = 10;
    constexpr int kPaletteTileSize =
        32;  // A palette is presented as a 32*32 square.
    constexpr int kPaletteSpacingBetweenPatternTable = 32;
    constexpr int kHeight =
        kMargin * 2 + 8 * kPatternTableHeight + 7 * kSpacing;
    constexpr int kWidth = kMargin * 2 + kPatternTableWidth +
                           kPaletteSpacingBetweenPatternTable +
                           kPaletteTileSize * 4;

    QImage image(QSize(kWidth, kHeight), QImage::Format_ARGB32);
    QPainter painter(&image);
    {
      painter.save();
      QBrush b;
      b.setStyle(Qt::BrushStyle::SolidPattern);
      b.setColor(QColorConstants::Gray);
      painter.setBrush(b);
      painter.drawRect(QRect(0, 0, kWidth, kHeight));
      painter.restore();
    }

    // kBackgroundPalette0 - kSpritePalette3
    int offset_y = kMargin;

    kiwi::nes::Palette* palette = GetPPUContext().palette;
    assert(palette);

    for (int i = 0; i < 8; ++i) {
      // Draw each pattern table.
      kiwi::nes::Colors bgra =
          GetPatternTableBGRA(static_cast<kiwi::nes::PaletteName>(i));

      {
        painter.save();
        QImage pattern_table(reinterpret_cast<uchar*>(bgra.data()),
                             kPatternTableWidth, kPatternTableHeight,
                             sizeof(bgra[0]) * kPatternTableWidth,
                             QImage::Format_ARGB32);
        painter.drawImage(QPoint(kMargin, offset_y), pattern_table);
        painter.restore();
      }

      // Draw each palette.
      int palette_x =
          kMargin + kPatternTableWidth + kPaletteSpacingBetweenPatternTable;
      int palette_y = offset_y + (kPatternTableHeight - kPaletteTileSize) / 2;
      kiwi::nes::Byte indices[4];
      GetPaletteIndices(static_cast<kiwi::nes::PaletteName>(i), indices);
      for (unsigned char index : indices) {
        {
          painter.save();
          QBrush b;
          kiwi::nes::Color palette_bgra = palette->GetColorBGRA(index);
          b.setStyle(Qt::SolidPattern);
          b.setColor(QColor::fromRgb(
              (palette_bgra >> 16) & 0xff, (palette_bgra >> 8) & 0xff,
              palette_bgra & 0xff, (palette_bgra >> 24) & 0xff));
          painter.setBrush(b);
          painter.drawRect(
              QRect(palette_x, palette_y, kPaletteTileSize, kPaletteTileSize));

          DrawPaletteIndex(painter, index, b.color(), palette_x, palette_y,
                           kPaletteTileSize, kPaletteTileSize);
          painter.restore();
        }

        palette_x += kPaletteTileSize;
      }

      offset_y += kPatternTableHeight + kSpacing;
    }

    kiwi::base::FilePath save_path =
        file_path.empty()
            ? kiwi::base::FilePath::FromUTF8Unsafe("pattern_table.png")
            : file_path;
    image.save(QString::fromUtf8(save_path.AsUTF8Unsafe()));
    return save_path;
  }
#endif
  std::cout << "Save pattern table is not supported without Qt backend."
            << std::endl;
  return kiwi::base::FilePath();
}

kiwi::base::FilePath DebuggerDebugPort::SaveNametable(
    const kiwi::base::FilePath& file_path) {
#if USE_QT6
  if (IsQtBackend()) {
    constexpr int kWidth = 256 * 2;
    constexpr int kHeight = 240 * 2;

    QImage image(QSize(kWidth, kHeight), QImage::Format_ARGB32);
    QPainter painter(&image);

    kiwi::nes::Colors bgra = GetNametableBGRA();
    QImage nametable(reinterpret_cast<uchar*>(bgra.data()), kWidth, kHeight,
                     sizeof(bgra[0]) * kWidth, QImage::Format_ARGB32);
    painter.drawImage(0, 0, nametable);

    kiwi::base::FilePath save_path =
        file_path.empty()
            ? kiwi::base::FilePath::FromUTF8Unsafe("nametable.png")
            : file_path;
    image.save(QString::fromUtf8(save_path.AsUTF8Unsafe()));
    return save_path;
  }
#endif
  std::cout << "Save nametable is not supported without Qt backend."
            << std::endl;
  return kiwi::base::FilePath();
}

kiwi::base::FilePath DebuggerDebugPort::SaveSprites(
    const kiwi::base::FilePath& file_path) {
#if USE_QT6
  if (IsQtBackend()) {
    constexpr int kSpriteWidth = 8;
    const int kSpriteHeight = GetPPUContext().registers.PPUCTRL.H ? 16 : 8;
    constexpr int kCanvasWidth = kSpriteWidth * 8;
    const int kCanvasHeight = kSpriteHeight * 8;

    QImage image(QSize(kCanvasWidth, kCanvasHeight), QImage::Format_ARGB32);
    QPainter painter(&image);

    for (kiwi::nes::Byte i = 0; i < 64; ++i) {
      kiwi::nes::Sprite sprite = GetSpriteInfo(i);

      painter.save();
      QImage sprite_image(reinterpret_cast<uchar*>(sprite.bgra.data()),
                          kSpriteWidth, kSpriteHeight,
                          sizeof(sprite.bgra[0]) * kSpriteWidth,
                          QImage::Format_ARGB32);
      painter.drawImage(
          QPoint(((i % 8) * kSpriteWidth), ((i / 8) * kSpriteHeight)),
          sprite_image);
      painter.restore();
    }

    kiwi::base::FilePath save_path =
        file_path.empty() ? kiwi::base::FilePath::FromUTF8Unsafe("sprite.png")
                          : file_path;

    image.save(QString::fromUtf8(save_path.AsUTF8Unsafe()));
    return save_path;
  }
#endif
  std::cout << "Save palette is not supported without Qt backend." << std::endl;
  return kiwi::base::FilePath();
}

kiwi::base::FilePath DebuggerDebugPort::SavePalette(
    const kiwi::base::FilePath& file_path) {
#if USE_QT6
  if (IsQtBackend()) {
    kiwi::nes::Palette* palette = GetPPUContext().palette;
    Q_ASSERT(palette);
    constexpr int kPaletteTileSize = 32;
    constexpr int kTilePerRow = 16;
    constexpr int kTileCount = 0x40;

    QImage image(QSize(kPaletteTileSize * kTilePerRow,
                       kPaletteTileSize * kTileCount / kTilePerRow),
                 QImage::Format_ARGB32);
    QPainter painter(&image);
    for (kiwi::nes::Byte i = 0x00; i < 0x40; ++i) {
      painter.save();

      QBrush b;
      kiwi::nes::Color palette_bgra = palette->GetColorBGRA(i);
      b.setStyle(Qt::SolidPattern);
      b.setColor(QColor::fromRgb(
          (palette_bgra >> 16) & 0xff, (palette_bgra >> 8) & 0xff,
          palette_bgra & 0xff, (palette_bgra >> 24) & 0xff));

      QPen p;
      p.setStyle(Qt::SolidLine);
      p.setColor(b.color());
      int x = (i % kTilePerRow) * kPaletteTileSize;
      int y = (i / kTilePerRow) * kPaletteTileSize;
      painter.setBrush(b);
      painter.drawRect(QRect(x, y, kPaletteTileSize, kPaletteTileSize));

      DrawPaletteIndex(painter, i, b.color(), x, y, kPaletteTileSize,
                       kPaletteTileSize);
      painter.restore();
    }

    kiwi::base::FilePath save_path =
        file_path.empty() ? kiwi::base::FilePath::FromUTF8Unsafe("palette.png")
                          : file_path;

    image.save(QString::fromUtf8(save_path.AsUTF8Unsafe()));
    return save_path;
  }
#endif
  std::cout << "Save palette is not supported without Qt backend." << std::endl;
  return kiwi::base::FilePath();
}

kiwi::base::FilePath DebuggerDebugPort::SaveFrame(
    const kiwi::base::FilePath& file_path) {
#if USE_QT6
  if (IsQtBackend()) {
    constexpr int kWidth = 256;
    constexpr int kHeight = 240;

    QImage image(QSize(kWidth, kHeight), QImage::Format_ARGB32);
    QPainter painter(&image);

    kiwi::nes::Colors bgra = GetCurrentFrame();
    QImage pattern_table(reinterpret_cast<uchar*>(bgra.data()), kWidth, kHeight,
                         sizeof(bgra[0]) * kWidth, QImage::Format_ARGB32);
    painter.drawImage(QPoint(0, 0), pattern_table);

    kiwi::base::FilePath save_path =
        file_path.empty() ? kiwi::base::FilePath::FromUTF8Unsafe("frame.png")
                          : file_path;

    bool success = image.save(QString::fromUtf8(save_path.AsUTF8Unsafe()));
    return save_path;
  }
#endif
  std::cout << "Save frame is not supported without Qt backend." << std::endl;
  return kiwi::base::FilePath();
}

void DebuggerDebugPort::PrintMemory(kiwi::nes::Address start) {
  PrintMemory(start, 0xffff, &DebuggerDebugPort::CPUReadByte);
}

void DebuggerDebugPort::PrintPPUMemory(kiwi::nes::Address start) {
  PrintMemory(start, 0x3fff, &DebuggerDebugPort::PPUReadByte);
}

void DebuggerDebugPort::PrintOAMMemory() {
  PrintMemory(0x00, 0xff, &DebuggerDebugPort::OAMReadByte);
}

void DebuggerDebugPort::PrintMemory(
    kiwi::nes::Address start,
    kiwi::nes::Address max,
    kiwi::nes::Byte (kiwi::nes::DebugPort::*func)(kiwi::nes::Address, bool*)) {
  start &= 0xfff0;
  kiwi::nes::Address last = max & 0xff00;
  kiwi::nes::Address end = ((start <= last) ? start + 0x00ff : max);
  kiwi::nes::Byte cache[0x100];
  memset(cache, 0xff, sizeof(cache));
  for (unsigned int i = start; i <= end; ++i) {
    bool can_read;
    cache[i - start] = (this->*func)(i, &can_read);
  }
  std::cout << "       +0 +1 +2 +3 +4 +5 +6 +7 +8 +9 +A +B +C +D +E +F "
               "0123456789ABCDEF"
            << std::endl;
  for (unsigned int i = start; i <= end; i += 0x10) {
    std::cout << "$" << Hex<16>{static_cast<kiwi::nes::Address>(i)} << "  ";
    for (unsigned int j = 0; j < 0x10; ++j) {
      std::cout << Hex<8>{cache[i + j - start]} << " ";
    }
    for (unsigned int j = 0; j < 0x10; ++j) {
      if (cache[i + j - start] >= 32 && cache[i + j - start] <= 126) {
        std::cout << cache[i + j - start];
      } else {
        std::cout << ".";
      }
    }
    std::cout << std::endl;
  }
}

bool DebuggerDebugPort::AddBreakpoint_PPUADDR(kiwi::nes::Address address) {
  return breakpoints_PPUADDR_.insert(address).second;
}

bool DebuggerDebugPort::RemoveBreakpoint_PPUADDR(kiwi::nes::Address address) {
  return breakpoints_PPUADDR_.erase(address) != 0;
}

void DebuggerDebugPort::PrintBreakpoint_PPUADDR() {
  for (kiwi::nes::Address address : breakpoints_PPUADDR_) {
    std::cout << "$" << Hex<16>{address} << std::endl;
  }
}

bool DebuggerDebugPort::AddBreakpoint_ScanlineStart(int scanline) {
  return breakpoints_scanline_start_.insert(scanline).second;
}

bool DebuggerDebugPort::RemoveBreakpoint_ScanlineStart(int scanline) {
  return breakpoints_scanline_start_.erase(scanline) != 0;
}

void DebuggerDebugPort::PrintBreakpoint_ScanlineStart() {
  for (int scanline : breakpoints_scanline_start_) {
    std::cout << std::dec << scanline << std::endl;
  }
}

bool DebuggerDebugPort::AddBreakpoint_ScanlineEnd(int scanline) {
  return breakpoints_scanline_end_.insert(scanline).second;
}

bool DebuggerDebugPort::RemoveBreakpoint_ScanlineEnd(int scanline) {
  return breakpoints_scanline_end_.erase(scanline) != 0;
}

void DebuggerDebugPort::PrintBreakpoint_ScanlineEnd() {
  for (int scanline : breakpoints_scanline_end_) {
    std::cout << std::dec << scanline << std::endl;
  }
}

void DebuggerDebugPort::AddBreakpoint_NMI() {
  break_on_nmi_ = true;
}

void DebuggerDebugPort::RemoveBreakpoint_NMI() {
  break_on_nmi_ = false;
}

void DebuggerDebugPort::RunTestROMs(
    const kiwi::base::FilePath& directory,
    uint64_t instructions_count,
    kiwi::nes::Address output_start_address,
    kiwi::base::OnceCallback<void(const std::vector<ROMTestResult>&)>
        callback) {
  std::cout << "Running test ROMs in directory " << directory.AsUTF8Unsafe()
            << std::endl;
  std::unique_ptr<kiwi::base::FileEnumerator> enumerator =
      std::make_unique<kiwi::base::FileEnumerator>(
          directory, false, kiwi::base::FileEnumerator::FILES);

  std::vector<ROMTestResult> results;
  DoNextROMTest(instructions_count, output_start_address, std::move(callback),
                std::move(enumerator), std::move(results));
}

void DebuggerDebugPort::DoROMTest(
    std::unique_ptr<kiwi::base::FileEnumerator> enumerator,
    const kiwi::base::FilePath& rom_path,
    std::vector<ROMTestResult> results,
    uint64_t instructions_count,
    kiwi::nes::Address output_start_address,
    kiwi::base::OnceCallback<void(std::unique_ptr<kiwi::base::FileEnumerator>,
                                  std::vector<ROMTestResult>)> callback,
    bool success) {
  ROMTestResult result = {rom_path};
  if (success) {
    // Run ROM first.
    emulator()->Run();

    // Run instructions.
    for (uint64_t i = 0; i < instructions_count; ++i) {
      StepToNextCPUInstruction();
    }

    std::string r;
    kiwi::nes::Address output = output_start_address;
    kiwi::nes::Byte ch = 0;
    do {
      ch = CPUReadByte(output++);
      if (ch == '\r' || ch == '\n')
        ch = ' ';
      r += ch;
    } while (ch);
    result.result = r;
  } else {
    result.result = "Failed to load ROM.";
  }
  results.push_back(std::move(result));
  std::move(callback).Run(std::move(enumerator), std::move(results));
}

void DebuggerDebugPort::DoNextROMTest(
    uint64_t instructions_count,
    kiwi::nes::Address output_start_address,
    kiwi::base::OnceCallback<void(const std::vector<ROMTestResult>&)> callback,
    std::unique_ptr<kiwi::base::FileEnumerator> enumerator,
    std::vector<ROMTestResult> results) {
  kiwi::base::FilePath rom_path = enumerator->Next();
  if (!rom_path.empty()) {
    emulator()->LoadFromFile(
        rom_path,
        kiwi::base::BindOnce(
            &DebuggerDebugPort::DoROMTest, kiwi::base::Unretained(this),
            nullptr, rom_path, std::move(results), instructions_count,
            output_start_address,
            kiwi::base::BindOnce(&DebuggerDebugPort::DoNextROMTest,
                                 kiwi::base::Unretained(this),
                                 instructions_count, output_start_address,
                                 std::move(callback))));
  } else {
    PrintROMTestResults(results);
    std::move(callback).Run(results);
  }
}

void DebuggerDebugPort::PrintROMTestResults(
    const std::vector<ROMTestResult>& results) {
  std::cout << "No  | File                  | Output" << std::endl;
  std::cout << "========================================================"
            << std::endl
            << std::dec;

  int i = 0;
  for (const ROMTestResult& result : results) {
    std::cout << std::setw(3) << std::setfill(' ') << std::left << i++ << " | "
              << std::setw(21) << result.rom_path.BaseName() << " | "
              << result.result << std::endl;
  }
}
