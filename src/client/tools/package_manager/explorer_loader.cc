// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "explorer_loader.h"

#include <chrono>
#include <exception>

bool ExplorerLoader::Start(const kiwi::base::FilePath& roms_path,
                           const kiwi::base::FilePath& zipped_path,
                           ROMSystem system) {
  if (loading_)
    return false;

  error_.clear();
  loading_ = true;
  try {
    future_ =
        std::async(std::launch::async, [roms_path, zipped_path, system]() {
          ExplorerLoadResult result;
          result.system = system;
          InitializeExplorerFiles(roms_path, zipped_path, system, result.files);
          return result;
        });
  } catch (const std::exception& error) {
    loading_ = false;
    error_ = error.what();
    return false;
  }
  return true;
}

std::optional<ExplorerLoadResult> ExplorerLoader::Poll() {
  if (!loading_ || !future_.valid())
    return std::nullopt;
  if (future_.wait_for(std::chrono::milliseconds(0)) !=
      std::future_status::ready) {
    return std::nullopt;
  }

  loading_ = false;
  try {
    return future_.get();
  } catch (const std::exception& error) {
    error_ = error.what();
  } catch (...) {
    error_ = "Unknown explorer loading error";
  }
  return std::nullopt;
}
