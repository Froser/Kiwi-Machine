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

#include "debug/debug_port.h"

#include <SDL.h>
#include <optional>

namespace {

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

// Gets 4 bits from Byte:
#define HIGH_HALF_BYTE(x) (static_cast<kiwi::nes::Byte>(x >> 4))
#define LOW_HALF_BYTE(x) (static_cast<kiwi::nes::Byte>(x & 0x7))

// Gets Byte from Address:
#define HIGH_BYTE(x) static_cast<kiwi::nes::Byte>((x >> 8))
#define LOW_BYTE(x) static_cast<kiwi::nes::Byte>((x & 0xff))

#define CHECK_BREAK() \
  if (break_) {       \
    break_ = false;   \
    on_break_.Run();  \
    break;            \
  }

}  // namespace

DebugPortObserver::DebugPortObserver() = default;
DebugPortObserver::~DebugPortObserver() = default;
void DebugPortObserver::OnFrameEnd(int since_last_frame_end_ms) {}

DebugPort::DebugPort(kiwi::nes::Emulator* emulator) : Base(emulator) {}
DebugPort::~DebugPort() = default;

void DebugPort::OnFrameEnd() {
  ++frame_counter_;
  int ms = frame_generation_timer_.ElapsedInMillisecondsAndReset();
  for (DebugPortObserver* observer : observers_) {
    observer->OnFrameEnd(ms);
  }
}

void DebugPort::AddObserver(DebugPortObserver* observer) {
  observers_.insert(observer);
}

void DebugPort::RemoveObserver(DebugPortObserver* observer) {
  observers_.erase(observer);
}

std::string DebugPort::GetPrettyPrintCPUMemory(kiwi::nes::Address start) {
  return GetPrettyPrintMemory(start, 0xffff, &DebugPort::CPUReadByte);
}

std::string DebugPort::GetPrettyPrintPPUMemory(kiwi::nes::Address start) {
  return GetPrettyPrintMemory(start, 0x3fff, &DebugPort::PPUReadByte);
}

std::string DebugPort::GetPrettyPrintOAMMemory(kiwi::nes::Address start) {
  return GetPrettyPrintMemory(start, 0xff, &DebugPort::OAMReadByte);
}

std::string DebugPort::GetPrettyPrintDisassembly(kiwi::nes::Address address,
                                                 int instruction_count) {
  std::stringstream ss;
  for (int i = 0; i < instruction_count; ++i) {
    kiwi::nes::Disassembly disassembly = kiwi::nes::Disassemble(this, address);
    if (i == 0) {
      ss << "--> ";
    } else {
      ss << "    ";
    }
    ss << "$" << Hex<16>{address} << ": " << Hex<8>{disassembly.opcode};
    if (disassembly.operand_size == 1) {
      ss << " " << Hex<8>{LOW_BYTE(disassembly.operand)} << "     ";
    } else if (disassembly.operand_size == 2) {
      ss << " " << Hex<8>{LOW_BYTE(disassembly.operand)} << " "
         << Hex<8>{HIGH_BYTE(disassembly.operand)} << "  ";
    } else {
      SDL_assert(disassembly.operand_size == 0);
      ss << "        ";
    }
    ss << " <" << std::dec << static_cast<int>(disassembly.cycle) << "> "
       << disassembly.pretty_print << std::endl;

    if (disassembly.next_instruction < address) {
      // Overflow, break.
      break;
    } else {
      address = disassembly.next_instruction;
    }
  }
  return ss.str();
}

int64_t DebugPort::StepToNextCPUInstruction() {
  int64_t cycle = 1;
  emulator()->Step();
  kiwi::nes::CPUContext context = GetCPUContext();
  while (context.last_action.cycles_to_wait) {
    emulator()->Step();
    context = GetCPUContext();
    ++cycle;

    CHECK_BREAK();
  }
  return cycle;
}

int64_t DebugPort::StepToNextScanline(uint64_t scanline) {
  int64_t cycle = 0;
  uint64_t scanline_now = scanline_counter_;
  while (scanline_counter_ - scanline_now < scanline) {
    cycle += StepToNextCPUInstruction();

    CHECK_BREAK();
  }
  return cycle;
}

int64_t DebugPort::StepToNextFrame(uint64_t frame) {
  int64_t cycle = 0;
  uint64_t frame_now = frame_counter_;
  while (frame_counter_ - frame_now < frame) {
    cycle += StepToNextCPUInstruction();

    CHECK_BREAK();
  }
  return cycle;
}

void DebugPort::AddBreakpoint(kiwi::nes::Address address) {
  if (std::find(breakpoints_.begin(), breakpoints_.end(), address) ==
      breakpoints_.cend())
    breakpoints_.push_back(address);
}

void DebugPort::RemoveBreakpoint(kiwi::nes::Address address) {
  auto iter = std::find(breakpoints_.begin(), breakpoints_.end(), address);
  if (iter != breakpoints_.cend()) {
    breakpoints_.erase(iter);
  }
}

void DebugPort::ClearBreakpoints() {
  breakpoints_.clear();
}

void DebugPort::OnScanlineEnd(int scanline) {
  ++scanline_counter_;
}

void DebugPort::OnCPUBeforeStep(kiwi::nes::CPUDebugState& state) {
  kiwi::nes::CPUContext cpu_context = GetCPUContext();
  if (std::find(breakpoints_.begin(), breakpoints_.end(),
                cpu_context.registers.PC) != breakpoints_.cend()) {
    break_ = true;
    SDL_assert(on_break_);
    on_break_.Run();
    state.should_break = true;
  }
}

std::string DebugPort::GetPrettyPrintMemory(
    kiwi::nes::Address start,
    kiwi::nes::Address max,
    kiwi::nes::Byte (kiwi::nes::DebugPort::*func)(kiwi::nes::Address, bool*)) {
  std::stringstream ss;
  start &= 0xfff0;
  kiwi::nes::Address last = max & 0xff00;
  kiwi::nes::Address end = ((start <= last) ? start + 0x00ff : max);
  std::optional<kiwi::nes::Byte> cache[0x100];
  memset(cache, 0xff, sizeof(cache));
  for (unsigned int i = start; i <= end; ++i) {
    bool can_read;
    cache[i - start] = (this->*func)(i, &can_read);
    if (!can_read)
      cache[i - start] = std::nullopt;
  }

  for (unsigned int i = start; i <= end; i += 0x10) {
    ss << "$" << Hex<16>{static_cast<kiwi::nes::Address>(i)} << "  ";
    for (unsigned int j = 0; j < 0x10; ++j) {
      if (cache[i - start].has_value())
        ss << Hex<8>{*cache[i + j - start]} << " ";
      else
        ss << "?? ";
    }
    for (unsigned int j = 0; j < 0x10; ++j) {
      if (cache[i + j - start].has_value() && cache[i + j - start] >= 32 &&
          cache[i + j - start] <= 126) {
        ss << *cache[i + j - start];
      } else {
        ss << ".";
      }
    }
    ss << std::endl;
  }

  return ss.str();
}
