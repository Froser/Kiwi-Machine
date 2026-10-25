// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "package_manager_ui.h"

#include <SDL.h>
#include <gflags/gflags.h>
#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "explorer_loader.h"
#include "package_manager_widgets.h"
#include "rom_window.h"
#include "util.h"
#include "workspace.h"

DECLARE_string(km_path);

namespace {

constexpr ImVec4 kGreen = ImVec4(0.18f, 0.49f, 0.35f, 1.00f);
constexpr ImVec4 kRed = ImVec4(0.74f, 0.30f, 0.27f, 1.00f);
constexpr ImVec4 kAmber = ImVec4(0.76f, 0.48f, 0.13f, 1.00f);
constexpr ImVec4 kMuted = ImVec4(0.45f, 0.50f, 0.48f, 1.00f);
constexpr ImVec4 kPanelBg = ImVec4(0.97f, 0.98f, 0.98f, 1.00f);

enum class FilterOptions {
  kAll,
  kIgnoreMarked,
  kUnmatchedOnly,
  kIssuesOnly,
  kUnsupportedOnly,
  kUninterestedOnly,
  kImperfectOnly,
  kDuplicatedOnly,
};

struct ExplorerState {
  Explorer explorer;
  ROMSystem system = ROMSystem::kNES;
  bool first_open = true;
  FilterOptions filter = FilterOptions::kAll;
  bool exclude_unl = true;
  Explorer::File* selected_item = nullptr;
  char search[256] = {};
};

struct ExplorerStats {
  int total = 0;
  int matched = 0;
  int pending = 0;
  int issues = 0;
};

SDL_Renderer* g_renderer = nullptr;
std::vector<ROMWindow> g_rom_windows;
int g_active_rom_window = -1;
bool g_select_active_tab = false;
ExplorerState g_explorer;
kiwi::base::FilePath g_dropped_jpg;
kiwi::base::FilePath g_dropped_rom;
std::string g_last_pack_result_str;
kiwi::base::FilePath g_last_pack_dir;
bool g_open_workspace_settings = false;
bool g_open_package_result = false;
bool g_open_rewrite_confirmation = false;
ExplorerLoader g_explorer_loader;
ROMSystem g_loading_system = ROMSystem::kNES;

void RefreshExplorer();
void CreateROMFromFile(const kiwi::base::FilePath& rom_path);

using package_manager::EllipsizedPathText;
using package_manager::FilterButton;
using package_manager::LoadingSpinner;
using package_manager::PrimaryButton;

bool IsMarked(const Explorer::File& item) {
  return item.mark != Explorer::Mark::kNoMark;
}

bool IsIssue(const Explorer::File& item) {
  return !item.supported || item.mark == Explorer::Mark::kImprefect;
}

bool IsPending(const Explorer::File& item) {
  return item.supported && !item.matched &&
         item.mark == Explorer::Mark::kNoMark;
}

const char* GetMarkDescription(Explorer::Mark mark) {
  switch (mark) {
    case Explorer::Mark::kNoMark:
      return U8("无标记");
    case Explorer::Mark::kUninterested:
      return U8("不感兴趣");
    case Explorer::Mark::kImprefect:
      return U8("不完美");
    case Explorer::Mark::kDuplicated:
      return U8("重复");
  }
  return U8("无标记");
}

const char* GetFilterDescription(FilterOptions filter) {
  switch (filter) {
    case FilterOptions::kAll:
      return U8("显示全部");
    case FilterOptions::kIgnoreMarked:
      return U8("忽略已标记");
    case FilterOptions::kUnmatchedOnly:
      return U8("只看未打包");
    case FilterOptions::kIssuesOnly:
      return U8("只看异常");
    case FilterOptions::kUnsupportedOnly:
      return U8("只看不支持");
    case FilterOptions::kUninterestedOnly:
      return U8("只看不感兴趣");
    case FilterOptions::kImperfectOnly:
      return U8("只看不完美");
    case FilterOptions::kDuplicatedOnly:
      return U8("只看重复");
  }
  return U8("显示全部");
}

bool MatchesExplorerFilter(const Explorer::File& item) {
  if (g_explorer.exclude_unl && item.title.find("(Unl)") != std::string::npos) {
    return false;
  }

  switch (g_explorer.filter) {
    case FilterOptions::kIgnoreMarked:
      if (IsMarked(item))
        return false;
      break;
    case FilterOptions::kUnmatchedOnly:
      if (!IsPending(item))
        return false;
      break;
    case FilterOptions::kIssuesOnly:
      if (!IsIssue(item))
        return false;
      break;
    case FilterOptions::kUnsupportedOnly:
      if (item.supported || item.mark == Explorer::Mark::kUninterested)
        return false;
      break;
    case FilterOptions::kUninterestedOnly:
      if (item.mark != Explorer::Mark::kUninterested)
        return false;
      break;
    case FilterOptions::kImperfectOnly:
      if (item.mark != Explorer::Mark::kImprefect)
        return false;
      break;
    case FilterOptions::kDuplicatedOnly:
      if (item.mark != Explorer::Mark::kDuplicated)
        return false;
      break;
    case FilterOptions::kAll:
      break;
  }

  if (g_explorer.search[0]) {
    const std::string query =
        kiwi::base::ToLowerASCII(std::string(g_explorer.search));
    const std::string title = kiwi::base::ToLowerASCII(item.title);
    const std::string mapper = kiwi::base::ToLowerASCII(item.mapper);
    if (title.find(query) == std::string::npos &&
        mapper.find(query) == std::string::npos) {
      return false;
    }
  }
  return true;
}

ExplorerStats GetExplorerStats() {
  ExplorerStats stats;
  for (const auto& item : g_explorer.explorer.explorer_files) {
    if (g_explorer.exclude_unl &&
        item.title.find("(Unl)") != std::string::npos) {
      continue;
    }
    ++stats.total;
    if (item.matched)
      ++stats.matched;
    if (IsPending(item))
      ++stats.pending;
    if (IsIssue(item))
      ++stats.issues;
  }
  return stats;
}

ROMWindow* GetActiveEditor() {
  if (g_active_rom_window < 0 ||
      g_active_rom_window >= static_cast<int>(g_rom_windows.size())) {
    return nullptr;
  }
  ROMWindow& editor = g_rom_windows[g_active_rom_window];
  return editor.closed() ? nullptr : &editor;
}

void CreateROMWindow(ROMSystem system,
                     ROMS roms,
                     const kiwi::base::FilePath& file,
                     bool new_rom,
                     bool fetch_image) {
  ROMWindow window(g_renderer, system, std::move(roms), file);
  if (new_rom)
    window.NewRom();
  if (fetch_image && window.first_rom()) {
    window.TryFetchCoverByName(*window.first_rom(),
                               kiwi::base::FilePath::FromUTF8Unsafe(
                                   window.first_rom()->rom_file_name));
  }
  g_rom_windows.push_back(std::move(window));
  g_active_rom_window = static_cast<int>(g_rom_windows.size()) - 1;
  g_select_active_tab = true;
}

void CreateROMFromFile(const kiwi::base::FilePath& rom_path) {
  std::optional<ROMSystem> system =
      GetROMSystemForExtension(rom_path.AsUTF8Unsafe());
  if (!system)
    return;

  auto rom_data = kiwi::base::ReadFileToBytes(rom_path);
  if (!rom_data)
    return;

  ROM rom;
  rom.system = *system;
  kiwi::base::strlcpy(rom.rom_file_name,
                      rom_path.BaseName().AsUTF8Unsafe().c_str(),
                      sizeof(rom.rom_file_name));
  rom.key = "default";
  rom.rom_data = std::move(*rom_data);
  FillRomDetailsAutomatically(rom, rom_path.BaseName());
  CreateROMWindow(*system, std::vector<ROM>{std::move(rom)},
                  kiwi::base::FilePath(), false, true);
}

void OpenExplorerItem(const Explorer::File& item, bool force_new) {
  const kiwi::base::FilePath rom_file =
      item.dir.Append(kiwi::base::FilePath::FromUTF8Unsafe(item.title));
  if (!force_new && item.matched &&
      IsZipExtension(item.compared_zip_path.AsUTF8Unsafe())) {
    CreateROMWindow(g_explorer.system,
                    ReadZipFromFile(item.compared_zip_path, g_explorer.system),
                    item.compared_zip_path, false, false);
    return;
  }
  CreateROMFromFile(rom_file);
}

void SelectSystem(ROMSystem system) {
  if (g_explorer_loader.loading() ||
      (g_explorer.system == system && !g_explorer.first_open)) {
    return;
  }
  if (g_explorer_loader.Start(GetWorkspace().GetRomsPath(system),
                              GetWorkspace().GetZippedPath(system), system)) {
    g_loading_system = system;
  }
}

void RefreshExplorer() {
  if (g_explorer_loader.loading())
    return;
  if (g_explorer_loader.Start(GetWorkspace().GetRomsPath(g_explorer.system),
                              GetWorkspace().GetZippedPath(g_explorer.system),
                              g_explorer.system)) {
    g_loading_system = g_explorer.system;
  }
}

void PollExplorerLoader() {
  const bool was_loading = g_explorer_loader.loading();
  std::optional<ExplorerLoadResult> result = g_explorer_loader.Poll();
  if (!result) {
    if (was_loading && !g_explorer_loader.loading()) {
      g_explorer.system = g_loading_system;
      g_explorer.first_open = false;
      g_explorer.selected_item = nullptr;
      g_explorer.explorer.explorer_files.clear();
    }
    return;
  }

  g_explorer.system = result->system;
  g_explorer.first_open = false;
  g_explorer.selected_item = nullptr;
  g_explorer.explorer.explorer_files = std::move(result->files);
}

void RemoveClosedEditors() {
  int active_window_id = -1;
  if (ROMWindow* active = GetActiveEditor())
    active_window_id = active->window_id();

  for (auto it = g_rom_windows.begin(); it != g_rom_windows.end();) {
    if (it->closed())
      it = g_rom_windows.erase(it);
    else
      ++it;
  }

  g_active_rom_window = -1;
  for (size_t i = 0; i < g_rom_windows.size(); ++i) {
    if (g_rom_windows[i].window_id() == active_window_id) {
      g_active_rom_window = static_cast<int>(i);
      break;
    }
  }
  if (g_active_rom_window < 0 && !g_rom_windows.empty())
    g_active_rom_window = static_cast<int>(g_rom_windows.size()) - 1;
}

void PackCurrentSystem() {
  auto result = PackEntireDirectory(
      GetWorkspace().GetZippedPath(g_explorer.system),
      GetWorkspace().GetPackageOutputPath(), g_explorer.system);
  if (result.empty()) {
    g_last_pack_result_str = U8("打包失败。请检查终端日志。");
    g_last_pack_dir.clear();
  } else {
    g_last_pack_result_str = U8("打包完成：\n");
    g_last_pack_dir = GetWorkspace().GetPackageOutputPath();
    for (const auto& path : result)
      g_last_pack_result_str += path.AsUTF8Unsafe() + "\n";
  }
  g_open_package_result = true;
}

void RunLastPackage() {
  if (g_last_pack_dir.empty())
    return;

  kiwi::base::FilePath executable_path;
  if (!FLAGS_km_path.empty())
    executable_path = kiwi::base::FilePath::FromUTF8Unsafe(FLAGS_km_path);
#if BUILDFLAG(IS_MAC)
  if (executable_path.empty())
    executable_path =
        kiwi::base::FilePath(FILE_PATH_LITERAL("Kiwi Machine.app"));
  RunExecutable(
      executable_path,
      {"--package-dir=" + g_last_pack_dir.AsUTF8Unsafe(), "--enable_debug"});
#elif BUILDFLAG(IS_WIN)
  if (executable_path.empty())
    executable_path =
        kiwi::base::FilePath(FILE_PATH_LITERAL("kiwi_machine.exe"));
  RunExecutable(executable_path,
                {L"--package-dir=\"" + g_last_pack_dir.value() + L"\"",
                 L"--enable_debug"});
#else
  if (executable_path.empty())
    executable_path = kiwi::base::FilePath(FILE_PATH_LITERAL("kiwi_machine"));
  RunExecutable(
      executable_path,
      {"--package-dir=" + g_last_pack_dir.AsUTF8Unsafe(), "--enable_debug"});
#endif
}

void RewriteAllZips() {
  const kiwi::base::FilePath zipped_path =
      GetWorkspace().GetZippedPath(g_explorer.system);
  kiwi::base::FileEnumerator files(zipped_path, true,
                                   kiwi::base::FileEnumerator::FILES,
                                   FILE_PATH_LITERAL("*.zip"));
  for (kiwi::base::FilePath current = files.Next(); !current.empty();
       current = files.Next()) {
    ROMS roms = ReadZipFromFile(current, g_explorer.system);
    bool missing_boxarts_manifest = false;
    for (const ROM& rom : roms) {
      if (!rom.has_boxarts_size_hint) {
        missing_boxarts_manifest = true;
        break;
      }
    }

    const kiwi::base::FilePath output_file = WriteZip(zipped_path, roms);
    if (missing_boxarts_manifest &&
        current.BaseName() != output_file.BaseName()) {
      kiwi::base::DeletePathRecursively(current);
    }
  }
  RefreshExplorer();
}

void PaintTopToolbar() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 10));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1, 1, 1, 1));
  ImGui::BeginChild("##TopToolbar", ImVec2(0, 68), ImGuiChildFlags_Borders);

  ImGui::BeginGroup();
  ImGui::TextUnformatted("KIWI PACKAGE STUDIO");
  ImGui::TextColored(kMuted, "ROM COLLECTION WORKBENCH");
  ImGui::EndGroup();

  ImGui::SameLine(320);
  ImGui::BeginGroup();
  ImGui::TextColored(kGreen, U8("● 工作空间已连接"));
  const float workspace_width =
      std::min(460.0f, std::max(80.0f, ImGui::GetWindowWidth() - 772.0f));
  EllipsizedPathText(GetWorkspace().workspace_dir, workspace_width, kMuted);
  ImGui::EndGroup();

  constexpr float kActionsWidth = 420;
  const float target_x = std::max(ImGui::GetCursorPosX() + 24,
                                  ImGui::GetWindowWidth() - kActionsWidth);
  ImGui::SameLine(target_x);
  if (g_explorer_loader.loading())
    ImGui::BeginDisabled();
  if (ImGui::Button(U8("新建 ZIP"))) {
    CreateROMWindow(g_explorer.system, ROMS(), kiwi::base::FilePath(), true,
                    false);
  }
  ImGui::SameLine();
  if (ImGui::Button(U8("刷新")))
    RefreshExplorer();
  ImGui::SameLine();
  if (ImGui::Button(U8("工作空间")))
    g_open_workspace_settings = true;
  ImGui::SameLine();
  if (PrimaryButton(U8("打包资源"), ImVec2(100, 0)))
    PackCurrentSystem();
  if (g_explorer_loader.loading())
    ImGui::EndDisabled();

  ImGui::EndChild();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
}

