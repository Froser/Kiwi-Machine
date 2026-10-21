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

#ifndef UI_WIDGETS_NES_DEBUG_WIDGET_H_
#define UI_WIDGETS_NES_DEBUG_WIDGET_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "ui/widgets/widget.h"

class DebugPort;
class DisassemblyWidget;
class MainWindow;
class MemoryWidget;
class NametableWidget;
class NESFrame;
class PaletteWidget;
class PatternWidget;
class PerformanceWidget;

class NESDebugWidget : public Widget {
 public:
  NESDebugWidget(MainWindow* main_window,
                 scoped_refptr<NESFrame> frame,
                 kiwi::base::RepeatingClosure on_toggle_pause,
                 kiwi::base::RepeatingCallback<bool()> is_pause);
  ~NESDebugWidget() override;

  void SetDebugPort(DebugPort* debug_port);
  void Refresh();

 protected:
  void Paint() override;
  void OnWindowPreRender() override;

 private:
  void PaintToolbar();
  void PaintGraphicsTab();
  void PaintAudioTab();

  MainWindow* main_window_ = nullptr;
  kiwi::base::RepeatingClosure on_toggle_pause_;
  kiwi::base::RepeatingCallback<bool()> is_pause_;
  DebugPort* debug_port_ = nullptr;
  bool nametable_tab_visible_ = false;

  std::unique_ptr<PaletteWidget> palette_widget_;
  std::unique_ptr<PatternWidget> pattern_widget_;
  std::unique_ptr<PerformanceWidget> performance_widget_;
  std::unique_ptr<MemoryWidget> memory_widget_;
  std::unique_ptr<DisassemblyWidget> disassembly_widget_;
  std::unique_ptr<NametableWidget> nametable_widget_;
};

#endif  // UI_WIDGETS_NES_DEBUG_WIDGET_H_
