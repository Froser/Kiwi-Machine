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

#include <gflags/gflags.h>
#include <kiwi_nes.h>
#include <cstdint>
#include <iostream>

#include "base/runloop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_executor.h"
#include "debugger/debugger_debug_port.h"
#include "nes/types.h"

std::unique_ptr<DebuggerDebugPort> debug_port;
scoped_refptr<kiwi::nes::Emulator> emulator;

DEFINE_string(test_roms_dir, "", "Set test ROM's path.");
DEFINE_uint64(test_cycles, 4000000, "Set cycles count to test.");
DEFINE_uint64(test_output_address,
              0x6005,
              "The address that test ROMs' will write into.");
DEFINE_string(backend_name, "SDL2", "Set Kiwi backend, Qt or SDL2");

int Extract2Params(const kiwi::base::StringPiece& input,
                   std::string& str1,
                   std::string& str2) {
  auto result = kiwi::base::SplitString(input, " ", kiwi::base::TRIM_WHITESPACE,
                                        kiwi::base::SPLIT_WANT_NONEMPTY);
  if (result.size() == 1) {
    str1 = result[0];
  } else if (result.size() >= 2) {
    str1 = result[0];
    str2 = result[1];
  }
  return result.size();
}

bool ExtractParamsUInt64(const kiwi::base::StringPiece& input,
                         std::string& str,
                         uint64_t& int64) {
  std::string second_str;
  int c = Extract2Params(input, str, second_str);
  if (c == 2) {
    if (second_str[0] == '$' && second_str.size() > 1)
      kiwi::base::HexStringToUInt64("0x" + second_str.substr(1), &int64);
    else
      kiwi::base::StringToUint64(second_str, &int64);
    return true;
  }
  return false;
}

