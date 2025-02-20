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

#ifndef NES_FRAME_H_
#define NES_FRAME_H_

#include <SDL.h>
#include <kiwi_nes.h>
#include <chrono>
#include <set>

#include "models/nes_runtime.h"
#include "utility/timer.h"

class WindowBase;

class NESFrameObserver {
 public:
  virtual void OnShouldRender(int since_last_frame_ms) {}
};

// A NESFrame represents a frame gets from RenderDevice.
class NESFrame : public kiwi::base::RefCounted<NESFrame>,
                 public kiwi::nes::IODevices::RenderDevice {
  friend class kiwi::base::RefCounted<NESFrame>;

 private:
  ~NESFrame() override;

 public:
  explicit NESFrame(WindowBase* window, NESRuntimeID runtime_id);

  void AddObserver(NESFrameObserver* observer);
  void RemoveObserver(NESFrameObserver* observer);

  // RenderDevice:
  void Render(int width, int height, const kiwi::nes::Colors& buffer) override;
  bool NeedRender() override;

  int width() { return render_width_; }
  int height() { return render_height_; }
  SDL_Texture* texture() { return screen_texture_; }
  const Buffer& GetLastFrame();
  const Buffer& GetCurrentFrame();

 private:
  WindowBase* window_ = nullptr;
  NESRuntime::Data* runtime_data_ = nullptr;

  SDL_Texture* screen_texture_ = nullptr;
  int render_width_ = 0;   // UI thread access only
  int render_height_ = 0;  // UI thread access only
  Timer frame_elapsed_counter_;
  std::set<NESFrameObserver*> observers_;
};

#endif  // NES_FRAME_H_