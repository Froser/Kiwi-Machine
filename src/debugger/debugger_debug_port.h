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

#ifndef DEBUGGER_DEBUGGER_DEBUG_PORT_H_
#define DEBUGGER_DEBUGGER_DEBUG_PORT_H_

#include <kiwi_nes.h>
#include <stdint.h>
#include <set>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"

class DebuggerDebugPort : public kiwi::nes::DebugPort {
 public:
  DebuggerDebugPort(kiwi::nes::Emulator* emulator);
  ~DebuggerDebugPort() override;

  struct ROMTestResult {
    kiwi::base::FilePath rom_path;
    std::string result;
  };

 protected:
  void OnCPUReset(const kiwi::nes::CPUContext& cpu_context) override;
  void OnPPUReset(const kiwi::nes::PPUContext& ppu_context) override;
  void OnRomLoaded(bool success, const kiwi::nes::RomData* rom_data) override;
  void OnPPUADDR(kiwi::nes::Address address) override;
  void OnCPUNMI() override;
  void OnScanlineStart(int scanline) override;
  void OnScanlineEnd(int scanline) override;
  void OnFrameEnd() override;

 public:
  int64_t StepToNextCPUInstruction();
  int64_t StepInstructionCount(uint64_t count);
  int64_t StepToInstruction(kiwi::nes::Byte opcode);
  int64_t StepToNextScanline(uint64_t scanline);
  int64_t StepToNextFrame(uint64_t frame);
  void PrintCPURegisters();
  void PrintPPURegisters();
  void PrintDisassembly(kiwi::nes::Address address, int instruction_count);
  void PrintROM();
  void PrintPatternTable();
  kiwi::base::FilePath SavePatternTable(const kiwi::base::FilePath& file_path);
  kiwi::base::FilePath SaveNametable(const kiwi::base::FilePath& file_path);
  kiwi::base::FilePath SaveSprites(const kiwi::base::FilePath& file_path);
  kiwi::base::FilePath SavePalette(const kiwi::base::FilePath& file_path);
  kiwi::base::FilePath SaveFrame(const kiwi::base::FilePath& file_path);
  void PrintMemory(kiwi::nes::Address start);
  void PrintPPUMemory(kiwi::nes::Address start);
  void PrintOAMMemory();
  // Break when PPUADDR==|address|
  bool AddBreakpoint_PPUADDR(kiwi::nes::Address address);
  bool RemoveBreakpoint_PPUADDR(kiwi::nes::Address address);
  void PrintBreakpoint_PPUADDR();
  // Break when scanline meets condition.
  bool AddBreakpoint_ScanlineStart(int scanline);
  bool RemoveBreakpoint_ScanlineStart(int scanline);
  void PrintBreakpoint_ScanlineStart();
  bool AddBreakpoint_ScanlineEnd(int scanline);
  bool RemoveBreakpoint_ScanlineEnd(int scanline);
  void PrintBreakpoint_ScanlineEnd();
  void AddBreakpoint_NMI();
  void RemoveBreakpoint_NMI();
  void RunTestROMs(
      const kiwi::base::FilePath& directory,
      uint64_t instructions_count,
      kiwi::nes::Address output_start_address,
      kiwi::base::OnceCallback<void(const std::vector<ROMTestResult>&)> callback);

 private:
  void PrintMemory(
      kiwi::nes::Address start,
      kiwi::nes::Address max,
      kiwi::nes::Byte (kiwi::nes::DebugPort::*func)(kiwi::nes::Address, bool*));
  void DoROMTest(std::unique_ptr<kiwi::base::FileEnumerator> enumerator,
                 const kiwi::base::FilePath& rom_path,
                 std::vector<ROMTestResult> results,
                 uint64_t instructions_count,
                 kiwi::nes::Address output_start_address,
                 kiwi::base::OnceCallback<void(std::unique_ptr<kiwi::base::FileEnumerator>,
                                         std::vector<ROMTestResult>)> callback,
                 bool success);
  void DoNextROMTest(
      uint64_t instructions_count,
      kiwi::nes::Address output_start_address,
      kiwi::base::OnceCallback<void(const std::vector<ROMTestResult>&)> callback,
      std::unique_ptr<kiwi::base::FileEnumerator> enumerator,
      std::vector<ROMTestResult> results);
  void PrintROMTestResults(const std::vector<ROMTestResult>& results);

 private:
  const kiwi::nes::RomData* rom_data_ = nullptr;
  bool break_ = false;
  std::string break_reason_;
  uint64_t frame_counter_ = 0;
  uint64_t scanline_counter_ = 0;

  std::set<kiwi::nes::Address> breakpoints_PPUADDR_;
  std::set<int> breakpoints_scanline_start_;
  std::set<int> breakpoints_scanline_end_;
  bool break_on_nmi_ = false;
  bool break_because_nmi_ = false;
};

#endif  // DEBUGGER_DEBUGGER_DEBUG_PORT_H_
