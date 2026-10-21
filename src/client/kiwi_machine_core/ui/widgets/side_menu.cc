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

#include "ui/widgets/side_menu.h"
#include "build/build_config.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

#include "models/nes/runtime.h"
#include "ui/main_window.h"
#include "ui/styles.h"
#include "utility/audio_effects.h"
#include "utility/fonts.h"
#include "utility/images.h"
#include "utility/key_mapping_util.h"
#include "utility/localization.h"
#include "utility/math.h"

const int kItemHeight = styles::side_menu::GetItemHeight();
const int kButtonHeight = styles::side_menu::GetButtonHeight();
const int kItemMarginBottom = styles::side_menu::GetMarginBottom();
constexpr ImVec2 kItemSpacing(3, 10);
constexpr int kItemAnimationMs = 50;
constexpr int kHoverAnimationMs = 180;
constexpr int kSystemToggleAnimationMs = 150;
constexpr int kPointerCollapseDelayMs = 120;
#if BUILDFLAG(IS_SWITCH)
constexpr int kIconInset = 24;
constexpr int kTextSpacing = 16;
constexpr float kIconSizeScale = .78f;
constexpr float kFontSizeScale = 1.25f;
#else
constexpr int kIconInset = 4;
constexpr int kTextSpacing = 4;
constexpr float kIconSizeScale = .7f;
constexpr float kFontSizeScale = 1.f;
#endif
constexpr ImU32 kBackgroundColor = IM_COL32(244, 252, 227, 255);
constexpr ImU32 kBorderColor = IM_COL32(116, 184, 22, 255);
constexpr ImU32 kDividerColor = IM_COL32(216, 245, 162, 255);
constexpr ImU32 kTextColor = IM_COL32(52, 58, 64, 255);
constexpr ImU32 kMutedIconColor = IM_COL32(73, 80, 87, 255);
constexpr ImU32 kSelectedBackgroundColor = IM_COL32(216, 245, 162, 255);
constexpr ImU32 kHoverFillColor = IM_COL32(233, 250, 200, 255);
constexpr ImU32 kAccentColor = IM_COL32(116, 184, 22, 255);
constexpr ImU32 kAccentTextColor = IM_COL32(92, 148, 13, 255);
constexpr ImU32 kGBABackgroundColor = IM_COL32(242, 241, 248, 255);
constexpr ImU32 kGBABorderColor = IM_COL32(81, 70, 155, 255);
constexpr ImU32 kGBADividerColor = IM_COL32(224, 222, 242, 255);
constexpr ImU32 kGBASelectedBackgroundColor = IM_COL32(224, 222, 242, 255);
constexpr ImU32 kGBAHoverFillColor = IM_COL32(234, 232, 247, 255);
constexpr ImU32 kGBAAccentColor = IM_COL32(81, 70, 155, 255);
constexpr ImU32 kGBAAccentTextColor = IM_COL32(71, 61, 139, 255);
#if BUILDFLAG(IS_SWITCH)
constexpr Sint16 kSystemToggleTriggerThreshold = 16000;
constexpr Sint16 kSystemToggleTriggerReleaseThreshold = 8000;
#endif
const PreferredFontSize kPreferredFontSize(
    styles::side_menu::GetPreferredFontSize());

#define SCALED(x) \
  static_cast<int>((main_window_->window_scale() >= 3.f ? (x) : ((x) / 1.5f)))

