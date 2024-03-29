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
#include "preset_roms/preset_roms.h"
#include "resources/image_resources.h"
#include "ui/application.h"
#include "ui/widgets/about_widget.h"
#include "ui/widgets/canvas.h"
#include "ui/widgets/demo_widget.h"
#include "ui/widgets/disassembly_widget.h"
#include "ui/widgets/export_widget.h"
#include "ui/widgets/frame_rate_widget.h"
#include "ui/widgets/group_widget.h"
#include "ui/widgets/kiwi_bg_widget.h"
#include "ui/widgets/kiwi_items_widget.h"
#include "ui/widgets/loading_widget.h"
#include "ui/widgets/memory_widget.h"
#include "ui/widgets/nametable_widget.h"
#include "ui/widgets/palette_widget.h"
#include "ui/widgets/pattern_widget.h"
#include "ui/widgets/splash.h"
#include "ui/widgets/stack_widget.h"
#include "ui/widgets/toast.h"
#include "utility/audio_effects.h"
#include "utility/key_mapping_util.h"
#include "utility/localization.h"
#include "utility/zip_reader.h"

namespace {

DEFINE_bool(has_menu, false, "Shows a menu bar at the top of the window.");

MainWindow* g_main_window_instance = nullptr;

kiwi::nes::Bytes ReadFromRawBinary(const kiwi::nes::Byte* data,
                                   size_t data_size) {
  kiwi::nes::Bytes bytes;
  bytes.resize(data_size);
  memcpy(bytes.data(), data, data_size);
  return bytes;
}

constexpr int kDefaultWindowWidth = Canvas::kNESFrameDefaultWidth;
constexpr int kDefaultWindowHeight = Canvas::kNESFrameDefaultHeight;
constexpr int kDefaultFontSize = 15;

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
  if (SDL_IsGameController(which)) {
    std::string name = SDL_GameControllerNameForIndex(which);
    Toast::ShowToast(
        window, name + (is_added ? " is connected." : " is disconnected."));
  }
}

#if ENABLE_EXPORT_ROMS
bool ExportNES(const kiwi::base::FilePath& output_path,
               const std::string& title,
               const kiwi::nes::Byte* zip_data,
               size_t zip_size) {
  if (!kiwi::base::PathExists(output_path)) {
    kiwi::base::CreateDirectory(output_path);
  }

  kiwi::base::File output_zip_file(
      output_path.Append(kiwi::base::FilePath::FromUTF8Unsafe(title + ".zip")),
      kiwi::base::File::FLAG_CREATE_ALWAYS | kiwi::base::File::FLAG_WRITE);
  if (zip_size != output_zip_file.Write(
                      0, reinterpret_cast<const char*>(zip_data), zip_size))
    return false;

  return true;
}

std::pair<bool, int> OnExportGameROM(MainWindow* main_window,
                                     const kiwi::base::FilePath& export_path,
                                     int current_export_rom_index) {
  const preset_roms::PresetROM& rom =
      preset_roms::GetPresetRoms()[current_export_rom_index];
  main_window->Exporting(rom.name);
  return std::make_pair(
      ExportNES(export_path, rom.name, rom.zip_data, rom.zip_size),
      current_export_rom_index + 1);
}

void OnGameROMExported(MainWindow* main_window,
                       const kiwi::base::FilePath& export_path,
                       const std::pair<bool, int>& result) {
  if (result.second >= preset_roms::GetPresetRomsCount()) {
    // No more roms to be exported.
    main_window->ExportDone();
    return;
  }

  const std::string& next_rom_name =
      preset_roms::GetPresetRoms()[result.second].name;
  if (result.first) {
    main_window->ExportSucceeded(next_rom_name);
  } else {
    main_window->ExportFailed(next_rom_name);
  }

  scoped_refptr<kiwi::base::SequencedTaskRunner> io_task_runner =
      Application::Get()->GetIOTaskRunner();
  io_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&OnExportGameROM, main_window, export_path,
                           result.second),
      kiwi::base::BindOnce(&OnGameROMExported, main_window, export_path));
}

#endif

