// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "package_manager_widgets.h"

#include <algorithm>

namespace package_manager {
namespace {

constexpr ImVec4 kGreen = ImVec4(0.18f, 0.49f, 0.35f, 1.00f);
constexpr ImVec4 kGreenDark = ImVec4(0.13f, 0.38f, 0.27f, 1.00f);
constexpr ImVec4 kGreenSoft = ImVec4(0.86f, 0.94f, 0.90f, 1.00f);

size_t PreviousUTF8Character(const std::string& text, size_t position) {
  if (position == 0)
    return 0;
  --position;
  while (position > 0 &&
         (static_cast<unsigned char>(text[position]) & 0xc0) == 0x80) {
    --position;
  }
  return position;
}

std::string EllipsizePath(const std::string& path, float max_width) {
  if (ImGui::CalcTextSize(path.c_str()).x <= max_width)
    return path;

  constexpr char kEllipsis[] = "...";
  if (ImGui::CalcTextSize(kEllipsis).x >= max_width)
    return kEllipsis;

  std::string result = kEllipsis;
  size_t suffix_start = path.size();
  while (suffix_start > 0) {
    const size_t candidate_start = PreviousUTF8Character(path, suffix_start);
    const std::string candidate =
        std::string(kEllipsis) + path.substr(candidate_start);
    if (ImGui::CalcTextSize(candidate.c_str()).x > max_width)
      break;
    result = candidate;
    suffix_start = candidate_start;
  }
  return result;
}

}  // namespace

bool PrimaryButton(const char* label, const ImVec2& size) {
  ImGui::PushStyleColor(ImGuiCol_Button, kGreen);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.22f, 0.56f, 0.40f, 1.00f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, kGreenDark);
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
  const bool pressed = ImGui::Button(label, size);
  ImGui::PopStyleColor(4);
  return pressed;
}

bool FilterButton(const std::string& label, bool selected, const ImVec2& size) {
  if (selected) {
    ImGui::PushStyleColor(ImGuiCol_Button, kGreenSoft);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kGreenSoft);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kGreenSoft);
    ImGui::PushStyleColor(ImGuiCol_Text, kGreenDark);
  }
  const bool pressed = ImGui::Button(label.c_str(), size);
  if (selected)
    ImGui::PopStyleColor(4);
  return pressed;
}

void EllipsizedPathText(const std::string& path,
                        float max_width,
                        const ImVec4& color) {
  const std::string displayed = EllipsizePath(path, std::max(1.0f, max_width));
  ImGui::TextColored(color, "%s", displayed.c_str());
  if (displayed != path && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(std::min(700.0f, ImGui::GetFontSize() * 45.0f));
    ImGui::TextUnformatted(path.c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

void LoadingSpinner(const char* id,
                    float radius,
                    float thickness,
                    const ImVec4& color) {
  const ImVec2 size(radius * 2.0f, radius * 2.0f);
  const ImVec2 position = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton(id, size);

  constexpr float kPi = 3.14159265358979323846f;
  const float start = static_cast<float>(ImGui::GetTime() * 4.0);
  const ImVec2 center(position.x + radius, position.y + radius);
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->PathArcTo(center, radius, start, start + kPi * 1.55f, 32);
  draw_list->PathStroke(ImGui::GetColorU32(color), 0, thickness);
}

}  // namespace package_manager