namespace {

ImU32 ColorWithOpacity(ImU32 color, float opacity) {
  const int alpha = static_cast<int>(255.f * std::clamp(opacity, 0.f, 1.f));
  return (color & ~IM_COL32_A_MASK) |
         (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
}

ImU32 BlendColor(ImU32 from, ImU32 to, float progress) {
  progress = std::clamp(progress, 0.f, 1.f);
  auto blend_channel = [progress](int shift) {
    return [=](ImU32 from_color, ImU32 to_color) {
      const float from_channel = (from_color >> shift) & 0xff;
      const float to_channel = (to_color >> shift) & 0xff;
      return static_cast<ImU32>(from_channel +
                                (to_channel - from_channel) * progress);
    };
  };
  const ImU32 red = blend_channel(IM_COL32_R_SHIFT)(from, to);
  const ImU32 green = blend_channel(IM_COL32_G_SHIFT)(from, to);
  const ImU32 blue = blend_channel(IM_COL32_B_SHIFT)(from, to);
  const ImU32 alpha = blend_channel(IM_COL32_A_SHIFT)(from, to);
  return IM_COL32(red, green, blue, alpha);
}

float GetFontRenderSize(const ScopedFont& font) {
  return font.GetFontSize() * kFontSizeScale;
}

ImVec2 MeasureText(const ScopedFont& font, const char* text) {
  return font.GetFont()->CalcTextSizeA(GetFontRenderSize(font), FLT_MAX, 0.f,
                                       text);
}

float EaseOutCubic(float progress) {
  progress = std::clamp(progress, 0.f, 1.f);
  return 1.f - std::pow(1.f - progress, 3.f);
}

}  // namespace

SideMenu::SideMenu(MainWindow* main_window)
    : Widget(main_window), main_window_(main_window) {
  set_flags(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
  set_title("SideMenu");
}

SideMenu::~SideMenu() = default;

void SideMenu::set_activate(bool activate) {
  if (activate_ == activate)
    return;

  if (activate) {
    pointer_expanded_ = false;
    pointer_collapse_pending_ = false;
  } else {
    pointer_expanded_ = false;
    pointer_collapse_pending_ = false;
    suppress_pointer_expansion_ = pointer_inside_;
  }
  activate_ = activate;
}

void SideMenu::Paint() {
  Layout();
  UpdateHoverState();
  UpdateHoverAnimations();

  const bool is_gba = IsGBASystemSelected();
  const ImU32 background_color =
      is_gba ? kGBABackgroundColor : kBackgroundColor;
  const ImU32 border_color = is_gba ? kGBABorderColor : kBorderColor;
  const ImU32 divider_color = is_gba ? kGBADividerColor : kDividerColor;
  const ImU32 selected_background_color =
      is_gba ? kGBASelectedBackgroundColor : kSelectedBackgroundColor;
  const ImU32 hover_fill_color = is_gba ? kGBAHoverFillColor : kHoverFillColor;
  const ImU32 accent_color = is_gba ? kGBAAccentColor : kAccentColor;
  const ImU32 accent_text_color =
      is_gba ? kGBAAccentTextColor : kAccentTextColor;

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const SDL_Rect kBoundsToWindow = MapToWindow(bounds());
  draw_list->AddRectFilled(ImVec2(kBoundsToWindow.x, kBoundsToWindow.y),
                           ImVec2(kBoundsToWindow.x + kBoundsToWindow.w,
                                  kBoundsToWindow.y + kBoundsToWindow.h),
                           background_color);
  draw_list->AddLine(
      ImVec2(kBoundsToWindow.x + kBoundsToWindow.w - 1, kBoundsToWindow.y),
      ImVec2(kBoundsToWindow.x + kBoundsToWindow.w - 1,
             kBoundsToWindow.y + kBoundsToWindow.h),
      border_color);

  PaintSystemToggle(draw_list, divider_color, hover_fill_color, accent_color,
                    accent_text_color);

  for (int i = 0; i < button_items_.size(); ++i) {
    const SDL_Rect button_bounds =
        MapLocalBoundsToWindow(buttons_bounds_map_[i]);
    const int kIconSize = kIconSizeScale * button_bounds.h;
    const int kIconTop = button_bounds.y + (button_bounds.h - kIconSize) / 2;
#if BUILDFLAG(IS_SWITCH)
    const int kIconLeft =
        expanded() ? button_bounds.x + kIconInset
                   : button_bounds.x + (button_bounds.w - kIconSize) / 2;
#else
    const int kIconLeft = button_bounds.x + SCALED(kIconInset);
#endif
    const bool selected = button_items_[i].callbacks.is_selected &&
                          button_items_[i].callbacks.is_selected.Run();
    const float hover_opacity = selected ? 0.f : button_hover_opacities_[i];

    if (selected) {
      draw_list->AddRectFilled(ImVec2(button_bounds.x, button_bounds.y),
                               ImVec2(button_bounds.x + button_bounds.w,
                                      button_bounds.y + button_bounds.h),
                               selected_background_color);
      const int accent_height = static_cast<int>(button_bounds.h * .56f);
      const int accent_top =
          button_bounds.y + (button_bounds.h - accent_height) / 2;
      draw_list->AddRectFilled(
          ImVec2(button_bounds.x, accent_top),
          ImVec2(button_bounds.x + SCALED(3), accent_top + accent_height),
          accent_color, SCALED(2), ImDrawFlags_RoundCornersRight);
    } else if (hover_opacity > 0.f) {
      draw_list->AddRectFilled(
          ImVec2(button_bounds.x, button_bounds.y),
          ImVec2(button_bounds.x + button_bounds.w,
                 button_bounds.y + button_bounds.h),
          ColorWithOpacity(hover_fill_color, hover_opacity), SCALED(4));
    }

    draw_list->AddLine(
        ImVec2(button_bounds.x, button_bounds.y + button_bounds.h - 1),
        ImVec2(button_bounds.x + button_bounds.w,
               button_bounds.y + button_bounds.h - 1),
        divider_color);

    if (expanded()) {
      std::string contents =
          button_items_[i].string_updater->GetLocalizedString();
      ScopedFont font = GetPreferredFont(kPreferredFontSize, contents.c_str());
      const ImVec2 text_size = MeasureText(font, contents.c_str());
      int text_top = button_bounds.y + (button_bounds.h - text_size.y) / 2;
      const float text_left = kIconLeft + kIconSize + SCALED(kTextSpacing);
      const ImVec4 text_clip(
          text_left, button_bounds.y,
          button_bounds.x + button_bounds.w - SCALED(kTextSpacing),
          button_bounds.y + button_bounds.h);
      draw_list->AddText(
          font.GetFont(), GetFontRenderSize(font), ImVec2(text_left, text_top),
          selected ? accent_text_color
                   : BlendColor(kTextColor, accent_text_color, hover_opacity),
          contents.c_str(), nullptr, 0.f, &text_clip);
    }

    SDL_Texture* icon_texture =
        GetImage(window()->renderer(), button_items_[i].icon);
    draw_list->AddImage(
        reinterpret_cast<ImTextureID>(icon_texture),
        ImVec2(kIconLeft, kIconTop),
        ImVec2(kIconLeft + kIconSize, kIconTop + kIconSize), ImVec2(0, 0),
        ImVec2(1, 1),
        selected
            ? accent_text_color
            : BlendColor(kMutedIconColor, accent_text_color, hover_opacity));
  }

  for (int i = menu_items_.size() - 1; i >= 0; --i) {
    if (!IsMenuVisible(i))
      continue;

    const SDL_Rect global_item_rect =
        MapLocalBoundsToWindow(items_bounds_map_[i]);
    const int kIconSize = kIconSizeScale * global_item_rect.h;
#if BUILDFLAG(IS_SWITCH)
    const int kIconLeft =
        expanded() ? global_item_rect.x + kIconInset
                   : global_item_rect.x + (global_item_rect.w - kIconSize) / 2;
#else
    const int kIconLeft = global_item_rect.x + SCALED(kIconInset);
#endif
    const int kIconTop =
        global_item_rect.y + (global_item_rect.h - kIconSize) / 2;

    std::string menu_content =
        menu_items_[i].string_updater->GetLocalizedString();
    ScopedFont font =
        GetPreferredFont(kPreferredFontSize, menu_content.c_str());
    const bool selected = i == current_index_;
    const float hover_opacity = selected ? 0.f : menu_hover_opacities_[i];
    if (hover_opacity > 0.f) {
      draw_list->AddRectFilled(
          ImVec2(global_item_rect.x, global_item_rect.y),
          ImVec2(global_item_rect.x + global_item_rect.w,
                 global_item_rect.y + global_item_rect.h),
          ColorWithOpacity(hover_fill_color, hover_opacity), SCALED(4));
    }

    if (selected) {
      // Animation
      if (SDL_RectEmpty(&selection_current_rect_in_global_)) {
        selection_current_rect_in_global_ = items_bounds_map_[i];
      }
      selection_target_rect_in_global_ = items_bounds_map_[i];
      float percentage =
          timer_.ElapsedInMilliseconds() / static_cast<float>(kItemAnimationMs);
      if (percentage >= 1.f) {
        percentage = 1.f;
        selection_current_rect_in_global_ = selection_target_rect_in_global_;
      }

      SDL_Rect global_selection_rect = MapLocalBoundsToWindow(
          Lerp(selection_current_rect_in_global_,
               selection_target_rect_in_global_, percentage));
      SDL_Rect global_target_selection_rect =
          MapLocalBoundsToWindow(selection_target_rect_in_global_);

      draw_list->AddRectFilled(
          ImVec2(global_selection_rect.x, global_selection_rect.y),
          ImVec2(global_selection_rect.x + global_selection_rect.w,
                 global_selection_rect.y + global_selection_rect.h),
          selected_background_color);
      const int accent_height =
          static_cast<int>(global_selection_rect.h * .56f);
      const int accent_top = global_selection_rect.y +
                             (global_selection_rect.h - accent_height) / 2;
      draw_list->AddRectFilled(ImVec2(global_selection_rect.x, accent_top),
                               ImVec2(global_selection_rect.x + SCALED(3),
                                      accent_top + accent_height),
                               accent_color, SCALED(2),
                               ImDrawFlags_RoundCornersRight);

      if (expanded()) {
        // Calculates text area
        const ImVec2 text_size = MeasureText(font, menu_content.c_str());
        const int text_top = global_target_selection_rect.y +
                             (global_target_selection_rect.h - text_size.y) / 2;
        const float text_left = kIconLeft + kIconSize + SCALED(kTextSpacing);
        const ImVec4 text_clip(
            text_left, global_target_selection_rect.y,
            global_target_selection_rect.x + global_target_selection_rect.w -
                SCALED(kTextSpacing),
            global_target_selection_rect.y + global_target_selection_rect.h);
        draw_list->AddText(font.GetFont(), GetFontRenderSize(font),
                           ImVec2(text_left, text_top), accent_text_color,
                           menu_content.c_str(), nullptr, 0.f, &text_clip);
      }
    } else if (expanded()) {
      // Calculates text area
      const ImVec2 text_size = MeasureText(font, menu_content.c_str());
      const int text_top =
          global_item_rect.y + (global_item_rect.h - text_size.y) / 2;
      const float text_left = kIconLeft + kIconSize + SCALED(kTextSpacing);
      const ImVec4 text_clip(
          text_left, global_item_rect.y,
          global_item_rect.x + global_item_rect.w - SCALED(kTextSpacing),
          global_item_rect.y + global_item_rect.h);
      draw_list->AddText(
          font.GetFont(), GetFontRenderSize(font), ImVec2(text_left, text_top),
          BlendColor(kTextColor, accent_text_color, hover_opacity),
          menu_content.c_str(), nullptr, 0.f, &text_clip);
    }

    // Use the monochrome source for every state and tint it consistently.
    SDL_Texture* icon_texture =
        GetImage(window()->renderer(), menu_items_[i].icon);
    draw_list->AddImage(
        reinterpret_cast<ImTextureID>(icon_texture),
        ImVec2(kIconLeft, kIconTop),
        ImVec2(kIconLeft + kIconSize, kIconTop + kIconSize), ImVec2(0, 0),
        ImVec2(1, 1),
        selected
            ? accent_text_color
            : BlendColor(kMutedIconColor, accent_text_color, hover_opacity));
  }
}

void SideMenu::AddMenu(std::unique_ptr<LocalizedStringUpdater> string_updater,
                       image_resources::ImageID icon,
                       image_resources::ImageID highlight_icon,
                       MenuCallbacks callbacks) {
  menu_items_.emplace_back(std::move(string_updater), icon, highlight_icon,
                           callbacks);
  invalidate();

  // When the first widget is added, trigger its selected callback, because it
  // is selected by default.
  if (menu_items_.size() == 1 && auto_trigger_first_menu_)
    menu_items_[0].callbacks.trigger_callback.Run(0);
}

void SideMenu::AddMenu(std::unique_ptr<LocalizedStringUpdater> string_updater,
                       const kiwi::nes::Bytes& icon_data,
                       const kiwi::nes::Bytes& highlight_icon_data,
                       MenuCallbacks callbacks) {
  AddMenu(std::move(string_updater), ImageRegister(icon_data),
          ImageRegister(highlight_icon_data), std::move(callbacks));
}

void SideMenu::AddButton(std::unique_ptr<LocalizedStringUpdater> string_updater,
                         image_resources::ImageID icon,
                         ButtonCallbacks callbacks,
                         SDL_KeyCode hotkey,
                         SDL_GameControllerButton controller_hotkey) {
  button_items_.push_back(ButtonItem(std::move(string_updater), icon,
                                     std::move(callbacks), hotkey,
                                     controller_hotkey));
}

void SideMenu::SetSystemToggle(
    std::unique_ptr<LocalizedStringUpdater> nes_string_updater,
    std::unique_ptr<LocalizedStringUpdater> gba_string_updater,
    image_resources::ImageID nes_icon,
    image_resources::ImageID gba_icon,
    SystemToggleCallbacks callbacks) {
  system_toggle_ = std::make_unique<SystemToggleItem>();
  system_toggle_->nes_string_updater = std::move(nes_string_updater);
  system_toggle_->gba_string_updater = std::move(gba_string_updater);
  system_toggle_->nes_icon = nes_icon;
  system_toggle_->gba_icon = gba_icon;
  system_toggle_->callbacks = std::move(callbacks);
  system_toggle_animation_initialized_ = false;
  invalidate();
}

void SideMenu::PaintSystemToggle(ImDrawList* draw_list,
                                 ImU32 divider_color,
                                 ImU32 hover_fill_color,
                                 ImU32 accent_color,
                                 ImU32 accent_text_color) {
  if (!system_toggle_)
    return;

  const SDL_Rect toggle_bounds = MapLocalBoundsToWindow(system_toggle_bounds_);
  if (SDL_RectEmpty(&toggle_bounds))
    return;

  if (system_toggle_focused_ && activate_) {
    draw_list->AddRectFilled(ImVec2(toggle_bounds.x, toggle_bounds.y),
                             ImVec2(toggle_bounds.x + toggle_bounds.w,
                                    toggle_bounds.y + toggle_bounds.h),
                             ColorWithOpacity(accent_color, .14f), SCALED(4));
    const int accent_height = static_cast<int>(toggle_bounds.h * .56f);
    const int accent_top =
        toggle_bounds.y + (toggle_bounds.h - accent_height) / 2;
    draw_list->AddRectFilled(
        ImVec2(toggle_bounds.x, accent_top),
        ImVec2(toggle_bounds.x + SCALED(3), accent_top + accent_height),
        accent_color, SCALED(2), ImDrawFlags_RoundCornersRight);
  } else if (system_toggle_hover_opacity_ > 0.f) {
    draw_list->AddRectFilled(
        ImVec2(toggle_bounds.x, toggle_bounds.y),
        ImVec2(toggle_bounds.x + toggle_bounds.w,
               toggle_bounds.y + toggle_bounds.h),
        ColorWithOpacity(hover_fill_color, system_toggle_hover_opacity_),
        SCALED(4));
  }

  const bool is_gba = IsGBASystemSelected();
  if (!system_toggle_animation_initialized_) {
    system_toggle_animation_initialized_ = true;
    system_toggle_animation_from_gba_ = is_gba;
    system_toggle_last_gba_ = is_gba;
  } else if (system_toggle_last_gba_ != is_gba) {
    system_toggle_animation_from_gba_ = system_toggle_last_gba_;
    system_toggle_last_gba_ = is_gba;
    system_toggle_animation_timer_.Reset();
  }
  const float animation_progress =
      EaseOutCubic(system_toggle_animation_timer_.ElapsedInMilliseconds() /
                   static_cast<float>(kSystemToggleAnimationMs));
  const float selection_progress =
      Lerp(system_toggle_animation_from_gba_ ? 1.f : 0.f, is_gba ? 1.f : 0.f,
           animation_progress);

#if BUILDFLAG(IS_SWITCH)
  constexpr int kExpandedControlWidth = 220;
  constexpr int kExpandedControlHeight = 42;
  constexpr int kCollapsedControlWidth = 72;
  constexpr int kCollapsedControlHeight = 36;
#elif BUILDFLAG(IS_ANDROID)
  constexpr int kExpandedControlWidth = 220;
  constexpr int kExpandedControlHeight = 44;
  constexpr int kCollapsedControlWidth = 72;
  constexpr int kCollapsedControlHeight = 36;
#else
  const int kExpandedControlWidth = SCALED(122);
  const int kExpandedControlHeight = SCALED(28);
  const int kCollapsedControlWidth = SCALED(36);
  const int kCollapsedControlHeight = SCALED(24);
#endif

  const int control_width =
      std::min(expanded() ? kExpandedControlWidth : kCollapsedControlWidth,
               toggle_bounds.w - SCALED(kItemSpacing.x) * 2);
  const int control_height =
      std::min(expanded() ? kExpandedControlHeight : kCollapsedControlHeight,
               toggle_bounds.h - SCALED(6));
  const int control_left =
      toggle_bounds.x + (toggle_bounds.w - control_width) / 2;
  const int control_top =
      toggle_bounds.y + (toggle_bounds.h - control_height) / 2;
  const int first_segment_width = control_width / 2;
  const float control_radius = SCALED(6);

  draw_list->AddRectFilled(
      ImVec2(control_left, control_top),
      ImVec2(control_left + control_width, control_top + control_height),
      divider_color, control_radius);
  draw_list->AddRect(
      ImVec2(control_left, control_top),
      ImVec2(control_left + control_width, control_top + control_height),
      ColorWithOpacity(accent_color, .45f), control_radius);

  const float selection_left =
      Lerp(static_cast<float>(control_left),
           static_cast<float>(control_left + first_segment_width),
           selection_progress);
  draw_list->AddRectFilled(
      ImVec2(selection_left + SCALED(2), control_top + SCALED(2)),
      ImVec2(selection_left + first_segment_width - SCALED(2),
             control_top + control_height - SCALED(2)),
      ColorWithOpacity(accent_color, .2f), SCALED(4));
  draw_list->AddLine(
      ImVec2(control_left + first_segment_width, control_top + SCALED(5)),
      ImVec2(control_left + first_segment_width,
             control_top + control_height - SCALED(5)),
      ColorWithOpacity(accent_color, .28f));

  if (expanded()) {
    const std::array<std::string, 2> labels = {
        system_toggle_->nes_string_updater->GetLocalizedString(),
        system_toggle_->gba_string_updater->GetLocalizedString()};
    ScopedFont nes_font = GetPreferredFont(
        kPreferredFontSize, labels[0].c_str(), FontType::kSystemDefault);
    ScopedFont gba_font = GetPreferredFont(
        kPreferredFontSize, labels[1].c_str(), FontType::kSystemDefault);
    ScopedFont* fonts[] = {&nes_font, &gba_font};
    for (size_t i = 0; i < labels.size(); ++i) {
      const int segment_left = control_left + first_segment_width * i;
      const int segment_width =
          i == 0 ? first_segment_width : control_width - first_segment_width;
      const float font_size = GetFontRenderSize(*fonts[i]);
      const ImVec2 text_size = fonts[i]->GetFont()->CalcTextSizeA(
          font_size, FLT_MAX, 0.f, labels[i].c_str());
      const bool selected = is_gba == (i == 1);
      draw_list->AddText(
          fonts[i]->GetFont(), font_size,
          ImVec2(segment_left + (segment_width - text_size.x) / 2,
                 control_top + (control_height - text_size.y) / 2),
          selected ? accent_text_color : kTextColor, labels[i].c_str());
    }
  } else {
    const std::array<image_resources::ImageID, 2> icons = {
        system_toggle_->nes_icon, system_toggle_->gba_icon};
    const int icon_size = static_cast<int>(control_height * .54f);
    for (size_t i = 0; i < icons.size(); ++i) {
      const int segment_left = control_left + first_segment_width * i;
      const int segment_width =
          i == 0 ? first_segment_width : control_width - first_segment_width;
      const int icon_left = segment_left + (segment_width - icon_size) / 2;
      const int icon_top = control_top + (control_height - icon_size) / 2;
      const bool selected = is_gba == (i == 1);
      SDL_Texture* icon_texture = GetImage(window()->renderer(), icons[i]);
      draw_list->AddImage(reinterpret_cast<ImTextureID>(icon_texture),
                          ImVec2(icon_left, icon_top),
                          ImVec2(icon_left + icon_size, icon_top + icon_size),
                          ImVec2(0, 0), ImVec2(1, 1),
                          selected ? accent_text_color : kMutedIconColor);
    }
  }

  draw_list->AddLine(
      ImVec2(toggle_bounds.x, toggle_bounds.y + toggle_bounds.h - 1),
      ImVec2(toggle_bounds.x + toggle_bounds.w,
             toggle_bounds.y + toggle_bounds.h - 1),
      divider_color);
}

int SideMenu::GetSuggestedCollapsedWidth() {
#if BUILDFLAG(IS_SWITCH)
  return 96;
#else
  const int item_x = SCALED(kItemSpacing.x);
  const int item_height = SCALED(kItemHeight + kItemSpacing.y * 2);
  return (item_x + SCALED(kIconInset)) * 2 + item_height * kIconSizeScale;
#endif
}

int SideMenu::GetSuggestedExtendedWidth(int available_width) {
#if BUILDFLAG(IS_ANDROID)
  constexpr float kWidthRatio = .13f;
  constexpr int kMinWidth = 260;
  constexpr int kMaxWidth = 340;
#elif BUILDFLAG(IS_SWITCH)
  constexpr float kWidthRatio = .225f;
  constexpr int kMinWidth = 288;
  constexpr int kMaxWidth = 288;
#elif BUILDFLAG(IS_IOS)
  constexpr float kWidthRatio = .14f;
  constexpr int kMinWidth = 108;
  constexpr int kMaxWidth = 136;
#else
  constexpr float kWidthRatio = .125f;
  constexpr int kMinWidth = 104;
  constexpr int kMaxWidth = 148;
#endif
  return std::clamp(static_cast<int>(available_width * kWidthRatio), kMinWidth,
                    kMaxWidth);
}

void SideMenu::UpdateHoverState() {
#if BUILDFLAG(IS_MOBILE) || BUILDFLAG(IS_SWITCH)
  return;
#else
  int hovered_menu_index = -1;
  int hovered_button_index = -1;
  bool system_toggle_hovered = false;
  const ImVec2 mouse_position = ImGui::GetIO().MousePos;
  const int mouse_x = static_cast<int>(mouse_position.x);
  const int mouse_y = static_cast<int>(mouse_position.y);

  const SDL_Rect global_bounds = MapToWindow(bounds());
  const bool pointer_inside = Contains(global_bounds, mouse_x, mouse_y);
  if (pointer_inside) {
    FindItemIndexByMousePosition(mouse_x, mouse_y, hovered_menu_index);
    if (hovered_menu_index < 0) {
      const SDL_Rect toggle_bounds =
          MapLocalBoundsToWindow(system_toggle_bounds_);
      system_toggle_hovered =
          system_toggle_ && Contains(toggle_bounds, mouse_x, mouse_y);
      if (!system_toggle_hovered) {
        for (int i = 0; i < buttons_bounds_map_.size(); ++i) {
          const SDL_Rect button_bounds =
              MapLocalBoundsToWindow(buttons_bounds_map_[i]);
          if (Contains(button_bounds, mouse_x, mouse_y)) {
            hovered_button_index = i;
            break;
          }
        }
      }
    }
  }

  if (pointer_inside) {
    pointer_collapse_pending_ = false;
    if (!activate_ && !suppress_pointer_expansion_)
      pointer_expanded_ = true;
  } else {
    if (pointer_inside_) {
      suppress_pointer_expansion_ = false;
      if (pointer_expanded_) {
        pointer_collapse_pending_ = true;
        pointer_leave_timer_.Reset();
      }
    }
    if (pointer_collapse_pending_ &&
        pointer_leave_timer_.ElapsedInMilliseconds() >=
            kPointerCollapseDelayMs) {
      pointer_expanded_ = false;
      pointer_collapse_pending_ = false;
    }
  }
  pointer_inside_ = pointer_inside;
  hovered_menu_index_ = hovered_menu_index;
  hovered_button_index_ = hovered_button_index;
  system_toggle_hovered_ = system_toggle_hovered;
#endif
}

void SideMenu::UpdateHoverAnimations() {
  if (menu_hover_opacities_.size() != menu_items_.size())
    menu_hover_opacities_.resize(menu_items_.size(), 0.f);
  if (button_hover_opacities_.size() != button_items_.size())
    button_hover_opacities_.resize(button_items_.size(), 0.f);

  const int elapsed_ms =
      std::min(hover_frame_timer_.ElapsedInMillisecondsAndReset(), 50);
  const float opacity_step = elapsed_ms / static_cast<float>(kHoverAnimationMs);
  auto update_opacity = [opacity_step](float& opacity, bool hovered) {
    if (hovered)
      opacity = std::min(1.f, opacity + opacity_step);
    else
      opacity = std::max(0.f, opacity - opacity_step);
  };

  for (int i = 0; i < menu_hover_opacities_.size(); ++i) {
    update_opacity(menu_hover_opacities_[i], IsMenuVisible(i) &&
                                                 i == hovered_menu_index_ &&
                                                 i != current_index_);
  }
  for (int i = 0; i < button_hover_opacities_.size(); ++i)
    update_opacity(button_hover_opacities_[i], i == hovered_button_index_);
  update_opacity(system_toggle_hover_opacity_, system_toggle_hovered_);
}

bool SideMenu::HandleInputEvent(SDL_KeyboardEvent* k,
                                SDL_ControllerButtonEvent* c) {
  if (!activate_)
    return false;

  if (IsKeyboardOrControllerAxisMotionMatch(
          main_window_->runtime(), kiwi::nes::ControllerButton::kUp, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
    if (system_toggle_focused_)
      return true;
    const int next_index = FindVisibleMenu(current_index_ - 1, -1);
    if (next_index >= 0) {
      SetIndex(next_index);
    } else if (system_toggle_) {
      PlayEffect(audio_resources::AudioID::kSelect);
      system_toggle_focused_ = true;
    }
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          main_window_->runtime(), kiwi::nes::ControllerButton::kDown, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
    if (system_toggle_focused_) {
      const int first_index = FindVisibleMenu(0, 1);
      if (first_index >= 0)
        SetIndex(first_index);
      return true;
    }
    const int next_index = FindVisibleMenu(current_index_ + 1, 1);
    if (next_index >= 0) {
      SetIndex(next_index);
    }
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          main_window_->runtime(), kiwi::nes::ControllerButton::kRight, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
    if (activate_) {
      if (system_toggle_focused_)
        main_window_->ChangeFocus(MainWindow::MainFocus::kContents);
      else
        EnterIndex(current_index_);
    }
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          main_window_->runtime(), kiwi::nes::ControllerButton::kA, k) ||
      IsKeyboardOrControllerAxisMotionMatch(
          main_window_->runtime(), kiwi::nes::ControllerButton::kStart, k) ||
      IsGameSelectionConfirmButton(c)) {
    if (activate_) {
      if (system_toggle_focused_)
        TriggerSystemToggle();
      else
        TriggerCurrentItem();
    }
    return true;
  }

  for (const auto& button : button_items_) {
    const bool hotkey_matches = (k && k->keysym.sym == button.hotkey) ||
                                (c && c->button == button.controller_hotkey);
    if (hotkey_matches && button.callbacks.trigger_callback) {
      button.callbacks.trigger_callback.Run();
      return true;
    }
  }

  return false;
}

bool SideMenu::HandleMouseOrFingerDown(int x_in_window, int y_in_window) {
  const SDL_Rect toggle_bounds = MapLocalBoundsToWindow(system_toggle_bounds_);
  if (system_toggle_ && Contains(toggle_bounds, x_in_window, y_in_window)) {
    mouse_locked_ = true;
    system_toggle_press_pending_ = true;
    if (activate_)
      system_toggle_focused_ = true;
    return true;
  }

  if (!activate_) {
#if !BUILDFLAG(IS_MOBILE)
    if (pointer_expanded_) {
      mouse_locked_ = true;
      return true;
    }
#endif
    PlayEffect(audio_resources::AudioID::kSelect);
    main_window_->ChangeFocus(MainWindow::MainFocus::kSideMenu);
    return true;
  }

  mouse_locked_ = true;
  return true;
}

bool SideMenu::HandleMouseOrFingerUp(MouseButton button,
                                     int x_in_window,
                                     int y_in_window) {
  if (system_toggle_press_pending_) {
    const SDL_Rect toggle_bounds =
        MapLocalBoundsToWindow(system_toggle_bounds_);
    if (button == MouseButton::kLeftButton &&
        Contains(toggle_bounds, x_in_window, y_in_window)) {
      TriggerSystemToggle();
    }
    system_toggle_press_pending_ = false;
    mouse_locked_ = false;
    return true;
  }

  if ((activate_ || pointer_expanded_) && mouse_locked_) {
    if (button == MouseButton::kLeftButton) {
      int index = 0;
      // Finds menu item first.
      if (FindItemIndexByMousePosition(x_in_window, y_in_window, index)) {
        if (current_index_ != index)
          SetIndex(index);
        TriggerCurrentItem();
      } else {
        // If there's no menu item found, find button and trigger if any.
        FindButtonAndTrigger(x_in_window, y_in_window);
      }
    }
  } else if (button == MouseButton::kRightButton) {
    PlayEffect(audio_resources::AudioID::kSelect);
    main_window_->ChangeFocus(MainWindow::MainFocus::kSideMenu);
  }
  mouse_locked_ = false;
  return true;
}

void SideMenu::Layout() {
  if (bounds_valid_)
    return;

  items_bounds_map_.resize(menu_items_.size());
  buttons_bounds_map_.resize(button_items_.size());
  const SDL_Rect local_bounds = GetLocalBounds();
  const int item_x = local_bounds.x + SCALED(kItemSpacing.x);
  const int item_width = local_bounds.w - SCALED(kItemSpacing.x);
  const int button_height = SCALED(kButtonHeight + kItemSpacing.y * 2);
  const int item_height = SCALED(kItemHeight + kItemSpacing.y * 2);

  int y = local_bounds.y + SCALED(kItemMarginBottom);
  if (system_toggle_) {
    system_toggle_bounds_ = SDL_Rect{item_x, y, item_width, button_height};
    y += button_height;
  } else {
    system_toggle_bounds_ = SDL_Rect{};
  }
  for (size_t i = 0; i < button_items_.size(); ++i) {
    buttons_bounds_map_[i] = SDL_Rect{item_x, y, item_width, button_height};
    y += button_height;
  }

  y = local_bounds.y + local_bounds.h - SCALED(kItemMarginBottom) - item_height;
  for (int i = static_cast<int>(menu_items_.size()) - 1; i >= 0; --i) {
    if (!IsMenuVisible(i)) {
      items_bounds_map_[i] = SDL_Rect{};
      continue;
    }
    items_bounds_map_[i] = SDL_Rect{item_x, y, item_width, item_height};
    y -= item_height;
  }

  bounds_valid_ = true;
}

void SideMenu::SetIndex(int index) {
  if (!IsMenuVisible(index))
    return;
  PlayEffect(audio_resources::AudioID::kSelect);
  system_toggle_focused_ = false;
  current_index_ = index;
  timer_.Reset();
}

SDL_Rect SideMenu::MapLocalBoundsToWindow(const SDL_Rect& local_bounds) {
  const SDL_Rect widget_bounds = bounds();
  return MapToWindow(SDL_Rect{widget_bounds.x + local_bounds.x,
                              widget_bounds.y + local_bounds.y, local_bounds.w,
                              local_bounds.h});
}

void SideMenu::EnterIndex(int index) {
  if (!IsMenuVisible(index))
    return;
  SetIndex(triggered_index_);
  menu_items_[index].callbacks.enter_callback.Run();
  if (activate_)
    main_window_->ChangeFocus(MainWindow::MainFocus::kContents);
}

void SideMenu::TriggerCurrentItem() {
  if (!IsMenuVisible(current_index_))
    return;
  triggered_index_ = current_index_;
  menu_items_[current_index_].callbacks.trigger_callback.Run(triggered_index_);
  menu_items_[current_index_].callbacks.enter_callback.Run();
}

void SideMenu::TriggerSystemToggle(int direction) {
  if (!system_toggle_ || !system_toggle_->callbacks.cycle_callback)
    return;
  PlayEffect(audio_resources::AudioID::kSelect);
  system_toggle_->callbacks.cycle_callback.Run(direction);
}

bool SideMenu::IsGBASystemSelected() const {
  return system_toggle_ && system_toggle_->callbacks.is_gba_selected &&
         system_toggle_->callbacks.is_gba_selected.Run();
}

bool SideMenu::IsMenuVisible(int index) const {
  if (index < 0 || index >= static_cast<int>(menu_items_.size()))
    return false;
  const MenuCallbacks& callbacks = menu_items_[index].callbacks;
  return !callbacks.is_visible || callbacks.is_visible.Run();
}

int SideMenu::FindVisibleMenu(int start, int step) const {
  for (int index = start;
       index >= 0 && index < static_cast<int>(menu_items_.size());
       index += step) {
    if (IsMenuVisible(index))
      return index;
  }
  return -1;
}

bool SideMenu::FindItemIndexByMousePosition(int x_in_window,
                                            int y_in_window,
                                            int& index_out) {
  // There's no intersection between each item's bounds, so we can find the
  // target item easily
  for (int i = 0; i < items_bounds_map_.size(); ++i) {
    if (!IsMenuVisible(i))
      continue;
    SDL_Rect bounds_to_window = MapLocalBoundsToWindow(items_bounds_map_[i]);
    if (Contains(bounds_to_window, x_in_window, y_in_window)) {
      index_out = i;
      return true;
    }
  }
  return false;
}

void SideMenu::FindButtonAndTrigger(int x_in_window, int y_in_window) {
  for (int i = 0; i < buttons_bounds_map_.size(); ++i) {
    SDL_Rect bounds_to_window = MapLocalBoundsToWindow(buttons_bounds_map_[i]);
    if (Contains(bounds_to_window, x_in_window, y_in_window)) {
      const ButtonCallbacks& callbacks = button_items_[i].callbacks;
      if (callbacks.trigger_callback)
        callbacks.trigger_callback.Run();
      break;
    }
  }
}

bool SideMenu::OnKeyPressed(SDL_KeyboardEvent* event) {
  return HandleInputEvent(event, nullptr);
}

bool SideMenu::OnMousePressed(SDL_MouseButtonEvent* event) {
  return HandleMouseOrFingerDown(event->x, event->y);
}

bool SideMenu::OnMouseReleased(SDL_MouseButtonEvent* event) {
  MouseButton button = (event->button == SDL_BUTTON_LEFT
                            ? MouseButton::kLeftButton
                            : ((event->button == SDL_BUTTON_RIGHT
                                    ? MouseButton::kRightButton
                                    : MouseButton::kUnknownButton)));
  return HandleMouseOrFingerUp(button, event->x, event->y);
}

bool SideMenu::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  return HandleInputEvent(nullptr, event);
}

bool SideMenu::OnControllerAxisMotionEvent(SDL_ControllerAxisEvent* event) {
#if BUILDFLAG(IS_SWITCH)
  const bool is_left_trigger = event->axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT;
  const bool is_right_trigger = event->axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
  if (system_toggle_ && (is_left_trigger || is_right_trigger)) {
    bool& trigger_pressed = is_left_trigger
                                ? system_toggle_left_trigger_pressed_
                                : system_toggle_right_trigger_pressed_;
    if (event->value >= kSystemToggleTriggerThreshold) {
      if (!trigger_pressed) {
        trigger_pressed = true;
        TriggerSystemToggle(is_left_trigger ? -1 : 1);
      }
    } else if (event->value <= kSystemToggleTriggerReleaseThreshold) {
      trigger_pressed = false;
    }
    return true;
  }
#endif
  return HandleInputEvent(nullptr, nullptr);
}

void SideMenu::OnWindowPreRender() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
}

