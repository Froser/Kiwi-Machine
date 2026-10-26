// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef EXPLORER_LOADER_H_
#define EXPLORER_LOADER_H_

#include <future>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "util.h"

struct ExplorerLoadResult {
  ROMSystem system = ROMSystem::kNES;
  std::vector<Explorer::File> files;
};

class ExplorerLoader {
 public:
  ExplorerLoader() = default;
  ~ExplorerLoader() = default;

  ExplorerLoader(const ExplorerLoader&) = delete;
  ExplorerLoader& operator=(const ExplorerLoader&) = delete;

  bool Start(const kiwi::base::FilePath& roms_path,
             const kiwi::base::FilePath& zipped_path,
             ROMSystem system);
  std::optional<ExplorerLoadResult> Poll();

  bool loading() const { return loading_; }
  const std::string& error() const { return error_; }

 private:
  bool loading_ = false;
  std::string error_;
  std::future<ExplorerLoadResult> future_;
};

#endif  // EXPLORER_LOADER_H_
