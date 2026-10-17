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
#include "build/build_config.h"

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "build/kiwi_defines.h"
#include "emulator/emulator.h"
#include "emulator/gba/io_devices.h"
#include "kiwi/emulator/nes/controller.h"
#include "models/config.h"
#include "models/gba/audio.h"
#include "models/gba/runtime.h"
#include "models/nes/audio.h"
#include "models/nes/frame.h"
#include "models/nes/runtime.h"
#include "ui/widgets/canvas_observer.h"
#include "ui/widgets/in_game_menu.h"
#include "ui/widgets/menu_bar.h"
#include "ui/window_base.h"

class Canvas;
class GameLibraryView;
class InGameMenu;
class KiwiBgWidget;
class LoadingWidget;
class StackWidget;
class MemoryWidget;
class DisassemblyWidget;
class NametableWidget;
class Splash;
class FpsCounter;
class PerformanceWidget;
#if BUILDFLAG(ENABLE_HD_TEXTURE)
class TextureRenderer;
#endif

namespace preset_roms {
enum class ROMEdition;
struct PresetROM;
}  // namespace preset_roms

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
    virtual void OnSplashFinished();
    virtual void OnSaveStateSucceeded(int slot);
    virtual void OnSaveStateFailed(int slot);
    virtual void OnLoadStateSucceeded(int slot);
    virtual void OnLoadStateFailed(int slot);
    virtual void OnABSwapChanged(int player, bool enabled);
  };

 public:
  explicit MainWindow(const std::string& title,
                      scoped_refptr<ApplicationConfig> config);
  ~MainWindow() override;

  // InitializeAsync() must be called before rendering.
  void InitializeAsync(kiwi::base::OnceClosure callback);

#if BUILDFLAG(IS_WASM)
  // WASM environment uses this instance to load roms.
  static MainWindow* GetInstance();
  void LoadROM_WASM(kiwi::base::FilePath rom_path);
  void SetVolume_WASM(float volume);
  void ResetROM_WASM();
  float GetFPS_WASM();
  void JoystickButtonDown_WASM(kiwi::nes::ControllerButton button);
  void JoystickButtonUp_WASM(kiwi::nes::ControllerButton button);
  void SetABSwapped_WASM(int player, bool swapped);
  bool IsABSwapped_WASM(int player) const;
  void SaveState_WASM(int slot);
  void LoadState_WASM(int slot);
  void DeleteState_WASM(int slot);
  int GetSaveStatesCount_WASM();
  bool HasSaveState_WASM(int slot);
  std::string GetSaveStateThumbnail_WASM(int slot);
#endif

  float window_scale() { return ui_scale_; }
  bool is_fullscreen() { return config_->data().is_fullscreen; }
  bool IsABSwapped(int player) const;
  bool IsLandscape();
#if BUILDFLAG(IS_MOBILE)
  bool is_stretch_mode() { return config_->data().is_stretch_mode; }
#endif
  SDL_Rect Scaled(const SDL_Rect& rect);
  ImVec2 Scaled(const ImVec2& vec2);
  int Scaled(int i);
  int GetMainMenuContentLeft();
  void ChangeFocus(MainFocus focus);
  kiwi::emu::Emulator::Type emulation_system() const {
    return emulation_system_;
  }
  Runtime* runtime() { return runtime_.get(); }
  NESRuntime* nes_runtime();
  const NESRuntime* nes_runtime() const;
  DebugPort* debug_port();
  float audio_volume() const { return config_->data().volume; }

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
  void HandleJoystickButtonEvent(SDL_ControllerButtonEvent* event) override;
  void OnControllerDeviceAdded(SDL_ControllerDeviceEvent* event) override;
  void OnControllerDeviceRemoved(SDL_ControllerDeviceEvent* event) override;
  void HandleResizedEvent() override;
  void HandleFocusChangedEvent(bool focused) override;
  void HandleDisplayEvent(SDL_DisplayEvent* event) override;
  void HandleDropFileEvent(SDL_DropEvent* event) override;
  void Render() override;

  // CanvasObserver:
  void OnAboutToRenderFrame(Canvas* canvas,
                            scoped_refptr<NESFrame> frame) override;

 private:
  struct PendingROM;

  void InitializeRuntimeData();
  void InitializeAudio();
  void InitializeUI();
  void InitializeIODevices();
  void InitializeDebugROMs();
  void LoadTestRomIfSpecified();

  void LoadROMByPath(
      kiwi::base::FilePath rom_path,
      std::optional<kiwi::base::FilePath> save_path = std::nullopt);
  void LoadGBAROMByPath(kiwi::base::FilePath rom_path);
  void ReplaceRuntime(kiwi::emu::Emulator::Type system);
  void PowerOffRuntime();
  void SwitchEmulationSystem(kiwi::emu::Emulator::Type system);
  bool IsEmulationSystem(kiwi::emu::Emulator::Type system) const;
  void UpdateWorkspaceForSystem();
  void ResetAudio();
  std::vector<MenuBar::Menu> GetMenuModel();
  void SetLoading(bool is_loading);
  void ShowMainMenu(bool show, bool load_from_finger_gesture);
  void UpdateUIScale();
  void UpdateGameControllerMapping();
  void CreateVirtualTouchButtons();
  void LayoutVirtualTouchButtons();
