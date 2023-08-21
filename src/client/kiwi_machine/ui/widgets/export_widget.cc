// Copyright (C) 2023 Yisi Yu
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

#include "ui/widgets/export_widget.h"

#include <imgui.h>

ExportWidget::ExportWidget(WindowBase* window_base) : Widget(window_base) {
  set_flags(ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
  set_title("Export");
}

ExportWidget::~ExportWidget() = default;

void ExportWidget::Start(int max, const kiwi::base::FilePath& export_path) {
  current_ = 0;
  max_ = max;
  export_path_ = export_path;
  succeeded_files_.clear();
  failed_files_.clear();
  is_started_ = true;
  set_visible(true);
}

void ExportWidget::SetCurrent(const kiwi::base::FilePath& file) {
  current_text_ = file.AsUTF8Unsafe();
  ++current_;
}

void ExportWidget::Succeeded(const kiwi::base::FilePath& file) {
  succeeded_files_.push_back(file);
  ++current_;
}

void ExportWidget::Failed(const kiwi::base::FilePath& file) {
  failed_files_.push_back(file);
  ++current_;
}

void ExportWidget::Done() {
  is_started_ = false;
}

void ExportWidget::Paint() {
  if (is_started_) {
    ImGui::ProgressBar(static_cast<float>(current_) / max_, ImVec2(200, 25),
                       current_text_.c_str());
  } else {
    ImGui::TextWrapped("Roms are exported to %s\n%d succeeded, %d failed.",
                       export_path_.AsUTF8Unsafe().c_str(),
                       static_cast<int>(succeeded_files_.size()),
                       static_cast<int>(failed_files_.size()));
    ImGui::Text("Succeeded roms:");
    for (const auto& f : succeeded_files_) {
      ImGui::TextColored(ImVec4(0, 255, 0, 255), "%s",
                         f.AsUTF8Unsafe().c_str());
    }

    ImGui::Text("Failed roms:");
    for (const auto& f : failed_files_) {
      ImGui::TextColored(ImVec4(0, 255, 0, 255), "%s",
                         f.AsUTF8Unsafe().c_str());
    }

    if (ImGui::Button("Done")) {
      set_visible(false);
    }
  }
}