void PaintSummary(const ExplorerStats& stats, bool loading) {
  if (!ImGui::BeginTable(
          "##ExplorerSummary", 3,
          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame)) {
    return;
  }
  const int values[] = {stats.total, stats.matched, stats.pending};
  const char* labels[] = {U8("源 ROM"), U8("ZIP 包"), U8("待处理")};
  for (int i = 0; i < 3; ++i) {
    ImGui::TableNextColumn();
    if (loading)
      ImGui::TextUnformatted("...");
    else
      ImGui::Text("%d", values[i]);
    ImGui::TextColored(kMuted, "%s", labels[i]);
  }
  ImGui::EndTable();
}

void PaintExplorerRow(Explorer::File& item) {
  ImGui::PushID(&item);
  const bool selected = g_explorer.selected_item == &item;
  const ImVec2 row_size(0, 54);
  if (ImGui::Selectable("##ExplorerRow", selected,
                        ImGuiSelectableFlags_AllowDoubleClick, row_size)) {
    for (auto& other : g_explorer.explorer.explorer_files)
      other.selected = false;
    item.selected = true;
    g_explorer.selected_item = &item;
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
      OpenExplorerItem(item, false);
  }

  const ImVec2 row_min = ImGui::GetItemRectMin();
  const ImVec2 row_max = ImGui::GetItemRectMax();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImVec4 state_color = !item.supported
                                 ? kMuted
                                 : (item.mark == Explorer::Mark::kImprefect
                                        ? kAmber
                                        : (item.matched ? kGreen : kRed));
  draw_list->AddRectFilled(row_min, ImVec2(row_min.x + 4, row_max.y),
                           ImGui::GetColorU32(state_color));

  const char* state_text =
      !item.supported ? U8("不支持")
                      : (item.mark == Explorer::Mark::kImprefect
                             ? U8("不完美")
                             : (item.matched ? U8("已打包") : U8("未打包")));
  const ImVec2 state_size = ImGui::CalcTextSize(state_text);
  const float state_x = row_max.x - state_size.x - 10;
  draw_list->PushClipRect(ImVec2(row_min.x + 12, row_min.y),
                          ImVec2(state_x - 8, row_max.y), true);
  draw_list->AddText(ImVec2(row_min.x + 12, row_min.y + 7),
                     ImGui::GetColorU32(ImGuiCol_Text), item.title.c_str());
  draw_list->PopClipRect();

  std::string metadata;
  if (g_explorer.system == ROMSystem::kNES && !item.mapper.empty())
    metadata = "Mapper " + item.mapper + "  |  ";
  metadata += GetMarkDescription(item.mark);
  draw_list->AddText(ImVec2(row_min.x + 12, row_min.y + 30),
                     ImGui::GetColorU32(kMuted), metadata.c_str());
  draw_list->AddText(ImVec2(state_x, row_min.y + 18),
                     ImGui::GetColorU32(state_color), state_text);
  ImGui::PopID();
}

