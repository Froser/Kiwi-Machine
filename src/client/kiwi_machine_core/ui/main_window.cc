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

#include "ui/main_window.h"
#include "build/build_config.h"

#include <gflags/gflags.h>

#include <algorithm>
#include <cstdint>

#include "build/kiwi_defines.h"
#include "debug/debug_roms.h"
#include "kiwi_flags.h"
#include "preset_roms/preset_roms.h"
#include "ui/application.h"
#include "ui/widgets/about_widget.h"
#include "ui/widgets/canvas.h"
#include "ui/widgets/demo_widget.h"
#include "ui/widgets/disassembly_widget.h"
#include "ui/widgets/game_library_view.h"
#include "ui/widgets/kiwi_bg_widget.h"
#include "ui/widgets/loading_widget.h"
#include "ui/widgets/memory_widget.h"
#include "ui/widgets/nametable_widget.h"
#include "ui/widgets/palette_widget.h"
#include "ui/widgets/pattern_widget.h"
#include "ui/widgets/performance_widget.h"
#include "ui/widgets/splash.h"
#include "ui/widgets/stack_widget.h"
#include "ui/widgets/toast.h"
#include "utility/algorithm.h"
#include "utility/audio_effects.h"
#include "utility/fps_counter.h"
#include "utility/key_mapping_util.h"
#include "utility/localization.h"
#include "utility/logging.h"
#include "utility/runtime_util.h"
#include "utility/save_state_util.h"
#if BUILDFLAG(ENABLE_HD_TEXTURE)
#include "utility/texture_renderer.h"
#endif
#if BUILDFLAG(IS_SWITCH)
#include "utility/switch/controller_support.h"
#endif
#include "utility/zip_reader.h"

DEFINE_bool(enable_debug, false, "Shows a menu bar at the top of the window.");

namespace {

MainWindow* g_main_window_instance = nullptr;

constexpr int kWindowPadding = 50;
constexpr int kDefaultWindowWidth =
    Canvas::kNESFrameDefaultWidth + kWindowPadding * 2;
constexpr int kDefaultWindowHeight = Canvas::kNESFrameDefaultHeight;
constexpr int kDefaultFontSize = 15;
constexpr int kSplashTimeoutMs = 2000;
constexpr float kMinUIScale = 1.f;
constexpr float kMaxUIScale = 4.f;

#if BUILDFLAG(IS_SWITCH)
// devkitPro uses Xbox-style SDL labels: Switch A/Y map to SDL B/X, while
// Switch B/X map to SDL A/Y.
SDL_GameControllerButton GetEquivalentSwitchFaceButton(
    SDL_GameControllerButton button) {
  switch (button) {
    case SDL_CONTROLLER_BUTTON_B:
      return SDL_CONTROLLER_BUTTON_X;
    case SDL_CONTROLLER_BUTTON_X:
      return SDL_CONTROLLER_BUTTON_B;
    case SDL_CONTROLLER_BUTTON_A:
      return SDL_CONTROLLER_BUTTON_Y;
    case SDL_CONTROLLER_BUTTON_Y:
      return SDL_CONTROLLER_BUTTON_A;
    default:
      return SDL_CONTROLLER_BUTTON_INVALID;
  }
}
#endif

const kiwi::base::RepeatingCallback<bool()> kNoCheck =
    kiwi::base::RepeatingCallback<bool()>();

int GetDefaultMenuHeight() {
  return kDefaultFontSize + ImGui::GetStyle().FramePadding.y * 2;
}

void FillLayout(WindowBase* window, Widget* widget) {
  SDL_Rect client_bounds = window->GetClientBounds();
  widget->set_bounds(SDL_Rect{0, 0, client_bounds.w, client_bounds.h});
}

void StartPeriodicSaves(Runtime* runtime, scoped_refptr<NESFrame> frame) {
  runtime_util::StartPeriodicSaves(
      runtime,
      kiwi::base::BindRepeating(&NESFrame::GetLastFrame, std::move(frame)));
}

bool CanSaveOrLoadState(MainWindow* window) {
  return save_state_util::CanSaveOrLoadState(window->nes_runtime());
}

void ToastGameControllersAddedOrRemoved(WindowBase* window,
                                        bool is_added,
                                        int which) {
  using namespace string_resources;
  if (SDL_IsGameController(which)) {
    std::string name = SDL_GameControllerNameForIndex(which);
    Toast::ShowToast(
        window,
        name + (is_added ? GetLocalizedString(IDR_MAIN_WINDOW_CONNECTED)
                         : GetLocalizedString(IDR_MAIN_WINDOW_DISCONNECTED)));
  }
}

int CalculateWindowWidth(float window_scale) {
  return kDefaultWindowWidth * window_scale;
}

// Calculates window's height. If |menu_bar| is nullptr, use default menu's
// height if having menu flag.
int CalculateWindowHeight(float window_scale, Widget* menu_bar = nullptr) {
  if (FLAGS_enable_debug) {
    if (menu_bar && menu_bar->bounds().h > 0) {
      // Menu bar is painted, we can get exact menu height here.
      return kDefaultWindowHeight * window_scale + menu_bar->bounds().h;
    }

    return kDefaultWindowHeight * window_scale + GetDefaultMenuHeight();
  }

  return kDefaultWindowHeight * window_scale;
}

SDL_Rect FitRectPreservingAspect(const SDL_Rect& bounds,
                                 int content_width,
                                 int content_height) {
  if (bounds.w <= 0 || bounds.h <= 0 || content_width <= 0 ||
      content_height <= 0) {
    return SDL_Rect{bounds.x, bounds.y, 0, 0};
  }

  int width;
  int height;
  if (static_cast<int64_t>(bounds.w) * content_height <=
      static_cast<int64_t>(bounds.h) * content_width) {
    width = bounds.w;
    height = static_cast<int>(static_cast<int64_t>(width) * content_height /
                              content_width);
  } else {
    height = bounds.h;
    width = static_cast<int>(static_cast<int64_t>(height) * content_width /
                             content_height);
  }

  return SDL_Rect{bounds.x + (bounds.w - width) / 2,
                  bounds.y + (bounds.h - height) / 2, width, height};
}

}  // namespace

struct MainWindow::PendingROM {
  std::string game_title;
  std::string window_title;
  bool load_from_finger_gesture = false;
#if BUILDFLAG(ENABLE_HD_TEXTURE)
  bool hd_texture_available = false;
  bool hd_texture_enabled = false;
  std::unique_ptr<TextureRenderer> texture_renderer;
#endif
};

// A mask widget to handle finger events.
class FullscreenMask : public Widget {
 public:
  FullscreenMask(MainWindow* main_window);
  ~FullscreenMask() override = default;

 protected:
  bool OnMousePressed(SDL_MouseButtonEvent* event) override;
  bool OnMouseReleased(SDL_MouseButtonEvent* event) override;
  bool OnTouchFingerDown(SDL_TouchFingerEvent* event) override;
  bool IsWindowless() override;

 private:
  MainWindow* main_window_ = nullptr;
};

FullscreenMask::FullscreenMask(MainWindow* main_window)
    : Widget(main_window), main_window_(main_window) {}

bool FullscreenMask::OnMousePressed(SDL_MouseButtonEvent* event) {
  return false;
}

bool FullscreenMask::OnMouseReleased(SDL_MouseButtonEvent* event) {
  return false;
}

bool FullscreenMask::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  return main_window_->HandleWindowFingerDown();
}

bool FullscreenMask::IsWindowless() {
  return true;
}

// Observer
MainWindow::Observer::Observer() = default;
MainWindow::Observer::~Observer() = default;
void MainWindow::Observer::OnVolumeChanged(float new_value) {}
void MainWindow::Observer::OnSplashFinished() {}
void MainWindow::Observer::OnSaveStateSucceeded(int slot) {}
void MainWindow::Observer::OnSaveStateFailed(int slot) {}
void MainWindow::Observer::OnLoadStateSucceeded(int slot) {}
void MainWindow::Observer::OnLoadStateFailed(int slot) {}
void MainWindow::Observer::OnABSwapChanged(int player, bool enabled) {}

MainWindow::MainWindow(const std::string& title,
                       scoped_refptr<ApplicationConfig> config)
    : WindowBase(title,
                 CalculateWindowWidth(config->data().window_scale),
                 CalculateWindowHeight(config->data().window_scale)),
      config_(config),
      ui_scale_(config->data().window_scale) {
  if (config_->data().last_system ==
      static_cast<int>(kiwi::emu::Emulator::Type::kGBA)) {
    emulation_system_ = kiwi::emu::Emulator::Type::kGBA;
  }
  is_focused_ =
      (SDL_GetWindowFlags(native_window()) & SDL_WINDOW_INPUT_FOCUS) != 0;
#if !BUILDFLAG(IS_MOBILE) && !BUILDFLAG(IS_WASM)
  SDL_SetWindowMinimumSize(native_window(), kDefaultWindowWidth,
                           kDefaultWindowHeight);
#endif
#if BUILDFLAG(IS_WASM)
  // Only one main window instance should exist in WASM.
  SDL_assert(!g_main_window_instance);
  g_main_window_instance = this;
  fps_counter_ = std::make_unique<FpsCounter>();
#endif
}

NESRuntime* MainWindow::nes_runtime() {
  if (!runtime_ ||
      runtime_->emulator()->GetType() != kiwi::emu::Emulator::Type::kNES) {
    return nullptr;
  }
  return static_cast<NESRuntime*>(runtime_.get());
}

const NESRuntime* MainWindow::nes_runtime() const {
  if (!runtime_ ||
      runtime_->emulator()->GetType() != kiwi::emu::Emulator::Type::kNES) {
    return nullptr;
  }
  return static_cast<const NESRuntime*>(runtime_.get());
}

DebugPort* MainWindow::debug_port() {
  NESRuntime* current_runtime = nes_runtime();
  return current_runtime ? current_runtime->debug_port() : nullptr;
}

// Initialization flow:
// UI: InitializeRuntimeData -> Audio -> Debug ROMs -> UI -> IODevices
// IO: ROMs data -------------------------------------^
void MainWindow::InitializeAsync(kiwi::base::OnceClosure callback) {
  InitializeRuntimeData();
  InitializeAudio();
  if (!FLAGS_enable_debug) {
    // If the main window has menu, it won't show splash for speeding up.
    ShowSplash(kiwi::base::DoNothing());
  }
  // The splash is rendered by the main loop while ROM packages and game
  // metadata are initialized on the IO thread.
  Application::Get()->Initialize(
      kiwi::base::BindOnce(&MainWindow::InitializeDebugROMs,
                           kiwi::base::Unretained(this))
          .Then(kiwi::base::BindOnce(&MainWindow::InitializeUI,
                                     kiwi::base::Unretained(this)))
          .Then(kiwi::base::BindOnce(&MainWindow::InitializeIODevices,
                                     kiwi::base::Unretained(this)))
          .Then(kiwi::base::BindOnce(&MainWindow::LoadTestRomIfSpecified,
                                     kiwi::base::Unretained(this)))
          .Then(std::move(callback)));
}

