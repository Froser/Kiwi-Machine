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

#include "build/kiwi_defines.h"
#include "models/nes_audio.h"
#include "models/nes_frame.h"
#include "models/nes_runtime.h"
#include "ui/widgets/canvas_observer.h"
#include "ui/widgets/in_game_menu.h"
#include "ui/widgets/menu_bar.h"
#include "ui/widgets/side_menu.h"
#include "ui/window_base.h"
#include "utility/timer.h"

class Canvas;
class InGameMenu;
class KiwiBgWidget;
class LoadingWidget;
class ExportWidget;
class StackWidget;
class MemoryWidget;
class DisassemblyWidget;
class GroupWidget;
class FlexItemsWidget;
class CardWidget;
class Splash;

namespace preset_roms {
struct PresetROM;
}

class MainWindow : public WindowBase,
                   public kiwi::nes::IODevices::InputDevice,
                   public CanvasObserver {
 public:
  enum class VirtualTouchButton {
    kStart,
    kSelect,
    kJoystick,
    kA,
    kB,
    kAB,
    kPause,
  };

  enum class MainFocus {
    kSideMenu,
    kContents,
  };

  class Observer {
   public:
    Observer();
    virtual ~Observer();

   public:
    virtual void OnVolumeChanged(float new_value);
  };

 public:
  explicit MainWindow(const std::string& title,
                      NESRuntimeID runtime_id,
                      scoped_refptr<NESConfig> config);
  ~MainWindow() override;

  // InitializeAsync() must be called before rendering.
  void InitializeAsync(kiwi::base::OnceClosure callback);

#if KIWI_WASM
  // WASM environment uses this instance to load roms.
  static MainWindow* GetInstance();
  void LoadROM_WASM(kiwi::base::FilePath rom_path);
  void SetVolume_WASM(float volume);
  void CallMenu_WASM();
#endif

  float window_scale() { return config_->data().window_scale; }
  bool is_fullscreen() { return config_->data().is_fullscreen; }
  bool IsLandscape();
#if KIWI_MOBILE
  bool is_stretch_mode() { return config_->data().is_stretch_mode; }
#endif
  SDL_Rect Scaled(const SDL_Rect& rect);
  ImVec2 Scaled(const ImVec2& vec2);
  int Scaled(int i);
  void ChangeFocus(MainFocus focus);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

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
  void HandleKeyEvent(SDL_KeyboardEvent* event) override;
  void OnControllerDeviceAdded(SDL_ControllerDeviceEvent* event) override;
  void OnControllerDeviceRemoved(SDL_ControllerDeviceEvent* event) override;
  void HandleResizedEvent() override;
  void HandlePostEvent() override;
  void HandleDisplayEvent(SDL_DisplayEvent* event) override;
  void Render() override;

  // CanvasObserver:
  void OnAboutToRenderFrame(Canvas* canvas,
                            scoped_refptr<NESFrame> frame) override;

 private:
  void InitializeRuntimeData();
  void InitializeAudio();
  void InitializeUI();
  void InitializeIODevices();
  void InitializeIO();

  void LoadROMByPath(kiwi::base::FilePath rom_path);
  void StartAutoSave();
  void StopAutoSave();
  void ResetAudio();
  std::vector<MenuBar::Menu> GetMenuModel();
  void SetLoading(bool is_loading);
  void ShowMainMenu(bool show);
  void OnScaleChanged();
  void UpdateGameControllerMapping();
  void CreateVirtualTouchButtons();
  void LayoutVirtualTouchButtons();
  void SetVirtualButtonsVisible(bool visible);
  void SaveConfig();
  void SetVirtualTouchButtonVisible(VirtualTouchButton button, bool visible);
  void SetVirtualJoystickButton(int which,
                                kiwi::nes::ControllerButton button,
                                bool pressed);
  bool IsVirtualJoystickButtonPressed(int which,
                                      kiwi::nes::ControllerButton button);
  void CloseInGameMenu();
  void FlexLayout();

  SideMenu::MenuCallbacks CreateMenuSettingsCallbacks();
  SideMenu::MenuCallbacks CreateMenuAboutCallbacks();
  SideMenu::MenuCallbacks CreateMenuChangeFocusToGameItemsCallbacks(
      FlexItemsWidget* items_widget);

  // Splash screen
  void ShowSplash(kiwi::base::OnceClosure callback);
  void CloseSplash(kiwi::base::OnceClosure callback);

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
  void OnLoadDebugROM(kiwi::base::FilePath rom_path);
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
#if ENABLE_EXPORT_ROMS
  void OnExportGameROMs();
#endif
  void OnDebugMemory();
  void OnDebugDisassembly();
  void OnDebugNametable();
  void OnShowUiDemoWidget();
  void OnInGameMenuTrigger();
  void OnInGameMenuItemTrigger(InGameMenu::MenuItem item, int param);
  void OnInGameSettingsItemTrigger(InGameMenu::SettingsItem item,
                                   InGameMenu::SettingsItemContext context);
  void OnInGameSettingsHandleWindowSize(bool is_left);
  void OnInGameSettingsHandleVolume(bool is_left);
  void OnInGameSettingsHandleVolume(const SDL_Rect& volume_bounds,
                                    const SDL_Point& trigger_point);
  void OnVirtualJoystickChanged(int state);

#if KIWI_MOBILE
  void OnScaleModeChanged();
#endif

 private:
  // A headless application means it has no menu, running game directly, and
  // can't go back to main menu. It is used in wasm mode, which the .wasm file
  // shouldn't load all ROMs in a row, but has to load the ROM dynamically.
  bool is_headless_ = false;
  std::set<int> pressing_keys_;
  Splash* splash_ = nullptr;
  // Canvas is owned by this window.
  Canvas* canvas_ = nullptr;
  InGameMenu* in_game_menu_ = nullptr;
  Widget* menu_bar_ = nullptr;
  Widget* palette_widget_ = nullptr;
  Widget* pattern_widget_ = nullptr;
  Widget* frame_rate_widget_ = nullptr;
  Widget* demo_widget_ = nullptr;
  StackWidget* main_stack_widget_ = nullptr;
  KiwiBgWidget* bg_widget_ = nullptr;
  FlexItemsWidget* main_nes_items_widget_ = nullptr;
  FlexItemsWidget* special_nes_items_widget_ = nullptr;
  LoadingWidget* loading_widget_ = nullptr;
  ExportWidget* export_widget_ = nullptr;
  SideMenu* side_menu_ = nullptr;
  Timer side_menu_timer_;
  int side_menu_target_width_ = 0;
  int side_menu_original_width_ = 0;
  CardWidget* contents_card_widget_ = nullptr;
  MemoryWidget* memory_widget_ = nullptr;
  DisassemblyWidget* disassembly_widget_ = nullptr;
  Widget* nametable_widget_ = nullptr;
  std::set<MainWindow::Observer*> observers_;

#if KIWI_MOBILE
  // Main menu buttons
  Widget* vtb_joystick_ = nullptr;
  Widget* vtb_a_ = nullptr;
  Widget* vtb_b_ = nullptr;
  Widget* vtb_ab_ = nullptr;
  Widget* vtb_start_ = nullptr;
  Widget* vtb_select_ = nullptr;
  Widget* vtb_pause_ = nullptr;
#endif

  NESRuntimeID runtime_id_ = NESRuntimeID();
  NESRuntime::Data* runtime_data_ = nullptr;
  std::unique_ptr<NESAudio> audio_;
  scoped_refptr<NESConfig> config_;

  bool virtual_controller_button_states_[2][static_cast<int>(
      kiwi::nes::ControllerButton::kMax)]{false};
};

#endif  // UI_MAIN_WINDOW_H_