void PaintFilterCombo() {
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (!ImGui::BeginCombo("##ExplorerFilter",
                         GetFilterDescription(g_explorer.filter))) {
    return;
  }
  const FilterOptions filters[] = {
      FilterOptions::kAll,
      FilterOptions::kIgnoreMarked,
      FilterOptions::kUnmatchedOnly,
      FilterOptions::kIssuesOnly,
      FilterOptions::kUnsupportedOnly,
      FilterOptions::kUninterestedOnly,
      FilterOptions::kImperfectOnly,
      FilterOptions::kDuplicatedOnly,
  };
  for (FilterOptions filter : filters) {
    if (ImGui::Selectable(GetFilterDescription(filter),
                          g_explorer.filter == filter)) {
      g_explorer.filter = filter;
    }
  }
  ImGui::EndCombo();
}

void PaintLibrary() {
  if (g_explorer.first_open)
    RefreshExplorer();

  bool loading = g_explorer_loader.loading();
  const ROMSystem displayed_system =
      loading ? g_loading_system : g_explorer.system;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanelBg);
  ImGui::BeginChild("##LibraryPane", ImVec2(0, 0),
                    ImGuiChildFlags_AlwaysUseWindowPadding);

  const float switch_width = (ImGui::GetContentRegionAvail().x - 8) * 0.5f;
  if (loading)
    ImGui::BeginDisabled();
  if (FilterButton("NES / FC", displayed_system == ROMSystem::kNES,
                   ImVec2(switch_width, 0))) {
    SelectSystem(ROMSystem::kNES);
  }
  ImGui::SameLine();
  if (FilterButton("GBA", displayed_system == ROMSystem::kGBA,
                   ImVec2(switch_width, 0))) {
    SelectSystem(ROMSystem::kGBA);
  }
  if (loading)
    ImGui::EndDisabled();

  loading = g_explorer_loader.loading();
  const ExplorerStats stats = GetExplorerStats();
  PaintSummary(stats, loading);
  ImGui::Spacing();
  if (loading)
    ImGui::BeginDisabled();
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::InputTextWithHint("##ExplorerSearch", U8("搜索文件名或 Mapper"),
                           g_explorer.search, sizeof(g_explorer.search));

  const float filter_width = (ImGui::GetContentRegionAvail().x - 16) / 3;
  const std::string all_label =
      loading ? U8("全部 ...") : U8("全部 ") + std::to_string(stats.total);
  const std::string pending_label =
      loading ? U8("未打包 ...")
              : U8("未打包 ") + std::to_string(stats.pending);
  const std::string issues_label =
      loading ? U8("异常 ...") : U8("异常 ") + std::to_string(stats.issues);
  if (FilterButton(all_label, g_explorer.filter == FilterOptions::kAll,
                   ImVec2(filter_width, 0))) {
    g_explorer.filter = FilterOptions::kAll;
  }
  ImGui::SameLine();
  if (FilterButton(pending_label,
                   g_explorer.filter == FilterOptions::kUnmatchedOnly,
                   ImVec2(filter_width, 0))) {
    g_explorer.filter = FilterOptions::kUnmatchedOnly;
  }
  ImGui::SameLine();
  if (FilterButton(issues_label,
                   g_explorer.filter == FilterOptions::kIssuesOnly,
                   ImVec2(filter_width, 0))) {
    g_explorer.filter = FilterOptions::kIssuesOnly;
  }
  PaintFilterCombo();
  if (loading)
    ImGui::EndDisabled();

  std::vector<Explorer::File*> visible_items;
  if (!loading) {
    visible_items.reserve(g_explorer.explorer.explorer_files.size());
    for (auto& item : g_explorer.explorer.explorer_files) {
      if (MatchesExplorerFilter(item))
        visible_items.push_back(&item);
    }
  }

  if (loading) {
    ImGui::TextColored(kMuted, U8("正在读取 %s 资源"),
                       GetROMSystemName(g_loading_system));
  } else {
    ImGui::TextColored(kMuted, U8("资源清单  %zu 项"), visible_items.size());
  }
  ImGui::BeginChild("##ExplorerList", ImVec2(0, -104), ImGuiChildFlags_Borders);
  if (loading) {
    const char* loading_text = U8("正在扫描 ROM、Mapper 和 ZIP…");
    const float content_width = ImGui::GetContentRegionAvail().x;
    const float group_width = 24.0f + ImGui::GetStyle().ItemSpacing.x +
                              ImGui::CalcTextSize(loading_text).x;
    ImGui::SetCursorPosY(
        std::max(20.0f, ImGui::GetContentRegionAvail().y * 0.36f));
    ImGui::SetCursorPosX(std::max(ImGui::GetStyle().WindowPadding.x,
                                  (content_width - group_width) * 0.5f));
    LoadingSpinner("##ExplorerLoadingSpinner", 12.0f, 3.0f, kGreen);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(kMuted, "%s", loading_text);
  } else if (!g_explorer_loader.error().empty()) {
    ImGui::TextColored(kRed, U8("资源读取失败"));
    ImGui::TextWrapped("%s", g_explorer_loader.error().c_str());
    if (ImGui::Button(U8("重试")))
      RefreshExplorer();
  } else {
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(visible_items.size()), 54);
    while (clipper.Step()) {
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        PaintExplorerRow(*visible_items[i]);
    }
  }
  ImGui::EndChild();

  if (loading)
    ImGui::BeginDisabled();
  ImGui::Checkbox(U8("排除未授权 ROM"), &g_explorer.exclude_unl);
  const bool has_selection = !loading && g_explorer.selected_item != nullptr;
  if (!has_selection)
    ImGui::BeginDisabled();
  if (ImGui::Button(U8("制作 ZIP"), ImVec2(filter_width, 0)))
    OpenExplorerItem(*g_explorer.selected_item, true);
  ImGui::SameLine();
  if (ImGui::Button(U8("打开 ZIP"), ImVec2(filter_width, 0)))
    OpenExplorerItem(*g_explorer.selected_item, false);
  if (!has_selection)
    ImGui::EndDisabled();
  if (loading)
    ImGui::EndDisabled();

  ImGui::EndChild();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
}

