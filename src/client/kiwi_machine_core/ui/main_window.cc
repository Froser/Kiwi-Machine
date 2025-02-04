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

#include <gflags/gflags.h>
#include <cmath>
#include <iostream>
#include <queue>

#include "build/kiwi_defines.h"
#include "debug/debug_roms.h"
#include "kiwi_flags.h"
#include "preset_roms/preset_roms.h"
#include "resources/image_resources.h"
#include "ui/application.h"
#include "ui/widgets/about_widget.h"
#include "ui/widgets/canvas.h"
#include "ui/widgets/card_widget.h"
#include "ui/widgets/demo_widget.h"
#include "ui/widgets/disassembly_widget.h"
#include "ui/widgets/flex_items_widget.h"
#include "ui/widgets/kiwi_bg_widget.h"
#include "ui/widgets/loading_widget.h"
#include "ui/widgets/memory_widget.h"
#include "ui/widgets/nametable_widget.h"
#include "ui/widgets/palette_widget.h"
#include "ui/widgets/pattern_widget.h"
#include "ui/widgets/performance_widget.h"
#include "ui/widgets/side_menu.h"
#include "ui/widgets/splash.h"
#include "ui/widgets/stack_widget.h"
#include "ui/widgets/toast.h"
#include "utility/algorithm.h"
#include "utility/audio_effects.h"
#include "utility/key_mapping_util.h"
#include "utility/localization.h"
#include "utility/math.h"
#include "utility/zip_reader.h"

DEFINE_bool(enable_debug, false, "Shows a menu bar at the top of the window.");

