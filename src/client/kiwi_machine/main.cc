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

#include <SDL.h>

#include <kiwi_nes.h>

#include "debug/debug_port.h"
#include "models/nes_runtime.h"
#include "ui/application.h"
#include "ui/main_window.h"
#include "ui/widgets/demo_widget.h"

DEFINE_bool(demo_window, false, "Show ImGui demo window.");

#if defined(_WIN32)
#include <windows.h>
int WINAPI wWinMain(HINSTANCE hInstance,
                    HINSTANCE hPrevInstance,
                    PWSTR pCmdLine,
                    int nCmdShow) {
  Application application;
#else
int main(int argc, char** argv) {
  Application application(argc, argv);
#endif

  // Make a kiwi-nes runtime.
  NESRuntimeID runtime_id = NESRuntime::GetInstance()->CreateData();
  NESRuntime::Data* runtime_data =
      NESRuntime::GetInstance()->GetDataById(runtime_id);
  runtime_data->emulator = kiwi::nes::CreateEmulator();
  runtime_data->debug_port =
      std::make_unique<DebugPort>(runtime_data->emulator.get());
  runtime_data->emulator->SetDebugPort(runtime_data->debug_port.get());

  // Set key mappings.
  NESRuntime::Data::ControllerMapping key_mapping1 = {
      SDLK_j, SDLK_k, SDLK_l, SDLK_RETURN, SDLK_w, SDLK_s, SDLK_a, SDLK_d};
  NESRuntime::Data::ControllerMapping key_mapping2 = {
      SDLK_DELETE, SDLK_END,  SDLK_PAGEDOWN, SDLK_HOME,
      SDLK_UP,     SDLK_DOWN, SDLK_LEFT,     SDLK_RIGHT};
  runtime_data->keyboard_mappings[0] = key_mapping1;
  runtime_data->keyboard_mappings[1] = key_mapping2;
  runtime_data->emulator->PowerOn();

  MainWindow main_window("Kiwi Machine", runtime_id);

  if (FLAGS_demo_window) {
    main_window.AddWidget(std::make_unique<DemoWidget>(&main_window));
  }

  application.Run();
  return 0;
}