void SideMenu::OnWindowPostRender() {
  ImGui::PopStyleVar(2);
}

SideMenu::MenuItem::MenuItem(
    std::unique_ptr<LocalizedStringUpdater> string_updater,
    image_resources::ImageID icon,
    image_resources::ImageID highlight_icon,
    MenuCallbacks callbacks) {
  this->string_updater = std::move(string_updater);
  this->icon = icon;
  this->highlight_icon = highlight_icon;
  this->callbacks = callbacks;
}

SideMenu::MenuItem::MenuItem(MenuItem&& rhs) {
  *this = std::move(rhs);
}

SideMenu::MenuItem& SideMenu::MenuItem::operator=(SideMenu::MenuItem&& rhs) {
  string_updater = std::move(rhs.string_updater);
  icon = rhs.icon;
  rhs.icon = image_resources::ImageID::kLast;
  highlight_icon = rhs.highlight_icon;
  rhs.highlight_icon = image_resources::ImageID::kLast;
  callbacks = rhs.callbacks;
  rhs.callbacks = SideMenu::MenuCallbacks{};
  return *this;
}

SideMenu::MenuItem::~MenuItem() {
  if (icon > image_resources::ImageID::kLast)
    ImageUnregister(icon);
  if (highlight_icon > image_resources::ImageID::kLast)
    ImageUnregister(highlight_icon);
}

