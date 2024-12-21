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

#ifndef UI_WIDGETS_SIDE_MENU_H_
#define UI_WIDGETS_SIDE_MENU_H_

#include <kiwi_nes.h>

#include "models/nes_runtime.h"
#include "ui/widgets/widget.h"
#include "utility/images.h"
#include "utility/timer.h"

class MainWindow;
class LocalizedStringUpdater;
class SideMenu : public Widget {
 public:
  struct MenuCallbacks {
    // Triggers when joystick key 'A' pressed
    kiwi::base::RepeatingCallback<void(int)> trigger_callback =
        kiwi::base::DoNothing();

    // Triggers when joystick 'right' pressed
    kiwi::base::RepeatingClosure enter_callback = kiwi::base::DoNothing();
  };

  explicit SideMenu(MainWindow* main_window, NESRuntimeID runtime_id);
  ~SideMenu() override;

 public:
  void AddMenu(std::unique_ptr<LocalizedStringUpdater> string_updater,
               image_resources::ImageID icon,
               image_resources::ImageID highlight_icon,
               MenuCallbacks callbacks);

  void AddMenu(std::unique_ptr<LocalizedStringUpdater> string_updater,
               const kiwi::nes::Bytes& icon_data,
               const kiwi::nes::Bytes& highlight_icon_data,
               MenuCallbacks callbacks);

  void set_activate(bool activate) { activate_ = activate; }
  bool activate() { return activate_; }
  void invalidate() { bounds_valid_ = false; }
  void set_auto_trigger_first_item(bool trigger) {
    auto_trigger_first_menu_ = trigger;
  }

  int GetSuggestedCollapsedWidth();
  int GetMinExtendedWidth();
  void Layout();
  void SetIndex(int index);

 private:
  void Paint() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnMousePressed(SDL_MouseButtonEvent* event) override;
  bool OnMouseReleased(SDL_MouseButtonEvent* event) override;
  bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event) override;
  bool OnControllerAxisMotionEvent(SDL_ControllerAxisEvent* event) override;
#if KIWI_MOBILE
  bool OnTouchFingerDown(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerUp(SDL_TouchFingerEvent* event) override;
#endif
  void OnWindowPreRender() override;
  void OnWindowPostRender() override;

  bool HandleInputEvent(SDL_KeyboardEvent* k, SDL_ControllerButtonEvent* c);
  bool HandleMouseOrFingerDown();
  enum class MouseButton {
    kLeftButton,
    kRightButton,
    kUnknownButton,
  };
  bool HandleMouseOrFingerUp(MouseButton button,
                             int x_in_window,
                             int y_in_window);
  void EnterIndex(int index);
  void TriggerCurrentItem();
  bool FindItemIndexByMousePosition(int x_in_window,
                                    int y_in_window,
                                    int& index_out);

 private:
  MainWindow* main_window_ = nullptr;
  NESRuntime::Data* runtime_data_ = nullptr;
  bool auto_trigger_first_menu_ = true;
  struct MenuItem {
    MenuItem(std::unique_ptr<LocalizedStringUpdater> string_updater,
             image_resources::ImageID icon,
             image_resources::ImageID highlight_icon,
             MenuCallbacks callbacks);
    ~MenuItem();

    MenuItem(MenuItem&& rhs);
    MenuItem& operator=(MenuItem&& rhs);

    std::unique_ptr<LocalizedStringUpdater> string_updater;
    image_resources::ImageID icon;
    image_resources::ImageID highlight_icon;
    MenuCallbacks callbacks;
  };
  std::vector<MenuItem> menu_items_;
  // Order: from last added menu to first added menu
  std::vector<SDL_Rect> items_bounds_map_;
  bool bounds_valid_ = false;

  int current_index_ = 0;
  int triggered_index_ = 0;
  bool activate_ = false;
  Timer timer_;
  SDL_Rect selection_current_rect_in_global_;
  SDL_Rect selection_target_rect_in_global_;

  bool mouse_locked_ = false;
};

#endif  // UI_WIDGETS_SIDE_MENU_H_