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
#include <SDL_image.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <imgui.h>

#include "preset_roms/preset_roms.h"
#include "ui/application.h"
#include "utility/audio_effects.h"
#include "utility/fonts.h"
#include "utility/images.h"
#include "utility/localization.h"
#include "utility/zip_reader.h"

#if BUILDFLAG(IS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#elif BUILDFLAG(IS_WIN)
#include <Windows.h>
#endif

namespace {
constexpr int kInitializeSDLFailed = -1;
constexpr int kInitializeSDLImageFailed = -2;
Application* g_app_instance = nullptr;
}  // namespace

DEFINE_string(lang, "", "Set application's language.");

ApplicationObserver::ApplicationObserver() = default;

ApplicationObserver::~ApplicationObserver() {
#if defined(KIWI_USE_EXTERNAL_PAK)
  ClosePackages();
#endif
}

void ApplicationObserver::OnPreRender(int since_last_frame_ms) {}

void ApplicationObserver::OnPostRender(int render_elapsed_ms) {}

#if defined(_WIN32)
char** g_argv = nullptr;

void Cleanup() {
  for (size_t i = 0; i < __argc; ++i) {
    free(g_argv[i]);
  }
  free(g_argv);
}

Application::Application() {
  // Convert wchar_t** to char**.
  wchar_t** wargv = __wargv;
  SDL_assert(wargv);
  g_argv = (char**)calloc(__argc, sizeof(char*));
  for (size_t i = 0; i < __argc; ++i) {
    g_argv[i] = (char*)calloc(wcslen(wargv[i]) + 1, sizeof(char));
    if (g_argv[i]) {
      size_t converted;
      wcstombs_s(&converted, g_argv[i], wcslen(wargv[i]) + 1, wargv[i],
                 wcslen(wargv[i]) + 1);
    }
  }
  atexit(Cleanup);

  InitializeApplication(__argc, g_argv);
}
#endif

Application::Application(int& argc, char** argv) {
  InitializeApplication(argc, argv);
}

Application::~Application() {
  SDL_assert(g_app_instance);
  UninitializeImGui();
  UninitializeGameControllers();
  UninitializeAudioEffects();
  UninitializeImageResources();
  kiwi::base::SetEventHandlerForSDL2(kiwi::base::DoNothing());
  kiwi::base::SetPostEventHandlerForSDL2(kiwi::base::DoNothing());
  g_app_instance = nullptr;
}

void Application::HandleEvent(SDL_Event* event) {
  switch (event->type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
      WindowBase* target = FindWindowFromID(event->key.windowID);
      if (target)
        target->HandleKeyEvent(&event->key);
    } break;
    case SDL_CONTROLLERAXISMOTION: {
      for (const auto& w : windows_) {
        w.second->HandleJoystickAxisMotionEvent(&event->caxis);
      }
    } break;
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
      for (const auto& w : windows_) {
        w.second->HandleJoystickButtonEvent(&event->cbutton);
      }
    } break;
    case SDL_CONTROLLERDEVICEADDED: {
      AddGameController(event->cdevice.which);
      for (const auto& w : windows_) {
        w.second->HandleJoystickDeviceEvent(&event->cdevice);
      }
    } break;
    case SDL_CONTROLLERDEVICEREMOVED: {
      for (const auto& w : windows_) {
        w.second->HandleJoystickDeviceEvent(&event->cdevice);
      }
      RemoveGameController(event->cdevice.which);
    } break;
    case SDL_WINDOWEVENT: {
      if (event->window.event == SDL_WINDOWEVENT_RESIZED ||
          event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        WindowBase* target = FindWindowFromID(event->window.windowID);
        if (target)
          target->HandleResizedEvent();
      }
    } break;
    case SDL_DISPLAYEVENT_ORIENTATION: {
      for (const auto& w : windows_) {
        w.second->HandleDisplayEvent(&event->display);
      }
    } break;
    case SDL_FINGERDOWN:
    case SDL_FINGERUP:
    case SDL_FINGERMOTION: {
      WindowBase* target = FindWindowFromID(event->tfinger.windowID);
      if (target)
        target->HandleTouchFingerEvent(&event->tfinger);
    } break;
