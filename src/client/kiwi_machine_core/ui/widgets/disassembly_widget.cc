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

#include "ui/widgets/disassembly_widget.h"

#include <kiwi_nes.h>

namespace {

constexpr ImVec2 kDisassemblyAreaSize(500, 550);
constexpr int kDisassemblyInstructionCount = 50;
static const std::string kShouldPauseStr =
    "You need to load a ROM and pause \nthe emulator to view disassembly.";

std::string NumberToHexString(int width, kiwi::nes::Address n) {
  std::stringstream stream;
  stream << std::setfill('0') << std::setw(width) << std::hex << n;
  return stream.str();
}

}  // namespace

DisassemblyWidget::DisassemblyWidget(
    WindowBase* window_base,
    NESRuntimeID runtime_id,
    kiwi::base::RepeatingClosure on_toggle_pause,
    kiwi::base::RepeatingCallback<bool()> is_pause)
    : Widget(window_base),
      on_toggle_pause_(on_toggle_pause),
      is_pause_(is_pause) {
  set_flags(ImGuiWindowFlags_AlwaysAutoResize);
  set_title("Disassembly");
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
  SDL_assert(runtime_data_);
}

DisassemblyWidget::~DisassemblyWidget() = default;

void DisassemblyWidget::UpdateDisassembly() {
  if (visible()) {
    disassembly_string_ = runtime_data_->debug_port->GetPrettyPrintDisassembly(
        runtime_data_->debug_port->GetCPUContext().registers.PC,
        kDisassemblyInstructionCount);
  }
}

