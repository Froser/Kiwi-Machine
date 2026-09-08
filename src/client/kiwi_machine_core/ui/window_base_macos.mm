// Copyright (C) 2026 Yisi Yu
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

#include "ui/window_base_macos.h"

#import <AppKit/AppKit.h>

#include <cstdint>

namespace {

constexpr Uint32 kInvalidEventType = static_cast<Uint32>(-1);

id g_mouse_wheel_phase_monitor = nil;
Uint32 g_mouse_wheel_phase_event_type = kInvalidEventType;

void PushMouseWheelPhaseEvent(MouseWheelPhase phase) {
  SDL_Window* window = SDL_GetMouseFocus();
  if (!window)
    window = SDL_GetKeyboardFocus();
  if (!window)
    return;

  int x = 0;
  int y = 0;
  SDL_GetMouseState(&x, &y);

  SDL_Event event = {};
  event.type = g_mouse_wheel_phase_event_type;
  event.user.windowID = SDL_GetWindowID(window);
  event.user.code = static_cast<Sint32>(phase);
  event.user.data1 = reinterpret_cast<void*>(static_cast<intptr_t>(x));
  event.user.data2 = reinterpret_cast<void*>(static_cast<intptr_t>(y));
  SDL_PushEvent(&event);
}

}  // namespace

void InitializeMacMouseWheelPhaseMonitor() {
  if (g_mouse_wheel_phase_monitor)
    return;

  g_mouse_wheel_phase_event_type = SDL_RegisterEvents(1);
  SDL_assert(g_mouse_wheel_phase_event_type != kInvalidEventType);
  if (g_mouse_wheel_phase_event_type == kInvalidEventType)
    return;

  g_mouse_wheel_phase_monitor = [NSEvent
      addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel
                                   handler:^NSEvent*(NSEvent* event) {
                                     const NSEventPhase momentum_phase =
                                         event.momentumPhase;
                                     const NSEventPhase phase = event.phase;
                                     if (momentum_phase != NSEventPhaseNone) {
                                       PushMouseWheelPhaseEvent(
                                           MouseWheelPhase::kNativeMomentum);
                                     } else if (phase & (NSEventPhaseMayBegin |
                                                         NSEventPhaseBegan)) {
                                       PushMouseWheelPhaseEvent(
                                           MouseWheelPhase::kBegin);
                                     } else if (phase & NSEventPhaseEnded) {
                                       PushMouseWheelPhaseEvent(
                                           MouseWheelPhase::kEnd);
                                     } else if (phase & NSEventPhaseCancelled) {
                                       PushMouseWheelPhaseEvent(
                                           MouseWheelPhase::kCancel);
                                     }
                                     return event;
                                   }];
}

void UninitializeMacMouseWheelPhaseMonitor() {
  if (g_mouse_wheel_phase_monitor) {
    [NSEvent removeMonitor:g_mouse_wheel_phase_monitor];
    g_mouse_wheel_phase_monitor = nil;
  }
  g_mouse_wheel_phase_event_type = kInvalidEventType;
}

bool DecodeMacMouseWheelPhaseEvent(const SDL_Event* event,
                                   MouseWheelPhaseEvent* phase_event) {
  if (g_mouse_wheel_phase_event_type == kInvalidEventType ||
      event->type != g_mouse_wheel_phase_event_type) {
    return false;
  }

  phase_event->phase = static_cast<MouseWheelPhase>(event->user.code);
  phase_event->timestamp = event->user.timestamp;
  phase_event->window_id = event->user.windowID;
  phase_event->x =
      static_cast<int>(reinterpret_cast<intptr_t>(event->user.data1));
  phase_event->y =
      static_cast<int>(reinterpret_cast<intptr_t>(event->user.data2));
  return true;
}
