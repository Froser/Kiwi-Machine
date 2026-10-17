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

#ifndef MODELS_NES_FRAME_H_
#define MODELS_NES_FRAME_H_

#include <SDL.h>
#include <kiwi_nes.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <set>

#include "build/kiwi_defines.h"
#include "models/frame_base.h"
#include "utility/timer.h"

class WindowBase;
#if BUILDFLAG(ENABLE_HD_TEXTURE)
class TextureRenderer;
#endif

class NESFrameObserver {
 public:
  virtual void OnShouldRender(int since_last_frame_ms) {}
};

// A NESFrame represents a frame gets from RenderDevice.
class NESFrame : public kiwi::nes::IODevices::RenderDevice, public FrameBase {
 private:
  ~NESFrame() override;

 public:
  explicit NESFrame(WindowBase* window);
  void SetEmulator(kiwi::nes::Emulator* emulator);

  void AddObserver(NESFrameObserver* observer);
  void RemoveObserver(NESFrameObserver* observer);

  // RenderDevice:
  void Render(const kiwi::nes::PPUFrameData& frame) override;
  bool NeedRender() override;
#if BUILDFLAG(ENABLE_HD_TEXTURE)
  void SetTextureRenderer(std::unique_ptr<TextureRenderer> texture_renderer);
  void SetHDTextureRenderingEnabled(bool enabled);
  bool IsHDTextureRenderingEnabled() const;
  bool HasHDTextureRenderer() const;
#endif

  const Buffer& GetLastFrame();
  const Buffer& GetCurrentFrame();

 private:
#if BUILDFLAG(ENABLE_HD_TEXTURE)
  bool RenderTextureFrame(const kiwi::nes::PPUFrameData& frame);
#endif
  void NotifyObservers();
  void UpdateTexture(int width, int height, const kiwi::nes::Colors& buffer);

  scoped_refptr<kiwi::nes::Emulator> emulator_;

#if BUILDFLAG(ENABLE_HD_TEXTURE)
  std::unique_ptr<TextureRenderer> texture_renderer_;
  std::atomic_bool hd_texture_rendering_enabled_ = false;
#endif
#if BUILDFLAG(ENABLE_HD_TEXTURE)
  Buffer last_rendered_frame_;
#endif
  Timer frame_elapsed_counter_;
  std::set<NESFrameObserver*> observers_;
};

#endif  // MODELS_NES_FRAME_H_
