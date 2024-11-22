﻿// Copyright (C) 2024 Yisi Yu
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

#include "rom_window.h"

#include <SDL_image.h>
#include <imgui.h>

#include "base/files/file_util.h"
#include "base/strings/string_util.h"

static int g_window_id = 0;

// Implements in main.cc
kiwi::base::FilePath GetDroppedJPG();
void ClearDroppedJPG();
kiwi::base::FilePath GetDroppedROM();
void ClearDroppedROM();

SDL_Texture* EmptyTexture(SDL_Renderer* renderer = nullptr) {
  static SDL_Texture* texture = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 1, 1);
  return texture;
}

ROMWindow::ROMWindow(SDL_Renderer* renderer,
                     ROMS roms,
                     kiwi::base::FilePath file)
    : roms_(roms), file_(file), renderer_(renderer) {
  EmptyTexture(renderer);

  strncpy(save_path_, GetDefaultSavePath().AsUTF8Unsafe().c_str(), ROM::MAX);
  window_id_ = g_window_id++;
  for (auto& rom : roms_) {
    if (!rom.cover_data.empty()) {
      SDL_RWops* res =
          SDL_RWFromMem(const_cast<unsigned char*>(rom.cover_data.data()),
                        rom.cover_data.size());
      SDL_Texture* texture = IMG_LoadTextureTyped_RW(renderer, res, 1, nullptr);
      SDL_SetTextureScaleMode(texture, SDL_ScaleModeBest);
      rom.cover_texture_ = texture;
    }
  }
}

ROMWindow::~ROMWindow() {
  for (auto& rom : roms_) {
    if (rom.cover_texture_) {
      SDL_DestroyTexture(rom.cover_texture_);
      rom.cover_texture_ = nullptr;
    }
  }
}

