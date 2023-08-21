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

#ifndef UI_WIDGETS_CANVAS_H_
#define UI_WIDGETS_CANVAS_H_

#include <set>

#include "models/nes_frame.h"
#include "ui/widgets/widget.h"

// A canvas is a widget to render NES frame.
class WindowBase;
class CanvasObserver;
class Canvas : public Widget, public NESFrameObserver {
 public:
  enum Size {
    kNESFrameDefaultWidth = 256,
    kNESFrameDefaultHeight = 240,
  };

  explicit Canvas(WindowBase* window_base, NESRuntimeID runtime_id);
  ~Canvas() override;

  void Clear();

  void set_frame_scale(float scale) {
    frame_scale_ = scale;
    UpdateBounds();
  }

  void set_in_menu_trigger_callback(kiwi::base::RepeatingClosure callback) {
    on_menu_trigger_ = callback;
  }

  scoped_refptr<NESFrame> frame() { return frame_; }
  float frame_scale() { return frame_scale_; }
  kiwi::nes::IODevices::RenderDevice* render_device() { return frame_.get(); }

  void AddObserver(CanvasObserver* observer);
  void RemoveObserver(CanvasObserver* observer);

 protected:
  // Widget:
  void Paint() override;
  bool IsWindowless() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event) override;

  // NESFrameObserver:
  void OnShouldRender(int since_last_frame_ms) override;

 private:
  // Update bounds with frame scale.
  void UpdateBounds();
  void InvokeInGameMenu();

 private:
  float frame_scale_ = 1.f;
  bool nes_frame_is_ready_ = false;
  scoped_refptr<NESFrame> frame_;
  SDL_Texture* screen_texture_ = nullptr;
  kiwi::base::RepeatingClosure on_menu_trigger_;
  std::set<CanvasObserver*> observers_;
};

#endif  // UI_WIDGETS_CANVAS_H_