MainWindow::~MainWindow() {
  if (canvas_) {
    canvas_->RemoveObserver(this);
  }
  SaveConfig();
  PowerOffRuntime();
  Application::Get()->set_runtime(nullptr);

#if BUILDFLAG(IS_WASM)
  g_main_window_instance = nullptr;
#endif
}

#if BUILDFLAG(IS_WASM)

// WASM environment uses this instance to load roms.
MainWindow* MainWindow::GetInstance() {
  SDL_assert(g_main_window_instance);
  return g_main_window_instance;
}

void MainWindow::LoadROM_WASM(kiwi::base::FilePath rom_path) {
  LoadROMByPath(rom_path);
}

void MainWindow::SetVolume_WASM(float volume) {
  OnSetAudioVolume(volume);
}

void MainWindow::ResetROM_WASM() {
  OnResetROM();
}

float MainWindow::GetFPS_WASM() {
  if (fps_counter_) {
    return fps_counter_->GetCurrentFPS();
  }
  return 0.0f;
}

void MainWindow::JoystickButtonDown_WASM(kiwi::nes::ControllerButton button) {
  SetVirtualJoystickButton(0, button, true);
}
void MainWindow::JoystickButtonUp_WASM(kiwi::nes::ControllerButton button) {
  SetVirtualJoystickButton(0, button, false);
}

void MainWindow::SetABSwapped_WASM(int player, bool swapped) {
  SetABSwapped(player, swapped);
}

bool MainWindow::IsABSwapped_WASM(int player) const {
  return IsABSwapped(player);
}

void MainWindow::SaveState_WASM(int slot) {
  if (save_state_util::CanSaveOrLoadState(nes_runtime())) {
    OnSaveState(slot);
  }
}

void MainWindow::LoadState_WASM(int slot) {
  if (save_state_util::CanSaveOrLoadState(nes_runtime())) {
    OnLoadState(slot);
  }
}

int MainWindow::GetSaveStatesCount_WASM() {
  return NESRuntime::MaxSaveStates;
}

bool MainWindow::HasSaveState_WASM(int slot) {
  if (!nes_runtime() || !runtime_->emulator()->AsNESEmulator()) {
    return false;
  }
  auto* rom_data = runtime_->emulator()->AsNESEmulator()->GetRomData();
  if (!rom_data) {
    return false;
  }
  return nes_runtime()->SaveStateExists(rom_data->crc, slot);
}

std::string MainWindow::GetSaveStateThumbnail_WASM(int slot) {
  if (!nes_runtime() || !runtime_->emulator()->AsNESEmulator()) {
    return "";
  }
  auto* rom_data = runtime_->emulator()->AsNESEmulator()->GetRomData();
  if (!rom_data) {
    return "";
  }
  kiwi::nes::Bytes thumbnail_data =
      nes_runtime()->ReadSaveStateThumbnail(rom_data->crc, slot);
  if (thumbnail_data.empty()) {
    return "";
  }
  return "data:image/jpeg;base64," +
         Base64Encode(thumbnail_data.data(), thumbnail_data.size());
}

void MainWindow::DeleteState_WASM(int slot) {
  NESRuntime* current_runtime = nes_runtime();
  if (current_runtime) {
    auto* rom_data = runtime_->emulator()->AsNESEmulator()->GetRomData();
    if (rom_data) {
      current_runtime->DeleteSaveState(rom_data->crc, slot);
    }
  }
}

#endif

#if !BUILDFLAG(IS_MOBILE)
bool MainWindow::IsLandscape() {
  return true;
}
#endif

void MainWindow::ShowSplash(kiwi::base::OnceClosure callback) {
#if !BUILDFLAG(IS_IOS) && \
    !BUILDFLAG(IS_WASM)  // iOS has launch storyboard, WASM will handle it
                         // on frontend here.
  // If we have menu, we don't show splash screen, because we probably want to
  // do some debug works.
  SDL_assert(!splash_);
  std::unique_ptr<Splash> splash = std::make_unique<Splash>(this);
  splash_ = splash.get();
  splash_->SetZOrder(INT_MAX);
  SDL_Rect window_bounds = GetWindowBounds();
  splash_->set_bounds(SDL_Rect{0, 0, window_bounds.w, window_bounds.h});
  PlayEffect(audio_resources::AudioID::kStartup);
  AddWidget(std::move(splash));
#endif
}

void MainWindow::CloseSplash() {
  if (splash_) {
    int ms = splash_->GetElapsedMs();
    if (ms < kSplashTimeoutMs) {
      kiwi::base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          kiwi::base::BindOnce(&MainWindow::CloseSplash,
                               kiwi::base::Unretained(this)),
          kiwi::base::Milliseconds(kSplashTimeoutMs - ms));
      return;
    }

    if (!splash_->is_closing()) {
      main_stack_widget_->set_visible(true);
      splash_->StartClosing();
    }
    // Render() removes the splash after all transition frames are presented.
  } else {
    main_stack_widget_->set_visible(true);
    // Notify observers that splash is finished
    for (auto* observer : observers_) {
      observer->OnSplashFinished();
    }
  }
}

SDL_Rect MainWindow::Scaled(const SDL_Rect& rect) {
  return SDL_Rect{
      static_cast<int>(rect.x * window_scale()),
      static_cast<int>(rect.y * window_scale()),
      static_cast<int>(rect.w * window_scale()),
      static_cast<int>(rect.h * window_scale()),
  };
}

ImVec2 MainWindow::Scaled(const ImVec2& vec2) {
  return ImVec2(vec2.x * window_scale(), vec2.y * window_scale());
}

int MainWindow::Scaled(int i) {
  return i * window_scale();
}

int MainWindow::GetMainMenuContentLeft() {
  return game_library_view_ ? game_library_view_->GetContentLeft() : 0;
}

void MainWindow::ChangeFocus(MainFocus focus) {
  SDL_assert(game_library_view_);
  switch (focus) {
    case MainFocus::kContents:
      game_library_view_->ChangeFocus(GameLibraryView::Focus::kContents);
      break;
    case MainFocus::kSideMenu:
      game_library_view_->ChangeFocus(GameLibraryView::Focus::kSideMenu);
      break;
    default:
      SDL_assert(false);  // Bad focus. Shouldn't be here.
  }
}

void MainWindow::AddObserver(Observer* observer) {
  observers_.insert(observer);
}

void MainWindow::RemoveObserver(Observer* observer) {
  observers_.erase(observer);
}

bool MainWindow::IsABSwapped(int player) const {
  SDL_assert(player == 0 || player == 1);
  return config_->data().swap_ab[player];
}

kiwi::nes::ControllerButton MainWindow::MapPhysicalInputButton(
    int controller_id,
    kiwi::nes::ControllerButton button) const {
  if (!IsABSwapped(controller_id))
    return button;

  if (button == kiwi::nes::ControllerButton::kA)
    return kiwi::nes::ControllerButton::kB;
  if (button == kiwi::nes::ControllerButton::kB)
    return kiwi::nes::ControllerButton::kA;
  return button;
}

bool MainWindow::IsControllerButtonPressedForGameplay(
    SDL_GameController* controller,
    SDL_GameControllerButton button) {
  const bool pressed = SDL_GameControllerGetButton(controller, button);
  const SDL_JoystickID instance_id =
      SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
  const auto key = std::make_pair(instance_id, button);
  const auto ignored = ignored_controller_buttons_.find(key);
  if (ignored == ignored_controller_buttons_.end())
    return pressed;

  if (!pressed)
    ignored_controller_buttons_.erase(ignored);
  return false;
}

void MainWindow::IgnoreHeldControllerButtonsUntilRelease() {
  ignored_controller_buttons_.clear();
  for (int player = 0; player < 2; ++player) {
    auto* controller = reinterpret_cast<SDL_GameController*>(
        runtime_->joystick_mapping(player).which);
    if (!IsGameControllerInputSupported(controller))
      continue;

    const SDL_JoystickID instance_id =
        SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
    for (int value = 0; value < SDL_CONTROLLER_BUTTON_MAX; ++value) {
      const auto button = static_cast<SDL_GameControllerButton>(value);
      if (SDL_GameControllerGetButton(controller, button))
        ignored_controller_buttons_.insert({instance_id, button});
    }
  }
}

bool MainWindow::IsKeyDown(int controller_id,
                           kiwi::nes::ControllerButton button) {
  // Matching virtual joysticks.
  bool matched = IsVirtualJoystickButtonPressed(controller_id, button);
  if (matched)
    return true;

  // Virtual buttons keep their logical meaning and move visually on mobile.
  // Only keyboard and physical controller inputs need remapping here.
  const kiwi::nes::ControllerButton physical_button =
      MapPhysicalInputButton(controller_id, button);

  // Matching keyboard
  matched =
      pressing_keys_.find(runtime_->keyboard_mapping(controller_id)
                              .mapping[static_cast<int>(physical_button)]) !=
      pressing_keys_.cend();

  if (matched) {
    OnKeyboardMatched();
    return true;
  }

  // Key doesn't match. Try joysticks.
  Runtime::JoystickMapping joystick_mapping =
      runtime_->joystick_mapping(controller_id);
  if (joystick_mapping.which) {
    // If mapping is available, cast it to game controller interface.
    SDL_GameController* game_controller =
        reinterpret_cast<SDL_GameController*>(joystick_mapping.which);

    if (!IsGameControllerInputSupported(game_controller))
      return false;

    const SDL_GameControllerButton mapped_button =
        static_cast<SDL_GameControllerButton>(
            runtime_->joystick_mapping(controller_id)
                .mapping.mapping[static_cast<int>(physical_button)]);
    matched =
        IsControllerButtonPressedForGameplay(game_controller, mapped_button);

#if BUILDFLAG(IS_SWITCH)
    if (!matched) {
      const SDL_GameControllerButton equivalent_button =
          GetEquivalentSwitchFaceButton(mapped_button);
      if (equivalent_button != SDL_CONTROLLER_BUTTON_INVALID) {
        matched = IsControllerButtonPressedForGameplay(game_controller,
                                                       equivalent_button);
      }
    }
#endif

    if (!matched) {
      // X and Y are evaluated independently for each requested NES direction,
      // so diagonal stick positions hold one horizontal and one vertical key.
      constexpr Sint16 kDeadZoom = SDL_JOYSTICK_AXIS_MAX / 3;
      switch (physical_button) {
        case kiwi::nes::ControllerButton::kLeft: {
          Sint16 x = SDL_GameControllerGetAxis(game_controller,
                                               SDL_CONTROLLER_AXIS_LEFTX);
          if (SDL_JOYSTICK_AXIS_MIN <= x && x <= -kDeadZoom)
            matched = true;
        } break;
        case kiwi::nes::ControllerButton::kRight: {
          Sint16 x = SDL_GameControllerGetAxis(game_controller,
                                               SDL_CONTROLLER_AXIS_LEFTX);
          if (kDeadZoom <= x && x <= SDL_JOYSTICK_AXIS_MAX)
            matched = true;
        } break;
        case kiwi::nes::ControllerButton::kUp: {
          Sint16 y = SDL_GameControllerGetAxis(game_controller,
                                               SDL_CONTROLLER_AXIS_LEFTY);
          if (SDL_JOYSTICK_AXIS_MIN <= y && y <= -kDeadZoom)
            matched = true;
        } break;
        case kiwi::nes::ControllerButton::kDown: {
          Sint16 y = SDL_GameControllerGetAxis(game_controller,
                                               SDL_CONTROLLER_AXIS_LEFTY);
          if (kDeadZoom <= y && y <= SDL_JOYSTICK_AXIS_MAX)
            matched = true;
        } break;
        default:
          break;
      }
    }

    if (matched) {
      OnJoystickButtonsMatched();
    }
  }

  return matched;
}