void ROMWindow::Paint() {
  std::string window_name = GetUniqueName(file_.AsUTF8Unsafe(), window_id());
  ImGui::Begin(window_name.c_str(), nullptr);
  ImGui::SetWindowSize(window_name.c_str(), ImVec2(800, 700));

  if ((check_close_ && ImGui::IsWindowFocused())) {
    closed_ = true;
    ImGui::End();
    return;
  }

  // Render manifest
  if (!roms_.empty()) {
    int id = 0;
    for (auto& rom : roms_) {
      ImGui::BeginGroup();
      ImGui::TextUnformatted(rom.key.c_str());

      ImGui::InputText(GetUniqueName(u8"中文标题", id).c_str(), rom.zh,
                       rom.MAX);
      ImGui::InputText(GetUniqueName(u8"中文提示", id).c_str(), rom.zh_hint,
                       rom.MAX);
      ImGui::InputText(GetUniqueName(u8"日文标题", id).c_str(), rom.ja,
                       rom.MAX);
      ImGui::InputText(GetUniqueName(u8"日文提示", id).c_str(), rom.ja_hint,
                       rom.MAX);
      ImGui::EndGroup();
      ImGui::BeginGroup();
      ImGui::SameLine();
      if (rom.cover_texture_) {
        ImGui::Image(rom.cover_texture_, ImVec2(100, 100));
      } else {
        ImGui::Image(EmptyTexture(), ImVec2(100, 100), ImVec2(0, 0),
                     ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1));
      }

      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone)) {
        kiwi::base::FilePath path = GetDroppedJPG();
        if (!path.empty()) {
          if (rom.cover_texture_)
            SDL_DestroyTexture(rom.cover_texture_);

          SDL_RWops* res = SDL_RWFromFile(path.AsUTF8Unsafe().c_str(), "rb");
          if (res) {
            SDL_Texture* texture =
                IMG_LoadTextureTyped_RW(renderer_, res, 1, nullptr);
            SDL_SetTextureScaleMode(texture, SDL_ScaleModeBest);
            rom.cover_texture_ = texture;
            auto cover_data = kiwi::base::ReadFileToBytes(path);
            SDL_assert(cover_data);
            rom.cover_data = std::move(*cover_data);
          }
        }
        ClearDroppedJPG();
      }
      ImGui::EndGroup();

      ImGui::BeginGroup();
      ImGui::TextUnformatted(u8"将nes拖拽到此处进行增加/修改");
      ImGui::InputText(GetUniqueName(u8"ROM名称", id).c_str(),
                       rom.nes_file_name, rom.MAX);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone)) {
        kiwi::base::FilePath path = GetDroppedROM();
        if (!path.empty()) {
          std::optional<std::vector<uint8_t>> rom_contents =
              kiwi::base::ReadFileToBytes(path);
          if (rom_contents) {
            strncpy(rom.nes_file_name, path.BaseName().AsUTF8Unsafe().c_str(),
                    rom.MAX);
            rom.nes_data = std::move(*rom_contents);
            if (kiwi::base::EqualsCaseInsensitiveASCII(rom.key, "default") != 0)
              rom.key = path.BaseName().RemoveExtension().AsUTF8Unsafe();
          }
        }
        ClearDroppedROM();
      }

      std::string mapper;
      bool supported = IsMapperSupported(rom.nes_data, mapper);
      ImGui::Text(u8"Mapper: %s", mapper.c_str());
      ImGui::Text(u8"KiwiMachine是否支持打开: %s", supported ? u8"是" : u8"否");
      ImGui::EndGroup();

      if (ImGui::Button(GetUniqueName(u8"删除此ROM", id).c_str())) {
        to_be_deleted_ = id;
      }

      ImGui::NewLine();
      ++id;
    }
  }

  if (ImGui::Button(GetUniqueName(u8"增加一个ROM", 0).c_str())) {
    ROM new_rom;
    if (roms_.empty())
      new_rom.key = "default";
    roms_.push_back(std::move(new_rom));
  }

  ImGui::NewLine();
  ImGui::InputText(GetUniqueName(u8"另存文件目录", 0).c_str(), save_path_,
                   sizeof(save_path_));
  if (ImGui::Button(GetUniqueName(u8"保存", 0).c_str())) {
    if (kiwi::base::FilePath output =
            WriteZip(kiwi::base::FilePath::FromUTF8Unsafe(save_path_), roms_);
        !output.empty()) {
      generated_packaged_path_ = output;
      show_message_box_ = true;
    } else {
      generated_packaged_path_.clear();
      show_message_box_ = true;
    }
  }

  ImGui::SameLine();
  if (ImGui::Button(GetUniqueName(u8"关闭", 0).c_str())) {
    Close();
  }

  ImGui::End();

  if (show_message_box_) {
    ImGui::Begin(u8"提示", &show_message_box_,
                 ImGuiWindowFlags_AlwaysAutoResize);
    if (!generated_packaged_path_.empty()) {
      ImGui::Text(u8"保存文件成功：%s",
                  generated_packaged_path_.AsUTF8Unsafe().c_str());
      if (ImGui::Button(GetUniqueName(u8"打开zip", 0).c_str())) {
        ShellOpen(generated_packaged_path_);
      }
      ImGui::SameLine();
      if (ImGui::Button(GetUniqueName(u8"打开zip所在文件夹", 0).c_str())) {
        ShellOpenDirectory(generated_packaged_path_);
      }
      ImGui::SameLine();
      if (ImGui::Button(GetUniqueName(u8"测试", 0).c_str())) {
        kiwi::base::FilePath package_path =
            PackZip(generated_packaged_path_, GetDefaultSavePath());
        if (!package_path.empty()) {
#if BUILDFLAG(IS_MAC)
          kiwi::base::FilePath kiwi_machine(
              FILE_PATH_LITERAL("kiwi_machine.app"));
          RunExecutable(
              kiwi_machine,
              {"--test-pak=" + package_path.AsUTF8Unsafe(), "--has_menu"});
#elif BUILDFLAG(IS_WIN)
          kiwi::base::FilePath kiwi_machine(
              FILE_PATH_LITERAL("kiwi_machine.exe"));
          RunExecutable(kiwi_machine,
                        {L"--test-pak=" + package_path.value(), L"--has_menu"});
#else
          kiwi::base::FilePath kiwi_machine(FILE_PATH_LITERAL("kiwi_machine"));
          RunExecutable(
              kiwi_machine,
              {"--test-pak=" + package_path.AsUTF8Unsafe(), "--has_menu"});
#endif
        }
      }
      ImGui::SameLine();
      if (ImGui::Button(GetUniqueName(u8"确认", 0).c_str())) {
        show_message_box_ = false;
      }
    } else {
      ImGui::TextUnformatted(u8"保存文件失败");
    }
    ImGui::End();
  }
}

void ROMWindow::Close() {
  check_close_ = true;
}

std::string ROMWindow::GetUniqueName(const std::string& name, int unique_id) {
  std::string unique_part = std::to_string(unique_id) +
                            std::to_string(reinterpret_cast<uintptr_t>(this));
  return name + "##" + name + unique_part;
}

void ROMWindow::Painted() {
  if (to_be_deleted_ >= 0) {
    roms_.erase(roms_.begin() + to_be_deleted_);
  }
  to_be_deleted_ = -1;
}