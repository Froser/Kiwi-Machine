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

#include "ui/widgets/memory_widget.h"

#include <kiwi_nes.h>

namespace {

constexpr size_t kAddressMaxSize = 5;
constexpr ImVec2 kMemoryAreaSize(550, 240);

std::string NumberToHexString(kiwi::nes::Address n) {
  std::stringstream stream;
  stream << std::setfill('0') << std::setw(4) << std::hex << n;
  return stream.str();
}

}  // namespace

MemoryWidget::MemoryWidget(WindowBase* window_base,
                           NESRuntimeID runtime_id,
                           kiwi::base::RepeatingClosure on_toggle_pause,
                           kiwi::base::RepeatingCallback<bool()> is_pause)
    : Widget(window_base),
      on_toggle_pause_(on_toggle_pause),
      is_pause_(is_pause) {
  set_flags(ImGuiWindowFlags_AlwaysAutoResize);
  set_title("Memory");
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
  SDL_assert(runtime_data_);
  cpu_address_.resize(kAddressMaxSize);
  ppu_address_.resize(kAddressMaxSize);
  oam_address_.resize(kAddressMaxSize);
}

MemoryWidget::~MemoryWidget() = default;

void MemoryWidget::UpdateMemory() {
  if (visible()) {
    if (runtime_data_->emulator->GetRunningState() ==
        kiwi::nes::Emulator::RunningState::kPaused) {
      // Only update when paused.

      // Update CPU, PPU
      {
        kiwi::nes::Address address =
            FormatAddress(MemoryType::kCPU, cpu_address_);
        cpu_memory_ = runtime_data_->debug_port->GetPrettyPrintCPUMemory(
            static_cast<kiwi::nes::Address>(address));
      }
      {
        kiwi::nes::Address address =
            FormatAddress(MemoryType::kPPU, ppu_address_);
        ppu_memory_ = runtime_data_->debug_port->GetPrettyPrintPPUMemory(
            static_cast<kiwi::nes::Address>(address));
      }
      {
        kiwi::nes::Address address =
            FormatAddress(MemoryType::kOAM, oam_address_);
        oam_memory_ = runtime_data_->debug_port->GetPrettyPrintOAMMemory(
            static_cast<kiwi::nes::Address>(address));
      }
    }
  }
}

void MemoryWidget::CreateTab(MemoryType type,
                             const std::string& tab_name,
                             const std::string& which_address,
                             const std::string& which_buffer) {
  static const std::string kShouldPauseStr =
      "You need to load a ROM and pause the emulator to view memory.";
  static const std::string kHeader =
      "       +0 +1 +2 +3 +4 +5 +6 +7 +8 +9 +A +B +C +D +E +F "
      "0123456789ABCDEF";

  bool is_pausing = is_pause_.Run();

  if (ImGui::BeginTabItem(tab_name.c_str())) {
    if (ImGui::InputText(("##" + tab_name).c_str(),
                         const_cast<char*>(which_address.c_str()),
                         kAddressMaxSize,
                         ImGuiInputTextFlags_CharsHexadecimal |
                             ImGuiInputTextFlags_EnterReturnsTrue |
                             ImGuiInputTextFlags_CharsNoBlank |
                             ImGuiInputTextFlags_AutoSelectAll)) {
      UpdateMemory();
    }

    ImGui::SameLine();
    if (ImGui::Button("Goto")) {
      UpdateMemory();
    }
    ImGui::SameLine();

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

    if (is_pausing)
      ImGui::Text("%s", kHeader.c_str());
    else
      ImGui::Text("");

    const std::string& target_text =
        is_pausing ? which_buffer : kShouldPauseStr;
    ImGui::InputTextMultiline(
        ("##View" + tab_name).c_str(), const_cast<char*>(target_text.c_str()),
        target_text.size(), kMemoryAreaSize, ImGuiInputTextFlags_ReadOnly);

    if (ImGui::IsItemHovered()) {
      int wheel_counter = ImGui::GetIO().MouseWheel;
      if (wheel_counter != 0) {
        ChangeAddress(type, wheel_counter * -0x10);
      }
    }

    ImGui::EndTabItem();
  }
}

void MemoryWidget::ChangeAddress(MemoryType type, int delta) {
  switch (type) {
    case MemoryType::kCPU: {
      AdjustAddressWithDelta(type, delta, cpu_address_);
      UpdateMemory();
      break;
    }
    case MemoryType::kPPU: {
      AdjustAddressWithDelta(type, delta, ppu_address_);
      UpdateMemory();
      break;
    }
    case MemoryType::kOAM: {
      AdjustAddressWithDelta(type, delta, oam_address_);
      UpdateMemory();
      break;
    }
    default:
      SDL_assert(false);
      break;
  }
}

void MemoryWidget::Paint() {
  if (ImGui::BeginTabBar("Memory selector", ImGuiTabBarFlags_None)) {
    CreateTab(MemoryType::kCPU, "CPU", cpu_address_, cpu_memory_);
    CreateTab(MemoryType::kPPU, "PPU", ppu_address_, ppu_memory_);
    CreateTab(MemoryType::kOAM, "OAM", oam_address_, oam_memory_);
    ImGui::EndTabBar();
  }
}

kiwi::nes::Address MemoryWidget::FormatAddress(MemoryType type,
                                               std::string& address_string) {
  uint64_t address;
  bool converted = kiwi::base::HexStringToUInt64(address_string, &address);
  SDL_assert(converted);
  address = ClampAddress(type, address);
  address &= 0xfff0;
  address_string = NumberToHexString(address);
  return address;
}

void MemoryWidget::AdjustAddressWithDelta(MemoryType type,
                                          int delta,
                                          std::string& which) {
  uint64_t tmp;
  bool converted = kiwi::base::HexStringToUInt64(which, &tmp);
  SDL_assert(converted);
  int64_t address = static_cast<int64_t>(tmp) + delta;
  which = NumberToHexString(ClampAddress(type, address));
  FormatAddress(type, which);
}

kiwi::nes::Address MemoryWidget::ClampAddress(MemoryType type,
                                              int64_t address) {
  if (address <= 0) {
    address = 0;
  } else {
    if (type == MemoryType::kCPU && address >= 0xffff)
      address = 0xffff;
    else if (type == MemoryType::kPPU && address >= 0x3fff)
      address = 0x3fff;
    else if (type == MemoryType::kOAM && address >= 0xff)
      address = 0xff;
  }
  return address;
}