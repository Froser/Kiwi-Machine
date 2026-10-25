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
#include <gflags/gflags.h>
#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "workspace.h"

namespace {

constexpr ImVec4 kGreen = ImVec4(0.18f, 0.49f, 0.35f, 1.00f);
constexpr ImVec4 kGreenDark = ImVec4(0.13f, 0.38f, 0.27f, 1.00f);
constexpr ImVec4 kGreenSoft = ImVec4(0.86f, 0.94f, 0.90f, 1.00f);
constexpr ImVec4 kRed = ImVec4(0.74f, 0.30f, 0.27f, 1.00f);
constexpr ImVec4 kAmber = ImVec4(0.76f, 0.48f, 0.13f, 1.00f);
constexpr ImVec4 kMuted = ImVec4(0.45f, 0.50f, 0.48f, 1.00f);

int g_window_id = 0;

bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0, 0)) {
  ImGui::PushStyleColor(ImGuiCol_Button, kGreen);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.22f, 0.56f, 0.40f, 1.00f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, kGreenDark);
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
  const bool pressed = ImGui::Button(label, size);
  ImGui::PopStyleColor(4);
  return pressed;
}

void ValidationLine(bool valid, const char* label, const char* detail) {
  ImGui::TextColored(valid ? kGreen : kAmber, valid ? "[OK]" : "[!]");
  ImGui::SameLine();
  ImGui::TextUnformatted(label);
  if (detail && detail[0]) {
    const float detail_width = ImGui::CalcTextSize(detail).x;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 8,
                             ImGui::GetWindowWidth() - detail_width - 10));
    ImGui::TextColored(kMuted, "%s", detail);
  }
}

}  // namespace

DECLARE_string(km_path);

// package_manager_ui.cc
void NotifySaved(ROMSystem system,
                 const kiwi::base::FilePath& updated_zip_file);
kiwi::base::FilePath GetDroppedJPG();
void ClearDroppedJPG();
kiwi::base::FilePath GetDroppedROM();
void ClearDroppedROM();

ROMWindow::ROMWindow(SDL_Renderer* renderer,
                     ROMSystem system,
                     ROMS roms,
                     kiwi::base::FilePath file)
    : system_(system),
      file_(std::move(file)),
      roms_(std::move(roms)),
      dirty_(file_.empty()),
      renderer_(renderer) {
  cover_update_mutex_ = SDL_CreateMutex();
  window_id_ = g_window_id++;
  for (auto& rom : roms_) {
    if (!rom.boxart_data.empty())
      FillCoverData(rom, rom.boxart_data);
  }
}

ROMWindow::~ROMWindow() {
  for (auto& rom : roms_) {
    if (rom.boxart_texture_) {
      SDL_DestroyTexture(rom.boxart_texture_);
      rom.boxart_texture_ = nullptr;
    }
  }
}

ROM* ROMWindow::selected_rom() {
  if (roms_.empty())
    return nullptr;
  selected_rom_index_ =
      std::clamp(selected_rom_index_, 0, static_cast<int>(roms_.size()) - 1);
  return &roms_[selected_rom_index_];
}

const ROM* ROMWindow::selected_rom() const {
  if (roms_.empty())
    return nullptr;
  const int index =
      std::clamp(selected_rom_index_, 0, static_cast<int>(roms_.size()) - 1);
  return &roms_[index];
}

std::string ROMWindow::GetTabTitle() const {
  if (!file_.empty())
    return file_.BaseName().RemoveExtension().AsUTF8Unsafe();
  if (const ROM* rom = selected_rom()) {
    if (rom->zh[0])
      return rom->zh;
    if (rom->rom_file_name[0]) {
      return kiwi::base::FilePath::FromUTF8Unsafe(rom->rom_file_name)
          .BaseName()
          .RemoveExtension()
          .AsUTF8Unsafe();
    }
  }
  return system_ == ROMSystem::kNES ? U8("未命名 NES 包") : U8("未命名 GBA 包");
}

