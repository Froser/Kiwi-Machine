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

#include "rom_window.h"

#include <SDL_image.h>
#include <imgui.h>

static int g_window_id = 0;

ROMWindow::ROMWindow(SDL_Renderer* renderer,
                     ROMS roms,
                     kiwi::base::FilePath file)
    : roms_(roms), file_(file) {
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
  ImGui::Begin(GetUniqueName(file_.AsUTF8Unsafe(), window_id()).c_str(),
               nullptr, ImGuiWindowFlags_AlwaysAutoResize);

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
      if (rom.name == std::string("default")) {
        ImGui::TextUnformatted(rom.name);
      } else {
        ImGui::InputText(GetUniqueName("", id).c_str(), rom.name, rom.MAX);
      }

      ImGui::InputText(GetUniqueName("zh", id).c_str(), rom.zh, rom.MAX);
      ImGui::InputText(GetUniqueName("zh_hint", id).c_str(), rom.zh_hint,
                       rom.MAX);
      ImGui::InputText(GetUniqueName("ja", id).c_str(), rom.ja, rom.MAX);
      ImGui::InputText(GetUniqueName("ja_hint", id).c_str(), rom.ja_hint,
                       rom.MAX);
      ImGui::EndGroup();
      ImGui::BeginGroup();
      ImGui::SameLine();
      if (rom.cover_texture_)
        ImGui::Image(rom.cover_texture_, ImVec2(100, 100));
      else
        ImGui::Dummy(ImVec2(100, 100));
      ImGui::EndGroup();
      if (ImGui::Button(GetUniqueName(u8"删除此ROM", id).c_str())) {
        to_be_deleted_ = id;
      }

      ImGui::NewLine();
      ++id;
    }
  }

  if (ImGui::Button(GetUniqueName(u8"增加一个ROM", 0).c_str())) {
    roms_.push_back(ROM());
  }

  if (ImGui::Button(GetUniqueName(u8"关闭", 0).c_str())) {
    Close();
  }

  ImGui::End();
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