#if BUILDFLAG(ENABLE_HD_TEXTURE)
  void SetHDTextureToggleState(bool visible, bool hd_enabled);
#endif
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
  kiwi::nes::ControllerButton MapPhysicalInputButton(
      int controller_id,
      kiwi::nes::ControllerButton button) const;
  bool IsControllerButtonPressedForGameplay(SDL_GameController* controller,
                                            SDL_GameControllerButton button);
  void IgnoreHeldControllerButtonsUntilRelease();
  void SetABSwapped(int player, bool swapped);
  void CloseInGameMenu();
  void ShowSettings();
  void ShowAbout();

  // Splash screen
  void ShowSplash(kiwi::base::OnceClosure callback);
  void CloseSplash();

  // Menu callbacks:
  void OnRomLoaded(std::unique_ptr<PendingROM> pending_rom, bool success);
  void OnGBARomLoaded(std::string fallback_title, bool success);
  void OnQuit();
  void OnResetROM();
  void OnBackToMainMenu();
  void OnRomUnloaded(bool success);
  void OnSaveState(int which_state);
  void OnLoadState(int which_state);
  void OnLoadAutoSavedState(int timestamp, bool discard_newer_states);
  void OnStateSaved(int slot, bool succeed);
  void PrepareToLoadState();
  void OnStateLoaded(int slot_or_timestamp, bool succeed);
  void OnTogglePause();
  void OnPause();
  void OnResume();
  void PauseForFocusLossIfNeeded();
  bool IsPause();
  void OnLoadPresetROM(preset_roms::PresetROM& rom,
                       preset_roms::ROMEdition edition,
                       bool load_from_finger_gesture);
  void OnLoadDebugROM(kiwi::base::FilePath rom_path);
#if BUILDFLAG(ENABLE_HD_TEXTURE)
  void OnSetHDTextureRenderingEnabled(bool enabled);
  void OnToggleHDTextureRendering();
#endif
  void OnToggleAudioEnabled();
  void OnSetAudioVolume(float volume);
  bool IsAudioEnabled();
  void OnToggleAudioChannelMasks(kiwi::nes::AudioChannel which_mask);
  bool IsAudioChannelOn(kiwi::nes::AudioChannel which_mask);
  void OnToggleRenderPaused();
  bool IsRenderPaused();
  void OnSetFullscreen();
  void OnUnsetFullscreen();
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
#if BUILDFLAG(IS_SWITCH)
  void OnQuickLoadTrigger();
#endif
  void OnInGameMenuItemTrigger(const InGameMenu::MenuCommand& command);
  void OnInGameSettingsItemTrigger(InGameMenu::SettingsItem item,
                                   InGameMenu::SettingsItemValue value);
  void OnInGameSettingsHandleWindowMode(bool is_left);
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

#if BUILDFLAG(IS_MOBILE)
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
  PerformanceWidget* performance_widget_ = nullptr;
  Widget* demo_widget_ = nullptr;
  StackWidget* main_stack_widget_ = nullptr;
  KiwiBgWidget* bg_widget_ = nullptr;
  GameLibraryView* game_library_view_ = nullptr;
  LoadingWidget* loading_widget_ = nullptr;
  MemoryWidget* memory_widget_ = nullptr;
  DisassemblyWidget* disassembly_widget_ = nullptr;
  NametableWidget* nametable_widget_ = nullptr;
  std::set<MainWindow::Observer*> observers_;
#if BUILDFLAG(ENABLE_DEBUG_ROMS)
  MenuBar::MenuItem debug_roms_;
#endif

#if BUILDFLAG(IS_WASM)
  std::unique_ptr<FpsCounter> fps_counter_;
#endif

#if BUILDFLAG(IS_MOBILE)
  // Main menu buttons
  Widget* vtb_joystick_ = nullptr;
  Widget* vtb_a_ = nullptr;
  Widget* vtb_b_ = nullptr;
  Widget* vtb_ab_ = nullptr;
  Widget* vtb_start_ = nullptr;
  Widget* vtb_select_ = nullptr;
  Widget* vtb_pause_ = nullptr;
#if BUILDFLAG(ENABLE_HD_TEXTURE)
  Widget* hd_texture_toggle_ = nullptr;
#endif
  bool stashed_virtual_joysticks_visible_state_ = false;
#endif

  std::unique_ptr<Runtime> runtime_;
  std::unique_ptr<NESAudio> audio_;
  std::unique_ptr<GBAAudio> gba_audio_;
  scoped_refptr<ApplicationConfig> config_;
  kiwi::emu::Emulator::Type emulation_system_ = kiwi::emu::Emulator::Type::kNES;
  float ui_scale_ = 1.f;
  std::string current_game_title_;
#if BUILDFLAG(ENABLE_HD_TEXTURE)
  bool hd_texture_available_ = false;
  bool hd_texture_enabled_ = false;
#endif
  bool is_focused_ = true;
  bool resume_after_focus_gained_ = false;

  std::set<std::pair<SDL_JoystickID, SDL_GameControllerButton>>
      ignored_controller_buttons_;
  bool virtual_controller_button_states_[2][static_cast<int>(
      kiwi::nes::ControllerButton::kMax)]{false};
};

#endif  // UI_MAIN_WINDOW_H_
