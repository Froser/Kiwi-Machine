// Copyright (C) 2025 Yisi Yu
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

#include "util.h"

#include <spawn.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/files/file_path.h"

#include <vector>

#include "base/files/file_path.h"

extern char** environ;

namespace {

void LaunchProcessPosix(const std::vector<std::string>& argv) {
  pid_t pid;
  std::vector<char*> exec_argv;
  for (const auto& arg : argv) {
    exec_argv.push_back(const_cast<char*>(arg.c_str()));
  }
  exec_argv.push_back(nullptr);

  int status = posix_spawn(&pid, exec_argv[0], nullptr, nullptr,
                           exec_argv.data(), environ);
  if (status == 0) {
    // The process was spawned successfully. We don't wait for it to complete,
    // effectively launching it in the background.
    // If you needed to wait for the process to finish, you could call:
    // waitpid(pid, &status, 0);
  } else {
    // posix_spawn failed. You might want to log the error `status`.
  }
}

}  // namespace

void ShellOpen(const kiwi::base::FilePath& file) {
  std::vector<std::string> argv;
  argv.push_back("xdg-open");
  argv.push_back(file.AsUTF8Unsafe());
  LaunchProcessPosix(argv);
}

void ShellOpenDirectory(const kiwi::base::FilePath& file) {
  ShellOpen(file.DirName());
}

void RunExecutable(const kiwi::base::FilePath& bundle,
                   const std::vector<std::string>& args) {
  std::vector<std::string> argv;
  argv.push_back(bundle.AsUTF8Unsafe());
  for (const auto& arg : args) {
    argv.push_back(arg);
  }
  LaunchProcessPosix(argv);
}

std::vector<uint8_t> ReadImageAsJPGFromClipboard() {
  // TODO(yisiyu): Implement this.
  return std::vector<uint8_t>();
}