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

#ifndef UI_WIDGETS_ABOUT_WIDGET_H_
#define UI_WIDGETS_ABOUT_WIDGET_H_

#include <array>
#include <string>

#include "models/nes_runtime.h"
#include "ui/widgets/widget.h"
#include "utility/fonts.h"

class StackWidget;
class MainWindow;
class AboutWidget : public Widget {
 public:
  explicit AboutWidget(MainWindow* main_window,
                       StackWidget* parent,
                       NESRuntimeID runtime_id);
  ~AboutWidget() override;

 private:
  void Close();

 protected:
  // Widget:
  void Paint() override;
  void OnWindowResized() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event) override;
  bool OnControllerAxisMotionEvent(SDL_ControllerAxisEvent* event) override;
  void OnWindowPreRender() override;
  void OnWindowPostRender() override;
  bool OnMouseMove(SDL_MouseMotionEvent* event) override;
  bool OnMousePressed(SDL_MouseButtonEvent* event) override;
  bool OnMouseReleased(SDL_MouseButtonEvent* event) override;
#if KIWI_MOBILE
  bool OnTouchFingerDown(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerUp(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerMove(SDL_TouchFingerEvent* event) override;
#endif

 private:
  enum class InputPage {
    kKeyboard,
    kGamepad,
  };

#if KIWI_MOBILE
  enum class MobileSection {
    kControls,
    kGameSelection,
    kAbout,
  };
#endif

  enum class LayoutMode {
    kTwoColumn,
    kSingleColumn,
  };

  enum class HitTarget {
    kNone,
    kKeyboardTab,
    kGamepadTab,
    kRepository,
    kBack,
#if KIWI_MOBILE
    kControlsSection,
    kGameSelectionSection,
    kAboutSection,
#endif
  };

  struct FrameText {
    std::string title;
    std::string subtitle;
    std::string version;
    std::string short_version;
    std::string controller;
    std::string keyboard;
    std::string gamepad;
    std::string input;
    std::string direction;
    std::string button_a;
    std::string button_b;
    std::string select;
    std::string start;
    std::string menu;
    std::string player_one;
    std::string player_two;
    std::string xbox;
    std::string xbox_direction;
    std::string xbox_menu;
    std::string game_selection;
    std::array<std::string, 3> game_selection_description;
    std::string about;
    std::string repository_label;
    std::string author;
  };

  struct FrameLayout {
    LayoutMode mode = LayoutMode::kSingleColumn;
    PreferredFontSize title_font = PreferredFontSize::k1x;
    PreferredFontSize body_font = PreferredFontSize::k1x;
    float font_height = 0.f;
    float scale = 1.f;
    float padding = 0.f;

    SDL_Rect safe_area = {};
    SDL_Rect page = {};
    SDL_Rect header = {};
    SDL_Rect logo = {};
    SDL_Rect title = {};
    SDL_Rect subtitle = {};
    SDL_Rect version = {};
    SDL_Rect back_button = {};

#if KIWI_MOBILE
    SDL_Rect mobile_section_bar = {};
    std::array<SDL_Rect, 3> mobile_section_tabs = {};
#endif

    SDL_Rect controls_panel = {};
    SDL_Rect controls_title = {};
    SDL_Rect keyboard_tab = {};
    SDL_Rect gamepad_tab = {};
    SDL_Rect controls_body = {};

    SDL_Rect game_selection_panel = {};
    SDL_Rect game_selection_title = {};
    SDL_Rect game_selection_body = {};

    SDL_Rect about_panel = {};
    SDL_Rect about_title = {};
    SDL_Rect about_body = {};
    SDL_Rect repository = {};
    SDL_Rect metadata = {};
  };

  FrameText BuildFrameText() const;
  FrameLayout CalculateFrameLayout(const FrameText& text) const;
  void DrawFrame(const FrameText& text, const FrameLayout& layout);
  void DrawBackground(const FrameLayout& layout);
  void DrawHeader(const FrameText& text, const FrameLayout& layout);
#if KIWI_MOBILE
  void DrawMobileSectionTabs(const FrameText& text, const FrameLayout& layout);
  void SelectMobileSection(MobileSection section);
  void MoveMobileSection(int delta);
#endif
  void DrawControls(const FrameText& text, const FrameLayout& layout);
  void DrawKeyboard(const FrameText& text, const FrameLayout& layout);
  void DrawGamepad(const FrameText& text, const FrameLayout& layout);
  void DrawGameSelection(const FrameText& text, const FrameLayout& layout);
  void DrawAbout(const FrameText& text, const FrameLayout& layout);

  bool HandleInputEvent(SDL_KeyboardEvent* keyboard,
                        SDL_ControllerButtonEvent* controller);
  HitTarget HitTest(const FrameLayout& layout, float x, float y) const;
  void ActivateHitTarget(HitTarget target);
  void SelectInputPage(InputPage page);
  void OpenRepository();

 private:
  NESRuntime::Data* runtime_data_ = nullptr;
  StackWidget* parent_ = nullptr;
  MainWindow* main_window_ = nullptr;
  InputPage input_page_ = InputPage::kKeyboard;
#if KIWI_MOBILE
  MobileSection mobile_section_ = MobileSection::kControls;
#endif
  FrameLayout last_layout_;
  bool has_layout_ = false;
  HitTarget hovered_target_ = HitTarget::kNone;
  HitTarget pressed_target_ = HitTarget::kNone;
  bool mouse_pressed_ = false;
#if KIWI_MOBILE
  bool touch_active_ = false;
  SDL_FingerID active_touch_id_ = 0;
  ImVec2 touch_down_position_;
  bool touch_moved_ = false;
#endif
};

#endif  // UI_WIDGETS_ABOUT_WIDGET_H_
