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

#include "ui/widgets/canvas.h"
#include "ui/widgets/canvas_observer.h"
#include "ui/window_base.h"

#include <imgui.h>

Canvas::Canvas(WindowBase* window_base, NESRuntimeID runtime_id)
    : Widget(window_base) {
  frame_ = kiwi::base::MakeRefCounted<NESFrame>(runtime_id);
  frame_->AddObserver(this);
  set_bounds(SDL_Rect{0, 0, kNESFrameDefaultWidth, kNESFrameDefaultHeight});
}

Canvas::~Canvas() {
  if (screen_texture_)
    SDL_DestroyTexture(screen_texture_);
}

void Canvas::Clear() {
  nes_frame_is_ready_ = false;
  SDL_RenderClear(window()->renderer());
}

void Canvas::AddObserver(CanvasObserver* observer) {
  observers_.insert(observer);
}

void Canvas::RemoveObserver(CanvasObserver* observer) {
  observers_.erase(observer);
}

void Canvas::Paint() {
  if (nes_frame_is_ready_) {
    // Create a texture surface here, if not exists.
    if (!screen_texture_) {
      SDL_assert(frame_->width() > 0 && frame_->height() > 0);
      screen_texture_ = SDL_CreateTexture(
          window()->renderer(), SDL_PIXELFORMAT_ARGB8888,
          SDL_TEXTUREACCESS_STREAMING, frame_->width(), frame_->height());
    }

    // Notify all observers
    for (CanvasObserver* observer : observers_) {
      observer->OnAboutToRenderFrame(this, frame_.get());
    }

    // Update texture, and render
    int result =
        SDL_UpdateTexture(screen_texture_, nullptr, frame_->buffer().data(),
                          frame_->width() * sizeof(frame_->buffer()[0]));
    SDL_assert(result == 0);
    SDL_Rect src_rect = {0, 0, frame_->width(), frame_->height()};
    SDL_Rect dest_rect = bounds();
    SDL_RenderCopy(window()->renderer(), screen_texture_, &src_rect,
                   &dest_rect);
  }
}

bool Canvas::IsWindowless() {
  return true;
}

bool Canvas::OnKeyPressed(SDL_KeyboardEvent* event) {
  if (event->keysym.sym == SDLK_ESCAPE) {
    InvokeInGameMenu();
    return true;
  }
  return false;
}

bool Canvas::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  SDL_GameController* controller =
      SDL_GameControllerFromInstanceID(event->which);
  if (SDL_GameControllerGetButton(controller,
                                  SDL_CONTROLLER_BUTTON_LEFTSHOULDER) &&
      SDL_GameControllerGetButton(controller,
                                  SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
    InvokeInGameMenu();
    return true;
  }
  return false;
}

void Canvas::OnShouldRender(int since_last_frame_ms) {
  nes_frame_is_ready_ = true;
}

void Canvas::UpdateBounds() {
  SDL_Rect r = GetLocalBounds();
  set_bounds(SDL_Rect{r.x, r.y, static_cast<int>(r.w * frame_scale()),
                      static_cast<int>(r.h * frame_scale())});
}

void Canvas::InvokeInGameMenu() {
  if (on_menu_trigger_)
    on_menu_trigger_.Run();
}