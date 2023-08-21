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

#ifndef UI_WIDGETS_DISASSEMBLY_WIDGET_H_
#define UI_WIDGETS_DISASSEMBLY_WIDGET_H_

#include <string>

#include "models/nes_runtime.h"
#include "ui/widgets/widget.h"

class StackWidget;
class DisassemblyWidget : public Widget {
 public:
  explicit DisassemblyWidget(WindowBase* window_base,
                             NESRuntimeID runtime_id,
                             kiwi::base::RepeatingClosure on_toggle_pause,
                             kiwi::base::RepeatingCallback<bool()> is_pause);
  ~DisassemblyWidget() override;

 public:
  void UpdateDisassembly();

 protected:
  // Widget:
  void Paint() override;

 private:
  NESRuntime::Data* runtime_data_ = nullptr;
  kiwi::base::RepeatingClosure on_toggle_pause_;
  kiwi::base::RepeatingCallback<bool()> is_pause_;
  int current_selected_breakpoint_ = -1;
  std::string disassembly_string_;
  char breakpoint_address_input[5];
  std::string item_getter_buffer_;
};

#endif  // UI_WIDGETS_DISASSEMBLY_WIDGET_H_