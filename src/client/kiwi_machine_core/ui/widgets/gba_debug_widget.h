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

#ifndef UI_WIDGETS_GBA_DEBUG_WIDGET_H_
#define UI_WIDGETS_GBA_DEBUG_WIDGET_H_

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "emulator/gba/emulator.h"
#include "ui/widgets/widget.h"

class MainWindow;

class GBADebugWidget : public Widget {
 public:
  GBADebugWidget(MainWindow* main_window,
                 kiwi::base::RepeatingClosure on_toggle_pause,
                 kiwi::base::RepeatingCallback<bool()> is_pause);
  ~GBADebugWidget() override;

  void SetEmulator(scoped_refptr<kiwi::gba::Emulator> emulator);
  void Refresh();

 protected:
  void Paint() override;

 private:
  static constexpr size_t kPlotSampleCount = 120;

  static void DispatchSnapshot(
      std::weak_ptr<int> weak_lifetime,
      GBADebugWidget* widget,
      uint64_t request_id,
      std::optional<kiwi::gba::Emulator::DebugSnapshot> snapshot);
  static void DispatchMemorySnapshot(
      std::weak_ptr<int> weak_lifetime,
      GBADebugWidget* widget,
      uint64_t request_id,
      std::optional<kiwi::gba::Emulator::DebugMemorySnapshot> snapshot);
  static void DispatchGraphicsSnapshot(
      std::weak_ptr<int> weak_lifetime,
      GBADebugWidget* widget,
      uint64_t request_id,
      std::optional<kiwi::gba::Emulator::DebugGraphicsSnapshot> snapshot);
  static void DispatchAudioSnapshot(
      std::weak_ptr<int> weak_lifetime,
      GBADebugWidget* widget,
      uint64_t request_id,
      std::optional<kiwi::gba::Emulator::DebugAudioSnapshot> snapshot);
  static void DispatchResult(std::weak_ptr<int> weak_lifetime,
                             GBADebugWidget* widget,
                             bool success);

  void RequestSnapshot(bool step = false);
  void RequestMemorySnapshot();
  void RequestGraphicsSnapshot();
  void RequestAudioSnapshot();
  void OnSnapshot(uint64_t request_id,
                  std::optional<kiwi::gba::Emulator::DebugSnapshot> snapshot);
  void OnMemorySnapshot(
      uint64_t request_id,
      std::optional<kiwi::gba::Emulator::DebugMemorySnapshot> snapshot);
  void OnGraphicsSnapshot(
      uint64_t request_id,
      std::optional<kiwi::gba::Emulator::DebugGraphicsSnapshot> snapshot);
  void OnAudioSnapshot(
      uint64_t request_id,
      std::optional<kiwi::gba::Emulator::DebugAudioSnapshot> snapshot);

  void PaintToolbar();
  void PaintCpuTab();
  void PaintMemoryTab();
  void PaintGraphicsTab();
  void PaintPaletteView();
  void PaintTilesView();
  void PaintOAMView();
  void PaintLayersView();
  void PaintAudioTab();
  void PaintPerformanceTab();
  void PaintRegisters(const kiwi::gba::Emulator::DebugSnapshot& snapshot);
  void PaintDisassembly(const kiwi::gba::Emulator::DebugSnapshot& snapshot);
  void PaintDebugPoints(const kiwi::gba::Emulator::DebugSnapshot& snapshot);
  void PaintStackTrace(const kiwi::gba::Emulator::DebugSnapshot& snapshot);

  void UpdateTileTexture();
  void UpdateSpriteTexture();
  void UpdatePerformanceMetrics();
  void AddPlotSample(std::array<float, kPlotSampleCount>* plot,
                     size_t* index,
                     float value);
  uint32_t ParseAddress(const char* value, uint32_t fallback) const;

  kiwi::base::RepeatingClosure on_toggle_pause_;
  kiwi::base::RepeatingCallback<bool()> is_pause_;
  scoped_refptr<kiwi::gba::Emulator> emulator_;
  std::optional<kiwi::gba::Emulator::DebugSnapshot> snapshot_;
  std::optional<kiwi::gba::Emulator::DebugMemorySnapshot> memory_snapshot_;
  std::optional<kiwi::gba::Emulator::DebugGraphicsSnapshot> graphics_snapshot_;
  std::optional<kiwi::gba::Emulator::DebugAudioSnapshot> audio_snapshot_;
  std::shared_ptr<int> lifetime_token_ = std::make_shared<int>(0);
  uint64_t snapshot_request_id_ = 0;
  uint64_t memory_request_id_ = 0;
  uint64_t graphics_request_id_ = 0;
  uint64_t audio_request_id_ = 0;
  bool snapshot_loading_ = false;
  bool memory_loading_ = false;
  bool graphics_loading_ = false;
  bool audio_loading_ = false;
  bool refresh_after_step_ = false;

  char memory_address_[9] = "02000000";
  char breakpoint_address_[9] = "08000000";
  char watchpoint_address_[9] = "02000000";
  int watchpoint_type_ = 0;
  int selected_sprite_ = 0;
  int tile_region_ = 0;
  int tile_character_block_ = 0;
  int tile_palette_ = 0;
  bool tile_8bpp_ = false;
  SDL_Texture* tile_texture_ = nullptr;
  SDL_Texture* sprite_texture_ = nullptr;
  std::vector<kiwi::gba::Color> tile_pixels_;
  std::vector<kiwi::gba::Color> sprite_pixels_;

  std::array<float, kPlotSampleCount> frame_rate_samples_ = {};
  std::array<float, kPlotSampleCount> frame_time_samples_ = {};
  std::array<float, kPlotSampleCount> audio_samples_ = {};
  size_t frame_rate_index_ = 0;
  size_t frame_time_index_ = 0;
  size_t audio_index_ = 0;
  uint64_t last_metrics_frame_count_ = 0;
  std::chrono::steady_clock::time_point last_metrics_time_;
};

#endif  // UI_WIDGETS_GBA_DEBUG_WIDGET_H_
