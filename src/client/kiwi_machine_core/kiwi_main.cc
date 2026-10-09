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

#include "kiwi_main.h"

#include <SDL.h>
#if defined(__SWITCH__)
#include <glog/logging.h>
#endif
#include <kiwi_nes.h>

#if defined(__SWITCH__)
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#endif
#include "debug/debug_port.h"
#include "ui/application.h"
#include "ui/main_window.h"

#if defined(__IPHONEOS__)
#include <SDL_main.h>
#endif

#if defined(__SWITCH__)
namespace {

void ConfigureSwitchLogDirectory(const char* executable_path) {
  if (!executable_path || !executable_path[0])
    return;

  const kiwi::base::FilePath log_directory =
      kiwi::base::FilePath::FromUTF8Unsafe(executable_path)
          .DirName()
          .Append(FILE_PATH_LITERAL("logs"));
  if (kiwi::base::CreateDirectory(log_directory))
    FLAGS_log_dir = log_directory.AsUTF8Unsafe();
}

}  // namespace
#endif

#if defined(__IPHONEOS__)
int KiwiMain(int argc, char** argv) {
  int KiwiMainReal(int argc, char** argv);
  return SDL_UIKitRunApp(argc, argv, KiwiMainReal);
}
#define KiwiMain KiwiMainReal
#endif

#if defined(_WIN32)
#include <windows.h>
int WINAPI KiwiMain(HINSTANCE hInstance,
                    HINSTANCE hPrevInstance,
                    PWSTR pCmdLine,
                    int nCmdShow) {
  Application application;
#else
int KiwiMain(int argc, char** argv) {
#if defined(__SWITCH__)
  ConfigureSwitchLogDirectory(argv[0]);
#endif
  Application application(argc, argv);
#endif

  MainWindow main_window("Kiwi Machine", application.runtime_id(),
                         application.config());
  main_window.InitializeAsync(kiwi::base::DoNothing());
  application.Run();
  return 0;
}