bool ExecuteCommand(const kiwi::base::StringPiece& command, bool* should_quit) {
  if (command == "quit") {
    *should_quit = true;
    return true;
  }

  if (command == "reset" || command == "run") {
    emulator->Reset(kiwi::base::DoNothing());
  } else if (command == "cart" || command == "rom") {  // Cartridge(ROM)
    debug_port->PrintROM();
  } else if (command == "cr") {  // CPU registers
    debug_port->PrintCPURegisters();
  } else if (command == "pr" || command == "ppu") {  // PPU registers
    debug_port->PrintPPURegisters();
  } else if (command == "ar") {  // APU registers
    // debug_port->PrintAPURegisters();
  } else if (command == "r" || command == "cpu") {  // All registers
    // debug_port->PrintAPURegisters();
    debug_port->PrintCPURegisters();
    debug_port->PrintPPURegisters();
  } else if (command == "s" || kiwi::base::StartsWith(command, "s ")) {
    // s x
    // |x| stands for instructions count.
    std::string s;
    uint64_t x = 1;
    ExtractParamsUInt64(command, s, x);
    int64_t cycle = debug_port->StepInstructionCount(x);
    std::cout << std::dec << cycle << " CPU cycle(s) passed." << std::endl;
  } else if (kiwi::base::StartsWith(command, "si ")) {
    // Step to specific instruction before it runs
    // si addr ($xx)
    std::string si;
    uint64_t opcode = 0;
    ExtractParamsUInt64(command, si, opcode);
    int64_t cycle = debug_port->StepToInstruction(opcode);
    std::cout << std::dec << cycle << " CPU cycle(s) passed." << std::endl;
    debug_port->PrintDisassembly(debug_port->GetCPUContext().registers.PC, 10);
  } else if (command == "sf" || kiwi::base::StartsWith(command, "sf ")) {
    // Step to next n frames
    std::string sf;
    uint64_t frame = 1;
    ExtractParamsUInt64(command, sf, frame);
    int64_t cycle = debug_port->StepToNextFrame(frame);
    std::cout << std::dec << cycle << " CPU cycle(s) passed." << std::endl;
    debug_port->PrintDisassembly(debug_port->GetCPUContext().registers.PC, 10);
  } else if (command == "ss" || kiwi::base::StartsWith(command, "ss ")) {
    // Step to next n scanlines
    std::string ss;
    uint64_t sl = 1;
    ExtractParamsUInt64(command, ss, sl);
    int64_t cycle = debug_port->StepToNextScanline(sl);
    std::cout << std::dec << cycle << " CPU cycle(s) passed." << std::endl;
    debug_port->PrintDisassembly(debug_port->GetCPUContext().registers.PC, 10);
  } else if (command == "d" ||
             kiwi::base::StartsWith(command, "d ")) {  // Disassembly
    // d n(=10)
    std::string d;
    uint64_t n = 10;
    ExtractParamsUInt64(command, d, n);
    debug_port->PrintDisassembly(debug_port->GetCPUContext().registers.PC, n);
  } else if (command == "ptv" || command == "patternv") {  // Pattern table view
    debug_port->PrintPatternTable();
  } else if (command == "pt" ||
             kiwi::base::StartsWith(command, "pt ")) {  // pattern table image
    // pt filepath
    std::string pt, filepath;
    int c = Extract2Params(command, pt, filepath);
    if (kiwi::base::FilePath saved_path = debug_port->SavePatternTable(
            c == 2 ? kiwi::base::FilePath::FromUTF8Unsafe(filepath)
                   : kiwi::base::FilePath());
        !saved_path.empty()) {
      std::cout << "Pattern table saved: " << saved_path;
    } else {
      std::cout << "Failed to save pattern table.";
    }
  } else if (command == "nt" ||
             kiwi::base::StartsWith(command, "nt ")) {  // nametable image
    // nt filepath
    std::string nt, filepath;
    int c = Extract2Params(command, nt, filepath);
    if (kiwi::base::FilePath saved_path = debug_port->SaveNametable(
            c == 2 ? kiwi::base::FilePath::FromUTF8Unsafe(filepath)
                   : kiwi::base::FilePath());
        !saved_path.empty()) {
      std::cout << "Nametable saved: " << saved_path;
    } else {
      std::cout << "Failed to save nametable.";
    }
  } else if (command == "sprites" ||
             kiwi::base::StartsWith(command, "sprites ")) {  // sprites image
    // sprites filepath
    std::string sprites, filepath;
    int c = Extract2Params(command, sprites, filepath);
    if (kiwi::base::FilePath saved_path = debug_port->SaveSprites(
            c == 2 ? kiwi::base::FilePath::FromUTF8Unsafe(filepath)
                   : kiwi::base::FilePath());
        !saved_path.empty()) {
      std::cout << "Sprites saved: " << saved_path;
    } else {
      std::cout << "Failed to save sprites.";
    }
  } else if (command == "palette" ||
             kiwi::base::StartsWith(command,
                                    "palette ")) {  // global palette image
    // palette filepath
    std::string palette, filepath;
    int c = Extract2Params(command, palette, filepath);
    if (kiwi::base::FilePath saved_path = debug_port->SavePalette(
            c == 2 ? kiwi::base::FilePath::FromUTF8Unsafe(filepath)
                   : kiwi::base::FilePath());
        !saved_path.empty()) {
      std::cout << "Global palette saved: " << saved_path;
    } else {
      std::cout << "Failed to save global palette.";
    }
  } else if (command == "f" || command == "frame" ||
             kiwi::base::StartsWith(command, "f ") ||
             kiwi::base::StartsWith(command, "frame ")) {  // current frame
    // frame filepath
    std::string frame, filepath;
    int c = Extract2Params(command, frame, filepath);
    if (kiwi::base::FilePath saved_path = debug_port->SaveFrame(
            c == 2 ? kiwi::base::FilePath::FromUTF8Unsafe(filepath)
                   : kiwi::base::FilePath());
        !saved_path.empty()) {
      std::cout << "Current frame saved: " << saved_path;
    } else {
      std::cout << "Failed to save current frame.";
    }
  } else if (command == "m" ||
             kiwi::base::StartsWith(command, "m ")) {  // Memory
    // m addr, addr=$xxxx(hex) or xxxx(dec)
    std::string m;
    uint64_t address = 0;
    ExtractParamsUInt64(command, m, address);
    debug_port->PrintMemory(address);
  } else if (command == "mp" || kiwi::base::StartsWith(command, "mp ")) {
    // PPU memory
    std::string m;
    uint64_t address = 0;
    ExtractParamsUInt64(command, m, address);
    debug_port->PrintPPUMemory(address);
  } else if (command == "mo" || kiwi::base::StartsWith(command, "mo ")) {
    // OMA memory
    debug_port->PrintOAMMemory();
  } else if (command == "bppuaddr") {
    debug_port->PrintBreakpoint_PPUADDR();
  } else if (kiwi::base::StartsWith(command, "bppuaddr ")) {
    // Break on specific PPUADDR
    std::string ppuaddr;
    uint64_t address = 0;
    ExtractParamsUInt64(command, ppuaddr, address);
    if (debug_port->AddBreakpoint_PPUADDR(address)) {
      std::cout << "Breakpoint set succeeded." << std::endl;
    } else {
      std::cout << "Breakpoint set failed." << std::endl;
    }
  } else if (kiwi::base::StartsWith(command, "-bppuaddr ")) {
    // Remove breakpoints on specific PPUADDR
    std::string ppuaddr;
    uint64_t address = 0;
    ExtractParamsUInt64(command, ppuaddr, address);
    if (debug_port->RemoveBreakpoint_PPUADDR(address)) {
      std::cout << "Breakpoint remove succeeded." << std::endl;
    } else {
      std::cout << "Breakpoint remove failed." << std::endl;
    }
  } else if (command == "bnmi") {
    // Break when NMI
    debug_port->AddBreakpoint_NMI();
    std::cout << "NES will break when NMI occurs." << std::endl;
  } else if (command == "-bnmi") {
    debug_port->RemoveBreakpoint_NMI();
    std::cout << "Breakpoint on NMI has been removed." << std::endl;
  } else if (command == "bscanline") {
    debug_port->PrintBreakpoint_ScanlineStart();
  } else if (kiwi::base::StartsWith(command, "bscanline ")) {
    // Break on specific scanline start
    std::string scanline;
    uint64_t scanline_id = 0;
    ExtractParamsUInt64(command, scanline, scanline_id);
    if (debug_port->AddBreakpoint_ScanlineStart(scanline_id)) {
      std::cout << "Breakpoint set succeeded." << std::endl;
    } else {
      std::cout << "Breakpoint set failed." << std::endl;
    }
  } else if (kiwi::base::StartsWith(command, "-bscanline ")) {
    // Remove breakpoints on specific scanline start
    std::string scanline;
    uint64_t scanline_id = 0;
    ExtractParamsUInt64(command, scanline, scanline_id);
    if (debug_port->RemoveBreakpoint_ScanlineStart(scanline_id)) {
      std::cout << "Breakpoint remove succeeded." << std::endl;
    } else {
      std::cout << "Breakpoint remove failed." << std::endl;
    }
  } else if (command == "bscanlineend") {
    debug_port->PrintBreakpoint_ScanlineEnd();
  } else if (kiwi::base::StartsWith(command, "bscanlineend ")) {
    // Break specific scanline end
    std::string scanline;
    uint64_t scanline_id = 0;
    ExtractParamsUInt64(command, scanline, scanline_id);
    if (debug_port->AddBreakpoint_ScanlineEnd(scanline_id)) {
      std::cout << "Breakpoint set succeeded." << std::endl;
    } else {
      std::cout << "Breakpoint set failed." << std::endl;
    }
  } else if (kiwi::base::StartsWith(command, "-bscanlineend ")) {
    // Remove specific scanline end
    std::string scanline;
    uint64_t scanline_id = 0;
    ExtractParamsUInt64(command, scanline, scanline_id);
    if (debug_port->RemoveBreakpoint_ScanlineEnd(scanline_id)) {
      std::cout << "Breakpoint remove succeeded." << std::endl;
    } else {
      std::cout << "Breakpoint remove failed." << std::endl;
    }
  } else {
    return false;
  }

  return true;
}