int MainWindow::GetZapperState() {
  if (canvas_)
    return canvas_->GetZapperState();

  return ZapperState::kNone;
}

SDL_Rect MainWindow::GetClientBounds() {
  // Excludes menu bar's height;
  SDL_Rect render_bounds = WindowBase::GetClientBounds();
  if (menu_bar_) {
    render_bounds.y += menu_bar_->bounds().h;
    render_bounds.h -= menu_bar_->bounds().h;
  }
  return render_bounds;
}

void MainWindow::HandleKeyEvent(SDL_KeyboardEvent* event) {
  if (!IsPause()) {
    // Do not handle emulator's key event when paused.
    if (event->type == SDL_KEYDOWN)
      pressing_keys_.insert(event->keysym.sym);
    else if (event->type == SDL_KEYUP)
      pressing_keys_.erase(event->keysym.sym);
  }

  WindowBase::HandleKeyEvent(event);
}

void MainWindow::HandleJoystickButtonEvent(SDL_ControllerButtonEvent* event) {
  if (event->type == SDL_CONTROLLERBUTTONUP) {
    ignored_controller_buttons_.erase(
        {event->which, static_cast<SDL_GameControllerButton>(event->button)});
  }
  WindowBase::HandleJoystickButtonEvent(event);
}

void MainWindow::HandleFocusChangedEvent(bool focused) {
  is_focused_ = focused;
  if (!focused) {
    PauseForFocusLossIfNeeded();
  } else if (resume_after_focus_gained_) {
    resume_after_focus_gained_ = false;
    if (IsPause()) {
      OnResume();
    }
  }
}

void MainWindow::OnControllerDeviceAdded(SDL_ControllerDeviceEvent* event) {
  ToastGameControllersAddedOrRemoved(this, true, event->which);
  UpdateGameControllerMapping();
}

void MainWindow::OnControllerDeviceRemoved(SDL_ControllerDeviceEvent* event) {
  ToastGameControllersAddedOrRemoved(this, false, event->which);
  UpdateGameControllerMapping();
}

void MainWindow::HandleResizedEvent() {
  UpdateUIScale();

#if !BUILDFLAG(IS_MOBILE)
  const bool native_fullscreen =
      (SDL_GetWindowFlags(native_window()) & SDL_WINDOW_FULLSCREEN) != 0;
  if (config_->data().is_fullscreen != native_fullscreen) {
    config_->data().is_fullscreen = native_fullscreen;
    config_->SaveConfig();
  }
#endif

  if (main_stack_widget_) {
    FillLayout(this, main_stack_widget_);
  }

  if (game_library_view_) {
    FillLayout(this, game_library_view_);
    game_library_view_->Layout(false);
  }

  if (in_game_menu_) {
    FillLayout(this, in_game_menu_);
  }

  if (splash_) {
    FillLayout(this, splash_);
  }

  if (fullscreen_mask_) {
    FillLayout(this, fullscreen_mask_);
  }

  LayoutVirtualTouchButtons();

  WindowBase::HandleResizedEvent();
}

void MainWindow::HandleDisplayEvent(SDL_DisplayEvent* event) {
  // Display event changing treats as resizing.
  if (event->type == SDL_DISPLAYEVENT_ORIENTATION)
    HandleResizedEvent();
}

void MainWindow::HandleDropFileEvent(SDL_DropEvent* event) {
#if defined(NDEBUG)
  if (!FLAGS_enable_debug) {
    SDL_free(event->file);
    return;
  }
#endif

  SDL_assert(event->file);
  LoadROMByPath(kiwi::base::FilePath::FromUTF8Unsafe(event->file));
  SDL_free(event->file);
}

void MainWindow::Render() {
  WindowBase::Render();

  if (splash_ && splash_->is_closing() &&
      splash_->GetRemainingCloseAnimationMs() == 0) {
    Splash* splash_to_remove = splash_;
    splash_ = nullptr;
    splash_to_remove->set_visible(false);
    RemoveWidgetLater(splash_to_remove);
    for (auto* observer : observers_) {
      observer->OnSplashFinished();
    }
  }

  render_done_ = true;
}

#if !BUILDFLAG(IS_MOBILE)
void MainWindow::OnAboutToRenderFrame(Canvas* canvas,
                                      scoped_refptr<NESFrame> frame) {
  SDL_Rect render_bounds = GetClientBounds();
  const int frame_width = canvas->frame_width();
  const int frame_height = canvas->frame_height();
  SDL_Rect dest_rect =
      FitRectPreservingAspect(render_bounds, frame_width, frame_height);
  canvas->set_frame_scale(
      std::min(static_cast<float>(dest_rect.w) / frame_width,
               static_cast<float>(dest_rect.h) / frame_height));
  canvas->set_bounds(dest_rect);
}
#endif

void MainWindow::InitializeRuntimeData() {
  ReplaceRuntime(emulation_system_);
  if (!FLAGS_enable_debug) {
    // If there's no menu, disable text input.
    SDL_StopTextInput();
  }
}

void MainWindow::ResetAudio() {
  if (emulation_system_ == kiwi::emu::Emulator::Type::kNES) {
    audio_->Reset();
    kiwi::nes::IODevices* devices =
        runtime_->emulator()->AsNESEmulator()->GetIODevices();
    if (devices) {
      devices->set_audio_device(audio_.get());
    }
  } else {
    gba_audio_->Reset();
    kiwi::gba::IODevices* devices =
        runtime_->emulator()->AsGBAEmulator()->GetIODevices();
    if (devices) {
      devices->set_audio_device(gba_audio_.get());
    }
  }
}

void MainWindow::InitializeAudio() {
  SDL_assert(!audio_);
  audio_ = std::make_unique<NESAudio>();
  audio_->Initialize();
  audio_->Start();
  gba_audio_ = std::make_unique<GBAAudio>();
  gba_audio_->Initialize();
  gba_audio_->Start();
  OnSetAudioVolume(config_->data().volume);
}

