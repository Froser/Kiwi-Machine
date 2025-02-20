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

#include <optional>
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
  int GetZapperState();

  void set_frame_scale(float scale) { frame_scale_ = scale; }

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
  bool OnMousePressed(SDL_MouseButtonEvent* event) override;
  bool OnMouseReleased(SDL_MouseButtonEvent* event) override;
  bool OnTouchFingerDown(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerUp(SDL_TouchFingerEvent* event) override;

  // NESFrameObserver:
  void OnShouldRender(int since_last_frame_ms) override;

 private:
  void InvokeInGameMenu();

  struct ZapperDetails {
    int original_x;
    int original_y;
  };
  // x and y are relative to the window
  ZapperDetails CreateZapperDetailsByMouseOrFingerPosition(int x, int y);
  enum class Input {
    kMouse,
    kFinger,
  };
  bool ZapperTest(Input input);

 private:
  float frame_scale_ = 1.f;
  bool mouse_or_finger_down_ = false;
  std::optional<std::pair<int, int>> touch_point_;
  scoped_refptr<NESFrame> frame_;
  kiwi::base::RepeatingClosure on_menu_trigger_;
  std::set<CanvasObserver*> observers_;
};

#endif  // UI_WIDGETS_CANVAS_H_