void ROMWindow::PaintROMSelector() {
  ImGui::TextColored(kMuted, U8("ROM 版本"));
  if (roms_.empty()) {
    ImGui::TextColored(kMuted, U8("当前 ZIP 尚未包含 ROM。"));
  } else {
    for (size_t i = 0; i < roms_.size(); ++i) {
      if (i > 0)
        ImGui::SameLine();
      std::string title =
          roms_[i].key.empty() ? std::to_string(i + 1) : roms_[i].key;
      title += "##ROMVersion" + std::to_string(i);
      const bool is_selected = selected_rom_index_ == static_cast<int>(i);
      if (is_selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, kGreenSoft);
        ImGui::PushStyleColor(ImGuiCol_Text, kGreenDark);
      }
      if (ImGui::Button(title.c_str()))
        selected_rom_index_ = static_cast<int>(i);
      if (is_selected)
        ImGui::PopStyleColor(2);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(U8("+ 增加版本")))
    NewRom();
}

void ROMWindow::PaintLocalizedFields(ROM& rom, int id) {
  const kiwi::base::FilePath rom_base_name =
      kiwi::base::FilePath::FromUTF8Unsafe(rom.rom_file_name);
  ImGui::TextUnformatted(U8("本地化标题"));
  ImGui::SameLine();
  const float action_width = 420;
  ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 20,
                           ImGui::GetWindowWidth() - action_width));
  if (ImGui::Button(GetUniqueName(U8("自动填充"), id).c_str())) {
    dirty_ = true;
    if (!FillRomDetailsAutomatically(rom, rom_base_name)) {
      ShellOpen(kiwi::base::FilePath::FromUTF8Unsafe(
          "https://google.com/search?q=" +
          rom_base_name.RemoveExtension().AsUTF8Unsafe() + " とは"));
      ShellOpen(kiwi::base::FilePath::FromUTF8Unsafe(
          "https://google.com/search?q=" +
          rom_base_name.RemoveExtension().AsUTF8Unsafe() + " 中文名"));
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(GetUniqueName(U8("补全中文读音"), id).c_str())) {
    const std::string result = TryGetPinyin(rom.zh);
    if (!result.empty()) {
      kiwi::base::strlcpy(rom.zh_hint, result.c_str(), sizeof(rom.zh_hint));
      dirty_ = true;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(GetUniqueName(U8("补全日文读音"), id).c_str())) {
    const std::string result = TryGetKana(rom.ja);
    if (!result.empty()) {
      kiwi::base::strlcpy(rom.ja_hint, result.c_str(), sizeof(rom.ja_hint));
      dirty_ = true;
    }
  }

  if (ImGui::BeginTable(GetUniqueName("##LocalizedFields", id).c_str(), 2,
                        ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 110);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    struct Field {
      const char* label;
      const char* id;
      char* value;
    };
    const Field fields[] = {
        {U8("中文标题"), "##ChineseTitle", rom.zh},
        {U8("中文读音"), "##ChineseHint", rom.zh_hint},
        {U8("日文标题"), "##JapaneseTitle", rom.ja},
        {U8("日文读音"), "##JapaneseHint", rom.ja_hint},
    };
    for (const Field& field : fields) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(field.label);
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::InputText(GetUniqueName(field.id, id).c_str(), field.value,
                           ROM::MAX)) {
        dirty_ = true;
      }
    }
    ImGui::EndTable();
  }

  static const std::vector<const char*> kVersions = {
      U8("（日）"), U8(" (ri)"),  U8("（日）"), U8("（にち）"),
      U8("（美）"), U8(" (mei)"), U8("（米）"), U8("（べい）")};
  ImGui::TextColored(kMuted, U8("地区后缀"));
  ImGui::SameLine();
  if (ImGui::Button(GetUniqueName(U8("日版"), id).c_str())) {
    ReplaceAndAppendUnsafe(rom.zh, kVersions, kVersions[0]);
    ReplaceAndAppendUnsafe(rom.zh_hint, kVersions, kVersions[1]);
    ReplaceAndAppendUnsafe(rom.ja, kVersions, kVersions[2]);
    ReplaceAndAppendUnsafe(rom.ja_hint, kVersions, kVersions[3]);
    dirty_ = true;
  }
  ImGui::SameLine();
  if (ImGui::Button(GetUniqueName(U8("美版"), id).c_str())) {
    ReplaceAndAppendUnsafe(rom.zh, kVersions, kVersions[4]);
    ReplaceAndAppendUnsafe(rom.zh_hint, kVersions, kVersions[5]);
    ReplaceAndAppendUnsafe(rom.ja, kVersions, kVersions[6]);
    ReplaceAndAppendUnsafe(rom.ja_hint, kVersions, kVersions[7]);
    dirty_ = true;
  }
}