namespace {

MainWindow* g_main_window_instance = nullptr;

kiwi::nes::Bytes ReadFromRawBinary(const kiwi::nes::Byte* data,
                                   size_t data_size) {
  kiwi::nes::Bytes bytes;
  bytes.resize(data_size);
  memcpy(bytes.data(), data, data_size);
  return bytes;
}

constexpr int kWindowPadding = 50;
constexpr int kDefaultWindowWidth =
    Canvas::kNESFrameDefaultWidth + kWindowPadding * 2;
constexpr int kDefaultWindowHeight = Canvas::kNESFrameDefaultHeight;
constexpr int kDefaultFontSize = 15;
constexpr int kSideMenuAnimationMs = 50;
constexpr int kSplashTimeoutMs = 2000;

const kiwi::base::RepeatingCallback<bool()> kNoCheck =
    kiwi::base::RepeatingCallback<bool()>();

int GetDefaultMenuHeight() {
  return kDefaultFontSize + ImGui::GetStyle().FramePadding.y * 2;
}

void FillLayout(WindowBase* window, Widget* widget) {
  SDL_Rect client_bounds = window->GetClientBounds();
  widget->set_bounds(SDL_Rect{0, 0, client_bounds.w, client_bounds.h});
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

class SideMenuTitleStringUpdater : public LocalizedStringUpdater {
 public:
  explicit SideMenuTitleStringUpdater(preset_roms::Package* package);
  ~SideMenuTitleStringUpdater() override = default;

 protected:
  std::string GetLocalizedString() override;
  std::string GetCollateStringHint() override;
  bool IsTitleMatchedFilter(const std::string& filter,
                            int& similarity) override;

 private:
  preset_roms::Package* package_;
};

SideMenuTitleStringUpdater::SideMenuTitleStringUpdater(
    preset_roms::Package* package)
    : package_(package) {}

std::string SideMenuTitleStringUpdater::GetLocalizedString() {
  SDL_assert(package_);
  return package_->GetTitleForLanguage(GetCurrentSupportedLanguage());
}

std::string SideMenuTitleStringUpdater::GetCollateStringHint() {
  return GetLocalizedString();
}

bool SideMenuTitleStringUpdater::IsTitleMatchedFilter(const std::string& filter,
                                                      int& similarity) {
  // SideMenu won't call this function.
  SDL_assert(false);
  return false;
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

class StringUpdater : public LocalizedStringUpdater {
 public:
  explicit StringUpdater(int string_id);
  ~StringUpdater() override = default;

 protected:
  std::string GetLocalizedString() override;
  std::string GetCollateStringHint() override;
  bool IsTitleMatchedFilter(const std::string& filter,
                            int& similarity) override;

 private:
  int string_id_;
};

StringUpdater::StringUpdater(int string_id) : string_id_(string_id) {}

std::string StringUpdater::GetLocalizedString() {
  return ::GetLocalizedString(string_id_);
}

std::string StringUpdater::GetCollateStringHint() {
  return ::GetLocalizedString(string_id_);
}

bool StringUpdater::IsTitleMatchedFilter(const std::string& filter,
                                         int& similarity) {
  // StringUpdater won't call this function.
  SDL_assert(false);
  return false;
}

class ROMTitleUpdater : public LocalizedStringUpdater {
 public:
  explicit ROMTitleUpdater(const preset_roms::PresetROM& preset_rom);
  ~ROMTitleUpdater() override = default;

 protected:
  std::string GetLocalizedString() override;
  std::string GetCollateStringHint() override;
  bool IsTitleMatchedFilter(const std::string& filter,
                            int& similarity) override;

 private:
  const preset_roms::PresetROM& preset_rom_;
};

ROMTitleUpdater::ROMTitleUpdater(const preset_roms::PresetROM& preset_rom)
    : preset_rom_(preset_rom) {}

std::string ROMTitleUpdater::GetLocalizedString() {
  return GetROMLocalizedTitle(preset_rom_);
}

std::string ROMTitleUpdater::GetCollateStringHint() {
  return GetROMLocalizedCollateStringHint(preset_rom_);
}

bool ROMTitleUpdater::IsTitleMatchedFilter(const std::string& filter,
                                           int& similarity) {
  if (HasString(filter, preset_rom_.name)) {
    similarity = std::string_view(preset_rom_.name).size() - filter.size();
    return true;
  }

  std::string hint = language_conversion::KanaToRomaji(GetCollateStringHint());
  if (HasString(filter, hint)) {
    similarity = hint.size() - filter.size();
    return true;
  }

  return false;
}

}  // namespace

// A mask widget to handle finger events.
class FullscreenMask : public Widget {
 public:
  FullscreenMask(MainWindow* main_window);
  ~FullscreenMask() override = default;

 protected:
  bool OnTouchFingerDown(SDL_TouchFingerEvent* event) override;
  bool IsWindowless() override;

 private:
  MainWindow* main_window_ = nullptr;
};

FullscreenMask::FullscreenMask(MainWindow* main_window)
    : Widget(main_window), main_window_(main_window) {}

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

MainWindow::MainWindow(const std::string& title,
                       NESRuntimeID runtime_id,
                       scoped_refptr<NESConfig> config)
    : WindowBase(title,
                 CalculateWindowWidth(config->data().window_scale),
                 CalculateWindowHeight(config->data().window_scale)),
      config_(config),
      runtime_id_(runtime_id) {
#if KIWI_WASM
  // Only one main window instance should exist in WASM.
  SDL_assert(!g_main_window_instance);
  g_main_window_instance = this;
#endif
}

// Initialization flow:
// UI: InitializeRuntimeData -> Audio -> UI -> IODevices
// IO: ROMs data ------------------------^
void MainWindow::InitializeAsync(kiwi::base::OnceClosure callback) {
  InitializeRuntimeData();
  InitializeAudio();
  if (!FLAGS_enable_debug) {
    // If the main window has menu, it won't show splash for speeding up.
    ShowSplash(kiwi::base::DoNothing());
  }
  Application::Get()->Initialize(
      kiwi::base::BindOnce(&MainWindow::InitializeDebugROMsOnIOThread,
                           kiwi::base::Unretained(this)),
      kiwi::base::BindOnce(&MainWindow::InitializeUI,
                           kiwi::base::Unretained(this))
          .Then(kiwi::base::BindOnce(&MainWindow::InitializeIODevices,
                                     kiwi::base::Unretained(this)))
          .Then(kiwi::base::BindOnce(&MainWindow::LoadTestRomIfSpecified,
                                     kiwi::base::Unretained(this)))
          .Then(std::move(callback)));
}

MainWindow::~MainWindow() {
  SDL_assert(runtime_data_);
  SDL_assert(runtime_data_->emulator);
  SDL_assert(canvas_);
  runtime_data_->emulator->PowerOff();
  canvas_->RemoveObserver(this);
  SaveConfig();

#if KIWI_WASM
  g_main_window_instance = nullptr;
#endif
}

#if KIWI_WASM

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

void MainWindow::CallMenu_WASM() {
  if (in_game_menu_->visible())
    CloseInGameMenu();
  else
    OnInGameMenuTrigger();
}

#endif

#if !KIWI_MOBILE
bool MainWindow::IsLandscape() {
  return true;
}
#endif

void MainWindow::ShowSplash(kiwi::base::OnceClosure callback) {
#if !KIWI_IOS  // iOS has launch storyboard, so we don't need to show a splash
               // here.
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
    if (ms > kSplashTimeoutMs) {
      main_stack_widget_->set_visible(true);
      RemoveWidgetLater(splash_);
    } else {
      kiwi::base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          kiwi::base::BindOnce(&MainWindow::CloseSplash,
                               kiwi::base::Unretained(this)),
          kiwi::base::Milliseconds(kSplashTimeoutMs - ms));
    }
  } else {
    main_stack_widget_->set_visible(true);
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

void MainWindow::ChangeFocus(MainFocus focus) {
  SDL_assert(side_menu_);
  switch (focus) {
    case MainFocus::kContents:
      if (contents_card_widget_->HasWidgets()) {
        side_menu_->set_activate(false);
        SwitchToSideMenuByCurrentFlexItemWidget();
        for (auto* items_widget : items_widgets_) {
          items_widget->SetActivate(true);
        }
      }
      break;
    case MainFocus::kSideMenu:
      if (contents_card_widget_->HasWidgets()) {
        side_menu_->set_activate(true);
        for (auto* items_widget : items_widgets_) {
          items_widget->SetActivate(false);
        }
      }
      break;
    default:
      SDL_assert(false);  // Bad focus. Shouldn't be here.
  }
  FlexLayout();
}

void MainWindow::AddObserver(Observer* observer) {
  observers_.insert(observer);
}

void MainWindow::RemoveObserver(Observer* observer) {
  observers_.erase(observer);
}

bool MainWindow::IsKeyDown(int controller_id,
                           kiwi::nes::ControllerButton button) {
  // Matching virtual joysticks.
  bool matched = IsVirtualJoystickButtonPressed(controller_id, button);
  if (matched)
    return true;

  // Matching keyboard
  matched = pressing_keys_.find(runtime_data_->keyboard_mappings[controller_id]
                                    .mapping[static_cast<int>(button)]) !=
            pressing_keys_.cend();

  if (matched) {
    OnKeyboardMatched();
    return true;
  }

  // Key doesn't match. Try joysticks.
  NESRuntime::Data::JoystickMapping joystick_mapping =
      runtime_data_->joystick_mappings[controller_id];
  if (joystick_mapping.which) {
    // If mapping is available, cast it to game controller interface.
    SDL_GameController* game_controller =
        reinterpret_cast<SDL_GameController*>(joystick_mapping.which);

    // Unknown type may have wrong axis behaviour.
    if (SDL_GameControllerGetType(game_controller) ==
        SDL_CONTROLLER_TYPE_UNKNOWN)
      return false;

    matched = SDL_GameControllerGetButton(
        game_controller, static_cast<SDL_GameControllerButton>(
                             runtime_data_->joystick_mappings[controller_id]
                                 .mapping.mapping[static_cast<int>(button)]));

    if (!matched) {
      // Not matched, try axis motion.
      constexpr Sint16 kDeadZoom = SDL_JOYSTICK_AXIS_MAX / 3;
      switch (button) {
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

void MainWindow::OnControllerDeviceAdded(SDL_ControllerDeviceEvent* event) {
  ToastGameControllersAddedOrRemoved(this, true, event->which);
  UpdateGameControllerMapping();
}

void MainWindow::OnControllerDeviceRemoved(SDL_ControllerDeviceEvent* event) {
  ToastGameControllersAddedOrRemoved(this, false, event->which);
  UpdateGameControllerMapping();
}

void MainWindow::HandleResizedEvent() {
  if (main_stack_widget_) {
    FillLayout(this, main_stack_widget_);
  }

  if (side_menu_) {
    SDL_Rect client_bounds = GetClientBounds();
    FlexLayout();
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

  if (is_fullscreen()) {
    // Calculate fullscreen's frame scale, and set.
    SDL_Rect client_bounds = GetClientBounds();
    float scale = static_cast<float>(client_bounds.h) / kDefaultWindowWidth;
    if (config_->data().window_scale != scale) {
      config_->data().window_scale = scale;
      config_->SaveConfig();
      OnScaleChanged();
    }
  }

  WindowBase::HandleResizedEvent();
}

void MainWindow::HandleDisplayEvent(SDL_DisplayEvent* event) {
  // Display event changing treats as resizing.
  if (event->type == SDL_DISPLAYEVENT_ORIENTATION)
    HandleResizedEvent();
}

void MainWindow::HandleDropFileEvent(SDL_DropEvent* event) {
  if (FLAGS_enable_debug) {
    SDL_assert(event->file);
    LoadROMByPath(kiwi::base::FilePath::FromUTF8Unsafe(event->file));
  }
  SDL_free(event->file);
}

void MainWindow::Render() {
  WindowBase::Render();

  // Handles animations here
  if (side_menu_) {
    float percentage = side_menu_timer_.ElapsedInMilliseconds() /
                       static_cast<float>(kSideMenuAnimationMs);
    if (percentage >= 1.f)
      percentage = 1.f;

    int side_menu_width =
        Lerp(side_menu_original_width_, side_menu_target_width_, percentage);
    SDL_Rect side_menu_target_bounds =
        SDL_Rect{0, 0, side_menu_width, GetClientBounds().h};
    SDL_Rect side_menu_current_bounds = side_menu_->bounds();
    if (!SDL_RectEquals(&side_menu_target_bounds, &side_menu_current_bounds)) {
      side_menu_->set_bounds(side_menu_target_bounds);
      side_menu_->invalidate();
    }
  }

  render_done_ = true;
}

#if !KIWI_MOBILE
void MainWindow::OnAboutToRenderFrame(Canvas* canvas,
                                      scoped_refptr<NESFrame> frame) {
  // Always adjusts the canvas to the middle of the render area (excludes menu
  // bar).
  SDL_Rect render_bounds = GetClientBounds();

  if (!is_fullscreen()) {
    SDL_Rect src_rect = {0, 0, frame->width(), frame->height()};
    SDL_Rect dest_rect = {
        static_cast<int>(
            (render_bounds.w - src_rect.w * canvas->frame_scale()) / 2) +
            render_bounds.x,
        static_cast<int>(
            (render_bounds.h - src_rect.h * canvas->frame_scale()) / 2) +
            render_bounds.y,
        static_cast<int>(frame->width() * canvas->frame_scale()),
        static_cast<int>(frame->height() * canvas->frame_scale())};
    canvas->set_bounds(dest_rect);
  } else {
    bool horizontal_screen = render_bounds.w > render_bounds.h;
    int dest_width, dest_height;
    if (horizontal_screen) {
      dest_height = render_bounds.h;
      dest_width = static_cast<float>(Canvas::kNESFrameDefaultWidth) /
                   Canvas::kNESFrameDefaultHeight * dest_height;
    } else {
      dest_width = render_bounds.w;
      dest_height = static_cast<float>(Canvas::kNESFrameDefaultHeight) /
                    Canvas::kNESFrameDefaultWidth * dest_width;
    }

    SDL_Rect dest_rect = {
        static_cast<int>((render_bounds.w - dest_width) / 2) + render_bounds.x,
        static_cast<int>((render_bounds.h - dest_height) / 2) + render_bounds.y,
        dest_width, dest_height};
    canvas->set_bounds(dest_rect);
  }
}
#endif

void MainWindow::InitializeRuntimeData() {
  SDL_assert(runtime_id_ >= 0);
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id_);

  if (FLAGS_enable_debug) {
    // If menu is visible, debug will be enabled as well. Otherwise, there's no
    // entry to debug namespace, memory, and disassembly. In this scenario,
    // there's no need to set a debug port.
    runtime_data_->emulator->SetDebugPort(runtime_data_->debug_port.get());

    SDL_assert(runtime_data_->debug_port);
    runtime_data_->debug_port->set_on_breakpoint_callback(
        kiwi::base::BindRepeating(&MainWindow::OnPause,
                                  kiwi::base::Unretained(this)));
  } else {
    // If there's no menu, disable text input.
    SDL_StopTextInput();
  }
}

void MainWindow::ResetAudio() {
  audio_->Reset();
  if (runtime_data_->emulator && runtime_data_->emulator->GetIODevices()) {
    runtime_data_->emulator->GetIODevices()->set_audio_device(audio_.get());
  }
}

void MainWindow::InitializeAudio() {
  SDL_assert(!audio_);
  audio_ = std::make_unique<NESAudio>(runtime_id_);
  audio_->Initialize();
  audio_->Start();
  OnSetAudioVolume(config_->data().volume);
}

void MainWindow::InitializeUI() {
  ScopedDisableEffect scoped_disable_effect;
#if BUILDFLAG(IS_WASM)
  is_headless_ = preset_roms::GetPresetOrTestRomsPackages().empty();
#else
  is_headless_ = false;
#endif

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
      std::make_unique<KiwiBgWidget>(this, runtime_id_);
  bg_widget_ = bg_widget.get();
  main_stack_widget_->PushWidget(std::move(bg_widget));

  if (!is_headless_) {
    // Contents stack widget
    std::unique_ptr<CardWidget> contents_card_widget =
        std::make_unique<CardWidget>(this);
    contents_card_widget_ = contents_card_widget.get();
    contents_card_widget_->set_bounds(client_bounds);
    bg_widget_->AddWidget(std::move(contents_card_widget));

    // Main game items
    const auto& packages = preset_roms::GetPresetOrTestRomsPackages();
    for (const auto& package : packages) {
      std::unique_ptr<FlexItemsWidget> items_widget =
          std::make_unique<FlexItemsWidget>(this, runtime_id_);
      items_widgets_.push_back(items_widget.get());
      items_widget->set_back_callback(kiwi::base::BindRepeating(
          &MainWindow::ChangeFocus, kiwi::base::Unretained(this),
          MainWindow::MainFocus::kSideMenu));
      SDL_assert(package->GetRomsCount() > 0);
      for (size_t i = 0; i < package->GetRomsCount(); ++i) {
        std::vector<preset_roms::PresetROM*> roms;

        auto& rom = package->GetRomsByIndex(i);
        roms.push_back(&rom);
        for (auto& alternative_rom : rom.alternates) {
          roms.push_back(&alternative_rom);
        }

        // Sorts ROMS by current locale
        auto priority_rom = std::find_if(
            roms.begin(), roms.end(), [](preset_roms::PresetROM* r) {
              SupportedLanguage current_language =
                  GetCurrentSupportedLanguage();
              if (current_language == SupportedLanguage::kEnglish &&
                  r->region == preset_roms::Region::kUSA)
                return true;

#if !DISABLE_JAPANESE_FONT
              if (current_language == SupportedLanguage::kJapanese &&
                  r->region == preset_roms::Region::kJapan)
                return true;
#endif

#if !DISABLE_CHINESE_FONT
              if (current_language == SupportedLanguage::kSimplifiedChinese &&
                  r->region == preset_roms::Region::kCN)
                return true;

              // Chinese matches USA version .
              if (current_language == SupportedLanguage::kSimplifiedChinese &&
                  r->region == preset_roms::Region::kUSA)
                return true;
#endif

              return false;
            });

        if (priority_rom != roms.end()) {
          preset_roms::PresetROM& target_rom = **priority_rom;
          size_t main_item_index = items_widget->AddItem(
              std::make_unique<ROMTitleUpdater>(target_rom),
              target_rom.rom_cover.data(), target_rom.rom_cover.size(),
              kiwi::base::BindRepeating(&MainWindow::OnLoadPresetROM,
                                        kiwi::base::Unretained(this),
                                        std::ref(target_rom)));
          // Removes the first rom, and puts remaining roms to the alternative
          // rom's list.
          roms.erase(priority_rom);
          for (auto* alternative_rom : roms) {
            items_widget->AddSubItem(
                main_item_index,
                std::make_unique<ROMTitleUpdater>(*alternative_rom),
                alternative_rom->rom_cover.data(),
                alternative_rom->rom_cover.size(),
                kiwi::base::BindRepeating(&MainWindow::OnLoadPresetROM,
                                          kiwi::base::Unretained(this),
                                          std::ref(*alternative_rom)));
          }

        } else {
          // No priority rom found. Uses default order.
          size_t main_item_index = items_widget->AddItem(
              std::make_unique<ROMTitleUpdater>(rom), rom.rom_cover.data(),
              rom.rom_cover.size(),
              kiwi::base::BindRepeating(&MainWindow::OnLoadPresetROM,
                                        kiwi::base::Unretained(this),
                                        std::ref(rom)));

          for (auto& alternative_rom : rom.alternates) {
            items_widget->AddSubItem(
                main_item_index,
                std::make_unique<ROMTitleUpdater>(alternative_rom),
                alternative_rom.rom_cover.data(),
                alternative_rom.rom_cover.size(),
                kiwi::base::BindRepeating(&MainWindow::OnLoadPresetROM,
                                          kiwi::base::Unretained(this),
                                          std::ref(alternative_rom)));
          }
        }
      }

      contents_card_widget_->AddWidget(std::move(items_widget));
    }

    if (FlexItemsWidget* main_items_widget = GetMainItemsWidget();
        main_items_widget) {
      int main_items_index =
          std::clamp(config_->data().last_index, 0,
                     static_cast<int>(main_items_widget->size() - 1));
      main_items_widget->SetIndex(main_items_index);
    }

    // Side menu
    std::unique_ptr<SideMenu> side_menu =
        std::make_unique<SideMenu>(this, runtime_id_);
    if (preset_roms::GetPresetOrTestRomsPackages().empty()) {
      // If there's no game package, show a default background.
      // There's no game selection menu, so we won't trigger the first menu
      // item, because the first menu item will be a settings menu.
      side_menu->set_auto_trigger_first_item(false);
    }

    side_menu->set_activate(true);

    size_t package_index = 0;
    for (auto* items_widget : items_widgets_) {
      preset_roms::Package* package =
          preset_roms::GetPresetOrTestRomsPackages()[package_index];
      side_menu->AddMenu(
          std::make_unique<SideMenuTitleStringUpdater>(package),
          package->GetSideMenuImage(), package->GetSideMenuHighlightImage(),
          CreateMenuChangeFocusToGameItemsCallbacks(items_widget));
      package_index++;
    }
    side_menu->AddMenu(std::make_unique<StringUpdater>(
                           string_resources::IDR_SIDE_MENU_SETTINGS),
                       image_resources::ImageID::kMenuSettings,
                       image_resources::ImageID::kMenuSettingsHighlight,
                       CreateMenuSettingsCallbacks());
    side_menu->AddMenu(
        std::make_unique<StringUpdater>(string_resources::IDR_SIDE_MENU_ABOUT),
        image_resources::ImageID::kMenuAbout,
        image_resources::ImageID::kMenuAboutHighlight,
        CreateMenuAboutCallbacks());

#if !KIWI_MOBILE
    // Mobile apps needn't quit the application manually.
    SideMenu::MenuCallbacks quit_callbacks;
    quit_callbacks.trigger_callback = kiwi::base::BindRepeating(
        [](MainWindow* this_window, int) { this_window->OnQuit(); },
        kiwi::base::Unretained(this));
    side_menu->AddMenu(
        std::make_unique<StringUpdater>(string_resources::IDR_SIDE_MENU_QUIT),
        image_resources::ImageID::kMenuQuit,
        image_resources::ImageID::kMenuQuitHighlight, quit_callbacks);
#endif

    {
      SideMenu::ButtonCallbacks button_callbacks = {
          // Callback when search button is triggered
          kiwi::base::BindRepeating(
              &MainWindow::ChangeFocusToCurrentSideMenuAndShowFilter,
              kiwi::base::Unretained(this))};
      side_menu->AddButton(std::make_unique<StringUpdater>(
                               string_resources::IDR_SIDE_MENU_SEARCH),
                           image_resources::ImageID::kMenuSearch,
                           button_callbacks, SDLK_f);
    }

    side_menu->Layout();
    side_menu_ = side_menu.get();
    bg_widget_->AddWidget(std::move(side_menu));
    ChangeFocus(MainFocus::kContents);
  }

  // Create virtual touch buttons, which will invoke main items_widget's
  // methods.
  CreateVirtualTouchButtons();
  LayoutVirtualTouchButtons();

  std::unique_ptr<Canvas> canvas = std::make_unique<Canvas>(this, runtime_id_);
  canvas_ = canvas.get();
  canvas_->set_visible(false);
  canvas_->AddObserver(this);
  canvas_->set_frame_scale(2.f);
  canvas_->set_in_menu_trigger_callback(kiwi::base::BindRepeating(
      &MainWindow::OnInGameMenuTrigger, kiwi::base::Unretained(this)));
  AddWidget(std::move(canvas));

  std::unique_ptr<InGameMenu> in_game_menu = std::make_unique<InGameMenu>(
      this, runtime_id_,
      kiwi::base::BindRepeating(&MainWindow::OnInGameMenuItemTrigger,
                                kiwi::base::Unretained(this)),
      kiwi::base::BindRepeating(&MainWindow::OnInGameSettingsItemTrigger,
                                kiwi::base::Unretained(this)));

  std::unique_ptr<Widget> fullscreen_mask =
      std::make_unique<FullscreenMask>(this);
  fullscreen_mask_ = fullscreen_mask.get();
  fullscreen_mask_->set_visible(false);
  AddWidget(std::move(fullscreen_mask));

  if (is_headless_)
    in_game_menu->HideMenu(6);  // Hides 'Quit Game' when headless
  in_game_menu_ = in_game_menu.get();
  in_game_menu_->set_visible(false);
  AddWidget(std::move(in_game_menu));

  // Loading spinning widget.
  std::unique_ptr<LoadingWidget> loading_widget =
      std::make_unique<LoadingWidget>(this);
  loading_widget_ = loading_widget.get();
  loading_widget_->set_visible(false);
  AddWidget(std::move(loading_widget));

  // Debug widgets
  if (FLAGS_enable_debug) {
    std::unique_ptr<PaletteWidget> palette_widget =
        std::make_unique<PaletteWidget>(this, runtime_data_->debug_port.get());
    palette_widget_ = palette_widget.get();
    palette_widget_->set_visible(false);
    AddWidget(std::move(palette_widget));

    std::unique_ptr<PatternWidget> pattern_widget =
        std::make_unique<PatternWidget>(this, runtime_data_->debug_port.get());
    pattern_widget_ = pattern_widget.get();
    pattern_widget_->set_visible(false);
    AddWidget(std::move(pattern_widget));

    std::unique_ptr<PerformanceWidget> performance_widget =
        std::make_unique<PerformanceWidget>(this, canvas_->frame(),
                                            runtime_data_->debug_port.get());
    performance_widget_ = performance_widget.get();
    performance_widget_->set_visible(false);
    AddWidget(std::move(performance_widget));

    std::unique_ptr<MemoryWidget> memory_widget =
        std::make_unique<MemoryWidget>(
            this, runtime_id_,
            kiwi::base::BindRepeating(&MainWindow::OnTogglePause,
                                      kiwi::base::Unretained(this)),
            kiwi::base::BindRepeating(&MainWindow::IsPause,
                                      kiwi::base::Unretained(this)));
    memory_widget_ = memory_widget.get();
    memory_widget_->set_visible(false);
    AddWidget(std::move(memory_widget));

    std::unique_ptr<DisassemblyWidget> disassembly_widget =
        std::make_unique<DisassemblyWidget>(
            this, runtime_id_,
            kiwi::base::BindRepeating(&MainWindow::OnTogglePause,
                                      kiwi::base::Unretained(this)),
            kiwi::base::BindRepeating(&MainWindow::IsPause,
                                      kiwi::base::Unretained(this)));
    disassembly_widget_ = disassembly_widget.get();
    disassembly_widget_->set_visible(false);
    AddWidget(std::move(disassembly_widget));

    std::unique_ptr<NametableWidget> nametable_widget =
        std::make_unique<NametableWidget>(this, runtime_id_);
    nametable_widget_ = nametable_widget.get();
    nametable_widget_->set_visible(false);
    AddWidget(std::move(nametable_widget));

    std::unique_ptr<Widget> demo_widget = std::make_unique<DemoWidget>(this);
    demo_widget_ = demo_widget.get();
    demo_widget_->set_visible(false);
    AddWidget(std::move(demo_widget));
  }

#if !KIWI_MOBILE
  OnScaleChanged();
  HandleResizedEvent();
  if (is_fullscreen())
    OnSetFullscreen();
#else
  OnScaleModeChanged();
#endif

  CloseSplash();
}

void MainWindow::InitializeIODevices() {
  SDL_assert(runtime_data_);
  SDL_assert(runtime_data_->emulator);
  SDL_assert(canvas_);
  std::unique_ptr<kiwi::nes::IODevices> io_devices =
      std::make_unique<kiwi::nes::IODevices>();
  io_devices->set_input_device(this);
  io_devices->add_render_device(canvas_->render_device());
  io_devices->set_audio_device(audio_.get());
  runtime_data_->emulator->SetIODevices(std::move(io_devices));
}

void MainWindow::InitializeDebugROMsOnIOThread() {
#if ENABLE_DEBUG_ROMS
  if (HasDebugRoms()) {
    debug_roms_ = CreateDebugRomsMenu(kiwi::base::BindRepeating(
        &MainWindow::OnLoadDebugROM, kiwi::base::Unretained(this)));
  }
#endif
}

void MainWindow::LoadTestRomIfSpecified() {
  if (!FLAGS_test_rom.empty()) {
    if (FLAGS_enable_debug) {
      LoadROMByPath(kiwi::base::FilePath::FromUTF8Unsafe(FLAGS_test_rom));
    }
  }
}

void MainWindow::LoadROMByPath(kiwi::base::FilePath rom_path) {
  SDL_assert(runtime_data_->emulator);
  SetLoading(true);

  runtime_data_->emulator->LoadAndRun(
      rom_path, kiwi::base::BindOnce(
                    &MainWindow::OnRomLoaded, kiwi::base::Unretained(this),
                    rom_path.BaseName().AsUTF8Unsafe(), false));
}

void MainWindow::StartAutoSave() {
  constexpr int kAutoSaveTimeDelta = 5000;
  runtime_data_->StartAutoSave(
      kiwi::base::Milliseconds(kAutoSaveTimeDelta),
      kiwi::base::BindRepeating(&NESFrame::GetLastFrame, canvas_->frame()));
}

void MainWindow::StopAutoSave() {
  SDL_assert(runtime_data_);
  runtime_data_->StopAutoSave();
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

    // Screen size
    {
      MenuBar::MenuItem screen_size;
      screen_size.title = "Screen size";
      for (int i = 2; i <= 4; ++i) {
        screen_size.sub_items.push_back(
            {kiwi::base::NumberToString(i) + "x",
             kiwi::base::BindRepeating(&MainWindow::OnSetScreenScale,
                                       kiwi::base::Unretained(this), i),
             kiwi::base::BindRepeating(&MainWindow::ScreenScaleIs,
                                       kiwi::base::Unretained(this), i)});
      }
      emulator.menu_items.push_back(std::move(screen_size));
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
           kiwi::base::BindRepeating(&MainWindow::CanSaveOrLoadState,
                                     kiwi::base::Unretained(this))});
      states.sub_items.push_back(
          {"Load state",
           kiwi::base::BindRepeating(&MainWindow::OnLoadState,
                                     kiwi::base::Unretained(this), 0),
           kNoCheck,
           kiwi::base::BindRepeating(&MainWindow::CanSaveOrLoadState,
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
      };
      debug.menu_items.push_back(std::move(disable_render));
    }

#if ENABLE_DEBUG_ROMS
    if (!debug_roms_.sub_items.empty())
      debug.menu_items.push_back(std::move(debug_roms_));
      // debug_roms_ should no longer use.
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
  SDL_assert(bg_widget_);
  SDL_assert(loading_widget_);
  bg_widget_->SetLoading(is_loading);
  loading_widget_->set_visible(is_loading);
}

void MainWindow::ShowMainMenu(bool show, bool load_from_finger_gesture) {
  SDL_assert(bg_widget_);
  SDL_assert(canvas_);
  canvas_->set_visible(!show);
  fullscreen_mask_->set_visible(!show);
  bg_widget_->set_visible(show);

  if (!load_from_finger_gesture) {
    // If the ROM is not loaded by finger events (Finger Up), it means we have
    // mouse, joysticks or keyboard, so we do not show virtual buttons here.
    SetVirtualButtonsVisible(false);
  } else {
    SetVirtualButtonsVisible(!show);
  }
  SetLoading(false);
}

void MainWindow::OnScaleChanged() {
  if (!is_fullscreen()) {
    Resize(CalculateWindowWidth(window_scale()),
           CalculateWindowHeight(window_scale(), menu_bar_));
  }

  if (canvas_)
    canvas_->set_frame_scale(window_scale());
}

void MainWindow::UpdateGameControllerMapping() {
  const auto& game_controllers = Application::Get()->game_controllers();
  int index = 0;
  for (auto* game_controller : game_controllers) {
    // If one's controller is already set, we don't change it.
    if (runtime_data_->joystick_mappings[0].which != game_controller &&
        runtime_data_->joystick_mappings[1].which != game_controller) {
      SetControllerMapping(runtime_data_, index++, game_controller, false);
    }
    if (index >= 2)
      break;
  }

  // If any game controller is removed, remove it from joystick mapping as well.
  if (std::find(game_controllers.begin(), game_controllers.end(),
                reinterpret_cast<SDL_GameController*>(
                    runtime_data_->joystick_mappings[0].which)) ==
      game_controllers.end()) {
    runtime_data_->joystick_mappings[0].which = nullptr;
  }
  if (std::find(game_controllers.begin(), game_controllers.end(),
                reinterpret_cast<SDL_GameController*>(
                    runtime_data_->joystick_mappings[1].which)) ==
      game_controllers.end()) {
    runtime_data_->joystick_mappings[1].which = nullptr;
  }
}

#if !KIWI_MOBILE
void MainWindow::CreateVirtualTouchButtons() {}

void MainWindow::SetVirtualTouchButtonVisible(VirtualTouchButton button,
                                              bool visible) {}

void MainWindow::LayoutVirtualTouchButtons() {}

void MainWindow::OnVirtualJoystickChanged(int state) {}

void MainWindow::OnKeyboardMatched() {}

void MainWindow::OnJoystickButtonsMatched() {}

bool MainWindow::HandleWindowFingerDown() {
  return false;
}

void MainWindow::StashVirtualButtonsVisible() {}

void MainWindow::PopVirtualButtonsVisible() {}

#endif

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

void MainWindow::CloseInGameMenu() {
  OnResume();
  in_game_menu_->Close();
}

void MainWindow::FlexLayout() {
  SDL_Rect client_bounds = GetClientBounds();
  side_menu_timer_.Reset();
  int left_width = side_menu_->GetSuggestedCollapsedWidth();
  int right_width = client_bounds.w - left_width;

  // An activated side menu needs more space.
  int extended_width =
      client_bounds.w * .15f < side_menu_->GetMinExtendedWidth()
          ? side_menu_->GetMinExtendedWidth()
          : client_bounds.w * .15f;

  if (side_menu_->activate()) {
    side_menu_original_width_ = left_width;
    side_menu_target_width_ = extended_width;
  } else {
    side_menu_original_width_ = extended_width;
    side_menu_target_width_ = left_width;
  }

  contents_card_widget_->set_bounds(
      SDL_Rect{left_width, 0, right_width, client_bounds.h});
}

FlexItemsWidget* MainWindow::GetMainItemsWidget() {
  if (!items_widgets_.empty())
    return items_widgets_[0];

  return nullptr;
}

SideMenu::MenuCallbacks MainWindow::CreateMenuSettingsCallbacks() {
  SideMenu::MenuCallbacks callbacks;
  callbacks.trigger_callback = kiwi::base::BindRepeating(
      [](MainWindow* window, NESRuntimeID runtime_id, int) {
        std::unique_ptr<InGameMenu> in_game_menu = std::make_unique<InGameMenu>(
            window, runtime_id,
            kiwi::base::BindRepeating(
                [](StackWidget* stack_widget, InGameMenu::MenuItem item,
                   int param) {
                  // Mapping button 'B' will trigger kContinue.
                  if (item == InGameMenu::MenuItem::kToGameSelection ||
                      item == InGameMenu::MenuItem::kContinue) {
                    stack_widget->PopWidget();
                  }
                },
                window->main_stack_widget_),
            kiwi::base::BindRepeating(&MainWindow::OnInGameSettingsItemTrigger,
                                      kiwi::base::Unretained(window)));
        in_game_menu->HideMenu(0);  // Hides 'Continue'
        in_game_menu->HideMenu(1);  // Hides 'Load Auto Save'
        in_game_menu->HideMenu(2);  // Hides 'Load State'
        in_game_menu->HideMenu(3);  // Hides 'Save State'
        in_game_menu->HideMenu(5);  // Hides 'Reset Game'
        in_game_menu->set_bounds(
            SDL_Rect{0, 0, window->main_stack_widget_->bounds().w,
                     window->main_stack_widget_->bounds().h});
        window->main_stack_widget_->PushWidget(std::move(in_game_menu));
      },
      this, runtime_id_);
  return callbacks;
}

SideMenu::MenuCallbacks MainWindow::CreateMenuAboutCallbacks() {
  SideMenu::MenuCallbacks callbacks;
  callbacks.trigger_callback = kiwi::base::BindRepeating(
      [](MainWindow* window, NESRuntimeID runtime_id, int) {
        std::unique_ptr<AboutWidget> about_widget =
            std::make_unique<AboutWidget>(window, window->main_stack_widget_,
                                          runtime_id);
        about_widget->set_bounds(
            SDL_Rect{0, 0, window->main_stack_widget_->bounds().w,
                     window->main_stack_widget_->bounds().h});
        window->main_stack_widget_->PushWidget(std::move(about_widget));
      },
      this, runtime_id_);
  return callbacks;
}

SideMenu::MenuCallbacks MainWindow::CreateMenuChangeFocusToGameItemsCallbacks(
    FlexItemsWidget* items_widget) {
  SDL_assert(items_widget);
  SideMenu::MenuCallbacks callbacks;
  callbacks.trigger_callback = kiwi::base::BindRepeating(
      [](MainWindow* this_window, FlexItemsWidget* items_widget,
         int menu_index) {
        this_window->flex_items_map_[menu_index] = items_widget;
        this_window->SwitchToWidgetForSideMenu(menu_index);
      },
      kiwi::base::Unretained(this), kiwi::base::Unretained(items_widget));
  callbacks.enter_callback = kiwi::base::BindRepeating(
      &MainWindow::ChangeFocus, kiwi::base::Unretained(this),
      MainFocus::kContents);

  return callbacks;
}

void MainWindow::SwitchToWidgetForSideMenu(int menu_index) {
  SDL_assert(contents_card_widget_);
  SDL_assert(flex_items_map_[menu_index]);
  bool succeeded =
      contents_card_widget_->SetCurrentWidget(flex_items_map_[menu_index]);
  SDL_assert(succeeded);
}

void MainWindow::SwitchToSideMenuByCurrentFlexItemWidget() {
  SDL_assert(contents_card_widget_);
  Widget* current_widget = contents_card_widget_->GetCurrentWidget();
  SDL_assert(current_widget);
  int target_index = -1;
  for (auto [side_menu_index, widget] : flex_items_map_) {
    if (widget == current_widget) {
      target_index = side_menu_index;
      break;
    }
  }
  if (target_index == -1) {
    SDL_assert(false);  // Shouldn't be here
    target_index = 0;
  }
  side_menu_->SetIndex(target_index);
}

void MainWindow::ChangeFocusToCurrentSideMenuAndShowFilter() {
  // A test-rom commandline will have no side menu items.
  if (flex_items_map_.find(side_menu_->current_index()) !=
      flex_items_map_.end()) {
    ChangeFocus(MainFocus::kContents);
    flex_items_map_[side_menu_->current_index()]->ShowFilterWidget();
  }
}

void MainWindow::OnRomLoaded(const std::string& name,
                             bool load_from_finger_gesture,
                             bool success) {
  SetLoading(false);
  ShowMainMenu(false, load_from_finger_gesture);
  SetTitle(name);
  StartAutoSave();

  if (!success) {
    Toast::ShowToast(this,
                     GetLocalizedString(
                         string_resources::IDR_MAIN_WINDOW_ROM_NOT_SUPPORTED));
  }
}

void MainWindow::OnQuit() {
  SDL_Event quit_event;
  quit_event.type = SDL_QUIT;
  SDL_PushEvent(&quit_event);
}

void MainWindow::OnResetROM() {
  SDL_assert(runtime_data_->emulator);
  runtime_data_->emulator->Reset(kiwi::base::BindOnce(
      &MainWindow::ResetAudio, kiwi::base::Unretained(this)));
}

void MainWindow::OnBackToMainMenu() {
  // Unload ROM, and show main menu.
  SetTitle("Kiwi Machine");
  SetLoading(true);
  StopAutoSave();

  SDL_assert(runtime_data_->emulator);
  runtime_data_->emulator->Unload(
      kiwi::base::BindRepeating(&MainWindow::ShowMainMenu,
                                kiwi::base::Unretained(this), true, false)
          .Then(kiwi::base::BindRepeating(
              &LoadingWidget::set_visible,
              kiwi::base::Unretained(loading_widget_), false))
          .Then(kiwi::base::BindRepeating(&Canvas::Clear,
                                          kiwi::base::Unretained(canvas_))));
}

void MainWindow::OnSaveState(int which_state) {
  SDL_assert(runtime_data_->emulator);
  runtime_data_->emulator->SaveState(kiwi::base::BindOnce(
      [](MainWindow* window, NESRuntime::Data* runtime_data, int which_state,
         kiwi::nes::Bytes data) {
        SDL_assert(which_state < NESRuntime::Data::MaxSaveStates);
        auto* rom_data = runtime_data->emulator->GetRomData();
        SDL_assert(rom_data);
        if (!data.empty()) {
          runtime_data->SaveState(
              rom_data->crc, which_state, data,
              window->canvas_->frame()->GetLastFrame(),
              kiwi::base::BindOnce(&MainWindow::OnStateSaved,
                                   kiwi::base::Unretained(window)));
        } else {
          window->OnStateSaved(false);
        }
      },
      this, runtime_data_, which_state));
}

void MainWindow::OnLoadState(int which_state) {
  SDL_assert(which_state < NESRuntime::Data::MaxSaveStates);
  auto* rom_data = runtime_data_->emulator->GetRomData();
  if (rom_data) {
    runtime_data_->GetState(runtime_data_->emulator->GetRomData()->crc,
                            which_state,
                            kiwi::base::BindOnce(&MainWindow::OnStateLoaded,
                                                 kiwi::base::Unretained(this)));
  } else {
    NESRuntime::Data::StateResult failed_result{false};
    OnStateLoaded(failed_result);
  }
}

void MainWindow::OnLoadAutoSavedState(int timestamp) {
  auto* rom_data = runtime_data_->emulator->GetRomData();
  if (rom_data) {
    runtime_data_->GetAutoSavedStateByTimestamp(
        runtime_data_->emulator->GetRomData()->crc, timestamp,
        kiwi::base::BindOnce(&MainWindow::OnStateLoaded,
                             kiwi::base::Unretained(this)));
  } else {
    NESRuntime::Data::StateResult failed_result{false};
    OnStateLoaded(failed_result);
  }
}

void MainWindow::OnStateSaved(bool succeed) {
  using namespace string_resources;

  if (succeed) {
    SDL_assert(in_game_menu_);
    in_game_menu_->RequestCurrentThumbnail();
    Toast::ShowToast(this, GetLocalizedString(IDR_MAIN_WINDOW_SAVE_SUCCEEDED));
  } else {
    Toast::ShowToast(this, GetLocalizedString(IDR_MAIN_WINDOW_SAVE_FAILED));
  }
}

void MainWindow::OnStateLoaded(
    const NESRuntime::Data::StateResult& state_result) {
  using namespace string_resources;

  if (state_result.success && !state_result.state_data.empty()) {
    audio_->Reset();
    runtime_data_->emulator->LoadState(
        state_result.state_data,
        kiwi::base::BindOnce(
            [](MainWindow* window, bool success) {
              if (success)
                Toast::ShowToast(
                    window, GetLocalizedString(IDR_MAIN_WINDOW_LOAD_SUCCEEDED));
              else
                Toast::ShowToast(
                    window, GetLocalizedString(IDR_MAIN_WINDOW_LOAD_FAILED));
            },
            this));
    audio_->Start();
  }
}

bool MainWindow::CanSaveOrLoadState() {
  SDL_assert(runtime_data_->emulator);
  return runtime_data_->emulator->GetRunningState() !=
         kiwi::nes::Emulator::RunningState::kStopped;
}

void MainWindow::OnTogglePause() {
  SDL_assert(runtime_data_->emulator);
  if (IsPause()) {
    OnResume();
  } else {
    OnPause();
  }
}

void MainWindow::OnPause() {
  StopAutoSave();
  // Cleanup all pressing keys when pausing.
  pressing_keys_.clear();
  runtime_data_->emulator->Pause();
  if (memory_widget_)
    memory_widget_->UpdateMemory();
  if (disassembly_widget_)
    disassembly_widget_->UpdateDisassembly();
  StashVirtualButtonsVisible();
  SetVirtualButtonsVisible(false);
}

void MainWindow::OnResume() {
  runtime_data_->emulator->Run();
  PopVirtualButtonsVisible();
  StartAutoSave();
}

void MainWindow::OnLoadPresetROM(preset_roms::PresetROM& rom,
                                 bool load_from_finger_gesture) {
  SDL_assert(runtime_data_->emulator);
  SetLoading(true);

  Application::Get()->GetIOTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      kiwi::base::BindOnce(&LoadPresetROM, std::ref(rom), RomPart::kContent),
      kiwi::base::BindOnce(
          [](MainWindow* this_window,
             scoped_refptr<kiwi::nes::Emulator> emulator,
             preset_roms::PresetROM& rom, bool load_from_finger_gesture) {
            emulator->LoadAndRun(
                ReadFromRawBinary(rom.rom_data.data(), rom.rom_data.size()),
                kiwi::base::BindOnce(&MainWindow::OnRomLoaded,
                                     kiwi::base::Unretained(this_window),
                                     GetROMLocalizedTitle(rom),
                                     load_from_finger_gesture));
          },
          kiwi::base::Unretained(this),
          kiwi::base::RetainedRef(runtime_data_->emulator), std::ref(rom),
          load_from_finger_gesture));
}

void MainWindow::OnLoadDebugROM(kiwi::base::FilePath rom_path) {
  LoadROMByPath(rom_path);
}

void MainWindow::OnToggleAudioEnabled() {
  SDL_assert(runtime_data_->emulator);
  runtime_data_->emulator->SetVolume(IsAudioEnabled() ? 0 : 1);
  SetEffectVolume(IsAudioEnabled() ? 0 : 1);
}

void MainWindow::OnSetAudioVolume(float volume) {
  if (volume < 0)
    volume = 0;
  else if (volume > 1.f)
    volume = 1.f;

  SDL_assert(runtime_data_->emulator);
  runtime_data_->emulator->SetVolume(volume);
  SetEffectVolume(volume);
  config_->data().volume = volume;
  config_->SaveConfig();

  for (auto* observer : observers_) {
    observer->OnVolumeChanged(volume);
  }
}

bool MainWindow::IsAudioEnabled() {
  return runtime_data_->emulator->GetVolume() > 0;
}

void MainWindow::OnToggleAudioChannelMasks(kiwi::nes::AudioChannel which_mask) {
  SDL_assert(runtime_data_->debug_port);
  int current_mask = runtime_data_->debug_port->GetAudioChannelMasks();
  runtime_data_->debug_port->SetAudioChannelMasks(current_mask ^ which_mask);
}

bool MainWindow::IsAudioChannelOn(kiwi::nes::AudioChannel which_mask) {
  SDL_assert(runtime_data_->debug_port);
  return runtime_data_->debug_port->GetAudioChannelMasks() & which_mask;
}

void MainWindow::OnToggleRenderPaused() {
  SDL_assert(runtime_data_->debug_port);
  runtime_data_->debug_port->set_render_paused(!IsRenderPaused());
}

bool MainWindow::IsRenderPaused() {
  SDL_assert(runtime_data_->debug_port);
  return runtime_data_->debug_port->render_paused();
}

void MainWindow::OnSetScreenScale(float scale) {
  if (config_->data().window_scale != scale) {
    config_->data().window_scale = scale;
    config_->SaveConfig();
    OnScaleChanged();
  }
}

void MainWindow::OnSetFullscreen() {
  config_->data().is_fullscreen = true;
  config_->data().window_scale = InGameMenu::kMaxScaling;
  config_->SaveConfig();

  SDL_SetWindowFullscreen(native_window(), SDL_WINDOW_FULLSCREEN_DESKTOP);
  OnScaleChanged();
}

void MainWindow::OnUnsetFullscreen(float scale) {
  config_->data().is_fullscreen = false;
  config_->data().window_scale = scale;
  config_->SaveConfig();
  SDL_SetWindowFullscreen(native_window(), 0);
  OnScaleChanged();
}

bool MainWindow::ScreenScaleIs(float scale) {
  SDL_assert(canvas_);
  SDL_assert(window_scale() == canvas_->frame_scale());
  return canvas_->frame_scale() == scale;
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

void MainWindow::OnInGameMenuItemTrigger(InGameMenu::MenuItem item, int param) {
  switch (item) {
    case InGameMenu::MenuItem::kContinue: {
      CloseInGameMenu();
    } break;
    case InGameMenu::MenuItem::kLoadAutoSave: {
      OnLoadAutoSavedState(param);
      CloseInGameMenu();
    } break;
    case InGameMenu::MenuItem::kLoadState: {
      SDL_assert(param < NESRuntime::Data::MaxSaveStates);
      OnLoadState(param);
      CloseInGameMenu();
    } break;
    case InGameMenu::MenuItem::kSaveState: {
      SDL_assert(param < NESRuntime::Data::MaxSaveStates);
      OnSaveState(param);
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
      PlayEffect(audio_resources::AudioID::kSelect);
      if (go_left_ptr)
        OnInGameSettingsHandleVolume(*go_left_ptr);
      else
        OnSetAudioVolume(*value_ptr);
      break;
    case InGameMenu::SettingsItem::kWindowSize:
      SDL_assert(go_left_ptr);
      OnInGameSettingsHandleWindowSize(*go_left_ptr);
      break;
    case InGameMenu::SettingsItem::kJoyP1:
    case InGameMenu::SettingsItem::kJoyP2: {
      SDL_assert(go_left_ptr);
      std::vector<SDL_GameController*> controllers = GetControllerList();
      int player_index = (item == InGameMenu::SettingsItem::kJoyP1 ? 0 : 1);
      // Find next controller
      auto iter =
          std::find(controllers.begin(), controllers.end(),
                    runtime_data_->joystick_mappings[player_index].which);
      if (*go_left_ptr && iter != controllers.begin()) {
        SDL_GameController* next_controller = *(iter - 1);
        SetControllerMapping(runtime_data_, player_index, next_controller,
                             false);
      } else if (!*go_left_ptr && (iter != controllers.end() - 1)) {
        SDL_GameController* next_controller = *(iter + 1);
        SetControllerMapping(runtime_data_, player_index, next_controller,
                             false);
      }
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

#if !KIWI_MOBILE
void MainWindow::OnInGameSettingsHandleWindowSize(bool is_left) {
#if !KIWI_WASM  // Disable window settings. It should be handled by <canvas>.
  if (is_fullscreen() && !is_left)
    return;

  if (is_fullscreen() && is_left) {
    OnUnsetFullscreen(InGameMenu::kMaxScaling);
  } else {
    int scale = window_scale();
    scale = (is_left ? scale - 1 : scale + 1);
    if (scale < 2) {
      scale = 2;
      OnSetScreenScale(scale);
    } else if (scale > InGameMenu::kMaxScaling) {
      // There's an issue(perhaps a bug) on Emscripten when set fullscreen.
      // "Operation does not support unaligned accesses" at
      // wasm.emscripten_thread_mailbox_ref.
      // So fullscreen is disabled here.
      OnSetFullscreen();
    } else {
      OnSetScreenScale(scale);
    }
  }
#endif
}

void MainWindow::OnInGameSettingsHandleVolume(bool is_left) {
  float volume = runtime_data_->emulator->GetVolume();
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
  if (!is_headless_) {
    FlexItemsWidget* main_items_widget = GetMainItemsWidget();
    if (main_items_widget) {
      config_->data().last_index = main_items_widget->current_index();
      config_->SaveConfig();
    }
  }
}

bool MainWindow::IsPause() {
  SDL_assert(runtime_data_->emulator);
  return runtime_data_->emulator->GetRunningState() ==
         kiwi::nes::Emulator::RunningState::kPaused;
}
