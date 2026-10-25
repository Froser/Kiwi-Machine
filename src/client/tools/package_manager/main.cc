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

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "package_manager_ui.h"
#include "texture_packager.h"
#include "util.h"

DEFINE_string(km_path, "", "Kiwi-Machine executable/bundle directory.");
DEFINE_string(zipped_path, "", "Input path for zipped files.");
DEFINE_string(output_path, "", "Output path for packaged files.");
DEFINE_string(mesen_hd_texture_path,
              "",
              "Input directory containing Mesen HD Texture Pack folders.");
DEFINE_string(rom_system, "nes", "ROM system for --zipped_path: nes or gba.");

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace {

SDL_Renderer* g_renderer = nullptr;

SDL_Window* CreateMainWindow() {
  constexpr int kInitialWidth = 1440;
  constexpr int kInitialHeight = 900;
  return SDL_CreateWindow(
      U8("Kiwi Package Studio"), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      kInitialWidth, kInitialHeight,
      SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
}

void InitializeStyle() {
  ImGui::StyleColorsLight();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowPadding = ImVec2(0, 0);
  style.FramePadding = ImVec2(10, 7);
  style.CellPadding = ImVec2(10, 7);
  style.ItemSpacing = ImVec2(8, 7);
  style.ItemInnerSpacing = ImVec2(6, 5);
  style.ScrollbarSize = 12;
  style.WindowRounding = 0;
  style.ChildRounding = 0;
  style.FrameRounding = 5;
  style.PopupRounding = 6;
  style.GrabRounding = 4;
  style.TabRounding = 4;
  style.WindowBorderSize = 0;
  style.ChildBorderSize = 1;
  style.FrameBorderSize = 1;

  ImVec4* colors = style.Colors;
  colors[ImGuiCol_Text] = ImVec4(0.09f, 0.13f, 0.12f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.50f, 0.48f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.96f, 0.97f, 0.97f, 1.00f);
  colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
  colors[ImGuiCol_Border] = ImVec4(0.82f, 0.86f, 0.84f, 1.00f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
  colors[ImGuiCol_FrameBg] = ImVec4(0.98f, 0.99f, 0.98f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.92f, 0.96f, 0.94f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.86f, 0.93f, 0.89f, 1.00f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.94f, 0.96f, 0.95f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.94f, 0.96f, 0.95f, 1.00f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.95f, 0.97f, 0.96f, 1.00f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.95f, 0.97f, 0.96f, 1.00f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.70f, 0.76f, 0.73f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.56f, 0.65f, 0.61f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.55f, 0.48f, 1.00f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.18f, 0.49f, 0.35f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.18f, 0.49f, 0.35f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.13f, 0.38f, 0.27f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.86f, 0.94f, 0.90f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.74f, 0.87f, 0.80f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.88f, 0.95f, 0.91f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.82f, 0.92f, 0.87f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.74f, 0.87f, 0.80f, 1.00f);
  colors[ImGuiCol_Separator] = ImVec4(0.82f, 0.86f, 0.84f, 1.00f);
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.32f, 0.58f, 0.46f, 1.00f);
  colors[ImGuiCol_SeparatorActive] = ImVec4(0.18f, 0.49f, 0.35f, 1.00f);
  colors[ImGuiCol_ResizeGrip] = ImVec4(0.30f, 0.55f, 0.43f, 0.20f);
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.30f, 0.55f, 0.43f, 0.67f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(0.18f, 0.49f, 0.35f, 0.95f);
  colors[ImGuiCol_Tab] = ImVec4(0.94f, 0.96f, 0.95f, 1.00f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.84f, 0.93f, 0.88f, 1.00f);
  colors[ImGuiCol_TabSelected] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.18f, 0.49f, 0.35f, 1.00f);
  colors[ImGuiCol_TableHeaderBg] = ImVec4(0.93f, 0.95f, 0.94f, 1.00f);
  colors[ImGuiCol_TableBorderStrong] = ImVec4(0.77f, 0.82f, 0.80f, 1.00f);
  colors[ImGuiCol_TableBorderLight] = ImVec4(0.87f, 0.90f, 0.89f, 1.00f);
  colors[ImGuiCol_NavHighlight] = ImVec4(0.18f, 0.49f, 0.35f, 1.00f);
}

bool InitSDL() {
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    return false;

  SDL_Window* window = CreateMainWindow();
  if (!window)
    return false;

  SDL_Renderer* renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    SDL_DestroyWindow(window);
    return false;
  }

  g_renderer = renderer;
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#if BUILDFLAG(IS_APPLE)
  io.Fonts->AddFontFromFileTTF("/System/Library/Fonts/STHeiti Light.ttc", 16);
#endif
#if BUILDFLAG(IS_WIN)
  io.Fonts->AddFontFromFileTTF(GetFontsPath()
                                   .Append(FILE_PATH_LITERAL("msyh.ttc"))
                                   .AsUTF8Unsafe()
                                   .c_str(),
                               16);
#endif
#if BUILDFLAG(IS_LINUX)
  io.Fonts->AddFontFromFileTTF(
      "/usr/share/fonts/opentype/noto/NotoSansCJK-Black.ttc", 16);
#endif

  InitializeStyle();
  ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer2_Init(renderer);
  InitializePackageManagerUI(renderer);
  return true;
}

