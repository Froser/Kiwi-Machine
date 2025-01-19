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

#ifndef UI_WIDGETS_FRAME_RATE_WIDGET_H_
#define UI_WIDGETS_FRAME_RATE_WIDGET_H_

#include <kiwi_nes.h>

#include "debug/debug_port.h"
#include "models/nes_frame.h"
#include "ui/application.h"
#include "ui/widgets/widget.h"

// A demo widget shows IMGui's demo.
class DebugPort;
class PerformanceWidget : public Widget,
                          public NESFrameObserver,
                          public ApplicationObserver,
                          public DebugPortObserver {
 public:
  enum {
    kSampleCount = 60,
  };

  explicit PerformanceWidget(WindowBase* window_base,
                             scoped_refptr<NESFrame> frame,
                             DebugPort* debug_port);
  ~PerformanceWidget() override;

 protected:
  // Widget:
  void Paint() override;

  // NESFrameObserver:
  void OnShouldRender(int since_last_frame_ms) override;

  // ApplicationObserver:
  void OnPreRender(int since_last_frame_ms) override;

  // DebugPortObserver:
  void OnFrameEnd(int since_last_frame_duration_ms,
                  int cpu_last_frame_duration_ms,
                  int ppu_last_frame_duration_ms) override;

 private:
  struct Plot {
    float samples[kSampleCount] = {0};
    size_t index = 0;
  };

 private:
  void Update(Plot* plot, float value);

 private:
  scoped_refptr<NESFrame> frame_;
  DebugPort* debug_port_ = nullptr;

  Plot app_frame_since_last_;
  Plot nes_frame_generate_;
  Plot nes_frame_present_;
  Plot nes_cpu_ppu_total_ms_costs_per_frame_;
  Plot nes_cpu_ms_costs_per_frame_;
  Plot nes_ppu_ms_costs_per_frame_;
};

#endif  // UI_WIDGETS_FRAME_RATE_WIDGET_H_