void ROMWindow::PaintROMFile(ROM& rom, int id) {
  ImGui::TextUnformatted(U8("ROM 文件"));
  ImGui::TextColored(kMuted, U8("拖入 %s 文件可替换当前版本"),
                     GetROMExtension(system_));
  ImGui::SetNextItemWidth(-150);
  if (ImGui::InputText(GetUniqueName(U8("##ROMFileName"), id).c_str(),
                       rom.rom_file_name, ROM::MAX)) {
    dirty_ = true;
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone)) {
    const kiwi::base::FilePath path = GetDroppedROM();
    if (!path.empty() && IsROMExtension(path.AsUTF8Unsafe(), system_)) {
      std::optional<std::vector<uint8_t>> contents =
          kiwi::base::ReadFileToBytes(path);
      if (contents) {
        kiwi::base::strlcpy(rom.rom_file_name,
                            path.BaseName().AsUTF8Unsafe().c_str(),
                            sizeof(rom.rom_file_name));
        rom.rom_data = std::move(*contents);
        if (kiwi::base::CompareCaseInsensitiveASCII(rom.key, "default") != 0)
          rom.key = path.BaseName().RemoveExtension().AsUTF8Unsafe();
        dirty_ = true;
        tested_ = false;
      }
      ClearDroppedROM();
    }
  }

  ImGui::SameLine();
  if (ImGui::Button(GetUniqueName(U8("测试 ROM"), id).c_str()))
    TestROM(rom);
  ImGui::TextColored(kMuted, U8("Manifest key: %s"), rom.key.c_str());
  ImGui::TextColored(kMuted, U8("ROM 大小: %zu KB"),
                     rom.rom_data.size() / 1024);
  if (rom.rom_data.empty())
    ImGui::TextColored(kRed, U8("尚未设置 ROM 数据"));

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, kRed);
  if (ImGui::Button(GetUniqueName(U8("删除此版本"), id).c_str()))
    to_be_deleted_ = id;
  ImGui::PopStyleColor();
}

void ROMWindow::PaintEditor() {
  ImGui::TextColored(kGreen, "%s PACKAGE / %zu ROM", GetROMSystemName(system_),
                     roms_.size());
  ImGui::TextUnformatted(GetTabTitle().c_str());
  if (!file_.empty())
    ImGui::TextColored(kMuted, "%s", file_.AsUTF8Unsafe().c_str());
  else
    ImGui::TextColored(kMuted, U8("尚未保存到工作空间"));
  ImGui::Separator();

  PaintROMSelector();
  ImGui::Separator();
  ROM* rom = selected_rom();
  if (!rom) {
    ImGui::TextColored(kMuted, U8("增加一个 ROM 版本后即可编辑元数据。"));
    return;
  }

  const int id = selected_rom_index_;
  PaintLocalizedFields(*rom, id);
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  PaintROMFile(*rom, id);
}

