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

#include "ui/application.h"
#include "utility/audio_effects.h"
#include "utility/fonts.h"
#include "utility/images.h"

namespace {
constexpr int kInitializeSDLFailed = -1;
constexpr int kInitializeSDLImageFailed = -2;
Application* g_app_instance = nullptr;
}  // namespace

ApplicationObserver::ApplicationObserver() = default;
ApplicationObserver::~ApplicationObserver() = default;
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
        target->HandleKeyEvents(&event->key);
    } break;
    case SDL_CONTROLLERAXISMOTION: {
      for (const auto& w : windows_) {
        w.second->HandleJoystickAxisMotionEvents(&event->caxis);
      }
    } break;
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
      for (const auto& w : windows_) {
        w.second->HandleJoystickButtonEvents(&event->cbutton);
      }
    } break;
    case SDL_CONTROLLERDEVICEADDED: {
      AddGameController(event->cdevice.which);
      for (const auto& w : windows_) {
        w.second->HandleJoystickDeviceEvents(&event->cdevice);
      }
    } break;
    case SDL_CONTROLLERDEVICEREMOVED: {
      for (const auto& w : windows_) {
        w.second->HandleJoystickDeviceEvents(&event->cdevice);
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

void Application::Run() {
  runloop_.Run();
}

void Application::AddObserver(ApplicationObserver* observer) {
  observers_.insert(observer);
}

void Application::RemoveObserver(ApplicationObserver* observer) {
  observers_.erase(observer);
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
  InitializeFonts();
}

void Application::UninitializeImGui() {
  kiwi::base::SetPreEventHandlerForSDL2(kiwi::base::DoNothing());
  kiwi::base::SetRenderHandlerForSDL2(kiwi::base::DoNothing());
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
}

void Application::InitializeStyles() {
  ImGuiStyle& style = ImGui::GetStyle();
  style.ItemSpacing.x = 10;
}

void Application::AddWindowToEventHandler(WindowBase* window) {
  windows_.insert(std::make_pair(window->GetWindowID(), window));
}

void Application::RemoveWindowFromEventHandler(WindowBase* window) {
  windows_.erase(window->GetWindowID());
}