#if !KIWI_MOBILE
    case SDL_MOUSEMOTION: {
      WindowBase* target = FindWindowFromID(event->motion.windowID);
      if (target)
        target->HandleMouseMoveEvent(&event->motion);
    } break;
    case SDL_MOUSEWHEEL: {
      WindowBase* target = FindWindowFromID(event->wheel.windowID);
      if (target)
        target->HandleMouseWheelEvent(&event->wheel);
    } break;
    case SDL_MOUSEBUTTONDOWN: {
      WindowBase* target = FindWindowFromID(event->button.windowID);
      if (target)
        target->HandleMousePressedEvent(&event->button);
    } break;
    case SDL_MOUSEBUTTONUP: {
      WindowBase* target = FindWindowFromID(event->button.windowID);
      if (target)
        target->HandleMouseReleasedEvent(&event->button);
    } break;
#endif
    default:
      break;
  }
}

WindowBase* Application::FindWindowFromID(uint32_t id) {
  auto window = windows_.find(id);
  if (window == windows_.cend())
    return nullptr;

  return window->second;
}

uint32_t Application::FindIDFromWindow(WindowBase* window) {
  for (const auto& w : windows_) {
    if (w.second == window)
      return w.first;
  }
  return -1;
}

void Application::Render() {
  int elapsed_ms_ = frame_elapsed_counter_.ElapsedInMillisecondsAndReset();
  render_counter_.Start();

  for (ApplicationObserver* observer : observers_) {
    observer->OnPreRender(elapsed_ms_);
  }

  for (const auto& window : windows_) {
    window.second->Render();
  }

  int render_elapsed_ms = render_counter_.ElapsedInMilliseconds();
  for (ApplicationObserver* observer : observers_) {
    observer->OnPostRender(render_elapsed_ms);
  }
}

void Application::LocaleChanged() {
  for (const auto& w : windows_) {
    w.second->HandleLocaleChanged();
  }
}

void Application::FontChanged() {
  for (const auto& w : windows_) {
    w.second->HandleFontChanged();
  }
}

void Application::HandlePostEvent() {
  for (const auto& w : windows_) {
    w.second->HandlePostEvent();
  }
}

Application* Application::Get() {
  SDL_assert(g_app_instance);
  return g_app_instance;
}

scoped_refptr<kiwi::base::SequencedTaskRunner> Application::GetIOTaskRunner() {
  return io_thread_->task_runner();
}

void Application::Initialize(kiwi::base::OnceClosure other_io_task,
                             kiwi::base::OnceClosure callback) {
  if (!initialized_) {
    GetIOTaskRunner()->PostTaskAndReply(
        FROM_HERE,
        kiwi::base::BindOnce(&Application::InitializeROMs,
                             kiwi::base::Unretained(this))
            .Then(std::move(other_io_task)),
        kiwi::base::BindOnce(&InitializeFonts)
            .Then(kiwi::base::BindOnce(&Application::FontChanged,
                                       kiwi::base::Unretained(this)))
            .Then(std::move(callback)));
    initialized_ = true;
  } else {
    std::move(callback).Run();
  }
}

void Application::Run() {
  runloop_.Run();
}

void Application::AddObserver(ApplicationObserver* observer) {
  observers_.insert(observer);
}

void Application::RemoveObserver(ApplicationObserver* observer) {
  observers_.erase(observer);
}

void Application::SetLanguage(SupportedLanguage language) {
  ::SetLanguage(language);
  LocaleChanged();
  config_->data().language = static_cast<int>(language);
  config_->SaveConfig();
}