void MainLoop(bool success) {
  if (success) {
    bool should_quit = false;
    std::string last;
    do {
      std::cout << "KIWI NES Debugger> ";
      std::string command;
      std::getline(std::cin, command);
      kiwi::base::StringPiece c =
          kiwi::base::TrimWhitespaceASCII(command, kiwi::base::TRIM_ALL);
      if (c.empty()) {
        c = kiwi::base::TrimWhitespaceASCII(last, kiwi::base::TRIM_ALL);
      } else {
        last = command;
      }
      if (ExecuteCommand(c, &should_quit))
        std::cout << std::endl;
    } while (!should_quit);
  } else {
    std::cout << "Can't load ROM. ROM is not valid." << std::endl;
  }
}

int main(int argc, char** argv) {
  std::cout << "Kiwi NES Debugger" << std::endl;
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  kiwi::base::PlatformFactoryBackend backend =
      kiwi::base::PlatformFactoryBackend::kSDL2;
  if (FLAGS_backend_name == "Qt") {
    backend = kiwi::base::PlatformFactoryBackend::kQt6;
    std::cout << "Backend: Qt6" << std::endl;
  } else {
    std::cout << "Backend: SDL2" << std::endl;
  }

  kiwi::base::InitializePlatformFactory(argc, argv, backend);
  kiwi::base::SingleThreadTaskExecutor executor(
      kiwi::base::MessagePumpType::UI);
  kiwi::base::RunLoop run_loop;

  if (!FLAGS_test_roms_dir.empty()) {
    kiwi::base::FilePath test_roms_dir =
        kiwi::base::FilePath::FromUTF8Unsafe(FLAGS_test_roms_dir);
    // test ROMs' result usually start at $6005
    emulator = kiwi::nes::CreateEmulator();
    debug_port = std::make_unique<DebuggerDebugPort>(emulator.get());
    emulator->PowerOn();
    emulator->SetDebugPort(debug_port.get());
    debug_port->RunTestROMs(
        test_roms_dir, FLAGS_test_cycles,
        static_cast<kiwi::nes::Address>(FLAGS_test_output_address),
        kiwi::base::BindOnce(
            [](kiwi::base::OnceClosure quit_closure,
               const std::vector<DebuggerDebugPort::ROMTestResult>&) {
              std::move(quit_closure).Run();
            },
            run_loop.QuitClosure()));
  } else if (argc > 1) {
    emulator = kiwi::nes::CreateEmulator();
    debug_port = std::make_unique<DebuggerDebugPort>(emulator.get());
    emulator->PowerOn();
    emulator->SetDebugPort(debug_port.get());
    emulator->LoadFromFile(kiwi::base::FilePath::FromUTF8Unsafe(argv[1]),
                           kiwi::base::BindOnce(MainLoop));
  }

  run_loop.Run();
  return 0;
}
