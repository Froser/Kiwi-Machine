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

#ifndef DEBUG_DEBUG_PORT_H_
#define DEBUG_DEBUG_PORT_H_

#include <kiwi_nes.h>
#include <set>
#include <vector>

#include "utility/timer.h"

class DebugPortObserver {
 public:
  DebugPortObserver();
  virtual ~DebugPortObserver();

 public:
  virtual void OnFrameEnd(int since_last_frame_duration_ms,
                          int cpu_last_frame_duration_ms,
                          int ppu_last_frame_duration_ms);
};

class DebugPort : public kiwi::nes::DebugPort {
 public:
  using Base = kiwi::nes::DebugPort;
  explicit DebugPort(kiwi::nes::Emulator* emulator);
  ~DebugPort() override;

 public:
  void set_on_breakpoint_callback(kiwi::base::RepeatingClosure on_break) {
    on_break_ = on_break;
  }

  void AddObserver(DebugPortObserver* observer);
  void RemoveObserver(DebugPortObserver* observer);
  std::string GetPrettyPrintCPUMemory(kiwi::nes::Address start);
  std::string GetPrettyPrintPPUMemory(kiwi::nes::Address start);
  std::string GetPrettyPrintOAMMemory(kiwi::nes::Address start);
  std::string GetPrettyPrintDisassembly(kiwi::nes::Address address,
                                        int instruction_count);

  int64_t StepToNextCPUInstruction();
  int64_t StepToNextScanline(uint64_t scanline);
  int64_t StepToNextFrame(uint64_t frame);
  void AddBreakpoint(kiwi::nes::Address address);
  void RemoveBreakpoint(kiwi::nes::Address address);
  void ClearBreakpoints();
  const std::vector<kiwi::nes::Address>& breakpoints() { return breakpoints_; }

 private:
  std::string GetPrettyPrintMemory(
      kiwi::nes::Address start,
      kiwi::nes::Address max,
      kiwi::nes::Byte (kiwi::nes::DebugPort::*func)(kiwi::nes::Address, bool*));

 protected:
  // kiwi::nes::DebugPort:
  // Calculate frame generation rate and increase frame counter. It will be
  // invoked in emulator's worker thread.
  void OnFrameEnd() override;
  void OnScanlineEnd(int scanline) override;
  void OnCPUBeforeStep(kiwi::nes::CPUDebugState& state) override;

 private:
  Timer frame_generation_timer_;
  std::set<DebugPortObserver*> observers_;
  uint64_t frame_counter_ = 0;
  uint64_t scanline_counter_ = 0;
  bool break_ = false;
  std::vector<kiwi::nes::Address> breakpoints_;
  kiwi::base::RepeatingClosure on_break_;
};

#endif  // DEBUG_DEBUG_PORT_H_