void Application::InitializeApplication(int& argc, char** argv) {
  SDL_assert(!g_app_instance);
  g_app_instance = this;

  // Creates an IO thread to manipulate file (if any) operations.
  io_thread_ = std::make_unique<kiwi::base::Thread>("Kiwi Machine IO Thread");
  kiwi::base::Thread::Options options;
  options.message_pump_type = kiwi::base::MessagePumpType::IO;
  io_thread_->StartWithOptions(std::move(options));

  // Using gflags to parse command line.
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  // Initialize runtime configs.
  InitializeRuntimeAndConfigs();

  if (!FLAGS_lang.empty()) {
    ::SetLanguage(FLAGS_lang.c_str());
  } else {
    // Loads language settings from config
    if (config_->data().language != -1) {
      // -1 means automatic. If it is not -1, it represents a certain supported
      // language.
      SupportedLanguage language =
          static_cast<SupportedLanguage>(config_->data().language);
      if (language >= SupportedLanguage::kMax)
        language = static_cast<SupportedLanguage>(0);
      ::SetLanguage(language);
    }
  }

  kiwi::base::InitializePlatformFactory(argc, argv);
  if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
    exit(kInitializeSDLFailed);
  }

  InitializeImGui();
  InitializeAudioEffects();
  if (!InitializeImageResources()) {
    exit(kInitializeSDLImageFailed);
  }

  event_handler_ = kiwi::base::BindRepeating(&Application::HandleEvent,
                                             kiwi::base::Unretained(this));
  render_handler_ = kiwi::base::BindRepeating(&Application::Render,
                                              kiwi::base::Unretained(this));
  post_event_handler_ = kiwi::base::BindRepeating(&Application::HandlePostEvent,
                                                  kiwi::base::Unretained(this));
  kiwi::base::SetEventHandlerForSDL2(event_handler_);
  kiwi::base::SetRenderHandlerForSDL2(render_handler_);
  kiwi::base::SetPostEventHandlerForSDL2(post_event_handler_);
}

void Application::UninitializeGameControllers() {
  for (SDL_GameController* game_controller : game_controllers_) {
    SDL_GameControllerClose(game_controller);
  }
}

void Application::AddGameController(int which) {
  if (SDL_IsGameController(which)) {
    SDL_GameController* controller = SDL_GameControllerOpen(which);
    game_controllers_.insert(controller);
  }
}

void Application::RemoveGameController(int which) {
  if (SDL_IsGameController(which)) {
    SDL_GameController* controller = SDL_GameControllerFromInstanceID(which);
    SDL_GameControllerClose(controller);
    game_controllers_.erase(controller);
  }
}

void Application::InitializeImGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  ImGui::StyleColorsClassic();

  kiwi::base::SetPreEventHandlerForSDL2(kiwi::base::BindRepeating(
      [](SDL_Event* event) { ImGui_ImplSDL2_ProcessEvent(event); }));

  InitializeStyles();
  InitializeSystemFonts();
}

void Application::UninitializeImGui() {
  kiwi::base::SetPreEventHandlerForSDL2(kiwi::base::DoNothing());
  kiwi::base::SetRenderHandlerForSDL2(kiwi::base::DoNothing());
  ImGui::DestroyContext();
}

void Application::InitializeStyles() {
  ImGuiStyle& style = ImGui::GetStyle();
  style.ItemSpacing.x = 10;
}

void Application::InitializeRuntimeAndConfigs() {
  // Make a kiwi-nes runtime.
  NESRuntimeID runtime_id = NESRuntime::GetInstance()->CreateData("Default");
  NESRuntime::Data* runtime_data =
      NESRuntime::GetInstance()->GetDataById(runtime_id);
  runtime_data->emulator = kiwi::nes::CreateEmulator();
  runtime_data->debug_port =
      std::make_unique<DebugPort>(runtime_data->emulator.get());
  runtime_id_ = runtime_id;

  // Create configs
  scoped_refptr<NESConfig> config =
      kiwi::base::MakeRefCounted<NESConfig>(runtime_data->profile_path);

  // Set key mappings.
  NESRuntime::Data::ControllerMapping key_mapping1 = {
      SDLK_j, SDLK_k, SDLK_l, SDLK_RETURN, SDLK_w, SDLK_s, SDLK_a, SDLK_d};
  NESRuntime::Data::ControllerMapping key_mapping2 = {
      SDLK_DELETE, SDLK_END,  SDLK_PAGEDOWN, SDLK_HOME,
      SDLK_UP,     SDLK_DOWN, SDLK_LEFT,     SDLK_RIGHT};
  runtime_data->keyboard_mappings[0] = key_mapping1;
  runtime_data->keyboard_mappings[1] = key_mapping2;
  runtime_data->emulator->PowerOn();
  config->LoadConfigAndWait();
  config_ = config;
}

