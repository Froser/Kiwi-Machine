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
#include <gflags/gflags.h>

#include "build/kiwi_defines.h"
#include "kiwi/nes/controller.h"
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
class StackWidget;
class MemoryWidget;
class DisassemblyWidget;
class FlexItemsWidget;
class CardWidget;
class Splash;

namespace preset_roms {
struct PresetROM;
}

class FullscreenMask;
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

 protected:
  // InputDevice:
  bool IsKeyDown(int controller_id,
                 kiwi::nes::ControllerButton button) override;
  int GetZapperState() override;

  // WindowBase:
  SDL_Rect GetClientBounds() override;
  void HandleKeyEvent(SDL_KeyboardEvent* event) override;
  void OnControllerDeviceAdded(SDL_ControllerDeviceEvent* event) override;
  void OnControllerDeviceRemoved(SDL_ControllerDeviceEvent* event) override;
  void HandleResizedEvent() override;
  void HandleDisplayEvent(SDL_DisplayEvent* event) override;
  void HandleDropFileEvent(SDL_DropEvent* event) override;
  void Render() override;

  // CanvasObserver:
  void OnAboutToRenderFrame(Canvas* canvas,
                            scoped_refptr<NESFrame> frame) override;

 private:
  void InitializeRuntimeData();
  void InitializeAudio();
  void InitializeUI();
  void InitializeIODevices();
  void InitializeDebugROMsOnIOThread();
  void LoadTestRomIfSpecified();

  void LoadROMByPath(kiwi::base::FilePath rom_path);
  void StartAutoSave();
  void StopAutoSave();
  void ResetAudio();
  std::vector<MenuBar::Menu> GetMenuModel();
  void SetLoading(bool is_loading);
  void ShowMainMenu(bool show, bool load_from_finger_gesture);
  void OnScaleChanged();
  void UpdateGameControllerMapping();
  void CreateVirtualTouchButtons();
  void LayoutVirtualTouchButtons();
  void SetVirtualButtonsVisible(bool visible);
  void StashVirtualButtonsVisible();
  void PopVirtualButtonsVisible();
  void SaveConfig();
  void SetVirtualTouchButtonVisible(VirtualTouchButton button, bool visible);
  void SetVirtualJoystickButton(int which,
                                kiwi::nes::ControllerButton button,
                                bool pressed);
  bool IsVirtualJoystickButtonPressed(int which,
                                      kiwi::nes::ControllerButton button);
  void CloseInGameMenu();
  void FlexLayout();
  FlexItemsWidget* GetMainItemsWidget();

  SideMenu::MenuCallbacks CreateMenuSettingsCallbacks();
  SideMenu::MenuCallbacks CreateMenuAboutCallbacks();
  SideMenu::MenuCallbacks CreateMenuChangeFocusToGameItemsCallbacks(
      FlexItemsWidget* items_widget);
  void SwitchToWidgetForSideMenu(int menu_index);
  void SwitchToSideMenuByCurrentFlexItemWidget();
  void ChangeFocusToCurrentSideMenuAndShowFilter();

  // Splash screen
  void ShowSplash(kiwi::base::OnceClosure callback);
  void CloseSplash();

  // Menu callbacks:
  void OnRomLoaded(const std::string& name,
                   bool load_from_finger_gesture,
                   bool success);
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
  void OnLoadPresetROM(preset_roms::PresetROM& rom,
                       bool load_from_finger_gesture);
  void OnLoadDebugROM(kiwi::base::FilePath rom_path);
  void OnToggleAudioEnabled();
  void OnSetAudioVolume(float volume);
  bool IsAudioEnabled();
  void OnToggleAudioChannelMasks(kiwi::nes::AudioChannel which_mask);
  bool IsAudioChannelOn(kiwi::nes::AudioChannel which_mask);
  void OnToggleRenderPaused();
  bool IsRenderPaused();
  void OnSetScreenScale(float scale);
  void OnSetFullscreen();
  void OnUnsetFullscreen(float scale);
  bool ScreenScaleIs(float scale);
  void OnTogglePaletteWidget();
  bool IsPaletteWidgetShown();
  void OnTogglePatternWidget();
  bool IsPatternWidgetShown();
  void OnPerformanceWidget();
  bool IsPerformanceWidgetShown();
  void OnDebugMemory();
  void OnDebugDisassembly();
  void OnDebugNametable();
  void OnShowUiDemoWidget();
  void OnInGameMenuTrigger();
  void OnInGameMenuItemTrigger(InGameMenu::MenuItem item, int param);
  void OnInGameSettingsItemTrigger(InGameMenu::SettingsItem item,
                                   InGameMenu::SettingsItemValue value);
  void OnInGameSettingsHandleWindowSize(bool is_left);
  void OnInGameSettingsHandleVolume(bool is_left);
  void OnInGameSettingsHandleVolume(const SDL_Rect& volume_bounds,
                                    const SDL_Point& trigger_point);
  void OnVirtualJoystickChanged(int state);
  void OnKeyboardMatched();
  void OnJoystickButtonsMatched();
  void OnSetJoystickType(int id, kiwi::nes::Controller::Type type);
  bool IsJoystickType(int id, kiwi::nes::Controller::Type type);

  // FullscreenMask will call HandleWindowFingerDown()
  friend class FullscreenMask;
  bool HandleWindowFingerDown();

  // Give a chance to pause the game, to view disassembly widget.
  void PauseGameIfDisassemblyVisible();

