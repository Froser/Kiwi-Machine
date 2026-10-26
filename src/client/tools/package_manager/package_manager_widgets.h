// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef PACKAGE_MANAGER_WIDGETS_H_
#define PACKAGE_MANAGER_WIDGETS_H_

#include <imgui.h>

#include <string>

namespace package_manager {

bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
bool FilterButton(const std::string& label, bool selected, const ImVec2& size);
void EllipsizedPathText(const std::string& path,
                        float max_width,
                        const ImVec4& color);
void LoadingSpinner(const char* id,
                    float radius,
                    float thickness,
                    const ImVec4& color);

}  // namespace package_manager

#endif  // PACKAGE_MANAGER_WIDGETS_H_
