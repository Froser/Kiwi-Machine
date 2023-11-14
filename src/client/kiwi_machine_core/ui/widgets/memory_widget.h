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

#ifndef UI_WIDGETS_MEMORY_WIDGET_H_
#define UI_WIDGETS_MEMORY_WIDGET_H_

#include <string>

#include "models/nes_runtime.h"
#include "ui/widgets/widget.h"

class StackWidget;
class MemoryWidget : public Widget {
 public:
  explicit MemoryWidget(WindowBase* window_base,
                        NESRuntimeID runtime_id,
                        kiwi::base::RepeatingClosure on_toggle_pause,
                        kiwi::base::RepeatingCallback<bool()> is_pause);
  ~MemoryWidget() override;

 public:
  void UpdateMemory();

 private:
  enum class MemoryType {
    kCPU,
    kPPU,
    kOAM,
  };

  void CreateTab(MemoryType type,
                 const std::string& tab_name,
                 const std::string& which_address,
                 const std::string& which_buffer);
  void ChangeAddress(MemoryType type, int delta);
  kiwi::nes::Address FormatAddress(MemoryType type,
                                   std::string& address_string);
  void AdjustAddressWithDelta(MemoryType type, int delta, std::string& which);
  kiwi::nes::Address ClampAddress(MemoryType type, int64_t address);

 protected:
  // Widget:
  void Paint() override;

 private:
  NESRuntime::Data* runtime_data_ = nullptr;
  kiwi::base::RepeatingClosure on_toggle_pause_;
  kiwi::base::RepeatingCallback<bool()> is_pause_;
  // Many test ROMs write result to address $6000, so sets it as default
  // address.
  std::string cpu_address_ = "6000";
  std::string cpu_memory_;
  std::string ppu_address_ = "0";
  std::string ppu_memory_;
  std::string oam_address_ = "0";
  std::string oam_memory_;
};

#endif  // UI_WIDGETS_MEMORY_WIDGET_H_