#if KIWI_MOBILE
  void OnScaleModeChanged();
#endif

 private:
  // A headless application means it has no menu, running game directly, and
  // can't go back to main menu. It is used in wasm mode, which the .wasm file
  // shouldn't load all ROMs in a row, but has to load the ROM dynamically.
  bool is_headless_ = false;

  // Frame works with following workflow: RenderFrame, LogicalFrame,
  // RenderFrame, LogicalFrame, ...
  // When render_done_ is true, it means a logical frame should be processed.
  bool render_done_ = false;

  std::set<int> pressing_keys_;
  Splash* splash_ = nullptr;
  // Canvas is owned by this window.
  Canvas* canvas_ = nullptr;
  Widget* fullscreen_mask_ = nullptr;
  InGameMenu* in_game_menu_ = nullptr;
  Widget* menu_bar_ = nullptr;
  Widget* palette_widget_ = nullptr;
  Widget* pattern_widget_ = nullptr;
  Widget* performance_widget_ = nullptr;
  Widget* demo_widget_ = nullptr;
  StackWidget* main_stack_widget_ = nullptr;
  KiwiBgWidget* bg_widget_ = nullptr;
  std::vector<FlexItemsWidget*> items_widgets_;
  LoadingWidget* loading_widget_ = nullptr;
  SideMenu* side_menu_ = nullptr;
  // Side menu index to item widgets' map
  std::map<int, FlexItemsWidget*> flex_items_map_;

  Timer side_menu_timer_;
  int side_menu_target_width_ = 0;
  int side_menu_original_width_ = 0;
  CardWidget* contents_card_widget_ = nullptr;
  MemoryWidget* memory_widget_ = nullptr;
  DisassemblyWidget* disassembly_widget_ = nullptr;
  Widget* nametable_widget_ = nullptr;
  std::set<MainWindow::Observer*> observers_;
#if ENABLE_DEBUG_ROMS
  MenuBar::MenuItem debug_roms_;
#endif

#if KIWI_MOBILE
  // Main menu buttons
  Widget* vtb_joystick_ = nullptr;
  Widget* vtb_a_ = nullptr;
  Widget* vtb_b_ = nullptr;
  Widget* vtb_ab_ = nullptr;
  Widget* vtb_start_ = nullptr;
  Widget* vtb_select_ = nullptr;
  Widget* vtb_pause_ = nullptr;
  bool stashed_virtual_joysticks_visible_state_ = false;
#endif

  NESRuntimeID runtime_id_ = NESRuntimeID();
  NESRuntime::Data* runtime_data_ = nullptr;
  std::unique_ptr<NESAudio> audio_;
  scoped_refptr<NESConfig> config_;

  bool virtual_controller_button_states_[2][static_cast<int>(
      kiwi::nes::ControllerButton::kMax)]{false};
};

#endif  // UI_MAIN_WINDOW_H_