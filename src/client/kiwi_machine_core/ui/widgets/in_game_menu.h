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

#ifndef UI_WIDGETS_IN_GAME_MENU_H_
#define UI_WIDGETS_IN_GAME_MENU_H_

#include <kiwi_nes.h>

#if KIWI_MOBILE
#include <unordered_map>
#include <vector>
#endif

#include "build/kiwi_defines.h"
#include "models/nes_runtime.h"
#include "ui/widgets/loading_widget.h"
#include "ui/widgets/widget.h"
#include "utility/fonts.h"

class MainWindow;
class InGameMenu : public Widget {
 public:
  enum {
    kMaxScaling = 4,
  };

  enum class MenuItem {
    kContinue,
    kLoadAutoSave,
    kLoadState,
    kSaveState,
    kOptions,
    kResetGame,
    kToGameSelection,

    kMax,
  };

  enum class SettingsItem {
    kVolume,
    kWindowSize,
    kJoyP1,
    kJoyP2,
    kLanguage,

    kMax,
  };

  using MenuItemCallback = kiwi::base::RepeatingCallback<void(MenuItem, int)>;
  struct SettingsItemContext {
    enum class SettingsItemType {
      kPrompt,
      kComplex,
    };

    SettingsItemType type;
    union {
      bool go_left;
      struct {
        SDL_Rect items_bounds;
        SDL_Point trigger_position;
      } bounds;
    };
  };
  using SettingsItemCallback =
      kiwi::base::RepeatingCallback<void(SettingsItem, SettingsItemContext)>;

  explicit InGameMenu(MainWindow* main_window,
                      NESRuntimeID runtime_id,
                      MenuItemCallback menu_callback,
                      SettingsItemCallback settings_callback);
  ~InGameMenu() override;
  void Close();
  void Show();
  void RequestCurrentThumbnail();
  void RequestCurrentSaveStatesCount();
  void OnGotState(const NESRuntime::Data::StateResult& result);
  void HideMenu(int index);

 private:
  bool HandleInputEvent(SDL_KeyboardEvent* k, SDL_ControllerButtonEvent* c);
  void HandleMenuItemForCurrentSelection();
  void HandleSettingsItemForCurrentSelection(SettingsItemContext context);
  void HandleSettingsItemForCurrentSelectionInternal(
      SettingsItemContext context);
  void MoveSelection(bool up);
  void SetFirstSelection();

 protected:
  // Widget:
  void Paint() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event) override;
  bool OnControllerAxisMotionEvent(SDL_ControllerAxisEvent* event) override;
  bool OnMousePressed(SDL_MouseButtonEvent* event) override;

#if KIWI_MOBILE
  bool OnTouchFingerDown(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerMove(SDL_TouchFingerEvent* event) override;
#endif

 private:
  // Layout flow
  struct LayoutImmediateContext {
    ImVec2 window_pos;
    ImVec2 window_size;
    std::vector<const char*> menu_items;
    PreferredFontSize font_size;

    // Following variables are for faster calculation:
    const int title_menu_height = 0;
    const int window_center_x = 0;

    // Menu items
    int menu_tops[static_cast<int>(
        MenuItem::kMax)];  // A list of each menu item's top position
    int menu_font_size;

    // Options
    std::vector<const char*> options_items;
    using OptionItemPaintHandler =
        void (InGameMenu::*)(LayoutImmediateContext& context);
    std::vector<OptionItemPaintHandler> options_handlers;
    int window_scaling_for_options;
    int volume_bar_height;
    int volume_bar_spacing;
  };

  enum LayoutConstants {
    kMenuItemMargin = 10,
  };

  LayoutImmediateContext PreLayoutImmediate();
  void DrawBackgroundImmediate(LayoutImmediateContext& context);
  void DrawMenuItemsImmediate(LayoutImmediateContext& context);
  void DrawMenuItemsImmediate_LayoutMenuItems(LayoutImmediateContext& context);
  void DrawMenuItemsImmediate_DrawMenuItems(LayoutImmediateContext& context);
  void DrawMenuItemsImmediate_DrawMenuItems_SaveLoad(
      LayoutImmediateContext& context);
  void DrawMenuItemsImmediate_DrawMenuItems_Options(
      LayoutImmediateContext& context);
  void DrawMenuItemsImmediate_DrawMenuItems_OptionsLayout(
      LayoutImmediateContext& context);
  void DrawMenuItemsImmediate_DrawMenuItems_Options_PaintEachOption(
      LayoutImmediateContext& context);
  void DrawMenuItemsImmediate_DrawMenuItems_Options_Volume(
      LayoutImmediateContext& context);
  void DrawMenuItemsImmediate_DrawMenuItems_Options_WindowSize(
      LayoutImmediateContext& context);
  void DrawMenuItemsImmediate_DrawMenuItems_Options_Joysticks(
      LayoutImmediateContext& context);
  void DrawMenuItemsImmediate_DrawMenuItems_Options_Languages(
      LayoutImmediateContext& context);
  void DrawSelectionImmediate(LayoutImmediateContext& context);

  void AddRectForSettingsItemPrompt(SettingsItem settings_index,
                                    const SDL_Rect& rect_for_left,
                                    const SDL_Rect& rect_for_right);
  void AddRectForSettingsItemPrompt(SettingsItem settings_index,
                                    const SDL_Rect& complex_item_rect);
  void AddRectForSettingsItem(SettingsItem settings_index,
                              const SDL_Rect& rect);
  void CleanUpSettingsItemRects();
  // Handle finger's down/move event and mouse pressed events.
  enum class MouseOrFingerEventType {
    kMousePressed,
    kFingerMove,
    kFingerDown,
  };
  bool HandleMouseOrFingerEvents(MouseOrFingerEventType type,
                                 int x_in_window,
                                 int y_in_window);

 private:
  MainWindow* main_window_ = nullptr;
  NESRuntime::Data* runtime_data_ = nullptr;
  bool first_paint_ = true;
  MenuItem current_selection_ = MenuItem::kContinue;
  SettingsItem current_setting_ = SettingsItem::kVolume;
  bool settings_entered_ = false;
  MenuItemCallback menu_callback_;
  SettingsItemCallback settings_callback_;
  int which_state_ = 0;
  int state_timestamp_ = 0;
  int which_autosave_state_slot_ = 0;
  std::set<int> hide_menus_;

  // Snapshot
  std::unique_ptr<LoadingWidget> loading_widget_;
  bool is_loading_snapshot_ = false;
  SDL_Texture* snapshot_ = nullptr;
  bool currently_has_snapshot_ = false;
  int current_auto_states_count_ = 0;

  // Menu and settings positions to handle finger touch or mouse events
  std::unordered_map<MenuItem, SDL_Rect> menu_positions_;
  std::unordered_map<SettingsItem, SDL_Rect> settings_positions_;
  std::unordered_map<SettingsItem,
                     std::vector<std::pair<SDL_Rect, SettingsItemContext>>>
      settings_prompt_positions_;
};

#endif  // UI_WIDGETS_IN_GAME_MENU_H_