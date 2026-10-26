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

#include "models/nes/frame.h"
#include "build/build_config.h"

#include <cstdint>
#include <limits>
#include <utility>

#include "build/kiwi_defines.h"
#include "emulator/nes/ppu_observer.h"
#if BUILDFLAG(ENABLE_HD_TEXTURE)
#include "utility/texture_renderer.h"
#endif

namespace {
#if BUILDFLAG(ENABLE_HD_TEXTURE) && BUILDFLAG(IS_ANDROID)
constexpr uint32_t kMaximumHDTextureOutputScale = 2;
#endif
}  // namespace

NESFrame::NESFrame(WindowBase* window)
    : FrameBase(window, SDL_PIXELFORMAT_ARGB8888) {}

NESFrame::~NESFrame() = default;

void NESFrame::SetEmulator(kiwi::nes::Emulator* emulator) {
  emulator_ = emulator;
}

#if BUILDFLAG(ENABLE_HD_TEXTURE)
void NESFrame::SetTextureRenderer(
    std::unique_ptr<TextureRenderer> texture_renderer) {
#if BUILDFLAG(IS_ANDROID)
  if (texture_renderer) {
    texture_renderer->SetMaximumOutputScale(kMaximumHDTextureOutputScale);
  }
#endif
  texture_renderer_ = std::move(texture_renderer);
  last_rendered_frame_.clear();
  hd_texture_rendering_enabled_.store(texture_renderer_ != nullptr,
                                      std::memory_order_release);
}

void NESFrame::SetHDTextureRenderingEnabled(bool enabled) {
  hd_texture_rendering_enabled_.store(enabled && texture_renderer_ != nullptr,
                                      std::memory_order_release);
}

bool NESFrame::IsHDTextureRenderingEnabled() const {
  return hd_texture_rendering_enabled_.load(std::memory_order_acquire);
}

bool NESFrame::HasHDTextureRenderer() const {
  return texture_renderer_ != nullptr;
}
#endif

void NESFrame::AddObserver(NESFrameObserver* observer) {
  observers_.insert(observer);
}

void NESFrame::RemoveObserver(NESFrameObserver* observer) {
  observers_.erase(observer);
}

void NESFrame::Render(const kiwi::nes::PPUFrameData& frame) {
  if (!frame.native_pixels) {
    return;
  }

#if BUILDFLAG(ENABLE_HD_TEXTURE)
  if (frame.type == kiwi::nes::PPUFrameData::Type::kTextureMetadata &&
      texture_renderer_ && IsHDTextureRenderingEnabled() &&
      RenderTextureFrame(frame)) {
    return;
  }
#endif
  UpdateTexture(frame.width, frame.height, *frame.native_pixels);
}

#if BUILDFLAG(ENABLE_HD_TEXTURE)
bool NESFrame::RenderTextureFrame(const kiwi::nes::PPUFrameData& frame) {
  const uint64_t output_width =
      static_cast<uint64_t>(frame.width) * texture_renderer_->GetScale();
  const uint64_t output_height =
      static_cast<uint64_t>(frame.height) * texture_renderer_->GetScale();
  if (output_width > std::numeric_limits<int>::max() ||
      output_height > std::numeric_limits<int>::max() ||
      !EnsureTexture(static_cast<int>(output_width),
                     static_cast<int>(output_height))) {
    return false;
  }

  void* pixels = nullptr;
  int pitch = 0;
  if (SDL_LockTexture(texture(), nullptr, &pixels, &pitch) != 0) {
    return false;
  }

  bool rendered = false;
  const size_t minimum_pitch =
      static_cast<size_t>(width()) * sizeof(kiwi::nes::Color);
  if (pixels && pitch > 0 && static_cast<size_t>(pitch) >= minimum_pitch &&
      pitch % static_cast<int>(sizeof(kiwi::nes::Color)) == 0) {
    const TextureRenderTarget target = {
        static_cast<kiwi::nes::Color*>(pixels),
        static_cast<size_t>(pitch) / sizeof(kiwi::nes::Color),
        width(),
        height(),
    };
    rendered = texture_renderer_->RenderFrame(frame, target);
    if (rendered) {
      const uint32_t scale = texture_renderer_->GetScale();
      const uint32_t sample_offset = scale / 2;
      last_rendered_frame_.resize(static_cast<size_t>(frame.width) *
                                  frame.height);
      for (int y = 0; y < frame.height; ++y) {
        const kiwi::nes::Color* source =
            target.pixels +
            static_cast<size_t>(y * scale + sample_offset) * target.stride +
            sample_offset;
        kiwi::nes::Color* destination =
            last_rendered_frame_.data() + static_cast<size_t>(y) * frame.width;
        for (int x = 0; x < frame.width; ++x) {
          destination[x] = source[static_cast<size_t>(x) * scale];
        }
      }
    }
  }
  SDL_UnlockTexture(texture());

  if (rendered) {
    NotifyObservers();
  }
  return rendered;
}
#endif

void NESFrame::UpdateTexture(int width,
                             int height,
                             const kiwi::nes::Colors& buffer) {
  if (!FrameBase::UpdateTexture(width, height, buffer)) {
    SDL_assert(false);
    return;
  }
  NotifyObservers();
}

void NESFrame::NotifyObservers() {
  if (!observers_.empty()) {
    int elapsed_ms = frame_elapsed_counter_.ElapsedInMillisecondsAndReset();
    for (NESFrameObserver* observer : observers_) {
      observer->OnShouldRender(elapsed_ms);
    }
  }
}

bool NESFrame::NeedRender() {
  return true;
}

const NESFrame::Buffer& NESFrame::GetLastFrame() {
#if BUILDFLAG(ENABLE_HD_TEXTURE)
  if (IsHDTextureRenderingEnabled() && !last_rendered_frame_.empty()) {
    return last_rendered_frame_;
  }
#endif
  SDL_assert(emulator_);
  return emulator_->GetLastFrame();
}

const NESFrame::Buffer& NESFrame::GetCurrentFrame() {
  SDL_assert(emulator_);
  return emulator_->GetCurrentFrame();
}