void MainWindow::InitializeUI() {
  ScopedDisableEffect scoped_disable_effect;
#if BUILDFLAG(IS_WASM)
  is_headless_ = preset_roms::GetPresetOrTestRomsPackages().empty();
#else
  is_headless_ = false;
#endif

  WASM_TRACE_LOG << "Headless mode: " << is_headless_;
  if (FLAGS_enable_debug) {
    // Menu bar
    std::unique_ptr<MenuBar> menu_bar = std::make_unique<MenuBar>(this);
    menu_bar_ = menu_bar.get();
    menu_bar_->set_flags(ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    menu_bar->set_title("Kiwi Machine");
    std::vector<MenuBar::Menu> all_menu = GetMenuModel();
    for (const auto& menu : all_menu) {
      menu_bar->AddMenu(menu);
    }
    AddWidget(std::move(menu_bar));
  }

  // Main stack widget, has background widget and settings widget.
  std::unique_ptr<StackWidget> main_stack_widget =
      std::make_unique<StackWidget>(this);
  main_stack_widget_ = main_stack_widget.get();
  main_stack_widget_->set_visible(false);
  FillLayout(this, main_stack_widget_);
  AddWidget(std::move(main_stack_widget));

  // Background
  SDL_Rect client_bounds = GetClientBounds();
  std::unique_ptr<KiwiBgWidget> bg_widget =
      std::make_unique<KiwiBgWidget>(this);
  bg_widget_ = bg_widget.get();
  main_stack_widget_->PushWidget(std::move(bg_widget));

  if (!is_headless_) {
    GameLibraryView::Callbacks callbacks;
    callbacks.load_rom = kiwi::base::BindRepeating(
        &MainWindow::OnLoadPresetROM, kiwi::base::Unretained(this));
    callbacks.switch_emulation_system = kiwi::base::BindRepeating(
        &MainWindow::SwitchEmulationSystem, kiwi::base::Unretained(this));
    callbacks.show_settings = kiwi::base::BindRepeating(
        &MainWindow::ShowSettings, kiwi::base::Unretained(this));
    callbacks.show_about = kiwi::base::BindRepeating(
        &MainWindow::ShowAbout, kiwi::base::Unretained(this));
    callbacks.quit = kiwi::base::BindRepeating(&MainWindow::OnQuit,
                                               kiwi::base::Unretained(this));
    callbacks.layout_changed = kiwi::base::BindRepeating(
        &MainWindow::LayoutVirtualTouchButtons, kiwi::base::Unretained(this));

    std::unique_ptr<GameLibraryView> game_library_view =
        std::make_unique<GameLibraryView>(this, emulation_system_,
                                          config_->data().last_index,
                                          std::move(callbacks));
    game_library_view_ = game_library_view.get();
    FillLayout(this, game_library_view_);
    bg_widget_->AddWidget(std::move(game_library_view));
    UpdateWorkspaceForSystem();
    ChangeFocus(MainFocus::kContents);
  }

  // Create virtual touch buttons, which will invoke main items_widget's
  // methods.
  CreateVirtualTouchButtons();
  LayoutVirtualTouchButtons();

  std::unique_ptr<Canvas> canvas = std::make_unique<Canvas>(this);
  canvas_ = canvas.get();
  canvas_->frame()->SetEmulator(emulation_system_ ==
                                        kiwi::emu::Emulator::Type::kNES
                                    ? runtime_->emulator()->AsNESEmulator()
                                    : nullptr);
  canvas_->SetEmulationSystem(emulation_system_);
  canvas_->set_visible(false);
  canvas_->AddObserver(this);
  canvas_->set_frame_scale(2.f);
  canvas_->set_in_menu_trigger_callback(kiwi::base::BindRepeating(
      &MainWindow::OnInGameMenuTrigger, kiwi::base::Unretained(this)));
#if BUILDFLAG(IS_SWITCH)
  canvas_->set_quick_load_trigger_callback(kiwi::base::BindRepeating(
      &MainWindow::OnQuickLoadTrigger, kiwi::base::Unretained(this)));
#endif
#if BUILDFLAG(ENABLE_HD_TEXTURE)
  canvas_->set_hd_texture_toggle_callback(kiwi::base::BindRepeating(
      &MainWindow::OnToggleHDTextureRendering, kiwi::base::Unretained(this)));
#endif
  AddWidget(std::move(canvas));

  std::unique_ptr<InGameMenu> in_game_menu = std::make_unique<InGameMenu>(
      this,
      kiwi::base::BindRepeating(&MainWindow::OnInGameMenuItemTrigger,
                                kiwi::base::Unretained(this)),
      kiwi::base::BindRepeating(&MainWindow::OnInGameSettingsItemTrigger,
                                kiwi::base::Unretained(this)));

  std::unique_ptr<Widget> fullscreen_mask =
      std::make_unique<FullscreenMask>(this);
  fullscreen_mask_ = fullscreen_mask.get();
  fullscreen_mask_->set_visible(false);
  AddWidget(std::move(fullscreen_mask));

  if (is_headless_) {
    in_game_menu->SetMenuItemVisible(InGameMenu::MenuItem::kToGameSelection,
                                     false);
  }
  in_game_menu_ = in_game_menu.get();
  in_game_menu_->set_visible(false);
  AddWidget(std::move(in_game_menu));

  // Loading spinning widget.
  std::unique_ptr<LoadingWidget> loading_widget =
      std::make_unique<LoadingWidget>(this);
  loading_widget_ = loading_widget.get();
  loading_widget_->set_visible(false);
  loading_widget_->set_bounds(SDL_Rect{0, 0, client_bounds.w, client_bounds.h});
  AddWidget(std::move(loading_widget));

  // Debug widgets
  if (FLAGS_enable_debug) {
    std::unique_ptr<PaletteWidget> palette_widget =
        std::make_unique<PaletteWidget>(this);
    palette_widget_ = palette_widget.get();
    palette_widget_->set_visible(false);
    AddWidget(std::move(palette_widget));

    std::unique_ptr<PatternWidget> pattern_widget =
        std::make_unique<PatternWidget>(this);
    pattern_widget_ = pattern_widget.get();
    pattern_widget_->set_visible(false);
    AddWidget(std::move(pattern_widget));

    std::unique_ptr<PerformanceWidget> performance_widget =
        std::make_unique<PerformanceWidget>(this, canvas_->frame());
    performance_widget->SetDebugPort(debug_port());
    performance_widget_ = performance_widget.get();
    performance_widget_->set_visible(false);
    AddWidget(std::move(performance_widget));

    std::unique_ptr<MemoryWidget> memory_widget =
        std::make_unique<MemoryWidget>(
            this,
            kiwi::base::BindRepeating(&MainWindow::OnTogglePause,
                                      kiwi::base::Unretained(this)),
            kiwi::base::BindRepeating(&MainWindow::IsPause,
                                      kiwi::base::Unretained(this)));
    memory_widget_ = memory_widget.get();
    memory_widget_->set_visible(false);
    AddWidget(std::move(memory_widget));

    std::unique_ptr<DisassemblyWidget> disassembly_widget =
        std::make_unique<DisassemblyWidget>(
            this,
            kiwi::base::BindRepeating(&MainWindow::OnTogglePause,
                                      kiwi::base::Unretained(this)),
            kiwi::base::BindRepeating(&MainWindow::IsPause,
                                      kiwi::base::Unretained(this)));
    disassembly_widget_ = disassembly_widget.get();
    disassembly_widget_->set_visible(false);
    AddWidget(std::move(disassembly_widget));

    std::unique_ptr<NametableWidget> nametable_widget =
        std::make_unique<NametableWidget>(this);
    nametable_widget->SetDebugPort(debug_port());
    nametable_widget_ = nametable_widget.get();
    nametable_widget_->set_visible(false);
    AddWidget(std::move(nametable_widget));

    std::unique_ptr<Widget> demo_widget = std::make_unique<DemoWidget>(this);
    demo_widget_ = demo_widget.get();
    demo_widget_->set_visible(false);
    AddWidget(std::move(demo_widget));
  }

#if !BUILDFLAG(IS_MOBILE)
  if (is_fullscreen())
    OnSetFullscreen();
  HandleResizedEvent();
#else
  OnScaleModeChanged();
#endif

  CloseSplash();
}

void MainWindow::InitializeIODevices() {
  SDL_assert(canvas_);
  if (emulation_system_ == kiwi::emu::Emulator::Type::kNES) {
    std::unique_ptr<kiwi::nes::IODevices> io_devices =
        std::make_unique<kiwi::nes::IODevices>();
    io_devices->set_input_device(this);
    io_devices->add_render_device(canvas_->render_device());
    io_devices->set_audio_device(audio_.get());
    runtime_->emulator()->AsNESEmulator()->SetIODevices(std::move(io_devices));
  } else {
    std::unique_ptr<kiwi::gba::IODevices> io_devices =
        std::make_unique<kiwi::gba::IODevices>();
    io_devices->set_input_device(this);
    io_devices->add_render_device(canvas_->gba_render_device());
    io_devices->set_audio_device(gba_audio_.get());
    runtime_->emulator()->AsGBAEmulator()->SetIODevices(std::move(io_devices));
  }
}

void MainWindow::InitializeDebugROMs() {
#if BUILDFLAG(ENABLE_DEBUG_ROMS)
  if (HasDebugRoms()) {
    debug_roms_ = CreateDebugRomsMenu(kiwi::base::BindRepeating(
        &MainWindow::OnLoadDebugROM, kiwi::base::Unretained(this)));
  }
#endif
}

void MainWindow::LoadTestRomIfSpecified() {
  if (!FLAGS_test_rom.empty() && FLAGS_enable_debug) {
    std::optional<kiwi::base::FilePath> save_path;
    if (!FLAGS_sav.empty()) {
      save_path = kiwi::base::FilePath::FromUTF8Unsafe(FLAGS_sav);
    }
    LoadROMByPath(kiwi::base::FilePath::FromUTF8Unsafe(FLAGS_test_rom),
                  save_path);
  }
}

void MainWindow::LoadROMByPath(kiwi::base::FilePath rom_path,
                               std::optional<kiwi::base::FilePath> save_path) {
  const kiwi::emu::Emulator::Type target_system =
      runtime_util::GetEmulationSystemForROMPath(rom_path);
  if (canvas_ && canvas_->visible() && emulation_system_ != target_system) {
    Toast::ShowToast(this,
                     GetLocalizedString(
                         string_resources::IDR_MAIN_WINDOW_ROM_NOT_SUPPORTED));
  } else {
    SwitchEmulationSystem(target_system);
    if (target_system == kiwi::emu::Emulator::Type::kGBA) {
      LoadGBAROMByPath(std::move(rom_path));
    } else {
      SDL_assert(runtime_->emulator()->AsNESEmulator());
      SetLoading(true);
      runtime_util::StopPeriodicSaves(runtime_.get());

      auto pending_rom = std::make_unique<PendingROM>();
      pending_rom->game_title = rom_path.BaseName().AsUTF8Unsafe();
      pending_rom->window_title = pending_rom->game_title;

      nes_runtime()->LoadROM(rom_path,
                             kiwi::base::BindOnce(&MainWindow::OnRomLoaded,
                                                  kiwi::base::Unretained(this),
                                                  std::move(pending_rom)),
                             save_path);
    }
  }
}

void MainWindow::LoadGBAROMByPath(kiwi::base::FilePath rom_path) {
  SDL_assert(runtime_.get());
  SetLoading(true);
  runtime_util::StopPeriodicSaves(runtime_.get());
  const std::string fallback_title = rom_path.BaseName().AsUTF8Unsafe();
  runtime_->LoadROM(rom_path, kiwi::base::BindOnce(&MainWindow::OnGBARomLoaded,
                                                   kiwi::base::Unretained(this),
                                                   fallback_title));
}

std::vector<MenuBar::Menu> MainWindow::GetMenuModel() {
  std::vector<MenuBar::Menu> result;

  // Games
  {
    MenuBar::Menu games;
    games.title = "Games";
    games.menu_items.push_back(
        {"Reset ROM", kiwi::base::BindRepeating(&MainWindow::OnResetROM,
                                                kiwi::base::Unretained(this))});

    games.menu_items.push_back(
        {"Back To Main Menu",
         kiwi::base::BindRepeating(&MainWindow::OnBackToMainMenu,
                                   kiwi::base::Unretained(this))});

    games.menu_items.push_back(
        {"Quit", kiwi::base::BindRepeating(&MainWindow::OnQuit,
                                           kiwi::base::Unretained(this))});
    result.push_back(std::move(games));
  }

  // Emulator
  {
    MenuBar::Menu emulator;
    emulator.title = "Emulator";

    emulator.menu_items.push_back(
        {"Pause",
         kiwi::base::BindRepeating(&MainWindow::OnTogglePause,
                                   kiwi::base::Unretained(this)),
         kiwi::base::BindRepeating(&MainWindow::IsPause,
                                   kiwi::base::Unretained(this))});

    emulator.menu_items.push_back(
        {"Enable audio",
         kiwi::base::BindRepeating(&MainWindow::OnToggleAudioEnabled,
                                   kiwi::base::Unretained(this)),
         kiwi::base::BindRepeating(&MainWindow::IsAudioEnabled,
                                   kiwi::base::Unretained(this))});

#if !BUILDFLAG(IS_SWITCH)
    // Window mode
    {
      MenuBar::MenuItem window_mode;
      window_mode.title = "Window mode";
      window_mode.sub_items.push_back(
          {"Windowed",
           kiwi::base::BindRepeating(&MainWindow::OnUnsetFullscreen,
                                     kiwi::base::Unretained(this)),
           kiwi::base::BindRepeating(
               [](MainWindow* window) { return !window->is_fullscreen(); },
               kiwi::base::Unretained(this))});
      window_mode.sub_items.push_back(
          {"Fullscreen",
           kiwi::base::BindRepeating(&MainWindow::OnSetFullscreen,
                                     kiwi::base::Unretained(this)),
           kiwi::base::BindRepeating(&MainWindow::is_fullscreen,
                                     kiwi::base::Unretained(this))});
      emulator.menu_items.push_back(std::move(window_mode));
    }
#endif

    // Controllers
    {
      MenuBar::MenuItem controllers;
      controllers.title = "Controllers";
      {
        // Joy1
        constexpr const char* kJoys[] = {"Joy1", "Joy2"};
        constexpr const char* kTypeName[] = {
            "Standard",
            "Zapper",
        };
        for (int id = 0; id < 2; ++id) {
          MenuBar::MenuItem joy;
          joy.title = kJoys[id];
          for (int i = 0;
               i <= static_cast<int>(kiwi::nes::Controller::Type::kLast); ++i) {
            joy.sub_items.push_back(
                {kTypeName[i],
                 kiwi::base::BindRepeating(
                     &MainWindow::OnSetJoystickType,
                     kiwi::base::Unretained(this), id,
                     static_cast<kiwi::nes::Controller::Type>(i)),
                 kiwi::base::BindRepeating(
                     &MainWindow::IsJoystickType, kiwi::base::Unretained(this),
                     id, static_cast<kiwi::nes::Controller::Type>(i))});
          }
          controllers.sub_items.push_back(std::move(joy));
        }
      }
      emulator.menu_items.push_back(std::move(controllers));
    }

    // Save and load menu
    {
      MenuBar::MenuItem states;
      states.title = "States";
      states.sub_items.push_back(
          {"Save state",
           kiwi::base::BindRepeating(&MainWindow::OnSaveState,
                                     kiwi::base::Unretained(this), 0),
           kNoCheck,
           kiwi::base::BindRepeating(&CanSaveOrLoadState,
                                     kiwi::base::Unretained(this))});
      states.sub_items.push_back(
          {"Load state",
           kiwi::base::BindRepeating(&MainWindow::OnLoadState,
                                     kiwi::base::Unretained(this), 0),
           kNoCheck,
           kiwi::base::BindRepeating(&CanSaveOrLoadState,
                                     kiwi::base::Unretained(this))});
      emulator.menu_items.push_back(std::move(states));
    }

    result.push_back(std::move(emulator));
  }

  // Debug menu
  {
    MenuBar::Menu debug;
    debug.title = "Debug";

    {
      MenuBar::MenuItem debug_audio;
      debug_audio.title = "Audio";
      debug_audio.is_enabled = kiwi::base::BindRepeating(
          &MainWindow::IsEmulationSystem, kiwi::base::Unretained(this),
          kiwi::emu::Emulator::Type::kNES);

      MenuBar::MenuItem debug_audio_square1 = {
          "Square 1",
          kiwi::base::BindRepeating(&MainWindow::OnToggleAudioChannelMasks,
                                    kiwi::base::Unretained(this),
                                    kiwi::nes::kSquare_1),
          kiwi::base::BindRepeating(&MainWindow::IsAudioChannelOn,
                                    kiwi::base::Unretained(this),
                                    kiwi::nes::kSquare_1)};
      MenuBar::MenuItem debug_audio_square2 = {
          "Square 2",
          kiwi::base::BindRepeating(&MainWindow::OnToggleAudioChannelMasks,
                                    kiwi::base::Unretained(this),
                                    kiwi::nes::kSquare_2),
          kiwi::base::BindRepeating(&MainWindow::IsAudioChannelOn,
                                    kiwi::base::Unretained(this),
                                    kiwi::nes::kSquare_2)};
      MenuBar::MenuItem debug_audio_triangle = {
          "Triangle",
          kiwi::base::BindRepeating(&MainWindow::OnToggleAudioChannelMasks,
                                    kiwi::base::Unretained(this),
                                    kiwi::nes::kTriangle),
          kiwi::base::BindRepeating(&MainWindow::IsAudioChannelOn,
                                    kiwi::base::Unretained(this),
                                    kiwi::nes::kTriangle)};
      MenuBar::MenuItem debug_audio_noise = {
          "Noise",
          kiwi::base::BindRepeating(&MainWindow::OnToggleAudioChannelMasks,
                                    kiwi::base::Unretained(this),
                                    kiwi::nes::kNoise),
          kiwi::base::BindRepeating(&MainWindow::IsAudioChannelOn,
                                    kiwi::base::Unretained(this),
                                    kiwi::nes::kNoise)};
      MenuBar::MenuItem debug_audio_dmc = {
          "DMC",
          kiwi::base::BindRepeating(&MainWindow::OnToggleAudioChannelMasks,
                                    kiwi::base::Unretained(this),
                                    kiwi::nes::kDMC),
          kiwi::base::BindRepeating(&MainWindow::IsAudioChannelOn,
                                    kiwi::base::Unretained(this),
                                    kiwi::nes::kDMC)};

      debug_audio.sub_items.push_back(std::move(debug_audio_square1));
      debug_audio.sub_items.push_back(std::move(debug_audio_square2));
      debug_audio.sub_items.push_back(std::move(debug_audio_triangle));
      debug_audio.sub_items.push_back(std::move(debug_audio_noise));
      debug_audio.sub_items.push_back(std::move(debug_audio_dmc));

      debug.menu_items.push_back(std::move(debug_audio));
    }

    {
      MenuBar::MenuItem disable_render = {
          "Pause Render",
          kiwi::base::BindRepeating(&MainWindow::OnToggleRenderPaused,
                                    kiwi::base::Unretained(this)),
          kiwi::base::BindRepeating(&MainWindow::IsRenderPaused,
                                    kiwi::base::Unretained(this)),
          kiwi::base::BindRepeating(&MainWindow::IsEmulationSystem,
                                    kiwi::base::Unretained(this),
                                    kiwi::emu::Emulator::Type::kNES),
      };
      debug.menu_items.push_back(std::move(disable_render));
    }

#if BUILDFLAG(ENABLE_DEBUG_ROMS)
    if (!debug_roms_.sub_items.empty()) {
      debug.menu_items.push_back(std::move(debug_roms_));
      // debug_roms_ should no longer use.
    }
#endif

    debug.menu_items.push_back(
        {"Palette",
         kiwi::base::BindRepeating(&MainWindow::OnTogglePaletteWidget,
                                   kiwi::base::Unretained(this)),
         kiwi::base::BindRepeating(&MainWindow::IsPaletteWidgetShown,
                                   kiwi::base::Unretained(this))});

    debug.menu_items.push_back(
        {"Patterns",
         kiwi::base::BindRepeating(&MainWindow::OnTogglePatternWidget,
                                   kiwi::base::Unretained(this)),
         kiwi::base::BindRepeating(&MainWindow::IsPatternWidgetShown,
                                   kiwi::base::Unretained(this))});

    debug.menu_items.push_back(
        {"Performance",
         kiwi::base::BindRepeating(&MainWindow::OnPerformanceWidget,
                                   kiwi::base::Unretained(this)),
         kiwi::base::BindRepeating(&MainWindow::IsPerformanceWidgetShown,
                                   kiwi::base::Unretained(this))});

    debug.menu_items.push_back(
        {"Memory", kiwi::base::BindRepeating(&MainWindow::OnDebugMemory,
                                             kiwi::base::Unretained(this))});

    debug.menu_items.push_back(
        {"Disassembly",
         kiwi::base::BindRepeating(&MainWindow::OnDebugDisassembly,
                                   kiwi::base::Unretained(this))});

    debug.menu_items.push_back(
        {"Nametable", kiwi::base::BindRepeating(&MainWindow::OnDebugNametable,
                                                kiwi::base::Unretained(this))});

    debug.menu_items.push_back(
        {"UI Demo Widget",
         kiwi::base::BindRepeating(&MainWindow::OnShowUiDemoWidget,
                                   kiwi::base::Unretained(this))});

    result.push_back(std::move(debug));
  }

  return result;
}

void MainWindow::SetLoading(bool is_loading) {
  if (bg_widget_ && loading_widget_) {
    bg_widget_->SetLoading(is_loading);
    loading_widget_->set_visible(is_loading);
  }
}

void MainWindow::ShowMainMenu(bool show, bool load_from_finger_gesture) {
  if (bg_widget_ && canvas_ && fullscreen_mask_) {
    canvas_->set_visible(!show);
    fullscreen_mask_->set_visible(!show);
    bg_widget_->set_visible(show);
  }

  if (!load_from_finger_gesture) {
    // If the ROM is not loaded by finger events (Finger Up), it means we have
    // mouse, joysticks or keyboard, so we do not show virtual buttons here.
    SetVirtualButtonsVisible(false);
  } else {
    SetVirtualButtonsVisible(!show);
  }
#if BUILDFLAG(ENABLE_HD_TEXTURE)
  SetHDTextureToggleState(!show && hd_texture_available_, hd_texture_enabled_);
#endif
  SetLoading(false);
}

void MainWindow::UpdateUIScale() {
#if !BUILDFLAG(IS_MOBILE)
  SDL_Rect client_bounds = GetClientBounds();
  const float width_scale =
      static_cast<float>(client_bounds.w) / kDefaultWindowWidth;
  const float height_scale =
      static_cast<float>(client_bounds.h) / kDefaultWindowHeight;
  ui_scale_ =
      std::clamp(std::min(width_scale, height_scale), kMinUIScale, kMaxUIScale);
#else
  ui_scale_ = config_->data().window_scale;
#endif
}

void MainWindow::UpdateGameControllerMapping() {
  const auto& game_controllers = Application::Get()->game_controllers();

  // If any game controller is removed, remove it from joystick mapping as well.
  for (int player = 0; player < 2; ++player) {
    auto* mapped_controller = reinterpret_cast<SDL_GameController*>(
        runtime_->joystick_mapping(player).which);
    if (mapped_controller &&
        game_controllers.find(mapped_controller) == game_controllers.end()) {
      runtime_->joystick_mapping(player).which = nullptr;
    }
  }

  const std::vector<SDL_GameController*> ordered_controllers =
      GetControllerList();
  auto is_mapped = [this](SDL_GameController* controller) {
    return runtime_->joystick_mapping(0).which == controller ||
           runtime_->joystick_mapping(1).which == controller;
  };

  // devkitPro SDL announces all eight Switch controller slots at startup.
  // Preserve existing assignments and only fill empty players; restarting from
  // P1 on every add event can otherwise leave gameplay bound to an idle slot.
  for (int player = 0; player < 2; ++player) {
    if (runtime_->joystick_mapping(player).which)
      continue;

    auto controller =
        std::find_if(ordered_controllers.begin(), ordered_controllers.end(),
                     [&is_mapped](SDL_GameController* candidate) {
                       return candidate && !is_mapped(candidate);
                     });
    if (controller == ordered_controllers.end())
      break;

    SetControllerMapping(runtime_.get(), player, *controller, false);
  }
}

#if !BUILDFLAG(IS_MOBILE)
void MainWindow::CreateVirtualTouchButtons() {}

void MainWindow::SetVirtualTouchButtonVisible(VirtualTouchButton button,
                                              bool visible) {}

void MainWindow::LayoutVirtualTouchButtons() {}

#if BUILDFLAG(ENABLE_HD_TEXTURE)
void MainWindow::SetHDTextureToggleState(bool visible, bool hd_enabled) {}
#endif

void MainWindow::OnVirtualJoystickChanged(int state) {}

void MainWindow::OnKeyboardMatched() {}

void MainWindow::OnJoystickButtonsMatched() {}

bool MainWindow::HandleWindowFingerDown() {
  return false;
}

void MainWindow::PauseGameIfDisassemblyVisible() {
  // Give a chance to pause the emulator
  if (disassembly_widget_ && disassembly_widget_->visible())
    OnPause();
}

void MainWindow::StashVirtualButtonsVisible() {}

void MainWindow::PopVirtualButtonsVisible() {}

#endif

void MainWindow::OnSetJoystickType(int id, kiwi::nes::Controller::Type type) {
  if (nes_runtime() && nes_runtime()->debug_port()) {
    nes_runtime()->debug_port()->SetControllerType(id, type);
  }
}

bool MainWindow::IsJoystickType(int id, kiwi::nes::Controller::Type type) {
  if (nes_runtime() && nes_runtime()->debug_port()) {
    return nes_runtime()->debug_port()->GetControllerType(id) == type;
  }

  return type == kiwi::nes::Controller::Type::kStandard;
}

void MainWindow::SetVirtualButtonsVisible(bool visible) {
  SetVirtualTouchButtonVisible(VirtualTouchButton::kJoystick, visible);
  SetVirtualTouchButtonVisible(VirtualTouchButton::kA, visible);
  SetVirtualTouchButtonVisible(VirtualTouchButton::kB, visible);
  SetVirtualTouchButtonVisible(VirtualTouchButton::kAB, visible);
  SetVirtualTouchButtonVisible(VirtualTouchButton::kStart, visible);
  SetVirtualTouchButtonVisible(VirtualTouchButton::kSelect, visible);
  SetVirtualTouchButtonVisible(VirtualTouchButton::kPause, visible);
}

void MainWindow::SetVirtualJoystickButton(int which,
                                          kiwi::nes::ControllerButton button,
                                          bool pressed) {
  SDL_assert(which == 0 || which == 1);
  virtual_controller_button_states_[which][static_cast<int>(button)] = pressed;
}

bool MainWindow::IsVirtualJoystickButtonPressed(
    int which,
    kiwi::nes::ControllerButton button) {
  SDL_assert(which == 0 || which == 1);
  return virtual_controller_button_states_[which][static_cast<int>(button)];
}

void MainWindow::SetABSwapped(int player, bool swapped) {
  SDL_assert(player == 0 || player == 1);
  if (config_->data().swap_ab[player] == swapped)
    return;

  SetVirtualJoystickButton(player, kiwi::nes::ControllerButton::kA, false);
  SetVirtualJoystickButton(player, kiwi::nes::ControllerButton::kB, false);
  config_->data().swap_ab[player] = swapped;
  config_->SaveConfig();
  if (player == 0)
    LayoutVirtualTouchButtons();
  for (auto* observer : observers_)
    observer->OnABSwapChanged(player, swapped);
}

void MainWindow::CloseInGameMenu() {
  // A menu action must not remain pressed when gameplay resumes.
  IgnoreHeldControllerButtonsUntilRelease();
  OnResume();
  in_game_menu_->Close();
}

void MainWindow::ReplaceRuntime(kiwi::emu::Emulator::Type system) {
  runtime_util::CreateRuntimeOptions options;
  options.volume = config_->data().volume;
  options.joystick_mappings =
      runtime_util::CopyJoystickMappings(runtime_.get());
  if (FLAGS_enable_debug) {
    options.breakpoint_callback = kiwi::base::BindRepeating(
        &MainWindow::OnPause, kiwi::base::Unretained(this));
  }

  if (runtime_) {
    PowerOffRuntime();
    Application::Get()->set_runtime(nullptr);
    runtime_.reset();
  }

  runtime_ = runtime_util::CreateRuntime(
      system, Application::Get()->profile_path(), std::move(options));
  Application::Get()->set_runtime(runtime_.get());

  if (performance_widget_) {
    performance_widget_->SetDebugPort(debug_port());
  }
  if (nametable_widget_) {
    nametable_widget_->SetDebugPort(debug_port());
  }
  if (canvas_) {
    canvas_->frame()->SetEmulator(system == kiwi::emu::Emulator::Type::kNES
                                      ? runtime_->emulator()->AsNESEmulator()
                                      : nullptr);
    InitializeIODevices();
  }
}

void MainWindow::PowerOffRuntime() {
  if (runtime_) {
    if (performance_widget_) {
      performance_widget_->SetDebugPort(nullptr);
    }
    if (nametable_widget_) {
      nametable_widget_->SetDebugPort(nullptr);
    }
    if (canvas_) {
      canvas_->frame()->SetEmulator(nullptr);
    }
    runtime_util::ShutdownRuntime(runtime_.get());
  }
}

void MainWindow::SwitchEmulationSystem(kiwi::emu::Emulator::Type system) {
  if (emulation_system_ != system && (!canvas_ || !canvas_->visible())) {
    emulation_system_ = system;
    ReplaceRuntime(system);
    config_->data().last_system = static_cast<int>(system);
    config_->SaveConfig();
    UpdateWorkspaceForSystem();
  }
}

bool MainWindow::IsEmulationSystem(kiwi::emu::Emulator::Type system) const {
  return emulation_system_ == system;
}

void MainWindow::UpdateWorkspaceForSystem() {
  if (canvas_) {
    canvas_->SetEmulationSystem(emulation_system_);
  }
  if (game_library_view_) {
    game_library_view_->SetEmulationSystem(emulation_system_,
                                           config_->data().last_index);
  }

  SetTitle(emulation_system_ == kiwi::emu::Emulator::Type::kGBA
               ? "Kiwi Machine [Game Boy Advance]"
               : "Kiwi Machine");
}

void MainWindow::ShowSettings() {
  std::unique_ptr<InGameMenu> in_game_menu = std::make_unique<InGameMenu>(
      this,
      kiwi::base::BindRepeating(
          [](StackWidget* stack_widget,
             const InGameMenu::MenuCommand& command) {
            // Mapping button 'B' will trigger kContinue.
            if (command.item == InGameMenu::MenuItem::kToGameSelection ||
                command.item == InGameMenu::MenuItem::kContinue) {
              stack_widget->PopWidget();
            }
          },
          main_stack_widget_),
      kiwi::base::BindRepeating(&MainWindow::OnInGameSettingsItemTrigger,
                                kiwi::base::Unretained(this)));
  in_game_menu->SetMenuItemVisible(InGameMenu::MenuItem::kContinue, false);
  in_game_menu->SetMenuItemVisible(InGameMenu::MenuItem::kLoadAutoSave, false);
  in_game_menu->SetMenuItemVisible(InGameMenu::MenuItem::kLoadState, false);
  in_game_menu->SetMenuItemVisible(InGameMenu::MenuItem::kSaveState, false);
  in_game_menu->SetMenuItemVisible(InGameMenu::MenuItem::kResetGame, false);
  in_game_menu->set_bounds(SDL_Rect{0, 0, main_stack_widget_->bounds().w,
                                    main_stack_widget_->bounds().h});
  main_stack_widget_->PushWidget(std::move(in_game_menu));
}

void MainWindow::ShowAbout() {
  std::unique_ptr<AboutWidget> about_widget =
      std::make_unique<AboutWidget>(this, main_stack_widget_);
  about_widget->set_bounds(SDL_Rect{0, 0, main_stack_widget_->bounds().w,
                                    main_stack_widget_->bounds().h});
  main_stack_widget_->PushWidget(std::move(about_widget));
}

void MainWindow::OnRomLoaded(std::unique_ptr<PendingROM> pending_rom,
                             bool success) {
  if (!success) {
    SetLoading(false);
    if (runtime_->emulator()->AsNESEmulator()->GetRomData()) {
      StartPeriodicSaves(runtime_.get(), canvas_->frame());
    }
    Toast::ShowToast(this,
                     GetLocalizedString(
                         string_resources::IDR_MAIN_WINDOW_ROM_NOT_SUPPORTED));
  } else {
    emulation_system_ = kiwi::emu::Emulator::Type::kNES;
    canvas_->SetEmulationSystem(emulation_system_);
    in_game_menu_->SetMenuItemVisible(InGameMenu::MenuItem::kLoadAutoSave,
                                      true);
    in_game_menu_->SetMenuItemVisible(InGameMenu::MenuItem::kLoadState, true);
    in_game_menu_->SetMenuItemVisible(InGameMenu::MenuItem::kSaveState, true);
    current_game_title_ = std::move(pending_rom->game_title);
#if BUILDFLAG(ENABLE_HD_TEXTURE)
    hd_texture_available_ = pending_rom->hd_texture_available;
    hd_texture_enabled_ = pending_rom->hd_texture_enabled;
    canvas_->frame()->SetTextureRenderer(
        std::move(pending_rom->texture_renderer));
    canvas_->frame()->SetHDTextureRenderingEnabled(hd_texture_enabled_);
    canvas_->SetHDTextureToggleAvailable(hd_texture_available_);
#endif
    ShowMainMenu(false, pending_rom->load_from_finger_gesture);
    SetTitle(pending_rom->window_title);
    StartPeriodicSaves(runtime_.get(), canvas_->frame());
    PauseGameIfDisassemblyVisible();
    PauseForFocusLossIfNeeded();
  }
}

void MainWindow::OnGBARomLoaded(std::string fallback_title, bool success) {
  if (!success) {
    SetLoading(false);
    if (runtime_->emulator()->HasROM()) {
      StartPeriodicSaves(runtime_.get(), canvas_->frame());
    }
    Toast::ShowToast(this,
                     GetLocalizedString(
                         string_resources::IDR_MAIN_WINDOW_ROM_NOT_SUPPORTED));
  } else {
    emulation_system_ = kiwi::emu::Emulator::Type::kGBA;
    config_->data().last_system = static_cast<int>(emulation_system_);
    config_->SaveConfig();
    canvas_->SetEmulationSystem(emulation_system_);
    in_game_menu_->SetMenuItemVisible(InGameMenu::MenuItem::kLoadAutoSave,
                                      false);
    in_game_menu_->SetMenuItemVisible(InGameMenu::MenuItem::kLoadState, false);
    in_game_menu_->SetMenuItemVisible(InGameMenu::MenuItem::kSaveState, false);
#if BUILDFLAG(ENABLE_HD_TEXTURE)
    hd_texture_available_ = false;
    hd_texture_enabled_ = false;
    canvas_->SetHDTextureToggleAvailable(false);
#endif

    const kiwi::gba::RomData* rom_data =
        runtime_->emulator()->AsGBAEmulator()->GetRomData();
    current_game_title_ =
        rom_data && !rom_data->title.empty() ? rom_data->title : fallback_title;
    ShowMainMenu(false, false);
    SetTitle(current_game_title_ + " [GBA]");
    StartPeriodicSaves(runtime_.get(), canvas_->frame());
    PauseForFocusLossIfNeeded();
  }
}

void MainWindow::OnQuit() {
  SDL_Event quit_event;
  quit_event.type = SDL_QUIT;
  SDL_PushEvent(&quit_event);
}

void MainWindow::OnResetROM() {
  kiwi::base::OnceClosure reset_callback = kiwi::base::BindOnce(
      &MainWindow::ResetAudio, kiwi::base::Unretained(this));
  if (emulation_system_ == kiwi::emu::Emulator::Type::kNES) {
    reset_callback = std::move(reset_callback)
                         .Then(kiwi::base::BindOnce(
                             &MainWindow::PauseGameIfDisassemblyVisible,
                             kiwi::base::Unretained(this)));
  }
  runtime_->emulator()->Reset(
      std::move(reset_callback)
          .Then(kiwi::base::BindOnce(&MainWindow::PauseForFocusLossIfNeeded,
                                     kiwi::base::Unretained(this))));
}

void MainWindow::OnBackToMainMenu() {
  SetLoading(true);
  runtime_util::StopPeriodicSaves(runtime_.get());
  runtime_->UnloadROM(kiwi::base::BindOnce(&MainWindow::OnRomUnloaded,
                                           kiwi::base::Unretained(this)));
}

void MainWindow::OnRomUnloaded(bool success) {
  if (!success) {
    SetLoading(false);
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Could not save battery-backed data before unloading ROM.");
    StartPeriodicSaves(runtime_.get(), canvas_->frame());
  } else {
    current_game_title_.clear();
#if BUILDFLAG(ENABLE_HD_TEXTURE)
    hd_texture_available_ = false;
    hd_texture_enabled_ = false;
    canvas_->SetHDTextureToggleAvailable(false);
    canvas_->frame()->SetTextureRenderer(nullptr);
#endif
    ShowMainMenu(true, false);
    canvas_->Clear();
    UpdateWorkspaceForSystem();
  }
}

void MainWindow::OnSaveState(int which_state) {
  save_state_util::SaveState(
      nes_runtime(), which_state,
      kiwi::base::BindRepeating(&NESFrame::GetLastFrame, canvas_->frame()),
      kiwi::base::BindOnce(&MainWindow::OnStateSaved,
                           kiwi::base::Unretained(this)));
}

void MainWindow::OnLoadState(int which_state) {
  save_state_util::LoadState(
      nes_runtime(), which_state,
      kiwi::base::BindOnce(&MainWindow::PrepareToLoadState,
                           kiwi::base::Unretained(this)),
      kiwi::base::BindOnce(&MainWindow::OnStateLoaded,
                           kiwi::base::Unretained(this)));
}

void MainWindow::OnLoadAutoSavedState(int timestamp,
                                      bool discard_newer_states) {
  save_state_util::LoadAutoSavedState(
      nes_runtime(), timestamp, discard_newer_states,
      kiwi::base::BindOnce(&MainWindow::PrepareToLoadState,
                           kiwi::base::Unretained(this)),
      kiwi::base::BindOnce(&MainWindow::OnStateLoaded,
                           kiwi::base::Unretained(this)));
}

void MainWindow::OnStateSaved(int slot, bool succeed) {
  using namespace string_resources;

  if (succeed) {
    SDL_assert(in_game_menu_);
    in_game_menu_->RefreshStateAvailability();
    in_game_menu_->RefreshStatePreview();
#if !BUILDFLAG(IS_WASM)
    Toast::ShowToast(this, GetLocalizedString(IDR_MAIN_WINDOW_SAVE_SUCCEEDED));
#endif
    for (auto* observer : observers_) {
      observer->OnSaveStateSucceeded(slot);
    }
  } else {
#if !BUILDFLAG(IS_WASM)
    Toast::ShowToast(this, GetLocalizedString(IDR_MAIN_WINDOW_SAVE_FAILED));
#endif
    for (auto* observer : observers_) {
      observer->OnSaveStateFailed(slot);
    }
  }
}

void MainWindow::PrepareToLoadState() {
  audio_->Reset();
  audio_->Start();
}

void MainWindow::OnStateLoaded(int slot_or_timestamp, bool succeed) {
  using namespace string_resources;

#if !BUILDFLAG(IS_WASM)
  Toast::ShowToast(this,
                   GetLocalizedString(succeed ? IDR_MAIN_WINDOW_LOAD_SUCCEEDED
                                              : IDR_MAIN_WINDOW_LOAD_FAILED));
#endif

  for (auto* observer : observers_) {
    if (succeed) {
      observer->OnLoadStateSucceeded(slot_or_timestamp);
    } else {
      observer->OnLoadStateFailed(slot_or_timestamp);
    }
  }
  PauseForFocusLossIfNeeded();
}

void MainWindow::OnTogglePause() {
  if (IsPause()) {
    OnResume();
  } else {
    OnPause();
  }
}

void MainWindow::OnPause() {
  resume_after_focus_gained_ = false;
  runtime_util::StopPeriodicSaves(runtime_.get());
  // Cleanup all pressing keys when pausing.
  pressing_keys_.clear();
  runtime_->emulator()->Pause();
  if (emulation_system_ == kiwi::emu::Emulator::Type::kNES) {
    if (memory_widget_)
      memory_widget_->UpdateMemory();
    if (disassembly_widget_)
      disassembly_widget_->UpdateDisassembly();
  }
  StashVirtualButtonsVisible();
  SetVirtualButtonsVisible(false);
#if BUILDFLAG(ENABLE_HD_TEXTURE)
  SetHDTextureToggleState(false, hd_texture_enabled_);
#endif
}

void MainWindow::OnResume() {
  if (!is_focused_) {
    PauseForFocusLossIfNeeded();
    resume_after_focus_gained_ = IsPause();
  } else {
    resume_after_focus_gained_ = false;
    runtime_->emulator()->Run();
    PopVirtualButtonsVisible();
#if BUILDFLAG(ENABLE_HD_TEXTURE)
    SetHDTextureToggleState(hd_texture_available_ && canvas_->visible(),
                            hd_texture_enabled_);
#endif
    StartPeriodicSaves(runtime_.get(), canvas_->frame());
  }
}

void MainWindow::PauseForFocusLossIfNeeded() {
  if (!is_focused_) {
    if (runtime_->emulator()->IsRunning()) {
      OnPause();
      resume_after_focus_gained_ = true;
    }
  }
}

void MainWindow::OnLoadPresetROM(preset_roms::PresetROM& rom,
                                 preset_roms::ROMEdition edition,
                                 bool load_from_finger_gesture) {
#if BUILDFLAG(ENABLE_HD_TEXTURE)
  SDL_assert(edition == preset_roms::ROMEdition::kOriginal ||
             rom.hd_edition_available);
#else
  SDL_assert(edition == preset_roms::ROMEdition::kOriginal);
#endif
  const kiwi::emu::Emulator::Type target_system =
      rom.type == preset_roms::PackageType::kGBA
          ? kiwi::emu::Emulator::Type::kGBA
          : kiwi::emu::Emulator::Type::kNES;
  SwitchEmulationSystem(target_system);
  SetLoading(true);
  runtime_util::StopPeriodicSaves(runtime_.get());

  Application::Get()->GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, kiwi::base::BindOnce(&LoadPresetROM, std::ref(rom), edition),
      kiwi::base::BindOnce(
          [](MainWindow* this_window, preset_roms::PresetROM& rom,
             preset_roms::ROMEdition edition, bool load_from_finger_gesture,
             LoadedPresetROM loaded_rom) {
            if (rom.type == preset_roms::PackageType::kGBA) {
              const std::string title = GetROMLocalizedTitle(rom);
              this_window->runtime_->LoadROM(
                  std::move(loaded_rom.rom_data),
                  kiwi::base::BindOnce(
                      [](MainWindow* this_window, std::string title,
                         bool success) {
                        this_window->OnGBARomLoaded(title, success);
                        if (success) {
                          this_window->current_game_title_ = title;
                          this_window->SetTitle(title + " [GBA]");
                        }
                      },
                      kiwi::base::Unretained(this_window), title));
            } else {
              auto pending_rom = std::make_unique<PendingROM>();
              pending_rom->game_title = GetROMLocalizedTitle(rom);
#if BUILDFLAG(ENABLE_HD_TEXTURE)
              const bool has_hd_renderer =
                  loaded_rom.texture_renderer != nullptr;
              const bool hd_enabled =
                  has_hd_renderer && edition == preset_roms::ROMEdition::kHD;
              pending_rom->window_title = GetROMLocalizedTitle(
                  rom, hd_enabled ? preset_roms::ROMEdition::kHD
                                  : preset_roms::ROMEdition::kOriginal);
              pending_rom->hd_texture_available =
                  has_hd_renderer && loaded_rom.hd_texture_toggle_available;
              pending_rom->hd_texture_enabled = hd_enabled;
#else
              pending_rom->window_title =
                  GetROMLocalizedTitle(rom, preset_roms::ROMEdition::kOriginal);
#endif
              pending_rom->load_from_finger_gesture = load_from_finger_gesture;
              kiwi::nes::Emulator::LoadOptions load_options;
#if BUILDFLAG(ENABLE_HD_TEXTURE)
              if (has_hd_renderer) {
                load_options.capture_ppu_texture_metadata = hd_enabled;
                load_options.uses_chr_ram =
                    loaded_rom.texture_renderer->IsUseChrRam();
              }
              pending_rom->texture_renderer =
                  std::move(loaded_rom.texture_renderer);
#endif
              this_window->nes_runtime()->LoadROM(
                  std::move(loaded_rom.rom_data),
                  kiwi::base::BindOnce(&MainWindow::OnRomLoaded,
                                       kiwi::base::Unretained(this_window),
                                       std::move(pending_rom)),
                  load_options);
            }
          },
          kiwi::base::Unretained(this), std::ref(rom), edition,
          load_from_finger_gesture));
}

