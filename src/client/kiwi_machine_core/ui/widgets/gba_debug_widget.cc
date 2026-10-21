// Copyright (C) 2026 Yisi Yu
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

#include "ui/widgets/gba_debug_widget.h"

#include <SDL.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "ui/main_window.h"
#include "ui/window_base.h"

namespace {

constexpr size_t kDisassemblyInstructionCount = 48;
constexpr size_t kMemoryByteCount = 256;
constexpr int kTileAtlasWidth = 256;
constexpr int kTileAtlasHeight = 128;
constexpr size_t kCharacterBlockBytes = 0x4000;
constexpr int kSpriteTextureSize = 64;

const char* GetPrivilegeModeName(uint32_t cpsr) {
  switch (cpsr & 0x1f) {
    case 0x10:
      return "User";
    case 0x11:
      return "FIQ";
    case 0x12:
      return "IRQ";
    case 0x13:
      return "Supervisor";
    case 0x17:
      return "Abort";
    case 0x1b:
      return "Undefined";
    case 0x1f:
      return "System";
    default:
      return "Unknown";
  }
}

std::string GetStatusFlags(uint32_t cpsr) {
  std::string flags = "-------";
  constexpr uint32_t kMasks[] = {
      1u << 31, 1u << 30, 1u << 29, 1u << 28, 1u << 7, 1u << 6, 1u << 5,
  };
  constexpr char kNames[] = {'N', 'Z', 'C', 'V', 'I', 'F', 'T'};
  for (size_t i = 0; i < 7; ++i) {
    if (cpsr & kMasks[i])
      flags[i] = kNames[i];
  }
  return flags;
}

const char* GetRegisterName(size_t index) {
  static constexpr const char* kNames[] = {
      "r0", "r1", "r2",  "r3",  "r4",  "r5",       "r6",       "r7",
      "r8", "r9", "r10", "r11", "r12", "r13 / SP", "r14 / LR", "r15 / PC",
  };
  return kNames[index];
}

std::string FormatOpcode(
    const kiwi::gba::Emulator::DebugInstruction& instruction,
    kiwi::gba::Emulator::DebugExecutionMode mode) {
  if (mode == kiwi::gba::Emulator::DebugExecutionMode::kArm) {
    return kiwi::base::StringPrintf("%08X", instruction.opcode);
  }
  if (instruction.width == 4) {
    return kiwi::base::StringPrintf("%04X %04X", instruction.opcode >> 16,
                                    instruction.opcode & 0xffff);
  }
  return kiwi::base::StringPrintf("%04X", instruction.opcode & 0xffff);
}

kiwi::gba::Color ColorFrom555(uint16_t color, bool transparent = false) {
  if (transparent)
    return 0;
  const uint32_t red = (color & 0x1f) * 255 / 31;
  const uint32_t green = ((color >> 5) & 0x1f) * 255 / 31;
  const uint32_t blue = ((color >> 10) & 0x1f) * 255 / 31;
  return 0xff000000 | (blue << 16) | (green << 8) | red;
}

ImVec4 ImGuiColorFrom555(uint16_t color) {
  return ImVec4((color & 0x1f) / 31.f, ((color >> 5) & 0x1f) / 31.f,
                ((color >> 10) & 0x1f) / 31.f, 1.f);
}

const char* GetWatchpointTypeName(
    kiwi::gba::Emulator::DebugWatchpointType type) {
  switch (type) {
    case kiwi::gba::Emulator::DebugWatchpointType::kWrite:
      return "Write";
    case kiwi::gba::Emulator::DebugWatchpointType::kRead:
      return "Read";
    case kiwi::gba::Emulator::DebugWatchpointType::kReadWrite:
      return "Read / Write";
    case kiwi::gba::Emulator::DebugWatchpointType::kWriteChange:
      return "Write Change";
  }
  return "Write";
}

const char* GetBreakReasonName(kiwi::gba::Emulator::DebugBreakReason reason) {
  switch (reason) {
    case kiwi::gba::Emulator::DebugBreakReason::kNone:
      return "None";
    case kiwi::gba::Emulator::DebugBreakReason::kBreakpoint:
      return "Breakpoint";
    case kiwi::gba::Emulator::DebugBreakReason::kWatchpoint:
      return "Watchpoint";
    case kiwi::gba::Emulator::DebugBreakReason::kIllegalInstruction:
      return "Illegal instruction";
    case kiwi::gba::Emulator::DebugBreakReason::kStack:
      return "Stack trace";
  }
  return "None";
}

}  // namespace

GBADebugWidget::GBADebugWidget(MainWindow* main_window,
                               kiwi::base::RepeatingClosure on_toggle_pause,
                               kiwi::base::RepeatingCallback<bool()> is_pause)
    : Widget(main_window),
      on_toggle_pause_(std::move(on_toggle_pause)),
      is_pause_(std::move(is_pause)),
      last_metrics_time_(std::chrono::steady_clock::now()) {
  set_flags(ImGuiWindowFlags_NoSavedSettings);
  set_bounds(SDL_Rect{80, 30, 1040, 660});
  set_title("GBA Debugger");
}