void DisassemblyWidget::Paint() {
  auto cpu = runtime_data_->debug_port->GetCPUContext().registers;
  auto ppu = runtime_data_->debug_port->GetPPUContext().registers;

  ImGui::BeginGroup();
  // runtime_data_->debug_port
  bool is_pausing = is_pause_.Run();
  const std::string& target_text =
      is_pausing ? disassembly_string_ : kShouldPauseStr;
  ImGui::InputTextMultiline(
      "##DisassemblyView", const_cast<char*>(target_text.c_str()),
      target_text.size(), kDisassemblyAreaSize, ImGuiInputTextFlags_ReadOnly);
  ImGui::EndGroup();

  ImGui::SameLine();

  ImGui::BeginGroup();

  // Registers
  ImGui::BeginGroup();

  // Registers - CPU
  ImGui::BeginGroup();
  ImGui::Text(
      "A:  %s\nX:  %s\nY:  %s\nS:  %s\nPC: %s\nP:  %s",
      NumberToHexString(2, cpu.A).c_str(), NumberToHexString(2, cpu.X).c_str(),
      NumberToHexString(2, cpu.Y).c_str(), NumberToHexString(2, cpu.S).c_str(),
      NumberToHexString(4, cpu.PC).c_str(),
      NumberToHexString(2, cpu.P.value).c_str());
  ImGui::EndGroup();
  ImGui::SameLine();
  // Registers - PPU
  ImGui::BeginGroup();
  ImGui::Text(
      "PPUCTRL:   %s\n"
      "PPUMASK:   %s\n"
      "PPUSTATUS: %s\n"
      "OAMADDR:   %s\n"
      "OAMDATA:   %s\n"
      "PPUSCROLL: %s\n"
      "PPUADDR:   %s\n"
      "PPUDATA:   %s\n"
      "OAMDMA:    %s",
      NumberToHexString(2, ppu.PPUCTRL.value).c_str(),
      NumberToHexString(2, ppu.PPUMASK.value).c_str(),
      NumberToHexString(2, ppu.PPUSTATUS.value).c_str(),
      NumberToHexString(2, ppu.OAMADDR).c_str(),
      NumberToHexString(2, ppu.OAMDATA).c_str(),
      NumberToHexString(2, ppu.PPUSCROLL).c_str(),
      NumberToHexString(2, ppu.PPUADDR).c_str(),
      NumberToHexString(2, ppu.PPUDATA).c_str(),
      NumberToHexString(2, ppu.OAMDMA).c_str());
  ImGui::EndGroup();
  // End of registers
  ImGui::EndGroup();

  // PPU Context
  ImGui::BeginGroup();
  ImGui::Text("Scanline: %d",
              runtime_data_->debug_port->GetPPUContext().scanline);
  ImGui::Text("Pixel (dot): %d",
              runtime_data_->debug_port->GetPPUContext().pixel);
  ImGui::EndGroup();

  // Breakpoints
  ImGui::InputText("##CPU Address", breakpoint_address_input,
                   sizeof(breakpoint_address_input),
                   ImGuiInputTextFlags_CharsHexadecimal);
  ImGui::BeginGroup();
  if (ImGui::Button("Add Breakpoint")) {
    uint64_t address;
    bool converted =
        kiwi::base::HexStringToUInt64(breakpoint_address_input, &address);
    if (converted) {
      runtime_data_->debug_port->AddBreakpoint(
          static_cast<kiwi::nes::Address>(address));
    }
  }

  auto item_getter = [](void* data, int index, const char** out_text) {
    DisassemblyWidget* d = reinterpret_cast<DisassemblyWidget*>(data);
    if (out_text) {
      d->item_getter_buffer_ = NumberToHexString(
          4, d->runtime_data_->debug_port->breakpoints()[index]);
      *out_text = d->item_getter_buffer_.c_str();
    }
    return true;
  };
  ImGui::ListBox("##Breakpoints", &current_selected_breakpoint_, item_getter,
                 this, runtime_data_->debug_port->breakpoints().size());
  if (ImGui::Button("Remove Breakpoint")) {
    if (current_selected_breakpoint_ >= 0 &&
        current_selected_breakpoint_ <=
            runtime_data_->debug_port->breakpoints().size()) {
      runtime_data_->debug_port->RemoveBreakpoint(
          runtime_data_->debug_port
              ->breakpoints()[current_selected_breakpoint_]);
    }
  }
  if (ImGui::Button("Clear Breakpoints")) {
    runtime_data_->debug_port->ClearBreakpoints();
  }
  ImGui::EndGroup();

  // Controls
  if (is_pausing) {
    if (ImGui::Button("Resume")) {
      if (on_toggle_pause_)
        on_toggle_pause_.Run();
    }
  } else {
    if (ImGui::Button("Pause")) {
      if (on_toggle_pause_)
        on_toggle_pause_.Run();
    }
  }

  ImGui::BeginDisabled(!is_pausing);
  if (ImGui::Button("Step Instruction")) {
    runtime_data_->debug_port->StepToNextCPUInstruction();
    UpdateDisassembly();
  }
  if (ImGui::Button("Step Scanline")) {
    runtime_data_->debug_port->StepToNextScanline(1);
    UpdateDisassembly();
  }
  if (ImGui::Button("Step Frame")) {
    runtime_data_->debug_port->StepToNextFrame(1);
    UpdateDisassembly();
  }
  ImGui::EndDisabled();

  // Patches
  ImGui::Text("Follow settings are subtle:");
  ImGui::Text("Scanline IRQ Cycle(Dot)");
  ImGui::InputText("##Scanline IRQ Dot", ppu_scanline_irq_dot_,
                   sizeof(ppu_scanline_irq_dot_),
                   ImGuiInputTextFlags_CharsDecimal);
  ImGui::SameLine();
  if (ImGui::Button("Ok")) {
    uint64_t delay;
    bool converted = kiwi::base::StringToUint64(
        kiwi::base::StringPiece(ppu_scanline_irq_dot_), &delay);
    if (converted)
      runtime_data_->debug_port->SetScanlineIRQCycle(delay);
  }

  ImGui::TextUnformatted("If this widget is visible");
  ImGui::TextUnformatted(
      "the rom will be paused automatically when loaded or reset");

  ImGui::EndGroup();
}
