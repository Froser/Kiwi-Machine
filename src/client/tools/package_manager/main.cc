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
#include "workspace.h"

DEFINE_string(km_path, "", "Kiwi-Machine executable/bundle directory.");

#if defined(_WIN32)
#include <windows.h>
#endif

SDL_Renderer* g_renderer;
std::vector<ROMWindow> g_rom_windows;
kiwi::base::FilePath g_dropped_jpg;
kiwi::base::FilePath g_dropped_rom;

struct {
  Explorer explorer;
  bool first_open = true;
  bool explorer_opened = false;
  bool ignore_marked = false;
  Explorer::File* selected_item = nullptr;
} g_explorer;

void NotifySaved(const kiwi::base::FilePath& updated_zip_file) {
  UpdateExplorerFiles(updated_zip_file, g_explorer.explorer.explorer_files);
}

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
      SDL_CreateWindow(U8("KiwiMachine 资源包管理器"), SDL_WINDOWPOS_UNDEFINED,
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
  ImGui::Begin(U8("全局设置"));
  ImGui::TextUnformatted(U8("包管理器，方便轻松打包NES资源。"));
  ImGui::TextUnformatted(U8("准备工作："));

  ImGui::Bullet();
  ImGui::SameLine();
  ImGui::TextUnformatted(U8("将KiwiMachine的非内嵌版拷贝到本程序路径下"));

  ImGui::Bullet();
  ImGui::SameLine();
  ImGui::TextUnformatted(
      U8("为了能够自动注音，需要安装Python3，并通过pip3安装pinyin依赖："));
  ImGui::TextUnformatted("\t");
  ImGui::SameLine();
  ImGui::Bullet();
  ImGui::TextUnformatted("pip3 install pinyin");
  ImGui::TextUnformatted("\t");
  ImGui::SameLine();
  ImGui::Bullet();
  ImGui::TextUnformatted("pip3 install pykakasi");

  ImGui::NewLine();
  ImGui::TextUnformatted(U8("工作空间 (--workspace)"));
  ImGui::InputText("##Workspace", GetWorkspace().workspace_dir,
                   sizeof(GetWorkspace().workspace_dir));
  ImGui::SameLine();
  if (ImGui::Button(U8("加载工作空间"))) {
    kiwi::base::FilePath manifest_path =
        kiwi::base::FilePath::FromUTF8Unsafe(GetWorkspace().workspace_dir)
            .Append(FILE_PATH_LITERAL("manifest.json"));
    GetWorkspace().ReadFromManifest(manifest_path);
  }

  ImGui::Bullet();
  ImGui::SameLine();
  ImGui::Text(U8("Zip包路径: %s"),
              GetWorkspace().GetZippedPath().AsUTF8Unsafe().c_str());

  ImGui::Bullet();
  ImGui::SameLine();
  ImGui::Text(U8("最终包输出路径: %s"),
              GetWorkspace().GetPackageOutputPath().AsUTF8Unsafe().c_str());

  ImGui::Bullet();
  ImGui::SameLine();
  ImGui::Text(U8("封面数据库: %s"),
              GetWorkspace().GetNESBoxartsPath().AsUTF8Unsafe().c_str());
  ImGui::End();
}