#if BUILDFLAG(ENABLE_HD_TEXTURE)
void MainWindow::OnSetHDTextureRenderingEnabled(bool enabled) {
  if (hd_texture_available_ && canvas_->frame()->HasHDTextureRenderer()) {
    hd_texture_enabled_ = enabled;
    runtime_->emulator()->AsNESEmulator()->SetTextureMetadataCaptureEnabled(
        hd_texture_enabled_);
    canvas_->frame()->SetHDTextureRenderingEnabled(hd_texture_enabled_);
    SetTitle(current_game_title_ + (hd_texture_enabled_ ? " [HD]" : ""));
    SetHDTextureToggleState(canvas_->visible(), hd_texture_enabled_);
  }
}

void MainWindow::OnToggleHDTextureRendering() {
  OnSetHDTextureRenderingEnabled(!hd_texture_enabled_);
}
#endif

void MainWindow::OnLoadDebugROM(kiwi::base::FilePath rom_path) {
  LoadROMByPath(rom_path);
}

void MainWindow::OnToggleAudioEnabled() {
  const float volume = IsAudioEnabled() ? 0.f : 1.f;
  OnSetAudioVolume(volume);
}

void MainWindow::OnSetAudioVolume(float volume) {
  if (volume < 0)
    volume = 0;
  else if (volume > 1.f)
    volume = 1.f;

  runtime_->emulator()->SetVolume(volume);
  SetEffectVolume(volume);
  config_->data().volume = volume;
  config_->SaveConfig();

  for (auto* observer : observers_) {
    observer->OnVolumeChanged(volume);
  }
}

