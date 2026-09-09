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

#include <algorithm>

#include "models/nes_runtime.h"
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
constexpr int kPointerCollapseDelayMs = 120;
constexpr int kIconSpacing = 4;
constexpr float kIconSizeScale = .7f;
constexpr ImU32 kBackgroundColor = IM_COL32(244, 252, 227, 255);
constexpr ImU32 kBorderColor = IM_COL32(116, 184, 22, 255);
constexpr ImU32 kDividerColor = IM_COL32(216, 245, 162, 255);
constexpr ImU32 kTextColor = IM_COL32(52, 58, 64, 255);
constexpr ImU32 kMutedIconColor = IM_COL32(73, 80, 87, 255);
constexpr ImU32 kSelectedBackgroundColor = IM_COL32(216, 245, 162, 255);
constexpr ImU32 kHoverFillColor = IM_COL32(233, 250, 200, 255);
constexpr ImU32 kAccentColor = IM_COL32(116, 184, 22, 255);
constexpr ImU32 kAccentTextColor = IM_COL32(92, 148, 13, 255);
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

}  // namespace

SideMenu::SideMenu(MainWindow* main_window, NESRuntimeID runtime_id)
    : Widget(main_window), main_window_(main_window) {
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
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

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const SDL_Rect kBoundsToWindow = MapToWindow(bounds());
  draw_list->AddRectFilled(ImVec2(kBoundsToWindow.x, kBoundsToWindow.y),
                           ImVec2(kBoundsToWindow.x + kBoundsToWindow.w,
                                  kBoundsToWindow.y + kBoundsToWindow.h),
                           kBackgroundColor);
  draw_list->AddLine(
      ImVec2(kBoundsToWindow.x + kBoundsToWindow.w - 1, kBoundsToWindow.y),
      ImVec2(kBoundsToWindow.x + kBoundsToWindow.w - 1,
             kBoundsToWindow.y + kBoundsToWindow.h),
      kBorderColor);

  for (int i = 0; i < button_items_.size(); ++i) {
    const SDL_Rect button_bounds = MapToWindow(buttons_bounds_map_[i]);
    const int kIconSize = kIconSizeScale * button_bounds.h;
    const int kIconTop = button_bounds.y + (button_bounds.h - kIconSize) / 2;
    const int kIconLeft = button_bounds.x + SCALED(kIconSpacing);
    const float hover_opacity = button_hover_opacities_[i];

    if (hover_opacity > 0.f) {
      draw_list->AddRectFilled(ImVec2(button_bounds.x, button_bounds.y),
                               ImVec2(button_bounds.x + button_bounds.w,
                                      button_bounds.y + button_bounds.h),
                               ColorWithOpacity(kHoverFillColor, hover_opacity),
                               SCALED(4));
    }

    draw_list->AddLine(
        ImVec2(button_bounds.x, button_bounds.y + button_bounds.h - 1),
        ImVec2(button_bounds.x + button_bounds.w,
               button_bounds.y + button_bounds.h - 1),
        kDividerColor);

    if (expanded()) {
      std::string contents =
          button_items_[i].string_updater->GetLocalizedString();
      ScopedFont font = GetPreferredFont(kPreferredFontSize, contents.c_str());
      const ImVec2 text_size = ImGui::CalcTextSize(contents.c_str());
      int text_top = button_bounds.y + (button_bounds.h - text_size.y) / 2;
      const float text_left = kIconLeft + kIconSize + SCALED(kIconSpacing);
      const ImVec4 text_clip(
          text_left, button_bounds.y,
          button_bounds.x + button_bounds.w - SCALED(kIconSpacing),
          button_bounds.y + button_bounds.h);
      draw_list->AddText(
          font.GetFont(), font.GetFontSize(), ImVec2(text_left, text_top),
          BlendColor(kTextColor, kAccentTextColor, hover_opacity),
          contents.c_str(), nullptr, 0.f, &text_clip);
    }

    SDL_Texture* icon_texture =
        GetImage(window()->renderer(), button_items_[i].icon);
    draw_list->AddImage(
        reinterpret_cast<ImTextureID>(icon_texture),
        ImVec2(kIconLeft, kIconTop),
        ImVec2(kIconLeft + kIconSize, kIconTop + kIconSize), ImVec2(0, 0),
        ImVec2(1, 1),
        BlendColor(kMutedIconColor, kAccentTextColor, hover_opacity));
  }

  for (int i = menu_items_.size() - 1; i >= 0; --i) {
    const SDL_Rect global_item_rect = MapToWindow(items_bounds_map_[i]);
    const int kIconSize = kIconSizeScale * global_item_rect.h;
    const int kIconLeft = global_item_rect.x + SCALED(kIconSpacing);
    const int kIconTop =
        global_item_rect.y + (global_item_rect.h - kIconSize) / 2;

    std::string menu_content =
        menu_items_[i].string_updater->GetLocalizedString();
    ScopedFont font =
        GetPreferredFont(kPreferredFontSize, menu_content.c_str());
    const bool selected = i == current_index_;
    const float hover_opacity = selected ? 0.f : menu_hover_opacities_[i];
    if (hover_opacity > 0.f) {
      draw_list->AddRectFilled(ImVec2(global_item_rect.x, global_item_rect.y),
                               ImVec2(global_item_rect.x + global_item_rect.w,
                                      global_item_rect.y + global_item_rect.h),
                               ColorWithOpacity(kHoverFillColor, hover_opacity),
                               SCALED(4));
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

      SDL_Rect global_selection_rect =
          MapToWindow(Lerp(selection_current_rect_in_global_,
                           selection_target_rect_in_global_, percentage));
      SDL_Rect global_target_selection_rect =
          MapToWindow(selection_target_rect_in_global_);

      draw_list->AddRectFilled(
          ImVec2(global_selection_rect.x, global_selection_rect.y),
          ImVec2(global_selection_rect.x + global_selection_rect.w,
                 global_selection_rect.y + global_selection_rect.h),
          kSelectedBackgroundColor);
      const int accent_height =
          static_cast<int>(global_selection_rect.h * .56f);
      const int accent_top = global_selection_rect.y +
                             (global_selection_rect.h - accent_height) / 2;
      draw_list->AddRectFilled(ImVec2(global_selection_rect.x, accent_top),
                               ImVec2(global_selection_rect.x + SCALED(3),
                                      accent_top + accent_height),
                               kAccentColor, SCALED(2),
                               ImDrawFlags_RoundCornersRight);

      if (expanded()) {
        // Calculates text area
        const ImVec2 text_size = ImGui::CalcTextSize(menu_content.c_str());
        const int text_top = global_target_selection_rect.y +
                             (global_target_selection_rect.h - text_size.y) / 2;
        const float text_left = kIconLeft + kIconSize + SCALED(kIconSpacing);
        const ImVec4 text_clip(
            text_left, global_target_selection_rect.y,
            global_target_selection_rect.x + global_target_selection_rect.w -
                SCALED(kIconSpacing),
            global_target_selection_rect.y + global_target_selection_rect.h);
        draw_list->AddText(font.GetFont(), font.GetFontSize(),
                           ImVec2(text_left, text_top), kAccentTextColor,
                           menu_content.c_str(), nullptr, 0.f, &text_clip);
      }
    } else if (expanded()) {
      // Calculates text area
      const ImVec2 text_size = ImGui::CalcTextSize(menu_content.c_str());
      const int text_top =
          global_item_rect.y + (global_item_rect.h - text_size.y) / 2;
      const float text_left = kIconLeft + kIconSize + SCALED(kIconSpacing);
      const ImVec4 text_clip(
          text_left, global_item_rect.y,
          global_item_rect.x + global_item_rect.w - SCALED(kIconSpacing),
          global_item_rect.y + global_item_rect.h);
      draw_list->AddText(
          font.GetFont(), font.GetFontSize(), ImVec2(text_left, text_top),
          BlendColor(kTextColor, kAccentTextColor, hover_opacity),
          menu_content.c_str(), nullptr, 0.f, &text_clip);
    }

    // Use the monochrome source for every state and tint it consistently.
    SDL_Texture* icon_texture =
        GetImage(window()->renderer(), menu_items_[i].icon);
    draw_list->AddImage(reinterpret_cast<ImTextureID>(icon_texture),
                        ImVec2(kIconLeft, kIconTop),
                        ImVec2(kIconLeft + kIconSize, kIconTop + kIconSize),
                        ImVec2(0, 0), ImVec2(1, 1),
                        selected ? kAccentTextColor
                                 : BlendColor(kMutedIconColor, kAccentTextColor,
                                              hover_opacity));
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
                         SDL_KeyCode hotkey) {
  button_items_.push_back(ButtonItem(std::move(string_updater), icon,
                                     std::move(callbacks), hotkey));
}

int SideMenu::GetSuggestedCollapsedWidth() {
  const int item_x = SCALED(kItemSpacing.x);
  const int item_height = SCALED(kItemHeight + kItemSpacing.y * 2);
  return (item_x + SCALED(kIconSpacing)) * 2 + item_height * kIconSizeScale;
}

int SideMenu::GetSuggestedExtendedWidth(int available_width) {
#if KIWI_ANDROID
  constexpr float kWidthRatio = .13f;
  constexpr int kMinWidth = 260;
  constexpr int kMaxWidth = 340;
#elif KIWI_IOS
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
#if KIWI_MOBILE
  return;
#else
  int hovered_menu_index = -1;
  int hovered_button_index = -1;
  const ImVec2 mouse_position = ImGui::GetIO().MousePos;
  const int mouse_x = static_cast<int>(mouse_position.x);
  const int mouse_y = static_cast<int>(mouse_position.y);

  const SDL_Rect global_bounds = MapToWindow(bounds());
  const bool pointer_inside = Contains(global_bounds, mouse_x, mouse_y);
  if (pointer_inside) {
    FindItemIndexByMousePosition(mouse_x, mouse_y, hovered_menu_index);
    if (hovered_menu_index < 0) {
      for (int i = 0; i < buttons_bounds_map_.size(); ++i) {
        const SDL_Rect button_bounds = MapToWindow(buttons_bounds_map_[i]);
        if (Contains(button_bounds, mouse_x, mouse_y)) {
          hovered_button_index = i;
          break;
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
    update_opacity(menu_hover_opacities_[i],
                   i == hovered_menu_index_ && i != current_index_);
  }
  for (int i = 0; i < button_hover_opacities_.size(); ++i)
    update_opacity(button_hover_opacities_[i], i == hovered_button_index_);
}

bool SideMenu::HandleInputEvent(SDL_KeyboardEvent* k,
                                SDL_ControllerButtonEvent* c) {
  if (!activate_)
    return false;

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kUp, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
    int next_index = (current_index_ <= 0 ? 0 : current_index_ - 1);
    if (next_index != current_index_) {
      SetIndex(next_index);
    }
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kDown, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
    int next_index =
        (current_index_ >= menu_items_.size() - 1 ? current_index_
                                                  : current_index_ + 1);
    if (next_index != current_index_) {
      SetIndex(next_index);
    }
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kRight, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
    if (activate_) {
      EnterIndex(current_index_);
    }
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kA, k) ||
      IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kStart, k) ||
      (c && c->button == SDL_CONTROLLER_BUTTON_A) ||
      (c && c->button == SDL_CONTROLLER_BUTTON_START)) {
    if (activate_) {
      TriggerCurrentItem();
    }
    return true;
  }

  for (const auto& button : button_items_) {
    if (k && k->keysym.sym == button.hotkey &&
        button.callbacks.trigger_callback) {
      button.callbacks.trigger_callback.Run();
    }
  }

  return false;
}

bool SideMenu::HandleMouseOrFingerDown() {
  if (!activate_) {
#if !KIWI_MOBILE
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
  for (size_t i = 0; i < button_items_.size(); ++i) {
    buttons_bounds_map_[i] = SDL_Rect{item_x, y, item_width, button_height};
    y += button_height;
  }

  y = local_bounds.y + local_bounds.h - SCALED(kItemMarginBottom) - item_height;
  for (int i = static_cast<int>(menu_items_.size()) - 1; i >= 0; --i) {
    items_bounds_map_[i] = SDL_Rect{item_x, y, item_width, item_height};
    y -= item_height;
  }

  bounds_valid_ = true;
}

void SideMenu::SetIndex(int index) {
  PlayEffect(audio_resources::AudioID::kSelect);
  current_index_ = index;
  timer_.Reset();
}

void SideMenu::EnterIndex(int index) {
  SetIndex(triggered_index_);
  menu_items_[index].callbacks.enter_callback.Run();
  if (activate_)
    main_window_->ChangeFocus(MainWindow::MainFocus::kContents);
}

void SideMenu::TriggerCurrentItem() {
  triggered_index_ = current_index_;
  menu_items_[current_index_].callbacks.trigger_callback.Run(triggered_index_);
  menu_items_[current_index_].callbacks.enter_callback.Run();
}

bool SideMenu::FindItemIndexByMousePosition(int x_in_window,
                                            int y_in_window,
                                            int& index_out) {
  // There's no intersection between each item's bounds, so we can find the
  // target item easily
  for (int i = 0; i < items_bounds_map_.size(); ++i) {
    SDL_Rect bounds_to_window = MapToWindow(items_bounds_map_[i]);
    if (Contains(bounds_to_window, x_in_window, y_in_window)) {
      index_out = i;
      return true;
    }
  }
  return false;
}

void SideMenu::FindButtonAndTrigger(int x_in_window, int y_in_window) {
  for (int i = 0; i < buttons_bounds_map_.size(); ++i) {
    SDL_Rect bounds_to_window = MapToWindow(buttons_bounds_map_[i]);
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
  return HandleMouseOrFingerDown();
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
    SDL_KeyCode hotkey) {
  this->string_updater = std::move(string_updater);
  this->icon = icon;
  this->callbacks = callbacks;
  this->hotkey = hotkey;
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
  rhs.hotkey = SDLK_UNKNOWN;
  rhs.icon = image_resources::ImageID::kLast;
  callbacks = rhs.callbacks;
  rhs.callbacks = ButtonCallbacks{};
  return *this;
}
