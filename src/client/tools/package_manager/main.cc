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

#include <SDL.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <imgui.h>
#include <string>

#include "rom_window.h"

SDL_Window* g_window;
SDL_Renderer* g_renderer;
std::vector<ROMWindow> g_rom_windows;
kiwi::base::FilePath g_dropped_jpg;
kiwi::base::FilePath g_dropped_rom;

kiwi::base::FilePath GetDroppedJPG() {
  return g_dropped_jpg;
}
void ClearDroppedJPG() {
  g_dropped_jpg.clear();
}
kiwi::base::FilePath GetDroppedROM() {
  return g_dropped_rom;
}
void ClearDroppedROM() {
  g_dropped_rom.clear();
}

void CreateROMWindow(ROMS roms, kiwi::base::FilePath file) {
  ROMWindow window(g_renderer, roms, file);
  g_rom_windows.push_back(std::move(window));
}

SDL_Window* CreateWindow() {
  SDL_Window* window =
      SDL_CreateWindow(u8"KiwiMachine 资源包管理器", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, 1024, 768, SDL_WINDOW_SHOWN);
  return window;
}

void RemoveClosedWindows() {
  for (auto iter = g_rom_windows.begin(); iter != g_rom_windows.end();) {
    if (iter->closed())
      iter = g_rom_windows.erase(iter);
    else
      ++iter;
  }
}

bool InitSDL() {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    return false;
  }

  SDL_Window* window = CreateWindow();
  if (!window) {
    return false;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    SDL_DestroyWindow(window);
    return false;
  }

  g_window = window;
  g_renderer = renderer;

  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
#if __APPLE__
  io.Fonts->AddFontFromFileTTF("/System/Library/Fonts/STHeiti Light.ttc", 16,
                               nullptr, io.Fonts->GetGlyphRangesChineseFull());
#endif
#if _WIN32
  io.Fonts->AddFontFromFileTTF(GetFontsPath()
                                   .Append(FILE_PATH_LITERAL("msyh.ttc"))
                                   .AsUTF8Unsafe()
                                   .c_str(),
                               16, nullptr,
                               io.Fonts->GetGlyphRangesChineseFull());
#endif

  ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer2_Init(renderer);

  return true;
}

void HandleDrop(SDL_Event& event) {
  char* dropped_file = event.drop.file;
  if (dropped_file) {
    if (IsPackageExtension(dropped_file)) {
      kiwi::base::FilePath file =
          kiwi::base::FilePath::FromUTF8Unsafe(dropped_file);
      CreateROMWindow(ReadZipFromFile(file), file);
    } else if (IsJPEGExtension(dropped_file)) {
      g_dropped_jpg = kiwi::base::FilePath::FromUTF8Unsafe(dropped_file);
    } else if (IsNESExtension(dropped_file)) {
      g_dropped_rom = kiwi::base::FilePath::FromUTF8Unsafe(dropped_file);
    }
    SDL_free(dropped_file);
  }
}

bool HandleEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL2_ProcessEvent(&event);
    switch (event.type) {
      case SDL_QUIT:
        return false;
      case SDL_DROPFILE:
        HandleDrop(event);
        break;
      default:
        break;
    }
  }
  return true;
}

void Render() {
  SDL_RenderClear(g_renderer);
  SDL_SetRenderDrawColor(g_renderer, 0x00, 0x00, 0x00, 0x00);
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
  // ImGui::ShowDemoWindow();

  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu(u8"资源包")) {
      if (ImGui::MenuItem(u8"新建", "CTRL+N")) {
        CreateROMWindow(ROMS(), kiwi::base::FilePath());
      }
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

  for (auto& rom_window : g_rom_windows) {
    rom_window.Paint();
  }

  ImGui::Render();
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
  SDL_RenderSetScale(g_renderer, ImGui::GetIO().DisplayFramebufferScale.x,
                     ImGui::GetIO().DisplayFramebufferScale.y);
  SDL_RenderPresent(g_renderer);

  for (auto& rom_window : g_rom_windows) {
    rom_window.Painted();
  }
  RemoveClosedWindows();
  ClearDroppedJPG();
  ClearDroppedROM();
}

void Cleanup() {
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_Quit();
}

#if defined(_WIN32)
#include <windows.h>
int WINAPI wWinMain(HINSTANCE hInstance,
                    HINSTANCE hPrevInstance,
                    PWSTR pCmdLine,
                    int nCmdShow) {
#else
int main(int argc, char** argv) {
#endif
  InitSDL();

  bool running = true;
  while (running) {
    if (!HandleEvents())
      running = false;
    Render();
  }

  Cleanup();
  return 0;
}