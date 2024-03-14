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

#include "utility/emscripten/bridge_api.h"

#include <emscripten.h>

#include "ui/main_window.h"

namespace {
class BridgeMainWindowObserver : public MainWindow::Observer {
 private:
  BridgeMainWindowObserver();
  ~BridgeMainWindowObserver() override;

  void OnVolumeChanged(float new_value) override;

 public:
  static BridgeMainWindowObserver* Setup();
};

BridgeMainWindowObserver::BridgeMainWindowObserver() {
  MainWindow::GetInstance()->AddObserver(this);
}

BridgeMainWindowObserver::~BridgeMainWindowObserver() {
  MainWindow::GetInstance()->RemoveObserver(this);
}

BridgeMainWindowObserver* BridgeMainWindowObserver::Setup() {
  static BridgeMainWindowObserver g_instance;
  return &g_instance;
}

void BridgeMainWindowObserver::OnVolumeChanged(float new_value) {
  EM_ASM({window.KiwiMachineCallback.onVolumeChanged({volume : $0})},
         new_value);
}

}  // namespace

extern "C" {
EMSCRIPTEN_KEEPALIVE
void LoadROMFromTempPath(const char* filename) {
  MainWindow* main_window = MainWindow::GetInstance();
  main_window->LoadROM_WASM(kiwi::base::FilePath::FromUTF8Unsafe(filename));
}

EMSCRIPTEN_KEEPALIVE
void SetupCallbacks() {
  BridgeMainWindowObserver::Setup();
}

EMSCRIPTEN_KEEPALIVE
void SetVolume(float volume) {
  MainWindow* main_window = MainWindow::GetInstance();
  main_window->SetVolume_WASM(volume);
}

EMSCRIPTEN_KEEPALIVE
void CallMenu() {
  MainWindow* main_window = MainWindow::GetInstance();
  main_window->CallMenu_WASM();
}

};
