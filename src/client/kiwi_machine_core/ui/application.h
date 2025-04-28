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

#ifndef UI_APPLICATION_H_
#define UI_APPLICATION_H_

#include <gflags/gflags.h>
#include <kiwi_nes.h>
#include <map>
#include <set>

#include "models/nes_runtime.h"
#include "ui/window_base.h"
#include "utility/localization.h"
#include "utility/timer.h"

class WindowBase;

class ApplicationObserver {
 public:
  ApplicationObserver();
  virtual ~ApplicationObserver();

 public:
  virtual void OnPreRender(int since_last_frame_ms);
  virtual void OnPostRender(int render_elapsed_ms);
};

class Application {
 public:
#if defined(_WIN32)
  Application();
#endif
  Application(int& argc, char** argv);
  ~Application();

 public:
  static Application* Get();
  scoped_refptr<kiwi::base::SequencedTaskRunner> GetIOTaskRunner();
  // Initialize application's necessary data.
  void Initialize(kiwi::base::OnceClosure other_io_task,
                  kiwi::base::OnceClosure callback);

  void Run();
  void AddObserver(ApplicationObserver* observer);
  void RemoveObserver(ApplicationObserver* observer);
  void SetLanguage(SupportedLanguage language);

  const std::set<SDL_GameController*>& game_controllers() {
    return game_controllers_;
  }

  NESRuntimeID runtime_id() { return runtime_id_; }
  scoped_refptr<NESConfig> config() { return config_; }

  // If application's font has been changed, this method should be called.
  void FontChanged();

 private:
  void InitializeApplication(int& argc, char** argv);
  void UninitializeGameControllers();
  void AddGameController(int which);
  void RemoveGameController(int which);
  void InitializeImGui();
  void UninitializeImGui();
  void InitializeStyles();
  void InitializeRuntimeAndConfigs();
  void InitializeROMs();
  std::vector<kiwi::base::FilePath> GetPackagePathList();

  // Window management:
  friend class WindowBase;
  void AddWindowToEventHandler(WindowBase* window);
  void RemoveWindowFromEventHandler(WindowBase* window);

 private:
  void HandleEvent(SDL_Event* event);
  WindowBase* FindWindowFromID(uint32_t id);
  uint32_t FindIDFromWindow(WindowBase* window);
  void Render();

  // If application's language has been changed, this method should be called.
  void LocaleChanged();

 private:
  bool initialized_ = false;
  NESRuntimeID runtime_id_ = 0;
  scoped_refptr<NESConfig> config_;
  std::unique_ptr<kiwi::base::Thread> io_thread_;
  Timer frame_elapsed_counter_;
  Timer render_counter_;
  kiwi::base::SingleThreadTaskExecutor executor_;
  kiwi::base::RunLoop runloop_;
  kiwi::base::RepeatingCallback<void(SDL_Event*)> event_handler_;
  kiwi::base::RepeatingClosure render_handler_;
  std::map<uint32_t, WindowBase*> windows_;
  std::set<SDL_GameController*> game_controllers_;
  std::set<ApplicationObserver*> observers_;
};

#endif  // UI_APPLICATION_H_