bool HandleEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL2_ProcessEvent(&event);
    switch (event.type) {
      case SDL_QUIT:
        return false;
      case SDL_DROPFILE:
        HandlePackageManagerDropFile(event.drop.file);
        SDL_free(event.drop.file);
        break;
      default:
        break;
    }
  }
  return true;
}

void Render() {
  SDL_SetRenderDrawColor(g_renderer, 0xe9, 0xee, 0xec, 0xff);
  SDL_RenderClear(g_renderer);
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  PaintPackageManagerUI();

  ImGui::Render();
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), g_renderer);
  SDL_RenderSetScale(g_renderer, ImGui::GetIO().DisplayFramebufferScale.x,
                     ImGui::GetIO().DisplayFramebufferScale.y);
  SDL_RenderPresent(g_renderer);
  PackageManagerUIPainted();
}

void Cleanup() {
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_Quit();
}

std::optional<ROMSystem> GetROMSystemFromFlag() {
  if (kiwi::base::CompareCaseInsensitiveASCII(FLAGS_rom_system, "nes") == 0 ||
      kiwi::base::CompareCaseInsensitiveASCII(FLAGS_rom_system, "fc") == 0) {
    return ROMSystem::kNES;
  }
  if (kiwi::base::CompareCaseInsensitiveASCII(FLAGS_rom_system, "gba") == 0)
    return ROMSystem::kGBA;
  return std::nullopt;
}

int RunCommandLinePackaging() {
  if (!FLAGS_mesen_hd_texture_path.empty()) {
    if (FLAGS_output_path.empty()) {
      fprintf(stderr, "--mesen_hd_texture_path requires --output_path.\n");
      return 1;
    }
    std::vector<kiwi::base::FilePath> texture_packages = PackMesenHDTextures(
        kiwi::base::FilePath::FromUTF8Unsafe(FLAGS_mesen_hd_texture_path),
        kiwi::base::FilePath::FromUTF8Unsafe(FLAGS_output_path));
    if (texture_packages.empty())
      return 1;

    printf("Done. Texture packages:\n");
    for (const auto& path : texture_packages)
      printf("%s\n", path.AsUTF8Unsafe().c_str());
    return 0;
  }

  if (!FLAGS_zipped_path.empty() && !FLAGS_output_path.empty()) {
    std::optional<ROMSystem> system = GetROMSystemFromFlag();
    if (!system) {
      fprintf(stderr, "--rom_system must be nes, fc, or gba.\n");
      return 1;
    }
    auto result = PackEntireDirectory(
        kiwi::base::FilePath::FromUTF8Unsafe(FLAGS_zipped_path),
        kiwi::base::FilePath::FromUTF8Unsafe(FLAGS_output_path), *system);
    if (result.empty()) {
      printf("打包失败。\n");
      return 1;
    }

    printf("Done. Result:\n");
    for (const auto& path : result)
      printf("%s\n", path.AsUTF8Unsafe().c_str());
    return 0;
  }
  return -1;
}

}  // namespace

#if BUILDFLAG(IS_WIN)
char** g_argv = nullptr;

void CleanupArgs() {
  for (size_t i = 0; i < __argc; ++i)
    free(g_argv[i]);
  free(g_argv);
}

int WINAPI wWinMain(HINSTANCE hInstance,
                    HINSTANCE hPrevInstance,
                    PWSTR pCmdLine,
                    int nCmdShow) {
  wchar_t** wargv = __wargv;
  SDL_assert(wargv);
  g_argv = static_cast<char**>(calloc(__argc, sizeof(char*)));
  for (size_t i = 0; i < __argc; ++i) {
    g_argv[i] = static_cast<char*>(calloc(wcslen(wargv[i]) + 1, sizeof(char)));
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

  const int command_line_result = RunCommandLinePackaging();
  if (command_line_result >= 0)
    return command_line_result;

  if (!InitSDL())
    return 1;

  bool running = true;
  while (running) {
    if (!HandleEvents())
      running = false;
    Render();
  }

  Cleanup();
  return 0;
}