void ROMWindow::PaintCover(ROM& rom, int id) {
  const kiwi::base::FilePath dropped_cover = GetDroppedJPG();
  if (!dropped_cover.empty()) {
    FillCoverData(rom, dropped_cover);
    ClearDroppedJPG();
    dirty_ = true;
  }

  ImGui::TextColored(kMuted, U8("封面"));
  const ImVec2 frame_size(ImGui::GetContentRegionAvail().x, 260);
  ImGui::BeginChild(GetUniqueName("##CoverFrame", id).c_str(), frame_size,
                    ImGuiChildFlags_Borders);
  if (rom.boxart_texture_) {
    int width = 0;
    int height = 0;
    SDL_QueryTexture(rom.boxart_texture_, nullptr, nullptr, &width, &height);
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float scale = std::min(available.x / static_cast<float>(width),
                                 available.y / static_cast<float>(height));
    const ImVec2 image_size(width * scale, height * scale);
    ImGui::SetCursorPos(
        ImVec2((ImGui::GetWindowWidth() - image_size.x) * 0.5f,
               (ImGui::GetWindowHeight() - image_size.y) * 0.5f));
    ImGui::Image(reinterpret_cast<ImTextureID>(rom.boxart_texture_),
                 image_size);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone) &&
        ImGui::BeginTooltip()) {
      const float preview_scale =
          std::min(1.0f, 500.0f / static_cast<float>(std::max(width, height)));
      ImGui::Image(reinterpret_cast<ImTextureID>(rom.boxart_texture_),
                   ImVec2(width * preview_scale, height * preview_scale));
      ImGui::EndTooltip();
    }
  } else {
    const char* empty_text = U8("拖入 JPG 或使用下方操作添加封面");
    const ImVec2 text_size = ImGui::CalcTextSize(empty_text);
    ImGui::SetCursorPos(
        ImVec2((ImGui::GetWindowWidth() - text_size.x) * 0.5f,
               (ImGui::GetWindowHeight() - text_size.y) * 0.5f));
    ImGui::TextColored(kMuted, "%s", empty_text);
  }
  ImGui::EndChild();

  const float half_width = (ImGui::GetContentRegionAvail().x - 8) * 0.5f;
  if (ImGui::Button(GetUniqueName(U8("匹配封面"), id).c_str(),
                    ImVec2(half_width, 0))) {
    const kiwi::base::FilePath suggested_url = TryFetchCoverByName(
        rom, kiwi::base::FilePath::FromUTF8Unsafe(rom.rom_file_name));
    if (!suggested_url.empty())
      ShellOpen(suggested_url);
  }
  ImGui::SameLine();
  if (ImGui::Button(GetUniqueName(U8("从剪贴板"), id).c_str(),
                    ImVec2(half_width, 0))) {
    const std::vector<uint8_t> pasted = ReadImageAsJPGFromClipboard();
    if (!pasted.empty()) {
      FillCoverData(rom, pasted);
      dirty_ = true;
    }
  }
  if (ImGui::Button(GetUniqueName(U8("顺时针旋转"), id).c_str(),
                    ImVec2(-FLT_MIN, 0))) {
    const std::vector<uint8_t> rotated = RotateJPEG(rom.boxart_data);
    if (!rotated.empty()) {
      FillCoverData(rom, rotated);
      dirty_ = true;
    }
  }
}

void ROMWindow::PaintValidation(const ROM& rom) {
  ImGui::TextColored(kMuted, U8("保存前检查"));
  const bool has_rom = !rom.rom_data.empty() && rom.rom_file_name[0];
  const bool has_titles =
      rom.zh[0] && rom.zh_hint[0] && rom.ja[0] && rom.ja_hint[0];
  const bool has_cover = !rom.boxart_data.empty();
  std::string mapper;
  const bool supported =
      system_ == ROMSystem::kGBA || IsMapperSupported(rom.rom_data, mapper);
  ValidationLine(has_rom, U8("ROM 数据"), has_rom ? U8("完整") : U8("缺失"));
  ValidationLine(has_titles, U8("本地化字段"),
                 has_titles ? U8("完整") : U8("待补全"));
  ValidationLine(has_cover, U8("封面"), has_cover ? U8("已设置") : U8("缺失"));
  ValidationLine(
      supported, U8("模拟支持"),
      system_ == ROMSystem::kNES
          ? (mapper.empty() ? U8("未知") : ("Mapper " + mapper).c_str())
          : U8("GBA"));
  ValidationLine(tested_, U8("Debug App 验证"),
                 tested_ ? U8("已执行") : U8("尚未执行"));
}

void ROMWindow::PaintInspector() {
  ImGui::TextUnformatted(U8("资源检查器"));
  ImGui::Separator();
  ROM* rom = selected_rom();
  if (!rom) {
    ImGui::TextColored(kMuted, U8("当前资源包中没有 ROM。"));
    return;
  }

  const int id = selected_rom_index_;
  PaintCover(*rom, id);
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::TextColored(kMuted, U8("ROM 信息"));
  ImGui::Text(U8("文件: %s"), rom->rom_file_name);
  ImGui::Text(U8("Key: %s"), rom->key.c_str());
  ImGui::Text(U8("大小: %zu KB"), rom->rom_data.size() / 1024);
  if (system_ == ROMSystem::kNES) {
    std::string mapper;
    const bool supported = IsMapperSupported(rom->rom_data, mapper);
    ImGui::Text(U8("Mapper: %s"), mapper.c_str());
    ImGui::TextColored(supported ? kGreen : kRed, U8("KiwiMachine: %s"),
                       supported ? U8("支持") : U8("不支持"));
  } else {
    ImGui::TextColored(kGreen, U8("KiwiMachine: 支持 GBA ROM"));
  }

  ImGui::Spacing();
  ImGui::Separator();
  PaintValidation(*rom);
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::TextColored(kMuted, U8("目标路径"));
  ImGui::TextWrapped(
      "%s", GetWorkspace().GetZippedPath(system_).AsUTF8Unsafe().c_str());
}

