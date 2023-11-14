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

#include "ui/widgets/loading_widget.h"

#include <imgui.h>
#include <cmath>

#include "ui/main_window.h"

constexpr float kCircleScale = 15.f;

LoadingWidget::LoadingWidget(MainWindow* main_window)
    : Widget(main_window), main_window_(main_window) {
  set_title("Loading Widget");
  set_flags(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);
}

LoadingWidget::~LoadingWidget() = default;

SDL_Rect LoadingWidget::CalculateCircleAABB(int* indicator_radius_out) {
  SDL_Rect aabb = MapToParent(spinning_bounds_);
  // Scaling the spinning bounds.
  aabb.w *= main_window_->window_scale();
  aabb.h *= main_window_->window_scale();

  // Uses the shortest edge to calculate radius.
  int min_length = std::min(aabb.w, aabb.h);
  aabb.w = aabb.h = min_length;
  float indicator_radius = (aabb.w - ImGui::GetStyle().FramePadding.x * 2) / 2;
  float kCircleRadius = indicator_radius / kCircleScale;
  indicator_radius -= kCircleRadius * 2;

  SDL_Rect circle_aabb = {
      static_cast<int>(aabb.x + (aabb.w - indicator_radius * 2) / 2.f),
      static_cast<int>(aabb.y + (aabb.h - indicator_radius * 2) / 2.f),
      static_cast<int>(indicator_radius * 2),
      static_cast<int>(indicator_radius * 2)};

  if (indicator_radius_out)
    *indicator_radius_out = indicator_radius;
  return circle_aabb;
}

void LoadingWidget::Paint() {
  SDL_Rect client_bounds = window()->GetClientBounds();
  if (first_paint_) {
    timer_.Start();
    set_bounds(SDL_Rect{0, 0, client_bounds.w, client_bounds.h});
    first_paint_ = false;
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  int indicator_radius;
  SDL_Rect circle_aabb = CalculateCircleAABB(&indicator_radius);
  float kCircleRadius = indicator_radius / kCircleScale;

  const auto degree_offset = 2.0f * M_PI / circle_count_;
  float elapsed_ms = timer_.ElapsedInMilliseconds();
  for (int i = 0; i < circle_count_; ++i) {
    const auto x = indicator_radius * std::sin(degree_offset * i);
    const auto y = indicator_radius * std::cos(degree_offset * i);
    const auto growth =
        std::max(0.0, std::sin(elapsed_ms * speed_ - i * degree_offset));
    ImVec4 color;
    color.x = color_.r * growth + backdrop_color_.r * (1.0f - growth);
    color.y = color_.g * growth + backdrop_color_.g * (1.0f - growth);
    color.z = color_.b * growth + backdrop_color_.b * (1.0f - growth);
    color.w = color_.a * growth + backdrop_color_.a * (1.0f - growth);
    draw_list->AddCircleFilled(ImVec2(circle_aabb.x + indicator_radius + x,
                                      circle_aabb.y + indicator_radius - y),
                               kCircleRadius + growth * kCircleRadius,
                               ImGui::GetColorU32(color));
  }
}