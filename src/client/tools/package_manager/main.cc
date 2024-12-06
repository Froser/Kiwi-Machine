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

#include <SDL.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <gflags/gflags.h>
#include <imgui.h>
#include <set>
#include <string>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "rom_window.h"
#include "util.h"

#if defined(_WIN32)
#include <windows.h>
#endif

DECLARE_string(output);
DEFINE_string(explorer_dir, "", "A default explorer directory path to view.");

SDL_Renderer* g_renderer;
std::vector<ROMWindow> g_rom_windows;
kiwi::base::FilePath g_dropped_jpg;
kiwi::base::FilePath g_dropped_rom;

struct {
  Explorer explorer;
  char explorer_dir[ROM::MAX];
  char compare_dir[ROM::MAX];
  bool first_open = true;
  bool explorer_opened = false;
  Explorer::File* selected_item = nullptr;
} g_explorer;

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

void CreateROMWindow(ROMS roms,
                     kiwi::base::FilePath file,
                     bool new_rom,
                     bool fetch_image) {
  ROMWindow window(g_renderer, roms, file);
  if (new_rom)
    window.NewRom();
  if (fetch_image && window.first_rom()) {
    window.TryFetchCoverByName(*window.first_rom(),
                               kiwi::base::FilePath::FromUTF8Unsafe(
                                   window.first_rom()->nes_file_name));
  }
  g_rom_windows.push_back(std::move(window));
}

void CreateROMFromNES(const kiwi::base::FilePath& rom_path) {
  auto nes_data = kiwi::base::ReadFileToBytes(rom_path);
  if (nes_data) {
    ROM rom;
    strcpy(rom.nes_file_name, rom_path.BaseName().AsUTF8Unsafe().c_str());
    rom.key = "default";
    rom.nes_data = std::move(*nes_data);
    FillRomDetailsAutomatically(rom, rom_path.BaseName());
    CreateROMWindow(std::vector<ROM>{std::move(rom)}, kiwi::base::FilePath(),
                    false, true);
  }
}