bool MainWindow::IsAudioEnabled() {
  return config_->data().volume > 0;
}

void MainWindow::OnToggleAudioChannelMasks(kiwi::nes::AudioChannel which_mask) {
  DebugPort* port = debug_port();
  if (port) {
    int current_mask = port->GetAudioChannelMasks();
    port->SetAudioChannelMasks(current_mask ^ which_mask);
  }
}

bool MainWindow::IsAudioChannelOn(kiwi::nes::AudioChannel which_mask) {
  DebugPort* port = debug_port();
  return port && (port->GetAudioChannelMasks() & which_mask);
}

void MainWindow::OnToggleRenderPaused() {
  DebugPort* port = debug_port();
  if (port) {
    port->set_render_paused(!port->render_paused());
  }
}

bool MainWindow::IsRenderPaused() {
  DebugPort* port = debug_port();
  return port && port->render_paused();
}

void MainWindow::OnSetFullscreen() {
  if (SDL_GetWindowFlags(native_window()) & SDL_WINDOW_FULLSCREEN)
    return;

  if (SDL_SetWindowFullscreen(native_window(), SDL_WINDOW_FULLSCREEN_DESKTOP) !=
      0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to enter fullscreen: %s",
                 SDL_GetError());
    return;
  }

  config_->data().is_fullscreen = true;
  config_->SaveConfig();
}

