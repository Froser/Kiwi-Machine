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

#ifndef UI_WIDGETS_KIWI_BG_WIDGET_H_
#define UI_WIDGETS_KIWI_BG_WIDGET_H_

#include <kiwi_nes.h>

#include <SDL.h>
#include <vector>

#include "models/nes_runtime.h"
#include "ui/widgets/widget.h"
#include "utility/timer.h"

class MainWindow;
class LoadingWidget;
class KiwiBgWidget : public Widget {
 public:
  explicit KiwiBgWidget(MainWindow* main_window, NESRuntimeID runtime_id);
  ~KiwiBgWidget() override;

 public:
  void SetLoading(bool is_loading);

 protected:
  void Paint() override;
  bool IsWindowless() override;

 private:
  MainWindow* main_window_ = nullptr;
  NESRuntime::Data* runtime_data_ = nullptr;
  bool is_loading_ = false;
  Timer bg_last_render_elapsed_;
  Timer bg_fade_out_timer_;
  int current_index_ = 0;
};

#endif  // UI_WIDGETS_KIWI_BG_WIDGET_H_