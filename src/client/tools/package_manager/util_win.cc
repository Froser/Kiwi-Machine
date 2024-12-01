// Copyright (C) 2024 Yisi Yu
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

#include <ShlObj.h>

#include "base/files/file_path.h"

kiwi::base::FilePath GetFontsPath() {
  PWSTR path = NULL;
  HRESULT hr = SHGetKnownFolderPath(FOLDERID_Fonts, 0, NULL, &path);
  if (SUCCEEDED(hr)) {
    return kiwi::base::FilePath(path);
  }

  return kiwi::base::FilePath();
}

kiwi::base::FilePath GetDefaultSavePath() {
  PWSTR path = NULL;
  HRESULT hr = SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &path);
  if (SUCCEEDED(hr)) {
    return kiwi::base::FilePath(path);
  }

  return kiwi::base::FilePath();
}

void ShellOpen(const kiwi::base::FilePath& file) {
  ShellExecuteW(NULL, L"open", file.value().c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void ShellOpenDirectory(const kiwi::base::FilePath& file) {
  ShellOpen(file.DirName());
}

void RunExecutable(const kiwi::base::FilePath& executable,
                   const std::vector<std::wstring>& args) {
  wchar_t current[MAX_PATH];
  GetModuleFileNameW(GetModuleHandleW(NULL), current, MAX_PATH);
  kiwi::base::FilePath current_path = kiwi::base::FilePath(current).DirName();

  std::wstring params;
  for (const auto& arg : args) {
    params += arg + L" ";
  }
  ShellExecuteW(NULL, L"open", executable.value().c_str(), params.c_str(),
                current_path.value().c_str(), SW_SHOWNORMAL);
}

std::vector<uint8_t> ReadImageAsJPGFromClipboard() {
  std::vector<uint8_t> result;
  if (OpenClipboard(NULL)) {
    bool avail = IsClipboardFormatAvailable(CF_DIB);
    if (avail) {
      HANDLE data_handle = GetClipboardData(CF_DIB);
      BITMAPINFOHEADER* header =
          static_cast<BITMAPINFOHEADER*>(GlobalLock(data_handle));
      size_t image_size =
          header->biCompression
              ? header->biSizeImage
              : header->biWidth * header->biHeight * header->biBitCount / 8;
      std::vector<uint8_t> buffer;
      for (size_t i = 0; i < image_size; i += 4) {
        buffer.push_back(*(reinterpret_cast<const char*>(header) + i + 2));
        buffer.push_back(*(reinterpret_cast<const char*>(header) + i + 1));
        buffer.push_back(*(reinterpret_cast<const char*>(header) + i));
      }
      result = ReadImageAsJPGFromImageData(header->biWidth, header->biHeight,
                                           header->biWidth * 3, buffer.data());
      GlobalUnlock(data_handle);
    }
    CloseClipboard();
  }
  return result;
}