GBADebugWidget::~GBADebugWidget() {
  if (tile_texture_)
    SDL_DestroyTexture(tile_texture_);
  if (sprite_texture_)
    SDL_DestroyTexture(sprite_texture_);
}

void GBADebugWidget::SetEmulator(scoped_refptr<kiwi::gba::Emulator> emulator) {
  ++snapshot_request_id_;
  ++memory_request_id_;
  ++graphics_request_id_;
  ++audio_request_id_;
  emulator_ = std::move(emulator);
  snapshot_.reset();
  memory_snapshot_.reset();
  graphics_snapshot_.reset();
  audio_snapshot_.reset();
  snapshot_loading_ = false;
  memory_loading_ = false;
  graphics_loading_ = false;
  audio_loading_ = false;
  refresh_after_step_ = false;
  last_metrics_frame_count_ = 0;
  last_metrics_time_ = std::chrono::steady_clock::now();
}

void GBADebugWidget::Refresh() {
  if (!visible() || !emulator_)
    return;
  if (is_pause_.Run()) {
    RequestSnapshot();
    RequestMemorySnapshot();
    RequestGraphicsSnapshot();
  }
  RequestAudioSnapshot();
}

void GBADebugWidget::Paint() {
  if (!emulator_ || !emulator_->HasROM()) {
    ImGui::TextUnformatted("Load a GBA ROM to use the debugger.");
    return;
  }

  UpdatePerformanceMetrics();
  PaintToolbar();
  ImGui::Separator();

  if (ImGui::BeginTabBar("GBA Debugger Tabs")) {
    if (ImGui::BeginTabItem("CPU")) {
      PaintCpuTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Memory / I/O")) {
      PaintMemoryTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Graphics")) {
      PaintGraphicsTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Audio")) {
      PaintAudioTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Performance")) {
      PaintPerformanceTab();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

void GBADebugWidget::PaintToolbar() {
  const bool paused = is_pause_.Run();
  if (ImGui::Button(paused ? "Resume" : "Pause")) {
    if (paused) {
      ++snapshot_request_id_;
      ++memory_request_id_;
      ++graphics_request_id_;
      snapshot_loading_ = false;
      memory_loading_ = false;
      graphics_loading_ = false;
    }
    on_toggle_pause_.Run();
  }

  ImGui::SameLine();
  ImGui::BeginDisabled(!paused || snapshot_loading_);
  if (ImGui::Button("Step Instruction"))
    RequestSnapshot(true);
  ImGui::SameLine();
  if (ImGui::Button("Refresh"))
    Refresh();
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::TextUnformatted(paused ? "Paused" : "Running");
}

void GBADebugWidget::PaintCpuTab() {
  if (!is_pause_.Run()) {
    ImGui::TextUnformatted("Pause emulation to inspect CPU state.");
    return;
  }
  if (snapshot_loading_) {
    ImGui::TextUnformatted("Reading CPU state...");
    return;
  }
  if (!snapshot_) {
    ImGui::TextUnformatted("CPU state is not available.");
    return;
  }

  const char* execution_mode =
      snapshot_->execution_mode ==
              kiwi::gba::Emulator::DebugExecutionMode::kThumb
          ? "Thumb"
          : "ARM";
  ImGui::Text("Current PC: %08X    Mode: %s    Cycles: %llu",
              snapshot_->instruction_address, execution_mode,
              static_cast<unsigned long long>(snapshot_->cycles));

  if (ImGui::BeginTable(
          "GBA CPU Layout", 2,
          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Registers", ImGuiTableColumnFlags_WidthFixed,
                            330.f);
    ImGui::TableSetupColumn("Disassembly", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    PaintRegisters(*snapshot_);
    ImGui::TableSetColumnIndex(1);
    PaintDisassembly(*snapshot_);
    ImGui::EndTable();
  }

  ImGui::Separator();
  if (ImGui::BeginTable(
          "GBA Debug Controls", 2,
          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame)) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    PaintDebugPoints(*snapshot_);
    ImGui::TableSetColumnIndex(1);
    PaintStackTrace(*snapshot_);
    ImGui::EndTable();
  }
}

void GBADebugWidget::PaintMemoryTab() {
  if (!is_pause_.Run()) {
    ImGui::TextUnformatted("Pause emulation to inspect memory.");
    return;
  }
  if (!memory_snapshot_) {
    ImGui::TextUnformatted(memory_loading_ ? "Reading memory..."
                                           : "Memory is not available.");
    return;
  }

  const char* preview = "Memory region";
  for (const auto& region : memory_snapshot_->regions) {
    const uint32_t address = ParseAddress(memory_address_, 0);
    if (address >= region.start && address < region.end) {
      preview = region.name.c_str();
      break;
    }
  }
  if (ImGui::BeginCombo("Region", preview)) {
    for (const auto& region : memory_snapshot_->regions) {
      if (ImGui::Selectable(region.name.c_str())) {
        const std::string address =
            kiwi::base::StringPrintf("%08X", region.start);
        kiwi::base::strlcpy(memory_address_, address.c_str(),
                            sizeof(memory_address_));
        RequestMemorySnapshot();
      }
    }
    ImGui::EndCombo();
  }

  ImGui::InputText("Address", memory_address_, sizeof(memory_address_),
                   ImGuiInputTextFlags_CharsHexadecimal |
                       ImGuiInputTextFlags_CharsUppercase);
  ImGui::SameLine();
  if (ImGui::Button("Go"))
    RequestMemorySnapshot();

  if (ImGui::BeginTabBar("Memory Views")) {
    if (ImGui::BeginTabItem("Hex")) {
      ImGui::BeginChild("GBA Memory Hex", ImVec2(0, 490), true,
                        ImGuiWindowFlags_HorizontalScrollbar);
      for (size_t offset = 0; offset < memory_snapshot_->data.size();
           offset += 16) {
        std::string line = kiwi::base::StringPrintf(
            "%08X  ",
            memory_snapshot_->address + static_cast<uint32_t>(offset));
        for (size_t i = 0; i < 16; ++i) {
          if (offset + i < memory_snapshot_->data.size()) {
            kiwi::base::StringAppendF(&line, "%02X ",
                                      memory_snapshot_->data[offset + i]);
          } else {
            line += "   ";
          }
        }
        line += ' ';
        for (size_t i = 0; i < 16 && offset + i < memory_snapshot_->data.size();
             ++i) {
          const unsigned char value = memory_snapshot_->data[offset + i];
          line += value >= 32 && value < 127 ? value : '.';
        }
        ImGui::TextUnformatted(line.c_str());
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("MMIO")) {
      if (ImGui::BeginTable("GBA MMIO Registers", 3,
                            ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_BordersInnerH |
                                ImGuiTableFlags_ScrollY,
                            ImVec2(0, 490))) {
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Register");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();
        for (const auto& reg : memory_snapshot_->io_registers) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%08X", reg.address);
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(reg.name.c_str());
          ImGui::TableNextColumn();
          ImGui::Text("%04X", reg.value);
        }
        ImGui::EndTable();
      }
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

void GBADebugWidget::PaintGraphicsTab() {
  if (!is_pause_.Run()) {
    ImGui::TextUnformatted("Pause emulation to inspect graphics memory.");
    return;
  }
  if (!graphics_snapshot_) {
    ImGui::TextUnformatted(graphics_loading_
                               ? "Reading graphics state..."
                               : "Graphics state is unavailable.");
    return;
  }

  if (ImGui::BeginTabBar("GBA Graphics Views")) {
    if (ImGui::BeginTabItem("Palette")) {
      PaintPaletteView();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Tiles")) {
      PaintTilesView();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("OAM")) {
      PaintOAMView();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Layers")) {
      PaintLayersView();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

void GBADebugWidget::PaintPaletteView() {
  static int palette_group = 0;
  ImGui::RadioButton("BG palette", &palette_group, 0);
  ImGui::SameLine();
  ImGui::RadioButton("OBJ palette", &palette_group, 1);
  const size_t base = palette_group ? 256 : 0;
  for (size_t index = 0; index < 256; ++index) {
    ImGui::PushID(static_cast<int>(index));
    ImGui::ColorButton(
        "##color", ImGuiColorFrom555(graphics_snapshot_->palette[base + index]),
        ImGuiColorEditFlags_NoDragDrop, ImVec2(24, 24));
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%02X  BGR555=%04X", static_cast<unsigned>(index),
                        graphics_snapshot_->palette[base + index]);
    }
    ImGui::PopID();
    if ((index + 1) % 16)
      ImGui::SameLine();
  }
}

void GBADebugWidget::PaintTilesView() {
  const char* regions[] = {"BG VRAM", "OBJ VRAM"};
  bool settings_changed = false;
  if (ImGui::Combo("Source", &tile_region_, regions, 2)) {
    tile_character_block_ = 0;
    settings_changed = true;
  }
  if (ImGui::Checkbox("8bpp / 256 colors", &tile_8bpp_))
    settings_changed = true;
  const int display_mode = graphics_snapshot_->display_control & 7;
  const int first_character_block = tile_region_ && display_mode >= 3 ? 1 : 0;
  const int character_block_count = tile_region_ ? 2 : 4;
  const int clamped_character_block = std::clamp(
      tile_character_block_, first_character_block, character_block_count - 1);
  if (tile_character_block_ != clamped_character_block) {
    tile_character_block_ = clamped_character_block;
    settings_changed = true;
  }
  if (ImGui::SliderInt("Character block", &tile_character_block_,
                       first_character_block, character_block_count - 1)) {
    settings_changed = true;
  }
  ImGui::BeginDisabled(tile_8bpp_);
  if (ImGui::SliderInt("Palette bank", &tile_palette_, 0, 15))
    settings_changed = true;
  ImGui::EndDisabled();
  if (settings_changed)
    UpdateTileTexture();

  if (tile_texture_) {
    const int atlas_width = tile_8bpp_ ? kTileAtlasWidth / 2 : kTileAtlasWidth;
    const size_t base =
        (tile_region_ ? 0x10000 : 0) +
        static_cast<size_t>(tile_character_block_) * kCharacterBlockBytes;
    ImGui::Text("VRAM %05X-%05X", static_cast<unsigned>(base),
                static_cast<unsigned>(base + kCharacterBlockBytes - 1));
    ImGui::Image(reinterpret_cast<ImTextureID>(tile_texture_),
                 ImVec2(atlas_width * 2, kTileAtlasHeight * 2), ImVec2(0, 0),
                 ImVec2(static_cast<float>(atlas_width) / kTileAtlasWidth, 1));
  }
}

void GBADebugWidget::PaintOAMView() {
  const auto& sprites = graphics_snapshot_->sprites;
  if (ImGui::BeginTable("GBA OAM", 7,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_ScrollY,
                        ImVec2(690, 490))) {
    ImGui::TableSetupColumn("#");
    ImGui::TableSetupColumn("Enabled");
    ImGui::TableSetupColumn("Position");
    ImGui::TableSetupColumn("Size");
    ImGui::TableSetupColumn("Tile");
    ImGui::TableSetupColumn("Palette");
    ImGui::TableSetupColumn("Priority");
    ImGui::TableHeadersRow();
    for (const auto& sprite : sprites) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      const std::string label = kiwi::base::StringPrintf("%u", sprite.index);
      if (ImGui::Selectable(label.c_str(), selected_sprite_ == sprite.index,
                            ImGuiSelectableFlags_SpanAllColumns)) {
        selected_sprite_ = sprite.index;
        UpdateSpriteTexture();
      }
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(sprite.enabled ? "Yes" : "No");
      ImGui::TableNextColumn();
      ImGui::Text("%d, %d", sprite.x, sprite.y);
      ImGui::TableNextColumn();
      ImGui::Text("%ux%u", sprite.width, sprite.height);
      ImGui::TableNextColumn();
      ImGui::Text("%03X", sprite.tile);
      ImGui::TableNextColumn();
      ImGui::Text("%u%s", sprite.palette, sprite.color_256 ? " (8bpp)" : "");
      ImGui::TableNextColumn();
      ImGui::Text("%u", sprite.priority);
    }
    ImGui::EndTable();
  }

  ImGui::SameLine();
  ImGui::BeginGroup();
  if (selected_sprite_ >= 0 &&
      selected_sprite_ < static_cast<int>(sprites.size())) {
    const auto& sprite = sprites[selected_sprite_];
    ImGui::Text("OBJ %d", selected_sprite_);
    ImGui::Text("Tile %03X", sprite.tile);
    ImGui::Text("%s%s%s", sprite.transformed ? "Affine " : "",
                sprite.hflip ? "HFlip " : "", sprite.vflip ? "VFlip" : "");
    if (sprite_texture_) {
      ImGui::Image(reinterpret_cast<ImTextureID>(sprite_texture_),
                   ImVec2(kSpriteTextureSize * 3, kSpriteTextureSize * 3));
    }
  }
  ImGui::EndGroup();
}

void GBADebugWidget::PaintLayersView() {
  ImGui::Text("DISPCNT: %04X", graphics_snapshot_->display_control);
  for (auto& layer : graphics_snapshot_->layers) {
    bool enabled = layer.enabled;
    std::string label = layer.name;
    if (!layer.type.empty())
      label += " (" + layer.type + ")";
    if (ImGui::Checkbox(label.c_str(), &enabled)) {
      layer.enabled = enabled;
      std::weak_ptr<int> weak_lifetime = lifetime_token_;
      emulator_->SetDebugVideoLayerEnabled(
          layer.id, enabled,
          kiwi::base::BindOnce(&GBADebugWidget::DispatchResult,
                               std::move(weak_lifetime), this));
    }
  }
}

void GBADebugWidget::PaintAudioTab() {
  if (!audio_snapshot_) {
    ImGui::TextUnformatted(audio_loading_ ? "Reading audio state..."
                                          : "Audio state is unavailable.");
    return;
  }
  for (auto& channel : audio_snapshot_->channels) {
    bool enabled = channel.enabled;
    std::string label = channel.name;
    if (!channel.type.empty())
      label += " (" + channel.type + ")";
    if (ImGui::Checkbox(label.c_str(), &enabled)) {
      channel.enabled = enabled;
      std::weak_ptr<int> weak_lifetime = lifetime_token_;
      emulator_->SetDebugAudioChannelEnabled(
          channel.id, enabled,
          kiwi::base::BindOnce(&GBADebugWidget::DispatchResult,
                               std::move(weak_lifetime), this));
    }
  }
  const auto metrics = emulator_->GetDebugMetrics();
  ImGui::Separator();
  ImGui::Text("Last mixed samples: %u", metrics.last_audio_sample_count);
}

void GBADebugWidget::PaintPerformanceTab() {
  const auto metrics = emulator_->GetDebugMetrics();
  ImGui::Text("Emulated frames: %llu",
              static_cast<unsigned long long>(metrics.frame_count));
  ImGui::Text("Last frame cost: %.3f ms",
              metrics.last_frame_duration_us / 1000.f);
  ImGui::PlotLines("Emulation FPS", frame_rate_samples_.data(),
                   frame_rate_samples_.size(), frame_rate_index_, nullptr, 0.f,
                   120.f, ImVec2(0, 130));
  ImGui::PlotLines("Frame cost (ms)", frame_time_samples_.data(),
                   frame_time_samples_.size(), frame_time_index_, nullptr, 0.f,
                   40.f, ImVec2(0, 130));
  ImGui::PlotLines("Audio samples / frame", audio_samples_.data(),
                   audio_samples_.size(), audio_index_, nullptr, 0.f, 2048.f,
                   ImVec2(0, 130));
}

void GBADebugWidget::PaintRegisters(
    const kiwi::gba::Emulator::DebugSnapshot& snapshot) {
  if (ImGui::BeginTable("GBA Registers", 4,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_SizingFixedFit)) {
    for (size_t row = 0; row < 8; ++row) {
      const size_t left = row;
      const size_t right = row + 8;
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(GetRegisterName(left));
      ImGui::TableNextColumn();
      ImGui::Text("%08X", snapshot.registers[left]);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(GetRegisterName(right));
      ImGui::TableNextColumn();
      ImGui::Text("%08X", snapshot.registers[right]);
    }
    ImGui::EndTable();
  }
  const std::string flags = GetStatusFlags(snapshot.cpsr);
  ImGui::Text("CPSR  %08X  [%s]", snapshot.cpsr, flags.c_str());
  ImGui::Text("SPSR  %08X", snapshot.spsr);
  ImGui::Text("Privilege  %s", GetPrivilegeModeName(snapshot.cpsr));
}

void GBADebugWidget::PaintDisassembly(
    const kiwi::gba::Emulator::DebugSnapshot& snapshot) {
  ImGui::BeginChild("GBA Disassembly", ImVec2(0, 260), true,
                    ImGuiWindowFlags_HorizontalScrollbar);
  for (const auto& instruction : snapshot.instructions) {
    const bool current = instruction.address == snapshot.instruction_address;
    const std::string opcode =
        FormatOpcode(instruction, snapshot.execution_mode);
    if (current) {
      ImGui::TextColored(ImVec4(1.f, 0.78f, 0.25f, 1.f), "> %08X  %-9s  %s",
                         instruction.address, opcode.c_str(),
                         instruction.text.c_str());
    } else {
      ImGui::Text("  %08X  %-9s  %s", instruction.address, opcode.c_str(),
                  instruction.text.c_str());
    }
  }
  ImGui::EndChild();
}

void GBADebugWidget::PaintDebugPoints(
    const kiwi::gba::Emulator::DebugSnapshot& snapshot) {
  ImGui::TextUnformatted("Breakpoints");
  ImGui::InputText("##breakpoint", breakpoint_address_,
                   sizeof(breakpoint_address_),
                   ImGuiInputTextFlags_CharsHexadecimal |
                       ImGuiInputTextFlags_CharsUppercase);
  ImGui::SameLine();
  if (ImGui::Button("Add##breakpoint")) {
    std::weak_ptr<int> weak_lifetime = lifetime_token_;
    emulator_->AddDebugBreakpoint(
        ParseAddress(breakpoint_address_, snapshot.instruction_address),
        kiwi::base::BindOnce(&GBADebugWidget::DispatchResult,
                             std::move(weak_lifetime), this));
  }

  ImGui::TextUnformatted("Watchpoints");
  ImGui::InputText("##watchpoint", watchpoint_address_,
                   sizeof(watchpoint_address_),
                   ImGuiInputTextFlags_CharsHexadecimal |
                       ImGuiInputTextFlags_CharsUppercase);
  const char* watch_types[] = {"Write", "Read", "Read / Write", "Write Change"};
  ImGui::Combo("##watchpoint_type", &watchpoint_type_, watch_types, 4);
  ImGui::SameLine();
  if (ImGui::Button("Add##watchpoint")) {
    std::weak_ptr<int> weak_lifetime = lifetime_token_;
    emulator_->AddDebugWatchpoint(
        ParseAddress(watchpoint_address_, 0x02000000),
        static_cast<kiwi::gba::Emulator::DebugWatchpointType>(watchpoint_type_),
        kiwi::base::BindOnce(&GBADebugWidget::DispatchResult,
                             std::move(weak_lifetime), this));
  }

  ImGui::BeginChild("GBA Debug Points", ImVec2(0, 105), true);
  for (const auto& point : snapshot.points) {
    ImGui::PushID(static_cast<int>(point.id));
    if (point.type == kiwi::gba::Emulator::DebugPointType::kBreakpoint) {
      ImGui::Text("BP  %08X", point.address);
    } else {
      ImGui::Text("WP  %08X  %s", point.address,
                  GetWatchpointTypeName(point.watchpoint_type));
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove")) {
      std::weak_ptr<int> weak_lifetime = lifetime_token_;
      emulator_->RemoveDebugPoint(
          point.id, kiwi::base::BindOnce(&GBADebugWidget::DispatchResult,
                                         std::move(weak_lifetime), this));
    }
    ImGui::PopID();
  }
  ImGui::EndChild();

  if (snapshot.last_break.reason !=
      kiwi::gba::Emulator::DebugBreakReason::kNone) {
    ImGui::Text("Last stop: %s at %08X",
                GetBreakReasonName(snapshot.last_break.reason),
                snapshot.last_break.address);
    if (snapshot.last_break.reason ==
        kiwi::gba::Emulator::DebugBreakReason::kWatchpoint) {
      ImGui::Text("Value: %08X -> %08X", snapshot.last_break.old_value,
                  snapshot.last_break.new_value);
    }
  }
}

void GBADebugWidget::PaintStackTrace(
    const kiwi::gba::Emulator::DebugSnapshot& snapshot) {
  bool enabled = snapshot.stack_trace_enabled;
  if (ImGui::Checkbox("Track call stack", &enabled)) {
    std::weak_ptr<int> weak_lifetime = lifetime_token_;
    emulator_->SetDebugStackTraceEnabled(
        enabled, kiwi::base::BindOnce(&GBADebugWidget::DispatchResult,
                                      std::move(weak_lifetime), this));
  }
  if (ImGui::BeginTable("GBA Stack Frames", 4,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_ScrollY,
                        ImVec2(0, 225))) {
    ImGui::TableSetupColumn("#");
    ImGui::TableSetupColumn("Entry");
    ImGui::TableSetupColumn("Call");
    ImGui::TableSetupColumn("SP");
    ImGui::TableHeadersRow();
    for (size_t i = 0; i < snapshot.stack_frames.size(); ++i) {
      const auto& frame = snapshot.stack_frames[i];
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%zu%s", i, frame.interrupt ? " IRQ" : "");
      ImGui::TableNextColumn();
      ImGui::Text("%08X", frame.entry_address);
      ImGui::TableNextColumn();
      ImGui::Text("%08X", frame.call_address);
      ImGui::TableNextColumn();
      ImGui::Text("%08X", frame.frame_base_address);
    }
    ImGui::EndTable();
  }
}

void GBADebugWidget::RequestSnapshot(bool step) {
  if (!emulator_ || !emulator_->HasROM() || !is_pause_.Run())
    return;
  snapshot_loading_ = true;
  refresh_after_step_ = step;
  const uint64_t request_id = ++snapshot_request_id_;
  std::weak_ptr<int> weak_lifetime = lifetime_token_;
  auto callback =
      kiwi::base::BindOnce(&GBADebugWidget::DispatchSnapshot,
                           std::move(weak_lifetime), this, request_id);
  if (step) {
    emulator_->StepAndGetDebugSnapshot(kDisassemblyInstructionCount,
                                       std::move(callback));
  } else {
    emulator_->GetDebugSnapshot(kDisassemblyInstructionCount,
                                std::move(callback));
  }
}

void GBADebugWidget::RequestMemorySnapshot() {
  if (!emulator_ || !emulator_->HasROM() || !is_pause_.Run())
    return;
  memory_loading_ = true;
  const uint64_t request_id = ++memory_request_id_;
  std::weak_ptr<int> weak_lifetime = lifetime_token_;
  emulator_->GetDebugMemorySnapshot(
      ParseAddress(memory_address_, 0x02000000), kMemoryByteCount,
      kiwi::base::BindOnce(&GBADebugWidget::DispatchMemorySnapshot,
                           std::move(weak_lifetime), this, request_id));
}

void GBADebugWidget::RequestGraphicsSnapshot() {
  if (!emulator_ || !emulator_->HasROM() || !is_pause_.Run())
    return;
  graphics_loading_ = true;
  const uint64_t request_id = ++graphics_request_id_;
  std::weak_ptr<int> weak_lifetime = lifetime_token_;
  emulator_->GetDebugGraphicsSnapshot(
      kiwi::base::BindOnce(&GBADebugWidget::DispatchGraphicsSnapshot,
                           std::move(weak_lifetime), this, request_id));
}

void GBADebugWidget::RequestAudioSnapshot() {
  if (!emulator_ || !emulator_->HasROM())
    return;
  audio_loading_ = true;
  const uint64_t request_id = ++audio_request_id_;
  std::weak_ptr<int> weak_lifetime = lifetime_token_;
  emulator_->GetDebugAudioSnapshot(
      kiwi::base::BindOnce(&GBADebugWidget::DispatchAudioSnapshot,
                           std::move(weak_lifetime), this, request_id));
}

void GBADebugWidget::DispatchSnapshot(
    std::weak_ptr<int> weak_lifetime,
    GBADebugWidget* widget,
    uint64_t request_id,
    std::optional<kiwi::gba::Emulator::DebugSnapshot> snapshot) {
  if (!weak_lifetime.expired())
    widget->OnSnapshot(request_id, std::move(snapshot));
}

void GBADebugWidget::DispatchMemorySnapshot(
    std::weak_ptr<int> weak_lifetime,
    GBADebugWidget* widget,
    uint64_t request_id,
    std::optional<kiwi::gba::Emulator::DebugMemorySnapshot> snapshot) {
  if (!weak_lifetime.expired())
    widget->OnMemorySnapshot(request_id, std::move(snapshot));
}

void GBADebugWidget::DispatchGraphicsSnapshot(
    std::weak_ptr<int> weak_lifetime,
    GBADebugWidget* widget,
    uint64_t request_id,
    std::optional<kiwi::gba::Emulator::DebugGraphicsSnapshot> snapshot) {
  if (!weak_lifetime.expired())
    widget->OnGraphicsSnapshot(request_id, std::move(snapshot));
}

void GBADebugWidget::DispatchAudioSnapshot(
    std::weak_ptr<int> weak_lifetime,
    GBADebugWidget* widget,
    uint64_t request_id,
    std::optional<kiwi::gba::Emulator::DebugAudioSnapshot> snapshot) {
  if (!weak_lifetime.expired())
    widget->OnAudioSnapshot(request_id, std::move(snapshot));
}

void GBADebugWidget::DispatchResult(std::weak_ptr<int> weak_lifetime,
                                    GBADebugWidget* widget,
                                    bool success) {
  if (!weak_lifetime.expired() && success)
    widget->Refresh();
}

void GBADebugWidget::OnSnapshot(
    uint64_t request_id,
    std::optional<kiwi::gba::Emulator::DebugSnapshot> snapshot) {
  if (request_id != snapshot_request_id_)
    return;
  snapshot_loading_ = false;
  snapshot_ = std::move(snapshot);
  if (refresh_after_step_) {
    refresh_after_step_ = false;
    RequestMemorySnapshot();
    RequestGraphicsSnapshot();
    RequestAudioSnapshot();
  }
}

void GBADebugWidget::OnMemorySnapshot(
    uint64_t request_id,
    std::optional<kiwi::gba::Emulator::DebugMemorySnapshot> snapshot) {
  if (request_id != memory_request_id_)
    return;
  memory_loading_ = false;
  memory_snapshot_ = std::move(snapshot);
}

void GBADebugWidget::OnGraphicsSnapshot(
    uint64_t request_id,
    std::optional<kiwi::gba::Emulator::DebugGraphicsSnapshot> snapshot) {
  if (request_id != graphics_request_id_)
    return;
  graphics_loading_ = false;
  graphics_snapshot_ = std::move(snapshot);
  UpdateTileTexture();
  UpdateSpriteTexture();
}

void GBADebugWidget::OnAudioSnapshot(
    uint64_t request_id,
    std::optional<kiwi::gba::Emulator::DebugAudioSnapshot> snapshot) {
  if (request_id != audio_request_id_)
    return;
  audio_loading_ = false;
  audio_snapshot_ = std::move(snapshot);
}

void GBADebugWidget::UpdateTileTexture() {
  if (!graphics_snapshot_)
    return;
  if (!tile_texture_) {
    tile_texture_ = SDL_CreateTexture(
        window()->renderer(), SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING, kTileAtlasWidth, kTileAtlasHeight);
    if (tile_texture_)
      SDL_SetTextureScaleMode(tile_texture_, SDL_ScaleModeNearest);
  }
  if (!tile_texture_)
    return;

  tile_pixels_.assign(kTileAtlasWidth * kTileAtlasHeight, 0xff000000);
  const size_t bytes_per_tile = tile_8bpp_ ? 64 : 32;
  const size_t base =
      (tile_region_ ? 0x10000 : 0) +
      static_cast<size_t>(tile_character_block_) * kCharacterBlockBytes;
  const size_t tile_count = kCharacterBlockBytes / bytes_per_tile;
  const size_t tiles_per_row = tile_8bpp_ ? 16 : 32;
  const size_t palette_base = tile_region_ ? 256 : 0;
  for (size_t tile = 0; tile < tile_count; ++tile) {
    const size_t tile_offset = base + tile * bytes_per_tile;
    if (tile_offset + bytes_per_tile > graphics_snapshot_->vram.size())
      break;
    const size_t tile_x = (tile % tiles_per_row) * 8;
    const size_t tile_y = (tile / tiles_per_row) * 8;
    for (size_t y = 0; y < 8; ++y) {
      for (size_t x = 0; x < 8; ++x) {
        uint8_t index;
        if (tile_8bpp_) {
          index = graphics_snapshot_->vram[tile_offset + y * 8 + x];
        } else {
          const uint8_t packed =
              graphics_snapshot_->vram[tile_offset + y * 4 + x / 2];
          index = (packed >> ((x & 1) * 4)) & 0xf;
          index += static_cast<uint8_t>(tile_palette_ * 16);
        }
        tile_pixels_[(tile_y + y) * kTileAtlasWidth + tile_x + x] =
            ColorFrom555(graphics_snapshot_->palette[palette_base + index]);
      }
    }
  }
  SDL_UpdateTexture(tile_texture_, nullptr, tile_pixels_.data(),
                    kTileAtlasWidth * sizeof(tile_pixels_[0]));
}

void GBADebugWidget::UpdateSpriteTexture() {
  if (!graphics_snapshot_ || selected_sprite_ < 0 ||
      selected_sprite_ >=
          static_cast<int>(graphics_snapshot_->sprites.size())) {
    return;
  }
  if (!sprite_texture_) {
    sprite_texture_ = SDL_CreateTexture(
        window()->renderer(), SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING, kSpriteTextureSize, kSpriteTextureSize);
    if (sprite_texture_) {
      SDL_SetTextureBlendMode(sprite_texture_, SDL_BLENDMODE_BLEND);
      SDL_SetTextureScaleMode(sprite_texture_, SDL_ScaleModeNearest);
    }
  }
  if (!sprite_texture_)
    return;

  sprite_pixels_.assign(kSpriteTextureSize * kSpriteTextureSize, 0);
  const auto& sprite = graphics_snapshot_->sprites[selected_sprite_];
  if (!sprite.enabled || !sprite.width || !sprite.height) {
    SDL_UpdateTexture(sprite_texture_, nullptr, sprite_pixels_.data(),
                      kSpriteTextureSize * sizeof(sprite_pixels_[0]));
    return;
  }

  const bool one_dimensional =
      (graphics_snapshot_->display_control & (1u << 6)) != 0;
  const int display_mode = graphics_snapshot_->display_control & 7;
  if (display_mode >= 3 && sprite.tile < 512) {
    SDL_UpdateTexture(sprite_texture_, nullptr, sprite_pixels_.data(),
                      kSpriteTextureSize * sizeof(sprite_pixels_[0]));
    return;
  }
  const size_t units_per_tile = sprite.color_256 ? 2 : 1;
  const size_t base_tile = sprite.color_256 ? sprite.tile & ~1u : sprite.tile;
  const size_t tile_columns = sprite.width / 8;
  for (size_t y = 0; y < sprite.height && y < kSpriteTextureSize; ++y) {
    for (size_t x = 0; x < sprite.width && x < kSpriteTextureSize; ++x) {
      const size_t source_x = sprite.hflip ? sprite.width - x - 1 : x;
      const size_t source_y = sprite.vflip ? sprite.height - y - 1 : y;
      const size_t tile_x = source_x / 8;
      const size_t tile_y = source_y / 8;
      const size_t tile_unit =
          base_tile +
          (one_dimensional ? tile_y * tile_columns * units_per_tile
                           : tile_y * 32) +
          tile_x * units_per_tile;
      const size_t tile_offset = 0x10000 + tile_unit * 32;
      uint8_t index = 0;
      if (sprite.color_256) {
        const size_t offset = tile_offset + (source_y & 7) * 8 + (source_x & 7);
        if (offset < graphics_snapshot_->vram.size())
          index = graphics_snapshot_->vram[offset];
      } else {
        const size_t offset =
            tile_offset + (source_y & 7) * 4 + (source_x & 7) / 2;
        if (offset < graphics_snapshot_->vram.size()) {
          const uint8_t packed = graphics_snapshot_->vram[offset];
          index = (packed >> ((source_x & 1) * 4)) & 0xf;
        }
      }
      if (!index)
        continue;
      const size_t palette_index =
          256 + (sprite.color_256 ? index : sprite.palette * 16 + index);
      sprite_pixels_[y * kSpriteTextureSize + x] =
          ColorFrom555(graphics_snapshot_->palette[palette_index]);
    }
  }
  SDL_UpdateTexture(sprite_texture_, nullptr, sprite_pixels_.data(),
                    kSpriteTextureSize * sizeof(sprite_pixels_[0]));
}

void GBADebugWidget::UpdatePerformanceMetrics() {
  if (!emulator_)
    return;
  const auto metrics = emulator_->GetDebugMetrics();
  if (metrics.frame_count == last_metrics_frame_count_)
    return;
  const auto now = std::chrono::steady_clock::now();
  if (metrics.frame_count < last_metrics_frame_count_) {
    last_metrics_frame_count_ = metrics.frame_count;
    last_metrics_time_ = now;
    return;
  }
  if (last_metrics_frame_count_) {
    const float seconds =
        std::chrono::duration<float>(now - last_metrics_time_).count();
    const uint64_t frames = metrics.frame_count - last_metrics_frame_count_;
    if (seconds > 0) {
      AddPlotSample(&frame_rate_samples_, &frame_rate_index_, frames / seconds);
    }
  }
  AddPlotSample(&frame_time_samples_, &frame_time_index_,
                metrics.last_frame_duration_us / 1000.f);
  AddPlotSample(&audio_samples_, &audio_index_,
                static_cast<float>(metrics.last_audio_sample_count));
  last_metrics_frame_count_ = metrics.frame_count;
  last_metrics_time_ = now;
}

void GBADebugWidget::AddPlotSample(std::array<float, kPlotSampleCount>* plot,
                                   size_t* index,
                                   float value) {
  (*plot)[*index] = value;
  *index = (*index + 1) % plot->size();
}

uint32_t GBADebugWidget::ParseAddress(const char* value,
                                      uint32_t fallback) const {
  if (!value || !*value)
    return fallback;
  char* end = nullptr;
  const unsigned long parsed = std::strtoul(value, &end, 16);
  return end && *end == '\0' ? static_cast<uint32_t>(parsed) : fallback;
}
