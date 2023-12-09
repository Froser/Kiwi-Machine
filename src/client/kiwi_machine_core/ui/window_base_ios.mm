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

#include "ui/window_base.h"

#import <UIKit/UIKit.h>

#include "third_party/SDL2/src/video/SDL_sysvideo.h"
#include "third_party/SDL2/src/video/uikit/SDL_uikitwindow.h"

SDL_Rect WindowBase::GetSafeAreaInsets() {
  SDL_WindowData* data = (SDL_WindowData*)window_->driverdata;
  UIEdgeInsets insets = data.uiwindow.safeAreaInsets;
  return SDL_Rect{static_cast<int>(insets.left), static_cast<int>(insets.top),
                  static_cast<int>(insets.right),
                  static_cast<int>(insets.bottom)};
}