void Application::InitializeROMs() {
#if defined(KIWI_USE_EXTERNAL_PAK)
  // Iterates all pak file for loading package.
  std::vector<kiwi::base::FilePath> file_paths = GetPackagePathList();
  for (kiwi::base::FilePath file_path : file_paths) {
    OpenPackageFromFile(file_path);
  }
#endif

  for (auto* package : preset_roms::GetPresetRomsPackages()) {
    for (size_t i = 0; i < package->GetRomsCount(); ++i) {
      auto& rom = package->GetRomsByIndex(i);
      InitializePresetROM(rom);
      LoadPresetROM(rom, RomPart::kCover);
      for (auto& alternative_rom : rom.alternates) {
        LoadPresetROM(alternative_rom, RomPart::kCover);
      }
    }
  }
}

std::vector<kiwi::base::FilePath> Application::GetPackagePathList() {
#if BUILDFLAG(IS_MAC)
  std::vector<kiwi::base::FilePath> list;
  CFBundleRef main_bundle = CFBundleGetMainBundle();
  CFArrayRef pak_list =
      CFBundleCopyResourceURLsOfType(main_bundle, CFSTR("pak"), nullptr);
  CFIndex count = CFArrayGetCount(pak_list);
  for (CFIndex i = 0; i < count; ++i) {
    CFURLRef relative_url =
        reinterpret_cast<CFURLRef>(CFArrayGetValueAtIndex(pak_list, i));
    CFURLRef url = CFBundleCopyResourceURL(
        main_bundle, CFURLGetString(relative_url), nullptr, nullptr);
    std::string resource_abs_path;
    resource_abs_path.resize(PATH_MAX);
    bool success = CFURLGetFileSystemRepresentation(
        url, false, reinterpret_cast<UInt8*>(resource_abs_path.data()),
        resource_abs_path.size());
    SDL_assert(success);
    CFRelease(url);
    list.push_back(
        std::move(kiwi::base::FilePath::FromUTF8Unsafe(resource_abs_path)));
  }
  CFRelease(pak_list);
  return list;
#elif BUILDFLAG(IS_WIN)
  std::vector<kiwi::base::FilePath> list;
  CHAR current_exe_filename[MAX_PATH];
  DWORD buffer_len = GetModuleFileNameA(NULL, current_exe_filename, MAX_PATH);
  kiwi::base::FilePath resource_path =
      kiwi::base::FilePath::FromUTF8Unsafe(current_exe_filename).DirName();
  kiwi::base::FileEnumerator package_enumator(resource_path, false,
                                              kiwi::base::FileEnumerator::FILES,
                                              FILE_PATH_LITERAL("*.pak"));
  kiwi::base::FilePath file_path = package_enumator.Next();
  while (!file_path.empty()) {
    list.push_back(file_path);
    file_path = package_enumator.Next();
  }
  return list;
#else
  return std::vector<kiwi::base::FilePath>{
      kiwi::base::FilePath(kiwi::base::FilePath::kCurrentDirectory)};
#endif
}

void Application::AddWindowToEventHandler(WindowBase* window) {
  windows_.insert(std::make_pair(window->GetWindowID(), window));
}

void Application::RemoveWindowFromEventHandler(WindowBase* window) {
  windows_.erase(window->GetWindowID());
}