void MainWindow::OnUnsetFullscreen() {
  if (!(SDL_GetWindowFlags(native_window()) & SDL_WINDOW_FULLSCREEN))
    return;

  if (SDL_SetWindowFullscreen(native_window(), 0) != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to leave fullscreen: %s",
                 SDL_GetError());
    return;
  }

  config_->data().is_fullscreen = false;
  config_->SaveConfig();
}

void MainWindow::OnTogglePaletteWidget() {
  SDL_assert(palette_widget_);
  palette_widget_->set_visible(!palette_widget_->visible());
}

bool MainWindow::IsPaletteWidgetShown() {
  SDL_assert(palette_widget_);
  return palette_widget_->visible();
}

void MainWindow::OnTogglePatternWidget() {
  SDL_assert(pattern_widget_);
  pattern_widget_->set_visible(!pattern_widget_->visible());
}

bool MainWindow::IsPatternWidgetShown() {
  SDL_assert(pattern_widget_);
  return pattern_widget_->visible();
}

void MainWindow::OnPerformanceWidget() {
  SDL_assert(performance_widget_);
  performance_widget_->set_visible(!performance_widget_->visible());
}

bool MainWindow::IsPerformanceWidgetShown() {
  SDL_assert(performance_widget_);
  return performance_widget_->visible();
}