void ROMWindow::PaintFooter() {
  ImGui::TextColored(dirty_ ? kAmber : kMuted,
                     dirty_ ? U8("● 有未保存更改") : U8("● 所有更改已保存"));

  const float button_area = 350;
  ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 20,
                           ImGui::GetWindowWidth() - button_area));
  if (ImGui::Button(U8("关闭编辑器")))
    Close();
  ImGui::SameLine();
  if (ImGui::Button(U8("保存 ZIP")))
    Save();
  ImGui::SameLine();
  if (PrimaryButton(U8("保存并测试"), ImVec2(110, 0)))
    SaveAndTest();
}

void ROMWindow::PaintDialogs() {
  const std::string popup_name = GetUniqueName(U8("保存结果"), window_id_);
  if (show_message_box_) {
    ImGui::OpenPopup(popup_name.c_str());
    show_message_box_ = false;
  }
  if (!ImGui::BeginPopupModal(popup_name.c_str(), nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  if (!generated_packaged_path_.empty()) {
    ImGui::TextWrapped(U8("保存成功：%s"),
                       generated_packaged_path_.AsUTF8Unsafe().c_str());
    if (ImGui::Button(U8("打开 ZIP")))
      ShellOpen(generated_packaged_path_);
    ImGui::SameLine();
    if (ImGui::Button(U8("打开目录")))
      ShellOpenDirectory(generated_packaged_path_);
    ImGui::SameLine();
    if (PrimaryButton(U8("测试包"))) {
      PackSingleZipAndRun(generated_packaged_path_,
                          GetWorkspace().GetTestPath(), system_);
      tested_ = true;
    }
  } else {
    ImGui::TextColored(kRed, U8("保存失败，请检查终端日志。"));
  }
  ImGui::SameLine();
  if (ImGui::Button(U8("关闭")))
    ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

void ROMWindow::Save() {
  generated_packaged_path_ =
      SaveROMs(GetWorkspace().GetZippedPath(system_), roms_);
  if (!generated_packaged_path_.empty()) {
    file_ = generated_packaged_path_;
    dirty_ = false;
    NotifySaved(system_, generated_packaged_path_);
  }
  show_message_box_ = true;
}

void ROMWindow::SaveAndTest() {
  generated_packaged_path_ =
      SaveROMs(GetWorkspace().GetZippedPath(system_), roms_);
  if (!generated_packaged_path_.empty()) {
    file_ = generated_packaged_path_;
    dirty_ = false;
    NotifySaved(system_, generated_packaged_path_);
    PackSingleZipAndRun(generated_packaged_path_, GetWorkspace().GetTestPath(),
                        system_);
    tested_ = true;
  }
  show_message_box_ = true;
}

void ROMWindow::TestROM(const ROM& rom) {
  const kiwi::base::FilePath output_rom =
      WriteROM(rom.rom_file_name, rom.rom_data, GetWorkspace().GetTestPath());
  if (output_rom.empty())
    return;

  kiwi::base::FilePath executable_path;
  if (!FLAGS_km_path.empty())
    executable_path = kiwi::base::FilePath::FromUTF8Unsafe(FLAGS_km_path);
#if BUILDFLAG(IS_MAC)
  if (executable_path.empty())
    executable_path =
        kiwi::base::FilePath(FILE_PATH_LITERAL("Kiwi Machine.app"));
  RunExecutable(executable_path,
                {"--test-rom=" + output_rom.AsUTF8Unsafe(), "--enable_debug"});
#elif BUILDFLAG(IS_WIN)
  if (executable_path.empty())
    executable_path =
        kiwi::base::FilePath(FILE_PATH_LITERAL("kiwi_machine.exe"));
  RunExecutable(executable_path, {L"--test-rom=\"" + output_rom.value() + L"\"",
                                  L"--enable_debug"});
#else
  if (executable_path.empty())
    executable_path = kiwi::base::FilePath(FILE_PATH_LITERAL("kiwi_machine"));
  RunExecutable(executable_path,
                {"--test-rom=" + output_rom.AsUTF8Unsafe(), "--enable_debug"});
#endif
  tested_ = true;
}

void ROMWindow::Close() {
  closed_ = true;
}

std::string ROMWindow::GetUniqueName(const std::string& name, int unique_id) {
  const std::string unique_part =
      std::to_string(unique_id) +
      std::to_string(reinterpret_cast<uintptr_t>(this));
  return name + "##" + name + unique_part;
}

void ROMWindow::Painted() {
  if (to_be_deleted_ >= 0 && to_be_deleted_ < static_cast<int>(roms_.size())) {
    ROM& rom = roms_[to_be_deleted_];
    if (rom.boxart_texture_) {
      SDL_DestroyTexture(rom.boxart_texture_);
      rom.boxart_texture_ = nullptr;
    }
    roms_.erase(roms_.begin() + to_be_deleted_);
    dirty_ = true;
    selected_rom_index_ =
        roms_.empty()
            ? 0
            : std::min(selected_rom_index_, static_cast<int>(roms_.size()) - 1);
  }
  to_be_deleted_ = -1;
}

void ROMWindow::FillCoverData(ROM& rom, const kiwi::base::FilePath& path) {
  const auto data = kiwi::base::ReadFileToBytes(path);
  if (data)
    FillCoverData(rom, *data);
}

void ROMWindow::FillCoverData(ROM& rom, const std::vector<uint8_t>& data) {
  if (data.empty())
    return;
  SDL_RWops* source =
      SDL_RWFromConstMem(data.data(), static_cast<int>(data.size()));
  if (!source)
    return;
  SDL_Texture* texture = IMG_LoadTextureTyped_RW(renderer_, source, 1, nullptr);
  if (!texture)
    return;

  SDL_SetTextureScaleMode(texture, SDL_ScaleModeBest);
  if (rom.boxart_texture_)
    SDL_DestroyTexture(rom.boxart_texture_);
  rom.boxart_texture_ = texture;
  UpdateCover(rom, data);
}

void ROMWindow::UpdateCover(ROM& rom, const std::vector<uint8_t>& data) {
  SDL_LockMutex(cover_update_mutex_);
  rom.boxart_data = data;
  SDL_UnlockMutex(cover_update_mutex_);
}

kiwi::base::FilePath ROMWindow::SaveROMs(const kiwi::base::FilePath& save_path,
                                         const ROMS& roms) {
  return WriteZip(save_path, roms);
}

void ROMWindow::NewRom() {
  ROM new_rom;
  new_rom.system = system_;
  if (roms_.empty()) {
    new_rom.key = "default";
  } else {
    kiwi::base::strlcpy(new_rom.zh, roms_[0].zh, sizeof(new_rom.zh));
    kiwi::base::strlcpy(new_rom.zh_hint, roms_[0].zh_hint,
                        sizeof(new_rom.zh_hint));
    kiwi::base::strlcpy(new_rom.ja, roms_[0].ja, sizeof(new_rom.ja));
    kiwi::base::strlcpy(new_rom.ja_hint, roms_[0].ja_hint,
                        sizeof(new_rom.ja_hint));
    new_rom.key = "version-" + std::to_string(roms_.size() + 1);
  }
  roms_.push_back(std::move(new_rom));
  selected_rom_index_ = static_cast<int>(roms_.size()) - 1;
  dirty_ = true;
  tested_ = false;
}

kiwi::base::FilePath ROMWindow::TryFetchCoverByName(
    ROM& rom,
    const kiwi::base::FilePath& rom_base_name) {
  kiwi::base::FilePath suggested_url;
  const std::vector<uint8_t> data = TryFetchBoxArtImage(
      rom_base_name.AsUTF8Unsafe(), system_, &suggested_url);
  if (!data.empty()) {
    FillCoverData(rom, data);
    dirty_ = true;
  }
  return suggested_url;
}