SideMenu::ButtonItem::ButtonItem(
    std::unique_ptr<LocalizedStringUpdater> string_updater,
    image_resources::ImageID icon,
    ButtonCallbacks callbacks,
    SDL_KeyCode hotkey,
    SDL_GameControllerButton controller_hotkey) {
  this->string_updater = std::move(string_updater);
  this->icon = icon;
  this->callbacks = callbacks;
  this->hotkey = hotkey;
  this->controller_hotkey = controller_hotkey;
}

SideMenu::ButtonItem::~ButtonItem() {
  if (icon > image_resources::ImageID::kLast)
    ImageUnregister(icon);
}

SideMenu::ButtonItem::ButtonItem(ButtonItem&& rhs) {
  *this = std::move(rhs);
}

SideMenu::ButtonItem& SideMenu::ButtonItem::operator=(
    SideMenu::ButtonItem&& rhs) {
  string_updater = std::move(rhs.string_updater);
  icon = rhs.icon;
  hotkey = rhs.hotkey;
  controller_hotkey = rhs.controller_hotkey;
  rhs.hotkey = SDLK_UNKNOWN;
  rhs.controller_hotkey = SDL_CONTROLLER_BUTTON_INVALID;
  rhs.icon = image_resources::ImageID::kLast;
  callbacks = rhs.callbacks;
  rhs.callbacks = ButtonCallbacks{};
  return *this;
}