void MainWindow::OnDebugMemory() {
  memory_widget_->set_visible(true);
  memory_widget_->UpdateMemory();
}

void MainWindow::OnDebugDisassembly() {
  disassembly_widget_->set_visible(true);
  disassembly_widget_->UpdateDisassembly();
}

void MainWindow::OnDebugNametable() {
  nametable_widget_->set_visible(true);
}

void MainWindow::OnShowUiDemoWidget() {
  demo_widget_->set_visible(true);
}

void MainWindow::OnInGameMenuTrigger() {
  in_game_menu_->Show();
  OnPause();
}

#if BUILDFLAG(IS_SWITCH)
void MainWindow::OnQuickLoadTrigger() {
  in_game_menu_->ShowQuickLoad();
  OnPause();
}
#endif

void MainWindow::OnInGameMenuItemTrigger(
    const InGameMenu::MenuCommand& command) {
  switch (command.item) {
    case InGameMenu::MenuItem::kContinue: {
      CloseInGameMenu();
    } break;
    case InGameMenu::MenuItem::kLoadAutoSave: {
      const auto* timestamp =
          std::get_if<InGameMenu::AutoSaveTimestamp>(&command.payload);
      if (!timestamp)
        break;
      OnLoadAutoSavedState(timestamp->value, timestamp->discard_newer_states);
      CloseInGameMenu();
    } break;
    case InGameMenu::MenuItem::kLoadState: {
      const auto* slot = std::get_if<InGameMenu::StateSlot>(&command.payload);
      if (!slot)
        break;
      SDL_assert(slot->value < NESRuntime::MaxSaveStates);
      OnLoadState(slot->value);
      CloseInGameMenu();
    } break;
    case InGameMenu::MenuItem::kSaveState: {
      const auto* slot = std::get_if<InGameMenu::StateSlot>(&command.payload);
      if (!slot)
        break;
      SDL_assert(slot->value < NESRuntime::MaxSaveStates);
      OnSaveState(slot->value);
    } break;
    case InGameMenu::MenuItem::kResetGame: {
      OnResetROM();
      CloseInGameMenu();
    } break;
    case InGameMenu::MenuItem::kToGameSelection: {
      in_game_menu_->Close();
      OnBackToMainMenu();
    } break;
    default:
      break;
  }
}

void MainWindow::OnInGameSettingsItemTrigger(
    InGameMenu::SettingsItem item,
    InGameMenu::SettingsItemValue value) {
  bool* go_left_ptr = std::get_if<bool>(&value);
  float* value_ptr = std::get_if<float>(&value);

  switch (item) {
    case InGameMenu::SettingsItem::kVolume:
      if (go_left_ptr)
        OnInGameSettingsHandleVolume(*go_left_ptr);
      else
        OnSetAudioVolume(*value_ptr);
      break;
#if !BUILDFLAG(IS_SWITCH)
    case InGameMenu::SettingsItem::kWindowMode:
      SDL_assert(go_left_ptr);
      OnInGameSettingsHandleWindowMode(*go_left_ptr);
      break;
#endif
#if BUILDFLAG(IS_SWITCH)
    case InGameMenu::SettingsItem::kControllerSupport: {
      SDL_assert(go_left_ptr);
      if (!kiwi::switch_platform::ShowControllerSupport(1, 2))
        break;

      for (int player = 0; player < 2; ++player) {
        runtime_->joystick_mapping(player).which = nullptr;
      }
      if (!Application::Get()->ReloadGameControllers()) {
        SDL_LogError(SDL_LOG_CATEGORY_INPUT,
                     "Could not reload controllers after native selection: %s",
                     SDL_GetError());
        break;
      }
      UpdateGameControllerMapping();
    } break;
#else
    case InGameMenu::SettingsItem::kJoyP1:
    case InGameMenu::SettingsItem::kJoyP2: {
      SDL_assert(go_left_ptr);
      std::vector<SDL_GameController*> controllers = GetControllerList();
      int player_index = (item == InGameMenu::SettingsItem::kJoyP1 ? 0 : 1);
      // Find next controller
      auto iter = std::find(controllers.begin(), controllers.end(),
                            runtime_->joystick_mapping(player_index).which);
      if (iter == controllers.end())
        iter = controllers.begin();
      if (*go_left_ptr && iter != controllers.begin()) {
        SDL_GameController* next_controller = *(iter - 1);
        SetControllerMapping(runtime_.get(), player_index, next_controller,
                             false);
      } else if (!*go_left_ptr && (iter != controllers.end() - 1)) {
        SDL_GameController* next_controller = *(iter + 1);
        SetControllerMapping(runtime_.get(), player_index, next_controller,
                             false);
      }
    } break;
#endif
    case InGameMenu::SettingsItem::kSwapABP1:
    case InGameMenu::SettingsItem::kSwapABP2: {
      SDL_assert(go_left_ptr);
      const int player = item == InGameMenu::SettingsItem::kSwapABP1 ? 0 : 1;
      SetABSwapped(player, !*go_left_ptr);
    } break;
    case InGameMenu::SettingsItem::kLanguage: {
      SDL_assert(go_left_ptr);
      SupportedLanguage language = GetCurrentSupportedLanguage();
      if (*go_left_ptr) {
        language = static_cast<SupportedLanguage>(
            (static_cast<int>(language) - 1 < 0)
                ? static_cast<int>(SupportedLanguage::kMax) - 1
                : static_cast<int>(language) - 1);
      } else {
        language = static_cast<SupportedLanguage>(
            (static_cast<int>(language) + 1 >=
             static_cast<int>(SupportedLanguage::kMax))
                ? 0
                : static_cast<int>(language) + 1);
      }

      Application::Get()->SetLanguage(language);
    } break;
    default:
      break;
  }
}

#if !BUILDFLAG(IS_MOBILE)
void MainWindow::OnInGameSettingsHandleWindowMode(bool is_left) {
// Disable window settings. It should be handled by <canvas>.
#if !BUILDFLAG(IS_WASM)
  if (is_left)
    OnUnsetFullscreen();
  else
    OnSetFullscreen();
#endif
}

void MainWindow::OnInGameSettingsHandleVolume(bool is_left) {
  float volume = config_->data().volume;
  volume = (is_left ? volume - .1f : volume + .1f);
  OnSetAudioVolume(volume);
}

void MainWindow::OnInGameSettingsHandleVolume(const SDL_Rect& volume_bounds,
                                              const SDL_Point& trigger_point) {
  // Divide volume bar to `kSeparatedCount` segments
  constexpr int kSeparatedCount = 10;
  if (SDL_PointInRect(&trigger_point, &volume_bounds)) {
    float percentage = (trigger_point.x - volume_bounds.x) /
                       static_cast<float>(volume_bounds.w);
    OnSetAudioVolume(percentage);
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "%s: point is not in bounds, which is unexpected.", __func__);
  }
}
#endif

void MainWindow::SaveConfig() {
  // Before main window destruct, save current game index.
  // This happens when MainWindow is about to destroy, and has IO operation.
  if (!is_headless_ && game_library_view_) {
    const std::optional<int> current_index =
        game_library_view_->GetCurrentIndex();
    if (current_index) {
      config_->data().last_index = *current_index;
      config_->data().last_system = static_cast<int>(emulation_system_);
      config_->SaveConfig();
    }
  }
}

bool MainWindow::IsPause() {
  return runtime_->emulator()->IsPaused();
}