SDL_Window* CreateMainWindow() {
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

  SDL_Window* window = CreateMainWindow();
  if (!window) {
    return false;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    SDL_DestroyWindow(window);
    return false;
  }

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
    if (IsZipExtension(dropped_file)) {
      kiwi::base::FilePath file =
          kiwi::base::FilePath::FromUTF8Unsafe(dropped_file);
      CreateROMWindow(ReadZipFromFile(file), file, false, false);
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

void PaintGlobal() {
  ImGui::Begin(u8"全局设置");
  ImGui::TextUnformatted(u8"包管理器，方便轻松打包NES资源。");
  ImGui::TextUnformatted(u8"准备工作：");

  ImGui::Bullet();
  ImGui::SameLine();
  ImGui::TextUnformatted(u8"将KiwiMachine的非内嵌版拷贝到本程序路径下");

  ImGui::Bullet();
  ImGui::SameLine();
  ImGui::TextUnformatted(
      u8"为了能够自动注音，需要安装Python3，并通过pip3安装pinyin依赖：");
  ImGui::TextUnformatted("\t");
  ImGui::SameLine();
  ImGui::Bullet();
  ImGui::TextUnformatted("pip3 install pinyin");
  ImGui::TextUnformatted("\t");
  ImGui::SameLine();
  ImGui::Bullet();
  ImGui::TextUnformatted("pip3 install pykakasi");

  ImGui::NewLine();
  ImGui::TextUnformatted(u8"全局设置");
  ImGui::TextUnformatted(u8"游戏封面数据库路径");
  ImGui::InputText("##BoxartPath", GetSettings().boxarts_package,
                   sizeof(GetSettings().boxarts_package));
  ImGui::TextUnformatted(u8"最终输出路径 (--output)");
  ImGui::InputText("##Output", GetSettings().zip_output_path,
                   sizeof(GetSettings().zip_output_path));
  ImGui::End();
}

void PaintExplorer() {
  if (g_explorer.explorer_opened) {
    ImGui::Begin(u8"目录浏览器##Explorer", &g_explorer.explorer_opened);
    ImGui::InputText(u8"文件夹路径##", g_explorer.explorer_dir,
                     sizeof(g_explorer.explorer_dir));
    ImGui::SameLine();
    if (ImGui::Button(u8"刷新") || g_explorer.first_open) {
      InitializeExplorerFiles(
          kiwi::base::FilePath::FromUTF8Unsafe(g_explorer.explorer_dir),
          kiwi::base::FilePath::FromUTF8Unsafe(g_explorer.compare_dir),
          g_explorer.explorer.explorer_files);
      g_explorer.first_open = false;
    }

    ImGui::InputText(u8"对比路径##", g_explorer.compare_dir,
                     sizeof(g_explorer.compare_dir));

    constexpr int kItemCount = 20;
    if (ImGui::BeginListBox(
            u8"文件##Files",
            ImVec2(-FLT_MIN,
                   kItemCount * ImGui::GetTextLineHeightWithSpacing()))) {
      for (auto& item : g_explorer.explorer.explorer_files) {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              item.supported
                                  ? (item.matched ? ImColor(0, 255, 0).Value
                                                  : ImColor(255, 0, 0).Value)
                                  : ImColor(127, 127, 127).Value);
        if (ImGui::Selectable(item.title.c_str(), &item.selected,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
          for (auto& i : g_explorer.explorer.explorer_files) {
            if (&i != &item)
              i.selected = false;
          }
          if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            kiwi::base::FilePath nes_file = item.dir.Append(
                kiwi::base::FilePath::FromUTF8Unsafe(item.title));
            CreateROMFromNES(nes_file);
          }
          g_explorer.selected_item = &item;
        }
        ImGui::PopStyleColor();
      }
      ImGui::EndListBox();

      if (g_explorer.selected_item) {
        ImGui::Text(u8"Mapper: %s, 是否支持：%s",
                    g_explorer.selected_item->mapper.c_str(),
                    g_explorer.selected_item->supported ? u8"是" : u8"否");
      }

      if (!g_explorer.selected_item || !g_explorer.selected_item->matched)
        ImGui::BeginDisabled();
      if (ImGui::Button(u8"打开对应的压缩包")) {
        const kiwi::base::FilePath& file =
            g_explorer.selected_item->compared_zip_path;
        if (IsZipExtension(
                g_explorer.selected_item->compared_zip_path.AsUTF8Unsafe())) {
          CreateROMWindow(ReadZipFromFile(file), file, false, false);
        }
      }
      if (!g_explorer.selected_item || !g_explorer.selected_item->matched)
        ImGui::EndDisabled();

      ImGui::NewLine();
      ImGui::TextUnformatted(u8"颜色说明：");
      ImGui::PushStyleColor(ImGuiCol_Text, ImColor(255, 0, 0).Value);
      ImGui::TextUnformatted(u8"红色表示文件在对比路径中不存在");
      ImGui::PopStyleColor();

      ImGui::PushStyleColor(ImGuiCol_Text, ImColor(0, 255, 0).Value);
      ImGui::TextUnformatted(u8"绿色表示文件在对比路径中已存在");
      ImGui::PopStyleColor();

      ImGui::PushStyleColor(ImGuiCol_Text, ImColor(127, 127, 127).Value);
      ImGui::TextUnformatted(u8"灰色表示文件不支持被打开");
      ImGui::PopStyleColor();

      ImGui::End();
    }
  }
}

void Render() {
  SDL_RenderClear(g_renderer);
  SDL_SetRenderDrawColor(g_renderer, 0x00, 0x00, 0x00, 0x00);
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu(u8"资源包")) {
      if (ImGui::MenuItem(u8"新建压缩包")) {
        CreateROMWindow(ROMS(), kiwi::base::FilePath(), true, false);
      }
      if (ImGui::MenuItem(u8"目录浏览器")) {
        g_explorer.explorer_opened = true;
      }

      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

  PaintGlobal();
  PaintExplorer();

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

  if (!g_dropped_rom.empty()) {
    CreateROMFromNES(g_dropped_rom);
  }
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
char** g_argv = nullptr;

void CleanupArgs() {
  for (size_t i = 0; i < __argc; ++i) {
    free(g_argv[i]);
  }
  free(g_argv);
}

int WINAPI wWinMain(HINSTANCE hInstance,
                    HINSTANCE hPrevInstance,
                    PWSTR pCmdLine,
                    int nCmdShow) {
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
  atexit(CleanupArgs);

  gflags::ParseCommandLineFlags(&__argc, &g_argv, true);

#else
int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
#endif

  InitSDL();

  if (!FLAGS_explorer_dir.empty()) {
    memcpy(g_explorer.explorer_dir, FLAGS_explorer_dir.data(),
           FLAGS_explorer_dir.size());
  }
  if (!FLAGS_output.empty()) {
    memcpy(g_explorer.compare_dir, FLAGS_output.data(), FLAGS_output.size());
  }

  bool running = true;
  while (running) {
    if (!HandleEvents())
      running = false;
    Render();
  }

  Cleanup();
  return 0;
}
