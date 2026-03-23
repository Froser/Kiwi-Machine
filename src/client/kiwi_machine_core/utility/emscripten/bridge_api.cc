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

#include "nes/controller.h"
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

EMSCRIPTEN_KEEPALIVE
float GetFPS() {
  MainWindow* main_window = MainWindow::GetInstance();
  return main_window->GetFPS_WASM();
}

EMSCRIPTEN_KEEPALIVE
void JoystickButtonDown(int button) {
  kiwi::nes::ControllerButton b =
      static_cast<kiwi::nes::ControllerButton>(button);
  MainWindow* main_window = MainWindow::GetInstance();
  main_window->JoystickButtonDown_WASM(b);
}

EMSCRIPTEN_KEEPALIVE
void JoystickButtonUp(int button) {
  kiwi::nes::ControllerButton b =
      static_cast<kiwi::nes::ControllerButton>(button);
  MainWindow* main_window = MainWindow::GetInstance();
  main_window->JoystickButtonUp_WASM(b);
}

EMSCRIPTEN_KEEPALIVE
void SaveState(int slot) {
  MainWindow* main_window = MainWindow::GetInstance();
  main_window->SaveState_WASM(slot);
}

EMSCRIPTEN_KEEPALIVE
void LoadState(int slot) {
  MainWindow* main_window = MainWindow::GetInstance();
  main_window->LoadState_WASM(slot);
}

EMSCRIPTEN_KEEPALIVE
int GetSaveStatesCount() {
  MainWindow* main_window = MainWindow::GetInstance();
  return main_window->GetSaveStatesCount_WASM();
}

EMSCRIPTEN_KEEPALIVE
bool HasSaveState(int slot) {
  MainWindow* main_window = MainWindow::GetInstance();
  return main_window->HasSaveState_WASM(slot);
}

EMSCRIPTEN_KEEPALIVE
const char* GetSaveStateThumbnail(int slot) {
  MainWindow* main_window = MainWindow::GetInstance();
  std::string thumbnail = main_window->GetSaveStateThumbnail_WASM(slot);
  static std::string static_thumbnail;
  static_thumbnail = thumbnail;
  return static_thumbnail.c_str();
}

EMSCRIPTEN_KEEPALIVE
void SyncFilesystem() {
  EM_ASM({
    if (window.syncFilesystemToDB) {
      window.syncFilesystemToDB();
    }
  });
}

};
