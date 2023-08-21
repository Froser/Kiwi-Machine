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

#ifndef UI_WIDGETS_NAMETABLE_WIDGET_H_
#define UI_WIDGETS_NAMETABLE_WIDGET_H_

#include <kiwi_nes.h>

#include "models/nes_runtime.h"
#include "ui/widgets/widget.h"

struct SDL_Texture;
class WindowBase;
class NametableWidget : public Widget,
                        public kiwi::nes::IODevices::RenderDevice {
 public:
  explicit NametableWidget(WindowBase* window_base, NESRuntimeID runtime_id);
  ~NametableWidget() override;

 protected:
  // Widget:
  void Paint() override;

  // kiwi::nes::IODevices::RenderDevice:
  void Render(int width, int height, const Buffer& buffer) override;
  bool NeedRender() override;

 private:
  NESRuntime::Data* runtime_data_ = nullptr;
  SDL_Texture* screen_texture_ = nullptr;
  Buffer screen_buffer_;
};

#endif  // UI_WIDGETS_NAMETABLE_WIDGET_H_