void PaintEditorTabs() {
  if (g_rom_windows.empty())
    return;
  if (!ImGui::BeginTabBar(
          "##OpenEditors",
          ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
    return;
  }

  for (size_t i = 0; i < g_rom_windows.size(); ++i) {
    ROMWindow& editor = g_rom_windows[i];
    bool open = !editor.closed();
    ImGuiTabItemFlags flags =
        editor.dirty() ? ImGuiTabItemFlags_UnsavedDocument : 0;
    if (g_select_active_tab && static_cast<int>(i) == g_active_rom_window) {
      flags |= ImGuiTabItemFlags_SetSelected;
    }
    const std::string label =
        editor.GetTabTitle() + "###Editor" + std::to_string(editor.window_id());
    if (ImGui::BeginTabItem(label.c_str(), &open, flags)) {
      g_active_rom_window = static_cast<int>(i);
      ImGui::EndTabItem();
    }
    if (!open)
      editor.Close();
  }
  ImGui::EndTabBar();
  g_select_active_tab = false;
}

void PaintSelectedOverview() {
  ImGui::Spacing();
  if (!g_explorer.selected_item) {
    ImGui::TextColored(kMuted, U8("从左侧选择一个 ROM 开始整理资源。"));
    ImGui::Spacing();
    if (PrimaryButton(U8("新建空白 ZIP")))
      CreateROMWindow(g_explorer.system, ROMS(), kiwi::base::FilePath(), true,
                      false);
    return;
  }

  const Explorer::File& item = *g_explorer.selected_item;
  ImGui::TextUnformatted(item.title.c_str());
  ImGui::TextColored(item.matched ? kGreen : kRed,
                     item.matched ? U8("已匹配现有 ZIP") : U8("尚未打包"));
  ImGui::Separator();
  if (g_explorer.system == ROMSystem::kNES) {
    ImGui::Text(U8("Mapper: %s"), item.mapper.c_str());
    ImGui::Text(U8("KiwiMachine 支持: %s"),
                item.supported ? U8("是") : U8("否"));
  } else {
    ImGui::TextUnformatted(U8("系统: Game Boy Advance"));
    ImGui::Text(U8("KiwiMachine 支持: %s"),
                item.supported ? U8("是") : U8("否"));
  }
  ImGui::TextWrapped(
      U8("源文件: %s"),
      item.dir.Append(kiwi::base::FilePath::FromUTF8Unsafe(item.title))
          .AsUTF8Unsafe()
          .c_str());
  if (item.matched) {
    ImGui::TextWrapped(U8("对应 ZIP: %s"),
                       item.compared_zip_path.AsUTF8Unsafe().c_str());
  }
  ImGui::Spacing();
  if (PrimaryButton(U8("制作 ZIP"), ImVec2(120, 0)))
    OpenExplorerItem(item, true);
  ImGui::SameLine();
  if (!item.matched)
    ImGui::BeginDisabled();
  if (ImGui::Button(U8("打开对应 ZIP")))
    OpenExplorerItem(item, false);
  if (!item.matched)
    ImGui::EndDisabled();
}

void PaintExplorerInspector() {
  ImGui::TextUnformatted(U8("资源检查器"));
  ImGui::Separator();
  if (!g_explorer.selected_item) {
    ImGui::TextColored(kMuted, U8("选择资源后显示 Mapper、标记和路径。"));
    return;
  }

  Explorer::File& item = *g_explorer.selected_item;
  ImGui::TextWrapped("%s", item.title.c_str());
  ImGui::Spacing();
  ImGui::TextColored(kMuted, U8("状态"));
  ImGui::TextColored(item.matched ? kGreen : kRed,
                     item.matched ? U8("已打包") : U8("未打包"));
  ImGui::TextColored(kMuted, U8("模拟支持"));
  ImGui::Text("%s", item.supported ? U8("支持") : U8("不支持"));
  if (g_explorer.system == ROMSystem::kNES)
    ImGui::Text("Mapper %s", item.mapper.c_str());

  ImGui::Spacing();
  ImGui::TextColored(kMuted, U8("标记"));
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::BeginCombo("##ExplorerMark", GetMarkDescription(item.mark))) {
    const Explorer::Mark marks[] = {
        Explorer::Mark::kNoMark, Explorer::Mark::kUninterested,
        Explorer::Mark::kImprefect, Explorer::Mark::kDuplicated};
    for (Explorer::Mark mark : marks) {
      if (ImGui::Selectable(GetMarkDescription(mark), item.mark == mark)) {
        item.mark = mark;
        UpdateMarks(GetWorkspace().GetRomsPath(g_explorer.system),
                    g_explorer.explorer.explorer_files);
      }
    }
    ImGui::EndCombo();
  }

  ImGui::Separator();
  ImGui::TextColored(kMuted, U8("路径"));
  ImGui::TextWrapped(
      "%s", item.dir.Append(kiwi::base::FilePath::FromUTF8Unsafe(item.title))
                .AsUTF8Unsafe()
                .c_str());
  if (item.matched) {
    ImGui::Spacing();
    ImGui::TextColored(kMuted, U8("ZIP"));
    ImGui::TextWrapped("%s", item.compared_zip_path.AsUTF8Unsafe().c_str());
  }
}