std::string g_last_pack_result_str;
kiwi::base::FilePath g_last_pack_dir;
void PaintExplorer() {
  if (g_explorer.explorer_opened) {
    ImGui::Begin(U8("目录浏览器##Explorer"), &g_explorer.explorer_opened);
    ImGui::Text(U8("NES Roms 路径: %s"),
                GetWorkspace().GetNESRomsPath().AsUTF8Unsafe().c_str());
    ImGui::SameLine();
    if (ImGui::Button(U8("与打包路径对比")) || g_explorer.first_open) {
      InitializeExplorerFiles(GetWorkspace().GetNESRomsPath(),
                              GetWorkspace().GetZippedPath(),
                              g_explorer.explorer.explorer_files);
      g_explorer.first_open = false;
    }

    ImGui::Text(U8("打包路径: %s"),
                GetWorkspace().GetZippedPath().AsUTF8Unsafe().c_str());

    constexpr int kItemCount = 20;
    static const char* kPrefix[] = {U8("(不支持) "), U8("(不关心) "),
                                    U8("(不完美) "), U8("(重复) ")};

    if (ImGui::BeginListBox(
            U8("文件##Files"),
            ImVec2(-FLT_MIN,
                   kItemCount * ImGui::GetTextLineHeightWithSpacing()))) {
      for (auto& item : g_explorer.explorer.explorer_files) {
        if (g_explorer.ignore_marked && item.mark != Explorer::Mark::kNoMark)
          continue;

        const char* prefix =
            !item.supported
                ? (!g_explorer.ignore_marked ? kPrefix[0] : "")
                : (item.mark == Explorer::Mark::kUninterested
                       ? kPrefix[1]
                       : (item.mark == Explorer::Mark::kImprefect
                              ? kPrefix[2]
                              : (item.mark == Explorer::Mark::kDuplicated
                                     ? kPrefix[3]
                                     : "")));
        ImGui::PushStyleColor(
            ImGuiCol_Text,
            (item.supported && item.mark != Explorer::Mark::kUninterested &&
             item.mark != Explorer::Mark::kImprefect &&
             item.mark != Explorer::Mark::kDuplicated)
                ? (item.matched ? ImColor(0, 255, 0).Value
                                : ImColor(255, 0, 0).Value)
                : ImColor(127, 127, 127).Value);
        if (ImGui::Selectable((prefix + item.title).c_str(), &item.selected,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
          for (auto& i : g_explorer.explorer.explorer_files) {
            if (&i != &item)
              i.selected = false;
          }
          if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            kiwi::base::FilePath nes_file = item.dir.Append(
                kiwi::base::FilePath::FromUTF8Unsafe(item.title));
            if (!g_explorer.selected_item ||
                !g_explorer.selected_item->matched) {
              // Creates a new one if no zip matched
              CreateROMFromNES(nes_file);
            } else {
              // Creates from a matched path
              const kiwi::base::FilePath& file =
                  g_explorer.selected_item->compared_zip_path;
              if (IsZipExtension(g_explorer.selected_item->compared_zip_path
                                     .AsUTF8Unsafe())) {
                CreateROMWindow(ReadZipFromFile(file), file, false, false);
              } else {
                // Not a zip file, so we create a new one.
                CreateROMFromNES(nes_file);
              }
            }
          }
          g_explorer.selected_item = &item;
        }
        ImGui::PopStyleColor();
      }

      ImGui::EndListBox();

      if (g_explorer.selected_item) {
        ImGui::Text(U8("Mapper: %s, 是否支持：%s"),
                    g_explorer.selected_item->mapper.c_str(),
                    g_explorer.selected_item->supported ? U8("是") : U8("否"));
      }

      {
        if (!g_explorer.selected_item)
          ImGui::BeginDisabled();
        if (ImGui::Button(U8("以此NES开始制作压缩包"))) {
          kiwi::base::FilePath nes_file = g_explorer.selected_item->dir.Append(
              kiwi::base::FilePath::FromUTF8Unsafe(
                  g_explorer.selected_item->title));
          CreateROMFromNES(nes_file);
        }
        if (!g_explorer.selected_item)
          ImGui::EndDisabled();
        ImGui::SameLine();
      }

      {
        if (!g_explorer.selected_item || !g_explorer.selected_item->matched)
          ImGui::BeginDisabled();
        if (ImGui::Button(U8("打开对应的压缩包"))) {
          const kiwi::base::FilePath& file =
              g_explorer.selected_item->compared_zip_path;
          if (IsZipExtension(
                  g_explorer.selected_item->compared_zip_path.AsUTF8Unsafe())) {
            CreateROMWindow(ReadZipFromFile(file), file, false, false);
          }
        }
        if (!g_explorer.selected_item || !g_explorer.selected_item->matched)
          ImGui::EndDisabled();
      }

      ImGui::SameLine();
      if (ImGui::Button(U8("对文件夹打包"))) {
        auto result =
            PackEntireDirectory(GetWorkspace().GetZippedPath(),
                                GetWorkspace().GetPackageOutputPath());
        if (!result.empty()) {
          g_last_pack_result_str = U8("打包成功。生成文件：\n");
          g_last_pack_dir = GetWorkspace().GetPackageOutputPath();
          for (const auto& path : result) {
            g_last_pack_result_str += path.AsUTF8Unsafe() + "\n";
          }
        } else {
          g_last_pack_result_str = U8("打包失败。");
          g_last_pack_dir.clear();
        }
      }

      {
        static const char* kMarkedList[]{
            U8("没有标记"),
            U8("标记为忽略-不感兴趣的游戏"),
            U8("标记为忽略-未完全模拟的游戏"),
            U8("标记位忽略-重复的游戏"),
        };

        const char* marked_desc = U8("未选择");
        if (g_explorer.selected_item) {
          switch (g_explorer.selected_item->mark) {
            case Explorer::Mark::kNoMark:
              marked_desc = kMarkedList[0];
              break;
            case Explorer::Mark::kUninterested:
              marked_desc = kMarkedList[1];
              break;
            case Explorer::Mark::kImprefect:
              marked_desc = kMarkedList[2];
              break;
            case Explorer::Mark::kDuplicated:
              marked_desc = kMarkedList[3];
            default:
              SDL_assert(false);
              break;
          }
        } else {
          ImGui::BeginDisabled();
        }

        if (ImGui::BeginCombo(U8("##Marked"), marked_desc)) {
          if (ImGui::Selectable(
                  kMarkedList[0],
                  g_explorer.selected_item && g_explorer.selected_item->mark ==
                                                  Explorer::Mark::kNoMark)) {
            g_explorer.selected_item->mark = Explorer::Mark::kNoMark;
            UpdateMarks(GetWorkspace().GetNESRomsPath(),
                        g_explorer.explorer.explorer_files);
          }
          if (ImGui::Selectable(kMarkedList[1],
                                g_explorer.selected_item &&
                                    g_explorer.selected_item->mark ==
                                        Explorer::Mark::kUninterested)) {
            g_explorer.selected_item->mark = Explorer::Mark::kUninterested;
            UpdateMarks(GetWorkspace().GetNESRomsPath(),
                        g_explorer.explorer.explorer_files);
          }
          if (ImGui::Selectable(
                  kMarkedList[2],
                  g_explorer.selected_item && g_explorer.selected_item->mark ==
                                                  Explorer::Mark::kImprefect)) {
            g_explorer.selected_item->mark = Explorer::Mark::kImprefect;
            UpdateMarks(GetWorkspace().GetNESRomsPath(),
                        g_explorer.explorer.explorer_files);
          }
          if (ImGui::Selectable(kMarkedList[3],
                                g_explorer.selected_item &&
                                    g_explorer.selected_item->mark ==
                                        Explorer::Mark::kDuplicated)) {
            g_explorer.selected_item->mark = Explorer::Mark::kDuplicated;
            UpdateMarks(GetWorkspace().GetNESRomsPath(),
                        g_explorer.explorer.explorer_files);
          }
          ImGui::EndCombo();
        }

        if (!g_explorer.selected_item) {
          ImGui::EndDisabled();
        }

        ImGui::SameLine();
        ImGui::Checkbox(U8("忽略被标记的项"), &g_explorer.ignore_marked);
      }

      if (!g_last_pack_dir.empty()) {
        ImGui::TextUnformatted(g_last_pack_result_str.c_str());
        if (ImGui::Button(U8("测试包##TestPackage"))) {
          kiwi::base::FilePath kiwi_machine_path_from_cmdline;
          if (!FLAGS_km_path.empty()) {
            kiwi_machine_path_from_cmdline =
                kiwi::base::FilePath::FromUTF8Unsafe(FLAGS_km_path);
          }
#if BUILDFLAG(IS_MAC)
          kiwi::base::FilePath kiwi_machine(
              FILE_PATH_LITERAL("kiwi_machine.app"));
          if (!kiwi_machine_path_from_cmdline.empty())
            kiwi_machine = kiwi_machine_path_from_cmdline;
          RunExecutable(kiwi_machine,
                        {"--package-dir=" + g_last_pack_dir.AsUTF8Unsafe(),
                         "--enable_debug"});
#elif BUILDFLAG(IS_WIN)
          kiwi::base::FilePath kiwi_machine(
              FILE_PATH_LITERAL("kiwi_machine.exe"));
          if (!kiwi_machine_path_from_cmdline.empty())
            kiwi_machine = kiwi_machine_path_from_cmdline;
          RunExecutable(kiwi_machine,
                        {L"--package-dir=\"" + g_last_pack_dir.value() + L"\"",
                         L"--enable_debug"});
#else
          kiwi::base::FilePath kiwi_machine(FILE_PATH_LITERAL("kiwi_machine"));
          if (!kiwi_machine_path_from_cmdline.empty())
            kiwi_machine = kiwi_machine_path_from_cmdline;
          RunExecutable(kiwi_machine,
                        {"--package-dir=" + g_last_pack_dir.AsUTF8Unsafe(),
                         "--enable_debug"});
#endif
        }
      }

      ImGui::NewLine();
      ImGui::TextUnformatted(U8("颜色说明："));
      ImGui::PushStyleColor(ImGuiCol_Text, ImColor(255, 0, 0).Value);
      ImGui::TextUnformatted(U8("红色表示文件在目标路径中不存在"));
      ImGui::PopStyleColor();

      ImGui::PushStyleColor(ImGuiCol_Text, ImColor(0, 255, 0).Value);
      ImGui::TextUnformatted(U8("绿色表示文件在目标路径中已存在"));
      ImGui::PopStyleColor();

      ImGui::PushStyleColor(ImGuiCol_Text, ImColor(127, 127, 127).Value);
      ImGui::TextUnformatted(U8("灰色表示文件不支持被打开"));
      ImGui::PopStyleColor();
    }
    ImGui::End();
  }
}

void Render() {
  SDL_RenderClear(g_renderer);
  SDL_SetRenderDrawColor(g_renderer, 0x00, 0x00, 0x00, 0x00);
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu(U8("资源包"))) {
      if (ImGui::MenuItem(U8("新建压缩包"))) {
        CreateROMWindow(ROMS(), kiwi::base::FilePath(), true, false);
      }
      if (ImGui::MenuItem(U8("目录浏览器"))) {
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

  bool running = true;
  while (running) {
    if (!HandleEvents())
      running = false;
    Render();
  }

  Cleanup();
  return 0;
}
