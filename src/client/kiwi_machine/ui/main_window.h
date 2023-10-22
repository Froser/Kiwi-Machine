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

#ifndef UI_MAIN_WINDOW_H_
#define UI_MAIN_WINDOW_H_

#include <SDL.h>

#include "models/nes_audio.h"
#include "models/nes_frame.h"
#include "models/nes_runtime.h"
#include "ui/widgets/canvas_observer.h"
#include "ui/widgets/in_game_menu.h"
#include "ui/widgets/menu_bar.h"
#include "ui/window_base.h"

class Canvas;
class InGameMenu;
class KiwiBgWidget;
class LoadingWidget;
class ExportWidget;
class StackWidget;
class MemoryWidget;
class DisassemblyWidget;
class GroupWidget;
class KiwiItemsWidget;

namespace preset_roms {
class PresetROM;
}

class MainWindow : public WindowBase,
                   public kiwi::nes::IODevices::InputDevice,
                   public CanvasObserver {
 public:
  explicit MainWindow(const std::string& title,
                      NESRuntimeID runtime_id,
                      scoped_refptr<NESConfig> config,
                      bool has_demo_widget);
  ~MainWindow() override;

  float window_scale() { return config_->data().window_scale; }
  bool is_fullscreen() { return config_->data().is_fullscreen; }
  SDL_Rect Scaled(const SDL_Rect& rect);
  ImVec2 Scaled(const ImVec2& vec2);
  int Scaled(int i);

  // Export ROMs
  void ExportDone();
  void ExportSucceeded(const std::string& rom_name);
  void ExportFailed(const std::string& rom_name);
  void Exporting(const std::string& rom_name);

 protected:
  // InputDevice:
  bool IsKeyDown(int controller_id,
                 kiwi::nes::ControllerButton button) override;

  // WindowBase:
  SDL_Rect GetClientBounds() override;
  void HandleKeyEvents(SDL_KeyboardEvent* event) override;
  void OnControllerDeviceAdded(SDL_ControllerDeviceEvent* event) override;
  void OnControllerDeviceRemoved(SDL_ControllerDeviceEvent* event) override;
  void HandleResizedEvent() override;
  void HandlePostEvent() override;

  // CanvasObserver:
  void OnAboutToRenderFrame(Canvas* canvas,
                            scoped_refptr<NESFrame> frame) override;

 private:
  void Initialize(NESRuntimeID runtime_id);
  void InitializeAudio();
  void InitializeUI();
  void InitializeIODevices();
  void StartAutoSave();
  void StopAutoSave();
  void ResetAudio();
  std::vector<MenuBar::Menu> GetMenuModel();
  void SetLoading(bool is_loading);
  void ShowMainMenu(bool show);
  void OnScaleChanged();
  void UpdateGameControllerMapping();

  // Menu callbacks:
  void OnRomLoaded(const std::string& name);
  void OnQuit();
  void OnResetROM();
  void OnBackToMainMenu();
  void OnSaveState(int which_state);
  void OnLoadState(int which_state);
  void OnLoadAutoSavedState(int timestamp);
  void OnStateSaved(bool succeed);
  void OnStateLoaded(const NESRuntime::Data::StateResult& state_result);
  bool CanSaveOrLoadState();
  void OnTogglePause();
  void OnPause();
  void OnResume();
  bool IsPause();
  void OnLoadPresetROM(const preset_roms::PresetROM& rom);
  void OnLoadDebugROM(kiwi::base::FilePath nes_path);
  void OnToggleAudioEnabled();
  void OnSetAudioVolume(float volume);
  bool IsAudioEnabled();
  void OnToggleAudioChannelMasks(kiwi::nes::AudioChannel which_mask);
  bool IsAudioChannelOn(kiwi::nes::AudioChannel which_mask);
  void OnSetScreenScale(float scale);
  void OnSetFullscreen();
  void OnUnsetFullscreen(float scale);
  bool ScreenScaleIs(float scale);
  void OnTogglePaletteWidget();
  bool IsPaletteWidgetShown();
  void OnTogglePatternWidget();
  bool IsPatternWidgetShown();
  void OnFrameRateWidget();
  bool IsFrameRateWidgetShown();
  void OnExportGameROMs();
  void OnDebugMemory();
  void OnDebugDisassembly();
  void OnDebugNametable();
  void OnInGameMenuTrigger();
  void OnInGameMenuItemTrigger(InGameMenu::MenuItem item, int param);
  void OnInGameSettingsItemTrigger(InGameMenu::SettingsItem item, bool is_left);

  void SaveConfig();

 private:
  std::set<int> pressing_keys_;
  bool has_demo_widget_ = false;
  // Canvas is owned by this window.
  Canvas* canvas_ = nullptr;
  InGameMenu* in_game_menu_ = nullptr;
  Widget* menu_bar_ = nullptr;
  Widget* palette_widget_ = nullptr;
  Widget* pattern_widget_ = nullptr;
  Widget* frame_rate_widget_ = nullptr;
  KiwiBgWidget* bg_widget_ = nullptr;
  GroupWidget* main_group_widget_ = nullptr;
  KiwiItemsWidget* main_items_widget_ = nullptr;
  LoadingWidget* loading_widget_ = nullptr;
  ExportWidget* export_widget_ = nullptr;
  StackWidget* stack_widget_ = nullptr;
  MemoryWidget* memory_widget_ = nullptr;
  DisassemblyWidget* disassembly_widget_ = nullptr;
  Widget* nametable_widget_ = nullptr;

  NESRuntimeID runtime_id_ = NESRuntimeID();
  NESRuntime::Data* runtime_data_ = nullptr;
  std::unique_ptr<NESAudio> audio_;
  scoped_refptr<NESConfig> config_;
};

#endif  // UI_MAIN_WINDOW_H_