void PaintWorkspace() {
  const ImGuiTableFlags flags = ImGuiTableFlags_Resizable |
                                ImGuiTableFlags_BordersInnerV |
                                ImGuiTableFlags_SizingStretchProp;
  if (!ImGui::BeginTable("##PackageWorkspace", 3, flags, ImVec2(0, 0)))
    return;

  ImGui::TableSetupColumn("Library", ImGuiTableColumnFlags_WidthFixed, 300);
  ImGui::TableSetupColumn("Editor", ImGuiTableColumnFlags_WidthStretch, 1);
  ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, 330);
  ImGui::TableNextRow();

  ImGui::TableNextColumn();
  PaintLibrary();

  ImGui::TableNextColumn();
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 12));
  ImGui::BeginChild("##EditorPane", ImVec2(0, 0),
                    ImGuiChildFlags_AlwaysUseWindowPadding);
  PaintEditorTabs();
  if (ROMWindow* editor = GetActiveEditor()) {
    ImGui::BeginChild("##EditorContent", ImVec2(0, -72));
    editor->PaintEditor();
    ImGui::EndChild();
    ImGui::Separator();
    editor->PaintFooter();
  } else {
    PaintSelectedOverview();
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();

  ImGui::TableNextColumn();
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanelBg);
  ImGui::BeginChild("##InspectorPane", ImVec2(0, 0),
                    ImGuiChildFlags_AlwaysUseWindowPadding);
  if (ROMWindow* editor = GetActiveEditor())
    editor->PaintInspector();
  else
    PaintExplorerInspector();
  ImGui::EndChild();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();

  ImGui::EndTable();
}

