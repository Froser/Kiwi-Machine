// Copyright (C) 2024 Yisi Yu
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

#include "ui/widgets/flex_items_widget.h"

#include <SDL.h>

#include "ui/window_base.h"

bool FlexItemsWidget::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  SDL_Rect window_rect = window()->GetWindowBounds();
  return HandleMouseOrFingerEvents(
      MouseOrFingerEventType::kFingerDown, MouseButton::kLeftButton,
      event->x * window_rect.w, event->y * window_rect.h);
}

bool FlexItemsWidget::OnTouchFingerMove(SDL_TouchFingerEvent* event) {
  // return HandleMouseOrFingerEvents(MouseOrFingerEventType::kFingerMove,
  //                                  MouseButton::kLeftButton,
  //                                  event->x * bounds().w,
  //                                  event->y * bounds().h);
  return false;
}

bool FlexItemsWidget::OnTouchFingerUp(SDL_TouchFingerEvent* event) {
  SDL_Rect window_rect = window()->GetWindowBounds();
  return HandleMouseOrFingerEvents(
      MouseOrFingerEventType::kFingerUp, MouseButton::kLeftButton,
      event->x * window_rect.w, event->y * window_rect.h);
}