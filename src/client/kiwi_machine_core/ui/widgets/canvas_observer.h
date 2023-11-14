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

#ifndef UI_WIDGETS_CANVAS_OBSERVER_H_
#define UI_WIDGETS_CANVAS_OBSERVER_H_

#include <kiwi_nes.h>

#include "models/nes_frame.h"

class Canvas;
class NESFrame;
class CanvasObserver {
 public:
  CanvasObserver();
  ~CanvasObserver();

  virtual void OnAboutToRenderFrame(Canvas* canvas,
                                    scoped_refptr<NESFrame> frame);
};

#endif  // UI_WIDGETS_CANVAS_OBSERVER_H_