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

#include "kiwi_main.h"

#include <SDL.h>
#include <kiwi_nes.h>

#include "debug/debug_port.h"
#include "ui/application.h"
#include "ui/main_window.h"

#if defined(__IPHONEOS__)
#include <SDL_main.h>
#endif

DEFINE_bool(demo_window, false, "Show ImGui demo window.");

#if defined(__IPHONEOS__)
int KiwiMain(int argc, char** argv) {
  int KiwiMainReal(int argc, char** argv);
  return SDL_UIKitRunApp(argc, argv, KiwiMainReal);
}
#define KiwiMain KiwiMainReal
#endif

#if defined(_WIN32)
#include <windows.h>
int WINAPI KiwiMain(HINSTANCE hInstance,
                    HINSTANCE hPrevInstance,
                    PWSTR pCmdLine,
                    int nCmdShow) {
  Application application;
#else
int KiwiMain(int argc, char** argv) {
  Application application(argc, argv);
#endif

  MainWindow main_window("Kiwi Machine", application.runtime_id(),
                         application.config(), FLAGS_demo_window);

  application.Run();
  return 0;
}