void PaintWorkspaceSettings() {
  if (g_open_workspace_settings) {
    ImGui::OpenPopup(U8("工作空间设置"));
    g_open_workspace_settings = false;
  }
  ImGui::SetNextWindowSize(ImVec2(720, 0), ImGuiCond_Appearing);
  if (!ImGui::BeginPopupModal(U8("工作空间设置"), nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  ImGui::TextUnformatted(U8("工作空间根目录"));
  if (g_explorer_loader.loading())
    ImGui::BeginDisabled();
  ImGui::SetNextItemWidth(560);
  ImGui::InputText("##WorkspacePath", GetWorkspace().workspace_dir,
                   sizeof(GetWorkspace().workspace_dir));
  ImGui::SameLine();
  if (PrimaryButton(U8("加载"))) {
    const kiwi::base::FilePath manifest_path =
        kiwi::base::FilePath::FromUTF8Unsafe(GetWorkspace().workspace_dir)
            .Append(FILE_PATH_LITERAL("manifest.json"));
    if (GetWorkspace().ReadFromManifest(manifest_path)) {
      g_explorer.first_open = true;
      RefreshExplorer();
    }
  }
  if (g_explorer_loader.loading())
    ImGui::EndDisabled();

  ImGui::Separator();
  for (ROMSystem system : {ROMSystem::kNES, ROMSystem::kGBA}) {
    ImGui::Text("%s ROM: %s", GetROMSystemName(system),
                GetWorkspace().GetRomsPath(system).AsUTF8Unsafe().c_str());
    ImGui::Text("%s ZIP: %s", GetROMSystemName(system),
                GetWorkspace().GetZippedPath(system).AsUTF8Unsafe().c_str());
    ImGui::Text("%s Box Art: %s", GetROMSystemName(system),
                GetWorkspace().GetBoxartsPath(system).AsUTF8Unsafe().c_str());
    ImGui::Spacing();
  }
  ImGui::Text(U8("PAK 输出: %s"),
              GetWorkspace().GetPackageOutputPath().AsUTF8Unsafe().c_str());

  if (ImGui::CollapsingHeader(U8("维护与依赖"))) {
    ImGui::TextWrapped(
        U8("自动补全读音需要 Python 3、pinyin 和 pykakasi。测试功能需要通过 "
           "--km-path 指向 Kiwi Machine 可执行程序或应用包。"));
    ImGui::TextUnformatted("pip3 install pinyin pykakasi");
    if (ImGui::Button(U8("检查并重写当前系统全部 ZIP")))
      g_open_rewrite_confirmation = true;
  }

  ImGui::Separator();
  if (ImGui::Button(U8("关闭"), ImVec2(100, 0)))
    ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

void PaintRewriteConfirmation() {
  if (g_open_rewrite_confirmation) {
    ImGui::OpenPopup(U8("确认重写 ZIP"));
    g_open_rewrite_confirmation = false;
  }
  if (!ImGui::BeginPopupModal(U8("确认重写 ZIP"), nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }
  ImGui::TextWrapped(
      U8("将读取并重新写入当前 %s 工作空间中的全部 ZIP。是否继续？"),
      GetROMSystemName(g_explorer.system));
  if (PrimaryButton(U8("继续重写"), ImVec2(120, 0))) {
    RewriteAllZips();
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button(U8("取消"), ImVec2(100, 0)))
    ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

void PaintPackageResult() {
  if (g_open_package_result) {
    ImGui::OpenPopup(U8("打包结果"));
    g_open_package_result = false;
  }
  if (!ImGui::BeginPopupModal(U8("打包结果"), nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }
  ImGui::TextWrapped("%s", g_last_pack_result_str.c_str());
  if (!g_last_pack_dir.empty()) {
    if (ImGui::Button(U8("打开输出目录")))
      ShellOpenDirectory(g_last_pack_dir);
    ImGui::SameLine();
    if (PrimaryButton(U8("在 Kiwi Machine 中测试")))
      RunLastPackage();
  }
  ImGui::SameLine();
  if (ImGui::Button(U8("关闭")))
    ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

}  // namespace

void NotifySaved(ROMSystem system,
                 const kiwi::base::FilePath& updated_zip_file) {
  if (system == g_explorer.system && !updated_zip_file.empty()) {
    UpdateExplorerFiles(updated_zip_file, system,
                        g_explorer.explorer.explorer_files);
  }
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

void InitializePackageManagerUI(SDL_Renderer* renderer) {
  g_renderer = renderer;
}

void HandlePackageManagerDropFile(const char* dropped_file) {
  if (!dropped_file)
    return;
  if (IsZipExtension(dropped_file)) {
    const kiwi::base::FilePath file =
        kiwi::base::FilePath::FromUTF8Unsafe(dropped_file);
    ROMS roms = ReadZipFromFile(file);
    if (!roms.empty()) {
      CreateROMWindow(roms.front().system, std::move(roms), file, false, false);
    }
  } else if (IsJPEGExtension(dropped_file)) {
    g_dropped_jpg = kiwi::base::FilePath::FromUTF8Unsafe(dropped_file);
  } else if (GetROMSystemForExtension(dropped_file)) {
    g_dropped_rom = kiwi::base::FilePath::FromUTF8Unsafe(dropped_file);
  }
}

void PaintPackageManagerUI() {
  PollExplorerLoader();

  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
  ImGui::Begin("##PackageManagerRoot", nullptr, flags);
  PaintTopToolbar();
  PaintWorkspace();
  ImGui::End();

  PaintWorkspaceSettings();
  PaintRewriteConfirmation();
  PaintPackageResult();
  for (auto& editor : g_rom_windows)
    editor.PaintDialogs();
}

void PackageManagerUIPainted() {
  for (auto& editor : g_rom_windows)
    editor.Painted();
  RemoveClosedEditors();
  ClearDroppedJPG();
  if (!g_dropped_rom.empty())
    CreateROMFromFile(g_dropped_rom);
  ClearDroppedROM();
}