class StringUpdater : public LocalizedStringUpdater {
 public:
  explicit StringUpdater(int string_id);
  ~StringUpdater() override = default;

 protected:
  std::string GetLocalizedString() override;
  std::string GetCollateStringHint() override;

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

class ROMTitleUpdater : public LocalizedStringUpdater {
 public:
  explicit ROMTitleUpdater(const preset_roms::PresetROM& preset_rom);
  ~ROMTitleUpdater() override = default;

 protected:
  std::string GetLocalizedString() override;
  std::string GetCollateStringHint() override;

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

}  // namespace

// Observer
MainWindow::Observer::Observer() = default;
MainWindow::Observer::~Observer() = default;
void MainWindow::Observer::OnVolumeChanged(float new_value) {}

MainWindow::MainWindow(const std::string& title,
                       NESRuntimeID runtime_id,
                       scoped_refptr<NESConfig> config,
                       bool has_demo_widget)
    : WindowBase(title), config_(config), has_demo_widget_(has_demo_widget) {
#if KIWI_WASM
  // Only one main window instance should exist in WASM.
  SDL_assert(!g_main_window_instance);
  g_main_window_instance = this;
#endif

  Initialize(runtime_id);
  InitializeAudio();
  InitializeUI();
  InitializeIODevices();
}

MainWindow::~MainWindow() {
  SDL_assert(runtime_data_);
  SDL_assert(runtime_data_->emulator);
  SDL_assert(canvas_);
  runtime_data_->emulator->PowerOff();
  canvas_->RemoveObserver(this);
  SaveConfig();

#if defined(KIWI_USE_EXTERNAL_PAK)
  CloseRomDataFromPackage(preset_roms::GetPresetRoms());
  CloseRomDataFromPackage(preset_roms::specials::GetPresetRoms());
#endif

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
  AddAfterSplashCallback(kiwi::base::BindOnce(
      &MainWindow::LoadROMByPath, kiwi::base::Unretained(this), rom_path));
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

void MainWindow::ExportDone() {
  SDL_assert(export_widget_);
  export_widget_->Done();
}

void MainWindow::ExportSucceeded(const std::string& rom_name) {
  export_widget_->Succeeded(kiwi::base::FilePath::FromUTF8Unsafe(rom_name));
}

void MainWindow::ExportFailed(const std::string& rom_name) {
  export_widget_->Failed(kiwi::base::FilePath::FromUTF8Unsafe(rom_name));
}

void MainWindow::Exporting(const std::string& rom_name) {
  export_widget_->SetCurrent(kiwi::base::FilePath::FromUTF8Unsafe(rom_name));
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

void MainWindow::AddObserver(Observer* observer) {
  observers_.insert(observer);
}

void MainWindow::RemoveObserver(Observer* observer) {
  observers_.erase(observer);
}

bool MainWindow::IsKeyDown(int controller_id,
                           kiwi::nes::ControllerButton button) {
  bool matched =
      pressing_keys_.find(runtime_data_->keyboard_mappings[controller_id]
                              .mapping[static_cast<int>(button)]) !=
      pressing_keys_.cend();

  if (matched)
    return true;

  matched = IsVirtualJoystickButtonPressed(controller_id, button);
  if (matched)
    return true;

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

    if (matched)
      return true;

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

  return matched;
}

SDL_Rect MainWindow::GetClientBounds() {
  // Excludes menu bar's height;
  SDL_Rect render_bounds = WindowBase::GetClientBounds();
  if (menu_bar_) {
    if (menu_bar_->bounds().h > 0) {
      render_bounds.y += menu_bar_->bounds().h;
      render_bounds.h -= menu_bar_->bounds().h;
    } else {
      // Menu bar doesn't render yet. Use default value.
      render_bounds.y += GetDefaultMenuHeight();
      render_bounds.h -= GetDefaultMenuHeight();
    }
  }
  return render_bounds;
}

void MainWindow::HandleKeyEvents(SDL_KeyboardEvent* event) {
  if (!IsPause()) {
    // Do not handle emulator's key event when paused.
    if (event->type == SDL_KEYDOWN)
      pressing_keys_.insert(event->keysym.sym);
    else if (event->type == SDL_KEYUP)
      pressing_keys_.erase(event->keysym.sym);
  }

  WindowBase::HandleKeyEvents(event);
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
  if (bg_widget_) {
    SDL_Rect client_bounds = GetClientBounds();
    FillLayout(this, bg_widget_);
    if (!is_headless_) {
      // In headless mode, |main_group_widget_| won't be created. The game will
      // start as soon as canvas created.
      SDL_assert(main_group_widget_);
      FillLayout(this, main_group_widget_);
    }
    FillLayout(this, stack_widget_);
  }

  if (in_game_menu_) {
    FillLayout(this, in_game_menu_);
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

  if (main_group_widget_)
    main_group_widget_->RecalculateBounds();

  WindowBase::HandleResizedEvent();
}

void MainWindow::HandlePostEvent() {
  SDL_assert(runtime_data_);
  runtime_data_->emulator->RunOneFrame();
}

void MainWindow::HandleDisplayEvent(SDL_DisplayEvent* event) {
  // Display event changing treats as resizing.
  if (event->type == SDL_DISPLAYEVENT_ORIENTATION)
    HandleResizedEvent();
}

#if !KIWI_MOBILE
void MainWindow::OnAboutToRenderFrame(Canvas* canvas,
                                      scoped_refptr<NESFrame> frame) {
  // Always adjusts the canvas to the middle of the render area (excludes menu
  // bar).
  SDL_Rect render_bounds = GetClientBounds();
  SDL_Rect src_rect = {0, 0, frame->width(), frame->height()};

  SDL_Rect dest_rect = {
      static_cast<int>((render_bounds.w - src_rect.w * canvas->frame_scale()) /
                       2) +
          render_bounds.x,
      static_cast<int>((render_bounds.h - src_rect.h * canvas->frame_scale()) /
                       2) +
          render_bounds.y,
      static_cast<int>(frame->width() * canvas->frame_scale()),
      static_cast<int>(frame->height() * canvas->frame_scale())};
  canvas->set_bounds(dest_rect);

  // Updates window size, to fit the frame.
  if (menu_bar_) {
    SDL_Rect menu_rect = menu_bar_->bounds();
    int desired_window_width = dest_rect.w;
    int desired_window_height = menu_rect.h + dest_rect.h;
    Resize(desired_window_width, desired_window_height);
  } else {
    Resize(dest_rect.w, dest_rect.h);
  }
}
#endif

void MainWindow::Initialize(NESRuntimeID runtime_id) {
  SDL_assert(runtime_id >= 0);
  is_headless_ = preset_roms::GetPresetRomsCount() == 0;
  runtime_id_ = runtime_id;
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);

  if (FLAGS_has_menu) {
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
  if (FLAGS_has_menu) {
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

  // Background
  SDL_Rect client_bounds = GetClientBounds();
  std::unique_ptr<KiwiBgWidget> bg_widget =
      std::make_unique<KiwiBgWidget>(this);
  bg_widget_ = bg_widget.get();
  FillLayout(this, bg_widget_);
  AddWidget(std::move(bg_widget));

  // Stack widget
  std::unique_ptr<StackWidget> stack_widget =
      std::make_unique<StackWidget>(this);
  stack_widget_ = stack_widget.get();
  stack_widget_->set_bounds(client_bounds);
  bg_widget_->AddWidget(std::move(stack_widget));

  // Create virtual touch buttons, which will invoke main items_widget's
  // methods.
  CreateVirtualTouchButtons();

  if (!is_headless_) {
    // Main menu groups
    std::unique_ptr<GroupWidget> group_widget =
        std::make_unique<GroupWidget>(this, runtime_id_);
    main_group_widget_ = group_widget.get();
    FillLayout(this, main_group_widget_);
    stack_widget_->PushWidget(std::move(group_widget));

    // Game items
    std::unique_ptr<KiwiItemsWidget> items_widget =
        std::make_unique<KiwiItemsWidget>(this, runtime_id_);
    main_items_widget_ = items_widget.get();

#if defined(KIWI_USE_EXTERNAL_PAK)
    OpenRomDataFromPackage(preset_roms::GetPresetRoms(),
                           kiwi::base::FilePath::FromUTF8Unsafe(
                               preset_roms::GetPresetRomsPackageName()));
    OpenRomDataFromPackage(
        preset_roms::specials::GetPresetRoms(),
        kiwi::base::FilePath::FromUTF8Unsafe(
            preset_roms::specials::GetPresetRomsPackageName()));
#endif

    SDL_assert(preset_roms::GetPresetRomsCount() > 0);
    for (size_t i = 0; i < preset_roms::GetPresetRomsCount(); ++i) {
      const auto& rom = preset_roms::GetPresetRoms()[i];
      FillRomDataFromZip(rom);
      int main_item_index = items_widget->AddItem(
          std::make_unique<ROMTitleUpdater>(rom), rom.rom_cover.data(),
          rom.rom_cover.size(),
          kiwi::base::BindRepeating(&MainWindow::OnLoadPresetROM,
                                    kiwi::base::Unretained(this), rom));

      for (const auto& alternative_rom : rom.alternates) {
        items_widget->AddSubItem(
            main_item_index, std::make_unique<ROMTitleUpdater>(alternative_rom),
            alternative_rom.rom_cover.data(), alternative_rom.rom_cover.size(),
            kiwi::base::BindRepeating(&MainWindow::OnLoadPresetROM,
                                      kiwi::base::Unretained(this),
                                      alternative_rom));
      }
    }
    items_widget->Sort();

    int main_items_index = std::clamp(config_->data().last_index, 0,
                                      items_widget->GetItemCount() - 1);
    items_widget->SetIndex(main_items_index);

    main_group_widget_->AddWidget(std::move(items_widget));

    // Game items (special)
    std::unique_ptr<KiwiItemsWidget> specials_item_widget =
        std::make_unique<KiwiItemsWidget>(this, runtime_id_);

    SDL_assert(preset_roms::specials::GetPresetRomsCount() > 0);
    for (size_t i = 0; i < preset_roms::specials::GetPresetRomsCount(); ++i) {
      const auto& rom = preset_roms::specials::GetPresetRoms()[i];
      FillRomDataFromZip(rom);

      specials_item_widget->AddItem(
          std::make_unique<ROMTitleUpdater>(rom), rom.rom_cover.data(),
          rom.rom_cover.size(),
          kiwi::base::BindRepeating(&MainWindow::OnLoadPresetROM,
                                    kiwi::base::Unretained(this), rom));
    }
    specials_item_widget->Sort();

    if (!specials_item_widget->IsEmpty())
      main_group_widget_->AddWidget(std::move(specials_item_widget));

    // About
    std::unique_ptr<KiwiItemsWidget> settings_widget =
        std::make_unique<KiwiItemsWidget>(this, runtime_id_);

    // Settings items
    std::unique_ptr<KiwiItemsWidget> controller_widget =
        std::make_unique<KiwiItemsWidget>(this, runtime_id_);

    settings_widget->set_can_sort(false);
    settings_widget->AddItem(
        std::make_unique<StringUpdater>(
            string_resources::IDR_MAIN_WINDOW_SETTINGS),
        image_resources::kSettingsLogo, image_resources::kSettingsLogoSize,
        kiwi::base::BindRepeating(
            [](MainWindow* window, StackWidget* stack_widget,
               NESRuntimeID runtime_id) {
              std::unique_ptr<InGameMenu> in_game_menu =
                  std::make_unique<InGameMenu>(
                      window, runtime_id,
                      kiwi::base::BindRepeating(
                          [](StackWidget* stack_widget,
                             InGameMenu::MenuItem item, int param) {
                            // Mapping button 'B' will trigger kContinue.
                            if (item ==
                                    InGameMenu::MenuItem::kToGameSelection ||
                                item == InGameMenu::MenuItem::kContinue) {
                              stack_widget->PopWidget();
                            }
                          },
                          stack_widget),
                      kiwi::base::BindRepeating(
                          &MainWindow::OnInGameSettingsItemTrigger,
                          kiwi::base::Unretained(window)));
              in_game_menu->HideMenu(0);  // Hides 'Continue'
              in_game_menu->HideMenu(1);  // Hides 'Load Auto Save'
              in_game_menu->HideMenu(2);  // Hides 'Load State'
              in_game_menu->HideMenu(3);  // Hides 'Save State'
              in_game_menu->HideMenu(5);  // Hides 'Reset Game'
              FillLayout(window, in_game_menu.get());
              stack_widget->PushWidget(std::move(in_game_menu));
            },
            this, stack_widget_, runtime_id_));

    settings_widget->AddItem(
        std::make_unique<StringUpdater>(
            string_resources::IDR_MAIN_WINDOW_ABOUT),
        image_resources::kBackgroundLogo, image_resources::kBackgroundLogoSize,
        kiwi::base::BindRepeating(
            [](MainWindow* window, StackWidget* stack_widget,
               NESRuntimeID runtime_id) {
              stack_widget->PushWidget(std::make_unique<AboutWidget>(
                  window, stack_widget, runtime_id));
            },
            this, stack_widget_, runtime_id_));

#if !KIWI_IOS
    // iOS needn't quit the application manually.
    settings_widget->AddItem(
        std::make_unique<StringUpdater>(string_resources::IDR_MAIN_WINDOW_QUIT),
        image_resources::kExitLogo, image_resources::kExitLogoSize,
        kiwi::base::BindRepeating(&MainWindow::OnQuit,
                                  kiwi::base::Unretained(this)));
#endif

    // End of settings items
    main_group_widget_->AddWidget(std::move(settings_widget));
  }

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

  std::unique_ptr<FrameRateWidget> frame_rate_widget =
      std::make_unique<FrameRateWidget>(this, canvas_->frame(),
                                        runtime_data_->debug_port.get());
  frame_rate_widget_ = frame_rate_widget.get();
  frame_rate_widget_->set_visible(false);
  AddWidget(std::move(frame_rate_widget));

  std::unique_ptr<ExportWidget> export_widget =
      std::make_unique<ExportWidget>(this);
  export_widget_ = export_widget.get();
  export_widget_->set_visible(false);
  AddWidget(std::move(export_widget));

  std::unique_ptr<MemoryWidget> memory_widget = std::make_unique<MemoryWidget>(
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

  if (has_demo_widget_)
    AddWidget(std::make_unique<DemoWidget>(this));

  bool has_splash = false;
#if !KIWI_IOS  // iOS has launch storyboard, so we don't need to show a splash
               // here.
  // Splash
  if (!FLAGS_has_menu) {
    // If we have menu, we don't show splash screen, because we probably want to
    // do some debug works.
    std::unique_ptr<Splash> splash =
        std::make_unique<Splash>(this, stack_widget_, runtime_id_);
    splash->Play();
    splash->SetClosedCallback(kiwi::base::BindRepeating(
        &MainWindow::RunAllAfterSplashCallbacks, kiwi::base::Unretained(this)));
    stack_widget_->PushWidget(std::move(splash));
    has_splash = true;
  }
#endif

#if !KIWI_MOBILE
  OnScaleChanged();
  if (is_fullscreen())
    OnSetFullscreen();
#else
  OnScaleModeChanged();
#endif
  LayoutVirtualTouchButtons();

  // If there's no splash, run callbacks at once.
  if (!has_splash) {
    RunAllAfterSplashCallbacks();
  }
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

void MainWindow::AddAfterSplashCallback(kiwi::base::OnceClosure callback) {
  if (splash_done_)
    std::move(callback).Run();
  else
    post_splash_callbacks_.push_back(std::move(callback));
}

void MainWindow::RunAllAfterSplashCallbacks() {
  splash_done_ = true;
  for (auto&& callback : post_splash_callbacks_) {
    std::move(callback).Run();
  }
  post_splash_callbacks_.clear();
}

void MainWindow::LoadROMByPath(kiwi::base::FilePath rom_path) {
  SDL_assert(runtime_data_->emulator);
  SetLoading(true);

  runtime_data_->emulator->LoadAndRun(
      rom_path, kiwi::base::BindOnce(&MainWindow::OnRomLoaded,
                                     kiwi::base::Unretained(this),
                                     rom_path.BaseName().AsUTF8Unsafe()));
}

void MainWindow::StartAutoSave() {
  constexpr int kAutoSaveTimeDelta = 5000;
  runtime_data_->StartAutoSave(
      kiwi::base::Milliseconds(kAutoSaveTimeDelta),
      kiwi::base::BindRepeating(
          [](Canvas* canvas) { return canvas->frame()->buffer(); }, canvas_));
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

#if ENABLE_DEBUG_ROMS
    if (HasDebugRoms()) {
      MenuBar::MenuItem debug_roms =
          CreateDebugRomsMenu(kiwi::base::BindRepeating(
              &MainWindow::OnLoadDebugROM, kiwi::base::Unretained(this)));
      debug.menu_items.push_back(std::move(debug_roms));
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
        {"Frame rate",
         kiwi::base::BindRepeating(&MainWindow::OnFrameRateWidget,
                                   kiwi::base::Unretained(this)),
         kiwi::base::BindRepeating(&MainWindow::IsFrameRateWidgetShown,
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

#if ENABLE_EXPORT_ROMS
    debug.menu_items.push_back(
        {"Export All Games",
         kiwi::base::BindRepeating(&MainWindow::OnExportGameROMs,
                                   kiwi::base::Unretained(this))});

    result.push_back(std::move(debug));
#endif
  }

  return result;
}

void MainWindow::SetLoading(bool is_loading) {
  SDL_assert(bg_widget_);
  SDL_assert(loading_widget_);
  bg_widget_->SetLoading(is_loading);
  loading_widget_->set_visible(is_loading);
}

void MainWindow::ShowMainMenu(bool show) {
  SDL_assert(bg_widget_);
  SDL_assert(canvas_);
  canvas_->set_visible(!show);
  bg_widget_->set_visible(show);
  SetVirtualButtonsVisible(!show);
  SetLoading(false);
}

void MainWindow::OnScaleChanged() {
  if (!is_fullscreen()) {
    const int default_menu_height = GetDefaultMenuHeight();
    if (menu_bar_) {
      if (menu_bar_->bounds().h > 0) {
        // Menu bar is painted, we can get exact menu height here.
        Resize(kDefaultWindowWidth * window_scale(),
               kDefaultWindowHeight * window_scale() + menu_bar_->bounds().h);
      } else {
        Resize(kDefaultWindowWidth * window_scale(),
               kDefaultWindowHeight * window_scale() + default_menu_height);
      }
    } else {
      Resize(kDefaultWindowWidth * window_scale(),
             kDefaultWindowHeight * window_scale());
    }
    MoveToCenter();
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

void MainWindow::OnRomLoaded(const std::string& name) {
  SetLoading(false);
  ShowMainMenu(false);
  SetTitle(name);
  StartAutoSave();
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
                                kiwi::base::Unretained(this), true)
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
              window->canvas_->frame()->buffer(),
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
  if (succeed) {
    SDL_assert(in_game_menu_);
    in_game_menu_->RequestCurrentThumbnail();
    Toast::ShowToast(this, "State saved.");
  } else {
    Toast::ShowToast(this, "State save failed.");
  }
}

void MainWindow::OnStateLoaded(
    const NESRuntime::Data::StateResult& state_result) {
  if (state_result.success && !state_result.state_data.empty()) {
    audio_->Reset();
    runtime_data_->emulator->LoadState(
        state_result.state_data,
        kiwi::base::BindOnce(
            [](MainWindow* window, bool success) {
              if (success)
                Toast::ShowToast(window, "State loaded.");
              else
                Toast::ShowToast(window, "State load failed.");
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
  // Cleanup all pressing keys when pausing.
  pressing_keys_.clear();
  runtime_data_->emulator->Pause();
  memory_widget_->UpdateMemory();
  disassembly_widget_->UpdateDisassembly();
  SetVirtualButtonsVisible(false);
}

void MainWindow::OnResume() {
  runtime_data_->emulator->Run();
  SetVirtualButtonsVisible(true);
}

void MainWindow::OnLoadPresetROM(const preset_roms::PresetROM& rom) {
  SDL_assert(runtime_data_->emulator);
  SetLoading(true);

  runtime_data_->emulator->LoadAndRun(
      ReadFromRawBinary(rom.rom_data.data(), rom.rom_data.size()),
      kiwi::base::BindOnce(&MainWindow::OnRomLoaded,
                           kiwi::base::Unretained(this),
                           GetROMLocalizedTitle(rom)));
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

  // Windows system use a fake fullscreen to avoid changing resolution.
  // While macOS can use the real fullscreen to avoid fullscreen's
  // window animation.
#if defined(WIN32)
  SDL_SetWindowFullscreen(native_window(), SDL_WINDOW_FULLSCREEN_DESKTOP);
#else
  SDL_SetWindowFullscreen(native_window(), SDL_WINDOW_FULLSCREEN);
#endif
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

void MainWindow::OnFrameRateWidget() {
  SDL_assert(frame_rate_widget_);
  frame_rate_widget_->set_visible(!frame_rate_widget_->visible());
}

bool MainWindow::IsFrameRateWidgetShown() {
  SDL_assert(frame_rate_widget_);
  return frame_rate_widget_->visible();
}

#if ENABLE_EXPORT_ROMS

void MainWindow::OnExportGameROMs() {
  char* pref_path = SDL_GetPrefPath("Kiwi", "KiwiMachine");
  kiwi::base::FilePath export_path =
      kiwi::base::FilePath::FromUTF8Unsafe(pref_path).Append(
          FILE_PATH_LITERAL("nes"));
  SDL_free(pref_path);
  scoped_refptr<kiwi::base::SequencedTaskRunner> io_task_runner =
      Application::Get()->GetIOTaskRunner();

  export_widget_->Start(preset_roms::GetPresetRomsCount(), export_path);
  export_widget_->SetCurrent(kiwi::base::FilePath::FromUTF8Unsafe(
      preset_roms::GetPresetRoms()[0].name));

  io_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, kiwi::base::BindOnce(&OnExportGameROM, this, export_path, 0),
      kiwi::base::BindOnce(&OnGameROMExported, this, export_path));
}

#endif

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
      CloseInGameMenu();
    } break;
    default:
      break;
  }
}

void MainWindow::OnInGameSettingsItemTrigger(InGameMenu::SettingsItem item,
                                             bool is_left) {
  switch (item) {
    case InGameMenu::SettingsItem::kVolume:
      PlayEffect(audio_resources::AudioID::kSelect);
      OnInGameSettingsHandleVolume(is_left);
      break;
    case InGameMenu::SettingsItem::kWindowSize:
      OnInGameSettingsHandleWindowSize(is_left);
      break;
    case InGameMenu::SettingsItem::kJoyP1:
    case InGameMenu::SettingsItem::kJoyP2: {
      std::vector<SDL_GameController*> controllers = GetControllerList();
      int player_index = (item == InGameMenu::SettingsItem::kJoyP1 ? 0 : 1);
      // Find next controller
      auto iter =
          std::find(controllers.begin(), controllers.end(),
                    runtime_data_->joystick_mappings[player_index].which);
      if (is_left && iter != controllers.begin()) {
        SDL_GameController* next_controller = *(iter - 1);
        SetControllerMapping(runtime_data_, player_index, next_controller,
                             false);
      } else if (!is_left && (iter != controllers.end() - 1)) {
        SDL_GameController* next_controller = *(iter + 1);
        SetControllerMapping(runtime_data_, player_index, next_controller,
                             false);
      }
    } break;
    case InGameMenu::SettingsItem::kLanguage: {
      SupportedLanguage language = GetCurrentSupportedLanguage();
      if (is_left) {
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
#endif

void MainWindow::SaveConfig() {
  // Before main window destruct, save current game index.
  // This happens when MainWindow is about to destroy, and has IO operation.
  if (!is_headless_) {
    SDL_assert(main_items_widget_);
    config_->data().last_index = main_items_widget_->current_index();
    config_->SaveConfig();
  }
}

bool MainWindow::IsPause() {
  SDL_assert(runtime_data_->emulator);
  return runtime_data_->emulator->GetRunningState() ==
         kiwi::nes::Emulator::RunningState::kPaused;
}
